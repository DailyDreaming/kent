/* Implementation file for some portability stuff mostly aimed
 * at making the same code run under different web servers.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "htmshell.h"
#include "portable.h"
#include "obscure.h"
#include "portimpl.h"

static char const rcsid[] = "$Id: portimpl.c,v 1.8 2003/05/06 07:33:43 kate Exp $";

static struct webServerSpecific *wss = NULL;

static void setupWss()
{
if (wss == NULL)
    {
    char *s = getenv("SERVER_SOFTWARE");
    wss = &wssDefault;
    if (s == NULL)
        {
	wss = &wssCommandLine;
        }
    else
        {
        if (strncmp(wssMicrosoftII.name, s, strlen(wssMicrosoftII.name)) == 0)
            wss = &wssMicrosoftII;
        else if (strncmp(wssMicrosoftPWS.name, s, strlen(wssMicrosoftPWS.name)) == 0)
            wss = &wssMicrosoftPWS;
	else 
	    {
	    char *t = getenv("HTTP_HOST");
	    if (t != NULL)
		{
		if (sameWord(t, "Crunx"))
		    wss = &wssLinux;
		}
	    }
        }
    }
}

void makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
setupWss();
wss->makeTempName(tn,base,suffix);
}

char *cgiDir()
{
setupWss();
return wss->cgiDir();
}

char *cgiSuffix()
{
setupWss();
return wss->cgiSuffix();
}
    
double machineSpeed()
/* Return relative speed of machine.  UCSC CSE dept. 1999 web server is 1.0 */
{
setupWss();
return wss->speed();
}

