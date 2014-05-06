/**
 * @file grib.h
 *
 * @brief Class to implement grib writing and reading. Actual grib opening and reading is done by fmigrib library.
 *
 * @date Nov 20, 2012
 * @author partio
 */

#ifndef GRIB_H
#define GRIB_H

#include "auxiliary_plugin.h"
#include "NFmiGrib.h"
#include "search_options.h"

namespace himan
{
namespace plugin
{

class grib : public auxiliary_plugin
{

public:

	grib();

	virtual ~grib() {}

	grib(const grib& other) = delete;
	grib& operator=(const grib& other) = delete;

	virtual std::string ClassName() const
	{
		return "himan::plugin::grib";
	};

	virtual HPPluginClass PluginClass() const
	{
		return kAuxiliary;
	};

	virtual HPVersionNumber Version() const
	{
		return HPVersionNumber(1, 0);
	}

	std::shared_ptr<NFmiGrib> Reader();

	/**
	 * @brief Return all data from a grib file.
	 *
	 * This function reads a grib file and returns the metadata+data (if specified) in a one or
	 * more info class instance(s).
	 *
	 * Function returns a vector because unlike with querydata, one grib file can contain many messages
	 * with totally different areas and projections. A single info-class instance can handle different times,
	 * levels, params and even changing grid size but it cannot handle different sized areas. Therefore from
	 * this function we need to return a vector.
	 *
	 * @param file Input file name
	 * @param options Search options (param, level, time)
	 * @param readContents Specify if data should also be read (and not only metadata)
	 * @param readPackedData Whether to read packed data (from grib). Caller must do unpacking.
	 *
	 * @return A vector of shared_ptr'd infos.
	 */

	std::vector<std::shared_ptr<info>> FromFile(const std::string& inputFile, const search_options& options, bool readContents = true, bool readPackedData = false);

	bool ToFile(std::shared_ptr<info> anInfo, const std::string& outputFile, HPFileType fileType, HPFileWriteOption fileWriteOption);

private:

	bool WriteGrib(std::shared_ptr<const info> anInfo, const std::string& outputFile, HPFileType fileType, bool appendToFile = false);
	void WriteAreaAndGrid(std::shared_ptr<const info> anInfo);
	void WriteTime(std::shared_ptr<const info> anInfo);
	void WriteParameter(std::shared_ptr<const info> anInfo);

	/**
	 * @brief UnpackBitmap
	 *
	 * Transform regular bitmap (unsigned char) to a int-based bitmap where each array key represents
	 * an actual data value. If bitmap is zero for that key, zero is also put to the int array. If bitmap
	 * is set for that key, the value is one.
	 *
	 * TODO: Change int-array to unpacked unsigned char array (reducing size 75%) or even not unpack bitmap beforehand but do it
	 * while computing stuff with the data array.
	 *
	 * @param bitmap Original bitmap read from grib
	 * @param unpacked Unpacked bitmap where number of keys is the same as in the data array
	 * @param len Length of original bitmap
	 */
	
	void UnpackBitmap(const unsigned char* __restrict__ bitmap, int* __restrict__ unpacked, size_t len, size_t unpackedLen) const;
	
	std::shared_ptr<NFmiGrib> itsGrib;

};

#ifndef HIMAN_AUXILIARY_INCLUDE

// the class factory

extern "C" std::shared_ptr<himan_plugin> create()
{
	return std::shared_ptr<grib> (new grib());
}

#endif /* HIMAN_AUXILIARY_INCLUDE */

} // namespace plugin
} // namespace himan

#endif /* GRIB_H */
