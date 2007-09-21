#!/bin/tcsh

###############################################
#  05-10-04
# 
#  Checks for table values off the end of chrom
# 
###############################################

set db=""
set chr=""
set track=""
set end=""

if ($#argv != 2) then
  echo
  echo "  checks for entries beyond the end of the chromsome."
  echo '    finds the proper column names if "chrom", "tName" or "genoName".'
  echo
  echo "    usage:  database, table"
  echo
  exit
else
  set db=$1
  set track=$2
endif

set chr=""
set end=""

set chr=`getChromFieldName.csh $track $db`
if ($status) then
  exit 1
endif

set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "chromEnd"`
if ($status) then
  set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "tEnd"`
  if ($status) then
    set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "txEnd"`
    if ($status) then
      set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "repEnd"`
      if ($status) then
        echo '\n  '$db.$track' has no "chromEnd", "tEnd", "txEnd" or "repEnd" fields.\n'
         exit 1 
      endif
    endif 
  endif 
endif 


echo
echo "Looking for off the end of each chrom"

# Will also use the chromInfo table
hgsql -N -e "SELECT chromInfo.chrom, chromInfo.size - MAX($track.$end) \
     AS dist_from_end FROM chromInfo, $track \
     WHERE chromInfo.chrom = $track.$chr \
     GROUP BY chromInfo.chrom" $db > $db.$track.tx.offEnd


awk '{if($2<0) {print $2} }' $db.$track.tx.offEnd
echo "expect blank. any chrom with annotations off the end would be printed."
echo "results are in $db.$track.tx.offEnd.\n"


