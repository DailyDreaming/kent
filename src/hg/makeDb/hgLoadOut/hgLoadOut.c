/* hgLoadOut - load RepeatMasker .out files into database. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "options.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "hdb.h"
#include "jksql.h"
#include "rmskOut.h"

static char const rcsid[] = "$Id: hgLoadOut.c,v 1.14 2004/12/06 22:33:46 hiram Exp $";

char *createRmskOut = "CREATE TABLE %s (\n"
"   bin smallint unsigned not null,     # bin index field for range queries\n"
"   swScore int unsigned not null,	# Smith Waterman alignment score\n"
"   milliDiv int unsigned not null,	# Base mismatches in parts per thousand\n"
"   milliDel int unsigned not null,	# Bases deleted in parts per thousand\n"
"   milliIns int unsigned not null,	# Bases inserted in parts per thousand\n"
"   genoName varchar(255) not null,	# Genomic sequence name\n"
"   genoStart int unsigned not null,	# Start in genomic sequence\n"
"   genoEnd int unsigned not null,	# End in genomic sequence\n"
"   genoLeft int not null,	# Size of genomic sequence\n"
"   strand char(1) not null,	# Relative orientation + or -\n"
"   repName varchar(255) not null,	# Name of repeat\n"
"   repClass varchar(255) not null,	# Class of repeat\n"
"   repFamily varchar(255) not null,	# Family of repeat\n"
"   repStart int not null,	# Start in repeat sequence\n"
"   repEnd int not null,	# End in repeat sequence\n"
"   repLeft int not null,	# Size of repeat sequence\n"
"   id char(1) not null,	# '*' or ' '.  I don't know what this means\n"
"             #Indices\n";

boolean noBin = FALSE;
boolean noSplit = FALSE;
char *tabFileName = NULL;
char *suffix = NULL;
FILE *tabFile = NULL;
int badRepCnt = 0;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"tabFile", OPTION_STRING},
    {"tabfile", OPTION_STRING},
    {"nosplit", OPTION_BOOLEAN},
    {"noSplit", OPTION_BOOLEAN},
    {"table", OPTION_STRING},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadOut - load RepeatMasker .out files into database\n"
  "usage:\n"
  "   hgLoadOut database file(s).out\n"
  "For each table chrN.out this will create the table\n"
  "chrN_rmsk in the database\n"
  "options:\n"
  "   -tabFile=text.tab - don't actually load database, just create tab file\n"
  "   -nosplit - assume single rmsk table rather than chrN_rmsks\n"
  "   -table=name - use a different suffix other than the default (rmsk)\n");
}

void badFormat(struct lineFile *lf, int id)
/* Print generic bad format message. */
{
errAbort("Badly formatted line %d in %s\n", lf->lineIx, lf->fileName);
}

int makeMilli(char *s, struct lineFile *lf)
/* Convert ascii floating point to parts per thousand representation. */
{
/* Cope with -0.0  and -0.2 etc.*/
if (s[0] == '-')
    {
    if (!sameString(s, "-0.0"))
        warn("Strange perc. field %s line %d of %s", s, lf->lineIx, lf->fileName);
    s = "0.0";
    }
if (!isdigit(s[0]))
    badFormat(lf,1);
return round(10.0*atof(s));
}

int parenSignInt(char *s, struct lineFile *lf)
/* Convert from ascii notation where a parenthesized integer is negative
 * to straight int. */
{
if (s[0] == '(')
    return -atoi(s+1);
else
    return atoi(s);
}

boolean checkRepeat(struct rmskOut *r)
/* check for bogus repeat */
{
/* this is bogus on both strands */
if (r->repStart > r->repEnd)
    {
    badRepCnt++;
    if (verboseLevel() > 1)
        {
        verbose(2, "bad rep range in: ");
        rmskOutTabOut(r, stderr);
        }
    return FALSE;
    }
return TRUE;
}

void loadOneOut(struct sqlConnection *conn, char *rmskFile, char *suffix)
/* Load one RepeatMasker .out file into database. */
{
char dir[256], base[128], extension[64];
char tableName[128];
struct lineFile *lf;
char *line, *words[24];
int lineSize, wordCount;
char *tempName = "out.tab";
FILE *f = NULL;
struct dyString *query = newDyString(1024);

verbose(1, "Processing %s\n", rmskFile);

/* Make temporary tab delimited file. */
if (tabFile != NULL)
    f = tabFile;
else
    f = mustOpen(tempName, "w");

/* Open .out file and process header. */
lf = lineFileOpen(rmskFile, TRUE);
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("Empty %s", lf->fileName);
if (!startsWith("   SW  perc perc", line))
    errAbort("%s doesn't seem to be a RepeatMasker .out file", lf->fileName);
lineFileNext(lf, &line, &lineSize);
lineFileNext(lf, &line, &lineSize);

/* Process line oriented records of .out file. */
while (lineFileNext(lf, &line, &lineSize))
    {
    static struct rmskOut r;
    char *s;

    wordCount = chopLine(line, words);
    if (wordCount < 14)
        errAbort("Expecting 14 or 15 words line %d of %s", 
	    lf->lineIx, lf->fileName);
    r.swScore = atoi(words[0]);
    r.milliDiv = makeMilli(words[1], lf);
    r.milliDel = makeMilli(words[2], lf);
    r.milliIns = makeMilli(words[3], lf);
    r.genoName = words[4];
    r.genoStart = atoi(words[5])-1;
    r.genoEnd = atoi(words[6]);
    r.genoLeft = parenSignInt(words[7], lf);
    r.strand[0]  = (words[8][0] == '+' ? '+' : '-');
    r.repName = words[9];
    r.repClass = words[10];
    s = strchr(r.repClass, '/');
    if (s == NULL)
        r.repFamily = r.repClass;
    else
       {
       *s++ = 0;
       r.repFamily = s;
       }
    r.repStart = parenSignInt(words[11], lf);
    r.repEnd = atoi(words[12]);
    r.repLeft = parenSignInt(words[13], lf);
    r.id[0] = ((wordCount > 14) ? words[14][0] : ' ');
    if (checkRepeat(&r))
        {
        if (!noBin)
            fprintf(f, "%u\t", hFindBin(r.genoStart, r.genoEnd));
        rmskOutTabOut(&r, f);
        }
    }

/* Create database table. */
if (tabFile == NULL)
    {
    carefulClose(&f);
    splitPath(rmskFile, dir, base, extension);
    if (!noSplit)
	{
	chopSuffix(base);
	sprintf(tableName, "%s_%s", base,suffix);
	}
    else
        sprintf(tableName, "%s", suffix);
    verbose(1, "Loading up table %s\n", tableName);
    if (sqlTableExists(conn, tableName))
	{
	dyStringPrintf(query, "DROP table %s", tableName);
	sqlUpdate(conn, query->string);
	}

    /* Create first part of table definitions, the fields. */
    dyStringClear(query);
    dyStringPrintf(query, createRmskOut, tableName);

    /* Create the indexes */
    if (!noSplit)
        {
	dyStringAppend(query, 
	   "INDEX(bin), INDEX(genoStart), INDEX(genoEnd))\n");
	}
    else
        {
	int indexLen = hGetMinIndexLength();
	dyStringPrintf(query, 
	   "INDEX(genoName(%d),bin),\n"
	   "INDEX(genoName(%d),genoStart), \n"
	   "INDEX(genoName(%d),genoEnd))\n"
	   , indexLen, indexLen, indexLen);
	}

    sqlUpdate(conn, query->string);

    /* Load database from tab-file. */
    dyStringClear(query);
    dyStringPrintf(query, 
	"LOAD data local infile '%s' into table %s", tempName, tableName);
    sqlUpdate(conn, query->string);
    remove(tempName);
    }
}


void hgLoadOut(char *database, int rmskCount, char *rmskFileNames[], char *suffix)
/* hgLoadOut - load RepeatMasker .out files into database. */
{
struct sqlConnection *conn = NULL;
int i;

if (tabFile == NULL)
    {
    hSetDb(database);
    conn = hAllocConn();
    verbose(2,"#\thgLoadOut: connected to database: %s\n", database);
    }
for (i=0; i<rmskCount; ++i)
    {
    loadOneOut(conn, rmskFileNames[i], suffix);
    }
sqlDisconnect(&conn);
if (badRepCnt > 0)
    {
    warn("note: %d records dropped due to repStart > repEnd\n", badRepCnt);
    if (verboseLevel() < 2)
        warn("      run with -verbose=2 for details\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
noSplit = (optionExists("noSplit") || optionExists("nosplit"));
suffix = optionVal("table", "rmsk");
tabFileName = optionVal("tabFile", tabFileName);
if (tabFileName == NULL)
    tabFileName = optionVal("tabfile", tabFileName);
if (tabFileName != NULL)
    tabFile = mustOpen(tabFileName, "w");
hgLoadOut(argv[1], argc-2, argv+2, suffix) ;
return 0;
}
