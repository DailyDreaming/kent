/* wigTrack - stuff to handle loading and display of
 * wig type tracks in browser. Wigs are arbitrary data graphs
 */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "wiggle.h"
#include "scoredRef.h"

static char const rcsid[] = "$Id: wigTrack.c,v 1.38 2004/02/04 18:13:43 hiram Exp $";

/*	wigCartOptions structure - to carry cart options from wigMethods
 *	to all the other methods via the track->extraUiData pointer
 */
struct wigCartOptions
    {
    boolean zoomCompression;	/*  true - do max() averaging over the bin
    				 *  false - simple pick one of the
				 *  points in the bin.
				 */
    enum wiggleGridOptEnum horizontalGrid;	/*  grid lines, ON/OFF */
    enum wiggleGraphOptEnum lineBar;		/*  Line or Bar chart */
    enum wiggleScaleOptEnum autoScale;		/*  autoScale on */
    enum wiggleWindowingEnum windowingFunction;	/*  max,mean,min */
    enum wiggleSmoothingEnum smoothingWindow;	/*  N: [1:15] */
    enum wiggleYLineMarkEnum yLineOnOff;	/*  OFF/ON	*/
    double minY;	/*	from trackDb.ra words, the absolute minimum */
    double maxY;	/*	from trackDb.ra words, the absolute maximum */
    int maxHeight;	/*	maximum pixels height from trackDb	*/
    int defaultHeight;	/*	requested height from cart	*/
    int minHeight;	/*	minimum pixels height from trackDb	*/
    double yLineMark;	/*	user requested line at y = */
    };

struct preDrawElement
    {
	double	max;	/*	maximum value seen for this point	*/
	double	min;	/*	minimum value seen for this point	*/
	unsigned long long	count;	/* number of datum at this point */
	double	sumData;	/*	sum of all values at this point	*/
	double  sumSquares;	/* sum of (values squared) at this point */
	double  plotValue;	/*	raw data to plot	*/
	double  smooth;	/*	smooth data values	*/
    };

struct wigItem
/* A wig track item. */
    {
    struct wigItem *next;
    int start, end;	/* Start/end in chrom (aka browser) coordinates. */
    char *name;		/* Common name */
    char *db;		/* Database */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    unsigned span;      /* each value spans this many bases */
    unsigned count;     /* number of values to use */
    unsigned offset;    /* offset in File to fetch data */
    char *file; /* path name to data file, one byte per value */
    double lowerLimit;  /* lowest data value in this block */
    double dataRange;   /* lowerLimit + dataRange = upperLimit */
    unsigned validCount;        /* number of valid data values in this block */
    double sumData;     /* sum of the data points, for average and stddev calc */
    double sumSquares;      /* sum of data points squared, for stddev calc */
    double graphUpperLimit;	/* filled in by DrawItems	*/
    double graphLowerLimit;	/* filled in by DrawItems	*/
    };

static void wigItemFree(struct wigItem **pEl)
    /* Free up a wigItem. */
{
struct wigItem *el = *pEl;
if (el != NULL)
    {
    freeMem(el->name);
    freeMem(el->db);
    freeMem(el->file);
    freez(pEl);
    }
}

void wigItemFreeList(struct wigItem **pList)
    /* Free a list of dynamically allocated wigItem's */
{
struct wigItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wigItemFree(&el);
    }
*pList = NULL;
}

/*	trackSpans - hash of hashes, first hash is via trackName
 *	the element for trackName is a hash itself where each element
 *	is a Span found in the data (==zoom level indication)
 */
static struct hash *trackSpans = NULL;	/* hash of hashes */

/*	The item names have been massaged during the Load.  An
 *	individual item may have been read in on multiple table rows and
 *	had an extension on it to make it unique from the others.  Also,
 *	each different zoom level had a different extension name.
 *	All these names were condensed into the root of the name with
 *	the extensions removed.
 */
static char *wigName(struct track *tg, void *item)
/* Return name of wig level track. */
{
struct wigItem *wi = item;
return wi->name;
}

/*	This is practically identical to sampleUpdateY in sampleTracks.c
 *	In fact is is functionally identical except jkLib functions are
 *	used instead of the actual string functions.  I will consult
 *	with Ryan to see if one of these copies can be removed.
 */
boolean sameWigGroup(char *name, char *nextName, int lineHeight)
/* Only increment height when name root (extension removed)
 * is different from previous one.  Assumes entries are sorted by name.
 */
{
int different = 0;
char *s0;
char *s1;
s0 = cloneString(name);
s1 = cloneString(nextName);
chopSuffix(s0);
chopSuffix(s1);
different = differentString(s0,s1);
freeMem(s0);
freeMem(s1);
if (different)
    return lineHeight;
else
    return 0;
}

static int wigTotalHeight(struct track *tg, enum trackVisibility vis)
/* Wiggle track will use this to figure out the height they use
   as defined in the cart */
{
struct wigItem *item;
int defaultHeight;
struct wigCartOptions *wigCart;
int itemCount = 1;

wigCart = (struct wigCartOptions *) tg->extraUiData;

/*
 *	A track is just one
 *	item, so there is nothing to do here, either it is the tvFull
 *	height as chosen by the user from TrackUi, or it is the dense
 *	mode.
 */
if (vis == tvDense)
    tg->lineHeight = tl.fontHeight+1;
else if (vis == tvFull)
    tg->lineHeight = max(wigCart->minHeight, wigCart->defaultHeight);

tg->heightPer = tg->lineHeight;
tg->height = tg->lineHeight;

return tg->height;
}

/*	wigLoadItems - read the table rows that hRangeQuery returns
 *	With appropriate adjustment to help hRangeQuery limit its
 *	result to specific "Span" based on the basesPerPixel.
 *	From the rows returned, turn each one into a wigItem, add it to
 *	the growing wiList, and return that wiList as the tg->items.
 *
 *	To help DrawItems, we are going to make up a list of Spans that
 *	were read in for this track.  DrawItems can use that list to
 *	limit itself to the appropriate span.  (Even though we tried to
 *	use Span to limit the hRangeQuery, there may be other Spans in
 *	the result that are not needed during the drawing.)
 *	This list of spans is actually a hash of hashes.
 *	The first level is a hash of track names from each call to
 *	wigLoadItems.  The second level chosen from the track name is
 *	the actual hash of Spans for this particular track.
 *
 *	With 1K zoom Spans available, no more than approximately 1024
 *	rows will need to be loaded at any one time.
 */
static void wigLoadItems(struct track *tg) {
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct wiggle *wiggle;
struct wigItem *wiList = NULL;
char *whereNULL = NULL;
int itemsLoaded = 0;
char spanName[128];
struct hashEl *el, *elList;
struct hashEl *el2, *elList2;
struct hash *spans = NULL;	/* Spans encountered during load */
/*	Check our scale from the global variables that exist in
 *	hgTracks.c - This can give us a guide about which rows to load.
 *	If the scale is more than 1K bases per pixel, we can try loading
 *	only those rows with Span == 1024 to see if an appropriate zoom
 *	level exists.
 */
int basesPerPixel = (int)((double)(winEnd - winStart)/(double)insideWidth);
char *span1K = "Span = 1024 limit 1";
char *spanOver1K = "Span >= 1024";

if (basesPerPixel >= 1024) {
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
	span1K, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
	++itemsLoaded;
}
/*	If that worked, excellent, then we have at least another zoom level
 *	So, for our actual load, use spanOver1K to fetch not only the 1K
 *	zooms, but potentially others that may be useful.  This will
 *	save us a huge amount in loaded rows.  On a 250 Mbase chromosome
 *	there would be 256,000 rows at the 1 base level and only
 *	256 rows at the 1K zoom level.  Otherwise, we go back to the
 *	regular query which will give us all rows.
 */
if (itemsLoaded)
    {
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
	spanOver1K, &rowOffset);
    }
else
    {
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
	whereNULL, &rowOffset);
    }

/*	Allocate trackSpans one time only	*/
if (! trackSpans)
    trackSpans = newHash(0);
/*	Each instance of this LoadItems will create a new spans hash
 *	It will be the value included in the trackSpans hash
 */
spans = newHash(0);
/*	Each row read will be turned into an instance of a wigItem
 *	A growing list of wigItems will be the items list to return
 */
itemsLoaded = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
	struct wigItem *wi;
	size_t fileNameSize = 0;

	++itemsLoaded;
	wiggle = wiggleLoad(row + rowOffset);
	AllocVar(wi);
	wi->start = wiggle->chromStart;
	wi->end = wiggle->chromEnd;
	/*	May need unique name here some day XXX	*/
	wi->name = tg->shortLabel;
	fileNameSize = strlen(wiggle->file) + 1;

	if (! fileExists(wiggle->file))
	    errAbort("wigLoadItems: file '%s' missing", wiggle->file);
	wi->file = cloneString(wiggle->file);

	wi->span = wiggle->span;
	wi->count = wiggle->count;
	wi->offset = wiggle->offset;
	wi->lowerLimit = wiggle->lowerLimit;
	wi->dataRange = wiggle->dataRange;
	wi->validCount = wiggle->validCount;
	wi->sumData = wiggle->sumData;
	wi->sumSquares = wiggle->sumSquares;

	el = hashLookup(trackSpans, wi->name);
	if ( el == NULL)
	    	hashAdd(trackSpans, wi->name, spans);
	snprintf(spanName, sizeof(spanName), "%d", wi->span);
	el = hashLookup(spans, spanName);
	if ( el == NULL)
	    	hashAddInt(spans, spanName, wi->span);
	slAddHead(&wiList, wi);
	wiggleFree(&wiggle);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);

slReverse(&wiList);
tg->items = wiList;
}	/*	wigLoadItems()	*/

static void wigFreeItems(struct track *tg) {
#if defined(DEBUG)
snprintf(dbgMsg, DBGMSGSZ, "I haven't seen wigFreeItems ever called ?");
wigDebugPrint("wigFreeItems");
#endif
}

/*	DrawItems has grown too large.  It has several distinct sections
 *	in it that should now be broken out into their own routines.
 */
static void wigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
struct wigItem *wi;
double pixelsPerBase = scaleForPixels(width);
double basesPerPixel = 1.0;
struct rgbColor *normal = &(tg->color);
Color drawColor = vgFindColorIx(vg, 0, 0, 0);
int itemCount = 0;
char *currentFile = (char *) NULL;	/*	the binary file name */
FILE *f = (FILE *) NULL;		/*	file handle to binary file */
struct hashEl *el, *elList;
char cartStr[64];	/*	to set cart strings	*/
struct wigCartOptions *wigCart;
enum wiggleGridOptEnum horizontalGrid;
enum wiggleGraphOptEnum lineBar;
enum wiggleScaleOptEnum autoScale;
enum wiggleWindowingEnum windowingFunction;
enum wiggleYLineMarkEnum yLineOnOff;
double yLineMark;
Color black = vgFindColorIx(vg, 0, 0, 0);
struct rgbColor blackColor = {0, 0, 0};
struct rgbColor whiteColor = {255, 255, 255};
struct preDrawElement *preDraw;	/* to accumulate everything in prep for draw */
int preDrawZero;		/* location in preDraw where screen starts */
int preDrawSize;		/* size of preDraw array */
int i;				/* an integer loop counter	*/
double overallUpperLimit = -1.0e+300;	/*	determined from data	*/
double overallLowerLimit = 1.0e+300;	/*	determined from data	*/
double overallRange;		/*	determined from data	*/
double graphUpperLimit;		/*	scaling choice will set these	*/
double graphLowerLimit;		/*	scaling choice will set these	*/
double graphRange;		/*	scaling choice will set these	*/
double epsilon;			/*	range of data in one pixel	*/
int x1 = 0;			/*	screen coordinates	*/
int x2 = 0;			/*	screen coordinates	*/

wigCart = (struct wigCartOptions *) tg->extraUiData;
horizontalGrid = wigCart->horizontalGrid;
lineBar = wigCart->lineBar;
autoScale = wigCart->autoScale;
windowingFunction = wigCart->windowingFunction;
yLineOnOff = wigCart->yLineOnOff;
yLineMark = wigCart->yLineMark;

if (pixelsPerBase > 0.0)
    basesPerPixel = 1.0 / pixelsPerBase;

/*	width - width of drawing window in pixels
 *	pixelsPerBase - pixels per base
 *	basesPerPixel - calculated as 1.0/pixelsPerBase
 */
itemCount = 0;
/*	we are going to keep an array that is three times the size of
 *	the screen to allow a screen full on either side of the visible
 *	region.  Those side screens can be used in the smoothing
 *	operation so there won't be any discontinuity at the visible screen
 *	boundaries.
 */
preDrawSize = width * 3;
preDraw = (struct preDrawElement *) needMem ((size_t)
		preDrawSize * sizeof(struct preDrawElement));
preDrawZero = width / 3;
for (i = 0; i < preDrawSize; ++i) {
	preDraw[i].count = 0;
	preDraw[i].max = -1.0e+300;
	preDraw[i].min = 1.0e+300;
}

/*	walk through all the data and prepare the preDraw array	*/
for (wi = tg->items; wi != NULL; wi = wi->next)
    {
    unsigned char *ReadData;	/* the bytes read in from the file */
    int dataOffset = 0;		/*	within data block during drawing */
    int usingDataSpan = 1;		/* will become larger if possible */
    int minimalSpan = 100000000;	/*	a lower limit safety check */

    ++itemCount;
    /*	Take a look through the potential spans, and given what we have
     *	here for basesPerPixel, pick the largest usingDataSpan that is
     *	not greater than the basesPerPixel
     */
    el = hashLookup(trackSpans, wi->name);	/*  What Spans do we have */
    elList = hashElListHash(el->val);		/* Our pointer to spans hash */
    for (el = elList; el != NULL; el = el->next)
	{
	int Span;
	Span = (int) el->val;
	if ((Span < basesPerPixel) && (Span > usingDataSpan))
	    usingDataSpan = Span;
	if (Span < minimalSpan)
	    minimalSpan = Span;
	}
    hashElFreeList(&elList);

    /*	There may not be a span of 1, use whatever is lowest	*/
    if (minimalSpan > usingDataSpan)
	usingDataSpan = minimalSpan;

    /*	Now that we know what Span to draw, see if this item should be
     *	drawn at all.
     */
    if (usingDataSpan == wi->span)
	{
	/*	Check our data file, see if we need to open a new one */
	if (currentFile)
	    {
	    if (differentString(currentFile,wi->file))
		{
		if (f != (FILE *) NULL)
		    {
		    fclose(f);
		    freeMem(currentFile);
		    }
		currentFile = cloneString(wi->file);
		f = mustOpen(currentFile, "r");
		}
	    }
	else
	    {
	    currentFile = cloneString(wi->file);
	    f = mustOpen(currentFile, "r");
	    }
/*	Ready to draw, what do we know:
 *	the feature being processed:
 *	chrom coords:  [wi->start : wi-end)
 *
 *	The data to be drawn: to be read from file f at offset wi->Offset
 *	data points available: wi->Count, representing wi->Span bases
 *	for each data point
 *
 *	The drawing window, in pixels:
 *	xOff = left margin, yOff = top margin, h = height of drawing window
 *	drawing window in chrom coords: seqStart, seqEnd
 *	'basesPerPixel' is known, 'pixelsPerBase' is known
 */
	fseek(f, wi->offset, SEEK_SET);
	ReadData = (unsigned char *) needMem((size_t) (wi->count + 1));
	fread(ReadData, (size_t) wi->count, (size_t) sizeof(unsigned char), f);

	/*	let's check end point screen coordinates.  If they are
 	 *	the same, then this entire data block lands on one pixel,
 	 *	no need to walk through it, just use the block's specified
 	 *	max/min.  It is OK if these end up + or -, we do want to
 	 *	keep track of pixels before and after the screen for
 	 *	later smoothing operations
	 */
	x1 = (wi->start - seqStart) * pixelsPerBase;
	x2 = ((wi->start+(wi->count * usingDataSpan))-seqStart) * pixelsPerBase;

	if (x2 > x1) {
	    /*	walk through all the data in this block	*/
	    for (dataOffset = 0; dataOffset < wi->count; ++dataOffset)
		{
		unsigned char datum = ReadData[dataOffset];
		if (datum != WIG_NO_DATA)
		    {
    x1 = ((wi->start-seqStart) + (dataOffset * usingDataSpan)) * pixelsPerBase;
		    x2 = x1 + (usingDataSpan * pixelsPerBase);
		    for (i = x1; i <= x2; ++i)
			{
			int xCoord = preDrawZero + i;
			if ((xCoord >= 0) && (xCoord < preDrawSize))
			    {
			double dataValue =
	wi->lowerLimit+(((double)datum/(double)MAX_WIG_VALUE)*wi->dataRange);
			    ++preDraw[xCoord].count;
			    if (dataValue > preDraw[xCoord].max)
				preDraw[xCoord].max = dataValue;
			    if (dataValue < preDraw[xCoord].min)
				preDraw[xCoord].min = dataValue;
			    preDraw[xCoord].sumData += dataValue;
			    preDraw[xCoord].sumSquares += dataValue * dataValue;
			    }
			}
		    }
		}
	} else {	/*	only one pixel for this block of data */
	    int xCoord = preDrawZero + x1;
	    /*	if the point falls within our array, record it.
	     *	the (wi->validCount > 0) is a safety check.  It
	     *	should always be true unless the data was
	     *	prepared incorrectly.
	     */
	    if ((wi->validCount > 0) && (xCoord >= 0) && (xCoord < preDrawSize))
		{
		double upperLimit;
		preDraw[xCoord].count += wi->validCount;
		upperLimit = wi->lowerLimit + wi->dataRange;
		if (upperLimit > preDraw[xCoord].max)
		    preDraw[xCoord].max = upperLimit;
		if (wi->lowerLimit < preDraw[xCoord].min)
		    preDraw[xCoord].min = wi->lowerLimit;
		preDraw[xCoord].sumData += wi->sumData;
		preDraw[xCoord].sumSquares += wi->sumSquares;
		}
	}
	freeMem(ReadData);
	}	/*	Draw if span is correct	*/
    }	/*	for (wi = tg->items; wi != NULL; wi = wi->next)	*/
if (f != (FILE *) NULL)
    {
    fclose(f);
    }
if (currentFile)
    freeMem(currentFile);

/*	now we are ready to draw.  Each element in the preDraw[] array
 *	cooresponds to a single pixel on the screen
 */

/*	Determine the raw plotting value	*/
for (i = 0; i < preDrawSize; ++i)
    {
    double dataValue;
    if (preDraw[i].count)
	{
    switch (windowingFunction)
	{
	case (wiggleWindowingMin):
		if (fabs(preDraw[i].min)
				< fabs(preDraw[i].max))
		    dataValue = preDraw[i].min;
		else 
		    dataValue = preDraw[i].max;
		break;
	case (wiggleWindowingMean):
		dataValue =
		    preDraw[i].sumData / preDraw[i].count;
		break;
	default:
	case (wiggleWindowingMax):
		if (fabs(preDraw[i].min)
			> fabs(preDraw[i].max))
		    dataValue = preDraw[i].min;
		else 
		    dataValue = preDraw[i].max;
		break;
	}
	preDraw[i].plotValue = dataValue;
	preDraw[i].smooth = dataValue;
	}
    }

/*	Are we perhaps doing smoothing ?  smoothingWindow is 1 off due
 *	to enum funny business in inc/hui.h and lib/hui.c 	*/
if (wigCart->smoothingWindow > 0)
    {
    int winSize = wigCart->smoothingWindow + 1; /* enum funny business */
    int winBegin = 0;
    int winMiddle = -(winSize/2);
    int winEnd = -winSize;
    double sum = 0.0;
    unsigned long long points = 0LL;

    for (winBegin = 0; winBegin < preDrawSize; ++winBegin)
	{
	if (winEnd >=0)
	    {
	    if (preDraw[winEnd].count)
		{
		points -= preDraw[winEnd].count;
		sum -= preDraw[winEnd].plotValue * preDraw[winEnd].count;
		}
	    }
	if (preDraw[winBegin].count)
	    {
	    points += preDraw[winBegin].count;
	    sum += preDraw[winBegin].plotValue * preDraw[winBegin].count;
	    }
	if ((winMiddle >= 0) && points && preDraw[winMiddle].count)
		preDraw[winMiddle].smooth = sum / points;
	++winEnd;
	++winMiddle;
	}
    }

for (i = preDrawZero; i < preDrawZero+width; ++i)
    {
    if (preDraw[i].max > overallUpperLimit)
	overallUpperLimit = preDraw[i].max;
    if (preDraw[i].min < overallLowerLimit)
	overallLowerLimit = preDraw[i].min;
    }
overallRange = overallUpperLimit - overallLowerLimit;

if (autoScale == wiggleScaleAuto)
    {
    if (overallRange == 0.0)
	{
	if (overallUpperLimit > 0.0)
	    {
	    graphUpperLimit = overallUpperLimit;
	    graphLowerLimit = 0.0;
	    } else if (overallUpperLimit < 0.0) {
	    graphUpperLimit = 0.0;
	    graphLowerLimit = overallUpperLimit;
	    } else {
	    graphUpperLimit = 1.0;
	    graphLowerLimit = -1.0;
	    }
	    graphRange = graphUpperLimit - graphLowerLimit;
	} else {
	graphUpperLimit = overallUpperLimit;
	graphLowerLimit = overallLowerLimit;
	}
    } else {
	graphUpperLimit = wigCart->maxY;
	graphLowerLimit = wigCart->minY;
    }
graphRange = graphUpperLimit - graphLowerLimit;
epsilon = graphRange / tg->lineHeight;

/*
 *	We need to put the graphing limits back into the items
 *	so the LeftLabels routine can find these numbers.
 *	This may seem like overkill to put it into each item but
 *	this will become necessary when these graphs stack up upon
 *	each other in a multiple item display, each one will have its
 *	own graph.
 */
for (wi = tg->items; wi != NULL; wi = wi->next)
    {
	wi->graphUpperLimit = graphUpperLimit;
	wi->graphLowerLimit = graphLowerLimit;
    }
/*	right now this is a simple pixel by pixel loop.  Future
 *	enhancements will smooth this data and draw boxes where pixels
 *	are all the same height in a run.
 */
for (x1 = 0; x1 < width; ++x1)
    {
    int preDrawIndex = x1 + preDrawZero;
    /*	count is non-zero meaning valid data exists here	*/
    if (preDraw[preDrawIndex].count)
	{
	int h = tg->lineHeight;	/*	the height of our drawing window */
	int boxHeight;		/*	the size of our box to draw	*/
	int boxTop;		/*	box top starts here	*/
	int y1;			/*	y coordinate of data point */
	int y0;			/*	y coordinate of data = 0.0 */
	int yPointGraph;	/*	y coordinate of data for point style */
	double dataValue;	/*	the data value in data space	*/

	/*	The graphing coordinate conversion situation is:
	 *	graph coordinate y = 0 is graphUpperLimit data space
	 *	and total graph height is h which is graphRange in data space
	 *	The Y axis is positive down, negative up.
	 *
	 *	Taking a simple coordinate conversion from data space
	 *	to the graphing space, the data value is at:
	 *	h * ((graphUpperLimit - dataValue)/graphRange)
	 *	and a data value zero line is at:
	 *	h * (graphUpperLimit/graphRange)
	 *	These may end up to be negative meaning they are above
	 *	the upper graphing limit, or be very large, meaning they
	 *	are below the lower graphing limit.  This is OK, the
	 *	clipping will be taken care of by the vgBox() function.
	 */

	/*	data value has been picked by previous scanning.
	 *	Could be smoothed, maybe not.
	 */
	dataValue = preDraw[preDrawIndex].smooth;

	y1 = h * ((graphUpperLimit - dataValue)/graphRange);
	yPointGraph = yOff + y1 - 1;
	y0 = h * ((graphUpperLimit)/graphRange);
	boxHeight = abs(y1 - y0);
	boxTop = min(y1,y0);
	/*	special case where dataValue is on the zero line, it
 	 *	needs to have a boxHeight of 1, otherwise it disappears into
 	 *	zero nothingness
	 */
	if (fabs(dataValue) < epsilon)
	    {
	    boxTop -= 1;
	    boxHeight = 1;
	    }
	/*	Last pixel (bottom) is a special case of a close interval */
	if ((boxTop == h) && (boxHeight == 0))
	    {
	    boxTop = h - 1;
	    boxHeight = 1;
	    }
	/*	Special case data value on upper limit line	*/
	if ((boxTop+boxHeight) == 0)
		boxHeight += 1;
	/*	negative data is the alternate color	*/
	if (dataValue < 0.0)
	    drawColor = tg->ixAltColor;
	else
	    drawColor = tg->ixColor;

	/*	vgBox will take care of clipping.  No need to worry
	 *	about coordinates or height of line to draw.
	 *	We are actually drawing single pixel wide lines here.
	 */
	if (vis == tvFull)
	    {
	    if (lineBar == wiggleGraphBar)
		{
		vgBox(vg, x1+xOff, yOff+boxTop, 1, boxHeight, drawColor);
		}
	    else
		{	/*	draw a 3 pixel height box	*/
		vgBox(vg, x1+xOff, yPointGraph, 1, 3, drawColor);
		}
	    }	/*	vis == tvFull	*/
	else if (vis == tvDense)
	    {
	    double dataValue;
	    int grayIndex;
	    dataValue = preDraw[preDrawIndex].smooth - graphLowerLimit;
	    grayIndex = (dataValue/graphRange) * MAX_WIG_VALUE;

	    drawColor =
		tg->colorShades[grayInRange(grayIndex, 0, MAX_WIG_VALUE)];

	    boxHeight = tg->lineHeight;
	    vgBox(vg, x1+xOff, yOff, 1,
		boxHeight, drawColor);
	    }	/*	vis == tvDense	*/
	}	/*	if (preDraw[].count)	*/
    }	/*	for (x1 = 0; x1 < width; ++x1)	*/

/*	Do we need to draw a zero line ?
 *	This is to be generalized in the future to allow horizontal grid
 *	lines, perhaps user specified to indicate thresholds.
 */
if ((vis == tvFull) && (horizontalGrid == wiggleHorizontalGridOn))
    {
    int x1, x2, y1, y2;
    int gridLines = 2;

    x1 = xOff;
    x2 = x1 + width;

    /*	Let's see if the zero line can be drawn	*/
    if ((0.0 <= graphUpperLimit) && (0.0 >= graphLowerLimit))
	{
	int zeroOffset;
	drawColor = vgFindColorIx(vg, 0, 0, 0);
	zeroOffset = (int)((graphUpperLimit * tg->lineHeight) /
			(graphUpperLimit - graphLowerLimit));
	y1 = yOff + zeroOffset;
	if (y1 >= (yOff + tg->lineHeight)) y1 = yOff + tg->lineHeight - 1;
	y2 = y1;
	vgLine(vg,x1,y1,x2,y2,black);
	}

    }	/*	drawing horizontalGrid	*/

/*	Optionally, a user requested Y marker line at some value */
if ((vis == tvFull) && (yLineOnOff == wiggleYLineMarkOn))
    {
    int x1, x2, y1, y2;
    int gridLines = 2;

    x1 = xOff;
    x2 = x1 + width;

    /*	Let's see if this marker line can be drawn	*/
    if ((yLineMark <= graphUpperLimit) && (yLineMark >= graphLowerLimit))
	{
	int Offset;
	drawColor = vgFindColorIx(vg, 0, 0, 0);
	Offset = tg->lineHeight * ((graphUpperLimit - yLineMark)/graphRange);
	y1 = yOff + Offset;
	if (y1 >= (yOff + tg->lineHeight)) y1 = yOff + tg->lineHeight - 1;
	y2 = y1;
	vgLine(vg,x1,y1,x2,y2,black);
	}

    }	/*	drawing y= line marker	*/

freeMem(preDraw);
}	/*	wigDrawItems()	*/

static void wigLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
{
struct wigItem *wi;
int fontHeight = tl.fontHeight+1;
int centerOffset = 0;
enum wiggleYLineMarkEnum yLineOnOff;
double yLineMark;
double lines[2];	/*	lines to label	*/
int numberOfLines = 1;	/*	at least one: 0.0	*/
int i;			/*	loop counter	*/
struct wigCartOptions *wigCart;

wigCart = (struct wigCartOptions *) tg->extraUiData;
lines[0] = 0.0;
lines[1] = wigCart->yLineMark;
if (wigCart->yLineOnOff == wiggleYLineMarkOn)
    ++numberOfLines;

if (withCenterLabels)
	centerOffset = fontHeight;

/*	We only do Dense and Full	*/
if (tg->visibility == tvDense)
    {
    vgTextRight(vg, xOff, yOff+centerOffset, width - 1, height-centerOffset,
	tg->ixColor, font, tg->shortLabel);
    }
else if (tg->visibility == tvFull)
    {
    int centerLabel = (height/2)-(fontHeight/2);
    int labelWidth = 0;

    /* track label is centered in the whole region */
    vgText(vg, xOff, yOff+centerLabel, tg->ixColor, font, tg->shortLabel);
    labelWidth = mgFontStringWidth(font,tg->shortLabel);
    /*	Is there room left to draw the min, max ?	*/
    if (height >= (3 * fontHeight))
	{
	boolean zeroOK = TRUE;
	double graphUpperLimit = -1.0e+300;
	double graphLowerLimit = 1.0e+300;
	char upper[128];
	char lower[128];
	char upperTic = '-';	/* as close as we can get with ASCII */
			/* the ideal here would be to draw tic marks in
 			 * exactly the correct location.
			 */
	Color drawColor;
	if (withCenterLabels)
	    {
	    centerOffset = fontHeight;
	    upperTic = '_';	/*	this is correct	*/
	    }
	for (wi = tg->items; wi != NULL; wi = wi->next)
	    {
	    if (wi->graphUpperLimit > graphUpperLimit)
		graphUpperLimit = wi->graphUpperLimit;
	    if (wi->graphLowerLimit < graphLowerLimit)
		graphLowerLimit = wi->graphLowerLimit;
	    }
	/*  In areas where there is no data, these limits do not change */
	if (graphUpperLimit < graphLowerLimit)
	    {
	    double d = graphLowerLimit;
	    graphLowerLimit = graphUpperLimit;
	    graphUpperLimit = d;
	    snprintf(upper, 128, "No data %c", upperTic);
	    snprintf(lower, 128, "No data _");
	    zeroOK = FALSE;
	    }
	else
	    {
	    snprintf(upper, 128, "%g %c", graphUpperLimit, upperTic);
	    snprintf(lower, 128, "%g _", graphLowerLimit);
	    }
	drawColor = tg->ixColor;
	if (graphUpperLimit < 0.0) drawColor = tg->ixAltColor;
	vgTextRight(vg, xOff, yOff, width - 1, fontHeight, drawColor,
	    font, upper);
	drawColor = tg->ixColor;
	if (graphLowerLimit < 0.0) drawColor = tg->ixAltColor;
	vgTextRight(vg, xOff, yOff+height-fontHeight, width - 1, fontHeight,
	    drawColor, font, lower);

	for (i = 0; i < numberOfLines; ++i )
	    {
	    double lineValue = lines[i];
	    /*	Maybe zero can be displayed */
	    /*	It may overwrite the track label ...	*/
	    if (zeroOK && (lineValue < graphUpperLimit) &&
		(lineValue > graphLowerLimit))
		{
		int offset;
		int Width;

		drawColor = vgFindColorIx(vg, 0, 0, 0);
		offset = centerOffset +
		    (int)(((graphUpperLimit - lineValue) *
				(height - centerOffset)) /
			(graphUpperLimit - graphLowerLimit));
		/*	reusing the lower string here	*/
		if (i == 0)
		    snprintf(lower, 128, "0 -");
		else
		    snprintf(lower, 128, "%g -", lineValue);
		/*	only draw if it is far enough away from the
		 *	upper and lower labels, and it won't overlap with
		 *	the center label.
		 */
		Width = mgFontStringWidth(font,lower);
		if ( !( (offset < centerLabel+fontHeight) &&
		    (offset > centerLabel-(fontHeight/2)) &&
		    (Width+labelWidth >= width) ) &&
		    (offset > (fontHeight*2)) &&
		    (offset < height-(fontHeight*2)) )
		    {
		    vgTextRight(vg, xOff, yOff+offset-(fontHeight/2),
			width - 1, fontHeight, drawColor, font, lower);
		    }
		}	/*	drawing a zero label	*/
	    }	/*	drawing 0.0 and perhaps yLineMark	*/
	}	/* if (height >= (3 * fontHeight))	*/
    }	/*	if (tg->visibility == tvFull)	*/
}	/* wigLeftLabels */

/* Make track group for wig multiple alignment.
 *	WARNING ! - track->visibility is merely the default value
 *	from the trackDb entry at this time.  It will be set after this
 *	 by hgTracks from its cart UI setting.  When called in
 *	 TotalHeight it will then be the requested visibility.
 */
void wigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
{
int defaultHeight;	/*	truncated by limits	*/
enum wiggleGraphOptEnum wiggleLineBar = wiggleGraphStringToEnum("Bar");
double minY;	/*	from trackDb or cart, requested minimum */
double maxY;	/*	from trackDb or cart, requested maximum */
double tDbMinY;	/*	from trackDb type line, the absolute minimum */
double tDbMaxY;	/*	from trackDb type line, the absolute maximum */
double yLineMark;	/*	from trackDb or cart */
char cartStr[64];	/*	to set cart strings	*/
struct wigCartOptions *wigCart;
int maxHeight = atoi(DEFAULT_HEIGHT_PER);
int defaultHeightPixels = maxHeight;
int minHeight = MIN_HEIGHT_PER;

AllocVar(wigCart);

/*	These Fetch functions look for variables in the cart bounded by
 *	limits specified in trackDb or returning defaults
 */
wigCart->lineBar = wigFetchGraphType(tdb, (char **) NULL);
wigCart->horizontalGrid = wigFetchHorizontalGrid(tdb, (char **) NULL);

wigCart->autoScale = wigFetchAutoScale(tdb, (char **) NULL);
wigCart->windowingFunction = wigFetchWindowingFunction(tdb, (char **) NULL);
wigCart->smoothingWindow = wigFetchSmoothingWindow(tdb, (char **) NULL);

wigFetchMinMaxPixels(tdb, &minHeight, &maxHeight, &defaultHeight);
wigFetchYLineMarkValue(tdb, &yLineMark);
wigCart->yLineMark = yLineMark;
wigCart->yLineOnOff = wigFetchYLineMark(tdb, (char **) NULL);

wigCart->maxHeight = maxHeight;
wigCart->defaultHeight = defaultHeight;
wigCart->minHeight = minHeight;

wigFetchMinMaxY(tdb, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
track->minRange = minY;
track->maxRange = maxY;
wigCart->minY = track->minRange;
wigCart->maxY = track->maxRange;

track->loadItems = wigLoadItems;
track->freeItems = wigFreeItems;
track->drawItems = wigDrawItems;
track->itemName = wigName;
track->mapItemName = wigName;
track->totalHeight = wigTotalHeight;
track->itemHeight = tgFixedItemHeight;
track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
track->mapsSelf = TRUE;
track->extraUiData = (void *) wigCart;
track->colorShades = shadesOfGray;
track->drawLeftLabels = wigLeftLabels;
}	/*	wigMethods()	*/
