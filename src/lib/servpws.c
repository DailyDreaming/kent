static char const rcsid[] = "$Id: servpws.c,v 1.3 2003/05/05 06:45:36 kate Exp $";

/* Stuff that's specific for the Personal Web Server goes here. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"


static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
long tempIx = incCounterFile("tcounter");
sprintf(tn->forCgi, "..\\trash\\%s%ld%s", base, tempIx, suffix);
sprintf(tn->forHtml, "..\\trash\\%s%ld%s", base, tempIx, suffix);
}

static char *_cgiDir()
{
return "../cgi-bin/";
}

static char *_cgiSuffix()
{
return ".exe";
}

static double _speed()
{
return 1.25;
}
        
struct webServerSpecific wssMicrosoftPWS =
    {
    "Microsoft-PWS",
    _makeTempName,
    _cgiDir,
    _cgiSuffix,
    _speed,
    };
