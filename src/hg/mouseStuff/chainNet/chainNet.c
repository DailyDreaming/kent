/* chainNet - Make alignment nets out of chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "rbTree.h"
#include "chainBlock.h"

static char const rcsid[] = "$Id: chainNet.c,v 1.27 2003/12/03 22:20:22 kent Exp $";

int minSpace = 25;	/* Minimum gap size to fill. */
int minFill;		/* Minimum fill to record. */
double minScore = 2000;	/* Minimum chain score to look at. */
boolean verbose = FALSE;/* Verbose output. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainNet - Make alignment nets out of chains\n"
  "usage:\n"
  "   chainNet in.chain target.sizes query.sizes target.net query.net\n"
  "where:\n"
  "   in.chain is the chain file sorted by score\n"
  "   target.sizes contains the size of the target sequences\n"
  "   query.sizes contains the size of the query sequences\n"
  "   target.net is the output over the target genome\n"
  "   query.net is the output over the query genome\n"
  "options:\n"
  "   -minSpace=N - minimum gap size to fill, default %d\n"
  "   -minFill=N  - default half of minSpace\n"
  "   -minScore=N - minimum chain score to consider, default %d\n"
  "   -verbose - make copious output\n"
  , minSpace, minScore);
}

struct gap
/* A gap in sequence alignments. */
    {
    struct gap *next;	    /* Next gap in list. */
    int start,end;	    /* Range covered in chromosome. */
    int oStart, oEnd;	    /* Range covered in other chromosome. */
    struct fill *fillList;  /* List of gap fillers. */
    };

struct fill
/* Some alignments that hopefully fill some gaps. */
    {
    struct fill *next;	   /* Next fill in list. */
    int start,end;	   /* Range covered, always plus strand. */
    int oStart,oEnd;	   /* Range covered in other coordinate, always plus. */
    struct gap *gapList;   /* List of internal gaps. */
    struct chain *chain;   /* Alignment chain (not all is necessarily used) */
    };

struct space
/* A part of a gap. */
    {
    int start, end;	/* Portion of gap covered, always plus strand. */
                        /* The start of this structure has to be identical
			 * with struct range above. */
    struct gap *gap;    /* The actual gap. */
    };

struct chrom
/* A chromosome. */
    {
    struct chrom *next;	    /* Next in list. */
    char *name;		    /* Name - allocated in hash */
    int size;		    /* Size of chromosome. */
    struct gap *root;	    /* Root of the gap/chain tree */
    struct rbTree *spaces;  /* Store gaps here for fast lookup. Items are spaces. */
    };



int gapCmpStart(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct gap *a = *((struct gap **)va);
const struct gap *b = *((struct gap **)vb);
return a->start - b->start;
}

int fillCmpStart(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct fill *a = *((struct fill **)va);
const struct fill *b = *((struct fill **)vb);
return a->start - b->start;
}


int spaceCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct space *a = va;
struct space *b = vb;
if (a->end <= b->start)
    return -1;
else if (b->end <= a->start)
    return 1;
else
    return 0;
}

void dumpSpace(void *item, FILE *f)
/* Print out range info. */
{
struct space *space = item;
fprintf(f, "%d,%d", space->start, space->end);
}

void doSpace(void *item)
/* Do something to range. */
{
struct space *space = item;
printf("%d,%d\n", space->start, space->end);
}

void addSpaceForGap(struct chrom *chrom, struct gap *gap)
/* Given a gap, create corresponding space in chromosome's
 * space rbTree. */
{
struct space *space;
AllocVar(space);
space->gap = gap;
space->start = gap->start;
space->end = gap->end;
rbTreeAdd(chrom->spaces, space);
}

struct gap *gapNew(int start, int end, int oStart, int oEnd)
/* Create a new gap. */
{
struct gap *gap;
AllocVar(gap);
gap->start = start;
gap->end = end;
gap->oStart = oStart;
gap->oEnd = oEnd;
return gap;
}

boolean strictlyInside(int minStart, int maxEnd, int start, int end)
/* Return TRUE if minStart < start <= end < maxEnd 
 * and end - start >= minSpace */
{
return (minStart < start && start + minSpace <= end && end < maxEnd);
}

void makeChroms(char *fileName, struct hash **retHash, struct chrom **retList)
/* Read size file and make chromosome structure for each  element. */
{
char *row[2];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
struct chrom *chrom, *chromList = NULL;

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    if (hashLookup(hash, name) != NULL)
        errAbort("Duplicate %s in %s", name, fileName);
    AllocVar(chrom);
    slAddHead(&chromList, chrom);
    hashAddSaveName(hash, name, chrom, &chrom->name);
    chrom->size = lineFileNeedNum(lf, row, 1);
    chrom->spaces = rbTreeNew(spaceCmp);
    chrom->root = gapNew(0, chrom->size, 0, 0);
    addSpaceForGap(chrom, chrom->root);
    }
lineFileClose(&lf);
slReverse(&chromList);
*retHash = hash;
*retList = chromList;
}

boolean innerBounds(struct boxIn *startBlock,
	boolean isQ, int inStart, int inEnd, int *retStart, int *retEnd)
/* Return the portion of chain which is between inStart and
 * inEnd.  Only considers blocks inbetween inStart and inEnd.  
 * Return NULL if chain has no blocks between these. */
{
struct boxIn *b;
int start = BIGNUM, end = -BIGNUM;
for (b = startBlock; b != NULL; b = b->next)
    {
    int s, e;
    if (isQ)
       {
       s = b->qStart;
       e = b->qEnd;
       }
    else
       {
       s = b->tStart;
       e = b->tEnd;
       }
    if (e <= inStart)
        continue;
    if (s >= inEnd)
        break;
    if (s < inStart) s = inStart;
    if (e > inEnd) e = inEnd;
    if (start > s) start = s;
    if (end < e) end = e;
    }
if (end - start < minFill)
    return FALSE;
*retStart = start;
*retEnd = end;
return TRUE;
}

static void qFillOtherRange(struct fill *fill)
/* Given bounds of fill in q coordinates, calculate
 * oStart/oEnd in t coordinates, and refine 
 * start/end to reflect parts of chain actually used. */
{
struct chain *chain = fill->chain;
int clipStart = fill->start;
int clipEnd = fill->end;
boolean isRev = (chain->qStrand == '-');
int tMin = BIGNUM, tMax = -BIGNUM;
int qMin = BIGNUM, qMax = -BIGNUM;
struct boxIn *b;

if (isRev)
    reverseIntRange(&clipStart, &clipEnd, chain->qSize);
for (b = chain->blockList; b != NULL; b = b->next)
    {
    int qs, qe, ts, te;	/* Clipped bounds of block */
    if ((qe = b->qEnd) <= clipStart)
        continue;
    if ((qs = b->qStart) >= clipEnd)
        break;
    ts = b->tStart;
    te = b->tEnd;
    if (qs < clipStart)
        {
	ts += (clipStart - qs);
	qs = clipStart;
	}
    if (qe > clipEnd)
        {
	te  -= (clipEnd - qe);
	qe = clipEnd;
	}
    if (qMin > qs) qMin = qs;
    if (qMax < qe) qMax = qe;
    if (tMin > ts) tMin = ts;
    if (tMax < te) tMax = te;
    }
if (isRev)
    reverseIntRange(&qMin, &qMax, chain->qSize);
fill->start = qMin;
fill->end = qMax;
fill->oStart = tMin;
fill->oEnd = tMax;
assert(tMin < tMax);
}

static void tFillOtherRange(struct fill *fill)
/* Given bounds of fill in t coordinates, calculate
 * oStart/oEnd in q coordinates, and refine 
 * start/end to reflect parts of chain actually used. */
{
struct chain *chain = fill->chain;
int clipStart = fill->start;
int clipEnd = fill->end;
boolean isRev = (chain->qStrand == '-');
int tMin = BIGNUM, tMax = -BIGNUM;
int qMin = BIGNUM, qMax = -BIGNUM;
struct boxIn *b;
for (b = chain->blockList; b != NULL; b = b->next)
    {
    int qs, qe, ts, te;	/* Clipped bounds of block */
    if ((te = b->tEnd) <= clipStart)
        continue;
    if ((ts = b->tStart) >= clipEnd)
        break;
    qs = b->qStart;
    qe = b->qEnd;
    if (ts < clipStart)
        {
	qs += (clipStart - ts);
	ts = clipStart;
	}
    if (te > clipEnd)
        {
	qe  -= (clipEnd - te);
	te = clipEnd;
	}
    if (qMin > qs) qMin = qs;
    if (qMax < qe) qMax = qe;
    if (tMin > ts) tMin = ts;
    if (tMax < te) tMax = te;
    }
if (isRev)
    reverseIntRange(&qMin, &qMax, chain->qSize);
fill->start = tMin;
fill->end = tMax;
fill->oStart = qMin;
fill->oEnd = qMax;
assert(qMin < qMax);
}


struct fill *fillSpace(struct chrom *chrom, struct space *space, 
	struct chain *chain, struct boxIn *startBlock, 
	boolean isQ)
/* Fill in space with chain, remove existing space from chrom,
 * and add smaller spaces on either side if big enough. */
{
struct fill *fill;
int s, e;
struct space *lSpace, *rSpace;

if (!innerBounds(startBlock, isQ, space->start, space->end, &s, &e))
    return NULL;
assert(s < e);
AllocVar(fill);
fill->start = s;
fill->end = e;
fill->chain = chain;
rbTreeRemove(chrom->spaces, space);
if (s - space->start >= minSpace)
    {
    AllocVar(lSpace);
    lSpace->gap = space->gap;
    lSpace->start = space->start;
    lSpace->end = s;
    rbTreeAdd(chrom->spaces, lSpace);
    }
if (space->end - e >= minSpace)
    {
    AllocVar(rSpace);
    rSpace->gap = space->gap;
    rSpace->start = e;
    rSpace->end = space->end;
    rbTreeAdd(chrom->spaces, rSpace);
    }
slAddHead(&space->gap->fillList, fill);
return fill;
}

static struct slRef *fsList;

void fsAdd(void *item)
/* Add item to fsList. */
{
refAdd(&fsList, item);
}

struct slRef *findSpaces(struct rbTree *tree, int start, int end)
/* Return a list of spaces that intersect interval between start
 * and end. */
{
static struct space space;
space.start = start;
space.end = end;
fsList = NULL;
rbTreeTraverseRange(tree, &space, &space, fsAdd);
slReverse(&fsList);
return fsList;
}


void reverseBlocksQ(struct boxIn **pList, int qSize)
/* Reverse qside of blocks. */
{
struct boxIn *b;
slReverse(pList);
for (b = *pList; b != NULL; b = b->next)
    reverseIntRange(&b->qStart, &b->qEnd, qSize);
}


void addChainT(struct chrom *chrom, struct chrom *otherChrom, struct chain *chain)
/* Add T side of chain to fill/gap tree of chromosome. 
 * This is the easier case since there are no strand
 * issues to worry about. */
{
struct slRef *spaceList;
struct slRef *ref;
struct boxIn *startBlock, *block, *nextBlock;
struct gap *gap;
struct fill *fill = NULL;

spaceList = findSpaces(chrom->spaces,chain->tStart,chain->tEnd);
startBlock = chain->blockList;
for (ref = spaceList; ref != NULL; ref = ref->next)
    {
    struct space *space = ref->val;
    struct fill *fill;
    int gapStart, gapEnd;
    for (;;)
        {
	nextBlock = startBlock->next;
	if (nextBlock == NULL)
	    break;
	gapEnd = nextBlock->tStart;
	if (gapEnd > space->start)
	    break;
	startBlock = nextBlock;
	}
    if ((fill = fillSpace(chrom, space, chain, startBlock, FALSE)) != NULL)
	{
	for (block = startBlock; ; block = nextBlock)
	    {
	    nextBlock = block->next;
	    if (nextBlock == NULL)
		break;
	    gapStart = block->tEnd;
	    gapEnd = nextBlock->tStart;
	    if (strictlyInside(space->start, space->end, gapStart, gapEnd))
		{
		int qs = block->qEnd;
		int qe = nextBlock->qStart;
		if (chain->qStrand == '-')
		    reverseIntRange(&qs, &qe, chain->qSize);
		gap = gapNew(gapStart, gapEnd, qs, qe);
		addSpaceForGap(chrom, gap);
		slAddHead(&fill->gapList, gap);
		}
	    }
	freez(&ref->val);	/* aka space */
	}
    }
slFreeList(&spaceList);
}

void addChainQ(struct chrom *chrom, struct chrom *otherChrom, struct chain *chain)
/* Add Q side of chain to fill/gap tree of chromosome. 
 * For this side we have to cope with reverse strand
 * issues. */
{
struct slRef *spaceList;
struct slRef *ref;
struct boxIn *startBlock, *block, *nextBlock;
int gapStart, gapEnd;
struct gap *gap;
struct fill *fill = NULL;
boolean isRev = (chain->qStrand == '-'); 
int qStart = chain->qStart, qEnd = chain->qEnd;

if (isRev)
    {
    reverseIntRange(&qStart, &qEnd, chain->qSize);
    reverseBlocksQ(&chain->blockList, chain->qSize);
    }
spaceList = findSpaces(chrom->spaces,qStart,qEnd);
startBlock = chain->blockList;

for (ref = spaceList; ref != NULL; ref = ref->next)
    {
    struct space *space = ref->val;
    struct fill *fill;
    for (;;)
        {
	nextBlock = startBlock->next;
	if (nextBlock == NULL)
	    break;
	gapEnd = nextBlock->qStart;
	if (gapEnd > space->start)
	    break;
	startBlock = nextBlock;
	}
    if ((fill = fillSpace(chrom, space, chain, startBlock, TRUE)) 
    	!= NULL)
	{
	for (block = startBlock; ; block = nextBlock)
	    {
	    nextBlock = block->next;
	    if (nextBlock == NULL)
		break;
	    gapStart = block->qEnd;
	    gapEnd = nextBlock->qStart;
	    if (strictlyInside(space->start, space->end, gapStart, gapEnd))
		{
		int ts, te;
		if (chain->qStrand == '+')
		    {
		    ts = block->tEnd;
		    te = nextBlock->tStart;
		    }
		else
		    {
		    ts = nextBlock->tStart;
		    te = block->tEnd;
		    }
		gap = gapNew(gapStart, gapEnd, ts, te);
		addSpaceForGap(chrom, gap);
		slAddHead(&fill->gapList, gap);
		}
	    }
	freez(&ref->val);	/* aka space */
	}
    }
slFreeList(&spaceList);
if (isRev)
    reverseBlocksQ(&chain->blockList, chain->qSize);
}

void addChain(struct chrom *qChrom, struct chrom *tChrom, struct chain *chain)
/* Add as much of chain as possible to chromosomes. */
{
addChainQ(qChrom, tChrom, chain);
addChainT(tChrom, qChrom, chain);
}

boolean chromHasData(struct chrom *chrom)
/* Return TRUE if chrom is non-empty */
{
return chrom != NULL && chrom->root != NULL && chrom->root->fillList != NULL;
}

void sortNet(struct gap *gap)
/* Recursively sort lists. */
{
struct fill *fill;
struct gap *g;
slSort(&gap->fillList, fillCmpStart);
for (fill = gap->fillList; fill != NULL; fill = fill->next)
    {
    slSort(&fill->gapList, gapCmpStart);
    for (gap = fill->gapList; gap != NULL; gap = gap->next)
        sortNet(gap);
    }
}


void rCalcOtherFill(struct gap *gap, boolean isQ)
/* Recursively fill in oStart/oEnd fields of fill
 * structures. */
{
struct fill *fill;
for (fill = gap->fillList; fill != NULL; fill = fill->next)
    {
    struct gap *g;
    if (isQ)
	qFillOtherRange(fill);
    else
        tFillOtherRange(fill);
    for (g = fill->gapList; g != NULL; g = g->next)
        rCalcOtherFill(g, isQ);
    }
}

void finishNet(struct chrom *chromList, boolean isQ)
/* Fill in oStart/oEnd fields of fill structures attatched
 * to chromosomes, sort, and generally spiff up net. */
{
struct chrom *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (chromHasData(chrom))
	{
	sortNet(chrom->root);
	rCalcOtherFill(chrom->root, isQ);
	}
    }
}


int rOutDepth = 0;
boolean rOutQ = FALSE;

static void rOutputGap(struct fill *parent, struct gap *gap, FILE *f);
static void rOutputFill(struct fill *fill, FILE *f);

static void rOutputGap(struct fill *parent, struct gap *gap, FILE *f)
/* Recursively output gap and it's fillers. */
{
struct fill *fill;
struct chain *chain = parent->chain;
char *oChrom = (rOutQ ? chain->tName : chain->qName);
++rOutDepth;
spaceOut(f, rOutDepth);
fprintf(f, "gap %d %d %s %c %d %d\n", 
	gap->start, gap->end - gap->start,
	oChrom, chain->qStrand, gap->oStart, gap->oEnd - gap->oStart);
for (fill = gap->fillList; fill != NULL; fill = fill->next)
    rOutputFill(fill, f);
--rOutDepth;
}

int chainBaseCount(struct chain *chain)
/* Return number of bases in gap free alignments in chain. */
{
struct boxIn *b;
int total = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    total += b->qEnd - b->qStart;
return total;
}

int chainBaseCountSubT(struct chain *chain, int tMin, int tMax)
/* Return number of bases in gap free alignments in chain between 
 * tMin and tMax. */
{
struct boxIn *b;
int total = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    total += positiveRangeIntersection(b->tStart, b->tEnd, tMin, tMax);
return total;
}

int chainBaseCountSubQ(struct chain *chain, int qMin, int qMax)
/* Return number of bases in gap free alignments in chain between 
 * tMin and tMax. */
{
struct boxIn *b;
int total = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    total += positiveRangeIntersection(b->qStart, b->qEnd, qMin, qMax);
return total;
}

void subchainInfo(struct chain *chain, int start, int end, boolean isQ,
	int *retSize, double *retScore)
/* Return score of subchain. */
{
int fullSize = chainBaseCount(chain);
if (isQ)
    {
    if (chain->qStrand == '-')
        reverseIntRange(&start, &end, chain->qSize);
    if (start <= chain->qStart && end >= chain->qEnd)
        {
	*retScore = chain->score;
	*retSize = fullSize;
	}
    else
        {
	int subSize = chainBaseCountSubQ(chain, start, end);
	*retScore = chain->score * subSize / fullSize;
	*retSize = subSize;
	// uglyf("subchainInfo Q%c %d,%d %d,%d ali %d, subSize %d, score %f, subScore %f\n", chain->qStrand, start, end, chain->qStart, chain->qEnd, fullSize, subSize, chain->score, *retScore);
	}
    }
else
    {
    if (start <= chain->tStart && end >= chain->tEnd)
        {
	*retScore = chain->score;
	*retSize = fullSize;
	}
    else
        {
	int subSize = chainBaseCountSubT(chain, start, end);
	*retScore = chain->score * subSize / fullSize;
	*retSize = subSize;
	if (chain->id == 7307)
	    uglyf("subchainInfo T %d,%d %d,%d ali %d, subSize %d, score %f, subScore %f\n", start, end, chain->tStart, chain->tEnd, fullSize, subSize, chain->score, *retScore);
	}
    }
}

void fillOut(FILE *f, struct fill *fill, char *oChrom, int depth,
	int subSize, double subScore)
/* Output fill. */
{
struct chain *chain = fill->chain;
spaceOut(f, depth);
fprintf(f, "fill %d %d %s %c %d %d id %d score %1.0f ali %d\n", 
	fill->start, fill->end - fill->start,
	oChrom, chain->qStrand, 
	fill->oStart, fill->oEnd - fill->oStart, chain->id,
	subScore, subSize);
}

static void rOutputFill(struct fill *fill, FILE *f)
/* Recursively output fill and it's gaps. */
{
struct gap *gap;
struct chain *chain = fill->chain;
int subSize;
double subScore;

subchainInfo(chain, fill->start, fill->end, rOutQ, &subSize, &subScore);
if (subScore >= minScore && subSize >= minFill)
    {
    ++rOutDepth;
    if (rOutQ)
	fillOut(f, fill, fill->chain->tName, rOutDepth, subSize, subScore);
    else
	fillOut(f, fill, fill->chain->qName, rOutDepth, subSize, subScore);
    for (gap = fill->gapList; gap != NULL; gap = gap->next)
	rOutputGap(fill, gap, f);
    --rOutDepth;
    }
}

void outputNetSide(struct chrom *chromList, char *fileName, boolean isQ)
/* Output one side of net */
{
FILE *f = mustOpen(fileName, "w");
struct chrom *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    rOutDepth = 0;
    rOutQ = isQ;
    if (chromHasData(chrom))
	{
	struct fill *fill;
	fprintf(f, "net %s %d\n", chrom->name, chrom->size);
	for (fill = chrom->root->fillList; fill != NULL; fill = fill->next)
	    rOutputFill(fill, f);
	}
    }
}

void printMem()
/* Print out memory used and other stuff from linux. */
{
struct lineFile *lf = lineFileOpen("/proc/self/stat", TRUE);
char *line, *words[50];
int wordCount;
if (lineFileNext(lf, &line, NULL))
    {
    wordCount = chopLine(line, words);
    if (wordCount >= 23)
        printf("memory usage %s, utime %s s/100, stime %s\n", 
		words[22], words[13], words[14]);
    }
lineFileClose(&lf);
}



void chainNet(char *chainFile, char *tSizes, char *qSizes, 
	char *tNet, char *qNet)
/* chainNet - Make alignment nets out of chains. */
{
struct lineFile *lf = lineFileOpen(chainFile, TRUE);
struct hash *qHash, *tHash;
struct chrom *qChromList, *tChromList, *tChrom, *qChrom;
struct chain *chain;
double lastScore = -1;

makeChroms(qSizes, &qHash, &qChromList);
makeChroms(tSizes, &tHash, &tChromList);
printf("Got %d chroms in %s, %d in %s\n", slCount(tChromList), tSizes,
	slCount(qChromList), qSizes);

/* Loop through chain file building up net. */
while ((chain = chainRead(lf)) != NULL)
    {
    /* Make sure that input is really sorted. */
    if (lastScore >= 0 && chain->score > lastScore)
        errAbort("%s must be sorted in order of score", chainFile);
    lastScore = chain->score;

    if (chain->score < minScore) 
	{
    	break;
	}
    if (verbose)
	{
	printf("chain %f (%d els) %s %d-%d %c %s %d-%d\n", 
		chain->score, slCount(chain->blockList), 
		chain->tName, chain->tStart, chain->tEnd, 
		chain->qStrand, chain->qName, chain->qStart, chain->qEnd);
	}
    qChrom = hashMustFindVal(qHash, chain->qName);
    if (qChrom->size != chain->qSize)
        errAbort("%s is %d in %s but %d in %s", chain->qName, 
		chain->qSize, chainFile,
		qChrom->size, qSizes);
    tChrom = hashMustFindVal(tHash, chain->tName);
    if (tChrom->size != chain->tSize)
        errAbort("%s is %d in %s but %d in %s", chain->tName, 
		chain->tSize, chainFile,
		tChrom->size, tSizes);
    addChain(qChrom, tChrom, chain);
    if (verbose)
	printf("%s has %d inserts, %s has %d\n", tChrom->name, 
		tChrom->spaces->n, qChrom->name, qChrom->spaces->n);
    }
/* Build up other side of fills.  It's just for historical 
 * reasons this is not done during the main build up.   
 * It's a little less efficient this way, but to change it
 * some hard reverse strand issues would have to be juggled. */
printf("Finishing nets\n");
finishNet(qChromList, TRUE);
finishNet(tChromList, FALSE);

/* Write out basic net files. */
printf("writing %s\n", tNet);
outputNetSide(tChromList, tNet, FALSE);
printf("writing %s\n", qNet);
outputNetSide(qChromList, qNet, TRUE);

printMem();
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 6)
    usage();
minSpace = optionInt("minSpace", minSpace);
minFill = optionInt("minFill", minSpace/2);
minScore = optionInt("minScore", minScore);
verbose = optionExists("verbose");
chainNet(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
