/* b2bb - Convert bed to bigBed.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "asParse.h"
#include "basicBed.h"
#include "sig.h"
#include "rangeTree.h"
#include "sqlNum.h"
#include "bigBed.h"

static char const rcsid[] = "$Id: b2bb.c,v 1.6 2009/08/14 21:18:04 kent Exp $";

int blockSize = 1024;
int itemsPerSlot = 256;
int bedFields = 0;
char *as = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "b2bb - Convert bed file to bigBed.\n"
  "usage:\n"
  "   b2bb in.bed chrom.sizes out.bb\n"
  "Where in.bed is in one of the ascii bed formats, but not including track lines\n"
  "and chrom.sizes is two column: <chromosome name> <size in bases>\n"
  "and out.bb is the output indexed big bed file.\n"
  "The in.bed file must be sorted by chromosome,start.\n"
  "\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  "   -bedFields=N - Number of fields that fit standard bed definition.  If undefined\n"
  "                  assumes all fields in bed are defined.\n"
  "   -as=fields.as - If have non-standard fields, it's great to put a definition of\n"
  "                   each field in a row in AutoSql format here.\n"
  "   -clip - If set just issue warning messages rather than dying if wig\n"
  "                  file contains items off end of chromosome."
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"bedFields", OPTION_INT},
   {"as", OPTION_STRING},
   {"clip", OPTION_BOOLEAN},
   {NULL, 0},
};

void writeBlocks(struct bbiChromUsage *usageList, struct lineFile *lf, struct asObject *as, 
	bits16 definedFieldCount, int itemsPerSlot, struct bbiBoundsArray *bounds, 
	int sectionCount, FILE *f, int resTryCount, int resScales[], int resSizes[],
	bits16 *retFieldCount, bits16 *retDefinedFieldCount)
/* Read through lf, writing it in f.  Save starting points of blocks (every itemsPerSlot)
 * to boundsArray */
{
struct bbiChromUsage *usage = usageList;
char *line, **row = NULL;
int fieldCount = 0, fieldAlloc=0, lastField = 0;
int itemIx = 0, sectionIx = 0;
bits64 blockOffset = 0;
int startPos = 0, endPos = 0;
bits32 chromId = 0;
boolean allocedAs = FALSE;

/* Will keep track of some things that help us determine how much to reduce. */
bits32 resEnds[resTryCount];
int resTry;
for (resTry = 0; resTry < resTryCount; ++resTry)
    resEnds[resTry] = 0;
boolean atEnd = FALSE, sameChrom = FALSE;
bits32 start = 0, end = 0;
char *chrom = NULL;

for (;;)
    {
    /* Get next line of input if any. */
    if (lineFileNextReal(lf, &line))
	{
	/* First time through figure out the field count, and if not set, the defined field count. */
	if (fieldCount == 0)
	    {
	    if (as == NULL)
		{
		fieldCount = chopByWhite(line, NULL, 0);
		if (definedFieldCount == 0)
		    definedFieldCount = fieldCount;
		char *asText = bedAsDef(definedFieldCount, fieldCount);
		as = asParseText(asText);
		allocedAs = TRUE;
		freeMem(asText);
		}
	    else
		{
		fieldCount = slCount(as->columnList);
		}
	    fieldAlloc = fieldCount + 1;
	    lastField = fieldCount - 1;
	    AllocArray(row, fieldAlloc);
	    *retFieldCount = fieldCount;
	    *retDefinedFieldCount = definedFieldCount;
	    }

	/* Chop up line and make sure the word count is right. */
	int wordCount = chopByWhite(line, row, fieldAlloc);
	lineFileExpectWords(lf, fieldCount, wordCount);

	/* Parse out first three fields. */
	chrom = row[0];
	start = lineFileNeedNum(lf, row, 1);
	end = lineFileNeedNum(lf, row, 2);

	/* Check remaining fields are formatted right. */
	if (fieldCount > 3)
	    {
	    /* Go through and check that numerical strings really are numerical. */
	    struct asColumn *asCol = slElementFromIx(as->columnList, 3);
	    int i;
	    for (i=3; i<fieldCount; ++i)
		{
		enum asTypes type = asCol->lowType->type;
		if (! (asCol->isList || asCol->isArray))
		    {
		    if (asTypesIsInt(type))
			lineFileNeedFullNum(lf, row, i);
		    else if (asTypesIsFloating(type))
			lineFileNeedDouble(lf, row, i);
		    }
		asCol = asCol->next;
		}
	    }

	sameChrom = sameString(chrom, usage->name);
	}
    else  /* No next line */
	{
	atEnd = TRUE;
	}


    /* Check conditions that would end block and save block info and advance to next if need be. */
    if (atEnd || !sameChrom || itemIx >= itemsPerSlot)
        {
	/* Save info on existing block. */
	struct bbiBoundsArray *b = &bounds[sectionIx];
	b->offset = blockOffset;
	b->range.chromIx = chromId;
	b->range.start = startPos;
	b->range.end = endPos;
	++sectionIx;
	itemIx = 0;

	if (atEnd)
	    break;
	}

    /* Advance to next chromosome if need be and get chromosome id. */
    if (!sameChrom)
        {
	usage = usage->next;
	assert(usage != NULL);
	assert(sameString(chrom, usage->name));
	for (resTry = 0; resTry < resTryCount; ++resTry)
	    resEnds[resTry] = 0;
	}
    chromId = usage->id;

    /* At start of block we save a lot of info. */
    if (itemIx == 0)
        {
	blockOffset = ftell(f);
	startPos = start;
	endPos = end;
	}
    /* Otherwise just update end. */
        {
	if (endPos < end)
	    endPos = end;
	/* No need to update startPos since list is sorted. */
	}

    /* Write out data. */
    writeOne(f, chromId);
    writeOne(f, start);
    writeOne(f, end);
    if (fieldCount > 3)
        {
	int i;
	/* Write 3rd through next to last field and a tab separator. */
	for (i=3; i<lastField; ++i)
	    {
	    char *s = row[i];
	    int len = strlen(s);
	    mustWrite(f, s, len);
	    fputc('\t', f);
	    }
	/* Write last field and terminal zero */
	char *s = row[lastField];
	int len = strlen(s);
	mustWrite(f, s, len);
	}
    fputc(0, f);

    itemIx += 1;

    /* Do zoom counting. */
    for (resTry = 0; resTry < resTryCount; ++resTry)
        {
	bits32 resEnd = resEnds[resTry];
	if (start >= resEnd)
	    {
	    resSizes[resTry] += 1;
	    resEnds[resTry] = resEnd = start + resScales[resTry];
	    }
	while (end > resEnd)
	    {
	    resSizes[resTry] += 1;
	    resEnds[resTry] = resEnd = resEnd + resScales[resTry];
	    }
	}
    }
assert(sectionIx == sectionCount);
freez(&row);
if (allocedAs)
    asObjectFreeList(&as);
}

struct rbTree *rangeTreeForBedChrom(struct lineFile *lf, char *chrom)
/* Read lines from bed file as long as they match chrom.  Return a rangeTree that
 * corresponds to the coverage. */
{
struct rbTree *tree = rangeTreeNew();
char *line;
while (lineFileNextReal(lf, &line))
    {
    if (!startsWithWord(chrom, line))
        {
	lineFileReuse(lf);
	break;
	}
    char *row[3];
    chopLine(line, row);
    unsigned start = sqlUnsigned(row[1]);
    unsigned end = sqlUnsigned(row[2]);
    rangeTreeAddToCoverageDepth(tree, start, end);
    }
return tree;
}

struct bbiSummary *writeReducedOnceReturnReducedTwice(struct bbiChromUsage *usageList, 
	int fieldCount, struct lineFile *lf, int initialReduction, int initialReductionCount, 
	int zoomIncrement, int blockSize, int itemsPerSlot, 
	struct lm *lm, FILE *f, bits64 *retDataStart, bits64 *retIndexStart)
/* Write out data reduced by factor of initialReduction.  Also calculate and keep in memory
 * next reduction level.  This is more work than some ways, but it keeps us from having to
 * keep the first reduction entirely in memory. */
{
struct bbiSummary *twiceReducedList = NULL;
bits32 doubleReductionSize = initialReduction * zoomIncrement;
struct bbiChromUsage *usage = usageList;
struct bbiBoundsArray *boundsArray, *boundsPt, *boundsEnd;
boundsPt = AllocArray(boundsArray, initialReductionCount);
boundsEnd = boundsPt + initialReductionCount;

*retDataStart = ftell(f);

/* This gets a little complicated I'm afraid.  The strategy is to:
 *   1) Build up a range tree that represents coverage depth on that chromosome
 *      This also has the nice side effect of getting rid of overlaps.
 *   2) Stream through the range tree, outputting the initial summary level and
 *      further reducing. 
 */
for (usage = usageList; usage != NULL; usage = usage->next)
    {
    struct bbiSummary oneSummary, *sum = NULL;
    struct rbTree *rangeTree = rangeTreeForBedChrom(lf, usage->name);
    struct range *range, *rangeList = rangeTreeList(rangeTree);
    for (range = rangeList; range != NULL; range = range->next)
        {
	double val = ptToInt(range->val);
	int start = range->start;
	int end = range->end;
	/* If start past existing block then output it. */
	if (sum != NULL && sum->end <= start)
	    {
	    bbiOutputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize, 
		&boundsPt, boundsEnd, usage->size, lm, f);
	    sum = NULL;
	    }
	/* If don't have a summary we're working on now, make one. */
	if (sum == NULL)
	    {
	    oneSummary.chromId = usage->id;
	    oneSummary.start = start;
	    oneSummary.end = start + initialReduction;
	    if (oneSummary.end > usage->size) oneSummary.end = usage->size;
	    oneSummary.minVal = oneSummary.maxVal = val;
	    oneSummary.sumData = oneSummary.sumSquares = 0.0;
	    oneSummary.validCount = 0;
	    sum = &oneSummary;
	    }
	/* Deal with case where might have to split an item between multiple summaries.  This
	 * loop handles all but the final affected summary in that case. */
	while (end > sum->end)
	    {
	    verbose(3, "Splitting size %d at %d\n", end - start, end);
	    /* Fold in bits that overlap with existing summary and output. */
	    bits32 overlap = rangeIntersection(start, end, sum->start, sum->end);
	    sum->validCount += overlap;
	    if (sum->minVal > val) sum->minVal = val;
	    if (sum->maxVal < val) sum->maxVal = val;
	    sum->sumData += val * overlap;
	    sum->sumSquares += val * overlap;
	    bbiOutputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize, 
		    &boundsPt, boundsEnd, usage->size, lm, f);

	    /* Move summary to next part. */
	    sum->start = start = sum->end;
	    sum->end = start + initialReduction;
	    if (sum->end > usage->size) sum->end = usage->size;
	    sum->minVal = sum->maxVal = val;
	    sum->sumData = sum->sumSquares = 0.0;
	    sum->validCount = 0;
	    }

	/* Add to summary. */
	bits32 size = end - start;
	sum->validCount += size;
	if (sum->minVal > val) sum->minVal = val;
	if (sum->maxVal < val) sum->maxVal = val;
	sum->sumData += val * size;
	sum->sumSquares += val * size;
	}
    if (sum != NULL)
	{
	bbiOutputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize, 
	    &boundsPt, boundsEnd, usage->size, lm, f);
	}
    rangeTreeFree(&rangeTree);
    }

/* Write out 1st zoom index. */
int indexOffset = *retIndexStart = ftell(f);
assert(boundsPt == boundsEnd);
cirTreeFileBulkIndexToOpenFile(boundsArray, sizeof(boundsArray[0]), initialReductionCount,
    blockSize, itemsPerSlot, NULL, bbiBoundsArrayFetchKey, bbiBoundsArrayFetchOffset, 
    indexOffset, f);

freez(&boundsArray);
slReverse(&twiceReducedList);
return twiceReducedList;
}


void bbFileCreate(
	char *inName, 	  /* Input file in a tabular bed format <chrom><start><end> + whatever. */
	char *chromSizes, /* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,	  /* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot, /* Number of items in lowest level of tree.  64 is good. */
	bits16 definedFieldCount,  /* Number of defined bed fields - 3-16 or so.  0 means all fields
				    * are the defined bed ones. */
	char *asFileName, /* If non-null points to a .as file that describes fields. */
	boolean clip,     /* If set silently clip out of bound coordinates. */
	char *outName)    /* BigBed output file name. */
/* Convert tab-separated bed file to binary indexed, zoomed bigBed version. */
{
/* Set up timing measures. */
verboseTime(1, NULL);
struct lineFile *lf = lineFileOpen(inName, TRUE);

/* Load up as object if defined in file. */
struct asObject *as = NULL;
if (asFileName != NULL)
    {
    /* Parse it and do sanity check. */
    as = asParseFile(asFileName);
    if (as->next != NULL)
        errAbort("Can only handle .as files containing a single object.");
    }

/* Load in chromosome sizes. */
struct hash *chromSizesHash = bbiChromSizesFromFile(chromSizes);
verbose(2, "Read %d chromosomes and sizes from %s\n",  chromSizesHash->elCount, chromSizes);

/* Do first pass, mostly just scanning file and counting hits per chromosome. */
int minDiff = 0;
double aveSpan = 0;
struct bbiChromUsage *usageList = bbiChromUsageFromBedFile(lf, chromSizesHash, &minDiff, &aveSpan);
verboseTime(1, "pass1 - making usageList");
verbose(2, "%d chroms in %s. Average span of beds %f\n", slCount(usageList), inName, aveSpan);

/* Open output file and write dummy header. */
FILE *f = mustOpen(outName, "wb");
bbiWriteDummyHeader(f);
bbiWriteDummyZooms(f);

/* Write out asFile if any */
bits64 asOffset = 0;
if (asFileName != NULL)
    {
    int colCount = slCount(as->columnList);
    asOffset = ftell(f);
    FILE *asFile = mustOpen(asFileName, "r");
    copyOpenFile(asFile, f);
    fputc(0, f);
    carefulClose(&asFile);
    verbose(2, "%s has %d columns\n", asFileName, colCount);
    }

/* Write out chromosome/size database. */
bits64 chromTreeOffset = ftell(f);
bbiWriteChromInfo(usageList, blockSize, f);

/* Set up to keep track of possible initial reduction levels. */
int resTryCount = 10, resTry;
int resIncrement = 4;
int resScales[resTryCount], resSizes[resTryCount];
int minZoom = 10;
int res = aveSpan;
if (res < minZoom)
    res = minZoom;
for (resTry = 0; resTry < resTryCount; ++resTry)
    {
    resSizes[resTry] = 0;
    resScales[resTry] = res;
    res *= resIncrement;
    }

/* Write out primary full resolution data in sections, collect stats to use for reductions. */
bits64 dataOffset = ftell(f);
bits32 blockCount = bbiCountSectionsNeeded(usageList, itemsPerSlot);
struct bbiBoundsArray *boundsArray;
AllocArray(boundsArray, blockCount);
lineFileRewind(lf);
bits16 fieldCount=0;
writeBlocks(usageList, lf, as, definedFieldCount, itemsPerSlot, boundsArray, blockCount, f,
	resTryCount, resScales, resSizes, &fieldCount, &definedFieldCount);
verboseTime(1, "pass2 - checking and writing primary data");

/* Write out primary data index. */
bits64 indexOffset = ftell(f);
cirTreeFileBulkIndexToOpenFile(boundsArray, sizeof(boundsArray[0]), blockCount,
    blockSize, 1, NULL, bbiBoundsArrayFetchKey, bbiBoundsArrayFetchOffset, 
    indexOffset, f);
freez(&boundsArray);
verboseTime(1, "index write");

/* Declare arrays and vars that track the zoom levels we actually output. */
bits32 zoomAmounts[bbiMaxZoomLevels];
bits64 zoomDataOffsets[bbiMaxZoomLevels];
bits64 zoomIndexOffsets[bbiMaxZoomLevels];
int zoomLevels = 0;

/* Write out first zoomed section while storing in memory next zoom level. */
if (aveSpan > 0)
    {
    bits64 dataSize = indexOffset - dataOffset;
    int maxReducedSize = dataSize/2;
    int initialReduction = 0, initialReducedCount = 0;

    /* Figure out initialReduction for zoom. */
    for (resTry = 0; resTry < resTryCount; ++resTry)
	{
	bits64 reducedSize = resSizes[resTry] * sizeof(struct bbiSummaryOnDisk);
	if (reducedSize <= maxReducedSize)
	    {
	    initialReduction = resScales[resTry];
	    initialReducedCount = resSizes[resTry];
	    break;
	    }
	}
    verbose(2, "initialReduction %d, initialReducedCount = %d\n", 
    	initialReduction, initialReducedCount);
    verbose(2, "dataSize %lld, reducedSize %lld, resScales[0] = %d\n", dataSize, (bits64)initialReducedCount*sizeof(struct bbiSummaryOnDisk), resScales[0]);

    if (initialReduction > 0)
        {
	struct lm *lm = lmInit(0);
	int zoomIncrement = 4;
	lineFileRewind(lf);
	struct bbiSummary *rezoomedList = writeReducedOnceReturnReducedTwice(usageList, 
		fieldCount, lf, initialReduction, initialReducedCount,
		resIncrement, blockSize, itemsPerSlot, lm, 
		f, &zoomDataOffsets[0], &zoomIndexOffsets[0]);
	verboseTime(1, "pass3 - writeReducedOnceReturnReducedTwice");
	zoomAmounts[0] = initialReduction;
	zoomLevels = 1;

	int zoomCount = initialReducedCount;
	int reduction = initialReduction * zoomIncrement;
	while (zoomLevels < bbiMaxZoomLevels)
	    {
	    int rezoomCount = slCount(rezoomedList);
	    if (rezoomCount >= zoomCount)
	        break;
	    zoomCount = rezoomCount;
	    zoomDataOffsets[zoomLevels] = ftell(f);
	    zoomIndexOffsets[zoomLevels] = bbiWriteSummaryAndIndex(rezoomedList, 
	    	blockSize, itemsPerSlot, f);
	    zoomAmounts[zoomLevels] = reduction;
	    ++zoomLevels;
	    reduction *= zoomIncrement;
	    rezoomedList = bbiSummarySimpleReduce(rezoomedList, reduction, lm);
	    }
	lmCleanup(&lm);
	verboseTime(1, "further reductions");
	}
    }
/* Go back and rewrite header. */
rewind(f);
bits32 sig = bigBedSig;
bits16 version = 1;
bits16 summaryCount = zoomLevels;
bits32 reserved32 = 0;
bits32 reserved64 = 0;

/* Write fixed header */
writeOne(f, sig);
writeOne(f, version);
writeOne(f, summaryCount);
writeOne(f, chromTreeOffset);
writeOne(f, dataOffset);
writeOne(f, indexOffset);
writeOne(f, fieldCount);
writeOne(f, definedFieldCount);
writeOne(f, asOffset);
int i;
for (i=0; i<5; ++i)
    writeOne(f, reserved32);

/* Write summary headers with data. */
verbose(2, "Writing %d levels of zoom\n", zoomLevels);
for (i=0; i<zoomLevels; ++i)
    {
    verbose(3, "zoomAmounts[%d] = %d\n", i, (int)zoomAmounts[i]);
    writeOne(f, zoomAmounts[i]);
    writeOne(f, reserved32);
    writeOne(f, zoomDataOffsets[i]);
    writeOne(f, zoomIndexOffsets[i]);
    }
/* Write rest of summary headers with no data. */
for (i=zoomLevels; i<bbiMaxZoomLevels; ++i)
    {
    writeOne(f, reserved32);
    writeOne(f, reserved32);
    writeOne(f, reserved64);
    writeOne(f, reserved64);
    }


/* Clean up. */
lineFileClose(&lf);
carefulClose(&f);
freeHash(&chromSizesHash);
bbiChromUsageFreeList(&usageList);
asObjectFreeList(&as);
}

void b2bb(char *inName, char *chromSizes, char *outName)
/* b2bb - Convert bed file to bigBed.. */
{
bbFileCreate(inName, chromSizes, blockSize, itemsPerSlot, bedFields, as, 
	optionExists("clip"), outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
bedFields = optionInt("bedFields", bedFields);
as = optionVal("as", as);
if (argc != 4)
    usage();
b2bb(argv[1], argv[2], argv[3]);
optionFree();
return 0;
}
