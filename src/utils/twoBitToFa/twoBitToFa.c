/* twoBitToFa - Convert all or part of twoBit file to fasta. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: twoBitToFa.c,v 1.6 2005/03/24 12:57:33 baertsch Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twoBitToFa - Convert all or part of .2bit file to fasta\n"
  "usage:\n"
  "   twoBitToFa input.2bit output.fa\n"
  "options:\n"
  "   -seq=name - restrict this to just one sequence\n"
  "   -start=X  - start at given position in sequence (zero-based)\n"
  "   -end=X - end at given position in sequence (non-inclusive)\n"
  "\n"
  "Sequence and range may also be specified as part of the input\n"
  "file name using the syntax:\n"
  "      /path/input.2bit:name\n"
  "   or\n"
  "      /path/input.2bit:name:start-end\n"
  );
}

char *clSeq = NULL;	/* Command line sequence. */
int clStart = 0;	/* Start from command line. */
int clEnd = 0;		/* End from command line. */

static struct optionSpec options[] = {
   {"seq", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {NULL, 0},
};

void outputOne(struct twoBitFile *tbf, char *seqName, FILE *f, 
	int start, int end)
/* Output sequence. */
{
struct dnaSeq *seq = twoBitReadSeqFrag(tbf, seqName, start, end);
faWriteNext(f, seq->name, seq->dna, seq->size);
dnaSeqFree(&seq);
}

void outputFa(struct dnaSeq *seq, FILE *f)
/* Output sequence. */
{
faWriteNext(f, seq->name, seq->dna, seq->size);
}

void twoBitToFa(char *inName, char *outName)
/* twoBitToFa - Convert all or part of twoBit file to fasta. */
{
FILE *outFile = mustOpen(outName, "w");
struct dnaSeq *seq, *seqList = NULL ; //twoBitReadSeqFrag(tbf, seqName, start, end);

/* check for sequence/range in path */

if (clSeq == NULL)
    {
    seqList = twoBitLoadAll(inName);
    }
else
    {
    char newSpec[512];
    safef(newSpec, sizeof(newSpec), "%s:%s:%d-%d",inName,clSeq, clStart, clEnd);
    seqList = twoBitLoadAll(newSpec);
    }

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    outputFa(seq, outFile);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clSeq = optionVal("seq", clSeq);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
dnaUtilOpen();
twoBitToFa(argv[1], argv[2]);
return 0;
}
