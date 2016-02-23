/**
 * @file luatool.h
 *
 * @date 2014-12-01
 * @author partio
 *
*/

#ifndef LUATOOL_H
#define LUATOOL_H

#include "compiled_plugin.h"
#include "compiled_plugin_base.h"
#include "himan_common.h"

extern "C"
{
#include <lua.h>
}

namespace luabind
{
namespace detail
{
namespace has_get_pointer_
{
	template<class T>
	T * get_pointer(std::shared_ptr<T> const& p) { return p.get(); }
}
}
}

#include <luabind/object.hpp>

namespace himan
{
namespace plugin
{

class luatool : public compiled_plugin, public compiled_plugin_base
{
public:
	luatool();
	virtual ~luatool();

	virtual std::string ClassName() const
	{
		return "himan::plugin::luatool";
	}

	virtual HPPluginClass PluginClass() const
	{
		return kCompiled;
	}

	virtual HPVersionNumber Version() const
	{
		return HPVersionNumber(1, 1);
	}

	void Process(std::shared_ptr<const plugin_configuration> configuration);
	luabind::object Fetch(const forecast_time& theTime, const level& theLevel, const param& theParam) const;
	luabind::object Fetch(const forecast_time& theTime, const level& theLevel, const param& theParam, const forecast_type& theType = forecast_type(kDeterministic)) const;
	void WriteToFile(const info& targetInfo, write_options opts = write_options()) override;
	void WriteToFile(const info_t& targetInfo);

protected:
	/* These functions exists because we need to stop himan
	 * from writing data to disk when calculation finished.
	 *
	 * All data write in luatool should be initiated from the
	 * lua scripts!
	 */

	void Finish() const;
	void Run(info_t myTargetInfo, unsigned short threadIndex);
	
private:
    void Calculate(std::shared_ptr<info> theTargetInfo, unsigned short theThreadIndex);
	void InitLua(info_t myTargetInfo);	
	bool ReadFile(const std::string& luaFile);
	
	write_options itsWriteOptions;
};

#ifndef HIMAN_AUXILIARY_INCLUDE

extern "C" std::shared_ptr<himan_plugin> create()
{
	return std::make_shared<luatool> ();
}

#endif /* HIMAN_AUXILIARY_INCLUDE */

} // namespace plugin
} // namespace himan

#endif /* LUATOOL_H */
