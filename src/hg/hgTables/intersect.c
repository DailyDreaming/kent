/* intersect - handle intersecting beds. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "bits.h"
#include "bed.h"
#include "hdb.h"
#include "featureBits.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: intersect.c,v 1.25 2005/06/07 14:04:09 giardine Exp $";

/* We keep two copies of variables, so that we can
 * cancel out of the page. */

static char *curVars[] = {hgtaIntersectGroup, hgtaIntersectTrack,
	hgtaIntersectOp, hgtaMoreThreshold, hgtaLessThreshold,
	hgtaInvertTable, hgtaInvertTable2,
	};
static char *nextVars[] = {hgtaNextIntersectGroup, hgtaNextIntersectTrack,
	hgtaNextIntersectOp, hgtaNextMoreThreshold, hgtaNextLessThreshold,
	hgtaNextInvertTable, hgtaNextInvertTable2,
	};

boolean anyIntersection()
/* Return TRUE if there's an intersection to do. */
{
return cartVarExists(cart, hgtaIntersectTrack);
}

static char *onChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
jsDropDownCarryOver(dy, hgtaNextIntersectGroup);
jsDropDownCarryOver(dy, hgtaNextIntersectTrack);
jsTrackedVarCarryOver(dy, hgtaNextIntersectOp, "op");
jsTextCarryOver(dy, hgtaNextMoreThreshold);
jsTextCarryOver(dy, hgtaNextLessThreshold);
jsTrackedVarCarryOver(dy, hgtaNextInvertTable, "invertTable");
jsTrackedVarCarryOver(dy, hgtaNextInvertTable2, "invertTable2");
return dy;
}

static char *onChangeEither()
/* Get group-changing javascript. */
{
struct dyString *dy = onChangeStart();
return onChangeEnd(&dy);
}

void makeOpButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
jsMakeTrackingRadioButton(hgtaNextIntersectOp, "op", val, selVal);
}

struct trackDb *showGroupTrackRow(char *groupVar, char *groupScript,
    char *trackVar, char *trackScript, struct sqlConnection *conn)
/* Show group & track row of controls.  Returns selected track */
{
struct trackDb *track;
struct grp *selGroup;
hPrintf("<TR><TD>");
selGroup = showGroupField(groupVar, groupScript, conn, FALSE);
track = showTrackField(selGroup, trackVar, trackScript);
hPrintf("</TD></TR>\n");
return track;
}


void doIntersectMore(struct sqlConnection *conn)
/* Continue working in intersect page. */
{
struct trackDb *iTrack;
char *name = curTableLabel();
char *iName;
char *onChange = onChangeEither();
char *op, *setting;
htmlOpen("Intersect with %s", name);

hPrintf("<FORM ACTION=\"..%s\" NAME=\"mainForm\" METHOD=GET>\n", getScriptName());
cartSaveSession(cart);
hPrintf("<TABLE BORDER=0>\n");
/* Print group and track line. */

hPrintf("Select a group and track to intersect with:\n");
iTrack = showGroupTrackRow(hgtaNextIntersectGroup, onChange, 
	hgtaNextIntersectTrack, onChange, conn);
iName = iTrack->shortLabel;
hPrintf("</TABLE>\n");

if (!isWiggle(database, curTable))
    {
    hPrintf("<BR>\n");
    hPrintf("These combinations will maintain the gene/alignment structure (if any) of %s: <P>\n",
       name);
    }
else
    hPrintf("<P>\n");

op = cartUsualString(cart, hgtaNextIntersectOp, "any");
jsTrackingVar("op", op);
makeOpButton("any", op);
printf("All %s records that have any overlap with %s <BR>\n",
       name, iName);
makeOpButton("none", op);
printf("All %s records that have no overlap with %s <BR>\n",
       name, iName);

if (!isWiggle(database, curTable))
    {
    makeOpButton("more", op);
    printf("All %s records that have at least ",
	   name);
    setting = cartCgiUsualString(cart, hgtaNextMoreThreshold, "80");
    cgiMakeTextVar(hgtaNextMoreThreshold, setting, 3);
    printf(" %% overlap with %s <BR>\n", iName);
    makeOpButton("less", op);
    printf("All %s records that have at most ",
	   name);
    setting = cartCgiUsualString(cart, hgtaNextLessThreshold, "80");
    cgiMakeTextVar(hgtaNextLessThreshold, setting, 3);
    printf(" %% overlap with %s <P>\n", iName);
    }
else
    {
    /*	keep javaScript onClick happy	*/
    hPrintf("<input TYPE=HIDDEN NAME=\"hgta_nextMoreThreshold\" VALUE=80>\n");
    hPrintf("<input TYPE=HIDDEN NAME=\"hgta_nextLessThreshold\" VALUE=80>\n");
    hPrintf(" <P>\n");
    }


if (!isWiggle(database, curTable))
    {
    printf("These combinations will discard the gene/alignment structure (if any) of %s and produce a simple list of position ranges.<P>\n",
       name);
    makeOpButton("and", op);
    printf("Base-pair-wise intersection (AND) of %s and %s <BR>\n",
       name, iName);
    makeOpButton("or", op);
    printf("Base-pair-wise union (OR) of %s and %s <P>\n",
	name, iName);
    puts("Check the following boxes to complement one or both tables. To complement a table means to include a row in the intersection if it \n"
     "is <I>not</I> included in the table. <P>");
    jsMakeTrackingCheckBox(hgtaNextInvertTable, "invertTable", FALSE);
    printf("Complement %s before intersection/union <BR>\n", name);
    jsMakeTrackingCheckBox(hgtaNextInvertTable2, "invertTable2", FALSE);
    printf("Complement %s before intersection/union <P>\n", iName);
    }
else
    {
    /*	keep javaScript onClick happy	*/
    jsTrackingVar("op", op);
    hPrintf("<SCRIPT>\n");
    hPrintf("var invertTable=0;\n");
    hPrintf("var invertTable2=0;\n");
    hPrintf("</SCRIPT>\n");
    hPrintf("(data track %s is not composed of gene records.  Specialized intersection operations are not available.)<P>\n", name);
    }

cgiMakeButton(hgtaDoIntersectSubmit, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[32];
    int varCount = ArraySize(nextVars);
    memcpy(saveVars, nextVars, varCount * sizeof(saveVars[0]));
    saveVars[varCount] = hgtaDoIntersectMore;
    jsCreateHiddenForm(saveVars, varCount+1);
    }

htmlClose();
}

void removeCartVars(struct cart *cart, char **vars, int varCount)
/* Remove array of variables from cart. */
{
int i;
for (i=0; i<varCount; ++i)
    cartRemove(cart, vars[i]);
}

void copyCartVars(struct cart *cart, char **source, char **dest, int count)
/* Copy from source to dest. */
{
int i;
for (i=0; i<count; ++i)
    {
    char *s = cartOptionalString(cart, source[i]);
    if (s != NULL)
        cartSetString(cart, dest[i], s);
    else
        cartRemove(cart, dest[i]);
    }
}


void doIntersectPage(struct sqlConnection *conn)
/* Respond to intersect create/edit button */
{
if (ArraySize(curVars) != ArraySize(nextVars))
    internalErr();
copyCartVars(cart, curVars, nextVars, ArraySize(curVars));
doIntersectMore(conn);
}

void doClearIntersect(struct sqlConnection *conn)
/* Respond to click on clear intersection. */
{
removeCartVars(cart, curVars, ArraySize(curVars));
doMainPage(conn);
}

void doIntersectSubmit(struct sqlConnection *conn)
/* Respond to submit on intersect page. */
{
copyCartVars(cart, nextVars, curVars, ArraySize(curVars));
doMainPage(conn);
}

static int countBasesOverlap(struct bed *bedItem, Bits *bits, boolean hasBlocks)
/* Return the number of bases belonging to bedItem covered by bits. */
{
int count = 0;
int i, j;
if (hasBlocks)
    {
    for (i=0;  i < bedItem->blockCount;  i++)
	{
	int start = bedItem->chromStart + bedItem->chromStarts[i];
	int end   = start + bedItem->blockSizes[i];
	for (j=start;  j < end;  j++)
	    if (bitReadOne(bits, j))
		count++;
	}
    }
else
    {
    for (i=bedItem->chromStart;  i < bedItem->chromEnd;  i++)
	if (bitReadOne(bits, i))
	    count++;
    }
    return(count);
}

static struct bed *bitsToBed4List(Bits *bits, int bitSize, 
	char *chrom, int minSize, int rangeStart, int rangeEnd,
	struct lm *lm)
/* Translate ranges of set bits to bed 4 items. */
{
struct bed *bedList = NULL, *bed;
int start = 0;
int end = 0;
int id = 0;
char name[128];

if (rangeStart < 0)
    rangeStart = 0;
if (rangeEnd > bitSize)
    rangeEnd = bitSize;
end = rangeStart;

/* We depend on extra zero BYTE at end in case bitNot was used on bits. */
for (;;)
    {
    start = bitFindSet(bits, end, rangeEnd);
    if (start >= rangeEnd)
        break;
    end = bitFindClear(bits, start, rangeEnd);
    if (end - start >= minSize)
	{
	lmAllocVar(lm, bed);
	bed->chrom = chrom;
	bed->chromStart = start;
	bed->chromEnd = end;
	snprintf(name, sizeof(name), "%s.%d", chrom, ++id);
	bed->name = lmCloneString(lm, name);
	slAddHead(&bedList, bed);
	}
    }
slReverse(&bedList);
return(bedList);
}




static struct bed *intersectOnRegion(
	struct sqlConnection *conn,	/* Open connection to database. */
	struct region *region, 		/* Region to work inside */
	char *table1,			/* Table input list is from. */
	struct bed *bedList1,	/* List before intersection, should be
	                                 * all within region. */
	struct lm *lm,	   /* Local memory pool. */
	int *retFieldCount)	   /* Field count. */
/* Intersect bed list, consulting CGI vars to figure out
 * with what table and how.  Return intersected result,
 * which is independent from input.  This potentially will
 * chew up bedList1. */
{
/* Grab parameters for intersection from cart. */
int moreThresh = cartCgiUsualInt(cart, hgtaMoreThreshold, 0);
int lessThresh = cartCgiUsualInt(cart, hgtaLessThreshold, 100);
boolean invTable = cartCgiUsualBoolean(cart, hgtaInvertTable, FALSE);
boolean invTable2 = cartCgiUsualBoolean(cart, hgtaInvertTable2, FALSE);
char *op = cartString(cart, hgtaIntersectOp);
char *table2 = cartString(cart, hgtaIntersectTrack);
/* Load up intersecting bedList2 (to intersect with) */
struct hTableInfo *hti2 = getHti(database, table2);
struct trackDb *track2 = findTrack(table2, fullTrackList);
struct lm *lm2 = lmInit(64*1024);
struct bed *bedList2 = getFilteredBeds(conn, track2->tableName, region, lm2, 
	NULL);
/* Set up some other local vars. */
struct hTableInfo *hti1 = getHti(database, table1);
struct featureBits *fbList2 = NULL;
struct bed *bed;
Bits *bits2;
int chromSize = hChromSize(region->chrom);
boolean isBpWise = (sameString("and", op) || sameString("or", op));
struct bed *intersectedBedList = NULL;

/* Sanity check on intersect op. */
if ((!sameString("any", op)) &&
    (!sameString("none", op)) &&
    (!sameString("more", op)) &&
    (!sameString("less", op)) &&
    (!sameString("and", op)) &&
    (!sameString("or", op)))
    {
    errAbort("Invalid value \"%s\" of CGI variable %s", op, hgtaIntersectOp);
    }


/* Load intersecting track into a bitmap. */
fbList2 = fbFromBed(table2, hti2, bedList2, region->start, region->end,
		     isBpWise, FALSE);
bits2 = bitAlloc(chromSize+8);
fbOrBits(bits2, chromSize, fbList2, 0);
featureBitsFreeList(&fbList2);

/* Produce intersectedBedList. */
if (isBpWise)
    {
    /* Base-pair-wise operation: get bitmap  for primary table too */
    struct featureBits *fbList1 = fbFromBed(table1, hti1, bedList1, 
    			               region->start, region->end, 
				       isBpWise, FALSE);
    Bits *bits1 = bitAlloc(chromSize+8);
    fbOrBits(bits1, chromSize, fbList1, 0);
    featureBitsFreeList(&fbList1);
    /* invert inputs if necessary */
    if (invTable)
	bitNot(bits1, chromSize);
    if (invTable2)
	bitNot(bits2, chromSize);
    /* do the intersection/union */
    if (sameString("and", op))
	bitAnd(bits1, bits2, chromSize);
    else
	bitOr(bits1, bits2, chromSize);
    /* translate back to bed */
    intersectedBedList = bitsToBed4List(bits1, chromSize, 
    	region->chrom, 1, region->start, region->end, lm);
    if (retFieldCount != NULL)
	*retFieldCount = 4;
    bitFree(&bits1);
    }
else
    {
    struct bed *nextBed;
    /* Loop through primary bed list seeing if each one intersects
     * enough to keep. */
    for (bed = bedList1;  bed != NULL;  bed = nextBed)
	{
	int numBasesOverlap = countBasesOverlap(bed, bits2, hti1->hasBlocks);
	int length = 0;
	double pctBasesOverlap;
	nextBed = bed->next;
	if (hti1->hasBlocks)
	    {
	    int i;
	    for (i=0;  i < bed->blockCount;  i++)
		length += bed->blockSizes[i];
	    }
	else
	    length = (bed->chromEnd - bed->chromStart);
	if (length == 0)
	    length = 1;
	pctBasesOverlap = ((numBasesOverlap * 100.0) / length);
	if ((sameString("any", op) && (numBasesOverlap > 0)) ||
	    (sameString("none", op) && (numBasesOverlap == 0)) ||
	    (sameString("more", op) &&
	     (pctBasesOverlap >= moreThresh)) ||
	    (sameString("less", op) &&
	     (pctBasesOverlap <= lessThresh)))
	    {
	    slAddHead(&intersectedBedList, bed);
	    }
	}
    slReverse(&intersectedBedList);
    } 
bitFree(&bits2);
lmCleanup(&lm2);
return intersectedBedList;
}

static struct bed *getIntersectedBeds(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm, int *retFieldCount)
/* Get list of beds in region that pass intersection
 * (and filtering) */
{
struct bed *bedList = getFilteredBeds(conn, table, region, lm, retFieldCount);
/*	wiggle tracks have already done the intersection if there was one */
if (!isWiggle(database, table) && anyIntersection())
    {
    struct bed *iBedList = intersectOnRegion(conn, region, table, bedList, 
    	lm, retFieldCount);
    return iBedList;
    }
else
    return bedList;
}

struct bed *cookedBedList(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm, int *retFieldCount)
/* Get data for track in region after all processing steps (filtering
 * intersecting etc.) in BED format.  The retFieldCount variable will be
 * updated if the cooking process takes us down to bed 4 (which happens)
 * with bitwise intersections. */
{
return getIntersectedBeds(conn, table, region, lm, retFieldCount);
}


struct bed *cookedBedsOnRegions(struct sqlConnection *conn, 
	char *table, struct region *regionList, struct lm *lm, int *retFieldCount)
/* Get cooked beds on all regions. */
{
struct bed *bedList = NULL;
struct region *region;
for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *rBedList = getIntersectedBeds(conn, table, region, lm, retFieldCount);
    bedList = slCat(bedList, rBedList);
    }
return bedList;
}

