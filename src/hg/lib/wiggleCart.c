/*	wiggleCart - take care of parsing and combining values from the
 *	wiggle trackDb optional settings and the same values that may be
 *	in the cart.
 */
#include "common.h"
#include "jksql.h"
#include "trackDb.h"
#include "cart.h"
#include "dystring.h"
#include "hui.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: wiggleCart.c,v 1.9 2004/05/10 23:45:02 hiram Exp $";

extern struct cart *cart;      /* defined in hgTracks.c or hgTrackUi */

#define correctOrder(min,max) if (max < min) \
	{ double d; d = max; max = min; min = d; }
/* check a min,max pair (doubles) and keep them properly in order */

#if defined(DEBUG)	/*	dbg	*/

#include "portable.h"

static long wigProfileEnterTime = 0;

void wigProfileEnter()
{
wigProfileEnterTime = clock1000();
}

long wigProfileLeave()
{
long deltaTime = 0;
deltaTime = clock1000() - wigProfileEnterTime;
return deltaTime;
}

/****           some simple debug output during development	*/
static char dbgFile[] = "trash/wig.dbg";
static boolean debugOpened = FALSE;
static FILE * dF;

static void wigDebugOpen(char * name) {
if (debugOpened) return;
dF = fopen( dbgFile, "w");
if ((FILE *)NULL == dF)
	errAbort("wigDebugOpen: can not open %s", dbgFile);
fprintf( dF, "opened by %s\n", name);
chmod(dbgFile, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP  | S_IROTH | S_IWOTH | S_IXOTH);
debugOpened = TRUE;
}

#define DBGMSGSZ	1023
char dbgMsg[DBGMSGSZ+1];
void wigDebugPrint(char * name) {
wigDebugOpen(name);
if (debugOpened)
    {
    if (dbgMsg[0])
	fprintf( dF, "%s: %s\n", name, dbgMsg);
    else
	fprintf( dF, "%s:\n", name);
    }
    dbgMsg[0] = (char) NULL;
    fflush(dF);
}

#ifdef NOT
/*	example usage:	*/
snprintf(dbgMsg, DBGMSGSZ, "%s pixels: min,default,max: %d:%d:%d", tdb->tableName, wigCart->minHeight, wigCart->defaultHeight, wigCart->maxHeight);
wigDebugPrint("wigFetch");
#endif

#endif

/*	Min, Max Y viewing limits
 *	Absolute limits are defined on the trackDb type line
 *	User requested limits are defined in the cart
 *	Default opening display limits are optionally defined with the
 *		defaultViewLimits declaration from trackDb
 *****************************************************************************/
void wigFetchMinMaxY(struct trackDb *tdb, double *min, double *max,
    double *tDbMin, double *tDbMax, int wordCount, char **words)
{
char o4[MAX_OPT_STRLEN]; /* Option 4 - minimum Y axis value: .minY	*/
char o5[MAX_OPT_STRLEN]; /* Option 5 - maximum Y axis value: .minY	*/
char *minY_str = NULL;  /*	string from cart	*/
char *maxY_str = NULL;  /*	string from cart	*/
double minYc;   /*	value from cart */
double maxYc;   /*	value from cart */
double minY;    /*	from trackDb.ra words, the absolute minimum */
double maxY;    /*	from trackDb.ra words, the absolute maximum */
char * tdbDefault = cloneString(
    trackDbSettingOrDefault(tdb, DEFAULTVIEWLIMITS, "NONE") );
double defaultViewMinY = 0.0;	/* optional default viewing window	*/
double defaultViewMaxY = 0.0;	/* can be different than absolute min,max */
boolean optionalViewLimitsExist = FALSE;	/* to decide if using these */


/*	Allow the word "viewLimits" to be recognized too */
if (sameWord("NONE",tdbDefault))
    {
    freeMem(tdbDefault);
    tdbDefault = cloneString(trackDbSettingOrDefault(tdb, VIEWLIMITS, "NONE"));
    }

if (sameWord("NONE",tdbDefault))
    {
    struct hashEl *hel;
    /*	no viewLimits from trackDb, maybe it is in tdb->settings
     *	(custom tracks keep settings here)
     */
    if ((tdb->settings != (char *)NULL) &&
	(tdb->settingsHash != (struct hash *)NULL))
	{
	if ((hel = hashLookup(tdb->settingsHash, VIEWLIMITS)) != NULL)
	    {
	    freeMem(tdbDefault);
	    tdbDefault = cloneString((char *)hel->val);
	    }
	}
    }

/*	Assume last resort defaults, these should never be used
 *	The only case they would be used is if trackDb settings are not
 *	there or can not be parsed properly
 *	A trackDb wiggle track entry should read with three words:
 *	type wig min max
 *	where min and max are floating point numbers (integers OK)
 */
*min = minY = DEFAULT_MIN_Yv;
*max = maxY = DEFAULT_MAX_Yv;

/*	Let's see what trackDb has to say about these things	*/
switch (wordCount)
    {
	case 3:
	    maxY = atof(words[2]);
	    minY = atof(words[1]);
	    break;
	case 2:
	    minY = atof(words[1]);
	    break;
	default:
	    break;
    } 
correctOrder(minY,maxY);
*tDbMin = minY;
*tDbMax = maxY;

/*	See if a default viewing window is specified in the trackDb.ra file
 *	Yes, it is true, this parsing is paranoid and verifies that the
 *	input values are valid in order to be used.  If they are no
 *	good, they are as good as not there and the result is a pair of
 *	zeros and they are not used.
 */
if (differentWord("NONE",tdbDefault))
    {
    char *words[2];
    char *sep = ":";
    int wordCount = chopString(tdbDefault,sep,words,ArraySize(words));
    if (wordCount == 2)
	{
	defaultViewMinY = atof(words[0]);
	defaultViewMaxY = atof(words[1]);
	/*	make sure they are in order	*/
	correctOrder(defaultViewMinY,defaultViewMaxY);

	/*	and they had better be different	*/
	if (! ((defaultViewMaxY - defaultViewMinY) > 0.0))
	    {
	    defaultViewMaxY = defaultViewMinY = 0.0;	/* failed the test */
	    }
	else
	    optionalViewLimitsExist = TRUE;
	}
    }

/*	And finally, let's see if values are available in the cart */
snprintf( o4, sizeof(o4), "%s.%s", tdb->tableName, MIN_Y);
snprintf( o5, sizeof(o5), "%s.%s", tdb->tableName, MAX_Y);
minY_str = cartOptionalString(cart, o4);
maxY_str = cartOptionalString(cart, o5);

if (minY_str && maxY_str)	/*	if specified in the cart */
    {
    minYc = atof(minY_str);	/*	try to use them	*/
    maxYc = atof(maxY_str);
    }
else
    {
    if (optionalViewLimitsExist)	/* if specified in trackDb	*/
	{
	minYc = defaultViewMinY;	/*	try to use these	*/
	maxYc = defaultViewMaxY;
	}
    else
	{
	minYc = minY;		/* no cart, no other settings, use the	*/
	maxYc = maxY;		/* values from the trackDb type line	*/
	}
    }


/*	Finally, clip the possible user requested settings to those
 *	limits within the range of the trackDb type line
 */
*min = max( minY, minYc);
*max = min( maxY, maxYc);
/*      And ensure their order is correct     */
correctOrder(*min,*max);

freeMem(tdbDefault);
}	/*	void wigFetchMinMaxY()	*/

/*	Min, Max, Default Pixel height of track
 *	Limits may be defined in trackDb with the maxHeightPixels string,
 *	Or user requested limits are defined in the cart.
 *	And default opening display limits may optionally be defined with the
 *		maxHeightPixels declaration from trackDb
 *****************************************************************************/
void wigFetchMinMaxPixels(struct trackDb *tdb, int *Min, int *Max, int *Default)
{
char option[MAX_OPT_STRLEN]; /* Option 1 - track pixel height:  .heightPer  */
char *heightPer = NULL; /*	string from cart	*/
int maxHeightPixels = atoi(DEFAULT_HEIGHT_PER);
int defaultHeightPixels = maxHeightPixels;
int defaultHeight;      /*      truncated by limits     */
int minHeightPixels = MIN_HEIGHT_PER;
char * tdbDefault = cloneString(
    trackDbSettingOrDefault(tdb, MAXHEIGHTPIXELS, DEFAULT_HEIGHT_PER) );

if (sameWord(DEFAULT_HEIGHT_PER,tdbDefault))
    {
    struct hashEl *hel;
    /*	no maxHeightPixels from trackDb, maybe it is in tdb->settings
     *	(custom tracks keep settings here)
     */
    if ((tdb->settings != (char *)NULL) &&
	(tdb->settingsHash != (struct hash *)NULL))
	{
	if ((hel = hashLookup(tdb->settingsHash, MAXHEIGHTPIXELS)) != NULL)
	    {
	    freeMem(tdbDefault);
	    tdbDefault = cloneString((char *)hel->val);
	    }
	}
    }

/*	the maxHeightPixels string can be one, two, or three words
 *	separated by :
 *	All three would be: 	max:default:min
 *	When only two: 		max:default
 *	When only one: 		max
 *	(this works too:	min:default:max)
 *	Where min is minimum allowed, default is initial default setting
 *	and max is the maximum allowed
 *	If it isn't available, these three have already been set
 *	in their declarations above
 */
if (differentWord(DEFAULT_HEIGHT_PER,tdbDefault))
    {
    char *words[3];
    char *sep = ":";
    int wordCount;
    wordCount=chopString(tdbDefault,sep,words,ArraySize(words));
    switch (wordCount)
	{
	case 3:
	    minHeightPixels = atoi(words[2]);
	    defaultHeightPixels = atoi(words[1]);
	    maxHeightPixels = atoi(words[0]);
	    correctOrder(minHeightPixels,maxHeightPixels);
	    if (defaultHeightPixels > maxHeightPixels)
		defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	case 2:
	    defaultHeightPixels = atoi(words[1]);
	    maxHeightPixels = atoi(words[0]);
	    if (defaultHeightPixels > maxHeightPixels)
		defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	case 1:
	    maxHeightPixels = atoi(words[0]);
	    defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	default:
	    break;
	}
    }
snprintf( option, sizeof(option), "%s.%s", tdb->tableName, HEIGHTPER);
heightPer = cartOptionalString(cart, option);
/*      Clip the cart value to range [minHeightPixels:maxHeightPixels] */
if (heightPer) defaultHeight = min( maxHeightPixels, atoi(heightPer));
else defaultHeight = defaultHeightPixels;
defaultHeight = max(minHeightPixels, defaultHeight);

*Max = maxHeightPixels;
*Default = defaultHeight;
*Min = minHeightPixels;

freeMem(tdbDefault);
}	/* void wigFetchMinMaxPixels()	*/

/*	A common operation for binary options (two values possible)
 *	check for trackDb.ra, then tdb->settings values
 *	return one of the two possibilities if found
 *	(the tdbString and secondTdbString are a result of
 *		early naming conventions changing over time resulting in
 *		two possible names for the same thing ...)
 */
static char *wigCheckBinaryOption(struct trackDb *tdb, char *Default,
    char *notDefault, char *tdbString, char *secondTdbString)
{
char *tdbDefault = trackDbSettingOrDefault(tdb, tdbString, "NONE");
char *ret;

ret = Default;	/* the answer, unless found to be otherwise	*/

if (sameWord("NONE",tdbDefault) && (secondTdbString != (char *)NULL))
	tdbDefault = trackDbSettingOrDefault(tdb, secondTdbString, "NONE");

if (differentWord("NONE",tdbDefault))
    {
    if (differentWord(Default,tdbDefault))
    	ret = notDefault;
    }
else
    {
    struct hashEl *hel;
    /*	no setting from trackDb, maybe it is in tdb->settings
     *	(custom tracks keep settings here)
     */
    if ((tdb->settings != (char *)NULL) &&
	(tdb->settingsHash != (struct hash *)NULL))
	{
	if ((hel = hashLookup(tdb->settingsHash, tdbString)) != NULL)
	    {
	    if (differentWord(Default,(char *)hel->val))
		ret = notDefault;
	    }
	else if (secondTdbString != (char *)NULL)
	    {
	    if ((hel = hashLookup(tdb->settingsHash, secondTdbString)) != NULL)
		{
		if (differentWord(Default,(char *)hel->val))
		    ret = notDefault;
		}
	    }
	}
    }
return(cloneString(ret));
}

/*	horizontalGrid - off by default **********************************/
enum wiggleGridOptEnum wigFetchHorizontalGrid(struct trackDb *tdb,
    char **optString)
{
char *Default = wiggleGridEnumToString(wiggleHorizontalGridOff);
char *notDefault = wiggleGridEnumToString(wiggleHorizontalGridOn);
char option[MAX_OPT_STRLEN]; /* .horizGrid  */
char *horizontalGrid = NULL;
enum wiggleGridOptEnum ret;

snprintf( option, sizeof(option), "%s.%s", tdb->tableName, HORIZGRID );
horizontalGrid = cloneString(cartOptionalString(cart, option));

if (!horizontalGrid)	/*	if it is NULL	*/
    horizontalGrid = wigCheckBinaryOption(tdb,Default,notDefault,GRIDDEFAULT,
	HORIZGRID);

if (optString)
    *optString = cloneString(horizontalGrid);

ret = wiggleGridStringToEnum(horizontalGrid);
freeMem(horizontalGrid);
return(ret);
}	/*	enum wiggleGridOptEnum wigFetchHorizontalGrid()	*/

/******	autoScale - on by default ***************************************/
enum wiggleScaleOptEnum wigFetchAutoScale(struct trackDb *tdb, char **optString)
{
char *Default = wiggleScaleEnumToString(wiggleScaleAuto);
char *notDefault = wiggleScaleEnumToString(wiggleScaleManual);
char option[MAX_OPT_STRLEN]; /* .autoScale  */
char *autoScale = NULL;
enum wiggleScaleOptEnum ret;

snprintf( option, sizeof(option), "%s.%s", tdb->tableName, AUTOSCALE );
autoScale = cloneString(cartOptionalString(cart, option));

if (!autoScale)	/*	if nothing from the Cart, check trackDb/settings */
    autoScale = wigCheckBinaryOption(tdb,Default,notDefault,AUTOSCALEDEFAULT,
	AUTOSCALE);

if (optString)
    *optString = cloneString(autoScale);

ret = wiggleScaleStringToEnum(autoScale);
freeMem(autoScale);
return(ret);
}	/*	enum wiggleScaleOptEnum wigFetchAutoScale()	*/

/******	graphType - line(points) or bar graph *****************************/
enum wiggleGraphOptEnum wigFetchGraphType(struct trackDb *tdb, char **optString)
{
char *Default = wiggleGraphEnumToString(wiggleGraphBar);
char *notDefault = wiggleGraphEnumToString(wiggleGraphPoints);
char option[MAX_OPT_STRLEN]; /* .lineBar  */
char *graphType = NULL;
enum wiggleGraphOptEnum ret;

snprintf( option, sizeof(option), "%s.%s", tdb->tableName, LINEBAR );
graphType = cloneString(cartOptionalString(cart, option));

if (!graphType)	/*	if nothing from the Cart, check trackDb/settings */
    graphType = wigCheckBinaryOption(tdb,Default,notDefault,GRAPHTYPEDEFAULT,
	GRAPHTYPE);

if (optString)
    *optString = cloneString(graphType);

ret = wiggleGraphStringToEnum(graphType);
freeMem(graphType);
return(ret);
}	/*	enum wiggleGraphOptEnum wigFetchGraphType()	*/

/******	windowingFunction - Maximum by default **************************/
enum wiggleWindowingEnum wigFetchWindowingFunction(struct trackDb *tdb,
	char **optString)
{
char *Default = wiggleWindowingEnumToString(wiggleWindowingMax);
char option[MAX_OPT_STRLEN]; /* .windowingFunction  */
char *windowingFunction = NULL;
enum wiggleWindowingEnum ret;

snprintf( option, sizeof(option), "%s.%s", tdb->tableName, WINDOWINGFUNCTION );
windowingFunction = cloneString(cartOptionalString(cart, option));

/*	If windowingFunction is a string, it came from the cart, otherwise
 *	see if it is specified in the trackDb option, finally
 *	return the default.
 */
if (!windowingFunction)
    {
    char * tdbDefault = 
	trackDbSettingOrDefault(tdb, WINDOWINGFUNCTION, Default);

    freeMem(windowingFunction);
    if (differentWord(Default,tdbDefault))
	windowingFunction = cloneString(tdbDefault);
    else
	{
	struct hashEl *hel;
	/*	no windowingFunction from trackDb, maybe it is in tdb->settings
	 *	(custom tracks keep settings here)
	 */
	windowingFunction = cloneString(Default);
	if ((tdb->settings != (char *)NULL) &&
	    (tdb->settingsHash != (struct hash *)NULL))
	    {
	    if ((hel =hashLookup(tdb->settingsHash, WINDOWINGFUNCTION)) !=NULL)
		if (differentWord(Default,(char *)hel->val))
		    {
		    freeMem(windowingFunction);
		    windowingFunction = cloneString((char *)hel->val);
		    }
	    }
	}
    }

if (optString)
    *optString = cloneString(windowingFunction);

ret = wiggleWindowingStringToEnum(windowingFunction);
freeMem(windowingFunction);
return(ret);
}	/*	enum wiggleWindowingEnum wigFetchWindowingFunction() */

/******	smoothingWindow - OFF by default **************************/
enum wiggleSmoothingEnum wigFetchSmoothingWindow(struct trackDb *tdb,
	char **optString)
{
char * Default = wiggleSmoothingEnumToString(wiggleSmoothingOff);
char option[MAX_OPT_STRLEN]; /* .smoothingWindow  */
char * smoothingWindow = NULL;
enum wiggleSmoothingEnum ret;

snprintf( option, sizeof(option), "%s.%s", tdb->tableName, SMOOTHINGWINDOW );
smoothingWindow = cloneString(cartOptionalString(cart, option));

if (!smoothingWindow) /* if nothing from the Cart, check trackDb/settings */
    {
    char * tdbDefault = 
	trackDbSettingOrDefault(tdb, SMOOTHINGWINDOW, Default);


    if (differentWord(Default,tdbDefault))
	smoothingWindow = cloneString(tdbDefault);
    else
	{
	struct hashEl *hel;
	/*	no smoothingWindow from trackDb, maybe it is in tdb->settings
	 *	(custom tracks keep settings here)
	 */
	smoothingWindow = cloneString(Default);
	if ((tdb->settings != (char *)NULL) &&
	    (tdb->settingsHash != (struct hash *)NULL))
	    {
	    if ((hel = hashLookup(tdb->settingsHash, SMOOTHINGWINDOW)) != NULL)
		if (differentWord(Default,(char *)hel->val))
		    {
		    freeMem(smoothingWindow);
		    smoothingWindow = cloneString((char *)hel->val);
		    }
	    }
	}
    }

if (optString)
    *optString = cloneString(smoothingWindow);

ret = wiggleSmoothingStringToEnum(smoothingWindow);
freeMem(smoothingWindow);
return(ret);
}	/*	enum wiggleSmoothingEnum wigFetchSmoothingWindow()	*/

/*	yLineMark - off by default **********************************/
enum wiggleYLineMarkEnum wigFetchYLineMark(struct trackDb *tdb,
    char **optString)
{
char *Default = wiggleYLineMarkEnumToString(wiggleYLineMarkOff);
char *notDefault = wiggleYLineMarkEnumToString(wiggleYLineMarkOn);
char option[MAX_OPT_STRLEN]; /* .yLineMark  */
char *yLineMark = NULL;
enum wiggleYLineMarkEnum ret;

snprintf( option, sizeof(option), "%s.%s", tdb->tableName, YLINEONOFF );
yLineMark = cloneString(cartOptionalString(cart, option));

if (!yLineMark)	/*	if nothing from the Cart, check trackDb/settings */
    yLineMark = wigCheckBinaryOption(tdb,Default,notDefault,YLINEONOFF,
	(char *)NULL);

if (optString)
    *optString = cloneString(yLineMark);

ret = wiggleYLineMarkStringToEnum(yLineMark);
freeMem(yLineMark);
return(ret);
}	/*	enum wiggleYLineMarkEnum wigFetchYLineMark()	*/

/*	y= marker line value
 *	User requested value is defined in the cart
 *	A Default value can be defined as
 *		yLineMark declaration from trackDb
 *****************************************************************************/
void wigFetchYLineMarkValue(struct trackDb *tdb, double *tDbYMark )
{
char option[MAX_OPT_STRLEN]; /* Option 11 - value from: .yLineMark */
char *yLineMarkValue = NULL;  /*	string from cart	*/
double yLineValue;   /*	value from cart or trackDb */
char * tdbDefault = cloneString(
    trackDbSettingOrDefault(tdb, YLINEMARK, "NONE") );

if (sameWord("NONE",tdbDefault))
    {
    struct hashEl *hel;
    /*	no yLineMark from trackDb, maybe it is in tdb->settings
     *	(custom tracks keep settings here)
     */
    if ((tdb->settings != (char *)NULL) &&
	(tdb->settingsHash != (struct hash *)NULL))
	{
	if ((hel = hashLookup(tdb->settingsHash, YLINEMARK)) != NULL)
	    {
	    freeMem(tdbDefault);
	    tdbDefault = cloneString((char *)hel->val);
	    }
	}
    }

/*	If nothing else, it is zero	*/
yLineValue = 0.0;

/*	Let's see if a value is available in the cart */
snprintf( option, sizeof(option), "%s.%s", tdb->tableName, YLINEMARK);
yLineMarkValue = cartOptionalString(cart, option);

/*	if yLineMarkValue is non-Null, it is the requested value 	*/
if (yLineMarkValue)
    yLineValue = atof(yLineMarkValue);
else /*    See if a default line is specified in the trackDb.ra file */
    if (differentWord("NONE",tdbDefault))
	yLineValue = atof(tdbDefault);

/*	If possible to return	*/
if (tDbYMark)
	*tDbYMark = yLineValue;

freeMem(tdbDefault);
}	/*	void wigFetchYLineMarkValue()	*/
