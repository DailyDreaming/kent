/* netClass - Add classification info to net. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "rbTree.h"
#include "jksql.h"
#include "hdb.h"
#include "localmem.h"
#include "agpGap.h"
#include "simpleRepeat.h"
#include "liftUp.h"
#include "chainNet.h"

static char const rcsid[] = "$Id: netClass.c,v 1.15 2003/11/20 17:31:33 angie Exp $";

char *tNewR = NULL;
char *qNewR = NULL;
boolean noAr = FALSE;
struct hash *liftHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netClass - Add classification info to net\n"
  "usage:\n"
  "   netClass in.net tDb qDb out.net\n"
  "options:\n"
  "   -tNewR=dir - Dir of chrN.out.spec files, with RepeatMasker .out format\n"
  "                lines describing lineage specific repeats in target\n"
  "   -qNewR=dir - Dir of chrN.out.spec files for query\n"
  "   -noAr - Don't look for ancient repeats\n"
  "   -liftQ=file.lft - Lift in.net's query coords to chrom-level using\n"
  "                     file.lft (for accessing chrom-level coords in qDb)\n"
  );
}

struct chrom
/* Basic information on a chromosome. */
    {
    struct chrom *next;	  /* Next in list */
    char *name;		  /* Chromosome name, allocated in hash. */
    int size;		  /* Chromosome size. */
    struct rbTree *nGaps; /* Gaps in sequence (Ns) */
    struct rbTree *repeats; /* Repeats in sequence */
    struct rbTree *newRepeats; /* New (lineage specific) repeats. */
    struct rbTree *oldRepeats; /* Old (pre-split) repeats. */
    struct rbTree *trf;	       /* Simple repeats. */
    };

struct range
/* A part of a chromosome. */
    {
    int start, end;	/* Half open zero based coordinates. */
    };

int rangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct range *a = va;
struct range *b = vb;
if (a->end <= b->start)
    return -1;
else if (b->end <= a->start)
    return 1;
else
    return 0;
}

static int interSize;	/* Size of intersection. */
static struct range interRange; /* Range to intersect with. */

void addInterSize(void *item)
/* Add range to interSize. */
{
struct range *r = item;
int size;
size = rangeIntersection(r->start, r->end, interRange.start, interRange.end);
interSize += size;
}

int intersectionSize(struct rbTree *tree, int start, int end)
/* Return total size of things intersecting range start-end. */
{
interRange.start = start;
interRange.end = end;
interSize = 0;
rbTreeTraverseRange(tree, &interRange, &interRange, addInterSize);
return interSize;
}

struct rbTree *getSeqGaps(char *db, char *chrom)
/* Return a tree of ranges for sequence gaps in chromosome */
{
struct sqlConnection *conn = sqlConnect(db);
struct rbTree *tree = rbTreeNew(rangeCmp);
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, "gap", chrom, NULL, &rowOffset);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct agpGap gap;
    struct range *range;
    agpGapStaticLoad(row+rowOffset, &gap);
    lmAllocVar(tree->lm, range);
    range->start = gap.chromStart;
    range->end = gap.chromEnd;
    rbTreeAdd(tree, range);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return tree;
}

struct rbTree *getTrf(char *db, char *chrom)
/* Return a tree of ranges for simple repeats in chromosome. */
{
struct rbTree *tree = rbTreeNew(rangeCmp);
struct range *range;
struct sqlConnection *conn = sqlConnect(db);
char query[256];
struct sqlResult *sr;
char **row;

sprintf(query, "select chromStart,chromEnd from simpleRepeat "
               "where chrom = '%s'",
	       chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    lmAllocVar(tree->lm, range);
    range->start = sqlUnsigned(row[0]);
    range->end = sqlUnsigned(row[1]);
    rbTreeAdd(tree, range);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return tree;
}

void getRepeats(char *db, struct hash *arHash, char *chrom,
	struct rbTree **retAllRepeats,  struct rbTree **retNewRepeats)
/* Return a tree of ranges for sequence gaps in chromosome */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
struct rbTree *allTree = rbTreeNew(rangeCmp);
struct rbTree *newTree = rbTreeNew(rangeCmp);
char tableName[64];
char query[256];

safef(tableName, sizeof(tableName), "%s_rmsk", chrom);
if (! sqlTableExists(conn, tableName))
    errAbort("Can't find rmsk table for %s (%s.%s)\n", chrom, db, tableName);
sprintf(query, "select genoStart,genoEnd,repName,repClass,repFamily from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct range *range;
    char arKey[512];
    lmAllocVar(allTree->lm, range);
    range->start = sqlUnsigned(row[0]);
    range->end = sqlUnsigned(row[1]);
    rbTreeAdd(allTree, range);
    sprintf(arKey, "%s.%s.%s", row[2], row[3], row[4]);
    if (arHash != NULL && hashLookup(arHash, arKey))
        {
	lmAllocVar(newTree->lm, range);
	range->start = sqlUnsigned(row[0]);
	range->end = sqlUnsigned(row[1]);
	rbTreeAdd(newTree, range);
	}
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
*retAllRepeats = allTree;
*retNewRepeats = newTree;
}

struct rbTree *getNewRepeats(char *dirName, char *chrom)
/* Read in repeatMasker .out line format file into a tree of ranges. */
/* Handles lineage-specific files that preserve header */
{
struct rbTree *tree = rbTreeNew(rangeCmp);
struct range *range;
char fileName[512];
struct lineFile *lf;
char *row[7];
boolean headerDone = FALSE;

sprintf(fileName, "%s/%s.out.spec", dirName, chrom);
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    /* skip header lines (don't contain numeric first field) */
    if (!headerDone && atoi(row[0]) == 0)
        continue;
    if (!sameString(chrom, row[4]))
        errAbort("Expecting %s word 5, line %d of %s\n", 
		chrom, lf->lineIx, lf->fileName);
    headerDone = TRUE;
    lmAllocVar(tree->lm, range);
    range->start = lineFileNeedNum(lf, row, 5) - 1;
    range->end = lineFileNeedNum(lf, row, 6);
    rbTreeAdd(tree, range);
    }
lineFileClose(&lf);
return tree;
}

boolean tableExists(char *db, char *table)
/* Return TRUE if table exists in database. */
{
struct sqlConnection *conn = sqlConnect(db);
boolean exists = sqlTableExists(conn, table);
sqlDisconnect(&conn);
return exists;
}

struct hash *getAncientRepeats(char *tDb, char *qDb)
/* Get hash of ancient repeats.  This keyed by name.family.class. */
{
char *db = NULL;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char key[512];
struct hash *hash = newHash(10);

if (tableExists(tDb, "ancientRepeat"))
    db = tDb;
else if (tableExists(qDb, "ancientRepeat"))
    db = qDb;
else
    errAbort("Can't find ancientRepeat table in %s or %s", tDb, qDb);
conn = sqlConnect(db);
sr = sqlGetResult(conn, "select name,family,class from ancientRepeat");
while ((row = sqlNextRow(sr)) != NULL)
    {
    sprintf(key, "%s.%s.%s", row[0], row[1], row[2]);
    hashAdd(hash, key, NULL);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return hash;
}

void getChroms(char *db, struct hash **retHash, struct chrom **retList)
/* Get hash of chromosomes from database. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
struct chrom *chromList = NULL, *chrom;
struct hash *hash = hashNew(8);

sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(chrom);
    hashAddSaveName(hash, row[0], chrom, &chrom->name);
    chrom->size = atoi(row[1]);
    slAddHead(&chromList, chrom);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&chromList);
*retHash = hash;
*retList = chromList;
}

void tAddN(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add tN's to all gaps underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tN = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddN(net, fill->children, tree);
    }
}

struct chrom *getQChrom(char *qName, struct hash *qChromHash)
/* Lift qName to chrom if necessary and dig up from qChromHash. */
{
struct chrom *qChrom = NULL;
if (liftHash != NULL)
    {
    struct liftSpec *lft = hashMustFindVal(liftHash, qName);
    qChrom = hashMustFindVal(qChromHash, lft->newName);
    }
else
    qChrom = hashMustFindVal(qChromHash, qName);
return(qChrom);
}

int liftQStart(char *qName, int qStart)
/* Lift qStart if necessary. */
{
int s = qStart;
if (liftHash != NULL)
    {
    struct liftSpec *lft = hashMustFindVal(liftHash, qName);
    s += lft->offset;
    }
return s;
}

void qAddN(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add qN's to all gaps underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    struct rbTree *tree = qChrom->nGaps;
    int s = liftQStart(fill->qName, fill->qStart);
    fill->qN = intersectionSize(tree, s, s + fill->qSize);
    if (fill->children)
	qAddN(net, fill->children, qChromHash);
    }
}

void tAddR(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add t repeats's to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tR = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddR(net, fill->children, tree);
    }
}

void qAddR(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add q repeats to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    int s = liftQStart(fill->qName, fill->qStart);
    fill->qR = intersectionSize(qChrom->repeats, s, s + fill->qSize);
    if (fill->children)
	qAddR(net, fill->children, qChromHash);
    }
}

void tAddNewR(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add t new repeats's to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tNewR = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddNewR(net, fill->children, tree);
    }
}

void qAddNewR(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add q new repeats to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    int s = liftQStart(fill->qName, fill->qStart);
    fill->qNewR = intersectionSize(qChrom->newRepeats, s, s + fill->qSize);
    if (fill->children)
	qAddNewR(net, fill->children, qChromHash);
    }
}

void tAddOldR(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add t new repeats's to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tOldR = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddOldR(net, fill->children, tree);
    }
}

void qAddOldR(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add q new repeats to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    int s = liftQStart(fill->qName, fill->qStart);
    fill->qOldR = intersectionSize(qChrom->oldRepeats, s, s + fill->qSize);
    if (fill->children)
	qAddOldR(net, fill->children, qChromHash);
    }
}

void tAddTrf(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add t simple repeats's to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tTrf = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddTrf(net, fill->children, tree);
    }
}

void qAddTrf(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add q new repeats to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    int s = liftQStart(fill->qName, fill->qStart);
    fill->qTrf = intersectionSize(qChrom->trf, s, s + fill->qSize);
    if (fill->children)
	qAddTrf(net, fill->children, qChromHash);
    }
}



void netClass(char *inName, char *tDb, char *qDb, char *outName)
/* netClass - Add classification info to net. */
{
struct chainNet *net;
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct chrom *qChromList, *chrom;
struct hash *qChromHash;
struct hash *arHash = NULL;

if (!noAr)
    arHash = getAncientRepeats(tDb, qDb);

getChroms(qDb, &qChromHash, &qChromList);


printf("Reading gaps in %s\n", qDb);
for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
    chrom->nGaps = getSeqGaps(qDb, chrom->name);

if (qNewR)
    {
    printf("Reading new repeats from %s\n", qNewR);
    for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
        chrom->newRepeats = getNewRepeats(qNewR, chrom->name);
    }

printf("Reading simpleRepeats in %s\n", qDb);
for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
    chrom->trf = getTrf(qDb, chrom->name);

printf("Reading repeats in %s\n", qDb);
for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
    getRepeats(qDb, arHash, chrom->name, &chrom->repeats, &chrom->oldRepeats);

while ((net = chainNetRead(lf)) != NULL)
    {
    struct rbTree *tN, *tRepeats, *tOldRepeats, *tTrf;

    printf("Processing %s.%s\n", tDb, net->name);
    tN = getSeqGaps(tDb, net->name);
    tAddN(net, net->fillList, tN);
    rbTreeFree(&tN);
    qAddN(net, net->fillList, qChromHash);

    getRepeats(tDb, arHash, net->name, &tRepeats, &tOldRepeats);
    tAddR(net, net->fillList, tRepeats);
    if (!noAr)
	tAddOldR(net, net->fillList, tOldRepeats);
    rbTreeFree(&tRepeats);
    rbTreeFree(&tOldRepeats);
    qAddR(net, net->fillList, qChromHash);
    if (!noAr)
	qAddOldR(net, net->fillList, qChromHash);

    tTrf = getTrf(tDb, net->name);
    tAddTrf(net, net->fillList, tTrf);
    rbTreeFree(&tTrf);
    qAddTrf(net, net->fillList, qChromHash);

    if (tNewR)
        {
	struct rbTree *tree = getNewRepeats(tNewR, net->name);
	tAddNewR(net, net->fillList, tree);
	rbTreeFree(&tree);
	}
    if (qNewR)
        qAddNewR(net, net->fillList, qChromHash);
    chainNetWrite(net, f);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *liftFile = NULL;
optionHash(&argc, argv);
if (argc != 5)
    usage();
tNewR = optionVal("tNewR", tNewR);
qNewR = optionVal("qNewR", qNewR);
noAr = optionExists("noAr");
liftFile = optionVal("liftQ", liftFile);
if (liftFile != NULL)
    {
    struct liftSpec *lifts = readLifts(liftFile);
    liftHash = hashLift(lifts, TRUE);
    }
netClass(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
