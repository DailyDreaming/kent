/* schema - display info about database organization. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "obscure.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "trackDb.h"
#include "joiner.h"
#include "tableDescriptions.h"
#include "asParse.h"
#include "customTrack.h"
#include "bedCart.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: schema.c,v 1.27 2004/11/23 23:25:52 hiram Exp $";

static char *nbForNothing(char *val)
/* substitute &nbsp; for empty strings to keep table formating sane */
{
char *s = skipLeadingSpaces(val);
if ((s == NULL) || (s[0] == '\0'))
    return "&nbsp;";
else
    return val;
}

static char *abbreviateInPlace(char *val, int len)
/* Abbreviate a string to len characters.  */
{
int vlen = strlen(val);
if (vlen > len)
    strcpy(val+len-3, "...");
return val;
}

static char *cleanExample(char *val)
/* Abbreviate example if necessary and add non-breaking space if need be */
{
val = abbreviateInPlace(val, 30);
val = nbForNothing(val);
return val;
}

static struct slName *storeRow(struct sqlConnection *conn, char *query)
/* Just save the results of a single row query in a string list. */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
struct slName *list = NULL, *el;
int i, colCount = sqlCountColumns(sr);
if ((row = sqlNextRow(sr)) != NULL)
     {
     for (i=0; i<colCount; ++i)
         {
	 el = slNameNew(row[i]);
	 slAddTail(&list, el);
	 }
     }
sqlFreeResult(&sr);
return list;
}

void describeFields(char *db, char *table, 
	struct asObject *asObj, struct sqlConnection *conn)
/* Print out an HTML table showing table fields and types, and optionally 
 * offering histograms for the text/enum fields. */
{
struct sqlResult *sr;
char **row;
#define TOO_BIG_FOR_HISTO 500000
boolean tooBig = (sqlTableSize(conn, table) > TOO_BIG_FOR_HISTO);
char query[256];
struct slName *exampleList, *example;
boolean showItemRgb = FALSE;

showItemRgb=bedItemRgb(curTrack);	/* should we expect itemRgb */
					/*	instead of "reserved" */

safef(query, sizeof(query), "select * from %s limit 1", table);
exampleList = storeRow(conn, query);
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);

hTableStart();
hPrintf("<TR><TH>field</TH>");
if (exampleList != NULL)
    hPrintf("<TH>example</TH>");
hPrintf("<TH>SQL type</TH> ");
if (!tooBig)
    hPrintf("<TH>info</TH> ");
if (asObj != NULL)
    hPrintf("<TH>description</TH> ");
puts("</TR>\n");
example = exampleList;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (showItemRgb && (sameWord(row[0],"reserved")))
	hPrintf("<TR><TD><TT>itemRgb</TT></TD> ");
    else
	hPrintf("<TR><TD><TT>%s</TT></TD> ", row[0]);
    if (exampleList != NULL)
        {
	hPrintf("<TD>");
	if (example != NULL)
	     hPrintf("%s", cleanExample(example->name));
	else
	     hPrintf("n/a");
	hPrintf("</TD>");
	}
    hPrintf("<TD><TT>%s</TT></TD>", row[1]);
    if (!tooBig)
	{
	hPrintf(" <TD>");
	if (isSqlStringType(row[1]) && !sameString(row[1], "longblob"))
	    {
	    hPrintf("<A HREF=\"../cgi-bin/hgTables");
	    hPrintf("?%s", cartSidUrlString(cart));
	    hPrintf("&%s=%s", hgtaDatabase, db);
	    hPrintf("&%s=%s", hgtaHistoTable, table);
	    hPrintf("&%s=%s", hgtaDoValueHistogram, row[0]);
	    hPrintf("\">");
	    hPrintf("values");
	    hPrintf("</A>");
	    }
	else if (isSqlNumType(row[1]))
	    {
	    hPrintf("<A HREF=\"../cgi-bin/hgTables");
	    hPrintf("?%s", cartSidUrlString(cart));
	    hPrintf("&%s=%s", hgtaDatabase, db);
	    hPrintf("&%s=%s", hgtaHistoTable, table);
	    hPrintf("&%s=%s", hgtaDoValueRange, row[0]);
	    hPrintf("\">");
	    hPrintf("range");
	    hPrintf("</A>");
	    }
	else
	    {
	    hPrintf("&nbsp;");
	    }
	hPrintf("</TD>");
	}
    if (asObj != NULL)
        {
	struct asColumn *asCol = asColumnFind(asObj, row[0]);
	hPrintf(" <TD>");
	if (asCol != NULL)
	    hPrintf("%s", asCol->comment);
	else
	    {
	    if (sameString("bin", row[0]))
	       hPrintf("Indexing field to speed chromosome range queries.");
	    else
		hPrintf("&nbsp;");
	    }
	hPrintf("</TD>");
	}
    puts("</TR>");
    if (example != NULL)
	example = example->next;
    }
hTableEnd();
sqlFreeResult(&sr);
}

static void explainCoordSystem()
/* Our coord system is counter-intuitive to users.  Warn them in advance to 
 * reduce the frequency with which they find this "bug" on their own and 
 * we have to explain it on the genome list. */
{
puts("<P><I>Note: all start coordinates in our database are 0-based, not \n"
     "1-based.  See explanation \n"
     "<A HREF=\"http://genome.ucsc.edu/FAQ/FAQtracks#tracks1\">"
     "here</A>.</I></P>");
}


static void printSampleRows(int sampleCount, struct sqlConnection *conn, char *table)
/* Put up sample values. */
{
char query[256];
struct sqlResult *sr;
char **row;
int i, columnCount = 0;
int itemRgbCol = -1;
boolean showItemRgb = FALSE;

showItemRgb=bedItemRgb(curTrack);	/* should we expect itemRgb */
					/*	instead of "reserved" */

/* Make table with header row containing name of fields. */
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
hTableStart();
hPrintf("<TR>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (showItemRgb && sameWord(row[0],"reserved"))
	{
	hPrintf("<TH>itemRgb</TH>");
	itemRgbCol = columnCount;
	}
    else
	hPrintf("<TH>%s</TH>", row[0]);
    ++columnCount;
    }
hPrintf("</TR>");
sqlFreeResult(&sr);

/* Get some sample fields. */
safef(query, sizeof(query), "select * from %s limit %d", table, sampleCount);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<TR>");
    for (i=0; i<columnCount; ++i)
	{
	if (showItemRgb && (i == itemRgbCol))
	    {
	    int rgb = atoi(row[i]);
	    hPrintf("<TD>%d,%d,%d</TD>", (rgb & 0xff0000) >> 16,
		(rgb & 0xff00) >> 8, (rgb & 0xff));
	    }
	else
	    hPrintf("<TD>%s</TD>", row[i]);
	}
    hPrintf("</TR>\n");
    }
sqlFreeResult(&sr);
hTableEnd();
explainCoordSystem();
}

static int joinerPairCmpOnB(const void *va, const void *vb)
/* Compare two joinerPair based on b element of pair. */
{
const struct joinerPair *jpA = *((struct joinerPair **)va);
const struct joinerPair *jpB = *((struct joinerPair **)vb);
struct joinerDtf *a = jpA->b;
struct joinerDtf *b = jpB->b;
int diff;
diff = strcmp(a->database, b->database);
if (diff == 0)
   {
   diff = strcmp(a->table, b->table);
   if (diff == 0)
       diff = strcmp(a->field, b->field);
   }
return diff;
}


static boolean isViaIndex(struct joinerSet *jsPrimary, struct joinerDtf *dtf)
/* Return's TRUE if dtf is part of identifier only by an array index. */
{
struct joinerField *jf;
struct slRef *chain, *link;
struct joinerSet *js;
boolean retVal = FALSE;
boolean gotRetVal = FALSE;

chain = joinerSetInheritanceChain(jsPrimary);
for (link = chain; link != NULL; link = link->next)
    {
    js = link->val;
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
	{
	if (sameString(jf->table, dtf->table))
	    if (sameString(jf->field, dtf->field))
		if (slNameInList(jf->dbList, dtf->database))
		    {
		    retVal = jf->indexOf;
		    gotRetVal = TRUE;
		    break;
		    }
	}
    if (gotRetVal)
        break;
    }
slFreeList(&chain);
return retVal;
}

static void showSchemaDb(char *db, char *table)
/* Show schema to open html page. */
{
struct sqlConnection *conn = sqlConnect(db);
struct joiner *joiner = allJoiner;
struct joinerPair *jpList, *jp;
struct asObject *asObj = asForTable(conn, table);
char *splitTable = chromTable(conn, table);

hPrintf("<B>Database:</B> %s", db);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Primary Table:</B> %s", table);
if (!sameString(splitTable, table))
    hPrintf(" (%s)", splitTable);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Row Count:</B> ");
printLongWithCommas(stdout, sqlTableSize(conn, splitTable));
hPrintf("<BR>\n");
if (asObj != NULL)
    hPrintf("<B>Description:</B> %s<BR>", asObj->comment);
describeFields(db, splitTable, asObj, conn);

jpList = joinerRelate(joiner, db, table);
slSort(&jpList, joinerPairCmpOnB);
if (jpList != NULL)
    {
    webNewSection("Connected Tables and Joining Fields");
    for (jp = jpList; jp != NULL; jp = jp->next)
	{
	struct joinerSet *js = jp->identifier;
	boolean aViaIndex, bViaIndex;
	hPrintSpaces(6);
	hPrintf("%s.", jp->b->database);
	hPrintf("<A HREF=\"../cgi-bin/hgTables?");
	hPrintf("%s&", cartSidUrlString(cart));
	hPrintf("%s=%s&", hgtaDoSchemaDb, jp->b->database);
	hPrintf("%s=%s", hgtaDoSchemaTable, jp->b->table);
	hPrintf("\">");
	hPrintf("%s", jp->b->table);
	hPrintf("</A>");
	aViaIndex = isViaIndex(js, jp->a);
	bViaIndex = isViaIndex(js, jp->b);
	hPrintf(".%s ", jp->b->field);
	if (aViaIndex && bViaIndex)
	    {
	    hPrintf("(%s.%s and %s.%s are arrays sharing an index)",
	        jp->a->table, jp->a->field,
	        jp->b->table, jp->b->field);
	    	
	    }
	else if (aViaIndex)
	    {
	    hPrintf("(which is an array index into %s.%s)", 
	    	jp->a->table, jp->a->field);
	    }
	else if (bViaIndex)
	    {
	    hPrintf("(%s.%s is an array index into %s.%s)", 
		jp->a->table, jp->a->field,
	    	jp->b->table, jp->b->field);
	    }
	else
	    {
	    hPrintf("(via %s.%s)", jp->a->table, jp->a->field);
	    }
	hPrintf("<BR>\n");
	}
    }
webNewSection("Sample Rows");
printSampleRows(10, conn, splitTable);
}

static void showSchemaCtWiggle(char *table, struct customTrack *ct)
/* Show schema on wiggle format custom track. */
{
hPrintf("<B>Wiggle Custom Track ID:</B> %s<BR>\n", table);
hPrintf("Wiggle custom tracks are stored in a dense binary format.");
}

static void showSchemaCtBed(char *table, struct customTrack *ct)
/* Show schema on bed format custom track. */
{
struct bed *bed;
int count = 0;

/* Find named custom track. */
hPrintf("<B>Custom Track ID:</B> %s ", table);
hPrintf("<B>Field Count:</B> %d<BR>", ct->fieldCount);
hPrintf("On loading all custom tracks are converted to ");
hPrintf("<A HREF=\"/goldenPath/help/customTrack.html#BED\">BED</A> ");
hPrintf("format.");
webNewSection("Sample Rows");
hPrintf("<TT><PRE>");
for (bed = ct->bedList; bed != NULL && count < 10; bed = bed->next, ++count)
    bedTabOutN(bed, ct->fieldCount, stdout);
hPrintf("</PRE></TT>\n");
}

static void showSchemaCt(char *table)
/* Show schema on custom track. */
{
struct customTrack *ct = lookupCt(table);
if (ct->wiggle)
    showSchemaCtWiggle(table, ct);
else
    showSchemaCtBed(table, ct);
}

static void showSchema(char *db, char *table)
/* Show schema to open html page. */
{
if (isCustomTrack(table))
    showSchemaCt(table);
else
    showSchemaDb(db, table);
}

void doTableSchema(char *db, char *table, struct sqlConnection *conn)
/* Show schema around table. */
{
char parseBuf[256];
dbOverrideFromTable(parseBuf, &db, &table);
htmlOpen("Schema for %s", table);
showSchema(db, table);
htmlClose();
}

void doSchema(struct sqlConnection *conn)
/* Show schema around current track. */
{
if (curTrack != NULL && sameString(curTrack->tableName, curTable))
    {
    struct trackDb *track = curTrack;
    char *table = connectingTableForTrack(curTable);
    htmlOpen("Schema for %s - %s", track->shortLabel, track->longLabel);
    showSchema(database, table);
    htmlClose();
    }
else
    doTableSchema(database, curTable, conn);
}

