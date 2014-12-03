/**
 * @file cache.h
 *
 * @date Nov 20, 2012
 * @author perämäki
 */

#ifndef CACHE_H
#define CACHE_H

#include "auxiliary_plugin.h"
#include "search_options.h"

namespace himan
{
namespace plugin
{


class cache : public auxiliary_plugin
{	

public:

	cache(); 
	~cache() {}

	cache(const cache& other) = delete;
	cache& operator=(const cache& other) = delete;

	/**
	 * @brief Insert data to cache
	 *
	 * @param anInfo Info class instance containing the data
	 * @param activeOnly Specify if we want to copy only the active part of the info class
	 * to cache
	 */

	void Insert(info& anInfo, bool activeOnly = true);
	std::vector<std::shared_ptr<himan::info>> GetInfo(const search_options& options);	
	void Clean();

	virtual std::string ClassName() const
	{
		return "himan::plugin::cache";
	};

	virtual HPPluginClass PluginClass() const
	{
		return kAuxiliary;
	};

	virtual HPVersionNumber Version() const
	{
		return HPVersionNumber(1, 1);
	}


private:
	void SplitToPool(info& anInfo);
	std::string UniqueName(const info& anInfo);
	std::string UniqueNameFromOptions(const search_options& options);

};

class cache_pool : public auxiliary_plugin
{	

public:
 
	~cache_pool() { delete itsInstance; }

	cache_pool(const cache_pool& other) = delete;
	cache_pool& operator=(const cache_pool& other) = delete;

	static cache_pool* Instance();
	bool Find(const std::string& uniqueName);
	void Insert(const std::string& uniqueName, std::shared_ptr<himan::info> info);
	std::shared_ptr<himan::info> GetInfo(const std::string& uniqueName);
	void Clean();

	virtual std::string ClassName() const
	{
		return "himan::plugin::cache_pool";
	};

	virtual HPPluginClass PluginClass() const
	{
		return kAuxiliary;
	};

	virtual HPVersionNumber Version() const
	{
		return HPVersionNumber(0, 1);
	}


private:

	cache_pool();
	
	std::map<std::string, std::shared_ptr<himan::info>> itsCache;
	static cache_pool* itsInstance;
	std::mutex itsInsertMutex;
	std::mutex itsGetMutex;
	std::mutex itsDeleteMutex;
	std::map<std::string, time_t> itsCacheItems;

};

#ifndef HIMAN_AUXILIARY_INCLUDE

// the class factory
extern "C" std::shared_ptr<himan_plugin> create()
{
	return std::shared_ptr<cache> (new cache());
}

#endif /* HIMAN_AUXILIARY_INCLUDE */

} // namespace plugin
} // namespace himan

#endif /* CACHE_H */
