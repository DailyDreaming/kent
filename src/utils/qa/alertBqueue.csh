#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  10-03-2008
#
#  checks pushQ for B entries and send email to developer and QA
#
#  Robert Kuhn
#
################################

set go=""
# make lists of substitutions for email addresses
set counter=( 1 2 3 4 5 6 7 8 9 10 11 12 )
set alias=( andy belinda  brian  brooke bob  fan    jim  jing larry  mark  rachel  zach     )
set email=( aamp giardine braney rhead  kuhn fanhsu kent jzhu larrym markd hartera jsanborn )


if ( $#argv != 1  ) then
  echo
  echo "  checks pushQ for B entries and sends email to developer and QA."
  echo "  uses hard-coded aliases to get email address from nicknames."
  echo
  echo "    usage:  go"
  echo
  exit
else
  set go=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
  echo "\n error: you must run this script on dev!\n"
  exit 1
endif

if ( $go != "go" ) then
  echo 
  echo ' only the argument "go" is allowed.'
  echo 
  exit 1
endif 

# echo "testing \n \n sending only to ann and bob right now \n \n "  > Bfile
echo "greetings. \n\n  you have content in the B-queue that someone should look at." > Bfile
echo "  this is a periodic reminder from a QA cronjob.\n" >> Bfile
hgsql -h $sqlbeta -t -e "SELECT dbs, track, reviewer, sponsor, \
  qadate FROM pushQ WHERE priority = 'B' ORDER BY qadate" qapushq >> Bfile

# get list of all developers and QA involved in B-queue tracks
set contacts=`hgsql -N -h $sqlbeta -e "SELECT sponsor, reviewer, sponsor FROM pushQ \
  WHERE priority = 'B'" qapushq`
# clean up list to get unique names
set contacts=`echo $contacts | sed "s/,/ /g" | sed "s/ /\n/g" \
  | perl -wpe '$_ = lcfirst($_);' | sort -u`

set debug=false
if ( $debug == "true" ) then
  echo "\ncontacts $contacts"
    set contacts=`echo $contacts | sed "s/,/ /" | sed "s/ /\n/g" \
    | perl -wpe '$_ = lcfirst($_);' | sort -u`
  set contacts="larrym kate, fan ting ann Hiram rachel Andy andy bob larry kayla"
  echo "contacts $contacts"
endif

# replace common names with email addresses
foreach i ( $counter )
  set contacts=`echo $contacts | sed "s/$alias[$i] /$email[$i] /g"`
  if ( $debug == "true" ) then
    echo    here5 $i
    echo    $alias[$i]
    echo    $email[$i]
    echo "   contacts $contacts"
    ## send output only to selected people
    # set contacts="ann kuhn pauline rhead kayla"
    # set contacts="pauline rhead ann kayla"
    echo "   contacts $contacts"
  endif 
end

if ( $debug == "true" ) then
  exit
endif

# add donnak to list
set contacts="$contacts donnak"

# cat Bfile | mail -c $contacts'@soe.ucsc.edu' -s "test. ignore  " $USER
cat Bfile | mail -c $contacts'@soe.ucsc.edu' -s "B-queue alert" $USER
rm Bfile

