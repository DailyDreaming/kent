#!/bin/tcsh

if (( "$HOST" != "hgwbeta" ) && ( "$HOST" != "$BOX32" )) then
 echo "Error: this script must be run from hgwbeta or $BOX32. [${0}: `date`]"
 exit 1
endif

cd $WEEKLYBLD

echo "BRANCHNN=$BRANCHNN"
if ( "$BRANCHNN" == "" ) then
 echo "BRANCHNN undefined."
 exit 1
endif

echo
echo "now unpacking new branch $BRANCHNN on `uname -n` [${0}: `date`]"

#unpack the new branch on BUILDDIR for beta
# for later: is this faster to co on kkstore or not?
cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch" 
if ( -d $dir ) then
 echo "removing old branch dir. [${0}: `date`]"
 rm -fr $dir 
endif
mkdir -p $dir
cd $dir
echo "Checking out branch $BRANCHNN. [${0}: `date`]"
git clone -q $GITSHAREDREPO kent
cd kent
git checkout -tb $dir origin/$dir
set err = $status
if ( $err ) then
 echo "error running git clone and checkout of kent in $BUILDDIR/$dir : $err [${0}: `date`]" 
 exit 1
endif 
cd ..


# configure settings like SSL and BAM in common.mk
echo "Configuring settings on branch $BRANCHNN in $BUILDDIR/$dir [${0}: `date`]" 
$WEEKLYBLD/configureSandbox.csh . $WEEKLYBLD/defaultBuildSettings.mk

echo "Done checking out branch $BRANCHNN in $BUILDDIR/$dir [${0}: `date`]" 
echo "Now you should go and do build on beta! [${0}: `date`]"

exit 0

