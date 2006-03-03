/* hgLoadWiggle - Load a Wiggle track "bed" file into database. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "chromInfo.h"
#include "wiggle.h"
#include "hdb.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgLoadWiggle.c,v 1.15 2006/03/03 22:23:07 hiram Exp $";

/* Command line switches. */
static boolean noBin = FALSE;		/* Suppress bin field. */
static boolean noLoad = FALSE;		/* Do not load table, create tab file */
static boolean strictTab = FALSE;	/* Separate on tabs. */
static boolean oldTable = FALSE;	/* Don't redo table. */
static char *pathPrefix = NULL;	/* path prefix instead of /gbdb/hg16/wib */

static struct hash *chromHash = NULL;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"smallInsertSize", OPTION_INT},
    {"tab", OPTION_BOOLEAN},
    {"noBin", OPTION_BOOLEAN},
    {"noLoad", OPTION_BOOLEAN},
    {"oldTable", OPTION_BOOLEAN},
    {"pathPrefix", OPTION_STRING},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadWiggle - Load a wiggle track definition into database\n"
  "usage:\n"
  "   hgLoadWiggle [options] database track files(s).wig\n"
  "options:\n"
  "   -noBin\tsuppress bin field\n"
  "   -noLoad\tdo not load table, only create .tab file\n"
  "   -oldTable\tadd to existing table\n"
  "   -tab\t\tSeparate by tabs rather than space\n"
  "   -pathPrefix=<path>\t.wib file path prefix to use "
      "(default /gbdb/<DB>/wib)\n"
  "   -verbose=N\tN=2 see # of lines input and SQL create statement,\n"
  "\t\tN=3 see chrom size info, N=4 see details on chrom size info"
  );
}

static struct hash *loadAllChromInfo(char *database)
/* Load up all chromosome infos. */
{
struct chromInfo *el;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
struct hash *ret;
char **row;

ret = newHash(0);

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    verbose(4, "Add hash %s value %u (%#lx)\n", el->chrom, el->size, (unsigned long)&el->size);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return ret;
}

static unsigned chromosomeSize(char *chromosome)
/* Return full extents of chromosome.  Warn and fill in if none. */
{
struct hashEl *el = hashLookup(chromHash,chromosome);

if (el == NULL)
    errAbort("Couldn't find size of chromosome %s", chromosome);
return *(unsigned *)el->val;
}


static int findWiggleSize(char *fileName)
/* Read first line of file and figure out how many words in it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line;
int wordCount;
lineFileNeedNext(lf, &line, NULL);
if (strictTab)
    wordCount = chopTabs(line, words);
else
    wordCount = chopLine(line, words);
if (wordCount == 0)
    errAbort("%s appears to be empty", fileName);
lineFileClose(&lf);
return wordCount;
}

struct wiggleStub
/* A line in a wiggle file with chromosome, start, end position parsed out. */
    {
    struct wiggleStub *next;	/* Next in list. */
    char *chrom;                /* Chromosome . */
    int chromStart;             /* Start position. */
    int chromEnd;		/* End position. */
    char *line;                 /* Line. */
    };

int wiggleStubCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct wiggleStub *a = *((struct wiggleStub **)va);
const struct wiggleStub *b = *((struct wiggleStub **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}


void loadOneWiggle(char *fileName, int wiggleSize, struct wiggleStub **pList)
/* Load one wiggle file.  Make sure all lines have wiggleSize fields.
 * Put results in *pList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line, *dupe;
int wordCount;
struct wiggleStub *wiggle;
int lineCount = 0;

while (lineFileNext(lf, &line, NULL))
    {
    char *chrName;
    int chrStart;
    int chrEnd;

    ++lineCount;
    dupe = cloneString(line);
    if (strictTab)
	wordCount = chopTabs(line, words);
    else
	wordCount = chopLine(line, words);
    lineFileExpectWords(lf, wiggleSize, wordCount);
    chrName = cloneString(words[0]);
    chrStart = lineFileNeedNum(lf, words, 1);
    chrEnd = lineFileNeedNum(lf, words, 2);
    AllocVar(wiggle);
    wiggle->chrom = chrName;
    wiggle->chromStart = chrStart;
    wiggle->chromEnd = chrEnd;
    wiggle->line = dupe;
    slAddHead(pList, wiggle);
    }
lineFileClose(&lf);
verbose(2, "Read %d lines from %s\n", lineCount, fileName);
}

void writeWiggleTab(char *fileName, struct wiggleStub *wiggleList,
	int wiggleSize, char *database)
/* Write out wiggle list to tab-separated file. */
{
struct wiggleStub *wiggle;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, wordCount;

for (wiggle = wiggleList; wiggle != NULL; wiggle = wiggle->next)
    {
    static char *chrom = NULL;
    static unsigned size = 0;
    unsigned start;
    unsigned end;
    unsigned span;
    unsigned count;
    unsigned validCount;
    boolean valid = TRUE;
    if (strictTab)
	wordCount = chopTabs(wiggle->line, words);
    else
	wordCount = chopLine(wiggle->line, words);
    start = sqlUnsigned(words[1]);
    end = sqlUnsigned(words[2]);
    span = sqlUnsigned(words[4]);
    count = sqlUnsigned(words[5]);
    validCount = sqlUnsigned(words[10]);
    if (chrom && differentWord(chrom, words[0]))
	{
	chrom = words[0];
	size = chromosomeSize(chrom);
verbose(3, "chrom: %s size: %u\n", chrom, size);
	}
    else if (!chrom)
	{
	chrom = words[0];
	size = chromosomeSize(chrom);
verbose(3, "chrom: %s size: %u\n", chrom, size);
	}
    valid = TRUE;
    if (end > size)
	{
	unsigned overrun = 0;
	unsigned dropCount = 0;
	overrun = end - size;
	dropCount = 1 + (overrun / span);
	warn("WARNING: Exceeded %s size %u > %u. dropping %u data point(s)", chrom, end, size, dropCount);
	if (dropCount >= count)
	    valid = FALSE;
	else
	    {
	    count -= dropCount;
	    if (validCount > count)
		validCount = count;
	    if ((end-(dropCount*span)) > start)
		end -= dropCount*span;
	    else
		valid = FALSE;
	    }
	}
    if (valid)
	{
	if (!noBin)
	    fprintf(f, "%u\t", hFindBin(wiggle->chromStart, wiggle->chromEnd));
	for (i=0; i<wordCount; ++i)
	    {
	    switch(i)
		{
		case 2:
		    fprintf(f,"%u", end);
		    break;
		case 5:
		    fprintf(f,"%u", count);
		    break;
		case 7:
		    if (pathPrefix )
			fprintf(f,"%s/", pathPrefix );
		    else
			fprintf(f,"/gbdb/%s/wib/", database );
		    fputs(words[i], f);
		    break;
		case 10:
		    fprintf(f,"%u", validCount);
		    break;
		default:
		    fputs(words[i], f);
		    break;
		}
	    if (i == wordCount-1)
		fputc('\n', f);
	    else
		fputc('\t', f);
	    }
	}
    }
fclose(f);
}

void loadDatabase(char *database, char *track, int wiggleSize, struct wiggleStub *wiggleList)
/* Load database from wiggleList. */
{
struct sqlConnection *conn = (struct sqlConnection *)NULL;
struct dyString *dy = newDyString(1024);
char *tab = "wiggle.tab";

if (! noLoad)
    {
    hSetDb(database);
    conn = hAllocConn();
    verbose(1, "Connected to database %s for track %s\n", database, track);
    }

/* First make table definition. */
if ((!oldTable) && (!noLoad))
    {
    int indexLen = hGetMinIndexLength();

    /* Create definition statement. */
    verbose(1, "Creating table definition with %d columns in %s.%s\n",
	    wiggleSize, database, track);
    dyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
    dyStringAppend(dy, "  chrom varchar(255) not null,\n");
    dyStringAppend(dy, "  chromStart int unsigned not null,\n");
    dyStringAppend(dy, "  chromEnd int unsigned not null,\n");
    dyStringAppend(dy, "  name varchar(255) not null,\n");
    dyStringAppend(dy, "  span int unsigned not null,\n");
    dyStringAppend(dy, "  count int unsigned not null,\n");
    dyStringAppend(dy, "  offset int unsigned not null,\n");
    dyStringAppend(dy, "  file varchar(255) not null,\n");
    dyStringAppend(dy, "  lowerLimit double not null,\n");
    dyStringAppend(dy, "  dataRange double not null,\n");
    dyStringAppend(dy, "  validCount int unsigned not null,\n");
    dyStringAppend(dy, "  sumData double not null,\n");
    dyStringAppend(dy, "  sumSquares double not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (!noBin)
	dyStringPrintf(dy, "  INDEX(chrom(%d),bin)\n", indexLen);
    else
	{
	dyStringPrintf(dy, "  INDEX(chrom(%d),chromStart),\n", indexLen);
	dyStringPrintf(dy, "  INDEX(chrom(%d),chromEnd)\n", indexLen);
	}
    dyStringAppend(dy, ")\n");
    verbose(2, "%s", dy->string);
    sqlRemakeTable(conn, track, dy->string);
    }

verbose(1, "Saving %s\n", tab);
writeWiggleTab(tab, wiggleList, wiggleSize, database);

if (! noLoad)
    {
    char comment[256];
    char pathAdded[192];
    verbose(1, "Loading %s\n", database);
    dyStringClear(dy);
    dyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
    sqlUpdate(conn, dy->string);
    if (pathPrefix)
	safef(pathAdded, sizeof(pathAdded), "%s/", pathPrefix);
    else
	safef(pathAdded, sizeof(pathAdded), "/gbdb/%s/wib/", database);
    if (oldTable)
	safef(comment, sizeof(comment),
	    "adding to wiggle table %s from %s/%s with wib path %s", track,
		getCurrentDir(), tab, pathAdded);
    else
	safef(comment, sizeof(comment),
	    "new wiggle table %s from %s/%s with wib path %s", track,
		getCurrentDir(), tab, pathAdded);
    hgHistoryComment(conn, comment);
    verbose(2, "#\t%s\n", comment);
    sqlDisconnect(&conn);
    }
else
    verbose(1, "noLoad option requested, see resulting file: %s\n", tab);
}

void hgLoadWiggle(char *database, char *track, int wiggleCount, char *wiggleFiles[])
/* hgLoadWiggle - Load a generic wiggle file into database. */
{
int wiggleSize = findWiggleSize(wiggleFiles[0]);
struct wiggleStub *wiggleList = NULL;
int i;

chromHash = loadAllChromInfo(database);

if (verboseLevel() > 2)
    {
    struct hashCookie cookie;
    struct hashEl *el;

    cookie = hashFirst(chromHash);

    verbose(3,"chrom\tsize\n");
    while ((el = hashNext(&cookie)) != NULL)
	{
	unsigned size;
	size = chromosomeSize(el->name);
	verbose(3,"%s\t%u\n", el->name, size);
	}
    }

for (i=0; i<wiggleCount; ++i)
    loadOneWiggle(wiggleFiles[i], wiggleSize, &wiggleList);
slSort(&wiggleList, wiggleStubCmp);
loadDatabase(database, track, wiggleSize, wiggleList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

if (argc < 4)
    usage();
noBin = optionExists("noBin");
noLoad = optionExists("noLoad");
strictTab = optionExists("tab");
oldTable = optionExists("oldTable");
pathPrefix = optionVal("pathPrefix",NULL);
verbose(2, "noBin: %s, noLoad: %s, tab: %s, oldTable: %s\n",
	noBin ? "TRUE" : "FALSE",
	noLoad ? "TRUE" : "FALSE",
	strictTab ? "TRUE" : "FALSE",
	oldTable ? "TRUE" : "FALSE");
if (pathPrefix)
    verbose(2, "pathPrefix: %s\n", pathPrefix);
hgLoadWiggle(argv[1], argv[2], argc-3, argv+3);
return 0;
}
