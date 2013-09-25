/**
 * @file grib.cpp
 *
 * @date Nov 20, 2012
 * @author partio
 */

#include "grib.h"
#include "logger_factory.h"
#include "plugin_factory.h"
#include "producer.h"
#include "util.h"
#include "grid.h"

using namespace std;
using namespace himan::plugin;

#define HIMAN_AUXILIARY_INCLUDE

#include "neons.h"

#undef HIMAN_AUXILIARY_INCLUDE

#include "cuda_helper.h"
#include "simple_packed.h"

grib::grib()
{

	itsLogger = std::unique_ptr<logger> (logger_factory::Instance()->GetLog("grib"));

	itsGrib = shared_ptr<NFmiGrib> (new NFmiGrib());
}

shared_ptr<NFmiGrib> grib::Reader()
{
	return itsGrib;
}

bool grib::ToFile(shared_ptr<info> anInfo, const string& outputFile, HPFileType fileType, HPFileWriteOption fileWriteOption)
{

	if (fileWriteOption == kNeons || fileWriteOption == kMultipleFiles)
	{
		// Write only that data which is currently set at descriptors

		WriteGrib(anInfo, outputFile, fileType);
	}

	else
	{
		anInfo->ResetTime();

		while (anInfo->NextTime())
		{
			anInfo->ResetLevel();

			while (anInfo->NextLevel())
			{
				anInfo->ResetParam();

				while (anInfo->NextParam())
				{
					if (!WriteGrib(anInfo, outputFile, fileType, true))
					{
						itsLogger->Error("Error writing grib to file");
					}
				}
			}
		}
	}

	return true;

}

bool grib::WriteGrib(shared_ptr<const info> anInfo, const string& outputFile, HPFileType fileType, bool appendToFile)
{
	const static long edition = static_cast<long> (fileType);

	itsGrib->Message()->Edition(edition);

	shared_ptr<neons> n; 

	if (anInfo->Producer().Centre() == kHPMissingInt)
	{
		n = dynamic_pointer_cast<neons> (plugin_factory::Instance()->Plugin("neons"));

		map<string, string> producermap = n->NeonsDB().GetGridModelDefinition(anInfo->Producer().Id());

		itsGrib->Message()->Centre(boost::lexical_cast<long> (producermap["ident_id"]));
		itsGrib->Message()->Process(boost::lexical_cast<long> (producermap["model_id"]));

		if (edition == 1)
		{
			itsGrib->Message()->Table2Version(boost::lexical_cast<long> (producermap["no_vers"]));
		}
	}
	else
	{
		itsGrib->Message()->Centre(anInfo->Producer().Centre());
		itsGrib->Message()->Process(anInfo->Producer().Process());

		if (edition == 1)
		{
			itsGrib->Message()->Table2Version(anInfo->Producer().TableVersion());
		}
	}

	// Parameter
	
	WriteParameter(anInfo);

	// Area and Grid
	
	WriteAreaAndGrid(anInfo);

	// Time information

	WriteTime(anInfo);
	
	// Level

	itsGrib->Message()->LevelValue(static_cast<long> (anInfo->Level().Value()));

	// Himan levels equal to grib 1

	if (edition == 1)
	{
		itsGrib->Message()->LevelType(anInfo->Level().Type());
	}
	else if (edition == 2)
	{
		itsGrib->Message()->LevelType(itsGrib->Message()->LevelTypeToAnotherEdition(anInfo->Level().Type(),2));
	}

	// Misc

	if (edition == 2)
	{
		itsGrib->Message()->TypeOfGeneratingProcess(2); // Forecast
	}
	
	itsGrib->Message()->Bitmap(true);

	//itsGrib->Message()->BitsPerValue(16);

	itsGrib->Message()->Values(anInfo->Data()->Values(), anInfo->Ni() * anInfo->Nj());

	if (edition == 2)
	{
		itsGrib->Message()->PackingType("grid_jpeg");
	}

	/*
	 *  GRIB 1
	 *
	 * 	BIT	VALUE	MEANING
	 *	1	0		Direction increments not given
	 *	1	1		Direction increments given
	 *	2	0		Earth assumed spherical with radius = 6367.47 km
	 *	2	1		Earth assumed oblate spheroid with size as determined by IAU in 1965: 6378.160 km, 6356.775 km, f = 1/297.0
	 *	3-4	0		reserved (set to 0)
	 *	5	0		u- and v-components of vector quantities resolved relative to easterly and northerly directions
	 * 	5	1		u and v components of vector quantities resolved relative to the defined grid in the direction of increasing x and y (or i and j) coordinates respectively
	 *	6-8	0		reserved (set to 0)
	 *
	 *	GRIB2
	 *
	 *	Bit No. 	Value 	Meaning
	 *	1-2			Reserved
	 *	3		0	i direction increments not given
	 *	3		1	i direction increments given
	 *	4		0	j direction increments not given
	 *	4		1	j direction increments given
	 *	5		0	Resolved u and v components of vector quantities relative to easterly and northerly directions
	 *	5		1	Resolved u and v components of vector quantities relative to the defined grid in the direction
	 *				of increasing x and y (or i and j) coordinates, respectively.
	 *	6-8			Reserved - set to zero
	 *
	 */

	if (edition == 1)
	{
		itsGrib->Message()->ResolutionAndComponentFlags(128); // 10000000
	}
	else
	{
		itsGrib->Message()->ResolutionAndComponentFlags(48); // 00110000
	}

	vector<double> AB = anInfo->Grid()->AB();

	if (!AB.empty())
	{
		itsGrib->Message()->NV(AB.size());
		itsGrib->Message()->PV(AB, AB.size());
	}

	itsGrib->Message()->Write(outputFile, appendToFile);

	string verb = (appendToFile ? "Appended to " : "Wrote ");
	itsLogger->Info(verb + "file '" + outputFile + "'");

	return true;
}

vector<shared_ptr<himan::info>> grib::FromFile(const string& theInputFile, const search_options& options, bool readContents, bool readPackedData)
{

	shared_ptr<neons> n = dynamic_pointer_cast<neons> (plugin_factory::Instance()->Plugin("neons"));

	vector<shared_ptr<himan::info>> infos;

	if (!itsGrib->Open(theInputFile))
	{
		itsLogger->Error("Opening file '" + theInputFile + "' failed");
		return infos;
	}

	itsLogger->Debug("Reading file '" + theInputFile + "'");

	int foundMessageNo = 0;

	if (options.prod.Centre() == kHPMissingInt)
	{
		itsLogger->Error("Process and centre information for producer " + boost::lexical_cast<string> (options.prod.Id()) + " are undefined");
		return infos;
	}

	while (itsGrib->NextMessage())
	{

		foundMessageNo++;

		/*
		 * One grib file may contain many grib messages. Loop though all messages
		 * and get all that match our search options.
		 *
		 */

		//<!todo Should we actually return all matching messages or only the first one

		long centre = itsGrib->Message()->Centre();
		long process = itsGrib->Message()->Process();

		if (options.prod.Process() != process || options.prod.Centre() != centre)
		{
			itsLogger->Trace("centre/process do not match: " + boost::lexical_cast<string> (options.prod.Process()) + " vs " + boost::lexical_cast<string> (process));
			itsLogger->Trace("centre/process do not match: " + boost::lexical_cast<string> (options.prod.Centre()) + " vs " + boost::lexical_cast<string> (centre));
			//continue;
		}

		param p;

		long number = itsGrib->Message()->ParameterNumber();

		if (itsGrib->Message()->Edition() == 1)
		{
			long no_vers = itsGrib->Message()->Table2Version();

			p.Name(n->GribParameterName(number, no_vers));
			p.GribParameter(number);
			p.GribTableVersion(no_vers);
				
		}
		else
		{
			long category = itsGrib->Message()->ParameterCategory();
			long discipline = itsGrib->Message()->ParameterDiscipline();
			long process = options.prod.Process();
		
			p.Name(n->GribParameterName(number, category, discipline, process));

			p.GribParameter(number);
			p.GribDiscipline(discipline);
			p.GribCategory(category);

			if (p.Name() == "T-C" && options.prod.Centre() == 7)
			{
				p.Name("T-K");
			}
		}

		if (itsGrib->Message()->ParameterUnit() == "K")
		{
		   	p.Unit(kK);
		}
		else if (itsGrib->Message()->ParameterUnit() == "Pa s-1")
		{
		   	p.Unit(kPas);
		}
		else if (itsGrib->Message()->ParameterUnit() == "%")
		{
		   	p.Unit(kPrcnt);
		}
		else if (itsGrib->Message()->ParameterUnit() == "m s**-1")
		{
			p.Unit(kMs);
		}
		else if (itsGrib->Message()->ParameterUnit() == "m")
		{
			p.Unit(kM);
		}
		else if (itsGrib->Message()->ParameterUnit() == "mm")
		{
			p.Unit(kMm);
		}
		else if (itsGrib->Message()->ParameterUnit() == "Pa")
		{
			p.Unit(kPa);
		}
		else if (itsGrib->Message()->ParameterUnit() == "m**2 s**-2")
		{
			p.Unit(kGph);
		}
		else
		{
			itsLogger->Trace("Unable to determine himan parameter unit for grib unit " + itsGrib->Message()->ParameterUnit());
		}

		if (p != options.param)
		{
			itsLogger->Trace("Parameter does not match: " + options.param.Name() + " vs " + p.Name());
			continue;
		}

		string dataDate = boost::lexical_cast<string> (itsGrib->Message()->DataDate());

		/*
		 * dataTime is HH24MM in long datatype.
		 *
		 * So, for example analysistime 00 is 0, and 06 is 600.
		 *
		 */

		long dt = itsGrib->Message()->DataTime();

		string dataTime = boost::lexical_cast<string> (dt);

		if (dt < 1000)
		{
			dataTime = "0" + dataTime;
		}

		/* Determine time step */

		long step;

		if (itsGrib->Message()->TimeRangeIndicator() == 10)
		{
			long P1 = itsGrib->Message()->P1();
			long P2 = itsGrib->Message()->P2();

			step = (P1 << 8 ) | P2;
		}
		else
		{
			step = itsGrib->Message()->EndStep();

			if (itsGrib->Message()->Edition() == 2)
			{
				step = itsGrib->Message()->ForecastTime();
			}
		}
		
		string originDateTimeStr = dataDate + dataTime;

		raw_time originDateTime (originDateTimeStr, "%Y%m%d%H%M");

		forecast_time t (originDateTime, originDateTime);

		long unitOfTimeRange = itsGrib->Message()->UnitOfTimeRange();

		HPTimeResolution timeResolution = kHourResolution;

		if (unitOfTimeRange == 0)
		{
			timeResolution = kMinuteResolution;
		}

		t.StepResolution(timeResolution);

		t.ValidDateTime()->Adjust(timeResolution, static_cast<int> (step));

		if (t != options.time)
		{
			itsLogger->Trace("Times do not match");
			itsLogger->Trace("OriginDateTime: " + options.time.OriginDateTime()->String() + " (requested) vs " + t.OriginDateTime()->String() + " (found)");
			itsLogger->Trace("ValidDateTime: " + options.time.ValidDateTime()->String() + " (requested) vs " + t.ValidDateTime()->String() + " (found)");
			continue;
		}

		long gribLevel = itsGrib->Message()->NormalizedLevelType();

		himan::HPLevelType levelType;

		switch (gribLevel)
		{
		case 1:
			levelType = himan::kGround;
			break;

		case 100:
			levelType = himan::kPressure;
			break;

		case 102:
			levelType = himan::kMeanSea;
			break;

		case 105:
			levelType = himan::kHeight;
			break;

		case 109:
			levelType = himan::kHybrid;
			break;

		case 112:
			levelType = himan::kGndLayer;
			break;

		default:
			itsLogger->Error(ClassName() + ": Unsupported level type: " + boost::lexical_cast<string> (gribLevel));
			continue;
			break;

		}

		level l (levelType, static_cast<float> (itsGrib->Message()->LevelValue()));

		if (l != options.level)
		{
			itsLogger->Trace("Level does not match: " + 
				string(HPLevelTypeToString.at(options.level.Type())) +
				" " +
				string(boost::lexical_cast<string> (options.level.Value())) +
				" vs " +
				string(HPLevelTypeToString.at(l.Type())) +
				" " +
				string(boost::lexical_cast<string> (l.Value())));
			continue;
		}

		std::vector<double> ab;

		if (levelType == himan::kHybrid)
		{
		 	long nv = itsGrib->Message()->NV();
		 	long lev = itsGrib->Message()->LevelValue();
			ab = itsGrib->Message()->PV(nv, lev);
		}

		// END VALIDATION OF SEARCH PARAMETERS

		shared_ptr<info> newInfo (new info());
		shared_ptr<grid> newGrid (new grid());

		producer prod(itsGrib->Message()->Centre(), process);

		newGrid->AB(ab);

		newInfo->Producer(prod);

		vector<param> theParams;

		theParams.push_back(p);

		newInfo->Params(theParams);

		vector<forecast_time> theTimes;

		theTimes.push_back(t);

		newInfo->Times(theTimes);

		vector<level> theLevels;

		theLevels.push_back(l);

		newInfo->Levels(theLevels);

		/*
		 * Get area information from grib.
		 */

		size_t ni = itsGrib->Message()->SizeX();
		size_t nj = itsGrib->Message()->SizeY();

		switch (itsGrib->Message()->NormalizedGridType())
		{
		case 0:
			newGrid->Projection(kLatLonProjection);
			break;

		case 5:
			newGrid->Projection(kStereographicProjection);
			
			newGrid->Orientation(itsGrib->Message()->GridOrientation());
			newGrid->Di(itsGrib->Message()->XLengthInMeters() / (ni - 1));
			newGrid->Dj(itsGrib->Message()->YLengthInMeters() / (nj - 1));
			break;

		case 10:
			newGrid->Projection(kRotatedLatLonProjection);
			newGrid->SouthPole(himan::point(itsGrib->Message()->SouthPoleX(), itsGrib->Message()->SouthPoleY()));
			break;

		default:
			throw runtime_error(ClassName() + ": Unsupported projection: " + boost::lexical_cast<string> (itsGrib->Message()->NormalizedGridType()));
			break;

		}

		bool iNegative = itsGrib->Message()->IScansNegatively();
		bool jPositive = itsGrib->Message()->JScansPositively();

		HPScanningMode m = kUnknownScanningMode;

		if (!iNegative && !jPositive)
		{
			m = kTopLeft;
		}
		else if (iNegative && !jPositive)
		{
			m = kTopRight;
		}
		else if (iNegative && jPositive)
		{
			m = kBottomRight;
		}
		else if (!iNegative && jPositive)
		{
			m = kBottomLeft;
		}
		else
		{
			throw runtime_error("WHAT?");
		}

		newGrid->ScanningMode(m);

		if (newGrid->Projection() == kRotatedLatLonProjection)
		{
			newGrid->UVRelativeToGrid(itsGrib->Message()->UVRelativeToGrid());
		}

		if (newGrid->Projection() == kStereographicProjection)
		{
			/*
			 * Do not support stereographic projections but in bottom left scanning mode.
			 *
			 * The calculation of grid extremes could be done with f.ex. NFmiAzimuthalArea
			 * but lets not do that unless it's absolutely necessary.
			 */
			
			if (newGrid->ScanningMode() != kBottomLeft)
			{
				itsLogger->Error(ClassName() + ": stereographic projection only supported when scanning mode is bottom left");
				continue;
			}

			newGrid->BottomLeft(himan::point(itsGrib->Message()->X0(),itsGrib->Message()->Y0()));
		}
		else
		{

			pair<point,point> coordinates = util::CoordinatesFromFirstGridPoint(himan::point(itsGrib->Message()->X0(), itsGrib->Message()->Y0()), ni, nj, itsGrib->Message()->iDirectionIncrement(),itsGrib->Message()->jDirectionIncrement(), m);

			newGrid->BottomLeft(coordinates.first);
			newGrid->TopRight(coordinates.second);
		}

		newInfo->Create(newGrid);

		// Set descriptors

		newInfo->Param(p);
		newInfo->Time(t);
		newInfo->Level(l);

		shared_ptr<unpacked> dm = shared_ptr<unpacked> (new unpacked(ni, nj));

		/*
		 * Read data from grib *
		 */

		size_t len = 0;

#if defined GRIB_READ_PACKED_DATA && defined HAVE_CUDA

		if (readPackedData && itsGrib->Message()->PackingType() == "grid_simple")
		{
			len = itsGrib->Message()->PackedValuesLength();

			itsLogger->Debug("Current CUDA device id: " + CudaDeviceId());
			
			unsigned char* data = 0, *bitmap = 0;
			int* unpackedBitmap;
			
			CUDA_CHECK(cudaHostAlloc(reinterpret_cast<void**> (&data), len * sizeof(unsigned char), cudaHostAllocMapped));

			// Get packed values from grib
			
			itsGrib->Message()->PackedValues(data);

			itsLogger->Debug("Retrieved " + boost::lexical_cast<string> (len) + " bytes of packed data from grib");
			
			double bsf = itsGrib->Message()->BinaryScaleFactor();
			double dsf = itsGrib->Message()->DecimalScaleFactor();
			double rv = itsGrib->Message()->ReferenceValue();
			long bpv = itsGrib->Message()->BitsPerValue();

			auto packed = std::make_shared<simple_packed> (bpv, util::ToPower(bsf,2), util::ToPower(-dsf, 10), rv);

			packed->Set(data, len, itsGrib->Message()->SizeX() * itsGrib->Message()->SizeY());

			if (itsGrib->Message()->Bitmap())
			{
				size_t bitmap_len =itsGrib->Message()->BytesLength("bitmap");
				size_t bitmap_size = ceil(bitmap_len/8);

				CUDA_CHECK(cudaHostAlloc(reinterpret_cast<void**> (&unpackedBitmap), bitmap_len * sizeof(int), cudaHostAllocMapped));

				bitmap = new unsigned char[bitmap_size];

				itsGrib->Message()->Bytes("bitmap", bitmap);

				UnpackBitmap(bitmap, unpackedBitmap, bitmap_size);
				
				packed->Bitmap(unpackedBitmap, bitmap_len);

				delete [] bitmap;
			}

			newInfo->Grid()->PackedData(packed);
		}
		else
#endif
		if (readContents)
		{
			len = itsGrib->Message()->ValuesLength();

			double* d = itsGrib->Message()->Values();

			dm->Set(d, len);

			free(d);
		}

		newInfo->Grid()->Data(dm);

		infos.push_back(newInfo);

		break ; // We found what we were looking for
	}

	if (infos.size())
	{
		// This will be broken when/if we return multiple infos from this function
		itsLogger->Trace("Data found from message " + boost::lexical_cast<string> (foundMessageNo) + "/" + boost::lexical_cast<string> (itsGrib->MessageCount()));
	}

	return infos;
}


#define BitMask1(i)	(1u << i)
#define BitTest(n,i)	!!((n) & BitMask1(i))

void grib::UnpackBitmap(const unsigned char* __restrict__ bitmap, int* __restrict__ unpacked, size_t len) const
{
	size_t i, v = 1, idx = 0;
	short j = 0;

	for (i = 0; i < len; i++)
	{
		for (j = 7; j >= 0; j--)
		{
			if (BitTest(bitmap[i], j))
			{
				unpacked[idx] = v++;
			}

			idx++;
	    }
	}
}

void grib::WriteAreaAndGrid(std::shared_ptr<const info> anInfo)
{
	himan::point firstGridPoint = anInfo->Grid()->FirstGridPoint();
	himan::point lastGridPoint = anInfo->Grid()->LastGridPoint();

	long edition = itsGrib->Message()->Edition();
	
	switch (anInfo->Grid()->Projection())
	{
		case kLatLonProjection:
		{
			long gridType = 0; // Grib 1

			if (edition == 2)
			{
				gridType = itsGrib->Message()->GridTypeToAnotherEdition(gridType, 2);
			}

			itsGrib->Message()->GridType(gridType);

			itsGrib->Message()->X0(firstGridPoint.X());
			itsGrib->Message()->X1(lastGridPoint.X());
			itsGrib->Message()->Y0(firstGridPoint.Y());
			itsGrib->Message()->Y1(lastGridPoint.Y());

			itsGrib->Message()->iDirectionIncrement(anInfo->Di());
			itsGrib->Message()->jDirectionIncrement(anInfo->Dj());

			break;
		}

		case kRotatedLatLonProjection:
		{

			long gridType = 10; // Grib 1

			if (edition == 2)
			{
				gridType = itsGrib->Message()->GridTypeToAnotherEdition(gridType, 2);
			}

			itsGrib->Message()->GridType(gridType);

			itsGrib->Message()->X0(firstGridPoint.X());
			itsGrib->Message()->Y0(firstGridPoint.Y());
			itsGrib->Message()->X1(lastGridPoint.X());
			itsGrib->Message()->Y1(lastGridPoint.Y());

			itsGrib->Message()->SouthPoleX(anInfo->Grid()->SouthPole().X());
			itsGrib->Message()->SouthPoleY(anInfo->Grid()->SouthPole().Y());

			itsGrib->Message()->iDirectionIncrement(anInfo->Grid()->Di());
			itsGrib->Message()->jDirectionIncrement(anInfo->Grid()->Dj());

			itsGrib->Message()->GridType(gridType);

			itsGrib->Message()->UVRelativeToGrid(anInfo->Grid()->UVRelativeToGrid());

			break;
		}

		case kStereographicProjection:
		{
			long gridType = 5; // Grib 1

			if (edition == 2)
			{
				gridType = itsGrib->Message()->GridTypeToAnotherEdition(gridType, 2);
			}

			itsGrib->Message()->GridType(gridType);

			itsGrib->Message()->X0(anInfo->Grid()->BottomLeft().X());
			itsGrib->Message()->Y0(anInfo->Grid()->BottomLeft().Y());

			itsGrib->Message()->GridOrientation(anInfo->Grid()->Orientation());
			itsGrib->Message()->XLengthInMeters(anInfo->Grid()->Di() * (anInfo->Grid()->Ni() - 1));
			itsGrib->Message()->YLengthInMeters(anInfo->Grid()->Dj() * (anInfo->Grid()->Nj() - 1));
			break;
		}

		default:
			throw runtime_error(ClassName() + ": invalid projection while writing grib: " + boost::lexical_cast<string> (anInfo->Grid()->Projection()));
			break;
	}

	itsGrib->Message()->SizeX(anInfo->Ni());
	itsGrib->Message()->SizeY(anInfo->Nj());

	bool iNegative = itsGrib->Message()->IScansNegatively();
	bool jPositive = itsGrib->Message()->JScansPositively();

	switch (anInfo->Grid()->ScanningMode())
	{
		case kTopLeft:
			iNegative = false;
			jPositive = false;
			break;

		case kTopRight:
			iNegative = true;
			jPositive = false;
			break;

		case kBottomLeft:
			iNegative = false;
			jPositive = true;
			break;

		case kBottomRight:
			iNegative = true;
			jPositive = true;
			break;

		default:
			throw runtime_error(ClassName() + ": Uknown scanning mode when writing grib");
			break;
	}

	itsGrib->Message()->IScansNegatively(iNegative);
	itsGrib->Message()->JScansPositively(jPositive);
}

void grib::WriteTime(std::shared_ptr<const info> anInfo)
{
	itsGrib->Message()->DataDate(boost::lexical_cast<long> (anInfo->Time().OriginDateTime()->String("%Y%m%d")));
	itsGrib->Message()->DataTime(boost::lexical_cast<long> (anInfo->Time().OriginDateTime()->String("%H%M")));

	if (itsGrib->Message()->Edition() == 1)
	{
		itsGrib->Message()->StartStep(anInfo->Time().Step());
		itsGrib->Message()->EndStep(anInfo->Time().Step());
	}
	else
	{
		itsGrib->Message()->ForecastTime(anInfo->Time().Step());
	}

	/*
	 * Check if this is an aggregated parameter.
	 *
	 * At least when writing grib2, the aggregation of the parameter is defined outside
	 * the actual parameter numbering scheme.
	 *
	 * One thing to note is that when we have mixed time types in the grib, for example
	 * when calculating hourly parameters to harmonie (forecast time in minutes, aggregation
	 * time in hours) we must convert them to the same unit, in harmonies case minute.
	 */

	if (anInfo->Param().Aggregation().AggregationType() != kUnknownAggregationType)
	{
		long timeRangeValue = 1;
		long unitForTimeRange = 1;

		if (anInfo->Param().Aggregation().TimeResolution() == kHourResolution)
		{
			timeRangeValue = 1;

			int timeResolutionValue = anInfo->Param().Aggregation().TimeResolutionValue();

			if (anInfo->Time().StepResolution() == kHourResolution)
			{

				if (timeResolutionValue == 1)
				{
					unitForTimeRange = 1; // hour
				}
				else if (timeResolutionValue == 3)
				{
					unitForTimeRange = 10; // 3 hours
				}
				else if (timeResolutionValue == 6)
				{
					unitForTimeRange = 11; // 6 hours
				}
				else if (timeResolutionValue == 12)
				{
					unitForTimeRange = 12; // 1 hours
				}
				else
				{
					throw runtime_error(ClassName() + ": Invalid timeResolutionValue: " + boost::lexical_cast<string> (timeResolutionValue));
				}
			}
			else
			{
				// mixed time types in the grib, must convert
				timeRangeValue = 60 * timeResolutionValue;
				unitForTimeRange = 0; // minute
				
			}
			
		}
		else if (anInfo->Param().Aggregation().TimeResolution() == kMinuteResolution)
		{
			itsLogger->Warning(ClassName() + ": minute resolution for aggregated data, seems fishy");
			
			unitForTimeRange = 0; // minute
			timeRangeValue = anInfo->Param().Aggregation().TimeResolutionValue();
		}

		itsGrib->Message()->LengthOfTimeRange(timeRangeValue);
		itsGrib->Message()->UnitForTimeRange(unitForTimeRange);

	}
	
	long unitOfTimeRange = 1; // hour

	if (anInfo->Time().StepResolution() == kMinuteResolution)
	{
		unitOfTimeRange = 0;
	}
	
	itsGrib->Message()->UnitOfTimeRange(unitOfTimeRange);

	if (anInfo->StepSizeOverOneByte()) // Forecast with stepvalues that don't fit in one byte
	{
		itsGrib->Message()->TimeRangeIndicator(10);

		long step = anInfo->Time().Step();
		long p1 = (step & 0xFF00) >> 8;
		long p2 = step & 0x00FF;

		itsGrib->Message()->P1(p1);
		itsGrib->Message()->P2(p2);

	}
	else
	{
		itsGrib->Message()->TimeRangeIndicator(0); // Force forecast
	}
}

void grib::WriteParameter(std::shared_ptr<const info> anInfo)
{
	/*
	 * For grib1, get param_id from neons since its dependant on the table2version
	 *
	 * For grib2, assume the plugin has set the correct numbers since they are "static".
	 */

	if (itsGrib->Message()->Edition() == 1)
	{
		shared_ptr<neons> n;
		
		long parm_id = anInfo->Param().GribIndicatorOfParameter();

		if (parm_id == kHPMissingInt)
		{
			if (!n)
			{
				n = dynamic_pointer_cast<neons> (plugin_factory::Instance()->Plugin("neons"));
			}

			parm_id = n->NeonsDB().GetGridParameterId(itsGrib->Message()->Table2Version(), anInfo->Param().Name());
		}

		itsGrib->Message()->ParameterNumber(parm_id);
	}
	else
	{
		itsGrib->Message()->ParameterNumber(anInfo->Param().GribParameter());
		itsGrib->Message()->ParameterCategory(anInfo->Param().GribCategory());
		itsGrib->Message()->ParameterDiscipline(anInfo->Param().GribDiscipline()) ;

		if (anInfo->Param().Aggregation().AggregationType() != kUnknownAggregationType)
		{
			itsGrib->Message()->ProductDefinitionTemplateNumber(8);

			long type;

			switch (anInfo->Param().Aggregation().AggregationType())
			{
				default:
				case kAverage:
					type=0;
					break;
				case kAccumulation:
					type=1;
					break;
				case kMaximum:
					type=2;
					break;
				case kMinimum:
					type=3;
					break;
			}

			itsGrib->Message()->TypeOfStatisticalProcessing(type);
		}
	}
}
