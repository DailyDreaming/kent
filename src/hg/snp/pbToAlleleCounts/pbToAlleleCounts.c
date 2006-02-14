/* pbToAlleleCounts.c -- reformat Seattel SNP data from prettybase format to bed with alleles and counts */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "hdb.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "dystring.h"
#include "options.h"

static char const rcsid[] = "$Id: pbToAlleleCounts.c,v 1.1 2006/02/14 00:16:53 daryl Exp $";

static int ssnpId=0;
boolean strictSnp=FALSE;
boolean strictBiallelic=FALSE;

static struct optionSpec optionSpecs[] =
/* command line option specifications */
{
    {"strictBiallelic", OPTION_BOOLEAN},
    {"strictSnp", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort("pbToAlleleCounts -- reformat Seattle SNP data from prettybase format to bed with alleles and counts\n"
	 "Usage:   pbToAlleleCounts prettybaseFile bedFile\n"
	 "Options: strictSnp - ignore loci with non-SNP mutations\n"
	 "         strictBiallelic - ignore loci with more than two alleles \n");
}

struct alleleInfo
/* */
{
    struct alleleInfo *next;
    char  *allele;
    int    count;
};

struct locus
/*  */
{
    struct locus *next;
    char   *chrom;
    int     chromStart;
    int     chromEnd;
    char   *name; /* unique locus id */
    int     sampleSize;
    int     alleleCount;
    boolean strictSnp;
    struct  alleleInfo *alleles;
};

void convertToUppercase(char *ptr)
/* convert and entire string to uppercase */
{
for( ; *ptr !='\0'; ptr++)
    *ptr=toupper(*ptr);
}

void printAlleles(FILE *f, struct alleleInfo *ai)
/* print alleles and counts for this locus */
{
struct alleleInfo *i = NULL;

for (i=ai; i!=NULL; i=i->next)
    fprintf(f,"\t%s\t%d", i->allele, i->count);
fprintf(f,"\n");
}

void printLocus(FILE *f, struct locus *l)
/* print positional information for this locus */
{
if (strictSnp&&!l->strictSnp)
    return;
if (strictBiallelic&&l->alleleCount!=2)
    return;
fprintf(f,"%s\t%d\t%d\t%s\t0\t+\t%d\t%d", l->chrom, l->chromStart, l->chromEnd, l->name, l->sampleSize, l->alleleCount);
printAlleles(f, l->alleles);
}

void printLoci(char *output, struct locus *head)
/* print all information for this locus */
{
FILE  *f        = mustOpen(output, "w");
struct locus *l = NULL;

for (l=head; l!=NULL; l=l->next)
    printLocus(f, l);
}

struct locus *readSs(char *input)
/* determine which allele matches assembly and store in details file */
{
struct locus     *l  = NULL;
struct lineFile  *lf1 = lineFileOpen(input, TRUE), *lf2; /* input file */
char             *row[4], *row2[3]; /* number of fields in input file */
char  *pbName;
char   chrom[32];
int    chromStart;
int    chromEnd;
char   name[32];
char  *allele;

while (lineFileRow(lf1, row)) /* process one snp at a time */
    {
    struct alleleInfo *ai1 = NULL, *ai2 = NULL, *aiPtr;
    struct locus *m        = NULL;

    pbName = replaceChars(row[0], "-", "\t");
    lf2 = lineFileOnString("pbName", TRUE, pbName);
    lineFileRow(lf2, row2);
    chromEnd       = atoi(row2[0]);
    chromStart     = chromEnd-1;
    safef(chrom, sizeof(chrom), "chr%s", row2[2]);

    if(l==NULL||l->chrom==NULL||l->chromStart!=chromStart||!(sameString(l->chrom,chrom)))
	{
	AllocVar(m);
	safef(name, sizeof(name), "ssnp%d", ++ssnpId);
	m->chrom       = cloneString(chrom);
	m->chromStart  = chromStart;
	m->chromEnd    = chromEnd;
	m->name        = cloneString(name);
	m->strictSnp   = TRUE;
	slAddHead(&l, m);
	}

    convertToUppercase(row[2]);
    allele=cloneString(row[2]);
    if ( differentString(allele,"A") && 
	 differentString(allele,"C") && 
	 differentString(allele,"G") && 
	 differentString(allele,"T") )
	l->strictSnp = FALSE;
    for (aiPtr=l->alleles; aiPtr!=NULL; aiPtr=aiPtr->next)
	if (sameString(aiPtr->allele, allele))
	    break;
    if (aiPtr==NULL)
	{
	AllocVar(ai1);
	ai1->allele=cloneString(allele);
	slAddHead(&(l->alleles), ai1);
	l->alleleCount++;
	}
    l->sampleSize++;
    l->alleles->count++;

    convertToUppercase(row[3]);
    allele=cloneString(row[3]);
    if ( differentString(allele,"A") && 
	 differentString(allele,"C") && 
	 differentString(allele,"G") && 
	 differentString(allele,"T") )
	l->strictSnp = FALSE;
    for (aiPtr=l->alleles; aiPtr!=NULL; aiPtr=aiPtr->next)
	if (sameString(aiPtr->allele, allele))
	    break;
    if (aiPtr==NULL)
	{
	AllocVar(ai2);
	ai2->allele=cloneString(allele);
	slAddHead(&(l->alleles), ai2);
	l->alleleCount++;
	}
    l->sampleSize++;
    l->alleles->count++;
    }
slReverse(&l);
return l;
}

int main(int argc, char *argv[])
/* error check, process command line input, and call getSnpDetails */
{
struct locus *l = NULL;

optionInit(&argc, argv, optionSpecs);
strictSnp = optionExists("strictSnp");
strictBiallelic = optionExists("strictBiallelic");
if (argc != 3)
    usage();
l = readSs(argv[1]);
printLoci(argv[2], l);
return 0;
}
