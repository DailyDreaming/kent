/* liftOver - Move annotations from one assembly to another. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "chain.h"
#include "bed.h"
#include "genePred.h"
#include "sample.h"
#include "liftOver.h"

static char const rcsid[] = "$Id: liftOver.c,v 1.14 2004/09/08 01:42:06 kate Exp $";

double minMatch = LIFTOVER_MINMATCH;
double minBlocks = LIFTOVER_MINBLOCKS;
double minSizeT = LIFTOVER_MINSIZE_TARGET;
double minSizeQ = LIFTOVER_MINSIZE_QUERY;
bool fudgeThick = FALSE;
bool errorHelp = FALSE;
bool multiple = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftOver - Move annotations from one assembly to another\n"
  "usage:\n"
  "   liftOver oldFile map.chain newFile unMapped\n"
  "oldFile and newFile are in bed format by default, but can be in GFF and\n"
  "maybe eventually others with the appropriate flags below.\n"
  "options:\n"
  "   -minMatch=0.N Minimum ratio of bases that must remap. Default %3.2f\n"
  "   -gff  File is in gff/gtf format.  Note that the gff lines are converted\n"
  "         separately.  It would be good to have a separate check after this\n"
  "         that the lines that make up a gene model still make a plausible gene\n"
  "         after liftOver\n"
  "   -genePred - File is in genePred format\n"
  "   -sample - File is in sample format\n"
  "   -pslT - File is in psl format, map target side only\n"
  "   -minBlocks=0.N Minimum ratio of alignment blocks/exons that must map.\n"
  "                  Default %3.2f\n"
  "   -fudgeThick    If thickStart/thickEnd is not mapped, use the closest \n"
  "                  mapped base.  Recommended if using -minBlocks.\n"
  "   -multiple               Allow multiple output regions.\n"
  "   -minSizeT, -minSizeQ    Minimum chain size in target/query,\n" 
  "                             when mapping to multiple output regions\n"
  "                                     (default 4K, 20K).\n"
  "   -multiple               Allow multiple output regions.\n"
  "   -errorHelp              Explain error messages.\n",
    LIFTOVER_MINMATCH, LIFTOVER_MINBLOCKS
    //LIFTOVER_MINSIZE_TARGET, LIFTOVER_MINSIZE_QUERY
  );
}

void liftOver(char *oldFile, char *mapFile, double minMatch, 
                double minBlocks, int minSizeT, int minSizeQ,
                bool multiple, char *newFile, char *unmappedFile)
/* liftOver - Move annotations from one assembly to another. */
{
struct hash *chainHash = newHash(0);		/* Old chromosome name keyed, chromMap valued. */
FILE *mapped = mustOpen(newFile, "w");
FILE *unmapped = mustOpen(unmappedFile, "w");
int errCt;

if (!fileExists(oldFile))
    errAbort("Can't find file: %s\n", oldFile);
fprintf(stderr, "Reading liftover chains\n");
readLiftOverMap(mapFile, chainHash);
fprintf(stderr, "Mapping coordinates\n");
if (optionExists("gff"))
    liftOverGff(oldFile, chainHash, minMatch, minBlocks, mapped, unmapped);
else if (optionExists("genePred"))
    liftOverGenePred(oldFile, chainHash, minMatch, minBlocks, fudgeThick,
                        mapped, unmapped);
else if (optionExists("sample"))
    liftOverSample(oldFile, chainHash, minMatch, minBlocks, fudgeThick,
                        mapped, unmapped);
else if (optionExists("pslT"))
    liftOverPsl(oldFile, chainHash, minMatch, minBlocks, fudgeThick,
                        mapped, unmapped);
else
    liftOverBed(oldFile, chainHash, minMatch, minBlocks, minSizeT, minSizeQ, 
                    fudgeThick, mapped, unmapped, multiple, &errCt);
carefulClose(&mapped);
carefulClose(&unmapped);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
minMatch = optionFloat("minMatch", minMatch);
minBlocks = optionFloat("minBlocks", minBlocks);
fudgeThick = optionExists("fudgeThick");
multiple = optionExists("multiple");
minSizeT = optionInt("minSizeT", minSizeT);
minSizeQ = optionInt("minSizeQ", minSizeQ);
if (optionExists("errorHelp"))
    errAbort(liftOverErrHelp());
if (argc != 5)
    usage();
liftOver(argv[1], argv[2], minMatch, minBlocks, minSizeT, minSizeQ, 
                        multiple, argv[3], argv[4]);
return 0;
}
