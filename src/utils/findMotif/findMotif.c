/* findMotif - find specified motif in nib files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "portable.h"

static char const rcsid[] = "$Id: findMotif.c,v 1.7 2004/04/12 22:49:46 hiram Exp $";

char *chr = (char *)NULL;	/*	process the one chromosome listed */
char *motif = (char *)NULL;	/*	specified motif string */
unsigned motifLen = 0;		/*	length of motif	*/
unsigned long long motifVal;	/*	motif converted to a number	*/
unsigned long long complementVal;	/*	- strand complement	*/
boolean bedOutput = TRUE;	/*	output bed file (default) */
boolean wigOutput = FALSE;	/*  output wiggle format instead of bed file */
char *strand = (char *)NULL;
boolean doPlusStrand = TRUE;	/*	output bed file instead of wiggle */
boolean doMinusStrand = TRUE;	/*	output bed file instead of wiggle */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "findMotif - find specified motif in nib files\n"
  "usage:\n"
  "   findMotif [options] -motif=<acgt...> nibDir\n"
  "options:\n"
  "   nibDir - path to nib directory, relative or absolute path OK\n"
  "   -motif=<acgt...> - search for this specified motif (case ignored, [acgt] only)\n"
  "   -chr=<chrN> - process only this one chrN from the nibDir\n"
  "   -strand=<+|-> - limit to only one strand.  Default is both.\n"
  "   -bedOutput - output bed format (this is the default)\n"
  "   -wigOutput - output wiggle data format instead of bed file\n"
  "   -verbose=N - set information level [1-4]\n"
  "   NOTE: motif must be longer than 4 characters, less than 17\n"
  "   -verbose=4 - will display gaps as bed file data lines to stderr"
  );
}

static struct optionSpec options[] = {
   {"chr", OPTION_STRING},
   {"strand", OPTION_STRING},
   {"motif", OPTION_STRING},
   {"bedOutput", OPTION_BOOLEAN},
   {"wigOutput", OPTION_BOOLEAN},
   {NULL, 0},
};

static void scanNib(char *nibFile, char *chrom)
{
int chromSize, start, end, blockSize;
int i, val;
int winSize=131072;
FILE *nf = NULL;
struct dnaSeq *seq = NULL;
DNA *dna;
int blockCount = 0;
unsigned long long mask;
unsigned long long chromPosition = 0;
register unsigned long long incomingVal = 0;
unsigned long long incomingLength = 0;
unsigned long long posFound = 0;
unsigned long long negFound = 0;
unsigned long long posPreviousPosition = 0;
unsigned long long negPreviousPosition = 0;
register unsigned long long posNeedle = motifVal;
register unsigned long long negNeedle = complementVal;
unsigned long long enterGap = 1;
unsigned long long gapCount = 0;

nibOpenVerify(nibFile, &nf, &chromSize);
mask = 3;
for (i=1; i < motifLen; ++i )
	mask = (mask << 2) | 3;
verbose(2, "#\tnib: %s size: %d, motifMask: %#lx\n", nibFile, chromSize, mask);
verbose(2, "#\tmotif numerical value: %llu (%#lx)\n", posNeedle, posNeedle);
for (start=0; start<chromSize; start = end)
    {
    end = start + winSize;
    if (end > chromSize)
	end = chromSize;
    blockSize = end - start;
    seq = nibLdPart(nibFile, nf, chromSize, start, blockSize);
    dna = seq->dna;
    for (i=0; i < blockSize; ++i)
	{
        ++chromPosition;
	val = ntVal[dna[i]];
	switch (val)
	    {
	    case T_BASE_VAL:
	    case C_BASE_VAL:
	    case A_BASE_VAL:
	    case G_BASE_VAL:
    		incomingVal = mask & ((incomingVal << 2) | val);
		if (! incomingLength)
		    {
		    if ((chromPosition - enterGap) > 0)
			{
			++gapCount;
			verbose(3,
			    "#\treturn from gap at %llu, gap length: %llu\n",
			    chromPosition, chromPosition - enterGap);
			verbose(4, "#GAP %s\t%llu\t%llu\t%llu\t%llu\t%s\n",
			    chrom, enterGap-1, chromPosition-1, gapCount,
			    chromPosition - enterGap, "+");
			}
		    }
		++incomingLength;

		if (doPlusStrand && (incomingLength >= motifLen)
			&& (incomingVal == posNeedle))
		    {
		    ++posFound;
		    if (wigOutput)
		printf("%llu 1 %#llx == %#llx\n", chromPosition-motifLen+1, incomingVal&mask,posNeedle);
		    else
		printf("%s\t%llu\t%llu\t%llu\t%d\t%s\n", chrom, chromPosition-motifLen, chromPosition, posFound+negFound, 1000, "+");

		    if ((posPreviousPosition + motifLen) > chromPosition)
verbose(2, "#\toverlapping + at: %s:%llu-%llu\n", chrom, posPreviousPosition, chromPosition);
		    posPreviousPosition = chromPosition;
		    }

		if (doMinusStrand && (incomingLength >= motifLen)
			&& (incomingVal == negNeedle))
		    {
		    ++negFound;
		    if (wigOutput)
		printf("%llu -1 %#llx == %#llx\n", chromPosition-motifLen+1, incomingVal&mask,negNeedle);
		    else
		printf("%s\t%llu\t%llu\t%llu\t%d\t%s\n", chrom, chromPosition-motifLen, chromPosition, posFound+negFound, 1000, "-");

		    if ((negPreviousPosition + motifLen) > chromPosition)
verbose(2, "#\toverlapping - at: %s:%llu-%llu\n", chrom, negPreviousPosition, chromPosition);
		    negPreviousPosition = chromPosition;
		    }
		break;

	    default:
		if (incomingLength)
		    {
		    verbose(3, "#\tenter gap at %llu\n", chromPosition);
		    enterGap = chromPosition;
		    }
    		incomingVal = 0;
    		incomingLength = 0;
		break;
	    }
	}
    freeDnaSeq(&seq);
    ++blockCount;
    }

verbose(2, "#\tfound: %llu times + strand, %llu times - strand\n",
    posFound, negFound );
verbose(2, "#\t%% of chromosome: %g %% + strand %g %% - strand\n",
    (double)(posFound*motifLen)/(double)chromPosition,
    (double)(negFound*motifLen)/(double)chromPosition);

carefulClose(&nf);
}

static void findMotif(char *nibDir)
/* findMotif - find specified motif in nib files. */
{
char dir[256], chrom[128], ext[64];
struct fileInfo *nibList = listDirX(nibDir, "*.nib", TRUE), *nibEl;

for (nibEl = nibList; nibEl != NULL; nibEl = nibEl->next)
    {
    splitPath(nibEl->name, dir, chrom, ext);
    if (chr)
	{
	char chrNib[256];
	safef(chrNib, ArraySize(chrNib), "%s/%s.nib", nibDir, chr);
	verbose(3, "#\tchecking name: %s =? %s\n", chrNib, nibEl->name);
	if (sameString(chrNib,nibEl->name))
	    {
	    verbose(2, "#\tprocessing: %s\n", nibEl->name);
	    scanNib(nibEl->name, chrom);
	    }
	}
    else
	{
	verbose(2, "#\tprocessing: %s\n", nibEl->name);
	scanNib(nibEl->name, chrom);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
char *cp;
unsigned long long reversed;

optionInit(&argc, argv, options);

if (argc < 2)
    usage();

dnaUtilOpen();

motif = optionVal("motif", NULL);
chr = optionVal("chr", NULL);
strand = optionVal("strand", NULL);
bedOutput = optionExists("bedOutput");
wigOutput = optionExists("wigOutput");

if (wigOutput)
    bedOutput = FALSE;
else
    bedOutput = TRUE;

if (chr)
    verbose(2, "#\tprocessing chr: %s\n", chr);
if (strand)
    verbose(2, "#\tprocessing strand: '%s'\n", strand);
if (motif)
    verbose(2, "#\tsearching for motif: %s\n", motif);
else {
    warn("ERROR: -motif string empty, please specify a motif\n");
    usage();
}
verbose(2, "#\ttype output: %s\n", wigOutput ? "bed format" : "wiggle data");
verbose(2, "#\tspecified nibDir: %s\n", argv[1]);
verbose(2, "#\tsizeof(motifVal): %d\n", sizeof(motifVal));
if (strand)
    {
    if (! (sameString(strand,"+") | sameString(strand,"-")))
	{
	warn("ERROR: -strand specified ('%s') is not + or - ?\n", strand);
	usage();
	}
    /*	They are both on by default, turn off the one not specified */
    if (sameString(strand,"-"))
	doPlusStrand = FALSE;
    if (sameString(strand,"+"))
	doMinusStrand = FALSE;
    }
motifLen = strlen(motif);
/*	at two bits per character, size limit of motif is
 *	number of bits in motifVal / 2
 */
if (motifLen > (4*sizeof(motifVal))/2 )
    {
    warn("ERROR: motif string too long, limit %d\n", (4*sizeof(motifVal))/2 );
    usage();
    }
cp = motif;
motifVal = 0;
complementVal = 0;
for (i = 0; i < motifLen; ++i)
    {
	switch (*cp)
	{
	case 'a':
	case 'A':
	    motifVal = (motifVal << 2) | A_BASE_VAL;
	    complementVal = (complementVal << 2) | T_BASE_VAL;
	    break;
	case 'c':
	case 'C':
	    motifVal = (motifVal << 2) | C_BASE_VAL;
	    complementVal = (complementVal << 2) | G_BASE_VAL;
	    break;
	case 'g':
	case 'G':
	    motifVal = (motifVal << 2) | G_BASE_VAL;
	    complementVal = (complementVal << 2) | C_BASE_VAL;
	    break;
	case 't':
	case 'T':
	    motifVal = (motifVal << 2) | T_BASE_VAL;
	    complementVal = (complementVal << 2) | A_BASE_VAL;
	    break;
	default:
	    warn(
		"ERROR: character in motif: '%c' is not one of ACGT\n", *cp);
	    usage();
	}
	++cp;
    }
reversed = 0;
for (i = 0; i < motifLen; ++i)
    {
    int base;
    base = complementVal & 3;
    reversed = (reversed << 2) | base;
    complementVal >>= 2;
    }
complementVal = reversed;
verbose(2, "#\tmotif numerical value: %llu (%#llx)\n", motifVal, motifVal);
verbose(2, "#\tcomplement numerical value: %llu (%#llx)\n", complementVal, complementVal);
if (motifLen < 5)
    {
    warn("ERROR: motif string must be more than 4 characters\n");
    usage();
    }

findMotif(argv[1]);
return 0;
}
