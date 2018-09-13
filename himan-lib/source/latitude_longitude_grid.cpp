#include "latitude_longitude_grid.h"
#include "info.h"
#include <NFmiRotatedLatLonArea.h>

#ifdef HAVE_CUDA
#include "simple_packed.h"
#endif

using namespace himan;
using namespace std;

latitude_longitude_grid::latitude_longitude_grid()
    : regular_grid(),
      itsBottomLeft(),
      itsTopRight(),
      itsBottomRight(),
      itsTopLeft(),
      itsDi(kHPMissingValue),
      itsDj(kHPMissingValue),
      itsNi(kHPMissingInt),
      itsNj(kHPMissingInt),
      itsIsGlobal(false)
{
	itsLogger = logger("latitude_longitude_grid");
	Type(kLatitudeLongitude);
}

latitude_longitude_grid::latitude_longitude_grid(HPScanningMode theScanningMode, point theBottomLeft, point theTopRight)
    : regular_grid(),
      itsBottomLeft(theBottomLeft),
      itsTopRight(theTopRight),
      itsBottomRight(),
      itsTopLeft(),
      itsDi(kHPMissingValue),
      itsDj(kHPMissingValue),
      itsNi(kHPMissingInt),
      itsNj(kHPMissingInt),
      itsIsGlobal(false)
{
	itsLogger = logger("latitude_longitude_grid");
	Type(kLatitudeLongitude);
	ScanningMode(theScanningMode);
	UpdateCoordinates();
}

latitude_longitude_grid::latitude_longitude_grid(const latitude_longitude_grid& other)
    : regular_grid(other),
      itsBottomLeft(other.itsBottomLeft),
      itsTopRight(other.itsTopRight),
      itsBottomRight(other.itsBottomRight),
      itsTopLeft(other.itsTopLeft),
      itsDi(other.itsDi),
      itsDj(other.itsDj),
      itsNi(other.itsNi),
      itsNj(other.itsNj),
      itsIsGlobal(other.itsIsGlobal)
{
	itsLogger = logger("latitude_longitude_grid");
}

size_t latitude_longitude_grid::Size() const
{
	if (itsNi == static_cast<size_t>(kHPMissingInt) || itsNj == static_cast<size_t>(kHPMissingInt))
	{
		return kHPMissingInt;
	}

	return itsNi * itsNj;
}

point latitude_longitude_grid::TopRight() const
{
	return itsTopRight;
}
point latitude_longitude_grid::BottomLeft() const
{
	return itsBottomLeft;
}
point latitude_longitude_grid::TopLeft() const
{
	return itsTopLeft;
}
point latitude_longitude_grid::BottomRight() const
{
	return itsBottomRight;
}
void latitude_longitude_grid::TopRight(const point& theTopRight)
{
	itsTopRight = theTopRight;
	UpdateCoordinates();
}

void latitude_longitude_grid::BottomLeft(const point& theBottomLeft)
{
	itsBottomLeft = theBottomLeft;
	UpdateCoordinates();
}

void latitude_longitude_grid::TopLeft(const point& theTopLeft)
{
	itsTopLeft = theTopLeft;
	UpdateCoordinates();
}

void latitude_longitude_grid::BottomRight(const point& theBottomRight)
{
	itsBottomRight = theBottomRight;
	UpdateCoordinates();
}

point latitude_longitude_grid::FirstPoint() const
{
	switch (itsScanningMode)
	{
		case kTopLeft:
			return itsTopLeft;
		case kBottomLeft:
			return itsBottomLeft;
		default:
			return point();
	}
}

point latitude_longitude_grid::LastPoint() const
{
	switch (itsScanningMode)
	{
		case kTopLeft:
			return itsBottomRight;
		case kBottomLeft:
			return itsTopRight;
		default:
			return point();
	}
}

point latitude_longitude_grid::XY(const himan::point& latlon) const
{
	double x = (latlon.X() - itsBottomLeft.X()) / Di();

	if (itsIsGlobal && (x < 0 || x > static_cast<double>(Ni()) - 1))
	{
		// wrap x if necessary
		// this might happen f.ex. with EC where grid start at 0 meridian and
		// we interpolate from say -10 to 40 longitude

		while (x < 0)
		{
			x += static_cast<double>(Ni());
		}
		while (x > static_cast<double>(Ni()) - 1)
		{
			x -= static_cast<double>(Ni()) - 1.;
		}
	}

	double y;

	switch (itsScanningMode)
	{
		case kBottomLeft:
			y = (latlon.Y() - itsBottomLeft.Y()) / Dj();
			break;
		case kTopLeft:
			y = (itsTopLeft.Y() - latlon.Y()) / Dj();
			break;
		default:
			throw runtime_error("Scanning mode not supported: " + HPScanningModeToString.at(itsScanningMode));
			break;
	}

	return point(x, y);
}

point latitude_longitude_grid::LatLon(size_t locationIndex) const
{
	ASSERT(itsNi != static_cast<size_t>(kHPMissingInt));
	ASSERT(itsNj != static_cast<size_t>(kHPMissingInt));
	ASSERT(!IsKHPMissingValue(Di()));
	ASSERT(!IsKHPMissingValue(Dj()));
	ASSERT(locationIndex < itsNi * itsNj);

	point firstPoint = FirstPoint();

	double j = floor(static_cast<double>(locationIndex / itsNi));
	double i = fmod(static_cast<double>(locationIndex), static_cast<double>(itsNi));

	point ret(firstPoint.X() + i * Di(), kHPMissingInt);

	switch (itsScanningMode)
	{
		case kBottomLeft:
			ret.Y(firstPoint.Y() + j * Dj());
			break;

		case kTopLeft:
			ret.Y(firstPoint.Y() - j * Dj());
			break;
		default:
			throw runtime_error("Scanning mode not supported: " + HPScanningModeToString.at(itsScanningMode));
	}

	return ret;
}

void latitude_longitude_grid::Ni(size_t theNi)
{
	itsNi = theNi;
}
void latitude_longitude_grid::Nj(size_t theNj)
{
	itsNj = theNj;
}
size_t latitude_longitude_grid::Ni() const
{
	return itsNi;
}
size_t latitude_longitude_grid::Nj() const
{
	return itsNj;
}
void latitude_longitude_grid::Di(double theDi)
{
	itsDi = theDi;
}
void latitude_longitude_grid::Dj(double theDj)
{
	itsDj = theDj;
}
double latitude_longitude_grid::Di() const
{
	if (IsKHPMissingValue(itsDi) && itsNi != static_cast<size_t>(kHPMissingInt) &&
	    !IsKHPMissingValue(FirstPoint().X()) && !IsKHPMissingValue(LastPoint().X()))
	{
		double fx = FirstPoint().X();
		double lx = LastPoint().X();

		if (fx == 0. && lx < 0)
			lx += 360;
		itsDi = fabs((fx - lx) / (static_cast<double>(itsNi) - 1.));
	}

	return itsDi;
}

double latitude_longitude_grid::Dj() const
{
	if (IsKHPMissingValue(itsDj) && itsNj != static_cast<size_t>(kHPMissingInt) &&
	    !IsKHPMissingValue(FirstPoint().X()) && !IsKHPMissingValue(LastPoint().X()))
	{
		itsDj = fabs((FirstPoint().Y() - LastPoint().Y()) / (static_cast<double>(itsNj) - 1.));
	}

	return itsDj;
}

void latitude_longitude_grid::FirstPoint(const point& theFirstPoint)
{
	switch (itsScanningMode)
	{
		case kBottomLeft:
			itsBottomLeft = theFirstPoint;
			break;
		case kTopLeft:
			itsTopLeft = theFirstPoint;
			break;
		default:
			break;
	}
	UpdateCoordinates();
}

void latitude_longitude_grid::LastPoint(const point& theLastPoint)
{
	switch (itsScanningMode)
	{
		case kBottomLeft:
			itsTopRight = theLastPoint;
			break;
		case kTopLeft:
			itsBottomRight = theLastPoint;
			break;
		default:
			break;
	}
	UpdateCoordinates();
}

void latitude_longitude_grid::UpdateCoordinates() const
{
	const point missing;

	if ((itsBottomLeft != missing && itsTopRight != missing) && (itsTopLeft == missing || itsBottomRight == missing))
	{
		if (itsBottomLeft.X() == 0. && itsTopRight.X() < 0)
		{
			itsTopRight.X(itsTopRight.X() + 360.);
		}

		itsTopLeft = point(itsBottomLeft.X(), itsTopRight.Y());
		itsBottomRight = point(itsTopRight.X(), itsBottomLeft.Y());
	}
	else if ((itsBottomLeft == missing || itsTopRight == missing) &&
	         (itsTopLeft != missing && itsBottomRight != missing))
	{
		if (itsTopLeft.X() == 0. && itsBottomRight.X() < 0)
		{
			itsBottomRight.X(itsBottomRight.X() + 360.);
		}

		itsBottomLeft = point(itsTopLeft.X(), itsBottomRight.Y());
		itsTopRight = point(itsBottomRight.X(), itsTopLeft.Y());
	}

	Di();
	Dj();

	if (FirstPoint() != missing && LastPoint() != missing && !IsKHPMissingValue(Di()))
	{
		double span = itsBottomLeft.X() + itsTopRight.X() + Di();
		itsIsGlobal = (span == 0. || span == 360.);
	}
}

bool latitude_longitude_grid::operator!=(const grid& other) const
{
	return !(other == *this);
}
bool latitude_longitude_grid::operator==(const grid& other) const
{
	const latitude_longitude_grid* g = dynamic_cast<const latitude_longitude_grid*>(&other);

	if (g)
	{
		return EqualsTo(*g);
	}

	return false;
}

bool latitude_longitude_grid::EqualsTo(const latitude_longitude_grid& other) const
{
	if (!grid::EqualsTo(other))
	{
		return false;
	}

	if (itsBottomLeft != other.BottomLeft())
	{
		itsLogger.Trace("BottomLeft does not match: X " + to_string(itsBottomLeft.X()) + " vs " +
		                to_string(other.BottomLeft().X()));
		itsLogger.Trace("BottomLeft does not match: Y " + to_string(itsBottomLeft.Y()) + " vs " +
		                to_string(other.BottomLeft().Y()));
		return false;
	}

	if (itsTopRight != other.TopRight())
	{
		itsLogger.Trace("TopRight does not match: X " + to_string(itsTopRight.X()) + " vs " +
		                to_string(other.TopRight().X()));
		itsLogger.Trace("TopRight does not match: Y " + to_string(itsTopRight.Y()) + " vs " +
		                to_string(other.TopRight().Y()));
		return false;
	}

	const double kEpsilon = 0.0001;

	if (fabs(Di() - other.Di()) > kEpsilon)
	{
		itsLogger.Trace("Di does not match: " + to_string(Di()) + " vs " + to_string(other.Di()));
		return false;
	}

	if (fabs(Dj() - other.Dj()) > kEpsilon)
	{
		itsLogger.Trace("Dj does not match: " + to_string(Dj()) + " vs " + to_string(other.Dj()));
		return false;
	}

	if (itsNi != other.Ni())
	{
		itsLogger.Trace("Ni does not match: " + to_string(itsNi) + " vs " + to_string(other.Ni()));
		return false;
	}

	if (itsNj != other.Nj())
	{
		itsLogger.Trace("Nj does not match: " + to_string(itsNj) + " vs " + to_string(other.Nj()));
		return false;
	}

	return true;
}

unique_ptr<grid> latitude_longitude_grid::Clone() const
{
	return unique_ptr<grid>(new latitude_longitude_grid(*this));
}

ostream& latitude_longitude_grid::Write(std::ostream& file) const
{
	grid::Write(file);

	file << itsBottomLeft << itsTopLeft << itsTopRight << itsBottomRight << "__itsNi__ " << itsNi << endl
	     << "__itsNj__ " << itsNj << endl
	     << "__itsDi__ " << itsDi << endl
	     << "__itsDj__ " << itsDj << endl
	     << "__itsIsGlobal__" << itsIsGlobal << endl;

	return file;
}

rotated_latitude_longitude_grid::rotated_latitude_longitude_grid() : latitude_longitude_grid(), itsSouthPole()
{
	itsGridType = kRotatedLatitudeLongitude;
	itsLogger = logger("rotated_latitude_longitude_grid");
}

rotated_latitude_longitude_grid::rotated_latitude_longitude_grid(HPScanningMode theScanningMode, point theBottomLeft,
                                                                 point theTopRight, point theSouthPole,
                                                                 bool initiallyRotated)
    : latitude_longitude_grid(theScanningMode, theBottomLeft, theTopRight), itsSouthPole(theSouthPole)
{
	if (!initiallyRotated)
		throw std::runtime_error("Unable to create rotated_latitude_longitude_grid with unrotated coordinates, yet");

	itsGridType = kRotatedLatitudeLongitude;
	itsLogger = logger("rotated_latitude_longitude_grid");
}

rotated_latitude_longitude_grid::rotated_latitude_longitude_grid(const rotated_latitude_longitude_grid& other)
    : latitude_longitude_grid(other), itsSouthPole(other.itsSouthPole)
{
	itsLogger = logger("rotated_latitude_longitude_grid");
}

rotated_latitude_longitude_grid::~rotated_latitude_longitude_grid() = default;

bool rotated_latitude_longitude_grid::operator!=(const grid& other) const
{
	return !(other == *this);
}
bool rotated_latitude_longitude_grid::operator==(const grid& other) const
{
	const rotated_latitude_longitude_grid* g = dynamic_cast<const rotated_latitude_longitude_grid*>(&other);

	if (g)
	{
		return EqualsTo(*g);
	}

	return false;
}

bool rotated_latitude_longitude_grid::EqualsTo(const rotated_latitude_longitude_grid& other) const
{
	if (!grid::EqualsTo(other) || !latitude_longitude_grid::EqualsTo(other))
	{
		return false;
	}

	if (itsSouthPole != other.SouthPole())
	{
		itsLogger.Trace("SouthPole does not match: X " + to_string(itsSouthPole.X()) + " vs " +
		                to_string(other.SouthPole().X()));
		itsLogger.Trace("SouthPole does not match: Y " + to_string(itsSouthPole.Y()) + " vs " +
		                to_string(other.SouthPole().Y()));
		return false;
	}

	return true;
}

unique_ptr<grid> rotated_latitude_longitude_grid::Clone() const
{
	return unique_ptr<grid>(new rotated_latitude_longitude_grid(*this));
}

point rotated_latitude_longitude_grid::SouthPole() const
{
	return itsSouthPole;
}
void rotated_latitude_longitude_grid::SouthPole(const point& theSouthPole)
{
	itsSouthPole = theSouthPole;
}
point rotated_latitude_longitude_grid::XY(const point& latlon) const
{
	if (!itsRotLatLonArea)
	{
		InitNewbaseArea();
	}

	auto rotpoint = itsRotLatLonArea->ToRotLatLon(NFmiPoint(latlon.X(), latlon.Y()));

	return latitude_longitude_grid::XY(point(rotpoint.X(), rotpoint.Y()));
}

point rotated_latitude_longitude_grid::LatLon(size_t locationIndex) const
{
	point rll = latitude_longitude_grid::LatLon(locationIndex);  // rotated coordinates

	if (!itsRotLatLonArea)
	{
		InitNewbaseArea();
	}

	auto regpoint = itsRotLatLonArea->ToRegLatLon(NFmiPoint(rll.X(), rll.Y()));

	return point(regpoint.X(), regpoint.Y());
}

point rotated_latitude_longitude_grid::RotatedLatLon(size_t locationIndex) const
{
	return latitude_longitude_grid::LatLon(locationIndex);
}

ostream& rotated_latitude_longitude_grid::Write(std::ostream& file) const
{
	latitude_longitude_grid::Write(file);

	file << itsSouthPole;

	return file;
}

void rotated_latitude_longitude_grid::InitNewbaseArea() const
{
	call_once(itsNewbaseAreaFlag, [&]() {
		ASSERT(itsBottomLeft != point());
		ASSERT(itsTopRight != point());
		ASSERT(itsSouthPole != point());

		itsRotLatLonArea = unique_ptr<NFmiRotatedLatLonArea>(new NFmiRotatedLatLonArea(
		    NFmiPoint(itsBottomLeft.X(), itsBottomLeft.Y()), NFmiPoint(itsTopRight.X(), itsTopRight.Y()),
		    NFmiPoint(itsSouthPole.X(), itsSouthPole.Y()), NFmiPoint(0., 0.), NFmiPoint(1., 1.), true));
	});
}
