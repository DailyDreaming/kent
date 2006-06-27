#!/bin/tcsh
cd $WEEKLYBLD

# ------------------------------------
# Note - this script can NOT be launched from beta
#  using something like ssh $BOX32 $WEEKLYBLD/buildCgi32.csh
#  because when scp needs the password typed in, apparently
#  the stdin is not available from the terminal.
# Instead, log directly into box32 and execute the script.
#  then when prompted for the password, put in the qateam pwd. 
# ------------------------------------

if ("$HOST" != "$BOX32") then
 echo "error: you must run this script on $BOX32!"
 exit 1
endif

cd $BUILDDIR/v${BRANCHNN}_branch/kent/src

make libs >& make.log
sed -i -e "s/-DJK_WARN//g" make.log
sed -i -e "s/-Werror//g" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "libs errs found:"
 echo "$res"
 exit 1
endif
#
echo "Make alpha."
cd hg
make CGI_BIN=$BUILDDIR/cgi-bin-32 alpha >& make.alpha.log
sed -i -e "s/-DJK_WARN//g" make.alpha.log
sed -i -e "s/-Werror//g" make.alpha.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.alpha.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "alpha errs found:"
 echo "$res"
 echo "ignore the one error from vgGetText for now."
 #exit 1
endif
#

cd $BUILDDIR/cgi-bin-32
rm all.joiner
rm galaAvail.tab
rm -fr hgNearData
rm -fr hgGeneData
rm -fr visiGeneData
rm -fr hgcData
echo "the customTrack loader is to be released soon, then remove the next line:"
rm -fr loader
echo "the hgSession and hgCustom cgi is to be released soon, then remove the next line:"
if (-e hgSession) then
    rm hgSession
endif    
if (-e hgSession) then
    rm hgCustom
endif    

#echo "debug: skipping scp"
scp -p * qateam@hgdownload:/mirrordata/apache/cgi-bin-i386/

echo
echo "32-bit cgis built on $HOST and scp'd to hgdownload"
#
exit 0

