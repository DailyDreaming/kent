/* netChainSubset - Create chain file with subset of chains that appear in the net. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"
#include "chainNet.h"

static char const rcsid[] = "$Id: netChainSubset.c,v 1.7 2005/01/10 00:38:44 kent Exp $";

char *type = NULL;
boolean splitOnInsert = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netChainSubset - Create chain file with subset of chains that appear in the net\n"
  "usage:\n"
  "   netChainSubset in.net in.chain out.chain\n"
  "options:\n"
  "   -gapOut=gap.tab - Output gap sizes to file\n"
  "   -type=XXX - Restrict output to particular type in net file\n"
  "   -splitOnInsert - Split chain when get an insertion of another chain\n"
  );
}

struct optionSpec options[] = {
   {"gapOut", OPTION_STRING},
   {"type", OPTION_STRING},
   {"splitOnInsert", OPTION_BOOLEAN},
   {NULL, 0},
};


void gapWrite(struct chain *chain, FILE *f)
/* Write gaps to simple two column file. */
{
struct cBlock *a, *b;
a = chain->blockList;
for (b = a->next; b != NULL; b = b->next)
    {
    fprintf(f, "%d\t%d\n", b->tStart - a->tEnd, b->qStart - a->qEnd);
    a = b;
    }
}

void writeChainPart(struct chain *chain, int tStart, int tEnd, FILE *f,
	FILE *gapFile)
/* Write out part of a chain. */
{
struct chain *subChain, *chainToFree;

chainSubsetOnT(chain, tStart, tEnd, &subChain, &chainToFree);
assert(subChain != NULL);
chainWrite(subChain, f);
if (gapFile != NULL)
    gapWrite(subChain, gapFile);
chainFree(&chainToFree);
}

struct cnFill *nextGapWithInsert(struct cnFill *gapList)
/* Find next in list that has a non-empty child.   */
{
struct cnFill *gap;
for (gap = gapList; gap != NULL; gap = gap->next)
    {
    if (gap->children != NULL)
        break;
    }
return gap;
}

void splitWrite(struct cnFill *fill, struct chain *chain, 
    FILE *f, FILE *gapFile)
/* Split chain into pieces if it has inserts.  Write out
 * each piece. */
{
int tStart = fill->tStart, tEnd;
struct cnFill *child = fill->children;

for (;;)
    {
    child = nextGapWithInsert(child);
    if (child == NULL)
        break;
    writeChainPart(chain, tStart, child->tStart, f, gapFile);
    tStart = child->tStart + child->tSize;
    child = child->next;
    }
writeChainPart(chain, tStart, fill->tStart + fill->tSize, f, gapFile);
}

void convertFill(struct cnFill *fill, 
	struct chain *chain, FILE *f, FILE *gapFile)
/* Convert subset of chain as defined by fill to axt. */
{
if (type != NULL)
    {
    if (!sameString(type, fill->type))
        return;
    }
if (splitOnInsert)
    splitWrite(fill, chain, f, gapFile);
else
    writeChainPart(chain, fill->tStart, fill->tStart + fill->tSize, f, gapFile);
}

void rConvert(struct cnFill *fillList, 
	struct hash *chainHash, FILE *f, FILE *gapFile)
/* Recursively output chains in net as axt. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        convertFill(fill, 
		chainLookup(chainHash, fill->chainId), f, gapFile);
    if (fill->children)
        rConvert(fill->children, chainHash, f, gapFile);
    }
}

void netChainSubset(char *netIn, char *chainIn, char *chainOut)
/* netChainSubset - Create chain file with subset of *
 * chains that appear in the net. */
{
struct hash *chainHash;
struct chainNet *net;
struct lineFile *lf = lineFileOpen(netIn, TRUE);
FILE *f = mustOpen(chainOut, "w");
char *gapFileName = optionVal("gapOut", NULL);
FILE *gapFile = NULL;

if (gapFileName)
    gapFile = mustOpen(gapFileName, "w");
chainHash = chainReadAll(chainIn);
while ((net = chainNetRead(lf)) != NULL)
    {
    fprintf(stderr, "Processing %s\n", net->name);
    rConvert(net->fillList, chainHash, f, gapFile);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
type = optionVal("type", type);
splitOnInsert = optionExists("splitOnInsert");
if (argc != 4)
    usage();
netChainSubset(argv[1], argv[2], argv[3]);
return 0;
}
