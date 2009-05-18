/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "customTrack.h"
#include "customFactory.h"
#include "hui.h"

static char const rcsid[] = "$Id: refreshNamedSessionCustomTracks.c,v 1.9 2009/05/18 21:38:07 galt Exp $";

#define savedSessionTable "namedSessionDb"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "refreshNamedSessionCustomTracks -- scan central database's %s\n"
  "    contents for custom tracks and touch any that are found, to prevent\n"
  "    them from being removed by the custom track cleanup process.\n"
  "usage:\n"
  "    refreshNamedSessionCustomTracks hgcentral[test,beta]\n"
  "This is intended to be run as a nightly cron job for each central db.\n"
  "The ~/.hg.conf file (or $HGDB_CONF) must specify the same central db\n"
  "as the command line.  [The command line arg exists only to suppress this\n"
  "message].\n"
  "\n",
  savedSessionTable
  );
}

/* Options: */
static struct optionSpec options[] = {
    {"hardcore", OPTION_BOOLEAN}, /* Intentionally omitted from usage(). */
    {NULL, 0},
};

char *scanSettingsForCT(char *userName, char *sessionName, char *contents,
			int *pLiveCount, int *pExpiredCount)
/* Parse the CGI-encoded session contents into {var,val} pairs and search
 * for custom tracks.  If found, refresh the custom track.  Parsing code 
 * taken from cartParseOverHash. 
 * If any nonexistent custom track files are found, return a SQL update
 * command that will remove those from this session.  We can't just do 
 * the update here because that messes up the caller's query. */
{
int contentLength = strlen(contents);
struct dyString *newContents = dyStringNew(contentLength+1);
struct dyString *oneSetting = dyStringNew(contentLength / 4);
char *updateIfAny = NULL;
char *contentsToChop = cloneString(contents);
char *namePt = contentsToChop;
verbose(3, "Scanning %s %s\n", userName, sessionName);
while (isNotEmpty(namePt))
    {
    char *dataPt = strchr(namePt, '=');
    char *nextNamePt;
    if (dataPt == NULL)
	errAbort("Mangled session content string %s", namePt);
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt != NULL)
	*nextNamePt++ = 0;
    dyStringClear(oneSetting);
    dyStringPrintf(oneSetting, "%s=%s%s",
		   namePt, dataPt, (nextNamePt ? "&" : ""));
    if (startsWith(CT_FILE_VAR_PREFIX, namePt))
	{
	boolean thisGotLiveCT = FALSE, thisGotExpiredCT = FALSE;
	cgiDecode(dataPt, dataPt, strlen(dataPt));
	verbose(3, "Found variable %s = %s\n", namePt, dataPt);
	/* If the file does not exist, omit this setting from newContents so 
	 * it doesn't get copied from session to session.  If it does exist,
	 * leave it up to customFactoryTestExistence to parse the file for 
	 * possible customTrash table references, some of which may exist 
	 * and some not. */
	if (! fileExists(dataPt))
	    {
	    verbose(3, "Removing %s from %s %s\n", oneSetting->string,
		    userName, sessionName);
	    thisGotExpiredCT = TRUE;
	    }
	else
	    {
	    char *db = namePt + strlen(CT_FILE_VAR_PREFIX);
	    dyStringAppend(newContents, oneSetting->string);
	    customFactoryTestExistence(db, dataPt,
				       &thisGotLiveCT, &thisGotExpiredCT);
	    }
	if (thisGotLiveCT && pLiveCount != NULL)
	    (*pLiveCount)++;
	if (thisGotExpiredCT && pExpiredCount != NULL)
	    (*pExpiredCount)++;
	if (thisGotExpiredCT)
	    {
	    if (verboseLevel() >= 3)
		verbose(3, "Found expired custom track in %s %s: %s\n",
			userName, sessionName, dataPt);
	    else
		verbose(2, "Found expired custom track: %s\n", dataPt);
	    }
	if (thisGotLiveCT)
	    verbose(4, "Found live custom track: %s\n", dataPt);
	}
    else
	dyStringAppend(newContents, oneSetting->string);
    namePt = nextNamePt;
    }
if (newContents->stringSize != contentLength)
    {
    struct dyString *update = dyStringNew(contentLength*2);
    if (newContents->stringSize > contentLength)
	errAbort("Uh, why is newContents (%d) longer than original (%d)??",
		 newContents->stringSize, contentLength);
    dyStringPrintf(update, "UPDATE %s set contents='", savedSessionTable);
    dyStringAppendN(update, newContents->string, newContents->stringSize);
    dyStringPrintf(update, "', lastUse=now(), useCount=useCount+1 "
		   "where userName=\"%s\" and sessionName=\"%s\";",
		   userName, sessionName);
    verbose(3, "Removing one or more dead CT file settings from %s %s "
	    "(original length %d, now %d)\n", 
	    userName, sessionName,
	    contentLength, newContents->stringSize);
    updateIfAny = dyStringCannibalize(&update);
    }
dyStringFree(&oneSetting);
dyStringFree(&newContents);
freeMem(contentsToChop);
return updateIfAny;
}

void refreshNamedSessionCustomTracks(char *centralDbName)
/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */
{
struct sqlConnection *conn = hConnectCentral();
struct slPair *updateList = NULL, *update;
char *actualDbName = sqlGetDatabase(conn);
int liveCount=0, expiredCount=0;

setUdcCacheDir();  /* programs that use udc must call this to initialize cache dir location */

if (!sameString(centralDbName, actualDbName))
    errAbort("Central database specified in hg.conf file is %s but %s "
	     "was specified on the command line.",
	     actualDbName, centralDbName);
else
    verbose(2, "Got connection to %s\n", centralDbName);

if (sqlTableExists(conn, savedSessionTable))
    {
    struct sqlResult *sr = NULL;
    char **row = NULL;
    char query[512];
    safef(query, sizeof(query),
	  "select userName,sessionName,contents from %s "
	  "order by userName,sessionName", savedSessionTable);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *updateIfAny = scanSettingsForCT(row[0], row[1], row[2],
					      &liveCount, &expiredCount);
	if (updateIfAny)
	    {
	    AllocVar(update);
	    update->name = updateIfAny;
	    slAddHead(&updateList, update);
	    }
	}
    sqlFreeResult(&sr);
    }

/* Now that we're done reading from savedSessionTable, we can modify it: */
if (optionExists("hardcore"))
    {
    for (update = updateList;  update != NULL;  update = update->next)
	sqlUpdate(conn, update->name);
    }
hDisconnectCentral(&conn);
verbose(1, "Found %d live and %d expired custom tracks in %s.\n",
	liveCount, expiredCount, centralDbName);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
setCurrentDir(CGI_BIN);
refreshNamedSessionCustomTracks(argv[1]);
return 0;
}

