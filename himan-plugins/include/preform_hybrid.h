/**
 * @file preform_hybrid.h
 *
 * @date Sep 26, 2013
 * @author: partio
 *
 * @brief Precipitation form algorithm based on hybrid fields, Korpela-Koistinen version
 *
 * Formula implementation in smarttool language by Simo Neiglick.
 *
 * From wiki: https://wiki.fmi.fi/pages/viewpage.action?pageId=21139101
 *
 * ===============================================================================
 *
 * ===============================================================================
 *
 * Output is one of
 *
 * 0 = tihku, 1 = vesi, 2 = räntä, 3 = lumi, 4 = jäätävä tihku, 5 = jäätävä sade
 *
 */

#ifndef PREFORM_HYBRID_H
#define PREFORM_HYBRID_H

#include "compiled_plugin.h"
#include "compiled_plugin_base.h"

namespace himan
{
namespace plugin
{

class preform_hybrid : public compiled_plugin, private compiled_plugin_base
{
public:
	preform_hybrid();

	inline virtual ~preform_hybrid() {}

	preform_hybrid(const preform_hybrid& other) = delete;
	preform_hybrid& operator=(const preform_hybrid& other) = delete;

	virtual void Process(std::shared_ptr<const plugin_configuration> conf);

	virtual std::string ClassName() const
	{
		return "himan::plugin::preform_hybrid";
	}

	virtual HPPluginClass PluginClass() const
	{
		return kCompiled;
	}

	virtual HPVersionNumber Version() const
	{
		return HPVersionNumber(1, 1);
	}

protected:
	virtual void Calculate(std::shared_ptr<info> myTargetInfo, unsigned short threadIndex);
	
private:
	/**
	 * @brief Calculate properties of a stratus cloud
	 * @return info containing multiple parameters describing stratus
	 */

	void Stratus(std::shared_ptr<const plugin_configuration> conf, const forecast_time& ftime, std::shared_ptr<info>& result, const grid* baseGrid);

	/**
	 * @brief Calculate freezing area information (number of zero level etc)
	 */

	void FreezingArea(std::shared_ptr<const plugin_configuration> conf, const forecast_time& ftime, std::shared_ptr<info>& result, const grid* baseGrid);

};

// the class factory

extern "C" std::shared_ptr<himan_plugin> create()
{
	return std::shared_ptr<himan_plugin>(new preform_hybrid());
}

} // namespace plugin
} // namespace himan


#endif /* PREFORM_HYBRID_H */
