#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "genePred.h"
#include "dnaseq.h"
#include "dystring.h"
#include "htmshell.h"
#include "cart.h"
#include "jksql.h"
#include "knownInfo.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hgFind.h"
#include "hui.h"
#include "hash.h"
#include "fa.h"
#include "psl.h"
#include "nib.h"
#include "web.h"
#include "dbDb.h"
#include "kxTok.h"
#include "hgSeq.h"
#include "featureBits.h"
#include "bed.h"
#include "portable.h"
#include "customTrack.h"

/* sources of tracks, other than the current database: */
static char *hgFixed = "hgFixed";
static char *customTrackPseudoDb = "customTrack";

/* getCustomTracks() initializes this only once: */
struct customTrack *theCtList = NULL;

/* doMiddle() sets these: */
struct cart *cart = NULL;
char *database = NULL;
char *organism = NULL;
char *freezeName = NULL;
boolean tableIsPositional = FALSE;
boolean tableIsSplit = FALSE;
char fullTableName[256];
char *position = NULL;
char *chrom = NULL;
int winStart = 0;
int winEnd = 0;
boolean allGenome = FALSE;

/* main() sets this: */
struct hash *oldVars = NULL;

/* simple integer statistics */
struct intStats
{
    struct intStats *next;
    int n;
    int min;
    int max;
    double avg;
    double stdev;
};

void intStatsCopy(struct intStats *dst, struct intStats *src)
/* Copy src's fields into dst. */
{
dst->n = src->n;
dst->min = src->min;
dst->max = src->max;
dst->avg = src->avg;
dst->stdev = src->stdev;
}

void intStatsFromArr(int *X, int N, struct intStats *retStats)
/* simple statistics on n datapoints in X; results are stored in retStats. */
{
int min = BIGNUM;
int max = -BIGNUM;
double total;
double avg;
int i;

if (N < 1)
    errAbort("Don't call intStatsFromArr with 0 datapoints.");

total = 0.0;
for (i=0;  i < N;  i++)
    {
    if (X[i] < min)
	min = X[i];
    if (X[i] > max)
	max = X[i];
    total += (double)X[i];
    }
avg = total / N;
/* second pass for stdev: */
total = 0.0;
for (i=0;  i < N;  i++)
    {
    double diff = (double)X[i] - avg;
    total += (diff * diff);
    }
retStats->n = N;
retStats->min = min;
retStats->max = max;
retStats->avg = avg;
retStats->stdev = sqrt(total / N);
}


/* copied from hgGateway: */
static char * const onChangeText = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.submit();\"";

/* Droplist menu for selecting output type: */
#define allFieldsPhase      "Tab-separated, All fields"
#define chooseFieldsPhase   "Tab-separated, Choose fields..."
#define seqOptionsPhase     "FASTA (DNA sequence)..."
#define gffPhase            "GTF"
#define bedOptionsPhase     "BED/Custom Track..."
#define linksPhase          "Hyperlinks to Genome Browser"
#define statsPhase          "Summary/Statistics"
char *outputTypePosMenu[] =
{
    bedOptionsPhase,
    seqOptionsPhase,
    gffPhase,
    linksPhase,
    allFieldsPhase,
    chooseFieldsPhase,
    statsPhase,
};
int outputTypePosMenuSize = 7;
char *outputTypeNonPosMenu[] =
{
    allFieldsPhase,
    chooseFieldsPhase,
    statsPhase,
};
int outputTypeNonPosMenuSize = 3;
/* Other values that the "phase" var can take on: */
#define chooseTablePhase    "table"
#define outputOptionsPhase  "Advanced query..."
#define getOutputPhase      "Get results"
#define getSomeFieldsPhase  "Get these fields"
#define getSequencePhase    "Get sequence"
#define getBedPhase         "Get BED"
#define intersectOptionsPhase "Intersect Results..."
#define histPhase           "Get histogram"
/* Old "phase" values handled for backwards compatibility: */
#define oldAllFieldsPhase   "Get all fields"

/* Droplist menus for filtering on fields: */
char *ddOpMenu[] =
{
    "does",
    "doesn't"
};
int ddOpMenuSize = 2;

char *logOpMenu[] =
{
    "AND",
    "OR"
};
int logOpMenuSize = 2;

char *cmpOpMenu[] =
{
    "ignored",
    "in range",
    "<",
    "<=",
    "=",
    "!=",
    ">=",
    ">"
};
int cmpOpMenuSize = 8;

char *eqOpMenu[] =
{
    "ignored",
    "=",
    "!=",
};
int eqOpMenuSize = 3;

/* Droplist menu for custom track visibility: */
char *ctVisMenu[] =
{
    "hide",
    "dense",
    "squish",
    "pack",
    "full",
};
int ctVisMenuSize = 5;


void storeStringIfSet(char *var)
/* If var is in CGI, store it in cart. */
{
char *val = cgiOptionalString(var);
if (val != NULL)
    cartSetString(cart, var, val);
}

void storeBooleanIfSet(char *var)
/* If var is in CGI, store it in cart. */
{
boolean val = cgiBoolean(var);
if (cgiBooleanDefined(var) || val)
    cartSetBoolean(cart, var, val);
}

void saveChooseTableState()
/* Store in cart the user's settings in doChooseTable() form */
{
storeStringIfSet("table0");
storeStringIfSet("table1");
}

void saveOutputOptionsState()
/* Store in cart the user's settings in doOutputOptions() form */
{
storeStringIfSet("outputType");
storeStringIfSet("table2");
}

void saveChooseFieldsState()
/* Store in cart the user's settings in doChooseFields() form */
{
struct cgiVar *cv;

for (cv=cgiVarList();  cv != NULL;  cv=cv->next)
    {
    if (startsWith("field_", cv->name))
	cartSetBoolean(cart, cv->name, TRUE);
    }
}

void saveSequenceOptionsState()
/* Store in cart the user's settings in doSequenceOptions() form */
{
struct cgiVar *cv;
char shadowPrefix[128];

/* First pass: just store everything in cart as it appears in CGI. */
for (cv=cgiVarList();  cv != NULL;  cv=cv->next)
    {
    if (startsWith("hgSeq", cv->name))
	cartSetString(cart, cv->name, cv->val);
    }
/* Second pass: booleans may not appear in CGI; also, their representation 
 * is different so translate to boolean and store boolean. */
snprintf(shadowPrefix, sizeof(shadowPrefix), "%s%s",
	 cgiBooleanShadowPrefix(), "hgSeq");
for (cv=cgiVarList();  cv != NULL;  cv=cv->next)
    {
    if (startsWith(shadowPrefix, cv->name))
	{
	char *varName = cv->name + strlen(cgiBooleanShadowPrefix());
	cartSetBoolean(cart, varName, cgiBoolean(varName));
	}
    }
}

void saveBedOptionsState()
/* Store in cart the user's settings in doBedOptions() form */
{
storeBooleanIfSet("hgt.doCustomTrack");
storeStringIfSet("hgt.ctName");
storeStringIfSet("hgt.ctDesc");
storeStringIfSet("hgt.ctVis");
storeStringIfSet("hgt.ctUrl");
storeBooleanIfSet("hgt.loadCustomTrack");
}

void saveIntersectOptionsState()
/* Store in cart the user's settings in doIntersectOptions() form */
{
storeStringIfSet("hgt.intersectOp");
storeStringIfSet("hgt.moreThresh");
storeStringIfSet("hgt.lessThresh");
}

boolean isGenome(char *pos)
/* Return TRUE if pos is genome. */
{
pos = trimSpaces(pos);
return(sameWord(pos, "genome"));
}

void positionLookup(char *phase)
/* print the location and a jump button */
{
char pos[64];

if (! isGenome(position))
    {
    snprintf(pos, sizeof(pos), "%s:%d-%d", chrom, winStart+1, winEnd);
    position = cloneString(pos);
    }
cgiMakeTextVar("position", position, 30);
cgiMakeHiddenVar("origPhase", phase);
cgiMakeButton("submit", "Look up");
}

void positionLookupSamePhase()
/* print the location and a jump button if the table is positional */
{
if (tableIsPositional)
    {
    puts("position: ");
    positionLookup(cgiString("phase"));
    puts("<P>");
    }
else
    {
    cgiMakeHiddenVar("position", position);
    }
}


char *searchPosition(char *pos, char **retChrom, int *retStart, int *retEnd)
/* Use hgFind if necessary; return NULL 
 * if we had to display the gateway page or hgFind's selection page. */
{
if (! isGenome(pos))
    {
    struct hgPositions *hgp = findGenomePosWeb(pos, retChrom, retStart, retEnd,
					       cart, TRUE, hgTextName());

    if ((hgp == NULL) || (hgp->singlePos == NULL))
	return NULL;
    }
return(pos);
}


void handleDbChange()
/* 
   Copied from hgGateway:
   If we are changing databases via explicit cgi request,
   then remove custom track data which will 
   be irrelevant in this new database .
   If databases were changed then use the new default position too.
*/
{
char *oldDb  = hashFindVal(oldVars, "db");
char *oldPos = hashFindVal(oldVars, "position");
if ((oldDb != NULL) && (! sameWord(oldDb, database)))
    {
    if ((! isGenome(position)) &&
	(oldPos != NULL) && sameWord(position, oldPos))
	position = searchPosition(hDefaultPos(database),
				  &chrom, &winStart, &winEnd);
    cartRemove(cart, "hgt.customText");
    cartRemove(cart, "hgt.customFile");
    cartRemove(cart, "ct");
    }
}

void doGateway()
/* Table Browser gateway page: select organism, db, position */
{
char *oldDb;

position = cartCgiUsualString(cart, "position", hDefaultPos(database));
webStart(cart, "Table Browser: %s: Choose Organism &amp; Assembly",
	 freezeName);

handleDbChange();

puts(
"<CENTER>"
"Web tool created by "
"<A HREF=\"http://www.soe.ucsc.edu/~kent\">Jim Kent</A>, "
"<A HREF=\"http://www.soe.ucsc.edu/~sugnet\">Charles Sugnet</A>, "
"<A HREF=\"http://www.soe.ucsc.edu/~booch\">Terry Furey</A>, "
"<A HREF=\"http://www.soe.ucsc.edu/~haussler\">David Haussler</A>, "
"<A HREF=\"mailto:matt@soe.ucsc.edu\">Matt Schwartz</A>, "
"<A HREF=\"mailto:angie@soe.ucsc.edu\">Angie Hinrichs</A>, "
"<A HREF=\"mailto:donnak@soe.ucsc.edu\">Donna Karolchik</A>, "
"<A HREF=\"mailto:heather@soe.ucsc.edu\">Heather Trumbower</A>, "
"and the Genome Bioinformatics Group of UC Santa Cruz.</CENTER>\n"
"<P><CENTER>Copyright 2001 The Regents of the University of California. "
"All rights reserved.<P></CENTER><P>\n");

puts("<P>This tool allows you to download portions of the Genome Browser 
	database in several output formats.
	Enter a genome position (or enter \"genome\" to 
        search all chromosomes), then press the Submit button.\n");

printf("Use <A HREF=\"/cgi-bin/hgBlat?db=%s&hgsid=%d\">BLAT Search</A> to 
        locate a particular sequence in the genome.\n",
       database, cartSessionId(cart));
puts("See the <A HREF=\"/goldenPath/help/hgTextHelp.html\">Table Browser "
     "User Guide</A> for more information.<P>\n");

cgiMakeHiddenVar("org", organism);

puts(
"<center>\n"
"<table bgcolor=\"cccc99\" border=\"0\" CELLPADDING=1 CELLSPACING=0>\n"
"<tr><td>\n"
"<table BGCOLOR=\"FEFDEF\" BORDERCOLOR=\"CCCC99\" BORDER=0 CELLPADDING=0 CELLSPACING=0>\n"  
"<tr><td>\n"
);

puts(
"<table bgcolor=\"FFFEF3\" border=0>\n"
"<tr>\n"
"<td>\n"
);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=\"GET\"\">\n",
       hgTextName());
cartSaveSession(cart);
puts(
"<input TYPE=\"IMAGE\" BORDER=\"0\" NAME=\"hgt.dummyEnterButton\" src=\"/images/DOT.gif\">\n"
"<table><tr>\n"
"<td align=center valign=baseline>genome</td>\n"
"<td align=center valign=baseline>assembly</td>\n"
"<td align=center valign=baseline>position</td>\n"
);

puts("<tr><td align=center>\n");
printGenomeListHtml(database, onChangeText);
puts("</td>\n");

puts("<td align=center>\n");
printAssemblyListHtml(database);
puts("</td>\n");

puts("<td align=center>\n");
cgiMakeTextVar("position", position, 30);
cgiMakeHiddenVar("phase", chooseTablePhase);
puts("</td><td>");
cgiMakeButton("submit", "Submit");
puts(
"</td></tr></table>\n"
"</FORM>"
"</td></tr>\n"
"</table>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
"</center>\n"
);

printf("<FORM ACTION=\"%s\" METHOD=\"GET\" NAME=\"orgForm\">\n", hgTextName());
cgiMakeHiddenVar("org", organism);
cartSaveSession(cart);
puts("</FORM>");

hgPositionsHelpHtml(organism);
webEnd();
}

static boolean allLetters(char *s)
/* returns true if the string only has letters number and underscores */
{
int i;
for (i = 0; i < strlen(s); i++)
    if (!isalnum(s[i]) && s[i] != '_')
	return FALSE;

return TRUE;
}

void checkIsAlpha(char *desc, char *word)
/* make sure that the table name doesn't have anything "weird" in it */
{
if (!allLetters(word))
    webAbort("Error", "Invalid %s \"%s\".", desc, word);
}

boolean isSqlStringType(char *type)
{
return(strstr(type, "char") ||
       strstr(type, "text") ||
       strstr(type, "blob"));
}


char *getPosition(char **retChrom, int *retStart, int *retEnd)
/* Get position from cgi (not cart); use hgFind if necessary; return NULL 
 * if we had to display the gateway page or hgFind's selection page. */
{
char *pos = cloneString(cgiOptionalString("position"));

if (pos == NULL)
    pos = "";
if (pos[0] == '\0')
    {
    doGateway();
    return NULL;
    }
return(searchPosition(pos, retChrom, retStart, retEnd));
}


char *getTableVar()
{
char *table  = cgiOptionalString("table");
char *table0 = cgiOptionalString("table0");
char *table1 = cgiOptionalString("table1");
	
if (table != 0 && strcmp(table, "Choose table") == 0)
    table = 0;

if (table0 != 0 && strcmp(table0, "Choose table") == 0)
    table0 = 0;
	
if (table1 != 0 && strcmp(table1, "Choose table") == 0)
    table1 = 0;

if (table != 0)
    return table;
else if (table0 != 0)
    return table0;
else
    return table1;
}

char *getTableName()
{
char *val, *ptr;
	
val = getTableVar();
if (val == NULL)
    return val;
else if ((ptr = strchr(val, '.')) != NULL)
    return ptr + 1;
else
    return val;
}

char *getTableDb()
{
char *val, *ptr;
	
val = cloneString(getTableVar());
if (val == NULL)
    return NULL;
if ((ptr = strchr(val, '.')) != NULL)
    *ptr = 0;
return(val);
}

char *getTrackName()
{
char *trackName = cloneString(getTableName());
if (startsWith("chrN_", trackName))
    strcpy(trackName, trackName+strlen("chrN_"));
return trackName;
}

char *getTable2Var()
{
char *table2  = cgiOptionalString("table2");
if ((table2 != NULL) && sameString(table2, "Choose table"))
    table2 = NULL;
return table2;
}

char *getTable2Name()
{
char *val, *ptr;
	
val = getTable2Var();
if (val == NULL)
    return val;
else if ((ptr = strchr(val, '.')) != NULL)
    return ptr + 1;
else
    return val;
}

char *getTable2Db()
{
char *val, *ptr;
	
val = cloneString(getTable2Var());
if (val == NULL)
    return(val);
if ((ptr = strchr(val, '.')) != NULL)
    *ptr = 0;
return(val);
}

char *getTrack2Name()
{
char *trackName = cloneString(getTable2Name());
if ((trackName != NULL) && startsWith("chrN_", trackName))
    strcpy(trackName, trackName+strlen("chrN_"));
return trackName;
}

struct customTrack *getCustomTracks()
{
if (theCtList == NULL)
    theCtList = customTracksParseCart(cart, NULL, NULL);
return(theCtList);
}

struct customTrack *lookupCt(char *name)
{
struct customTrack *ctList = getCustomTracks();
struct customTrack *ct;
for (ct=ctList;  ct != NULL;  ct=ct->next)
    {
    if (sameString(ct->tdb->tableName, name))
	return ct;
    }
return NULL;
}

boolean tableExists(char *table, char *db)
{
if (sameString(customTrackPseudoDb, db))
    return (lookupCt(table) != NULL);
if (sameString(database, db))
    return hTableExists(table);
if (sameString(hGetDb2(), db))
    return hTableExists2(table);
else
    {
    errAbort("Unrecognized database name: %s", db);
    return FALSE;
    }
}

void checkTableExists(char *table)
{
if (! tableExists(table, getTableDb()))
    webAbort("No data", "Table %s (%s) does not exist in database %s.",
	     getTableName(), table, getTableDb());
}

void checkTable2Exists(char *table)
{
if (! tableExists(table, getTable2Db()))
    webAbort("No data", "Table %s (%s) does not exist in database %s.",
	     getTable2Name(), table, getTable2Db());
}

static boolean existsAndEqual(char *var, char *value)
/* returns true is the given CGI var exists and equals value */
{
if (cgiOptionalString(var) != 0 && sameString(cgiOptionalString(var), value))
    return TRUE;
else
    return FALSE;
}

static boolean existsAndStartsWith(char *var, char *value)
/* returns true is the given CGI var exists and starts with value */
{
if (cgiOptionalString(var) != 0 && startsWith(value, cgiOptionalString(var)))
    return TRUE;
else
    return FALSE;
}


static void printSelectOptions(struct hashEl *optList, char *varName)
/* Print out an HTML select option for each element in a hashEl list.
 * Mark as selected if it's the same as varName; strip prefix for display. */
{
struct hashEl *cur;
char *noPrefix;
char *curSetting = cartCgiUsualString(cart, varName, "");

for (cur = optList;  cur != NULL;  cur = cur->next)
    {
    if ((noPrefix = strchr(cur->name, '.')) != NULL)
	noPrefix++;
    else
	noPrefix = cur->name;
    if (sameString(curSetting, cur->name))
	printf("<OPTION SELECTED VALUE=\"%s\">%s</OPTION>\n",
	       cur->name, noPrefix);
    else
	printf("<OPTION VALUE=\"%s\">%s</OPTION>\n",
	       cur->name, noPrefix);
    }
}


int compareTable(const void *elem1,  const void *elem2)
/* compairs two hash element by name */
{
struct hashEl* a = *((struct hashEl **)elem1);
struct hashEl* b = *((struct hashEl **)elem2);
char *na, *nb;
if ((na = strchr(a->name, '.')) == NULL)
    na = a->name;
if ((nb = strchr(b->name, '.')) == NULL)
    nb = b->name;
return strcmp(na, nb);
}

boolean excludeTable(char *tbl)
/* Exclude these large tables. I think we should use a better algo. than 
 * hardcoding -- a count(*) cutoff?  And should alert the user that the 
 * tables exist, but are being excluded due to size. */
{
return(sameString(tbl, "all_est") ||
       sameString(tbl, "all_mrna"));
}

void getTableNames(char *db, struct sqlConnection *conn,
		   struct hashEl **retPosTableList,
		   struct hashEl **retNonposTableList)
/* separate tables in db into positional and nonpositional lists, 
 * with db added as a prefix to each name. */
{
struct hash *posTableHash = newHash(7);
struct hashEl *posTableList;
struct hash *nonposTableHash = newHash(7);
struct hashEl *nonposTableList;
struct sqlResult *sr;
char **row;
char query[256];
char name[128];
char chrom[32];
char post[64];
char fullName[128];

strcpy(query, "SHOW TABLES");
sr = sqlGetResult(conn, query);
while((row = sqlNextRow(sr)) != NULL)
    {
    if (excludeTable(row[0]))
	continue;

    /* if table name is of the form, chr*_random_* or chr*_*: */
    if (sscanf(row[0], "chr%32[^_]_random_%64s", chrom, post) == 2 ||
	sscanf(row[0], "chr%32[^_]_%64s", chrom, post) == 2)
	{
	snprintf(name, sizeof(name), "chrN_%s", post);
	// If a chrN_ table is already in the (positional) hash, 
	// don't bother looking up its fields.
	if (hashLookup(posTableHash, name))
	    continue;
	}
    else
	{
	strncpy(name, row[0], sizeof(name));
	}

    snprintf(fullName, sizeof(fullName), "%s.%s", db, name);
    if (hFindChromStartEndFieldsDb(db, row[0], query, query, query))
	hashStoreName(posTableHash, cloneString(fullName));
    else
	hashStoreName(nonposTableHash, cloneString(fullName));
    }
sqlFreeResult(&sr);
posTableList = hashElListHash(posTableHash);
slSort(&posTableList, compareTable);
nonposTableList = hashElListHash(nonposTableHash);
slSort(&nonposTableList, compareTable);
if (retPosTableList != NULL)
    *retPosTableList = posTableList;
if (retNonposTableList != NULL)
    *retNonposTableList = nonposTableList;
}

struct hashEl *getCustomTrackNames()
/* store custom track names in a hash (custom tracks are always positional) */
{
struct customTrack *ctList = getCustomTracks();
struct hash *ctTableHash = newHash(7);
struct hashEl *ctTableList;
struct customTrack *ct;
char fullName[128];

for (ct=ctList;  ct != NULL;  ct=ct->next)
    {
    snprintf(fullName, sizeof(fullName), "%s.%s",
	     customTrackPseudoDb, ct->tdb->tableName);
    hashStoreName(ctTableHash, cloneString(fullName));
    }
ctTableList = hashElListHash(ctTableHash);
slSort(&ctTableList, compareTable);
return(ctTableList);
}

void categorizeTables(struct hashEl **retPosTableList,
		      struct hashEl **retNonposTableList)
/* Return sorted lists of positional and non-positional table names 
 * from the current database and hgFixed. */
{
struct sqlConnection *conn;
struct hashEl *posTableList = NULL;
struct hashEl *nonposTableList = NULL;
struct hashEl *fixedPosTableList = NULL;
struct hashEl *fixedNonposTableList = NULL;
struct hashEl *ctPosTableList = NULL;

/* get table names from the database */
conn = hAllocConn();
getTableNames(database, conn, &posTableList, &nonposTableList);
hFreeConn(&conn);
/* get table names from hgFixed too */
conn = sqlConnect(hgFixed);
getTableNames(hgFixed, conn, &fixedPosTableList, &fixedNonposTableList);
sqlDisconnect(&conn);
/* get custom track names */
ctPosTableList = getCustomTrackNames();
/* prepend ct, append hgFixed db lists onto default db lists and return */
posTableList = slCat(posTableList, fixedPosTableList);
slSort(&posTableList, compareTable);
*retPosTableList = slCat(ctPosTableList, posTableList);
*retNonposTableList = slCat(nonposTableList, fixedNonposTableList);
slSort(retNonposTableList, compareTable);
}

void doChooseTable()
/* get the table selection from the user */
{
struct hashEl *posTableList;
struct hashEl *nonposTableList;

webStart(cart, "Table Browser: %s: Choose a table", freezeName);
handleDbChange();

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
puts("<TABLE CELLPADDING=\"8\">");
puts("<TR><TD>");
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#ChooseTable\">"
     "<B>Help</B></A>");
puts("</TD></TR>");
puts("<TR><TD>");

puts("Choose a position: ");
puts("</TD><TD>");
positionLookup(chooseTablePhase);
puts("</TD></TR>");

puts("<TR><TD>");

categorizeTables(&posTableList, &nonposTableList);
puts("Choose a table:");
puts("</TD><TD>");
puts("<SELECT NAME=table0 SIZE=1>");
printf("<OPTION VALUE=\"Choose table\">Positional tables</OPTION>\n");
printSelectOptions(posTableList, "table0");
puts("</SELECT>");

puts("<SELECT NAME=table1 SIZE=1>");
printf("<OPTION VALUE=\"Choose table\">Non-positional tables</OPTION>\n");
printSelectOptions(nonposTableList, "table1");
puts("</SELECT>");
hashElFreeList(&posTableList);
hashElFreeList(&nonposTableList);
puts("</TD></TR>");

puts("<TR><TD>");
puts("Choose an action: ");
puts("</TD><TD>");
cgiMakeButton("phase", oldAllFieldsPhase);
cgiMakeButton("phase", outputOptionsPhase);
puts("</TD></TR>");

puts("</TABLE>");
puts("</FORM>");
webEnd();
}

void getFullTableName(char *dest, char *newChrom, char *table)
/* given a chrom return the table name of the table selected by the user */
{
char post[64];

if (newChrom == NULL)
    newChrom = "chr1";
chrom = newChrom;
if (allGenome)
    {
    winStart = 0;
    winEnd   = hChromSize(chrom);
    }
if (sscanf(table, "chrN_%64s", post) == 1)
    snprintf(dest, 256, "%s_%s", chrom, post);
else
    strncpy(dest, table, 256);

/* make sure that the table name doesn't have anything "weird" in it */
checkIsAlpha("table name", dest);
}

void stringFilterOption(char *field, char *tableId, char *logOp)
/* Print out a table row with filter constraint options for a string/char. */
{
char name[128];

printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", field);
snprintf(name, sizeof(name), "dd%s_%s", tableId, field);
cgiMakeDropList(name, ddOpMenu, ddOpMenuSize,
		cgiUsualString(name, ddOpMenu[0]));
puts(" match </TD><TD>");
snprintf(name, sizeof(name), "pat%s_%s", tableId, field);
cgiMakeTextVar(name, cgiUsualString(name, "*"), 20);
if (logOp == NULL)
    logOp = "";
printf("</TD><TD> %s </TD></TR>\n", logOp);
}

void numericFilterOption(char *field, char *fieldLabel, char *tableId,
			 char *logOp)
/* Print out a table row with filter constraint options for a number. */
{
char name[128];

printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", fieldLabel);
puts(" is ");
snprintf(name, sizeof(name), "cmp%s_%s", tableId, field);
cgiMakeDropList(name, cmpOpMenu, cmpOpMenuSize,
		cgiUsualString(name, cmpOpMenu[0]));
puts("</TD><TD>\n");
snprintf(name, sizeof(name), "pat%s_%s", tableId, field);
cgiMakeTextVar(name, cgiUsualString(name, ""), 20);
if (logOp == NULL)
    logOp = "";
printf("</TD><TD>%s</TD></TR>\n", logOp);
}

void eqFilterOption(char *field, char *fieldLabel1, char *fieldLabel2,
		    char *tableId, char *logOp)
/* Print out a table row with filter constraint options for an equality 
 * comparison. */
{
char name[128];

printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", fieldLabel1);
puts(" is ");
snprintf(name, sizeof(name), "cmp%s_%s", tableId, field);
cgiMakeDropList(name, eqOpMenu, eqOpMenuSize,
		cgiUsualString(name, eqOpMenu[0]));
/* make a dummy pat_ CGI var for consistency with other filter options */
snprintf(name, sizeof(name), "pat%s_%s", tableId, field);
cgiMakeHiddenVar(name, "0");
puts("</TD><TD>\n");
printf("%s\n", fieldLabel2);
if (logOp == NULL)
    logOp = "";
printf("<TD>%s</TD></TR>\n", logOp);
}

void filterOptionsCustomTrack(char *table, char *tableId)
/* Print out an HTML table with form inputs for constraints on custom track */
{
struct customTrack *ct = lookupCt(table);

puts("<TABLE>");
if (ct->fieldCount >= 3)
    {
    stringFilterOption("chrom", tableId, " AND ");
    numericFilterOption("chromStart", "chromStart", tableId, " AND ");
    numericFilterOption("chromEnd", "chromEnd", tableId, " AND ");
    }
if (ct->fieldCount >= 4)
    {
    stringFilterOption("name", tableId, " AND ");
    }
if (ct->fieldCount >= 5)
    {
    numericFilterOption("score", "score", tableId, " AND ");
    }
if (ct->fieldCount >= 6)
    {
    stringFilterOption("strand", tableId, " AND ");
    }
if (ct->fieldCount >= 8)
    {
    numericFilterOption("thickStart", "thickStart", tableId, " AND ");
    numericFilterOption("thickEnd", "thickEnd", tableId, " AND ");
    }
if (ct->fieldCount >= 12)
    {
    numericFilterOption("blockCount", "blockCount", tableId, " AND ");
    }
/* These are not bed fields, just extra constraints that we offer: */
if (ct->fieldCount >= 3)
    {
    numericFilterOption("chromLength", "(chromEnd - chromStart)", tableId,
			(ct->fieldCount >= 8) ? " AND " : "");
    }
if (ct->fieldCount >= 8)
    {
    numericFilterOption("thickLength", "(thickEnd - thickStart)",
			tableId, " AND ");
    eqFilterOption("compareStarts", "chromStart", "thickStart", tableId,
		   " AND ");
    eqFilterOption("compareEnds", "chromEnd", "thickEnd", tableId, "");
    }
puts("</TABLE>");
}

void filterOptionsTableDb(char *fullTblName, char *db, char *tableId)
/* Print out an HTML table with form inputs for constraints on table fields */
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
boolean gotFirst;
char query[256];
char name[128];
char *newVal;

snprintf(query, sizeof(query), "DESCRIBE %s", fullTblName);
if (sameString(database, db))
    conn = hAllocConn();
else
    conn = hAllocConn2();
sr = sqlGetResult(conn, query);

puts("<TABLE><TR><TD>\n");
puts("<TABLE>\n");
gotFirst = FALSE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (! sameWord(row[1], "longblob"))
	{
	if (! gotFirst)
	    gotFirst = TRUE;
	else
	    puts(" AND </TD></TR>\n");
	printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", row[0]);
	if (isSqlStringType(row[1]))
	    {
	    snprintf(name, sizeof(name), "dd%s_%s", tableId, row[0]);
	    cgiMakeDropList(name, ddOpMenu, ddOpMenuSize,
			    cgiUsualString(name, ddOpMenu[0]));
	    puts(" match </TD><TD>\n");
	    newVal = "*";
	    }
	else
	    {
	    puts(" is \n");
	    snprintf(name, sizeof(name), "cmp%s_%s", tableId, row[0]);
	    cgiMakeDropList(name, cmpOpMenu, cmpOpMenuSize,
			    cgiUsualString(name, cmpOpMenu[0]));
	    puts("</TD><TD>\n");
	    newVal = "";
	    }
	snprintf(name, sizeof(name), "pat%s_%s", tableId, row[0]);
	cgiMakeTextVar(name, cgiUsualString(name, newVal), 20);
	}
    }
sqlFreeResult(&sr);
if (sameString(database, db))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
puts("</TD></TR></TABLE>\n");
puts(" </TD></TR><TR VALIGN=BOTTOM><TD>");
snprintf(name, sizeof(name), "log_rawQuery%s", tableId);
cgiMakeDropList(name, logOpMenu, logOpMenuSize,
		cgiUsualString(name, logOpMenu[0]));
puts("Free-form query: ");
snprintf(name, sizeof(name), "rawQuery%s", tableId);
cgiMakeTextVar(name, cgiUsualString(name, ""), 50);
puts("</TD></TR></TABLE>");
}


void doOutputOptions()
/* print out a form with output table format & filtering options */
{
struct hashEl *posTableList;
struct hashEl *nonposTableList;
char *table = getTableName();
char *db = getTableDb();

saveChooseTableState();

webStart(cart, "Table Browser: %s: %s", freezeName, outputOptionsPhase);
checkTableExists(fullTableName);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();

printf("<P><HR><H3> Select Output Format for %s </H3>\n", table);
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Formats\">"
     "<B>Help</B></A><P>");
if (tableIsPositional)
    cgiMakeDropList("outputType", outputTypePosMenu, outputTypePosMenuSize,
		    cartCgiUsualString(cart, "outputType",
				       outputTypePosMenu[0]));
else
    cgiMakeDropList("outputType", outputTypeNonPosMenu,
		    outputTypeNonPosMenuSize,
		    cartCgiUsualString(cart, "outputType",
				       outputTypeNonPosMenu[0]));
cgiMakeButton("phase", getOutputPhase);

printf("<P><HR><H3> (Optional) Filter %s Records by Field Values </H3>",
       table);
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Constraints\">"
     "<B>Help</B></A><P>");
if (sameString(customTrackPseudoDb, db))
    filterOptionsCustomTrack(table, "");
else
    filterOptionsTableDb(fullTableName, db, "");
cgiMakeButton("phase", getOutputPhase);

if (tableIsPositional)
    {
    puts("<P><HR><H3> (Optional) Intersect Results with Another Table </H3>");
    puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Intersection\">"
	 "<B>Help</B></A><P>");
    puts("Note: Output Format must be FASTA, BED, Hyperlinks, GTF \n"
	 "or Summary/Statistics "
	 "for this feature.  <P>");
    categorizeTables(&posTableList, &nonposTableList);
    puts("Choose a second table:");
    puts("<SELECT NAME=table2 SIZE=1>");
    printf("<OPTION VALUE=\"Choose table\">Positional tables</OPTION>\n");
    printSelectOptions(posTableList, "table2");
    puts("</SELECT>");
    hashElFreeList(&posTableList);
    hashElFreeList(&nonposTableList);
    
    puts("<P>");
    cgiMakeButton("phase", intersectOptionsPhase);
    }

puts("</FORM>");
webEnd();
}


void parseNum(char *fieldName, struct kxTok **tok, struct dyString *q)
{
if (*tok == NULL)
    webAbort("Error", "Parse error when reading number for field %s.",
	     fieldName);

if ((*tok)->type == kxtString)
    {
    if (! isdigit((*tok)->string[0]))
	webAbort("Error", "Parse error when reading number for field %s.",
		 fieldName);
    dyStringAppend(q, (*tok)->string);
    *tok = (*tok)->next;
    }
else
    {
    webAbort("Error", "Parse error when reading number for field %s: Incorrect token type %d for token \"%s\"",
	     fieldName, (*tok)->type, (*tok)->string);
    }
}

void constrainNumber(char *fieldName, char *op, char *pat, char *log,
		     struct dyString *clause)
{
struct kxTok *tokList, *tokPtr;
int i;
boolean legit;

if (fieldName == NULL || op == NULL || pat == NULL || log == NULL)
    webAbort("Error", "CGI var error: not all required vars were defined for field %s.", fieldName);

/* complain if op is not a legitimate value */
legit = FALSE;
for (i=0;  i < cmpOpMenuSize;  i++)
    {
    if (sameString(cmpOpMenu[i], op))
	{
	legit = TRUE;
	break;
	}
    }
if (! legit)
    webAbort("Error", "Illegal comparison operator \"%s\"", op);

/* tokenize (don't expect wildcards) */
tokPtr = tokList = kxTokenize(pat, FALSE);

if (clause->stringSize > 0)
    dyStringPrintf(clause, " %s ", log);
else
    dyStringAppend(clause, "(");
dyStringPrintf(clause, "(%s %s ", fieldName, op);
parseNum(fieldName, &tokPtr, clause);
dyStringAppend(clause, ")");

slFreeList(&tokList);
}

void constrainRange(char *fieldName, char *pat, char *log,
		    struct dyString *clause)
{
struct kxTok *tokList, *tokPtr;

if (fieldName == NULL || pat == NULL || log == NULL)
    webAbort("Error", "CGI var error: not all required vars were defined for field %s.", fieldName);

/* tokenize (don't expect wildcards) */
tokPtr = tokList = kxTokenize(pat, FALSE);

if (clause->stringSize > 0)
    dyStringPrintf(clause, " %s ", log);
else
    dyStringAppend(clause, "(");
dyStringPrintf(clause, "((%s >= ", fieldName);
parseNum(fieldName, &tokPtr, clause);
dyStringPrintf(clause, ") && (%s <= ", fieldName);
while (tokPtr != NULL && (tokPtr->type == kxtSub ||
			  tokPtr->type == kxtPunct))
    tokPtr = tokPtr->next;
parseNum(fieldName, &tokPtr, clause);
dyStringAppend(clause, "))");

slFreeList(&tokList);
}

void constrainPattern(char *fieldName, char *dd, char *pat, char *log,
		      struct dyString *clause)
{
struct kxTok *tokList, *tokPtr;
boolean needOr = FALSE;
char *cmp, *or, *ptr;
int i;
boolean legit;

if (fieldName == NULL || dd == NULL || pat == NULL || log == NULL)
    webAbort("Error", "CGI var error: not all required vars were defined for field %s.", fieldName);

/* complain if dd is not a legitimate value */
legit = FALSE;
for (i=0;  i < ddOpMenuSize;  i++)
    {
    if (sameString(ddOpMenu[i], dd))
	{
	legit = TRUE;
	break;
	}
    }
if (! legit)
    webAbort("Error", "Illegal does/doesn't value \"%s\"", dd);

/* tokenize (do allow wildcards, SQL wildcards, hyphens) */
tokList = kxTokenizeFancy(pat, TRUE, TRUE, TRUE);

/* The subterms are joined by OR if dd="does", AND if dd="doesn't" */
or  = sameString(dd, "does") ? " OR " : " AND ";
cmp = sameString(dd, "does") ? "LIKE"   : "NOT LIKE";

if (clause->stringSize > 0)
    dyStringPrintf(clause, " %s ", log);
else
    dyStringAppend(clause, "(");
dyStringAppend(clause, "(");
for (tokPtr = tokList;  tokPtr != NULL;  tokPtr = tokPtr->next)
    {
    if (tokPtr->type == kxtWildString || tokPtr->type == kxtString ||
	/* Allow these types for strand matches too: */
	tokPtr->type == kxtSub || tokPtr->type == kxtAdd)
	{
	if (needOr)
	    dyStringAppend(clause, or);
	/* Replace normal wildcard characters with SQL: */
	while ((ptr = strchr(tokPtr->string, '?')) != NULL)
	    *ptr = '_';
	while ((ptr = strchr(tokPtr->string, '*')) != NULL)
	    *ptr = '%';
	dyStringPrintf(clause, "(%s %s \"%s\")",
		       fieldName, cmp, tokPtr->string);
	needOr = TRUE;
	}
    else if (tokPtr->type == kxtEnd)
	break;
    else
	{
	webAbort("Error", "Match pattern parse error for field %s: bad token type (%d) for this word: \"%s\".",
		 fieldName, tokPtr->type, tokPtr->string);
	}
    }
dyStringAppend(clause, ")");

slFreeList(&tokList);
}

void constrainFreeForm(char *rawQuery, struct dyString *clause)
/* Let the user type in an expression that may contain
 * - field names
 * - parentheses
 * - comparison/arithmetic/logical operators
 * - numbers
 * - patterns with wildcards
 * Make sure they don't use any SQL reserved words, ;'s, etc.
 * Let SQL handle the actual parsing of nested expressions etc. - 
 * this is just a token cop. */
{
struct kxTok *tokList, *tokPtr;
struct slName *fnPtr;
char *ptr;
int numLeftParen, numRightParen;

if ((rawQuery == NULL) || (rawQuery[0] == 0))
    return;

/* tokenize (do allow wildcards, and include quotes.) */
kxTokIncludeQuotes(TRUE);
tokList = kxTokenizeFancy(rawQuery, TRUE, TRUE, TRUE);

/* to be extra conservative, wrap the whole expression in parens. */
dyStringAppend(clause, "(");
numLeftParen = numRightParen = 0;
for (tokPtr = tokList;  tokPtr != NULL;  tokPtr = tokPtr->next)
    {
    if ((tokPtr->type == kxtEquals) ||
	(tokPtr->type == kxtGT) ||
	(tokPtr->type == kxtGE) ||
	(tokPtr->type == kxtLT) ||
	(tokPtr->type == kxtLE) ||
	(tokPtr->type == kxtAnd) ||
	(tokPtr->type == kxtOr) ||
	(tokPtr->type == kxtNot) ||
	(tokPtr->type == kxtAdd) ||
	(tokPtr->type == kxtSub) ||
	(tokPtr->type == kxtDiv))
	{
	dyStringAppend(clause, tokPtr->string);
	}
    else if (tokPtr->type == kxtOpenParen)
	{
	dyStringAppend(clause, tokPtr->string);
	numLeftParen++;
	}
    else if (tokPtr->type == kxtCloseParen)
	{
	dyStringAppend(clause, tokPtr->string);
	numRightParen++;
	}
    else if ((tokPtr->type == kxtWildString) ||
	     (tokPtr->type == kxtString))
	{
	char *word = cloneString(tokPtr->string);
	toUpperN(word, strlen(word));
	if (startsWith("SQL_", word) || 
	    startsWith("MYSQL_", word) || 
	    sameString("ALTER", word) || 
	    sameString("BENCHMARK", word) || 
	    sameString("CHANGE", word) || 
	    sameString("CREATE", word) || 
	    sameString("DELAY", word) || 
	    sameString("DELETE", word) || 
	    sameString("DROP", word) || 
	    sameString("FLUSH", word) || 
	    sameString("GET_LOCK", word) || 
	    sameString("GRANT", word) || 
	    sameString("INSERT", word) || 
	    sameString("KILL", word) || 
	    sameString("LOAD", word) || 
	    sameString("LOAD_FILE", word) || 
	    sameString("LOCK", word) || 
	    sameString("MODIFY", word) || 
	    sameString("PROCESS", word) || 
	    sameString("QUIT", word) || 
	    sameString("RELEASE_LOCK", word) || 
	    sameString("RELOAD", word) || 
	    sameString("REPLACE", word) || 
	    sameString("REVOKE", word) || 
	    sameString("SELECT", word) || 
	    sameString("SESSION_USER", word) || 
	    sameString("SHOW", word) || 
	    sameString("SYSTEM_USER", word) || 
	    sameString("UNLOCK", word) || 
	    sameString("UPDATE", word) || 
	    sameString("USE", word) || 
	    sameString("USER", word) || 
	    sameString("VERSION", word))
	    {
	    webAbort("Error", "Illegal SQL word \"%s\" in free-form query string",
		     tokPtr->string);
	    }
	else if (sameString("*", tokPtr->string))
	    {
	    // special case for multiplication in a wildcard world
	    dyStringPrintf(clause, " %s ", tokPtr->string);
	    }
	else
	    {
	    /* Replace normal wildcard characters with SQL: */
	    while ((ptr = strchr(tokPtr->string, '?')) != NULL)
		*ptr = '_';
	    while ((ptr = strchr(tokPtr->string, '*')) != NULL)
	    *ptr = '%';
	    dyStringPrintf(clause, " %s ", tokPtr->string);
	    }
	}
    else if (tokPtr->type == kxtEnd)
	{
	break;
	}
    else
	{
	webAbort("Error", "Unrecognized token \"%s\" in free-form query string",
		 tokPtr->string);
	}
    }
dyStringAppend(clause, ")");

if (numLeftParen != numRightParen)
    webAbort("Error", "Unequal number of left parentheses (%d) and right parentheses (%d) in free-form query expression",
	     numLeftParen, numRightParen);

slFreeList(&tokList);
}

char *constrainFields(char *tableId)
/* If the user specified constraints, append SQL conditions (suitable
 * for a WHERE clause) to q. */
{
struct cgiVar *current;
struct dyString *freeClause = newDyString(512);
struct dyString *andClause = newDyString(512);
struct dyString *clause;
char *fieldName;
char *rawQuery;
char *rQLogOp;
char *dd, *cmp, *pat;
char varName[128];
char *ret;

if (tableId == NULL)
    tableId = "";

dyStringClear(andClause);
for (current = cgiVarList();  current != NULL;  current = current->next)
    {
    /* Look for pattern variable associated with each field. */
    snprintf(varName, sizeof(varName), "pat%s_", tableId);
    if (startsWith(varName, current->name))
	{	
	fieldName = current->name + strlen(varName);
	/* make sure that the field name doesn't have anything "weird" in it */
	checkIsAlpha("field name", fieldName);
	pat = current->val;
	snprintf(varName, sizeof(varName), "dd%s_%s", tableId, fieldName);
	dd = cgiOptionalString(varName);
	snprintf(varName, sizeof(varName), "cmp%s_%s", tableId, fieldName);
	cmp = cgiOptionalString(varName);
	/* If it's a null constraint, skip it. */
	if ( (dd != NULL &&
	      (pat == NULL || pat[0] == 0 ||
	       sameString(trimSpaces(pat), "*"))) ||
	     (cmp != NULL && sameString(cmp, "ignored")) )
	    continue;
	/* Otherwise, expect it to be a well-formed constraint and tack 
	 * it on to the clause. */
	clause = andClause;
	if (cmp != NULL && sameString(cmp, "in range"))
	    constrainRange(fieldName, pat, "AND", clause);
	else if (cmp != NULL)
	    constrainNumber(fieldName, cmp, pat, "AND", clause);
	else
	    constrainPattern(fieldName, dd, pat, "AND", clause);
	}
    }
if (andClause->stringSize > 0)
    dyStringAppend(andClause, ")");

dyStringClear(freeClause);
snprintf(varName, sizeof(varName), "rawQuery%s", tableId);
rawQuery = cgiOptionalString(varName);
snprintf(varName, sizeof(varName), "log_rawQuery%s", tableId);
rQLogOp  = cgiOptionalString(varName);
constrainFreeForm(rawQuery, freeClause);
// force rQLogOp to a legit value:
if ((rQLogOp != NULL) && (! sameString("AND", rQLogOp)))
    rQLogOp = "OR";

if (freeClause->stringSize > 0 && andClause->stringSize > 0)
    dyStringPrintf(freeClause, " %s ", rQLogOp);

if (freeClause->stringSize > 0)
    {
    dyStringAppend(freeClause, andClause->string);
    }
else
    {
    dyStringAppend(freeClause, andClause->string);
    }
ret = cloneString(freeClause->string);
freeDyString(&freeClause);
freeDyString(&andClause);
return ret;
}


void cgiToCharFilter(char *dd, char *pat, enum charFilterType *retCft,
		     char **retVals, boolean *retInv)
/* Given a "does/doesn't" and a (list of) literal chars from CGI, fill in 
 * retCft, retVals and retInv to make a filter. */
{
char *vals, *ptrs[32];
int numWords;
int i;

assert(retCft != NULL);
assert(retVals != NULL);
assert(retInv != NULL);
assert(sameString(dd, "does") || sameString(dd, "doesn't"));

/* catch null-constraint cases.  ? will be treated as a literal match, 
 * which would make sense for bed strand and maybe other single-char things: */
if (pat == NULL)
    pat = "";
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*"))
    {
    *retCft = cftIgnore;
    return;
    }

*retCft = cftMultiLiteral;
numWords = chopByWhite(pat, ptrs, ArraySize(ptrs));
vals = needMem((numWords+1) * sizeof(char));
for (i=0;  i < numWords;  i++)
    vals[i] = ptrs[i][0];
vals[i] = 0;
*retVals = vals;
*retInv = sameString("does", dd);
}

void cgiToStringFilter(char *dd, char *pat, enum stringFilterType *retSft,
		       char ***retVals, boolean *retInv)
/* Given a "does/doesn't" and a (list of) regexps from CGI, fill in 
 * retCft, retVals and retInv to make a filter. */
{
char **vals, *ptrs[32];
int numWords;
int i;

assert(retSft != NULL);
assert(retVals != NULL);
assert(retInv != NULL);
assert(sameString(dd, "does") || sameString(dd, "doesn't"));

/* catch null-constraint cases: */
if (pat == NULL)
    pat = "";
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*"))
    {
    *retSft = sftIgnore;
    return;
    }

*retSft = sftMultiRegexp;
numWords = chopByWhite(pat, ptrs, ArraySize(ptrs));
vals = needMem((numWords+1) * sizeof(char *));
for (i=0;  i < numWords;  i++)
    vals[i] = cloneString(ptrs[i]);
vals[i] = NULL;
*retVals = vals;
*retInv = sameString("doesn't", dd);
}

void cgiToIntFilter(char *cmp, char *pat, enum numericFilterType *retNft,
		    int **retVals)
/* Given a comparison operator and a (pair of) integers from CGI, fill in 
 * retNft and retVals to make a filter. */
{
char *ptrs[3];
int *vals;
int numWords;

assert(retNft != NULL);
assert(retVals != NULL);

/* catch null-constraint cases: */
if (pat == NULL)
    pat = "";
pat = trimSpaces(pat);
if ((pat[0] == 0) || sameString(pat, "*") || sameString(cmp, "ignored"))
    {
    *retNft = nftIgnore;
    return;
    }
else if (sameString(cmp, "in range"))
    {
    *retNft = nftInRange;
    numWords = chopString(pat, " \t,", ptrs, ArraySize(ptrs));
    if (numWords != 2)
	errAbort("For \"in range\" constraint, you must give two numbers separated by whitespace or comma.");
    vals = needMem(2 * sizeof(int)); 
    vals[0] = atoi(ptrs[0]);
    vals[1] = atoi(ptrs[1]);
    if (vals[0] > vals[1])
	{
	int tmp = vals[0];
	vals[0] = vals[1];
	vals[1] = tmp;
	}
    *retVals = vals;
   }
else
    {
    if (sameString(cmp, "<"))
	*retNft = nftLessThan;
    else if (sameString(cmp, "<="))
	*retNft = nftLTE;
    else if (sameString(cmp, "="))
	*retNft = nftEqual;
    else if (sameString(cmp, "!="))
	*retNft = nftNotEqual;
    else if (sameString(cmp, ">="))
	*retNft = nftGTE;
    else if (sameString(cmp, ">"))
	*retNft = nftGreaterThan;
    else
	errAbort("Unrecognized comparison operator %s", cmp);
    vals = needMem(sizeof(int));
    vals[0] = atoi(pat);
    *retVals = vals;
    }
}

struct bedFilter *constrainBedFields(char *tableId)
/* If the user specified constraints, then translate them to a bedFilter. */
{
struct bedFilter *bf;
struct cgiVar *current;
char *fieldName;
char *dd, *cmp, *pat;
char varName[128];
int *trash;

if (tableId == NULL)
    tableId = "";

AllocVar(bf);
for (current = cgiVarList();  current != NULL;  current = current->next)
    {
    /* Look for pattern variable associated with each field. */
    snprintf(varName, sizeof(varName), "pat%s_", tableId);
    if (startsWith(varName, current->name))
	{	
	fieldName = current->name + strlen(varName);
	checkIsAlpha("field name", fieldName);
	pat = cloneString(current->val);
	snprintf(varName, sizeof(varName), "dd%s_%s", tableId, fieldName);
	dd = cgiOptionalString(varName);
	snprintf(varName, sizeof(varName), "cmp%s_%s", tableId, fieldName);
	cmp = cgiOptionalString(varName);
	if (sameString("chrom", fieldName))
	    cgiToStringFilter(dd, pat, &(bf->chromFilter), &(bf->chromVals),
			      &(bf->chromInvert));
	else if (sameString("chromStart", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->chromStartFilter), &(bf->chromStartVals));
	else if (sameString("chromEnd", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->chromEndFilter), &(bf->chromEndVals));
	else if (sameString("name", fieldName))
	    cgiToStringFilter(dd, pat, &(bf->nameFilter), &(bf->nameVals),
			      &(bf->nameInvert));
	else if (sameString("score", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->scoreFilter), &(bf->scoreVals));
	else if (sameString("strand", fieldName))
	    cgiToCharFilter(dd, pat, &(bf->strandFilter), &(bf->strandVals),
			    &(bf->strandInvert));
	else if (sameString("thickStart", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->thickStartFilter), &(bf->thickStartVals));
	else if (sameString("thickEnd", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->thickEndFilter), &(bf->thickEndVals));
	else if (sameString("blockCount", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->blockCountFilter), &(bf->blockCountVals));
	else if (sameString("chromLength", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->chromLengthFilter), &(bf->chromLengthVals));
	else if (sameString("thickLength", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->thickLengthFilter), &(bf->thickLengthVals));
	else if (sameString("compareStarts", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->compareStartsFilter), &trash);
	else if (sameString("compareEnds", fieldName))
	    cgiToIntFilter(cmp, pat,
			   &(bf->compareEndsFilter), &trash);
	}
    }
return(bf);
}


void preserveConstraints(char *fullTblName, char *db, char *tableId)
/* Add CGI variables for filtering constraints, so they will be passed to 
 * the next page.  Also parse the constraints and do a null query with them 
 * in order to catch any syntax errors sooner rather than later. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
struct cgiVar *current;
struct dyString *query = newDyString(512);
struct bedFilter *bf = constrainBedFields(tableId);
char *constraints = constrainFields(tableId);
char varName[128];

if ((constraints != NULL) && (constraints[0] != 0) &&
    (! sameString(customTrackPseudoDb, db)))
    {
    // Null query will cause errAbort if there's a syntax error, no-op if OK.
    dyStringPrintf(query, "SELECT 1 FROM %s WHERE 0 AND %s",
		   fullTblName, constraints);
    if (sameString(database, db))
	conn = hAllocConn();
    else
	conn = hAllocConn2();
    sr = sqlGetResult(conn, query->string);
    dyStringFree(&query);
    sqlFreeResult(&sr);
    if (sameString(database, db))
	hFreeConn(&conn);
    else
	hFreeConn2(&conn);
    }

if (tableId == NULL)
    tableId = "";
for (current = cgiVarList();  current != NULL;  current = current->next)
    {
    /* Look for pattern variable associated with each field. */
    snprintf(varName, sizeof(varName), "pat%s_", tableId);
    if (startsWith(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    snprintf(varName, sizeof(varName), "dd%s_", tableId);
    if (startsWith(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    snprintf(varName, sizeof(varName), "cmp%s_", tableId);
    if (startsWith(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    snprintf(varName, sizeof(varName), "log_rawQuery%s", tableId);
    if (sameString(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    snprintf(varName, sizeof(varName), "rawQuery%s", tableId);
    if (sameString(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    }
}


void preserveTable2()
{
char *table2 = getTable2Name();
char *op = cgiOptionalString("hgt.intersectOp");
if ((table2 != NULL) && (table2[0] != 0) && (op != NULL))
    {
    char *db2 = getTable2Db();
    char fullTableName2[256];
    cgiContinueHiddenVar("table2");
    cgiContinueHiddenVar("hgt.intersectOp");
    cgiMakeHiddenVar("hgt.moreThresh", cgiUsualString("hgt.moreThresh", "0"));
    cgiMakeHiddenVar("hgt.lessThresh", cgiUsualString("hgt.lessThresh", "100"));
    cgiContinueHiddenVar("hgt.invertTable");
    cgiContinueHiddenVar("hgt.invertTable2");
    getFullTableName(fullTableName2, chrom, table2);
    preserveConstraints(fullTableName2, db2, "2");
    }
}

struct hTableInfo *ctToHti(struct customTrack *ct)
/* Create an hTableInfo from a customTrack. */
{
struct hTableInfo *hti;

AllocVar(hti);
hti->rootName = cloneString(ct->tdb->tableName);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString(ct->tdb->type);
if (ct->fieldCount >= 3)
    {
    strncpy(hti->chromField, "chrom", 32);
    strncpy(hti->startField, "chromStart", 32);
    strncpy(hti->endField, "chromEnd", 32);
    }
if (ct->fieldCount >= 4)
    {
    strncpy(hti->nameField, "name", 32);
    }
if (ct->fieldCount >= 5)
    {
    strncpy(hti->scoreField, "score", 32);
    }
if (ct->fieldCount >= 6)
    {
    strncpy(hti->strandField, "strand", 32);
    }
if (ct->fieldCount >= 8)
    {
    strncpy(hti->cdsStartField, "thickStart", 32);
    strncpy(hti->cdsEndField, "thickEnd", 32);
    hti->hasCDS = TRUE;
    }
if (ct->fieldCount >= 12)
    {
    strncpy(hti->countField, "blockCount", 32);
    strncpy(hti->startsField, "chromStarts", 32);
    strncpy(hti->endsSizesField, "blockSizes", 32);
    hti->hasBlocks = TRUE;
    }

return(hti);
}

struct hTableInfo *getHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti;

if (sameString(customTrackPseudoDb, db))
    {
    struct customTrack *ct = lookupCt(table);
    hti = ctToHti(ct);
    }
else
    {
    char *track;
    if (startsWith("chrN_", table))
	track = table + strlen("chrN_");
    else
	track = table;
    hti = hFindTableInfoDb(db, chrom, track);
    }

if (hti == NULL)
    webAbort("Error", "Could not find table info for table %s in db %s",
	     table, db);
return(hti);
}


struct hTableInfo *getOutputHti()
/* Return effective table info for the output.  If we're doing a 
 * base-pair-wise intersection/union of 2 tables, this will be a 
 * bed4 that isn't a track (unless user makes it a custom track).  
 * Otherwise this will just be the primary table info. */
{
char *db = getTableDb();
char *table = getTableName();
struct hTableInfo *hti = getHti(db, table);
char *table2 = getTable2Name();
char *op = cgiOptionalString("hgt.intersectOp");

if ((table2 != NULL) && (table2[0] != 0) && (op != NULL))
    {
    if (sameString("and", op) || sameString("or", op))
	{
	struct hTableInfo *effHti;
	char buf[128];
	AllocVar(effHti);
	snprintf(buf, sizeof(buf), "%s_%s_%s", table, op, table2);
	effHti->rootName = cloneString(buf);
	effHti->isPos = TRUE;
	effHti->isSplit = FALSE;
	effHti->hasBin = FALSE;
	snprintf(effHti->chromField, 32, "N/A");
	snprintf(effHti->startField, 32, "N/A");
	snprintf(effHti->endField, 32, "N/A");
	snprintf(effHti->nameField, 32, "N/A");
	effHti->scoreField[0] = 0;
	effHti->strandField[0] = 0;
	effHti->cdsStartField[0] = 0;
	effHti->cdsEndField[0] = 0;
	effHti->countField[0] = 0;
	effHti->startsField[0] = 0;
	effHti->endsSizesField[0] = 0;
	effHti->hasCDS = FALSE;
	effHti->hasBlocks = FALSE;
	strcpy(buf, "bed 4");
	effHti->type = cloneString(buf);
	return(effHti);
	}
    }
return(hti);
}

struct bed *bitsToBed4List(Bits *bits, int bitSize, char *chrom, int minSize,
			   int rangeStart, int rangeEnd)
{
struct bed *bedList = NULL, *bed;
int i;
boolean thisBit, lastBit;
int start = 0;
int end;
int id = 0;
char name[128];

if (rangeStart < 0)
    rangeStart = 0;
if (rangeEnd > bitSize)
    rangeEnd = bitSize;

/* We depend on extra zero BYTE at end in case bitNot was used on bits. */
thisBit = FALSE;
for (i=0;  i < bitSize+8;  ++i)
    {
    lastBit = thisBit;
    thisBit = bitReadOne(bits, i);
    if (thisBit)
	{
	if (!lastBit)
	    start = i;
	}
    else
        {
	end = i;
	if (end >= bitSize)
	    end = bitSize - 1;
	// Lop off elements that go all the way to the beginning/end of the 
	// chrom... unless our range actually includes the beginning/end.
	// (That can happen with the AND/OR of two NOT's...)
	if (lastBit &&
	    ((end - start) >= minSize) &&
	    ((rangeStart == 0) || (start > 0)) &&
	    ((rangeEnd == bitSize) || (end < bitSize)))
	    {
	    AllocVar(bed);
	    bed->chrom = cloneString(chrom);
	    bed->chromStart = start;
	    bed->chromEnd = end;
	    snprintf(name, sizeof(name), "%s.%d", chrom, ++id);
	    bed->name = cloneString(name);
	    slAddHead(&bedList, bed);
	    }
	}
    }

slReverse(&bedList);
return(bedList);
}


int countBasesOverlap(struct bed *bedItem, Bits *bits, boolean hasBlocks)
/* Return the number of bases belonging to bedItem covered by bits. */
{
int count = 0;
int i, j;
if (hasBlocks)
    {
    for (i=0;  i < bedItem->blockCount;  i++)
	{
	int start = bedItem->chromStart + bedItem->chromStarts[i];
	int end   = start + bedItem->blockSizes[i];
	for (j=start;  j < end;  j++)
	    if (bitReadOne(bits, j))
		count++;
	}
    }
else
    {
    for (i=bedItem->chromStart;  i < bedItem->chromEnd;  i++)
	if (bitReadOne(bits, i))
	    count++;
    }
    return(count);
}


struct bed *getBedList(boolean ignoreConstraints, char *onlyThisChrom)
/* For any positional table output: get the features selected by the user 
 * and return them as a bed list.  This is where table intersection happens. */
{
struct slName *chromList, *chromPtr;
struct bed *bedList = NULL, *bedListT1 = NULL, *bedListChrom = NULL;
char *db = getTableDb();
char *table = getTableName();
struct hTableInfo *hti = getHti(db, table);
char *constraints;
char *table2 = getTable2Name();
char *op = cgiOptionalString("hgt.intersectOp");
char *track = getTrackName();
char *outputType = cgiString("outputType");
int fields;
int i, totalCount;

checkTableExists(fullTableName);
constraints = constrainFields(NULL);
if (ignoreConstraints ||
    ((constraints != NULL) && (constraints[0] == 0)))
    constraints = NULL;

if (onlyThisChrom != NULL)
    chromList = newSlName(onlyThisChrom);
else if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);
for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    bedListChrom = NULL;
    getFullTableName(fullTableName, chromPtr->name, table);
    if (sameString(customTrackPseudoDb, db))
	{
	struct customTrack *ct = lookupCt(table);
	struct bedFilter *bf = constrainBedFields(NULL);
	bedListT1 = bedFilterListInRange(ct->bedList, bf,
					 chrom, winStart, winEnd);
	}
    else
	bedListT1 = hGetBedRangeDb(db, fullTableName, chrom, winStart,
				   winEnd, constraints);
    /* If 2 tables are named, get their intersection. */
    if ((table2 != NULL) && (table2[0] != 0) && (op != NULL))
	{
	struct featureBits *fbListT2 = NULL;
	struct bed *bed;
	Bits *bitsT2;
	int moreThresh = cgiOptionalInt("hgt.moreThresh", 0);
	int lessThresh = cgiOptionalInt("hgt.lessThresh", 100);
	boolean invTable = cgiBoolean("hgt.invertTable");
	boolean invTable2 = cgiBoolean("hgt.invertTable2");
	char *track2 = getTrack2Name();
	char *db2 = getTable2Db();
	char *constraints2 = constrainFields("2");
	char fullTableName2[256];
	int chromSize = hChromSize(chromPtr->name);
	if ((!sameString("any", op)) &&
	    (!sameString("none", op)) &&
	    (!sameString("more", op)) &&
	    (!sameString("less", op)) &&
	    (!sameString("and", op)) &&
	    (!sameString("or", op)))
	    {
	    webAbort("Error", "Invalid value \"%s\" of CGI variable hgt.intersectOp", op);
	    }
	if (ignoreConstraints ||
	    ((constraints2 != NULL) && (constraints2[0] == 0)))
	    constraints2 = NULL;
	fprintf(stderr, "TBQuery:  p=%s:%d-%d  t1=%s  q1=%s  t2=%s  q2=%s  op=%s  mT=%d  lT=%d  iT=%d  iT2=%d\n",
		chrom, winStart, winEnd,
		table, constraints,
		table2, constraints2,
		op, moreThresh, lessThresh, invTable, invTable2);
	getFullTableName(fullTableName2, chromPtr->name, table2);
	checkTable2Exists(fullTableName2);
	if (sameString(customTrackPseudoDb, db2))
	    {
	    struct customTrack *ct2 = lookupCt(table2);
	    struct bedFilter *bf2 = constrainBedFields("2");
	    struct bed *bedListT2 = bedFilterListInRange(ct2->bedList, bf2,
						      chrom, winStart, winEnd);
	    struct hTableInfo *hti2 = ctToHti(ct2);
	    fbListT2 = fbFromBed(track2, hti2, bedListT2, winStart, winEnd,
				 FALSE, FALSE);
	    bedFreeList(&bedListT2);
	    }
	else
	    fbListT2 = fbGetRangeQueryDb(db2, track2, chrom, winStart, winEnd,
					 constraints2, FALSE, FALSE);
	bitsT2 = bitAlloc(chromSize+8);
	fbOrBits(bitsT2, chromSize, fbListT2, 0);
	if (sameString("and", op) || sameString("or", op))
	    {
	    // Base-pair-wise operation: get featureBits for primary table too
	    struct featureBits *fbListT1;
	    Bits *bitsT1;
	    fbListT1 = fbFromBed(track, hti, bedListT1, winStart, winEnd,
				 FALSE, FALSE);
	    bitsT1 = bitAlloc(chromSize+8);
	    fbOrBits(bitsT1, chromSize, fbListT1, 0);
	    // invert inputs if necessary
	    if (invTable)
		bitNot(bitsT1, chromSize);
	    if (invTable2)
		bitNot(bitsT2, chromSize);
	    // do the intersection/union
	    if (sameString("and", op))
		bitAnd(bitsT1, bitsT2, chromSize);
	    else
		bitOr(bitsT1, bitsT2, chromSize);
	    // translate back to bed
	    bedListChrom = bitsToBed4List(bitsT1, chromSize, chrom, 1,
					  winStart, winEnd);
	    bitFree(&bitsT1);
	    featureBitsFreeList(&fbListT1);
	    }
	else
	    {
	    for (bed = bedListT1;  bed != NULL;  bed = bed->next)
		{
		struct bed *newBed;
		int numBasesOverlap = countBasesOverlap(bed, bitsT2,
							hti->hasBlocks);
		int length = 0;
		double pctBasesOverlap;
		if (hti->hasBlocks)
		    for (i=0;  i < bed->blockCount;  i++)
			length += bed->blockSizes[i];
		else
		    length = (bed->chromEnd - bed->chromStart);
		if (length == 0)
		    length = 1;
		pctBasesOverlap = ((numBasesOverlap * 100.0) / length);
		if ((sameString("any", op) && (numBasesOverlap > 0)) ||
		    (sameString("none", op) && (numBasesOverlap == 0)) ||
		    (sameString("more", op) &&
		     (pctBasesOverlap >= moreThresh)) ||
		    (sameString("less", op) &&
		     (pctBasesOverlap <= lessThresh)))
		    {
		    newBed = cloneBed(bed);		    
		    slAddHead(&bedListChrom, newBed);
		    }
		}
	    slReverse(&bedListChrom);
	    } // end foreach primary table bed item
	bedFreeList(&bedListT1);
	bitFree(&bitsT2);
	featureBitsFreeList(&fbListT2);
	}
    else
	{
	/* Only one table was given, no intersection necessary. */
	bedListChrom = bedListT1;
	}
    bedList = slCat(bedList, bedListChrom);
    } // end foreach chromosome in position range
return(bedList);
}


struct slName *getAllFields()
/* Return a list of all field names in the primary table. */
{
struct slName *fieldList = NULL, *field;
char *db = getTableDb();

if (sameString(customTrackPseudoDb, db))
    {
    char *table = getTableName();
    struct customTrack *ct = lookupCt(table);
    if (ct->fieldCount >= 3)
	{
	field = newSlName("chrom");
	slAddHead(&fieldList, field);
	field = newSlName("chromStart");
	slAddHead(&fieldList, field);
	field = newSlName("chromEnd");
	slAddHead(&fieldList, field);
	}
    if (ct->fieldCount >= 4)
	{
	field = newSlName("name");
	slAddHead(&fieldList, field);
	}
    if (ct->fieldCount >= 5)
	{
	field = newSlName("score");
	slAddHead(&fieldList, field);
	}
    if (ct->fieldCount >= 6)
	{
	field = newSlName("strand");
	slAddHead(&fieldList, field);
	}
    if (ct->fieldCount >= 8)
	{
	field = newSlName("thickStart");
	slAddHead(&fieldList, field);
	field = newSlName("thickEnd");
	slAddHead(&fieldList, field);
	}
    if (ct->fieldCount >= 12)
	{
	field = newSlName("reserved");
	slAddHead(&fieldList, field);
	field = newSlName("blockCount");
	slAddHead(&fieldList, field);
	field = newSlName("blockSizes");
	slAddHead(&fieldList, field);
	field = newSlName("chromStarts");
	slAddHead(&fieldList, field);
	}
    }
else
    {
    struct sqlConnection *conn;
    struct sqlResult *sr;
    char **row;
    char query[256];
    if (sameString(database, db))
	conn = hAllocConn();
    else
	conn = hAllocConn2();
    snprintf(query, sizeof(query), "DESCRIBE %s", fullTableName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	field = newSlName(row[0]);
	slAddHead(&fieldList, field);
	}
    sqlFreeResult(&sr);
    if (sameString(database, getTableDb()))
	hFreeConn(&conn);
    else
	hFreeConn2(&conn);
    }
slReverse(&fieldList);
return(fieldList);
}

struct slName *getChosenFields(boolean allFields)
/* Return a list of chosen field names. */
{
struct slName *tableFields = getAllFields();
if (allFields)
    {
    return(tableFields);
    }
else
    {
    struct slName *fieldList = NULL, *field;
    struct cgiVar* varPtr;
    for (varPtr = cgiVarList();  varPtr != NULL;  varPtr = varPtr->next)
	{
	if (startsWith("field_", varPtr->name) &&
	    sameString("on", varPtr->val))
	    {	
	    char *fieldStr = varPtr->name + strlen("field_");
	    checkIsAlpha("field name", fieldStr);
	    /* check that the field is there in the current table (and not 
	     * just a stale CGI var) */
	    if (slNameInList(tableFields, fieldStr))
		{
		field = newSlName(fieldStr);
		slAddHead(&fieldList, field);
		}
	    }
	}
    slReverse(&fieldList);
    return(fieldList);
    }
}

boolean printTabbedResults(struct sqlResult *sr, boolean initialized)
{
char **row;
char *field;
int i;
int numberColumns = sqlCountColumns(sr);

row = sqlNextRow(sr);
if (row == NULL)
    return(initialized);

if (! initialized)
    {
    initialized = TRUE;
    /* print the columns names */
    printf("#");
    while((field = sqlFieldName(sr)) != NULL)
	printf("%s\t", field);
    printf("\n");
    }

/* print the data */
do
    {
    for (i = 0; i < numberColumns; i++)
	printf("%s\t", row[i]);
    printf("\n");
    }
while((row = sqlNextRow(sr)) != NULL);
return(initialized);
}

boolean printTabbedBed(struct bed *bedList, struct slName *chosenFields,
		       boolean initialized)
/* Print out the chosen fields of a bedList. */
{
struct bed *bed;
struct slName *field;

if (bedList == NULL)
    return(initialized);

if (! initialized)
    {
    boolean gotFirst = FALSE;
    initialized = TRUE;
    printf("#");
    for (field=chosenFields;  field != NULL;  field=field->next)
	{
	if (! gotFirst)
	    gotFirst = TRUE;
	else
	    printf("\t");
	printf("%s", field->name);
	}
    printf("\n");
    }

for (bed=bedList;  bed != NULL;  bed=bed->next)
    {
    boolean gotFirst = FALSE;
    for (field=chosenFields;  field != NULL;  field=field->next)
	{
	if (! gotFirst)
	    gotFirst = TRUE;
	else
	    printf("\t");
	if (sameString(field->name, "chrom"))
	    printf("%s", bed->chrom);
	else if (sameString(field->name, "chromStart"))
	    printf("%d", bed->chromStart);
	else if (sameString(field->name, "chromEnd"))
	    printf("%d", bed->chromEnd);
	else if (sameString(field->name, "name"))
	    printf("%s", bed->name);
	else if (sameString(field->name, "score"))
	    printf("%d", bed->score);
	else if (sameString(field->name, "strand"))
	    printf("%s", bed->strand);
	else if (sameString(field->name, "thickStart"))
	    printf("%d", bed->thickStart);
	else if (sameString(field->name, "thickEnd"))
	    printf("%d", bed->thickEnd);
	else if (sameString(field->name, "reserved"))
	    printf("%d", bed->reserved);
	else if (sameString(field->name, "blockCount"))
	    printf("%d", bed->blockCount);
	else if (sameString(field->name, "blockSizes"))
	    {
	    int i;
	    for (i=0;  i < bed->blockCount;  i++)
		printf("%d,", bed->blockSizes[i]);
	    }
	else if (sameString(field->name, "chromStarts"))
	    {
	    int i;
	    for (i=0;  i < bed->blockCount;  i++)
		printf("%d,", bed->chromStarts[i]);
	    }
	else
	    errAbort("\nUnrecognized bed field name \"%s\"", field->name);
	}
    printf("\n");
    }
return(initialized);
}

void doTabSeparatedCT(boolean allFields)
{
struct slName *chromList, *chromPtr;
struct bed *bedList, *bed;
char *table = getTableName();
struct customTrack *ct = lookupCt(table);
struct slName *chosenFields;
struct bedFilter *bf;
boolean gotResults;

printf("Content-Type: text/plain\n\n");
webStartText();
checkTableExists(fullTableName);
bf = constrainBedFields(NULL);

if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

chosenFields = getChosenFields(allFields);
if (chosenFields == NULL)
    {
    printf("\n# Error: at least one field must be selected.\n\n");
    return;
    }

gotResults = FALSE;
for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    getFullTableName(fullTableName, chromPtr->name, table);
    bedList = bedFilterListInRange(ct->bedList, bf, chrom, winStart, winEnd);
    gotResults = printTabbedBed(bedList, chosenFields, gotResults);
    bedFreeList(&bedList);
    }

if (! gotResults)
    printf("\n# No results returned from query.\n\n");
}

void doTabSeparated(boolean allFields)
{
struct slName *chromList, *chromPtr;
struct sqlConnection *conn;
struct sqlResult *sr;
struct dyString *query = newDyString(512);
struct dyString *fieldSpec = newDyString(256);
char *table = getTableName();
char *db = getTableDb();
char chromField[32];
char startField[32];
char endField[32];
char *constraints;
boolean gotResults;

saveChooseTableState();
saveChooseFieldsState();
saveOutputOptionsState();
saveIntersectOptionsState();

if (sameString(customTrackPseudoDb, db))
    {
    doTabSeparatedCT(allFields);
    return;
    }

printf("Content-Type: text/plain\n\n");
webStartText();
checkTableExists(fullTableName);
constraints = constrainFields(NULL);

if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

if (sameString(database, db))
    conn = hAllocConn();
else
    conn = hAllocConn2();

dyStringClear(fieldSpec);
if (allFields)
    dyStringAppend(fieldSpec, "*");
else
    {
    struct slName *chosenFields = getChosenFields(allFields);
    struct slName *field;
    boolean gotFirst = FALSE;
    if (chosenFields == NULL)
	{
	printf("\n# Error: at least one field must be selected.\n\n");
	return;
	}
    for (field=chosenFields;  field != NULL;  field=field->next)
	{
	if (! gotFirst)
	    gotFirst = TRUE;
	else
	    dyStringAppend(fieldSpec, ",");
	dyStringAppend(fieldSpec, field->name);
	}
    }

hFindChromStartEndFieldsDb(db, fullTableName, chromField, startField, endField);
gotResults = FALSE;
if (tableIsSplit)
    {
    for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
	{
	getFullTableName(fullTableName, chromPtr->name, table);
	dyStringClear(query);
	dyStringPrintf(query, "SELECT %s FROM %s",
		       fieldSpec->string, fullTableName);
	if ((! allGenome) && tableIsPositional)
	    {
	    dyStringPrintf(query, " WHERE %s < %d AND %s > %d",
			   startField, winEnd, endField, winStart);
	    if ((constraints != NULL) && (constraints[0] != 0))
		dyStringPrintf(query, " AND %s", constraints);
	    }
	else if ((constraints != NULL) && (constraints[0] != 0))
	    dyStringPrintf(query, " WHERE %s", constraints);
	sr = sqlGetResult(conn, query->string);
	gotResults = printTabbedResults(sr, gotResults);
	sqlFreeResult(&sr);
	}
    }
else
    {
    dyStringClear(query);
    dyStringPrintf(query, "SELECT %s FROM %s",
		   fieldSpec->string, fullTableName);
    if ((! allGenome) && tableIsPositional)
	{
	dyStringPrintf(query, " WHERE %s < %d AND %s > %d",
		       startField, winEnd, endField, winStart);
	if (! sameString("", chromField))
	    dyStringPrintf(query, " AND %s = \'%s\'",
			   chromField, chrom);
	if ((constraints != NULL) && (constraints[0] != 0))
	    dyStringPrintf(query, " AND %s", constraints);
	}
    else if ((constraints != NULL) && (constraints[0] != 0))
	dyStringPrintf(query, " WHERE %s", constraints);
    sr = sqlGetResult(conn, query->string);
    gotResults = printTabbedResults(sr, gotResults);
    sqlFreeResult(&sr);
    }
if (! gotResults)
    printf("\n# No results returned from query.\n\n");

dyStringFree(&query);
if (sameString(database, db))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
}

void doChooseFields()
{
struct slName *allFields = getAllFields();
struct slName *field;
char *table = getTableName();
char *db = getTableDb();
char *outputType = cgiUsualString("outputType", cgiString("phase"));
char name[128];
boolean checkAll;
boolean clearAll;

saveOutputOptionsState();
saveIntersectOptionsState();

webStart(cart, "Table Browser: %s: Choose Fields of %s", freezeName, table);
checkTableExists(fullTableName);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();
cgiMakeHiddenVar("outputType", outputType);
preserveConstraints(fullTableName, db, NULL);
preserveTable2();

printf("<H3> Select Fields of %s: </H3>\n", getTableName());
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#ChooseFields\">"
     "<B>Help</B></A><P>");
cgiMakeHiddenVar("origPhase", cgiString("phase"));
cgiMakeButton("submit", "Check All");
cgiMakeButton("submit", "Clear All");

checkAll = existsAndEqual("submit", "Check All") ? TRUE : FALSE;
clearAll = existsAndEqual("submit", "Clear All") ? TRUE : FALSE;
puts("<TABLE>");
for (field=allFields;  field != NULL;  field=field->next)
    {
    boolean fieldChecked;
    puts("<TR><TD>");
    snprintf(name, sizeof(name), "field_%s", field->name);
    fieldChecked = cartCgiUsualBoolean(cart, name, FALSE);
    cgiMakeCheckBox(name,
		    (checkAll || (fieldChecked && !clearAll)));
    puts(field->name);
    puts("</TD></TR>");
    }
puts("</TABLE>\n");
cgiMakeButton("phase", getSomeFieldsPhase);
puts("</FORM>");
webEnd();
}


void doSequenceOptions()
{
struct hTableInfo *hti = getOutputHti();
char *outputType = cgiUsualString("outputType", cgiString("phase"));

saveOutputOptionsState();
saveIntersectOptionsState();

webStart(cart, "Table Browser: %s: %s", freezeName, seqOptionsPhase);
checkTableExists(fullTableName);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();
cgiMakeHiddenVar("outputType", outputType);
preserveConstraints(fullTableName, getTableDb(), NULL);
preserveTable2();
printf("<H3>Table: %s</H3>\n", hti->rootName);
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#FASTAOptions\">"
     "<B>Help</B></A><P>");
hgSeqOptionsHtiCart(hti, cart);
cgiMakeButton("phase", getSequencePhase);
puts("</FORM>");
webEnd();
}


void doGetSequence()
{
struct hTableInfo *hti = getOutputHti();
struct bed *bedList = getBedList(FALSE, NULL);
int itemCount;

saveOutputOptionsState();
saveIntersectOptionsState();
saveSequenceOptionsState();

printf("Content-Type: text/plain\n\n");
webStartText();

itemCount = hgSeqBedDb(database, hti, bedList);
bedFreeList(&bedList);
if (itemCount == 0)
    printf("\n# No results returned from query.\n\n");
}

static void addGffLineFromBed(struct gffLine **pGffList, struct bed *bed,
			      char *source, char *feature,
			      int start, int end, char frame)
/* Create a gffLine from a bed and line-specific parameters, add to list. */
{
struct gffLine *gff;
AllocVar(gff);
gff->seq = cloneString(bed->chrom);
gff->source = cloneString(source);
gff->feature = cloneString(feature);
gff->start = start;
gff->end = end;
gff->score = bed->score;
gff->strand = bed->strand[0];
gff->frame = frame;
gff->group = gff->geneId = cloneString(bed->name);
slAddHead(pGffList, gff);
}


static void addCdsStartStop(struct gffLine **pGffList, struct bed *bed,
			    char *source, int s, int e, char *frames,
			    int i, int startIndx, int stopIndx,
			    boolean gtf2StopCodons)
{
// start_codon (goes first for + strand) overlaps with CDS
if ((i == startIndx) && (bed->strand[0] != '-'))
    {
    addGffLineFromBed(pGffList, bed, source, "start_codon",
		      s, s+3, '.');
    }
// stop codon does not overlap with CDS as of GTF2
if ((i == stopIndx) && gtf2StopCodons)
    {
    if (bed->strand[0] == '-')
	{
	addGffLineFromBed(pGffList, bed, source, "stop_codon",
			  s, s+3, '.');
	addGffLineFromBed(pGffList, bed, source, "CDS", s+3, e,
			  frames[i]);
	}
    else
	{
	addGffLineFromBed(pGffList, bed, source, "CDS", s, e-3,
			  frames[i]);
	addGffLineFromBed(pGffList, bed, source, "stop_codon",
			  e-3, e, '.');
	}
    }
else
    {
    addGffLineFromBed(pGffList, bed, source, "CDS", s, e,
		      frames[i]);
    }
// start_codon (goes last for - strand) overlaps with CDS
if ((i == startIndx) && (bed->strand[0] == '-'))
    {
    addGffLineFromBed(pGffList, bed, source, "start_codon",
		      e-3, e, '.');
    }
}


struct gffLine *bedToGffLines(struct bed *bedList, struct hTableInfo *hti,
			      char *source)
/* Translate a (list of) bed into list of gffLine elements. */
{
struct gffLine *gffList = NULL, *gff;
struct bed *bed;
int i, j, s, e;

for (bed = bedList;  bed != NULL;  bed = bed->next)
    {
    if (hti->hasBlocks && hti->hasCDS)
	{
	char *frames = needMem(bed->blockCount);
	boolean gotFirstCds = FALSE;
	int nextPhase = 0;
	int startIndx = 0;
	int stopIndx = 0;
	// need a separate pass to compute frames, in case strand is '-'
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    if (bed->strand[0] == '-')
		j = bed->blockCount-i-1;
	    else
		j = i;
	    s = bed->chromStart + bed->chromStarts[j];
	    e = s + bed->blockSizes[j];
	    if ((s < bed->thickEnd) && (e > bed->thickStart))
		{
		int cdsSize = e - s;
		if (e > bed->thickEnd)
		    cdsSize = bed->thickEnd - s;
		else if (s < bed->thickStart)
		    cdsSize = e - bed->thickStart;
		if (! gotFirstCds)
		    {
		    gotFirstCds = TRUE;
		    startIndx = j;
		    }
		frames[j] = '0' + nextPhase;
		nextPhase = (3 + ((nextPhase - cdsSize) % 3)) % 3;
		stopIndx = j;
		}
	    else
		{
		frames[j] = '.';
		}
	    }
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    s = bed->chromStart + bed->chromStarts[i];
	    e = s + bed->blockSizes[i];
	    if ((s >= bed->thickStart) && (e <= bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source, s, e, frames,
				i, startIndx, stopIndx, FALSE);
		}
	    else if ((s < bed->thickStart) && (e > bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source,
				bed->thickStart, bed->thickEnd,
				frames, i, startIndx, stopIndx, FALSE);
		}
	    else if ((s < bed->thickStart) && (e > bed->thickStart))
		{
		addCdsStartStop(&gffList, bed, source, bed->thickStart, e,
				frames, i, startIndx, stopIndx, FALSE);
		}
	    else if ((s < bed->thickEnd) && (e > bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source, s, bed->thickEnd,
				frames, i, startIndx, stopIndx, FALSE);
		}
	    addGffLineFromBed(&gffList, bed, source, "exon", s, e, '.');
	    }
	freeMem(frames);
	}
    else if (hti->hasBlocks)
	{
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    s = bed->chromStart + bed->chromStarts[i];
	    e = s + bed->blockSizes[i];
	    addGffLineFromBed(&gffList, bed, source, "exon", s, e, '.');
	    }
	}
    else if (hti->hasCDS)
	{
	if (bed->thickStart > bed->chromStart)
	    {
	    addGffLineFromBed(&gffList, bed, source, "exon", bed->chromStart,
			      bed->thickStart, '.');
	    }
	addGffLineFromBed(&gffList, bed, source, "CDS", bed->thickStart,
			  bed->thickEnd, '0');
	if (bed->thickEnd < bed->chromEnd)
	    {
	    addGffLineFromBed(&gffList, bed, source, "exon", bed->thickEnd,
			      bed->chromEnd, '.');
	    }
	}
    else
	{
	addGffLineFromBed(&gffList, bed, source, "exon", bed->chromStart,
			  bed->chromEnd, '.');
	}
    }
slReverse(&gffList);
return(gffList);
}

void doGetGFF()
{
struct hTableInfo *hti = getOutputHti();
struct bed *bedList = getBedList(FALSE, NULL);
struct gffLine *gffList, *gffPtr;
char source[64];
char *db = getTableDb();
char *track = getTrackName();
int itemCount;

saveOutputOptionsState();
saveIntersectOptionsState();

printf("Content-Type: text/plain\n\n");
webStartText();

if (sameString(customTrackPseudoDb, db))
    snprintf(source, sizeof(source), "%s", track);
else
    snprintf(source, sizeof(source), "%s_%s", db, track);
itemCount = 0;
gffList = bedToGffLines(bedList, hti, source);
bedFreeList(&bedList);
for (gffPtr = gffList;  gffPtr != NULL;  gffPtr = gffPtr->next)
    {
    gffTabOut(gffPtr, stdout);
    itemCount++;
    }
slFreeList(&gffList);
if (itemCount == 0)
    printf("\n# No results returned from query.\n\n");
}


void doBedOptions()
{
struct hTableInfo *hti = getOutputHti();
char *table = getTableName();
char *table2 = getTable2Name();
char *op = cgiOptionalString("hgt.intersectOp");
char *track = getTrackName();
char *db = getTableDb();
char *outputType = cgiUsualString("outputType", cgiString("phase"));
char *setting;
char buf[256];

saveOutputOptionsState();
saveIntersectOptionsState();

webStart(cart, "Table Browser: %s: %s", freezeName, bedOptionsPhase);
checkTableExists(fullTableName);
puts("<FORM ACTION=\"/cgi-bin/hgText\" NAME=\"mainForm\" METHOD=\"GET\">\n");
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();
cgiMakeHiddenVar("outputType", outputType);
preserveConstraints(fullTableName, db, NULL);
preserveTable2();
printf("<H3> Select BED Options for %s: </H3>\n", hti->rootName);
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#BEDOptions\">"
     "<B>Help</B></A><P>");
puts("<TABLE><TR><TD>");
cgiMakeCheckBox("hgt.doCustomTrack",
		cartCgiUsualBoolean(cart, "hgt.doCustomTrack", TRUE));
puts("</TD><TD> <B> Include "
     "<A HREF=\"/goldenPath/help/customTrack.html\" TARGET=_blank>"
     "custom track</A> header: </B>");
puts("</TD></TR><TR><TD></TD><TD>name=");
if (op == NULL)
    table2 = NULL;
snprintf(buf, sizeof(buf), "tb_%s%s%s", hti->rootName,
	 (table2 ? "_" : ""),
	 (table2 ? table2 : ""));
setting = cgiUsualString("hgt.ctName", buf);
cgiMakeTextVar("hgt.ctName", setting, 16);
puts("</TD></TR><TR><TD></TD><TD>description=");
snprintf(buf, sizeof(buf), "table browser query on %s%s%s",
	 hti->rootName,
	 (table2 ? ", " : ""),
	 (table2 ? table2 : ""));
setting = cgiUsualString("hgt.ctDesc", buf);
cgiMakeTextVar("hgt.ctDesc", setting, 50);
puts("</TD></TR><TR><TD></TD><TD>visibility=");
setting = cartCgiUsualString(cart, "hgt.ctVis", ctVisMenu[3]);
cgiMakeDropList("hgt.ctVis", ctVisMenu, ctVisMenuSize, setting);
puts("</TD></TR><TR><TD></TD><TD>url=");
setting = cartCgiUsualString(cart, "hgt.ctUrl", "");
cgiMakeTextVar("hgt.ctUrl", setting, 50);
puts("</TD></TR><TR><TD></TD><TD>");
cgiMakeCheckBox("hgt.loadCustomTrack",
		cartCgiUsualBoolean(cart, "hgt.loadCustomTrack", FALSE));
puts("Load this custom track into my session");
puts("</TD></TR></TABLE>");
puts("<P> <B> Create one BED record per: </B>");
fbOptionsHtiCart(hti, cart);
cgiMakeButton("phase", getBedPhase);
puts("</FORM>");
webEnd();
}


struct customTrack *newCT(char *ctName, char *ctDesc, int visNum, char *ctUrl,
			  int fields)
/* Make a new custom track record for the query results. */
{
struct customTrack *ct;
struct trackDb *tdb;
char buf[256];
AllocVar(ct);
AllocVar(tdb);
snprintf(buf, sizeof(buf), "ct_%s", ctName);
tdb->tableName = cloneString(buf);
tdb->shortLabel = ctName;
tdb->longLabel = ctDesc;
snprintf(buf, sizeof(buf), "bed %d .", fields);
tdb->type = cloneString(buf);
tdb->visibility = visNum;
tdb->url = ctUrl;
ct->tdb = tdb;
ct->fieldCount = fields;
ct->needsLift = FALSE;
ct->fromPsl = FALSE;
return(ct);
}

void doGetBed()
{
struct hTableInfo *hti = getOutputHti();
struct bed *bedList = getBedList(FALSE, NULL);
struct bed *bedPtr;
struct featureBits *fbList = NULL, *fbPtr;
struct customTrack *ctNew = NULL;
struct tempName tn;
char *table = getTableName();
char *track = getTrackName();
boolean doCT = cgiBoolean("hgt.doCustomTrack");
char *ctName = cgiUsualString("hgt.ctName", table);
char *ctDesc = cgiUsualString("hgt.ctDesc", table);
char *ctVis  = cgiUsualString("hgt.ctVis", "dense");
char *ctUrl  = cgiUsualString("hgt.ctUrl", "");
boolean loadCT = cgiBoolean("hgt.loadCustomTrack");
char *ctFileName = NULL;
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields;
boolean gotResults = FALSE;

saveOutputOptionsState();
saveIntersectOptionsState();
saveBedOptionsState();

printf("Content-Type: text/plain\n\n");
webStartText();

if (hti->hasBlocks)
    fields = 12;
else if (hti->hasCDS)
    fields = 8;
else if (hti->strandField[0] != 0)
    fields = 6;
else if (hti->scoreField[0] != 0)
    fields = 5;
else
    fields = 4;

if (doCT && (bedList != NULL))
    {
    int visNum = (sameString("hide", ctVis)   ? 0 :
		  sameString("dense", ctVis)  ? 1 :
		  sameString("squish", ctVis) ? 2 : 
		  sameString("pack", ctVis)   ? 3 : 
		  sameString("full", ctVis)   ? 4 : 3);
    if (loadCT)
	ctNew = newCT(ctName, ctDesc, visNum, ctUrl, fields);
    printf("track name=\"%s\" description=\"%s\" visibility=%d url=%s \n",
	   ctName, ctDesc, visNum, ctUrl);
    }

if ((fbQual == NULL) || (fbQual[0] == 0))
    {
    for (bedPtr = bedList;  bedPtr != NULL;  bedPtr = bedPtr->next)
	{
	char *ptr = strchr(bedPtr->name, ' ');
	if (ptr != NULL)
	    *ptr = 0;
	bedTabOutN(bedPtr, fields, stdout);
	gotResults = TRUE;
	}
    if (ctNew != NULL)
	ctNew->bedList = bedList;
    }
else
    {
    snprintf(fbTQ, sizeof(fbTQ), "%s:%s", hti->rootName, fbQual);
    fbList = fbFromBed(fbTQ, hti, bedList, winStart, winEnd, FALSE, FALSE);
    for (fbPtr=fbList;  fbPtr != NULL;  fbPtr=fbPtr->next)
	{
	char *ptr = strchr(fbPtr->name, ' ');
	if (ptr != NULL)
	    *ptr = 0;
	printf("%s\t%d\t%d\t%s\t%d\t%c\n",
	       fbPtr->chrom, fbPtr->start, fbPtr->end, fbPtr->name,
	       0, fbPtr->strand);
	gotResults = TRUE;
	}
    if (ctNew != NULL)
	ctNew->bedList = fbToBed(fbList);
    featureBitsFreeList(&fbList);
    }

if ((ctNew != NULL) && (ctNew->bedList != NULL))
    {
    /* Load existing custom tracks and add this new one: */
    struct customTrack *ctList = getCustomTracks();
    slAddHead(&ctList, ctNew);
    /* Save the custom tracks out to file (overwrite the old file): */
    ctFileName = cartOptionalString(cart, "ct");
    if (ctFileName == NULL)
	{
	makeTempName(&tn, "hgtct", ".bed");
	ctFileName = cloneString(tn.forCgi);
	}
    customTrackSave(ctList, ctFileName);
    cartSetString(cart, "ct", ctFileName);
    }

if (! gotResults)
    printf("\n# No results returned from query.\n\n");
bedFreeList(&bedList);
}


void doGetBrowserLinks()
{
struct hTableInfo *hti = getOutputHti();
struct bed *bedList = getBedList(FALSE, NULL);
struct bed *bedPtr;
char *table = getTableName();
char *track = getTrackName();
char *track2 = getTrack2Name();
char track2CGI[128];
int itemCount;

saveOutputOptionsState();
saveIntersectOptionsState();

webStart(cart, "Table Browser: %s: %s", freezeName, linksPhase);
printf("<H3> Links to Genome Browser from %s </H3>\n", getTableName());
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Hyperlinks\">"
     "<B>Help</B></A><P>");

if ((track2 != NULL) && (track2[0] != 0))
    snprintf(track2CGI, sizeof(track2CGI), "&%s=full", track2);
else
    track2CGI[0] = 0;

itemCount = 0;
for (bedPtr = bedList;  bedPtr != NULL;  bedPtr = bedPtr->next)
    {
    printf("<A HREF=\"%s?db=%s&position=%s:%d-%d&%s=full%s&hgsid=%d\"",
	   hgTracksName(), hGetDb(), bedPtr->chrom, bedPtr->chromStart+1,
	   bedPtr->chromEnd, track, track2CGI, cartSessionId(cart));
    printf(" TARGET=_blank> %s %s:%d-%d <BR>\n", bedPtr->name, bedPtr->chrom,
	   bedPtr->chromStart+1, bedPtr->chromEnd);
    itemCount++;
    }
bedFreeList(&bedList);
if (itemCount == 0)
    puts("No results returned from query.");
webEnd();
}


void doIntersectOptions()
{
struct bed *bedList, *bedPtr;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char *outputType = cgiString("outputType");
char *table = getTableName();
char *table2 = getTable2Name();
char *track = getTrackName();
char *db = getTableDb();
char *db2 = getTable2Db();
char *setting, *op;
char fullTableName2[256];
char query[256];
char name[128];
boolean gotFirst;

saveOutputOptionsState();

webStart(cart, "Table Browser: %s: %s", freezeName, intersectOptionsPhase);
checkTableExists(fullTableName);
if (table2 == NULL)
    webAbort("Missing table selection",
	     "Please choose a table and try again.");
getFullTableName(fullTableName2, chrom, table2);
checkTable2Exists(fullTableName2);
if (! tableIsPositional)
    webAbort("Error", "Table %s is not a positional table.", table);
if (! sameString(outputType, seqOptionsPhase) &&
    ! sameString(outputType, bedOptionsPhase) &&
    ! sameString(outputType, linksPhase) &&
    ! sameString(outputType, statsPhase) &&
    ! sameString(outputType, gffPhase))
    webAbort("Error", "Please choose one of the supported output formats "
	     "(FASTA, BED, HyperLinks, GTF or Summary/Statistics) for "
	     "intersection of tables.");

puts("<FORM ACTION=\"/cgi-bin/hgText\" NAME=\"mainForm\" METHOD=\"GET\">\n");
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();
cgiMakeHiddenVar("outputType", outputType);
cgiMakeHiddenVar("table2", getTable2Var());
preserveConstraints(fullTableName, db, NULL);
preserveConstraints(fullTableName2, db2, "2");
printf("<H3> Specify how to combine tables %s and %s: </H3>\n",
       table, table2);
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Intersection\">"
     "<B>Help</B></A><P>");

printf("These combinations will maintain the gene/alignment structure (if any) of %s: <P>\n",
       table);
op = cartCgiUsualString(cart, "hgt.intersectOp", "any");
cgiMakeRadioButton("hgt.intersectOp", "any", sameString(op, "any"));
printf("All %s records that have any overlap with %s <P>\n",
       table, table2);
cgiMakeRadioButton("hgt.intersectOp", "none", sameString(op, "none"));
printf("All %s records that have no overlap with %s <P>\n",
       table, table2);
cgiMakeRadioButton("hgt.intersectOp", "more", sameString(op, "more"));
printf("All %s records that have at least ",
       table);
setting = cartCgiUsualString(cart, "hgt.moreThresh", "80");
cgiMakeTextVar("hgt.moreThresh", setting, 3);
printf(" %% overlap with %s <P>\n", table2);
cgiMakeRadioButton("hgt.intersectOp", "less", sameString(op, "less"));
printf("All %s records that have at most ",
       table);
setting = cartCgiUsualString(cart, "hgt.lessThresh", "80");
cgiMakeTextVar("hgt.lessThresh", setting, 3);
printf(" %% overlap with %s <P>\n", table2);

printf("These combinations will discard the gene/alignment structure (if any) of %s and produce a simple list of position ranges.\n",
       table);
puts("To complement a table means to consider a position included if it \n"
     "is <I>not</I> included in the table. <P>");
cgiMakeRadioButton("hgt.intersectOp", "and", sameString(op, "and"));
printf("Base-pair-wise intersection (AND) of %s and %s <P>\n",
       table, table2);
cgiMakeRadioButton("hgt.intersectOp", "or", sameString(op, "or"));
printf("Base-pair-wise union (OR) of %s and %s <P>\n",
       table, table2);
cgiMakeCheckBox("hgt.invertTable", cgiBoolean("hgt.invertTable"));
printf("Complement %s before intersection/union <P>\n", table);
cgiMakeCheckBox("hgt.invertTable2", cgiBoolean("hgt.invertTable2"));
printf("Complement %s before intersection/union <P>\n", table2);

cgiMakeButton("phase", outputType);

printf("<P><HR><H3> (Optional) Filter %s Records by Field Values </H3>\n",
       table2);
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Constraints\">"
     "<B>Help</B></A><P>");
if (sameString(customTrackPseudoDb, db2))
    filterOptionsCustomTrack(table2, "2");
else
    filterOptionsTableDb(fullTableName2, db2, "2");
cgiMakeButton("phase", outputType);
puts("</FORM>");
webEnd();
}


void descForm()
/* Print out an HTML FORM showing table fields and types, and offering 
 * histograms for the text/enum fields. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
struct dyString *query = newDyString(256);
char **row;
char *table = getTableName();
char *db = getTableDb();
char button[64];

// Not supported for custom tracks, just for database tables.
if (sameString(customTrackPseudoDb, db))
    return;

dyStringClear(query);
dyStringPrintf(query, "desc %s", fullTableName);
if (sameString(database, db))
    conn = hAllocConn();
else
    conn = hAllocConn2();
sr = sqlGetResult(conn, query->string);
printf("<FORM ACTION=\"%s\" NAME=\"descForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
preserveConstraints(fullTableName, db, NULL);
cgiContinueHiddenVar("position");
printf("<H4> Fields of %s: </H4>\n", table);
puts("<TABLE BORDER=1> <TR> <TH>name</TH> <TH>type</TH> <TH></TH> </TR>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("<TR> <TD>%s</TD> <TD>%s</TD> <TD>", row[0], row[1]);
    if ((isSqlStringType(row[1]) || startsWith("enum", row[1])) &&
	! sameString(row[1], "longblob"))
	{
	snprintf(button, sizeof(button), "%s for %s", histPhase, row[0]);
	cgiMakeButton("phase", button);
	}
    puts("</TD> </TR>\n");
    }
puts("</TABLE>");
puts("</FORM>");
sqlFreeResult(&sr);

if (sameString(database, db))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
}

void doGetStatsNonpositional()
/* Print out statistics about nonpositional query results. */
{
struct sqlConnection *conn;
struct dyString *query = newDyString(256);
char **row;
char *constraints;
char *table = getTableName();
char *db = getTableDb();
int numRows;

saveOutputOptionsState();

webStart(cart, "Table Browser: %s: %s", freezeName, statsPhase);
checkTableExists(fullTableName);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
preserveConstraints(fullTableName, db, NULL);
cgiContinueHiddenVar("position");

printf("<H4> Get "
       "<A HREF=\"/goldenPath/help/hgTextHelp.html#Formats\">Output</A>"
       ": </H4>\n");
cgiMakeDropList("outputType", outputTypeNonPosMenu, outputTypeNonPosMenuSize,
		statsPhase);
cgiMakeButton("phase", getOutputPhase);
puts("</FORM>");

puts("<HR>");
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Stats\">"
     "<B>Help</B></A><P>");
printf("<H4> Your query on %s: </H4>\n", table);
constraints = constrainFields(NULL);
if ((constraints != NULL) && (constraints[0] == 0))
    constraints = NULL;

if (constraints != NULL)
    printf("Constraints on %s: %s<P>\n", table, constraints);
else
    printf("No constraints selected on fields of %s.<P>\n", table);

dyStringClear(query);
dyStringPrintf(query, "select count(*) from %s%s%s", table,
	       (constraints ? " where "   : ""),
	       (constraints ? constraints : ""));
if (sameString(database, db))
    conn = hAllocConn();
else
    conn = hAllocConn2();
numRows = sqlQuickNum(conn, query->string);
if (sameString(database, db))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
printf("Number of rows in %s%s: %d<P>\n", table,
       constraints ? " matching constraints" : "", numRows);

descForm();
webEnd();
}

struct slName *getOrderedChromList()
/* Put the _random's at the end, and break them into two lines. */
/* Also, put the alpha-name chroms after the numeric-name chroms. */
{
struct slName *randList = NULL, *nonrList = NULL;
struct slName *randAlphList = NULL, *nonrAlphList = NULL;
struct slName *chromPtr;
struct slName *chromList = hAllChromNames();

for (chromPtr=chromList;  chromPtr != NULL;  chromPtr=chromPtr->next)
    {
    struct slName *newEl = newSlName(chromPtr->name);
    if (endsWith(chromPtr->name, "_random"))
	{
	if (isdigit(chromPtr->name[3]))
	    slAddHead(&randList, newEl);
	else
	    slAddHead(&randAlphList, newEl);
	}
    else
	{
	if (isdigit(chromPtr->name[3]))
	    slAddHead(&nonrList, newEl);
	else
	    slAddHead(&nonrAlphList, newEl);
	}
    }
slFreeList(&chromList);
slReverse(&nonrList);
slReverse(&nonrAlphList);
slReverse(&randList);
slReverse(&randAlphList);
slCat(randList, randAlphList);
slCat(nonrAlphList, randList);
chromList = slCat(nonrList, nonrAlphList);
return(chromList);
}

void getCumulativeStats(int **Xarrs, int *Ns, int num, struct intStats *stats)
/* Copy the arrays X[1]...X[num] into X[0], according to lengths in N.
 * Put stats on X[0] into stats[0]. */
{
/* Actually, only go to the trouble if there are multiple arrays.
 * If not, just copy the single array [1] to the cumulative location [0]. */
if (num > 1)
    {
    int i;
    int offset = 0;
    Xarrs[0] = needMem(Ns[0] * sizeof(int));
    for (i=1;  i <= num;  i++)
	{
	memcpy(Xarrs[0]+offset, Xarrs[i], Ns[i] * sizeof(int));
	freez(&(Xarrs[i]));
	offset += Ns[i];
	}
    assert(offset == Ns[0]);
    intStatsFromArr(Xarrs[0], Ns[0], stats);
    }
else
    {
    Xarrs[0] = Xarrs[1];
    intStatsCopy(stats, &(stats[1]));
    }
}

void getBedBaseCounts(struct bed *bedList, boolean hasBlocks,
		      int *utr5s, int *cdss, int *utr3s)
/* Given a list of beds with CDS, tally up the number of bases in 
 * 5'UTR, CDS and 3'UTR and store them in the count arrays. */
{
struct bed *bed;
int j;

for (bed=bedList, j=0;  bed != NULL;  bed=bed->next, j++)
    {
    int utr5=0, cds=0, utr3=0;
    boolean isRc = (bed->strand[0] == '-');
    if (hasBlocks)
	{
	int k;
	for (k=0;  k < bed->blockCount; k++)
	    {
	    int s = bed->chromStart + bed->chromStarts[k];
	    int e = s + bed->blockSizes[k];
	    if (e < bed->thickStart)
		if (isRc)
		    utr3 += e - s;
		else
		    utr5 += e - s;
	    else if (s < bed->thickStart)
		if (e > bed->thickEnd)
		    {
		    cds += bed->thickEnd - bed->thickStart;
		    if (isRc)
			{
			utr3 += bed->thickStart - s;
			utr5 += e - bed->thickEnd;
			}
		    else
			{
			utr5 += bed->thickStart - s;
			utr3 += e - bed->thickEnd;
			}
		    }
		else
		    {
		    cds += e - bed->thickStart;
		    if (isRc)
			utr3 += bed->thickStart - s;
		    else
			utr5 += bed->thickStart - s;
		    }
	    else if (s > bed->thickEnd)
		if (isRc)
		    utr5 += e - s;
		else
		    utr3 += e - s;
	    else if (e > bed->thickEnd)
		{
		cds += bed->thickEnd - s;
		if (isRc)
		    utr5 += e - bed->thickEnd;
		else
		    utr3 += e - bed->thickEnd;
		}
	    else
		cds += e - s;
	    }
	}
    else
	{
	cds  = bed->thickEnd - bed->thickStart;
	if (isRc)
	    {
	    utr3 = bed->thickStart - bed->chromStart;
	    utr5 = bed->chromEnd - bed->thickEnd;
	    }
	else
	    {
	    utr5 = bed->thickStart - bed->chromStart;
	    utr3 = bed->chromEnd - bed->thickEnd;
	    }
	}
    utr5s[j] = utr5;
    cdss[j]  = cds;
    utr3s[j] = utr3;
    }
}

char *breakChromRandomName(char *name)
/* If name ends with _random, put an HTML linebreak in there. */
{
if (endsWith(name, "_random"))
    {
    char buf[32];
    char *ptr = strstr(name, "_random");
    *ptr = 0;
    snprintf(buf, sizeof(buf), "%s_<BR>random", name);
    return(cloneString(buf));
    }
else
    return(cloneString(name));
}

void doGetStatsPositional()
/* Print out statistics about positional query results. */
{
struct hTableInfo *hti = getOutputHti();
struct slName *chromList, *chromPtr;
struct intStats *chromLengthStats;
struct intStats *scoreStats;
struct intStats *utr5Stats;
struct intStats *cdsStats;
struct intStats *utr3Stats;
struct intStats *blockCountStats;
struct intStats *blockSizeStats;
char *db = getTableDb();
char *table = getTableName();
char *db2 = getTable2Db();
char *table2 = getTable2Name();
char *op = cgiOptionalString("hgt.intersectOp");
char *constraints, *constraints2;
char fullTableName2[256];
int numChroms;
int numCols;
int *itemCounts, *bitCounts, *itemUncCounts;
int *strandPCounts, *strandMCounts, *strandQCounts;
int **chromLengthArrs;
int **scoreArrs;
int **utr5Arrs;
int **cdsArrs;
int **utr3Arrs;
int **blockCountArrs;
int **blockSizeArrs;
int i, j;

saveOutputOptionsState();
saveIntersectOptionsState();

if (op == NULL)
    table2 = NULL;

if (allGenome)
    chromList = getOrderedChromList();
else
    chromList = newSlName(chrom);

numChroms = slCount(chromList);
itemCounts = needMem((numChroms+1) * sizeof(int));
bitCounts = needMem((numChroms+1) * sizeof(int));
itemUncCounts = needMem((numChroms+1) * sizeof(int));
strandPCounts = needMem((numChroms+1) * sizeof(int));
strandMCounts = needMem((numChroms+1) * sizeof(int));
strandQCounts = needMem((numChroms+1) * sizeof(int));
chromLengthArrs = needMem((numChroms+1) * sizeof(int *));
chromLengthStats = needMem((numChroms+1) * sizeof(struct intStats));
scoreArrs = needMem((numChroms+1) * sizeof(int *));
scoreStats = needMem((numChroms+1) * sizeof(struct intStats));
utr5Arrs = needMem((numChroms+1) * sizeof(int *));
utr5Stats = needMem((numChroms+1) * sizeof(struct intStats));
cdsArrs = needMem((numChroms+1) * sizeof(int *));
cdsStats = needMem((numChroms+1) * sizeof(struct intStats));
utr3Arrs = needMem((numChroms+1) * sizeof(int *));
utr3Stats = needMem((numChroms+1) * sizeof(struct intStats));
blockCountArrs = needMem((numChroms+1) * sizeof(int *));
blockCountStats = needMem((numChroms+1) * sizeof(struct intStats));
blockSizeArrs = needMem((numChroms+1) * sizeof(int *));
blockSizeStats = needMem((numChroms+1) * sizeof(struct intStats));

webStart(cart, "Table Browser: %s: %s", freezeName, statsPhase);
checkTableExists(fullTableName);

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();
preserveConstraints(fullTableName, db, NULL);
preserveTable2();

printf("<H4> Get "
       "<A HREF=\"/goldenPath/help/hgTextHelp.html#Formats\">Output</A>"
       ": </H4>\n");
cgiMakeDropList("outputType", outputTypePosMenu, outputTypePosMenuSize,
		statsPhase);
cgiMakeButton("phase", getOutputPhase);
puts("</FORM>");

puts("<HR>");
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Stats\">"
     "<B>Help</B></A><P>");
printf("<H4> Your query on %s%s%s: </H4>\n", table,
       (table2 != NULL) ? " vs. " : "",
       (table2 != NULL) ? table2 : "");
constraints = constrainFields(NULL);
if ((constraints != NULL) && (constraints[0] == 0))
    constraints = NULL;
constraints2 = constrainFields("2");
if ((constraints2 != NULL) && (constraints2[0] == 0))
    constraints2 = NULL;

printf("Position range: %s\n", position);
printf("<P> Primary table: %s\n", table);
if (constraints != NULL)
    printf("<P> Constraints on %s: %s \n", table, constraints);
else
    printf("<P> No additional constraints selected on fields of %s.\n", table);
if (table2 != NULL)
    {
    char tableUse[128], table2Use[128];
    printf("<P> Secondary table: %s\n", table2);
    if (constraints2 != NULL)
	printf("<P> Constraints on %s: %s\n", table2, constraints2);
    else
	printf("<P> No additional constraints selected on fields of %s.\n",
	       table2);
    if (cgiBoolean("hgt.invertTable"))
	snprintf(tableUse, sizeof(tableUse), "complement of %s", table);
    else
	strncpy(tableUse, table, sizeof(tableUse));
    if (cgiBoolean("hgt.invertTable2"))
	snprintf(table2Use, sizeof(table2Use), "complement of %s", table2);
    else
	strncpy(table2Use, table2, sizeof(table2Use));
    puts("<P> Means of combining tables:");
    if (sameString(op, "any"))
	printf("Include %s records that have any overlap with %s <P>\n",
	       table, table2);
    else if (sameString(op, "none"))
	printf("Include %s records that have no overlap with %s <P>\n",
	       table, table2);
    else if (sameString(op, "more"))
	printf("Include %s records that have at least %s%% overlap with %s <P>\n",
	       table, cgiString("hgt.moreThresh"), table2);
    else if (sameString(op, "less"))
	printf("Include %s records that have at most %s%% overlap with %s <P>\n",
	       table, cgiString("hgt.lessThresh"), table2);
    else if (sameString(op, "and"))
	printf("List positions of base pairs covered by both %s and %s <P>\n",
	       tableUse, table2Use);
    else if (sameString(op, "or"))
	printf("List positions of base pairs covered by either %s or %s <P>\n",
	       tableUse, table2Use);
    else
	errAbort("Unrecognized table combination type.");
    }

/* Print out a big table of stats... */
puts("<P>");
numCols = (numChroms > 1) ? numChroms+1 : 1;
for (chromPtr=chromList,i=1;  chromPtr != NULL;  chromPtr=chromPtr->next,i++)
    {
    struct bed *bedListConstr, *bed;
    int count;
    getFullTableName(fullTableName, chromPtr->name, table);
    bedListConstr = getBedList(FALSE, chrom);
    count = slCount(bedListConstr);
    itemCounts[0] += count;
    itemCounts[i] = count;
    if (count > 0)
	{
	struct featureBits *fbList =
	    fbFromBed("out", hti, bedListConstr, winStart, winEnd,
		      FALSE, FALSE);
	int chromSize = hChromSize(chrom);
	Bits *bits = bitAlloc(chromSize+8);
	fbOrBits(bits, chromSize, fbList, 0);
	count = bitCountRange(bits, 0, chromSize);
	bitFree(&bits);
	}
    else
	count = 0;
    bitCounts[0] += count;
    bitCounts[i] = count;
    if ((constraints != NULL) || ((table2 != NULL) && (constraints2 != NULL)))
	{
	struct bed *bedListUnc = getBedList(TRUE, chrom);
	count = slCount(bedListUnc);
	itemUncCounts[0] += count;
	itemUncCounts[i] = count;
	}
    if (itemCounts[i] > 0)
	{
	chromLengthArrs[i] = needMem(itemCounts[i] * sizeof(int));
	for (bed=bedListConstr, j=0;  bed != NULL;  bed=bed->next, j++)
	    chromLengthArrs[i][j] = bed->chromEnd - bed->chromStart;
	intStatsFromArr(chromLengthArrs[i], itemCounts[i],
			&(chromLengthStats[i]));
	if (hti->scoreField[0] != 0)
	    {
	    scoreArrs[i] = needMem(itemCounts[i] * sizeof(int));
	    for (bed=bedListConstr, j=0;  bed != NULL;  bed=bed->next, j++)
		scoreArrs[i][j] = bed->score;
	    intStatsFromArr(scoreArrs[i], itemCounts[i], &(scoreStats[i]));
	    }
	if (hti->strandField[0] != 0)
	    {
	    for (bed=bedListConstr;  bed != NULL;  bed=bed->next)
		{
		if (bed->strand[0] == '+')
		    {
		    strandPCounts[0]++;
		    strandPCounts[i]++;
		    }
		else if (bed->strand[0] == '-')
		    {
		    strandMCounts[0]++;
		    strandMCounts[i]++;
		    }
		else
		    {
		    strandQCounts[0]++;
		    strandQCounts[i]++;
		    }
		}
	    }
	if (hti->hasCDS)
	    {
	    utr5Arrs[i] = needMem(itemCounts[i] * sizeof(int));
	    cdsArrs[i]  = needMem(itemCounts[i] * sizeof(int));
	    utr3Arrs[i] = needMem(itemCounts[i] * sizeof(int));
	    getBedBaseCounts(bedListConstr, hti->hasBlocks,
			     utr5Arrs[i], cdsArrs[i], utr3Arrs[i]);
	    intStatsFromArr(utr5Arrs[i], itemCounts[i], &(utr5Stats[i]));
	    intStatsFromArr(cdsArrs[i],  itemCounts[i], &(cdsStats[i]));
	    intStatsFromArr(utr3Arrs[i], itemCounts[i], &(utr3Stats[i]));
	    }
	if (hti->hasBlocks)
	    {
	    blockCountArrs[i] = needMem(itemCounts[i] * sizeof(int));
	    blockSizeArrs[i]  = needMem(itemCounts[i] * sizeof(int));
	    for (bed=bedListConstr, j=0;  bed != NULL;  bed=bed->next, j++)
		{
		int k, totalSize = 0;
		if (bed->blockCount < 1)
		    errAbort("Illegal bed: appears to have blocks, but blockCount is %d (%s\t%d\t%d\t%s...)",
			     bed->blockCount, bed->chrom, bed->chromStart,
			     bed->chromEnd, bed->name);
		blockCountArrs[i][j] = bed->blockCount;
		for (k=0;  k < bed->blockCount;  k++)
		    totalSize += bed->blockSizes[k];
		// we lose some granularity here, but not too much...
		blockSizeArrs[i][j] = round(totalSize / bed->blockCount);
		}
	    intStatsFromArr(blockCountArrs[i], itemCounts[i],
			    &(blockCountStats[i]));
	    intStatsFromArr(blockSizeArrs[i], itemCounts[i],
			    &(blockSizeStats[i]));
	    }
	}
    }

getCumulativeStats(chromLengthArrs, itemCounts, numChroms,
		   chromLengthStats);
if (hti->scoreField[0] != 0)
    getCumulativeStats(scoreArrs, itemCounts, numChroms, scoreStats);
if (hti->hasCDS)
    {
    getCumulativeStats(utr5Arrs, itemCounts, numChroms, utr5Stats);
    getCumulativeStats(cdsArrs,  itemCounts, numChroms, cdsStats);
    getCumulativeStats(utr3Arrs, itemCounts, numChroms, utr3Stats);
    }
if (hti->hasBlocks)
    {
    getCumulativeStats(blockCountArrs, itemCounts, numChroms, blockCountStats);
    getCumulativeStats(blockSizeArrs, itemCounts, numChroms, blockSizeStats);
    }

/* Use fixed-font for decimal point/integer alignment: */
puts("<TABLE BORDER=\"1\">");
/* All these non-blocking spaces are to widen the first column so that some 
 * row descriptions below do not get wrapped (which would mess up the <br> 
 * formatting of row contents). */
puts("<TR><TH><TT>statistic&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</TT></TH>");
/* These non-blocking spaces are for decimal point/integer alignment: */
puts("<TH ALIGN=\"RIGHT\"><TT>total&nbsp;&nbsp;&nbsp;</TT></TH>");
if (numCols > 1)
    for (chromPtr=chromList;  chromPtr != NULL;  chromPtr=chromPtr->next)
	printf("<TH><TT>%s</TT></TH>", breakChromRandomName(chromPtr->name));
puts("</TR>");
puts("<TR><TD><TT>items matching query</TT></TD>");
for (i=0;  i < numCols;  i++)
    printf("<TD ALIGN=\"RIGHT\"><TT>%d&nbsp;&nbsp;&nbsp;</TT></TD>",
	   itemCounts[i]);
puts("</TR>");
puts("<TR><TD><TT>bases covered by matching items</TT></TD>");
for (i=0;  i < numCols;  i++)
    printf("<TD ALIGN=\"RIGHT\"><TT>%d&nbsp;&nbsp;&nbsp;</TT></TD>",
	   bitCounts[i]);
puts("</TR>");
if ((constraints != NULL) || ((table2 != NULL) && (constraints2 != NULL)))
    {
    puts("<TR><TD><TT>items without constraints</TT></TD>");
    for (i=0;  i < numCols;  i++)
	printf("<TD ALIGN=\"RIGHT\"><TT>%d&nbsp;&nbsp;&nbsp;</TT></TD>",
	       itemUncCounts[i]);
    puts("</TR>");
    }
if (itemCounts[0] > 0)
    {
    if (hti->strandField[0] != 0)
	{
	puts("<TR><TD><TT>items on strand: <br>+<br>-<br>?</TT></TD>");
	for (i=0;  i < numCols;  i++)
	    printf("<TD ALIGN=\"RIGHT\"><TT><br>%d&nbsp;&nbsp;&nbsp;<br>%d&nbsp;&nbsp;&nbsp;<br>%d&nbsp;&nbsp;&nbsp;</TT></TD>",
		   strandPCounts[i], strandMCounts[i], strandQCounts[i]);
	puts("</TR>");
	}
    puts("<TR><TD><TT>(chromEnd - chromStart): <br>min<br>avg<br>max<br>stdev</TT></TD>");
    for (i=0;  i < numCols;  i++)
	printf("<TD ALIGN=\"RIGHT\"><TT><br>%d&nbsp;&nbsp;&nbsp;<br>%.2f<br>%d&nbsp;&nbsp;&nbsp;<br>%.2f</TT></TD>",
	       chromLengthStats[i].min, chromLengthStats[i].avg,
	       chromLengthStats[i].max, chromLengthStats[i].stdev);
    puts("</TR>");
    if (hti->scoreField[0] != 0)
	{
	puts("<TR><TD><TT>score: <br>min<br>avg<br>max<br>stdev</TT></TD>");
	for (i=0;  i < numCols;  i++)
	    printf("<TD ALIGN=\"RIGHT\"><TT><br>%d&nbsp;&nbsp;&nbsp;<br>%.2f<br>%d&nbsp;&nbsp;&nbsp;<br>%.2f</TT></TD>",
		   scoreStats[i].min, scoreStats[i].avg,
		   scoreStats[i].max, scoreStats[i].stdev);
	puts("</TR>");
	}
    if (hti->hasCDS != 0)
	{
	char *exons = hti->hasBlocks ? " exons" : "";
	printf("<TR><TD><TT>bases in 5\' UTR%s: <br>min<br>avg<br>max<br>stdev</TT></TD>\n",
	       exons);
	for (i=0;  i < numCols;  i++)
	    printf("<TD ALIGN=\"RIGHT\"><TT><br>%d&nbsp;&nbsp;&nbsp;<br>%.2f<br>%d&nbsp;&nbsp;&nbsp;<br>%.2f</TT></TD>",
		   utr5Stats[i].min, utr5Stats[i].avg,
		   utr5Stats[i].max, utr5Stats[i].stdev);
	puts("</TR>");
	printf("<TR><TD><TT>bases in CDS%s: <br>min<br>avg<br>max<br>stdev</TT></TD>\n",
	       exons);
	for (i=0;  i < numCols;  i++)
	    printf("<TD ALIGN=\"RIGHT\"><TT><br>%d&nbsp;&nbsp;&nbsp;<br>%.2f<br>%d&nbsp;&nbsp;&nbsp;<br>%.2f</TT></TD>",
		   cdsStats[i].min, cdsStats[i].avg,
		   cdsStats[i].max, cdsStats[i].stdev);
	puts("</TR>");
	printf("<TR><TD><TT>bases in 3\' UTR%s: <br>min<br>avg<br>max<br>stdev</TT></TD>\n",
	       exons);
	for (i=0;  i < numCols;  i++)
	    printf("<TD ALIGN=\"RIGHT\"><TT><br>%d&nbsp;&nbsp;&nbsp;<br>%.2f<br>%d&nbsp;&nbsp;&nbsp;<br>%.2f</TT></TD>",
		   utr3Stats[i].min, utr3Stats[i].avg,
		   utr3Stats[i].max, utr3Stats[i].stdev);
	puts("</TR>");
	}
    if (hti->hasBlocks != 0)
	{
	char *thingy = startsWith("psl", hti->type) ? "gapless block" : "exon";
	printf("<TR><TD><TT>%ss: <br>min<br>avg<br>max<br>stdev</TT></TD>\n",
	       thingy);
	for (i=0;  i < numCols;  i++)
	    printf("<TD ALIGN=\"RIGHT\"><TT><br>%d&nbsp;&nbsp;&nbsp;<br>%.2f<br>%d&nbsp;&nbsp;&nbsp;<br>%.2f</TT></TD>",
		   blockCountStats[i].min, blockCountStats[i].avg,
		   blockCountStats[i].max, blockCountStats[i].stdev);
	puts("</TR>");
	printf("<TR><TD><TT>bases per %s: <br>min<br>avg<br>max<br>stdev</TT></TD>\n",
	       thingy);
	for (i=0;  i < numCols;  i++)
	    printf("<TD ALIGN=\"RIGHT\"><TT><br>%d&nbsp;&nbsp;&nbsp;<br>%.2f<br>%d&nbsp;&nbsp;&nbsp;<br>%.2f</TT></TD>",
		   blockSizeStats[i].min, blockSizeStats[i].avg,
		   blockSizeStats[i].max, blockSizeStats[i].stdev);
	puts("</TR>");
	}
    }
puts("</TABLE>");
freez(&(chromLengthArrs[0]));
freez(&chromLengthArrs);
freez(&chromLengthStats);
if (hti->scoreField[0] != 0)
    freez(&(scoreArrs[0]));
freez(&scoreArrs);
freez(&scoreStats);
if (hti->hasCDS)
    {
    freez(&(utr5Arrs[0]));
    freez(&(cdsArrs[0]));
    freez(&(utr3Arrs[0]));
    }
if (hti->hasBlocks)
    {
    freez(&(blockCountArrs[0]));
    freez(&(blockSizeArrs[0]));
    }
freez(&utr5Arrs);
freez(&utr5Stats);
freez(&cdsArrs);
freez(&cdsStats);
freez(&utr3Arrs);
freez(&utr3Stats);
freez(&blockCountArrs);
freez(&blockCountStats);
freez(&blockSizeArrs);
freez(&blockSizeStats);
freez(&itemCounts);
freez(&bitCounts);
freez(&itemUncCounts);
freez(&strandPCounts);
freez(&strandMCounts);
freez(&strandQCounts);

descForm();
webEnd();
}


void doGetStats()
/* Print out statistics about the query results. */
{
if (tableIsPositional)
    doGetStatsPositional();
else
    doGetStatsNonpositional();
}


static int descendingFreqCmp(const void *el1, const void *el2)
/* descending sort on pointers to pointers to hash elements whose 
 * values are int (not pointer to int! because of hashAddInt()). */
{
const struct hashEl *hel1 = *((struct hashEl **)el1);
const struct hashEl *hel2 = *((struct hashEl **)el2);
int v1 = (int)(hel1->val);
int v2 = (int)(hel2->val);
int dif;

dif = v2 - v1;
if (dif == 0)
    dif = strcmp(hel1->name, hel2->name);
return(dif);
}

void doGetHistogram()
/* Print out a histogram for the text value of some field. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
struct dyString *query = newDyString(256);
struct hash *freqHash = newHash(16);
struct hashEl *els, *el;
struct slName *chromList, *chromPtr;
char **row;
char *words[5];
char *constraints;
char *table = getTableName();
char *db = getTableDb();
struct hTableInfo *hti = getHti(db, table);
char *phase = cgiString("phase");
char *field;
int count;
int maxFreq, freq;
int i;
double scale;

webStart(cart, "Table Browser: %s: %s", freezeName, histPhase);
checkTableExists(fullTableName);
count = chopLine(phase, words);
if (count < 4)
    errAbort("doGetHistogram: CGI var phase should be of the form \"%s for XXXX\" where XXXX is a valid field name", histPhase);
field = words[3];
if (sameString(customTrackPseudoDb, db))
    {
    printf("Histograms are only supported on fields of database tables, not custom tracks.\n");
    webEnd();
    return;
    }

constraints = constrainFields(NULL);
if ((constraints != NULL) && (constraints[0] == 0))
    constraints = NULL;
if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

if (sameString(database, db))
    conn = hAllocConn();
else
    conn = hAllocConn2();
for (chromPtr=chromList;  chromPtr != NULL;  chromPtr=chromPtr->next)
    {
    getFullTableName(fullTableName, chromPtr->name, table);
    dyStringClear(query);
    dyStringPrintf(query, "SELECT %s FROM %s", field, fullTableName);
    if (tableIsPositional)
	{
	dyStringPrintf(query, " WHERE %s < %d AND %s > %d",
		       hti->startField, winEnd, hti->endField, winStart);
	if (! sameString("", hti->chromField))
	    dyStringPrintf(query, " AND %s = \'%s\'",
			   hti->chromField, chrom);
	if ((constraints != NULL) && (constraints[0] != 0))
	    dyStringPrintf(query, " AND %s", constraints);
	}
    else if (constraints)
	dyStringPrintf(query, " WHERE %s", constraints);
    sr = sqlGetResult(conn, query->string);
    // make a hash of field values to frequencies:
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if ((el = hashLookup(freqHash, row[0])) == NULL)
	    hashAddInt(freqHash, row[0], 1);
	else
	    {
	    if (! sameString(el->name, row[0]))
		printf("Hash-collision warning: %s --> %s<P>\n",
		       el->name, row[0]);
	    (int)(el->val) = (int)(el->val) + 1;
	    }
	}
    sqlFreeResult(&sr);
    }
if (sameString(database, db))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);

// sort the elements by count, descending:
els = hashElListHash(freqHash);
if (els == NULL)
    {
    printf("No results returned from query.\n");
    webEnd();
    return;
    }
slSort(&els, descendingFreqCmp);
maxFreq = (int)els->val;
if (maxFreq > 50)
    scale = (50.0 / maxFreq);
else
    scale = 1.0;
// print out name, bar, count for each element
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#Stats\">"
     "<B>Help</B></A><P>");
printf("<H4> Histogram of values for %s.%s%s%s, sorted by descending frequency: </H4>\n",
       table, field,
       (constraints? " "         : ""),
       (constraints? constraints : ""));
puts("<TT><PRE>");
printf("%50s %-50s %8s\n", "value", "", "count");
puts("--------------------------------------------------------------------------------------------------------------");
for (el = els;  el != NULL;  el=el->next)
    {
    printf("%50s ", el->name);
    freq = (int)el->val;
    freq = (int)(scale * freq);
    for (i=0;  i < freq;  i++)
	putchar('*');
    for (i=freq; i < 50;  i++)
	putchar(' ');
    printf(" %8d\n", (int)el->val);
    }
puts("</PRE></TT>");
webEnd();
}


void doMiddle(struct cart *theCart)
/* the main body of the program */
{
char *table = getTableName();
char trash[32];
char *db = getTableDb();

cart = theCart;
getDbAndGenome(cart, &database, &organism);
database = cloneString(database);
hSetDb(database);
hDefaultConnect();

freezeName = hFreezeFromDb(database);
if (freezeName == NULL)
    freezeName = "Unknown";

fullTableName[0] = 0;
position = getPosition(&chrom, &winStart, &winEnd);
if (position == NULL)
    return;
allGenome = isGenome(position);

if (existsAndEqual("submit", "Look up") ||
    (cgiOptionalString("phase") == NULL))
    {
    // Stay in same phase if we're just looking up position.
    char *origPhase = cgiOptionalString("origPhase");
    if (origPhase != NULL)
	cgiVarSet("phase", origPhase);
    }

if (table == NULL || existsAndEqual("phase", chooseTablePhase))
    {
    if (existsAndEqual("table0", "Choose table") &&
	existsAndEqual("table1", "Choose table") &&
	!existsAndEqual("submit", "Look up"))
	webAbort("Missing table selection",
		 "Please choose a table and try again.");
    else
	{
	doChooseTable();
	}
    }
else
    {
    if ((! sameString(database, db)) && (! sameString(hGetDb2(), db)) &&
	(! sameString(customTrackPseudoDb, db)))
	hSetDb2(db);

    if (existsAndEqual("table0", "Choose table") &&
	existsAndEqual("table1", "Choose table"))
	webAbort("Missing table selection", "Please choose a table.");

    if (allGenome)
	getFullTableName(fullTableName, "chr1", table);
    else
	getFullTableName(fullTableName, chrom, table);

    if (sameString(customTrackPseudoDb, db))
	tableIsPositional = TRUE;
    else
	tableIsPositional = hFindChromStartEndFieldsDb(db, fullTableName,
						       trash, trash, trash);
    if (strstr(table, "chrN_") == table)
	tableIsSplit = TRUE;

    if (existsAndEqual("phase", outputOptionsPhase))
	doOutputOptions();
    else if (existsAndEqual("phase", getOutputPhase))
	{
	if (existsAndEqual("outputType", allFieldsPhase) ||
	    existsAndEqual("outputType", oldAllFieldsPhase))
	    doTabSeparated(TRUE);
	else if (existsAndEqual("outputType", chooseFieldsPhase))
	    doChooseFields();
	else if (existsAndEqual("outputType", seqOptionsPhase))
	    doSequenceOptions();
	else if (existsAndEqual("outputType", gffPhase))
	    doGetGFF();
	else if (existsAndEqual("outputType", bedOptionsPhase))
	    doBedOptions();
	else if (existsAndEqual("outputType", linksPhase))
	    doGetBrowserLinks();
	else if (existsAndEqual("outputType", statsPhase))
	    doGetStats();
	else
	    webAbort("Table Browser: CGI option error",
		     "Error: unrecognized value of CGI var outputType: %s",
		     cgiUsualString("outputType", "(Undefined)"));
	}
    else if (existsAndEqual("phase", allFieldsPhase) ||
	     existsAndEqual("phase", oldAllFieldsPhase))
	doTabSeparated(TRUE);
    else if (existsAndEqual("phase", chooseFieldsPhase))
	doChooseFields();
    else if (existsAndEqual("phase", getSomeFieldsPhase))
	doTabSeparated(FALSE);
    else if (existsAndEqual("phase", seqOptionsPhase))
	doSequenceOptions();
    else if (existsAndEqual("phase", getSequencePhase))
	doGetSequence();
    else if (existsAndEqual("phase", gffPhase))
	doGetGFF();
    else if (existsAndEqual("phase", bedOptionsPhase))
	doBedOptions();
    else if (existsAndEqual("phase", getBedPhase))
	doGetBed();
    else if (existsAndEqual("phase", linksPhase))
	doGetBrowserLinks();
    else if (existsAndEqual("phase", statsPhase))
	doGetStats();
    else if (existsAndEqual("phase", intersectOptionsPhase))
	doIntersectOptions();
    else if (existsAndStartsWith("phase", histPhase))
	doGetHistogram();
    else
	webAbort("Table Browser: CGI option error",
		 "Error: unrecognized value of CGI var phase: %s",
		 cgiUsualString("phase", "(Undefined)"));
    }
cartSetString(cart, "db", database);
cartSetString(cart, "position", position);
}


int main(int argc, char *argv[])
/* Process command line. */
{
struct cart *theCart;
char *excludeVars[] = {NULL};

oldVars = hashNew(8);
cgiSpoof(&argc, argv);
// Sometimes we output HTML and sometimes plain text; let each outputter 
// take care of headers instead of using a fixed cart*Shell().
theCart = cartAndCookieWithHtml(hUserCookie(), excludeVars, oldVars, FALSE);
doMiddle(theCart);
cartCheckout(&theCart);
return 0;
}
