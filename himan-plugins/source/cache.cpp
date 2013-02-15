/**
 * @file cache.cpp
 *
 * @date Nov 21, 2012
 * @author perämäki
 */

#include "cache.h"
#include "logger_factory.h"
#include "info.h"
#include "plugin_factory.h"
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace himan::plugin;

cache::cache()
{
    itsLogger = std::unique_ptr<logger> (logger_factory::Instance()->GetLog("cache"));
}

string cache::UniqueName(const search_options& options)
{
	string forecast_time = (*options.time.OriginDateTime()).String() + (*options.time.ValidDateTime()).String();
	string param = options.param.Name();
	string level_id = boost::lexical_cast<string>(options.level.Value());
	string level = boost::lexical_cast<string>(options.level.Index());
	string config_source = boost::lexical_cast<string>((*options.configuration).SourceProducer().Id());
	string config_target = boost::lexical_cast<string>((*options.configuration).TargetProducer().Id());
	return forecast_time + param + level_id + level + config_source + config_target;
}

void cache::Insert(const search_options& options, vector<shared_ptr<himan::info>> infos)
{	
	string uniqueName = UniqueName(options);
	
	if (!(cache_pool::Instance()->Find(uniqueName)))
	{
		for (size_t i = 0; i < infos.size(); i++)
		{
			cache_pool::Instance()->Insert(uniqueName, infos[i]);
		}	
	}
}

vector<shared_ptr<himan::info>> cache::GetInfo(const search_options& options) 
{
	string uniqueName = UniqueName(options);

	vector<shared_ptr<himan::info>> info;
	
	if (cache_pool::Instance()->Find(uniqueName))
	{
		info.push_back(cache_pool::Instance()->GetInfo(uniqueName));
	}
	return info;
}

typedef lock_guard<mutex> Lock;

cache_pool* cache_pool::itsInstance = NULL;

cache_pool::cache_pool()
{
    itsLogger = std::unique_ptr<logger> (logger_factory::Instance()->GetLog("cache_pool"));
}

cache_pool* cache_pool::Instance()
{
	if (!itsInstance) 
	{
		itsInstance = new cache_pool();
	}

	return itsInstance;
}

bool cache_pool::Find(const string& uniqueName) 
{

	for (map<string, shared_ptr<himan::info>>::iterator it = itsCache.begin(); it != itsCache.end(); ++it)
	{
		if (it->first == uniqueName)
		{
			return true;
		}
	}

	return false;
}

void cache_pool::Insert(const string& uniqueName, shared_ptr<himan::info> info)
{
	Lock lock(itsMutex);

	itsCache.insert(pair<string, shared_ptr<himan::info>>(uniqueName, info));
	
	itsLogger->Debug("Data added to cache with name: " + uniqueName);
}

shared_ptr<himan::info> cache_pool::GetInfo(const string& uniqueName)
{
	Lock lock(itsMutex);

	return shared_ptr<info> (new info(*itsCache[uniqueName]));
}
