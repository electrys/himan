/* 
 * File:   packed_data.h
 * Author: partio
 *
 * Created on April 5, 2013, 10:36 PM
 */

#ifndef PACKED_DATA_H
#define	PACKED_DATA_H

#include "matrix.h"

using namespace himan;

class packed_data : public uc_matrix_t
{
public:
	packed_data() {}
	virtual ~packed_data() {}
	packed_data(const packed_data& other) : uc_matrix_t(other) {};
	
	virtual std::string ClassName() const { return "packed_data"; }

protected:

};

class simple_packed : public packed_data
{
public:
	simple_packed();
	simple_packed(int theBitsPerValue, double theBinaryScaleFactor, double theDecimaleScaleFactor, double theReferenceValue);

	simple_packed(const simple_packed& other);
	
	virtual ~simple_packed() {}

	virtual std::string ClassName() const { return "simple_packed"; }

	
private:
	int itsBitsPerValue;
	double itsBinaryScaleFactor;
	double itsDecimalScaleFactor;
	double itsReferenceValue;
	size_t itsDataLength;


};

inline simple_packed::simple_packed() {}

inline simple_packed::simple_packed(int theBitsPerValue, double theBinaryScaleFactor, double theDecimalScaleFactor, double theReferenceValue)
	: itsBitsPerValue(theBitsPerValue)
	, itsBinaryScaleFactor(theBinaryScaleFactor)
	, itsDecimalScaleFactor(theDecimalScaleFactor)
	, itsReferenceValue(theReferenceValue)
{}

#endif	/* PACKED_DATA_H */

