#!/bin/sh

set -x

if [ -z "$HIMAN" ]; then
	export HIMAN="../../himan"
fi

rm -f RR*grib

echo "Hirlam cannot be tested until neons has been fixed with proper parameter information"
echo "Currently hirlam has data with indicatorOfParameter=4 but neons has it with value 0"
echo "We cannot fix this until we get rid of hil_pp which does not understand multiple rows"
echo "for one parameter in table grid_param_grib"

exit 0 

$HIMAN -d 5 -f precipitation_hl.json -t grib hl_source.grib --no-cuda -s hl_nocuda

grib_compare hl_result_3_24.grib RR-3-MM_height_0_rll_1030_816_0_024.grib

if [ $? -ne 0 ];then
  echo precipitation/hirlam failed on CPU
  exit 1
fi

grib_compare hl_result_3_25.grib RR-3-MM_height_0_rll_1030_816_0_025.grib

if [ $? -ne 0 ];then
  echo precipitation/hirlam failed on CPU
  exit 1
fi

grib_compare hl_result_6_24.grib RR-6-MM_height_0_rll_1030_816_0_024.grib

if [ $? -ne 0 ];then
  echo precipitation/hirlam failed on CPU
  exit 1
fi

grib_compare hl_result_6_25.grib RR-6-MM_height_0_rll_1030_816_0_025.grib

if [ $? -eq 0 ];then
  echo precipitation/hirlam success on CPU!
else
  echo precipitation/hirlam failed on CPU
  exit 1
fi

$HIMAN -d 5 -f precipitation_hl_rate.json -t grib hl_source_rate.grib --no-cuda -s hl_nocuda

grib_compare hl_result_rrr_24.grib RRR-KGM2_height_0_rll_1030_816_0_024.grib

if [ $? -ne 0 ];then
  echo precipitation/hirlam failed on CPU
  exit 1
fi

grib_compare hl_result_rrr_25.grib RRR-KGM2_height_0_rll_1030_816_0_025.grib

if [ $? -eq 0 ];then
  echo precipitation/hirlam success on CPU!
else
  echo precipitation/hirlam failed on CPU
  exit 1
fi
