/* axtChain - Chain together axt alignments.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "axt.h"
#include "psl.h"
#include "boxClump.h"
#include "chainBlock.h"
#include "portable.h"

static char const rcsid[] = "$Id: axtChain.c,v 1.23 2004/04/28 21:12:39 kent Exp $";

int minScore = 1000;
char *detailsName = NULL;
char *gapFileName = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtChain - Chain together axt alignments.\n"
  "usage:\n"
  "   axtChain in.axt tNibDir qNibDir out.chain\n"
  "options:\n"
  "   -psl Use psl instead of axt format for input\n"
  "   -faQ qNibDir is a fasta file with multiple sequences for query\n"
  "   -minScore=N  Minimum score for chain, default %d\n"
  "   -details=fileName Output some additional chain details\n"
  "   -scoreScheme=fileName Read the scoring matrix from a blastz-format file\n"
  "   -linearGap=filename Read piecewise linear gap from tab delimited file\n"
  "   sample linearGap file \n"
  "tablesize 11\n"
  "smallSize 111\n"
  "position 1 2 3 11 111 2111 12111 32111 72111 152111 252111\n"
  "qGap 350 425 450 600 900 2900 22900 57900 117900 217900 317900\n"
  "tGap 350 425 450 600 900 2900 22900 57900 117900 217900 317900\n"
  "bothGap 750 825 850 1000 1300 3300 23300 58300 118300 218300 318300\n"
  , minScore

  );
}

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    struct boxIn *blockList; /* List of alignments. */
    int axtCount;		/* Count of axt's that make this up (just for stats) */
    };

int seqPairCmp(const void *va, const void *vb)
/* Compare to sort based on tName,qName. */
{
const struct seqPair *a = *((struct seqPair **)va);
const struct seqPair *b = *((struct seqPair **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = strcmp(a->qName, b->qName);
if (dif == 0)
    dif = (int)a->qStrand - (int)b->qStrand;
return dif;
}

void addAxtBlocks(struct boxIn **pList, struct axt *axt)
/* Add blocks (gapless subalignments) from axt to block list. */
{
boolean thisIn, lastIn = FALSE;
int qPos = axt->qStart, tPos = axt->tStart;
int qStart = 0, tStart = 0;
int i;

for (i=0; i<=axt->symCount; ++i)
    {
    int advanceQ = (isalpha(axt->qSym[i]) ? 1 : 0);
    int advanceT = (isalpha(axt->tSym[i]) ? 1 : 0);
    thisIn = (advanceQ && advanceT);
    if (thisIn)
        {
	if (!lastIn)
	    {
	    qStart = qPos;
	    tStart = tPos;
	    }
	}
    else
        {
	if (lastIn)
	    {
	    int size = qPos - qStart;
	    assert(size == tPos - tStart);
	    if (size > 0)
	        {
		struct boxIn *b;
		AllocVar(b);
		b->qStart = qStart;
		b->qEnd = qPos;
		b->tStart = tStart;
		b->tEnd = tPos;
		slAddHead(pList, b);
		}
	    }
	}
    lastIn = thisIn;
    qPos += advanceQ;
    tPos += advanceT;
    }
}

void addPslBlocks(struct boxIn **pList, struct psl *psl)
/* Add blocks (gapless subalignments) from psl to block list. */
{
int i;
for (i=0; i<psl->blockCount; ++i)
    {
    struct boxIn *b;
    int size;
    AllocVar(b);
    size = psl->blockSizes[i];
    b->qStart = b->qEnd = psl->qStarts[i];
    b->qEnd += size;
    b->tStart = b->tEnd = psl->tStarts[i];
    b->tEnd += size;
    slAddHead(pList, b);
    }
}

void loadIfNewSeq(char *nibDir, char *newName, char strand, 
	char **pName, struct dnaSeq **pSeq, char *pStrand)
/* Load sequence unless it is already loaded.  Reverse complement
 * if necessary. */
{
struct dnaSeq *seq;
if (sameString(newName, *pName))
    {
    if (strand != *pStrand)
        {
	seq = *pSeq;
	reverseComplement(seq->dna, seq->size);
	*pStrand = strand;
	}
    }
else
    {
    char fileName[512];
    freeDnaSeq(pSeq);
    snprintf(fileName, sizeof(fileName), "%s/%s.nib", nibDir, newName);
    *pName = newName;
    *pSeq = seq = nibLoadAllMasked(NIB_MASK_MIXED, fileName);
    *pStrand = strand;
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);
    verbose(1, "Loaded %d bases in %s\n", seq->size, fileName);
    }
}

void loadFaSeq(struct hash *faHash, char *newName, char strand, 
	char **pName, struct dnaSeq **pSeq, char *pStrand)
/* retrieve sequence from hash.  Reverse complement
 * if necessary. */
{
struct dnaSeq *seq;
if (sameString(newName, *pName))
    {
    if (strand != *pStrand)
        {
	seq = *pSeq;
	reverseComplement(seq->dna, seq->size);
	*pStrand = strand;
	}
    }
else
    {
    char fileName[512];
    *pName = newName;
    *pSeq = seq = hashFindVal(faHash, newName);
    *pStrand = strand;
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);
    verbose(1, "Loaded %d bases from %s fa\n", seq->size, newName);
    }
}
int boxInCmpBoth(const void *va, const void *vb)
/* Compare to sort based on query, then target. */
{
const struct boxIn *a = *((struct boxIn **)va);
const struct boxIn *b = *((struct boxIn **)vb);
int dif;
dif = a->qStart - b->qStart;
if (dif == 0)
    dif = a->tStart - b->tStart;
return dif;
}

void removeExactOverlaps(struct boxIn **pBoxList)
/* Remove from list blocks that start in exactly the same
 * place on both coordinates. */
{
struct boxIn *newList = NULL, *b, *next, *last = NULL;
slSort(pBoxList, boxInCmpBoth);
for (b = *pBoxList; b != NULL; b = next)
    {
    next = b->next;
    if (last != NULL && b->qStart == last->qStart && b->tStart == last->tStart)
        {
	/* Fold this block into previous one. */
	if (last->qEnd < b->qEnd) last->qEnd = b->qEnd;
	if (last->tEnd < b->tEnd) last->tEnd = b->tEnd;
	freeMem(b);
	}
    else
	{
	slAddHead(&newList, b);
	last = b;
	}
    }
slReverse(&newList);
*pBoxList = newList;
}

double scoreBlock(char *q, char *t, int size, int matrix[256][256])
/* Score block through matrix. */
{
double score = 0;
int i;
for (i=0; i<size; ++i)
    score += matrix[q[i]][t[i]];
return score;
}


static void findCrossover(struct boxIn *left, struct boxIn *right,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq,  
	int overlap, int matrix[256][256], int *retPos, int *retScoreAdjustment)
/* Find ideal crossover point of overlapping blocks.  That is
 * the point where we should start using the right block rather
 * than the left block.  This point is an offset from the start
 * of the overlapping region (which is the same as the start of the
 * right block). */
{
int bestPos = 0;
char *rqStart = qSeq->dna + right->qStart;
char *lqStart = qSeq->dna + left->qEnd - overlap;
char *rtStart = tSeq->dna + right->tStart;
char *ltStart = tSeq->dna + left->tEnd - overlap;
int i;
double score, bestScore, rScore, lScore;

/* Make sure overlap is not larger than either block size: */
if (overlap > (left->tEnd - left->tStart) ||
    overlap > (right->tEnd - right->tStart))
    errAbort("overlap is %d -- too large for one of these:\n"
	     "qSize=%d  tSize=%d\n"
	     "left: qStart=%d qEnd=%d (%d) tStart=%d tEnd=%d (%d)\n"
	     "right: qStart=%d qEnd=%d (%d) tStart=%d tEnd=%d (%d)\n",
	     overlap, qSeq->size, tSeq->size,
	     left->qStart, left->qEnd, left->qEnd - left->qStart,
	     left->tStart, left->tEnd, left->tEnd - left->tStart,
	     right->qStart, right->qEnd, right->qEnd - right->qStart, 
	     right->tStart, right->tEnd, right->tEnd - right->tStart);

score = bestScore = rScore = scoreBlock(rqStart, rtStart, overlap, matrix);
lScore = scoreBlock(lqStart, ltStart, overlap, matrix);
for (i=0; i<overlap; ++i)
    {
    score += matrix[lqStart[i]][ltStart[i]];
    score -= matrix[rqStart[i]][rtStart[i]];
    if (score > bestScore)
	{
	bestScore = score;
	bestPos = i+1;
	}
    }
*retPos = bestPos;
*retScoreAdjustment = rScore + lScore - bestScore;
}

struct scoreData
/* Data needed to score block. */
    {
    struct dnaSeq *qSeq;	/* Query sequence. */
    struct dnaSeq *tSeq;	/* Target sequence. */
    struct axtScoreScheme *ss;  /* Scoring scheme. */
    };
struct scoreData scoreData;
struct axtScoreScheme *scoreScheme = NULL;

int interpolate(int x, int *s, double *v, int sCount)
/* Find closest value to x in s, and then lookup corresponding
 * value in v.  Interpolate where necessary. */
{
int i, ds, ss;
double dv;
for (i=0; i<sCount; ++i)
    {
    ss = s[i];
    if (x == ss)
        return v[i];
    else if (x < ss)
        {
	ds = ss - s[i-1];
	dv = v[i] - v[i-1];
	return v[i-1] + dv * (x - s[i-1]) / ds;
	}
    }
/* If get to here extrapolate from last two values */
ds = s[sCount-1] - s[sCount-2];
dv = v[sCount-1] - v[sCount-2];
return v[sCount-2] + dv * (x - s[sCount-2]) / ds;
}

static int *gapInitPos ;  
static double *gapInitQGap ;  
static double *gapInitTGap ;  
static double *gapInitBothGap ;

static int gapInitPosDefault[] = { 
   1,   2,   3,   11,  111, 2111, 12111, 32111,  72111, 152111, 252111,
};
static double gapInitQGapDefault[] = { 
   350, 425, 450, 600, 900, 2900, 22900, 57900, 117900, 217900, 317900,
};
static double gapInitTGapDefault[] = { 
   350, 425, 450, 600, 900, 2900, 22900, 57900, 117900, 217900, 317900,
};
static double gapInitBothGapDefault[] = { 
   400+350, 400+425, 400+450, 400+600, 400+900, 400+2900, 
   400+22900, 400+57900, 400+117900, 400+217900, 400+317900,
};



struct gapAid
/* A structure that bundles together stuff to help us
 * calculate gap costs quickly. */
    {
    int smallSize; /* Size of tables for doing quick lookup of small gaps. */
    int *qSmall;   /* Table for small gaps in q; */
    int *tSmall;   /* Table for small gaps in t. */
    int *bSmall;   /* Table for small gaps in either. */
    int *longPos;/* Table of positions to interpolate between for larger gaps. */
    double *qLong; /* Values to interpolate between for larger gaps in q. */
    double *tLong; /* Values to interpolate between for larger gaps in t. */
    double *bLong; /* Values to interpolate between for larger gaps in both. */
    int longCount;	/* Number of long positions overall in longPos. */
    int qPosCount;	/* Number of long positions in q. */
    int tPosCount;	/* Number of long positions in t. */
    int bPosCount;	/* Number of long positions in b. */
    int qLastPos;	/* Maximum position we have data on in q. */
    int tLastPos;	/* Maximum position we have data on in t. */
    int bLastPos;	/* Maximum position we have data on in b. */
    double qLastPosVal;	/* Value at max pos. */
    double tLastPosVal;	/* Value at max pos. */
    double bLastPosVal;	/* Value at max pos. */
    double qLastSlope;	/* What to add for each base after last. */
    double tLastSlope;	/* What to add for each base after last. */
    double bLastSlope;	/* What to add for each base after last. */
    } aid;

double calcSlope(double y2, double y1, double x2, double x1)
/* Calculate slope of line from x1/y1 to x2/y2 */
{
return (y2-y1)/(x2-x1);
}

void initGapAid(char *gapFileName)
/* Initialize gap aid structure for faster gap
 * computations. */
{
int i, tableSize, startLong = -1;
char *sizeDesc[2];
char *words[128];

if (gapFileName != NULL)
    {
    struct lineFile *lf = lineFileOpen(gapFileName, TRUE);
    int count;

    lineFileNextRowTab(lf, sizeDesc, 2);
    tableSize = atoi(sizeDesc[1]);
    AllocArray(gapInitPos,tableSize);
    AllocArray(gapInitQGap,tableSize);
    AllocArray(gapInitTGap,tableSize);
    AllocArray(gapInitBothGap,tableSize);
    while (count = lineFileChopNext(lf, words, tableSize+1))
        {
        if (sameString(words[0],"smallSize"))
            {
            aid.smallSize = atoi(words[1]);
            }
        if (sameString(words[0],"position"))
            {
            for (i=0 ; i<count-1 ; i++)
                gapInitPos[i] = atoi(words[i+1]);
            }
        if (sameString(words[0],"qGap"))
            {
            for (i=0 ; i<count-1 ; i++)
                gapInitQGap[i] = atoi(words[i+1]);
            }
        if (sameString(words[0],"tGap"))
            {
            for (i=0 ; i<count-1 ; i++)
                gapInitTGap[i] = atoi(words[i+1]);
            }
        if (sameString(words[0],"bothGap"))
            {
            for (i=0 ; i<count-1 ; i++)
                gapInitBothGap[i] = atoi(words[i+1]);
            }
            
        }
    if (aid.smallSize == 0)
        errAbort("missing smallSize parameter in %s\n",gapFileName);
    lineFileClose(&lf);
    }
else
    {
    /* if no gap file, then setup default values */ 
    /* Set up to handle small values */
    aid.smallSize = 111;
    tableSize = 11;
    AllocArray(gapInitPos,tableSize);
    AllocArray(gapInitQGap,tableSize);
    AllocArray(gapInitTGap,tableSize);
    AllocArray(gapInitBothGap,tableSize);
    for (i = 0 ; i < tableSize ; i++)
        {
        gapInitPos[i] = gapInitPosDefault[i];
        gapInitTGap[i] = gapInitTGapDefault[i];
        gapInitQGap[i] = gapInitQGapDefault[i];
        gapInitBothGap[i] = gapInitBothGapDefault[i];
        }
    }
    AllocArray(aid.qSmall, aid.smallSize);
    AllocArray(aid.tSmall, aid.smallSize);
    AllocArray(aid.bSmall, aid.smallSize);
    for (i=1; i<aid.smallSize; ++i)
        {
        aid.qSmall[i] = 
            interpolate(i, gapInitPos, gapInitQGap, tableSize);
        aid.tSmall[i] = 
            interpolate(i, gapInitPos, gapInitTGap, tableSize);
        aid.bSmall[i] = interpolate(i, gapInitPos, 
            gapInitBothGap, tableSize);
        }

    /* Set up to handle intermediate values. */
    for (i=0; i<tableSize; ++i)
        {
        if (aid.smallSize == gapInitPos[i])
            {
            startLong = i;
            break;
            }
        }
    if (startLong < 0)
        errAbort("No position %d in initGapAid()\n", aid.smallSize);
    aid.longCount = tableSize - startLong;
    aid.qPosCount = tableSize - startLong;
    aid.tPosCount = tableSize - startLong;
    aid.bPosCount = tableSize - startLong;
    aid.longPos = cloneMem(gapInitPos + startLong, aid.longCount * sizeof(int));
    aid.qLong = cloneMem(gapInitQGap + startLong, aid.qPosCount * sizeof(double));
    aid.tLong = cloneMem(gapInitTGap + startLong, aid.tPosCount * sizeof(double));
    aid.bLong = cloneMem(gapInitBothGap + startLong, aid.bPosCount * sizeof(double));

    /* Set up to handle huge values. */
    aid.qLastPos = aid.longPos[aid.qPosCount-1];
    aid.tLastPos = aid.longPos[aid.tPosCount-1];
    aid.bLastPos = aid.longPos[aid.bPosCount-1];
    aid.qLastPosVal = aid.qLong[aid.qPosCount-1];
    aid.tLastPosVal = aid.tLong[aid.tPosCount-1];
    aid.bLastPosVal = aid.bLong[aid.bPosCount-1];
    aid.qLastSlope = calcSlope(aid.qLastPosVal, aid.qLong[aid.qPosCount-2],
                               aid.qLastPos, aid.longPos[aid.qPosCount-2]);
    aid.tLastSlope = calcSlope(aid.tLastPosVal, aid.tLong[aid.tPosCount-2],
                               aid.tLastPos, aid.longPos[aid.tPosCount-2]);
    aid.bLastSlope = calcSlope(aid.bLastPosVal, aid.bLong[aid.bPosCount-2],
                               aid.bLastPos, aid.longPos[aid.bPosCount-2]);
}

int gapCost(int dq, int dt)
/* Figure out gap costs. */
{
if (dt < 0) dt = 0;
if (dq < 0) dq = 0;
if (dt == 0)
    { 
    if (dq < aid.smallSize)
        return aid.qSmall[dq];
    else if (dq >= aid.qLastPos)
        return aid.qLastPosVal + aid.qLastSlope * (dq-aid.qLastPos);
    else
        return interpolate(dq, aid.longPos, aid.qLong, aid.qPosCount);
    }
else if (dq == 0)
    {
    if (dt < aid.smallSize)
        return aid.tSmall[dt];
    else if (dt >= aid.tLastPos)
        return aid.tLastPosVal + aid.tLastSlope * (dt-aid.tLastPos);
    else
        return interpolate(dt, aid.longPos, aid.tLong, aid.tPosCount);
    }
else
    {
    int both = dq + dt;
    if (both < aid.smallSize)
        return aid.bSmall[both];
    else if (both >= aid.bLastPos)
        return aid.bLastPosVal + aid.bLastSlope * (both-aid.bLastPos);
    else
        return interpolate(both, aid.longPos, aid.bLong, aid.bPosCount);
    }
}

static int connCount = 0;
static int overlapCount = 0;

int connectCost(struct boxIn *a, struct boxIn *b)
/* Calculate connection cost - including gap score
 * and overlap adjustments if any. */
{
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;
int overlapAdjustment = 0;

if (a->qStart >= b->qStart || a->tStart >= b->tStart)
    {
    errAbort("a (%d %d) not strictly before b (%d %d)",
    	a->qStart, a->tStart, b->qStart, b->tStart);
    }
if (dq < 0 || dt < 0)
   {
   int bSize = b->qEnd - b->qStart;
   int aSize = a->qEnd - a->qStart;
   int overlap = -min(dq, dt);
   int crossover;
   if (overlap >= bSize || overlap >= aSize) 
       {
       /* One of the blocks is enclosed completely on one dimension
        * or other by the other.  Discourage this overlap. */
       overlapAdjustment = 100000000;
       }
   else
       {
       findCrossover(a, b, scoreData.qSeq, scoreData.tSeq, overlap, 
	    scoreData.ss->matrix,
	    &crossover, &overlapAdjustment);
       dq += overlap;
       dt += overlap;
       ++overlapCount;
       }
   }
++connCount;
return overlapAdjustment + gapCost(dq, dt);
}

void calcChainBounds(struct chain *chain)
/* Recalculate chain boundaries. */
{
struct boxIn *b = chain->blockList;
if (chain->blockList == NULL)
    return;
chain->qStart = b->qStart;
chain->tStart = b->tStart;
b = slLastEl(chain->blockList);
chain->qEnd = b->qEnd;
chain->tEnd = b->tEnd;
}

boolean removeNegativeBlocks(struct chain *chain)
/* Removing the partial overlaps occassional results
 * in all of a block being removed.  This routine
 * removes the dried up husks of these blocks 
 * and returns TRUE if it finds any. */
{
struct boxIn *newList = NULL, *b, *next;
boolean gotNeg = FALSE;
for (b = chain->blockList; b != NULL; b = next)
    {
    next = b->next;
    if (b->qStart >= b->qEnd || b->tStart >= b->tEnd)
	{
        gotNeg = TRUE;
	freeMem(b);
	}
    else
        {
	slAddHead(&newList, b);
	}
    }
slReverse(&newList);
chain->blockList = newList;
if (gotNeg)
    calcChainBounds(chain);
return gotNeg;
}

void mergeAbutting(struct chain *chain)
/* Merge together blocks in a chain that abut each
 * other exactly. */
{
struct boxIn *newList = NULL, *b, *last = NULL, *next;
for (b = chain->blockList; b != NULL; b = next)
    {
    next = b->next;
    if (last == NULL || last->qEnd != b->qStart || last->tEnd != b->tStart)
	{
	slAddHead(&newList, b);
	last = b;
	}
    else
        {
	last->qEnd = b->qEnd;
	last->tEnd = b->tEnd;
	freeMem(b);
	}
    }
slReverse(&newList);
chain->blockList = newList;
}


void checkChainIncreases(struct chain *chain, char *message)
/* Check that qStart and tStart both strictly increase
 * in chain->blockList. */
{
struct boxIn *a, *b;
a = chain->blockList;
if (a == NULL)
    return;
for (b = a->next; b != NULL; b = b->next)
    {
    if (a->qStart >= b->qStart || a->tStart >= b->tStart)
	{
	errAbort("a (%d %d) not before b (%d %d) %s",
	    a->qStart, a->tStart, b->qStart, b->tStart, message);
	}
    a = b;
    }
}

void removePartialOverlaps(struct chain *chain, 
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, int matrix[256][256])
/* If adjacent blocks overlap then find crossover points between them. */
{
struct boxIn *a, *b;
boolean totalTrimA, totalTrimB;

assert(chain->blockList != NULL);

/* Do an internal sanity check to make sure that things
 * really are sorted by both qStart and tStart. */
checkChainIncreases(chain, "before removePartialOverlaps");

/* Remove overlapping portion of blocks.  In some
 * tricky repeating regions this can result in 
 * complete blocks being removed.  This complicates
 * the loop below in some cases, forcing us to essentially
 * start over when the first of the two blocks we
 * are examining gets trimmed out completely. */
for (;;)
    {
    totalTrimA = totalTrimB = FALSE;
    a = chain->blockList;
    b = a->next;
    for (;;)
        {
	if (b == NULL)
	    break;
	int dq = b->qStart - a->qEnd;
	int dt = b->tStart - a->tEnd;
	if (dq < 0 || dt < 0)
	   {
	   int overlap = -min(dq, dt);
	   int aSize = a->qEnd - a->qStart;
	   int bSize = b->qEnd - b->qStart;
	   int crossover, invCross, overlapAdjustment;
	   if (overlap >= aSize || overlap >= bSize)
	       {
	       totalTrimB = TRUE;
	       }
	   else
	       {
	       findCrossover(a, b, scoreData.qSeq, scoreData.tSeq, overlap, 
		    scoreData.ss->matrix,
		    &crossover, &overlapAdjustment);
	       b->qStart += crossover;
	       b->tStart += crossover;
	       invCross = overlap - crossover;
	       a->qEnd -= invCross;
	       a->tEnd -= invCross;
	       if (b->qEnd <= b->qStart)
		   {
		   totalTrimB = TRUE;
		   }
	       else if (a->qEnd <= a->qStart)
		   {
		   totalTrimA = TRUE;
		   }
	       }
	   }
	if (totalTrimA == TRUE)
	    {
	    removeNegativeBlocks(chain);
	    break;
	    }
	else if (totalTrimB == TRUE)
	    {
	    b = b->next;
	    freez(&a->next);
	    a->next = b;
	    totalTrimB = FALSE;
	    }
	else
	    {
	    a = b;
	    b = b->next;
	    }
	}
    if (!totalTrimA)
        break;
    }

/* Do an internal sanity check to make sure that things
 * still are sorted by both qStart and tStart. */
checkChainIncreases(chain, "after removePartialOverlaps");

}

#ifdef TESTONLY
void abortChain(struct chain *chain, char *message)
/* Report chain problem and abort. */
{
errAbort("%s tName %s, tStart %d, tEnd %d, qStrand %c, qName %s, qStart %d, qEnd %d", message, chain->tName, chain->tStart, chain->tEnd, chain->qStrand, chain->qName, chain->qStart, chain->qEnd);
}

void checkChainScore(struct chain *chain, struct dnaSeq *qSeq, struct dnaSeq *tSeq)
/* Check that chain score is reasonable. */
{
struct boxIn *b;
int totalBases = 0;
double maxPerBase = 100, maxScore;
int gapCount = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    {
    int size = b->qEnd - b->qStart;
    if (size != b->tEnd - b->tStart)
        abortChain(chain, "q/t size mismatch");
    totalBases += b->qEnd - b->qStart;
    ++gapCount;
    }
maxScore = totalBases * maxPerBase;
if (maxScore < chain->score)
    {
    int gaplessScore = 0;
    int oneScore = 0;
    verbose(1, "maxScore %f, chainScore %f\n", maxScore, chain->score);
    for (b = chain->blockList; b != NULL; b = b->next)
        {
	int size = b->qEnd - b->qStart;
	oneScore = scoreBlock(qSeq->dna + b->qStart, tSeq->dna + b->tStart, size, scoreData.ss->matrix);
        verbose(1, " q %d, t %d, size %d, score %d\n",
             b->qStart, b->tStart, size, oneScore);
	gaplessScore += oneScore;
	}
    verbose(1, "gaplessScore %d\n", gaplessScore);
    abortChain(chain, "score too big");
    }
}
#endif /* TESTONLY */

double chainScore(struct chain *chain, struct dnaSeq *qSeq, struct dnaSeq *tSeq,
    int matrix[256][256], int (*gapCost)(int dt, int dq))
/* Calculate score of chain from scratch looking at blocks. */
{
struct boxIn *b, *a = NULL;
double score = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    {
    int size = b->qEnd - b->qStart;
    score += scoreBlock(qSeq->dna + b->qStart, tSeq->dna + b->tStart, 
    	size, matrix);
    if (a != NULL)
	score -= gapCost(b->tStart - a->tEnd, b->qStart - a->qEnd);
    a = b;
    }
return score;
}

void chainPair(struct seqPair *sp,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct chain **pChainList,
	FILE *details)
/* Chain up blocks and output. */
{
struct chain *chainList, *chain, *next;
struct boxIn *b;
long startTime, dt;

verbose(1, "chainPair %s\n", sp->name);

/* Set up info for connect function. */
scoreData.qSeq = qSeq;
scoreData.tSeq = tSeq;
scoreData.ss = scoreScheme;

/* Score blocks. */
for (b = sp->blockList; b != NULL; b = b->next)
    {
    int size = b->qEnd - b->qStart;
    b->score = axtScoreUngapped(scoreData.ss, 
    	qSeq->dna + b->qStart, tSeq->dna + b->tStart, size);
    }


/* Get chain list and clean it up a little. */
startTime = clock1000();
chainList = chainBlocks(sp->qName, qSeq->size, sp->qStrand, 
	sp->tName, tSeq->size, &sp->blockList, connectCost, gapCost, details);
dt = clock1000() - startTime;
verbose(1, "Main chaining step done in %ld milliseconds\n", dt);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    removePartialOverlaps(chain, qSeq, tSeq, scoreData.ss->matrix);
    mergeAbutting(chain);
    chain->score = chainScore(chain, qSeq, tSeq, scoreData.ss->matrix, gapCost);
    }

/* Move chains scoring over threshold to master list. */
for (chain = chainList; chain != NULL; chain = next)
    {
    next = chain->next;
    if (chain->score >= minScore)
        {
	slAddHead(pChainList, chain);
	}
    else 
        {
	chainFree(&chain);
	}
    }
}

struct seqPair *readAxtBlocks(char *fileName, struct hash *pairHash)
/* Read in axt file and parse blocks into pairHash */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct dyString *dy = newDyString(512);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
while ((axt = axtRead(lf)) != NULL)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(axt->qName);
	sp->tName = cloneString(axt->tName);
	sp->qStrand = axt->qStrand;
	}
    addAxtBlocks(&sp->blockList, axt);
    sp->axtCount += 1;
    axtFree(&axt);
    }
lineFileClose(&lf);
dyStringFree(&dy);
slSort(&spList, seqPairCmp);
return spList;
}

struct seqPair *readPslBlocks(char *fileName, struct hash *pairHash)
/* Read in psl file and parse blocks into pairHash */
{
struct seqPair *spList = NULL, *sp;
struct lineFile *lf = pslFileOpen(fileName);
struct dyString *dy = newDyString(512);
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%s%s", psl->qName, psl->strand, psl->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(psl->qName);
	sp->tName = cloneString(psl->tName);
	sp->qStrand = psl->strand[0];
	}
    addPslBlocks(&sp->blockList, psl);
    sp->axtCount += 1;
    pslFree(&psl);
    }

lineFileClose(&lf);
dyStringFree(&dy);
return spList;
}

void axtChain(char *axtIn, char *tNibDir, char *qNibDir, char *chainOut)
/* axtChain - Chain together axt alignments.. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(chainOut, "w");
char *qName = "",  *tName = "";
struct dnaSeq *qSeq = NULL, *tSeq = NULL;
char qStrand = 0, tStrand = 0;
struct chain *chainList = NULL, *chain;
FILE *details = NULL;
struct lineFile *lf = NULL;
struct dnaSeq *seq, *seqList = NULL;
struct hash *faHash = newHash(0);
char comment[1024];
FILE *faF;

if (detailsName != NULL)
    details = mustOpen(detailsName, "w");
/* Read input file and divide alignments into various parts. */
if (optionExists("psl"))
    spList = readPslBlocks(axtIn, pairHash);
else
    spList = readAxtBlocks(axtIn, pairHash);

if (optionExists("faQ"))
    {
    faF = mustOpen(qNibDir, "r");
    while ( faReadMixedNext(faF, TRUE, NULL, TRUE, NULL, &seq))
        {
        hashAdd(faHash, seq->name, seq);
        slAddHead(&seqList, seq);
        }
    fclose(faF);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->blockList);
    removeExactOverlaps(&sp->blockList);
    verbose(1, "%d blocks after duplicate removal\n", slCount(sp->blockList));
    if (optionExists("faQ"))
        {
        assert (faHash != NULL);
        loadFaSeq(faHash, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
        }
    else
        loadIfNewSeq(qNibDir, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
    loadIfNewSeq(tNibDir, sp->tName, '+', &tName, &tSeq, &tStrand);
    chainPair(sp, qSeq, tSeq, &chainList, details);
    }
slSort(&chainList, chainCmpScore);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    assert(chain->qStart == chain->blockList->qStart 
	&& chain->tStart == chain->blockList->tStart);
    chainWrite(chain, f);
    }

carefulClose(&f);
}

void testGaps()
{
int i;
for (i=1; i<=10; i++)
   {
   verbose(1, "%d: %d %d %d\n", i, gapCost(i, 0), gapCost(0, i), gapCost(i/2, i-i/2));
   }
for (i=1; ; i *= 10)
   {
   verbose(1, "%d: %d %d %d\n", i, gapCost(i, 0), gapCost(0, i), gapCost(i/2, i-i/2));
   if (i == 1000000000)
       break;
   }
verbose(1, "%d %d cost %d\n", 6489540, 84240, gapCost(84240, 6489540));
verbose(1, "%d %d cost %d\n", 2746361, 1075188, gapCost(1075188, 2746361));
verbose(1, "%d %d cost %d\n", 6489540 + 2746361 + 72, 84240 + 1075188 + 72, gapCost(84240 + 1075188 + 72, 6489540 + 2746361 + 72));
}


int main(int argc, char *argv[])
/* Process command line. */
{
char *scoreSchemeName = NULL;
optionHash(&argc, argv);
minScore = optionInt("minScore", minScore);
detailsName = optionVal("details", NULL);
gapFileName = optionVal("linearGap", NULL);
scoreSchemeName = optionVal("scoreScheme", NULL);
if (scoreSchemeName != NULL)
    {
    printf("Reading scoring matrix from %s\n", scoreSchemeName);
    scoreScheme = axtScoreSchemeRead(scoreSchemeName);
    }
else
    scoreScheme = axtScoreSchemeDefault();
dnaUtilOpen();
initGapAid(gapFileName);
/* testGaps(); */
if (argc != 5)
    usage();
axtChain(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
