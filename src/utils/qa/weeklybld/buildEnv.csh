setenv BRANCHNN 193
setenv TODAY 2008-10-14     # v193 final
setenv LASTWEEK 2008-09-30     # v192 final
setenv REVIEWDAY 2008-10-07    # preview of v193
setenv LASTREVIEWDAY 2008-09-23    # preview of v192

setenv WEEKLYBLD /cluster/bin/build/scripts
setenv BOX32 titan

setenv CVSROOT /projects/compbiousr/cvsroot
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
    setenv JAVA_HOME /usr/java/jdk1.5.0_10
endif
