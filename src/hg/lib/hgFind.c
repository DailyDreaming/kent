/* hgFind.c - Find things in human genome annotations. */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "dystring.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "ctgPos.h"
#include "clonePos.h"
#include "bactigPos.h"
#include "genePred.h"
#include "glDbRep.h"
#include "bed.h"
#include "cytoBand.h"
#include "mapSts.h"
#include "fishClones.h"
#include "lfs.h"
#include "snp.h"
#include "rnaGene.h"
#include "stsMarker.h"
#include "stsMap.h"
#include "stsMapMouse.h"
#include "stsMapMouseNew.h"
#include "stsMapRat.h"
#include "knownInfo.h"
#include "cart.h"
#include "hgFind.h"
#include "hdb.h"
#include "refLink.h"
#include "cheapcgi.h"
#include "web.h"
#include <regex.h>

static char const rcsid[] = "$Id: hgFind.c,v 1.83 2003/06/18 16:44:49 sugnet Exp $";

char *MrnaIDforGeneName(char *geneName)
/* return mRNA ID for a gene name */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
char query[256];
char **row;
boolean ok = FALSE;
char *result = NULL;

conn = hAllocConn();
if (hTableExists("refLink"))
    {
    safef(query, sizeof(query), "SELECT mrnaAcc FROM refLink WHERE name='%s'",
          geneName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        result = strdup(row[0]);
        }
    else
        {
        result = NULL;
        }

    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return result;
}

char *MrnaIDforProtein(char *proteinID)
/* return mRNA ID for a protein */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
char * result;

conn = hAllocConn();
query = newDyString(256);

dyStringPrintf(query, "SELECT mrnaID FROM spMrna WHERE spID='%s'", proteinID);
sr = sqlGetResult(conn, query->string);
if ((row = sqlNextRow(sr)) != NULL)
    {
    result = strdup(row[0]);
    }
else
    {
    result = NULL;
    }

freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return result;
}

static boolean findKnownGeneExact(char *spec, struct hgPositions *hgp, char *tableName)
/* Look for position in Known Genes table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
char *chrom;
struct snp snp;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
int rowOffset;
char *localName;

localName = spec;
if (!hTableExists(tableName))
    return FALSE;
rowOffset = hOffsetPastBin(NULL, tableName);
conn = hAllocConn();
query = newDyString(256);
dyStringPrintf(query, 
	       "SELECT chrom, txStart, txEnd, name FROM %s WHERE name='%s'", 
		tableName, localName);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	AllocVar(table);
	dyStringClear(query);
	dyStringPrintf(query, "%s Gene Predictions", tableName);
	table->description = cloneString("Known Genes");
	table->name = cloneString(query->string);
	slAddHead(&hgp->tableList, table);
	}
    AllocVar(pos);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL) 
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findKnownGeneLike(char *spec, struct hgPositions *hgp, char *tableName)
/* Look for position in gene prediction table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
char *chrom;
struct snp snp;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
int rowOffset;
char *localName;

localName = spec;
if (!hTableExists(tableName))
    return FALSE;
rowOffset = hOffsetPastBin(NULL, tableName);
conn = hAllocConn();
query = newDyString(256);
dyStringPrintf(query, "SELECT chrom, txStart, txEnd, name FROM %s WHERE name LIKE '%s'", tableName, localName);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	AllocVar(table);
	dyStringClear(query);
	table->description = cloneString("Known Genes");
	dyStringPrintf(query, "%s", tableName);
	table->name = cloneString(query->string);
	slAddHead(&hgp->tableList, table);
	}

    AllocVar(pos);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}
static boolean findKnownGene(char *spec, struct hgPositions *hgp, char *tableName)
/* Look for position in gene prediction table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
char *chrom;
struct snp snp;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
int rowOffset;
char *localName;
char *mrnaID;

if (! hTableExists("knownGene"))
    return FALSE;

if (findKnownGeneExact(spec, hgp, tableName))
    {    
    return(TRUE);
    }
else
    {
    mrnaID = MrnaIDforProtein(spec);
	
    if (mrnaID != NULL)
	{
	return(findKnownGeneExact(mrnaID, hgp, tableName));
	}
    else
	{
	mrnaID = MrnaIDforGeneName(spec);
	if (mrnaID != NULL)
	    {
	    return(findKnownGeneExact(mrnaID, hgp, tableName));
	    }
	else
	    {
	    return(findKnownGeneLike(spec, hgp, tableName));
	    }
	}
    }
}

static struct hgPositions *handleTwoSites(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart,
	boolean useWeb, char *hgAppName);
/* Function declaration because of circular calls between this and genomePos */
/* Deal with specifications that in form start;end. */


void hgPositionsFree(struct hgPositions **pEl)
/* Free up hgPositions. */
{
struct hgPositions *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->query);
    freeMem(el->extraCgi);
    hgPosTableFreeList(&el->tableList);
    freez(pEl);
    }
}

void hgPositionsFreeList(struct hgPositions **pList)
/* Free a list of dynamically allocated hgPos's */
{
struct hgPositions *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hgPositionsFree(&el);
    }
*pList = NULL;
}


static char *getUiUrl(struct cart *cart)
/* Get rest of UI from browser. */
{
static struct dyString *dy = NULL;
static char *s = NULL;
if (dy == NULL)
    {
    dy = newDyString(64);
    if (cart != NULL)
	dyStringPrintf(dy, "%s=%u", cartSessionVarName(), cartSessionId(cart));
    s = dy->string;
    }
return s;
}


static void singlePos(struct hgPositions *hgp, char *tableDescription, char *posDescription,
                      char *tableName, char *posName, char *chrom, int start, int end)
/* Fill in pos for simple case single position. */
{
struct hgPosTable *table;
struct hgPos *pos;

AllocVar(table);
AllocVar(pos);

slAddHead(&hgp->tableList, table);
table->posList = pos;
table->description = cloneString(tableDescription);
table->name = cloneString(tableName);
pos->chrom = chrom;
pos->chromStart = start;
pos->chromEnd = end;
pos->name = cloneString(posName);
pos->description = cloneString(posDescription);
}

static void fixSinglePos(struct hgPositions *hgp)
/* Fill in posCount and if proper singlePos fields of hgp
 * by going through tables... */
{
int posCount = 0;
struct hgPosTable *table;
struct hgPos *pos;

for (table = hgp->tableList; table != NULL; table = table->next)
    {
    for (pos = table->posList; pos != NULL; pos = pos->next)
        {
	++posCount;
	if (pos->chrom != NULL)
	    hgp->singlePos = pos;
	}
    }
if (posCount != 1)
   hgp->singlePos = NULL;
hgp->posCount = posCount;
}

static boolean isContigName(char *contig)
/* Return TRUE if a FPC contig name. */
{
return(startsWith("ctg", contig) ||
       startsWith("NT_", contig));
}

static boolean isBactigName(char *name)
/* Return TRUE if name matches the regular expression for a 
   Baylor rat assembly bactig name. */
{
char *exp = "^[gkt][a-z]{3}(_[gkt][a-z]{3})?(_[0-9])?$";
regex_t compiledExp;
char *errStr;
int errNum;

if (errNum = regcomp(&compiledExp, exp, REG_NOSUB | REG_EXTENDED | REG_ICASE))
    errAbort("Regular expression compilation error %d", errNum);

return(regexec(&compiledExp, name, 0, NULL, 0) == 0);
}

static boolean isRatContigName(char *contig)
/* Return TRUE if a Baylor rat assembly contig name. */
{
return(startsWith("RNOR", contig) && (strlen(contig) == 12) &&
       isdigit(contig[4]) &&
       isdigit(contig[5]) &&
       isdigit(contig[6]) &&
       isdigit(contig[7]) &&
       isdigit(contig[8]) &&
       isdigit(contig[9]) &&
       isdigit(contig[10]) &&
       isdigit(contig[11]));
}

static boolean isAncientRName(char *name)
/* Return TRUE if name is an ancientRepeat ID. */
{
return startsWith("ar", name);
}



static void findAncientRPos(char *name, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find human/mouse ancient repeat start, end, and chrom
 * from "name".  Don't alter
 * return variables if some sort of error. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
struct bed *bed = NULL;
dyStringPrintf(query, "select * from ancientR where name = '%s'", name);
sr = sqlMustGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find human/mouse ancient repeat: %s", name);
bed = bedLoadN(row+1,12);  /* 1 here since hasBin is TRUE, 12 for extended bed 12*/
*retChromName = hgOfficialChromName(bed->chrom);
*retWinStart = bed->chromStart;
*retWinEnd = bed->chromEnd;
bedFree(&bed);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void findSuperfamily(char *spec, struct hgPositions *hgp)
/* Look up superfamily entry using sfDescription table. */
{
struct sqlConnection *conn  = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr  = NULL;
struct sqlResult *sr2 = NULL;
struct dyString *ds = newDyString(512);
char **row, **row2;
boolean gotOne = FALSE;
struct hgPosTable *table = NULL;
struct hgPos *pos;
struct bed *bed;

char *tname;
char *desc;

AllocVar(table);
slAddHead(&hgp->tableList, table);
table->description = cloneString("Superfamily Associated Search Results");
table->name = cloneString("superfamily");

dyStringClear(ds);
dyStringPrintf(ds, "select name, description from sfDescription where description like'%c%s%c';", '%',spec,'%');
sr2 = sqlGetResult(conn2, ds->string);

while ((row2 = sqlNextRow(sr2)) != NULL)
    {
    dyStringClear(ds);
    dyStringPrintf(ds, "select * from superfamily where name = '%s';", row2[0]);
    sr = sqlGetResult(conn, ds->string);
        
    while ((row = sqlNextRow(sr)) != NULL)
    	{
    	bed = bedLoad3(row+1);
 
    	AllocVar(pos);
    	slAddHead(&table->posList, pos);

    	pos->description = cloneString(row2[1]);
    	pos->name	 = cloneString(row2[0]);
    	
	pos->chrom 	= hgOfficialChromName(bed->chrom);
    	pos->chromStart = bed->chromStart - (bed->chromEnd - bed->chromStart)/100*200;
    	pos->chromEnd   = bed->chromEnd + (bed->chromEnd - bed->chromStart)/100*10;
    	}
    sqlFreeResult(&sr);
    }
sqlFreeResult(&sr2);
freeDyString(&ds);
hFreeConn(&conn);
}

boolean findContigPos(char *contig, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of contig.  Don't alter
 * return variables if some sort of error. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
boolean foundIt;

if (! hTableExists("ctgPos"))
    return FALSE;

dyStringPrintf(query, "select * from ctgPos where contig = '%s'", contig);
sr = sqlMustGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    foundIt = FALSE;
else
    {
    struct ctgPos *ctgPos = ctgPosLoad(row);
    *retChromName = hgOfficialChromName(ctgPos->chrom);
    *retWinStart = ctgPos->chromStart;
    *retWinEnd = ctgPos->chromEnd;
    ctgPosFree(&ctgPos);
    foundIt = TRUE;
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return foundIt;
}

boolean findBactigPos(char *bactig, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of bactig.  Don't alter return variables 
 * unless found. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
boolean foundIt;

if (! hTableExists("bactigPos"))
    return FALSE;

dyStringPrintf(query, "select * from bactigPos where name = '%s'", bactig);
sr = sqlMustGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    foundIt = FALSE;
else
    {
    struct bactigPos *bactigPos = bactigPosLoad(row);
    *retChromName = hgOfficialChromName(bactigPos->chrom);
    *retWinStart = bactigPos->chromStart;
    *retWinEnd = bactigPos->chromEnd;
    bactigPosFree(&bactigPos);
    foundIt = TRUE;
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return foundIt;
}

boolean findRatContigPos(char *name, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of Baylor rat assembly RNOR* contig.  
 * Don't alter return variables unless found. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct slName *allChroms = hAllChromNames();
struct slName *chromPtr;
char **row;
char query[256];
boolean foundIt;

if (! hTableExists("chr1_gold"))
    return FALSE;

foundIt = FALSE;
for (chromPtr=allChroms;  chromPtr != NULL;  chromPtr=chromPtr->next)
    {
    snprintf(query, sizeof(query), "select chromStart,chromEnd from %s_gold where frag = '%s'",
	     chromPtr->name, name);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	*retChromName = chromPtr->name;
	*retWinStart = atoi(row[0]);
	*retWinEnd = atoi(row[1]);
	foundIt = TRUE;
	}
    sqlFreeResult(&sr);
    if (foundIt)
	break;
    }
hFreeConn(&conn);
return foundIt;
}

static char *startsWithShortHumanChromName(char *chrom)
/* Return "cannonical" name of chromosome or NULL
 * if not a chromosome.  This expects no 'chr' in name. */
{
int num;
char buf[64];
char c = chrom[0];

if (c == 'x' || c == 'X' || c == 'Y' || c == 'y')
    {
    sprintf(buf, "chr%c", toupper(c));
    return hgOfficialChromName(buf);
    }
if (!isdigit(chrom[0]))
    return NULL;
num = atoi(chrom);
if (num < 1 || num > 22)
    return NULL;
sprintf(buf, "chr%d", num);
return hgOfficialChromName(buf);
}

static struct cytoBand *loadAllBands()
/* Load up all bands from database. */
{
struct cytoBand *list = NULL, *el;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;

sr = sqlGetResult(conn, "select * from cytoBand");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = cytoBandLoad(row);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
hFreeConn(&conn);
return list;
}

static struct cytoBand *bandList = NULL;

void hgFindChromBand(char *chromosome, char *band, int *retStart, int *retEnd)
/* Return start/end of band in chromosome. */
{
struct cytoBand *chrStart = NULL, *chrEnd = NULL, *cb;
int start = 0, end = 500000000;
boolean anyMatch;
char choppedBand[64], *s, *e;

if (bandList == NULL)
    bandList = loadAllBands();

/* Find first band in chromosome. */
for (cb = bandList; cb != NULL; cb = cb->next)
    {
    if (sameString(cb->chrom, chromosome))
        {
	chrStart = cb;
	break;
	}
    }
if (chrStart == NULL)
    errAbort("Couldn't find chromosome %s in band list", chromosome);

/* Find last band in chromosome. */
for (cb = chrStart->next; cb != NULL; cb = cb->next)
    {
    if (!sameString(cb->chrom, chromosome))
        break;
    }
chrEnd = cb;

if (sameWord(band, "cen"))
    {
    for (cb = chrStart; cb != chrEnd; cb = cb->next)
        {
	if (cb->name[0] == 'p')
	    start = cb->chromEnd - 500000;
	else if (cb->name[0] == 'q')
	    {
	    end = cb->chromStart + 500000;
	    break;
	    }
	}
    *retStart = start;
    *retEnd = end;
    return;
    }
else if (sameWord(band, "qter"))
    {
    *retStart = *retEnd = hChromSize(chromosome);
    *retStart -= 1000000;
    return;
    }
/* Look first for exact match. */
for (cb = chrStart; cb != chrEnd; cb = cb->next)
    {
    if (sameWord(cb->name, band))
        {
	*retStart = cb->chromStart;
	*retEnd = cb->chromEnd;
	return;
	}
    }

/* See if query is less specific.... */
strcpy(choppedBand, band);
for (;;) 
    {
    anyMatch = FALSE;
    for (cb = chrStart; cb != chrEnd; cb = cb->next)
	{
	if (startsWith(choppedBand, cb->name))
	    {
	    if (!anyMatch)
		{
		anyMatch = TRUE;
		start = cb->chromStart;
		}
	    end = cb->chromEnd;
	    }
	}
    if (anyMatch)
	{
	*retStart = start;
	*retEnd = end;
	return;
	}
    s = strrchr(choppedBand, '.');
    if (s == NULL)
	errAbort("Couldn't find anything like band '%s'", band);
    else
	{
	e = choppedBand + strlen(choppedBand) - 1;
	*e = 0;
	if (e[-1] == '.')
	   e[-1] = 0;
        warn("Band %s%s is at higher resolution than data, chopping to %s%s",
	    chromosome+3, band, chromosome+3, choppedBand);
	}
    }
}

boolean hgIsCytoBandName(char *spec, char **retChromName, char **retBandName)
/* Return TRUE if spec is a cytological band name including chromosome short name.  
 * Returns chromosome chrN name and band (with chromosome stripped off). */
{
char *fullChromName, *shortChromName;
int len;
int dotCount = 0;
char *s, c;

/* First make sure spec is in format to be a band name. */
if ((fullChromName = startsWithShortHumanChromName(spec)) == NULL)
    return FALSE;
shortChromName = skipChr(fullChromName);
len = strlen(shortChromName);
spec += len;
c = spec[0];
if (c != 'p' && c != 'q')
    return FALSE;
if (!isdigit(spec[1]))
    return FALSE;

/* Make sure rest is digits with maybe one '.' */
s = spec+2;
while ((c = *s++) != 0)
    {
    if (c == '.')
        ++dotCount;
    else if (!isdigit(c))
        return FALSE;
    }
if (dotCount > 1)
    return FALSE;
*retChromName = fullChromName;
*retBandName = spec;
return TRUE;
}

boolean hgFindCytoBand(char *spec, char **retChromName, int *retWinStart, int *retWinEnd)
/* Return position associated with cytological band if spec looks to be in that form. */
{
char *bandName;

if (!hgIsCytoBandName(spec, retChromName, &bandName))
     return FALSE;
hgFindChromBand(*retChromName, bandName, retWinStart, retWinEnd);
return TRUE;
}

static boolean isAccForm(char *s)
/* Returns TRUE if s is of format to be a genbank accession. */
{
int len = strlen(s);
if (len < 6 || len > 10)
    return FALSE;
if (!isalpha(s[0]))
    return FALSE;
if (!isdigit(s[len-1]))
    return FALSE;
return TRUE;
}

boolean hgFindClonePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Return clone position. */
{
if (!isAccForm(spec) || !hTableExists("clonePos"))
    return FALSE;
else
    {
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr = NULL;
    struct dyString *query = newDyString(256);
    char **row;
    boolean ok = FALSE;
    struct clonePos *clonePos;
    dyStringPrintf(query, "select * from clonePos where name like '%s%%'", spec);
    sr = sqlGetResult(conn, query->string);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	clonePos = clonePosLoad(row);
	*retChromName = hgOfficialChromName(clonePos->chrom);
	*retWinStart = clonePos->chromStart;
	*retWinEnd = clonePos->chromEnd;
	clonePosFree(&clonePos);
	ok = TRUE;
	}
    freeDyString(&query);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    return ok;
    }
}

static boolean isRefSeqAcc(char *acc)
/* determine if an acc looking like a refseq acc */
{
char a0 = toupper(acc[0]);
return ((strlen(acc) > 3) && (acc[2] == '_') && ((a0 == 'N') || (a0 == 'X')));
}

static char *mrnaType(char *acc)
/* Return "mrna" or "est" if acc is mRNA, otherwise NULL.  Returns
 * NULL for refseq mRNAs */
{
/* for compat with older databases, just look at the seqId to
 * determine if it's a refseq, don't use table */
if (isRefSeqAcc(acc))
    return NULL;
else
    {
    static char typeBuf[16];
    char *type;
    struct sqlConnection *conn = hAllocConn();
    char query[256];

    safef(query, sizeof(query), "select type from mrna where acc = '%s'", acc);
    type = sqlQuickQuery(conn, query, typeBuf, sizeof(typeBuf));
    hFreeConn(&conn);
    return type;
    }
}

static struct psl *findAllAli(char *acc, char *type)
/* Find all alignments of the given type. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct psl *pslList = NULL, *psl;
struct sqlResult *sr;
char **row;
int rowOffset;
char table[64];

if (type[0] == 0)
   strncpy(table, "xenoMrna", sizeof(table));
else
    snprintf(table, sizeof(table), "all_%s", type);
rowOffset = hOffsetPastBin(NULL, table);
snprintf(query, sizeof(query), "select * from %s where qName = '%s'", table, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    slAddHead(&pslList, psl);
    }
hFreeConn(&conn);
slReverse(&pslList);
return pslList;
}

static int pslMrnaScore(const struct psl *psl)
/* Return a simple score for psl. */
{
return psl->match + (psl->repMatch>>1) - psl->misMatch - psl->qNumInsert;
}

static int pslCmpScore(const void *va, const void *vb)
/* Compare two psl to sort by score position . */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
return pslMrnaScore(b) - pslMrnaScore(a);
}

static void mrnaHtmlStart(struct hgPosTable *table, FILE *f)
/* Print preamble to mrna alignment positions. */
{
fprintf(f, "<H2>%s</H2>", table->description);
fprintf(f, "This aligns in multiple positions.  Click on a hyperlink to ");
fprintf(f, "go to tracks display at a particular alignment.<BR>");

fprintf(f, "<TT><PRE>");
fprintf(f, " SIZE IDENTITY CHROMOSOME STRAND  START     END       cDNA   START  END  TOTAL\n");
fprintf(f, "------------------------------------------------------------------------------\n");
}

static void mrnaHtmlEnd(struct hgPosTable *table, FILE *f)
/* Print end to mrna alignment positions. */
{
fprintf(f, "</TT></PRE>");
}

static void mrnaHtmlOnePos(struct hgPosTable *table, struct hgPos *pos, FILE *f)
/* Print one mrna alignment position. */
{
fprintf(f, "%s", pos->description);
}

static boolean findMrnaPos(char *acc,  struct hgPositions *hgp,
			   char *hgAppName, struct cart *cart)
/* Look to see if it's an mRNA.  Fill in hgp and return
 * TRUE if it is, otherwise return FALSE. */
{
char *type;
char *extraCgi = hgp->extraCgi;
char *ui = getUiUrl(cart);
char tableName [64];
if ((type = mrnaType(acc)) == NULL || type[0] == 0)
    return FALSE;
else
    {
    struct psl *pslList, *psl;
    int pslCount;
    char suffix[16];
    struct hgPosTable *table;
    struct hgPos *pos;

    strncpy(suffix, type, sizeof(suffix));
    tolowers(suffix);
    pslList = psl = findAllAli(acc, suffix);
    pslCount = slCount(pslList);
    if (pslCount <= 0)
        {
	errAbort("%s %s doesn't align anywhere in the genome",
		 type, acc);
	return FALSE;
	}
    else
        {
	struct dyString *dy = newDyString(1024);
	
        if (NULL == type)
            {
            strncpy(tableName, "xenoMrna", sizeof(tableName));
            }
        else
            {
            snprintf(tableName, sizeof(tableName), "%s", suffix);      
            }

	AllocVar(table);
	table->htmlStart = mrnaHtmlStart;
	table->htmlEnd = mrnaHtmlEnd;
	table->htmlOnePos = mrnaHtmlOnePos;
	slAddHead(&hgp->tableList, table);
	dyStringPrintf(dy, "%s Alignments", acc);
	table->description = cloneString(dy->string);
        table->name = cloneString(tableName);
	slSort(&pslList, pslCmpScore);
	for (psl = pslList; psl != NULL; psl = psl->next)
	    {
	    dyStringClear(dy);
	    AllocVar(pos);
	    pos->chrom = hgOfficialChromName(psl->tName);
	    pos->chromStart = psl->tStart;
	    pos->chromEnd = psl->tEnd;
	    pos->name = cloneString(psl->qName);
	    dyStringPrintf(dy, "<A HREF=\"%s?position=%s",
	        hgAppName, hgPosBrowserRange(pos, NULL) );
	    if (ui != NULL)
	        dyStringPrintf(dy, "&%s", ui);
	    dyStringPrintf(dy, "%s\">",
		extraCgi);
	    dyStringPrintf(dy, "%5d  %5.1f%%  %9s     %s %9d %9d  %8s %5d %5d %5d</A>",
		psl->match + psl->misMatch + psl->repMatch + psl->nCount,
		100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
		skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd,
		psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
	    dyStringPrintf(dy, "\n");
	    pos->description = cloneString(dy->string);
	    slAddHead(&table->posList, pos);
	    }
	slReverse(&table->posList);
	freeDyString(&dy);
	return TRUE;
	}
    }
}

static boolean findStsPos(char *spec, struct hgPositions *hgp)
/* Look for position in stsMarker/stsMap table. */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row;
boolean ok = FALSE;
char *alias = NULL, *temp;
struct stsMap sm;
struct stsMapMouse smm;
struct stsMapMouseNew smm_n;
struct stsMapRat smr;
char *tableName, *tableAlias;
boolean newFormat = FALSE, mouse = FALSE, rat = FALSE, mouse_n=FALSE;
char *chrom;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

 if (hTableExists("stsMapMouse"))
   {
     mouse = TRUE;
     tableName = "stsMapMouse";
     tableAlias = "stsAliasMouse";
   }
 else if (hTableExists("stsMapMouseNew"))
   {
    mouse_n = TRUE;
    tableName = "stsMapMouseNew";
    tableAlias = "stsAlias";
   }
 else if (hTableExists("stsMapRat"))
   {
     rat = TRUE;
    tableName = "stsMapRat";
    tableAlias = "stsAlias";
   }
 else if (hTableExists("stsMap"))
   {
     newFormat = TRUE;
     tableName = "stsMap";
     tableAlias = "stsAlias";
   }
 else if (hTableExists("stsMarker"))
   {
    newFormat = FALSE;
    tableName = "stsMarker";
    tableAlias = "stsAlias";
   }
 else
   return FALSE;
 
 conn = hAllocConn();
 query = newDyString(256);
 if (hTableExists(tableAlias))
    {
      dyStringPrintf(query, 
		     "select trueName from %s where alias = '%s'", tableAlias, spec);
      alias = sqlQuickQuery(conn, query->string, buf, sizeof(buf));
      if ((alias != NULL) && (!sameString(alias, spec)))
        {
	hgp->useAlias = TRUE;
	temp = spec;
	spec = alias;
	alias = temp;
	}
    }
dyStringClear(query);
dyStringPrintf(query, "select * from %s where name = '%s'", tableName, spec);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	AllocVar(table);
	dyStringClear(query);
	if (hgp->useAlias)
	    dyStringPrintf(query, "STS %s uses name %s in browser", alias, spec);
	else
	    dyStringPrintf(query, "STS %s Positions", spec);
	table->description = cloneString(query->string);
	table->name = cloneString(tableName);
	slAddHead(&hgp->tableList, table);
	}
    if (mouse)
      stsMapMouseStaticLoad(row, &smm);
    else if (mouse_n)
     stsMapMouseNewStaticLoad(row, &smm_n);
    else if (rat)
	stsMapRatStaticLoad(row, &smr);
    else if (newFormat)
	stsMapStaticLoad(row, &sm);
    else
        {
	struct stsMarker oldSm;
	stsMarkerStaticLoad(row, &oldSm);
	stsMapFromStsMarker(&oldSm, &sm);
	}
    if (mouse) 
	{
	if ((chrom = hgOfficialChromName(smm.chrom)) == NULL)
	errAbort("Internal Database error: Odd chromosome name '%s' in %s",
		 smm.chrom, tableName);
	}
    else if (mouse_n) 
	{
	if ((chrom = hgOfficialChromName(smm_n.chrom)) == NULL)
	errAbort("Internal Database error: Odd chromosome name '%s' in %s",
		 smm_n.chrom, tableName);
	}
    else if (rat) 
	{
	if ((chrom = hgOfficialChromName(smr.chrom)) == NULL)
	errAbort("Internal Database error: Odd chromosome name '%s' in %s",
		 smr.chrom, tableName);
	}
    else 
	{
	if ((chrom = hgOfficialChromName(sm.chrom)) == NULL)
	    errAbort("Internal Database error: Odd chromosome name '%s' in %s",
		     sm.chrom, tableName); 
	}
    AllocVar(pos);
    pos->chrom = chrom;
    if (mouse) 
	{
	pos->chromStart = smm.chromStart - 100000;
	pos->chromEnd = smm.chromEnd + 100000;
	}
    else if (mouse_n) 
	{
	pos->chromStart = smm_n.chromStart - 100000;
	pos->chromEnd = smm_n.chromEnd + 100000;
	}
    else if (rat) 
	{
	pos->chromStart = smr.chromStart - 100000;
	pos->chromEnd = smr.chromEnd + 100000;
	}
    else 
	{
	pos->chromStart = sm.chromStart - 100000;
	pos->chromEnd = sm.chromEnd + 100000;
	}
    pos->name = cloneString(spec);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findFishClones(char *spec, struct hgPositions *hgp)
/* Look for position in fishClones table. */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row;
boolean ok = FALSE;
struct fishClones *fc;
char *chrom;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (hTableExists("fishClones"))
    {
    conn = hAllocConn();
    query = newDyString(256);
    dyStringPrintf(query, "select * from fishClones where name = '%s'", spec);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (ok == FALSE)
            {
	    ok = TRUE;
	    AllocVar(table);
	    dyStringClear(query);
	    slAddHead(&hgp->tableList, table);
	    }
	AllocVar(fc);
	fc = fishClonesLoad(row);
	if ((chrom = hgOfficialChromName(fc->chrom)) == NULL)
	     errAbort("Internal Database error: Odd chromosome name '%s' in fishClones",
		      fc->chrom); 
	AllocVar(pos);
	pos->chrom = chrom;
	pos->chromStart = fc->chromStart;
	pos->chromEnd = fc->chromEnd;
	pos->name = cloneString(spec);
	dyStringPrintf(query, "%s Positions in FISH Clones track", spec);
	table->description = cloneString(query->string);
	table->name = cloneString("fishClones");
	slAddHead(&table->posList, pos);
	fishClonesFree(&fc);
	}
    if (table != NULL)
        slReverse(&table->posList);
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findBacEndPairs(char *spec, struct hgPositions *hgp)
/* Look for position in bacEndPairs table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row;
boolean ok = FALSE;
struct lfs *be;
char *chrom;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (hTableExists("bacEndPairs"))
    {
    conn = hAllocConn();
    query = newDyString(256);
    dyStringPrintf(query, "select * from bacEndPairs where name = '%s'", spec);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (ok == FALSE)
	    {
	    ok = TRUE;
	    AllocVar(table);
	    dyStringClear(query);
	    slAddHead(&hgp->tableList, table);
	    }
	AllocVar(be);
	be = lfsLoad(row+1);
	if ((chrom = hgOfficialChromName(be->chrom)) == NULL)
	    errAbort("Internal Database error: Odd chromosome name '%s' in bacEndPairs",
		     be->chrom); 
	AllocVar(pos);
	pos->chrom = chrom;
	pos->chromStart = be->chromStart;
	pos->chromEnd = be->chromEnd;
	pos->name = cloneString(spec);
	dyStringPrintf(query, "%s Positions found using BAC end sequences", spec);
	table->description = cloneString(query->string);
	table->name = cloneString("bacEndPairs");
	slAddHead(&table->posList, pos);
	lfsFree(&be);
	}
    if (table != NULL)
        slReverse(&table->posList);
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findFosEndPairs(char *spec, struct hgPositions *hgp)
/* Look for position in fosEndPairs table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row;
boolean ok = FALSE;
struct lfs *fe;
char *chrom;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (hTableExists("fosEndPairs"))
    {
    conn = hAllocConn();
    query = newDyString(256);
    dyStringPrintf(query, "select * from fosEndPairs where name = '%s'", spec);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (ok == FALSE)
	    {
	    ok = TRUE;
	    AllocVar(table);
	    dyStringClear(query);
	    slAddHead(&hgp->tableList, table);
	    }
	AllocVar(fe);
	fe = lfsLoad(row+1);
	if ((chrom = hgOfficialChromName(fe->chrom)) == NULL)
	    errAbort("Internal Database error: Odd chromosome name '%s' in fosEndPairs",
		     fe->chrom); 
	AllocVar(pos);
	pos->chrom = chrom;
	pos->chromStart = fe->chromStart;
	pos->chromEnd = fe->chromEnd;
	pos->name = cloneString(spec);
	dyStringPrintf(query, "%s Positions found using fosmid end sequences", spec);
	table->description = cloneString(query->string);
	table->name = cloneString("fosEndPairs");
	slAddHead(&table->posList, pos);
	lfsFree(&fe);
	}
    if (table != NULL)
        slReverse(&table->posList);
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findFosEndPairsBad(char *spec, struct hgPositions *hgp)
/* Look for position in fosEndPairsBad table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row;
boolean ok = FALSE;
struct lfs *fe;
char *chrom;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (hTableExists("fosEndPairsBad"))
    {
    conn = hAllocConn();
    query = newDyString(256);
    dyStringPrintf(query, "select * from fosEndPairsBad where name = '%s'", spec);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (ok == FALSE)
	    {
	    ok = TRUE;
	    AllocVar(table);
	    dyStringClear(query);
	    slAddHead(&hgp->tableList, table);
	    }
	AllocVar(fe);
	fe = lfsLoad(row+1);
	if ((chrom = hgOfficialChromName(fe->chrom)) == NULL)
	    errAbort("Internal Database error: Odd chromosome name '%s' in fosEndPairsBad",
		     fe->chrom); 
	AllocVar(pos);
	pos->chrom = chrom;
	pos->chromStart = fe->chromStart;
	pos->chromEnd = fe->chromEnd;
	pos->name = cloneString(spec);
	dyStringPrintf(query, "%s Positions found using fosmid end sequences", spec);
	table->description = cloneString(query->string);
	table->name = cloneString("fosEndPairsBad");
	slAddHead(&table->posList, pos);
	lfsFree(&fe);
	}
    if (table != NULL)
        slReverse(&table->posList);
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findGenePred(char *spec, struct hgPositions *hgp, char *tableName)
/* Look for position in gene prediction table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
char *chrom;
struct snp snp;
char buf[64];
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
int rowOffset;
char *localName;

localName = spec;
if (!hTableExists(tableName))
    return FALSE;
rowOffset = hOffsetPastBin(NULL, tableName);
conn = hAllocConn();
query = newDyString(256);
dyStringPrintf(query, "SELECT chrom, txStart, txEnd, name FROM %s WHERE name LIKE '%s'", tableName, localName);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
		ok = TRUE;
		AllocVar(table);
		dyStringClear(query);
		dyStringPrintf(query, "%s Gene Predictions", tableName);
		table->description = cloneString(query->string);
		table->name = cloneString(tableName);
		slAddHead(&hgp->tableList, table);
		}

    AllocVar(pos);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}


static boolean findSnpPos(char *spec, struct hgPositions *hgp, char *tableName)
/* Look for position in stsMarker table. */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
struct dyString *query = NULL;
char **row = NULL;
boolean ok = FALSE;
char *chrom = NULL;
struct snp snp;
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
int rowOffset = 0;

/* Make sure it starts with 'rs'.  Then skip over it. */
if (!startsWith("rs", spec))
    return FALSE;
if (!hTableExists(tableName))
    return FALSE;
rowOffset = hOffsetPastBin(NULL, tableName);
conn = hAllocConn();
query = newDyString(256);
dyStringPrintf(query, "select * from %s where name = '%s'", tableName, spec);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	AllocVar(table);
	dyStringClear(query);
	dyStringPrintf(query, "SNP %s Position", spec);
	table->description = cloneString(query->string);
	table->name = cloneString(tableName);
	slAddHead(&hgp->tableList, table);
	}
    snpStaticLoad(row+rowOffset, &snp);
    if ((chrom = hgOfficialChromName(snp.chrom)) == NULL)
	errAbort("Internal Database error: Odd chromosome name '%s' in %s",
		 snp.chrom, tableName); 
    AllocVar(pos);
    pos->chrom = chrom;
    pos->chromStart = snp.chromStart - 5000;
    pos->chromEnd = snp.chromEnd + 5000;
    pos->name = cloneString(spec);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findOldStsPos(char *table, char *spec,
                             char **retChromName, int *retWinStart, 
                             int *retWinEnd)
/* Look for position in some STS table. */
/* This code is being replaced by the newer sts pos finder,
 * but is still used on the hg3 (July 17 2000 Freeze) database. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
boolean ok = FALSE;
struct mapSts *mapSts;

if (!hTableExists(table))
    return FALSE;
dyStringPrintf(query, "select * from %s where name = '%s'", table, spec);
sr = sqlGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row != NULL)
    {
    mapSts = mapStsLoad(row);
    *retChromName = hgOfficialChromName(mapSts->chrom);
    *retWinStart = mapSts->chromStart - 100000;
    *retWinEnd = mapSts->chromEnd + 100000;
    mapStsFree(&mapSts);
    ok = TRUE;
    }
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findGenethonPos(char *spec, char **retChromName, int *retWinStart, int *retWinEnd)
/* See if it's a genethon map position. */
{
return findOldStsPos("mapGenethon", spec, retChromName, retWinStart, retWinEnd);
}

#ifdef OLD
static struct slName *accsThatMatchKey(char *key, char *field)
/* Return list of accessions that match key on a particular field. */
{
struct slName *list = NULL, *el;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

sprintf(query, "select mrna.acc from mrna,%s where %s.name like '%%%s%%'  and mrna.%s = %s.id", field, field, key, field, field);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = newSlName(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
return list;
}
#endif /* OLD */

static void findHitsToTables(char *key, char *tables[], int tableCount, 
    struct hash **retHash, struct slName **retList)
/* Return all unique accessions that match any table. */
{
struct slName *list = NULL, *el;
struct hash *hash = newHash(0);
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
char *field;
int i;

for (i = 0; i<tableCount; ++i)
    {
    struct slName *idList = NULL, *idEl;
    
    /* I'm doing this query in two steps in C rather than
     * in one step in SQL just because it somehow is much
     * faster this way (like 100x faster) when using mySQL. */
    field = tables[i];
    sprintf(query, "select id from %s where name like '%%%s%%'", 
       field, key);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	idEl = newSlName(row[0]);
	slAddHead(&idList, idEl);
	}
    sqlFreeResult(&sr);
    for (idEl = idList; idEl != NULL; idEl = idEl->next)
        {
        /* don't check srcDb to exclude refseq for compat with older tables */
	sprintf(query, "select acc from mrna where %s = %s and type = 'mRNA'", field, idEl->name);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    char *acc = row[0];
	    if (!isRefSeqAcc(acc) && !hashLookup(hash, acc))
		{
		el = newSlName(acc);
		slAddHead(&list, el);
		hashAdd(hash, acc, NULL);
		}
	    }
	sqlFreeResult(&sr);
        }
    slFreeList(&idList);
    }
hFreeConn(&conn);
slReverse(&list);
*retList = list;
*retHash = hash;
}


static void andHits(struct hash *aHash, struct slName *aList, 
	struct hash *bHash, struct slName *bList,
	struct hash **retHash, struct slName **retList)
/* Return hash/list that is intersection of lists a and b. */
{
struct slName *list = NULL, *el, *newEl;
struct hash *hash = newHash(0);
for (el = aList; el != NULL; el = el->next)
    {
    char *name = el->name;
    if (hashLookup(bHash, name) && !hashLookup(hash, name))
        {
	newEl = newSlName(name);
	slAddHead(&list, newEl);
	hashAdd(hash, name, NULL);
	}
    }
*retHash = hash;
*retList = list;
}

static void mrnaKeysHtmlOnePos(struct hgPosTable *table, struct hgPos *pos, FILE *f)
{
fprintf(f, "%s", pos->description);
}

static boolean mrnaAligns(struct sqlConnection *conn, char *acc)
/* Return TRUE accession is in one of our mRNA 
 * alignment tables. */
{
static char *estTables[] = { "all_est", "xenoEst", NULL};
static char *mrnaTables[] = { "all_mrna", "xenoMrna", NULL};
char **tables, *table;
char query[256], buf[64], *type;
safef(query, sizeof(query), "select type from mrna where acc = '%s'", acc);
type = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (type == NULL)
    internalErr();
if (sameWord(type, "EST"))
    tables = estTables;
else
    tables = mrnaTables;
while ((table = *tables++) != NULL)
    {
    if (sqlTableExists(conn, table))
	{
	safef(query, sizeof(query), 
	    "select qName from %s where qName = '%s'", table, acc);
	if (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL)
	    return TRUE;
	}
    }
return FALSE;
}

static void findMrnaKeys(char *keys, struct hgPositions *hgp,
			 char *hgAppName, struct cart *cart)
/* Find mRNA that has keyword in one of it's fields. */
{
char *words[32];
char buf[512];
int wordCount;
struct slName *el;
static char *tables[] = {
	"productName", "geneName",
	"author", "tissue", "cell", "description", "development", 
	};
struct hash *oneKeyHash = NULL;
struct slName *oneKeyList = NULL;
struct hash *allKeysHash = NULL;
struct slName *allKeysList = NULL;
struct hash *andedHash = NULL;
struct slName *andedList = NULL;
int i;
struct dyString *dy = NULL;
struct hgPosTable *table;
struct hgPos *pos;


strncpy(buf, keys, sizeof(buf));
wordCount = chopLine(buf, words);
if (wordCount == 0)
    return;
for (i=0; i<wordCount; ++i)
    {
    findHitsToTables(words[i], tables, ArraySize(tables), &oneKeyHash, &oneKeyList);
    if (allKeysHash == NULL)
        {
	allKeysHash = oneKeyHash;
	oneKeyHash = NULL;
	allKeysList = oneKeyList;
	oneKeyList = NULL;
	}
    else
        {
	andHits(oneKeyHash, oneKeyList, allKeysHash, allKeysList, &andedHash, &andedList);
	freeHash(&oneKeyHash);
	slFreeList(&oneKeyList);
	freeHash(&allKeysHash);
	slFreeList(&allKeysList);
	allKeysHash = andedHash;
	andedHash = NULL;
	allKeysList = andedList;
	andedList = NULL;
	}
    }
if (allKeysList == NULL)
    return;


dy = newDyString(256);
AllocVar(table);
slAddHead(&hgp->tableList, table);
table->name = NULL;
table->description = cloneString("mRNA Associated Search Results");
table->htmlOnePos = mrnaKeysHtmlOnePos;

/* Dummy block made to allow local declarations */
/* Fetch descriptions of all matchers and display. */
    {
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    char description[512];
    char *ui = getUiUrl(cart);
    for (el = allKeysList; el != NULL; el = el->next)
        {
	if (mrnaAligns(conn, el->name))
	    {
	    AllocVar(pos);
	    slAddHead(&table->posList, pos);
	    pos->name = cloneString(el->name);
	    dyStringClear(dy);
	    
	    dyStringPrintf(dy, "<A HREF=\"%s?position=%s", hgAppName, el->name);
	    
	    if (ui != NULL)
		dyStringPrintf(dy, "&%s", ui);
	    dyStringPrintf(dy, "%s\">", 
			   hgp->extraCgi);
	    dyStringPrintf(dy, "%s </A>", el->name);
	    sprintf(query, 
		    "select description.name from mrna,description"
		    " where mrna.acc = '%s' and mrna.description = description.id",
		    el->name);
	    if (sqlQuickQuery(conn, query, description, sizeof(description)))
		dyStringPrintf(dy, "- %s", description);
	    dyStringPrintf(dy, "\n");
	    pos->description = cloneString(dy->string);
	    }
        }
    slReverse(&table->posList);
    hFreeConn(&conn);
    }

    freeDyString(&dy);
}

static void findKnownGenes(char *spec, struct hgPositions *hgp)
/* Look up known genes in table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
boolean gotOne = FALSE;
struct hgPosTable *table = NULL;
struct hgPos *pos;
struct genePred *gp;
struct knownInfo *knownInfo;
char *kiTable = NULL;

if (sqlTableExists(conn, "knownMore"))
    kiTable = "knownMore";
else if (sqlTableExists(conn, "knownInfo"))
    kiTable = "knownInfo";
if (kiTable != NULL)
    {
    dyStringPrintf(query, "select * from %s where name like '%s%%'", kiTable, spec);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (!gotOne)
	    {
	    gotOne = TRUE;
	    AllocVar(table);
	    slAddHead(&hgp->tableList, table);
	    table->description = cloneString("RefSeq Genes");
	    table->name = cloneString(kiTable);
	    }
	knownInfo = knownInfoLoad(row);
	AllocVar(pos);
	slAddHead(&table->posList, pos);
	pos->name = cloneString(knownInfo->name);
	pos->description = cloneString(knownInfo->transId);
	}
    sqlFreeResult(&sr);

    if (table != NULL)
	{
	slReverse(&table->posList);
	for (pos = table->posList; pos != NULL; pos = pos->next)
	    {
	    dyStringClear(query);
	    dyStringPrintf(query, "select * from genieKnown where name = '%s'", pos->description);
	    sr = sqlGetResult(conn, query->string);
	    if ((row = sqlNextRow(sr)) == NULL)
		errAbort("Internal error: %s in knownInfo but not genieKnown",
			 pos->description);
	    gp = genePredLoad(row);
	    pos->chrom = hgOfficialChromName(gp->chrom);
	    pos->chromStart = gp->txStart;
	    pos->chromEnd = gp->txEnd;
	    freez(&pos->description);
	    genePredFree(&gp);
	    sqlFreeResult(&sr);
	    table->description = cloneString("RefSeq Genes");
	    table->name = cloneString("genieKnown");
	    }
	}
    }
freeDyString(&query);
hFreeConn(&conn);
}

static void addRefLinks(struct sqlConnection *conn, struct dyString *query,
	struct refLink **pList)
/* Query database and add returned refLinks to head of list. */
{
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct refLink *rl = refLinkLoad(row);
    slAddHead(pList, rl);
    }
sqlFreeResult(&sr);
}

static boolean isUnsignedInt(char *s)
/* Return TRUE if s is in format to be an unsigned int. */
{
int size=0;
char c;
while ((c = *s++) != 0)
    {
    if (++size > 10 || !isdigit(c))
        return FALSE;
    }
return TRUE;
}

static void findRefGenes(char *spec, struct hgPositions *hgp)
/* Look up refSeq genes in table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *ds = newDyString(256);
char **row;
boolean gotOne = FALSE;
struct hgPosTable *table = NULL;
struct hgPos *pos;
struct genePred *gp;
struct refLink *rlList = NULL, *rl;
boolean gotRefLink = sqlTableExists(conn, "refLink");

if (gotRefLink)
    {
    if (startsWith("NM_", spec) || startsWith("XM_", spec))
	{
	dyStringPrintf(ds, "select * from refLink where mrnaAcc = '%s'", spec);
	addRefLinks(conn, ds, &rlList);
	}
    else if (startsWith("NP_", spec) || startsWith("XP_", spec))
        {
	dyStringPrintf(ds, "select * from refLink where protAcc = '%s'", spec);
	addRefLinks(conn, ds, &rlList);
	}
    else if (isUnsignedInt(spec))
        {
	dyStringPrintf(ds, "select * from refLink where locusLinkId = %s", spec);
	addRefLinks(conn, ds, &rlList);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from refLink where omimId = %s", spec);
	addRefLinks(conn, ds, &rlList);
	}
    else 
	{
	dyStringPrintf(ds, "select * from refLink where name like '%%%s%%'", spec);
	addRefLinks(conn, ds, &rlList);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from refLink where product like '%%%s%%'", spec);
	addRefLinks(conn, ds, &rlList);
	}
    }
if (rlList != NULL)
    {
    struct hash *hash = newHash(8);
    AllocVar(table);
    slAddHead(&hgp->tableList, table);
    table->description = cloneString("RefSeq Genes");
    table->name = cloneString("refGene");
    for (rl = rlList; rl != NULL; rl = rl->next)
        {
        /* Don't return duplicate mrna accessions */
        if (hashFindVal(hash, rl->mrnaAcc))
            {            
            hashAdd(hash, rl->mrnaAcc, rl);
            continue;
            }

        hashAdd(hash, rl->mrnaAcc, rl);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from refGene where name = '%s'", rl->mrnaAcc);
	sr = sqlGetResult(conn, ds->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    gp = genePredLoad(row);
	    AllocVar(pos);
	    slAddHead(&table->posList, pos);
	    pos->name = cloneString(rl->name);
	    dyStringClear(ds);
	    dyStringPrintf(ds, "(%s) %s", rl->mrnaAcc, rl->product);
	    pos->description = cloneString(ds->string);
	    pos->chrom = hgOfficialChromName(gp->chrom);
	    pos->chromStart = gp->txStart;
	    pos->chromEnd = gp->txEnd;
	    genePredFree(&gp);
	    }
	sqlFreeResult(&sr);
	}
    refLinkFreeList(&rlList);
    freeHash(&hash);
    }
freeDyString(&ds);
hFreeConn(&conn);
}

static void findZooGenes(char *spec, struct hgPositions *hgp)
/* Look up zoo gene names in manual annotation table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *ds = newDyString(256);
char **row = NULL;
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

if (sqlTableExists(conn, "pjt_gene"))
    {
    dyStringPrintf(ds, "select * from pjt_gene where name like '%%%s%%'", spec);
    }
else
    {
    return;
    }

AllocVar(table);
slAddHead(&hgp->tableList, table);
table->description = cloneString("Curated Genes");
table->name = cloneString("pjt_gene");

sr = sqlGetResult(conn, ds->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(pos);
    slAddHead(&table->posList, pos);
    pos->name = cloneString(row[3]);
    pos->description = cloneString(row[3]);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = sqlUnsigned(row[6]);
    pos->chromEnd = sqlUnsigned(row[7]);
    }

freeDyString(&ds);
hFreeConn(&conn);
}

static void findRgdGenes(char *spec, struct hgPositions *hgp)
/* Look up zoo gene names in manual annotation table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *ds = newDyString(256);
char **row = NULL;
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;

AllocVar(table);
slAddHead(&hgp->tableList, table);
/* Default values */
table->description = cloneString("RGD Curated Genes");
table->name = cloneString("rgdGene");

if (sqlTableExists(conn, "rgdGene"))
    {
    dyStringPrintf(ds, "select * from rgdGene where name = '%s'", spec);    
    sr = sqlGetResult(conn, ds->string);
    
    while ((row = sqlNextRow(sr)) != NULL)
        {
        AllocVar(pos);
        slAddHead(&table->posList, pos);
        pos->name = cloneString(row[0]);
        pos->description = cloneString(row[0]);
        pos->chrom = hgOfficialChromName(row[1]);
        pos->chromStart = sqlUnsigned(row[5]);
        pos->chromEnd = sqlUnsigned(row[6]);
        }
    }

freeDyString(&ds);
hFreeConn(&conn);
}

static boolean isAffyProbeName(char *name)
/* Return TRUE if name is an Affymetrix Probe ID for HG-U95Av2. */
{
return startsWith("HG-U95Av2:", name);
}

static boolean isAffyU133ProbeName(char *name)
/* Return TRUE if name is an Affymetrix Probe ID for HG-U95Av2. */
{
return startsWith("HG-U133:", name);
}

static void findBedProbePos(char *table, char *name, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find bed start, end, and chrom from "name" from table.  Don't alter
 * return variables if some sort of error. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
struct bed *bed = NULL;
dyStringPrintf(query, "select chrom, chromStart, chromEnd, name from %s where name = '%s'", table, name);
if(!hTableExists(table))
    errAbort("Sorry %s track not available yet in this version of the browser.", table);
sr = sqlGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find record with name: %s in table: %s", name, table);
bed = bedLoadN(row,4);
*retChromName = hgOfficialChromName(bed->chrom);
*retWinStart = bed->chromStart;
*retWinEnd = bed->chromEnd;
bedFree(&bed);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void findAffyProbePos(char *name, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find affy probe start, end, and chrom
 * from "name".  Don't alter
 * return variables if some sort of error. */
{
char *temp = strstr(name, ":"); /* parse name out of something like "HG-U95Av2:probeName" */
assert(temp);
temp++;
findBedProbePos("affyRatio", temp, retChromName, retWinStart, retWinEnd);
}

static struct hgPositions *genomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart, boolean showAlias,
	boolean useWeb, char *hgAppName)
/* Search for positions in genome that match user query.   
 * Return TRUE if the query results in a unique position.  
 * Otherwise display list of positions and return FALSE. */
{ 
struct hgPositions *hgp;
struct hgPos *pos;
char *searchSpec = NULL;

/* Make sure to escape single quotes for DB parseability */
if (strchr(spec, '\''))
    {
    searchSpec = replaceChars(spec, "'", "''");
    }
else 
    {
    searchSpec = spec;
    }

if (strstr(searchSpec,";") != NULL)
    return handleTwoSites(searchSpec, retChromName, retWinStart, retWinEnd, cart,
			  useWeb, hgAppName);

hgp = hgPositionsFind(searchSpec, "", hgAppName, cart);
if (hgp == NULL || hgp->posCount == 0)
    {
    hgPositionsFree(&hgp);
    errAbort("Sorry, couldn't locate %s in genome database\n", spec);
    return NULL;
    }

if (((pos = hgp->singlePos) != NULL) && (!showAlias || !hgp->useAlias))
    {
    *retChromName = pos->chrom;
    *retWinStart = pos->chromStart;
    *retWinEnd = pos->chromEnd;
    return hgp;
    }
else
    {
    if (*retWinStart != 1)
	hgPositionsHtml(hgp, stdout, useWeb, hgAppName, cart);
    else
	*retWinStart = hgp->posCount;

    return NULL;
    }
}


struct hgPositions *findGenomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart)
/* Search for positions in genome that match user query.   
 * Return TRUE if the query results in a unique position.  
 * Otherwise display list of positions and return FALSE. */
{
return genomePos(spec, retChromName, retWinStart, retWinEnd, cart, TRUE, FALSE, "hgTracks");
}

struct hgPositions *findGenomePosWeb(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart,
	boolean useWeb, char *hgAppName)
/* Search for positions in genome that match user query.   
 * Use the web library to print out HTML headers if necessary, and use 
 * hgAppName when forming URLs (instead of "hgTracks").  
 * Return TRUE if the query results in a unique position.  
 * Otherwise display list of positions and return FALSE. */
{
struct hgPositions *hgp;
if (useWeb)
    webPushErrHandlers();
hgp = genomePos(spec, retChromName, retWinStart, retWinEnd, cart, TRUE,
		useWeb, hgAppName);
if (useWeb)
    webPopErrHandlers();
return hgp;
}

static struct hgPositions *handleTwoSites(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart, boolean useWeb,
	char *hgAppName)
/* Deal with specifications that in form start;end. */
{
char firststring[512];
char secondstring[512];
int commaspot;
char *firstChromName;
int firstWinStart = 0;
int firstWinEnd;
char *secondChromName;
int secondWinStart = 0;
int secondWinEnd;
struct hgPositions *firstSuccess;
struct hgPositions *secondSuccess;

firstWinStart = 1;     /* Pass flags indicating we are dealing with two sites through */
secondWinStart = 1;    /*    firstWinStart and secondWinStart.                        */

commaspot = strcspn(spec,";");
strncpy(firststring,spec,commaspot);
firststring[commaspot] = '\0';
strncpy(secondstring,spec + commaspot + 1,strlen(spec));
firstSuccess = genomePos(firststring, &firstChromName, &firstWinStart,
			 &firstWinEnd, cart, FALSE, useWeb, hgAppName);
secondSuccess = genomePos(secondstring, &secondChromName, &secondWinStart,
			  &secondWinEnd, cart, FALSE, useWeb, hgAppName);
if (NULL == firstSuccess && NULL == secondSuccess)
    {
    errAbort("Neither site uniquely determined.  %d locations for %s and %d locations for %s.",
	     firstWinStart, firststring, secondWinStart, secondstring);
    return NULL;
    }
if (NULL == firstSuccess) 
    {
    errAbort("%s not uniquely determined: %d locations.",
	     firststring, firstWinStart);
    return secondSuccess;
    }
if (NULL == secondSuccess)
    {
    errAbort("%s not uniquely determined: %d locations.",
	     secondstring, secondWinStart);
    return firstSuccess;
    }
if (strcmp(firstChromName,secondChromName) != 0)
    {
    errAbort("Sites occur on different chromosomes: %s,%s.",
	     firstChromName, secondChromName);
    return firstSuccess;
    }
*retChromName = firstChromName;
*retWinStart = min(firstWinStart,secondWinStart);
*retWinEnd = max(firstWinEnd,secondWinEnd);
return firstSuccess;
}


struct hgPositions *hgPositionsFind(char *query, char *extraCgi,
	char *hgAppName, struct cart *cart)
/* Return table of positions that match query or NULL if none such. */
{
struct hgPositions *hgp;
struct hgPosTable *table;
struct hgPos *pos;
int start,end;
char *chrom;
boolean relativeFlag;
char buf[256];
char *startOffset,*endOffset;
int iStart = 0, iEnd = 0;

AllocVar(hgp);
hgp->useAlias = FALSE;
query = trimSpaces(query);
if(query == 0)
    return hgp;

hgp->query = cloneString(query);
hgp->database = hGetDb();
if (extraCgi == NULL)
    extraCgi = "";
hgp->extraCgi = cloneString(extraCgi);

relativeFlag = FALSE;
safef(buf, sizeof(buf), "%s", query);
startOffset = strchr(buf, ':');
if (startOffset != NULL) 
    {
    *startOffset++ = 0;
    endOffset = strchr(startOffset, '-');
    if (endOffset != NULL)
	{
	*endOffset++ = 0;
	startOffset = trimSpaces(startOffset);
	endOffset = trimSpaces(endOffset);
	if ((isdigit(startOffset[0])) && (isdigit(endOffset[0])))
	    {
	    iStart = atoi(startOffset)-1;
	    iEnd = atoi(endOffset);
	    relativeFlag = TRUE;
	    query = buf;
	    }
	}
    }

if (hgIsChromRange(query))
    {
    hgParseChromRange(query, &chrom, &start, &end);
    if (relativeFlag == TRUE)
	{
	end = start + iEnd;
	start = start + iStart;
	}
    singlePos(hgp, "Chromosome Range", NULL, NULL, query, chrom, start, end);
    }
else if (isAffyProbeName(query))
    {
    findAffyProbePos(query, &chrom, &start, &end);
    singlePos(hgp, "GNF Ratio Expression data", NULL, "affyRatio", query, chrom, start, end);
    }
else if (isAffyU133ProbeName(query))
    {
    char *affyName = strstr(query,":");
    assert(affyName);
    affyName++;
    findBedProbePos("affyUcla", affyName , &chrom, &start, &end);
    singlePos(hgp, "UCLA U133 GeneChip Expression data", NULL, "affyUcla", query, chrom, start, end);
    }
else if (isContigName(query) && findContigPos(query, &chrom, &start, &end))
    {
    if (relativeFlag == TRUE)
	{
	end = start + iEnd;
	start = start + iStart;
	}
    singlePos(hgp, "Map Contig", NULL, "ctgPos", query, chrom, start, end);
    }
else if (hgFindCytoBand(query, &chrom, &start, &end))
    {
    singlePos(hgp, "Cytological Band", NULL, "cytoBand", query, chrom, start, end);
    }
else if (hgFindClonePos(query, &chrom, &start, &end))
    {
    if (relativeFlag == TRUE)
	{
	end = start + iEnd;
	start = start + iStart;
	}
    
    singlePos(hgp, "Genomic Clone", NULL, "clonePos", query, chrom, start, end);
    }
else if (findMrnaPos(query, hgp, hgAppName, cart))
    {
    }
else if (findGenethonPos(query, &chrom, &start, &end))	/* HG3 only. */
    {
    singlePos(hgp, "STS Position", NULL, "mapGenethon", query, chrom, start, end);
    }
else if (isBactigName(query) && findBactigPos(query, &chrom, &start, &end))
    {
    if (relativeFlag == TRUE)
	{
	end = start + iEnd;
	start = start + iStart;
	}
    singlePos(hgp, "Bactig", NULL, "bactigPos", query, chrom, start, end);
    }
else if (isRatContigName(query) && findRatContigPos(query, &chrom, &start, &end))
    {
    if (relativeFlag == TRUE)
	{
	end = start + iEnd;
	start = start + iStart;
	}
    singlePos(hgp, "Rat Contig", NULL, "gold", query, chrom, start, end);
    }
else 
    {
    findKnownGenes(query, hgp);
    findRefGenes(query, hgp);
    findKnownGene(query, hgp, "knownGene");
    if (hTableExists("superfamily")) findSuperfamily(query, hgp);
    findFishClones(query, hgp);
    findBacEndPairs(query, hgp);
    findFosEndPairs(query, hgp);
    findFosEndPairsBad(query, hgp);
    findStsPos(query, hgp);
    findMrnaKeys(query, hgp, hgAppName, cart);
    findZooGenes(query, hgp);
    findRgdGenes(query, hgp);
    findSnpPos(query, hgp, "snpTsc");
    findSnpPos(query, hgp, "snpNih");
    findGenePred(query, hgp, "sanger22");
    findGenePred(query, hgp, "sanger20");
    findGenePred(query, hgp, "ensGene");
    findGenePred(query, hgp, "genieAlt");
    findGenePred(query, hgp, "softberryGene");
    findGenePred(query, hgp, "acembly");
    findGenePred(query, hgp, "genscan");
    findGenePred(query, hgp, "sangerGene");
    }

slReverse(&hgp->tableList);
fixSinglePos(hgp);
return hgp;
}

void hgPosTableFree(struct hgPosTable **pEl)
/* Free up hgPosTable. */
{
struct hgPosTable *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->name);
    hgPosFreeList(&el->posList);
    freez(pEl);
    }
}

void hgPosTableFreeList(struct hgPosTable **pList)
/* Free a list of dynamically allocated hgPos's */
{
struct hgPosTable *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hgPosTableFree(&el);
    }
*pList = NULL;
}


void hgPosFree(struct hgPos **pEl)
/* Free up hgPos. */
{
struct hgPos *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->name);
    freeMem(el->description);
    freez(pEl);
    }
}

void hgPosFreeList(struct hgPos **pList)
/* Free a list of dynamically allocated hgPos's */
{
struct hgPos *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hgPosFree(&el);
    }
*pList = NULL;
}

char *hgPosBrowserRange(struct hgPos *pos, char range[64])
/* Convert pos to chrN:123-456 format.  If range parameter is NULL it returns
 * static buffer, otherwise writes and returns range. */
{
static char buf[64];

if (range == NULL)
    range = buf;
sprintf(range, "%s:%d-%d", pos->chrom, pos->chromStart, pos->chromEnd);
return range;
}

void hgPositionsHtml(struct hgPositions *hgp, FILE *f,
		     boolean useWeb, char *hgAppName, struct cart *cart)
/* Write out hgp table as HTML to file. */
{
struct hgPosTable *table;
struct hgPos *pos;
char *desc;
char range[64];
char *ui = getUiUrl(cart);
char *extraCgi = hgp->extraCgi;

if (useWeb)
    webStart(cart, "Select Position");

for (table = hgp->tableList; table != NULL; table = table->next)
    {
    if (table->posList != NULL)
	{
	if (table->htmlStart) 
	    table->htmlStart(table, f);
	else
	    fprintf(f, "<H2>%s</H2><PRE><TT>", table->description);
	for (pos = table->posList; pos != NULL; pos = pos->next)
	    {
	    if (table->htmlOnePos)
	        table->htmlOnePos(table, pos, f);
	    else
		{
		hgPosBrowserRange(pos, range);
		fprintf(f, "<A HREF=\"%s?position=%s",
		    hgAppName, range);
		if (ui != NULL)
		    fprintf(f, "&%s", ui);
		fprintf(f, "%s&%s=full\">%s at %s</A>",
		    extraCgi, table->name, pos->name, range);
		desc = pos->description;
		if (desc)
		    fprintf(f, " - %s", desc);
		fprintf(f, "\n");
		}
	    }
	if (table->htmlEnd) 
	    table->htmlEnd(table, f);
	else
	    fprintf(f, "</PRE></TT>\n");
	}
    }

if (useWeb)
    webEnd();
}


void hgPositionsHelpHtml(char *organism)
/* Explain the position box usage, give some organism-specific examples. */
{
if (strstrNoCase(organism, "human"))
    {
    puts(
"<P>A genome position can be specified by the accession number of a "
"sequenced genomic clone, an mRNA or EST or STS marker, or \n"
"a cytological band, a chromosomal coordinate range, or keywords from "
"the Genbank description of an mRNA. The following list provides "
"examples of various types of position queries for the human genome. "
"See the "
"<A HREF=\"http://genome.cse.ucsc.edu/goldenPath/help/hgTracksHelp.html\" TARGET=_blank>"
"User Guide</A> for more help. \n"
"<P>\n"
"\n"
"<P>\n"
"<TABLE  border=0 CELLPADDING=0 CELLSPACING=0>\n"
"<TR><TD VALIGN=Top NOWRAP><B>Request:</B><br></TD>\n"
"	<TD VALIGN=Top COLSPAN=2><B>&nbsp;&nbsp; Genome Browser Response:</B><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top NOWRAP>chr7</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays all of chromosome 7</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>20p13</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region for band p13 on chr 20</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>chr3:1-1000000</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays first million bases of chr 3, counting from p arm telomere</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>D16S3046</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region around STS marker D16S3046 from the Genethon/Marshfield maps.\n"
"Includes 100,000 bases on each side as well."
"</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>RH18061;RH80175</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region between STS markers RH18061;RH80175.\n"
"Includes 100,000 bases on each side as well."
"</TD></TR>\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>AA205474</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of EST with GenBank accession AA205474 in BRCA1 cancer gene on chr 17\n"
"</TD></TR>\n"
"<!-- <TR><TD VALIGN=Top NOWRAP>ctgchr7_ctg</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of the clone contig ctgchr7_ctg\n"
"	(set \"Map contigs\" track to \"dense\" and refresh to see contigs)</TD></TR> -->\n"
"<TR><TD VALIGN=Top NOWRAP>AC008101</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of clone with GenBank accession AC008101\n"
"</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>AF083811</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of mRNA with GenBank accession number AF083811</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>PRNP</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD>Displays region of genome with HUGO identifier PRNP</TD></TR>\n"
"\n"

"<tr>\n"
"<td valign=\"Top\" nowrap="">NM_017414</td>\n"
"<td><br></td>\n"
"<td valign=\"Top\">Displays the region of genome with RefSeq identifier NM_017414</td></tr>\n"
"<tr>\n"
"<td valign=\"Top\" nowrap="">NP_059110</td>\n"
"<td><br></td>\n"
"<td valign=\"Top\" nowrap=""> Displays the region of genome with protein acccession number NP_059110</td></tr>\n"
"<tr>\n"
"<td valign=\"Top\" nowrap="">11274</td>\n"
"<td><br></td>\n"
"<td valign=\"Top\" nowrap="">Displays the region of genome with LocusLink identifier 11274</td></tr>\n"


"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>pseudogene mRNA</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists transcribed pseudogenes but not cDNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>homeobox caudal</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs for caudal homeobox genes</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>zinc finger</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists many zinc finger mRNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>kruppel zinc finger</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists only kruppel-like zinc fingers</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>huntington</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists candidate genes associated with Huntington's disease</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>zahler</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs deposited by scientist named Zahler</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>Evans,J.E.</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs deposited by co-author J.E.  Evans</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD COLSPAN=\"3\" > Use this last format for entry authors -- "
"even though Genbank searches require Evans JE "
"format, GenBank entries themselves use Evans,J.E.  internally.\n"
"</TABLE>\n"
"\n");
    }
else if (strstrNoCase(organism, "mouse"))
    {
    puts("<P><H2>Mouse</P></H2>\n");
    puts(
"<P>A genome position can be specified by the accession number of a "
"sequenced genomic clone, an mRNA or EST or STS marker, or \n"
"a cytological band, a chromosomal coordinate range, or keywords from "
"the Genbank description of an mRNA. The following list provides "
"examples of various types of position queries for the mouse genome. "
"See the "
"<A HREF=\"http://genome.cse.ucsc.edu/goldenPath/help/hgTracksHelp.html\" TARGET=_blank>"
"User Guide</A> for more help. \n"
"<P>\n"
"\n"
"<P>\n"
"<TABLE  border=0 CELLPADDING=0 CELLSPACING=0>\n"
"<TR><TD VALIGN=Top NOWRAP><B>Request:</B><br></TD>\n"
"	<TD VALIGN=Top COLSPAN=2><B>&nbsp;&nbsp; Genome Browser Response:</B><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top NOWRAP>chr16</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays all of chromosome 16</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>chr16:1-5000000</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays first 5 million bases of chr 16</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>D16Mit120</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region around STS marker DMit16120\n"
" from the MGI consensus genetic map.\n"
" Includes 100,000 bases on each side as well." 
"</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>D16Mit203;D16Mit70</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region between STS markers D16Mit203 and D16Mit70.\n"
"Includes 100,000 bases on each side as well."
"</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>AW045217</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of EST with GenBank accession AW045217</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>Ncam2</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of genome with official MGI mouse genetic nomenclature Ncam2</TD></TR>\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>pseudogene mRNA</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists transcribed pseudogenes but not cDNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>zinc finger</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists many zinc finger mRNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>kruppel zinc finger</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists only kruppel-like zinc fingers</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>huntington</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists candidate genes associated with Huntington's disease</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>Evans,J.E.</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs deposited by co-author J.E.  Evans</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD COLSPAN=\"3\" > Use this last format for entry authors -- "
"even though Genbank searches require Evans JE "
"format, GenBank entries themselves use Evans,J.E.  internally.\n"
"</TABLE>\n"
"\n");
    }
else if (strstrNoCase(organism, "rat"))
    {
    puts("<P><H2>Rat</P></H2>\n");
    puts(
"<P>A genome position can be specified by the accession number of a "
"sequenced genomic clone, an mRNA or EST or STS marker, \n"
"a chromosomal coordinate range, or keywords from "
"the Genbank description of an mRNA. The following list provides "
"examples of various types of position queries for the rat genome. "
"See the "
"<A HREF=\"http://genome.cse.ucsc.edu/goldenPath/help/hgTracksHelp.html\" TARGET=_blank>"
"User Guide</A> for more help. \n"
"<P>\n"
"\n"
"<P>\n"
"<TABLE  border=0 CELLPADDING=0 CELLSPACING=0>\n"
"<TR><TD VALIGN=Top NOWRAP><B>Request:</B><br></TD>\n"
"	<TD VALIGN=Top COLSPAN=2><B>&nbsp;&nbsp; Genome Browser Response:</B><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top NOWRAP>chr16</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays all of chromosome 16</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>chr16:1-5000000</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays first 5 million bases of chr 16</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>RNOR01065682;RNOR01078956</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region between Assembly IDs RNOR01065682 and RNOR01078956\n"
"</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>AI501130</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of EST with GenBank accession AI501130</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>AF199335</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of mRNA with GenBank accession AF199335</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>apoe</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of genome with gene identifier apoE</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>NM_145881</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of genome with RefSeq identifier NM_145881</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>25728</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of genome with LocusLink identifier 25728</TD></TR>\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>pseudogene mRNA</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists transcribed pseudogenes but not cDNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>zinc finger</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists many zinc finger mRNAs</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>kruppel zinc finger</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists only kruppel-like zinc fingers</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>huntington</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists candidate genes associated with Huntington's disease</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>Jones,R.</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs deposited by co-author R. Jones</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD COLSPAN=\"3\" > Use this last format for entry authors -- "
"even though Genbank searches require Jones R "
"format, GenBank entries themselves use Jones,R.  internally.\n"
"</TABLE>\n"
"\n");
    }
else if (strstrNoCase(organism, "SARS"))
    {
    puts("<P><H2>SARS</P></H2>\n");
    puts(
"<P>A genome position can be specified by the accession number of an "
"mRNA, a coordinate range, a gene identifier, or keywords from "
"the Genbank description of an mRNA. The following list provides "
"examples of various types of position queries for the SARS coronavirus TOR2 genome. "
"See the "
"<A HREF=\"http://genome.cse.ucsc.edu/goldenPath/help/hgTracksHelp.html\" TARGET=_blank>"
"User Guide</A> for more help. \n"
"<P>\n"
"\n"
"<P>\n"
"<TABLE  border=0 CELLPADDING=0 CELLSPACING=0>\n"
"<TR><TD VALIGN=Top NOWRAP><B>Request:</B><br></TD>\n"
"	<TD VALIGN=Top COLSPAN=2><B>&nbsp;&nbsp; Genome Browser Response:</B><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD VALIGN=Top NOWRAP>chr1</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays the entire genome for SARS coronavirus TOR2</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>chr1:15720-186200</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays the region between bases 15720 and 186200 showing the protein NP_828870.1.</TD></TR>\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>AF391541</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of mRNA with GenBank accession AF391541</TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>CS000003</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Displays region of genome with fgenesv+ gene identifier CS000003</TD></TR>\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top NOWRAP>bovine coronavirus</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs associated with bovine coronavirus </TD></TR>\n"
"<TR><TD VALIGN=Top NOWRAP>Chouljenko,V.</TD>\n"
"	<TD WIDTH=14></TD>\n"
"	<TD VALIGN=Top>Lists mRNAs deposited by co-author V. Chouljenko</TD></TR>\n"
"\n"
"<TR><TD VALIGN=Top><br></TD></TR>\n"
"	\n"
"<TR><TD COLSPAN=\"3\" > Use this last format for entry authors -- "
"even though Genbank searches require Chouljenko V "
"format, GenBank entries themselves use Chouljenko,V. internally.\n"
"</TABLE>\n"
"\n");
    }
else 
    {
    printf("<H2>%s</H2>", organism);
    }
}


