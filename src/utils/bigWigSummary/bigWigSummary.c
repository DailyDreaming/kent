/* bigWigSummary - Extract summary information from a bigWig file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bigWigSummary.c,v 1.8 2009/02/01 04:02:25 kent Exp $";

char *summaryType = "mean";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigSummary - Extract summary information from a bigWig file.\n"
  "usage:\n"
  "   bigWigSummary file.bigWig chrom start end dataPoints\n"
  "Get summary data from bigWig for indicated region, broken into\n"
  "dataPoints equal parts.  (Use dataPoints=1 for simple summary.)\n"
  "options:\n"
  "   -type=X where X is one of:\n"
  "         mean - average value in region\n"
  "         min - minimum value in region\n"
  "         max - maximum value in region\n"
  "         coverage - %% of region that is covered\n"
  );
}

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {NULL, 0},
};

void bigWigSummary(char *bigWigFile, char *chrom, int start, int end, int dataPoints)
/* bigWigSummary - Extract summary information from a bigWig file.. */
{
/* Make up values array initialized to not-a-number. */
double nan0 = nan("");
double summaryValues[dataPoints];
int i;
for (i=0; i<dataPoints; ++i)
    summaryValues[i] = nan0;

if (bigWigSummaryArray(bigWigFile, chrom, start, end, bigWigSummaryTypeFromString(summaryType), 
      dataPoints, summaryValues))
    {
    for (i=0; i<dataPoints; ++i)
	{
	double val = summaryValues[i];
	if (i != 0)
	    printf("\t");
	if (isnan(val))
	    printf("n/a");
	else
	    printf("%g", val);
	}
    printf("\n");
    }
else
    {
    errAbort("no data in region %s:%d-%d in %s\n", chrom, start, end, bigWigFile);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
summaryType = optionVal("type", summaryType);
bigWigSummary(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]), sqlUnsigned(argv[5]));
return 0;
}
