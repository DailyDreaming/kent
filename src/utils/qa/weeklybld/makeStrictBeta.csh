#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwbeta" ) then
echo "error: you must run this script on hgwbeta!"
exit 1
endif

cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch"
cd $dir

cd kent
pwd
# the makefile now does zoo automatically now when you call it
echo "trackDb Make strict."
cd $BUILDDIR/$dir/kent/src/hg/makeDb/trackDb
make strict >& make.strict.log
set res = `/bin/egrep -i "error|warn" make.strict.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "trackDb strict errs found:"
 echo "$res"
 exit 1
endif
set res = `/bin/egrep -i "html missing" make.strict.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "trackDb strict html errs found:"
 echo "$res"
endif

echo "trackDb Make strict done on Beta"
exit 0

