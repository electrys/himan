/**
 * @file cape.h
 *
 */

#ifndef CAPE_H
#define CAPE_H

#include "compiled_plugin.h"
#include "compiled_plugin_base.h"

// T, Td, P
typedef std::tuple<std::vector<float>, std::vector<float>, std::vector<float>> cape_source;

namespace himan
{
namespace plugin
{
class cape : public compiled_plugin, private compiled_plugin_base
{
   public:
	cape();

	inline virtual ~cape()
	{
	}
	cape(const cape& other) = delete;
	cape& operator=(const cape& other) = delete;

	virtual void Process(std::shared_ptr<const plugin_configuration> conf);

	virtual std::string ClassName() const
	{
		return "himan::plugin::cape";
	}
	virtual HPPluginClass PluginClass() const
	{
		return kCompiled;
	}
	virtual HPVersionNumber Version() const
	{
		return HPVersionNumber(0, 1);
	}

   private:
	virtual void Calculate(std::shared_ptr<info<float>> theTargetInfo, unsigned short threadIndex);

	std::pair<std::vector<float>, std::vector<float>> GetLCL(std::shared_ptr<info<float>> myTargetInfo,
	                                                         const cape_source& source);

	std::pair<std::vector<float>, std::vector<float>> GetLFC(std::shared_ptr<info<float>> myTargetInfo,
	                                                         std::vector<float>& T, std::vector<float>& P);
	std::pair<std::vector<float>, std::vector<float>> GetLFCCPU(std::shared_ptr<info<float>> myTargetInfo,
	                                                            std::vector<float>& T, std::vector<float>& P,
	                                                            std::vector<float>& TenvLCL);

	// Functions to fetch different kinds of source data

	cape_source GetSurfaceValues(std::shared_ptr<info<float>> myTargetInfo);

	cape_source Get500mMixingRatioValues(std::shared_ptr<info<float>> myTargetInfo);
	cape_source Get500mMixingRatioValuesCPU(std::shared_ptr<info<float>> myTargetInfo);

	cape_source GetHighestThetaEValues(std::shared_ptr<info<float>> myTargetInfo);
	cape_source GetHighestThetaEValuesCPU(std::shared_ptr<info<float>> myTargetInfo);

	void GetCAPE(std::shared_ptr<info<float>> myTargetInfo,
	             const std::pair<std::vector<float>, std::vector<float>>& LFC);
	void GetCAPECPU(std::shared_ptr<info<float>> myTargetInfo, const std::vector<float>& T,
	                const std::vector<float>& P);

	void GetCIN(std::shared_ptr<info<float>> myTargetInfo, const std::vector<float>& Tsource,
	            const std::vector<float>& Psource, const std::vector<float>& TLCL, const std::vector<float>& PLCL,
	            const std::vector<float>& ZLCL, const std::vector<float>& PLFC, const std::vector<float>& ZLFC);
	void GetCINCPU(std::shared_ptr<info<float>> myTargetInfo, const std::vector<float>& Tsource,
	               const std::vector<float>& Psource, const std::vector<float>& TLCL, const std::vector<float>& PLCL,
	               const std::vector<float>& ZLCL, const std::vector<float>& PLFC, const std::vector<float>& ZLFC);

	level itsBottomLevel;
	bool itsUseVirtualTemperature;

	std::vector<level> itsSourceLevels;
};

// the class factory

extern "C" std::shared_ptr<himan_plugin> create()
{
	return std::make_shared<cape>();
}
}  // namespace plugin
}  // namespace himan

#endif /* SI */
