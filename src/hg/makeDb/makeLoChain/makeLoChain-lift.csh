#!/bin/csh -ef
# Name:         makeLoChain-lift
#
# Function:     Lift alignments for liftOver chain.
#               Relies on directory structure created by makeLoChain-align
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-lift.csh,v 1.5 2005/09/13 21:41:34 kate Exp $

if ( $#argv != 3 ) then
    echo "usage: $0 <old-assembly> <new-assembly> <new-liftdir>"
    exit 1
endif

set oldAssembly = $1
set newAssembly = $2
set newLiftDir = $3

set blatDir = /cluster/data/$oldAssembly/bed/blat.$newAssembly.`date +%Y-%m-%d`

if ( ! -e $blatDir/raw ) then
    echo "Can't find $blatDir/raw"
endif

if (`ls -1 $newLiftDir/*.lft | wc -l` < 1) then
    echo "Can't find any .lft files in $newLiftDir"
    exit 1
endif

cd $blatDir/raw
set fs = `fileServer $blatDir`
if ( $HOST != $fs ) then
    echo "Run this on $fs"
    exit 1
endif

foreach chr (`awk '{print $1;}' /cluster/data/$newAssembly/chrom.sizes`)
    echo $chr
    liftUp -pslQ ../psl/$chr.psl $newLiftDir/$chr.lft warn chr*_$chr.psl
end

set execDir = $0:h
echo ""
echo "DO THIS NEXT:"
echo "    ssh kki"
echo "    $execDir/makeLoChain-chain.csh $oldAssembly <$oldAssembly-nibdir> $newAssembly <$newAssembly-nibdir>"
echo ""
exit 0

