/**
 * @file grid.cpp
 *
 */

#include "grid.h"

using namespace himan;
using namespace std;

grid::grid()
    : itsGridClass(kUnknownGridClass),
      itsGridType(kUnknownGridType),
      itsIdentifier(),
      itsAB(),
      itsUVRelativeToGrid(false),
      itsEarthShape()
{
}

grid::grid(const grid& other)
    : itsGridClass(other.itsGridClass),
      itsGridType(other.itsGridType),
      itsIdentifier(other.itsIdentifier),
      itsAB(other.itsAB),
      itsUVRelativeToGrid(other.itsUVRelativeToGrid),
      itsEarthShape(other.itsEarthShape)
{
}

bool grid::EqualsTo(const grid& other) const
{
	if (other.itsGridType != itsGridType)
	{
		itsLogger.Trace("Grid type does not match: " + HPGridTypeToString.at(itsGridType) + " vs " +
		                HPGridTypeToString.at(other.Type()));
		return false;
	}

	// Comparison of earth shape turned off for now
	if ((false) && other.itsEarthShape != itsEarthShape)
	{
		itsLogger.Trace("Earth shape does not match: A: " + to_string(other.itsEarthShape.A()) + " vs " +
		                to_string(itsEarthShape.A()) + " and B: " + to_string(other.itsEarthShape.B()) + " vs " +
		                to_string(itsEarthShape.B()));
		return false;
	}

	if (other.itsGridClass != itsGridClass)
	{
		itsLogger.Trace("Grid class does not match: " + HPGridClassToString.at(itsGridClass) + " vs " +
		                HPGridClassToString.at(other.itsGridClass));
		return false;
	}

	// Disabled for now
	if ((false) && other.itsIdentifier != itsIdentifier)
	{
		itsLogger.Trace("Identifier does not match: " + itsIdentifier + " vs " + other.itsIdentifier);
		return false;
	}

	// We DON'T test for AB !
	// Why?

	return true;
}

HPGridType grid::Type() const
{
	return itsGridType;
}
void grid::Type(HPGridType theGridType)
{
	itsGridType = theGridType;
}
HPGridClass grid::Class() const
{
	return itsGridClass;
}
void grid::Class(HPGridClass theGridClass)
{
	itsGridClass = theGridClass;
}
std::string grid::Identifier() const
{
	return itsIdentifier;
}
void grid::Identifier(const std::string& theIdentifier)
{
	itsIdentifier = theIdentifier;
}

ostream& grid::Write(std::ostream& file) const
{
	file << "<" << ClassName() << ">" << std::endl;

	file << "__itsUVRelativeToGrid__ " << itsUVRelativeToGrid << std::endl;
	file << "__itsAB__";

	for (size_t i = 0; i < itsAB.size(); i++)
	{
		file << " " << itsAB[i];
	}

	file << std::endl;

	file << itsEarthShape;
	file << "__itsIdentifier__" << itsIdentifier << std::endl;

	return file;
}

size_t grid::Size() const
{
	throw runtime_error("grid::Size() called");
}
vector<double> grid::AB() const
{
	return itsAB;
}
void grid::AB(const vector<double>& theAB)
{
	itsAB = theAB;
}
point grid::LatLon(size_t theLocationIndex) const
{
	throw runtime_error("grid::LatLon() called");
}
bool grid::operator!=(const grid& other) const
{
	return !(other == *this);
}
bool grid::operator==(const grid& other) const
{
	return EqualsTo(other);
}
bool grid::UVRelativeToGrid() const
{
	return itsUVRelativeToGrid;
}
void grid::UVRelativeToGrid(bool theUVRelativeToGrid)
{
	itsUVRelativeToGrid = theUVRelativeToGrid;
}

earth_shape<double> grid::EarthShape() const
{
	return itsEarthShape;
}

void grid::EarthShape(const earth_shape<double>& theEarthShape)
{
	itsEarthShape = theEarthShape;
}

//--------------- regular grid
regular_grid::regular_grid() : grid(), itsScanningMode(kUnknownScanningMode)
{
	Class(kRegularGrid);
}

regular_grid::~regular_grid()
{
}
regular_grid::regular_grid(const regular_grid& other) : grid(other), itsScanningMode(other.itsScanningMode)
{
}

bool regular_grid::EqualsTo(const regular_grid& other) const
{
	if (!grid::EqualsTo(other))
	{
		return false;
	}

	if (other.itsScanningMode != itsScanningMode)
	{
		return false;
	}

	return true;
}

HPScanningMode regular_grid::ScanningMode() const
{
	return itsScanningMode;
}
void regular_grid::ScanningMode(HPScanningMode theScanningMode)
{
	itsScanningMode = theScanningMode;
}

//--------------- irregular grid

irregular_grid::irregular_grid() : grid()
{
	Class(kIrregularGrid);
}

irregular_grid::~irregular_grid()
{
}

irregular_grid::irregular_grid(const irregular_grid& other) : grid(other)
{
}

bool irregular_grid::EqualsTo(const irregular_grid& other) const
{
	if (!grid::EqualsTo(other))
	{
		return false;
	}

	return true;
}
