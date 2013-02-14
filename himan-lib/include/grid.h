/**
 * @file grid.h
 *
 * @date Jan 23, 2013
 * @author partio
 */

#ifndef GRID_H
#define GRID_H

/**
 * @class grid
 *
 * @brief Overcoat to matrix, ie. data + metadata concerning the data
 */

#include <string>
#include "himan_common.h"
#include "point.h"
#include "logger.h"
#include "matrix.h"
#include <NFmiGrid.h>

namespace himan
{

class grid
{
	public:
		grid();
		grid(HPScanningMode theScanningMode,
				bool theUVRelativeToGrid,
				HPProjectionType theProjection,
				point theBottomLeft,
				point theTopRight,
				point theSouthPole = point(),
				double theOrientation = kHPMissingFloat);

		~grid() {}

		/**
		 * @brief Copy constructor for grid. Will create new data class instance.
		 *
		 */

		grid(const grid& other);
		grid& operator=(const grid& other) = delete;


	    std::string ClassName() const
	    {
	        return "himan::grid";
	    }

	    HPVersionNumber Version() const
	    {
	        return HPVersionNumber(0, 1);
	    }

	    std::ostream& Write(std::ostream& file) const;

	    /**
		 * @return Number of points along X axis
		 */

		size_t Ni() const;

		/**
		 * @return Number of points along Y axis
		 */

		size_t Nj() const;

		/**
		 * @return Distance between two points in X axis in degrees
		 */

		double Di() const;

		/**
		 * @return Distance between two points in Y axis in degrees
		 */

		double Dj() const;

		void Ni(size_t theNi);
		void Nj(size_t theNj);

		void Di(double theDi);
		void Dj(double theDj);

		/**
		 *
		 * @return Data matrix
		 */

		std::shared_ptr<d_matrix_t> Data() const;

	    /**
	     * @brief Replace current data matrix with the function argument
	     * @param d shared pointer to a data matrix
	     */

	    void Data(std::shared_ptr<d_matrix_t> d);

		HPScanningMode ScanningMode() const;
	    void ScanningMode(HPScanningMode theScanningMode);

	    /**
	     * @return True if parameter UV components are grid relative, false if they are earth-relative.
	     * On parameters with no UV components this has no meaning.
	     */

	    bool UVRelativeToGrid() const;
	    void UVRelativeToGrid(bool theUVRelativeToGrid);

	    /**
	     * @brief Set the data value pointed by the iterators with a new one
	     * @return True if assignment was succesfull
	     */

	    bool Value(size_t locationIndex, double theValue);

	    /**
	     * @return Data value pointed by the iterators
	     */

	    double Value(size_t locationIndex) const;

	    /**
	     * @return Projection type of this info
	     *
	     * One info can hold only one type of projection
	     */

	    HPProjectionType Projection() const;
	    void Projection(HPProjectionType theProjection);

	    point BottomLeft() const;
	    point TopRight() const;

	    void BottomLeft(const point& theBottomLeft);
	    void TopRight(const point& theTopRight);

	    void Orientation(double theOrientation);
	    double Orientation() const;

	    point SouthPole() const;
	    void SouthPole(const point& theSouthPole);

	    /**
	     * @brief Calculates latitude and longitude of first grid point based on the area definition and scanning mode
	     * @return First grid point of the grid
	     * @todo How this works with stereographic projection
	     */

	    point FirstGridPoint() const;

	    /**
	     * @brief Calculates latitude and longitude of last grid point based on the area definition and scanning mode
	     * @return Last grid point of the grid
	     * @todo How this works with stereographic projection
	     */

	    point LastGridPoint() const;

	    /**
	     * @brief Calculate area coordinates from first gridpoint, scanning mode, grid size and distance between two gridpoints.
	     *
	     * This function is the opposite of FirstGridPoint(). NOTE: scanning mode must already be set when calling this function!
	     *
	     * @param firstPoint Latitude and longitude of first gridpoint
	     * @param ni Grid size in X direction
	     * @param ny Grid size in Y direction
	     * @param di Distance between two points in X direction
	     * @param dj Distance between two points in Y direction
	     *
	     * @return True if calculation is successful
	     */

	    bool SetCoordinatesFromFirstGridPoint(const point& firstPoint, size_t ni, size_t nj, double di, double dj);


	    /**
	     * @brief Create a newbase grid (NFmiGrid) from current data
	     * @return Raw pointer to NFmiGrid
	     */

	    NFmiGrid* ToNewbaseGrid() const;


	    /**
	     * @brief Check if grid and area are equal
	     */

	    bool operator==(const grid& other) const;
	    bool operator!=(const grid& other) const;

	private:

		std::shared_ptr<d_matrix_t> itsData;

	    HPScanningMode itsScanningMode; //<! When data is read from files, we need to know what is the scanning mode

	    bool itsUVRelativeToGrid; //<! If true, wind UV components are relative to grid north and east (ie. are not earth-relative)

	    HPProjectionType itsProjection;

	    point itsBottomLeft;
		point itsTopRight;
		point itsSouthPole;

		double itsOrientation;

	    std::unique_ptr<logger> itsLogger;

	    mutable double itsDi;
	    mutable double itsDj;

};

inline
std::ostream& operator<<(std::ostream& file, const grid& ob)
{
    return ob.Write(file);
}

} // namespace himan

#endif /* GRID_H */
