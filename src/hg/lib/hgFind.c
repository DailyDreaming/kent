/* hgFind.c - Find things in human genome annotations. */
#include "common.h"
#include <regex.h>
#include "obscure.h"
#include "hCommon.h"
#include "portable.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "web.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "genePred.h"
#include "bed.h"
#include "cytoBand.h"
#include "cart.h"
#include "hgFind.h"
#include "hgFindSpec.h"
#include "snp.h"
#include "refLink.h"
#include "kgAlias.h"
#include "kgProtAlias.h"
#include "findKGAlias.h"
#include "findKGProtAlias.h"
#include "tigrCmrGene.h"
#include "minGeneInfo.h"
#include "pipeline.h"
#include "hgConfig.h"
#include "trix.h"

static char const rcsid[] = "$Id: hgFind.c,v 1.180 2006/02/16 18:26:25 angie Exp $";

extern struct cart *cart;
char *hgAppName = "";

/* alignment tables to check when looking for mrna alignments */
static char *estTables[] = { "all_est", "xenoEst", NULL};
static char *mrnaTables[] = { "all_mrna", "xenoMrna", NULL};
static struct dyString *hgpMatchNames = NULL;

static void hgPosFree(struct hgPos **pEl)
/* Free up hgPos. */
{
struct hgPos *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->name);
    freeMem(el->description);
    freeMem(el->browserName);
    freez(pEl);
    }
}

static void hgPosFreeList(struct hgPos **pList)
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

static void hgPosTableFree(struct hgPosTable **pEl)
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

static void hgPosTableFreeList(struct hgPosTable **pList)
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


static void hgPositionsFree(struct hgPositions **pEl)
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

#if 0 /* not used */
static void hgPositionsFreeList(struct hgPositions **pList)
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
#endif

static char *hgPosBrowserRange(struct hgPos *pos, char range[64])
/* Convert pos to chrN:123-456 format.  If range parameter is NULL it returns
 * static buffer, otherwise writes and returns range. */
{
static char buf[64];

if (range == NULL)
    range = buf;
sprintf(range, "%s:%d-%d", pos->chrom, pos->chromStart, pos->chromEnd);
return range;
}


#if 0 /* not used */
static char *getGrepIndexFile(struct hgFindSpec *hfs)
/* Return grepIndex setting (may be relative to hg.conf grepIndex.default),
 * or NULL if the file doesn't exist. */
{
char *indexFile = hgFindSpecSetting(hfs, "grepIndex");
if (indexFile == NULL)
    return NULL;
else if (fileExists(indexFile))
    return cloneString(indexFile);
else if (! startsWith("/", indexFile))
    {
    char *grepIndexRoot = cfgOption("grepIndex.default");
    if (grepIndexRoot != NULL)
	{
	char absPath[1024];
	safef(absPath, sizeof(absPath), "%s/%s/%s",
	      grepIndexRoot, hGetDb(), indexFile);
	if (fileExists(absPath))
	    return cloneString(absPath);
	}
    }
return NULL;
}
#endif

#define HGFIND_MAX_KEYWORDS 16
#define HGFIND_MAX_CMDWORDS 6

static void makeCmds(char **cmds[HGFIND_MAX_KEYWORDS+1],
		     char **keyWords, int keyCount, char *extraOptions)
/* Fill in cmds, an array of command word arrays. */
{
int i;
for (i=0;  i < keyCount;  i++)
    {
    char **cmd = NULL;
    int j = 0;
    AllocArray(cmd, HGFIND_MAX_CMDWORDS);
    cmd[j++] = "fgrep";
    cmd[j++] = "-i";
    if (isNotEmpty(extraOptions))
	cmd[j++] = extraOptions;
    cmd[j++] = keyWords[i];
    cmd[j++] = NULL;
    if (j > HGFIND_MAX_CMDWORDS)
	errAbort("overflow error -- increase HGFIND_MAX_CMDWORDS.");
    cmds[i] = cmd;
    }
cmds[i] = NULL;
}

static void freeCmds(char **cmds[], int keyCount)
/* Free each element of cmds. */
{
int i;
for (i=0;  i < keyCount;  i++)
    {
    freez(&(cmds[i]));
    }
}

static boolean keyIsPrefix(char *key, char *text)
/* Return TRUE only if key is at the start of some word in text. 
 * For short keys (2 or less) it must be whole word. */
{
char *s = text;
int keyLen = strlen(key);
while ((s = stringIn(key, s)) != NULL)
    {
    if (s == text || !isalnum(s[-1]))
	{
	if (keyLen > 2 || !isalnum(s[keyLen]))
	    return TRUE;
	}
    s += 1;
    }
return FALSE;
}

static boolean keyIsPrefixIgnoreCase(char *key, char *text)
/* Case insensitive keyIsPrefix */
{
boolean isPrefix;
key = cloneString(key);
touppers(key);
text = cloneString(text);
touppers(text);
isPrefix = keyIsPrefix(key, text);
freeMem(key);
freeMem(text);
return isPrefix;
}

static boolean allKeysPrefix(char **keys, int keyCount, char *text)
/* Make sure that all keys in text are proper prefixes of a word. */
{
int i;
touppers(text);
for (i=0; i<keyCount; ++i)
    {
    if (!keyIsPrefix(keys[i], text))
        return FALSE;
    }
return TRUE;
}

static char *commonDescriptionWords[] = {
/* List of words that are too common in mRNA descriptions to be useful.
 * Every year or two we might want to update this. */
  "LIKE",
  "HUMAN",
  "FACTOR",
  "ISOFORM",
  "GENE",
  "FROM",
  "HYPOTHETICAL",
  "FAMILY",
  "UNKNOWN",
  "PA",
  "MEMBER",
  "SIMILAR",
  "TO",
  "REGION",
  "MUS",
  "MUSCULUS",
  "MELANOGASTER",
  "CAENORHABDITIS",
  "CHAIN",
  "ELEGANS",
  "DROSOPHILA",
  "COT",
  "NORMALIZED",
  "TRANSCRIPT",
  "FOR",
  "VARIANT",
  "FIS",
  "THALIANA",
  "ARABIDOPSIS",
  "PARTIAL",
  "LENGTH",
  "HUMAN",
  "FULL",
  "IMAGE",
  "PROTEIN",
  "OF",
  "COMPLETE",
  "CDNA",
  "CLONE",
  "CDS",
  "HOMO",
  "SAPIENS",
  "MRNA",
  "5",
  "4",
  "3",
  "2",
  "1",
};

static char *commonProductNameWords[] = {
/* List of words that are too common in productNames to be useful.
 * Every year or two we might want to update this. */
  "MEMBER",
  "FAMILY",
  "UNKNOWN",
  "FOR",
  "PUTATIVE",
  "LIKE",
  "5",
  "4",
  "3",
  "2",
  "1",
  "HYPOTHETICAl",
  "ISOFORM",
 "PROTEIN",
};

static boolean anyStartWith(char *prefix, char *words[], int wordCount)
/* Return TRUE if any words start with prefix. */
{
int i;
for (i=0; i<wordCount; ++i)
    {
    if (startsWith(prefix, words[i]))
        return TRUE;
    }
return FALSE;
}

static boolean isTooCommon(char *table, char *key)
/* Return TRUE if key is too common to be useful in table */
{
if (sameString(table, "description"))
    return anyStartWith(key, commonDescriptionWords, 
    	ArraySize(commonDescriptionWords));
else if (sameString(table, "productName"))
    return anyStartWith(key, commonProductNameWords, 
    	ArraySize(commonProductNameWords));
else if (sameString(table, "author"))
    return sameWord(key, "and");
else
    return FALSE;
}

static int removeTooCommon(char *table, char **keys, int keyCount)
/* Remove keys that are too common to be meaningful. */
{
int readIx, writeIx=0;
for (readIx=0; readIx < keyCount; ++readIx)
    {
    if (!isTooCommon(table, keys[readIx]))
        {
	keys[writeIx] = keys[readIx];
	writeIx += 1;
	}
    }
return writeIx;
}


static struct slName *doGrepQuery(char *indexFile, char *table, char *key,
				  char *extraOptions)
/* grep -i key indexFile, return a list of ids (first word of each line). */
{
struct pipeline *pl = NULL;
struct slName *idList = NULL;
struct lineFile *lf = NULL;
char *id, *rest, *line;
char *keyWords[HGFIND_MAX_KEYWORDS];
char **cmds[HGFIND_MAX_KEYWORDS+1];
/* No need to escape special chars here, it's already been done: */
char *escapedKey = cloneString(key);
int keyCount;

keyCount = chopLine(escapedKey, keyWords);
keyCount = removeTooCommon(table, keyWords, keyCount);
if (keyCount > 0)
    {
    if (extraOptions == NULL)
	extraOptions = "";
    makeCmds(cmds, keyWords, keyCount, extraOptions);

    pl = pipelineOpen(cmds, pipelineRead | pipelineNoAbort, indexFile);
    lf = pipelineLineFile(pl);
    verbose(3, "\n***Running this fgrep command with pipeline from %s:\n*** %s\n\n",
	    indexFile, pipelineDesc(pl));
    while (lineFileNextReal(lf, &line))
	{
	id = nextWord(&line);
	rest = skipLeadingSpaces(line);
	touppers(line);
	if (allKeysPrefix(keyWords, keyCount, rest))
	    {
	    struct slName *idEl = slNameNew(id);
	    slAddHead(&idList, idEl);
	    }
	}
    pipelineFree(&pl);  /* Takes care of lf too. */
    freeCmds(cmds, keyCount);
    if (verboseLevel() >= 3)
	{
	int count = slCount(idList);
	verbose(3, "*** Got %d results from %s\n\n", count, indexFile);
	}
    }
freeMem(escapedKey);
return idList;
}


char *MrnaIDforGeneName(char *geneName)
/* return mRNA ID for a gene name */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
char query[256];
char **row;
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

static boolean findKnownGeneExact(char *spec, char *geneSymbol,
				  struct hgPositions *hgp, char *tableName)
/* Look for position in Known Genes table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
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
	table->name = cloneString("knownGene");
	slAddHead(&hgp->tableList, table);
	}
    AllocVar(pos);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(geneSymbol);
/*    pos->browserName = cloneString(geneSymbol); highlight change */
    pos->browserName = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL) 
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static struct hgPosTable *addKnownGeneTable(struct hgPositions *hgp)
/* Create new table for known genes matches, add it to hgp, and return it. */
{
struct hgPosTable *table;
AllocVar(table);
table->description = cloneString("Known Genes");
table->name = cloneString("knownGene");
slAddHead(&hgp->tableList, table);
return table;
}

static boolean gotFullText()
/* Return TRUE if we have full text index. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "/gbdb/%s/knownGene.ix", hGetDb());
if (fileExists(path))
    return TRUE;
else
    {
    warn("%s doesn't exist", path);
    return FALSE;
    }
}

struct tsrPos
/* Little helper structure tying together search result
 * and pos, used by addKnownGeneItems */
    {
    struct tsrPos *next;	/* Next in list. */
    struct trixSearchResult *tsr;	/* Basically a gene symbol */
    struct hgPos *posList;		/* Associated list of positions. */
    };

static void addKnownGeneItems(struct hgPosTable *table,
	struct trixSearchResult *tsrList, struct sqlConnection *conn)
/* Convert tsrList to posList, and hang posList off of table. */
{
/* This code works with just two SQL queries no matter how
 * big the search result list is.  For cases where the search 
 * result list is big (say 100 or 1000 items) this is noticably
 * faster than the simpler-to-code approach that would do two 
 * queries for each search result.  We pay for this speed tweak
 * by having to construct a more elaborate query, and by having
 * to maintain a hash to connect the query results back to the
 * individual positions. */
struct dyString *dy = dyStringNew(0);
struct trixSearchResult *tsr;
struct hash *hash = hashNew(16);
struct hgPos *posList = NULL, *pos;
struct tsrPos *tpList = NULL, *tp;
struct sqlResult *sr;
char **row;
int maxToReturn = 500;

if (slCount(tsrList) > maxToReturn)
    {
    warn("Search terms are not very specific, only showing first %d matching known genes.",
    	maxToReturn);
    tsr = slElementFromIx(tsrList, maxToReturn-1);
    tsr->next = NULL;
    }

/* Make hash of all search results - one for each known gene ID. */
for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
    {
    lmAllocVar(hash->lm, tp);
    tp->tsr = tsr;
    slAddHead(&tpList, tp);
    hashAdd(hash, tsr->itemId, tp);
    }

/* Stream through knownGenes table and make up a pos
 * for each mapping of each gene matching search. */
dyStringAppend(dy, 
	"select name,chrom,txStart,txEnd from knownGene where name in (");
for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
    {
    dyStringAppendC(dy, '"');
    dyStringAppend(dy, tsr->itemId);
    dyStringAppendC(dy, '"');
    if (tsr->next != NULL)
        dyStringAppendC(dy, ',');
    }
dyStringAppend(dy, ")");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    tp = hashFindVal(hash, row[0]);
    if (tp == NULL)
        internalErr();
    AllocVar(pos);
    pos->chrom = cloneString(row[1]);
    pos->chromStart = sqlUnsigned(row[2]);
    pos->chromEnd = sqlUnsigned(row[3]);
    slAddHead(&tp->posList, pos);
    }
sqlFreeResult(&sr);

/* Stream through kgXref table adding description and geneSymbol */
dyStringClear(dy);
dyStringAppend(dy, 
	"select kgID,geneSymbol,description from kgXref where kgID in (");
for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
    {
    dyStringAppendC(dy, '"');
    dyStringAppend(dy, tsr->itemId);
    dyStringAppendC(dy, '"');
    if (tsr->next != NULL)
        dyStringAppendC(dy, ',');
    }
dyStringAppend(dy, ")");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    tp = hashFindVal(hash, row[0]);
    if (tp == NULL)
        internalErr();
    for (pos = tp->posList; pos != NULL; pos = pos->next)
        {
	char nameBuf[256];
	safef(nameBuf, sizeof(nameBuf), "%s (%s)", row[1], row[0]);
	pos->name = cloneString(nameBuf);
	pos->description = cloneString(row[2]);
	pos->browserName = cloneString(row[0]);
	}
    }
sqlFreeResult(&sr);

/* Hang all pos onto table. */
for (tp = tpList; tp != NULL; tp = tp->next)
    {
    struct hgPos *next;
    for (pos = tp->posList; pos != NULL; pos = next)
        {
	next = pos->next;
	slAddHead(&posList, pos);
	}
    }
table->posList = posList;
hashFree(&hash);
dyStringFree(&dy);
}

boolean findKnownGeneFullText(char *term,struct hgPositions *hgp)
/* Look for position in full text. */
{
char *db = hGetDb();
char path[PATH_LEN];
boolean gotIt = FALSE;
struct trix *trix;
struct trixSearchResult *tsrList;
char *lowered = cloneString(term);
char *keyWords[HGFIND_MAX_KEYWORDS];
int keyCount;

safef(path, sizeof(path), "/gbdb/%s/knownGene.ix", db);
trix = trixOpen(path);
tolowers(lowered);
keyCount = chopLine(lowered, keyWords);
tsrList = trixSearch(trix, keyCount, keyWords, TRUE);
if (tsrList != NULL)
    {
    struct hgPosTable *table = addKnownGeneTable(hgp);
    struct sqlConnection *conn = hAllocConn();
    addKnownGeneItems(table, tsrList, conn);
    hFreeConn(&conn);
    gotIt = TRUE;
    }
freez(&lowered);
trixSearchResultFreeList(&tsrList);
trixClose(&trix);
return gotIt;
}

static boolean findKnownGeneDescLike(char *spec, struct hgPositions *hgp,
				 char *tableName)
/* Look for position in gene prediction table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
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
dyStringPrintf(query, "SELECT chrom, txStart, txEnd, name, description FROM %s, kgXref "
	       "WHERE description LIKE '%%%s%%' and kgId=name",
	       tableName, localName);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	table = addKnownGeneTable(hgp);
	}
    AllocVar(pos);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(row[3]);
    pos->description = cloneString(row[4]);
    pos->browserName = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}
static boolean findKnownGeneLike(char *spec, struct hgPositions *hgp,
				 char *tableName)
/* Look for position in gene prediction table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
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
dyStringPrintf(query, "SELECT chrom, txStart, txEnd, name FROM %s "
	       "WHERE name LIKE '%s%%'",
	       tableName, localName);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	table = addKnownGeneTable(hgp);
	slAddHead(&hgp->tableList, table);
	}

    AllocVar(pos);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(row[3]);
    pos->browserName = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static boolean findKnownGene(char *spec, struct hgPositions *hgp,
			     char *tableName)
/* Look for position in gene prediction table. */
{
char *mrnaID;

if (! hTableExists("knownGene"))
    return FALSE;

if (findKnownGeneExact(spec, spec, hgp, tableName))
    {    
    return(TRUE);
    }
else
    {
    mrnaID = MrnaIDforProtein(spec);

    if (mrnaID == NULL)
	{
    	mrnaID = MrnaIDforProtein(spec);
	}
    if (mrnaID != NULL)
	{
	return(findKnownGeneExact(mrnaID, spec, hgp, tableName));
	}
    else
	{
	mrnaID = MrnaIDforGeneName(spec);
	if (mrnaID != NULL)
	    {
	    return(findKnownGeneExact(mrnaID, spec, hgp, tableName));
	    }
	else
	    {
	    if (findKnownGeneLike(spec, hgp, tableName) == TRUE)
	    	{
		return(TRUE);
		}
	    else
	    	{
	    	return(findKnownGeneDescLike(spec, hgp, tableName));
	    	}
	    }
	}
    }
return FALSE;
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


static void singlePos(struct hgPositions *hgp, char *tableDescription,
                      char *posDescription, char *tableName, char *posName,
                      char *browserName, char *chrom, int start, int end)
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
pos->browserName = cloneString(browserName);
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
/* Return TRUE if spec is a cytological band name including chromosome short 
 * name. Returns chromosome chrN name and band (with chromosome stripped off) */
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

boolean hgFindCytoBand(char *spec, char **retChromName, int *retWinStart,
		       int *retWinEnd)
/* Return position associated with cytological band if spec looks to be 
 * in that form. */
{
char *bandName;

if (!hgIsCytoBandName(spec, retChromName, &bandName))
     return FALSE;
hgFindChromBand(*retChromName, bandName, retWinStart, retWinEnd);
return TRUE;
}

boolean findChromContigPos(char *name, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of contig.  Look in all chroms.
 * Don't alter return variables unless found. */
/* NOTE: could probably speed this up by using the chromInfo hashtable */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct slName *allChroms = hAllChromNames();
struct slName *chromPtr;
char **row;
char query[256];
boolean foundIt = FALSE;

for (chromPtr=allChroms;  chromPtr != NULL;  chromPtr=chromPtr->next)
    {
    char tableName[256];
    safef(tableName, sizeof(tableName), "%s_gold", chromPtr->name);
    if (! sqlTableExists(conn, tableName))
	continue;
    safef(query, sizeof(query), 
	  "select chromStart,chromEnd from %s where frag = '%s'",
	  tableName, name);
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

#if 0 /* not used */
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
#endif

static boolean mrnaInfo(char *acc, struct sqlConnection *conn, 
                                char **mrnaType, int *organismID)
/* Return mrna/est type and organism name for the accession */
/* Ignores returned values if parameters are NULL */
/* Return TRUE if search succeeded, else FALSE */
/* NOTE: caller must free mrnaType */
{
char query[256];
struct sqlResult *sr;
char **row;
int ret;

sprintf(query, "select type, organism from gbCdnaInfo where acc = '%s'", acc);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (mrnaType != NULL)
        *mrnaType = cloneString(row[0]);
    if (organismID != NULL)
        *organismID = sqlUnsigned(row[1]);
    ret = TRUE;
    }
else
    ret = FALSE;
sqlFreeResult(&sr);
return ret;
}

boolean isRefSeqAcc(char *acc)
/* Return TRUE if acc looks like a RefSeq acc. */
{
return matchRegex(acc, "^(N|X)M_[0-9]{6}[0-9]*$");
}

static char *mrnaType(char *acc)
/* Return "mrna" or "est" if acc is mRNA, otherwise NULL.  Returns
 * NULL for refseq mRNAs */
/* for compat with older databases, just look at the seqId to
 * determine if it's a refseq, don't use table */
/* NOTE: caller must free returned type */
{
struct sqlConnection *conn;
char *type;
char *ret;

if (isRefSeqAcc(acc))
    return NULL;
conn = hAllocConn();
if (mrnaInfo(acc, conn, &type, NULL))
   ret = type;
else
   ret = NULL;
hFreeConn(&conn);
return ret;
}

static struct psl *findAllAli(char *acc, char *lowerType, boolean *retIsXeno)
/* Return a list of alignmentts to the given accession.  lowerType tells 
 * whether to look in mrna or est tables; if acc is found in a xeno table, 
 * set *retIsXeno to TRUE. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct psl *pslList = NULL, *psl;
struct sqlResult *sr;
char **row;
int rowOffset;
char **tables, *table;

if (sameWord(lowerType, "mrna"))
    tables = mrnaTables;
else if (sameWord(lowerType, "est"))
    tables = estTables;
else
    /* invalid type */
    return pslList;

while ((table = *tables++) != NULL)
    {
    if (sqlTableExists(conn, table))
	{
        rowOffset = hOffsetPastBin(NULL, table);
        snprintf(query, sizeof(query), "select * from %s where qName = '%s'",
                                                             table, acc);
        sr = sqlGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            psl = pslLoad(row+rowOffset);
            slAddTail(&pslList, psl);
            }
        if (pslList != NULL) 
	    {
	    if (retIsXeno != NULL)
		{
		if (startsWith("xeno", table))
		    *retIsXeno = TRUE;
		else
		    *retIsXeno = FALSE;
		}
            /* for speed -- found proper table, so don't need to look farther */
            break;
	    }
	}
    }
hFreeConn(&conn);
return pslList;
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

static void makeGbTrackTableName(char *tableName, size_t tnSize, char *base)
/* Now we have to watch out for scaffold-based browsers where 
 * the track is all_{mrna,est} not {mrna,est}. */
{
char splitTable[256];
safef(splitTable, sizeof(splitTable), "%s_%s",
      hDefaultChrom(), base);
if (hTableExists(splitTable))
    safef(tableName, tnSize, "%s", base);
else
    safef(tableName, tnSize, "all_%s", base);
}

static boolean findMrnaPos(char *acc,  struct hgPositions *hgp)
/* Find MRNA or EST position(s) from accession number.
 * Look to see if it's an mRNA or EST.  Fill in hgp and return
 * TRUE if it is, otherwise return FALSE. */
/* NOTE: this excludes RefSeq mrna's, as they are currently
 * handled in findRefGenes(), which is called later in the main function */
{
char *type;
char *extraCgi = hgp->extraCgi;
char *ui = getUiUrl(cart);
char tableName [64];
if (!hTableExists("gbCdnaInfo"))
    return FALSE;
if ((type = mrnaType(acc)) == NULL || type[0] == 0)
    /* this excludes refseq mrna's, and accessions with
     * invalid column type in mrna table (refseq's and ests) */
    return FALSE;
else
    {
    struct psl *pslList, *psl;
    int pslCount;
    char suffix[16];
    struct hgPosTable *table;
    struct hgPos *pos;
    char hgAppCombiner = (strchr(hgAppName, '?')) ? '&' : '?';
    boolean isXeno = FALSE;

    strncpy(suffix, type, sizeof(suffix));
    tolowers(suffix);
    pslList = psl = findAllAli(acc, suffix, &isXeno);
    pslCount = slCount(pslList);
    if (pslCount <= 0)
	return FALSE;
    else
        {
	struct dyString *dy = newDyString(1024);
	
        if (isXeno)
            {
	    safef(tableName, sizeof(tableName), "xeno%s", suffix);
	    toUpperN(tableName+strlen("xeno"), 1);
            }
        else
            {
	    makeGbTrackTableName(tableName, sizeof(tableName), suffix);
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
	    pos->browserName = cloneString(psl->qName);
	    dyStringPrintf(dy, "<A HREF=\"%s%cposition=%s&%s=%s",
	        hgAppName, hgAppCombiner, hgPosBrowserRange(pos, NULL),
		tableName, hTrackOpenVis(tableName));
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

static char *getGenbankGrepIndex(struct hgFindSpec *hfs, char *table,
				 char *suffix)
/* If hg.conf has a grepIndex.genbank setting, hfs has a (placeholder)
 * grepIndex setting, and we can access the index file for table, then
 * return the filename; else return NULL. */
/* Special case for genbank: Mark completely specifies the root in
 * hg.conf, so hfs's grepIndex setting value is ignored -- it is used
 * only to enable grep indexing.  So we have multiple ways to turn this 
 * off if necessary: remove hg.conf setting (takes out all dbs), 
 * remove hgFindSpec setting (takes out one db at a time), or remove 
 * a file (takes out one table at a time). */
{
char *grepIndexRoot = cfgOption("grepIndex.genbank");
char *hfsSetting = hgFindSpecSetting(hfs, "grepIndex");

if (grepIndexRoot != NULL && hfsSetting != NULL)
    {
    char buf[1024];
    safef(buf, sizeof(buf), "%s/%s/%s.%s",
	  grepIndexRoot, hGetDb(), table, suffix);
    if (fileExists(buf))
	return cloneString(buf);
    }
return NULL;
}


static struct slName *genbankGrepQuery(char *indexFile, char *table, char *key)
/* grep -i key indexFile, return a list of ids (first word of each line). */
{
char *extraOptions = "";
if (sameString(table, "author"))
    extraOptions = "-w";
return doGrepQuery(indexFile, table, key, extraOptions);
}

static struct slName *genbankSqlFuzzyQuery(struct sqlConnection *conn,
					   char *table, char *key)
/* Perform a fuzzy sql search for %key% in table.name; return list of 
 * corresponding table.id's.  */
{
struct slName *idList = NULL, *idEl = NULL;
if (!isTooCommon(table, key))
    {
    struct sqlResult *sr;
    char **row;
    char query[256];
    safef(query, sizeof(query),
	  "select id,name from %s where name like '%%%s%%'", table, key);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	touppers(row[1]);
	if (keyIsPrefix(key, row[1]))
	    {
	    idEl = newSlName(row[0]);
	    slAddHead(&idList, idEl);
	    }
	}
    sqlFreeResult(&sr);
    }
return idList;
}

static boolean gotAllGenbankGrepIndexFiles(struct hgFindSpec *hfs,
					   char *tables[], int tableCount)
/* Return TRUE if all tables have a readable genbank index file. */
{
int i;
for (i=0;  i < tableCount;  i++)
    if (! getGenbankGrepIndex(hfs, tables[i], "idName"))
	return FALSE;
return TRUE;;
}

static void findHitsToTables(struct hgFindSpec *hfs,
			     char *key, char *tables[], int tableCount, 
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
    char *grepIndexFile = NULL;
    
    /* I'm doing this query in two steps in C rather than
     * in one step in SQL just because it somehow is much
     * faster this way (like 100x faster) when using mySQL. */
    field = tables[i];
    if (!hTableExists(field))
	continue;
    if ((grepIndexFile = getGenbankGrepIndex(hfs, field, "idName")) != NULL)
	idList = genbankGrepQuery(grepIndexFile, field, key);
    else
	idList = genbankSqlFuzzyQuery(conn, field, key);
    for (idEl = idList; idEl != NULL; idEl = idEl->next)
        {
        /* don't check srcDb to exclude refseq for compat with older tables */
	safef(query, sizeof(query),
	      "select acc, organism from gbCdnaInfo where %s = %s "
	      " and type = 'mRNA'",
	      field, idEl->name);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    char *acc = row[0];
            /* will use this later to distinguish xeno mrna */
	    int organismID = sqlUnsigned(row[1]);
	    if (!isRefSeqAcc(acc) && !hashLookup(hash, acc))
		{
		el = newSlName(acc);
                slAddHead(&list, el);
                hashAddInt(hash, acc, organismID);
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
    int organismID = hashIntValDefault(bHash, name, -1);
    if (organismID >= 0 && !hashLookup(hash, name))
        {
	newEl = newSlName(name);
	slAddHead(&list, newEl);
	hashAddInt(hash, name, organismID);
	}
    }
*retHash = hash;
*retList = list;
}

static void mrnaKeysHtmlOnePos(struct hgPosTable *table, struct hgPos *pos,
			       FILE *f)
{
fprintf(f, "%s", pos->description);
}

static boolean mrnaAligns(struct sqlConnection *conn, char *table, char *acc)
/* Return TRUE if accession is in the designated alignment table (for speed,
 * this assumes that we've already checked that the table exists) */
{
char query[256];
safef(query, sizeof(query), 
      "select count(*) from %s where qName = '%s'", table, acc);
return (sqlQuickNum(conn, query) > 0);
}

static int addMrnaPositionTable(struct hgPositions *hgp, 
                                struct slName **pAccList,
				struct hash *accOrgHash, struct cart *cart,
                                struct sqlConnection *conn, char *hgAppName,
                                boolean aligns, boolean isXeno)
/* Generate table of positions that match criteria.
 * Add to hgp if any found. Return number found */
{
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
struct slName *el = NULL;
struct slName *elToFree = NULL;
struct dyString *dy = newDyString(256);
char query[256];
char description[512];
char product[256];
char organism[128];
char *ui = getUiUrl(cart);
char *acc = NULL;
int itemOrganismID;
int organismID = hOrganismID(hgp->database);   /* id from mrna organism table */
int alignCount = 0;
char hgAppCombiner = (strchr(hgAppName, '?')) ? '&' : '?';
char *mrnaTable = isXeno ? "xenoMrna" : "all_mrna";
boolean mrnaTableExists = hTableExistsDb(hgp->database, mrnaTable);

AllocVar(table);

/* Examine all accessions to see if they fit criteria for
 * this table. Add all matching to the position list, and
 * remove from the accession list */
for (el = *pAccList; el != NULL; el = el->next)
    {
    freez(&elToFree);
    acc = el->name;
    itemOrganismID = hashIntVal(accOrgHash, acc);

    /* check if item matches xeno criterion */
    if (isXeno == (itemOrganismID == organismID))
        continue;

    /* check if item matches alignment criterion */
    if (aligns != (mrnaTableExists && mrnaAligns(conn, mrnaTable, acc)))
	continue;

    /* item fits criteria, so enter in table */
    AllocVar(pos);
    slAddHead(&table->posList, pos);
    pos->name = cloneString(acc);
    pos->browserName = cloneString(acc);
    dyStringClear(dy);
    
    if (aligns)
        {
        dyStringPrintf(dy, "<A HREF=\"%s%cposition=%s",
		       hgAppName, hgAppCombiner, acc);
        }
    else
        {
	char tableName[64];
	if (isXeno)
	    safef(tableName, sizeof(tableName), "xenoMrna");
	else
	    makeGbTrackTableName(tableName, sizeof(tableName), "mrna");
        /* display mRNA details page -- need to add dummy CGI variables*/
        dyStringPrintf(dy, "<A HREF=\"%s%cg=%s&i=%s&c=0&o=0&l=0&r=0",
		       hgcName(), hgAppCombiner, tableName, acc);
        }
    if (ui != NULL)
        dyStringPrintf(dy, "&%s", ui);
    dyStringPrintf(dy, "%s\">", 
               hgp->extraCgi);
    dyStringPrintf(dy, "%s</A>", acc);

    /* print description for item, or lacking that, the product name */
    sprintf(description, "n/a"); 
    sprintf(query, 
        "select description.name from gbCdnaInfo,description"
        " where gbCdnaInfo.acc = '%s' and gbCdnaInfo.description = description.id", acc);
    sqlQuickQuery(conn, query, description, sizeof(description));
    if (sameString(description, "n/a"))
        {
        /* look for product name */
        sprintf(query, 
            "select productName.name from gbCdnaInfo,productName"
            " where gbCdnaInfo.acc = '%s' and gbCdnaInfo.productName = productName.id",
                 acc);
        sqlQuickQuery(conn, query, product, sizeof(product));
        if (!sameString(product, "n/a"))
            {
            /* get organism name */
            sprintf(query, 
                "select organism.name from gbCdnaInfo,organism"
                " where gbCdnaInfo.acc = '%s' and gbCdnaInfo.organism = organism.id", acc);
            *organism = 0;
            sqlQuickQuery(conn, query, organism, sizeof(organism));
            safef(description, sizeof(description), "%s%s%s",
                    *organism ? organism : "",
                    *organism ? ", " : "",
                    product);
            }
        }
    if (!sameString(description, "n/a"))
        /* print description if it has been loaded */
        dyStringPrintf(dy, " - %s", description);
    dyStringPrintf(dy, "\n");
    pos->description = cloneString(dy->string);

    /* remove processed element from accession list */
    slRemoveEl(pAccList, el);
    elToFree = el;
    }

/* fill in table and add to hgp only if it contains results */
alignCount = slCount(table->posList);
if (alignCount > 0)
    {
    char *organism = hOrganism(hgp->database);      /* dbDb organism column */
    char title[256];
    slReverse(&table->posList);
    safef(title, sizeof(title), "%s%s %sligned mRNA Search Results",
			isXeno ? "Non-" : "", organism, 
			aligns ?  "A" : "Una");
    freeMem(organism);
    table->description = cloneString(title);
    table->name = isXeno ? cloneString("xenoMrna") : cloneString("mrna");
    table->htmlOnePos = mrnaKeysHtmlOnePos;
    slAddHead(&hgp->tableList, table);
    }
freeDyString(&dy);
return alignCount;
}

static boolean findMrnaKeys(struct hgFindSpec *hfs,
			    char *keys, struct hgPositions *hgp)
/* Find mRNA that has keyword in one of its fields. */
{
int alignCount;
static char *tables[] = {
	"productName", "geneName",
	"author", "tissue", "cell", "description", "development", 
	};
struct hash *allKeysHash = NULL;
struct slName *allKeysList = NULL;
struct sqlConnection *conn = hAllocConn();
boolean found = FALSE;

/* If we can use grep to search all tables, then use piped grep to 
 * implement implicit "AND" of multiple keys. */
if (gotAllGenbankGrepIndexFiles(hfs, tables, ArraySize(tables)))
    {
    findHitsToTables(hfs, keys, tables, ArraySize(tables),
		     &allKeysHash, &allKeysList);
    }
else
    {
    struct hash *oneKeyHash = NULL;
    struct slName *oneKeyList = NULL;
    struct hash *andedHash = NULL;
    struct slName *andedList = NULL;
    char *words[32];
    char buf[512];
    int wordCount;
    int i;
    strncpy(buf, keys, sizeof(buf));
    buf[sizeof(buf)-1] = 0;
    wordCount = chopLine(buf, words);
    if (wordCount == 0)
	return FALSE;
    found = TRUE;
    for (i=0; i<wordCount; ++i)
	{
	findHitsToTables(hfs, words[i], tables, ArraySize(tables),
			 &oneKeyHash, &oneKeyList);
	if (allKeysHash == NULL)
	    {
	    allKeysHash = oneKeyHash;
	    oneKeyHash = NULL;
	    allKeysList = oneKeyList;
	    oneKeyList = NULL;
	    }
	else
	    {
	    andHits(oneKeyHash, oneKeyList, allKeysHash, allKeysList,
		    &andedHash, &andedList);
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
    }
if (allKeysList == NULL)
    return FALSE;

/* generate position lists and add to hgp */
/* organism aligning */
alignCount = addMrnaPositionTable(hgp, &allKeysList, allKeysHash, cart, conn,
				  hgAppName, TRUE, FALSE);
/* organism non-aligning */
addMrnaPositionTable(hgp, &allKeysList, allKeysHash, cart, conn,
		     hgAppName, FALSE, FALSE);
/* xeno aligning */
/* NOTE: to suppress display of xeno mrna's in non-model organisms
 * (RT 801 and 687), uncommented the following...
 * add to display list only if there is a scarcity of items
 * already listed as aligning (low number of own mRna's for this organism) */
if (alignCount < 20)
    addMrnaPositionTable(hgp, &allKeysList, allKeysHash, cart, conn,
			 hgAppName, TRUE, TRUE);
hashFree(&allKeysHash);
hFreeConn(&conn);
return(found);
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

#if 0 /* not used */
static void findAffyProbe(char *spec, struct hgPositions *hgp)
/* Look up affy probes. */
{
}
#endif

int findKgGenesByAlias(char *spec, struct hgPositions *hgp)
/* Look up Known Genes using the gene Alias table, kgAlias. */
{
struct sqlConnection *conn  = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr 	    = NULL;
struct dyString *ds 	    = newDyString(256);
char **row;
struct hgPosTable *table    = NULL;
struct hgPos *pos;
struct genePred *gp;
char *answer, cond_str[256];
char *desc;
struct kgAlias *kaList 	    = NULL, *kl;
boolean gotKgAlias 	    = sqlTableExists(conn, "kgAlias");
int kgFound 		    = 0;

if (gotKgAlias)
    {
    /* get a linked list of kgAlias (kgID/alias pair) nodes that match 
     * the spec using "Fuzzy" mode*/
    kaList = findKGAlias(hGetDb(), spec, "P");
    }

if (kaList != NULL)
    {
    struct hash *hash = newHash(8);
    kgFound = 1;
    AllocVar(table);
    slAddHead(&hgp->tableList, table);
    table->description = cloneString("Known Genes");
    table->name = cloneString("knownGene");
    
    for (kl = kaList; kl != NULL; kl = kl->next)
        {
        /* Don't return duplicate mrna accessions */
        if (hashFindVal(hash, kl->kgID))
            {            
            hashAdd(hash, kl->kgID, kl);
            continue;
            }

        hashAdd(hash, kl->kgID, kl);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from knownGene where name = '%s'",
		       kl->kgID);
	sr = sqlGetResult(conn, ds->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    gp = genePredLoad(row);
	    AllocVar(pos);
	    slAddHead(&table->posList, pos);
	    pos->name = cloneString(kl->alias);

/* 	    pos->browserName = cloneString(kl->alias); highlight change */
	    pos->browserName = cloneString(kl->kgID);
	    sprintf(cond_str, "kgID = '%s'", kl->kgID);
	    answer = sqlGetField(conn2, hGetDb(), "kgXref", "description",
				 cond_str);
	    if (answer != NULL) 
		{
		desc = answer;
		}
	    else
		{
		desc = kl->alias;
		}

	    dyStringClear(ds);
	    dyStringPrintf(ds, "(%s) %s", kl->kgID, desc);
	    pos->description = cloneString(ds->string);
	    pos->chrom = hgOfficialChromName(gp->chrom);
	    pos->chromStart = gp->txStart;
	    pos->chromEnd = gp->txEnd;
	    genePredFree(&gp);
	    }
	sqlFreeResult(&sr);
	}
    kgAliasFreeList(&kaList);
    freeHash(&hash);
    }
freeDyString(&ds);
hFreeConn(&conn);
hFreeConn(&conn2);
return(kgFound);
}

int findKgGenesByProtAlias(char *spec, struct hgPositions *hgp)
/* Look up Known Genes using the protein alias table, kgProtAlias. */
{
struct sqlConnection *conn  = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr 	    = NULL;
struct dyString *ds 	    = newDyString(256);
char **row;
struct hgPosTable *table    = NULL;
struct hgPos *pos;
struct genePred *gp;
char *answer, cond_str[256];
char *desc;
struct kgProtAlias *kpaList = NULL, *kl;
boolean gotKgProtAlias 	    = sqlTableExists(conn, "kgProtAlias");
int kgFound 		    = 0;

if (gotKgProtAlias)
    {
    /* get a link list of kgProtAlias (kgID, displayID, and alias) nodes that 
       match the query spec using "Fuzzy" search mode*/
    kpaList = findKGProtAlias(hGetDb(), spec, "P");
    }

if (kpaList != NULL)
    {
    struct hash *hash = newHash(8);
    kgFound = 1;
    AllocVar(table);
    slAddHead(&hgp->tableList, table);
    table->description = cloneString("Known Genes");
    table->name = cloneString("knownGene");
    
    for (kl = kpaList; kl != NULL; kl = kl->next)
        {
        /* Don't return duplicate mrna accessions */
        if (hashFindVal(hash, kl->kgID))
            {            
            hashAdd(hash, kl->kgID, kl);
            continue;
            }

        hashAdd(hash, kl->kgID, kl);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from knownGene where name = '%s'",
		       kl->kgID);
	sr = sqlGetResult(conn, ds->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    gp = genePredLoad(row);
	    AllocVar(pos);
	    slAddHead(&table->posList, pos);
	    pos->name = cloneString(kl->alias);
/* 	    pos->browserName = cloneString(kl->alias); highlight change */
	    pos->browserName = cloneString(kl->kgID);

	    sprintf(cond_str, "kgID = '%s'", kl->kgID);
	    answer = sqlGetField(conn2, hGetDb(), "kgXref", "description",
				 cond_str);
	    if (answer != NULL) 
		{
		desc = answer;
		}
	    else
		{
		desc = kl->alias;
		}

	    dyStringClear(ds);
	    dyStringPrintf(ds, "(%s) %s", kl->displayID, desc);
	    pos->description = cloneString(ds->string);
	    pos->chrom = hgOfficialChromName(gp->chrom);
	    pos->chromStart = gp->txStart;
	    pos->chromEnd = gp->txEnd;
	    genePredFree(&gp);
	    }
	sqlFreeResult(&sr);
	}
    kgProtAliasFreeList(&kpaList);
    freeHash(&hash);
    }
freeDyString(&ds);
hFreeConn(&conn);
hFreeConn(&conn2);
return(kgFound);
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

static void addRefLinkAccs(struct sqlConnection *conn, struct slName *accList,
			   struct refLink **pList)
/* Query database and add returned refLinks to head of list. */
{
struct slName *accEl = NULL;
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];

for (accEl = accList;  accEl != NULL;  accEl = accEl->next)
    {
    safef(query, sizeof(query), "select * from refLink where mrnaAcc = '%s'",
	  accEl->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct refLink *rl = refLinkLoad(row);
	slAddHead(pList, rl);
	}
    sqlFreeResult(&sr);
    }
}

static boolean findRefGenes(struct hgFindSpec *hfs, char *spec,
			    struct hgPositions *hgp)
/* Look up refSeq genes in table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *ds = newDyString(256);
char **row;
struct refLink *rlList = NULL, *rl;
boolean gotRefLink = hTableExists("refLink");
boolean found = FALSE;

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
	dyStringPrintf(ds, "select * from refLink where locusLinkId = %s",
		       spec);
	addRefLinks(conn, ds, &rlList);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from refLink where omimId = %s", spec);
	addRefLinks(conn, ds, &rlList);
	}
    else 
	{
	char *indexFile = getGenbankGrepIndex(hfs, "refLink", "mrnaAccProduct");
	dyStringPrintf(ds, "select * from refLink where name like '%s%%'",
		       spec);
	addRefLinks(conn, ds, &rlList);
	if (indexFile != NULL)
	    {
	    struct slName *accList = doGrepQuery(indexFile, "refLink", spec,
						 NULL);
	    addRefLinkAccs(conn, accList, &rlList);
	    }
	else
	    {
	    dyStringClear(ds);
	    dyStringPrintf(ds, "select * from refLink where product like '%%%s%%'",
			   spec);
	    addRefLinks(conn, ds, &rlList);
	    }
	}
    }
if (rlList != NULL)
    {
    struct hgPosTable *table = NULL;
    struct hash *hash = newHash(8);
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
	dyStringPrintf(ds, "select * from %s where name = '%s'",
		       hfs->searchTable, rl->mrnaAcc);
	sr = sqlGetResult(conn, ds->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    struct genePred *gp = genePredLoad(row);
	    struct hgPos *pos = NULL;
	    AllocVar(pos);
	    if (table == NULL)
		{
		char desc[256];
		AllocVar(table);
		table->name = cloneString(hfs->searchTable);
		if (startsWith("xeno", hfs->searchTable))
		    safef(desc, sizeof(desc), "Non-%s RefSeq Genes",
			  hOrganism(hGetDb()));
		else
		    safef(desc, sizeof(desc), "RefSeq Genes");
		table->description = cloneString(desc);
		slAddHead(&hgp->tableList, table);
		}
	    slAddHead(&table->posList, pos);
	    pos->name = cloneString(rl->name);
	    pos->browserName = cloneString(rl->mrnaAcc);
	    dyStringClear(ds);
	    dyStringPrintf(ds, "(%s) %s", rl->mrnaAcc, rl->product);
	    pos->description = cloneString(ds->string);
	    pos->chrom = hgOfficialChromName(gp->chrom);
	    pos->chromStart = gp->txStart;
	    pos->chromEnd = gp->txEnd;
	    genePredFree(&gp);
	    found = TRUE;
	    }
	sqlFreeResult(&sr);
	}
    refLinkFreeList(&rlList);
    freeHash(&hash);
    }
freeDyString(&ds);
hFreeConn(&conn);
return(found);
}

/* Lowe lab additions */

static void addTigrCmrGenes(struct sqlConnection *conn, struct dyString *query,
	struct tigrCmrGene **pList)
/* Query database and add returned tigrCmrGenes to head of list. */
{
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct tigrCmrGene *rl = tigrCmrGeneLoad(row);
    slAddHead(pList, rl);
    }
sqlFreeResult(&sr);
}

static void findTigrGenes(char *spec, struct hgPositions *hgp)
/* Look up TIGR and Genbank genes from keyword */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *ds = newDyString(256);
char **row;
struct hgPosTable *table = NULL;
struct hgPos *pos;
struct bed *bed;
struct tigrCmrGene *tigrList = NULL, *tigr;
/* struct minGeneInfo *gbList = NULL, *gb; */
boolean gotTIGRkeys = sqlTableExists(conn, "tigrCmrORFsInfo");

if (gotTIGRkeys)
    {
    dyStringPrintf(ds, "select * from tigrCmrORFsInfo where tigrCommon like '%%%s%%'", spec);
    addTigrCmrGenes(conn, ds, &tigrList);
    dyStringClear(ds);
    dyStringPrintf(ds, "select * from tigrCmrORFsInfo where tigrMainRole like '%%%s%%'", spec);
    addTigrCmrGenes(conn, ds, &tigrList);
    dyStringClear(ds);
    dyStringPrintf(ds, "select * from tigrCmrORFsInfo where tigrSubRole like '%%%s%%'", spec);
    addTigrCmrGenes(conn, ds, &tigrList);
    dyStringClear(ds);
    }
if (tigrList != NULL)
    {
    struct hash *hash = newHash(8);
    AllocVar(table);
    slAddHead(&hgp->tableList, table);
    table->description = cloneString("TIGR CMR Genes");
    table->name = cloneString("tigrORFsCmr");
    for (tigr = tigrList; tigr != NULL; tigr = tigr->next)
        {
        /* Don't return duplicate TIGR CMR accessions */
        if (hashFindVal(hash, tigr->name))
            {
            hashAdd(hash, tigr->name, tigr);
            continue;
            }
        hashAdd(hash, tigr->name, tigr);
	dyStringClear(ds);
	dyStringPrintf(ds, "select * from tigrCmrORFs where name = '%s'", tigr->name);
	sr = sqlGetResult(conn, ds->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    bed = bedLoadN(row+1,6);
	    AllocVar(pos);
	    slAddHead(&table->posList, pos);
	    pos->name = cloneString(tigr->name);
	    pos->browserName = cloneString(tigr->name);
	    dyStringClear(ds);
	    dyStringPrintf(ds, "%s; %s; %s", tigr->tigrCommon, tigr->tigrMainRole, tigr->tigrSubRole);
	    pos->description = cloneString(ds->string);
	    pos->chrom = hgOfficialChromName(bed->chrom);
	    pos->chromStart = bed->chromStart;
	    pos->chromEnd = bed->chromEnd;
	    bedFree(&bed);
	    }
	sqlFreeResult(&sr);
	}
    tigrCmrGeneFreeList(&tigrList);
    freeHash(&hash);
    }
freeDyString(&ds);
hFreeConn(&conn);
}

/* End of Lowe Lab stuff */

static boolean findGenePredPattern(char *pattern, struct hgPositions *hgp,
				   char *tableName, struct hgPosTable *table)
/* Look for position pattern in gene prediction table. */
{
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
struct dyString *query;
char **row;
boolean ok = FALSE;
struct hgPos *pos = NULL;
int rowOffset;
char *localName;

localName = pattern;
if (!hTableExists(tableName))
    return FALSE;
rowOffset = hOffsetPastBin(NULL, tableName);
conn = hAllocConn();
query = newDyString(256);
dyStringPrintf(query,
	      "SELECT chrom, txStart, txEnd, name FROM %s WHERE name LIKE '%s'",
	      tableName, pattern);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ok == FALSE)
        {
	ok = TRUE;
	if (table == NULL)
	    {
	    AllocVar(table);
	    dyStringClear(query);
	    dyStringPrintf(query, "%s Gene Predictions", tableName);
	    table->description = cloneString(query->string);
	    table->name = cloneString(tableName);
	    slAddHead(&hgp->tableList, table);
	    }
	}
    
    AllocVar(pos);
    pos->chrom = hgOfficialChromName(row[0]);
    pos->chromStart = atoi(row[1]);
    pos->chromEnd = atoi(row[2]);
    pos->name = cloneString(row[3]);
    pos->browserName = cloneString(row[3]);
    slAddHead(&table->posList, pos);
    }
if (table != NULL)
    slReverse(&table->posList);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return ok;
}

static void addUniqYeastGene(struct hash *uniqHash, 
	struct sqlConnection *conn, char *query, 
	struct hgPositions *hgp, char *geneTable,
	struct hgPosTable **pTable)
/* Execute query which returns a single row, and add genes. */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
struct hgPosTable *table = *pTable;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *id = row[0];
    if (!hashLookup(uniqHash, id))
	{
	hashAdd(uniqHash, id, NULL);
	if (table == NULL)
	    {
	    AllocVar(table);
	    table->name = geneTable;
	    table->description = "Genes from Sacchromyces Genome Database";
	    slAddHead(&hgp->tableList, table);
	    *pTable = table;
	    }
	findGenePredPattern(id, hgp, geneTable, table);
	}
    }
sqlFreeResult(&sr);
}

static boolean findYeastGenes(char *pattern, struct hgPositions *hgp)
/* Scan yeast-specific tables. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row, query[256];
struct hgPosTable *table = NULL;
boolean found = FALSE;

if (hTableExists("sgdGene"))
    {
    struct hash *uniqHash = newHash(0);
    boolean gotNames = FALSE, gotDescriptions = FALSE;
    safef(query, sizeof(query), 
        "select name from sgdGene where name = '%s'", pattern);
    addUniqYeastGene(uniqHash, conn, query, hgp, "sgdGene", &table);
    if (hTableExists("sgdToName"))
	{
	gotNames = TRUE;
	safef(query, sizeof(query), 
	    "select name from sgdToName where value like '%s%%'", pattern);
	addUniqYeastGene(uniqHash, conn, query, hgp, "sgdGene", &table);
	}
    if (hTableExists("sgdDescription"))
        {
	gotDescriptions = TRUE;
	safef(query, sizeof(query), 
	    "select name from sgdDescription where description like '%%%s%%'", 
	    pattern);
	addUniqYeastGene(uniqHash, conn, query, hgp, "sgdGene", &table);
	}
    hashFree(&uniqHash);

    /* Add descriptions to table. */
    if (table != NULL)
        {
	struct hgPos *pos;
	for (pos = table->posList; pos != NULL; pos = pos->next)
	    {
	    struct dyString *dy = newDyString(1024);
	    if (gotNames)
		{
		safef(query, sizeof(query),
		   "select value from sgdToName where name = '%s'", pos->name);
	        sr = sqlGetResult(conn, query);
		while ((row = sqlNextRow(sr)) != NULL)
		    dyStringPrintf(dy, "(%s) ", row[0]);
		sqlFreeResult(&sr);
		}
	    if (gotDescriptions)
		{
		safef(query, sizeof(query),
		   "select description from sgdDescription where name = '%s'", 
		   pos->name);
	        sr = sqlGetResult(conn, query);
		if ((row = sqlNextRow(sr)) != NULL)
		    dyStringPrintf(dy, "%s", row[0]);
		sqlFreeResult(&sr);
		}
	    if (dy->stringSize > 0)
		pos->description = cloneString(dy->string);
	    dyStringFree(&dy);
	    }
	found = TRUE;
	}
    }
hFreeConn(&conn);
return(found);
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
char hgAppCombiner = (strchr(hgAppName, '?')) ? '&' : '?';

if (useWeb)
    webStart(cart, "Select Position");

for (table = hgp->tableList; table != NULL; table = table->next)
    {
    char *vis = hTrackOpenVis(table->name);
    if (table->posList != NULL)
	{
	boolean excludeTable = FALSE;
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
		char *matches = excludeTable ? "" : pos->browserName;
		char *encMatches = cgiEncode(matches);
		hgPosBrowserRange(pos, range);
		fprintf(f, "<A HREF=\"%s%cposition=%s",
			hgAppName, hgAppCombiner, range);
		if (ui != NULL)
		    fprintf(f, "&%s", ui);
		fprintf(f, "%s&%s=%s&hgFind.matches=%s,\">",
			extraCgi, table->name, vis, encMatches);
		htmTextOut(f, pos->name);
		fprintf(f, " at %s</A>", range);
		desc = pos->description;
		if (desc)
		    {
		    fprintf(f, " - ");
		    htmTextOut(f, desc);
		    }
		fprintf(f, "\n");
		freeMem(encMatches);
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


static struct hgPositions *genomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart, boolean showAlias,
	boolean useWeb, char *hgAppName)
/* Search for positions in genome that match user query.   
 * Return an hgp if the query results in a unique position.  
 * Otherwise display list of positions, put # of positions in retWinStart,
 * and return NULL. */
{ 
struct hgPositions *hgp = NULL;
char *terms[16];
int termCount = 0;
int i = 0;
boolean multiTerm = FALSE;
char *chrom = NULL;
int start = BIGNUM;
int end = 0;

termCount = chopByChar(cloneString(spec), ';', terms, ArraySize(terms));
multiTerm = (termCount > 1);

for (i = 0;  i < termCount;  i++)
    {
    if (isEmpty(terms[i]))
	continue;
    hgp = hgPositionsFind(terms[i], "", hgAppName, cart, multiTerm);
    if (hgp == NULL || hgp->posCount == 0)
	{
	hgPositionsFree(&hgp);
	warn("Sorry, couldn't locate %s in genome database\n", terms[i]);
	if (multiTerm)
	    errAbort("%s not uniquely determined -- "
		     "can't do multi-position search.", terms[i]);
	*retWinStart = 0;
	return NULL;
	}
    
    if ((hgp->singlePos != NULL) && (!showAlias || !hgp->useAlias))
	{
	if (chrom != NULL && !sameString(chrom, hgp->singlePos->chrom))
	    errAbort("Sites occur on different chromosomes: %s, %s.",
		     chrom, hgp->singlePos->chrom);
	chrom = hgp->singlePos->chrom;
	if (hgp->singlePos->chromStart < start)
	    start = hgp->singlePos->chromStart;
	if (hgp->singlePos->chromEnd > end)
	    end = hgp->singlePos->chromEnd;
	}
    else
	{
	hgPositionsHtml(hgp, stdout, useWeb, hgAppName, cart);
	if (multiTerm && hgp->posCount != 1)
	    errAbort("%s not uniquely determined (%d locations) -- "
		     "can't do multi-position search.",
		     terms[i], hgp->posCount);
	*retWinStart = hgp->posCount;
	hgp = NULL;
	break;
	}
    }
if (hgp != NULL)
    {
    *retChromName = chrom;
    *retWinStart  = start;
    *retWinEnd    = end;
    }
return hgp;
}


struct hgPositions *findGenomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart)
/* Search for positions in genome that match user query.   
 * Return an hgp if the query results in a unique position.  
 * Otherwise display list of positions, put # of positions in retWinStart,
 * and return NULL. */
{
return genomePos(spec, retChromName, retWinStart, retWinEnd, cart, TRUE,
		 FALSE, "hgTracks");
}

struct hgPositions *findGenomePosWeb(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, struct cart *cart,
	boolean useWeb, char *hgAppName)
/* Search for positions in genome that match user query.   
 * Use the web library to print out HTML headers if necessary, and use 
 * hgAppName when forming URLs (instead of "hgTracks").  
 * Return an hgp if the query results in a unique position.  
 * Otherwise display list of positions, put # of positions in retWinStart,
 * and return NULL. */
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

#if 0 /* not used */
static void noRelative(boolean relativeFlag, int relStart, int relEnd,
		       char *table)
{
if (relativeFlag)
    errAbort("Sorry, range spec (\":%d-%d\") is not supported for %s.",
	     relStart+1, relEnd, table);

}
#endif

static boolean searchSpecial(struct hgFindSpec *hfs, char *term,
			     struct hgPositions *hgp, boolean relativeFlag,
			     int relStart, int relEnd, boolean *retFound)
/* Handle searchTypes for which we have special code.  Return true if 
 * we have special code.  Set retFind according to whether we find term. */
{
boolean isSpecial = TRUE;
boolean found = FALSE;
char *upcTerm = cloneString(term);
touppers(upcTerm);
if (sameString(hfs->searchType, "knownGene"))
    {
    if (gotFullText())
	found = findKnownGeneFullText(term, hgp);
    else	/* NOTE, in a few months (say by April 1 2006) get rid of else -JK */
	{
	if (!found && hTableExists("kgAlias"))
	    found = findKgGenesByAlias(term, hgp);
	if (!found && hTableExists("kgProtAlias"))
	    found = findKgGenesByProtAlias(term, hgp);
	if (!found)
	    found = findKnownGene(term, hgp, hfs->searchTable);
	}
    }
else if (sameString(hfs->searchType, "refGene"))
    {
    found = findRefGenes(hfs, term, hgp);
    }
else if (sameString(hfs->searchType, "cytoBand"))
    {
    char *chrom;
    int start, end;
    found = hgFindCytoBand(term, &chrom, &start, &end);
    if (found)
	singlePos(hgp, hfs->searchDescription, NULL, hfs->searchTable, term,
		  term, chrom, start, end);
    }
else if (sameString(hfs->searchType, "gold"))
    {
    char *chrom;
    int start, end;
    found = findChromContigPos(term, &chrom, &start, &end);
    if (found)
	{
	if (relativeFlag)
	    {
	    end = start + relEnd;
	    start = start + relStart;
	    }
	singlePos(hgp, hfs->searchDescription, NULL, hfs->searchTable, term,
		  term, chrom, start, end);
	}
    }
else if (sameString(hfs->searchType, "mrnaAcc"))
    {
    found = findMrnaPos(term, hgp);
    }
else if (sameString(hfs->searchType, "mrnaKeyword"))
    {
    found = findMrnaKeys(hfs, upcTerm, hgp);
    }
else if (sameString(hfs->searchType, "sgdGene"))
    {
    found = findYeastGenes(term, hgp);
    }
else
    {
    isSpecial = FALSE;
    }
*retFound = found;
freeMem(upcTerm);
return(isSpecial);
}


static struct slPair *getXrefTerms(struct hgFindSpec *hfs, char *term)
/* Search xrefTable for xrefQuery with term.  Return all matching names. */
{
struct slPair *xrefList = NULL, *xrefPtr = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
char buf[512];
boolean isFuzzy = sameWord(hfs->searchMethod, "fuzzy");

safef(buf, sizeof(buf), hfs->xrefQuery, hfs->xrefTable, term);
sr = sqlGetResult(conn, buf);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!isFuzzy || keyIsPrefixIgnoreCase(term, row[1]))
        {
	xrefPtr = slPairNew(cloneString(row[1]), cloneString(row[0]));
	slAddHead(&xrefList, xrefPtr);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&xrefList);
if (xrefList == NULL && hgFindSpecSetting(hfs, "searchBoth") != NULL)
    xrefList = slPairNew(cloneString(""), cloneString(term));
return(xrefList);
}


int vatruncatef(char *buf, int size, char *format, va_list args)
/* Like vasafef, but truncates the formatted string instead of barfing on 
 * overflow. */
{
char *truncStr = " [truncated]";
int sz = vsnprintf(buf, size, format, args);
/* note that some version return -1 if too small */
if ((sz < 0) || (sz >= size))
    strncpy(buf + size - 1 - strlen(truncStr), truncStr, strlen(truncStr));
buf[size-1] = 0;
return sz;
}

void truncatef(char *buf, int size, char *format, ...)
/* Like safef, but truncates the formatted string instead of barfing on 
 * overflow. */
{
int sz;
va_list args;
va_start(args, format);
sz = vatruncatef(buf, size, format, args);
va_end(args);
}

static boolean doQuery(struct hgFindSpec *hfs, char *xrefTerm, char *term,
		       struct hgPositions *hgp,
		       boolean relativeFlag, int relStart, int relEnd,
		       boolean multiTerm)
/* Perform a query as specified in hfs, assuming table existence has been 
 * checked and xref'ing has been taken care of. */
{
struct slName *tableList = hSplitTableNames(hfs->searchTable);
struct slName *tPtr = NULL;
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
char *termPrefix = hgFindSpecSetting(hfs, "termPrefix");
char *paddingStr = hgFindSpecSetting(hfs, "padding");
int padding = isEmpty(paddingStr) ? 0 : atoi(paddingStr);
boolean found = FALSE;
char *description = NULL;
char buf[2048];

if (isNotEmpty(termPrefix) && startsWith(termPrefix, term))
    term += strlen(termPrefix);
if (isEmpty(term))
    return(FALSE);

if (isNotEmpty(hfs->searchDescription))
    truncatef(buf, sizeof(buf), hfs->searchDescription);
else
    safef(buf, sizeof(buf), "%s", hfs->searchTable);
description = cloneString(buf);

if (hgp->tableList != NULL &&
    sameString(hgp->tableList->name, hfs->searchTable) &&
    sameString(hgp->tableList->description, description))
    table = hgp->tableList;

for (tPtr = tableList;  tPtr != NULL;  tPtr = tPtr->next)
    {
    safef(buf, sizeof(buf), hfs->query, tPtr->name, term);
    sr = sqlGetResult(conn, buf);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if(table == NULL)
	    {
	    AllocVar(table);
	    table->description = description;
	    table->name = cloneString(hfs->searchTable);
	    slAddHead(&hgp->tableList, table);
	    }
	found = TRUE;
	AllocVar(pos);
	pos->chrom = cloneString(row[0]);
	pos->chromStart = atoi(row[1]);
	pos->chromEnd = atoi(row[2]);
	if (isNotEmpty(xrefTerm))
	    truncatef(buf, sizeof(buf), xrefTerm);
	else
	    safef(buf, sizeof(buf), "%s%s",
		  termPrefix ? termPrefix : "", row[3]);
	pos->name = cloneString(buf);
	pos->browserName = cloneString(row[3]);
	if (isNotEmpty(xrefTerm))
	    {
	    safef(buf, sizeof(buf), "(%s%s)",
		  termPrefix ? termPrefix : "", row[3]);
	    pos->description = cloneString(buf);
	    }
	if (relativeFlag && (pos->chromStart + relEnd) <= pos->chromEnd)
	    {
	    pos->chromEnd   = pos->chromStart + relEnd;
	    pos->chromStart = pos->chromStart + relStart;
	    }
	else if (padding > 0 && !multiTerm)
	    {
	    int chromSize = hChromSize(pos->chrom);
	    pos->chromStart -= padding;
	    pos->chromEnd   += padding;
	    if (pos->chromStart < 0)
		pos->chromStart = 0;
	    if (pos->chromEnd > chromSize)
		pos->chromEnd = chromSize;
	    }
	slAddHead(&table->posList, pos);
	}

    }
if (table != NULL)
    slReverse(&table->posList);
sqlFreeResult(&sr);
hFreeConn(&conn);
slFreeList(&tableList);
return(found);
}

boolean hgFindUsingSpec(struct hgFindSpec *hfs, char *term,
			struct hgPositions *hgp, boolean relativeFlag,
			int relStart, int relEnd, boolean multiTerm)
/* Perform the search described by hfs on term.  If successful, put results
 * in hgp and return TRUE.  (If not, don't modify hgp.) */
{
struct slPair *xrefList = NULL, *xrefPtr = NULL; 
boolean found = FALSE;

if (hfs == NULL || term == NULL || hgp == NULL)
    errAbort("NULL passed to hgFindUsingSpec.\n");

if (strlen(term)<2 && !sameString(hfs->searchName, "knownGene")) return FALSE;

if (isNotEmpty(hfs->termRegex) && ! matchRegex(term, hfs->termRegex))
    return(FALSE);

if (! hTableOrSplitExists(hfs->searchTable))
    return(FALSE);

if (isNotEmpty(hfs->searchType) && searchSpecial(hfs, term, hgp, relativeFlag,
						 relStart, relEnd, &found))
    return(found);

if (isNotEmpty(hfs->xrefTable))
    {
    if (! hTableOrSplitExists(hfs->xrefTable))
	return(FALSE);
    xrefList = getXrefTerms(hfs, term);
    }
else
    xrefList = slPairNew(cloneString(""), cloneString(term));

for (xrefPtr = xrefList;  xrefPtr != NULL;  xrefPtr = xrefPtr->next)
    {
    found |= doQuery(hfs, xrefPtr->name, (char *)xrefPtr->val, hgp,
		     relativeFlag, relStart, relEnd, multiTerm);
    }
slPairFreeValsAndList(&xrefList);
return(found);
}


/* Support these formats for range specifiers.  Note the ()'s around chrom,
 * start and end portions for substring retrieval: */
char *canonicalRangeExp = 
		     "^([[:alnum:]._\\-]+)"
		     "[[:space:]]*:[[:space:]]*"
		     "([0-9,]+)"
		     "[[:space:]]*-[[:space:]]*"
		     "([0-9,]+)$";
char *gbrowserRangeExp = 
		     "^([[:alnum:]._\\-]+)"
		     "[[:space:]]*:[[:space:]]*"
		     "([0-9,]+)"
		     "[[:space:]]*\\.\\.[[:space:]]*"
		     "([0-9,]+)$";
char *lengthRangeExp = 
		     "^([[:alnum:]._\\-]+)"
		     "[[:space:]]*:[[:space:]]*"
		     "([0-9,]+)"
		     //"[[:space:]]*\\^[[:space:]]*"
		     "[[:space:]]*\\+[[:space:]]*"
		     "([0-9,]+)$";
char *bedRangeExp = 
		     "^([[:alnum:]._\\-]+)"
		     "[[:space:]]+"
		     "([0-9,]+)"
		     "[[:space:]]+"
		     "([0-9,]+)$";
char *sqlRangeExp = 
		     "^([[:alnum:]._\\-]+)"
		     "[[:space:]]*\\|[[:space:]]*"
		     "([0-9,]+)"
		     "[[:space:]]*\\|[[:space:]]*"
		     "([0-9,]+)$";

struct hgPositions *hgPositionsFind(char *term, char *extraCgi,
	char *hgAppNameIn, struct cart *cart, boolean multiTerm)
/* Return table of positions that match term or NULL if none such. */
{
struct hgPositions *hgp = NULL, *hgpItem = NULL;
regmatch_t substrs[4];
boolean canonicalSpec = FALSE;
boolean gbrowserSpec = FALSE;
boolean lengthSpec = FALSE;
boolean relativeFlag = FALSE;
int relStart = 0, relEnd = 0;

hgAppName = hgAppNameIn;


AllocVar(hgp);
hgp->useAlias = FALSE;
term = trimSpaces(term);
if(term == 0)
    return hgp;

hgp->query = cloneString(term);
hgp->database = hGetDb();
if (extraCgi == NULL)
    extraCgi = "";
hgp->extraCgi = cloneString(extraCgi);

/* Allow any search term to end with a :Start-End range -- also support stuff 
 * pasted in from BED (chrom start end) or SQL query (chrom | start | end).  
 * If found, strip it off and remember the start and end. */
if ((canonicalSpec = 
        matchRegexSubstr(term, canonicalRangeExp,
				  substrs, ArraySize(substrs))) ||
    (gbrowserSpec = 
        matchRegexSubstr(term, gbrowserRangeExp, 
                                substrs, ArraySize(substrs))) ||
    (lengthSpec = 
        matchRegexSubstr(term, lengthRangeExp, 
                                substrs, ArraySize(substrs))) ||
    matchRegexSubstr(term, bedRangeExp, substrs, ArraySize(substrs)) ||
    matchRegexSubstr(term, sqlRangeExp, substrs, ArraySize(substrs)))
    {
    /* Since we got a match, substrs[1] is the chrom/term, [2] is relStart, 
     * [3] is relEnd. ([0] is all.) */
    term[substrs[1].rm_eo] = 0;
    eraseTrailingSpaces(term);
    term[substrs[2].rm_eo] = 0;
    relStart = atoi(stripCommas(term+substrs[2].rm_so));
    term[substrs[3].rm_eo] = 0;
    relEnd   = atoi(stripCommas(term+substrs[3].rm_so));
    if (canonicalSpec || gbrowserSpec || lengthSpec)
	relStart--;
    if (lengthSpec)
        relEnd += relStart;
    if (relStart > relEnd)
	{
	int tmp  = relStart;
	relStart = relEnd;
	relEnd   = tmp;
	}
    relativeFlag = TRUE;
    }
term = sqlEscapeString(term);

if (hgOfficialChromName(term) != NULL)
    {
    char *chrom;
    int start, end;
    hgParseChromRange(term, &chrom, &start, &end);
    if (relativeFlag)
	{
	int chromSize = end;
	end = start + relEnd;
	start = start + relStart;
	if (end > chromSize)
	    end = chromSize;
	if (start < 0)
	    start = 0;
	}
    singlePos(hgp, "Chromosome Range", NULL, "chromInfo", term,
	      "", chrom, start, end);
    }
else
    {
    struct hgFindSpec *shortList = NULL, *longList = NULL;
    struct hgFindSpec *hfs;
    boolean done = FALSE;

    hgFindSpecGetAllSpecs(&shortList, &longList);
    for (hfs = shortList;  hfs != NULL;  hfs = hfs->next)
	{
	if (hgFindUsingSpec(hfs, term, hgp, relativeFlag, relStart, relEnd,
			    multiTerm))
	    {
	    done = TRUE;
	    break;
	    }
	}
    if (! done)
	{
	for (hfs = longList;  hfs != NULL;  hfs = hfs->next)
	    {
	    hgFindUsingSpec(hfs, term, hgp, relativeFlag, relStart, relEnd,
			    multiTerm);
	    }
	/* Lowe lab additions -- would like to replace these with specs, but 
	 * will leave in for now. */
	findTigrGenes(term, hgp);
	}
    hgFindSpecFreeList(&shortList);
    hgFindSpecFreeList(&longList);
    if(hgpMatchNames == NULL)
	hgpMatchNames = newDyString(256);
    for(hgpItem = hgp; hgpItem != NULL; hgpItem = hgpItem->next)
	{
	struct hgPosTable *hpTable = NULL;
	for(hpTable = hgpItem->tableList; hpTable != NULL; hpTable = hpTable->next)
	    {
	    struct hgPos *pos = NULL;
	    for(pos = hpTable->posList; pos != NULL; pos = pos->next)
		{
		dyStringPrintf(hgpMatchNames, "%s,", pos->browserName);
		}
	    }
	}
    if (cart != NULL)
        cartSetString(cart, "hgFind.matches", hgpMatchNames->string);
    }
slReverse(&hgp->tableList);
fixSinglePos(hgp);
return hgp;
}


void hgPositionsHelpHtml(char *organism, char *database)
/* Display contents of dbDb.htmlPath for database, or print an HTML comment 
 * explaining what's missing. */
{
char *htmlPath = hHtmlPath(database);
char *htmlString = NULL;
size_t htmlStrLength = 0;

if (strstrNoCase(organism, "zoo")) 
    webNewSection("About the NISC Comparative Sequencing Program Browser");
else
    webNewSection("About the %s %s (%s) assembly"
		  "  <A HREF=\"%s?%s=%d&chromInfoPage=\">(sequences)</A>",
		  organism, hFreezeFromDb(database), database,
		  hgTracksName(), cartSessionVarName(), cartSessionId(cart));

if (htmlPath != NULL && fileExists(htmlPath))
    readInGulp(htmlPath, &htmlString, &htmlStrLength);

if (htmlStrLength > 0)
    {
    puts(htmlString);
    freeMem(htmlString);
    freeMem(htmlPath);
    }
else
    {
    printf("<H2>%s</H2>\n", organism);
    if (htmlPath == NULL || htmlPath[0] == 0)
	printf("\n<!-- No dbDb.htmlPath for %s -->\n", database);
    else
	printf("\n<!-- Couldn't get contents of %s -->\n", htmlPath);
   } 
}

