#!/bin/tcsh


################################
#  06-15-04
#  Robert Kuhn
#
#  Runs joiner check, finding all identifiers for a table.
#
################################

set db=""
set table=""
set range=""
set joinerPath=""
# set joinerPath="~/schema"
set noTimes=""

if ( $#argv < 2 || $#argv > 4 ) then
  echo
  echo "  runs joiner check, -keys, finding all identifiers for a table."
  echo '  set database to "all" for global.'
  echo '  for chains/nets, use tablename format: chainDb#.'
  echo
  echo "    usage:  database, table, [path to all.joiner]"
  echo "           (defaults to tip of the tree) [noTimes]"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

if ( $#argv == 3 ) then
  set noTimes=$argv[3]
  if ( $noTimes != "noTimes" ) then
    set joinerPath=$argv[3]
    set noTimes=""
  endif
endif

if ( $#argv == 4 ) then
  set joinerPath=$argv[3]
  set noTimes=$argv[4]
  if ( $noTimes != "noTimes" ) then
    echo
    echo "${0}:"
    $0
    exit 1
  endif
endif

if ( $joinerPath == "" ) then
  # checkout tip of the tree
  if ( ! -d xxJoinDirxx ) then
    mkdir xxJoinDirxx
  endif
  set joinerPath="xxJoinDirxx"
  setenv CVS_RSH ssh
  cvs -d hgwdev:/projects/compbio/cvsroot co -d xxJoinDirxx \
    kent/src/hg/makeDb/schema/all.joiner >& /dev/null

  if ( $status ) then
    echo
    echo "  cvs check-out failed for all.joiner on $HOST"
    echo
    exit 1
  endif

endif

if ($db != "all") then
  set range="-database=$db"
endif

# set joinerFile and eliminate double "/" where present
set joinerFile=`echo ${joinerPath}/all.joiner | sed -e "s/\/\//\//"`

# --------------------------------------------

# get identifiers

# use chain identifier if table is netDb#
echo $table | grep "^net" >& /dev/null
if ( $status == 0 ) then
  set table=`echo $table | sed -e "s/net/chain/"`
endif

# check for chain identifiers
echo $table | grep "chain" >& /dev/null
if ( $status == 0 ) then
  echo "\nchain and net use same identifier"
  echo $table | grep "chainSelf" >& /dev/null
  if ( $status == 0 ) then
    echo ${table} > xxIDxx
  else
    echo ${table}Id > xxIDxx
  endif
else
  # set non-chain identifiers
  tac $joinerPath/all.joiner \
    | sed "/\.$table\./,/^identifier /\!d" | \
    grep "^identifier" | gawk '{print $2}' > xxIDxx
  if ( $status ) then
   # if no identifier, look for whether table is ignored
    echo
    tac $joinerPath/all.joiner \
      | sed "/$table/,/^tablesIgnored/\!d" | \
      grep "^tablesIgnored"
    if ( $status ) then 
      echo "\n  Identifier not found, and not in tablesIgnored"
    endif

    rm -f xxIDxx
    rm -fr xxJoinDirxx 
    echo
  exit 1
  endif
endif


if (-e xxIDxx) then
  set idVal=`wc -l xxIDxx | gawk '{print $1}'`
  echo
  if ($idVal != 0) then
    echo "found identifiers:"
    echo
    cat xxIDxx
  else
    echo "  no identifiers for this table"
  endif
  echo
endif

foreach identifier (`cat xxIDxx`)
  nice joinerCheck $range -identifier=$identifier -keys $joinerFile 
end

if ( $noTimes == "" ) then
  echo "running -times flag\n"
  nice joinerCheck $range -times $joinerFile
endif


rm -f xxIDxx
rm -fr xxJoinDirxx 
