/* pslPseudo - analyse repeats and generate list of processed pseudogenes
 * from a genome wide, sorted by mRNA .psl alignment file.
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
//#include "jksql.h"
//#include "hdb.h"
#include "psl.h"
#include "bed.h"
#include "axt.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "dlist.h"
#include "binRange.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"
#include "dystring.h"
#include "pseudoGeneLink.h"
//#include "scoreWindow.h" TODO fix this
#include "verbose.h"

#define ScoreNorm 3
#define BEDCOUNT 3
#define POLYASLIDINGWINDOW 10
#define POLYAREGION 70
#define INTRONMAGIC 10 /* allow more introns if lots of exons covered - if (exonCover - intronCount > INTRONMAGIC) */
/* label for classification stored in pseudoGeneLink table */
#define PSEUDO 1
#define NOTPSEUDO -1
#define EXPRESSED -2

static char const rcsid[] = "$Id: pslPseudo.c,v 1.25 2004/08/16 00:41:31 baertsch Exp $";

char *db;
char *nibDir;
char *mrnaSeq;
float minAli = 0.98;
float maxRep = 0.60;
float minAliPseudo = 0.60;
float nearTop = 0.005;
float repsPerIntron = 0.7;
float splicedOverlapRatio = 0.5;
float minCover = 0.50;
float minCoverPseudo = 0.01;
int maxBlockGap = 60;
int intronSlop = 30;
float intronRatio = 1.5;
boolean ignoreSize = FALSE;
boolean noIntrons = FALSE;
boolean singleHit = FALSE;
boolean noHead = FALSE;
boolean quiet = FALSE;
boolean skipExciseRepeats = FALSE;
double  wt[11];     /* weights on score function*/
int minNearTopSize = 10;
struct genePred *gpList1 = NULL, *gpList2 = NULL, *kgList = NULL;
FILE *bestFile, *pseudoFile, *linkFile, *axtFile;
struct axtScoreScheme *ss = NULL; /* blastz scoring matrix */
struct dnaSeq *mrnaList = NULL; /* list of all input mrna sequences */
struct hash *tHash = NULL;  /* seqFilePos value. */
struct hash *qHash = NULL;  /* seqFilePos value. */
struct dlList *fileCache = NULL;
struct hash *fileHash = NULL;  
char mrnaOverlap[255];

struct seqFilePos
/* Where a sequence is in a file. */
    {
    struct filePos *next;	/* Next in list. */
    char *name;	/* Sequence name. Allocated in hash. */
    char *file;	/* Sequence file name, allocated in hash. */
    long pos; /* Position in fa file/size of nib. */
    bool isNib;	/* True if a nib file. */
    };
void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "pslPseudo - analyse repeats and generate genome wide best\n"
    "alignments from a sorted set of local alignments\n"
    "usage:\n"
    "    pslPseudo db in.psl sizes.lst rmsk.bed trf.bed syntenic.bed mrna.psl out.psl pseudo.psl pseudoLink.txt nib.lst mrna.fa refGene.tab mgcGene.tab kglist.tab \n\n"
    "where in.psl is an blat alignment of mrnas sorted by pslSort\n"
    "blastz.psl is an blastz alignment of mrnas sorted by pslSort\n"
    "sizes.lst is a list of chrromosome followed by size\n"
    "rmsk.bed is the repeat masker bed file\n"
    "trf.bed is the simple repeat (trf) bed file\n"
    "mrna.psl is the blat best mrna alignments\n"
    "out.psl is the best mrna alignment for the gene \n"
    "and pseudo.psl contains pseudogenes\n"
    "pseudoLink.txt will have the link between gene and pseudogene\n"
    "nib.lst list of genome nib file\n"
    "mrna.fa sequence data for all aligned mrnas using blastz\n"
    "options:\n"
    "    -nohead don't add PSL header\n"
    "    -ignoreSize Will not weigh in favor of larger alignments so much\n"
    "    -noIntrons Will not penalize for not having introns when calculating\n"
    "              size factor\n"
    "    -minCover=0.N minimum coverage for mrna to output.  Default is 0.50\n"
    "    -minCoverPseudo=0.N minimum coverage of pseudogene to output.  Default is 0.01\n"
    "    -minAli=0.N minimum alignment ratio for mrna\n"
    "               default is 0.98\n"
    "    -minAliPseudo=0.N minimum alignment ratio for pseudogenes\n"
    "               default is 0.60\n"
    "    -splicedOverlapRatio=0.N max overlap with spliced mrna\n"
    "               default is 0.50\n"
    "    -intronSlop=N max delta of intron position on q side alignment\n"
    "               default is 30 bp\n"
    "    -nearTop=0.N how much can deviate from top and be taken\n"
    "               default is 0.01\n"
    "    -minNearTopSize=N  Minimum size of alignment that is near top\n"
    "               for aligmnent to be kept.  Default 10.\n"
    "    -maxBlockGap=N  Max gap size between adjacent blocks that are combined. \n"
    "               Default 60.\n"
    "    -maxRep=N  max ratio of overlap with repeat masker track \n"
    "               for aligmnent to be kept.  Default .70\n");
}

struct cachedFile
/* File in cache. */
    {
    struct cachedFile *next;	/* next in list. */
    char *name;		/* File name (allocated here) */
    FILE *f;		/* Open file. */
    };

boolean isFa(char *file)
/* Return TRUE if looks like a .fa file. */
{
FILE *f = mustOpen(file, "r");
int c = fgetc(f);
fclose(f);
return c == '>';
}

void addNib(char *file, struct hash *fileHash, struct hash *seqHash)
/* Add a nib file to hashes. */
{
struct seqFilePos *sfp;
char root[128];
int size;
FILE *f = NULL;
splitPath(file, NULL, root, NULL);
AllocVar(sfp);
hashAddSaveName(seqHash, root, sfp, &sfp->name);
sfp->file = hashStoreName(fileHash, file);
sfp->isNib = TRUE;
nibOpenVerify(file, &f, &size);
sfp->pos = size;
fclose(f);
}

void addFa(char *file, struct hash *fileHash, struct hash *seqHash)
/* Add a fa file to hashes. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line, *name;
char *rFile = hashStoreName(fileHash, file);

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	struct seqFilePos *sfp;
	line += 1;
	name = nextWord(&line);
	if (name == NULL)
	   errAbort("bad line %d of %s", lf->lineIx, lf->fileName);
	AllocVar(sfp);
	hashAddSaveName(seqHash, name, sfp, &sfp->name);
	sfp->file = rFile;
	sfp->pos = lineFileTell(lf);
	}
    }
lineFileClose(&lf);
}

void hashFileList(char *fileList, struct hash *fileHash, struct hash *seqHash)
/* Read file list into hash */
{
if (endsWith(fileList, ".nib"))
    addNib(fileList, fileHash, seqHash);
else if (isFa(fileList))
    addFa(fileList, fileHash, seqHash);
else
    {
    struct lineFile *lf = lineFileOpen(fileList, TRUE);
    char *row[1];
    while (lineFileRow(lf, row))
        {
	char *file = row[0];
	if (endsWith(file, ".nib"))
	    addNib(file, fileHash, seqHash);
	else
	    addFa(file, fileHash, seqHash);
	}
    lineFileClose(&lf);
    }
}

FILE *openFromCache(struct dlList *cache, char *fileName)
/* Return open file handle via cache.  The simple logic here
 * depends on not more than N files being returned at once. */
{
static int maxCacheSize=32;
int cacheSize = 0;
struct dlNode *node;
struct cachedFile *cf;

/* First loop through trying to find it in cache, counting
 * cache size as we go. */
for (node = cache->head; !dlEnd(node); node = node->next)
    {
    ++cacheSize;
    cf = node->val;
    if (sameString(fileName, cf->name))
        {
	dlRemove(node);
	dlAddHead(cache, node);
	return cf->f;
	}
    }

/* If cache has reached max size free least recently used. */
if (cacheSize >= maxCacheSize)
    {
    node = dlPopTail(cache);
    cf = node->val;
    carefulClose(&cf->f);
    freeMem(cf->name);
    freeMem(cf);
    freeMem(node);
    }

/* Cache new file. */
AllocVar(cf);
cf->name = cloneString(fileName);
cf->f = mustOpen(fileName, "rb");
dlAddValHead(cache, cf);
return cf->f;
}

struct dnaSeq *readCachedSeq(char *seqName, struct hash *hash, 
	struct dlList *fileCache)
/* Read sequence hopefully using file cache. */
{
struct dnaSeq *mrna = NULL;
struct dnaSeq *qSeq = NULL;
assert(seqName != NULL);
struct seqFilePos *sfp = hashMustFindVal(hash, seqName);
FILE *f = openFromCache(fileCache, sfp->file);
unsigned options = NIB_MASK_MIXED;
if (sfp->isNib)
    {
    return nibLdPartMasked(options, sfp->file, f, sfp->pos, 0, sfp->pos);
    }
else
    {
    for (mrna = mrnaList; mrna != NULL ; mrna = mrna->next)
        if (sameString(mrna->name, seqName))
            {
            qSeq = mrna;
            break;
            }
    return qSeq;
    }
}

struct dnaSeq *readSeqFromFaPos(struct seqFilePos *sfp,  FILE *f)
/* Read part of FA file. */
{
struct dnaSeq *seq;
fseek(f, sfp->pos, SEEK_SET);
if (!faReadNext(f, "", TRUE, NULL, &seq))
    errAbort("Couldn't faReadNext on %s in %s\n", sfp->name, sfp->file);
return seq;
}

void readCachedSeqPart(char *seqName, int start, int size, 
     struct hash *hash, struct dlList *fileCache, 
     struct dnaSeq **retSeq, int *retOffset, boolean *retIsNib)
/* Read sequence hopefully using file cashe. If sequence is in a nib
 * file just read part of it. */
{
struct seqFilePos *sfp = hashMustFindVal(hash, seqName);
FILE *f = openFromCache(fileCache, sfp->file);
unsigned options = NIB_MASK_MIXED;
if (sfp->isNib)
    {
    *retSeq = nibLdPartMasked(options, sfp->file, f, sfp->pos, start, size);
    *retOffset = start;
    *retIsNib = TRUE;
    }
else
    {
    *retSeq = readSeqFromFaPos(sfp, f);
    *retOffset = 0;
    *retIsNib = FALSE;
    }
}

struct hash *readBedCoordToBinKeeper(char *sizeFileName, char *bedFileName, int wordCount)
/* read a list of beds and return results in hash of binKeeper structure for fast query*/
/* free bed in binKeeper to save memory only start/end coord */
{
struct binKeeper *bk; 
struct bed *bed;
struct lineFile *lf = lineFileOpen(sizeFileName, TRUE);
struct lineFile *bf = lineFileOpen(bedFileName , TRUE);
struct hash *hash = newHash(0);
char *chromRow[2];
char *row[3] ;

assert (wordCount == 3);
while (lineFileRow(lf, chromRow))
    {
    char *name = chromRow[0];
    int size = lineFileNeedNum(lf, chromRow, 1);

    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
        {
        bk = binKeeperNew(0, size);
        assert(size > 1);
	hashAdd(hash, name, bk);
        }
    }
while (lineFileNextRow(bf, row, ArraySize(row)))
    {
    bed = bedLoadN(row, wordCount);
    bk = hashMustFindVal(hash, bed->chrom);
    binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
    bedFree(&bed);
    }
lineFileClose(&bf);
lineFileClose(&lf);
return hash;
}
struct axt *axtCreate(char *q, char *t, int size, struct psl *psl)
/* create axt */
{
int qs = psl->qStart, qe = psl->qEnd;
int ts = psl->tStart, te = psl->tEnd;
int symCount = 0;
struct axt *axt = NULL;

AllocVar(axt);
if (psl->strand[0] == '-')
    reverseIntRange(&qs, &qe, psl->qSize);

if (psl->strand[1] == '-')
    reverseIntRange(&ts, &te, psl->tSize);

axt->qName = cloneString(psl->qName);
axt->tName = cloneString(psl->tName);
axt->qStart = qs+1;
axt->qEnd = qe;
axt->qStrand = psl->strand[0];
axt->tStrand = '+';
if (psl->strand[1] != 0)
    {
    axt->tStart = ts+1;
    axt->tEnd = te;
    }
else
    {
    axt->tStart = psl->tStart+1;
    axt->tEnd = psl->tEnd;
    }
axt->symCount = symCount = strlen(t);
axt->tSym = cloneString(t);
if (strlen(q) != symCount)
    warn("Symbol count %d != %d inconsistent at t %s:%d and qName %s\n%s\n%s\n",
    	symCount, strlen(q), psl->tName, psl->tStart, psl->qName, t, q);
axt->qSym = cloneString(q);
axt->score = axtScoreFilterRepeats(axt, ss);
verbose(1,"axt score = %d\n",axt->score);
//for (i=0; i<size ; i++) 
//    fputc(t[i],f);
//for (i=0; i<size ; i++) 
//    fputc(q[i],f);
return axt;
}

void writeInsert(struct dyString *aRes, struct dyString *bRes, char *aSeq, int gapSize)
/* Write out gap, possibly shortened, to aRes, bRes. */
{
dyStringAppendN(aRes, aSeq, gapSize);
dyStringAppendMultiC(bRes, '-', gapSize);
}


void writeGap(struct dyString *aRes, int aGap, char *aSeq, struct dyString *bRes, int bGap, char *bSeq)
/* Write double - gap.  Something like:
 *         --c
 *         ag-  */

{
dyStringAppendMultiC(aRes, '-', bGap);
dyStringAppendN(bRes, bSeq, bGap);
dyStringAppendN(aRes, aSeq, aGap);
dyStringAppendMultiC(bRes, '-', aGap);
}

struct axt *pslToAxt(struct psl *psl, struct hash *qHash, struct hash *tHash,
	struct dlList *fileCache)
{
static char *tName = NULL, *qName = NULL;
static struct dnaSeq *tSeq = NULL, *qSeq = NULL, *mrna;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
int blockIx;
int qs, ts ;
int lastQ = 0, lastT = 0, size;
int qOffset = 0;
int tOffset = 0;
struct axt *axt = NULL;
boolean qIsNib = FALSE;
boolean tIsNib = FALSE;
int cnt = 0;

freeDnaSeq(&qSeq);
freez(&qName);
assert(mrnaList != NULL);
for (mrna = mrnaList; mrna != NULL ; mrna = mrna->next)
    {
    assert(mrna != NULL);
    cnt++;
    if (sameString(mrna->name, psl->qName))
        {
        qSeq = cloneDnaSeq(mrna);
        assert(qSeq != NULL);
        break;
        }
    }
if (qSeq == NULL)
    errAbort("mrna sequence data not found %s, searched %d sequences\n",psl->qName,cnt);
if (qSeq->size != psl->qSize)
    {
    warn("sequence %s aligned is different size %d from mrna.fa file %d \n",psl->qName,psl->qSize,qSeq->size);
    dyStringFree(&q);
    dyStringFree(&t);
    dnaSeqFree(&tSeq);
    dnaSeqFree(&qSeq);
    return NULL;
    }
qName = cloneString(psl->qName);
if (qIsNib && psl->strand[0] == '-')
    qOffset = psl->qSize - psl->qEnd;
else
    qOffset = 0;
verbose(3,"qString len = %d qOffset = %d\n",strlen(qSeq->dna),qOffset);
if (tName == NULL || !sameString(tName, psl->tName) || tIsNib)
    {
    freeDnaSeq(&tSeq);
    freez(&tName);
    tName = cloneString(psl->tName);
    readCachedSeqPart(tName, psl->tStart, psl->tEnd-psl->tStart, 
	tHash, fileCache, &tSeq, &tOffset, &tIsNib);
    }
if (tIsNib && psl->strand[1] == '-')
    tOffset = psl->tSize - psl->tEnd;
verbose(3,"tString len = %d tOffset = %d\n",strlen(tSeq->dna),tOffset);
if (psl->strand[0] == '-')
    reverseComplement(qSeq->dna, qSeq->size);
if (psl->strand[1] == '-')
    reverseComplement(tSeq->dna, tSeq->size);
for (blockIx=0; blockIx < psl->blockCount; ++blockIx)
    {
    qs = psl->qStarts[blockIx] - qOffset;
    ts = psl->tStarts[blockIx] - tOffset;

    if (blockIx != 0)
        {
	int qGap, tGap, minGap;
	qGap = qs - lastQ;
	tGap = ts - lastT;
	minGap = min(qGap, tGap);
	if (minGap > 0)
	    {
	    writeGap(q, qGap, qSeq->dna + lastQ, t, tGap, tSeq->dna + lastT);
	    }
	else if (qGap > 0)
	    {
	    writeInsert(q, t, qSeq->dna + lastQ, qGap);
	    }
	else if (tGap > 0)
	    {
	    writeInsert(t, q, tSeq->dna + lastT, tGap);
	    }
	}
    size = psl->blockSizes[blockIx];
    assert(qSeq != NULL);
    dyStringAppendN(q, qSeq->dna + qs, size);
    lastQ = qs + size;
    dyStringAppendN(t, tSeq->dna + ts, size);
    lastT = ts + size;
    }

if (strlen(q->string) != strlen(t->string))
    warn("Symbol count(t) %d != %d inconsistent at t %s:%d and qName %s\n%s\n%s\n",
    	strlen(t->string), strlen(q->string), psl->tName, psl->tStart, psl->qName, t->string, q->string);
if (psl->strand[0] == '-')
    {
    reverseComplement(q->string, q->stringSize);
    reverseComplement(t->string, t->stringSize);
    }
axt = axtCreate(q->string, t->string, min(q->stringSize,t->stringSize), psl);
dyStringFree(&q);
dyStringFree(&t);
dnaSeqFree(&tSeq);
dnaSeqFree(&qSeq);
if (qIsNib)
    freez(&qName);
if (tIsNib)
    freez(&tName);
return axt;
}

void binKeeperPslHashFree(struct hash **hash)
{
if (*hash != NULL)
    {
    struct hashEl *hashEl = NULL;
    struct hashCookie cookie = hashFirst(*hash);
    while ((hashEl = hashNext(&cookie)) != NULL)
        {
        struct binKeeper *bk = hashEl->val;
        struct binElement *elist = NULL, *el = NULL;;
        elist = binKeeperFindAll(bk) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            struct psl *psl = el->val;
            pslFreeList(&psl);
            }
        binKeeperFree(&bk);
        }
    hashFree(hash);
    }
}

void binKeeperHashFree(struct hash **hash)
{
if (*hash != NULL)
    {
    struct hashEl *hashEl = NULL;
    struct hashCookie cookie = hashFirst(*hash);
    while ((hashEl = hashNext(&cookie)) != NULL)
        {
        struct binKeeper *bk = hashEl->val;
        binKeeperFree(&bk);
        }
    hashFree(hash);
    }
}

int calcMilliScore(struct psl *psl)
/* Figure out percentage score. */
{
return 1000-pslCalcMilliBad(psl, TRUE);
}

void outputNoLinkScore(struct psl *psl, struct pseudoGeneLink *pg, int pseudoScore)
    
   /* char *type, char *bestqName, char *besttName, 
                int besttStart, int besttEnd, int maxExons, int geneOverlap, 
                char *bestStrand, int polyA, int polyAstart, int label, 
                int exonCover, int intronCount, int bestAliCount, 
                int rep, int qReps, int overlapDiagonal, int pseudoScore, 
                int oldIntronCount, struct dyString *iString, int conservedIntrons, int maxOverlap) */
/* output bed record with pseudogene details */
{
//struct bed *bed = bedFromPsl(psl);
int blockCount;
int i, *chromStarts, chromStart;
if (pseudoScore > 0)
    pg->score = pseudoScore;
else 
    pg->score = 0;
pg->milliBad = calcMilliScore(psl);
pg->coverage = ((psl->match+psl->repMatch)*100)/psl->qSize;
pg->oldScore = (pg->milliBad - (100-log(pg->polyA)*20) - (pg->overlapDiag*2) - (100-pg->coverage) 
        + log(psl->match+psl->repMatch)*100)/2 ;

pg->chrom = cloneString(psl->tName);
pg->chromStart = pg->thickStart = chromStart = psl->tStart;
pg->chromEnd = pg->thickEnd = psl->tEnd;
strncpy(pg->strand,  psl->strand, sizeof(pg->strand));
pg->blockCount = blockCount = psl->blockCount;
pg->blockSizes = (int *)cloneMem(psl->blockSizes,(sizeof(int)*psl->blockCount));
pg->chromStarts = chromStarts = (int *)cloneMem(psl->tStarts, (sizeof(int)*psl->blockCount));
pg->name = cloneString(psl->qName);

/* Switch minus target strand to plus strand. */
if (psl->strand[1] == '-')
    {
    int chromSize = psl->tSize;
    reverseInts(pg->blockSizes, blockCount);
    reverseInts(chromStarts, blockCount);
    for (i=0; i<blockCount; ++i)
	chromStarts[i] = chromSize - chromStarts[i];
    }

/* Convert coordinates to relative. */
for (i=0; i<blockCount; ++i)
    chromStarts[i] -= chromStart;

//bedOutputN( bed , 12, linkFile, '\t', '\t');
if (pg->type == NULL)
    pg->type = cloneString("NONE");
if (strlen(pg->type)<=1)
    pg->type = cloneString("NONE");
if (pg->gChrom==NULL)
    pg->gChrom = cloneString("NONE");
if (pg->gStrand[0] != '+' && pg->gStrand[0] == '-') 
    {
    pg->gStrand[0] = '0'; 
    pg->gStrand[1] = '\0'; 
    }
pg->polyAstart = (psl->strand[0] == '+') ? pg->polyAstart - psl->tEnd : psl->tStart - pg->polyAstart ;
pg->matches = psl->match+psl->repMatch ;
pg->qSize = psl->qSize;
pg->qEnd = psl->qSize - psl->qEnd;
if (pg->intronScores == NULL)
    pg->intronScores = cloneString("0,0");
if (pg->overName == NULL)
    pg->overName = cloneString("none");
if (strlen(pg->overStrand) < 1)
    {
    pg->overStrand[0] = '0'; 
    pg->overStrand[1] = '\0'; 
    }
pseudoGeneLinkOutput(pg, linkFile, '\t','\n');
        /* 13        14   15        16         17           18  */
//        trfRatio, type, bestqName, besttName, besttStart, besttEnd, 
//        (bestStrand[0] == '+' || bestStrand[0] == '-') ? bestStrand : "0", 
        /* 20           21      22 */
//        maxExons, geneOverlap, polyA, 
        /* polyAstart 23*/
//        (psl->strand[0] == '+') ? polyAstart - psl->tEnd : psl->tStart - polyAstart , 
        /* 24           25              26      27 */
//        exonCover, intronCount, bestAliCount, psl->match+psl->repMatch, 
        /* 28           29                30    31      32 */
//        psl->qSize, psl->qSize-psl->qEnd, rep, qReps, overlapDiagonal, 
        /* 33      34      35      36         37              38,                   39       40               41=axtScore*/
//        coverage, label, milliBad, oldScore, oldIntronCount, conservedIntrons, maxOverlap, iString->string != NULL ? iString->string :"0,0");
}

void initWeights()
{
/*
    0 = + milliBad
    1 = + exon Coverage
    2 = + log axtScore
    3 = + log polyA
    4 = - overlapDiag
    5 = NOT USED log (qSize - qEnd)
    6 = + intronCount ^.5
    7 = + log maxOverlap
    8 = + coverage *((qSize-qEnd)/qSize)
    9 = - repeats
 */
wt[0] = 0.3; wt[1] = 0.75; wt[2] = 0.9; wt[3] = 0.5; wt[4] = 2; 
wt[5] = 0; wt[6] = 1  ; wt[7] = 0.5; wt[8] = 0.7; wt[9] = 1;
}
void outputLink(struct psl *psl, struct pseudoGeneLink *pg , struct dyString *reason)
   /* char *type, char *bestqName, char *besttName, 
                int besttStart, int besttEnd, int maxExons, int geneOverlap, 
                char *bestStrand, int polyA, int polyAstart, int label, 
                int exonCover, int intronCount, int bestAliCount, 
                int tReps, int qReps, int overlapDiagonal, 
                struct genePred *kg, struct genePred *mgc, struct genePred *gp, 
                int oldIntronCount, struct dyString *iString, int conservedIntrons, int maxOverlap) */
/* output bed record with pseudogene details and link to gene*/
{
struct axt *axt = NULL;
int pseudoScore = 0;
pg->milliBad = calcMilliScore(psl);
pg->axtScore = -1;
pg->type = reason->string;


if (pg->label == PSEUDO || pg->label == EXPRESSED)
    {
    pslTabOut(psl, pseudoFile);
    axt = pslToAxt(psl, qHash, tHash, fileCache);
    if (axt != NULL)
        {
        pg->axtScore = axtScoreFilterRepeats(axt, ss);
        axtWrite(axt, axtFile);
        axtFree(&axt);
        }
    }

/* all weighted features are scaled to range from 0 to 1000 */
assert(psl->qSize > 0);
pseudoScore = ( wt[0]*pg->milliBad 
                + wt[1]*(log(pg->exonCover+1)/log(2))*200 
                + wt[2]*log(pg->axtScore>0?pg->axtScore:1)*70 
                + wt[3]*(log(pg->polyA+2)*200) 
                - wt[4]*(pg->overlapDiag*10)
                + wt[5]*(12-log(psl->qSize-psl->qEnd))*80 
                + wt[6]*pow(pg->intronCount,0.5)*2000 
                + wt[7]*(12-log(pg->maxOverlap+1))*80 
                + wt[8]*(pg->coverage*((psl->qSize-psl->qEnd)/psl->qSize)*10)
                - wt[9]*(pg->tReps*10)
                ) / ScoreNorm;
verbose(1,"##score %d %4.1f %4.1f %4.1f %4.1f %4.1f %4.1f %4.1f %4.1f %4.1f %4.1f %s %d %s\n", pseudoScore, 
                wt[0]*pg->milliBad, 
                wt[1]*(log(pg->exonCover+1)/log(2))*200 , 
                wt[2]*log(pg->axtScore>0?pg->axtScore:1)*70 , 
                wt[3]*(log(pg->polyA+2)*200) ,
                wt[4]*(pg->overlapDiag*10) ,
                wt[5]*(12-log(psl->qSize-psl->qEnd))*80 , 
                wt[6]*pow(pg->intronCount,0.5)*2000 ,
                wt[7]*(12-log(pg->maxOverlap+1))*80 ,
                wt[8]*(pg->coverage*((psl->qSize-psl->qEnd)/psl->qSize)*10),
                wt[9]*(pg->tReps*10), 
                psl->qName, ScoreNorm, pg->type
                ) ;

outputNoLinkScore(psl, pg, pseudoScore);
}

int intronFactor(struct psl *psl, struct hash *bkHash, struct hash *trfHash)
/* Figure approx number of introns.  
 * An intron in this case is just a gap of 0 bases in query and
 * maxBlockGap or more in target iwth repeats masked. */
{
int i, blockCount = psl->blockCount;
int ts, qs, te, qe, sz, tsRegion, teRegion;
struct binKeeper *bk;
int intronCount = 0, tGap;

assert (psl != NULL);
if (blockCount <= 1)
    return 0;
sz = psl->blockSizes[0];
qe = psl->qStarts[0] + sz;
te = psl->tStarts[0] + sz;
tsRegion = psl->tStarts[0];
for (i=1; i<blockCount; ++i)
    {
    int trf = 0;
    int reps = 0, regionReps = 0;
    struct binElement *el, *elist;
    qs = psl->qStarts[i];
    ts = psl->tStarts[i];
    teRegion = psl->tStarts[i] + psl->blockSizes[i];

    if (bkHash != NULL)
        {
        if (trfHash != NULL)
            {
            bk = hashFindVal(trfHash, psl->tName);
            elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
            trf = 0;
            for (el = elist; el != NULL ; el = el->next)
                {
                //bed = el->val;
                trf += positiveRangeIntersection(te, ts, el->start, el->end);
                }
            slFreeList(&elist);
            }
        bk = hashFindVal(bkHash, psl->tName);
        elist = binKeeperFindSorted(bk, tsRegion , teRegion ) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            /* count repeats in the gap */
            reps += positiveRangeIntersection(te, ts, el->start, el->end);
            /* count repeats in the gap  and surrounding exons  */
            regionReps += positiveRangeIntersection(tsRegion, teRegion, el->start, el->end);
            }
        if (elist != NULL)
            slFreeList(&elist);
        }
    /* don't subtract repeats if the entire region is masked */
    if ( regionReps + 10 >= teRegion - tsRegion )
        reps = 0;

    tGap = ts - te - reps*1.5 - trf;
    verbose(3,"%s:%d %d tGap %d ts %d te %d reps %d trf %d qs %d qe %d max %d\n",
            psl->tName, psl->tStart,2*abs(qs-qe),tGap, te, te, reps, trf, qs,qe, maxBlockGap);
    if (2*abs(qs - qe) <= tGap && tGap >= maxBlockGap ) 
        /* don't count q gaps in mrna as introns , if they are similar in size to tGap*/
        {
        intronCount++;
        }
    assert(psl != NULL);
    sz = psl->blockSizes[i];
    qe = qs + sz;
    te = ts + sz;
    tsRegion = psl->tStarts[i];
    }
return intronCount;
}

int sizeFactor(struct psl *psl, struct hash *bkHash, struct hash *trfHash)
/* Return a factor that will favor longer alignments. An intron is worth 3 bases...  */
{
int score;
if (ignoreSize) return 0;
assert(psl != NULL);
score = 4*round(sqrt(psl->match + psl->repMatch/4));
if (!noIntrons)
    {
    int bonus = intronFactor(psl, bkHash, trfHash) * 3;
    if (bonus > 10) bonus = 10;
    score += bonus;
    }
return score;
}

int calcSizedScore(struct psl *psl, struct hash *bkHash, struct hash *trfHash)
/* Return score that includes base matches and size. */
{
int score = calcMilliScore(psl) + sizeFactor(psl, bkHash, trfHash);
return score;
}

boolean closeToTop(struct psl *psl, int *scoreTrack , int milliScore) /*struct hash *bkHash, struct hash *trfHash)*/
/* Returns TRUE if psl is near the top scorer for at least 20 bases. */
{
int threshold = round(milliScore * (1.0+nearTop));
int i, blockIx;
int start, size, end;
int topCount = 0;
char strand = psl->strand[0];

verbose(3,"%s:%d milliScore %d, threshold %d\n", psl->tName, psl->tStart, milliScore, threshold);
for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
    {
    start = psl->qStarts[blockIx];
    size = psl->blockSizes[blockIx];
    end = start+size;
    if (strand == '-')
	reverseIntRange(&start, &end, psl->qSize);
    verbose(3,"block %d threshold=%d s-e:%d-%d. ",blockIx, threshold, start, end);
    for (i=start; i<end; ++i)
	{
        verbose(3,"s=%d tc=%d ",scoreTrack[i],topCount);
	if (scoreTrack[i] <= threshold)
	    {
	    if (++topCount >= minNearTopSize)
                {
		return TRUE;
                }
	    }
	}
    }
return FALSE;
}

//#ifdef NEVER
int scoreWindow(char c, char *s, int size, int *score, int *start, int *end, int match, int misMatch)
/* calculate score array with score at each position in s, match to char c adds 1 to score, mismatch adds -1 */
/* index of max score is returned , size is size of s */
{
int i=0, j=0, max=2, count = 0; 

*end = 0;

assert(match >= 0);
assert(misMatch <= 0);
for (i=0 ; i<size ; i++)
    {
    int prevScore = (i > 0) ? score[i-1] : 0;

    if (toupper(s[i]) == toupper(c) )
        score[i] = prevScore+match;
    else
        score[i] = prevScore+misMatch;
    if (score[i] >= max)
        {
        max = score[i];
        *end = i;
        for (j=i ; j>=0 ; j--)
            if (score[j] == 0)
                {
                *start = j+1;
                break;
                }
        verbose(4,"max is %d %d - %d ",max, *start, *end);
        }
    if (score[i] < 0) 
        score[i] = 0;
    }
assert (*end < size);
/* traceback to find start */
for (i=*end ; i>=0 ; i--)
    if (score[i] == 0)
        {
        *start = i+1;
        break;
        }

for (i=*start ; i<=*end ; i++)
    {
    assert (i < size);
    if (toupper(s[i]) == toupper(c) )
        count++;
    }
return count;
}
//#endif
   

#ifdef NEVER
int countCharsInWindow(char c, char *s, int size, int winSize, int *start)
/* Count number of characters matching c in s in a sliding window of size winSize. return start of polyA tail */
{
int i=0, count=0, maxCount=0, j=0;
for (j=0 ; j<=size-winSize ; j++)
    {
    count = 0;
    for (i=j; i<winSize+j ; i++)
        {
        assert (i < size);
        if (toupper(s[i]) == toupper(c) )
            count++;
        }
    maxCount = max(maxCount, count);
    *start = j;
    }
return maxCount;
}
#endif

int polyACalc(int start, int end, char *strand, int tSize, char *chrom, int region, int *polyAstart, int *polyAend)
/* get size of polyA tail in genomic dna , count bases in a 
 * sliding window in region of the end of the sequence*/
{
char nibFile[256];
int seqSize;
struct dnaSeq *seq = NULL;
int count = 0;
int length = 0;
int seqStart = strand[0] == '+' ? end : start - region;
int score[POLYAREGION+1], pStart = 0; 
struct seqFilePos *sfp = hashMustFindVal(tHash, chrom);
FILE *f = openFromCache(fileCache, sfp->file);
seqSize = sfp->pos;

assert(region > 0);
assert(end != 0);
*polyAstart = 0 , *polyAend = 0;
safef(nibFile, sizeof(nibFile), "%s/%s.nib", nibDir, chrom);
verbose(2,"polyA file %s size %d start %d tSize %d\n",sfp->file, seqSize, seqStart, tSize);
assert (seqSize == tSize);
if (seqStart < 0) seqStart = 0;
if (seqStart + region > seqSize) region = seqSize - seqStart;
if (region == 0)
    return 0;
assert(region > 0);
seq = nibLdPartMasked(NIB_MASK_MIXED, nibFile, f, seqSize, seqStart, region);
if (strand[0] == '+')
    {
    assert (seq->size <= POLYAREGION);
verbose(4,"\n + range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('A',seq->dna,seq->size, score, polyAstart, polyAend, 1, -1);
    }
else
    {
    assert (seq->size <= POLYAREGION);
verbose(4,"\n - range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('T',seq->dna,seq->size, score, polyAend, polyAstart, 1, -1);
    }
pStart += seqStart;
*polyAstart += seqStart;
*polyAend += seqStart;
length = strand[0]=='+'?*polyAend-*polyAstart:(*polyAstart)-(*polyAend);
verbose(3,"\npolyA is %d from end. seqStart=%d %s polyS/E %d-%d exact matches %d length %d\n",
        strand[0]=='+'?*polyAstart-seqStart:*polyAend-seqStart+seq->size, 
	seqStart, seq->dna+(*polyAstart)-seqStart, 
	*polyAstart, *polyAend, count, length);
freeDnaSeq(&seq);
return length;
}

void pslMergeBlocks(struct psl *psl, struct psl *outPsl,
                       int insertMergeSize)
/* merge together blocks separated by small inserts. */
{
int iBlk, iExon = -1;
int startIdx, stopIdx, idxIncr;

assert(outPsl!=NULL);
outPsl->qStarts = needMem(psl->blockCount*sizeof(unsigned));
outPsl->tStarts = needMem(psl->blockCount*sizeof(unsigned));
outPsl->blockSizes = needMem(psl->blockCount*sizeof(unsigned));

//if (psl->strand[1] == '-')
//    {
//    startIdx = psl->blockCount-1;
//    stopIdx = -1;
//    idxIncr = -1;
//    }
//else
//    {
    startIdx = 0;
    stopIdx = psl->blockCount;
    idxIncr = 1;
//    }

for (iBlk = startIdx; iBlk != stopIdx; iBlk += idxIncr)
    {
    unsigned tStart = psl->tStarts[iBlk];
    unsigned qStart = psl->qStarts[iBlk];
    unsigned size = psl->blockSizes[iBlk];
//    if (psl->strand[1] == '-')
//        reverseIntRange(&tStart, &tEnd, psl->tSize);
    if ((iExon < 0) || ((tStart - (outPsl->tStarts[iExon]+outPsl->blockSizes[iExon])) > insertMergeSize))
        {
        iExon++;
        outPsl->tStarts[iExon] = tStart;
        outPsl->qStarts[iExon] = qStart;
        outPsl->blockSizes[iExon] = size;
	}
    else
        outPsl->blockSizes[iExon] += size;
    }
outPsl->blockCount = iExon+1;
outPsl->match = psl->match;
outPsl->misMatch = psl->misMatch;
outPsl->repMatch = psl->repMatch;
outPsl->nCount = psl->nCount;
outPsl->qNumInsert = psl->qNumInsert;
outPsl->qBaseInsert = psl->qBaseInsert;
outPsl->tNumInsert = psl->tNumInsert;
outPsl->tBaseInsert = psl->tBaseInsert;
strcpy(outPsl->strand, psl->strand);
outPsl->qName = cloneString(psl->qName);
outPsl->qSize = psl->qSize;
outPsl->qStart = psl->qStart;
outPsl->qEnd = psl->qEnd;
outPsl->tName = cloneString(psl->tName);
outPsl->tSize = psl->tSize;
outPsl->tStart = psl->tStart;
outPsl->tEnd = psl->tEnd;
}

int interpolateStart(struct psl *gene, struct psl *pseudo)
/* estimate how much of the 5' end of the pseudogene was truncated */
{
int qs = pseudo->qStarts[0];
int qe = pseudo->qStarts[0]+pseudo->blockSizes[0];
int gs, ge, qExonStart = 0;
int geneBegin = gene->tStart;
int j, index = -1;
int offset = 0, bestOverlap = 0;
int exonStart = 0;
assert(pseudo!= NULL);
assert(gene!= NULL);
if (gene->strand[0] == '-')
    geneBegin = gene->tEnd;
for (j = 0 ; j < gene->blockCount ; j++)
    {
    int i = j;
    if (gene->strand[0] == '-')
        i = gene->blockCount - j -1;
    gs = gene->qStarts[i];
    ge = gene->qStarts[i]+gene->blockSizes[i];
    if (gene->strand[0] == '-')
        reverseIntRange(&gs, &ge, gene->qSize);
    int overlap = positiveRangeIntersection(qs, qe, gs, ge);
    if (overlap > bestOverlap)
        {
        bestOverlap = overlap;
        index = i;
        exonStart = gene->tStarts[i];
        qExonStart = gs;
        if (gene->strand[0] == '-')
            exonStart = exonStart + gene->blockSizes[i];
        }
    }
if (index >= 0)
    {
    if (gene->strand[0] == '+')
        offset = exonStart - geneBegin + qs - qExonStart;
    else
        offset = geneBegin - exonStart + qs - qExonStart;
    }
return offset;
}
bool isRepeat(char *chrom, int is, int ie, struct hash *bkHash)
/* check for repeats from intron*/
{
struct binKeeper *bk = NULL;
struct binElement *elist = NULL, *el = NULL;

if (bkHash != NULL)
    {
    int reps = 0;
    bk = hashFindVal(bkHash, chrom);
    elist = binKeeperFindSorted(bk, is, ie) ;
    for (el = elist; el != NULL ; el = el->next)
        {
        reps += positiveRangeIntersection(is, ie, el->start, el->end);
        verbose(3,"intron %s:%d-%d reps %d ratio %f\n",chrom,is, ie, reps,(float)reps/(float)(ie-is) );
        }
    slFreeList(&elist);
    if (reps > ie-is)
        {
        reps = ie-is;
        warn("Warning: too many reps %d in st %d in end %d\n",reps, is, ie);
        }
    if ((float)reps/(float)(ie-is) > repsPerIntron)
        {
        verbose(3,"Intron is repeat: ratio = %f \n",(float)reps/(float)(ie-is));
        return TRUE;
        }
    }
return FALSE;
}

bool getNextIntron(struct psl *psl, int i, int *tStart, int *tEnd, int *qStart, int *qEnd, struct hash *bkHash)
/* get next boundaries indexed intron */
/* t coords are virtual 0 = start of psl*/
/* return false of no next intron */
{
verbose(3,"\ngetNextIntron %s:%d-%d %d-%d i=%d\n",psl->tName, *tStart, *tEnd, *qStart, *qEnd, i);
if (i+1 >= psl->blockCount)
    return FALSE;
if (psl->strand[0] == '-')
    {
    i = psl->blockCount - i -1;
    *qStart = psl->qStarts[i-1] + psl->blockSizes[i-1];
    *tEnd = psl->tStarts[i]-2;
    *tStart = psl->tStarts[i-1] + psl->blockSizes[i-1];
    *qEnd = psl->qStarts[i];
//    *tStart = psl->tEnd - *tStart;
//    *tEnd = psl->tEnd - *tEnd;
    }
else
    {
    *qEnd = psl->qStarts[i+1];
    *tEnd = psl->tStarts[i+1];
    assert(i < psl->blockCount);
    *qStart = psl->qStarts[i] + psl->blockSizes[i];
    *tStart = psl->tStarts[i] + psl->blockSizes[i];
    assert(*tStart > psl->tStart);
//    *tStart = *tStart - psl->tStart;
//    *tEnd = *tEnd - psl->tStart;
    }
if (psl->strand[0] == '-')
    {
    reverseIntRange(qStart, qEnd, psl->qSize);
    }
if (isRepeat(psl->tName, *tStart,*tEnd,bkHash))
    {
    verbose(3,"isRepeat True %s:%d-%d %s\n",psl->tName,*tStart, *tEnd, psl->qName);
    return FALSE;
    }
verbose(3,"isRepeat FALSE %s:%d-%d %s\n",psl->tName,*tStart, *tEnd, psl->qName);
return TRUE;
}

void exciseRepeats(struct psl *psl, struct hash *bkHash)
/* remove repeats from intron, readjust t coordinates */
{
struct binKeeper *bk = NULL;
struct binElement *elist = NULL, *el = NULL;
int i;
unsigned *outReps = needMem(psl->blockCount*sizeof(unsigned));

for (i = 0 ; i < psl->blockCount-1 ; i++)
    {
    int is = psl->tStarts[i] + psl->blockSizes[i];
    int ie = psl->tStarts[i+1] ;
    outReps[i] = 0;
    /* skip exons that overlap repeats */
    if (bkHash != NULL)
        {
        int reps = 0;
        bk = hashFindVal(bkHash, psl->tName);
        elist = binKeeperFindSorted(bk, is, ie) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            reps += positiveRangeIntersection(is, ie, el->start, el->end);
            }
        slFreeList(&elist);
        if (reps > ie-is)
            {
            reps = ie-is;
            warn("Warning: too many reps %d in st %d in end %d\n",reps, is, ie);
            }
        outReps[i] = reps;
        }
    }
/* shrink the repeats from the gene */
for (i = 0 ; i < psl->blockCount-1 ; i++)
    {
    int j;
    for (j = i ; j < psl->blockCount-1 ; j++)
        psl->tStarts[j+1] -= outReps[i];
    psl->tEnd -= outReps[i];
    assert(psl->tEnd > psl->tStarts[i+1]);
    }
freeMem(outReps);
/*
outPsl->blockCount = psl->blockCount;
outPsl->match = psl->match;
outPsl->misMatch = psl->misMatch;
outPsl->repMatch = psl->repMatch;
outPsl->nCount = psl->nCount;
outPsl->qNumInsert = psl->qNumInsert;
outPsl->qBaseInsert = psl->qBaseInsert;
outPsl->tNumInsert = psl->tNumInsert;
outPsl->tBaseInsert = psl->tBaseInsert;
strcpy(outPsl->strand, psl->strand);
outPsl->qName = cloneString(psl->qName);
outPsl->qSize = psl->qSize;
outPsl->qStart = psl->qStart;
outPsl->qEnd = psl->qEnd;
outPsl->tName = cloneString(psl->tName);
outPsl->tSize = psl->tSize;
outPsl->tStart = psl->tStart;
outPsl->tEnd = psl->tEnd;
*/
}
char *getSpliceSite(struct psl *psl, int start)
/* get sequence for splice site */
{
char *tName ;
struct dnaSeq *tSeq;
int tOffset;
char *site = NULL;
boolean isNib = TRUE;
tName = cloneString(psl->tName);
readCachedSeqPart(tName, start, 2, tHash, fileCache, &tSeq, &tOffset, &isNib);
assert (tOffset == start);
site = cloneStringZ(tSeq->dna, 2);
if (psl->strand[0] == '-')
    {
    reverseComplement(site, 2);
    }
freeDnaSeq(&tSeq);
freez(&tName);
return site;
}
int pslCountIntrons(struct psl *genePsl, struct psl *pseudoPsl, int maxBlockGap, 
        struct hash *bkHash , int *tReps, int slop, struct dyString *iString , int *conservedIntron)
/* count pseudogene introns that match gene introns based on q position allow some slop*/
{
struct psl *gene , *pseudo;
int count = 0;
int i,j;
int gts, gte, gqs, gqe; /* gene intron boundaries */
int pts, pte, pqs, pqe; /* pseudoegene intron boundaries */
int offset = 0; /* 5' truncation of pseudogene */

if (genePsl == NULL || pseudoPsl == NULL)
    return 0;

if (pseudoPsl->blockCount == 1)
    return 0;
AllocVar(gene);
AllocVar(pseudo);
pslMergeBlocks(genePsl, gene, maxBlockGap);
pslMergeBlocks(pseudoPsl, pseudo, maxBlockGap);
//if (!skipExciseRepeats)
//    {
//    exciseRepeats(gene, bkHash );
//    exciseRepeats(pseudo, bkHash );
//    }
//offset = interpolateStart(gene, pseudo);
verbose(3,"pslCountIntrons gene cnt %d pseudocnt %d\n",gene->blockCount, pseudo->blockCount);
for (i = 0 ; i < gene->blockCount ; i++)
    {
    float intronG = 0.0;
    float intronP = 0.0;
    if (!getNextIntron(gene, i, &gts, &gte, &gqs, &gqe, bkHash))
        if (i+1 >= gene->blockCount)
            break;
    for (j = 0 ; j < pseudo->blockCount ; j++)
        {
        if (!getNextIntron(pseudo, j, &pts, &pte, &pqs, &pqe, bkHash))
            {
            verbose(3,"NO gt %d-%d %d pt (%c) %d-%d %d  gq %d-%d pq %d-%d offset %d\n",
                    gts,gte,gte-gts,pseudo->strand[0],pts,pte,pte-pts,gqs,gqe,pqs,pqe, offset);
            if (i+1 >= gene->blockCount)
                break;     /* done, get next exon on gene */
            else
                continue;  /* repeat, keep looking */
            }
        intronG = gte-gts;
        intronP = pte-pts;
        if (abs(gqs-pqs) < slop || abs(gqe-pqe) < slop) 
//                || ((abs(gqs-pqs) < slop && intronG/intronP) < intronRatio)
//                || ((abs(gqe-pqe) < slop && intronG/intronP) < intronRatio))
            {
            char *gd = NULL, *ga = NULL, *pd = NULL, *pa = NULL;
            if (genePsl->strand[0] == '+')
                {
                gd = getSpliceSite(genePsl, gts); /* donor */
                ga = getSpliceSite(genePsl, gte); /* acceptor */
                }
            else
                {
                gd = getSpliceSite(genePsl, gte); /* donor */
                ga = getSpliceSite(genePsl, gts); /* acceptor */
                }
            if (pseudoPsl->strand[0] == '+')
                {
                pd = getSpliceSite(pseudoPsl, pts); /* donor */
                pa = getSpliceSite(pseudoPsl, pte); /* acceptor */
                }
            else
                {
                pd = getSpliceSite(pseudoPsl, pte); /* donor */
                pa = getSpliceSite(pseudoPsl, pts); /* acceptor */
                }
            if (sameString(gd,pd) && sameString(ga,pa) && sameString(gd,"GT") && sameString(ga,"AG"))
                {
                *conservedIntron = (*conservedIntron) + 1;
                verbose(3,"conserved intron %s %s acc %s %s %d\n",gd,pd,ga,pa, *conservedIntron);
                }
            verbose(3,"part1 gt %d-%d %3.0f pt %d-%d %3.0f  gq %d-%d pq %d-%d diff %d ratio %f gd %s ga %s pd %s pa %s\n"
                    ,gts,gte,intronG,pts,pte,intronP,gqs,gqe,pqs,pqe, abs(gts-(pts+offset)),
                    intronG/intronP, gd, ga, pd, pa );
            if (intronP > 500 || (intronG/intronP) < intronRatio)
                {
                count++;
                verbose(3,"** Yes ** count = %d gt %d-%d %d pt %d-%d %d  gq %d-%d pq %d-%d offset %d\n",
                        count,gts,gte,gte-gts,pts,pte,pte-pts,gqs,gqe,pqs,pqe, offset);
                }
            freez(&gd); freez(&ga); freez(&pd); freez(&pa);
            }
        else
            {
            verbose(3,"NO gt %d-%d %d pt %d-%d %d  gq %d-%d pq %d-%d offset %d\n",
                    gts,gte,gte-gts,pts,pte,pte-pts,gqs,gqe,pqs,pqe, offset);
            }
        }
        dyStringPrintf(iString, "%d,%d,",intronG,intronP);
    }
verbose(3,"pslIntronCount returns %d\n\n",count);
pslFree(&pseudo);
pslFree(&gene);
return count;
}

int pslCountExonSpan(struct psl *target, struct psl *query, int maxBlockGap, struct hash *bkHash , int *tReps, int *qReps)
/* count the number of blocks in the query that overlap the target */
/* merge blocks that are closer than maxBlockGap */
{
int i, j, start = 0, qs, qqs;
int count = 0;
struct binKeeper *bk = NULL;
struct binElement *elist = NULL, *el = NULL;
struct psl *targetM , *queryM;

*tReps = 0; *qReps = 0;
if (target == NULL || query == NULL)
    return 0;

AllocVar(targetM);
AllocVar(queryM);
pslMergeBlocks(target, targetM, maxBlockGap);
pslMergeBlocks(query, queryM, maxBlockGap);

for (i = 0 ; i < targetM->blockCount ; i++)
  {
    int qe = targetM->qStarts[i] + targetM->blockSizes[i];
    int ts = targetM->tStarts[i] ;
    int te = targetM->tStarts[i] + targetM->blockSizes[i];
    int teReps = 0;
    qs = targetM->qStarts[i];
    /* combine blocks that are close together */
    if (i < (targetM->blockCount) -1)
        {
        /* skip exons that overlap repeats */
        if (bkHash != NULL)
            {
            bk = hashFindVal(bkHash, targetM->tName);
            elist = binKeeperFindSorted(bk, ts, te) ;
            for (el = elist; el != NULL ; el = el->next)
                {
                teReps += positiveRangeIntersection(ts, te, el->start, el->end);
                }
            slFreeList(&elist);
            *tReps += teReps;
            if (2 * teReps > (te-ts))
                continue;
            }
        }
    if (targetM->strand[0] == '-')
        reverseIntRange(&qs, &qe, targetM->qSize);
    if (qs < 0 || qe > targetM->qSize)
        {
        warn("Odd: qName %s tName %s qSize %d psl->qSize %d start %d end %d",
            targetM->qName, targetM->tName, targetM->qSize, targetM->qSize, qs, qe);
        if (qs < 0)
            qs = 0;
        if (qe > targetM->qSize)
            qe = targetM->qSize;
        }
    if (positiveRangeIntersection(queryM->qStart, queryM->qEnd, qs, qe) > min(20,qe-qs))
        {
        int qqe = 0;
        for (j = 0 ; j < queryM->blockCount ; j++)
            {
            int localReps = 0;
            qqs = queryM->qStarts[0];
            qqe = queryM->qStarts[j] + queryM->blockSizes[j];
            if (queryM->strand[0] == '-')
                reverseIntRange(&qqs, &qqe, queryM->qSize);
            assert(j < queryM->blockCount);
            /* mask repeats */
            if (bkHash != NULL)
                {
                bk = hashFindVal(bkHash, queryM->tName);
                if (j < (queryM->blockCount) -1)
                    {
                    elist = binKeeperFindSorted(bk, queryM->tStarts[j], queryM->tStarts[j+1]) ;
                    for (el = elist; el != NULL ; el = el->next)
                        {
                        localReps += positiveRangeIntersection(queryM->tStarts[j], queryM->tStarts[j]+queryM->blockSizes[j], el->start, el->end);
                        }
                    *qReps += localReps;
                    slFreeList(&elist);
                    }
                }
            }
        if (positiveRangeIntersection(qqs, qqe, qs, qe) > 10)
            count++;
        }
    start = i+1;
    }

if(count > targetM->blockCount )
    {
    verbose(1,"error in pslCountExonSpan: %s %s %s:%d-%d %d > targetBlk %d or query Blk %d \n",
            targetM->qName, queryM->qName, queryM->tName,queryM->tStart, queryM->tEnd, 
            count, targetM->blockCount, queryM->blockCount);
    assert(count > targetM->blockCount);
    }
pslFree(&targetM);
pslFree(&queryM);
return count;
}
float calcTrf(struct psl *psl, struct hash *trfHash)
{
int trf = 0;
if (trfHash != NULL)
    {
    int i, trf = 0;
    for (i = 0 ; i < psl->blockCount ; i++)
        {
        int ts = psl->tStarts[i] ;
        int te = psl->tStarts[i] + psl->blockSizes[i];
        struct binKeeper *bk = hashFindVal(trfHash, psl->tName);
        struct binElement *el, *elist = binKeeperFindSorted(bk, ts, te ) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            //bed = el->val;
            trf += positiveRangeIntersection(ts, te, el->start, el->end);
            }
        slFreeList(&elist);
        }
    }
return (float)trf/(float)(psl->match+psl->misMatch);
}

int overlapSplicedMrna(struct psl *psl, struct hash *mrnaHash, int *exonCount, struct psl **overlapPsl)
/* count bases that spliced mrna overlaps with pseudogenes. If self match then don't filter it.*/
/* exonCount has number of exons in matched mRna */
{
int maxOverlap = 0;
safef(mrnaOverlap,255,"NONE");
if (mrnaHash != NULL)
    {
    int mrnaBases = 0;
    struct psl *mPsl = NULL, *mPslMerge = NULL;
    struct binKeeper *bk = hashFindVal(mrnaHash, psl->tName);
    struct binElement *el, *elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
    int blockIx;
    for (el = elist; el != NULL ; el = el->next)
        {
        mrnaBases = 0;
        mPsl = el->val;
        if (mPsl != NULL)
            {
            assert (psl != NULL);
            assert (mPsl != NULL);
            assert (psl->tName != NULL);
            if (differentString(psl->qName, mPsl->qName))
                {
                AllocVar(mPslMerge);
                pslMergeBlocks(mPsl, mPslMerge, 30);
                assert(mPslMerge != NULL);
                if (mPslMerge->blockCount > 0)
                    {
                    for (blockIx = 0; blockIx < mPsl->blockCount; ++blockIx)
                        {
                        mrnaBases += positiveRangeIntersection(psl->tStart, psl->tEnd, 
                            mPsl->tStarts[blockIx], mPsl->tStarts[blockIx]+mPsl->blockSizes[blockIx]);
                        }
                    }
                else
                    {
                    mrnaBases += positiveRangeIntersection(psl->tStart, psl->tEnd, mPsl->tStart, mPsl->tEnd);
                    verbose(3,"blk %d %s %d ec %d\n",mPsl->blockCount, mPsl->qName, mrnaBases, exonCount);
                    verbose(3,"blk merge %d %s %d ec %d\n",mPslMerge->blockCount, mPsl->qName, mrnaBases, exonCount);
                    }
                verbose(2,"MRNABASES %d cnt %d %s %s %d-%d\n",mrnaBases, mPslMerge->blockCount, mPslMerge->qName, mPsl->qName, mPsl->tStart, mPsl->tEnd);
                if (mrnaBases > 50 && (mPslMerge->blockCount > 1))
                    {
                        *exonCount = (int)mPslMerge->blockCount;
                        safef(mrnaOverlap,255,"%s",mPsl->qName);
                        maxOverlap = mrnaBases;
                        *overlapPsl = mPsl;
                    }
                pslFree(&mPslMerge);
                }
            }
        }
    slFreeList(&elist);
    }
return maxOverlap ;
}


void processBestMulti(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, struct hash *bkHash)
/* Find psl's that are best anywhere along their length. */

{
struct psl *bestPsl = NULL, *psl, *bestSEPsl = NULL;
struct genePred *gp = NULL, *kg = NULL, *mgc = NULL;
int qSize = 0;
int *scoreTrack = NULL;
int maxExons = 0;
int geneOverlap = -1;
int milliScore;
int goodAliCount = 0;
int bestAliCount = 0;
int milliMin = 1000*minAli;
int milliMinPseudo = 1000*minAliPseudo;
int bestStart = -1, bestEnd = -1;
int bestSEStart = 0, bestSEEnd = 0;
int polyAstart = 0;
int polyAend = 0;
struct binElement *el, *elist;
struct binKeeper *bk;
int trf = 0, rep = 0;
char *bestChrom = NULL, *bestSEChrom = NULL;
int tReps , qReps;
int i ;
int bestScore = 0, bestSEScore = 0;
int conservedIntrons = 0;

if (pslList == NULL)
    return;

/* get biggest mrna to size Score array - some have polyA tail stripped off */
for (psl = pslList; psl != NULL; psl = psl->next)
    if (psl->qSize > qSize)
        qSize = psl->qSize;

AllocArray(scoreTrack, qSize+1);

for (psl = pslList; psl != NULL; psl = psl->next)
    {
    int blockIx;
    char strand = psl->strand[0];

    verbose(3,"checking %s %s:%d-%d\n",psl->qName, psl->tName, psl->tStart, psl->tEnd);
    assert (psl!= NULL);
    milliScore = calcMilliScore(psl);
    if (milliScore >= milliMin)
	{
	++goodAliCount;
	milliScore += sizeFactor(psl, bkHash, trfHash);
        verbose(3,"@ %s %s:%d milliScore %d\n", psl->qName, psl->tName, psl->tStart, milliScore);
	for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
	    {
	    int start = psl->qStarts[blockIx];
	    int size = psl->blockSizes[blockIx];
	    int end = start+size;
	    int i;
            if (strand == '-')
	        reverseIntRange(&start, &end, psl->qSize);
	    if (start < 0 || end > psl->qSize || psl->qSize > qSize)
		{
		warn("Error: qName %s tName %s qSize %d psl->qSize %d start %d end %d",
		    psl->qName, psl->tName, qSize, psl->qSize, start, end);
		}
            verbose(3,"milliScore: %d qName %s tName %s:%d-%d qSize %d psl->qSize %d start %d end %d \n",
                    milliScore, psl->qName, psl->tName, psl->tStart, psl->tEnd, qSize, psl->qSize, start, end);
	    for (i=start; i<end; ++i)
		{
                assert(i<=qSize);
		if (milliScore > scoreTrack[i])
                    {
                    verbose(3," %d ",milliScore);
                    scoreTrack[i] = milliScore;
                    }
		}
	    }
	}
    }
verbose(3,"---finding best---\n");
/* Print out any alignments that are within minTop% of top score. */
bestScore = 0;
bestSEScore = 0;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    struct psl *pslMerge;
    int score = calcSizedScore(psl, bkHash, trfHash);
    
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, score) 
        && (psl->match + psl->repMatch + psl->misMatch) >= round(minCover * psl->qSize))
	{
        ++bestAliCount;
        AllocVar(pslMerge);
        pslMergeBlocks(psl, pslMerge, 30);
        
        if (score  > bestScore && pslMerge->blockCount > 1)
            {
            bestPsl = psl;
            bestStart = psl->tStart;
            bestEnd = psl->tEnd;
            bestChrom = cloneString(psl->tName);
            bestScore = score;
            }
        if (score  > bestSEScore )
            {
            bestSEPsl = psl;
            bestSEStart = psl->tStart;
            bestSEEnd = psl->tEnd;
            bestSEChrom = cloneString(psl->tName);
            bestSEScore = score  ;
            }
        if (pslMerge->blockCount > maxExons )
            maxExons = pslMerge->blockCount;
        pslFree(&pslMerge);
	}
    }
/* output pseudogenes, if alignments have no introns and mrna alignmed with introns */
if (pslList != NULL)
    for (psl = pslList; psl != NULL; psl = psl->next)
    {
    int score = calcSizedScore(psl, bkHash, trfHash);
    int maxOverlap = 0;
    bool keepChecking = TRUE;
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, score) /*bkHash, trfHash)*/
        && psl->match + psl->misMatch + psl->repMatch >= minCover * psl->qSize)
            {
            pslTabOut(psl, bestFile);
            }
    else 
        {
        /* calculate various features of pseudogene */
        struct pseudoGeneLink *pg = NULL;
        struct dyString *iString = newDyString(16*1024);
        struct dyString *reason = newDyString(255);
        AllocVar(pg);
        pg->exonCount = maxExons;
        pg->bestAliCount = bestAliCount;
        verbose(1,"checking %s:%d-%d %s best %s:%d-%d\n",psl->tName,psl->tStart, psl->tEnd, psl->qName, bestChrom, bestStart, bestEnd);
        pg->oldIntronCount = intronFactor(psl, bkHash, trfHash);
        pg->gChrom = cloneString(bestChrom);
        pg->gStart = bestStart;
        pg->gEnd = bestEnd;
        pg->overlapDiag= -1;
        pg->overStart = pg->overEnd = pg->kStart = pg->kEnd = pg->rStart = pg->rEnd = pg->mStart = pg->mEnd = -1;
        strncpy(pg->gStrand, psl->strand , sizeof(pg->gStrand));
        pg->polyA = polyACalc(psl->tStart, psl->tEnd, psl->strand, psl->tSize, psl->tName, 
                        POLYAREGION, &polyAstart, &polyAend);
        pg->polyAstart = polyAstart;
        /* count # of alignments that span introns */
        pg->exonCover = pslCountExonSpan(bestPsl, psl, maxBlockGap, bkHash, &tReps, &qReps) ;
        pg->qReps = qReps;
        pg->intronCount = pslCountIntrons(bestPsl, psl, maxBlockGap, bkHash, &tReps, intronSlop, iString, &conservedIntrons) ;
        pg->conservedIntrons = conservedIntrons;
        pg->trfRatio = calcTrf(psl, trfHash);
        if (bestPsl == NULL)
            pg->intronCount = pg->oldIntronCount;

        geneOverlap = 0;
        genePredFree(&kg); 
        genePredFree(&gp); 
        genePredFree(&mgc); 
        if (bestPsl != NULL)
            {
            kg = getOverlappingGene(&kgList, "knownGene", bestPsl->tName, bestPsl->tStart, 
                                bestPsl->tEnd , bestPsl->qName, &geneOverlap);
            if (kg != NULL)
                {
                pg->kStart = kg->txStart;
                pg->kEnd = kg->txEnd;
                if (kg->name2 != NULL)
                    pg->kgName = cloneString(kg->name2);
                else
                    pg->kgName = cloneString(kg->name);
                }
            else
                {
                pg->kgName = cloneString("noKg");
                }
            gp = getOverlappingGene(&gpList1, "refGene", bestPsl->tName, bestPsl->tStart, 
                                bestPsl->tEnd , bestPsl->qName, &geneOverlap);
            if (gp != NULL)
                {
                pg->refSeq = cloneString(gp->name);
                pg->rStart = gp->txStart;
                pg->rEnd = gp->txEnd;
                }
            else
                {
                pg->refSeq = cloneString("noRefSeq");
                }
            mgc = getOverlappingGene(&gpList2, "mgcGenes", bestPsl->tName, bestPsl->tStart, 
                                bestPsl->tEnd , bestPsl->qName, &geneOverlap);
            if (mgc != NULL)
                {
                pg->mgc = cloneString(mgc->name);
                pg->mStart = mgc->txStart;
                pg->mEnd = mgc->txEnd;
                }
            else
                {
                pg->mgc = cloneString("noMgc");
                }
            }
        else
            {
            pg->refSeq = cloneString("noRefSeq");
            pg->kgName = cloneString("noKg");
            pg->mgc = cloneString("noMgc");
            }
        /* calculate if pseudogene overlaps the syntenic diagonal with another species */
        if (synHash != NULL)
            {
            bk = hashFindVal(synHash, psl->tName);
            elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
            pg->overlapDiag = 0;
            for (el = elist; el != NULL ; el = el->next)
                {
                //bed = el->val;
                pg->overlapDiag += positiveRangeIntersection(psl->tStart, psl->tEnd, 
                        el->start, el->end);
                }
            pg->overlapDiag = (pg->overlapDiag*100)/(psl->tEnd-psl->tStart);
            slFreeList(&elist);
            }
        if (pg->trfRatio > .5)
            {
            if (!quiet)
                verbose(1,"NO. %s trf overlap %f > .5 %s %d \n",
                        psl->qName, (float)trf/(float)(psl->match+psl->misMatch) ,
                        psl->tName, psl->tStart);
            keepChecking = FALSE;
            }
        /* blat sometimes overlaps parts of the same mrna , filter these */
        if ( keepChecking && positiveRangeIntersection(bestStart, bestEnd, psl->tStart, psl->tEnd) && 
                    sameString(psl->tName, bestChrom))
           {
           dyStringAppend(reason,"self ");
           keepChecking = FALSE;
           }

        /* count repeat overlap with pseudogenes and skip ones with more than maxRep% overlap*/
        if (keepChecking && bkHash != NULL)
            {
            rep = 0;
            assert (psl != NULL);
            for (i = 0 ; i < psl->blockCount ; i++)
                {
                int ts = psl->tStarts[i] ;
                int te = psl->tStarts[i] + psl->blockSizes[i];
                bk = hashFindVal(bkHash, psl->tName);
                elist = binKeeperFindSorted(bk, ts, te) ;
                for (el = elist; el != NULL ; el = el->next)
                    {
                    rep += positiveRangeIntersection(ts, te, el->start, el->end);
                    }
                slFreeList(&elist);
                }
            }
        pg->tReps = round((float)(rep*100)/(float)(psl->match+(psl->misMatch)));
        if ((float)rep/(float)(psl->match+(psl->misMatch)) > maxRep )
            {
            if (!quiet)
                verbose(1,"NO %s reps %.3f %.3f\n",psl->tName,(float)rep/(float)(psl->match+(psl->misMatch)) , maxRep);
            dyStringAppend(reason,"maxRep ");
            pg->label = NOTPSEUDO;
            keepChecking = FALSE;
            }

        if (keepChecking && (pg->intronCount == 0 /*|| (pg->exonCover - pg->intronCount > INTRONMAGIC)*/) && 
            maxExons > 1 && pg->bestAliCount > 0 && bestChrom != NULL &&
            (calcMilliScore(psl) >= milliMinPseudo && (pg->trfRatio < .5) &&
            psl->match + psl->misMatch + psl->repMatch >= minCoverPseudo * (float)psl->qSize))
            {
                struct psl *mPsl;
                int exonCount = -1;
                struct genePred *gene = getOverlappingGene(&gpList1, "refGene", psl->tName, psl->tStart, 
                                    psl->tEnd , psl->qName, &geneOverlap);
                maxOverlap = overlapSplicedMrna(psl, mrnaHash, &exonCount, &mPsl);
                pg->maxOverlap = maxOverlap;
                if ((float)maxOverlap/(float)(psl->match+psl->misMatch+psl->repMatch) > splicedOverlapRatio 
                        && maxOverlap > 50 ) 
                    /* if overlap > 50 bases  and 50% overlap with pseudogene, then skip */
                    {
                    if (!quiet)
                        verbose(1,"NO %s:%d-%d %s expressed blat mrna %s %d bases overlap %f %%\n",
                                psl->tName, psl->tStart, psl->tEnd, psl->qName,mrnaOverlap, 
                                maxOverlap, (float)maxOverlap/(float)psl->qSize);
                    dyStringAppend(reason,"expressed ");
                    pg->overName = cloneString(mPsl->qName); 
                    pg->overStart = mPsl->tStart;
                    pg->overEnd = mPsl->tEnd;
                    strncpy(pg->overStrand, mPsl->strand , sizeof(pg->overStrand));
                    pg->label = EXPRESSED;
                    outputLink(psl, pg, reason);
                    keepChecking = FALSE;
                    }


                if (pg->overlapDiag>= 40 && bestPsl == NULL)
                   {
                   if (!quiet)
                        verbose(1,"NO. %s %d diag %s %d  bestChrom %s\n",psl->qName, 
                                pg->overlapDiag, psl->tName, psl->tStart, bestChrom);
                   dyStringAppend(reason,"diagonal ");
                   keepChecking = FALSE;
                   }
                if (keepChecking)
                   {
                    verbose(2,"YES %s %d rr %3.1f rl %d ln %d %s iF %d maxE %d bestAli %d isp %d score %d match %d cover %3.1f rp %d polyA %d syn %d polyA %d start %d\n",
                        psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                        psl->tEnd-psl->tStart,psl->tName, pg->intronCount, 
                        maxExons , pg->bestAliCount, pg->exonCover,
                        calcMilliScore(psl),  psl->match + psl->misMatch + psl->repMatch , 
                        minCoverPseudo * (float)psl->qSize, pg->tReps + pg->qReps, pg->polyA, pg->overlapDiag,
                        pg->polyA, pg->polyAstart );
                   if (pg->exonCover < 2)
                       {
                       dyStringAppend(reason,"singleExon ");
                       pg->label = PSEUDO;
                       outputLink(psl, pg, reason);
                       }
                   else if (bestPsl == NULL)
                       {
                       dyStringAppend(reason,"noBest ");
                       pg->label = NOTPSEUDO;
                       outputLink(psl, pg, reason);
                       }
                   else if (kg == NULL && mgc == NULL && gp == NULL)
                       {
                       dyStringAppend(reason,"mrna");
                       pg->label = PSEUDO;
                       outputLink(psl, pg, reason);
                       }
                   else
                       {
                       dyStringAppend(reason,"good");
                       pg->label = PSEUDO;
                       outputLink(psl, pg, reason);
                       }
                   keepChecking = FALSE;
                   }
                }
            else
                {
                if (bestPsl == NULL)
                    dyStringAppend(reason,"noBest ");
                if (pg->bestAliCount < 1)
                    dyStringAppend(reason,"noAli ");
                if (pg->trfRatio > .5)
                    dyStringAppend(reason,"trf ");
                if (pg->intronCount > 0)
                    dyStringAppend(reason,"introns ");
                if (pg->exonCover < 2)
                    dyStringAppend(reason,"singleExon ");
                if (maxExons <= 1)
                    dyStringAppend(reason,"maxExons ");
                if (calcMilliScore(psl) < milliMinPseudo)
                    dyStringAppend(reason,"milliBad ");
                if (psl->match + psl->misMatch + psl->repMatch < minCoverPseudo * (float)psl->qSize)
                    dyStringAppend(reason,"coverage ");

               if (bestPsl == NULL)
                   {
                   pg->label = NOTPSEUDO;
                   outputLink(psl, pg, reason);
                   }
                else
                   {
                   pg->label = NOTPSEUDO;
                   outputLink(psl, pg, reason);
                   }
                verbose(2,"NO. %s %s %d rr %3.1f rl %d ln %d %s iF %d maxE %d bestAli %d isp %d score %d match %d cover %3.1f rp %d\n",
                    reason->string, psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                    psl->tEnd-psl->tStart,psl->tName, pg->intronCount, maxExons , pg->bestAliCount, pg->exonCover,
                    calcMilliScore(psl),  psl->match + psl->misMatch + psl->repMatch , 
                    minCoverPseudo * (float)psl->qSize, pg->tReps + pg->qReps);
                }
        dyStringFree(&iString);
        dyStringFree(&reason);
        //pseudoGeneLinkFree(&pg);
        }
    }
freeMem(scoreTrack);
}

void processBestSingle(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, struct hash *bkHash)
/* Find single best psl in list. */
{
struct psl *bestPsl = NULL, *psl;
int bestScore = 0, score, threshold;

assert(0==1); /* not all checks implemented */
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    score = pslScore(psl);
    if (score > bestScore)
        {
	bestScore = score;
	bestPsl = psl;
	}
    }
threshold = round((1.0 - nearTop)*bestScore);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if (pslScore(psl) >= threshold)
        pslTabOut(psl, bestFile);
    }
}

void doOneAcc(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, 
        struct hash *bkHash)
/* Process alignments of one piece of mRNA. */
{
if (singleHit)
    processBestSingle(acc, pslList, trfHash, synHash, mrnaHash, bkHash);
else
    processBestMulti(acc, pslList, trfHash, synHash, mrnaHash, bkHash);
}

void pslPseudo(char *inName, struct hash *bkHash, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, 
        char *bestAliName, char *psuedoFileName, char *linkFileName, char *axtFileName)
/* find best alignments with and without introns.
 * Put pseudogenes  in pseudoFileName. 
 * store link between pseudogene and gene in LinkFileName */
{
struct lineFile *in = pslFileOpen(inName);
int lineSize;
char *line;
char *words[32];
int wordCount;
struct psl *pslList = NULL, *psl;
char lastName[256] = "nofile";
int aliCount = 0;
quiet = sameString(bestAliName, "stdout") || sameString(psuedoFileName, "stdout");
bestFile = mustOpen(bestAliName, "w");
pseudoFile = mustOpen(psuedoFileName, "w");
linkFile = mustOpen(linkFileName, "w");
axtFile = mustOpen(axtFileName, "w");

if (!quiet)
    verbose(1,"Processing %s to %s and %s\n", inName, bestAliName, psuedoFileName);
 if (!noHead)
     pslWriteHead(bestFile);
safef(lastName, sizeof(lastName),"x");
while (lineFileNext(in, &line, &lineSize))
    {
    if ((++aliCount & 0x1ffff) == 0)
        {
	verboseDot();
	}
    wordCount = chopTabs(line, words);
    if (wordCount != 21)
	errAbort("Bad line %d of %s\n", in->lineIx, in->fileName);
    psl = pslLoad(words);
    if (!sameString(lastName, psl->qName))
	{
        slReverse(&pslList);
	doOneAcc(lastName, pslList, trfHash, synHash, mrnaHash, bkHash);
	pslFreeList(&pslList);
	safef(lastName, sizeof(lastName), psl->qName);
	}
    slAddHead(&pslList, psl);
    }
slReverse(&pslList);
doOneAcc(lastName, pslList, trfHash, synHash, mrnaHash, bkHash);
pslFreeList(&pslList);
lineFileClose(&in);
fclose(bestFile);
fclose(pseudoFile);
fclose(linkFile);
fclose(axtFile);
verbose(1,"Processed %d alignments\n", aliCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *bkHash = NULL, *trfHash = NULL, *synHash = NULL, *mrnaHash = NULL;
//struct lineFile *lf = NULL;
//struct bed *syntenicList = NULL, *bed;
//char  *row[3];
//struct genePredReader *gprKg;
optionHash(&argc, argv);
if (argc != 17)
    usage();
verboseSetLogFile("stdout");
ss = axtScoreSchemeDefault();
fileHash = newHash(0);  
tHash = newHash(20);  /* seqFilePos value. */
qHash = newHash(20);  /* seqFilePos value. */
fileCache = newDlList();
db = cloneString(argv[1]);
nibDir = cloneString(argv[11]);
minAli = optionFloat("minAli", minAli);
maxRep = optionFloat("maxRep", maxRep);
minAliPseudo = optionFloat("minAliPseudo", minAliPseudo);
nearTop = optionFloat("nearTop", nearTop);
splicedOverlapRatio = optionFloat("splicedOverlapRatio", splicedOverlapRatio);
minCover = optionFloat("minCover", minCover);
minCoverPseudo = optionFloat("minCoverPseudo", minCoverPseudo);
minNearTopSize = optionInt("minNearTopSize", minNearTopSize);
intronSlop = optionInt("intronSlop", intronSlop);
maxBlockGap = optionInt("maxBlockGap" , maxBlockGap) ;
ignoreSize = optionExists("ignoreSize");
skipExciseRepeats = optionExists("skipExciseRepeats");
noIntrons = optionExists("noIntrons");
//singleHit = optionExists("singleHit");
noHead = optionExists("nohead");
initWeights();

verbose(1,"Scanning %s\n", argv[12]);
hashFileList(argv[12], fileHash, tHash);
verbose(1,"Loading mrna sequences from %s\n",argv[13]);
mrnaList = faReadAllMixed(argv[13]);
if (mrnaList == NULL)
    errAbort("could not open %s\n",argv[13]);
gpList1 = genePredLoadAll(argv[14]);
gpList2 = genePredLoadAll(argv[15]);
//gprKg = genePredReaderFile(argv[16], NULL);
//kgLis = genePredReaderAll(gprKg);
kgList = genePredLoadAll(argv[16]);

verbose(1,"Loading Syntenic Bed %s\n",argv[5]);
synHash = readBedCoordToBinKeeper(argv[3], argv[5], BEDCOUNT);
verbose(1,"Loading Trf Bed %s\n",argv[6]);
trfHash = readBedCoordToBinKeeper(argv[3], argv[6], BEDCOUNT);

verbose(1,"Reading Repeats from %s\n",argv[4]);
bkHash = readBedCoordToBinKeeper(argv[3], argv[4], BEDCOUNT);

verbose(1,"Reading mrnas from %s\n",argv[7]);
mrnaHash = readPslToBinKeeper(argv[3], argv[7]);
verbose(1,"Scoring alignments from %s.\n",argv[2]);
verbose(1,"start\n");
pslPseudo(argv[2], bkHash, trfHash, synHash, mrnaHash, argv[8], argv[9], argv[10], argv[11]);
verbose(1,"freeing everything\n");
binKeeperPslHashFree(&mrnaHash);
binKeeperHashFree(&synHash);
binKeeperHashFree(&trfHash);
genePredFreeList(&gpList1);
genePredFreeList(&gpList2);
genePredFreeList(&kgList);
freeDnaSeqList(&mrnaList);
return 0;
}
