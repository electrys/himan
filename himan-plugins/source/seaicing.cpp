/**
 * @file seaicing.cpp
 *
 *  Created on: Jan 03, 2013
 *  @author aaltom
 */

#include "seaicing.h"
#include <iostream>
#include "plugin_factory.h"
#include "logger_factory.h"
#include <boost/lexical_cast.hpp>
#include "util.h"

#define HIMAN_AUXILIARY_INCLUDE

#include "fetcher.h"
#include "writer.h"
#include "pcuda.h"
#include "neons.h"

#undef HIMAN_AUXILIARY_INCLUDE

using namespace std;
using namespace himan::plugin;

#undef HAVE_CUDA

#ifdef HAVE_CUDA
namespace himan
{
namespace plugin
{
namespace icing_cuda
{
void doCuda(const float* Tin, float TBase, const float* Pin, float TScale, float* TPout, size_t N, float PConst, unsigned short index);
}
}
}
#endif

seaicing::seaicing() : itsUseCuda(false)
{
	itsClearTextFormula = "SeaIcing = FF * ( -0.35 -T2m ) / ( 1 + 0.3 * ( T0 + 0.35 ))";

	itsLogger = std::unique_ptr<logger> (logger_factory::Instance()->GetLog("seaicing"));

}

void seaicing::Process(shared_ptr<configuration> theConfiguration)
{

	shared_ptr<plugin::pcuda> c = dynamic_pointer_cast<plugin::pcuda> (plugin_factory::Instance()->Plugin("pcuda"));

	if (c && c->HaveCuda())
	{
		string msg = "I possess the powers of CUDA ";

		if (!theConfiguration->UseCuda())
		{
			msg += ", but I won't use them";
		}
		else
		{
			msg += ", and I'm not afraid to use them";
			itsUseCuda = true;
		}

		itsLogger->Info(msg);

	}

	// Get number of threads to use

	unsigned short threadCount = ThreadCount(theConfiguration->ThreadCount());

	boost::thread_group g;

	/*
	 * The target information is parsed from the configuration file.
	 */

	shared_ptr<info> theTargetInfo = theConfiguration->Info();

	/*
	 * Get producer information from neons if whole_file_write is false.
	 */

	if (!theConfiguration->WholeFileWrite())
	{
		shared_ptr<plugin::neons> n = dynamic_pointer_cast<plugin::neons> (plugin_factory::Instance()->Plugin("neons"));

		map<string,string> prodInfo = n->ProducerInfo(theTargetInfo->Producer().Id());

		if (!prodInfo.empty())
		{
			producer prod(theTargetInfo->Producer().Id());

			prod.Process(boost::lexical_cast<long> (prodInfo["process"]));
			prod.Centre(boost::lexical_cast<long> (prodInfo["centre"]));
			prod.Name(prodInfo["name"]);

			theTargetInfo->Producer(prod);
		}

	}

	/*
	 * Set target parameter to seaicing
	 * - name ICEIND-N
	 * - univ_id 190
	 * - grib2 descriptor 0'00'002
	 *
	 * We need to specify grib and querydata parameter information
	 * since we don't know which one will be the output format.
	 * (todo: we could check from theConfiguration but why bother?)
	 *
	 */

	vector<param> theParams;

	param theRequestedParam("ICEIND-N", 190);

	theRequestedParam.GribDiscipline(0);
	theRequestedParam.GribCategory(0);
	theRequestedParam.GribParameter(2);

	theParams.push_back(theRequestedParam);

	theTargetInfo->Params(theParams);

	/*
	 * Create data structures.
	 */

	theTargetInfo->Create();

	/*
	 * Initialize parent class functions for dimension handling
	 */

	Dimension(theConfiguration->LeadingDimension());
	FeederInfo(theTargetInfo->Clone());
	FeederInfo()->Param(theRequestedParam);

	/*
	 * Each thread will have a copy of the target info.
	 */

	vector<shared_ptr<info> > theTargetInfos;

	theTargetInfos.resize(threadCount);

	for (size_t i = 0; i < threadCount; i++)
	{

		itsLogger->Info("Thread " + boost::lexical_cast<string> (i + 1) + " starting");

		theTargetInfos[i] = theTargetInfo->Clone();

		boost::thread* t = new boost::thread(&seaicing::Run,
											 this,
											 theTargetInfos[i],
											 theConfiguration,
											 i + 1);

		g.add_thread(t);

	}

	g.join_all();

	if (theConfiguration->WholeFileWrite())
	{

		shared_ptr<writer> theWriter = dynamic_pointer_cast <writer> (plugin_factory::Instance()->Plugin("writer"));

		theTargetInfo->FirstTime();

		string theOutputFile = "himan_" + theTargetInfo->Param().Name() + "_" + theTargetInfo->Time().OriginDateTime()->String("%Y%m%d%H");
		theWriter->ToFile(theTargetInfo, theConfiguration->OutputFileType(), false, theOutputFile);

	}
}

void seaicing::Run(shared_ptr<info> myTargetInfo, shared_ptr<const configuration> theConfiguration, unsigned short theThreadIndex)
{
	while (AdjustLeadingDimension(myTargetInfo))
	{
		Calculate(myTargetInfo, theConfiguration, theThreadIndex);
	}
}

/*
 * Calculate()
 *
 * This function does the actual calculation.
 */

void seaicing::Calculate(shared_ptr<info> myTargetInfo, shared_ptr<const configuration> theConfiguration, unsigned short theThreadIndex)
{

	shared_ptr<fetcher> theFetcher = dynamic_pointer_cast <fetcher> (plugin_factory::Instance()->Plugin("fetcher"));

	// Required source parameters

	param TParam("T-K");
	level TgLevel(himan::kHeight, 0, "HEIGHT");
	param FfParam("FFG-MS");  // 10 meter wind
	level FfLevel(himan::kHeight, 10, "HEIGHT");

	unique_ptr<logger> myThreadedLogger = std::unique_ptr<logger> (logger_factory::Instance()->GetLog("seaicingThread #" + boost::lexical_cast<string> (theThreadIndex)));

	ResetNonLeadingDimension(myTargetInfo);

	myTargetInfo->FirstParam();

	while (AdjustNonLeadingDimension(myTargetInfo))
	{

		myThreadedLogger->Debug("Calculating time " + myTargetInfo->Time().ValidDateTime()->String("%Y%m%d%H") +
								" level " + boost::lexical_cast<string> (myTargetInfo->Level().Value()));

		myTargetInfo->Data()->Resize(theConfiguration->Ni(), theConfiguration->Nj());

		shared_ptr<info> TInfo;
		shared_ptr<info> TgInfo;
		shared_ptr<info> FfInfo;

		try
		{
			// Source info for T
			TInfo = theFetcher->Fetch(theConfiguration,
								 myTargetInfo->Time(),
								 myTargetInfo->Level(),
								 TParam);
				
			// Source info for Tg
			TgInfo = theFetcher->Fetch(theConfiguration,
								 myTargetInfo->Time(),
								 TgLevel,
								 TParam);

			// Source info for FF
			FfInfo = theFetcher->Fetch(theConfiguration,
								 myTargetInfo->Time(),
								 FfLevel,
								 FfParam);
				
		}
		catch (HPExceptionType e)
		{
			//HPExceptionType t = static_cast<HPExceptionType> (e);

			switch (e)
			{
			case kFileDataNotFound:
				itsLogger->Info("Skipping step " + boost::lexical_cast<string> (myTargetInfo->Time().Step()) + ", level " + boost::lexical_cast<string> (myTargetInfo->Level().Value()));
				myTargetInfo->Data()->Fill(kFloatMissing); // Fill data with missing value
				continue;
				break;

			default:
				throw runtime_error(ClassName() + ": Unable to proceed");
				break;
			}
		}

		shared_ptr<NFmiGrid> targetGrid(myTargetInfo->Grid()->ToNewbaseGrid());
		shared_ptr<NFmiGrid> TGrid(TInfo->Grid()->ToNewbaseGrid());
		shared_ptr<NFmiGrid> TgGrid(TgInfo->Grid()->ToNewbaseGrid());
		shared_ptr<NFmiGrid> FfGrid(FfInfo->Grid()->ToNewbaseGrid());

		int missingCount = 0;
		int count = 0;

		assert(targetGrid->Size() == myTargetInfo->Data()->Size());

		bool equalGrids = (*myTargetInfo->Grid() == *TInfo->Grid() &&
							*myTargetInfo->Grid() == *TgInfo->Grid() &&
							*myTargetInfo->Grid() == *FfInfo->Grid());

		myTargetInfo->ResetLocation();

		targetGrid->Reset();

		while (myTargetInfo->NextLocation() && targetGrid->Next())
		{
			count++;

			double T = kFloatMissing;
			double Tg = kFloatMissing;
			double Ff = kFloatMissing;

			InterpolateToPoint(targetGrid, TGrid, equalGrids, T);
			InterpolateToPoint(targetGrid, TgGrid, equalGrids, Tg);
			InterpolateToPoint(targetGrid, FfGrid, equalGrids, Ff);

			if (T == kFloatMissing || Tg == kFloatMissing || Ff == kFloatMissing)
			{
				missingCount++;

				myTargetInfo->Value(-10);  // No missing values
				continue;
			}

			double seaIcing;
			double TBase = 273.15;

			T = T - TBase;
			Tg = Tg - TBase;

			if (Tg < -2 )
			{
				seaIcing = -10;
			}
			else
			{
				seaIcing = Ff * ( -0.35 -T ) / ( 1 + 0.3 * ( Tg + 0.35 ));

				if (seaIcing > 100)
				{
					seaIcing = 100;
				}
			}

			if (!myTargetInfo->Value(seaIcing))
			{
				throw runtime_error(ClassName() + ": Failed to set value to matrix");
			}

		}

		/*
		 * Now we are done for this level
		 *
		 * Clone info-instance to writer since it might change our descriptor places		 
		 */

		myThreadedLogger->Info("Missing values: " + boost::lexical_cast<string> (missingCount) + "/" + boost::lexical_cast<string> (count));

		if (!theConfiguration->WholeFileWrite())
		{
			shared_ptr<writer> theWriter = dynamic_pointer_cast <writer> (plugin_factory::Instance()->Plugin("writer"));

			theWriter->ToFile(myTargetInfo->Clone(), theConfiguration->OutputFileType(), true);
		}
	}
}
