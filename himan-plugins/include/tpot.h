/*
 * tpot.h
 *
 *  Created on: Nov 20, 2012
 *      Author: partio
 */

#ifndef TPOT_H
#define TPOT_H

#include "compiled_plugin.h"
#include "compiled_plugin_base.h"

namespace himan
{
namespace plugin
{

class tpot : public compiled_plugin, private compiled_plugin_base
{
public:
    tpot();

    inline virtual ~tpot() {}

    tpot(const tpot& other) = delete;
    tpot& operator=(const tpot& other) = delete;

    virtual void Process(std::shared_ptr<const plugin_configuration> conf);

    virtual std::string ClassName() const
    {
        return "himan::plugin::tpot";
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

    void Run(std::shared_ptr<info> myTargetInfo, std::shared_ptr<const plugin_configuration> theConfiguration, unsigned short theThreadIndex);
    void Calculate(std::shared_ptr<info> myTargetInfo, std::shared_ptr<const plugin_configuration> theConfiguration, unsigned short theThreadIndex);

    bool itsThetaCalculation;
    bool itsThetaWCalculation;
    bool itsThetaECalculation;
    bool itsUseCuda;
    int itsCudaDeviceCount;
};

// the class factory

extern "C" std::shared_ptr<himan_plugin> create()
{
    return std::shared_ptr<tpot> (new tpot());
}

} // namespace plugin
} // namespace himan


#endif /* TPOT_H */
