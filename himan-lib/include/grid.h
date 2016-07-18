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
 * @brief Interface for all grids
 */

#include <string>
#include "himan_common.h"
#include "point.h"
#include "logger.h"
#include "packed_data.h"

#include "matrix.h"

namespace himan
{

class grid
{
	public:
		grid();
		grid(HPGridClass theGridClass, HPGridType theGridType);
		grid(HPGridClass theGridClass, HPGridType theGridType, HPScanningMode theScanningMode);
		
		virtual ~grid();

		grid(const grid& other);
		grid& operator=(const grid& other) = delete;
		
		virtual std::string ClassName() const
		{
			return "himan::grid";
		}

		virtual bool operator==(const grid& other) const ;
		virtual bool operator!=(const grid& other) const ;
		
		virtual std::ostream& Write(std::ostream& file) const;

		virtual grid* Clone() const = 0;

		/* 
		 * Functions that are common and valid to all types of grids,
		 * and are implemented in this class. 
		 */
		
		HPGridType Type() const;
		void Type(HPGridType theGridType);
		
		HPGridClass Class() const ;
		void Class(HPGridClass theGridClass);
		
		matrix<double>& Data();
		void Data(const matrix<double>& d);
		
		std::vector<double> AB() const ;
		void AB(const std::vector<double>& theAB) ;

		/* 
		 * Functions that are common and valid to all types of grids.
		 * 
		 * For those functions that clearly have some kind of default
		 * implementation, that implementation is done in grid-class,
		 * but so that it can be overridden in inheriting classes.
		 * 
		 * Functions whos implementation depends on the grid type are
		 * declared abstract should be implemented by deriving classes.
		 */
		
		virtual size_t Size() const;
		
		virtual bool Value(size_t locationIndex, double theValue);
		virtual double Value(size_t locationIndex) const;

		virtual point FirstPoint() const = 0;
		virtual point LastPoint() const = 0;
		virtual point LatLon(size_t locationIndex) const = 0 ;

		/*
		 * Functions that are only valid for some grid types, but for ease 
		 * of use they are declared here. It is up to the actual grid classes
		 * to implement correct functionality.
		 */

		virtual point BottomLeft() const = 0;
		virtual point TopRight() const = 0;

//		virtual void BottomLeft(const point& theBottomLeft) = 0 ;
//		virtual void TopRight(const point& theTopRight) = 0;
		
		virtual HPScanningMode ScanningMode() const;
		virtual void ScanningMode(HPScanningMode theScanningMode) ;

		virtual bool IsPackedData() const ;

		virtual bool Swap(HPScanningMode newScanningMode) = 0;
		
		virtual size_t Ni() const = 0;
		virtual size_t Nj() const = 0;
		
		virtual double Di() const = 0;
		virtual double Dj() const = 0;

	protected:
	
		bool EqualsTo(const grid& other) const;

		matrix<double> itsData; //<! Variable to hold unpacked data

		HPGridClass itsGridClass;
		HPGridType itsGridType;

		std::vector<double> itsAB;

		std::unique_ptr<logger> itsLogger;
		
		HPScanningMode itsScanningMode;
		std::unique_ptr<packed_data> itsPackedData; //<! Variable to hold packed data

};

inline
std::ostream& operator<<(std::ostream& file, const grid& ob)
{
	return ob.Write(file);
}

} // namespace himan

#endif /* GRID_H */
