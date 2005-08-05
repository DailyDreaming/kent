/* mafFilter - Filter out maf files. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"

static char const rcsid[] = "$Id: mafFilter.c,v 1.9 2005/08/05 00:24:25 kate Exp $";

#define DEFAULT_MIN_ROW 2
#define DEFAULT_MIN_COL 1
#define DEFAULT_FACTOR 5

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFilter - Filter out maf files. Output goes to standard out\n"
  "usage:\n"
  "   mafFilter file(s).maf\n"
  "options:\n"
  "   -tolerate - Just ignore bad input rather than aborting.\n"
  "   -minCol=N - Filter out blocks with fewer than N columns (default %d)\n"
  "   -minRow=N - Filter out blocks with fewer than N rows (default %d)\n"
  "   -factor - Filter out scores below -minFactor * (ncol**2) * nrow\n"
        /* score cutoff recommended by Webb Miller */
  "   -minFactor=N - Factor to use with -minFactor (default %d)\n"
  "   -minScore=N - Minimum allowed score (alternative to -minFactor)\n"
  "   -reject=filename - Save rejected blocks in filename\n"
  "   -needComp=species - all alignments must have species as one of the component\n"
  "   -overlap - Reject overlapping blocks in reference (assumes ordered blocks)\n",
        DEFAULT_MIN_COL, DEFAULT_MIN_ROW, DEFAULT_FACTOR
  );
}

static struct optionSpec options[] = {
   {"tolerate", OPTION_BOOLEAN},
   {"minCol", OPTION_INT},
   {"minRow", OPTION_INT},
   {"minScore", OPTION_FLOAT},
   {"factor", OPTION_BOOLEAN},
   {"minFactor", OPTION_INT},
   {"reject", OPTION_STRING},
   {"needComp", OPTION_STRING},
   {"overlap", OPTION_BOOLEAN},
   {NULL, 0},
};

int minCol = DEFAULT_MIN_COL;
int minRow = DEFAULT_MIN_ROW;
boolean gotMinScore = FALSE;
double minScore;
boolean gotMinFactor = FALSE;
int minFactor = DEFAULT_FACTOR;
char *rejectFile = NULL;
char *needComp = NULL;
/* for overlap detection */
int prevRefStart = 0, prevRefEnd = 0;

static jmp_buf recover;    /* Catch errors in load maf the hard way in C. */

static void abortHandler()
/* Abort fuzzy finding. */
{
if (optionExists("tolerate"))
    {
    longjmp(recover, -1);
    }
else
    exit(-1);
}

boolean filterOne(struct mafAli *maf)
/* Return TRUE if maf passes filters. */
{
int ncol = maf->textSize;
int nrow = slCount(maf->components);
double ncol2 = ncol * ncol;
double fscore = -minFactor * ncol2 * nrow;
int refStart = maf->components->start;
int refEnd = refStart + maf->components->size;

if (needComp && (mafMayFindCompPrefix(maf, needComp, "." ) == NULL))
    return FALSE;

if (nrow < minRow || ncol < minCol ||
    (gotMinScore && maf->score < minScore) ||
    (gotMinFactor && maf->score < fscore))
    {
    verbose(3, "col=%d, row=%d, score=%f, fscore=%f\n", 
                        ncol, nrow, maf->score, fscore);
    verbose(3, "ncol**2=%d\n", ncol * ncol);
    return FALSE;
    }
if (optionExists("overlap"))
    {
    if (refStart < prevRefEnd && refEnd > prevRefStart)
        return FALSE;
    }
prevRefStart = refStart;
prevRefEnd = refEnd;
return TRUE;
}

void mafFilter(int fileCount, char *files[])
/* mafFilter - Filter out maf files. */
{
FILE *f = stdout;
FILE *rf = NULL;
int i;
int status;
int rejects = 0;

for (i=0; i<fileCount; ++i)
    {
    char *fileName = files[i];
    struct mafFile *mf = mafOpen(fileName);
    struct mafAli *maf = NULL;
    if (i == 0)
        {
        mafWriteStart(f, mf->scoring);
        if (rejectFile && rf == NULL)
            {
            rf = mustOpen(rejectFile, "w");
            mafWriteStart(rf, mf->scoring);
            }
        }
    for (;;)
	{
	/* Effectively wrap try/catch around mafNext() */
	status = setjmp(recover);
	if (status == 0)    /* Always true except after long jump. */
	    {
	    pushAbortHandler(abortHandler);
	    maf = mafNext(mf);
	    if (maf != NULL)
                {
                if (filterOne(maf))
                    mafWrite(f, maf);
                else 
                    {
                    rejects++;
                    if (rf)
                        mafWrite(rf, maf);
                    }
                }
	    }
	popAbortHandler();
	if (maf == NULL)
	    break;
	}
    mafFileFree(&mf);
    }
fprintf(stderr, "rejected %d blocks\n", rejects);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
if (optionExists("minScore"))
    {
    gotMinScore = TRUE;
    minScore = optionFloat("minScore", 0);
    }
if (optionExists("factor"))
    {
    if (gotMinScore)
        errAbort("can't use both -minScore and -minFactor");
    gotMinFactor = TRUE;
    minFactor = optionInt("minFactor", DEFAULT_FACTOR);
    }
minCol = optionInt("minCol", DEFAULT_MIN_COL);
minRow = optionInt("minRow", DEFAULT_MIN_ROW);
rejectFile = optionVal("reject", NULL);
needComp = optionVal("needComp", NULL);
verbose(3, "minCol=%d, minRow=%d, gotMinScore=%d, minScore=%f, gotMinFactor=%d, minFactor=%d\n", 
        minCol, minRow, gotMinScore, minScore, gotMinFactor, minFactor);

mafFilter(argc-1, argv+1);
return 0;
}
