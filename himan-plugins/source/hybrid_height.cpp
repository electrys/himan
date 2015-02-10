/**
 * @file hybrid_height.cpp
 *
 * @date Apr 5, 2013
 * @author peramaki
 */

#include "hybrid_height.h"
#include "plugin_factory.h"
#include "logger_factory.h"
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#define HIMAN_AUXILIARY_INCLUDE

#include "neons.h"
#include "radon.h"

#undef HIMAN_AUXILIARY_INCLUDE

using namespace std;
using namespace himan::plugin;

const string itsName("hybrid_height");

hybrid_height::hybrid_height() : itsFastMode(false)
{
	itsClearTextFormula = "HEIGHT = prevH + (287/9.81) * (T+prevT)/2 * log(prevP / P)";
	itsLogger = logger_factory::Instance()->GetLog(itsName);

}

void hybrid_height::Process(std::shared_ptr<const plugin_configuration> conf)
{
	Init(conf);

	SetParams({param("HL-M", 3, 0, 3, 6)});

	/*
	 * For hybrid height we must go through the levels backwards.
	 */

	itsInfo->LevelOrder(kBottomToTop);

	HPDatabaseType dbtype = conf->DatabaseType();
		
	if (dbtype == kNeons || dbtype == kNeonsAndRadon)
	{
		auto n = GET_PLUGIN(neons);

		itsBottomLevel = boost::lexical_cast<int> (n->ProducerMetaData(itsConfiguration->SourceProducer().Id(), "last hybrid level number"));
	}

	if ((dbtype == kRadon || dbtype == kNeonsAndRadon) && itsBottomLevel == kHPMissingInt)
	{
		auto r = GET_PLUGIN(radon);

		itsBottomLevel = boost::lexical_cast<int> (r->ProducerMetaData(itsConfiguration->SourceProducer().Id(), "last hybrid level number"));
	}
	
	itsUseGeopotential = (itsConfiguration->SourceProducer().Id() == 1 || itsConfiguration->SourceProducer().Id() == 199);

	if (!itsConfiguration->Exists("fast_mode") && itsConfiguration->GetValue("fast_mode") == "true")
	{
		itsFastMode = true;
	}
	else if (!itsUseGeopotential)
	{
		// When doing exact calculation we must do them sequentially starting from
		// surface closest to ground because every surface's value is depended
		// on the surface below it. Therefore we cannot parallelize the calculation
		// on level basis.
		
		if (Dimension() != kTimeDimension)
		{
			itsLogger->Info("Changing leading_dimension to time");
			Dimension(kTimeDimension);
		}
	}

	Start();
	
}

/*
 * Calculate()
 *
 * This function does the actual calculation.
 */

void hybrid_height::Calculate(shared_ptr<info> myTargetInfo, unsigned short threadIndex)
{

	const params GPParam { param("LNSP-N") , param("P-PA") };
	const param PParam("P-HPA");
	//const params TParam = { param("TG-K"), param("T-K") };
	const param TParam("T-K");
	const param TGParam("TG-K");
	const param ZParam("Z-M2S2");
	
	level H2;//(himan::kHeight, 2, "HEIGHT");
	level H0;//(himan::kHeight, 0, "HEIGHT");
	if ( itsConfiguration->SourceProducer().Id() == 131)
	{
		H2 = level(himan::kHybrid, 137, "GROUND");
		H0 = level(himan::kHybrid, 1, "LNSP");
	}
	else
	{
		H2 = level(himan::kHeight, 2, "HEIGHT");
		H0 = level(himan::kHeight, 0, "HEIGHT");
	}

	auto myThreadedLogger = logger_factory::Instance()->GetLog(itsName + "Thread #" + boost::lexical_cast<string> (threadIndex));

	forecast_time forecastTime = myTargetInfo->Time();
	level forecastLevel = myTargetInfo->Level();

	myThreadedLogger->Info("Calculating time " + static_cast<string>(forecastTime.ValidDateTime()) + " level " + static_cast<string> (forecastLevel));

	level prevLevel;

	bool firstLevel = false;
		
	if (itsFastMode || myTargetInfo->Level().Value() == itsBottomLevel)
	{
		firstLevel = true;
	}
	else
	{
		prevLevel = level(myTargetInfo->Level());
		prevLevel.Value(myTargetInfo->Level().Value() + 1);

		prevLevel.Index(prevLevel.Index() + 1);
	}


	info_t TInfo, PInfo, prevPInfo, prevTInfo, prevHInfo, GPInfo, zeroGPInfo;

	if (itsUseGeopotential)
	{
		GPInfo = Fetch(forecastTime, forecastLevel, ZParam, false);
		zeroGPInfo = Fetch(forecastTime, H0, ZParam, false);
	}
	else
	{
		if (!firstLevel)
		{
			prevTInfo = Fetch(forecastTime, prevLevel, TParam, false);
			prevPInfo = Fetch(forecastTime, prevLevel, PParam, false);
			prevHInfo = Fetch(forecastTime, prevLevel, param("HL-M"), false);
		}
		else
		{
			if ( itsConfiguration->SourceProducer().Id() == 131 )
			{
				prevPInfo = Fetch(forecastTime, H0, GPParam, false);
				prevTInfo = Fetch(forecastTime, H2, TParam, false);
			}
			else
			{
				prevPInfo = Fetch(forecastTime, H0, GPParam, false);
				prevTInfo = Fetch(forecastTime, H2, TGParam, false);
			}
		}

		PInfo = Fetch(forecastTime, forecastLevel, PParam, false);
		TInfo = Fetch(forecastTime, forecastLevel, TParam, false);

	}

	if ((itsUseGeopotential && (!GPInfo || !zeroGPInfo)) || (!itsUseGeopotential && (!prevTInfo || !prevPInfo || ( !prevHInfo && !firstLevel ) || !PInfo || !TInfo)))
	{
		myThreadedLogger->Warning("Skipping step " + boost::lexical_cast<string> (forecastTime.Step()) + ", level " + static_cast<string> (forecastLevel));
		return;
	}
	
	string deviceType = "CPU";

	if (itsUseGeopotential)
	{
		SetAB(myTargetInfo, GPInfo);
		
		GPInfo->ResetLocation();
		zeroGPInfo->ResetLocation();
	}
	else
	{
		SetAB(myTargetInfo, TInfo);
		
		PInfo->ResetLocation();
		TInfo->ResetLocation();
		prevPInfo->ResetLocation();
		prevTInfo->ResetLocation();

		if (!firstLevel)
		{
			prevHInfo->ResetLocation();
		}
	}

	LOCKSTEP(myTargetInfo)
	{

		if (itsUseGeopotential)
		{
			GPInfo->NextLocation();
			double GP = GPInfo->Value();

			zeroGPInfo->NextLocation();
			double zeroGP = zeroGPInfo->Value();
		
			if (GP == kFloatMissing || zeroGP == kFloatMissing)
			{
				continue;
			}

			double height = (GP - zeroGP) * himan::constants::kIg;
			
			myTargetInfo->Value(height);
		}
		else
		{
			TInfo->NextLocation();
			double T = TInfo->Value();

			PInfo->NextLocation();
			double P = PInfo->Value();

			prevTInfo->NextLocation();
			double prevT = prevTInfo->Value();

			prevPInfo->NextLocation();
			double prevP = prevPInfo->Value();

			double prevH = kFloatMissing;
			
			if (!firstLevel)
			{
				prevHInfo->NextLocation();
				prevH = prevHInfo->Value();

				if (prevH == kFloatMissing )
				{
					continue; 
				}
			}

			if ( prevT == kFloatMissing|| prevP == kFloatMissing || T == kFloatMissing || P == kFloatMissing )
			{
				continue;
			}

			if (firstLevel)
			{
				if ( itsConfiguration->SourceProducer().Id() == 131 )
				{
					prevP = exp (prevP) * 0.01f;
				}
				else 
				{
					prevP *= 0.01f;
				}
			}
			
			double deltaZ = 14.628 * (prevT + T) * (log(prevP/P));
			double totalHeight(0);

			if (firstLevel)
			{
				totalHeight = deltaZ;		
			}
			else
			{	
				totalHeight = prevH + deltaZ;
			}

			myTargetInfo->Value(totalHeight);
		}
	}

	if (!itsFastMode)
	{
		firstLevel = false;
	}

	myThreadedLogger->Info("[" + deviceType + "] Missing values: " + boost::lexical_cast<string> (myTargetInfo->Data().MissingCount()) + "/" + boost::lexical_cast<string> (myTargetInfo->Data().Size()));

}
