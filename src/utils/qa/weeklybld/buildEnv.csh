setenv BRANCHNN 142
setenv TODAY 2006-09-11     # v142 final
setenv LASTWEEK 2006-08-28  # v141 final
setenv REVIEWDAY 2006-09-18  # preview of v143
setenv LASTREVIEWDAY 2006-09-04 # preview of v142

setenv WEEKLYBLD /cluster/bin/build/scripts
setenv BOX32 hgwdevold

setenv CVSROOT /projects/compbio/cvsroot
setenv CVS_RSH ssh

setenv MYSQLINC /usr/include/mysql
if ( "$MACHTYPE" == "x86_64" ) then
    setenv MYSQLLIBS '/usr/lib64/mysql/libmysqlclient.a -lz'
else
    setenv MYSQLLIBS '/usr/lib/mysql/libmysqlclient.a -lz'
endif

if ( "$HOST" == "$BOX32" ) then
    setenv BUILDDIR /scratch/releaseBuild
endif
if ( "$HOST" == "hgwbeta" ) then
    setenv BUILDDIR /data/tmp/releaseBuild
endif
if ( "$HOST" == "hgwdev" ) then
    setenv JAVABUILD /scratch/javaBuild
endif
