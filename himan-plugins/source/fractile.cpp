/**
 * @file fractile.cpp
 *
 * @date June, 2016
 * @author Tack
 **/

#include "fractile.h"

#include <iostream>
#include <algorithm>
#include <string>

#include <boost/thread.hpp>

#include "logger_factory.h"
#include "plugin_factory.h"

#include "json_parser.h"
#include "fetcher.h"
#include "ensemble.h"
#include "radon.h"

namespace himan
{

namespace plugin
{

fractile::fractile() : itsEnsembleSize(0)
{
    itsClearTextFormula = "%";
    itsCudaEnabledCalculation = false;
    itsLogger = logger_factory::Instance()->GetLog("fractile");
}

fractile::~fractile()
{
}

void fractile::Process(const std::shared_ptr<const plugin_configuration> conf)
{
    Init(conf);

    if(!itsConfiguration->GetValue("param").empty())
    {
        itsParamName = itsConfiguration->GetValue("param");
    }
    else
    {
        itsLogger->Error("Param not specified");
        return;
    }

    params calculatedParams;
    std::vector<std::string> fractiles = {"F0-","F10-","F25-","F50-","F75-","F90-","F100-"};

    for (const std::string& fractile : fractiles)
    {
		calculatedParams.push_back(param(fractile + itsParamName));
    }

    auto r = GET_PLUGIN(radon);
    std::string ensembleSizeStr = r->RadonDB().GetProducerMetaData(itsConfiguration->SourceProducer(0).Id(), "ensemble size");
	
    if (ensembleSizeStr.empty())
    {
        itsLogger->Error("Unable to find ensemble size from database");
        return;
    }

    itsEnsembleSize = boost::lexical_cast<int> (ensembleSizeStr);
 
    SetParams(calculatedParams);

    Start();
}

void fractile::Calculate(std::shared_ptr<info> myTargetInfo, uint16_t threadIndex)
{

    std::vector<int> fractile = {0,10,25,50,75,90,100};

    const std::string deviceType = "CPU";

    auto threadedLogger = logger_factory::Instance()->GetLog("fractileThread # " + boost::lexical_cast<std::string>(threadIndex));

    forecast_time forecastTime = myTargetInfo->Time();
    level forecastLevel = myTargetInfo->Level();

    threadedLogger->Info("Calculating time " + static_cast<std::string>(forecastTime.ValidDateTime()) +
                         " level " + static_cast<std::string>(forecastLevel));
    
    ensemble ens(param(itsParamName), itsEnsembleSize);

    try
    {
        ens.Fetch(itsConfiguration, forecastTime, forecastLevel);
    }
    catch (const HPExceptionType& e)
    {
	if (e == kFileDataNotFound)
	{
        	throw std::runtime_error(ClassName() + " failed to find ensemble data");
	}
    }

    myTargetInfo->ResetLocation();
    ens.ResetLocation();

    while (myTargetInfo->NextLocation() && ens.NextLocation())
    {
        auto sortedValues = ens.SortedValues();
	
        size_t targetInfoIndex = 0;
        for (auto i : fractile)
        {
            myTargetInfo->ParamIndex(targetInfoIndex);
            myTargetInfo->Value(sortedValues[i*(itsEnsembleSize-1)/100]);
            ++targetInfoIndex;
        }
    }

	threadedLogger->Info("[" + deviceType + "] Missing values: " + boost::lexical_cast<std::string>
			(myTargetInfo->Data().MissingCount()) + "/" + boost::lexical_cast<std::string> (myTargetInfo->Data().Size()));
}

} // plugin

} // namespace