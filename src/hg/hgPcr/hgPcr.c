/* hgPcr - In-silico PCR CGI for UCSC. */
#include "common.h"
#include "hash.h"
#include "errabort.h"
#include "errCatch.h"
#include "hCommon.h"
#include "dystring.h"
#include "jksql.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "gfPcrLib.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "dbDb.h"
#include "blatServers.h"
#include "web.h"
#include "botDelay.h"

static char const rcsid[] = "$Id: hgPcr.c,v 1.3 2004/06/07 23:41:55 kent Exp $";

struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgPcr - In-silico PCR CGI for UCSC\n"
  "usage:\n"
  "   hgPcr XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct pcrServer
/* Information on a server. */
   {
   struct pcrServer *next;  /* Next in list. */
   char *db;		/* Database name. */
   char *genome;	/* Genome name. */
   char *description;	/* Assembly description */
   char *host;		/* Name of machine hosting server. */
   char *port;		/* Port that hosts server. */
   char *seqDir;	/* Directory of sequence files. */
   };

struct pcrServer *getServerList()
/* Get list of available servers. */
{
struct pcrServer *serverList = NULL, *server;
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;

/* Do a little join to get data to fit into the pcrServer. */
sr = sqlGetResult(conn, 
   "select dbDb.name,dbDb.genome,dbDb.description,blatServers.host,"
   "blatServers.port,dbDb.nibPath "
   "from dbDb,blatServers where "
   "dbDb.name = blatServers.db "
   "and blatServers.canPcr = 1 order by dbDb.orderKey" );
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(server);
    server->db = cloneString(row[0]);
    server->genome = cloneString(row[1]);
    server->description = cloneString(row[2]);
    server->host = cloneString(row[3]);
    server->port = cloneString(row[4]);
    server->seqDir = cloneString(row[5]);
    slAddHead(&serverList, server);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
if (serverList == NULL)
    errAbort("Sorry, no PCR servers are available");
slReverse(&serverList);
return serverList;
}

struct pcrServer *findServer(char *db, struct pcrServer *serverList)
/* Return server for given database.  Db can either be
 * database name or description. */
{
struct pcrServer *server;
for (server = serverList; server != NULL; server = server->next)
    {
    if (sameString(db, server->db))
        return server;
    }
errAbort("Can't find a server for PCR database %s\n", db);
return NULL;
}

void doHelp()
/* Print up help page */
{
puts(
"<H1>In-Silico PCR User Guide</H1>\n"
"In-Silico PCR searches a sequence database with a pair of\n"
"PCR primers.  It uses an indexing strategy to do this quickly.\n"
"\n"
"<H2>Controls</H2>\n"
"<B>Forward Primer</B> - The primers must be at least 15 bases long.<BR>\n"
"<B>Reverse Primer</B> - This is on the opposite strand from the forward primer.<BR>\n"
"<B>Max Product Size</B> - Maximum size of amplified region.<BR>\n"
"<B>Min Perfect Match</B> - Number of bases that match exactly on 3' end of primers.  Must be at least 15.<BR>\n"
"<B>Min Good Match</B> - Number of bases on 3' end of primers where at least 2 out of 3 bases match.<BR>\n"
"<B>Database</B> - Which sequence database to search.<BR>\n"
"\n"
"<H2>Output</H2>\n"
"When the search is successful the output is a fasta format sequence\n"
"file containing all the regions in the database that lie between the \n"
"primer pair.  The fasta header describes the region in the database\n"
"and the primers.  The fasta body is capitalized where the primer\n"
"sequence matches the database sequence and lowercase elsewhere.  Here\n"
"is an example:<BR>\n"
"<TT><PRE>\n"
">chr22:31000551+31001000  TAACAGATTGATGATGCATGAAATGGG CCCATGAGTGGCTCCTAAAGCAGCTGC\n"
"TtACAGATTGATGATGCATGAAATGGGgggtggccaggggtggggggtga\n"
"gactgcagagaaaggcagggctggttcataacaagctttgtgcgtcccaa\n"
"tatgacagctgaagttttccaggggctgatggtgagccagtgagggtaag\n"
"tacacagaacatcctagagaaaccctcattccttaaagattaaaaataaa\n"
"gacttgctgtctgtaagggattggattatcctatttgagaaattctgtta\n"
"tccagaatggcttaccccacaatgctgaaaagtgtgtaccgtaatctcaa\n"
"agcaagctcctcctcagacagagaaacaccagccgtcacaggaagcaaag\n"
"aaattggcttcacttttaaggtgaatccagaacccagatgtcagagctcc\n"
"aagcactttgctctcagctccacGCAGCTGCTTTAGGAGCCACTCATGaG\n"
"</PRE></TT>\n"
"The + between the coordinates in the fasta header indicates that\n"
"this is on the positive strand.  \n"
);
}

void showGenomes(char *genome, struct pcrServer *serverList)
/* Put up drop-down list with genomes on it. */
{
struct hash *uniqHash = hashNew(8);
struct pcrServer *server;
char *onChangeText = "onchange=\"document.orgForm.org.value = "
    " document.mainForm.org.options[document.mainForm.org.selectedIndex].value; "
    " document.orgForm.wp_f.value = document.mainForm.wp_f.value; "
    " document.orgForm.wp_r.value = document.mainForm.wp_r.value; "
    " document.orgForm.wp_size.value = document.mainForm.wp_size.value; "
    " document.orgForm.wp_perfect.value = document.mainForm.wp_perfect.value; "
    " document.orgForm.wp_good.value = document.mainForm.wp_good.value; "
    " document.orgForm.db.value = 0; "
    " document.orgForm.submit();\"";

printf("<SELECT NAME=\"org\" %s>\n", onChangeText);
for (server = serverList; server != NULL; server = server->next)
    {
    if (!hashLookup(uniqHash, server->genome))
        {
	hashAdd(uniqHash, server->genome, NULL);
	printf("  <OPTION%s VALUE=\"%s\">%s</OPTION>\n", 
	    (sameWord(genome, server->genome) ? " SELECTED" : ""), 
	    server->genome, server->genome);
	}
    }
printf("</SELECT>\n");
hashFree(&uniqHash);
}

void showAssemblies(char *genome, char *db, struct pcrServer *serverList)
/* Put up drop-down list with assemblies on it. */
{
struct pcrServer *server;

printf("<SELECT NAME=\"db\">\n");
for (server = serverList; server != NULL; server = server->next)
    {
    if (sameWord(genome, server->genome))
	printf("  <OPTION%s VALUE=%s>%s</OPTION>\n", 
	    (sameString(db, server->db) ? " SELECTED" : ""), 
	    server->db, server->description);
    }
printf("</SELECT>\n");
}

void doGetPrimers(char *db, char *organism, struct pcrServer *serverList,
	char *fPrimer, char *rPrimer, int maxSize, int minPerfect, int minGood)
/* Put up form to get primers. */
{
struct pcrServer *server;

printf("<FORM ACTION=\"../cgi-bin/hgPcr\" METHOD=\"GET\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

printf("<TABLE BORDER=0 WIDTH=\"96%%\" COLS=7><TR>\n");

printf("%s", "<TD><CENTER>\n");
printf("Genome:<BR>");
showGenomes(organism, serverList);
printf("%s", "</TD>\n");

printf("%s", "<TD><CENTER>\n");
printf("Assembly:<BR>");
showAssemblies(organism, db, serverList);
printf("%s", "</TD>\n");

printf("%s", "<TD COLWIDTH=2><CENTER>\n");
printf("Forward Primer:<BR>");
cgiMakeTextVar("wp_f", fPrimer, 22);
printf("%s", "</TD>\n");

printf("%s", "<TD><CENTER COLWIDTH=2>\n");
printf(" Reverse Primer:<BR>");
cgiMakeTextVar("wp_r", rPrimer, 22);
printf("%s", "</TD>\n");

printf("%s", "<TD><CENTER>\n");
printf("&nbsp;<BR>");
cgiMakeButton("Submit", "Submit");
printf("%s", "</TD>\n");

printf("</TR></TABLE><BR>");

printf("<TABLE BORDER=0 WIDTH=\"96%%\" COLS=4><TR>\n");
printf("%s", "<TD><CENTER>\n");
printf("Max Product Size: ");
cgiMakeIntVar("wp_size", maxSize, 5);
printf("%s", "</TD>\n");

printf("%s", "<TD><CENTER>\n");
printf(" Min Perfect Match Size: ");
cgiMakeIntVar("wp_perfect", minPerfect, 2);
printf("%s", "</TD>\n");

printf("%s", "<TD><CENTER>\n");
printf(" Min Good Match Size: ");
cgiMakeIntVar("wp_good", minGood, 2);
printf("%s", "</TD>\n");

printf("%s", "<TD><CENTER>\n");
printf(" <A HREF=\"../cgi-bin/hgPcr?wp_help=on\" TARGET=\"_blank\">User Guide</A> \n");
printf("%s", "</TD>\n");
printf("</TR></TABLE><BR>");

printf("</FORM>\n");

/* Put up a second form who's sole purpose is to preserve state
 * when the user flips the genome button. */
printf("<FORM ACTION=\"../cgi-bin/hgPcr\" METHOD=\"GET\" NAME=\"orgForm\">"
       "<input type=\"hidden\" name=\"wp_foo\" value=\"\">\n"
       "<input type=\"hidden\" name=\"db\" value=\"\">\n"
       "<input type=\"hidden\" name=\"org\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_f\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_r\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_size\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_perfect\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_good\" value=\"\">\n"
       "<input type=\"hidden\" name=\"wp_showPage\" value=\"true\">\n");
cartSaveSession(cart);
cartSetString(cart, "db", db);
printf("</FORM>\n");

}

boolean doPcr(struct pcrServer *server,
	char *fPrimer, char *rPrimer, int maxSize, int minPerfect, int minGood)
/* Do the PCR, and show results. */
{
struct errCatch *errCatch = errCatchNew();
boolean ok = FALSE;

if (errCatchStart(errCatch))
    {
    struct gfPcrInput *gpi;
    struct gfPcrOutput *gpoList;

    AllocVar(gpi);
    gpi->fPrimer = fPrimer;
    gpi->rPrimer = rPrimer;
    gpoList = gfPcrViaNet(server->host, server->port, 
    	server->seqDir, gpi, maxSize, minPerfect, minGood);
    if (gpoList != NULL)
	{
	struct dyString *url = newDyString(0);
	dyStringPrintf(url, "%s?%s&db=%s", 
	    hgTracksName(), cartSidUrlString(cart), server->db);
	dyStringAppend(url, "&position=%s:%d-%d");

	printf("<TT><PRE>");
	gfPcrOutputWriteAll(gpoList, "fa", url->string, "stdout");
	printf("</PRE></TT>");
        ok = TRUE;
	dyStringFree(&url);
	}
    else
	errAbort("No matches to %s %s in %s %s", fPrimer, rPrimer, 
		server->genome, server->description);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
     {
     warn(errCatch->message->string);
     }
errCatchFree(&errCatch); 
return ok;
}

void dispatch()
/* Look at input variables and figure out which page to show. */
{
char *userSeq;
char *db, *organism;
char *showPage = FALSE;
int maxSize = 4000;
int minPerfect = 15;
int minGood = 15;
char *fPrimer = cartUsualString(cart, "wp_f", "");
char *rPrimer = cartUsualString(cart, "wp_r", "");
struct pcrServer *serverList = getServerList();

getDbAndGenome(cart, &db, &organism);

/* Get variables. */
maxSize = cartUsualInt(cart, "wp_size", maxSize);
minPerfect = cartUsualInt(cart, "wp_perfect", minPerfect);
minGood = cartUsualInt(cart, "wp_good", minGood);
if (minPerfect < 15)
     minPerfect = 15;
if (minGood < minPerfect)
     minGood = minPerfect;

/* Decide based on transient variables what page to put up. 
 * By default put up get primer page. */
if (cartVarExists(cart, "wp_help"))
    {
    doHelp();
    return;
    }
else if (cartVarExists(cart, "wp_f") && cartVarExists(cart, "wp_r") &&
	!cartVarExists(cart, "wp_showPage"))
    {
    fPrimer = gfPcrMakePrimer(fPrimer);
    rPrimer = gfPcrMakePrimer(rPrimer);
    if (doPcr(findServer(db, serverList), fPrimer, rPrimer, maxSize, minPerfect, minGood))
         return;
    }
doGetPrimers(db, organism, serverList,
	fPrimer, rPrimer, maxSize, minPerfect, minGood);
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
cart = theCart;
dnaUtilOpen();
cartWebStart(cart, "UCSC In-Silico PCR");
dispatch();
cartWebEnd();
}


char *excludeVars[] = {"Submit", "submit", 
	"wp_f", "wp_r", "wp_help", "wp_showPage", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);
htmlSetBackground("../images/floret.jpg");
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

