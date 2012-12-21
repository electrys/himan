/*
 * raw_time.cpp
 *
 *  Created on: Dec 9, 2012
 *      Author: partio
 */

#include "raw_time.h"

using namespace hilpee;

raw_time::raw_time(const std::string& theDateTime, const std::string& theTimeMask)
{
	// itsLogger = HilpeeLoggerFactory::GetLog("HilpeeTime");

	std::stringstream s(theDateTime);
	std::locale l(s.getloc(), new boost::posix_time::time_input_facet(theTimeMask.c_str()));

	s.imbue(l);

	s >> itsDateTime;

	if (itsDateTime == boost::date_time::not_a_date_time)
	{
		throw std::runtime_error(ClassName() + ": Unable to create time from source '" + theDateTime + "' with mask '" + theTimeMask + "'");
	}
}

raw_time::raw_time(const NFmiMetTime& theDateTime)
{

	std::string theTempMask = "YYYYMMDD";
	std::string theTempTime = static_cast<std::string> (theDateTime.ToStr(theTempMask));

	raw_time(theTempTime, theTempMask);
}

bool raw_time::operator==(const raw_time&  other)
{
	return (itsDateTime == other.itsDateTime);
}

bool raw_time::operator!=(const raw_time&  other)
{
	return !(*this == other);
}

std::string raw_time::String(const std::string& theTimeMask) const
{
	return FormatTime(itsDateTime, theTimeMask);
}


std::string raw_time::FormatTime(boost::posix_time::ptime theFormattedDateTime, const std::string& theTimeMask) const
{

	if (theFormattedDateTime == boost::date_time::not_a_date_time)
	{
		throw std::runtime_error("FormatTime(): input argument is 'not-a-date-time'");
	}

	std::stringstream s;
	std::locale l(s.getloc(), new boost::posix_time::time_facet(theTimeMask.c_str()));

	s.imbue(l);

	s << theFormattedDateTime;

	return s.str();

}

bool raw_time::Adjust(const std::string& theTimeType, int theValue)
{

	if (theTimeType == "hours")
	{
		boost::posix_time::hours adjustment (theValue);

		itsDateTime = itsDateTime + adjustment;
	}

	return true;
}

std::ostream& raw_time::Write(std::ostream& file) const
{

	file << "<" << ClassName() << " " << Version() << ">" << std::endl;
	file << "__itsDateTime__ " << FormatTime(itsDateTime, "%Y-%m-%d %H:%M:%S") << std::endl;

	return file;
}

