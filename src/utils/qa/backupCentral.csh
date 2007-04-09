#!/bin/tcsh

###############################################
# 
#  04-09-07
#  Robert Kuhn
#
#  Grab hgcentral* tables from dev, beta, rr and store as files.
# 
###############################################

set directory="" 
set today=""
set dirPath=""
set urlPath=""
set hgwdevString=""
set hgwbetaString=""
set rrString=""
 
set hgwdevString='hgcentraltest'
if ($#argv == 0 || $#argv > 1) then
  # no command line args
  echo
  echo "  grab hgcentral* tables from dev, beta, rr and store as files."
  echo
  echo "    usage:  go"
  echo
  exit
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

set directory="hgcentral" 
# set file paths and URLs
set today=`date +%Y-%m-%d`
# set today="2007-04-01"
set dirPath="/usr/local/apache/htdocs/qa/test-results/$directory"
set urlPath="http://hgwdev.cse.ucsc.edu/qa/test-results/$directory"
rm -rf $dirPath/$today/
mkdir -p $dirPath/$today
set urlPath="http://hgwdev.cse.ucsc.edu/qa/test-results/$directory/$today"

set hgwdevString='hgcentraltest'
set hgwbetaString='-h hgwbeta hgcentralbeta'
set rrString='-h genome-centdb hgcentral' 

foreach table ( blatServers clade dbDb defaultDb gdbPdb genomeClade liftOverChain )
  hgsql  $hgwdevString -N -e "SELECT * FROM $table" | sort >> $dirPath/$today/hgwdev.$table
  hgsql $hgwbetaString -N -e "SELECT * FROM $table" | sort >> $dirPath/$today/hgwbeta.$table
  hgsql      $rrString -N -e "SELECT * FROM $table" | sort >> $dirPath/$today/rr.$table
end

echo $urlPath


