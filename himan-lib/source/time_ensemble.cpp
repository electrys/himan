#include "time_ensemble.h"

#include "logger_factory.h"
#include "plugin_factory.h"

#define HIMAN_AUXILIARY_INCLUDE
#include "fetcher.h"
#undef HIMAN_AUXILIARY_INCLUDE

using namespace himan;
using namespace himan::plugin;

time_ensemble::time_ensemble(const param& parameter) : itsTimeSpan(kYearResolution)
{
	itsParam = parameter;
	itsExpectedEnsembleSize = 0;
	itsEnsembleType = kTimeEnsemble;

	itsLogger = std::unique_ptr<logger>(logger_factory::Instance()->GetLog("time_ensemble"));
}

time_ensemble::time_ensemble(const param& parameter, size_t expectedEnsembleSize, HPTimeResolution theTimeSpan)
    : itsTimeSpan(theTimeSpan)
{
	itsParam = parameter;
	itsExpectedEnsembleSize = expectedEnsembleSize;
	itsEnsembleType = kTimeEnsemble;

	itsLogger = std::unique_ptr<logger>(logger_factory::Instance()->GetLog("time_ensemble"));
}

void time_ensemble::Fetch(std::shared_ptr<const plugin_configuration> config, const forecast_time& time,
                          const level& forecastLevel)
{
	auto f = GET_PLUGIN(fetcher);

	try
	{
		forecast_time ftime(time);

		while (true)
		{
			if (itsExpectedEnsembleSize > 0 && itsForecasts.size() >= itsExpectedEnsembleSize)
			{
				break;
			}

			auto info = f->Fetch(config, ftime, forecastLevel, itsParam);

			itsForecasts.push_back(info);

			ftime.OriginDateTime().Adjust(itsTimeSpan, -1);
			ftime.ValidDateTime().Adjust(itsTimeSpan, -1);
		}
	}
	catch (HPExceptionType& e)
	{
		if (e == kFileDataNotFound)
		{
			if (itsExpectedEnsembleSize > 0 && itsForecasts.size() != itsExpectedEnsembleSize)
			{
				// NOTE let the plugin decide what to do with missing data
				throw;
			}
			else if (itsForecasts.size() == 0)
			{
				// no forecasts found
				throw;
			}
			else
			{
				itsExpectedEnsembleSize = itsForecasts.size();
			}
		}
	}

	assert(itsExpectedEnsembleSize == itsForecasts.size());

	itsLogger->Info("Read " + boost::lexical_cast<std::string>(itsForecasts.size()) + " different times to ensemble");
}
