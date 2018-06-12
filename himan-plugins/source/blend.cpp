#include "blend.h"
#include "fetcher.h"
#include "plugin_factory.h"
#include "radon.h"
#include "util.h"
#include "writer.h"

#include <future>
#include <mutex>
#include <thread>
#include <algorithm>

//
// Forecast blender
//

namespace himan
{
namespace plugin
{
using namespace std;

static mutex singleFileWriteMutex;

static const string kClassName = "himan::plugin::blend";

// 'decaying factor' for bias and mae
static const double alpha = 0.05;

static const producer kLapsProd(109, 86, 109, "LAPSSCAN");
static const string kLapsGeom = "LAPSSCANLARGE";
static const forecast_type kLapsFtype(kAnalysis);

static const producer kBlendWeightProd(182, 86, 182, "BLENDW");
static const producer kBlendRawProd(183, 86, 183, "BLENDR");
static const producer kBlendBiasProd(184, 86, 184, "BLENDB");

// Each blend producer is composed of these original producers. We use forecast_types to distinguish them
// from each other, and this way we don't have to create bunch of extra producers. But still, it is a hack.
static const forecast_type kMosFtype(kEpsPerturbation, 1.0);
static const forecast_type kEcmwfFtype(kEpsPerturbation, 2.0);
static const forecast_type kHirlamFtype(kEpsPerturbation, 3.0);
static const forecast_type kMepsFtype(kEpsPerturbation, 4.0);
static const forecast_type kGfsFtype(kEpsPerturbation, 5.0);

// When adjusting origin times, we need to check that the resulting time is compatible with the model's
// (used) forecast length.
const int kMosForecastLength = 192;
const int kEcmwfForecastLength = 192;
const int kHirlamForecastLength = 54;
const int kMepsForecastLength = 66;
const int kGfsForecastLength = 192;

blend::blend() : itsCalculationMode(kCalculateNone), itsNumHours(0), itsProducer(), itsProdFtype()
{
	itsLogger = logger("blend");
}

blend::~blend()
{
}

static info_t FetchWithProperties(shared_ptr<plugin_configuration> cnf, const forecast_time& forecastTime,
                                  HPTimeResolution stepResolution, const level& lvl, const param& parm,
                                  const forecast_type& type, const string& geom, const producer& prod)
{
	auto f = GET_PLUGIN(fetcher);

	forecast_time ftime = forecastTime;
	ftime.StepResolution(stepResolution);

	cnf->SourceGeomNames({geom});
	cnf->SourceProducers({prod});

	try
	{
		info_t ret = f->Fetch(cnf, ftime, lvl, parm, type, false);
		return ret;
	}
	catch (HPExceptionType& e)
	{
		if (e != kFileDataNotFound)
		{
			himan::Abort();
		}
		else
		{
			return nullptr;
		}
	}
}

static info_t FetchProd(shared_ptr<plugin_configuration> cnf, const forecast_time& forecastTime,
                        HPTimeResolution stepResolution, const level& lvl, const param& parm, const forecast_type& type,
                        const producer& prod)
{
	auto f = GET_PLUGIN(fetcher);

	const string geomName = cnf->TargetGeomName();

	forecast_time ftime = forecastTime;
	ftime.StepResolution(stepResolution);

	cnf->SourceGeomNames({geomName});
	cnf->SourceProducers({prod});

	return f->Fetch(cnf, ftime, lvl, parm, type, false);
}

static info_t FetchNoExcept(shared_ptr<plugin_configuration> cnf, const forecast_time& forecastTime,
                            HPTimeResolution stepResolution, const level& lvl, const param& parm,
                            const forecast_type& type, const producer& prod)
{
	auto f = GET_PLUGIN(fetcher);

	const string geomName = cnf->TargetGeomName();

	forecast_time ftime = forecastTime;
	ftime.StepResolution(stepResolution);

	cnf->SourceGeomNames({geomName});
	cnf->SourceProducers({prod});

	info_t I;
	try
	{
		I = f->Fetch(cnf, ftime, lvl, parm, type, false);
	}
	catch (HPExceptionType& e)
	{
		if (e != kFileDataNotFound)
		{
			himan::Abort();
		}
		else
		{
			return nullptr;
		}
	}
	return I;
}

static mutex dimensionMutex;

void blend::Run(unsigned short threadIndex)
{
	// Iterate through timesteps
	auto targetInfo = make_shared<info>(*itsInfo);

	while (itsDimensionsRemaining)
	{
		{
			lock_guard<mutex> lock(dimensionMutex);

			if (itsInfo->NextTime())
			{
				if (!targetInfo->Time(itsInfo->Time()))
				{
					throw std::runtime_error("invalid target time");
				}
			}
			else
			{
				itsDimensionsRemaining = false;
				break;
			}
		}

		Calculate(targetInfo, threadIndex);

		if (itsConfiguration->StatisticsEnabled())
		{
			// NOTE this is likely wrong. (Only counts the current iterator position's missing count.)
			itsConfiguration->Statistics()->AddToMissingCount(targetInfo->Data().MissingCount());
			itsConfiguration->Statistics()->AddToValueCount(targetInfo->Data().Size());
		}

		// NOTE: Each Calculate-function will call WriteToFile manually, since each of them have different requirements
		// for its behavior.
	}
}

raw_time blend::LatestOriginTimeForProducer(const string& producer) const
{
	int producerId = -1;
	string geom;
	if (producer == "MOS")
	{
		producerId = 120;
		geom = "MOSKRIGING2";
	}
	else if (producer == "ECG")
	{
		producerId = 131;
		geom = "ECGLO0100";
	}
	else if (producer == "HL2")
	{
		producerId = 1;
		geom = "RCR068";
	}
	else if (producer == "MEPS")
	{
		producerId = 4;
		geom = "MEPSSCAN2500";
	}
	else if (producer == "GFS")
	{
		producerId = 53;
		geom = "GFS0250";
	}
	else
	{
		itsLogger.Error("Invalid producer string: " + producer);
		himan::Abort();
	}

	auto r = GET_PLUGIN(radon);

	const string latest = r->RadonDB().GetLatestTime(producerId, geom, 0);
	if (latest.empty())
	{
		itsLogger.Error("Failed to find latest time for producer: " + to_string(producerId) + " and geom: " + geom +
		                " from Radon");
		himan::Abort();
	}

	raw_time raw = (latest);

	// With ECMWF and MOS we only want 00 and 12 times
	const int hour = stoi(raw.String("%H"));
	if (producer == "ECG" || producer == "MOS")
	{
		if (hour == 6 || hour == 18)
		{
			raw.Adjust(kHourResolution, -6);
		}
	}

	return raw;
}

void blend::Start()
{
	itsThreadCount = static_cast<short>(itsInfo->SizeTimes());

	vector<thread> threads;
	threads.reserve(itsThreadCount);

	itsInfo->Reset();
	itsInfo->FirstForecastType();
	itsInfo->FirstLevel();
	itsInfo->FirstParam();

	for (unsigned short i = 0; i < itsThreadCount; i++)
	{
		threads.push_back(move(thread(&blend::Run, this, i)));
	}

	for (auto&& t : threads)
	{
		t.join();
	}

	Finish();
}

void blend::Process(shared_ptr<const plugin_configuration> conf)
{
	Init(conf);
	vector<param> params;

	const string mode = conf->Exists("mode") ? conf->GetValue("mode") : "";
	if (mode.empty())
	{
		throw std::runtime_error(ClassName() + ": blender 'mode' not specified");
	}

	if (mode == "blend")
	{
		itsCalculationMode = kCalculateBlend;
	}
	else if (mode == "mae")
	{
		itsCalculationMode = kCalculateMAE;
	}
	else if (mode == "bias")
	{
		itsCalculationMode = kCalculateBias;
	}
	else
	{
		throw std::runtime_error(ClassName() + ": invalid blender 'mode' specified");
	}

	const string hours = conf->Exists("hours") ? conf->GetValue("hours") : "";
	if ((itsCalculationMode == kCalculateBias || itsCalculationMode == kCalculateMAE) && hours.empty())
	{
		throw std::runtime_error(ClassName() + ": number of previous hours ('hours') for calculation not specified");
	}
	if (itsCalculationMode == kCalculateBias || itsCalculationMode == kCalculateMAE)
	{
		itsNumHours = stoi(hours);
	}

    // Producer for bias and mae calculation
	const string prod = conf->Exists("producer") ? conf->GetValue("producer") : "";
	if ((itsCalculationMode == kCalculateBias || itsCalculationMode == kCalculateMAE) && prod.empty())
	{
		throw std::runtime_error(ClassName() + ": bias calculation data producer ('producer') not defined");
	}

	if (itsCalculationMode == kCalculateBias || itsCalculationMode == kCalculateMAE)
	{
		itsProducer = prod;
		if (prod == "ECG")
		{
			itsProdFtype = kEcmwfFtype;
		}
		else if (prod == "HL2")
		{
			itsProdFtype = kHirlamFtype;
		}
		else if (prod == "MEPS")
		{
			itsProdFtype = kMepsFtype;
		}
		else if (prod == "GFS")
		{
			itsProdFtype = kGfsFtype;
		}
        else if (prod == "MOS")
        {
            itsProdFtype = kMosFtype;
        }
		else
		{
			throw std::runtime_error(ClassName() + ": unknown producer for bias/mae calculation");
		}
	}

	// Each parameter has to be processed with a separate process queue invocation.
	const string p = conf->Exists("param") ? conf->GetValue("param") : "";
	if (p.empty())
	{
		throw std::runtime_error(ClassName() + ": parameter not specified");
	}
	params.push_back(param(p));

	PrimaryDimension(kTimeDimension);
	SetParams(params);
	Start();
}

void blend::Calculate(shared_ptr<info> targetInfo, unsigned short threadIndex)
{
	auto f = GET_PLUGIN(fetcher);
	auto log = logger("blendThread#" + to_string(threadIndex));
	const string deviceType = "CPU";

	forecast_time currentTime = targetInfo->Time();
	const param currentParam = targetInfo->Param();

	switch (itsCalculationMode)
	{
		case kCalculateBlend:
			log.Info("Calculating blend for " + currentParam.Name() + " " +
			         static_cast<string>(currentTime.ValidDateTime()));
			CalculateBlend(targetInfo, threadIndex);
			break;
		case kCalculateMAE:
			log.Info("Calculating weights for " + currentParam.Name() + " " +
			         static_cast<string>(currentTime.ValidDateTime()));
			CalculateMember(targetInfo, threadIndex, kCalculateMAE);
			break;
		case kCalculateBias:
			log.Info("Calculating bias corrected grids for " + currentParam.Name() + " " +
			         static_cast<string>(currentTime.ValidDateTime()));
			CalculateMember(targetInfo, threadIndex, kCalculateBias);
			break;
		default:
			log.Error("Invalid calculation mode");
			himan::Abort();
	}
}

static forecast_time CalculateAnalysisFetchTime(const forecast_time& currentTime)
{
	const int validHour = stoi(currentTime.ValidDateTime().String("%H"));
	const int validDay = stoi(currentTime.ValidDateTime().String("%d"));
	const int originHour = stoi(currentTime.OriginDateTime().String("%H"));
	const int originDay = stoi(currentTime.OriginDateTime().String("%d"));

	forecast_time analysisFetchTime = currentTime;

	analysisFetchTime.ValidDateTime().Adjust(kHourResolution, -validHour); // set to 00

	int ndays = validDay - originDay;
	/// This is how much to adjust from 00
	if (ndays > 0)
	{
		int nhours = originHour - (24 - validHour) + (validHour - originHour);
		analysisFetchTime.ValidDateTime().Adjust(kHourResolution, nhours);
	}
	else
	{
		int nhours = abs(originHour - validHour);
		analysisFetchTime.ValidDateTime().Adjust(kHourResolution, nhours);
	}

	analysisFetchTime.OriginDateTime() = analysisFetchTime.ValidDateTime();

	return analysisFetchTime;
}

matrix<double> blend::CalculateBias(logger& log, shared_ptr<info> targetInfo, const forecast_type& ftype,
                                    const level& lvl, const forecast_time& calcTime, int originTimeStep)
{
	shared_ptr<plugin_configuration> cnf = make_shared<plugin_configuration>(*itsConfiguration);

	param currentParam = targetInfo->Param();
	level currentLevel = targetInfo->Level();
	forecast_time currentTime = targetInfo->Time();
	forecast_time analysisFetchTime = CalculateAnalysisFetchTime(currentTime);

	HPTimeResolution currentRes = currentTime.StepResolution();

	log.Info("Calculation origin time: " + calcTime.OriginDateTime().String() +
	         " valid time: " + calcTime.ValidDateTime().String());

	info_t analysis = FetchWithProperties(cnf, analysisFetchTime, currentRes, level(kHeight, 2.0), currentParam,
	                                      kLapsFtype, kLapsGeom, kLapsProd);
	if (!analysis)
	{
		log.Error("Analysis (LAPS) data not found");
		himan::Abort();
	}

	matrix<double> currentBias(targetInfo->Data().SizeX(), targetInfo->Data().SizeY(), targetInfo->Data().SizeZ(),
	                           MissingDouble());

	forecast_time leadTime(calcTime);
	info_t Info = FetchProd(cnf, leadTime, currentRes, lvl, currentParam, ftype, kBlendRawProd);

	// Previous forecast's bias corrected data is optional. If the data is not found we'll set the grid to missing.
	// (This happens, for example, during initialization.)
	forecast_time prevLeadTime(leadTime);
	prevLeadTime.OriginDateTime().Adjust(currentRes, -originTimeStep);

	vector<double> prevBias;
	try
	{
		info_t temp = FetchProd(cnf, prevLeadTime, currentRes, lvl, currentParam, ftype, kBlendBiasProd);
		prevBias = VEC(temp);
	}
	catch (HPExceptionType& e)
	{
		if (e == kFileDataNotFound)
		{
			// There's no benefit to setting the grid to 0, since we might get a valid grid and this might contain
			// missing values. We still have to check for missing values in the calculation loop.
			prevBias = vector<double>(targetInfo->Data().Size(), MissingDouble());
		}
		else
		{
			throw;
		}
	}

	// Introduce shorter names for clarity
	const vector<double>& O = VEC(analysis);
	const vector<double>& F = VEC(Info);
	const vector<double>& BC = prevBias;
	vector<double>& B = currentBias.Values();

	for (size_t i = 0; i < F.size(); i++)
	{
		double f = F[i];
		double o = O[i];
		double bc = BC[i];

		if (IsMissing(bc))
		{
			bc = 0.0;
		}

		if (IsMissing(f) || IsMissing(o))
		{
			f = 0.0;
			o = 0.0;
		}
		B[i] = (1.0 - alpha) * bc + alpha * (f - o);
	}

	return currentBias;
}

matrix<double> blend::CalculateMAE(logger& log, shared_ptr<info> targetInfo, const forecast_type& ftype,
                                   const level& lvl, const forecast_time& calcTime, int originTimeStep)
{
	shared_ptr<plugin_configuration> cnf = make_shared<plugin_configuration>(*itsConfiguration);

	param currentParam = targetInfo->Param();
	level currentLevel = targetInfo->Level();
	forecast_time currentTime = targetInfo->Time();
	forecast_time analysisFetchTime = CalculateAnalysisFetchTime(currentTime);

	HPTimeResolution currentRes = currentTime.StepResolution();

	log.Info("Calculation origin time: " + calcTime.OriginDateTime().String() +
	         " valid time: " + calcTime.ValidDateTime().String());

	info_t analysis = FetchWithProperties(cnf, analysisFetchTime, currentRes, level(kHeight, 2.0), currentParam,
	                                      kLapsFtype, kLapsGeom, kLapsProd);
	if (!analysis)
	{
		log.Error("Analysis (LAPS) data not found");
		himan::Abort();
	}

	matrix<double> MAE(targetInfo->Data().SizeX(), targetInfo->Data().SizeY(), targetInfo->Data().SizeZ(),
	                   MissingDouble());

	forecast_time leadTime(calcTime);
	forecast_time prevLeadTime(leadTime);
	prevLeadTime.OriginDateTime().Adjust(currentRes, -originTimeStep);

	info_t bias = FetchProd(cnf, leadTime, currentRes, lvl, currentParam, ftype, kBlendBiasProd);
	info_t forecast = FetchProd(cnf, leadTime, currentRes, lvl, currentParam, ftype, kBlendRawProd);

	vector<double> prevMAE;
	try
	{
		info_t temp = FetchProd(cnf, prevLeadTime, currentRes, lvl, currentParam, ftype, kBlendWeightProd);
		prevMAE = VEC(temp);
	}
	catch (HPExceptionType& e)
	{
		if (e == kFileDataNotFound)
		{
			prevMAE = vector<double>(targetInfo->Data().Size(), MissingDouble());
		}
		else
		{
			throw;
		}
	}

	const vector<double>& O = VEC(analysis);
	const vector<double>& B = VEC(bias);
	const vector<double>& F = VEC(forecast);
	vector<double>& mae = MAE.Values();

	for (size_t i = 0; i < mae.size(); i++)
	{
		double o = O[i];
		double f = F[i];
        double b = B[i];
		double _prevMAE = prevMAE[i];

        if (IsMissing(f) || IsMissing(o))
        {
            f = 0.0;
            o = 0.0;
        }

        if (IsMissing(_prevMAE))
        {
            _prevMAE = 0.0;
        }

        if (IsMissing(b))
        {
            b = 0.0;
        }

        const double bcf = f - b;
        mae[i] = (1.0 - alpha) * _prevMAE + alpha * std::abs(bcf - o);
	}

	return MAE;
}

static bool TimeAdjustmentRequired(const forecast_time& current, const forecast_time& calc)
{
	// NOTE Assume that minutes and seconds are '00'
	const int currentHour = stoi(current.OriginDateTime().String("%H"));
	const int calcHour = stoi(calc.OriginDateTime().String("%H"));

	if (currentHour == 0 || currentHour == 6 || currentHour == 12 || currentHour == 18)
	{
		if (currentHour >= calcHour)
		{
			return true;
		}
	}
	return false;
}

void blend::CalculateMember(shared_ptr<info> targetInfo, unsigned short threadIdx, blend_mode mode)
{
	logger log;
	if (mode == kCalculateBias)
	{
		log = logger("calculateBias#" + to_string(threadIdx));
	}
	else
	{
		log = logger("calculateMAE#" + to_string(threadIdx));
	}

	forecast_type forecastType = itsProdFtype;
	level targetLevel = targetInfo->Level();
	forecast_time current = targetInfo->Time();

	raw_time latestOrigin = LatestOriginTimeForProducer(itsProducer);
	log.Info("Latest origin time for producer: " + latestOrigin.String());

	// Used for fetching raw model output, bias, and weight for models.
	forecast_time ftime(latestOrigin, current.ValidDateTime());

	int maxStep = 0;
	int originTimeStep;
	if (itsProdFtype == kMosFtype)
	{
		maxStep = kMosForecastLength;
		originTimeStep = 12; // only steps 00 and 12
	}
	else if (itsProdFtype == kEcmwfFtype)
	{
		maxStep = kEcmwfForecastLength;
		originTimeStep = 12; // forecast length of runs 06 and 18 cause problems, so skip them
	}
	else if (itsProdFtype == kHirlamFtype)
	{
		maxStep = kHirlamForecastLength;
		originTimeStep = 6;
	}
	else if (itsProdFtype == kMepsFtype)
	{
		maxStep = kMepsForecastLength;
		originTimeStep = 6;
	}
	else if (itsProdFtype == kGfsFtype)
	{
		maxStep = kGfsForecastLength;
		originTimeStep = 6;
	}
	else
	{
		log.Error("Invalid producer forecast type");
		himan::Abort();
	}

	if (TimeAdjustmentRequired(current, ftime))
	{
		// Analysis updates more frequently than models.
		// So if analysis time is 00, 06, 12, 18, then adjust the origin time for DMO fetching by -originTimeStep (valid time stays
		// the same).
		log.Info("Adjusting ftime");
		ftime.OriginDateTime().Adjust(kHourResolution, -originTimeStep);
	}
	else
	{
		log.Info("Not adjusting ftime");
	}

	ftime.OriginDateTime().Adjust(kHourResolution, -itsNumHours);

	// Check that we're not overstepping the forecast length.
	while (ftime.Step() > maxStep)
	{
		ftime.OriginDateTime().Adjust(kHourResolution, originTimeStep);
	}

	// Problem: targetInfo has information for the data that we want to fetch, but because of the convoluted way of
	// calculating everything, this doesn't match with the data we want to write out.
	// Solution: Create a new info and write that out.
	info_t Info = make_shared<info>(*targetInfo);
	vector<forecast_type> ftypes{itsProdFtype};

	if (mode == kCalculateBias)
	{
		Info->Producer(kBlendBiasProd);
	}
	else
	{
		Info->Producer(kBlendWeightProd);
	}

	SetupOutputForecastTimes(Info, latestOrigin, current, maxStep, originTimeStep);
	Info->ForecastTypes(ftypes);
	Info->Create(targetInfo->Grid(), true);
	Info->First();

	while (true)
	{
		/// LAPS fetching is done via forecast time in |targetInfo|. Each call to Calculate{Bias,MAE} calculates for one
		/// time step.
		/// So we want to adjust ftime based on time.

		// Newest forecast
		if (ftime.Step() < 0)
		{
			log.Trace("End of forecast, breaking");
			break;
		}

		if (ftime.OriginDateTime() >= current.OriginDateTime() || ftime.OriginDateTime() > latestOrigin)
		{
			break;
		}

		matrix<double> d;
		if (mode == kCalculateBias)
		{
			d = CalculateBias(log, targetInfo, forecastType, targetLevel, ftime, originTimeStep);
		}
		else
		{
			d = CalculateMAE(log, targetInfo, forecastType, targetLevel, ftime, originTimeStep);
		}

		if (Info->ForecastType(forecastType))
		{
			Info->Grid()->Data(d);
			WriteToFile(Info);
		}
		else
		{
			log.Error("Failed to set the correct output forecast type");
			himan::Abort();
		}

		Info->NextTime();

		ftime.OriginDateTime().Adjust(kHourResolution, originTimeStep);
	}
}

static std::vector<info_t> FetchRawGrids(shared_ptr<info> targetInfo, shared_ptr<plugin_configuration> cnf)
{
	auto f = GET_PLUGIN(fetcher);
	auto log = logger("calculateBlend_FetchRawGrids");

	forecast_time currentTime = targetInfo->Time();
	HPTimeResolution currentResolution = currentTime.StepResolution();
	const param currentParam = targetInfo->Param();

	// We want to return nullptrs here so that we can skip over these entries in the Calculate-loop.
	info_t mosRaw =
	    FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 0.0), currentParam, kMosFtype, kBlendRawProd);
	info_t ecRaw = FetchNoExcept(cnf, currentTime, currentResolution, level(kGround, 0.0), currentParam, kEcmwfFtype,
	                             kBlendRawProd);
	info_t mepsRaw = FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 2.0), currentParam, kMepsFtype,
	                               kBlendRawProd);
	info_t hirlamRaw = FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 2.0), currentParam,
	                                 kHirlamFtype, kBlendRawProd);
	info_t gfsRaw =
	    FetchNoExcept(cnf, currentTime, currentResolution, level(kGround, 0.0), currentParam, kGfsFtype, kBlendRawProd);

	if (mosRaw)
		log.Info("MOS_raw missing: " + to_string(mosRaw->Data().MissingCount()));
	if (ecRaw)
		log.Info("EC_raw missing: " + to_string(ecRaw->Data().MissingCount()));
	if (mepsRaw)
		log.Info("MEPS_raw missing: " + to_string(mepsRaw->Data().MissingCount()));
	if (hirlamRaw)
		log.Info("HIRLAM_raw missing: " + to_string(hirlamRaw->Data().MissingCount()));
	if (gfsRaw)
		log.Info("GFS_raw missing: " + to_string(gfsRaw->Data().MissingCount()));

	return std::vector<info_t>{mosRaw, ecRaw, mepsRaw, hirlamRaw, gfsRaw};
}

static std::vector<info_t> FetchBiasGrids(shared_ptr<info> targetInfo, shared_ptr<plugin_configuration> cnf)
{
	auto f = GET_PLUGIN(fetcher);
	auto log = logger("calculateBlend_FetchBiasGrids");

	forecast_time currentTime = targetInfo->Time();
	HPTimeResolution currentResolution = currentTime.StepResolution();
	const param currentParam = targetInfo->Param();

	info_t mosBias = FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 0.0), currentParam, kMosFtype,
	                               kBlendBiasProd);
	info_t ecBias = FetchNoExcept(cnf, currentTime, currentResolution, level(kGround, 0.0), currentParam, kEcmwfFtype,
	                              kBlendBiasProd);
	info_t mepsBias = FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 2.0), currentParam, kMepsFtype,
	                                kBlendBiasProd);
	info_t hirlamBias = FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 2.0), currentParam,
	                                  kHirlamFtype, kBlendBiasProd);
	info_t gfsBias = FetchNoExcept(cnf, currentTime, currentResolution, level(kGround, 0.0), currentParam, kEcmwfFtype,
	                               kBlendBiasProd);

	if (mosBias)
		log.Info("MOS_bias missing: " + to_string(mosBias->Data().MissingCount()));
	if (ecBias)
		log.Info("EC_bias missing: " + to_string(ecBias->Data().MissingCount()));
	if (mepsBias)
		log.Info("MEPS_bias missing: " + to_string(mepsBias->Data().MissingCount()));
	if (hirlamBias)
		log.Info("HIRLAM_bias missing: " + to_string(hirlamBias->Data().MissingCount()));
	if (gfsBias)
		log.Info("GFS_bias missing: " + to_string(gfsBias->Data().MissingCount()));

	return std::vector<info_t>{mosBias, ecBias, mepsBias, hirlamBias, gfsBias};
}

static std::vector<info_t> FetchMAEGrids(shared_ptr<info> targetInfo, shared_ptr<plugin_configuration> cnf)
{
	auto f = GET_PLUGIN(fetcher);
	auto log = logger("calculateBlend_FetchMAEGrids");

	forecast_time currentTime = targetInfo->Time();
	HPTimeResolution currentResolution = currentTime.StepResolution();
	const param currentParam = targetInfo->Param();

	info_t mos = FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 0.0), currentParam, kMosFtype,
	                           kBlendWeightProd);
	info_t ec = FetchNoExcept(cnf, currentTime, currentResolution, level(kGround, 0.0), currentParam, kEcmwfFtype,
	                          kBlendWeightProd);
	info_t meps = FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 2.0), currentParam, kMepsFtype,
	                            kBlendWeightProd);
	info_t hirlam = FetchNoExcept(cnf, currentTime, currentResolution, level(kHeight, 2.0), currentParam, kHirlamFtype,
	                              kBlendWeightProd);
	info_t gfs = FetchNoExcept(cnf, currentTime, currentResolution, level(kGround, 0.0), currentParam, kGfsFtype,
	                           kBlendWeightProd);

	if (mos)
		log.Info("MOS_mae missing: " + to_string(mos->Data().MissingCount()));
	if (ec)
		log.Info("EC_mae missing: " + to_string(ec->Data().MissingCount()));
	if (meps)
		log.Info("MEPS_mae missing: " + to_string(meps->Data().MissingCount()));
	if (hirlam)
		log.Info("HIRLAM_mae missing: " + to_string(hirlam->Data().MissingCount()));
	if (gfs)
		log.Info("GFS_mae missing: " + to_string(gfs->Data().MissingCount()));

	return std::vector<info_t>{mos, ec, meps, hirlam, gfs};
}

void blend::CalculateBlend(shared_ptr<info> targetInfo, unsigned short threadIdx)
{
	auto f = GET_PLUGIN(fetcher);
	auto log = logger("calculateBlend#" + to_string(threadIdx));
	const string deviceType = "CPU";

	forecast_time currentTime = targetInfo->Time();
	const level currentLevel = targetInfo->Level();
	const param currentParam = targetInfo->Param();

	log.Info("Blending " + currentParam.Name() + " " + static_cast<string>(currentTime.ValidDateTime()));

	// NOTE: If one of the grids is missing, we should still put an empty grid there. Since all the stages assume that
	// F[i], B[i], W[i] are all of the same model! This means that the ordering is fixed for the return vector of
	// FetchRawGrids, FetchBiasGrids, FetchMAEGrids.
	vector<info_t> forecasts = FetchRawGrids(targetInfo, make_shared<plugin_configuration>(*itsConfiguration));
	if (std::all_of(forecasts.begin(), forecasts.end(), [&](info_t i) { return i == nullptr; }))
	{
		log.Error("Failed to acquire any source data");
		himan::Abort();
	}

	// First reset all the locations for the upcoming loop.
	for (const auto& f : forecasts)
	{
		if (f)
		{
			f->ResetLocation();
		}
	}

	vector<info_t> biases = FetchBiasGrids(targetInfo, make_shared<plugin_configuration>(*itsConfiguration));
	if (std::all_of(biases.begin(), biases.end(), [&](info_t i) { return i == nullptr; }))
	{
		log.Error("Failed to acquire any bias grids");
		himan::Abort();
	}

	for (const auto& b : biases)
	{
		if (b)
		{
			b->ResetLocation();
		}
	}

	// Load all the precalculated weights from BLENDW
	vector<info_t> weights = FetchMAEGrids(targetInfo, make_shared<plugin_configuration>(*itsConfiguration));
	if (std::all_of(weights.begin(), weights.end(), [&](info_t i) { return i == nullptr; }))
	{
		log.Error("Failed to acquire any weight grids");
		himan::Abort();
	}

	for (auto& w : weights)
	{
		if (w)
		{
			w->ResetLocation();
		}
    }

	// Used to collect values in the case of missing values.
	vector<double> collectedValues;
	collectedValues.reserve(forecasts.size());

	vector<double> collectedWeights;
	collectedWeights.reserve(forecasts.size());

	vector<double> collectedBiases;
	collectedBiases.reserve(forecasts.size());

	size_t forecastWarnings = 0;
	size_t biasWarnings = 0;
	size_t weightWarnings = 0;

	targetInfo->ResetLocation();
	while (targetInfo->NextLocation())
	{
		double F = 0.0;
		size_t numMissing = 0;

		for (const auto&& tup : zip_range(forecasts, weights, biases))
		{
			info_t f = tup.get<0>();
			info_t w = tup.get<1>();
			info_t b = tup.get<2>();

			if (!f || !f->NextLocation())
			{
				forecastWarnings++;
				continue;
			}

			if (!w || !w->NextLocation())
			{
				weightWarnings++;
				continue;
			}

			if (!b || !b->NextLocation())
			{
				biasWarnings++;
				continue;
			}

			const double v = f->Value();
			const double wv = w->Value();
			const double bb = b->Value();
			if (IsMissing(v) || IsMissing(wv) || IsMissing(bb))
			{
				numMissing++;
				continue;
			}

			collectedValues.push_back(v);
			collectedWeights.push_back(wv);
			collectedBiases.push_back(bb);
		}

		// Used for the actual application of the weights.
        vector<double> weights;
		weights.resize(collectedWeights.size());

        for (size_t i = 0; i < weights.size(); i++)
        {
            double sum = 0.0;
            for (const double& w : collectedWeights)
            {
				// currentWeights already pruned of missing values
                sum +=  1.0 / w; // could be zero
            }

            weights[i] = 1.0 / sum;
        }

		if (numMissing == forecasts.size())
		{
			F = MissingDouble();
		}
		else
		{
			double sw = 0.0;
			for (const auto& w : weights)
			{
				sw += w;
			}

			if (sw > 0.0)
			{
				// Finally apply the weights to the forecast values.
				for (const auto&& tup : zip_range(weights, collectedValues, collectedBiases))
				{
					const double w = tup.get<0>();
					const double v = tup.get<1>();
					const double b = tup.get<2>();
					F += w * (v - b);
				}

				F /= sw;
			}
			else
			{
				F = MissingDouble();
			}
		}

		targetInfo->Value(F);

		collectedValues.clear();
		collectedBiases.clear();
		collectedWeights.clear();
	}

	log.Warning("Failed to advance forecast iterator position " + to_string(forecastWarnings) + " times");
	log.Warning("Failed to advance bias iterator position " + to_string(biasWarnings) + " times");
	log.Warning("Failed to advance weight iterator position " + to_string(weightWarnings) + " times");

	log.Info("[" + deviceType + "] Missing values: " + to_string(targetInfo->Data().MissingCount()) + "/" +
	         to_string(targetInfo->Data().Size()));

	WriteToFile(targetInfo);
}

void blend::WriteToFile(const info_t targetInfo, write_options writeOptions)
{
	auto aWriter = GET_PLUGIN(writer);

	aWriter->WriteOptions(writeOptions);
	auto tempInfo = make_shared<info>(*targetInfo);

	if (itsConfiguration->FileWriteOption() == kDatabase || itsConfiguration->FileWriteOption() == kMultipleFiles)
	{
		aWriter->ToFile(tempInfo, itsConfiguration);
	}
	else
	{
		lock_guard<mutex> lock(singleFileWriteMutex);

		aWriter->ToFile(tempInfo, itsConfiguration, itsConfiguration->ConfigurationFile());
	}

	if (itsConfiguration->UseDynamicMemoryAllocation())
	{
		DeallocateMemory(*tempInfo);
	}
}

void blend::SetupOutputForecastTimes(shared_ptr<info> Info, const raw_time& latestOrigin, const forecast_time& current,
                                     int maxStep, int originTimeStep)
{
	vector<forecast_time> ftimes;

	Info->ResetTime();

	forecast_time ftime(latestOrigin, current.ValidDateTime());
	ftime.OriginDateTime().Adjust(kHourResolution, -itsNumHours);

	while (ftime.Step() > maxStep)
	{
		ftime.OriginDateTime().Adjust(kHourResolution, originTimeStep);
	}

	for (int i = 0; i < itsNumHours; i += originTimeStep)
	{
		ftimes.push_back(ftime);
		ftime.OriginDateTime().Adjust(kHourResolution, originTimeStep);
	}
	ftimes.push_back(ftime);

	Info->Times(ftimes);
}

}  // namespace plugin
}  // namespace himan
