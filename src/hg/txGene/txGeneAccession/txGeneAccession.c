/* txGeneAccession - Assign permanent accession number to genes.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "binRange.h"
#include "rangeTree.h"
#include "sqlNum.h"
#include "minChromSize.h"

static char const rcsid[] = "$Id: txGeneAccession.c,v 1.11 2007/03/17 05:37:28 kent Exp $";

void idToAcc(int id, char acc[16])
/* Convert ID to accession. */
{
if (id >= 17576000)
    errAbort("Out of accessions!");
acc[8] = 0;
acc[7] = id%26 + 'a';
id /= 26;
acc[6] = id%26 + 'a';
id /= 26;
acc[5] = id%26 + 'a';
id /= 26;
acc[4] = id%10 + '0';
id /= 10;
acc[3] = id%10 + '0';
id /= 10;
acc[2] = id%10 + '0';
acc[1] = 'c';
acc[0] = 'u';
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneAccession - Assign permanent accession number to genes.\n"
  "usage:\n"
  "   txGeneAccession old.bed txLastId new.bed txToAcc.tab oldToNew.tab\n"
  "where:\n"
  "   old.bed is a bed file with the previous build with accessions for names\n"
  "   txLastId is a file with the current highest txId\n"
  "   new.bed is a bed file with the current build with temp IDs for names\n"
  "   txToAcc.tab is the output mapping temp IDs to accessions\n"
  "   oldToNew.tab gives information about the relationship between the accessions\n"
  "                in old.bed and the new accessions\n"
  "Use old.bed from previous build for new builds on same assembly.  For new\n"
  "assemblies first liftOver old.bed, then use this.\n"
  "options:\n"
  "   -test - Don't update txLastId file, so testing is reproducible\n"
  );
}

static struct optionSpec options[] = {
   {"test", OPTION_BOOLEAN},
   {NULL, 0},
};

int readNumberFromFile(char *fileName)
/* Read exactly one number from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
if (!lineFileRow(lf, row))
    errAbort("%s is empty", fileName);
int number = lineFileNeedNum(lf, row, 0);
lineFileClose(&lf);
return number;
}


boolean exactMatch(struct bed *oldBed, struct bed *newBed)
/* Return TRUE if it's an exact match. */
{
if (oldBed->blockCount != newBed->blockCount)
    return FALSE;
int oldSize = bedTotalBlockSize(oldBed);
int newSize = bedTotalBlockSize(newBed);
int overlap = bedSameStrandOverlap(oldBed, newBed);
return  (oldSize == newSize && oldSize == overlap);
}

boolean compatibleExtension(struct bed *oldBed, struct bed *newBed)
/* Return TRUE if newBed is a compatible extension of oldBed, meaning
 * all internal exons and all introns of old bed are contained, in the 
 * same order in the new bed. */
{
/* New bed must have at least as many exons as old bed... */
if (oldBed->blockCount > newBed->blockCount)
    return 0;

/* Look for an exact match */
int oldSize = bedTotalBlockSize(oldBed);
int newSize = bedTotalBlockSize(newBed);
int overlap = bedSameStrandOverlap(oldBed, newBed);
if (oldSize == newSize && oldSize == overlap)
    return TRUE;

/* Next handle case where old bed is a single exon.  For this
 * just require that old bed is a nearly proper superset of new bed. */
if (oldBed->blockCount == 0)
    return overlap >= 0.95*oldSize;

/* Otherwise we look for first intron start in old bed, and then
 * flip through new bed until we find an intron that starts at the
 * same place. */
int oldFirstIntronStart = oldBed->chromStart + oldBed->chromStarts[0] + oldBed->blockSizes[0];
int newLastBlock = newBed->blockCount-1, oldLastBlock = oldBed->blockCount-1;
int newIx, oldIx;
for (newIx=0; newIx < newLastBlock; ++newIx)
    {
    int iStartNew = newBed->chromStart + newBed->chromStarts[newIx] + newBed->blockSizes[newIx];
    if (iStartNew == oldFirstIntronStart)
        break;
    }
if (newIx == newLastBlock)
    return FALSE;

/* Now we go through all introns in old bed, and make sure they match. */
for (oldIx=0; oldIx < oldLastBlock; ++oldIx, ++newIx)
    {
    int iStartOld = oldBed->chromStart + oldBed->chromStarts[oldIx] + oldBed->blockSizes[oldIx];
    int iEndOld = oldBed->chromStart + oldBed->chromStarts[oldIx+1];
    int iStartNew = newBed->chromStart + newBed->chromStarts[newIx] + newBed->blockSizes[newIx];
    int iEndNew = newBed->chromStart + newBed->chromStarts[newIx+1];
    if (iStartOld != iStartNew || iEndOld != iEndNew)
        return FALSE;
    }
return TRUE;
}

struct bed *findExact(struct bed *newBed, struct hash *oldHash, struct hash *usedHash)
/* Try and find an old bed identical with new bed. */
{
struct binKeeper *bk = hashFindVal(oldHash, newBed->chrom);
if (bk == NULL)
    return NULL;
struct bed *matchingBed = NULL;
struct binElement *bin, *binList = binKeeperFind(bk, newBed->chromStart, newBed->chromEnd);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct bed *oldBed = bin->val;
    if (oldBed->strand[0] == newBed->strand[0])
        {
	if (!hashLookup(usedHash, oldBed->name))
	    {
	    if (exactMatch(oldBed, newBed))
		{
		matchingBed = oldBed;
		break;
		}
	    }
	}
    }
slFreeList(&binList);
return matchingBed;
}

struct bed *findCompatible(struct bed *newBed, struct hash *oldHash, struct hash *usedHash)
/* Try and find an old bed compatible with new bed. */
{
struct bed *matchingBed = NULL;
struct binKeeper *bk = hashFindVal(oldHash, newBed->chrom);
if (bk == NULL)
    return NULL;
struct binElement *bin, *binList = binKeeperFind(bk, newBed->chromStart, newBed->chromEnd);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct bed *oldBed = bin->val;
    if (oldBed->strand[0] == newBed->strand[0])
	{
	if (!hashLookup(usedHash, oldBed->name))
	    {
	    if (compatibleExtension(oldBed, newBed))
		{
		matchingBed = oldBed;
		break;
		}
	    }
	}
    }
slFreeList(&binList);
return matchingBed;
}

struct bed *findMostOverlapping(struct bed *bed, struct hash *keeperHash)
/* Try find most overlapping thing to bed in keeper hash. */
{
struct bed *bestBed = NULL;
int bestOverlap = 0;
struct binKeeper *bk = hashFindVal(keeperHash, bed->chrom);
if (bk == NULL)
    return NULL;
struct binElement *bin, *binList = binKeeperFind(bk, bed->chromStart, bed->chromEnd);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct bed *bed2 = bin->val;
    if (bed2->strand[0] == bed->strand[0])
	{
	int overlap = bedSameStrandOverlap(bed2, bed);
	if (overlap > bestOverlap)
	    {
	    bestOverlap = overlap;
	    bestBed = bed2;
	    }
	}
    }
slFreeList(&binList);
return bestBed;
}

void txGeneAccession(char *oldBedFile, char *lastIdFile, char *newBedFile, char *txToAccFile,
	char *oldToNewFile)
/* txGeneAccession - Assign permanent accession number to genes. */
{
/* Read in all input. */
struct bed *oldList = bedLoadNAll(oldBedFile, 12);
verbose(2, "Read %d from %s\n", slCount(oldList), oldBedFile);
struct bed *newList = bedLoadNAll(newBedFile, 12);
verbose(2, "Read %d from %s\n", slCount(newList), newBedFile);
int txId = readNumberFromFile(lastIdFile);
verbose(2, "Last txId used was %d (from %s)\n", txId, lastIdFile);

/* Make a random-access data structure for old list. */
struct hash *oldHash = bedsIntoKeeperHash(oldList);

/* Make a little hash to help prevent us from reusing an
 * old accession twice (which might happen if we extend it
 * in two incompatible ways). */
struct hash *usedHash = hashNew(16);

/* Record our decisions in hash as well as file. */
struct hash *idToAccHash = hashNew(16);

/* Loop through new list first looking for exact matches. Record
 * exact matches in hash so we don't look for them again during
 * the next, "compatable" match phase. */
struct hash *oldExactHash = hashNew(16), *newExactHash = hashNew(16);
struct bed *oldBed, *newBed;
FILE *f = mustOpen(txToAccFile, "w");
FILE *fOld = mustOpen(oldToNewFile, "w");
for (newBed = newList; newBed != NULL; newBed = newBed->next)
    {
    oldBed = findExact(newBed, oldHash, usedHash);
    if (oldBed != NULL)
        {
	hashAdd(oldExactHash, oldBed->name, oldBed);
	hashAdd(newExactHash, newBed->name, newBed);
	hashAdd(usedHash, oldBed->name, NULL);
        fprintf(f, "%s\t%s\n", newBed->name, oldBed->name);
	hashAdd(idToAccHash, newBed->name, oldBed->name);
	fprintf(fOld, "%s:%d-%d\t%s\t%s\t%s\n", oldBed->chrom, oldBed->chromStart, oldBed->chromEnd,
		oldBed->name, oldBed->name, "exact");
	}
    }

/* Loop through new bed looking for compatible things.  If
 * we can't find anything compatable, make up a new accession. */
for (newBed = newList; newBed != NULL; newBed = newBed->next)
    {
    if (!hashLookup(newExactHash, newBed->name))
	{
	oldBed = findCompatible(newBed, oldHash, usedHash);
	if (oldBed == NULL)
	    {
	    char newAcc[16];
	    idToAcc(++txId, newAcc);
	    strcat(newAcc, ".1");
	    fprintf(f, "%s\t%s\n", newBed->name, newAcc);
	    hashAdd(idToAccHash, newBed->name, newAcc);
	    oldBed = findMostOverlapping(newBed, oldHash);
	    char *oldAcc = (oldBed == NULL ? "" : oldBed->name);
	    fprintf(fOld, "%s:%d-%d\t%s\t%s\t%s\n", newBed->chrom, newBed->chromStart, newBed->chromEnd,
	    	oldAcc, newAcc, "new");
	    }
	else
	    {
	    char *acc = cloneString(oldBed->name);
	    char *ver = strchr(oldBed->name, '.');
	    if (ver == NULL)
	        errAbort("No version found in %s", oldBed->name);
	    *ver++ = 0;
	    int version = sqlUnsigned(ver);
	    char newAcc[16];
	    safef(newAcc, sizeof(newAcc), "%s.%d", acc, version+1);
	    hashAdd(usedHash, oldBed->name, NULL);
	    fprintf(f, "%s\t%s\n", newBed->name, newAcc);
	    hashAdd(idToAccHash, newBed->name, newAcc);
	    fprintf(fOld, "%s:%d-%d\t%s\t%s\t%s\n", newBed->chrom, newBed->chromStart, newBed->chromEnd,
	    	oldBed->name, newAcc, "compatible");
	    }
	}
    }
carefulClose(&f);

/* Make a random-access data structure for old list. */
struct hash *newHash = bedsIntoKeeperHash(newList);

/* Write record of ones that don't map. */
for (oldBed = oldList; oldBed != NULL; oldBed = oldBed->next)
    {
    if (!hashLookup(usedHash, oldBed->name))
	{
	char *newAcc = "";
	struct bed *newBed = findMostOverlapping(oldBed, newHash);
	if (newBed != NULL)
	    newAcc = hashMustFindVal(idToAccHash, newBed->name);
	fprintf(fOld, "%s:%d-%d\t%s\t%s\t%s\n", oldBed->chrom, oldBed->chromStart, oldBed->chromEnd,
		oldBed->name, newAcc, "lost");
	}
    }
carefulClose(&fOld);

if (!optionExists("test"))
    {
    FILE *fId = mustOpen(lastIdFile, "w");
    fprintf(fId, "%d\n", txId);
    carefulClose(&fId);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
txGeneAccession(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
