#pragma once

#include "himan_common.h"
#include <boost/optional.hpp>
#include <string>

namespace himan
{
enum class HPFileStorageType
{
	kFileSystem = 0,
	kS3
};

struct file_information
{
	std::string file_location;              // /path/to/file
	HPFileType file_type;                   // GRIB,csv, etc see himan_common.h
	HPFileStorageType storage_type;         // POSIX filesystem, S3, ...
	boost::optional<unsigned long> offset;  // "message" offset from file beginning
	boost::optional<unsigned long> length;  // "message" length
};
}  // namespace himan