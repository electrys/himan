/**
 * @file grid.cpp
 *
 * @date Jan 23, 2013
 * @author partio
 */

#include "grid.h"


using namespace himan;
using namespace std;

grid::grid() 
	: itsData(0, 0, 1, kFloatMissing)
	, itsGridClass(kUnknownGridClass)
	, itsGridType(kUnknownGridType) 
	, itsAB()
	, itsScanningMode(kUnknownScanningMode)
	, itsPackedData()
{}

grid::grid(HPGridClass theGridClass, HPGridType theGridType)
	: itsData(0, 0, 1, kFloatMissing)
	, itsGridClass(theGridClass)
	, itsGridType(theGridType)
	, itsAB()
	, itsScanningMode(kUnknownScanningMode)
	, itsPackedData()
{}

grid::grid(HPGridClass theGridClass, HPGridType theGridType, HPScanningMode theScanningMode)
	: itsData(0, 0, 1, kFloatMissing)
	, itsGridClass(theGridClass)
	, itsGridType(theGridType)
	, itsAB()
	, itsScanningMode(theScanningMode)
	, itsPackedData()
{}

grid::~grid() {}

grid::grid(const grid& other)
	: itsData(other.itsData)
	, itsGridClass(other.itsGridClass)
	, itsGridType(other.itsGridType)
	, itsAB(other.itsAB)
	, itsScanningMode(other.itsScanningMode)
	, itsPackedData()
{
#ifdef HAVE_CUDA
	if (other.itsPackedData)
	{
		switch (other.itsPackedData->packingType)
		{
			case kSimplePacking:
				itsPackedData = unique_ptr<simple_packed> (new simple_packed(*dynamic_cast<simple_packed*> (other.itsPackedData.get())));
				break;

			default:
				itsPackedData = unique_ptr<packed_data> (new packed_data(*itsPackedData));
				break;
		}
	}
#endif
}

bool grid::EqualsTo(const grid& other) const
{
	if (other.itsGridType != itsGridType)
	{
		itsLogger->Trace("Grid type does not match: " + HPGridTypeToString.at(itsGridType) + " vs " + HPGridTypeToString.at(other.Type()));
		return false;
	}
	
	if (other.itsGridClass != itsGridClass)
	{
		return false;
	}
	
	if (itsAB.size() != other.itsAB.size())
	{
		return false;
	}
	else
	{
		for (size_t i = 0; i < itsAB.size(); i++)
		{
			if (fabs(itsAB[i] - other.itsAB[i]) > 0.0001)
			{
				return false;
			}
		}
	}
	
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

HPScanningMode grid::ScanningMode() const
{
	return itsScanningMode;
}

void grid::ScanningMode(HPScanningMode theScanningMode)
{
	itsScanningMode = theScanningMode;
}

bool grid::IsPackedData() const
{
	if (itsPackedData && itsPackedData->HasData())
	{
		return true;
	}

	return false;
}

matrix<double>& grid::Data()
{
	return itsData;
}

void grid::Data(const matrix<double>& theData)
{
	itsData = theData;
}

ostream& grid::Write(std::ostream& file) const
{
	file << "<" << ClassName() << ">" << std::endl;

	file << "__itsScanningMode__ " << HPScanningModeToString.at(itsScanningMode) << std::endl;
	
	file << "__itsAB__" ;
	
	for (size_t i = 0; i < itsAB.size(); i++)
	{
		file << " " << itsAB[i] ;
	}
	
	file << std::endl;
	
	file << "__IsPackedData__ " << IsPackedData() << std::endl;
	file << itsData;

	return file;
	
}

size_t grid::Size() const
{
	throw runtime_error("grid::Size() called");
}

bool grid::Value(size_t theLocationIndex, double theValue)
{
	return itsData.Set(theLocationIndex, theValue) ;
}

double grid::Value(size_t theLocationIndex) const
{
	return double(itsData.At(theLocationIndex));
}

grid* grid::Clone() const
{
	throw runtime_error("grid::Clone() called");
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