/* netToAxt - Convert net (and chain) to axt.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainBlock.h"
#include "chainNet.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "chainToAxt.h"
#include "nib.h"
#include "axt.h"

static char const rcsid[] = "$Id: netToAxt.c,v 1.17 2003/12/12 11:20:52 kate Exp $";

boolean qChain = FALSE;  /* Do chain from query side. */
int maxGap = 100;
boolean noCache = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToAxt - Convert net (and chain) to axt.\n"
  "usage:\n"
  "   netToAxt in.net in.chain tNibDir qNibDir out.axt\n"
  "options:\n"
  "   -qChain - net is with respect to the q side of chains.\n"
  "   -maxGap=N - maximum size of gap before breaking. Default %d\n"
  "   -gapOut=gap.tab - Output gap sizes to file\n"
  "   -noCache - suppress nib cacheing -- use if too many open files\n"
  ,  maxGap
  );
}

void writeGaps(struct chain *chain, FILE *f)
/* Write gaps to simple two column file. */
{
struct boxIn *a, *b;
a = chain->blockList;
for (b = a->next; b != NULL; b = b->next)
    {
    fprintf(f, "%d\t%d\n", b->tStart - a->tEnd, b->qStart - a->qEnd);
    a = b;
    }
}

void writeAxtFromChain(struct chain *chain, struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, FILE *f, FILE *gapFile)
/* Write out axt's that correspond to chain. */
{
struct axt *axt, *axtList;

if (gapFile != NULL)
    writeGaps(chain, gapFile);
axtList = chainToAxt(chain, qSeq, qOffset, tSeq, tOffset, maxGap, BIGNUM);
for (axt = axtList; axt != NULL; axt = axt->next)
    axtWrite(axt, f);
axtFreeList(&axtList);
}

void convertFill(struct cnFill *fill, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, FILE *f, FILE *gapFile)
/* Convert subset of chain as defined by fill to axt. */
{
struct dnaSeq *qSeq;
boolean isRev = (chain->qStrand == '-');
struct chain *subChain, *chainToFree;
int qOffset;
char path[PATH_LEN];

/* Get query sequence fragment. */
    {
    struct nibInfo *nib;
    if (noCache)
        {
        safef(path, sizeof(path), "%s/%s.nib", nibDir, fill->qName);
        nib = nibInfoNew(path);
        }
    else
        nib = nibInfoFromCache(qChromHash, nibDir, fill->qName);
    qSeq = nibLoadPartMasked(NIB_MASK_MIXED, nib->fileName, 
    	fill->qStart, fill->qSize);
    if (noCache)
        //nibInfoFree(&nib);
        carefulClose(&nib->f);
    if (isRev)
	{
        reverseComplement(qSeq->dna, qSeq->size);
	qOffset = nib->size - (fill->qStart + fill->qSize);
	}
    else
	qOffset = fill->qStart;
    }
chainSubsetOnT(chain, fill->tStart, fill->tStart + fill->tSize, 
	&subChain, &chainToFree);
assert(subChain != NULL);
writeAxtFromChain(subChain, qSeq, qOffset, tChrom, 0, f, gapFile);
chainFree(&chainToFree);
freeDnaSeq(&qSeq);
}

void rConvert(struct cnFill *fillList, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *qNibDir, struct hash *chainHash, 
	FILE *f, FILE *gapFile)
/* Recursively output chains in net as axt. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        convertFill(fill, tChrom, qChromHash, qNibDir, 
		chainLookup(chainHash, fill->chainId), f, gapFile);
    if (fill->children)
        rConvert(fill->children, tChrom, qChromHash, qNibDir, chainHash, 
		f, gapFile);
    }
}

#define maxChainId (256*1024*1024)

Bits *findUsedIds(char *netFileName)
/* Create a bit array with 1's corresponding to
 * chainId's used in net file. */
{
struct lineFile *lf = lineFileOpen(netFileName, TRUE);
Bits *bits = bitAlloc(maxChainId);
struct chainNet *net;
while ((net = chainNetRead(lf)) != NULL)
    {
    chainNetMarkUsed(net, bits, maxChainId);
    chainNetFree(&net);
    }
lineFileClose(&lf);
return bits;
}


void netToAxt(char *netName, char *chainName, char *tNibDir, char *qNibDir, char *axtName)
/* netToAxt - Convert net (and chain) to axt.. */
{
Bits *usedBits = findUsedIds(netName);
struct hash *chainHash;
struct chainNet *net;
struct lineFile *lf = lineFileOpen(netName, TRUE);
FILE *f = mustOpen(axtName, "w");
struct dnaSeq *tChrom = NULL;
struct hash *qChromHash = hashNew(0);
char path[512];
char *gapFileName = optionVal("gapOut", NULL);
FILE *gapFile = NULL;

if (gapFileName)
    gapFile = mustOpen(gapFileName, "w");
chainHash = chainReadUsedSwap(chainName, qChain, usedBits);
bitFree(&usedBits);
while ((net = chainNetRead(lf)) != NULL)
    {
    fprintf(stderr, "Processing %s\n", net->name);
    sprintf(path, "%s/%s.nib", tNibDir, net->name);
    tChrom = nibLoadAllMasked( NIB_MASK_MIXED ,path);
    if (tChrom->size != net->size)
        errAbort("Size mismatch on %s.  Net/nib out of sync or possibly nib dirs swapped?", 
		tChrom->name);
    fprintf(stderr, "loaded %s\n", path);

    rConvert(net->fillList, tChrom, qChromHash, qNibDir, chainHash, f, gapFile);
    freeDnaSeq(&tChrom);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionHash(&argc, argv);
qChain = optionExists("qChain");
maxGap = optionInt("maxGap", maxGap);
noCache = optionExists("noCache");
if (argc != 6)
    usage();
netToAxt(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
