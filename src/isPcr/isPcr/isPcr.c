/* isPcr - Standalone In-Situ PCR Program. */
/* Copyright 2004 Jim Kent all rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genoFind.h"
#include "gfPcrLib.h"
#include "gfClientLib.h"

/* Variables that can be overridden by command line. */
char *ooc = NULL;
int tileSize = 11;
int stepSize = 5;
int minSize = 0;
int maxSize = 4000;
int minPerfect = 15;
int minGood = 15;
char *mask = NULL;
char *makeOoc = NULL;
int repMatch = 1024*4;
double minRepDivergence = 15;
char *out = "fa";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "isPcr - Standalone In-Situ PCR Program\n"
  "usage:\n"
  "   isPcr database query output\n"
  "where database is a fasta, nib, or twoBit file or a text file containing\n"
  "a list of these files,  query is a text file file containing three columns: name,\n"
  "forward primer, and reverse primer,  and output is where the results go.\n"
  "The names 'stdin' and 'stdout' can be used as file names to make using the\n"
  "program in pipes easier.\n"
  "options:\n"
  "   -ooc=N.ooc  Use overused tile file N.ooc.  N should correspond to \n"
  "               the tileSize\n"
  "   -tileSize=N the size of match that triggers an alignment.  \n"
  "               Default is %d .\n"
  "   -stepSize=N spacing between tiles. Default is %d.\n"
  "   -maxSize=N - Maximum size of PCR product (default %d)\n"
  "   -minSize=N - Minimum size of PCR product (default %d)\n"
  "   -minPerfect=N - Minimum size of perfect match at 3' end of primer (default %d)\n"
  "   -minGood=N - Minimum size where there must be 2 matches for each mismatch (default %d)\n"
  "   -mask=type  Mask out repeats.  Alignments won't be started in masked region\n"
  "               but may extend through it in nucleotide searches.  Masked areas\n"
  "               are ignored entirely in protein or translated searches. Types are\n"
  "                 lower - mask out lower cased sequence\n"
  "                 upper - mask out upper cased sequence\n"
  "                 out   - mask according to database.out RepeatMasker .out file\n"
  "                 file.out - mask database according to RepeatMasker file.out\n"
  "   -makeOoc=N.ooc Make overused tile file. Database needs to be complete genome.\n"
  "   -repMatch=N sets the number of repetitions of a tile allowed before\n"
  "               it is marked as overused.  Typically this is 256 for tileSize\n"
  "               12, 1024 for tile size 11, 4096 for tile size 10.\n"
  "               Default is 1024.  Only comes into play with makeOoc\n"
  "   -out=XXX - Output format.  Either\n"
  "      fa - fasta with position, primers in header (default)\n"
  "      bed - tab delimited format. Fields: chrom/start/end/name/score/strand\n"
  , tileSize, stepSize, maxSize, minSize, minPerfect, minGood
  );
}

static struct optionSpec options[] = {
   {"ooc", OPTION_STRING},
   {"tileSize", OPTION_INT},
   {"stepSize", OPTION_INT},
   {"mask", OPTION_STRING},
   {"maxSize", OPTION_INT},
   {"minPerfect", OPTION_INT},
   {"minGood", OPTION_INT},
   {"makeOoc", OPTION_STRING},
   {"repMatch", OPTION_INT},
   {"out", OPTION_STRING},
   {NULL, 0},
};


void pcrStrand(struct genoFind *gf, char *name, char *fPrimer, char *rPrimer, 
	int minSize, int maxSize, char strand, char *outFormat, FILE *f)
/* Do PCR on one strand. */
{
int maxPrimerSize;
struct gfClump *clumpList = NULL, *clump;
struct dnaSeq lSeq;
int fPrimerSize = strlen(fPrimer);
int rPrimerSize = strlen(rPrimer);
maxPrimerSize = max(fPrimerSize, rPrimerSize);
clumpList = gfPcrClumps(gf, fPrimer, fPrimerSize, rPrimer, rPrimerSize, 0, maxSize);
ZeroVar(&lSeq);
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct dnaSeq *seq = clump->target->seq;
    struct gfPcrOutput *gfoList = NULL, *gfo;
    int tStart = clump->tStart - maxPrimerSize;
    int tEnd = clump->tEnd + maxPrimerSize;
    if (tStart < 0)
	tStart = 0;
    if (tEnd > seq->size)
	tEnd = seq->size;
    lSeq.name = seq->name;
    lSeq.dna = seq->dna + tStart;
    lSeq.size = tEnd - tStart;
    if (strand == '-')
        reverseComplement(lSeq.dna, lSeq.size);
    gfPcrLocal(name, &lSeq, tStart, lSeq.name, maxSize, 
	    fPrimer, fPrimerSize, rPrimer, rPrimerSize,
	    minPerfect, minGood, strand, &gfoList);
    gfPcrOutputWriteList(gfoList, outFormat, f);
    gfPcrOutputFreeList(&gfoList);
    if (strand == '-')
        reverseComplement(lSeq.dna, lSeq.size);
    }
gfClumpFreeList(&clumpList);
}

void isPcr(char *dbFile, char *queryFile, char *outFile)
/* isPcr - Standalone In-Situ PCR Program. */
{
char **dbFiles, **queryFiles;
int dbCount, queryCount;
struct dnaSeq *dbSeqList, *seq;
FILE *f = mustOpen(outFile, "w");
boolean showStatus = (f != stdout);
struct genoFind *gf;
struct lineFile *lf;
char *row[3];

gfClientFileArray(dbFile, &dbFiles, &dbCount);
if (makeOoc != NULL)
    {
    gfMakeOoc(makeOoc, dbFiles, dbCount, tileSize, repMatch, gftDna);
    if (showStatus)
	printf("Done making %s\n", makeOoc);
    exit(0);
    }
dbSeqList = gfClientSeqList(dbCount, dbFiles, FALSE, FALSE, mask, 
	minRepDivergence, showStatus);
gf = gfIndexSeq(dbSeqList, 2, 2, tileSize, repMatch, ooc, 
    FALSE, 0, mask != NULL, stepSize);

lf = lineFileOpen(queryFile, TRUE);
while (lineFileRow(lf, row))
    {
    struct gfPcrInput gpi;
    gfPcrInputStaticLoad(row, &gpi);
    pcrStrand(gf, gpi.name, gpi.fPrimer, gpi.rPrimer, 0, maxSize, '+', out, f);
    pcrStrand(gf, gpi.name, gpi.fPrimer, gpi.rPrimer, 0, maxSize, '-', out, f);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
ooc = optionVal("ooc", ooc);
tileSize = optionInt("tileSize", tileSize);
stepSize = optionInt("stepSize", stepSize);
maxSize = optionInt("maxSize", maxSize);
minPerfect = optionInt("minPerfect", minPerfect);
minGood = optionInt("minGood", minGood);
mask = optionVal("mask", mask);
makeOoc = optionVal("makeOoc", makeOoc);
repMatch = optionInt("repMatch", repMatch);
minRepDivergence = optionInt("minRepDivergence", minRepDivergence);
out = optionVal("out", out);
isPcr(argv[1], argv[2], argv[3]);
return 0;
}
