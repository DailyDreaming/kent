#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
    echo "error: dozip.csh must be executed from hgwbeta!"
    exit 1
endif

./makeZip.csh
set err = $status
if ( $err ) then
    echo "error running makezip.csh: $err" 
    exit 1
endif 
./buildZip.csh
set err = $status
if ( $err ) then
    echo "error running buildzip.csh: $err" 
    exit 1
endif

echo "removing old jksrc zip and symlink"
ssh qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/jksrc.zip"
ssh qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/jksrc.v*.zip"
echo "scp-ing jksrc.v${BRANCHNN}.zip to hgdownload"
scp -p $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" qateam@hgdownload:/mirrordata/apache/htdocs/admin/
echo "updating jksrc.zip symlink"
ssh qateam@hgdownload "cd /mirrordata/apache/htdocs/admin/;ln -s jksrc.v${BRANCHNN}.zip jksrc.zip"

echo "scp-ing js/*.js files to hgdownload"
scp -p $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/js/*.js qateam@hgdownload:/mirrordata/apache/htdocs/js
