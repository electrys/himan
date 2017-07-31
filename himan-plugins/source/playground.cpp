/**
 * @file playground.cpp
 */

#include "playground.h"
#include "forecast_time.h"
#include "json_parser.h"
#include "level.h"
#include "logger_factory.h"
#include "plugin_factory.h"
#include "util.h"
#include <boost/lexical_cast.hpp>
#include "generator.h"
#include "hitool.h"
#include "fetcher.h"
#include <ctime>

using namespace std;
using namespace himan::plugin;

#include "cuda_helper.h"

playground::playground()
    : itsBase(0.0),
      itsScale(1.0),
      itsTargetUnivID(9999),
      itsApplyLandSeaMask(false),
      itsLandSeaMaskThreshold(0.5),
      itsInterpolationMethod(kUnknownInterpolationMethod)
{
	itsClearTextFormula = "target_param = source_param * itsScale + itsBase";
	itsCudaEnabledCalculation = true;

	itsLogger = logger_factory::Instance()->GetLog("playground");
}

vector<himan::level> playground::LevelsFromString(const string& levelType, const string& levelValues) const
{
	HPLevelType theLevelType = HPStringToLevelType.at(boost::to_lower_copy(levelType));

	vector<string> levelsStr = util::Split(levelValues, ",", true);

	vector<level> levels;

	for (size_t i = 0; i < levelsStr.size(); i++)
	{
		levels.push_back(level(theLevelType, boost::lexical_cast<float>(levelsStr[i]), levelType));
	}

	return levels;
}

void playground::SetAdditionalParameters()
{
	std::string itsSourceLevelType;
	std::string SourceLevels;

	if (!itsConfiguration->GetValue("base").empty())
	{
		itsBase = boost::lexical_cast<double>(itsConfiguration->GetValue("base"));
	}
	else
	{
		itsLogger->Warning("Base not specified, using default value 0.0");
	}

	if (!itsConfiguration->GetValue("scale").empty())
	{
		itsScale = boost::lexical_cast<double>(itsConfiguration->GetValue("scale"));
	}
	else
	{
		itsLogger->Warning("Scale not specified, using default value 1.0");
	}

	if (!itsConfiguration->GetValue("target_univ_id").empty())
	{
		itsTargetUnivID = boost::lexical_cast<int>(itsConfiguration->GetValue("target_univ_id"));
	}
	else
	{
		itsLogger->Warning("Target_univ_id not specified, using default value 9999");
	}

	if (!itsConfiguration->GetValue("target_param").empty())
	{
		itsTargetParam = itsConfiguration->GetValue("target_param");
	}
	else
	{
		throw runtime_error("Transformer_plugin: target_param not specified.");
	}

	if (!itsConfiguration->GetValue("source_param").empty())
	{
		itsSourceParam = itsConfiguration->GetValue("source_param");
	}
	else
	{
		itsSourceParam = itsTargetParam;
		itsLogger->Warning("Source_param not specified, source_param set to target_param");
	}

	if (!itsConfiguration->GetValue("source_level_type").empty())
	{
		itsSourceLevelType = itsConfiguration->GetValue("source_level_type");
	}
	else
	{
		itsLogger->Warning("Source_level not specified, source_level set to target level");
	}

	if (!itsConfiguration->GetValue("source_levels").empty())
	{
		SourceLevels = itsConfiguration->GetValue("source_levels");
	}
	else
	{
		itsLogger->Warning("Source_levels not specified, source_levels set to target levels");
	}

	// Check apply land sea mask parameter

	if (itsConfiguration->Exists("apply_landsea_mask") && itsConfiguration->GetValue("apply_landsea_mask") == "true")
	{
		itsApplyLandSeaMask = true;

		// Check for optional threshold parameter
		if (itsConfiguration->Exists("landsea_mask_threshold"))
		{
			itsLandSeaMaskThreshold = boost::lexical_cast<double>(itsConfiguration->GetValue("landsea_mask_threshold"));
		}
	}

	if (itsConfiguration->Exists("interpolation"))
	{
		itsInterpolationMethod = HPStringToInterpolationMethod.at(itsConfiguration->GetValue("interpolation"));
	}

	if (!SourceLevels.empty())
	{
		// looks useful to use this function to create source_levels

		itsSourceLevels = LevelsFromString(itsSourceLevelType, SourceLevels);
	}
	else
	{
		// copy levels from target
		auto x = make_shared<info> (*itsInfo);
		for (x->ResetLevel(); x->NextLevel();)
		{
			itsSourceLevels.push_back(x->Level());
		}
	}
}

void playground::Process(std::shared_ptr<const plugin_configuration> conf)
{
	Init(conf);
	SetAdditionalParameters();

	/*
	 * Set target parameter to T
	 * - name T-C
	 * - univ_id 4
	 * - grib2 descriptor 0'00'000
	 *
	 * We need to specify grib and querydata parameter information
	 * since we don't know which one will be the output format.
	 *
	 */

	vector<param> theParams;

	param requestedParam(itsTargetParam, itsTargetUnivID);

	// GRIB 2
	if (itsConfiguration->OutputFileType() == kGRIB2)
	{
		if (!itsConfiguration->GetValue("grib_discipline").empty() &&
		    !itsConfiguration->GetValue("grib_category").empty() &&
		    !itsConfiguration->GetValue("grib_parameter").empty())
		{
			requestedParam.GribDiscipline(boost::lexical_cast<int>(itsConfiguration->GetValue("grib_discipline")));
			requestedParam.GribCategory(boost::lexical_cast<int>(itsConfiguration->GetValue("grib_category")));
			requestedParam.GribParameter(boost::lexical_cast<int>(itsConfiguration->GetValue("grib_parameter")));
		}
	}

	if (itsInterpolationMethod != kUnknownInterpolationMethod)
	{
		requestedParam.InterpolationMethod(itsInterpolationMethod);
	}

	theParams.push_back(requestedParam);

	SetParams(theParams);

	Start();
}

/*
 * Calculate()
 *
 * This function does the actual calculation.
 */

void playground::Calculate(shared_ptr<info> myTargetInfo, unsigned short threadIndex)
{
	// Required source parameter

	param InputParam(itsSourceParam);

	auto myThreadedLogger =
	    logger_factory::Instance()->GetLog("playgroundThread #" + boost::lexical_cast<string>(threadIndex));

	forecast_time forecastTime = myTargetInfo->Time();
	level forecastLevel = myTargetInfo->Level();
	forecast_type forecastType = myTargetInfo->ForecastType();

	myThreadedLogger->Info("Calculating time " + static_cast<string>(forecastTime.ValidDateTime()) + " level " +
	                       static_cast<string>(forecastLevel));

	auto f = GET_PLUGIN(fetcher);

	if (itsApplyLandSeaMask)
	{
		f->ApplyLandSeaMask(true);
		f->LandSeaMaskThreshold(itsLandSeaMaskThreshold);
	}

	info_t sourceInfo;

	try
	{
		sourceInfo = f->Fetch(itsConfiguration, forecastTime, itsSourceLevels[myTargetInfo->LevelIndex()], InputParam,
		                      forecastType, itsConfiguration->UseCudaForPacking());
	}
	catch (HPExceptionType& e)
	{
		myThreadedLogger->Warning("Skipping step " + boost::lexical_cast<string>(forecastTime.Step()) + ", level " +
		                          static_cast<string>(forecastLevel));
		return;
	}

	SetAB(myTargetInfo, sourceInfo);

	bool levelOnly = (itsScale == 1.0 && itsBase == 0.0);

	string deviceType;

#ifdef HAVE_CUDA

	if (itsConfiguration->UseCuda() && !levelOnly)
	{
		deviceType = "GPU";

		auto opts = CudaPrepare(myTargetInfo, sourceInfo);

		playground_cuda::Process(*opts);
	}
	else
#endif
	{
		deviceType = "CPU";
        auto h = GET_PLUGIN(hitool);
        h->Configuration(itsConfiguration);
        h->Time(myTargetInfo->Time());
        h->ForecastType(myTargetInfo->ForecastType());
        h->HeightUnit(kM);

clock_t t;
t = clock();
        auto levelsForMaxHeight = h->LevelForHeight(producer(131), 1000.0);
        auto levelsForMinHeight = h->LevelForHeight(producer(240), 1000.0);

	level_series ParmLs(itsConfiguration,InputParam,forecastTime,levelsForMaxHeight.second,levelsForMinHeight.first,1.0);
        level_series HLs(itsConfiguration,param("HL-M"),forecastTime,levelsForMaxHeight.second,levelsForMinHeight.first,1.0);
	myTargetInfo->Data().Set(VEC(Value(ParmLs,HLs,1000.0)));
cout << float(clock() - t)/CLOCKS_PER_SEC << "s\n";

/*
	auto data = h->VerticalValue(param("T-K"),1000.0);
	myTargetInfo->Data().Set(data);
cout << float(clock() - t)/CLOCKS_PER_SEC << "s\n";*/

	/*	LOCKSTEP(myTargetInfo, sourceInfo)
		{
			double value = sourceInfo->Value();

			if (value == kFloatMissing)
			{
				continue;
			}

			if (!levelOnly)
			{
				double newValue = value * itsScale + itsBase;

				myTargetInfo->Value(newValue);
			}
			else
			{
				myTargetInfo->Value(value);
			}
		}*/
	}

	myThreadedLogger->Info("[" + deviceType + "] Missing values: " +
	                       boost::lexical_cast<string>(myTargetInfo->Data().MissingCount()) + "/" +
	                       boost::lexical_cast<string>(myTargetInfo->Data().Size()));
}

#ifdef HAVE_CUDA

unique_ptr<playground_cuda::options> playground::CudaPrepare(shared_ptr<info> myTargetInfo,
                                                               shared_ptr<info> sourceInfo)
{
	unique_ptr<playground_cuda::options> opts(new playground_cuda::options);

	opts->N = sourceInfo->Data().Size();

	opts->base = itsBase;
	opts->scale = itsScale;

	opts->source = sourceInfo->ToSimple();
	opts->dest = myTargetInfo->ToSimple();

	return opts;
}
#endif
