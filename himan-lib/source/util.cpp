/**
 * @file util.cpp
 *
 * @brief Different utility functions and classes in a namespace
 */

#include "util.h"
#include "cuda_helper.h"
#include "forecast_time.h"
#include "level.h"
#include "param.h"
#include <NFmiStereographicArea.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/math/constants/constants.hpp>
#include <iomanip>
#include <sstream>
#include <wordexp.h>

using namespace himan;
using namespace std;

string util::MakeFileName(HPFileWriteOption fileWriteOption, const info& info)
{
	ostringstream fileName;
	ostringstream base;

	base.str(".");

	// For neons get base directory

	if (fileWriteOption == kDatabase)
	{
		char* path;

		path = std::getenv("MASALA_PROCESSED_DATA_BASE");

		if (path != NULL)
		{
			base.str("");
			base << path;
		}
		else
		{
			cout << "Warning::util MASALA_PROCESSED_DATA_BASE not set" << endl;
		}

		base << "/" << info.Producer().Centre() << "_" << info.Producer().Process() << "/"
		     << info.Time().OriginDateTime().String("%Y%m%d%H%M") << "/" << info.Time().Step();
	}

	// Create a unique file name when creating multiple files from one info

	if (fileWriteOption == kDatabase || fileWriteOption == kMultipleFiles)
	{
		fileName << base.str() << "/" << info.Param().Name() << "_" << HPLevelTypeToString.at(info.Level().Type())
		         << "_" << info.Level().Value();

		if (info.Level().Value2() != kHPMissingValue)
		{
			fileName << "-" << info.Level().Value2();
		}

		fileName << "_" << HPGridTypeToString.at(info.Grid()->Type());

		if (info.Grid()->Class() == kRegularGrid)
		{
			fileName << "_" << info.Grid()->Ni() << "_" << info.Grid()->Nj();
		}

		fileName << "_0_" << setw(3) << setfill('0') << info.Time().Step();
		if (static_cast<int>(info.ForecastType().Type()) > 2)
		{
			fileName << "_" << static_cast<int>(info.ForecastType().Type()) << "_" << info.ForecastType().Value();
		}
	}
	else
	{
		// TODO!

		fileName << base.str() << "/"
		         << "TODO.file";
	}

	return fileName.str();
}

himan::HPFileType util::FileType(const string& theFile)
{
	using namespace std;

	// First check by extension since its cheap

	boost::filesystem::path p(theFile);

	if (p.extension().string() == ".gz" || p.extension().string() == ".bz2")
	{
		p = p.stem();
	}

	string ext = p.extension().string();

	if (ext == ".csv")
	{
		return kCSV;
	}
	else if (ext == ".fqd" || ext == ".sqd")
	{
		return kQueryData;
	}
	else if (ext == ".grib")
	{
		return kGRIB;
	}
	else if (ext == ".idx")
	{
		return kGRIBIndex;
	}
	else if (ext == ".grib2")
	{
		return kGRIB2;
	}
	else if (ext == ".nc")
	{
		return kNetCDF;
	}

	// Try the check the file header; CSV is not possible anymore

	ifstream f(theFile.c_str(), ios::in | ios::binary);

	long keywordLength = 4;

	char content[keywordLength];

	f.read(content, keywordLength);

	HPFileType ret = kUnknownFile;

	if (strncmp(content, "GRIB", 4) == 0)
	{
		ret = kGRIB;
	}
	else if (strncmp(content, "CDF", 3) == 0)
	{
		ret = kNetCDF;
	}
	else
	{
		// Not GRIB or NetCDF, keep on searching

		keywordLength = 5;

		char content[keywordLength];

		f.read(content, keywordLength);

		if (strncmp(content, "QINFO", 5) == 0)
		{
			ret = kQueryData;
		}
	}

	return ret;
}

// copied from http://stackoverflow.com/questions/236129/splitting-a-string-in-c and modified a bit

vector<string> util::Split(const string& s, const std::string& delims, bool fill)
{
	vector<string> orig_elems;

	boost::split(orig_elems, s, boost::is_any_of(delims));

	if (!fill || orig_elems.size() == 0)
	{
		return orig_elems;
	}

	vector<string> filled_elems;
	vector<string> splitted_elems;

	vector<string>::iterator it;

	for (it = orig_elems.begin(); it != orig_elems.end();)
	{
		boost::split(splitted_elems, *it, boost::is_any_of("-"));

		if (splitted_elems.size() == 2)
		{
			it = orig_elems.erase(it);

			int first = boost::lexical_cast<int>(splitted_elems[0]);
			int last = boost::lexical_cast<int>(splitted_elems.back());

			if (first <= last)
			{
				// levels are 1-65
				for (int i = first; i <= last; ++i) filled_elems.push_back(boost::lexical_cast<string>(i));
			}
			else
			{
				// levels are 65-1
				for (int i = first; i >= last; --i) filled_elems.push_back(boost::lexical_cast<string>(i));
			}
		}
		else
		{
			++it;
		}
	}

	vector<string> all_elems;

	all_elems.reserve(orig_elems.size() + filled_elems.size());

	all_elems.insert(all_elems.end(), orig_elems.begin(), orig_elems.end());
	all_elems.insert(all_elems.end(), filled_elems.begin(), filled_elems.end());

	return all_elems;
}

string util::Join(const vector<string>& elements, const string& delim)
{
	ostringstream s;

	for (size_t i = 0; i < elements.size(); i++)
	{
		if (i != 0)
		{
			s << delim;
		}
		s << elements[i];
	}

	return s.str();
}

pair<point, point> util::CoordinatesFromFirstGridPoint(const point& firstPoint, double orientation, size_t ni,
                                                       size_t nj, double xSizeInMeters, double ySizeInMeters)
{
	double xWidthInMeters = (static_cast<double>(ni) - 1.) * xSizeInMeters;
	double yWidthInMeters = (static_cast<double>(nj) - 1.) * ySizeInMeters;

	NFmiStereographicArea a(NFmiPoint(firstPoint.X(), firstPoint.Y()), xWidthInMeters, yWidthInMeters, orientation,
	                        NFmiPoint(0, 0), NFmiPoint(1, 1), 90.);

	point bottomLeft(a.BottomLeftLatLon().X(), a.BottomLeftLatLon().Y());
	point topRight(a.TopRightLatLon().X(), a.TopRightLatLon().Y());

	return pair<point, point>(bottomLeft, topRight);
}

tuple<double, double, double, double> util::EarthRelativeUVCoefficients(const himan::point& regPoint,
                                                                        const himan::point& rotPoint,
                                                                        const himan::point& southPole)
{
	point newSouthPole;

	if (southPole.Y() > 0)
	{
		newSouthPole.Y(-southPole.Y());
		newSouthPole.X(0);
	}
	else
	{
		newSouthPole = southPole;
	}

	double southPoleY = constants::kDeg * (newSouthPole.Y() + 90);
	double sinPoleY, cosPoleY;

	sincos(southPoleY, &sinPoleY, &cosPoleY);

	double cosRegY = cos(constants::kDeg * regPoint.Y());  // zcyreg

	double zxmxc = constants::kDeg * (regPoint.X() - newSouthPole.X());

	double sinxmxc, cosxmxc;

	sincos(zxmxc, &sinxmxc, &cosxmxc);

	double rotXRad = constants::kDeg * rotPoint.X();
	double rotYRad = constants::kDeg * rotPoint.Y();

	double sinRotX, cosRotX;
	sincos(rotXRad, &sinRotX, &cosRotX);

	double sinRotY, cosRotY;
	sincos(rotYRad, &sinRotY, &cosRotY);

	double PA = cosxmxc * cosRotX + cosPoleY * sinxmxc * sinRotX;
	double PB = cosPoleY * sinxmxc * cosRotX * sinRotY + sinPoleY * sinxmxc * cosRotY - cosxmxc * sinRotX * sinRotY;
	double PC = (-sinPoleY) * sinRotX / cosRegY;
	double PD = (cosPoleY * cosRotY - sinPoleY * cosRotX * sinRotY) / cosRegY;

	return make_tuple(PA, PB, PC, PD);
}

tuple<double, double, double, double> util::GridRelativeUVCoefficients(const himan::point& regPoint,
                                                                       const himan::point& rotPoint,
                                                                       const himan::point& southPole)
{
	point newSouthPole;

	if (southPole.Y() > 0)
	{
		newSouthPole.Y(-southPole.Y());
		newSouthPole.X(0);
	}
	else
	{
		newSouthPole = southPole;
	}

	double sinPoleY = sin(constants::kDeg * (newSouthPole.Y() + 90));  // zsyc
	double cosPoleY = cos(constants::kDeg * (newSouthPole.Y() + 90));  // zcyc

	// double sinRegX = sin(constants::kDeg * regPoint.X()); // zsxreg
	// double cosRegX = cos(constants::kDeg * regPoint.X()); // zcxreg
	double sinRegY = sin(constants::kDeg * regPoint.Y());  // zsyreg
	double cosRegY = cos(constants::kDeg * regPoint.Y());  // zcyreg

	double zxmxc = constants::kDeg * (regPoint.X() - newSouthPole.X());
	double sinxmxc = sin(zxmxc);  // zsxmxc
	double cosxmxc = cos(zxmxc);  // zcxmxc

	double sinRotX = sin(constants::kDeg * rotPoint.X());  // zsxrot
	double cosRotX = cos(constants::kDeg * rotPoint.X());  // zcxrot
	// double sinRotY = sin(constants::kDeg * rotPoint.Y()); // zsyrot
	double cosRotY = cos(constants::kDeg * rotPoint.Y());  // zcyrot

	double PA = cosPoleY * sinxmxc * sinRotX + cosxmxc * cosRotX;
	double PB = cosPoleY * cosxmxc * sinRegY * sinRotX - sinPoleY * cosRegY * sinRotX - sinxmxc * sinRegY * cosRotX;
	double PC = sinPoleY * sinxmxc / cosRotY;
	double PD = (sinPoleY * cosxmxc * sinRegY + cosPoleY * cosRegY) / cosRotY;

	return make_tuple(PA, PB, PC, PD);
}

point util::UVToGeographical(double longitude, const point& stereoUV)
{
	double U, V;

	if (stereoUV.X() == 0 && stereoUV.Y() == 0)
	{
		return point(0, 0);
	}

	double sinLon, cosLon;

	sincos(longitude * constants::kDeg, &sinLon, &cosLon);

	U = stereoUV.X() * cosLon + stereoUV.Y() * sinLon;
	V = -stereoUV.X() * sinLon + stereoUV.Y() * cosLon;

	return point(U, V);
}

double util::ToPower(double value, double power)
{
	double divisor = 1.0;

	while (value < 0)
	{
		divisor /= power;
		value++;
	}

	while (value > 0)
	{
		divisor *= power;
		value--;
	}

	return divisor;
}

pair<matrix<double>, matrix<double>> util::CentralDifference(matrix<double>& A, vector<double>& dx, vector<double>& dy)
{
	matrix<double> dA_dx(A.SizeX(), A.SizeY(), 1, A.MissingValue());
	matrix<double> dA_dy(A.SizeX(), A.SizeY(), 1, A.MissingValue());

	int ASizeX = int(A.SizeX());
	int ASizeY = int(A.SizeY());

	assert(dy.size() == A.SizeX() && dx.size() == A.SizeY());

	// check if data contains missing values
	if (A.MissingCount() == 0)  // if no missing values in the data we can use a faster algorithm
	{
		// calculate for inner field
		for (int j = 1; j < ASizeY - 1; ++j)  // rows
		{
			for (int i = 1; i < ASizeX - 1; ++i)  // columns
			{
				dA_dx.Set(i, j, 0,
				          (A.At(i + 1, j, 0) - A.At(i - 1, j, 0)) / (2 * dx[j]));  // central difference in x-direction
				dA_dy.Set(i, j, 0,
				          (A.At(i, j + 1, 0) - A.At(i, j - 1, 0)) / (2 * dy[i]));  // central difference in y-direction
			}
		}

		// treat boundaries separately
		for (int i = 1; i < ASizeX - 1; ++i)  // rows
		{
			// calculate for upper boundary
			dA_dx.Set(i, 0, 0,
			          (A.At(i + 1, 0, 0) - A.At(i - 1, 0, 0)) / (2 * dx[0]));  // central difference in x-direction
			dA_dy.Set(i, 0, 0, (A.At(i, 1, 0) - A.At(i, 0, 0)) / dy[i]);       // foreward difference in y-direction

			// calculate for lower boundary
			dA_dx.Set(i, ASizeY - 1, 0, (A.At(i + 1, ASizeY - 1, 0) - A.At(i - 1, ASizeY - 1, 0)) /
			                                (2 * dx[ASizeY - 1]));  // central difference in x-direction
			dA_dy.Set(i, ASizeY - 1, 0,
			          (A.At(i, ASizeY - 1, 0) - A.At(i, ASizeY - 2, 0)) / dy[i]);  // backward difference in y-direction
		}

		for (int j = 1; j < ASizeY - 1; ++j)  // columns
		{
			// calculate for left boundary
			dA_dx.Set(0, j, 0, (A.At(1, j, 0) - A.At(0, j, 0)) / dx[j]);  // foreward difference in x-direction
			dA_dy.Set(0, j, 0,
			          (A.At(0, j + 1, 0) - A.At(0, j - 1, 0)) / (2 * dy[0]));  // central difference in y-direction

			// calculate for right boundary
			dA_dx.Set(ASizeX - 1, j, 0,
			          (A.At(ASizeX - 1, j, 0) - A.At(ASizeX - 2, j, 0)) / dx[j]);  // backward difference in x-direction
			dA_dy.Set(ASizeX - 1, j, 0, (A.At(ASizeX - 1, j + 1, 0) - A.At(ASizeX - 1, j - 1, 0)) /
			                                (2 * dy[ASizeX - 1]));  // central difference in y-direction
		}

		// corner values last
		// top left
		dA_dx.Set(0, 0, 0, (A.At(1, 0, 0) - A.At(0, 0, 0)) / dx[0]);  // foreward difference in x-direction
		dA_dy.Set(0, 0, 0, (A.At(0, 1, 0) - A.At(0, 0, 0)) / dy[0]);  // foreward difference in y-direction

		// top right
		dA_dx.Set(ASizeX - 1, 0, 0,
		          (A.At(ASizeX - 1, 0, 0) - A.At(ASizeX - 2, 0, 0)) / dx[0]);  // foreward difference in x-direction
		dA_dy.Set(ASizeX - 1, 0, 0, (A.At(ASizeX - 1, 1, 0) - A.At(ASizeX - 1, 0, 0)) /
		                                dy[ASizeX - 1]);  // backward difference in y-direction

		// bottom left
		dA_dx.Set(0, ASizeY - 1, 0, (A.At(1, ASizeY - 1, 0) - A.At(0, ASizeY - 1, 0)) /
		                                dx[ASizeY - 1]);  // foreward difference in x-direction
		dA_dy.Set(0, ASizeY - 1, 0,
		          (A.At(0, ASizeY - 1, 0) - A.At(0, ASizeY - 2, 0)) / dy[0]);  // backward difference in y-direction

		// bottom right
		dA_dx.Set(ASizeX - 1, ASizeY - 1, 0, (A.At(ASizeX - 1, ASizeY - 1, 0) - A.At(ASizeX - 2, ASizeY - 1, 0)) /
		                                         dx[ASizeY - 1]);  // backward difference in x-direction
		dA_dy.Set(ASizeX - 1, ASizeY - 1, 0, (A.At(ASizeX - 1, ASizeY - 1, 0) - A.At(ASizeX - 1, ASizeY - 2, 0)) /
		                                         dy[ASizeX - 1]);  // backward difference in y-direction
	}
	else
	{
		// calculate for inner field
		for (int j = 1; j < ASizeY - 1; ++j)  // rows
		{
			for (int i = 1; i < ASizeX - 1; ++i)  // columns
			{
				if (!(A.At(i + 1, j, 0) == kFloatMissing || A.At(i - 1, j, 0) == kFloatMissing ||
				      A.At(i, j + 1, 0) == kFloatMissing || A.At(i, j - 1, 0) == kFloatMissing))
				{
					dA_dx.Set(i, j, 0, (A.At(i + 1, j, 0) - A.At(i - 1, j, 0)) /
					                       (2 * dx[j]));  // central difference in x-direction
					dA_dy.Set(i, j, 0, (A.At(i, j + 1, 0) - A.At(i, j - 1, 0)) /
					                       (2 * dy[i]));  // central difference in y-direction
				}
				else
				{
					dA_dx.Set(i, j, 0, kFloatMissing);
					dA_dy.Set(i, j, 0, kFloatMissing);
				}
			}
		}

		// treat boundaries separately
		for (int i = 1; i < ASizeX - 1; ++i)  // rows
		{
			if (!(A.At(i + 1, 0, 0) == kFloatMissing || A.At(i - 1, 0, 0) == kFloatMissing ||
			      A.At(i, 0, 0) == kFloatMissing || A.At(i, 1, 0) == kFloatMissing))
			{
				// calculate for upper boundary
				dA_dx.Set(i, 0, 0,
				          (A.At(i + 1, 0, 0) - A.At(i - 1, 0, 0)) / (2 * dx[0]));  // central difference in x-direction
				dA_dy.Set(i, 0, 0, (A.At(i, 1, 0) - A.At(i, 0, 0)) / dy[i]);       // foreward difference in y-direction
			}
			else
			{
				dA_dx.Set(i, 0, 0, kFloatMissing);
				dA_dy.Set(i, 0, 0, kFloatMissing);
			}

			if (!(A.At(i + 1, ASizeY - 1, 0) == kFloatMissing || A.At(i - 1, ASizeY - 1, 0) == kFloatMissing ||
			      A.At(i, ASizeY - 1, 0) == kFloatMissing || A.At(i, ASizeY - 2, 0) == kFloatMissing))
			{
				// calculate for lower boundary
				dA_dx.Set(i, ASizeY - 1, 0, (A.At(i + 1, ASizeY - 1, 0) - A.At(i - 1, ASizeY - 1, 0)) /
				                                (2 * dx[ASizeY - 1]));  // central difference in x-direction
				dA_dy.Set(i, ASizeY - 1, 0, (A.At(i, ASizeY - 1, 0) - A.At(i, ASizeY - 2, 0)) /
				                                dy[i]);  // backward difference in y-direction
			}
			else
			{
				dA_dx.Set(i, ASizeY - 1, 0, kFloatMissing);
				dA_dy.Set(i, ASizeY - 1, 0, kFloatMissing);
			}
		}

		for (int j = 1; j < ASizeY - 1; ++j)  // columns
		{
			if (!(A.At(0, j, 0) == kFloatMissing || A.At(1, j, 0) == kFloatMissing ||
			      A.At(0, j - 1, 0) == kFloatMissing || A.At(0, j + 1, 0) == kFloatMissing))
			{
				// calculate for left boundary
				dA_dx.Set(0, j, 0, (A.At(1, j, 0) - A.At(0, j, 0)) / dx[j]);  // foreward difference in x-direction
				dA_dy.Set(0, j, 0,
				          (A.At(0, j + 1, 0) - A.At(0, j - 1, 0)) / (2 * dy[0]));  // central difference in y-direction
			}
			else
			{
				dA_dx.Set(0, j, 0, kFloatMissing);
				dA_dx.Set(0, j, 0, kFloatMissing);
			}

			if (!(A.At(0, j, 0) == kFloatMissing || A.At(1, j, 0) == kFloatMissing ||
			      A.At(0, j - 1, 0) == kFloatMissing || A.At(0, j + 1, 0) == kFloatMissing))
			{
				// calculate for right boundary
				dA_dx.Set(ASizeX - 1, j, 0, (A.At(ASizeX - 1, j, 0) - A.At(ASizeX - 2, j, 0)) /
				                                dx[j]);  // backward difference in x-direction
				dA_dy.Set(ASizeX - 1, j, 0, (A.At(ASizeX - 1, j + 1, 0) - A.At(ASizeX - 1, j - 1, 0)) /
				                                (2 * dy[ASizeX - 1]));  // central difference in y-direction
			}
			else
			{
				dA_dx.Set(ASizeX - 1, j, 0, kFloatMissing);
				dA_dy.Set(ASizeX - 1, j, 0, kFloatMissing);
			}
		}

		// corner values last
		// top left
		if (!(A.At(0, 0, 0) == kFloatMissing || A.At(1, 0, 0) == kFloatMissing || A.At(0, 1, 0) == kFloatMissing))
		{
			dA_dx.Set(0, 0, 0, (A.At(1, 0, 0) - A.At(0, 0, 0)) / dx[0]);  // foreward difference in x-direction
			dA_dy.Set(0, 0, 0, (A.At(0, 1, 0) - A.At(0, 0, 0)) / dy[0]);  // foreward difference in y-direction
		}
		else
		{
			dA_dx.Set(0, 0, 0, kFloatMissing);
			dA_dy.Set(0, 0, 0, kFloatMissing);
		}

		// top right
		if (!(A.At(ASizeX - 1, 0, 0) == kFloatMissing || A.At(ASizeX - 2, 0, 0) == kFloatMissing ||
		      A.At(ASizeX - 1, 1, 0) == kFloatMissing))
		{
			dA_dx.Set(ASizeX - 1, 0, 0,
			          (A.At(ASizeX - 1, 0, 0) - A.At(ASizeX - 2, 0, 0)) / dx[0]);  // foreward difference in x-direction
			dA_dy.Set(ASizeX - 1, 0, 0, (A.At(ASizeX - 1, 1, 0) - A.At(ASizeX - 1, 0, 0)) /
			                                dy[ASizeX - 1]);  // backward difference in y-direction
		}
		else
		{
			dA_dx.Set(ASizeX - 1, 0, 0, kFloatMissing);
			dA_dy.Set(ASizeX - 1, 0, 0, kFloatMissing);
		}

		// bottom left
		if (!(A.At(0, ASizeY - 1, 0) == kFloatMissing || A.At(0, ASizeY - 2, 0) == kFloatMissing ||
		      A.At(1, ASizeY - 1, 0) == kFloatMissing))
		{
			dA_dx.Set(0, ASizeY - 1, 0, (A.At(1, ASizeY - 1, 0) - A.At(0, ASizeY - 1, 0)) /
			                                dx[ASizeY - 1]);  // foreward difference in x-direction
			dA_dy.Set(0, ASizeY - 1, 0,
			          (A.At(0, ASizeY - 1, 0) - A.At(0, ASizeY - 2, 0)) / dy[0]);  // backward difference in y-direction
		}
		else
		{
			dA_dx.Set(0, ASizeY - 1, 0, kFloatMissing);
			dA_dy.Set(0, ASizeY - 1, 0, kFloatMissing);
		}

		// bottom right
		if (!(A.At(ASizeX - 1, ASizeY - 1, 0) == kFloatMissing || A.At(ASizeX - 1, ASizeY - 2, 0) == kFloatMissing ||
		      A.At(ASizeX - 2, ASizeY - 1, 0) == kFloatMissing))
		{
			dA_dx.Set(ASizeX - 1, ASizeY - 1, 0, (A.At(ASizeX - 1, ASizeY - 1, 0) - A.At(ASizeX - 2, ASizeY - 1, 0)) /
			                                         dx[ASizeY - 1]);  // backward difference in x-direction
			dA_dy.Set(ASizeX - 1, ASizeY - 1, 0, (A.At(ASizeX - 1, ASizeY - 1, 0) - A.At(ASizeX - 1, ASizeY - 2, 0)) /
			                                         dy[ASizeX - 1]);  // backward difference in y-direction
		}
		else
		{
			dA_dx.Set(ASizeX - 1, ASizeY - 1, 0, kFloatMissing);
			dA_dy.Set(ASizeX - 1, ASizeY - 1, 0, kFloatMissing);
		}
	}

	pair<matrix<double>, matrix<double>> ret(dA_dx, dA_dy);
	return ret;
}

double util::LatitudeLength(double phi)
{
	double cos_phi = cos(phi * constants::kDeg);
	return 2 * boost::math::constants::pi<double>() * constants::kR * abs(cos_phi);
}

double util::round(double val, unsigned short numdigits)
{
	int div = static_cast<int>(pow(10, static_cast<double>(numdigits)));
	return std::round(val * div) / div;
}

string util::MakeSQLInterval(const himan::forecast_time& theTime)
{
	int step = theTime.Step();
	string ret;

	char i[11];

	if (theTime.StepResolution() == himan::kHourResolution)
	{
		snprintf(i, 11, "%02d:00:00", step);

		ret = i;
	}
	else
	{
		int hours, minutes;

		hours = static_cast<int>(floor(static_cast<double>(step) / 60));
		minutes = step - hours * 60;

		snprintf(i, 11, "%02d:%02d:00", hours, minutes);

		ret = i;
	}

	return ret;
}

string util::Expand(const string& in)
{
	if (in.find("$") == string::npos) return in;

	wordexp_t p;

	wordexp(in.c_str(), &p, 0);
	char** out = p.we_wordv;

	string ret(out[0]);

	wordfree(&p);

	return ret;
}

void util::DumpVector(const vector<double>& vec, const string& name)
{
	double min = 1e38, max = -1e38, sum = 0;
	size_t count = 0, missing = 0;

	for (const double& val : vec)
	{
		if (val == himan::kFloatMissing)
		{
			missing++;
			continue;
		}

		min = (val < min) ? val : min;
		max = (val > max) ? val : max;
		count++;
		sum += val;
	}

	double mean = numeric_limits<double>::quiet_NaN();

	if (count > 0)
	{
		mean = sum / static_cast<double>(count);
	}

	if (!name.empty())
	{
		cout << name << "\t";
	}

	cout << "min " << min << " max " << max << " mean " << mean << " count " << count << " missing " << missing << endl;

	int binn = 10;

	double binw = (max - min) / 10;

	double binmin = min;
	double binmax = binmin + binw;

	cout << "distribution:" << endl;

	for (int i = 1; i <= binn; i++)
	{
		if (i == binn) binmax += 0.001;

		size_t count = 0;

		for (const double& val : vec)
		{
			if (val == himan::kFloatMissing) continue;

			if (val >= binmin && val < binmax)
			{
				count++;
			}
		}

		if (i == binn) binmax -= 0.001;

		cout << binmin << ":" << binmax << " " << count << std::endl;

		binmin += binw;
		binmax += binw;
	}
}

string util::GetEnv(const string& key)
{
	const char* var = getenv(key.c_str());
	if (!var)
	{
		throw runtime_error("Environment variable '" + string(key) + "' not found");
	}
	return string(var);
}

#ifdef HAVE_CUDA
void util::Unpack(initializer_list<grid*> grids)
{
	vector<cudaStream_t*> streams;

	for (auto it = grids.begin(); it != grids.end(); ++it)
	{
		// regular_grid* tempGrid = dynamic_cast<regular_grid*> (*it);

		if (!(*it)->IsPackedData())
		{
			// Safeguard: This particular info does not have packed data
			continue;
		}

		assert((*it)->PackedData().ClassName() == "simple_packed" || (*it)->PackedData().ClassName() == "jpeg_packed");

		double* arr = 0;
		size_t N = (*it)->PackedData().unpackedLength;

		assert(N > 0);

		cudaStream_t* stream = new cudaStream_t;
		CUDA_CHECK(cudaStreamCreate(stream));
		streams.push_back(stream);

		assert((*it)->Data().Size() == N);
		arr = const_cast<double*>((*it)->Data().ValuesAsPOD());

		CUDA_CHECK(cudaHostRegister(reinterpret_cast<void*>(arr), sizeof(double) * N, 0));

		assert(arr);

		(*it)->PackedData().Unpack(arr, N, stream);

		CUDA_CHECK(cudaHostUnregister(arr));

		(*it)->PackedData().Clear();
	}

	for (size_t i = 0; i < streams.size(); i++)
	{
		CUDA_CHECK(cudaStreamDestroy(*streams[i]));
	}
}
#endif
