/**
 * @file	write_options.h
 */

#ifndef WRITE_OPTIONS_H
#define WRITE_OPTIONS_H

#include <plugin_configuration.h>

namespace himan
{
namespace plugin
{
struct write_options
{
	std::shared_ptr<const plugin_configuration> configuration;
	bool use_bitmap;             // use bitmap for grib if missing data exists
	HPPackingType packing_type;  // for grib, ie simple_packing, jpeg_packing
	bool write_empty_grid;       // write file even if all data is missing
	int precision;               // precision (decimal points)

	write_options() : use_bitmap(true), packing_type(kSimplePacking), write_empty_grid(true), precision(kHPMissingInt)
	{
	}
};
}
}

#endif /* WRITE_OPTIONS_H */
