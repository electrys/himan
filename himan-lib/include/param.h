/*
 * param.h
 *
 *  Created on: Nov 27, 2012
 *      Author: partio
 *
 * This class holds all necessary parameter information, basically referring to
 * those parameters defined in Neons.
 * (In routine operations we always need to have parameter support in Neons since
 * otherwise we cannot store the calculated parameter)
 *
 * Ad-hoc interpreted calculations differ from this a bit: the source and result parameter
 * can be defined only in the source data (basically always querydata) which means
 * we cannot have a very rigorous validation of the parameters.
 *
 */

#ifndef PARAM_H
#define PARAM_H

#include "logger.h"
#include <NFmiParam.h>
#include "NFmiGlobals.h" // FmiInterpolatioMethod

namespace himan
{

class param
{

public:

    param();
    param(const std::string& theName);
    param(const std::string& theName, unsigned long theUnivId);

    param(const std::string& theName, unsigned long theUnivId,
          float theScale,
          float theBase,
          const std::string& thePrecision = "%.1f",
          FmiInterpolationMethod theInterpolationMethod = kNearestPoint);

    ~param() {}

    param(const param& other);
    param& operator=(const param& other);

    std::string ClassName() const
    {
        return "himan::param";
    }

    HPVersionNumber Version() const
    {
        return HPVersionNumber(0, 1);
    }

    bool operator==(const param& other);
    bool operator!=(const param& other);

    /**
     * @brief Set grib parameter number (used both for grib 1 and 2)
     */

    void GribParameter(long theGribParameter);
    long GribParameter() const;

    /**
     * @brief Set grib parameter discipline (grib2)
     */

    void GribDiscipline(long theGribDiscipline);
    long GribDiscipline() const;

    /**
     * @brief Set grib parameter category (grib2)
     */

    void GribCategory(long theGribCategory);
    long GribCategory() const;

    /**
     * @brief Set grib parameter table version number (grib1)
     */

    void GribTableVersion(long theVersion);
    long GribTableVersion() const;

    /**
     * @brief Set universal id (newbase)
     */

    unsigned long UnivId() const;
    void UnivId(unsigned long theUnivId);

    std::string Name() const;
    void Name(std::string theName);

    /**
     *
     * @return Unit of parameter
     */

    HPParameterUnit Unit() const;
    void Unit(HPParameterUnit theUnit);

    std::ostream& Write(std::ostream& file) const;

private:

    std::shared_ptr<NFmiParam> itsParam; //!< newbase param will hold name, univ_id and scale+base

    long itsGribParameter; //!< Grib parameter number whether in grib 1 or 2
    long itsGribCategory; //!< Grib parameter category (only for grib2)
    long itsGribDiscipline; //!< Grib parameter discipline (only for grib2)
    long itsGribTableVersion; //!< Grib table version (only in grib 1)

    HPParameterUnit itsUnit; //!< Unit of the parameter
    double itsMissingValue; //!< Missing value (default kFloatMissing)

    std::unique_ptr<logger> itsLogger;

};

inline
std::ostream& operator<<(std::ostream& file, param& ob)
{
    return ob.Write(file);
}

} // namespace himan

#endif /* PARAM_H */
