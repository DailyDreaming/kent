/* getRnaPred - Get RNA for gene predictions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "genePred.h"
#include "fa.h"
#include "jksql.h"
#include "hdb.h"
#include "options.h"

static char const rcsid[] = "$Id: getRnaPred.c,v 1.6 2003/05/06 07:22:19 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getRnaPred - Get RNA for gene predictions\n"
  "usage:\n"
  "   getRnaPred database table chromosome output.fa\n"
  "options:\n"
  "   -weird Only get ones with weird splice sites\n"
  );
}

void getRna(char *table, char *chrom, char *faOut)
/* getRna - Get RNA for gene predictions and write to file. */
{
boolean weird = optionExists("weird");
int rowOffset;
char **row;
int i, start, end, size;
struct genePred *gp;
struct dnaSeq *seq;
struct dyString *dna = dyStringNew(16*1024);
FILE *f = mustOpen(faOut, "w");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = hChromQuery(conn, table, chrom, NULL, &rowOffset);
/* Open file and verify it's in good nibble format. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* Load gene prediction from database. */
    gp = genePredLoad(row+rowOffset);

    /* Load exons one by one into dna string. */
    dyStringClear(dna);
    if (weird)
        {
	boolean gotOdd = FALSE;
	for (i=1; i<gp->exonCount; ++i)
	    {
	    start = gp->exonEnds[i-1];
	    end = gp->exonStarts[i];
	    size = end - start;
	    if (size > 0)
		{
		DNA *s, *e;
		seq = hDnaFromSeq(chrom, start, end, dnaLower);
		s = seq->dna;
		e = seq->dna + seq->size;
		uglyf("%s %c%c/%c%c\n", gp->name, s[0], s[1], e[-2], e[-1]);
		if (gp->strand[0] == '-')
		    reverseComplement(seq->dna, seq->size);
		if (s[0] != 'g' || (s[1] != 't' && s[1] != 'c') || e[-2] != 'a' || e[-1] != 'g')
		    gotOdd = TRUE;
		freeDnaSeq(&seq);
		}
	    }
	if (!gotOdd)
	    continue;
	}
    for (i=0; i<gp->exonCount; ++i)
        {
	start = gp->exonStarts[i];
	end = gp->exonEnds[i];
	size = end - start;
	if (size < 0)
	    warn("%d sized exon in %s\n", size, gp->name);
	else
	    {
	    seq = hDnaFromSeq(chrom, start, end, dnaLower);
	    dyStringAppendN(dna, seq->dna, size);
	    }
	freeDnaSeq(&seq);
	}

    /* Reverse complement if necessary and output. */
    if (gp->strand[0] == '-')
        reverseComplement(dna->string, dna->stringSize);
    faWriteNext(f, gp->name, dna->string, dna->stringSize);
    }

/* Cleanup and go home. */
dyStringFree(&dna);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void getRnaPred(char *database, char *table, char *chrom, char *faOut)
/* Set database and call function that does real work. */
{
hSetDb(database);
getRna(table, chrom, faOut);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
getRnaPred(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
