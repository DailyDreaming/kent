/* ldHgGene - load a set of gene predictions from GFF  or GTF file into
 * mySQL database. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "linefile.h"
#include "gff.h"
#include "jksql.h"
#include "genePred.h"

static char const rcsid[] = "$Id: ldHgGene.c,v 1.14 2004/01/15 18:58:36 markd Exp $";

char *exonType = "exon";	/* Type field that signifies exons. */
boolean requireCDS = FALSE;     /* should genes with CDS be dropped */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"exon", OPTION_STRING},
    {"oldTable", OPTION_BOOLEAN},
    {"noncoding", OPTION_BOOLEAN},
    {"nonCoding", OPTION_BOOLEAN},
    {"gtf", OPTION_BOOLEAN},
    {"predTab", OPTION_BOOLEAN},
    {"requireCDS", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
{
errAbort(
    "ldHgGene - load database with gene predictions from a gff file.\n"
    "usage:\n"
    "     ldHgGene database table file(s).gff\n"
    "options:\n"
    "     -exon=type   Sets type field for exons to specific value\n"
    "     -oldTable    Don't overwrite what's already in table\n"
    "     -noncoding   Forces whole prediction to be UTR\n"
    "     -gtf         input is GTF, stop codon is not in CDS\n"
    "     -predTab     input is already in genePredTab format (one file only)\n"
    "     -requireCDS  discard genes that don't have CDS annotation\n");
}

char *createString = 
"CREATE TABLE %s ( \n"
"   name varchar(255) not null,	# Name of gene \n"
"   chrom varchar(255) not null,	# Chromosome name \n"
"   strand char(1) not null,	# + or - for strand \n"
"   txStart int unsigned not null,	# Transcription start position \n"
"   txEnd int unsigned not null,	# Transcription end position \n"
"   cdsStart int unsigned not null,	# Coding region start \n"
"   cdsEnd int unsigned not null,	# Coding region end \n"
"   exonCount int unsigned not null,	# Number of exons \n"
"   exonStarts longblob not null,	# Exon start positions \n"
"   exonEnds longblob not null,	# Exon end positions \n"
          "   #Indices \n"
"   INDEX(name(16)), \n"
"   INDEX(chrom(8),txStart), \n"
"   INDEX(chrom(8),txEnd) \n"
")";


void loadIntoDatabase(char *database, char *table, char *tabName)
/* Load tabbed file into database table. Drop and create table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *ds = newDyString(2048);

if (!optionExists("oldTable"))
    {
    dyStringPrintf(ds, createString, table);
    sqlMaybeMakeTable(conn, table, ds->string);
    dyStringClear(ds);
    dyStringPrintf(ds, 
       "delete from %s", table);
    sqlUpdate(conn, ds->string);
    dyStringClear(ds);
    }
dyStringPrintf(ds, 
   "LOAD data local infile '%s' into table %s", tabName, table);
sqlUpdate(conn, ds->string);
sqlDisconnect(&conn);
freeDyString(&ds);
}

char *convertSoftberryName(char *name)
/* Convert softberry name to simple form that is same as in
 * softberryPep table. */
{
static char *head = "gene_id S.";
char *s = strrchr(name, '.');

if (strstr(name, head) == NULL)
    errAbort("Unrecognized Softberry name %s, no %s", name, head);
return s+1;
}

void ldHgGene(char *database, char *table, int gtfCount, char *gtfNames[])
/* Load up database from a bunch of GTF files. */
{
struct gffFile *gff = gffFileNew("");
struct gffGroup *group;
int i;
char *fileName;
int lineCount;
struct genePred *gpList = NULL, *gp;
char *tabName = "genePred.tab";
FILE *f;
boolean nonCoding = optionExists("noncoding") || optionExists("nonCoding");
boolean isGtf = optionExists("gtf");

boolean isSoftberry = sameWord("softberryGene", table);
boolean isEnsembl = sameWord("ensGene", table);
boolean isSanger22 = sameWord("sanger22", table);

for (i=0; i<gtfCount; ++i)
    {
    fileName = gtfNames[i];
    printf("Reading %s\n", fileName);
    gffFileAdd(gff, fileName, 0);
    }
lineCount = slCount(gff->lineList);
printf("Read %d transcripts in %d lines in %d files\n", 
	slCount(gff->groupList), lineCount, gtfCount);
gffGroupLines(gff);
printf("  %d groups %d seqs %d sources %d feature types\n",
    slCount(gff->groupList), slCount(gff->seqList), slCount(gff->sourceList),
    slCount(gff->featureList));

/* Convert from gffGroup to genePred representation. */
for (group = gff->groupList; group != NULL; group = group->next)
    {
    char *name = group->name;
    if (isSoftberry)
        {
	name = convertSoftberryName(name);
	}
    if (isGtf)
        gp = genePredFromGroupedGtf(gff, group, name);
    else
        gp = genePredFromGroupedGff(gff, group, name, exonType);
    if (gp != NULL)
	{
	if (nonCoding)
	    gp->cdsStart = gp->cdsEnd = 0;
        if (requireCDS && (gp->cdsStart == gp->cdsEnd))
            genePredFree(&gp);
        else
            slAddHead(&gpList, gp);
	}
    }
printf("%d gene predictions\n", slCount(gpList));
slSort(&gpList, genePredCmp);

/* Create tab-delimited file. */
f = mustOpen(tabName, "w");
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    genePredTabOut(gp, f);
    }
fclose(f);
loadIntoDatabase(database, table, tabName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
if (optionExists("exon") && optionExists("gtf"))
    errAbort("can't specify -exon= with -gtf");
exonType = optionVal("exon", exonType);
requireCDS = optionExists("requireCDS");
if (optionExists("predTab"))
    loadIntoDatabase(argv[1], argv[2], argv[3]);
else
    ldHgGene(argv[1], argv[2], argc-3, argv+3);
return 0;
}
