/* chainSplit - Split chains up by target or query sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "chain.h"

static char const rcsid[] = "$Id: chainSplit.c,v 1.5 2004/10/25 19:01:35 kent Exp $";

boolean splitOnQ = FALSE;
int lump = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainSplit - Split chains up by target or query sequence\n"
  "usage:\n"
  "   chainSplit outDir inChain(s)\n"
  "options:\n"
  "   -q  - Split on query (default is on target)\n"
  "   -lump=N  Lump together so have only N pieces.\n"
  );
}

static struct optionSpec options[] = {
   {"q", OPTION_BOOLEAN},
   {"lump", OPTION_INT},
   {NULL, 0},
};

char *lumpName(char *name)
/* Look for integer part of name,  then do mod operation 
 * on it to assign to lump. */
{
char *s = name, c;
for (;;)
    {
    c = *s;
    if (c == 0)
        errAbort("No digits in %s,  need digits in name for lump optoin", 
		name);
    if (isdigit(c))
        {
	static char buf[32];
	int lumpIx = atoi(s) % lump;
	lumpIx %= lump;
	safef(buf, sizeof(buf), "%03d", lumpIx);
	return buf;
	}
    ++s;
    }
}

void chainSplit(char *outDir, int inCount, char *inFiles[])
/* chainSplit - Split chains up by target or query sequence. */
{
struct hash *hash = newHash(0);
int inIx;
makeDir(outDir);
for (inIx = 0; inIx < inCount; ++inIx)
    {
    struct lineFile *lf = lineFileOpen(inFiles[inIx], TRUE);
    struct chain *chain;
    FILE *f;
    while ((chain = chainRead(lf)) != NULL)
        {
	char *name = (splitOnQ ? chain->qName : chain->tName);
	if (lump > 0)
	    name = lumpName(name);
	if ((f = hashFindVal(hash, name)) == NULL)
	    {
	    char path[512];
	    sprintf(path, "%s/%s.chain", outDir, name);
	    f = mustOpen(path, "w");
	    hashAdd(hash, name, f);
	    }
	chainWrite(chain, f);
	chainFree(&chain);
	}
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
splitOnQ = optionExists("q");
lump = optionInt("lump", 0);
if (argc < 3)
    usage();
chainSplit(argv[1], argc-2, argv+2);
return 0;
}
