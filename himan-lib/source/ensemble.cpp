//
// @file ensemble.cpp
//
// @date June 2, 2016
// @author vanhatam
//

#include "ensemble.h"
#include "fetcher.h"
#include "plugin_factory.h"

#include <stdint.h>
#include <stddef.h>

namespace himan
{

ensemble::ensemble(const param& parameter, size_t ensembleSize)
	: itsParam(parameter)
	, itsEnsembleSize(ensembleSize)
{
	// ensembleSize includes the control forecast
	itsPerturbations = std::vector<forecast_type> (ensembleSize - 1);
	itsForecasts = std::vector<info_t> (ensembleSize);

	int perturbationNumber = 1;
	for (auto & p : itsPerturbations)
	{
		p = forecast_type (kEpsPerturbation, static_cast<double>(perturbationNumber));
		perturbationNumber++;
	}
}

ensemble::~ensemble()
{
}

ensemble::ensemble(const ensemble& other)
	: itsParam(other.itsParam)
	, itsEnsembleSize(other.itsEnsembleSize)
	, itsPerturbations(other.itsPerturbations)
	, itsForecasts(other.itsForecasts)
{
}

ensemble& ensemble::operator=(const ensemble& other)
{
	itsParam = other.itsParam;
	itsEnsembleSize = other.itsEnsembleSize;
	itsPerturbations = other.itsPerturbations;
	itsForecasts = other.itsForecasts;
	return *this;
}

void ensemble::Fetch(std::shared_ptr<const plugin_configuration> config, const forecast_time& time, const level& forecastLevel)
{
	// NOTE should this be stored some where else? Every time you call Fetch(), the instantiation will happen
	auto f = GET_PLUGIN(fetcher);

	try
	{
		// First get the control forecast
		itsForecasts[0] = f->Fetch(config, time, forecastLevel, itsParam, forecast_type(kEpsControl, 0), false);

		// Then get the perturbations
		for (size_t i = 1; i < itsPerturbations.size() + 1; i++)
		{
			itsForecasts[i] = f->Fetch(config, time, forecastLevel, itsParam, itsPerturbations[i-1], false);
		}
	}
	catch (HPExceptionType& e)
	{
		if (e != kFileDataNotFound)
		{
			throw std::runtime_error("Ensemble: unable to proceed");
		} 
		else
		{
			// NOTE let the plugin decide what to do with missing data
			throw e;
		}
	}
}

void ensemble::ResetLocation()
{
	for (size_t i = 0; i < itsForecasts.size(); i++) 
	{
		assert(itsForecasts[i]);

		itsForecasts[i]->ResetLocation();
	}
}

bool ensemble::NextLocation()
{
	for (size_t i = 0; i < itsForecasts.size(); i++)
	{
		assert(itsForecasts[i]);

		if (!itsForecasts[i]->NextLocation())
		{
			return false;
		}
	}
	return true;
}

std::vector<double> ensemble::Values() const
{
	std::vector<double> ret (itsEnsembleSize);
	size_t i = 0;
	for (auto& f : itsForecasts)
	{
		ret[i] = f->Value();
		i++;
	}
	return ret;
}

std::vector<double> ensemble::SortedValues() const
{
	std::vector<double> v = Values();
	std::sort(v.begin(), v.end());
	return v;
}

} // namespace himan
