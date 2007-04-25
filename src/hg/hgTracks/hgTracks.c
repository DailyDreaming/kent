/* hgTracks - Human Genome browser main cgi script. */
#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "portable.h"
#include "memalloc.h"
#include "localmem.h"
#include "obscure.h"
#include "dystring.h"
#include "hash.h"
#include "jksql.h"
#include "gfxPoly.h"
#include "memgfx.h"
#include "vGfx.h"
#include "psGfx.h"
#include "browserGfx.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "spDb.h"
#include "hui.h"
#include "hgFind.h"
#include "hgTracks.h"
#include "spaceSaver.h" 
#include "wormdna.h"
#include "aliType.h"
#include "psl.h"
#include "agpGap.h"
#include "cgh.h"
#include "bactigPos.h"
#include "genePred.h"
#include "genePredReader.h"
#include "bed.h"
#include "isochores.h"
#include "simpleRepeat.h"
#include "cpgIsland.h"
#include "gcPercent.h"
#include "genomicDups.h"
#include "mapSts.h"
#include "est3.h"
#include "exoFish.h"
#include "roughAli.h"
#include "snp.h"
#include "rnaGene.h"
#include "fishClones.h"
#include "stsMarker.h"
#include "stsMap.h"
#include "stsMapMouseNew.h"
#include "stsMapRat.h"
#include "snpMap.h"
#include "recombRate.h"
#include "recombRateRat.h"
#include "recombRateMouse.h"
#include "chr18deletions.h"
#include "mouseOrtho.h"
#include "humanParalog.h"
#include "synteny100000.h"
#include "mouseSyn.h"
#include "mouseSynWhd.h"
#include "ensFace.h"
#include "ensPhusionBlast.h"
#include "knownMore.h"
#include "customTrack.h"
#include "customFactory.h"
#include "trackDb.h"
#include "pslWScore.h"
#include "lfs.h"
#include "mcnBreakpoints.h"
#include "altGraph.h"
#include "altGraphX.h"
#include "loweLabTracks.h"
#include "geneGraph.h"
#include "genMapDb.h"
#include "genomicSuperDups.h"
#include "celeraDupPositive.h"
#include "celeraCoverage.h"
#include "web.h"
#include "grp.h"
#include "chromColors.h"
#include "cds.h"
#include "simpleNucDiff.h"
#include "tfbsCons.h"
#include "tfbsConsSites.h"
#include "itemAttr.h"
#include "encode.h"
#include "variation.h"
#include "estOrientInfo.h"
#include "versionInfo.h"
#include "bedCart.h"
#include "cytoBand.h"
#include "gencodeIntron.h"
#include "cutterTrack.h"
#include "retroGene.h"
#include "switchGear.h"
#include "dless.h"
#include "liftOver.h"
#include "hgConfig.h"
#include "gv.h"
#include "gvUi.h"
#include "oreganno.h"
#include "oregannoUi.h"
#include "bed12Source.h"
#include "dbRIP.h"
#include "wikiLink.h"
#include "dnaMotif.h"
#include "hapmapTrack.h"
#include "trashDir.h"

static char const rcsid[] = "$Id: hgTracks.c,v 1.1323 2007/04/25 22:21:06 heather Exp $";

boolean measureTiming = FALSE;	/* Flip this on to display timing
                                 * stats on each track at bottom of page. */
static long enteredMainTime = 0;	/* time at beginning of main()	*/
boolean isPrivateHost;		/* True if we're on genome-test. */
char *protDbName;               /* Name of proteome database for this genome. */
boolean gotKgColor;	/* Set to true if just take color from table. */

#define MAX_CONTROL_COLUMNS 5
#define CHROM_COLORS 26
#define LOW 1
#define MEDIUM 2
#define BRIGHT 3
#define MAXPIXELS 14000
#define MAXCHAINS 50000000
boolean hgDebug = FALSE;      /* Activate debugging code. Set to true by hgDebug=on in command line*/
int imagePixelHeight = 0;
int colorBin[MAXPIXELS][256]; /* count of colors for each pixel for each color */
/* Declare our color gradients and the the number of colors in them */
Color shadesOfGreen[EXPR_DATA_SHADES];
Color shadesOfRed[EXPR_DATA_SHADES];
Color shadesOfBlue[EXPR_DATA_SHADES];
Color orangeColor = 0;
Color brickColor = 0;
Color blueColor = 0;
Color darkBlueColor = 0;
Color greenColor = 0;
Color darkGreenColor = 0;
boolean exprBedColorsMade = FALSE; /* Have the shades of Green, Red, and Blue been allocated? */
int maxRGBShade = EXPR_DATA_SHADES - 1;

Color chromColor[CHROM_COLORS+1];
/* Declare colors for chromosome coloring, +1 for unused chrom 0 color */

/* Have the 3 shades of 8 chromosome colors been allocated? */
boolean chromosomeColorsMade = FALSE; 

int z;
int maxCount;
int bestColor;
int maxItemsInFullTrack = 250;  /* Maximum number of items displayed in full */
int maxItemsToUseOverflowDefault = 10000; /* # of items to allow overflow mode*/
int guidelineSpacing = 12;	/* Pixels between guidelines. */

struct cart *cart;	/* The cart where we keep persistent variables. */
struct hash *oldVars = NULL;
struct hash *hgFindMatches; /* The matches found by hgFind that should be highlighted. */

/* These variables persist from one incarnation of this program to the
 * next - living mostly in the cart. */
char *chromName;		/* Name of chromosome sequence . */
char *database;			/* Name of database we're using. */
char *organism;			/* Name of organism we're working on. */
int winStart;			/* Start of window in sequence. */
int winEnd;			/* End of window in sequence. */
static char *position = NULL; 		/* Name of position. */
static char *userSeqString = NULL;	/* User sequence .fa/.psl file. */

int gfxBorder = hgDefaultGfxBorder;	/* Width of graphics border. */
int leftLabelWidthChars = 17;   /* number of characters allowed for left label */
int trackTabWidth = 11;
int insideX;			/* Start of area to draw track in in pixels. */
int insideWidth;		/* Width of area to draw tracks in in pixels. */
int leftLabelX;			/* Start of area to draw left labels on. */
int leftLabelWidth;		/* Width of area to draw left labels on. */
boolean zoomedToBaseLevel; 	/* TRUE if zoomed so we can draw bases. */
boolean zoomedToCodonLevel; /* TRUE if zoomed so we can print codons text in genePreds*/
boolean zoomedToCdsColorLevel; /* TRUE if zoomed so we can color each codon*/
boolean baseShowPos;           /* TRUE if should display full position at top of base track */
boolean baseShowAsm;           /* TRUE if should display assembly info at top of base track */
char *baseTitle = NULL;        /* Title it should display top of base track (optional)*/

/* These variables are set by getPositionFromCustomTracks() at the very 
 * beginning of tracksDisplay(), and then used by loadCustomTracks(). */
char *ctFileName = NULL;	/* Custom track file. */
struct customTrack *ctList = NULL;  /* Custom tracks. */
boolean hasCustomTracks = FALSE;  /* whether any custom tracks are for this db*/
struct slName *browserLines = NULL; /* Custom track "browser" lines. */

boolean withIdeogram = TRUE;            /* Display chromosome ideogram? */
boolean ideogramAvail = FALSE;           /* Is the ideogram data available for this genome? */
boolean withLeftLabels = TRUE;		/* Display left labels? */
boolean withCenterLabels = TRUE;	/* Display center labels? */
boolean withGuidelines = TRUE;		/* Display guidelines? */
boolean withNextItemArrows = FALSE;	/* Display next feature (gene) navigation buttons near center labels? */
boolean withNextExonArrows = FALSE;	/* Display next exon navigation buttons near center labels? */
boolean withPriorityOverride = FALSE;	/* Display priority for each track to allow reordering */
boolean hideControls = FALSE;		/* Hide all controls? */

int rulerMode = tvHide;         /* on, off, full */

char *rulerMenu[] =
/* dropdown for ruler visibility */
    {
    "hide",
    "dense",
    "full"
    };

/* Structure returned from findGenomePos. 
 * We use this to to expand any tracks to full
 * that were found to contain the searched-upon
 * position string */
struct hgPositions *hgp = NULL;

/* subject list used for GSID */
struct gsidSubj *gsidSelectedSubjList = NULL;

struct trackLayout tl;

void initTl()
/* Initialize layout around small font and a picture about 600 pixels
 * wide. */
{
trackLayoutInit(&tl, cart);
tl.leftLabelWidth = leftLabelWidthChars*tl.nWidth + trackTabWidth;
}



/* Other global variables. */
int seqBaseCount;	/* Number of bases in sequence. */
int winBaseCount;	/* Number of bases in window. */

int maxShade = 9;	/* Highest shade in a color gradient. */
Color shadesOfGray[10+1];	/* 10 shades of gray from white to black
                                 * Red is put at end to alert overflow. */
Color shadesOfBrown[10+1];	/* 10 shades of brown from tan to tar. */
struct rgbColor brownColor = {100, 50, 0};
struct rgbColor tanColor = {255, 240, 200};
struct rgbColor guidelineColor = { 220, 220, 255};
struct rgbColor undefinedYellowColor = {240,240,180};

Color shadesOfSea[10+1];       /* Ten sea shades. */
struct rgbColor darkSeaColor = {0, 60, 120};
struct rgbColor lightSeaColor = {200, 220, 255};

void loadSampleIntoLinkedFeature(struct track *tg);

struct track *trackList = NULL;    /* List of all tracks. */
struct group *groupList = NULL;    /* List of all tracks. */


/* Some little functional stubs to fill in track group
 * function pointers with if we have nothing to do. */
void tgLoadNothing(struct track *tg){}
void tgFreeNothing(struct track *tg){}
int tgItemNoStart(struct track *tg, void *item) {return -1;}
int tgItemNoEnd(struct track *tg, void *item) {return -1;}

int tgCmpPriority(const void *va, const void *vb)
/* Compare to sort based on priority. */
{
const struct track *a = *((struct track **)va);
const struct track *b = *((struct track **)vb);
float dif = a->group->priority - b->group->priority;

if (dif == 0)
    dif = a->priority - b->priority;
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return 0;
else
   return 1;
}

int gCmpPriority(const void *va, const void *vb)
/* Compare to sort based on priority. */
{
const struct group *a = *((struct group **)va);
const struct group *b = *((struct group **)vb);
float dif = a->priority - b->priority;

if (dif == 0)
    return 0;
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return 0;
else
   return 1;
}

int tgFixedItemHeight(struct track *tg, void *item)
/* Return item height for fixed height track. */
{
return tg->lineHeight;
}

int packCountRowsOverflow(struct track *tg, int maxCount, 
			  boolean withLabels, boolean allowOverflow)
/* Return packed height. */
{
struct spaceSaver *ss;
struct slList *item;
MgFont *font = tl.font;
int extraWidth = tl.mWidth * 2;
long long start, end;
double scale = (double)insideWidth/(winEnd - winStart);
spaceSaverFree(&tg->ss);
ss = tg->ss = spaceSaverNew(0, insideWidth, maxCount);
for (item = tg->items; item != NULL; item = item->next)
    {
    int baseStart = tg->itemStart(tg, item);
    int baseEnd = tg->itemEnd(tg, item);
    if (baseStart < winEnd && baseEnd > winStart)
        {
	if (baseStart <= winStart)
	    start = 0;
	else
	    start = round((double)(baseStart - winStart)*scale);
	if (!tg->drawName && withLabels)
	    start -= mgFontStringWidth(font,
				       tg->itemName(tg, item)) + extraWidth;
	if (baseEnd >= winEnd)
	    end = insideWidth;
	else
	    end = round((baseEnd - winStart)*scale);
	if (start < 0) start = 0;
	if (spaceSaverAddOverflow(ss, start, end, item, allowOverflow) == NULL)
	    break;
	}
    }
spaceSaverFinish(ss);
return ss->rowCount;
}

int packCountRows(struct track *tg, int maxCount, boolean withLabels)
/* Return packed height. */
{
return packCountRowsOverflow(tg, maxCount, withLabels, FALSE);
}

static int maximumTrackHeight(struct track *tg)
/* Return the maximum track height allowed in pixels. */
{
int maxItems = maxItemsInFullTrack;
char *maxItemsString = trackDbSetting(tg->tdb, "maxItems");
if (maxItemsString != NULL)
    maxItems = sqlUnsigned(maxItemsString);
return maxItems * tl.fontHeight;
}

static int maxItemsToOverflow(struct track *tg)
/* Return the maximum number of items to allow overflow indication. */
{
int answer = maxItemsToUseOverflowDefault;
char *maxItemsString = trackDbSetting(tg->tdb, "maxItemsToOverflow");
if (maxItemsString != NULL)
    answer = sqlUnsigned(maxItemsString);

return answer;
}


int tgFixedTotalHeightOptionalOverflow(struct track *tg, enum trackVisibility vis, 
			       int lineHeight, int heightPer, boolean allowOverflow)
/* Most fixed height track groups will use this to figure out the height 
 * they use. */
{
int rows;
double maxHeight = maximumTrackHeight(tg);
int itemCount = slCount(tg->items);
int maxItemsToUseOverflow = maxItemsToOverflow(tg);
tg->heightPer = heightPer;
tg->lineHeight = lineHeight;

/* Note that the maxCount variable passed to packCountRowsOverflow() 
   is tied to the maximum height allowed for a track and influences
   decisions about when to squish, dense, or overflow a track.

   If doing overflow try to pack all the items into the maxHeight area
   or put all the overflow into the last row. If not doing overflow
   allow the track enough rows to go over the maxHeight (thus if the
   spaceSaver fills up the total height will be more than maxHeight).
*/

switch (vis)
    {
    case tvFull:
	rows = slCount(tg->items);
	break;
    case tvPack:
	{
	if(allowOverflow && itemCount < maxItemsToUseOverflow)
	    rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight), TRUE, allowOverflow);
	else
	    rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight)+1, TRUE, FALSE);
	break;
	}
    case tvSquish:
        {
	tg->heightPer = heightPer/2;
	if ((tg->heightPer & 1) == 0)
	    tg->heightPer -= 1;
	tg->lineHeight = tg->heightPer + 1;
	if(allowOverflow && itemCount < maxItemsToUseOverflow)
	    rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight), FALSE, allowOverflow);
	else
	    rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight)+1, FALSE, FALSE);
	break;
	}
    case tvDense:
    default:
        rows = 1;
	break;
    }
tg->height = rows * tg->lineHeight;
return tg->height;
}


int tgFixedTotalHeightNoOverflow(struct track *tg, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height 
 * they use. */
{
return tgFixedTotalHeightOptionalOverflow(tg,vis, tl.fontHeight+1, tl.fontHeight, FALSE);
}

int tgFixedTotalHeightUsingOverflow(struct track *tg, enum trackVisibility vis)
/* Returns how much height this track will use, tries to pack overflow into
  last row to avoid being more than maximumTrackHeight(). */
{
int height = tgFixedTotalHeightOptionalOverflow(tg, vis, tl.fontHeight+1, tl.fontHeight, TRUE);
return height;
}

void changeTrackVis(struct group *groupList, char *groupTarget, 
        int changeVis, boolean ifVisible)
/* Change track visibilities. If groupTarget is 
 * NULL then set visibility for tracks in all groups.  Otherwise,
 * just set it for the given group.  If vis is -2, then visibility is
 * unchanged.  If -1 then set visibility to default, otherwise it should 
 * be tvHide, tvDense, etc. The ifVisible flag when set, causes only
 * visibility to change only for non-hidden tracks.
 * If we are going back to default visibility, then reset the track
 * ordering also. */
{
struct group *group;
if (changeVis == -2)
    return;
for (group = groupList; group != NULL; group = group->next)
    {
    struct trackRef *tr;
    if (groupTarget == NULL || sameString(group->name,groupTarget))
        {
        static char pname[512];
        static char gname[512];
        /* if default vis then reset group priority */
        if (changeVis == -1)
            {
            group->priority = group->defaultPriority;
            }
	for (tr = group->trackList; tr != NULL; tr = tr->next)
	    {
	    struct track *track = tr->track;
	    if (changeVis == -1)
                {
	        track->visibility = track->tdb->visibility;
                /* set the track priority back to the default value */
                safef(pname, sizeof(pname), "%s.priority",track->mapName);
                cartRemove(cart, pname);
                track->priority = track->defaultPriority;
                if (track->defaultGroupName != NULL && 
                        differentString(track->groupName, track->defaultGroupName))
                    {
                    safef(gname, sizeof(gname), "%s.group",track->mapName);
                    cartRemove(cart, gname);
                    track->groupName = cloneString(track->defaultGroupName);
                    }
                }
            else if (track->visibility != tvHide || !ifVisible)
                track->visibility = changeVis;
	    cartSetString(cart, track->mapName, 
	    	hStringFromTv(track->visibility));
	    }
	}
    }
}

char *dnaInWindow()
/* This returns the DNA in the window, all in lower case. */
{
static struct dnaSeq *seq = NULL;
if (seq == NULL)
    seq = hDnaFromSeq(chromName, winStart, winEnd, dnaLower);
return seq->dna;
}


int orientFromChar(char c)
/* Return 1 or -1 in place of + or - */
{
if (c == '-')
    return -1;
if (c == '+')
    return 1;
return 0;
}

int trackOffsetX()
/* Return x offset where track display proper begins. */
{
int x = gfxBorder;
if (withLeftLabels)
    x += tl.leftLabelWidth + gfxBorder;
return x;
}


char charFromOrient(int orient)
/* Return + or - in place of 1 or -1 */
{
if (orient < 0)
    return '-';
if (orient > 0)
    return '+';
return '.';
}



static struct dyString *uiStateUrlPart(struct track *toggleGroup)
/* Return a string that contains all the UI state in CGI var
 * format.  If toggleGroup is non-null the visibility of that
 * track will be toggled in the string. */
{
struct dyString *dy = newDyString(512);
struct track *tg;

dyStringPrintf(dy, "%s=%u", cartSessionVarName(), cartSessionId(cart));
for (tg = trackList; tg != NULL; tg = tg->next)
    {
    int vis = tg->visibility;
    if (tg == toggleGroup)
	{
	char *encodedMapName = cgiEncode(tg->mapName);
	if (vis == tvDense)
	    {    
	    if (tg->canPack)
		vis = tvPack;
	    else
		vis = tvFull;
	    }
	else if (vis == tvFull || vis == tvPack || vis == tvSquish)
	    vis = tvDense;
	dyStringPrintf(dy, "&%s=%s", encodedMapName, hStringFromTv(vis));
	freeMem(encodedMapName);
	}
    }
return dy;
}

void mapStatusMessage(char *format, ...)
/* Write out stuff that will cause a status message to
 * appear when the mouse is over this box. */
{
va_list(args);
va_start(args, format);
hPrintf(" TITLE=\"");
hvPrintf(format, args);
hPutc('"');
va_end(args);
}

static void mapBoxUi(int x, int y, int width, int height,
                                char *name, char *shortLabel)
/* Print out image map rectangle that invokes hgTrackUi. */
{
char *encodedName = cgiEncode(name);

hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s?%s=%u&c=%s&g=%s\"", hgTrackUiName(), cartSessionVarName(),
                         cartSessionId(cart), chromName, encodedName);
mapStatusMessage("%s controls", shortLabel);
hPrintf(">\n");
freeMem(encodedName);
}

void mapBoxTrackUi(int x, int y, int width, int height, struct track *tg)
/* Print out image map rectangle that invokes hgTrackUi for track. */
{
mapBoxUi(x, y, width, height, tg->mapName, tg->shortLabel);
}

static void mapBoxToggleComplement(int x, int y, int width, int height, 
	struct track *toggleGroup, char *chrom,
	int start, int end, char *message)
/*print out a box along the DNA bases that toggles a cart variable
 * "complement" to complement the DNA bases at the top by the ruler*/
{
struct dyString *ui = uiStateUrlPart(toggleGroup);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s?complement=%d",
	hgTracksName(), !cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE));
hPrintf("&%s\"", ui->string);
freeDyString(&ui);
if (message != NULL)
    mapStatusMessage("%s", message);
hPrintf(">\n");
}

void mapBoxReinvokeExtra(int x, int y, int width, int height, 
                            struct track *toggleGroup, char *chrom,
                            int start, int end, char *message, char *extra)
/* Print out image map rectangle that would invoke this program again.
 * If toggleGroup is non-NULL then toggle that track between full and dense.
 * If chrom is non-null then jump to chrom:start-end.
 * Add extra string to the URL if it's not NULL */
{
struct dyString *ui = uiStateUrlPart(toggleGroup);

if (extra != NULL)
    {
    dyStringAppend(ui, "&");
    dyStringAppend(ui, extra);
    }
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
if (chrom == NULL)
    {
    chrom = chromName;
    start = winStart;
    end = winEnd;
    }
hPrintf("HREF=\"%s?position=%s:%d-%d",
	hgTracksName(), chrom, start+1, end);
hPrintf("&%s\"", ui->string);
freeDyString(&ui);
if (message != NULL)
    mapStatusMessage("%s", message);
hPrintf(">\n");
}

void mapBoxReinvoke(int x, int y, int width, int height, 
	struct track *toggleGroup, char *chrom,
	int start, int end, char *message)
/* Print out image map rectangle that would invoke this program again.
 * If toggleGroup is non-NULL then toggle that track between full and dense.
 * If chrom is non-null then jump to chrom:start-end. */
{
mapBoxReinvokeExtra(x, y, width, height, toggleGroup, chrom, start, end, 
                                message, NULL);
}

void mapBoxToggleVis(int x, int y, int width, int height, 
	struct track *curGroup)
/* Print out image map rectangle that would invoke this program again.
 * program with the current track expanded. */
{
char buf[256];
safef(buf, sizeof(buf), 
	"Toggle the display density of %s", curGroup->shortLabel);
mapBoxReinvoke(x, y, width, height, curGroup, NULL, 0, 0, buf);
}

void mapBoxJumpTo(int x, int y, int width, int height, 
	char *newChrom, int newStart, int newEnd, char *message)
/* Print out image map rectangle that would invoke this program again
 * at a different window. */
{
mapBoxReinvoke(x, y, width, height, NULL, newChrom, newStart, newEnd, message);
}


char *hgcNameAndSettings()
/* Return path to hgc with variables to store UI settings. */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    {
    dy = newDyString(128);
    dyStringPrintf(dy, "%s?%s", hgcName(), cartSidUrlString(cart));
    }
return dy->string;
}

void mapBoxHgcOrHgGene(int start, int end, int x, int y, int width, int height, 
	char *track, char *item, char *statusLine, char *directUrl, boolean withHgsid)
/* Print out image map rectangle that would invoke the hgc (human genome click)
 * program. */
{
int xEnd = x+width;
int yEnd = y+height;
if (x < 0) x = 0;
if (xEnd > tl.picWidth) xEnd = tl.picWidth;
if (x < xEnd)
    {
    char *encodedItem = cgiEncode(item);
    char *encodedTrack = cgiEncode(track);

    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, xEnd, yEnd);
    if (directUrl)
	{
	hPrintf("HREF=\"");
	hPrintf(directUrl, item, chromName, start, end, encodedTrack, database);
	if (withHgsid)
	     hPrintf("&%s", cartSidUrlString(cart));
	}
    else
	{
	hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
	    hgcNameAndSettings(), start, end, encodedTrack, encodedItem, 
	    chromName, winStart, winEnd, 
	    database, tl.picWidth);
	}
    if (statusLine != NULL)
	mapStatusMessage("%s", statusLine);
    hPrintf(">\n");
    freeMem(encodedItem);
    freeMem(encodedTrack);
    }
}

void mapBoxHc(int start, int end, int x, int y, int width, int height, 
	char *track, char *item, char *statusLine)
/* Print out image map rectangle that would invoke the hgc (human genome click)
 * program. */
{
mapBoxHgcOrHgGene(start, end, x, y, width, height, track, item, statusLine, NULL, FALSE);
}

boolean chromTableExists(char *tabSuffix)
/* Return true if chromosome specific version of table exists. */
{
char table[256];
sprintf(table, "%s%s", chromName, tabSuffix);
return hTableExists(table);
}

double scaleForPixels(double pixelWidth)
/* Return what you need to multiply bases by to
 * get to scale of pixel coordinates. */
{
return pixelWidth / (winEnd - winStart);
}

void drawScaledBox(struct vGfx *vg, int chromStart, int chromEnd, 
	double scale, int xOff, int y, int height, Color color)
/* Draw a box scaled from chromosome to window coordinates. 
 * Get scale first with scaleForPixels. */
{
int x1 = round((double)(chromStart-winStart)*scale) + xOff;
int x2 = round((double)(chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w < 1)
    w = 1;
vgBox(vg, x1, y, w, height, color);
}


void drawScaledBoxSample(struct vGfx *vg, 
	int chromStart, int chromEnd, double scale, 
	int xOff, int y, int height, Color color, int score)
/* Draw a box scaled from chromosome to window coordinates. */
{
int i;
int x1, x2, w;
x1 = round((double)(chromStart-winStart)*scale) + xOff;
x2 = round((double)(chromEnd-winStart)*scale) + xOff;

if (x2 >= MAXPIXELS)
    x2 = MAXPIXELS - 1;
w = x2-x1;
if (w < 1)
    w = 1;
vgBox(vg, x1, y, w, height, color);
if ((x1 >= 0) && (x1 < MAXPIXELS) && (chromEnd >= winStart) && (chromStart <= winEnd))
    {
    for (i = x1 ; i < x1+w; i++)
        {
        assert(i<MAXPIXELS);
        z = colorBin[i][color] ;  /*pick color of highest scoring alignment  for this pixel */
        colorBin[i][color] = (z > score)? z : score;
        }
    }
}


void filterItems(struct track *tg, 
    boolean (*filter)(struct track *tg, void *item),
    char *filterType)
/* Filter out items from track->itemList. */
{
struct slList *newList = NULL, *oldList = NULL, *el, *next;
boolean exclude = FALSE;
boolean color = FALSE;
enum trackVisibility vis = tvHide;

if (sameWord(filterType, "none"))
    return;

if (sameWord(filterType, "include"))
    exclude = FALSE;
else if (sameWord(filterType, "exclude"))
    exclude = TRUE;
else
    {
    color = TRUE;
    /* Important: call limitVisibility on *complete* item list, before it gets 
     * divided into new and old!  Otherwise tg->height is set incorrectly. */
    vis = limitVisibility(tg);
    }

for (el = tg->items; el != NULL; el = next)
    {
    next = el->next;
    if (filter(tg, el) ^ exclude)
        {
	slAddHead(&newList, el);
	}
    else
        {
	slAddHead(&oldList, el);
	}
    }
slReverse(&newList);
if (color)
   {
   slReverse(&oldList);
   /* Draw stuff that passes filter first in full mode, last in dense. */
   if (vis == tvDense)
       newList = slCat(oldList, newList);
   else
       newList = slCat(newList, oldList);
   }
tg->items = newList;
}

int getFilterColor(char *type, int colorIx)
/* Get color corresponding to type - MG_RED for "red" etc. */
{
if (sameString(type, "red"))
    colorIx = MG_RED;
else if (sameString(type, "green"))
    colorIx = MG_GREEN;
else if (sameString(type, "blue"))
    colorIx = MG_BLUE;
return colorIx;
}

struct track *trackNew()
/* Allocate track . */
{
struct track *tg;
AllocVar(tg);
return tg;
}

int linkedFeaturesCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return a->start - b->start;
}

char *linkedFeaturesName(struct track *tg, void *item)
/* Return name of item. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void linkedFeaturesFreeList(struct linkedFeatures **pList)
/* Free up a linked features list. */
{
struct linkedFeatures *lf;
for (lf = *pList; lf != NULL; lf = lf->next)
    {
    slFreeList(&lf->components);
    slFreeList(&lf->codons);
    }
slFreeList(pList);
}

void linkedFeaturesFreeItems(struct track *tg)
/* Free up linkedFeaturesTrack items. */
{
linkedFeaturesFreeList((struct linkedFeatures**)(&tg->items));
}

int exonSlRefCmp(const void *va, const void *vb)
/* Sort the exons put on an slRef. */
{
const struct slRef *a = *((struct slRef **)va);
const struct slRef *b = *((struct slRef **)vb);
struct simpleFeature *sfA = a->val;
struct simpleFeature *sfB = b->val;
return sfA->start - sfB->start;
}

int exonSlRefReverseCmp(const void *va, const void *vb)
/* Reverse of the exonSlRefCmp sort. */
{
return -1 * exonSlRefCmp(va, vb);
}

void linkedFeaturesMoveWinStart(int exonStart, int bufferToEdge, int newWinSize, int *pNewWinStart, int *pNewWinEnd)
/* A function used by linkedFeaturesNextPrevItem to make that function */
/* easy to read. Move the window so that the start of the exon in question */
/* is near the start of the window. */
{
*pNewWinStart = exonStart - bufferToEdge;
*pNewWinEnd = *pNewWinStart + newWinSize;
}

void linkedFeaturesMoveWinEnd(int exonEnd, int bufferToEdge, int newWinSize, int *pNewWinStart, int *pNewWinEnd)
/* A function used by linkedFeaturesNextPrevItem to make that function */
/* easy to read. Move the window so that the end of the exon in question */
/* is near the end of the browser window. */
{
*pNewWinEnd = exonEnd + bufferToEdge;
*pNewWinStart = *pNewWinEnd - newWinSize;
}

void linkedFeaturesNextPrevItem(struct track *tg, void *item, int x, int y, int w, int h, boolean next)
/* Draw a mapBox over the arrow-button on an *item already in the window*. */
/* Clicking this will do one of several things: */
{
struct linkedFeatures *lf = item;
struct simpleFeature *exons = lf->components;
struct simpleFeature *exon = exons;
int newWinSize = winEnd - winStart;
int bufferToEdge = 0.05 * newWinSize;
int newWinStart, newWinEnd;
int numExons = 0;
int exonIx = 0;
struct slRef *exonList = NULL, *ref;
while (exon != NULL)
/* Make a stupid list of exons separate from what's given. */
/* It seems like lf->components isn't necessarily sorted. */
    {
    refAdd(&exonList, exon);    
    exon = exon->next;
    }
/* Now sort it. */
if (next)
    slSort(&exonList, exonSlRefCmp);
else 
    slSort(&exonList, exonSlRefReverseCmp);
numExons = slCount(exonList);
for (ref = exonList; ref != NULL; ref = ref->next, exonIx++)
    {
    char mouseOverText[256];
    boolean bigExon = FALSE;
    exon = ref->val;
    if ((exon->end - exon->start) > (newWinSize - (2 * bufferToEdge)))
	bigExon = TRUE;
    if (next && (exon->end > winEnd))
	{
	if (exon->start < winEnd)
	    {
	    /* not an intron hanging over edge. */
	    if ((lf->tallEnd > winEnd) && (lf->tallEnd < exon->end) && (lf->tallEnd > exon->start))
		linkedFeaturesMoveWinEnd(lf->tallEnd, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	    else
		linkedFeaturesMoveWinEnd(exon->end, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	    }
	else if (bigExon)
	    linkedFeaturesMoveWinStart(exon->start, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	else
	    linkedFeaturesMoveWinEnd(exon->end, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	safef(mouseOverText, sizeof(mouseOverText), "Next Feature (%d/%d)", exonIx+1, numExons);
	mapBoxJumpTo(x, y, w, h, chromName, newWinStart, newWinEnd, mouseOverText);
	break;
	}
    else if (!next && (exon->start < winStart))
	{
	if (exon->end > winStart)
	    {
	    /* not an inron hanging over the edge. */
	    if ((lf->tallStart < winStart) && (lf->tallStart > exon->start) && (lf->tallStart < exon->end))
		linkedFeaturesMoveWinStart(lf->tallStart, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	    else
		linkedFeaturesMoveWinStart(exon->start, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	    }
	else if (bigExon)
	    linkedFeaturesMoveWinEnd(exon->end, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	else
	    linkedFeaturesMoveWinStart(exon->start, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	safef(mouseOverText, sizeof(mouseOverText), "Prev Feature (%d/%d)", numExons-exonIx, numExons);
	mapBoxJumpTo(x, y, w, h, chromName, newWinStart, newWinEnd, mouseOverText);
	break;
	}
    }    
slFreeList(&exonList);
}

void linkedFeaturesLabelNextPrevItem(struct track *tg, boolean next)
/* Default next-gene function for linkedFeatures.  Changes winStart/winEnd. */
{
int start = winStart;
int end = winEnd;
int size = winBaseCount;
int sizeWanted = size;
int bufferToEdge;
struct bed *items = NULL;
/* If there's stuff on the screen, skip past it. */
/* If not, skip to the edge of the window. */
if (next)
    {
    start = end;
    end = start + size;
    if (end > seqBaseCount)
	end = seqBaseCount;
    }
else
    {
    end = start;
    start = end - size;
    if (start < 0)
	start = 0;
    }
size = end - start;
/* Now it's time to do the search. */
for (;;)
    {
    items = hGetBedRange(tg->mapName, chromName, start, end, NULL);
    /* If we got something, or weren't able to search as big as we wanted to */
    /* (in case we're at the end of the chrom).  */
    if ((items != NULL) || (size < sizeWanted))
	{
	/* If none of these were on the original screen, we're done. */
	/* Remove the ones that were on the original screen. */
	struct bed *item;
	struct bed *goodList = NULL;
	while ((item = slPopHead(&items)) != NULL)
	    {
	    item->next = NULL;
	    if (((item->chromStart >= winStart) && (item->chromStart < winEnd)) ||
		((item->chromEnd > winStart) && (item->chromEnd <= winEnd)) ||
		((item->chromStart <= winStart) && (item->chromEnd >= winEnd)))
		bedFree(&item);
	    else
		slAddHead(&goodList, item);
	    }
	if (goodList)
	    {
	    slReverse(&goodList);
	    items = goodList;
	    break;
	    }
	}
    sizeWanted *= 2;
    if (next)
	{
	start = end;
	end += sizeWanted;
	if (end > seqBaseCount)
	    end = seqBaseCount;
	}
    else
	{
	end = start;
	start -= sizeWanted;
	if (start < 0)
	    start = 0;
	}
    size = end - start;
    }
/* Finally, we got something. */
sizeWanted = winEnd - winStart;
bufferToEdge = (int)(0.05 * (float)sizeWanted);
if (items)
    {
    if (next)
	{
	slSort(&items, bedCmp);
	if (items->chromEnd + bufferToEdge - sizeWanted < winEnd)
	    {
	    winEnd = items->chromEnd + bufferToEdge;
	    winStart = winEnd - sizeWanted;
	    }
	else if (items->chromStart + bufferToEdge - sizeWanted < winEnd)
	    {
	    winEnd = items->chromStart + bufferToEdge;
	    winStart = winEnd - sizeWanted;
	    }
	else
	    {
	    winStart = items->chromStart - bufferToEdge;
	    winEnd = winStart + sizeWanted;
	    }
	}
    else
	{
	slSort(&items, bedCmpEnd);
	slReverse(&items);
	if (items->chromStart - bufferToEdge + sizeWanted > winStart)
	    {
	    winStart = items->chromStart - bufferToEdge;
	    winEnd = winStart + sizeWanted;
	    }
        else if (items->chromEnd - bufferToEdge + sizeWanted > winStart)
	    {
	    winStart = items->chromEnd - bufferToEdge;
	    winEnd = winStart + sizeWanted;
	    }
	else 
	    {
	    winEnd = items->chromEnd + bufferToEdge;
	    winStart = winEnd - sizeWanted;
	    }
	}
    if (winEnd > seqBaseCount) 
	winEnd = seqBaseCount;
    if (winStart < 0)
	winStart = 0;
    bedFreeList(&items);
    }
else 
    warn("Sorry, no item found");
}

enum {blackShadeIx=9,whiteShadeIx=0};

void loadLinkedFeaturesWithLoaders(struct track *tg, struct slList *(*itemLoader)(char **row), 
				   struct linkedFeatures *(*lfFromWhatever)(struct slList *item),
				   void (*freeWhatever)(struct slList **pItem), char *scoreColumn, 
				   char *moreWhere, boolean (*itemFilter)(struct slList *item))
/* Make a linkedFeatures loader by providing three functions: (1) a regular */
/* item loader found in all autoSql modules, (2) a custom myStruct->linkedFeatures */
/* translating function, and (3) a function to free the the thing loaded in (1). */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeatures *lfList = NULL;
char optionScoreStr[128]; /* Option -  score filter */
int optionScore;
char extraWhere[128] ;
/* Use tg->tdb->tableName because subtracks inherit composite track's tdb 
 * by default, and the variable is named after the composite track. */
safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter",
      tg->tdb->tableName);
if ((scoreColumn != NULL) && (cartVarExists(cart, optionScoreStr)))
    {
    optionScore = cartUsualInt(cart, optionScoreStr, 0);
    if (optionScore < 0)
	{
	warn("%d is an invalid score for the filter on the %s track. Please choose a score in the valid range", optionScore, tg->tdb->tableName);
	cartRemove(cart, optionScoreStr);
	optionScore = 0;
	}
    if (moreWhere)
	safef(extraWhere, sizeof(extraWhere), "%s >= %d and %s", scoreColumn, optionScore, moreWhere);
    else
	safef(extraWhere, sizeof(extraWhere), "%s >= %d", scoreColumn, optionScore);
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, extraWhere, &rowOffset);
    }
else
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, moreWhere, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct slList *item = itemLoader(row + rowOffset);
    if ((itemFilter == NULL) || (itemFilter(item) == TRUE))
	{
	struct linkedFeatures *lf = lfFromWhatever(item);
	slAddHead(&lfList, lf);
	freeWhatever(&item);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

char *linkedFeaturesSeriesName(struct track *tg, void *item)
/* Return name of item */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->name;
}

int linkedFeaturesSeriesCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
return a->start - b->start;
}

void freeLinkedFeaturesSeries(struct linkedFeaturesSeries **pList)
/* Free up a linked features series list. */
{
struct linkedFeaturesSeries *lfs;

for (lfs = *pList; lfs != NULL; lfs = lfs->next)
    linkedFeaturesFreeList(&lfs->features);
slFreeList(pList);
}

void freeLinkedFeaturesSeriesItems(struct track *tg)
/* Free up linkedFeaturesSeriesTrack items. */
{
freeLinkedFeaturesSeries((struct linkedFeaturesSeries**)(&tg->items));
}

void linkedFeaturesToLinkedFeaturesSeries(struct track *tg)
/* Convert a linked features struct to a linked features series struct */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;

for (lf = tg->items; lf != NULL; lf = lf->next) 
    { 
    AllocVar(lfs);
    lfs->features = lf;
    lfs->grayIx = lf->grayIx;
    lfs->start = lf->start;
    lfs->end = lf->end;
    slAddHead(&lfsList, lfs);
    }
slReverse(&lfsList);
for (lfs = lfsList; lfs != NULL; lfs = lfs->next) 
    lfs->features->next = NULL;
tg->items = lfsList;
}

void linkedFeaturesSeriesToLinkedFeatures(struct track *tg)
/* Convert a linked features series struct to a linked features struct */
{
struct linkedFeaturesSeries *lfs;
struct linkedFeatures *lfList = NULL;

for (lfs = tg->items; lfs != NULL; lfs = lfs->next) 
    {
    slAddHead(&lfList, lfs->features);
    lfs->features = NULL;
    }
slReverse(&lfList);
freeLinkedFeaturesSeriesItems(tg);
tg->items = lfList;
}


Color whiteIndex()
/* Return index of white. */
{
return shadesOfGray[0];
}

Color blackIndex()
/* Return index of black. */
{
return shadesOfGray[maxShade];
}

Color grayIndex()
/* Return index of gray. */
{
return shadesOfGray[(maxShade+1)/2];
}

Color lightGrayIndex()
/* Return index of light gray. */
{
return shadesOfGray[3];
}

void makeGrayShades(struct vGfx *vg)
/* Make eight shades of gray in display. */
{
hMakeGrayShades(vg, shadesOfGray, maxShade);
shadesOfGray[maxShade+1] = MG_RED;
}

void vgMakeColorGradient(struct vGfx *vg, 
    struct rgbColor *start, struct rgbColor *end,
    int steps, Color *colorIxs)
/* Make a color gradient that goes smoothly from start
 * to end colors in given number of steps.  Put indices
 * in color table in colorIxs */
{
double scale = 0, invScale;
double invStep;
int i;
int r,g,b;

steps -= 1;	/* Easier to do the calculation in an inclusive way. */
invStep = 1.0/steps;
for (i=0; i<=steps; ++i)
    {
    invScale = 1.0 - scale;
    r = invScale * start->r + scale * end->r;
    g = invScale * start->g + scale * end->g;
    b = invScale * start->b + scale * end->b;
    colorIxs[i] = vgFindColorIx(vg, r, g, b);
    scale += invStep;
    }
}

void makeBrownShades(struct vGfx *vg)
/* Make some shades of brown in display. */
{
vgMakeColorGradient(vg, &tanColor, &brownColor, maxShade+1, shadesOfBrown);
}

void makeSeaShades(struct vGfx *vg)
/* Make some shades of blue in display. */
{
vgMakeColorGradient(vg, &lightSeaColor, &darkSeaColor, maxShade+1, shadesOfSea);
}

void makeRedGreenShades(struct vGfx *vg) 
/* Allocate the  shades of Red, Green and Blue */
{
static struct rgbColor black = {0, 0, 0};
static struct rgbColor red = {255, 0, 0};
static struct rgbColor green = {0, 255, 0};
static struct rgbColor blue = {0, 0, 255};
vgMakeColorGradient(vg, &black, &blue, EXPR_DATA_SHADES, shadesOfBlue);
vgMakeColorGradient(vg, &black, &red, EXPR_DATA_SHADES, shadesOfRed);
vgMakeColorGradient(vg, &black, &green, EXPR_DATA_SHADES, shadesOfGreen);
exprBedColorsMade = TRUE;
}

void makeLoweShades(struct vGfx *vg) 
/* Allocate the  shades of Red, Green and Blue */
{
static struct rgbColor black = {0, 0, 0};
static struct rgbColor shade1 = {120, 255, 255};
static struct rgbColor shade2 = {80,200, 255};
static struct rgbColor shade3 = {0,60, 255};
vgMakeColorGradient(vg, &black, &shade1, 11, shadesOfLowe1);
vgMakeColorGradient(vg, &black, &shade2, 11, shadesOfLowe2);
vgMakeColorGradient(vg, &black, &shade3, 11, shadesOfLowe3);

}


Color getOrangeColor()
{
return orangeColor;
}

Color getBrickColor()
{
return brickColor;
}

Color getBlueColor()
{
return blueColor;
}

Color getGreenColor()
{
return greenColor;
}

/* at windows >250Kbp, darken chrom break colors on MAF display */
#define CHROM_BREAK_DARK_COLOR_ZOOM 200000

Color getChromBreakGreenColor()
{
return (winEnd-winStart > CHROM_BREAK_DARK_COLOR_ZOOM ?
            darkGreenColor : greenColor);
}

Color getChromBreakBlueColor()
{
return (winEnd-winStart > CHROM_BREAK_DARK_COLOR_ZOOM ?
            darkBlueColor : blueColor);
}

/*	See inc/chromColors.h for color defines	*/
void makeChromosomeShades(struct vGfx *vg) 
/* Allocate the  shades of 8 colors in 3 shades to cover 24 chromosomes  */
{
    /*	color zero is for error conditions only	*/
chromColor[0] = vgFindColorIx(vg, 0, 0, 0);
chromColor[1] = vgFindColorIx(vg, CHROM_1_R, CHROM_1_G, CHROM_1_B);
chromColor[2] = vgFindColorIx(vg, CHROM_2_R, CHROM_2_G, CHROM_2_B);
chromColor[3] = vgFindColorIx(vg, CHROM_3_R, CHROM_3_G, CHROM_3_B);
chromColor[4] = vgFindColorIx(vg, CHROM_4_R, CHROM_4_G, CHROM_4_B);
chromColor[5] = vgFindColorIx(vg, CHROM_5_R, CHROM_5_G, CHROM_5_B);
chromColor[6] = vgFindColorIx(vg, CHROM_6_R, CHROM_6_G, CHROM_6_B);
chromColor[7] = vgFindColorIx(vg, CHROM_7_R, CHROM_7_G, CHROM_7_B);
chromColor[8] = vgFindColorIx(vg, CHROM_8_R, CHROM_8_G, CHROM_8_B);
chromColor[9] = vgFindColorIx(vg, CHROM_9_R, CHROM_9_G, CHROM_9_B);
chromColor[10] = vgFindColorIx(vg, CHROM_10_R, CHROM_10_G, CHROM_10_B);
chromColor[11] = vgFindColorIx(vg, CHROM_11_R, CHROM_11_G, CHROM_11_B);
chromColor[12] = vgFindColorIx(vg, CHROM_12_R, CHROM_12_G, CHROM_12_B);
chromColor[13] = vgFindColorIx(vg, CHROM_13_R, CHROM_13_G, CHROM_13_B);
chromColor[14] = vgFindColorIx(vg, CHROM_14_R, CHROM_14_G, CHROM_14_B);
chromColor[15] = vgFindColorIx(vg, CHROM_15_R, CHROM_15_G, CHROM_15_B);
chromColor[16] = vgFindColorIx(vg, CHROM_16_R, CHROM_16_G, CHROM_16_B);
chromColor[17] = vgFindColorIx(vg, CHROM_17_R, CHROM_17_G, CHROM_17_B);
chromColor[18] = vgFindColorIx(vg, CHROM_18_R, CHROM_18_G, CHROM_18_B);
chromColor[19] = vgFindColorIx(vg, CHROM_19_R, CHROM_19_G, CHROM_19_B);
chromColor[20] = vgFindColorIx(vg, CHROM_20_R, CHROM_20_G, CHROM_20_B);
chromColor[21] = vgFindColorIx(vg, CHROM_21_R, CHROM_21_G, CHROM_21_B);
chromColor[22] = vgFindColorIx(vg, CHROM_22_R, CHROM_22_G, CHROM_22_B);
chromColor[23] = vgFindColorIx(vg, CHROM_X_R, CHROM_X_G, CHROM_X_B);
chromColor[24] = vgFindColorIx(vg, CHROM_Y_R, CHROM_Y_G, CHROM_Y_B);
chromColor[25] = vgFindColorIx(vg, CHROM_M_R, CHROM_M_G, CHROM_M_B);
chromColor[26] = vgFindColorIx(vg, CHROM_Un_R, CHROM_Un_G, CHROM_Un_B);

chromosomeColorsMade = TRUE;
}

int grayInRange(int val, int minVal, int maxVal)
/* Return gray shade corresponding to a number from minVal - maxVal */
{
return hGrayInRange(val, minVal, maxVal, maxShade);
}


int percentGrayIx(int percent)
/* Return gray shade corresponding to a number from 50 - 100 */
{
return grayInRange(percent, 50, 100);
}



static int cmpLfsWhiteToBlack(const void *va, const void *vb)
/* Help sort from white to black. */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
return a->grayIx - b->grayIx;
}

static int cmpLfWhiteToBlack(const void *va, const void *vb)
/* Help sort from white to black. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
int diff = a->filterColor - b->filterColor;
if (diff == 0)
    diff = a->grayIx - b->grayIx;
return diff;
}

int linkedFeaturesCmpStart(const void *va, const void *vb)
/* Help sort linkedFeatures by starting pos. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return a->start - b->start;
}

void clippedBarbs(struct vGfx *vg, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle)
/* Draw barbed line.  Clip it to fit the window first though since
 * some barbed lines will span almost the whole chromosome, and the
 * clipping at the lower level is not efficient since we added
 * PostScript output support. */
{
int x2 = x + width;

if (barbDir == 0)
    return;

if (x < 0) x = 0;
if (x2 > vg->width) x2 = vg->width;
width = x2 - x;
if (width > 0)
    vgBarbedHorizontalLine(vg, x, y, width, barbHeight, barbSpacing, barbDir,
	    color, needDrawMiddle);
}

void innerLine(struct vGfx *vg, int x, int y, int w, Color color)
/* Draw a horizontal line of given width minus a pixel on either
 * end.  This pixel is needed for PostScript only, but doesn't
 * hurt elsewhere. */
{
if (w > 1)
   {
   /* Do some clipping here for the benefit of
    * PostScript.   Illustrator has
    * problems if you don't do this when you save
    * as web for some reason.  Can't hurt though in
    * a perfect world it would not be necessary, and
    * it's not necessary for ghostView. */
   int x1 = x+1;
   int x2 = x + w - 1;
   if (x1 < 0) x1 = 0;
   if (x2 > vg->width) x2 = vg->width;
   if (x2-x1 > 0)
       vgLine(vg, x1, y, x2, y, color);
   }
}

static void lfColors(struct track *tg, struct linkedFeatures *lf, 
        struct vGfx *vg, Color *retColor, Color *retBarbColor)
/* Figure out color to draw linked feature in. */
{
/* If this is the item that the user searched by
   make it be in red so visible. */
if(sameString(position, lf->name)) 
    {
    *retColor = *retBarbColor =  MG_RED;
    }
else if (lf->filterColor > 0)
    {
    if (lf->extra == (void *)USE_ITEM_RGB)
	{
	struct rgbColor itemRgb;
	itemRgb.r = (lf->filterColor & 0xff0000) >> 16;
	itemRgb.g = (lf->filterColor & 0xff00) >> 8;
	itemRgb.b = lf->filterColor & 0xff;
	*retColor = *retBarbColor =
		vgFindColorIx(vg, itemRgb.r, itemRgb.g, itemRgb.b);
	}
    else
	*retColor = *retBarbColor = lf->filterColor;
    }
else if (tg->itemColor)
    {
    *retColor = tg->itemColor(tg, lf, vg);
    *retBarbColor = tg->ixAltColor;
    }
else if (tg->colorShades) 
    {
    boolean isXeno = (tg->subType == lfSubXeno) 
				|| (tg->subType == lfSubChain) 
                                || startsWith("mrnaBla", tg->mapName);
    *retColor =  tg->colorShades[lf->grayIx+isXeno];
    *retBarbColor =  tg->colorShades[(lf->grayIx>>1)];
    }
else
    {
    *retColor = tg->ixColor;
    *retBarbColor = tg->ixAltColor;
    }
}

Color linkedFeaturesNameColor(struct track *tg, void *item, struct vGfx *vg)
/* Determine the color of the name for the linked feature. */
{
Color col, barbCol;
lfColors(tg, item, vg, &col, &barbCol);
return col;
}

struct simpleFeature *simpleFeatureCloneList(struct simpleFeature *list)
/* Just copies the simpleFeature list. This is good for making a copy */
/* when the codon list is made. */
{
struct simpleFeature *ret = NULL;
struct simpleFeature *cur;
for (cur = list; cur != NULL; cur = cur->next)
    {
    struct simpleFeature *newSf = NULL;
    AllocVar(newSf);
    newSf->start = cur->start;
    newSf->end = cur->end;
    newSf->qStart = cur->qStart;
    newSf->qEnd = cur->qEnd;
    newSf->grayIx = cur->grayIx;
    slAddHead(&ret, newSf);
    }
slReverse(&ret);
return ret;
}

void lfDrawSpecialGaps(struct linkedFeatures *lf,
		       int intronGap, boolean chainLines, int gapFactor,
		       struct track *tg, struct vGfx *vg, int xOff, int y,
		       double scale, Color color, Color bColor,
		       enum trackVisibility vis)
/* If applicable, draw something special for the gap following this block.
 * If intronGap has been specified, draw exon arrows only if the target gap
 * length is at least intronGap.
 * If chainLines, draw a double-line gap if both target and query have a gap 
 * (mismatching sequence). */
{
struct simpleFeature *sf = NULL;
int heightPer = tg->heightPer;
int midY = y + (heightPer>>1);
if (! ((intronGap > 0) || chainLines))
    return;
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    if (sf->next != NULL)
	{
	int s, e, qGap, tGap;
	int x1, x2, w;
	if (sf->start >= sf->next->end)
	    {
	    s = sf->next->end;
	    e = sf->start;
	    }
	else
	    {
	    s = sf->end;
	    e = sf->next->start;
	    }
	if (rangeIntersection(winStart, winEnd, s, e) <= 0)
	    continue;
	tGap = e - s;
	if (sf->qStart >= sf->next->qEnd)
	    qGap = sf->qStart - sf->next->qEnd;
	else
	    qGap = sf->next->qStart - sf->qEnd;

	x1 = round((double)((int)s-winStart)*scale) + xOff;
	x2 = round((double)((int)e-winStart)*scale) + xOff;
	w = x2 - x1;
	if (chainLines && tGap > 0)
	    {
	    /* Compensate for innerLine's lopping off of a pixel at each end: */
	    x1 -= 1;
	    w += 2;
	    /* If the gap in the target is more than gapFactor times the gap 
	     * in the query we draw only one line, otherwise two. */
	    if (qGap == 0 || (gapFactor > 0 && tGap > gapFactor * qGap))
		innerLine(vg, x1, midY, w, color);
	    else
		{
		int midY1 = midY - (heightPer>>2);
		int midY2 = midY + (heightPer>>2);
		if (chainLines && (vis == tvSquish))
		    {
		    midY1 = y;
		    midY2 = y + heightPer - 1;
		    }
		innerLine(vg, x1, midY1, w, color);
		innerLine(vg, x1, midY2, w, color);
		}
	    }
	if (intronGap && (qGap == 0) && (tGap >= intronGap))
	    {
	    clippedBarbs(vg, x1, midY, w, tl.barbHeight, tl.barbSpacing, 
			 lf->orientation, bColor, FALSE);
	    }
	}
    }
}

/* Rule of thumb for displaying chain gaps: consider a valid double-sided 
 * gap if target side is at most 5 times greater than query side. */
#define CHAIN_GAP_FACTOR 5

void linkedFeaturesDrawAt(struct track *tg, void *item,
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct linkedFeatures *lf = item; 
struct simpleFeature *sf, *components;
int heightPer = tg->heightPer;
int x1,x2;
int shortOff = heightPer/4;
int shortHeight = heightPer - 2*shortOff;
int tallStart, tallEnd, s, e, e2, s2;
Color bColor;
int intronGap = 0;
boolean chainLines = ((vis != tvDense)&&(tg->subType == lfSubChain));
int gapFactor = CHAIN_GAP_FACTOR;
boolean hideLine = ((tg->subType == lfSubChain) || 
	        ((vis == tvDense) && (tg->subType == lfSubXeno)));
boolean hideArrows = hideLine;
int midY = y + (heightPer>>1);
int w;
char *exonArrowsDense = trackDbSetting(tg->tdb, "exonArrowsDense");
boolean exonArrowsEvenWhenDense = (exonArrowsDense != NULL &&
				   !sameWord(exonArrowsDense, "off"));
boolean exonArrows = (tg->exonArrows &&
		      (vis != tvDense || exonArrowsEvenWhenDense));
boolean exonArrowsAlways = tg->exonArrowsAlways;
struct psl *psl = NULL;
struct dnaSeq *mrnaSeq = NULL;
enum baseColorDrawOpt drawOpt = baseColorDrawOff;
Color saveColor = color;
boolean indelShowDoubleInsert, indelShowQueryInsert, indelShowPolyA;

indelEnabled(cart, tg->tdb, &indelShowDoubleInsert, &indelShowQueryInsert,
	     &indelShowPolyA);
if (indelShowDoubleInsert && !hideLine)
    {
    /* If enabled and we weren't already suppressing the default line,
     * show chain-like lines (single/double gap lines) but without the 
     * chain track's gapFactor: */
    chainLines = TRUE;
    hideLine = TRUE;
    gapFactor = 0;
    }

/*if we are zoomed in far enough, look to see if we are coloring
  by codon, and setup if so.*/
if (vis != tvDense)
    {
    drawOpt = baseColorDrawSetup(vg, tg, lf, &mrnaSeq, &psl);
    if (drawOpt != baseColorDrawOff)
	exonArrows = FALSE;
    }
if ((tg->tdb != NULL) && (vis != tvDense))
    intronGap = atoi(trackDbSettingOrDefault(tg->tdb, "intronGap", "0"));

lfColors(tg, lf, vg, &color, &bColor);
if (vis == tvDense && trackDbSetting(tg->tdb, EXP_COLOR_DENSE))
    color = saveColor;

tallStart = lf->tallStart;
tallEnd = lf->tallEnd;
if ((tallStart == 0 && tallEnd == 0) && !sameWord(tg->mapName, "jaxQTL3"))
    {
    // sometimes a bed <8 will get passed off as a bed 8, tsk tsk
    tallStart = lf->start;
    tallEnd   = lf->end;
    }
x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
w = x2-x1;
if (!hideLine)
    {
    innerLine(vg, x1, midY, w, color);
    }
if (!hideArrows)
    {
    if ((intronGap == 0) && (vis == tvFull || vis == tvPack))
	{
	clippedBarbs(vg, x1, midY, w, tl.barbHeight, tl.barbSpacing, 
		 lf->orientation, bColor, FALSE);
	}
    }

components = (lf->codons && zoomedToCdsColorLevel) ? 
	      lf->codons : lf->components;
for (sf = components; sf != NULL; sf = sf->next)
    {
    s = sf->start; e = sf->end;

    /* Draw UTR portion(s) of exon, if any: */
    if (s < tallStart)
	{
	e2 = e;
	if (e2 > tallStart) e2 = tallStart;
	drawScaledBoxSample(vg, s, e2, scale, xOff, y+shortOff, shortHeight, 
            color, lf->score);
	s = e2;
	}
    if (e > tallEnd)
	{
	s2 = s;
	if (s2 < tallEnd) s2 = tallEnd; 
	drawScaledBoxSample(vg, s2, e, scale, xOff, y+shortOff, shortHeight, 
            color, lf->score);
	e = s2;
	}
    /* Draw "tall" portion of exon (or codon) */
    if (e > s)
	{
        if (drawOpt != baseColorDrawOff &&
            e + 6 >= winStart && s - 6 < winEnd &&
	    (e-s <= 3 || psl != NULL)) 
                baseColorDrawItem(tg, lf, sf->grayIx, vg, xOff, y, 
				  scale, font, s, e, heightPer, 
				  zoomedToCodonLevel, mrnaSeq, psl, 
				  drawOpt,
				  MAXPIXELS, winStart, color);
        else
            {
            drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
                                color, lf->score );

            if (exonArrowsAlways || (exonArrows &&
                /* Display barbs only if no intron is visible on the item.
                   This occurs when the exon completely spans the window,
                   or when it is the first or last intron in the feature and
                   the following/preceding intron isn't visible */
                (sf->start <= winStart || sf->start == lf->start) &&
                (sf->end >= winEnd || sf->end == lf->end)))
                    {
                    Color barbColor = vgContrastingColor(vg, color);
                    x1 = round((double)((int)s-winStart)*scale) + xOff;
                    x2 = round((double)((int)e-winStart)*scale) + xOff;
                    w = x2-x1;
                    clippedBarbs(vg, x1+1, midY, x2-x1-2, 
		    		tl.barbHeight, tl.barbSpacing, lf->orientation,
                                barbColor, TRUE);
                    }
            }
	}
    }
if ((intronGap > 0) || chainLines)
    lfDrawSpecialGaps(lf, intronGap, chainLines, gapFactor,
		      tg, vg, xOff, y, scale, color, bColor, vis);

if (vis != tvDense)
    {
    /* If highlighting differences between aligned sequence and genome when 
     * zoomed way out, this must be done in a separate pass after exons are 
     * drawn so that exons sharing the pixel don't overdraw differences. */
    if (indelShowQueryInsert || indelShowPolyA)
	baseColorOverdrawQInsert(tg, lf, vg, xOff, y, scale, heightPer,
				 mrnaSeq, psl, winStart, drawOpt,
				 indelShowQueryInsert, indelShowPolyA);
    baseColorOverdrawDiff(tg, lf, vg, xOff, y, scale, heightPer,
			  mrnaSeq, psl, winStart, drawOpt);
    baseColorDrawCleanup(lf, &mrnaSeq, &psl);
    }
}

static void lfSeriesDrawConnecter(struct linkedFeaturesSeries *lfs, 
	struct vGfx *vg, int start, int end, double scale, int xOff, int midY,
	Color color, Color bColor, enum trackVisibility vis)
/* Draw connection between two sets of linked features. */
{
if (start != -1 && !lfs->noLine)
    {
    int x1 = round((double)((int)start-winStart)*scale) + xOff;
    int x2 = round((double)((int)end-winStart)*scale) + xOff;
    int w = x2-x1;
    if (w > 0)
	{
	if (vis == tvFull || vis == tvPack) 
	    clippedBarbs(vg, x1, midY, w, tl.barbHeight, tl.barbSpacing, 
	  	lfs->orientation, bColor, TRUE);
	else
	    vgLine(vg, x1, midY, x2, midY, color);
	}
    }
}
	

void linkedFeaturesSeriesDrawAt(struct track *tg, void *item, 
        struct vGfx *vg, int xOff, int y, double scale,
	    MgFont *font, Color color, enum trackVisibility vis)
/* Draw a linked features series item at position. */
{
struct linkedFeaturesSeries *lfs = item;
struct linkedFeatures *lf;
Color bColor;
int midY = y + (tg->heightPer>>1);
int prevEnd = lfs->start;
int saveColor = color;

if ((lf = lfs->features) == NULL)
    return;
if (sameString(tg->tdb->type, "coloredExon"))
    {
    color = blackIndex();
    bColor = color;
    }
else 
    lfColors(tg, lf, vg, &color, &bColor);
if (vis == tvDense && trackDbSetting(tg->tdb, EXP_COLOR_DENSE))
    color = saveColor;
for (lf = lfs->features; lf != NULL; lf = lf->next)
    {
    lfSeriesDrawConnecter(lfs, vg, prevEnd, lf->start, scale, xOff, midY,
        color, bColor, vis);
    prevEnd = lf->end;
    linkedFeaturesDrawAt(tg, lf, vg, xOff, y, scale, font, color, vis);
    if(tg->mapsSelf) 
	{
	int x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
	int x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
	int w = x2-x1;
	tg->mapItem(tg, lf, lf->name, lf->start, lf->end, x1, y, w, tg->heightPer);
	}
    }
lfSeriesDrawConnecter(lfs, vg, prevEnd, lfs->end, scale, xOff, midY, 
	color, bColor, vis);
}

static void clearColorBin()
/* Clear structure which keeps track of color of highest scoring
 * structure at each pixel. */
{
memset(colorBin, 0, MAXPIXELS * sizeof(colorBin[0]));
}

void itemPixelPos(struct track *tg, void *item, int xOff, double scale, 
     int *retS, int *retE, int *retX1, int *retX2)
/* Figure out pixel position of item. */
{
int s = tg->itemStart(tg, item);
int e = tg->itemEnd(tg, item);
*retS = s;
*retE = e;
*retX1 = round((s - winStart)*scale) + xOff;
*retX2 = round((e - winStart)*scale) + xOff;
}

boolean highlightItem(struct track *tg, void *item)
/* Should this item be highlighted? */
{
char *mapName = NULL;
char *name = NULL;
char *chp;
boolean highlight = FALSE;
mapName = tg->mapItemName(tg, item);
name = tg->mapItemName(tg, item);

/* special process for KG, because of "hgg_prot" piggy back */
if (sameWord(tg->mapName, "knownGene"))
    {
    mapName = cloneString(mapName);
    chp = strstr(mapName, "&hgg_prot");
    if (chp != NULL) *chp = '\0';
    }

/* Only highlight if names are in the hgFindMatches hash with
   a 1. */
highlight = (hgFindMatches != NULL &&
	     ( hashIntValDefault(hgFindMatches, name, 0) == 1 ||
	       hashIntValDefault(hgFindMatches, mapName, 0) == 1));
return highlight;
}

double scaleForWindow(double width, int seqStart, int seqEnd)
/* Return the scale for the window. */
{
return width / (seqEnd - seqStart);
}

boolean nextItemCompatible(struct track *tg)
/* Check to see if we draw nextPrev item buttons on a track. */
{
return (withNextExonArrows && tg->nextItemButtonable && tg->nextPrevItem);
}

void genericDrawNextItemStuff(struct track *tg, struct vGfx *vg, enum trackVisibility vis, struct slList *item, 
			      int x2, int textX, int y, int heightPer,
			      boolean snapLeft, Color color, char *name, char *directUrl, boolean withHgsid)
/* After the item is drawn in genericDrawItems, draw next/prev item related */
/* buttons and the corresponding mapboxes. */
{
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int buttonW = heightPer-1 + 2*NEXT_ITEM_ARROW_BUFFER;
int s = tg->itemStart(tg, item);
int e = tg->itemEnd(tg, item);
boolean rButton = FALSE;
boolean lButton = FALSE;
/* Draw the actual triangles.  These are always at the edge of the window. */
if (s < winStart)
    {
    lButton = TRUE;
    vgNextItemButton(vg, insideX + NEXT_ITEM_ARROW_BUFFER, y, 
		     heightPer-1, heightPer-1, color, MG_WHITE, FALSE);
    }
if (e > winEnd)
    {
    rButton = TRUE;
    vgNextItemButton(vg, insideX + insideWidth - NEXT_ITEM_ARROW_BUFFER - heightPer, 
		     y, heightPer-1, heightPer-1, color, MG_WHITE, TRUE);
    }
/* If we're in pack, there's some crazy logic. */
if (vis == tvPack)
    {
    int w = x2-textX;
    if (lButton)
	{
	mapBoxHgcOrHgGene(s, e, textX, y, insideX-textX, heightPer, tg->mapName, 
			  tg->mapItemName(tg, item), name, directUrl, withHgsid);
	tg->nextPrevItem(tg, item, insideX, y, buttonW, heightPer, FALSE);
	if (rButton)
	    {
	    mapBoxHgcOrHgGene(s, e, insideX + buttonW, y, x2 - (insideX + 2*buttonW), heightPer, tg->mapName, 
			      tg->mapItemName(tg, item), name, directUrl, withHgsid);				    
	    tg->nextPrevItem(tg, item, x2-buttonW, y, buttonW, heightPer, TRUE);			    
	    }
	else 
	    mapBoxHgcOrHgGene(s, e, insideX + buttonW, y, x2 - (insideX + buttonW), heightPer, tg->mapName, 
			      tg->mapItemName(tg, item), name, directUrl, withHgsid);
	}
    else if (snapLeft && rButton)
	/* This is a special case where there's a next-item button, NO */
	/* prev-item button, AND the gene name is drawn left of the browser window. */
	{
	mapBoxHgcOrHgGene(s, e, textX, y, x2 - buttonW - textX, heightPer, tg->mapName, 
			  tg->mapItemName(tg, item), name, directUrl, withHgsid);
	tg->nextPrevItem(tg, item, x2-buttonW, y, buttonW, heightPer, TRUE);			
	}
    else if (rButton)
	{
	mapBoxHgcOrHgGene(s, e, textX, y, w - buttonW, heightPer, tg->mapName, 
			  tg->mapItemName(tg, item), name, directUrl, withHgsid);
	tg->nextPrevItem(tg, item, x2-buttonW, y, buttonW, heightPer, TRUE);			
	}
    else
	mapBoxHgcOrHgGene(s, e, textX, y, w, heightPer, tg->mapName, 
			  tg->mapItemName(tg, item), name, directUrl, withHgsid);
    }
/* Full mode is a little easier to deal with. */
else if (vis == tvFull)
    {
    int geneMapBoxX = insideX;
    int geneMapBoxW = insideWidth;
    /* Draw the first gene mapbox, in the left margin. */
    mapBoxHgcOrHgGene(s, e, trackPastTabX, y, insideX - trackPastTabX, heightPer, 
		      tg->mapName, tg->mapItemName(tg, item), name, directUrl, 
		      withHgsid);
    /* Make the button mapboxes. */
    if (lButton)
	tg->nextPrevItem(tg, item, insideX, y, buttonW, heightPer, FALSE);
    if (rButton)
	tg->nextPrevItem(tg, item, insideX + insideWidth - buttonW, y, buttonW, heightPer, TRUE);
    /* Depending on which button mapboxes we drew, draw the remaining mapbox. */
    if (lButton && rButton)
	{
	geneMapBoxX += buttonW;
	geneMapBoxW -= 2 * buttonW;
	}
    else if (lButton)
	{
	geneMapBoxX += buttonW;
	geneMapBoxW -= buttonW;
	}
    else if (rButton)
	geneMapBoxW -= buttonW;
    mapBoxHgcOrHgGene(s, e, geneMapBoxX, y, geneMapBoxW, heightPer, tg->mapName, 
		      tg->mapItemName(tg, item), name, directUrl, withHgsid);
    }
}

void genericDrawItems(struct track *tg, 
        int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw generic item list.  Features must be fixed height
 * and tg->drawItemAt has to be filled in. */
{
double scale = scaleForWindow(width, seqStart, seqEnd);
int lineHeight = tg->lineHeight;
int heightPer = tg->heightPer;
int y;
boolean withLabels = (withLeftLabels && vis == tvPack && !tg->drawName);
char *directUrl = trackDbSetting(tg->tdb, "directUrl");
boolean withHgsid = (trackDbSetting(tg->tdb, "hgsid") != NULL);

if (vis == tvPack || vis == tvSquish)
    {
    struct spaceSaver *ss = tg->ss;
    struct spaceNode *sn;
    /* These variables keep track of state if there are
       too many items and there is going to be an overflow row. */
    int maxHeight = maximumTrackHeight(tg);
    int overflowRow = (maxHeight - tl.fontHeight +1) / lineHeight;
    int overflowCount = 0;
    boolean overflowDrawn = FALSE;
    char nameBuff[128];
    boolean origWithLabels = withLabels;
    enum trackVisibility origVis = tg->limitedVis;
    int origLineHeight = lineHeight;
    int origHeightPer = heightPer;

    vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
    assert(ss);

    /* Loop though and count number of entries that will 
       end up in overflow. */
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
	if(sn->row >= overflowRow)
	    overflowCount++;

    /* Loop through and draw each item individually. */
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
        {
	struct slList *item = sn->val;
	int s = tg->itemStart(tg, item);
	int e = tg->itemEnd(tg, item);
	int sClp = (s < winStart) ? winStart : s;
	int eClp = (e > winEnd)   ? winEnd   : e;
	int x1 = round((sClp - winStart)*scale) + xOff;
	int x2 = round((eClp - winStart)*scale) + xOff;
	int textX = x1;
	char *name = tg->itemName(tg, item);
	boolean drawNameInverted = FALSE;
	int origRow = sn->row;
	boolean doingOverflow = FALSE;
	boolean snapLeft = FALSE;

	if(tg->itemNameColor != NULL) 
	    color = tg->itemNameColor(tg, item, vg);

	/* If this row falls outside of the last row have to
	   change some state to paint it in the "overflow" row. */
	if(sn->row >= overflowRow)
	    {
	    doingOverflow = TRUE;
	    sn->row = overflowRow;
	    vis = tg->limitedVis = tvDense;
	    heightPer = tg->heightPer = tl.fontHeight;
	    lineHeight = tg->lineHeight = tl.fontHeight+1;
	    withLabels = FALSE;
	    }
	y = yOff + origLineHeight * sn->row;
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, color, vis);
        if (withLabels)
            {
            int nameWidth = mgFontStringWidth(font, name);
            int dotWidth = tl.nWidth/2;
	    boolean snapLeft = FALSE;
	    drawNameInverted = highlightItem(tg, item);
            textX -= nameWidth + dotWidth;
	    snapLeft = (textX < insideX);
	    /* Special tweak for expRatio in pack mode: force all labels 
	     * left to prevent only a subset from being placed right: */
	    snapLeft |= (startsWith("expRatio", tg->tdb->type));
            if (snapLeft)        /* Snap label to the left. */
		{
		textX = leftLabelX;
		vgUnclip(vg);
		vgSetClip(vg, leftLabelX, yOff, insideWidth, tg->height);
		if(drawNameInverted)
		    {
		    int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
		    vgBox(vg, boxStart, y, nameWidth+1, heightPer - 1, color);
		    vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, heightPer,
				MG_WHITE, font, name);
		    }
		else
		    vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, heightPer,
				color, font, name);
		vgUnclip(vg);
		vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
		}
            else
		{
		if(drawNameInverted)
		    {
		    vgBox(vg, textX - 1, y, nameWidth+1, heightPer-1, color);
		    vgTextRight(vg, textX, y, nameWidth, heightPer, MG_WHITE, font, name);
		    }
		else
		    vgTextRight(vg, textX, y, nameWidth, heightPer, color, font, name);
		}
            }
        if (!tg->mapsSelf && !doingOverflow)
            {
            int w = x2-textX;
	    /* Arrows? */
	    if (w > 0)
		{
		if (nextItemCompatible(tg))
		    genericDrawNextItemStuff(tg, vg, vis, item, x2, textX, y, heightPer, snapLeft, 
					     color, name, directUrl, withHgsid);
		else
		    mapBoxHgcOrHgGene(s, e, textX, y, w, heightPer, tg->mapName, 
				      tg->mapItemName(tg, item), name, directUrl, withHgsid);
		}
            }

	/* If printing things to the "overflow" row return state to original 
	   configuration and print label. */
	if(doingOverflow)
	    {
	    /* Draw label if we haven't yet. */
	    if(withLeftLabels && !overflowDrawn)
		{
		vgUnclip(vg);
		vgSetClip(vg, leftLabelX, yOff, insideWidth, tg->height);
		safef(nameBuff, sizeof(nameBuff), "Last Row: %d", overflowCount);
		mgFontStringWidth(font, nameBuff);
		vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, lineHeight, 
			    color, font, nameBuff);
		vgUnclip(vg);
		vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
		overflowDrawn = TRUE;
		}
	    /* Put state back to original state. */
	    withLabels = origWithLabels;
	    sn->row = origRow;
	    vis = tg->limitedVis = origVis;
	    heightPer = tg->heightPer = origHeightPer;
	    lineHeight = tg->lineHeight = origLineHeight;
	    }
        }
    vgUnclip(vg);
    }
else
    {
    boolean isFull = (vis == tvFull);
    struct slList *item;
    y = yOff;
    for (item = tg->items; item != NULL; item = item->next)
	{
	if(tg->itemColor != NULL) 
	    color = tg->itemColor(tg, item, vg);
	tg->drawItemAt(tg, item, vg, xOff, y, scale, font, color, vis);
	if (isFull) 
	    {
	    /* The doMapItems will make the mapboxes normally but make */
	    /* them here if we're drawing nextItem buttons. */
	    if (nextItemCompatible(tg))
		genericDrawNextItemStuff(tg, vg, vis, item, -1, -1, y, heightPer, FALSE, 
					 color, tg->itemName(tg, item), directUrl, withHgsid);
	    y += lineHeight;
	    }
	} 
    }
}

static void linkedFeaturesSeriesDraw(struct track *tg, 
	int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
{
clearColorBin();
if (vis == tvDense && tg->colorShades)
    slSort(&tg->items, cmpLfsWhiteToBlack);
genericDrawItems(tg, seqStart, seqEnd, vg, xOff, yOff, width, 
	font, color, vis);
}

void linkedFeaturesDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
{
clearColorBin();
if (vis == tvDense && tg->colorShades)
    slSort(&tg->items, cmpLfWhiteToBlack);
genericDrawItems(tg, seqStart, seqEnd, vg, xOff, yOff, width, 
	font, color, vis);
}

void incRange(UBYTE *start, int size)
/* Add one to range of bytes, taking care to not overflow. */
{
int i;
UBYTE b;
for (i=0; i<size; ++i)
    {
    b = start[i];
    if (b < 254)
	start[i] = b+2;
    }
}

void resampleBytes(UBYTE *s, int sct, UBYTE *d, int dct)
/* Shrink or stretch an line of bytes. */
{
#define WHOLESCALE 256
if (sct > dct)	/* going to do some averaging */
	{
	int i;
	int j, jend, lj;
	long lasts, ldiv;
	long acc, div;
	long t1,t2;

	ldiv = WHOLESCALE;
	lasts = s[0];
	lj = 0;
	for (i=0; i<dct; i++)
		{
		acc = lasts*ldiv;
		div = ldiv;
		t1 = (i+1)*(long)sct;
		jend = t1/dct;
		for (j = lj+1; j<jend; j++)
			{
			acc += s[j]*WHOLESCALE;
			div += WHOLESCALE;
			}
		t2 = t1 - jend*(long)dct;
		lj = jend;
		lasts = s[lj];
		if (t2 == 0)
			{
			ldiv = WHOLESCALE;
			}
		else
			{
			ldiv = WHOLESCALE*t2/dct;
			div += ldiv;
			acc += lasts*ldiv;
			ldiv = WHOLESCALE-ldiv;
			}
		*d++ = acc/div;
		}
	}
else if (dct == sct)	/* they's the same */
	{
	while (--dct >= 0)
		*d++ = *s++;
	}
else if (sct == 1)
	{
	while (--dct >= 0)
		*d++ = *s;
	}
else/* going to do some interpolation */
	{
	int i;
	long t1;
	long p1;
	long err;
	int dct2;

	dct -= 1;
	sct -= 1;
	dct2 = dct/2;
	t1 = 0;
	for (i=0; i<=dct; i++)
		{
		p1 = t1/dct;
		err =  t1 - p1*dct;
		if (err == 0)
			*d++ = s[p1];
		else
			*d++ = (s[p1]*(dct-err)+s[p1+1]*err+dct2)/dct;
		t1 += sct;
		}
	}
}

void grayThreshold(UBYTE *pt, int count)
/* Convert from 0-4 representation to gray scale rep. */
{
UBYTE b;
int i;

for (i=0; i<count; ++i)
    {
    b = pt[i];
    if (b == 0)
	pt[i] = shadesOfGray[0];
    else if (b == 1)
	pt[i] = shadesOfGray[2];
    else if (b == 2)
	pt[i] = shadesOfGray[4];
    else if (b == 3)
	pt[i] = shadesOfGray[6];
    else if (b >= 4)
	pt[i] = shadesOfGray[9];
    }
}

static void countBaseRangeUse(int x1, int x2, int width, UBYTE *useCounts)
/* increment base use counts for a pixels from a feature */
{
if (x1 < 0)
    x1 = 0;
if (x2 >= width)
    x2 = width-1;
int w = x2-x1;
if (w >= 0)
    {
    if (w == 0)
        w = 1;
    incRange(useCounts+x1, w); 
    }
}

static void countLinkedFeaturesBaseUse(struct linkedFeatures *lf, int width, int baseWidth,
                                       UBYTE *useCounts, UBYTE *gapUseCounts)
/* increment base use counts for a set of linked features */
{
/* Performence-sensitive code.  Most of the overhead is in the mapping of base
 * to pixel.  This was change from using roundingScale() to doing it all in
 * floating point, with the divide take out of the loop. To avoid adding more
 * overhead when rendering gaps, the translation to pixel coordinates is done
 * here, and then we save the previous block coordinates for use in marking
 * the gap. */
struct simpleFeature *sf;
double scale = ((double)width)/((double)baseWidth);
int x1 = -1, x2 = -1, prevX1 = -1, prevX2 = -1;  /* pixel coords */
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    x1 = round(((double)(sf->start-winStart)) * scale);
    x2 = round(((double)(sf->end-winStart)) * scale);
    /* need to adjust x1/x2 before saving as prevX1/prevX2 */
    if (x1 < 0)
        x1 = 0;
    if (x2 >= width)
        x2 = width-1;
    countBaseRangeUse(x1, x2, width, useCounts);
    /* line from previous block to this block, however blocks seem to be reversed
     * sometimes, not sure why, so just check. */
    if ((gapUseCounts != NULL) && (prevX1 >= 0))
        countBaseRangeUse(((prevX2 < x1) ? prevX2: x1),
                          ((prevX2 < x1) ? x2-1: prevX1),
                          width, gapUseCounts);

    prevX1 = x1;
    prevX2 = x2;
    }
/* last gap */
if ((gapUseCounts != NULL) && (prevX1 >= 0))
    countBaseRangeUse(((prevX2 < x1) ? prevX2: x1),
                      ((prevX2 < x1) ? x2-1: prevX1),
                      width, gapUseCounts);
}

static void countTrackBaseUse(struct track *tg, int width, int baseWidth,
                              UBYTE *useCounts, UBYTE *gapUseCounts)
/* increment base use counts for a track */
{
struct linkedFeatures *lf;
for (lf = tg->items; lf != NULL; lf = lf->next)
    countLinkedFeaturesBaseUse(lf, width, baseWidth, useCounts, gapUseCounts);

if (gapUseCounts != NULL)
    {
    /* don't overwrite other exons with lighter intron lines */
    int i;
    for (i = 0; i < width; i++)
        {
        if (useCounts[i] > 0)
            gapUseCounts[i] = 0;
        }
    }
}

static void linkedFeaturesDrawAverage(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense items doing color averaging items. */
{
int baseWidth = seqEnd - seqStart;
UBYTE *useCounts, *gapUseCounts = NULL;
int lineHeight = mgFontLineHeight(font);

AllocArray(useCounts, width);
/* limit adding gap lines to <= 25mb to improve performance */
if (baseWidth <= 25000000)
    AllocArray(gapUseCounts, width);
countTrackBaseUse(tg, width, baseWidth, useCounts, gapUseCounts);

grayThreshold(useCounts, width);
vgVerticalSmear(vg,xOff,yOff,width,lineHeight,useCounts,TRUE);
freeMem(useCounts);
if (gapUseCounts != NULL)
    {
    int midY = yOff + (tg->heightPer>>1);
    grayThreshold(gapUseCounts, width);
    vgVerticalSmear(vg,xOff,midY,width,1,gapUseCounts,TRUE);
    freeMem(gapUseCounts);
    }
}

void linkedFeaturesAverageDense(struct track *tg, 
	int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense linked features items. */
{
if (vis == tvDense)
    linkedFeaturesDrawAverage(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
else
    linkedFeaturesDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
}

int lfCalcGrayIx(struct linkedFeatures *lf)
/* Calculate gray level from components. */
{
struct simpleFeature *sf;
int count = 0;
int total = 0;

for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    ++count;
    total += sf->grayIx;
    }
if (count == 0)
    return whiteShadeIx;
return (total+(count>>1))/count;
}

void linkedFeaturesBoundsAndGrays(struct linkedFeatures *lf)
/* Calculate beginning and end of lf from components, etc. */
{
struct simpleFeature *sf;

slReverse(&lf->components);
if ((sf = lf->components) != NULL)
    {
    int start = sf->start;
    int end = sf->end;

    for (sf = sf->next; sf != NULL; sf = sf->next)
	{
	if (sf->start < start)
	    start = sf->start;
	if (sf->end > end)
	    end = sf->end;
	}
    lf->start = lf->tallStart = start;
    lf->end = lf->tallEnd = end;
    }
lf->grayIx = lfCalcGrayIx(lf);
}

int linkedFeaturesItemStart(struct track *tg, void *item)
/* Return start chromosome coordinate of item. */
{
struct linkedFeatures *lf = item;
return lf->start;
}

int linkedFeaturesItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item. */
{
struct linkedFeatures *lf = item;
return lf->end;
}

void linkedFeaturesMethods(struct track *tg)
/* Fill in track methods for linked features. */
{
tg->freeItems = linkedFeaturesFreeItems;
tg->drawItems = linkedFeaturesDraw;
tg->drawItemAt = linkedFeaturesDrawAt;
tg->itemName = linkedFeaturesName;
tg->mapItemName = linkedFeaturesName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesItemStart;
tg->itemEnd = linkedFeaturesItemEnd;
tg->itemNameColor = linkedFeaturesNameColor;
tg->nextPrevItem = linkedFeaturesNextPrevItem;
tg->labelNextPrevItem = linkedFeaturesLabelNextPrevItem;
}

int linkedFeaturesSeriesItemStart(struct track *tg, void *item)
/* Return start chromosome coordinate of item. */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->start;
}

int linkedFeaturesSeriesItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item. */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->end;
}

void linkedFeaturesSeriesMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
tg->freeItems = freeLinkedFeaturesSeriesItems;
tg->drawItems = linkedFeaturesSeriesDraw;
tg->drawItemAt = linkedFeaturesSeriesDrawAt;
tg->itemName = linkedFeaturesSeriesName;
tg->mapItemName = linkedFeaturesSeriesName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesSeriesItemStart;
tg->itemEnd = linkedFeaturesSeriesItemEnd;
}

struct track *linkedFeaturesTg()
/* Return generic track for linked features. */
{
struct track *tg = trackNew();
linkedFeaturesMethods(tg);
tg->colorShades = shadesOfGray;
return tg;
}

struct linkedFeatures *lfFromBedExtra(struct bed *bed, int scoreMin, 
	int scoreMax)
/* Return a linked feature from a (full) bed. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(bed->score, scoreMin, scoreMax);
int *starts = bed->chromStarts, start;
int *sizes = bed->blockSizes;
int blockCount = bed->blockCount, i;

assert(starts != NULL && sizes != NULL && blockCount > 0);

AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, bed->name, sizeof(lf->name));
lf->orientation = orientFromChar(bed->strand[0]);
for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    start = starts[i] + bed->chromStart;
    sf->start = start;
    sf->end = start + sizes[i];
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
linkedFeaturesBoundsAndGrays(lf);
lf->tallStart = bed->thickStart;
lf->tallEnd = bed->thickEnd;
return lf;
}

struct linkedFeaturesSeries *lfsFromColoredExonBed(struct bed *bed)
/* Convert a single BED 14 thing into a special linkedFeaturesSeries */
/* where each linkedFeatures is a colored block. */
{
struct linkedFeaturesSeries *lfs;
struct linkedFeatures *lfList = NULL;
int *starts = bed->chromStarts;
int *sizes = bed->blockSizes;
int blockCount = bed->blockCount, i;
int expCount = bed->expCount;
if (expCount != blockCount)
    errAbort("bed->expCount != bed->blockCount");
AllocVar(lfs);
lfs->name = cloneString(bed->name);
lfs->start = bed->chromStart;
lfs->end = bed->chromEnd;
lfs->orientation = orientFromChar(bed->strand[0]);
lfs->grayIx = grayInRange(bed->score, 0, 1000);
for (i = 0; i < blockCount; i++)
    {
    struct linkedFeatures *lf;
    struct simpleFeature *sf;
    AllocVar(lf);
    strncpy(lf->name, bed->name, sizeof(lf->name));
    lf->start = starts[i] + bed->chromStart;
    lf->end = lf->start + sizes[i];
    AllocVar(sf);
    sf->start = lf->start;
    sf->end = lf->end;
    lf->orientation = lfs->orientation;
    lf->components = sf;
    lf->grayIx = lfs->grayIx;
    /* Do some logic for thickStart/thickEnd. */
    lf->tallStart = lf->start;
    lf->tallEnd = lf->end;
    if ((bed->thickStart < lf->end) && (bed->thickStart >= lf->start))
	lf->tallStart = bed->thickStart;
    if ((bed->thickEnd < lf->end) && (bed->thickEnd >= lf->start))
	lf->tallEnd = bed->thickEnd;
    if (((bed->thickStart < lf->start) && (bed->thickEnd < lf->start)) ||
	((bed->thickStart > lf->end) && (bed->thickEnd > lf->end)))
	lf->tallStart = lf->end;
    /* Finally the business about the color. */
    lf->extra = (void *)USE_ITEM_RGB;
    lf->filterColor = (unsigned)bed->expIds[i];
    slAddHead(&lfList, lf);
    }
slReverse(&lfList);
lfs->features = lfList;
lfs->noLine = FALSE;
return lfs;
}

struct linkedFeatures *lfFromBed(struct bed *bed)
{
    return lfFromBedExtra(bed, 0, 1000);
}

void setTgDarkLightColors(struct track *tg, int r, int g, int b)
/* Set track color to r,g,b.  Set altColor to a lighter version
 * of the same. */
{
tg->colorShades = NULL;
tg->color.r = r;
tg->color.g = g;
tg->color.b = b;
tg->altColor.r = (r+255)/2;
tg->altColor.g = (g+255)/2;
tg->altColor.b = (b+255)/2;
}



struct linkedFeaturesSeries *lfsFromBed(struct lfs *lfsbed)
/* Create linked feature series object from database bed record */ 
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row, rest[32];
int rowOffset, i;
struct linkedFeaturesSeries *lfs; 
struct linkedFeatures *lfList = NULL, *lf; 

AllocVar(lfs);
lfs->name = cloneString(lfsbed->name);
lfs->start = lfsbed->chromStart;
lfs->end = lfsbed->chromEnd;
lfs->orientation = orientFromChar(lfsbed->strand[0]);

/* Get linked features */
for (i = 0; i < lfsbed->lfCount; i++)  
    {
    AllocVar(lf);
    sprintf(rest, "qName = '%s'", lfsbed->lfNames[i]);
    sr = hRangeQuery(conn, lfsbed->pslTable, lfsbed->chrom, 
    	lfsbed->lfStarts[i], lfsbed->lfStarts[i] + lfsbed->lfSizes[i], rest, &rowOffset);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	struct psl *psl = pslLoad(row+rowOffset);
	lf = lfFromPsl(psl, FALSE);
	slAddHead(&lfList, lf);
	pslFree(&psl);
	}
    sqlFreeResult(&sr);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
lfs->features = lfList;
return lfs;
} 

struct linkedFeaturesSeries *lfsFromBedsInRange(char *table, int start, int end, char *chromName)
/* Return linked features from range of table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeaturesSeries *lfsList = NULL, *lfs; 
char optionScoreStr[128]; /* Option -  score filter */
int optionScore;

safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", table);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0) 
    {
    char extraWhere[128];
    safef(extraWhere, sizeof(extraWhere), "score >= %d", optionScore);
    sr = hOrderedRangeQuery(conn, table, chromName, start, end,
	extraWhere, &rowOffset);
    }
else
    {
    sr = hOrderedRangeQuery(conn, table, chromName, start, end,
	NULL, &rowOffset);
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct lfs *lfsbed = lfsLoad(row+rowOffset);
    lfs = lfsFromBed(lfsbed);
    slAddHead(&lfsList, lfs);
    lfsFree(&lfsbed);
    }
slReverse(&lfsList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfsList;
}

void loadBacEndPairs(struct track *tg)
/* Load up bac end pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("bacEndPairs", winStart, winEnd, chromName);
}

static Color dbRIPColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw dbRIP item */
{
struct dbRIP *thisItem = item;

if (startsWith("Other", thisItem->polySource))
    return tg->ixAltColor;
else
    return MG_BLUE;
}

static void loadDbRIP(struct track *tg)
/*	retroposons tracks load methods	*/
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct dbRIP *loadItem, *itemList = NULL;
struct dyString *query = dyStringNew(64);
char *option = NULL;
double freqLow = sqlFloat(cartCgiUsualString(cart, ALLELE_FREQ_LOW, "0.0"));
double freqHi = sqlFloat(cartCgiUsualString(cart, ALLELE_FREQ_HI, "1.0"));
boolean needJoin = FALSE;	/* need join to polyGenotype ?	*/

/*	safety check on bad user input, they may have set them illegally
 *	in which case reset them to defaults 0.0 <= f <= 1.0
 *	This reset also happens in hgTrackUi so they will see it reset there
 *	when they go back.
 */
if (! (freqLow < freqHi))
    {
    freqLow = 0.0;
    freqHi = 1.0;
    }
if ((freqLow > 0.0) || (freqHi < 1.0))
    needJoin = TRUE;

option = cartCgiUsualString(cart, ETHNIC_GROUP, ETHNIC_GROUP_DEFAULT);
if (differentString(option,ETHNIC_GROUP_DEFAULT))
    needJoin = TRUE;

if (needJoin)
    {
    dyStringPrintf(query, "select %s.* from %s,polyGenotype where ",
	tg->mapName, tg->mapName);

    if (differentString(option,ETHNIC_GROUP_DEFAULT))
	{
	char *optionNot =
	    cartCgiUsualString(cart, ETHNIC_GROUP_EXCINC, ETHNIC_NOT_DEFAULT);
	if (sameWord(optionNot,"include"))
	    {
	    dyStringPrintf(query, "%s.name=polyGenotype.name and "
		"polyGenotype.ethnicGroup=\"%s\" and ",
		    tg->mapName, option);
	    }
	    else
	    {
	    dyStringPrintf(query, "%s.name=polyGenotype.name and "
		"polyGenotype.ethnicGroup!=\"%s\" and ",
		    tg->mapName, option);
	    }
	}
    if ((freqLow > 0.0) || (freqHi < 1.0))
	{
	    dyStringPrintf(query,
		"polyGenotype.alleleFrequency>=\"%.1f\" and "
		    "polyGenotype.alleleFrequency<=\"%.1f\" and ",
			freqLow, freqHi);
	}
    }
else
    {
    dyStringPrintf(query, "select * from %s where ", tg->mapName);
    }

hAddBinToQuery(winStart, winEnd, query);
dyStringPrintf(query,
    "chrom=\"%s\" AND chromStart<%d AND chromEnd>%d ",
    chromName, winEnd, winStart);

option = cartCgiUsualString(cart, GENO_REGION, GENO_REGION_DEFAULT);

if (differentString(option,GENO_REGION_DEFAULT))
    dyStringPrintf(query, " and genoRegion=\"%s\"", option);

option = cartCgiUsualString(cart, POLY_SOURCE, POLY_SOURCE_DEFAULT);
if (differentString(option,POLY_SOURCE_DEFAULT))
    {
    char *ucsc = "UCSC";
    char *other = "Other";
    char *which;
    if (sameWord(option,"yes"))
	which = ucsc;
    else
	which = other;
    dyStringPrintf(query, " and polySource=\"%s\"", which);
    }

option = cartCgiUsualString(cart, POLY_SUBFAMILY, POLY_SUBFAMILY_DEFAULT);
if (differentString(option,POLY_SUBFAMILY_DEFAULT))
    dyStringPrintf(query, " and polySubfamily=\"%s\"", option);

option = cartCgiUsualString(cart, dbRIP_DISEASE, DISEASE_DEFAULT);
if (differentString(option,DISEASE_DEFAULT))
    {
    if (sameWord(option,"no"))
	dyStringPrintf(query, " and disease=\"NA\"");
    else
	dyStringPrintf(query, " and disease!=\"NA\"");
    }

dyStringPrintf(query, " group by %s.name", tg->mapName);

sr = sqlGetResult(conn, dyStringCannibalize(&query));
rowOffset=1;

while ((row = sqlNextRow(sr)) != NULL)
    {
    loadItem = dbRIPLoad(row+rowOffset);
    slAddHead(&itemList, loadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&itemList, bedCmp);
tg->items = itemList;
}


static void dbRIPMethods(struct track *tg)
/* Fill in track methods for dbRIP tracks */
{
bedMethods(tg);
tg->loadItems = loadDbRIP;
tg->itemColor = dbRIPColor;
tg->itemNameColor = dbRIPColor;
tg->itemLabelColor = dbRIPColor;
}

void bacEndPairsMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadBacEndPairs;
}


void loadBacEndPairsBad(struct track *tg)
/* Load up fosmid end pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("bacEndPairsBad", winStart, winEnd, chromName);
}


void bacEndPairsBadMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadBacEndPairsBad;
}

void loadBacEndPairsLong(struct track *tg)
/* Load up BAC end pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("bacEndPairsLong", winStart, winEnd, chromName);
}


void bacEndPairsLongMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadBacEndPairsLong;
}

void loadBacEndSingles(struct track *tg)
/* Load up BAC end pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("bacEndSingles", winStart, winEnd, chromName);
}

void bacEndSinglesMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadBacEndSingles;
}

void loadFosEndPairs(struct track *tg)
/* Load up fosmid end pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("fosEndPairs", winStart, winEnd, chromName);
}

void fosEndPairsMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadFosEndPairs;
}

void loadFosEndPairsBad(struct track *tg)
/* Load up fosmid end pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("fosEndPairsBad", winStart, winEnd, chromName);
}


void fosEndPairsBadMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadFosEndPairsBad;
}

void loadFosEndPairsLong(struct track *tg)
/* Load up fosmid end pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("fosEndPairsLong", winStart, winEnd, chromName);
}


void fosEndPairsLongMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadFosEndPairsLong;
}

void loadEarlyRep(struct track *tg)
/* Load up early replication cosmid  pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("earlyRep", winStart, winEnd, chromName);
}

void earlyRepMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadEarlyRep;
}


void loadEarlyRepBad(struct track *tg)
/* Load up bad early replication pairs from table into track items. */
{
tg->items = lfsFromBedsInRange("earlyRepBad", winStart, winEnd, chromName);
}


void earlyRepBadMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadEarlyRepBad;
}

char *lfMapNameFromExtra(struct track *tg, void *item)
/* Return map name of item from extra field. */
{
struct linkedFeatures *lf = item;
return lf->extra;
}

void parseSs(char *ss, char **retPsl, char **retFa)
/* Parse out ss variable into components. */
{
static char buf[1024];
char *words[2];
int wordCount;

strcpy(buf, ss);
wordCount = chopLine(buf, words);
if (wordCount < 2)
    errAbort("Badly formated ss variable");
*retPsl = words[0];
*retFa = words[1];
}

boolean ssFilesExist(char *ss)
/* Return TRUE if both files in ss exist. */
{
char *faFileName, *pslFileName;
parseSs(ss, &pslFileName, &faFileName);
return fileExists(pslFileName) && fileExists(faFileName);
}

void loadUserPsl(struct track *tg)
/* Load up rnas from table into track items. */
{
char *ss = userSeqString;
char buf2[3*512];
char *faFileName, *pslFileName;
struct lineFile *f;
struct psl *psl;
struct linkedFeatures *lfList = NULL, *lf;
enum gfType qt, tt;
int sizeMul = 1;

parseSs(ss, &pslFileName, &faFileName);
pslxFileOpen(pslFileName, &qt, &tt, &f);
if (qt == gftProt)
    {
    setTgDarkLightColors(tg, 0, 80, 150);
    tg->colorShades = NULL;
    sizeMul = 3;
    }
tg->itemName = linkedFeaturesName;
while ((psl = pslNext(f)) != NULL)
    {
    if (sameString(psl->tName, chromName) && psl->tStart < winEnd && psl->tEnd > winStart)
	{
	lf = lfFromPslx(psl, sizeMul, TRUE, FALSE, tg);
	sprintf(buf2, "%s %s", ss, psl->qName);
	lf->extra = cloneString(buf2);
	slAddHead(&lfList, lf);
	}
    pslFree(&psl);
    }
slSort(&lfList, linkedFeaturesCmpStart);
lineFileClose(&f);
tg->items = lfList;
}

struct track *userPslTg()
/* Make track of user pasted sequence. */
{
struct track *tg = linkedFeaturesTg();
struct trackDb *tdb;
AllocVar(tdb);
tg->mapName = "hgUserPsl";
tg->canPack = TRUE;
tg->visibility = tvPack;
tg->longLabel = "Your Sequence from Blat Search";
tg->shortLabel = "Blat Sequence";
tg->loadItems = loadUserPsl;
tg->mapItemName = lfMapNameFromExtra;
tg->priority = 100;
tg->groupName = "map";
tdb->tableName = tg->mapName;
tdb->shortLabel = tg->shortLabel;
tdb->longLabel = tg->longLabel;
tdb->type = cloneString("psl");
tg->exonArrows = TRUE;
trackDbPolish(tdb);
tg->tdb = tdb;
return tg;
}

static struct simpleFeature *sfFromGenePred(struct genePred *gp, int grayIx)
/* build a list of simpleFeature objects from a genePred */
{
struct simpleFeature *sfList = NULL, *sf;
unsigned *starts = gp->exonStarts;
unsigned *ends = gp->exonEnds;
int i, blockCount = gp->exonCount;

for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    sf->start = starts[i];
    sf->end = ends[i];
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
return sfList;
}
        

static struct linkedFeatures *connectedLfFromGenePredInRangeExtra(
        struct track *tg, struct sqlConnection *conn, char *table, 
	char *chrom, int start, int end, boolean extra)
/* Return linked features from range of a gene prediction table after 
 * we have already connected to database. Optinally Set lf extra to 
 * gene pred name2, to display gene name instead of transcript ID.*/
{
struct linkedFeatures *lfList = NULL, *lf;
int grayIx = maxShade;
struct genePredReader *gpr = NULL;
struct genePred *gp = NULL;
boolean nmdTrackFilter = sameString(trackDbSettingOrDefault(tg->tdb, "nmdFilter", "off"), "on");
boolean doNmd = FALSE;
char buff[256];
enum baseColorDrawOpt drawOpt = baseColorDrawOff;
safef(buff, sizeof(buff), "hgt.%s.nmdFilter",  tg->mapName);

/* Should we remove items that appear to be targets for nonsense
 * mediated decay? */
if(nmdTrackFilter)
    doNmd = cartUsualBoolean(cart, buff, FALSE);

if (table != NULL)
    drawOpt = baseColorGetDrawOpt(tg);

if (tg->itemAttrTbl != NULL)
    itemAttrTblLoad(tg->itemAttrTbl, conn, chrom, start, end);

gpr = genePredReaderRangeQuery(conn, table, chrom, start, end, NULL);
while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    if(doNmd && genePredNmdTarget(gp))
	{
	genePredFree(&gp);
	continue;
	}
    AllocVar(lf);
    lf->grayIx = grayIx;
    strncpy(lf->name, gp->name, sizeof(lf->name));
    if (extra && gp->name2)
        lf->extra = cloneString(gp->name2);
    lf->orientation = orientFromChar(gp->strand[0]);

    if (drawOpt != baseColorDrawOff && gp->cdsStart != gp->cdsEnd)
        lf->codons = baseColorCodonsFromGenePred(chrom, lf, gp, NULL,
                (gp->optFields >= genePredExonFramesFld),
		(drawOpt != baseColorDrawDiffCodons));
   
    lf->components = sfFromGenePred(gp, grayIx);

    if (tg->itemAttrTbl != NULL)
        lf->itemAttr = itemAttrTblGet(tg->itemAttrTbl, gp->name,
                                      gp->chrom, gp->txStart, gp->txEnd);

    linkedFeaturesBoundsAndGrays(lf);

    if (gp->cdsStart >= gp->cdsEnd)
        {
        lf->tallStart = gp->txEnd;
        lf->tallEnd = gp->txEnd;
        }
    else
        {
        lf->tallStart = gp->cdsStart;
        lf->tallEnd = gp->cdsEnd;
        }
    
    slAddHead(&lfList, lf);
    genePredFree(&gp);
    }
slReverse(&lfList);
genePredReaderFree(&gpr);
return lfList;
}

struct linkedFeatures *connectedLfFromGenePredInRange(
        struct track *tg, struct sqlConnection *conn, char *table, 
	char *chrom, int start, int end)
/* Return linked features from range of a gene prediction table after 
 * we have already connected to database. */
{
return connectedLfFromGenePredInRangeExtra(tg, conn, table, chrom, 
                                                start, end, FALSE);
}

struct linkedFeatures *lfFromGenePredInRange(struct track *tg, char *table, 
	char *chrom, int start, int end)
/* Return linked features from range of a gene prediction table. */
{
struct linkedFeatures *lfList = NULL;
struct sqlConnection *conn = hAllocConn();
lfList = connectedLfFromGenePredInRange(tg, conn, table, chrom, start, end);
hFreeConn(&conn);
return lfList;
}

void abbr(char *s, char *fluff)
/* Cut out fluff from s. */
{
int len;
s = strstr(s, fluff);
if (s != NULL)
   {
   len = strlen(fluff);
   strcpy(s, s+len);
   }
}

char *genieName(struct track *tg, void *item)
/* Return abbreviated genie name. */
{
struct linkedFeatures *lf = item;
char *full = lf->name;
static char abbrev[32];

strncpy(abbrev, full, sizeof(abbrev));
abbr(abbrev, "00000");
abbr(abbrev, "0000");
abbr(abbrev, "000");
abbr(abbrev, "ctg");
abbr(abbrev, "Affy.");
return abbrev;
}

void genieAltMethods(struct track *tg)
/* Make track of full length mRNAs. */
{
tg->itemName = genieName;
}

void loadGenePredWithName2(struct track *tg)
/* Convert gene pred in window to linked feature. Include alternate name
 * in "extra" field (usually gene name)*/
{
struct sqlConnection *conn = hAllocConn();
tg->items = connectedLfFromGenePredInRangeExtra(tg, conn, tg->mapName,
                                        chromName, winStart, winEnd, TRUE);
hFreeConn(&conn);
/* filter items on selected criteria if filter is available */
filterItems(tg, genePredClassFilter, "include");
}

void lookupKnownNames(struct linkedFeatures *lfList)
/* This converts the Genie ID to the HUGO name where possible. */
{
struct linkedFeatures *lf;
char query[256];
struct sqlConnection *conn = hAllocConn();

if (hTableExists("knownMore"))
    {
    struct knownMore *km;
    struct sqlResult *sr;
    char **row;

    for (lf = lfList; lf != NULL; lf = lf->next)
	{
	sprintf(query, "select * from knownMore where transId = '%s'", lf->name);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    km = knownMoreLoad(row);
	    strncpy(lf->name, km->name, sizeof(lf->name));
	    if (km->omimId)
	        lf->extra = km;
	    else
	        knownMoreFree(&km);
	    }
	sqlFreeResult(&sr);
	}
    }
else if (hTableExists("knownInfo"))
    {
    for (lf = lfList; lf != NULL; lf = lf->next)
	{
	sprintf(query, "select name from knownInfo where transId = '%s'", lf->name);
	sqlQuickQuery(conn, query, lf->name, sizeof(lf->name));
	}
    }
hFreeConn(&conn);
}

void loadGenieKnown(struct track *tg)
/* Load up Genie known genes. */
{
tg->items = lfFromGenePredInRange(tg, "genieKnown", chromName, winStart, winEnd);
if (limitVisibility(tg) == tvFull)
    {
    lookupKnownNames(tg->items);
    }
}

Color genieKnownColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw known gene in. */
{
struct linkedFeatures *lf = item;

if (startsWith("AK.", lf->name))
    {
    static int colIx = 0;
    if (!colIx)
	colIx = vgFindColorIx(vg, 0, 120, 200);
    return colIx;
    }
else
    {
    return tg->ixColor;
    }
}

void genieKnownMethods(struct track *tg)
/* Make track of known genes. */
{
tg->loadItems = loadGenieKnown;
tg->itemName = genieName;
tg->itemColor = genieKnownColor;
}

char *hg17KgName(struct track *tg, void *item)
{
static char cat[128];
struct linkedFeatures *lf = item;
if (lf->extra != NULL) 
    {
    sprintf(cat,"%s",(char *)lf->extra);
    return cat;
    }
else 
    return lf->name;
}

char *hg17KgMapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void lookupHg17KgNames(struct linkedFeatures *lfList)
/* This converts the known gene ID to a gene symbol */
{
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn();
char *geneSymbol;
char *protDisplayId;
char *mimId;
char cond_str[256];
char *hg17KgLabel = cartUsualString(cart, "hg17Kg.label", "gene symbol");

boolean useGeneSymbol= sameString(hg17KgLabel, "gene symbol")
    || sameString(hg17KgLabel, "all");

boolean useKgId      = sameString(hg17KgLabel, "UCSC Known Gene ID")
    || sameString(hg17KgLabel, "all");

boolean useProtDisplayId = sameString(hg17KgLabel, "UniProt Display ID")
    || sameString(hg17KgLabel, "all");

boolean useMimId = sameString(hg17KgLabel, "OMIM ID")
    || sameString(hg17KgLabel, "all");

boolean useAll = sameString(hg17KgLabel, "all");
	
if (hTableExists("kgXref"))
    {
    for (lf = lfList; lf != NULL; lf = lf->next)
	{
        struct dyString *name = dyStringNew(64);
    	if (useGeneSymbol)
            {
            sprintf(cond_str, "kgID='%s'", lf->name);
            geneSymbol = sqlGetField(conn, "hg17", "kgXref", "geneSymbol", cond_str);
            if (geneSymbol != NULL)
            	{
            	dyStringAppend(name, geneSymbol);
            	if (useAll) dyStringAppendC(name, '/');
            	}
            }
    	if (useKgId)
            {
            dyStringAppend(name, lf->name);
            if (useAll) dyStringAppendC(name, '/');
	    }
    	if (useProtDisplayId)
            {
	    safef(cond_str, sizeof(cond_str), "kgID='%s'", lf->name);
            protDisplayId = sqlGetField(conn, "hg17", "kgXref", "spDisplayID", cond_str);
            dyStringAppend(name, protDisplayId);
	    }
        if (useMimId && hTableExists("refLink"))
            {
            safef(cond_str, sizeof(cond_str), "select cast(refLink.omimId as char) from kgXref,refLink where kgID = '%s' and kgXref.refseq = refLink.mrnaAcc and refLink.omimId != 0", lf->name);
            mimId = sqlQuickString(conn, cond_str);
            if (mimId)
                dyStringAppend(name, mimId);
            }
    	lf->extra = dyStringCannibalize(&name);
	}
    }
hFreeConn(&conn);
}

void loadHg17Kg(struct track *tg)
/* Load up known genes. */
{
enum trackVisibility vis = tg->visibility;
tg->items = lfFromGenePredInRange(tg, "hg17Kg", chromName, winStart, winEnd);
if (vis != tvDense)
    {
    lookupHg17KgNames(tg->items);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
limitVisibility(tg);
}

void hg17KgMethods(struct track *tg)
/* Make track of known genes. */
{
tg->loadItems 	= loadHg17Kg;
tg->itemName 	= hg17KgName;
tg->mapItemName = hg17KgMapName;
}

char *knownGeneName(struct track *tg, void *item)
{
static char cat[128];
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    {
    safef(cat, sizeof(cat), "%s",((struct knownGenesExtra *)(lf->extra))->name);
    return cat;
    }
else
    return lf->name;
}

char *knownGeneMapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
char str2[255];
struct linkedFeatures *lf = item;
/* piggy back the protein ID (hgg_prot variable) on hgg_gene variable */
safef(str2, sizeof(str2), "%s&hgg_prot=%s", lf->name, ((struct knownGenesExtra *)(lf->extra))->hgg_prot);
return(cloneString(str2));
}

void lookupKnownGeneNames(struct linkedFeatures *lfList)
/* This converts the known gene ID to a gene symbol */
{
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn();
char *geneSymbol;
char *protDisplayId;
char *mimId;
char cond_str[256];

boolean useGeneSymbol= FALSE;
boolean useKgId      = FALSE;
boolean useProtDisplayId = FALSE;
boolean useMimId = FALSE;

struct hashEl *knownGeneLabels = cartFindPrefix(cart, "knownGene.label");
struct hashEl *label;
boolean labelStarted = FALSE;
	
if (hTableExists("kgXref"))
    {
    char omimLabel[48];
    safef(omimLabel, sizeof(omimLabel), "omim%s", cartString(cart, "db"));

    if (knownGeneLabels == NULL)
        {
        useGeneSymbol = TRUE; /* default to gene name */
        /* set cart to match what doing */
        cartSetBoolean(cart, "knownGene.label.gene", TRUE);
        }

    for (label = knownGeneLabels; label != NULL; label = label->next)
        {
        if (endsWith(label->name, "gene") && differentString(label->val, "0"))
            useGeneSymbol = TRUE;
        else if (endsWith(label->name, "kgId") && differentString(label->val, "0"))
            useKgId = TRUE;
        else if (endsWith(label->name, "prot") && differentString(label->val, "0"))
            useProtDisplayId = TRUE;
        else if (endsWith(label->name, omimLabel) && differentString(label->val, "0"))
            useMimId = TRUE;
        else if (!endsWith(label->name, "gene") && 
                 !endsWith(label->name, "kgId") &&
                 !endsWith(label->name, "prot") &&
                 !endsWith(label->name, omimLabel) )
            {
            useGeneSymbol = TRUE;
            cartRemove(cart, label->name);
            }
        }

    for (lf = lfList; lf != NULL; lf = lf->next)
	{
        struct dyString *name = dyStringNew(64);
        struct knownGenesExtra *kgE;
        AllocVar(kgE);
        labelStarted = FALSE; /* reset between items */
    	if (useGeneSymbol)
            {
            sprintf(cond_str, "kgID='%s'", lf->name);
            geneSymbol = sqlGetField(conn, database, "kgXref", "geneSymbol", cond_str);
            if (geneSymbol != NULL)
            	{
            	dyStringAppend(name, geneSymbol);
            	}
            labelStarted = TRUE;
            }
    	if (useKgId)
            {
            if (labelStarted) dyStringAppendC(name, '/');
            else labelStarted = TRUE;
            dyStringAppend(name, lf->name);
	    }
    	if (useProtDisplayId)
            {
            if (labelStarted) dyStringAppendC(name, '/');
            else labelStarted = TRUE;
            if (lf->extra != NULL)
                {
                dyStringAppend(name, (char *)lf->extra);
                }
            else 
                {
	        safef(cond_str, sizeof(cond_str), "kgID='%s'", lf->name);
                protDisplayId = sqlGetField(conn, database, "kgXref", "spDisplayID", cond_str);
                dyStringAppend(name, protDisplayId);
                }
	    }
        if (useMimId && hTableExists("refLink")) 
            {
            if (labelStarted) dyStringAppendC(name, '/');
            else labelStarted = TRUE;
            safef(cond_str, sizeof(cond_str), "select cast(refLink.omimId as char) from kgXref,refLink where kgID = '%s' and kgXref.refseq = refLink.mrnaAcc and refLink.omimId != 0", lf->name);
            mimId = sqlQuickString(conn, cond_str);
            if (mimId) 
                dyStringAppend(name, mimId);
            }
        /* should this be a hash instead? */
        kgE->name = dyStringCannibalize(&name);
        kgE->hgg_prot = lf->extra;
        lf->extra = kgE;
	}
    }
hFreeConn(&conn);
}

struct linkedFeatures *stripShortLinkedFeatures(struct linkedFeatures *list)
/* Remove linked features with no tall component from list. */
{
struct linkedFeatures *newList = NULL, *el, *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (el->tallStart < el->tallEnd)
        slAddHead(&newList, el);
    }
slReverse(&newList);
return newList;
}

struct linkedFeatures *stripLinkedFeaturesNotInHash(struct linkedFeatures *list, struct hash *hash)
/* Remove linked features not in hash from list. */
{
struct linkedFeatures *newList = NULL, *el, *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (hashLookup(hash, el->name))
        slAddHead(&newList, el);
    }
slReverse(&newList);
return newList;
}

void loadKnownGene(struct track *tg)
/* Load up known genes. */
{
struct trackDb *tdb = tg->tdb;
loadGenePredWithName2(tg);
char varName[64];
safef(varName, sizeof(varName), "%s.show.noncoding", tdb->tableName);
boolean showNoncoding = cartUsualBoolean(cart, varName, TRUE);
safef(varName, sizeof(varName), "%s.show.spliceVariants", tdb->tableName);
boolean showSpliceVariants = cartUsualBoolean(cart, varName, TRUE);
if (!showNoncoding)
    tg->items = stripShortLinkedFeatures(tg->items);
if (!showSpliceVariants)
    {
    char *canonicalTable = trackDbSettingOrDefault(tdb, "canonicalTable", "knownCanonical");
    if (hTableExists(canonicalTable))
        {
	/* Create hash of items in canonical table in region. */
	struct sqlConnection *conn = hAllocConn();
	struct hash *hash = hashNew(0);
	char query[512];
	safef(query, sizeof(query), 
		"select transcript from %s where chromStart < %d && chromEnd > %d", 
		canonicalTable, winEnd, winStart);
	struct sqlResult *sr = sqlGetResult(conn, query);
	char **row;
	while ((row = sqlNextRow(sr)) != NULL)
	    hashAdd(hash, row[0], NULL);
	sqlFreeResult(&sr);
	hFreeConn(&conn);

	/* Get rid of non-canonical items. */
	tg->items = stripLinkedFeaturesNotInHash(tg->items, hash);
	hashFree(&hash);
	}
    }
lookupKnownGeneNames(tg->items);
slSort(&tg->items, linkedFeaturesCmpStart);
limitVisibility(tg);
}

Color knownGeneColorCalc(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw known gene in. */
{
struct linkedFeatures *lf = item;
int col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter;
struct rgbColor lightest;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
char cond_str[256];
char *proteinID = NULL;
char *pdbID = NULL;
char *ans = NULL;
char *refAcc = NULL;

/* color scheme:

	Black: 		If the gene has a corresponding PDB entry
	Dark blue: 	If the gene has a corresponding SWISS-PROT entry
			or has a corresponding "Reviewed" or "Validated" RefSeq entry
	Lighter blue:  	If the gene has a corresponding RefSeq entry
	Lightest blue: 	Eveything else
*/

lighter.r = (6*normal->r + 4*255) / 10;
lighter.g = (6*normal->g + 4*255) / 10;
lighter.b = (6*normal->b + 4*255) / 10;

lightest.r = (1*normal->r + 2*255) / 3;
lightest.g = (1*normal->g + 2*255) / 3;
lightest.b = (1*normal->b + 2*255) / 3;

/* set default to the lightest color */
col = vgFindColorIx(vg, lightest.r, lightest.g, lightest.b);

/* set color first according to RefSeq status (if there is a corresponding RefSeq) */
sprintf(cond_str, "name='%s' ", lf->name);
refAcc = sqlGetField(conn, database, "refGene", "name", cond_str);
if (refAcc != NULL)
    {
    if (hTableExists("refSeqStatus"))
    	{
    	sprintf(query, "select status from refSeqStatus where mrnaAcc = '%s'", refAcc);
    	sr = sqlGetResult(conn, query);
    	if ((row = sqlNextRow(sr)) != NULL)
            {
	    if (startsWith("Reviewed", row[0]) || startsWith("Validated", row[0]))
	    	{
	    	/* Use the usual color */
	    	col = tg->ixColor;
	    	}
	    else
		{ 
	    	col = vgFindColorIx(vg, lighter.r, lighter.g, lighter.b);
		}	   
	    }
	sqlFreeResult(&sr);
	}
    }
    
/* set to dark blue if there is a corresponding Swiss-Prot protein */
sprintf(cond_str, "name='%s'", (char *)(lf->name));
proteinID= sqlGetField(conn, database, "knownGene", "proteinID", cond_str);
if (proteinID != NULL && protDbName != NULL)
    {
    sprintf(cond_str, "displayID='%s' AND biodatabaseID=1 ", proteinID);
    ans= sqlGetField(conn, protDbName, "spXref2", "displayID", cond_str);
    if (ans != NULL) 
    	{
    	col = tg->ixColor;
    	}
    }

/* if a corresponding PDB entry exists, set it to black */
if (protDbName != NULL)
    {
    sprintf(cond_str, "sp='%s'", proteinID);
    pdbID= sqlGetField(conn, protDbName, "pdbSP", "pdb", cond_str);
    }

if (pdbID != NULL) 
    {
    col = MG_BLACK;
    }

hFreeConn(&conn);
return(col);
}


Color knownGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color for a known gene item - looking it up in table in
 * newer versions, and calculating it on fly in later versions. */
{
if (gotKgColor)
    {
    struct linkedFeatures *lf = item;
    int colIx = MG_BLUE;
    struct sqlConnection *conn = hAllocConn();
    char query[512];
    safef(query, sizeof(query), "select r,g,b from kgColor where kgID='%s'", 
    	lf->name);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row = sqlNextRow(sr);
    if (row != NULL)
         colIx = vgFindColorIx(vg, sqlUnsigned(row[0]), sqlUnsigned(row[1]), sqlUnsigned(row[2]));
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    return colIx;
    }
else
    return knownGeneColorCalc(tg, item, vg);
}

void knownGeneMethods(struct track *tg)
/* Make track of known genes. */
{
/* use loadGenePredWithName2 instead of loadKnownGene to pick up proteinID */
tg->loadItems   = loadKnownGene;
tg->itemName 	= knownGeneName;
tg->mapItemName = knownGeneMapName;
tg->itemColor 	= knownGeneColor;
}

char *superfamilyName(struct track *tg, void *item)
/* Return map name of the track item (used by hgc). */
{
char *name;
char *proteinName;
struct sqlConnection *conn = hAllocConn();
char conditionStr[256];

struct bed *sw = item;

// This is necessary because Ensembl kept changing their xref table definition
sprintf(conditionStr, "transcript_name='%s'", sw->name);
if (hTableExists("ensGeneXref"))
    {
    proteinName = sqlGetField(conn, database, "ensGeneXref", "translation_name", conditionStr);
    }
else if (hTableExists("ensemblXref2"))
    {
    proteinName = sqlGetField(conn, database, "ensemblXref2", "translation_name", conditionStr);
    }
else
    {
    if (hTableExists("ensemblXref"))
    	{
    	proteinName = sqlGetField(conn, database, "ensemblXref", "translation_name", conditionStr);
    	}
    else
	{
	if (hTableExists("ensTranscript"))
	    {
	    proteinName = sqlGetField(conn,database,"ensTranscript","translation_name",conditionStr);
	    }
	else
	    {
	    if (hTableExists("ensemblXref3"))
    		{
		sprintf(conditionStr, "transcript='%s'", sw->name);
    		proteinName = sqlGetField(conn, database, "ensemblXref3", "protein", conditionStr);
    		}
	    else
	    	{
	    	proteinName = cloneString("");
		}
	    }
	}
    }
	
name = cloneString(proteinName);
hFreeConn(&conn);
/*
abbr(name, "000000");
abbr(name, "00000");
abbr(name, "0000");
*/
return(name);
}

char *superfamilyMapName(struct track *tg, void *item)
/* Return map name of the track item (used by hgc). */
{
char *name;
char *proteinName;
struct sqlConnection *conn = hAllocConn();
char conditionStr[256];

struct bed *sw = item;

// This is necessary because Ensembl kept changing their xref table definition
sprintf(conditionStr, "transcript_name='%s'", sw->name);
if (hTableExists("ensGeneXref"))
    {
    proteinName = sqlGetField(conn, database, "ensGeneXref", "translation_name", conditionStr);
    }
else if (hTableExists("ensemblXref2"))
    {
    proteinName = sqlGetField(conn, database, "ensemblXref2", "translation_name", conditionStr);
    }
else
    {
    if (hTableExists("ensemblXref"))
	{
    	proteinName = sqlGetField(conn, database, "ensemblXref", "translation_name", conditionStr);
    	}
    else
        {
        if (hTableExists("ensTranscript"))
            {
            proteinName = sqlGetField(conn,database,"ensTranscript","translation_name",conditionStr);
            }
        else
            {
	    if (hTableExists("ensemblXref3"))
    		{
		sprintf(conditionStr, "transcript='%s'", sw->name);
    		proteinName = sqlGetField(conn, database, "ensemblXref3", "protein", conditionStr);
    		}
	    else
	    	{
	    	proteinName = cloneString("");
		}
            }
        }
    }

name = cloneString(proteinName);
hFreeConn(&conn);

return(name);
}

// assuming no more than 100 domains in a protein
char sfDesc[100][256];
char sfBuffer[25600];

char *superfamilyNameLong(struct track *tg, void *item)
/* Return domain names of an entry of a Superfamily track item, 
   each item may have multiple names 
   due to possibility of multiple domains. */
{
struct bed *sw = item;
struct sqlConnection *conn;
int sfCnt;
char *desc;
char query[256];
struct sqlResult *sr;
char **row;
char *chp;
int i;

conn = hAllocConn();
sprintf(query, 
	"select description from sfDescription where name='%s';", 
	sw->name);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

sfCnt = 0;
while (row != NULL)
    {      
    desc = row[0];
    sprintf(sfDesc[sfCnt], "%s", desc);
    
    row = sqlNextRow(sr);
    sfCnt++;
    if (sfCnt >= 100) break;
    }

chp = sfBuffer;
for (i=0; i<sfCnt; i++)
    {
    if (i != 0)
	{
	sprintf(chp, "; ");
	chp++;chp++;
	}
    sprintf(chp, "%s", sfDesc[i]);
    chp = chp+strlen(sfDesc[i]);
    }

hFreeConn(&conn);
sqlFreeResult(&sr);
    
return(sfBuffer);
}

int superfamilyItemStart(struct track *tg, void *item)
/* Return start position of item. */
{
struct bed *bed = item;
return bed->chromStart;
}

int superfamilyItemEnd(struct track *tg, void *item)
/* Return end position of item. */
{
struct bed *bed = item;
return bed->chromEnd;
}

static void superfamilyDrawAt(struct track *tg, void *item, 
	struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single superfamily item at position. */
{
struct bed *bed = item;
char *sLong;
int heightPer = tg->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w;

sLong = superfamilyNameLong(tg, item);
if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, vg);
else
    {
    if (tg->colorShades)
	color = tg->colorShades[grayInRange(bed->score, 0, 1000)];
    }
w = x2-x1;
if (w < 1)
    w = 1;
if (color)
    {
    vgBox(vg, x1, y, w, heightPer, color);
    
    // special label processing for superfamily track, because long names provide
    // important info on structures and functions
    if (vis == tvFull)
        {
        vgTextRight(vg, x1-mgFontStringWidth(font, sLong)-2, y, mgFontStringWidth(font, sLong),
                heightPer, MG_BLACK, font, sLong);
        }
    if (tg->drawName && vis != tvSquish)
	{
	/* Clip here so that text will tend to be more visible... */
	char *s = tg->itemName(tg, bed);
	w = x2-x1;
	if (w > mgFontStringWidth(font, s))
	    {
	    Color textColor = vgContrastingColor(vg, color);
	    vgTextCentered(vg, x1, y, w, heightPer, textColor, font, s);
	    }
	}
    mapBoxHc(bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
	     tg->mapName, tg->mapItemName(tg, bed), sLong);
    }
if (tg->subType == lfWithBarbs)
    {
    int dir = 0;
    if (bed->strand[0] == '+')
	dir = 1;
    else if(bed->strand[0] == '-') 
	dir = -1;
    if (dir != 0 && w > 2)
	{
	int midY = y + (heightPer>>1);
	Color textColor = vgContrastingColor(vg, color);
	clippedBarbs(vg, x1, midY, w, tl.barbHeight, tl.barbSpacing, 	
		dir, textColor, TRUE);
	}
    }
}

static void superfamilyDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw superfamily items. */
{
if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->mapName);
genericDrawItems(tg, seqStart, seqEnd, vg, xOff, yOff, width, 
	font, color, vis);
}

void superfamilyMethods(struct track *tg)
/* Fill in methods for (simple) bed tracks. */
{
tg->drawItems 	= superfamilyDraw;
tg->drawItemAt 	= superfamilyDrawAt;
tg->itemName 	= superfamilyName;
tg->mapItemName = superfamilyMapName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight 	= tgFixedItemHeight;
tg->itemStart 	= superfamilyItemStart;
tg->itemEnd 	= superfamilyItemEnd;
tg->drawName 	= FALSE;
}

struct hash *gdHash;

/* reserve space no more than 20 unique gad disease entries */
char gadDiseaseClassBuffer[2000];

char *gadDiseaseClassList(struct track *tg, struct bed *item)
/* Return list of diseases associated with a GAD entry */
{
struct sqlConnection *conn;
char query[256];
struct sqlResult *sr;
char **row;
char *chp;
char *diseaseClassCode;

int i=0;
conn = hAllocConn();

sprintf(query, "select distinct diseaseClassCode from gadAll where geneSymbol='%s';", item->name);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

/* show up to 20 max entries */
chp = gadDiseaseClassBuffer;
while ((row != NULL) && i<20)
    {
    if (i != 0)
	{
	sprintf(chp, ",");
	chp++;
	}
    diseaseClassCode = row[0];
    
    sprintf(chp, "%s", diseaseClassCode);
    chp = chp+strlen(diseaseClassCode);
    row = sqlNextRow(sr);
    i++;
    }

if ((i == 20) && (row != NULL))
    {
    sprintf(chp, " ...");
    chp++;chp++;chp++;chp++;
    }

*chp = '\0';

hFreeConn(&conn);
sqlFreeResult(&sr);
return(gadDiseaseClassBuffer);
}

/* reserve space no more than 100 unique gad disease entries */
char gadBuffer[25600];

char *gadDiseaseList(struct track *tg, struct bed *item)
/* Return list of diseases associated with a GAD entry */
{
struct sqlConnection *conn;
char query[256];
struct sqlResult *sr;
char **row;
char *chp;
int i=0;

conn = hAllocConn();

sprintf(query, "select distinct broadPhen from gadAll where geneSymbol='%s';", item->name);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

/* show up to 20 max entries */
chp = gadBuffer;
while ((row != NULL) && i<20)
    {
    if (i != 0)
	{
	sprintf(chp, "; ");
	chp++;chp++;
	}
    sprintf(chp, "%s", row[0]);
    chp = chp+strlen(row[0]);
    row = sqlNextRow(sr);
    i++;
    }

if ((i == 20) && (row != NULL))
    {
    sprintf(chp, " ...");
    chp++;chp++;chp++;chp++;
    }

*chp = '\0';

hFreeConn(&conn);
sqlFreeResult(&sr);
return(gadBuffer);
}

static void gadDrawAt(struct track *tg, void *item, 
	struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single superfamily item at position. */
{
struct bed *bed = item;
char *sDiseases;
char *sDiseaseClasses;
int heightPer = tg->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w;

sDiseases = gadDiseaseList(tg, item);
sDiseaseClasses = gadDiseaseClassList(tg, item);
if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, vg);
else
    {
    if (tg->colorShades)
	color = tg->colorShades[grayInRange(bed->score, 0, 1000)];
    }
w = x2-x1;
if (w < 1)
    w = 1;
if (color)
    {
    vgBox(vg, x1, y, w, heightPer, color);
    
    if (vis == tvFull)
        {
        vgTextRight(vg, x1-mgFontStringWidth(font, sDiseaseClasses)-2, y, 
		    mgFontStringWidth(font, sDiseaseClasses),
                    heightPer, MG_BLACK, font, sDiseaseClasses);
        }
    if (tg->drawName && vis != tvSquish)
	{
	/* Clip here so that text will tend to be more visible... */
	char *s = tg->itemName(tg, bed);
	w = x2-x1;
	if (w > mgFontStringWidth(font, s))
	    {
	    Color textColor = vgContrastingColor(vg, color);
	    vgTextCentered(vg, x1, y, w, heightPer, textColor, font, s);
	    }
	}
    mapBoxHc(bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
	     tg->mapName, tg->mapItemName(tg, bed), sDiseases);
    }
if (tg->subType == lfWithBarbs)
    {
    int dir = 0;
    if (bed->strand[0] == '+')
	dir = 1;
    else if(bed->strand[0] == '-') 
	dir = -1;
    if (dir != 0 && w > 2)
	{
	int midY = y + (heightPer>>1);
	Color textColor = vgContrastingColor(vg, color);
	clippedBarbs(vg, x1, midY, w, tl.barbHeight, tl.barbSpacing, 	
		dir, textColor, TRUE);
	}
    }
}

void gadMethods(struct track *tg)
/* Methods for GAD track. */
{
tg->drawItemAt 	= gadDrawAt;
}

void rgdQtlDrawAt(struct track *tg, void *item, 
	struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single rgdQtl item at position. */
{
struct bed *bed = item;
struct sqlConnection *conn = hAllocConn();
char cond_str[256];
char *chp;

int heightPer = tg->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int x3, x4;
int w, w2;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));
char *directUrl = trackDbSetting(tdb, "directUrl");
boolean withHgsid = (trackDbSetting(tdb, "hgsid") != NULL);

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, vg);
else
    {
    if (tg->colorShades)
	color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
    }
w = x2-x1;
if (w < 1)
    w = 1;
if (color)
    {
    vgBox(vg, x1, y, w, heightPer, color);
    if (tg->drawName && vis != tvSquish)
	{
	char *s;
	
	/* get description from rgdQtlLink table */
	sprintf(cond_str, "name='%s'", tg->itemName(tg, bed));
        s  = sqlGetField(conn, database, "rgdQtlLink", "description", cond_str);

	/* chop off text starting from " (human)" */
	chp = strstr(s, " (human)");
	if (chp != NULL) *chp = '\0';
	
	x3 = x1;
	x4 = x2;
	
	/* adjust range of text display to fit within the display window */
	if (x3 < xOff) x3 = xOff;
	if (x4 > (insideWidth + xOff)) x4 = insideWidth + xOff;
	w2 = x4 - x3;
	if (w2 > mgFontStringWidth(font, s))
	    {
	    Color textColor = vgContrastingColor(vg, color);
	    vgTextCentered(vg, x3, y, w2, heightPer, textColor, font, s);
	    }
	else if (w2 > mgFontStringWidth(font, s)/2)
	    {
	    /* sqeez in the text for shorter QTL range */
	    Color textColor = vgContrastingColor(vg, color);
	    vgText(vg, x3+1, y+heightPer/2-2, textColor, font, s);
	    }
	/* enable mouse over */
	mapBoxHgcOrHgGene(bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
		tg->mapName, tg->mapItemName(tg, bed), s, directUrl, withHgsid);
	}
    }
hFreeConn(&conn);
}

void rgdQtlMethods(struct track *tg)
/* Fill in methods for rgdQtl track. */
{
tg->drawItemAt 	= rgdQtlDrawAt;
tg->drawName 	= TRUE;
}

char *getOrganism(struct sqlConnection *conn, char *acc)
/* lookup the organism for an mrna, or NULL if not found.  Warning: static
 * return */
{
static char orgBuf[256];
char query[256], *org;
sprintf(query, "select organism.name from gbCdnaInfo,organism where gbCdnaInfo.acc = '%s' and gbCdnaInfo.organism = organism.id", acc);
org = sqlQuickQuery(conn, query, orgBuf, sizeof(orgBuf));
if ((org != NULL) && (org[0] == '\0'))
    org = NULL;
return org;
}

char *getOrganismShort(struct sqlConnection *conn, char *acc)
/* lookup the organism for an mrna, or NULL if not found.  This will
 * only return the genus, and only the first seven letters of that.
 * Warning: static return */
{
int maxOrgSize = 7;
char *org = getOrganism(conn, acc);
if (org != NULL)
    {
    org = firstWordInLine(org);
    if (strlen(org) > maxOrgSize)
        org[maxOrgSize] = 0;
    }
return org;
}


char *getGeneName(struct sqlConnection *conn, char *acc)
/* get geneName from refLink or NULL if not found.  Warning: static return */
{
static char nameBuf[256];
char query[256], *name = NULL;
if (hTableExists("refLink"))
    {
    sprintf(query, "select name from refLink where mrnaAcc = '%s'", acc);
    name = sqlQuickQuery(conn, query, nameBuf, sizeof(nameBuf));
    if ((name != NULL) && (name[0] == '\0'))
        name = NULL;
    }
return name;
}

char *gencodeGeneName(struct track *tg, void *item)
/* Get name to use for Gencode gene item. */
{
struct linkedFeatures *lf = item;
if (lf->extra != NULL) 
    return lf->extra;
else 
    return lf->name;
}

char *refGeneName(struct track *tg, void *item)
/* Get name to use for refGene item. */
{
struct linkedFeatures *lf = item;
if (lf->extra != NULL) 
    return lf->extra;
else 
    return lf->name;
}

char *refGeneMapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void lookupRefNames(struct track *tg)
/* This converts the refSeq accession to a gene name where possible. */
{
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn();
boolean isNative = sameString(tg->mapName, "refGene");
boolean labelStarted = FALSE;
boolean useGeneName = FALSE; 
boolean useAcc =  FALSE;
boolean useMim =  FALSE; 

struct hashEl *refGeneLabels = cartFindPrefix(cart, (isNative ? "refGene.label" : "xenoRefGene.label"));
struct hashEl *label;
char omimLabel[48];
safef(omimLabel, sizeof(omimLabel), "omim%s", cartString(cart, "db"));

if (refGeneLabels == NULL)
    {
    useGeneName = TRUE; /* default to gene name */
    /* set cart to match what doing */
    if (isNative) cartSetBoolean(cart, "refGene.label.gene", TRUE);
    else cartSetBoolean(cart, "xenoRefGene.label.gene", TRUE);
    }
for (label = refGeneLabels; label != NULL; label = label->next)
    {
    if (endsWith(label->name, "gene") && differentString(label->val, "0"))
        useGeneName = TRUE;
    else if (endsWith(label->name, "acc") && differentString(label->val, "0"))
        useAcc = TRUE;
    else if (endsWith(label->name, omimLabel) && differentString(label->val, "0"))
        useMim = TRUE;
    else if (!endsWith(label->name, "gene") &&
             !endsWith(label->name, "acc")  &&
             !endsWith(label->name, omimLabel) )
        {
        useGeneName = TRUE;
        cartRemove(cart, label->name);
        }
    }

for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(64);
    labelStarted = FALSE; /* reset for each item in track */
    if ((useGeneName || useAcc || useMim) && 
        (!isNative || isNewChimp(database)))
                /* special handling for chimp -- both chimp and
                   human refSeq's are considered 'native', so we
                   label them to distinguish */

        {
        char *org = getOrganismShort(conn, lf->name);
        if (org != NULL)
            dyStringPrintf(name, "%s ", org);
        }
    if (useGeneName)
        {
        char *gene = getGeneName(conn, lf->name);
        if (gene != NULL)
            {
            dyStringAppend(name, gene);
            }
        labelStarted = TRUE;
        }
    if (useAcc)
        {
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        dyStringAppend(name, lf->name);
        }
    if (useMim)
        {
        char *mimId;
        char query[256];
        safef(query, sizeof(query), "select cast(omimId as char) from refLink where mrnaAcc = '%s'", lf->name);
        mimId = sqlQuickString(conn, query);
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        if (mimId && differentString(mimId, "0"))
            dyStringAppend(name, mimId);
        }
    lf->extra = dyStringCannibalize(&name);
    }
hFreeConn(&conn);
}

void lookupProteinNames(struct track *tg)
/* This converts the knownGene accession to a gene name where possible. */
{
struct linkedFeatures *lf;
boolean useGene, useAcc, useSprot, usePos;
char geneName[64];
char accName[64];
char sprotName[64];
char posName[64];
char *blastRef;
char *buffer;
char *table = NULL;

safef(geneName, sizeof(geneName), "%s.geneLabel", tg->tdb->tableName);
safef(accName, sizeof(accName), "%s.accLabel", tg->tdb->tableName);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tg->tdb->tableName);
safef(posName, sizeof(posName), "%s.posLabel", tg->tdb->tableName);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);
blastRef = trackDbSettingOrDefault(tg->tdb, "blastRef", NULL);

if (blastRef != NULL) 
    {
    char *thisDb = cloneString(blastRef);

    if ((table = strchr(thisDb, '.')) != NULL)
	{
	*table++ = 0;
	if (hTableExistsDb(thisDb, table))
	    {
	    char query[256];
	    struct sqlResult *sr;
	    char **row;
	    struct sqlConnection *conn = hAllocConn();
	    boolean added = FALSE;
	    char *ptr;
		
	    for (lf = tg->items; lf != NULL; lf = lf->next)
		{
		added = FALSE;
		
		buffer = needMem(strlen(lf->name) + 1);
		strcpy(buffer, lf->name);
		if ((char *)NULL != (ptr = strchr(buffer, '.')))
		    *ptr = 0;
		if (!startsWith("blastDm", tg->tdb->tableName))
		    safef(query, sizeof(query), "select geneId, refPos, extra1 from %s where acc = '%s'", blastRef, buffer);
		else
		    safef(query, sizeof(query), "select geneId, refPos from %s where acc = '%s'", blastRef, buffer);
		sr = sqlGetResult(conn, query);
		if ((row = sqlNextRow(sr)) != NULL)
		    {
		    lf->extra = needMem(strlen(lf->name) + strlen(row[0])+ strlen(row[1])+ strlen(row[2]) + 1);
		    if (useGene)
			{
			added = TRUE;
			strcat(lf->extra, row[0]);
			}
		    if (useAcc )
			{
			if (added)
			    strcat(lf->extra, "/");
			added = TRUE;
			strcat(lf->extra, lf->name);
			}
		    if (useSprot)
			{
			char *alias = uniProtFindPrimAcc(row[2]);

			if (added)
			    strcat(lf->extra, "/");
			added = TRUE;
			if (alias != NULL)
			    strcat(lf->extra, alias);
			else
			    strcat(lf->extra, row[2]);
			}
		    if (usePos)
			{
			char *startPos = strchr(row[1], ':');
			char *dash = strchr(row[1], '-');

			if ((startPos != NULL) && (dash != NULL))
			    {
			    *startPos++ = 0;
			    dash -= 3; /* divide by 1000 */
			    *dash = 0;
			    if (added)
				strcat(lf->extra, "/");
			    strcat(lf->extra, row[1]);
			    strcat(lf->extra, " ");
			    strcat(lf->extra, startPos);
			    strcat(lf->extra, "k");
			    }
			}
		    }
		sqlFreeResult(&sr);
		}
	    hFreeConn(&conn);
	    }
	}
    }
else
    for (lf = tg->items; lf != NULL; lf = lf->next)
	lf->extra = cloneString(lf->name);
}


void loadRefGene(struct track *tg)
/* Load up RefSeq known genes. */
{
enum trackVisibility vis = tg->visibility;
tg->items = lfFromGenePredInRange(tg, tg->mapName, chromName, winStart, winEnd);
if (vis != tvDense)
    {
    lookupRefNames(tg);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
vis = limitVisibility(tg);
}


Color blastColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw protein in. */
{
struct linkedFeatures *lf = item;
int col = tg->ixColor;
char *acc;
char *colon, *pos;
char *buffer;
char cMode[64];
int colorMode;
char *blastRef;

if (baseColorGetDrawOpt(tg) != baseColorDrawOff)
    return tg->ixColor;

safef(cMode, sizeof(cMode), "%s.cmode", tg->tdb->tableName);
colorMode = cartUsualInt(cart, cMode, 0);

switch(colorMode)
    {
    case 0: /* pslScore */
	col = shadesOfGray[lf->grayIx];
	break;
    case 1: /* human position */
	acc = buffer = cloneString(lf->name);
	blastRef = trackDbSettingOrDefault(tg->tdb, "blastRef", NULL);
	if (blastRef != NULL) 
	    {
	    char *thisDb = cloneString(blastRef);
	    char *table;

	    if ((table = strchr(thisDb, '.')) != NULL)
		{
		*table++ = 0;
		if (hTableExistsDb(thisDb, table))
		    {
		    char query[256];
		    struct sqlResult *sr;
		    char **row;
		    struct sqlConnection *conn = hAllocConn();

		    if ((pos = strchr(acc, '.')) != NULL)
			*pos = 0;
		    safef(query, sizeof(query), "select refPos from %s where acc = '%s'", blastRef, buffer);
		    sr = sqlGetResult(conn, query);
		    if ((row = sqlNextRow(sr)) != NULL)
			{
			if (startsWith("chr", row[0]) && ((colon = strchr(row[0], ':')) != NULL))
			    {
			    *colon = 0;
			    col = getSeqColor(row[0], vg);
			    }
			}
		    sqlFreeResult(&sr);
		    hFreeConn(&conn);
		    }
		}
	    }
	else
	    {
	    if ((pos = strchr(acc, '.')) != NULL)
		{
		pos += 1;
		if ((colon = strchr(pos, ':')) != NULL)
		    {
		    *colon = 0;
		    col = getSeqColor(pos, vg);
		    }
		}
	    }
	break;
    case 2: /* black */
	col = 1;
	break;
    }

tg->ixAltColor = col;
return(col);
}

Color refGeneColorByStatus(struct track *tg, char *name, struct vGfx *vg)
/* Get refseq gene color from refSeqStatus.
 * Reviewed, Validated -> normal, Provisional -> lighter, 
 * Predicted, Inferred(other) -> lightest 
 * If no refSeqStatus, color it normally. 
 */
{
int col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter, lightest;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
sprintf(query, "select status from refSeqStatus where mrnaAcc = '%s'",
        name);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (startsWith("Reviewed", row[0]) || startsWith("Validated", row[0]))
        {
        /* Use the usual color */
        }
    else if (startsWith("Provisional", row[0]))
        {
        lighter.r = (6*normal->r + 4*255) / 10;
        lighter.g = (6*normal->g + 4*255) / 10;
        lighter.b = (6*normal->b + 4*255) / 10;
        col = vgFindRgb(vg, &lighter);
        }
    else
        {
        lightest.r = (1*normal->r + 2*255) / 3;
        lightest.g = (1*normal->g + 2*255) / 3;
        lightest.b = (1*normal->b + 2*255) / 3;
        col = vgFindRgb(vg, &lightest);
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return col;
}

Color refGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw refseq gene in. */
{
struct linkedFeatures *lf = item;

/* allow itemAttr to override coloring */
if (lf->itemAttr != NULL)
    return vgFindColorIx(vg, lf->itemAttr->colorR, lf->itemAttr->colorG, lf->itemAttr->colorB);

/* If refSeqStatus is available, use it to determine the color.
 * Reviewed, Validated -> normal, Provisional -> lighter, 
 * Predicted, Inferred(other) -> lightest 
 * If no refSeqStatus, color it normally. 
 */
if (hTableExists("refSeqStatus"))
    return refGeneColorByStatus(tg, lf->name, vg);
else
    return(tg->ixColor);
}


void refGeneMethods(struct track *tg)
/* Make track of known genes from refSeq. */
{
tg->loadItems = loadRefGene;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = refGeneColor;
}

char *ensGeneName(struct track *tg, void *item)
/* Return abbreviated ensemble gene name. */
{
struct linkedFeatures *lf = item;
char *full = lf->name;
static char abbrev[32];

strncpy(abbrev, full, sizeof(abbrev));
/*abbr(abbrev, "SEPT20T.");
abbr(abbrev, "T000000");
abbr(abbrev, "T00000");
abbr(abbrev, "T0000");
*/
return abbrev;
}

void ensGeneMethods(struct track *tg)
/* Make track of Ensembl predictions. */
{
tg->itemName = ensGeneName;
}

int cDnaReadDirectionForMrna(struct sqlConnection *conn, char *acc)
/* Return the direction field from the mrna table for accession
   acc. Return -1 if not in table.*/
{
int direction = -1;
char query[512];
char buf[64], *s = NULL;
sprintf(query, "select direction from gbCdnaInfo where acc='%s'", acc);
if ((s = sqlQuickQuery(conn, query, buf, sizeof(buf))) != NULL)
    {
    direction = atoi(s);
    }
return direction;
}

void orientEsts(struct track *tg)
/* Orient ESTs from the estOrientInfo table.  */
{
struct linkedFeatures *lf = NULL, *lfList = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;
struct estOrientInfo ei;
int estOrient = 0;
struct hash *orientHash = NULL;
lfList = tg->items;

if(slCount(lfList) == 0)
    return; /* Nothing to orient. */

if(hTableExists("estOrientInfo"))
    {
    /* First load up a hash with the orientations. That
       way we only query the database once rather than
       hundreds or thousands of times. */
    int hashSize = (log(slCount(lfList))/log(2)) + 1;
    orientHash = newHash(hashSize);
    sr = hRangeQuery(conn, "estOrientInfo", chromName, 
		     winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	estOrientInfoStaticLoad(row + rowOffset, &ei);
	hashAddInt(orientHash, ei.name, ei.intronOrientation);
	}
    sqlFreeResult(&sr);

    /* Now lookup orientation of each est. If not in hash
       lookup read direction in mrna table. */
    for(lf = lfList; lf != NULL; lf = lf->next)
	{
	estOrient = hashIntValDefault(orientHash, lf->name, 0); 
	if(estOrient < 0) 
	    lf->orientation = -1 * lf->orientation;
	else if(estOrient == 0)
	    {
	    int dir = cDnaReadDirectionForMrna(conn, lf->name);
	    if(dir == 3) /* Est sequenced from 3' end. */
		lf->orientation = -1 * lf->orientation;
	    }
	}
    hashFree(&orientHash);
    }
else /* if can't find estOrientInfo table */
    {
    for(lf = lfList; lf != NULL; lf = lf->next)
	{
	int dir = cDnaReadDirectionForMrna(conn, lf->name);
	if(dir == 3) /* Est sequenced from 3' end. */
	    lf->orientation = -1 * lf->orientation;
	}
    }
hFreeConn(&conn);
}

void linkedFeaturesAverageDenseOrientEst(struct track *tg, 
	int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense linked features items. */
{
if(vis == tvSquish || vis == tvPack || vis == tvFull)
    orientEsts(tg);

if (vis == tvDense)
    linkedFeaturesDrawAverage(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
else
    linkedFeaturesDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
}



char *sanger22Name(struct track *tg, void *item)
/* Return Sanger22 name. */
{
struct linkedFeatures *lf = item;
char *full = lf->name;
static char abbrev[64];

strncpy(abbrev, full, sizeof(abbrev));
abbr(abbrev, "Em:");
abbr(abbrev, ".C22");
//abbr(abbrev, ".mRNA");
return abbrev;
}


void sanger22Methods(struct track *tg)
/* Make track of Sanger's chromosome 22 gene annotations. */
{
tg->itemName = sanger22Name;
}

Color vegaColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw vega gene/pseudogene in. */
{
struct linkedFeatures *lf = item;
Color col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter, lightest;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
struct dyString *dy = dyStringNew(256);
struct dyString *dy2 = dyStringNew(256);
char *infoTable = NULL, *infoCol = NULL;

/* If vegaInfo is available, use it to determine the color:
 *  Known - black
 *  {Novel_CDS, Novel_Transcript} - dark blue
 *  {Putative,Ig_Segment} - medium blue
 *  {Predicted_gene,Pseudogene,Ig_Pseudogene_Segment} - light blue
 *  None of the above - gray
 * If no vegaInfo, color it normally. 
 * For Zebrafish, the info table is called vegaInfoZfish and the 
 * categories are now different in zebrafish and in human from hg17 onwards.
 * KNOWN - black
 * NOVEL - dark blue
 * PUTATIVE - medium blue
 * PREDICTED - light blue
 * others e.g. UNCLASSIFIED will be gray.
 */

if (hTableExists("vegaInfo"))
    dyStringPrintf(dy, "%s", "vegaInfo");
else if (sameWord(organism, "Zebrafish") && hTableExists("vegaInfoZfish"))
    dyStringPrintf(dy, "%s", "vegaInfoZfish");
if (dy != NULL)
    infoTable = dyStringCannibalize(&dy);

/* for hg15 and hg16 coloring is based on entries in the confidence column */
if (sameString(database, "hg15") || sameString(database, "hg16"))
    dyStringPrintf(dy2, "%s", "method");
/* for other databases, coloring is based on entries in the method column */
else
    dyStringPrintf(dy2, "%s", "confidence");
if (dy2 != NULL)
    infoCol = dyStringCannibalize(&dy2);

if (infoTable != NULL && infoCol != NULL)
    {
    sprintf(query, "select %s from %s where transcriptId = '%s'",
	    infoCol, infoTable, lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	if (sameWord("Known", row[0]) ||
            sameWord("KNOWN", row[0]))
	    {
	    col = blackIndex();
	    }
	else if (sameWord("Novel_CDS", row[0]) ||
		 sameWord("Novel_Transcript", row[0]) ||
                 sameWord("NOVEL", row[0]))
	    {
	    /* Use the usual color (dark blue) */
	    }
	else if (sameWord("Putative", row[0]) ||
		 sameWord("Ig_Segment", row[0]) || 
                 sameWord("PUTATIVE", row[0]))
	    {
	    lighter.r = (6*normal->r + 4*255) / 10;
	    lighter.g = (6*normal->g + 4*255) / 10;
	    lighter.b = (6*normal->b + 4*255) / 10;
	    col = vgFindRgb(vg, &lighter);
	    }
	else if (sameWord("Predicted_gene", row[0]) ||
		 sameWord("Pseudogene", row[0]) ||
		 sameWord("Ig_Pseudogene_Segment", row[0]) ||
                 sameWord("PREDICTED", row[0]))
	    {
	    lightest.r = (1*normal->r + 2*255) / 3;
	    lightest.g = (1*normal->g + 2*255) / 3;
	    lightest.b = (1*normal->b + 2*255) / 3;
	    col = vgFindRgb(vg, &lightest);
	    }
	else
	    {
	    col = lightGrayIndex();
	    }
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return(col);
}

char *bdgpGeneName(struct track *tg, void *item)
/* Return bdgpGene symbol. */
{
struct linkedFeatures *lf = item;
char *name = cloneString(lf->name);
char infoTable[128];
safef(infoTable, sizeof(infoTable), "%sInfo", tg->mapName);
if (hTableExists(infoTable))
    {
    struct sqlConnection *conn = hAllocConn();
    char *symbol = NULL;
    char *ptr = strchr(name, '-');
    char query[256];
    char buf[64];
    if (ptr != NULL)
	*ptr = 0;
    safef(query, sizeof(query),
	  "select symbol from %s where bdgpName = '%s';", infoTable, name);
    symbol = sqlQuickQuery(conn, query, buf, sizeof(buf));
    hFreeConn(&conn);
    if (symbol != NULL)
	{
	char *ptr = stringIn("{}", symbol);
	if (ptr != NULL)
	    *ptr = 0;
	freeMem(name);
	name = cloneString(symbol);
	}
    }
return(name);
}

void bdgpGeneMethods(struct track *tg)
/* Special handling for bdgpGene items. */
{
tg->itemName = bdgpGeneName;
}

char *flyBaseGeneName(struct track *tg, void *item)
/* Return symbolic name for FlyBase gene with lookup table for name->symbol
 * specified in trackDb. */
{
struct linkedFeatures *lf = item;
char *name = cloneString(lf->name);
char *infoTable = trackDbSettingOrDefault(tg->tdb, "symbolTable", "");
if (isNotEmpty(infoTable) && hTableExists(infoTable))
    {
    struct sqlConnection *conn = hAllocConn();
    char *symbol = NULL;
    char query[256];
    char buf[64];
    safef(query, sizeof(query),
	  "select symbol from %s where name = '%s';", infoTable, name);
    symbol = sqlQuickQuery(conn, query, buf, sizeof(buf));
    hFreeConn(&conn);
    if (isNotEmpty(symbol))
	{
	freeMem(name);
	name = cloneString(symbol);
	}
    }
return(name);
}

void flyBaseGeneMethods(struct track *tg)
/* Special handling for FlyBase genes. */
{
tg->itemName = flyBaseGeneName;
}

char *sgdGeneName(struct track *tg, void *item)
/* Return sgdGene symbol. */
{
struct sqlConnection *conn = hAllocConn();
struct linkedFeatures *lf = item;
char *name = lf->name;
char *symbol = NULL;
char query[256];
char buf[64];
safef(query, sizeof(query),
      "select value from sgdToName where name = '%s'", name);
symbol = sqlQuickQuery(conn, query, buf, sizeof(buf));
hFreeConn(&conn);
if (symbol != NULL)
    name = symbol;
return(cloneString(name));
}

void sgdGeneMethods(struct track *tg)
/* Special handling for sgdGene items. */
{
tg->itemName = sgdGeneName;
}

void bedLoadItemByQuery(struct track *tg, char *table, 
			char *query, ItemLoader loader)
/* Generic tg->item loader. If query is NULL use generic
 hRangeQuery(). */
{
struct sqlConnection *conn = hAllocConn();
int rowOffset = 0;
struct sqlResult *sr = NULL;
char **row = NULL;
struct slList *itemList = NULL, *item = NULL;

if(query == NULL)
    sr = hRangeQuery(conn, table, chromName, 
		     winStart, winEnd, NULL, &rowOffset);
else
    sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    item = loader(row + rowOffset);
    slAddHead(&itemList, item);
    }
slSort(&itemList, bedCmp);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

void bedLoadItem(struct track *tg, char *table, ItemLoader loader)
/* Generic tg->item loader. */
{
bedLoadItemByQuery(tg, table, NULL, loader);
}


void bedDrawSimpleAt(struct track *tg, void *item, 
	struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct bed *bed = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));
char *directUrl = trackDbSetting(tdb, "directUrl");
boolean withHgsid = (trackDbSetting(tdb, "hgsid") != NULL);
boolean thickDrawItem = (trackDbSetting(tdb, "thickDrawItem") != NULL);

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, vg);
else
    {
    if (tg->colorShades)
	color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
    }
w = x2-x1;
if (w < 1)
    w = 1;
/*	Keep the item at least 4 pixels wide at all viewpoints */
if (thickDrawItem && (w < 4))
    {
    x1 -= ((5-w) >> 1);
    w = 4;
    x2 = x1 + w;
    }
if (color)
    {
    vgBox(vg, x1, y, w, heightPer, color);
    if (tg->drawName && vis != tvSquish)
	{
	/* Clip here so that text will tend to be more visible... */
	char *s = tg->itemName(tg, bed);
	w = x2-x1;
	if (w > mgFontStringWidth(font, s))
	    {
	    Color textColor = vgContrastingColor(vg, color);
	    vgTextCentered(vg, x1, y, w, heightPer, textColor, font, s);
	    }
	mapBoxHgcOrHgGene(bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
		tg->mapName, tg->mapItemName(tg, bed), NULL, directUrl, withHgsid);
	}
    }
if (tg->subType == lfWithBarbs || tg->exonArrows)
    {
    int dir = 0;
    if (bed->strand[0] == '+')
	dir = 1;
    else if(bed->strand[0] == '-') 
	dir = -1;
    if (dir != 0 && w > 2)
	{
	int midY = y + (heightPer>>1);
	Color textColor = vgContrastingColor(vg, color);
	clippedBarbs(vg, x1, midY, w, tl.barbHeight, tl.barbSpacing, 
		dir, textColor, TRUE);
	}
    }
}

static void logoDrawSimple(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw sequence logo */
{
struct dnaMotif *motif; 
int count = seqEnd - seqStart;
int ii;
FILE *f;
struct tempName pngTn;
unsigned char *buf;;

if (tg->items == NULL)
	return;

motif = tg->items;
buf = needMem(width + 1);;
dnaMotifNormalize(motif);
makeTempName(&pngTn, "logo", ".pgm");
dnaMotifToLogoPGM(motif, width / count, width  , 50, NULL, "../trash", pngTn.forCgi);

f = mustOpen(pngTn.forCgi, "r");

/* get rid of header */
for(ii=0; ii < 4; ii++)
    while(fgetc(f) != '\n')
	;

vgSetClip(vg, xOff, yOff, width*2 , 52);

/* map colors from PGM to browser colors */
for(ii=0; ii < 52; ii++)
    {
    int jj;
    fread(buf, 1, width, f);

    for(jj=0; jj < width + 2; jj++)
	{
	if (buf[jj] == 255) buf[jj] = 0;
	else if (buf[jj] == 0x44)buf[jj] = MG_RED;
	else if (buf[jj] == 0x69)buf[jj] = greenColor;
	else if (buf[jj] == 0x5e)buf[jj] = blueColor;
	}

    vgVerticalSmear(vg,xOff,yOff+ii,width ,1,buf,TRUE);
    }
vgUnclip(vg);

fclose(f);
remove(pngTn.forCgi);
}

static void bedDrawSimple(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw simple Bed items. */
{
if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->mapName);

genericDrawItems(tg, seqStart, seqEnd, vg, xOff, yOff, width, 
	font, color, vis);
}

char *bedName(struct track *tg, void *item)
/* Return name of bed track item. */
{
struct bed *bed = item;
if (bed->name == NULL)
    return "";
return bed->name;
}

int bedItemStart(struct track *tg, void *item)
/* Return start position of item. */
{
struct bed *bed = item;
return bed->chromStart;
}

int bedItemEnd(struct track *tg, void *item)
/* Return end position of item. */
{
struct bed *bed = item;
return bed->chromEnd;
}

void freeSimpleBed(struct track *tg)
/* Free the beds in a track group that has
   beds as its items. */
{
bedFreeList(((struct bed **)(&tg->items)));
}

void bedMethods(struct track *tg)
/* Fill in methods for (simple) bed tracks. */
{
tg->drawItems = bedDrawSimple;
tg->drawItemAt = bedDrawSimpleAt;
tg->itemName = bedName;
tg->mapItemName = bedName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = bedItemStart;
tg->itemEnd = bedItemEnd;
tg->freeItems = freeSimpleBed;
}

char *oligoMatchSeq()
/* Return sequence for oligo matching. */
{
char *s = cartOptionalString(cart, oligoMatchVar);
if (s != NULL)
    {
    int len;
    tolowers(s);
    dnaFilter(s, s);
    len = strlen(s);
    if (len < 2)
       s = NULL;
    }
if (s == NULL)
    s = cloneString(oligoMatchDefault);
return s;
}

char *oligoMatchName(struct track *tg, void *item)
/* Return name for oligo, which is just the base position. */
{
struct bed *bed = item;
static char buf[22];
buf[0] = bed->strand[0];
sprintLongWithCommas(buf+1, bed->chromStart+1);
return buf;
}

void oligoMatchLoad(struct track *tg)
/* Create track of perfect matches to oligo on either strand. */
{
char *dna = dnaInWindow();
char *fOligo = oligoMatchSeq();
int oligoSize = strlen(fOligo);
char *rOligo = cloneString(fOligo);
char *rMatch = NULL, *fMatch = NULL;
struct bed *bedList = NULL, *bed;
char strand;
int count = 0, maxCount = 1000000;

if (oligoSize >= 2)
    {
    fMatch = stringIn(fOligo, dna);
    reverseComplement(rOligo, oligoSize);
    if (sameString(rOligo, fOligo))
        rOligo = NULL;
    else
	rMatch = stringIn(rOligo, dna);
    for (;;)
        {
	char *oneMatch = NULL;
	if (rMatch == NULL)
	    {
	    if (fMatch == NULL)
	        break;
	    else
		{
	        oneMatch = fMatch;
		fMatch = stringIn(fOligo, fMatch+1);
		strand = '+';
		}
	    }
	else if (fMatch == NULL)
	    {
	    oneMatch = rMatch;
	    rMatch = stringIn(rOligo, rMatch+1);
	    strand = '-';
	    }
	else if (rMatch < fMatch)
	    {
	    oneMatch = rMatch;
	    rMatch = stringIn(rOligo, rMatch+1);
	    strand = '-';
	    }
	else
	    {
	    oneMatch = fMatch;
	    fMatch = stringIn(fOligo, fMatch+1);
	    strand = '+';
	    }
	if (count < maxCount)
	    {
	    ++count;
	    AllocVar(bed);
	    bed->chromStart = winStart + (oneMatch - dna);
	    bed->chromEnd = bed->chromStart + oligoSize;
	    bed->strand[0] = strand;
	    slAddHead(&bedList, bed);
	    }
	else
	    break;
	}
    slReverse(&bedList);
    if (count < maxCount)
	tg->items = bedList;
    else
        warn("More than %d items in %s, suppressing display",
	    maxCount, tg->shortLabel);
    }
}

struct track *oligoMatchTg()
/* Make track of perfect matches to oligomer. */
{
struct track *tg = trackNew();
char *oligo = oligoMatchSeq();
int oligoSize = strlen(oligo);
char *medOligo = cloneString(oligo);
static char longLabel[80];
struct trackDb *tdb;

/* Generate abbreviated strings. */
if (oligoSize >= 30)
    {
    memset(medOligo + 30-3, '.', 3);
    medOligo[30] = 0;
    }
touppers(medOligo);

bedMethods(tg);
AllocVar(tdb);
tg->mapName = "oligoMatch";
tg->canPack = TRUE;
tg->visibility = tvHide;
tg->hasUi = TRUE;
tg->shortLabel = cloneString(OLIGO_MATCH_TRACK_LABEL);
safef(longLabel, sizeof(longLabel), 
	"Perfect Matches to Short Sequence (%s)", medOligo);
tg->longLabel = longLabel;
tg->loadItems = oligoMatchLoad;
tg->itemName = oligoMatchName;
tg->mapItemName = oligoMatchName;
tg->priority = 99;
tg->defaultPriority = 99;
tg->groupName = "map";
tg->defaultGroupName = "map";
tdb->tableName = tg->mapName;
tdb->shortLabel = tg->shortLabel;
tdb->longLabel = tg->longLabel;
trackDbPolish(tdb);
tg->tdb = tdb;
return tg;
}

boolean tfbsConsSitesWeightFilterItem(struct track *tg, void *item,
	float cutoff)
/* Return TRUE if item passes filter. */
{
struct tfbsConsSites *el = item;

if (el->zScore < cutoff)
    return FALSE;
return TRUE;
}

void loadTfbsConsSites(struct track *tg)
/* Load conserved binding site track, all items that meet the cutoff. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct tfbsConsSites *ro, *list = NULL;
float tfbsConsSitesCutoff; /* Cutoff used for conserved binding site track. */

tfbsConsSitesCutoff =
    sqlFloat(cartUsualString(cart,TFBS_SITES_CUTOFF,TFBS_SITES_CUTOFF_DEFAULT));

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ro = tfbsConsSitesLoad(row+rowOffset);
    if (tfbsConsSitesWeightFilterItem(tg,ro,tfbsConsSitesCutoff))
        slAddHead(&list, ro);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = list;
}

void loadTfbsCons(struct track *tg)
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
char *lastName = NULL;
struct tfbsCons *tfbs, *list = NULL;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    tfbs = tfbsConsLoad(row+rowOffset);
    if ((lastName == NULL) || !sameString(lastName, tfbs->name))
	{
	slAddHead(&list, tfbs);
	freeMem(lastName);
	lastName = cloneString(tfbs->name);
	}
    }
freeMem(lastName);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
tg->items = list;
}

void tfbsConsSitesMethods(struct track *tg)
{
bedMethods(tg);
tg->loadItems = loadTfbsConsSites;
}

void tfbsConsMethods(struct track *tg)
{
bedMethods(tg);
tg->loadItems = loadTfbsCons;
}

void isochoreLoad(struct track *tg)
/* Load up isochores from database table to track items. */
{
bedLoadItem(tg, "isochores", (ItemLoader)isochoresLoad);
}

void isochoreFree(struct track *tg)
/* Free up isochore items. */
{
isochoresFreeList((struct isochores**)&tg->items);
}

char *isochoreName(struct track *tg, void *item)
/* Return name of gold track item. */
{
struct isochores *iso = item;
static char buf[64];
sprintf(buf, "%3.1f%% GC", 0.1*iso->gcPpt);
return buf;
}

static void isochoreDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw isochore items. */
{
struct isochores *item;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
double scale = scaleForPixels(width);

for (item = tg->items; item != NULL; item = item->next)
    {
    x1 = round((double)((int)item->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)item->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    color = shadesOfGray[grayInRange(item->gcPpt, 340, 617)];
    if (w < 1)
	w = 1;
    vgBox(vg, x1, y, w, heightPer, color);
    mapBoxHc(item->chromStart, item->chromEnd, x1, y, w, heightPer, tg->mapName,
	item->name, item->name);
    if (isFull)
	y += lineHeight;
    }
}

void isochoresMethods(struct track *tg)
/* Make track for isochores. */
{
tg->loadItems = isochoreLoad;
tg->freeItems = isochoreFree;
tg->drawItems = isochoreDraw;
tg->colorShades = shadesOfGray;
tg->itemName = isochoreName;
}

/* Ewan's stuff */
/******************************************************************/
					/*Royden fun test code*/
/******************************************************************/

void loadCeleraDupPositive(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "celeraDupPositive", (ItemLoader)celeraDupPositiveLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeCeleraDupPositive(struct track *tg)
/* Free up isochore items. */
{
celeraDupPositiveFreeList((struct celeraDupPositive**)&tg->items);
}

Color celeraDupPositiveColor(struct track *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
/*struct celeraDupPositive *dup = item;*/
/*int ppt = dup->score;*/
int grayLevel;

/*if (ppt > 990)*/
    return tg->ixColor;
/*else if (ppt > 980)*/
/*    return tg->ixAltColor;*/
/* grayLevel = grayInRange(ppt, 900, 1000); */
grayLevel=grayInRange(990,900,1000);

return shadesOfGray[grayLevel];
}

char *celeraDupPositiveName(struct track *tg, void *item)
/* Return full genie name. */
{
struct celeraDupPositive *gd = item;
char *full = gd->name;
static char abbrev[64];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void celeraDupPositiveMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadCeleraDupPositive;
tg->freeItems = freeCeleraDupPositive;
tg->itemName = celeraDupPositiveName;
tg->itemColor = celeraDupPositiveColor;
}

/******************************************************************/
		/*end of Royden test Code celeraDupPositive */
/******************************************************************/
/******************************************************************/
			/*Royden fun test code CeleraCoverage*/
/******************************************************************/


void loadCeleraCoverage(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "celeraCoverage", (ItemLoader)celeraCoverageLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeCeleraCoverage(struct track *tg)
/* Free up isochore items. */
{
celeraCoverageFreeList((struct celeraCoverage**)&tg->items);
}

Color celeraCoverageColor(struct track *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
/*struct celeraDupPositive *dup = item; */
/*int ppt = dup->score;*/
int grayLevel;

/*if (ppt > 990)*/
    return tg->ixColor;
/*else if (ppt > 980)*/
/*    return tg->ixAltColor;*/
/* grayLevel = grayInRange(ppt, 900, 1000); */
grayLevel=grayInRange(990,900,1000);

return shadesOfGray[grayLevel];
}

char *celeraCoverageName(struct track *tg, void *item)
/* Return full genie name. */
{
struct celeraCoverage *gd = item;
char *full = gd->name;
static char abbrev[64];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void celeraCoverageMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadCeleraCoverage;
tg->freeItems = freeCeleraCoverage;
tg->itemName = celeraCoverageName;
tg->itemColor = celeraCoverageColor;
}
/******************************************************************/
		/*end of Royden test Code celeraCoverage */
/******************************************************************/

void loadGenomicSuperDups(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "genomicSuperDups", (ItemLoader)genomicSuperDupsLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeGenomicSuperDups(struct track *tg)
/* Free up isochore items. */
{
genomicSuperDupsFreeList((struct genomicSuperDups**)&tg->items);
}

Color genomicSuperDupsColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of duplication - orange for > .990, yellow for > .980,
 * red for verdict == "BAD". */
{
struct genomicSuperDups *dup = (struct genomicSuperDups *)item;
static bool gotColor = FALSE;
static Color red, orange, yellow;
int grayLevel;

if (!gotColor)
    {
    red      = vgFindColorIx(vg, 255,  51, 51);
    orange   = vgFindColorIx(vg, 230, 130,  0);
    yellow   = vgFindColorIx(vg, 210, 200,  0);
    gotColor = TRUE;
    }
if (sameString(dup->verdict, "BAD"))
    return red;
if (dup->fracMatch > 0.990)
    return orange;
else if (dup->fracMatch > 0.980)
    return yellow;
grayLevel = grayInRange((int)(dup->fracMatch * 1000), 900, 1000);
return shadesOfGray[grayLevel];
}

char *genomicSuperDupsName(struct track *tg, void *item)
/* Return full genie name. */
{
struct genomicSuperDups *gd = item;
char *full = gd->name;
static char abbrev[64];
char *s = strchr(full, '.');
if (s != NULL)
    full = s+1;
safef(abbrev, sizeof(abbrev), "%s", full);
abbr(abbrev, "chrom");
return abbrev;
}


void genomicSuperDupsMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadGenomicSuperDups;
tg->freeItems = freeGenomicSuperDups;
tg->itemName = genomicSuperDupsName;
tg->itemColor = genomicSuperDupsColor;
}

/******************************************************************/
		/*end of Royden test Code genomicSuperDups */
/******************************************************************/
/*end Ewan's*/

/* Make track for Genomic Dups. */

void loadGenomicDups(struct track *tg)
/* Load up genomicDups from database table to track items. */
{
bedLoadItem(tg, "genomicDups", (ItemLoader)genomicDupsLoad);
if (limitVisibility(tg) == tvFull)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeGenomicDups(struct track *tg)
/* Free up isochore items. */
{
genomicDupsFreeList((struct genomicDups**)&tg->items);
}

Color dupPptColor(int ppt, struct vGfx *vg)
/* Return color of duplication - orange for > 990, yellow for > 980 */
{
static bool gotColor = FALSE;
static Color orange, yellow;
int grayLevel;
if (!gotColor)
    {
    orange = vgFindColorIx(vg, 230, 130, 0);
    yellow = vgFindColorIx(vg, 210, 200, 0);
    gotColor = TRUE;
    }
if (ppt > 990)
    return orange;
else if (ppt > 980)
    return yellow;
grayLevel = grayInRange(ppt, 900, 1000);
return shadesOfGray[grayLevel];
}

Color genomicDupsColor(struct track *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
struct genomicDups *dup = item;
return dupPptColor(dup->score, vg);
}

char *genomicDupsName(struct track *tg, void *item)
/* Return full genie name. */
{
struct genomicDups *gd = item;
char *full = gd->name;
static char abbrev[64];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void genomicDupsMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadGenomicDups;
tg->freeItems = freeGenomicDups;
tg->itemName = genomicDupsName;
tg->itemColor = genomicDupsColor;
}

Color jkDupliconColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of duplicon track item. */
{
struct bed *el = item;
int grayLevel = grayInRange(el->score, 600, 1000);
return shadesOfGray[grayLevel];
}

void jkDupliconMethods(struct track *tg)
/* Load up custom methods for duplicon. */
{
tg->itemColor = jkDupliconColor;
}


static void loadSimpleNucDiff(struct track *tg)
/* Load up simple diffs from database table to track items. */
{
bedLoadItem(tg, tg->mapName, (ItemLoader)simpleNucDiffLoad);
}

static char *simpleNucDiffName(struct track *tg, void *item)
/* Return name of simpleDiff item. */
{
static char buf[32];
struct simpleNucDiff *snd = item;
safef(buf, sizeof(buf), "%s<->%s", snd->tSeq, snd->qSeq);
return buf;
}

static void chimpSimpleDiffMethods(struct track *tg)
/* Load up custom methods for simple diff */
{
tg->loadItems = loadSimpleNucDiff;
tg->itemName = simpleNucDiffName;
}

char *simpleRepeatName(struct track *tg, void *item)
/* Return name of simpleRepeats track item. */
{
struct simpleRepeat *rep = item;
static char buf[17];
int len;

if (rep->sequence == NULL)
    return rep->name;
len = strlen(rep->sequence);
if (len <= 16)
    return rep->sequence;
else
    {
    memcpy(buf, rep->sequence, 13);
    memcpy(buf+13, "...", 3);
    buf[16] = 0;
    return buf;
    }
}

void loadSimpleRepeats(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, tg->mapName, (ItemLoader)simpleRepeatLoad);
}

void freeSimpleRepeats(struct track *tg)
/* Free up isochore items. */
{
simpleRepeatFreeList((struct simpleRepeat**)&tg->items);
}

void simpleRepeatMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadSimpleRepeats;
tg->freeItems = freeSimpleRepeats;
tg->itemName = simpleRepeatName;
}

Color cpgIslandColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of cpgIsland track item. */
{
struct cpgIsland *el = item;
return (el->length < 300 ? tg->ixAltColor : tg->ixColor);
}

void loadCpgIsland(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, tg->mapName, (ItemLoader)cpgIslandLoad);
}

void freeCpgIsland(struct track *tg)
/* Free up isochore items. */
{
cpgIslandFreeList((struct cpgIsland**)&tg->items);
}

void cpgIslandMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadCpgIsland;
tg->freeItems = freeCpgIsland;
tg->itemColor = cpgIslandColor;
}

char *rgdGeneItemName(struct track *tg, void *item)
/* Return name of RGD gene track item. */
{
static char name[32];
struct sqlConnection *conn = hAllocConn();
struct dyString *ds = newDyString(256);
struct linkedFeatures *lf = item;

dyStringPrintf(ds, "select name from rgdGeneLink where refSeq = '%s'", lf->name);
sqlQuickQuery(conn, ds->string, name, sizeof(name));
freeDyString(&ds);
hFreeConn(&conn);
return name;
}

void rgdGeneMethods(struct track *tg)
/* Make track for RGD genes. */
{
tg->itemName = rgdGeneItemName;
}

void loadGcPercent(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct gcPercent *itemList = NULL, *item;
char query[256];

sprintf(query, "select * from %s where chrom = '%s' and chromStart<%u and chromEnd>%u", tg->mapName,
    chromName, winEnd, winStart);

/* Get the frags and load into tg->items. */
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    item = gcPercentLoad(row);
    if (item->gcPpt != 0)
	{
	slAddHead(&itemList, item);
	}
    else
        gcPercentFree(&item);
    }
slReverse(&itemList);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

void freeGcPercent(struct track *tg)
/* Free up isochore items. */
{
gcPercentFreeList((struct gcPercent**)&tg->items);
}


char *gcPercentName(struct track *tg, void *item)
/* Return name of gcPercent track item. */
{
struct gcPercent *gc = item;
static char buf[32];

sprintf(buf, "%3.1f%% GC", 0.1*gc->gcPpt);
return buf;
}

static int gcPercentMin = 320;
static int gcPercentMax = 600;

Color gcPercentColor(struct track *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
struct gcPercent *gc = item;
int ppt = gc->gcPpt;
int grayLevel;

grayLevel = grayInRange(ppt, gcPercentMin, gcPercentMax);
return shadesOfGray[grayLevel];
}

void gcPercentMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadGcPercent;
tg->freeItems = freeGcPercent;
tg->itemName = gcPercentName;
tg->colorShades = shadesOfGray;
tg->itemColor = gcPercentColor;
}

char *recombRateMap;
enum recombRateOptEnum recombRateType;

boolean recombRateSetRate(struct track *tg, void *item)
/* Change the recombRate value to the one chosen */
{
struct recombRate *el = item;
switch (recombRateType)
    {
    case rroeDecodeAvg:
	return TRUE;
        break;
    case rroeDecodeFemale:
	el->decodeAvg = el->decodeFemale;
        return TRUE;
        break;
    case rroeDecodeMale:
	el->decodeAvg = el->decodeMale;
        return TRUE;
        break;
    case rroeMarshfieldAvg:
	el->decodeAvg = el->marshfieldAvg;
	return TRUE;
        break;
    case rroeMarshfieldFemale:
	el->decodeAvg = el->marshfieldFemale;
        return TRUE;
        break;
    case rroeMarshfieldMale:
	el->decodeAvg = el->marshfieldMale;
        return TRUE;
        break;
    case rroeGenethonAvg:
	el->decodeAvg = el->genethonAvg;
	return TRUE;
        break;
    case rroeGenethonFemale:
	el->decodeAvg = el->genethonFemale;
        return TRUE;
        break;
    case rroeGenethonMale:
	el->decodeAvg = el->genethonMale;
        return TRUE;
        break;
    default:
	return FALSE;
        break;
    }
}

void loadRecombRate(struct track *tg)
/* Load up recombRate from database table to track items. */
{
recombRateMap = cartUsualString(cart, "recombRate.type", rroeEnumToString(0));
recombRateType = rroeStringToEnum(recombRateMap);
bedLoadItem(tg, "recombRate", (ItemLoader)recombRateLoad);
filterItems(tg, recombRateSetRate, "include");
}

void freeRecombRate(struct track *tg)
/* Free up recombRate items. */
{
recombRateFreeList((struct recombRate**)&tg->items);
}

char *recombRateName(struct track *tg, void *item)
/* Return name of recombRate track item. */
{
struct recombRate *rr = item;
static char buf[32];

switch (recombRateType)
    {
    case rroeDecodeAvg: case rroeMarshfieldAvg: case rroeGenethonAvg:
	sprintf(buf, "%3.1f cM/Mb (Avg)", rr->decodeAvg);
        break;
    case rroeDecodeFemale: case rroeMarshfieldFemale: case rroeGenethonFemale:
	sprintf(buf, "%3.1f cM/Mb (F)", rr->decodeAvg);
        break;
    case rroeDecodeMale: case rroeMarshfieldMale: case rroeGenethonMale:
	sprintf(buf, "%3.1f cM/Mb (M)", rr->decodeAvg);
        break;
    default:
	sprintf(buf, "%3.1f cM/Mb (Avg)", rr->decodeAvg);
        break;
    }
return buf;
}

static int recombRateMin = 320;
static int recombRateMax = 600;

Color recombRateColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color for item in recombRate track item. */
{
struct recombRate *rr = item;
int rcr;
int grayLevel;

rcr = (int)(rr->decodeAvg * 200);
grayLevel = grayInRange(rcr, recombRateMin, recombRateMax);
return shadesOfGray[grayLevel];
}

void recombRateMethods(struct track *tg)
/* Make track for recombination rates. */
{
tg->loadItems = loadRecombRate;
tg->freeItems = freeRecombRate;
tg->itemName = recombRateName;
tg->colorShades = shadesOfGray;
tg->itemColor = recombRateColor;
}

char *recombRateRatMap;
enum recombRateRatOptEnum recombRateRatType;

boolean recombRateRatSetRate(struct track *tg, void *item)
/* Change the recombRateRat value to the one chosen */
{
struct recombRateRat *el = item;
switch (recombRateRatType)
    {
    case rrroeShrspAvg:
	return TRUE;
        break;
    case rrroeFhhAvg:
	el->shrspAvg = el->fhhAvg;
	return TRUE;
        break;
    default:
	return FALSE;
        break;
    }
}

void loadRecombRateRat(struct track *tg)
/* Load up recombRateRat from database table to track items. */
{
recombRateRatMap = cartUsualString(cart, "recombRateRat.type", rrroeEnumToString(0));
recombRateRatType = rrroeStringToEnum(recombRateRatMap);
bedLoadItem(tg, "recombRateRat", (ItemLoader)recombRateRatLoad);
filterItems(tg, recombRateRatSetRate, "include");
}

void freeRecombRateRat(struct track *tg)
/* Free up recombRateRat items. */
{
recombRateRatFreeList((struct recombRateRat**)&tg->items);
}

char *recombRateRatName(struct track *tg, void *item)
/* Return name of recombRateRat track item. */
{
struct recombRateRat *rr = item;
static char buf[32];

switch (recombRateRatType)
    {
    case rrroeShrspAvg: case rrroeFhhAvg:
	sprintf(buf, "%3.1f cM/Mb (Avg)", rr->shrspAvg);
        break;
    default:
	sprintf(buf, "%3.1f cM/Mb (Avg)", rr->shrspAvg);
        break;
    }
return buf;
}

Color recombRateRatColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color for item in recombRateRat track item. */
{
struct recombRateRat *rr = item;
int rcr;
int grayLevel;

rcr = (int)(rr->shrspAvg * 200);
grayLevel = grayInRange(rcr, recombRateMin, recombRateMax);
return shadesOfGray[grayLevel];
}

void recombRateRatMethods(struct track *tg)
/* Make track for rat recombination rates. */
{
tg->loadItems = loadRecombRateRat;
tg->freeItems = freeRecombRateRat;
tg->itemName = recombRateRatName;
tg->colorShades = shadesOfGray;
tg->itemColor = recombRateRatColor;
}


char *recombRateMouseMap;
enum recombRateMouseOptEnum recombRateMouseType;

boolean recombRateMouseSetRate(struct track *tg, void *item)
/* Change the recombRateMouse value to the one chosen */
{
struct recombRateMouse *el = item;
switch (recombRateMouseType)
    {
    case rrmoeWiAvg:
	return TRUE;
        break;
    case rrmoeMgdAvg:
	el->wiAvg = el->mgdAvg;
	return TRUE;
        break;
    default:
	return FALSE;
        break;
    }
}

void loadRecombRateMouse(struct track *tg)
/* Load up recombRateMouse from database table to track items. */
{
recombRateMouseMap = cartUsualString(cart, "recombRateMouse.type", rrmoeEnumToString(0));
recombRateMouseType = rrmoeStringToEnum(recombRateMouseMap);
bedLoadItem(tg, "recombRateMouse", (ItemLoader)recombRateMouseLoad);
filterItems(tg, recombRateMouseSetRate, "include");
}

void freeRecombRateMouse(struct track *tg)
/* Free up recombRateMouse items. */
{
recombRateMouseFreeList((struct recombRateMouse**)&tg->items);
}

char *recombRateMouseName(struct track *tg, void *item)
/* Return name of recombRateMouse track item. */
{
struct recombRateMouse *rr = item;
static char buf[32];

switch (recombRateMouseType)
    {
    case rrmoeWiAvg: case rrmoeMgdAvg:
	sprintf(buf, "%3.1f cM/Mb (Avg)", rr->wiAvg);
        break;
    default:
	sprintf(buf, "%3.1f cM/Mb (Avg)", rr->wiAvg);
        break;
    }
return buf;
}

Color recombRateMouseColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color for item in recombRateMouse track item. */
{
struct recombRateMouse *rr = item;
int rcr;
int grayLevel;

rcr = (int)(rr->wiAvg * 200);
grayLevel = grayInRange(rcr, recombRateMin, recombRateMax);
return shadesOfGray[grayLevel];
}

void recombRateMouseMethods(struct track *tg)
/* Make track for mouse recombination rates. */
{
tg->loadItems = loadRecombRateMouse;
tg->freeItems = freeRecombRateMouse;
tg->itemName = recombRateMouseName;
tg->colorShades = shadesOfGray;
tg->itemColor = recombRateMouseColor;
}


/* Chromosome 18 deletions track */
void loadChr18deletions(struct track *tg)
/* Load up chr18deletions from database table to track items. */
{
bedLoadItem(tg, "chr18deletions", (ItemLoader)chr18deletionsLoad);
}

void freeChr18deletions(struct track *tg)
/* Free up chr18deletions items. */
{
chr18deletionsFreeList((struct chr18deletions**)&tg->items);
}

static void drawChr18deletions(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chr18deletions items. */
{
struct chr18deletions *cds;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int shortOff = heightPer/4;
int shortHeight = heightPer - 2*shortOff;
int tallStart, tallEnd, shortStart, shortEnd;
boolean isFull = (vis == tvFull);
double scale = scaleForPixels(width);

memset(colorBin, 0, MAXPIXELS * sizeof(colorBin[0]));

if (vis == tvDense)
    {
    slSort(&tg->items, cmpLfsWhiteToBlack);
    }

for (cds = tg->items; cds != NULL; cds = cds->next)
    {
    int wTall, wShort, end, start, blocks;

    for (blocks = 0; blocks < cds->ssCount; blocks++) 
	{ 
    	tallStart = cds->largeStarts[blocks];
	tallEnd = cds->largeEnds[blocks];
	shortStart = cds->smallStarts[blocks];
	shortEnd = cds->smallEnds[blocks];
	wTall = tallEnd - tallStart;
	wShort = shortEnd - shortStart;

	if (shortStart < tallStart)
	    {
	    end = tallStart;
	    drawScaledBoxSample(vg, shortStart, end, scale, xOff, y+shortOff, shortHeight, color , cds->score);
	    }
	if (shortEnd > tallEnd)
	    {
	    start = tallEnd;
	    drawScaledBoxSample(vg, start, shortEnd, scale, xOff, y+shortOff, shortHeight, color , cds->score);
	    }
	if (tallEnd > tallStart)
	    {
	    drawScaledBoxSample(vg, tallStart, tallEnd, scale, xOff, y, heightPer, color, cds->score);
	    }
	}
    if (isFull) y += lineHeight;
    }
}

void chr18deletionsMethods(struct track *tg)
/* Make track for recombination rates. */
{
tg->loadItems = loadChr18deletions;
tg->freeItems = freeChr18deletions;
tg->drawItems = drawChr18deletions;
}

void loadGenethon(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "mapGenethon", (ItemLoader)mapStsLoad);
}

void freeGenethon(struct track *tg)
/* Free up isochore items. */
{
mapStsFreeList((struct mapSts**)&tg->items);
}

void genethonMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadGenethon;
tg->freeItems = freeGenethon;
}

Color exoFishColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of exofish track item. */
{
struct exoFish *el = item;
int ppt = el->score;
int grayLevel;

grayLevel = grayInRange(ppt, -500, 1000);
return shadesOfSea[grayLevel];
}

void exoFishMethods(struct track *tg)
/* Make track for exoFish. */
{
tg->itemColor = exoFishColor;
}

void loadExoMouse(struct track *tg)
/* Load up exoMouse from database table to track items. */
{
bedLoadItem(tg, "exoMouse", (ItemLoader)roughAliLoad);
if (tg->visibility == tvDense && slCount(tg->items) < 1000)
    {
    slSort(&tg->items, bedCmpScore);
    }
}

void freeExoMouse(struct track *tg)
/* Free up isochore items. */
{
roughAliFreeList((struct roughAli**)&tg->items);
}

char *exoMouseName(struct track *tg, void *item)
/* Return what to display on left column of open track. */
{
struct roughAli *exo = item;
static char name[17];

strncpy(name, exo->name, sizeof(name)-1);
return name;
}


Color exoMouseColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of exoMouse track item. */
{
struct roughAli *el = item;
int ppt = el->score;
int grayLevel;

grayLevel = grayInRange(ppt, -100, 1000);
return shadesOfBrown[grayLevel];
}


void exoMouseMethods(struct track *tg)
/* Make track for exoMouse. */
{
if (sameString(chromName, "chr22") && isPrivateHost)
    tg->visibility = tvDense;
else
    tg->visibility = tvHide;
tg->loadItems = loadExoMouse;
tg->freeItems = freeExoMouse;
tg->itemName = exoMouseName;
tg->itemColor = exoMouseColor;
}

char *xenoMrnaName(struct track *tg, void *item)
/* Return what to display on left column of open track:
 * In this case display 6 letters of organism name followed
 * by mRNA accession. */
{
struct linkedFeatures *lf = item;
char *name = lf->name;
struct sqlConnection *conn = hAllocConn();
char *org = getOrganism(conn, name);
hFreeConn(&conn);
if (org == NULL)
    return name;
else
    {
    static char compName[64];
    char *s;
    s = skipToSpaces(org);
    if (s != NULL)
      *s = 0;
    strncpy(compName, org, 7);
    compName[7] = 0;
    strcat(compName, " ");
    strcat(compName, name);
    return compName;
    }
return name;
}

void xenoMrnaMethods(struct track *tg)
/* Fill in custom parts of xeno mrna alignments. */
{
tg->itemName = xenoMrnaName;
tg->extraUiData = newMrnaUiData(tg->mapName, TRUE);
tg->totalHeight = tgFixedTotalHeightUsingOverflow;
}

void xenoRefGeneMethods(struct track *tg)
/* Make track of known genes from xenoRefSeq. */
{
tg->loadItems = loadRefGene;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = refGeneColor;
}

void mrnaMethods(struct track *tg)
/* Make track of mRNA methods. */
{
tg->extraUiData = newMrnaUiData(tg->mapName, FALSE);
if (isNewChimp(database))
    tg->itemName = xenoMrnaName;
}

char *interProName(struct track *tg, void *item)
{
char condStr[255];
char *desc;
struct linkedFeatures *lf = item;
struct sqlConnection *conn;

conn = hAllocConn();
sprintf(condStr, "interProId='%s' limit 1", lf->name);
desc = sqlGetField(conn, "proteome", "interProXref", "description", condStr);
hFreeConn(&conn);
return(desc);
}

void interProMethods(struct track *tg)
/* Make track of InterPro methods. */
{
tg->itemName    = interProName;
}

void estMethods(struct track *tg)
/* Make track of EST methods - overrides color handler. */
{
tg->drawItems = linkedFeaturesAverageDenseOrientEst;
tg->extraUiData = newMrnaUiData(tg->mapName, FALSE);
tg->totalHeight = tgFixedTotalHeightUsingOverflow;
if (isNewChimp(database))
    tg->itemName = xenoMrnaName;
}

boolean isNonChromColor(Color color)
/* test if color is a non-chrom color (black or gray) */
{
return color == chromColor[0];
}

Color getChromColor(char *name, struct vGfx *vg)
/* Return color index corresponding to chromosome name (assumes that name 
 * points to the first char after the "chr" prefix). */
{
int chromNum = 0;
Color colorNum = 0;
if (!chromosomeColorsMade)
    makeChromosomeShades(vg);
if (atoi(name) != 0)
    chromNum =  atoi(name);
else if (startsWith("U", name))
    chromNum = 26;
else if (startsWith("Y", name))
    chromNum = 24;
else if (startsWith("M", name))
    chromNum = 25;
else if (startsWith("XXI", name))
    chromNum = 21;
else if (startsWith("XX", name))
    chromNum = 20;
else if (startsWith("XIX", name))
    chromNum = 19;
else if (startsWith("XVIII", name))
    chromNum = 18;
else if (startsWith("XVII", name))
    chromNum = 17;
else if (startsWith("XVI", name))
    chromNum = 16;
else if (startsWith("XV", name))
    chromNum = 15;
else if (startsWith("XIV", name))
    chromNum = 14;
else if (startsWith("XIII", name))
    chromNum = 13;
else if (startsWith("XII", name))
    chromNum = 12;
else if (startsWith("XI", name))
    chromNum = 11;
else if (startsWith("X", name))
    /*   stickleback should be chr10   */
    chromNum = 23;
else if (startsWith("IX", name))
    chromNum = 9;
else if (startsWith("VIII", name))
    chromNum = 8;
else if (startsWith("VII", name))
    chromNum = 7;
else if (startsWith("VI", name))
    chromNum = 6;
else if (startsWith("V", name))
    chromNum = 5;
else if (startsWith("IV", name))
    chromNum = 4;
else if (startsWith("III", name))
    chromNum = 3;
else if (startsWith("II", name))
    chromNum = 2;
else if (startsWith("I", name))
    chromNum = 1;
if (chromNum > CHROM_COLORS) chromNum = 0;
colorNum = chromColor[chromNum];
return colorNum;
}

char *chromPrefixes[] = { "chr", "Group", 
			  NULL };

char *scaffoldPrefixes[] = { "scaffold_", "contig_", "SCAFFOLD", "Scaffold", 
			     "Contig", "SuperCont", "super_", "scaffold",
			     NULL };

char *maybeSkipPrefix(char *name, char *prefixes[])
/* Return a pointer into name just past the first matching string from 
 * prefixes[], if found.  If none are found, return NULL. */
{
char *skipped = NULL;
int i = 0;
for (i = 0;  prefixes[i] != NULL;  i++)
    {
    skipped = stringIn(prefixes[i], name);
    if (skipped != NULL)
	{
	skipped += strlen(prefixes[i]);
	break;
	}
    }
return skipped;
}

Color getScaffoldColor(char *scaffoldNum, struct vGfx *vg)
/* assign fake chrom color to scaffold/contig, based on number */
{
int chromNum = atoi(scaffoldNum) % CHROM_COLORS;
if (!chromosomeColorsMade)
    makeChromosomeShades(vg);
if (chromNum < 0 || chromNum > CHROM_COLORS)
    chromNum = 0;
return chromColor[chromNum];
}

Color getSeqColorDefault(char *seqName, struct vGfx *vg, Color defaultColor)
/* Return color of chromosome/scaffold/contig/numeric string, or 
 * defaultColor if seqName doesn't look like any of those. */
{
char *skipped = maybeSkipPrefix(seqName, chromPrefixes);
if (skipped != NULL)
    return getChromColor(skipped, vg);
skipped = maybeSkipPrefix(seqName, scaffoldPrefixes);
if (skipped != NULL)
    return getScaffoldColor(skipped, vg);
if (isdigit(seqName[0]))
    return getScaffoldColor(seqName, vg);
return defaultColor;
}

Color getSeqColor(char *seqName, struct vGfx *vg)
/* Return color of chromosome/scaffold/contig/numeric string. */
{
return getSeqColorDefault(seqName, vg, chromColor[0]);
}

Color lfChromColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of chromosome for linked feature type items
 * where the chromosome is listed somewhere in the lf->name. */
{
struct linkedFeatures *lf = item;
return getSeqColorDefault(lf->name, vg, tg->ixColor);
}

void loadRnaGene(struct track *tg)
/* Load up rnaGene from database table to track items. */
{
bedLoadItem(tg, "rnaGene", (ItemLoader)rnaGeneLoad);
}

void freeRnaGene(struct track *tg)
/* Free up rnaGene items. */
{
rnaGeneFreeList((struct rnaGene**)&tg->items);
}

Color rnaGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of rnaGene track item. */
{
struct rnaGene *el = item;

return (el->isPsuedo ? tg->ixAltColor : tg->ixColor);
}

char *rnaGeneName(struct track *tg, void *item)
/* Return RNA gene name. */
{
struct rnaGene *el = item;
char *full = el->name;
static char abbrev[64];
char *e;

strcpy(abbrev, skipChr(full));
subChar(abbrev, '_', ' ');
abbr(abbrev, " pseudogene");
if ((e = strstr(abbrev, "-related")) != NULL)
    strcpy(e, "-like");
return abbrev;
}

void rnaGeneMethods(struct track *tg)
/* Make track for rna genes . */
{
tg->loadItems = loadRnaGene;
tg->freeItems = freeRnaGene;
tg->itemName = rnaGeneName;
tg->itemColor = rnaGeneColor;
}


Color ncRnaColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of ncRna track item. */
{
char condStr[255];
char *rnaType;
Color color = {MG_GRAY};  /* Set default to gray */
struct rgbColor hAcaColor = {0, 128, 0}; /* darker green, per request by Weber */
Color hColor;
struct sqlConnection *conn;
char *name;

conn = hAllocConn();
hColor = vgFindColorIx(vg, hAcaColor.r, hAcaColor.g, hAcaColor.b);

name = tg->itemName(tg, item);
sprintf(condStr, "name='%s'", name);
rnaType = sqlGetField(conn, database, "ncRna", "type", condStr);

if (sameWord(rnaType, "miRNA"))    color = MG_RED;
if (sameWord(rnaType, "misc_RNA")) color = MG_BLACK;
if (sameWord(rnaType, "snRNA"))    color = MG_BLUE;
if (sameWord(rnaType, "snoRNA"))   color = MG_MAGENTA;
if (sameWord(rnaType, "rRNA"))     color = MG_CYAN;
if (sameWord(rnaType, "scRNA"))    color = MG_YELLOW;
if (sameWord(rnaType, "Mt_tRNA"))  color = MG_GREEN;
if (sameWord(rnaType, "Mt_rRNA"))  color = hColor;

hFreeConn(&conn);
return(color);
}

void ncRnaMethods(struct track *tg)
/* Make track for ncRna. */
{
tg->itemColor = ncRnaColor;
}

Color wgRnaColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of wgRna track item. */
{
char condStr[255];
char *rnaType;
Color color = {MG_BLACK};  /* Set default to black.  But, if we got black, something is wrong. */
Color hColor;
struct rgbColor hAcaColor = {0, 128, 0}; /* darker green */
struct sqlConnection *conn;
char *name;

conn = hAllocConn();
hColor = vgFindColorIx(vg, hAcaColor.r, hAcaColor.g, hAcaColor.b);

name = tg->itemName(tg, item);
sprintf(condStr, "name='%s'", name);
rnaType = sqlGetField(conn, database, "wgRna", "type", condStr);
if (sameWord(rnaType, "miRna"))   color = MG_RED;
if (sameWord(rnaType, "HAcaBox")) color = hColor;
if (sameWord(rnaType, "CDBox"))   color = MG_BLUE;
if (sameWord(rnaType, "scaRna"))  color = MG_MAGENTA;

hFreeConn(&conn);
return(color);
}

void wgRnaMethods(struct track *tg)
/* Make track for wgRna. */
{
tg->itemColor = wgRnaColor;
}

Color stsColor(struct vGfx *vg, int altColor, 
	char *genethonChrom, char *marshfieldChrom, 
	char *fishChrom, int ppt)
/* Return color given info about marker. */
{
if (genethonChrom[0] != '0' || marshfieldChrom[0] != '0')
    {
    if (ppt >= 900)
       return MG_BLUE;
    else
       return altColor;
    }
else if (fishChrom[0] != '0')
    {
    static int greenIx = -1;
    if (greenIx < 0)
        greenIx = vgFindColorIx(vg, 0, 200, 0);
    return greenIx;
    }
else
    {
    if (ppt >= 900)
       return MG_BLACK;
    else
       return MG_GRAY;
    }
}

void loadStsMarker(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
bedLoadItem(tg, "stsMarker", (ItemLoader)stsMarkerLoad);
}

void freeStsMarker(struct track *tg)
/* Free up stsMarker items. */
{
stsMarkerFreeList((struct stsMarker**)&tg->items);
}

Color stsMarkerColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of stsMarker track item. */
{
struct stsMarker *el = item;
return stsColor(vg, tg->ixAltColor, el->genethonChrom, el->marshfieldChrom,
    el->fishChrom, el->score);
}

void stsMarkerMethods(struct track *tg)
/* Make track for sts markers. */
{
tg->loadItems = loadStsMarker;
tg->freeItems = freeStsMarker;
tg->itemColor = stsMarkerColor;
}

char *stsMapFilter;
char *stsMapMap;
enum stsMapOptEnum stsMapType;
int stsMapFilterColor = MG_BLACK;

boolean stsMapFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct stsMap *el = item;
switch (stsMapType)
    {
    case smoeGenetic:
	return el->genethonChrom[0] != '0' || el->marshfieldChrom[0] != '0'
	    || el->decodeChrom[0] != '0';
        break;
    case smoeGenethon:
	return el->genethonChrom[0] != '0';
        break;
    case smoeMarshfield:
	return el->marshfieldChrom[0] != '0';
        break;
    case smoeDecode:
	return el->decodeChrom[0] != '0';
        break;
    case smoeGm99:
	return el->gm99Gb4Chrom[0] != '0';
        break;
    case smoeWiYac:
	return el->wiYacChrom[0] != '0';
        break;
    case smoeWiRh:
	return el->wiRhChrom[0] != '0';
        break;
    case smoeTng:
	return el->shgcTngChrom[0] != '0';
        break;
    default:
	return FALSE;
        break;
    }
}


void loadStsMap(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
stsMapType = smoeStringToEnum(stsMapMap);
bedLoadItem(tg, "stsMap", (ItemLoader)stsMapLoad);
filterItems(tg, stsMapFilterItem, stsMapFilter);
stsMapFilterColor = getFilterColor(stsMapFilter, MG_BLACK);
}

void loadStsMap28(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
stsMapType = smoeStringToEnum(stsMapMap);
bedLoadItem(tg, "stsMap", (ItemLoader)stsMapLoad28);
filterItems(tg, stsMapFilterItem, stsMapFilter);
stsMapFilterColor = getFilterColor(stsMapFilter, MG_BLACK);
}

void freeStsMap(struct track *tg)
/* Free up stsMap items. */
{
stsMapFreeList((struct stsMap**)&tg->items);
}

Color stsMapColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of stsMap track item. */
{
if (stsMapFilterItem(tg, item))
    return stsMapFilterColor;
else
    {
    struct stsMap *el = item;
    if (el->score >= 900)
        return MG_BLACK;
    else
        return MG_GRAY;
    }
}


void stsMapMethods(struct track *tg)
/* Make track for sts markers. */
{
struct sqlConnection *conn = hAllocConn();
if (sqlCountColumnsInTable(conn, "stsMap") == 26) 
    {
    tg->loadItems = loadStsMap;
    }
else
    {
    tg->loadItems = loadStsMap28;
    }
hFreeConn(&conn);
tg->freeItems = freeStsMap;
tg->itemColor = stsMapColor;
}

char *stsMapMouseFilter;
char *stsMapMouseMap;
enum stsMapMouseOptEnum stsMapMouseType;
int stsMapMouseFilterColor = MG_BLACK;

boolean stsMapMouseFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct stsMapMouseNew *el = item;
switch (stsMapMouseType)
    {
    case smmoeGenetic:
       return el->wigChr[0] != '\0' || el->mgiChrom[0] != '\0';
       break;
    case smmoeWig:
	return el->wigChr[0] != '\0';
        break;
    case smmoeMgi:
	return el->mgiChrom[0] != '\0';
        break;
    case smmoeRh:
	return el->rhChrom[0] != '\0';
        break;
    default:
	return FALSE;
        break;
    }
}

void loadStsMapMouse(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
stsMapMouseFilter = cartUsualString(cart, "stsMapMouse.filter", "blue");
stsMapMouseMap = cartUsualString(cart, "stsMapMouse.type", smmoeEnumToString(0));
stsMapMouseType = smmoeStringToEnum(stsMapMouseMap);
bedLoadItem(tg, "stsMapMouseNew", (ItemLoader)stsMapMouseNewLoad);
filterItems(tg, stsMapMouseFilterItem, stsMapMouseFilter);
stsMapMouseFilterColor = getFilterColor(stsMapMouseFilter, MG_BLACK);

}

void freeStsMapMouse(struct track *tg)
/* Free up stsMap items. */
{
stsMapMouseNewFreeList((struct stsMapMouseNew**)&tg->items);
}

Color stsMapMouseColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of stsMap track item. */
{
struct stsMapMouseNew *el = item;
if (stsMapMouseFilterItem(tg, item))
    {
    if(el->score >= 900)
	return stsMapMouseFilterColor;
    else
	{
	switch(stsMapMouseFilterColor)
	    {
	    case 1:
		return MG_GRAY;
		break;
	    case 2:
		return (vgFindColorIx(vg, 240, 128, 128)); //Light red
		break;
	    case 3:
		return (vgFindColorIx(vg, 154, 205, 154)); // light green	
		break;
	    case 4:
		return (vgFindColorIx(vg, 176, 226, 255)); // light blue
	    default:
		return MG_GRAY;
	    }
	}
    }
else
    {
    if(el->score < 900)
	return MG_GRAY;
    else
	return MG_BLACK;
    }
}

void stsMapMouseMethods(struct track *tg)
/* Make track for sts markers. */
{
tg->loadItems = loadStsMapMouse;
tg->freeItems = freeStsMapMouse;
tg->itemColor = stsMapMouseColor;
}

char *stsMapRatFilter;
char *stsMapRatMap;
enum stsMapRatOptEnum stsMapRatType;
int stsMapRatFilterColor = MG_BLACK;

boolean stsMapRatFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct stsMapRat *el = item;
switch (stsMapRatType)
    {
    case smroeGenetic:
       return el->fhhChr[0] != '\0' || el->shrspChrom[0] != '\0';
       break;
    case smroeFhh:
	return el->fhhChr[0] != '\0';
        break;
    case smroeShrsp:
	return el->shrspChrom[0] != '\0';
        break;
    case smroeRh:
	return el->rhChrom[0] != '\0';
        break;
    default:
	return FALSE;
        break;
    }
}


void loadStsMapRat(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
stsMapRatFilter = cartUsualString(cart, "stsMapRat.filter", "blue");
stsMapRatMap = cartUsualString(cart, "stsMapRat.type", smroeEnumToString(0));
stsMapRatType = smroeStringToEnum(stsMapRatMap);
bedLoadItem(tg, "stsMapRat", (ItemLoader)stsMapRatLoad);
filterItems(tg, stsMapRatFilterItem, stsMapRatFilter);
stsMapRatFilterColor = getFilterColor(stsMapRatFilter, MG_BLACK);
}

void freeStsMapRat(struct track *tg)
/* Free up stsMap items. */
{
stsMapRatFreeList((struct stsMapRat**)&tg->items);
}

Color stsMapRatColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of stsMap track item. */
{
if (stsMapRatFilterItem(tg, item))
    return stsMapRatFilterColor;
else
    {
    struct stsMapRat *el = item;
    if (el->score >= 900)
        return MG_BLACK;
    else
        return MG_GRAY;
    }
}

void stsMapRatMethods(struct track *tg)
/* Make track for sts markers. */
{
tg->loadItems = loadStsMapRat;
tg->freeItems = freeStsMapRat;
tg->itemColor = stsMapRatColor;
}

void loadGenMapDb(struct track *tg)
/* Load up genMapDb from database table to track items. */
{
bedLoadItem(tg, "genMapDb", (ItemLoader)genMapDbLoad);
}

void freeGenMapDb(struct track *tg)
/* Free up genMapDb items. */
{
genMapDbFreeList((struct genMapDb**)&tg->items);
}

void genMapDbMethods(struct track *tg)
/* Make track for GenMapDb Clones */
{
tg->loadItems = loadGenMapDb;
tg->freeItems = freeGenMapDb;
}

char *fishClonesFilter;
char *fishClonesMap;
enum fishClonesOptEnum fishClonesType;
int fishClonesFilterColor = MG_GREEN;

boolean fishClonesFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct fishClones *el = item;
int i;
switch (fishClonesType)
    {
    case fcoeFHCRC:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"FHCRC")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeNCI:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"NCI")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeSC:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"SC")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeRPCI:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"RPCI")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeCSMC:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"CSMC")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeLANL:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"LANL")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeUCSF:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"UCSF")) 
	        return TRUE;
        return FALSE;
        break;
    default:
	return FALSE;
        break;
    }
}

void loadFishClones(struct track *tg)
/* Load up fishClones from database table to track items. */
{
fishClonesFilter = cartUsualString(cart, "fishClones.filter", "green");
fishClonesMap = cartUsualString(cart, "fishClones.type", fcoeEnumToString(0));
fishClonesType = fcoeStringToEnum(fishClonesMap);
bedLoadItem(tg, "fishClones", (ItemLoader)fishClonesLoad);
filterItems(tg, fishClonesFilterItem, fishClonesFilter);
fishClonesFilterColor = getFilterColor(fishClonesFilter, 0);
}


void freeFishClones(struct track *tg)
/* Free up fishClones items. */
{
fishClonesFreeList((struct fishClones**)&tg->items);
}

Color fishClonesColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of fishClones track item. */
{
if ((fishClonesFilterItem(tg, item)) && (fishClonesFilterColor))
    return fishClonesFilterColor;
else
    return tg->ixColor;
}

void fishClonesMethods(struct track *tg)
/* Make track for FISH clones. */
{
tg->loadItems = loadFishClones;
tg->freeItems = freeFishClones;
tg->itemColor = fishClonesColor;
}

void loadSynteny(struct track *tg)
{
bedLoadItem(tg, tg->mapName, (ItemLoader)synteny100000Load);
slSort(&tg->items, bedCmp);
}

void freeSynteny(struct track *tg)
{
synteny100000FreeList((struct synteny100000**)&tg->items);
}

void loadMouseOrtho(struct track *tg)
{
bedLoadItem(tg, "mouseOrtho", (ItemLoader)mouseOrthoLoad);
slSort(&tg->items, bedCmpPlusScore);
}

void freeMouseOrtho(struct track *tg)
{
mouseOrthoFreeList((struct mouseOrtho**)&tg->items);
}

Color mouseOrthoItemColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of psl track item based on chromsome. */
{
char chromStr[20];     
struct mouseOrtho *ms = item;
if (strlen(ms->name) == 8)
{
    strncpy(chromStr,(char *)(ms->name+1),1);
    chromStr[1] = '\0';
}
else if (strlen(ms->name) == 9)
{
    strncpy(chromStr,(char *)(ms->name+1),2);
    chromStr[2] = '\0';
}
else
    strncpy(chromStr,ms->name,2);
return ((Color)getChromColor(chromStr, vg));
}

void mouseOrthoMethods(struct track *tg)
{
char option[128];
char *optionStr ;
tg->loadItems = loadMouseOrtho;
tg->freeItems = freeMouseOrtho;

safef( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "on");
if( sameString( optionStr, "on" )) /*use anti-aliasing*/
    tg->itemColor = mouseOrthoItemColor;
else
    tg->itemColor = NULL;
tg->drawName = TRUE;
}

void loadHumanParalog(struct track *tg)
{
bedLoadItem(tg, "humanParalog", (ItemLoader)humanParalogLoad);
slSort(&tg->items, bedCmpPlusScore);
}

void freeHumanParalog(struct track *tg)
{
humanParalogFreeList((struct humanParalog**)&tg->items);
}

Color humanParalogItemColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of psl track item based on chromsome. */
{
char chromStr[20];     
struct humanParalog *ms = item;
if (strlen(ms->name) == 8)
{
    strncpy(chromStr,(char *)(ms->name+1),1);
    chromStr[1] = '\0';
}
else if (strlen(ms->name) == 9)
{
    strncpy(chromStr,(char *)(ms->name+1),2);
    chromStr[2] = '\0';
}
else
    strncpy(chromStr,ms->name,2);
return ((Color)getChromColor(chromStr, vg));
}

void humanParalogMethods(struct track *tg)
{
char option[128];
char *optionStr ;
tg->loadItems = loadHumanParalog;
tg->freeItems = freeHumanParalog;

safef( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "on");
if( sameString( optionStr, "on" )) /*use anti-aliasing*/
    tg->itemColor = humanParalogItemColor;
else
    tg->itemColor = NULL;
tg->drawName = TRUE;
}

Color syntenyItemColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of psl track item based on chromsome. */
{
char chromStr[20];     
struct bed *ms = item;
if (strlen(ms->name) == 8)
    {
    strncpy(chromStr,(char *)(ms->name+1),1);
    chromStr[1] = '\0';
    }
else if (strlen(ms->name) == 9)
    {
    strncpy(chromStr,(char *)(ms->name+1),2);
    chromStr[2] = '\0';
    }
else
    {
    strncpy(chromStr,ms->name+3,2);
    chromStr[2] = '\0';
    }
return ((Color)getChromColor(chromStr, vg));
}

void syntenyMethods(struct track *tg)
{
tg->loadItems = loadSynteny;
tg->freeItems = freeSynteny;
tg->itemColor = syntenyItemColor;
tg->drawName = FALSE;
tg->subType = lfWithBarbs ;
}

void loadMouseSyn(struct track *tg)
/* Load up mouseSyn from database table to track items. */
{
bedLoadItem(tg, "mouseSyn", (ItemLoader)mouseSynLoad);
}


void freeMouseSyn(struct track *tg)
/* Free up mouseSyn items. */
{
mouseSynFreeList((struct mouseSyn**)&tg->items);
}

Color mouseSynItemColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of mouseSyn track item. */
{
char chromStr[20];     
struct mouseSyn *ms = item;

strncpy(chromStr, ms->name+strlen("mouse "), 2);
chromStr[2] = '\0';
return ((Color)getChromColor(chromStr, vg));
}

void mouseSynMethods(struct track *tg)
/* Make track for mouseSyn. */
{
tg->loadItems = loadMouseSyn;
tg->freeItems = freeMouseSyn;
tg->itemColor = mouseSynItemColor;
tg->drawName = TRUE;
}

void loadMouseSynWhd(struct track *tg)
/* Load up mouseSynWhd from database table to track items. */
{
bedLoadItem(tg, "mouseSynWhd", (ItemLoader)mouseSynWhdLoad);
}

void freeMouseSynWhd(struct track *tg)
/* Free up mouseSynWhd items. */
{
mouseSynWhdFreeList((struct mouseSynWhd**)&tg->items);
}

Color mouseSynWhdItemColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of mouseSynWhd track item. */
{
char chromStr[20];
struct mouseSynWhd *ms = item;

if (startsWith("chr", ms->name))
    {
    strncpy(chromStr, ms->name+strlen("chr"), 2);
    chromStr[2] = '\0';
    return((Color)getChromColor(chromStr, vg));
    }
else
    {
    return(tg->ixColor);
    }
}

void mouseSynWhdMethods(struct track *tg)
/* Make track for mouseSyn. */
{
tg->loadItems = loadMouseSynWhd;
tg->freeItems = freeMouseSynWhd;
tg->itemColor = mouseSynWhdItemColor;
tg->subType = lfWithBarbs;
}

void lfChromColorMethods(struct track *tg)
/* Standard linked features methods + chrom-coloring (for contrib. synt). */
{
tg->itemColor = lfChromColor;
tg->itemNameColor = NULL;
}

Color bedChromColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of chromosome for bed type items
 * where the chromosome/scaffold is listed somewhere in the bed->name. */
{
struct bed *bed = item;
return getSeqColorDefault(bed->name, vg, tg->ixColor);
}

void bedChromColorMethods(struct track *tg)
/* Standard simple bed methods + chrom-coloring (for contrib. synt). */
{
tg->itemColor = bedChromColor;
}

char *colonTruncName(struct track *tg, void *item)
/* Return name of bed track item, chopped at the first ':'. */
{
struct bed *bed = item;
char *ptr = NULL;
if (bed->name == NULL)
    return "";
ptr = strchr(bed->name, ':');
if (ptr != NULL)
    *ptr = 0;
return bed->name;
}

void deweySyntMethods(struct track *tg)
/* Standard simple bed methods + chrom-coloring + name-chopping. */
{
bedChromColorMethods(tg);
tg->itemName = colonTruncName;
}


void loadEnsPhusionBlast(struct track *tg)
/* Load up ensPhusionBlast from database table to track items. */
{
struct ensPhusionBlast *epb;
char *ptr;
char buf[16];

bedLoadItem(tg, tg->mapName, (ItemLoader)ensPhusionBlastLoad);
// for name, append abbreviated starting position to the xeno chrom:
for (epb=tg->items;  epb != NULL;  epb=epb->next)
    {
    ptr = strchr(epb->name, '.');
    if (ptr == NULL)
	ptr = epb->name;
    else
	ptr++;
    safef(buf, sizeof(buf), "%s %dk", ptr, (int)(epb->xenoStart/1000));
    free(epb->name);
    epb->name = cloneString(buf);
    }
}

void freeEnsPhusionBlast(struct track *tg)
/* Free up ensPhusionBlast items. */
{
ensPhusionBlastFreeList((struct ensPhusionBlast**)&tg->items);
}

Color ensPhusionBlastItemColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of ensPhusionBlast track item. */
{
struct ensPhusionBlast *epb = item;
return getSeqColorDefault(epb->name, vg, tg->ixColor);
}

void ensPhusionBlastMethods(struct track *tg)
/* Make track for mouseSyn. */
{
tg->loadItems = loadEnsPhusionBlast;
tg->freeItems = freeEnsPhusionBlast;
tg->itemColor = ensPhusionBlastItemColor;
tg->subType = lfWithBarbs;
}

void tetWabaMethods(struct track *tg)
/* Make track for Tetraodon alignments. */
{
wabaMethods(tg);
tg->customPt = "_tet_waba";
}

void cbrWabaMethods(struct track *tg)
/* Make track for C briggsae alignments. */
{
wabaMethods(tg);
tg->customPt = "_wabaCbr";
cartSetInt(cart, "cbrWaba.start", winStart);
cartSetInt(cart, "cbrWaba.end", winEnd);
}

void bactigLoad(struct track *tg)
/* Load up bactigs from database table to track items. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct bactigPos *bactigList = NULL, *bactig;
int rowOffset;

/* Get the bactigs and load into tg->items. */
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
		 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bactig = bactigPosLoad(row+rowOffset);
    slAddHead(&bactigList, bactig);
    }
slReverse(&bactigList);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = bactigList;
}

void bactigFree(struct track *tg)
/* Free up bactigTrackGroup items. */
{
bactigPosFreeList((struct bactigPos**)&tg->items);
}

char *abbreviateBactig(char *string, MgFont *font, int width)
/* Return a string abbreviated enough to fit into space. */
{
int textWidth;

/* There's no abbreviating to do for bactig names; just return the name 
 * if it fits, NULL if it doesn't fit. */
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;
return NULL;
}

void bactigMethods(struct track *tg)
/* Make track for bactigPos */
{
tg->loadItems = bactigLoad;
tg->freeItems = bactigFree;
// tg->drawItems = bactigDraw;
// tg->canPack = FALSE;
}


void gapLoad(struct track *tg)
/* Load up clone alignments from database tables and organize. */
{
bedLoadItem(tg, "gap", (ItemLoader)agpGapLoad);
}

void gapFree(struct track *tg)
/* Free up gap items. */
{
agpGapFreeList((struct agpGap**)&tg->items);
}

char *gapName(struct track *tg, void *item)
/* Return name of gap track item. */
{
static char buf[24];
struct agpGap *gap = item;
sprintf(buf, "%s %s", gap->type, gap->bridge);
return buf;
}

static void gapDrawAt(struct track *tg, void *item,
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw gap items. */
{
struct agpGap *gap = item;
int heightPer = tg->heightPer;
int x1,x2,w;
int halfSize = heightPer/2;

x1 = round((double)((int)gap->chromStart-winStart)*scale) + xOff;
x2 = round((double)((int)gap->chromEnd-winStart)*scale) + xOff;
w = x2-x1;
if (w < 1)
    w = 1;
if (sameString(gap->bridge, "no"))
    vgBox(vg, x1, y, w, heightPer, color);
else  /* Leave white line in middle of bridged gaps. */
    {
    vgBox(vg, x1, y, w, halfSize, color);
    vgBox(vg, x1, y+heightPer-halfSize, w, halfSize, color);
    }
}

void gapMethods(struct track *tg)
/* Make track for positions of all gaps. */
{
tg->loadItems = gapLoad;
tg->freeItems = gapFree;
tg->drawItemAt = gapDrawAt;
tg->drawItems = genericDrawItems;
tg->itemName = gapName;
tg->mapItemName = gapName;
}

int pslWScoreScale(struct pslWScore *psl, boolean isXeno, float maxScore)
/* takes the score field and scales it to the correct shade using maxShade and maxScore */
{
/* move from float to int by multiplying by 100 */
int score = (int)(100 * psl->score);
int level;
level = grayInRange(score, 0, (int)(100 * maxScore));
if(level==1) level++;
return level;
}

struct linkedFeatures *lfFromPslWScore(struct pslWScore *psl, int sizeMul, boolean isXeno, float maxScore)
/* Create a linked feature item from pslx.  Pass in sizeMul=1 for DNA, 
 * sizeMul=3 for protein. */
{
unsigned *starts = psl->tStarts;
unsigned *sizes = psl->blockSizes;
int i, blockCount = psl->blockCount;
int grayIx = pslWScoreScale(psl, isXeno, maxScore);
struct simpleFeature *sfList = NULL, *sf;
struct linkedFeatures *lf;
boolean rcTarget = (psl->strand[1] == '-');

AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, psl->qName, sizeof(lf->name));
lf->orientation = orientFromChar(psl->strand[0]);
if (rcTarget)
    lf->orientation = -lf->orientation;
for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    sf->start = sf->end = starts[i];
    sf->end += sizes[i]*sizeMul;
    if (rcTarget)
        {
	int s, e;
	s = psl->tSize - sf->end;
	e = psl->tSize - sf->start;
	sf->start = s;
	sf->end = e;
	}
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
linkedFeaturesBoundsAndGrays(lf);
lf->start = psl->tStart;	/* Correct for rounding errors... */
lf->end = psl->tEnd;
return lf;
}

struct linkedFeatures *lfFromPslsWScoresInRange(char *table, int start, int end, char *chromName, boolean isXeno, float maxScore)
/* Return linked features from range of table with the scores scaled appriately */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeatures *lfList = NULL, *lf;

sr = hRangeQuery(conn,table,chromName,start,end,NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    struct pslWScore *pslWS = pslWScoreLoad(row);
    lf = lfFromPslWScore(pslWS, 1, FALSE, maxScore);
    slAddHead(&lfList, lf);
    pslWScoreFree(&pslWS);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfList;
}

void loadUniGeneAli(struct track *tg)
{
tg->items = lfFromPslsWScoresInRange("uniGene", winStart, winEnd, 
	chromName,FALSE, 1.0);
}

void uniGeneMethods(struct track *tg)
/* Load up uniGene methods - a slight specialization of
 * linked features. */
{
linkedFeaturesMethods(tg);
tg->loadItems = loadUniGeneAli;
tg->colorShades = shadesOfGray;
}


struct linkedFeatures *bedFilterMinLength(struct bed *bedList, int minLength ) 
{
struct linkedFeatures *lf = NULL, *lfList=NULL;
struct bed *bed = NULL;

/* traditionally if there is nothing to show
   show nothing .... */
if(bedList == NULL)
    return NULL;

for(bed = bedList; bed != NULL; bed = bed->next)
    {
	/* create the linked features */
    if( bed->chromEnd - bed->chromStart >=  minLength )
        {
        lf = lfFromBed(bed);
        slAddHead(&lfList, lf);
        }
    }

slReverse(&lfList);
return lfList;
}

void lfFromAncientRBed(struct track *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
minimum munber of aligned bases cutoff */
{
struct bed *bedList= NULL;
int ancientRMinLength = atoi(cartUsualString(cart, "ancientR.minLength", "50"));
bedList = tg->items;
tg->items = bedFilterMinLength(bedList, ancientRMinLength);
bedFreeList(&bedList);
}


void loadAncientR(struct track *tg)
/* Load up ancient repeats from database table to track items
 * filtering out those below a certain length threshold,
   in number of aligned bases. */
{
bedLoadItem(tg, "ancientR", (ItemLoader)bedLoad12);
lfFromAncientRBed(tg);
}


void ancientRMethods(struct track *tg)
/* setup special methods for ancientR track */
{
tg->loadItems = loadAncientR;
//tg->trackFilter = lfsFromAncientRBed;
}


Color getExprDataColor(float val, float maxDeviation, boolean RG_COLOR_SCHEME ) 
/** Returns the appropriate Color from the shadesOfGreen and shadesOfRed arrays
 * @param float val - acutual data to be represented
 * @param float maxDeviation - maximum (and implicitly minimum) values represented
 * @param boolean RG_COLOR_SCHEME - are we red/green(TRUE) or red/blue(FALSE) ?
 */
{
float absVal = fabs(val);
int colorIndex = 0;

/* cap the value to be less than or equal to maxDeviation */
if(absVal > maxDeviation)
    absVal = maxDeviation;

/* project the value into the number of colors we have.  
 *   * i.e. if val = 1.0 and max is 2.0 and number of shades is 16 then index would be
 * 1 * 15 /2.0 = 7.5 = 7
 */
if(maxDeviation == 0) 
    errAbort("ERROR: hgTracksExample::getExprDataColor() maxDeviation can't be zero\n"); 

colorIndex = (int)(absVal * maxRGBShade/maxDeviation);

/* Return the correct color depending on color scheme and shades */
if(RG_COLOR_SCHEME) 
    {
    if(val > 0) 
	return shadesOfRed[colorIndex];
    else 
	return shadesOfGreen[colorIndex];
    }
else 
    {
    if(val > 0) 
	return shadesOfRed[colorIndex];
    else 
	return shadesOfBlue[colorIndex];
    }
}

/* Use the RepeatMasker style code to generate the
 * the Comparative Genomic Hybridization track */

static struct repeatItem *otherCghItem = NULL;
/*static char *cghClassNames[] = {
  "Breast", "CNS", "Colon", "Leukemia", "Lung", "Melanoma", "Ovary", "Prostate", "Renal",
  };*/
static char *cghClasses[] = {
  "BREAST", "CNS", "COLON", "LEUKEMIA", "LUNG", "MELANOMA", "OVARY", "PROSTATE", "RENAL",
  };
/* static char *cghClasses[] = {
  "BT549(D439b)", "HS578T(D268a)", "MCF7(D820b)", "MCF7ADR(D212a)", "MDA-231(D213b)", "MDA-435(D266a)", "MDA-N(D266b)", "T47D(D212b)", 
  }; */

struct repeatItem *makeCghItems()
/* Make the stereotypical CGH tracks */
{
struct repeatItem *ri, *riList = NULL;
int i;
int numClasses = ArraySize(cghClasses);
for (i=0; i<numClasses; ++i)
    {
    AllocVar(ri);
    ri->class = cghClasses[i];
    ri->className = cghClasses[i];
    slAddHead(&riList, ri);
    }
otherCghItem = riList;
slReverse(&riList);
return riList;
}

void cghLoadTrack(struct track *tg)
/* Load up CGH tracks.  (Will query database during drawing for a change.) */
{
tg->items = makeCghItems();
}

static void cghDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
int baseWidth = seqEnd - seqStart;
struct repeatItem *cghi;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
Color col;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct cgh cghRecord;

/* Set up the shades of colors */
if (!exprBedColorsMade)
    makeRedGreenShades(vg);

if (isFull)
    {
    /* Create tissue specific average track */
    struct hash *hash = newHash(6);

    for (cghi = tg->items; cghi != NULL; cghi = cghi->next)
        {
	cghi->yOffset = y;
	y += lineHeight;
	hashAdd(hash, cghi->class, cghi);
	}
    sr = hRangeQuery(conn, "cgh", chromName, winStart, winEnd, "type = 2", &rowOffset);
    /* sr = hRangeQuery(conn, "cgh", chromName, winStart, winEnd, "type = 3", &rowOffset); */
    while ((row = sqlNextRow(sr)) != NULL)
        {
	cghStaticLoad(row+rowOffset, &cghRecord);
	cghi = hashFindVal(hash, cghRecord.tissue); 
	/* cghi = hashFindVal(hash, cghRecord.name); */
	if (cghi == NULL)
	   cghi = otherCghItem;
	col = getExprDataColor((cghRecord.score * -1), 0.7, TRUE);
	x1 = roundingScale(cghRecord.chromStart-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(cghRecord.chromEnd-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w <= 0)
	    w = 1;
	vgBox(vg, x1, cghi->yOffset, w, heightPer, col);
        }
    freeHash(&hash);
    }
else
    {
    sr = hRangeQuery(conn, "cgh", chromName, winStart, winEnd, "type = 1", &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	cghStaticLoad(row+rowOffset, &cghRecord);
	col = getExprDataColor((cghRecord.score * -1), 0.5, TRUE);
	x1 = roundingScale(cghRecord.chromStart-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(cghRecord.chromEnd-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w <= 0)
	  w = 1;
	vgBox(vg, x1, yOff, w, heightPer, col);
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void cghMethods(struct track *tg)
/* Make track for CGH experiments. */
{
repeatMethods(tg);
tg->loadItems = cghLoadTrack;
tg->drawItems = cghDraw;
tg->colorShades = shadesOfGray;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
}


void loadMcnBreakpoints(struct track *tg)
/* Load up MCN breakpoints from database table to track items. */
{
bedLoadItem(tg, "mcnBreakpoints", (ItemLoader)mcnBreakpointsLoad);
}

void freeMcnBreakpoints(struct track *tg)
/* Free up MCN Breakpoints items. */
{
mcnBreakpointsFreeList((struct mcnBreakpoints**)&tg->items);
}

void mcnBreakpointsMethods(struct track *tg)
/* Make track for mcnBreakpoints. */
{
tg->loadItems = loadMcnBreakpoints;
tg->freeItems = freeMcnBreakpoints;
}


static void drawTriangle(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw triangle items.   Relies mostly on bedDrawSimple, but does put
 * a horizontal box connecting items in full mode. */
{
/* In dense mode try and draw golden background for promoter regions. */
if (vis == tvDense)
    {
    if (hTableExists("rnaCluster"))
        {
	int heightPer = tg->heightPer;
	Color gold = vgFindColorIx(vg, 250,190,60);
	int promoSize = 1000;
	int rowOffset;
	double scale = scaleForPixels(width);
	struct sqlConnection *conn = hAllocConn();
	struct sqlResult *sr = hRangeQuery(conn, "rnaCluster", chromName, 
		winStart - promoSize, winEnd + promoSize, NULL, &rowOffset);
	char **row;
	// vgBox(vg, xOff, yOff, width, heightPer, gold);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int start, end;
	    row += rowOffset;
	    if (row[5][0] == '-')
	        {
		start = atoi(row[2]);
		end = start + promoSize;
		}
	    else
	        {
		end = atoi(row[1]);
		start = end - promoSize;
		}
	    drawScaledBox(vg, start, end, scale, xOff, yOff, heightPer, gold);
	    }

	hFreeConn(&conn);
	}
    }
bedDrawSimple(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
}

void triangleMethods(struct track *tg)
/* Register custom methods for regulatory triangle track. */
{
tg->drawItems = drawTriangle;
}

static void drawEranModule(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw triangle items.   Relies mostly on bedDrawSimple, but does put
 * a horizontal box connecting items in full mode. */
{
/* In dense mode try and draw golden background for promoter regions. */
if (vis == tvDense)
    {
    if (hTableExists("esRegUpstreamRegion"))
        {
	int heightPer = tg->heightPer;
	Color gold = vgFindColorIx(vg, 250,190,60);
	int rowOffset;
	double scale = scaleForPixels(width);
	struct sqlConnection *conn = hAllocConn();
	struct sqlResult *sr = hRangeQuery(conn, "esRegUpstreamRegion", 
		chromName, winStart, winEnd, 
		NULL, &rowOffset);
	char **row;
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int start, end;
	    row += rowOffset;
	    start = atoi(row[1]);
	    end = atoi(row[2]);
	    drawScaledBox(vg, start, end, scale, xOff, yOff, heightPer, gold);
	    }
	hFreeConn(&conn);
	}
    }
bedDrawSimple(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
}

void eranModuleMethods(struct track *tg)
/* Register custom methods for eran regulatory module methods. */
{
tg->drawItems = drawEranModule;
}


void smallBreak()
/* Draw small horizontal break */
{
hPrintf("<FONT SIZE=1><BR></FONT>\n");
}

void drawColoredButtonBox(struct vGfx *vg, int x, int y, int w, int h, 
                                int enabled, Color shades[])
/* draw button box, providing shades of the desired button color */
{
int light = shades[1], mid = shades[2], dark = shades[4];
if (enabled) 
    {
    vgBox(vg, x, y, w, 1, light);
    vgBox(vg, x, y+1, 1, h-1, light);
    vgBox(vg, x+1, y+1, w-2, h-2, mid);
    vgBox(vg, x+1, y+h-1, w-1, 1, dark);
    vgBox(vg, x+w-1, y+1, 1, h-1, dark);
    }
else				/* try to make the button look as if
				 * it is already depressed */
    {
    vgBox(vg, x, y, w, 1, dark);
    vgBox(vg, x, y+1, 1, h-1, dark);
    vgBox(vg, x+1, y+1, w-2, h-2, light);
    vgBox(vg, x+1, y+h-1, w-1, 1, light);
    vgBox(vg, x+w-1, y+1, 1, h-1, light);
    }
}

void drawGrayButtonBox(struct vGfx *vg, int x, int y, int w, int h, int enabled)
/* Draw a gray min-raised looking button. */
{
    drawColoredButtonBox(vg, x, y, w, h, enabled, shadesOfGray);
}

void drawBlueButtonBox(struct vGfx *vg, int x, int y, int w, int h, int enabled)
/* Draw a blue min-raised looking button. */
{
    drawColoredButtonBox(vg, x, y, w, h, enabled, shadesOfSea);
}

void drawButtonBox(struct vGfx *vg, int x, int y, int w, int h, int enabled)
/* Draw a standard (gray) min-raised looking button. */
{
    drawGrayButtonBox(vg, x, y, w, h, enabled);
}

void beforeFirstPeriod( char *str )
{
char *t = rindex( str, '.' );

if( t == NULL )
    return;
else
    str[strlen(str) - strlen(t)] = '\0';
}

Color lighterColor(struct vGfx *vg, Color color)
/* Get lighter shade of a color */ 
{
struct rgbColor rgbColor =  vgColorIxToRgb(vg, color);
rgbColor.r = (rgbColor.r+255)/2;
rgbColor.g = (rgbColor.g+255)/2;
rgbColor.b = (rgbColor.b+255)/2;
return vgFindColorIx(vg, rgbColor.r, rgbColor.g, rgbColor.b);
}

int spreadStringCharWidth(int width, int count)
{
    return width/count;
}

void spreadAlignString(struct vGfx *vg, int x, int y, int width, int height,
		       Color color, MgFont *font, char *text, 
		       char *match, int count, bool dots, bool isCodon)
/* Draw evenly spaced letters in string.  For multiple alignments,
 * supply a non-NULL match string, and then matching letters will be colored
 * with the main color, mismatched letters will have alt color (or 
 * matching letters with a dot, and mismatched bases with main color if this
 * option is selected).
 * Draw a vertical bar in orange where sequence lacks gaps that
 * are in reference sequence (possible insertion) -- this is indicated
 * by an escaped insert count in the sequence.  The escape char is backslash.
 * The count param is the number of bases to print, not length of
 * the input line (text) */
{
char cBuf[2] = "";
int i,j,textPos=0;
int x1, x2;
char *motifString = cartOptionalString(cart,BASE_MOTIFS);
boolean complementsToo = cartUsualBoolean(cart, MOTIF_COMPLEMENT, FALSE);
char **motifs = NULL;
boolean *inMotif = NULL;
int motifCount = 0;
Color noMatchColor = lighterColor(vg, color);
Color clr;
int textLength = strlen(text);
bool selfLine = (match == text);
cBuf[1] = '\0';  

/* If we have motifs, look for them in the string. */
if(motifString != NULL && strlen(motifString) != 0 && !isCodon)
    {
    touppers(motifString);
    eraseWhiteSpace(motifString);
    motifString = cloneString(motifString);
    motifCount = chopString(motifString, ",", NULL, 0);
    if (complementsToo)
	AllocArray(motifs, motifCount*2);	/* twice as many */
    else
	AllocArray(motifs, motifCount);
    chopString(motifString, ",", motifs, motifCount);
    if (complementsToo)
	{
	for(i = 0; i < motifCount; i++)
	    {
	    int comp = i + motifCount;
	    motifs[comp] = cloneString(motifs[i]);
	    reverseComplement(motifs[comp],strlen(motifs[comp]));
	    }
	motifCount *= 2;	/* now we have this many	*/
	}
	
    AllocArray(inMotif, textLength);
    for(i = 0; i < motifCount; i++)
	{
	char *mark = text;
	while((mark = stringIn(motifs[i], mark)) != NULL)
	    {
	    int end = mark-text + strlen(motifs[i]);
	    for(j = mark-text; j < end && j < textLength ; j++)
		{
		inMotif[j] = TRUE;
		}
	    mark++;
	    }
	}
    freez(&motifString);
    freez(&motifs);
    }

for (i=0; i<count; i++, text++, textPos++)
    {
    x1 = i * width/count;
    x2 = (i+1) * width/count - 1;
    if (match != NULL && *text == '|')
        {
        /* insert count follows -- replace with a colored vertical bar */
        text++;
	textPos++;
        i--;
        vgBox(vg, x+x1, y, 1, height, getOrangeColor());
        continue;
        }
    cBuf[0] = *text;
    clr = color;
    if (dots)
        {
        /* display bases identical to reference as dots */
        /* suppress for first line (self line) */
        if (!selfLine && match != NULL && match[i])
            if ((*text != ' ') && (toupper(*text) == toupper(match[i])))
                cBuf[0] = '.';
        }
    else
        {
        /* display bases identical to reference in main color, mismatches
         * in alt color */
        if (match != NULL && match[i])
            if ((*text != ' ') && (toupper(*text) != toupper(match[i])))
                clr = noMatchColor;
        }
    if(inMotif != NULL && textPos < textLength && inMotif[textPos])
	{
	vgBox(vg, x1+x, y, x2-x1, height, clr);
	vgTextCentered(vg, x1+x, y, x2-x1, height, MG_WHITE, font, cBuf);
	}
    else
        {
        /* restore char for unaligned sequence to lower case */
        if (tolower(cBuf[0]) == tolower(UNALIGNED_SEQ))
            cBuf[0] = UNALIGNED_SEQ;
        /* display bases */
        vgTextCentered(vg, x1+x, y, x2-x1, height, clr, font, cBuf);
        }
    }
freez(&inMotif);
}

void spreadAlignStringProt(struct vGfx *vg, int x, int y, int width, int height,
		       Color color, MgFont *font, char *text, 
		       char *match, int count, bool dots, bool isCodon, int seqStart)
/* Draw evenly spaced letters in string for protein sequence.  
 * For multiple alignments,
 * supply a non-NULL match string, and then matching letters will be colored
 * with the main color, mismatched letters will have alt color (or 
 * matching letters with a dot, and mismatched bases with main color if this
 * option is selected).
 * Draw a vertical bar in orange where sequence lacks gaps that
 * are in reference sequence (possible insertion) -- this is indicated
 * by an escaped insert count in the sequence.  The escape char is backslash.
 * The count param is the number of bases to print, not length of
 * the input line (text) */
{
char cBuf[2] = "";
int i,j,textPos=0;
int x1, x2, xx1, xx2;
char *motifString = cartOptionalString(cart,BASE_MOTIFS);
boolean complementsToo = cartUsualBoolean(cart, MOTIF_COMPLEMENT, FALSE);
char **motifs = NULL;
boolean *inMotif = NULL;
int motifCount = 0;
Color noMatchColor = lighterColor(vg, color);
Color clr;
int textLength = strlen(text);
bool selfLine = (match == text);

/* set alternating colors */
Color color1, color2;
color1 = vgFindColorIx(vg, 12, 12, 120);
color1 = lighterColor(vg, color1);
color1 = lighterColor(vg, color1);
color2 = lighterColor(vg, color1);

cBuf[1] = '\0';  

/* If we have motifs, look for them in the string. */
if(motifString != NULL && strlen(motifString) != 0 && !isCodon)
    {
    touppers(motifString);
    eraseWhiteSpace(motifString);
    motifString = cloneString(motifString);
    motifCount = chopString(motifString, ",", NULL, 0);
    if (complementsToo)
	AllocArray(motifs, motifCount*2);	/* twice as many */
    else
	AllocArray(motifs, motifCount);
    chopString(motifString, ",", motifs, motifCount);
    if (complementsToo)
	{
	for(i = 0; i < motifCount; i++)
	    {
	    int comp = i + motifCount;
	    motifs[comp] = cloneString(motifs[i]);
	    reverseComplement(motifs[comp],strlen(motifs[comp]));
	    }
	motifCount *= 2;	/* now we have this many	*/
	}
	
    AllocArray(inMotif, textLength);
    for(i = 0; i < motifCount; i++)
	{
	char *mark = text;
	while((mark = stringIn(motifs[i], mark)) != NULL)
	    {
	    int end = mark-text + strlen(motifs[i]);
	    for(j = mark-text; j < end && j < textLength ; j++)
		{
		inMotif[j] = TRUE;
		}
	    mark++;
	    }
	}
    freez(&motifString);
    freez(&motifs);
    }
for (i=0; i<count; i++, text++, textPos++)
    {
    x1 = i * width/count;
    x2 = (i+1) * width/count - 1;

    xx1 = (i-1) * width/count;
    xx2 = (i+2) * width/count - 1;

    if (match != NULL && *text == '|')
        {
        /* insert count follows -- replace with a colored vertical bar */
        text++;
	textPos++;
        i--;
        vgBox(vg, x+x1, y, 1, height, getOrangeColor());
        continue;
        }
    if (*text != ' ')
	cBuf[0] = lookupCodon(text-1);
    else 
	cBuf[0] = ' ';
    if (cBuf[0] == 'X') cBuf[0] = '-';
    clr = color;
    if (dots)
        {
        /* display bases identical to reference as dots */
        /* suppress for first line (self line) */
        if (!selfLine && match != NULL && match[i])
            if ((*text != ' ') && (lookupCodon(text-1) ==  lookupCodon(match+textPos-1)))
                cBuf[0] = '.';
        }
    else
        {
        /* display bases identical to reference in main color, mismatches
         * in alt color */
        if (match != NULL && match[i])
            if ((*text != ' ') && (toupper(*text) != toupper(match[i])))
                clr = noMatchColor;
        }
    if(inMotif != NULL && textPos < textLength && inMotif[textPos])
	{
	vgBox(vg, x1+x, y, x2-x1, height, clr);
	vgTextCentered(vg, x1+x, y, x2-x1, height, MG_WHITE, font, cBuf);
	}
    else
        {
        /* restore char for unaligned sequence to lower case */
        if (tolower(cBuf[0]) == tolower(UNALIGNED_SEQ))
            cBuf[0] = UNALIGNED_SEQ;
        /* display bases */
        if (cBuf[0] != ' ') 
	    {
	    /* display AA at the center of a codon */
	    if (((seqStart + textPos) % 3) == 1)
	    	{
		/* display alternate background color */
            	if (((seqStart + textPos)/3 %2) == 0)
                    {
                    vgBox(vg, xx1+x, y, xx2-xx1, height, color1);
                    }
            	else
                    {
                    vgBox(vg, xx1+x, y, xx2-xx1, height, color2);
                    }

		/* display AA */
	    	vgTextCentered(vg, x1+x, y, x2-x1, height, clr, font, cBuf);
	    	}
	    }
	}
    }
freez(&inMotif);
}

void spreadBasesString(struct vGfx *vg, int x, int y, int width, 
                        int height, Color color, MgFont *font, 
                        char *s, int count, bool isCodon)
/* Draw evenly spaced letters in string. */
{
spreadAlignString(vg, x, y, width, height, color, font, s, 
                        NULL, count, FALSE, isCodon);
}

static void drawBases(struct vGfx *vg, int x, int y, int width, int height,
                        Color color, MgFont *font, boolean complementSeq, 
                        struct dnaSeq *thisSeq)
/* Draw evenly spaced bases. */
{
struct dnaSeq *seq;

if (thisSeq == NULL)
   seq = hDnaFromSeq(chromName, winStart, winEnd, dnaUpper);
else
    seq = thisSeq;

if (complementSeq)
    complement(seq->dna, seq->size);
spreadBasesString(vg, x, y, width, height, color, font, 
                                seq->dna, seq->size, FALSE);

if (thisSeq == NULL)
    freeDnaSeq(&seq);
}

void drawComplementArrow( struct vGfx *vg, int x, int y, 
                                int width, int height, MgFont *font)
/* Draw arrow and create clickbox for complementing ruler bases */
{
if(cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
    vgTextRight(vg, x, y, width, height, MG_GRAY, font, "<---");
else
    vgTextRight(vg, x, y, width, height, MG_BLACK, font, "--->");
mapBoxToggleComplement(x, y, width, height, NULL, chromName, winStart, winEnd,
                                "complement bases");
}

struct track *chromIdeoTrack(struct track *trackList)
/* Find chromosome ideogram track */
{
struct track *track;
for(track = trackList; track != NULL; track = track->next)
    {
    if(sameString(track->mapName, "cytoBandIdeo"))
	{
	if (hTableExists(track->mapName))
	    return track;
	else
	    return NULL;
	}
    }
return NULL;
}

void removeTrackFromGroup(struct track *track)
/* Remove track from group it is part of. */
{
struct trackRef *tr = NULL;
for(tr = track->group->trackList; tr != NULL; tr = tr->next)
    {
    if(tr->track == track)
	{
	slRemoveEl(&track->group->trackList, tr);
	break;
	}
    }
}


void fillInStartEndBands(struct track *ideoTrack, char *startBand, char *endBand, int buffSize)
/* Loop through the bands and fill in the one that the current window starts
   on and ends on. */
{
struct cytoBand *cb = NULL, *cbList = ideoTrack->items;
for(cb = cbList; cb != NULL; cb = cb->next)
    {
    /* If the start or end is encompassed by this band fill
       it in. */
    if(winStart >= cb->chromStart &&
       winStart <= cb->chromEnd) 
	{
	safef(startBand, buffSize, "%s", cb->name);
	}
    /* End is > rather than >= due to odditiy in the
       cytoband track where the starts and ends of two
       bands overlaps by one. */
    if(winEnd > cb->chromStart &&
       winEnd <= cb->chromEnd) 
	{
	safef(endBand, buffSize, "%s", cb->name);
	}
    }
}

void makeChromIdeoImage(struct track **pTrackList, char *psOutput)
/* Make an ideogram image of the chromsome and our position in
   it. */
{
struct track *ideoTrack = NULL;
MgFont *font = tl.font;
char *mapName = "ideoMap";
struct vGfx *vg;
struct tempName gifTn;
boolean doIdeo = TRUE;
int ideoWidth = round(.8 *tl.picWidth);
int ideoHeight = 0;
int textWidth = 0;

ideoTrack = chromIdeoTrack(*pTrackList);

/* If no ideogram don't draw. */
if(ideoTrack == NULL)
    doIdeo = FALSE;
else
    {
    ideogramAvail = TRUE;
    /* Remove the track from the group and track list. */
    removeTrackFromGroup(ideoTrack);
    slRemoveEl(pTrackList, ideoTrack);

    /* Fix for hide all button hiding the ideogram as well. */
    if(withIdeogram && ideoTrack->items == NULL)
	{
	ideoTrack->visibility = tvDense;
	ideoTrack->loadItems(ideoTrack);
	}
    limitVisibility(ideoTrack);
    
    /* If hidden don't draw. */
    if(ideoTrack->limitedVis == tvHide || !withIdeogram)
	doIdeo = FALSE;

    /* If doing postscript, skip ideogram. */
    if(psOutput)
	doIdeo = FALSE;
    }
if(doIdeo)
    {
    char startBand[16];
    char endBand[16];
    char title[32];
    startBand[0] = endBand[0] = '\0';
    fillInStartEndBands(ideoTrack, startBand, endBand, sizeof(startBand)); 
    /* Draw the ideogram. */
    trashDirFile(&gifTn, "hgtIdeo", "hgtIdeo", ".gif");
    /* Start up client side map. */
    hPrintf("<MAP Name=%s>\n", mapName);
    ideoHeight = gfxBorder + ideoTrack->height;
    vg = vgOpenGif(ideoWidth, ideoHeight, gifTn.forCgi);
    makeGrayShades(vg);
    makeBrownShades(vg);
    makeSeaShades(vg);
    ideoTrack->ixColor = vgFindRgb(vg, &ideoTrack->color);
    ideoTrack->ixAltColor = vgFindRgb(vg, &ideoTrack->altColor);
    vgSetClip(vg, 0, gfxBorder, ideoWidth, ideoTrack->height);
    if(sameString(startBand, endBand)) 
	safef(title, sizeof(title), "%s (%s)", chromName, startBand);
    else
	safef(title, sizeof(title), "%s (%s-%s)", chromName, startBand, endBand);
    textWidth = mgFontStringWidth(font, title);
    vgTextCentered(vg, 2, gfxBorder, textWidth, ideoTrack->height, MG_BLACK, font, title);
    ideoTrack->drawItems(ideoTrack, winStart, winEnd, vg, textWidth+4, gfxBorder, ideoWidth-textWidth-4,
			 font, ideoTrack->ixColor, ideoTrack->limitedVis);
    vgUnclip(vg);
    /* Save out picture and tell html file about it. */
    vgClose(&vg);
    /* Finish map. */
    hPrintf("</MAP>\n");
    }
hPrintf("<TABLE BORDER=0 CELLPADDING=0>");
if(doIdeo)
    {
    hPrintf("<TR><TD HEIGHT=5></TD></TR>");
    hPrintf("<TR><TD><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s >",
	    gifTn.forHtml, ideoWidth, ideoHeight, mapName);
    hPrintf("</TD></TR>");
    hPrintf("<TR><TD HEIGHT=5></TD></TR></TABLE>");
    }
else
    hPrintf("<TR><TD HEIGHT=10></TD></TR></TABLE>");
if(ideoTrack != NULL)
    {
    ideoTrack->limitedVisSet = TRUE;
    ideoTrack->limitedVis = tvHide; /* Don't draw in main gif. */
    }
}

static bool isCompositeTrack(struct track *track)
/* Determine if this is a composite track. This is currently defined
 * as a top-level dummy track, with a list of subtracks of the same type.
 * Need to check trackDb, as we need to ignore wigMaf's which have
 * subtracks but aren't composites */
{
if (track->tdb)
    return (track->subtracks != NULL && trackDbIsComposite(track->tdb));
return FALSE;
}

static boolean isSubtrack(struct track *track)
/* Return TRUE if track is a subtrack of a composite track. */
/* Subtracks usually inherit their parent track's tdb, so their tdbs may 
 * appear composite, but their mapNames will not be the same as tdb->tableName 
 * in that case. */
{
return ((trackDbSetting(track->tdb, "subTrack") != NULL) ||
	(trackDbIsComposite(track->tdb) &&
	 !sameString(track->mapName, track->tdb->tableName)));
}

boolean isWithCenterLabels(struct track *track)
/* Special cases: inhibit center labels of subtracks in dense mode, and 
 * of composite track in non-dense mode.
 * BUT if track->tdb has a centerLabelDense setting, let subtracks go with 
 * the default and inhibit composite track center labels in all modes.
 * Otherwise use the global boolean withCenterLabels. */
{
if (track != NULL)
    {
    char *centerLabelsDense = trackDbSetting(track->tdb, "centerLabelsDense");
    if (centerLabelsDense) 
	{
	boolean on =  sameWord(centerLabelsDense, "on") && withCenterLabels;
	return on;
	}
    else if (((limitVisibility(track) == tvDense) && isSubtrack(track)) ||
	     ((limitVisibility(track) != tvDense) && isCompositeTrack(track)))
	return FALSE;
    }
return withCenterLabels;
}

int trackPlusLabelHeight(struct track *track, int fontHeight)
/* Return the sum of heights of items in this track (or subtrack as it may be) 
 * and the center label(s) above the items (if any). */
{
int y = track->height;
if (isWithCenterLabels(track))
    y += fontHeight;
if (isCompositeTrack(track))
    {
    struct track *subtrack;
    for (subtrack = track->subtracks;  subtrack != NULL;
	 subtrack = subtrack->next)
	{
	if (isSubtrackVisible(subtrack) && isWithCenterLabels(subtrack))
	    y += fontHeight;
	}
    }
return y;
}

static int doLeftLabels(struct track *track, struct vGfx *vg, MgFont *font, 
                                int y)
/* Draw left labels.  Return y coord. */
{
struct slList *prev = NULL;

/* for sample tracks */
double minRangeCutoff, maxRangeCutoff;
double minRange, maxRange;
double min0, max0;
char minRangeStr[32];
char maxRangeStr[32];

int ymin, ymax;
int start;
int newy;
char o4[128];
char o5[128];
struct slList *item;
enum trackVisibility vis = track->limitedVis;
enum trackVisibility savedVis = vis;
Color labelColor = (track->labelColor ? 
                        track->labelColor : track->ixColor);
int fontHeight = mgFontLineHeight(font);
int tHeight = trackPlusLabelHeight(track, fontHeight);
if (vis == tvHide)
    return y;

/*  if a track can do its own left labels, do them after drawItems */
if (track->drawLeftLabels != NULL)
    return y + tHeight;

/*	Wiggle tracks depend upon clipping.  They are reporting
 *	totalHeight artifically high by 1 so this will leave a
 *	blank area one pixel high below the track.
 */
if (sameString("wig",track->tdb->type))
    vgSetClip(vg, leftLabelX, y, leftLabelWidth, tHeight-1);
else
    vgSetClip(vg, leftLabelX, y, leftLabelWidth, tHeight);

minRange = 0.0;
safef( o4, sizeof(o4),"%s.min.cutoff", track->mapName);
safef( o5, sizeof(o5),"%s.max.cutoff", track->mapName);
minRangeCutoff = max( atof(cartUsualString(cart,o4,"0.0"))-0.1, 
                                track->minRange );
maxRangeCutoff = min( atof(cartUsualString(cart,o5,"1000.0"))+0.1, 
                                track->maxRange);
if( sameString( track->mapName, "humMusL" ) ||
    sameString( track->mapName, "musHumL" ) ||
    sameString( track->mapName, "mm3Rn2L" ) ||		
    sameString( track->mapName, "hg15Mm3L" ) ||		
    sameString( track->mapName, "mm3Hg15L" ) ||
    sameString( track->mapName, "regpotent" ) ||
    sameString( track->mapName, "HMRConservation" )  )
    {
    int binCount = round(1.0/track->scaleRange);
    minRange = whichSampleBin( minRangeCutoff, track->minRange, track->maxRange, binCount );
    maxRange = whichSampleBin( maxRangeCutoff, track->minRange, track->maxRange ,binCount ); 
    min0 = whichSampleNum( minRange, track->minRange,track->maxRange, binCount );
    max0 = whichSampleNum( maxRange, track->minRange, track->maxRange, binCount );
    sprintf( minRangeStr, " "  );
    sprintf( maxRangeStr, " " );
    if( vis == tvFull && track->heightPer >= 74  )
        {
        samplePrintYAxisLabel( vg, y+5, track, "1.0", min0, max0 );
        samplePrintYAxisLabel( vg, y+5, track, "2.0", min0, max0 );
        samplePrintYAxisLabel( vg, y+5, track, "3.0", min0, max0 );
        samplePrintYAxisLabel( vg, y+5, track, "4.0", min0, max0 );
        samplePrintYAxisLabel( vg, y+5, track, "5.0", min0, max0 );
        samplePrintYAxisLabel( vg, y+5, track, "6.0", min0, max0 );
        }
    }
else
    {
    sprintf( minRangeStr, "%d", (int)round(minRangeCutoff));
    sprintf( maxRangeStr, "%d", (int)round(maxRangeCutoff));
    }
/* special label handling for wigMaf type tracks -- they
   display a left label in pack mode.  To use the full mode
   labeling, temporarily set visibility to full.
   Restore savedVis later */
if (startsWith("wigMaf", track->tdb->type))
    vis = tvFull;

switch (vis)
    {
    case tvHide:
        break;	/* Do nothing; */
    case tvPack:
    case tvSquish:
	y += tHeight;
        break;
    case tvFull:
        if (isWithCenterLabels(track))
            y += fontHeight;
        start = 1;

        if( track->subType == lfSubSample && track->items == NULL )
            y += track->height;

        for (item = track->items; item != NULL; item = item->next)
            {
            char *rootName;
            char *name = track->itemName(track, item);
            int itemHeight = track->itemHeight(track, item);
            newy = y;

            if (track->itemLabelColor != NULL)
                labelColor = track->itemLabelColor(track, item, vg);
            
            /* Do some fancy stuff for sample tracks. 
             * Draw y-value limits for 'sample' tracks. */
            if(track->subType == lfSubSample )
                {
                
                if( prev == NULL )
                    newy += itemHeight;
                else
                    newy += sampleUpdateY(name, 
                            track->itemName(track, prev), itemHeight);
                if( newy == y )
                    continue;

                if( track->heightPer > (3 * fontHeight ) )
                    {
                    ymax = y - (track->heightPer / 2) + (fontHeight / 2);
                    ymin = y + (track->heightPer / 2) - (fontHeight / 2);
                    vgTextRight(vg, leftLabelX, ymin, leftLabelWidth-1,
                                itemHeight, track->ixAltColor, 
                                font, minRangeStr );
                    vgTextRight(vg, leftLabelX, ymax, leftLabelWidth-1,
                                itemHeight, track->ixAltColor, 
                                font, maxRangeStr );
                    }
                prev = item;

                rootName = cloneString( name );
                beforeFirstPeriod( rootName );
                if( sameString( track->mapName, "humMusL" ) || 
                         sameString( track->mapName, "hg15Mm3L" ))
                    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1,
                             itemHeight, track->ixColor, font, "Mouse Cons");
                else if( sameString( track->mapName, "musHumL" ) ||
                         sameString( track->mapName, "mm3Hg15L"))
                    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, 
                                itemHeight, track->ixColor, font, "Human Cons");
                else if( sameString( track->mapName, "mm3Rn2L" ))
                    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, 
                                itemHeight, track->ixColor, font, "Rat Cons");
                else
                    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, 
                                itemHeight, track->ixColor, font, rootName );
                freeMem( rootName );
                start = 0;
                y = newy;
                }
            else
                {
                /* standard item labeling */
		if (highlightItem(track, item))
		    {
		    int nameWidth = mgFontStringWidth(font, name);
		    int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
		    vgBox(vg, boxStart, y, nameWidth+1, itemHeight - 1,
			  labelColor);
		    vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, 
				itemHeight, MG_WHITE, font, name);
		    }
		else
		    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, 
				itemHeight, labelColor, font, name);
                y += itemHeight;
                }
            }
        break;
    case tvDense:
        
        if (isWithCenterLabels(track))
            y += fontHeight;
        
        /*draw y-value limits for 'sample' tracks. 
         * (always puts 0-100% range)*/
        if( track->subType == lfSubSample && 
                track->heightPer > (3 * fontHeight ) )
            {
            ymax = y - (track->heightPer / 2) + (fontHeight / 2);
            ymin = y + (track->heightPer / 2) - (fontHeight / 2);
            vgTextRight(vg, leftLabelX, ymin, 
                        leftLabelWidth-1, track->lineHeight, 
                        track->ixAltColor, font, minRangeStr );
            vgTextRight(vg, leftLabelX, ymax, 
                        leftLabelWidth-1, track->lineHeight, 
                        track->ixAltColor, font, maxRangeStr );
            }
        vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, 
                    track->lineHeight, labelColor, font, 
                    track->shortLabel);
        y += track->height;
        break;
    }
/* NOTE: might want to just restore savedVis here for all track types,
   but I'm being cautious... */
if (sameString(track->tdb->type, "wigMaf"))
    vis = savedVis;
vgUnclip(vg);
return y;
}

static void doLabelNextItemButtons(struct track *track, struct track *parentTrack, struct vGfx *vg, MgFont *font, int y,
			      int trackPastTabX, int trackPastTabWidth, int fontHeight,
			      int insideHeight, Color labelColor)
/* If the track allows label next-item buttons (next gene), draw them. */
/* The button will cause hgTracks to run again with the additional CGI */
/* vars nextItem=trackName or prevItem=trackName, which will then  */
/* signal the browser to find the next thing on the track before it */
/* does anything else. */
{
int arrowWidth = insideHeight;
int arrowButtonWidth = arrowWidth + 2 * NEXT_ITEM_ARROW_BUFFER;
int rightButtonX = insideX + insideWidth - arrowButtonWidth - 1;
char buttonText[100];
Color fillColor = lightGrayIndex();
labelColor = blackIndex();
vgNextItemButton(vg, rightButtonX + NEXT_ITEM_ARROW_BUFFER, y, arrowWidth, arrowWidth, labelColor, fillColor, TRUE);
vgNextItemButton(vg, insideX + NEXT_ITEM_ARROW_BUFFER, y, arrowWidth, arrowWidth, labelColor, fillColor, FALSE);
safef(buttonText, ArraySize(buttonText), "hgt.prevItem=%s", track->mapName);
mapBoxReinvokeExtra(insideX, y + 1, arrowButtonWidth, insideHeight, NULL,
 		    NULL, 0, 0, "Previous item", buttonText);
mapBoxToggleVis(insideX + arrowButtonWidth, y + 1, insideWidth - (2 * arrowButtonWidth), insideHeight, parentTrack);
safef(buttonText, ArraySize(buttonText), "hgt.nextItem=%s", track->mapName);
mapBoxReinvokeExtra(insideX + insideWidth - arrowButtonWidth, y + 1, arrowButtonWidth, insideHeight, NULL,
 		    NULL, 0, 0, "Next item", buttonText);
}

static int doCenterLabels(struct track *track, struct track *parentTrack,
                                struct vGfx *vg, MgFont *font, int y)
/* Draw center labels.  Return y coord */
{
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackPastTabWidth = tl.picWidth - trackPastTabX;
int fontHeight = mgFontLineHeight(font);
int insideHeight = fontHeight-1;
if (track->limitedVis != tvHide)
    {
    Color labelColor = (track->labelColor ? 
                        track->labelColor : track->ixColor);
    vgTextCentered(vg, insideX, y+1, insideWidth, insideHeight, 
                        labelColor, font, track->longLabel);
    if (withNextItemArrows && track->labelNextItemButtonable && track->labelNextPrevItem)
	doLabelNextItemButtons(track, parentTrack, vg, font, y, trackPastTabX,
			  trackPastTabWidth, fontHeight, insideHeight, labelColor);
    else
	mapBoxToggleVis(trackPastTabX, y+1, 
			trackPastTabWidth, insideHeight, parentTrack);
    mapBoxToggleVis(trackPastTabX, y+1, 
                    trackPastTabWidth, insideHeight, parentTrack);
    y += fontHeight;
    y += track->height;
    }
return y;
}

static int doDrawItems(struct track *track, struct vGfx *vg, MgFont *font, 
                                    int y, long *lastTime)
/* Draw track items.  Return y coord */
{
int fontHeight = mgFontLineHeight(font);
int pixWidth = tl.picWidth;
if (isWithCenterLabels(track))
    y += fontHeight;
if (track->limitedVis == tvPack)
    {
    vgSetClip(vg, gfxBorder+trackTabWidth+1, y, 
        pixWidth-2*gfxBorder-trackTabWidth-1, track->height);
    }
else
    vgSetClip(vg, insideX, y, insideWidth, track->height);
track->drawItems(track, winStart, winEnd, vg, insideX, y, insideWidth, 
                 font, track->ixColor, track->limitedVis);
if (measureTiming && lastTime)
    {
    long thisTime = clock1000();
    track->drawTime = thisTime - *lastTime;
    *lastTime = thisTime;
    }
vgUnclip(vg);
y += track->height;
return y;
}

static int doMapItems(struct track *track, int fontHeight, int y)
/* Draw map boxes around track items */
{
char *type = track->tdb->type;
int newy;
char *directUrl = trackDbSetting(track->tdb, "directUrl");
boolean withHgsid = (trackDbSetting(track->tdb, "hgsid") != NULL);
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackPastTabWidth = tl.picWidth - trackPastTabX;
int start = 1;
struct slList *item;
boolean isWig = (sameString("wig", type) || startsWith("wig ", type) ||
		startsWith("bedGraph", type));

if (isWithCenterLabels(track))
    y += fontHeight;
if (track->mapsSelf)
    {
    /* Wiggle track's ->height is actually one less than what it returns from 
     * totalHeight()... I think the least disruptive way to account for this
     * (and not touch Ryan Weber's Sample stuff) is to just correct here if 
     * we see wiggle or bedGraph: */
    if (isWig)
        return y+track->height + 1;
    else
        return y+track->height;
    }
if (track->subType == lfSubSample && track->items == NULL)
     y += track->lineHeight;

/* override doMapItems for hapmapLd track */
/* does not scale with subtracks right now, so this is commented out until it can be fixed
if (startsWith("hapmapLd",track->mapName))
    {
    y += round((double)(scaleForPixels(insideWidth)*insideWidth/2));
    return y;
    }
*/
for (item = track->items; item != NULL; item = item->next)
    {
    int height = track->itemHeight(track, item);

    /*wiggle tracks don't always increment height (y-value) here*/
    if( track->subType == lfSubSample )
        {
        newy = y;
        if( !start && item->next != NULL  )
            {
            newy += sampleUpdateY( track->itemName(track, item),
                             track->itemName(track, item->next),
                             height );
            }
        else if( item->next != NULL || start )
            newy += height;
        start = 0;
        y = newy;
        }
    else
        {
        if (!track->mapsSelf)
            {
            mapBoxHgcOrHgGene(track->itemStart(track, item),
                    track->itemEnd(track, item), trackPastTabX,
                    y, trackPastTabWidth,height, track->mapName,
                    track->mapItemName(track, item),
                    track->itemName(track, item), directUrl, withHgsid);
            }
        y += height;
        }
    }
/* Wiggle track's ->height is actually one less than what it returns from 
 * totalHeight()... I think the least disruptive way to account for this
 * (and not touch Ryan Weber's Sample stuff) is to just correct here if 
 * we see wiggle or bedGraph: */
if (isWig)
    y++;
return y;
}

static int doOwnLeftLabels(struct track *track, struct vGfx *vg, 
                                                MgFont *font, int y)
/* Track draws it own, custom left labels */
{
int pixWidth = tl.picWidth;
int fontHeight = mgFontLineHeight(font);
int tHeight = trackPlusLabelHeight(track, fontHeight);
Color labelColor = (track->labelColor ? track->labelColor : track->ixColor);
if (track->limitedVis == tvPack)
    { /*XXX This needs to be looked at, no example yet*/
    vgSetClip(vg, gfxBorder+trackTabWidth+1, y, 
              pixWidth-2*gfxBorder-trackTabWidth-1, track->height);
    track->drawLeftLabels(track, winStart, winEnd,
                          vg, leftLabelX, y, leftLabelWidth, tHeight,
                          isWithCenterLabels(track), font, labelColor, 
                          track->limitedVis);
    }
else
    {
    vgSetClip(vg, leftLabelX, y, leftLabelWidth, tHeight);
    
    /* when the limitedVis == tvPack is correct above,
     *	this should be outside this else clause
     */
    track->drawLeftLabels(track, winStart, winEnd,
                          vg, leftLabelX, y, leftLabelWidth, tHeight,
                          isWithCenterLabels(track), font, labelColor, 
                          track->limitedVis);
    }
vgUnclip(vg);
y += tHeight;
return y;
}

static void setSubtrackVisible(char *tableName, bool visible)
/* Set tableName's _sel variable in cart. */
{
char option[64];
safef(option, sizeof(option), "%s_sel", tableName);
cartSetBoolean(cart, option, visible);
}

static void subtrackVisible(struct track *subtrack)
/* Determine if subtrack should be displayed.  Save in cart */
{
char option[64];
char *words[2];
char *setting;
bool selected = TRUE;

/* Note: this will only work when noInherit is used; otherwise subtrack 
 * inherits its parent's tdb: */
if ((setting = trackDbSetting(subtrack->tdb, "subTrack")) != NULL)
    {
    if (chopLine(cloneString(setting), words) >= 2)
        if (sameString(words[1], "off"))
            selected = FALSE;
    }
safef(option, sizeof(option), "%s_sel", subtrack->mapName);
selected = cartCgiUsualBoolean(cart, option, selected);
setSubtrackVisible(subtrack->mapName, selected);
}

bool isSubtrackSelected(struct track *tg)
/* Has this subtrack not been deselected in hgTrackUi? */
{
char option[64];
safef(option, sizeof(option), "%s_sel", tg->mapName);
return cartCgiUsualBoolean(cart, option, TRUE);
}

bool isSubtrackVisible(struct track *tg)
/* Should this subtrack be displayed?  Check cart, not cgi, for selectedness 
 * because by this point we may have overridden CGI settings and saved into 
 * the cart. */
{
char option[64];
safef(option, sizeof(option), "%s_sel", tg->mapName);
return tg->visibility != tvHide && cartUsualBoolean(cart, option, TRUE);
}

static int subtrackCount(struct track *trackList)
/* Count the number of visible subtracks in (sub)trackList. */
{
struct track *subtrack;
int ct = 0;
for (subtrack = trackList; subtrack; subtrack = subtrack->next)
    if (isSubtrackVisible(subtrack))
        ct++;
return ct;
}

enum trackVisibility estimateVisibility(struct track *tg)
/* Return estimate of what visibility will be without actually setting it. */ 
{
enum trackVisibility vis = tg->visibility;
int h = 0;
int maxHeight = 0;

if (tg->limitedVisSet)
    return tg->limitedVis;
if (vis == tvHide)
    return tvHide;
if (vis == tvDense)
    return tvDense;
maxHeight = maximumTrackHeight(tg);
if (isCompositeTrack(tg))
    maxHeight = maxHeight * subtrackCount(tg->subtracks);
h = tg->totalHeight(tg, vis);
if (h > maxHeight)
    {
    if (vis == tvFull && tg->canPack)
        vis = tvPack;
    else if (vis == tvPack)
        vis = tvSquish;
    else
        vis = tvDense;
    h = tg->totalHeight(tg, vis);
    if (h > maxHeight && vis == tvPack)
        {
        vis = tvSquish;
        h = tg->totalHeight(tg, vis);
        }
    if (h > maxHeight)
        vis = tvDense;
    }
return vis;
}

enum trackVisibility limitVisibility(struct track *tg)
/* Return default visibility limited by number of items. 
 * This also sets tg->height. */
{
if (!tg->limitedVisSet)
    {
    enum trackVisibility vis = tg->visibility;
    int h;
    int maxHeight = maximumTrackHeight(tg);
    tg->limitedVisSet = TRUE;
    if (vis == tvHide)
	{
	tg->height = 0;
	tg->limitedVis = tvHide;
	return tvHide;
	}
    if (isCompositeTrack(tg))
        maxHeight = maxHeight * subtrackCount(tg->subtracks);
    h = tg->totalHeight(tg, vis);
    if (h > maxHeight)
        {
	if (vis == tvFull && tg->canPack)
	    vis = tvPack;
	else if (vis == tvPack)
	    vis = tvSquish;
	else
	    vis = tvDense;
	h = tg->totalHeight(tg, vis);
	if (h > maxHeight && vis == tvPack)
	    {
	    vis = tvSquish;
	    h = tg->totalHeight(tg, vis);
	    }
	if (h > maxHeight)
	    {
	    vis = tvDense;
	    h = tg->totalHeight(tg, vis);
	    }
	}
    tg->height = h;
    tg->limitedVis = vis;
    }
return tg->limitedVis;
}

int doTrackMap(struct track *track, int y, int fontHeight, 
	       int trackPastTabX, int trackPastTabWidth)
/* Write out the map for this track. Return the new offset. */
{
int mapHeight = 0;
switch (track->limitedVis)
    {
    case tvHide:
	break;	/* Do nothing; */
    case tvPack:
    case tvSquish:
	y += trackPlusLabelHeight(track, fontHeight);
	break;
    case tvFull:
	if (!nextItemCompatible(track))
	    {
	    if (isCompositeTrack(track))
		{
		struct track *subtrack;
		for (subtrack = track->subtracks;  subtrack != NULL;
		     subtrack = subtrack->next)
		    if (isSubtrackVisible(subtrack))
			y = doMapItems(subtrack, fontHeight, y);
		}
	    else
		y = doMapItems(track, fontHeight, y);
	    }
	break;
    case tvDense:
	if (isWithCenterLabels(track))
	    y += fontHeight;
	if (isCompositeTrack(track))
	    mapHeight = track->height;
	else
	    mapHeight = track->lineHeight;
	mapBoxToggleVis(trackPastTabX, y, trackPastTabWidth, mapHeight, track);
	y += mapHeight;
	break;
    }
return y;
}

void makeActiveImage(struct track *trackList, char *psOutput)
/* Make image and image map. */
{
struct track *track;
MgFont *font = tl.font;
struct vGfx *vg;
struct tempName gifTn;
char *mapName = "map";
int fontHeight = mgFontLineHeight(font);
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackTabX = gfxBorder;
int trackPastTabWidth = tl.picWidth - trackPastTabX;
int pixWidth, pixHeight;
int y;
int titleHeight = fontHeight; 
int showPosHeight = fontHeight; 
int rulerHeight = fontHeight;
int baseHeight = fontHeight;
int basePositionHeight = rulerHeight;
int codonHeight = fontHeight;
int rulerTranslationHeight = codonHeight * 3;        // 3 frames
int yAfterRuler = gfxBorder;
int yAfterBases = yAfterRuler;  // differs if base-level translation shown
int relNumOff;
boolean rulerCds = zoomedToCdsColorLevel;

/* Start a global track hash. */
trackHash = newHash(8);
/* Figure out dimensions and allocate drawing space. */
pixWidth = tl.picWidth;

if (rulerMode != tvFull)
    {
    rulerCds = FALSE; 
    }
    
/* Figure out height of each visible track. */
pixHeight = gfxBorder;
if (rulerMode != tvHide)
    {
    if (baseTitle)
	basePositionHeight += titleHeight;
	
    if (baseShowPos||baseShowAsm)
	basePositionHeight += showPosHeight;
	
    if (zoomedToBaseLevel)
	basePositionHeight += baseHeight;
	
    yAfterRuler += basePositionHeight;
    yAfterBases = yAfterRuler;
    pixHeight += basePositionHeight;
    if (rulerCds)
        {
        yAfterRuler += rulerTranslationHeight;
        pixHeight += rulerTranslationHeight;
        }
    }

boolean safeHeight = TRUE;
/* firefox on Linux worked almost up to 34,000 at the default 620 width	*/
#define maxSafeHeight	32000
/* Hash tracks/subtracks, limit visibility and calculate total image height: */
for (track = trackList; track != NULL; track = track->next)
    {
    hashAddUnique(trackHash, track->mapName, track);
    limitVisibility(track);
    if (!safeHeight)
	{
	track->limitedVis = tvHide;
	track->limitedVisSet = TRUE;
	continue;
	}
    if (track->limitedVis != tvHide)
	{
        if (isCompositeTrack(track))
            {
            struct track *subtrack;
            for (subtrack = track->subtracks; subtrack != NULL;
                         subtrack = subtrack->next)
                {
		hashAddUnique(trackHash, subtrack->mapName, subtrack);
                if (!isSubtrackVisible(subtrack))
                    continue;
		if (!subtrack->limitedVisSet)
		    {
		    subtrack->visibility = track->visibility;
		    subtrack->limitedVis = track->limitedVis;
		    subtrack->limitedVisSet = TRUE;
		    }
                }
	    }
	if (maxSafeHeight < (pixHeight+trackPlusLabelHeight(track,fontHeight)))
	    {
	    char numBuf[64];
	    sprintLongWithCommas(numBuf, maxSafeHeight);
	    printf("warning: image is over %s pixels high at "
		"track '%s',<BR>remaining tracks set to hide "
		"for this view.<BR>\n", numBuf, track->tdb->shortLabel);
	    safeHeight = FALSE;
	    track->limitedVis = tvHide;
	    track->limitedVisSet = TRUE;
	    }
	else
	    pixHeight += trackPlusLabelHeight(track, fontHeight);
	}
    }

imagePixelHeight = pixHeight;
if (psOutput)
    vg = vgOpenPostScript(pixWidth, pixHeight, psOutput);
else
    {
    trashDirFile(&gifTn, "hgt", "hgt", ".gif");
    vg = vgOpenGif(pixWidth, pixHeight, gifTn.forCgi);
    }
makeGrayShades(vg);
makeBrownShades(vg);
makeSeaShades(vg);
orangeColor = vgFindColorIx(vg, 230, 130, 0);
brickColor = vgFindColorIx(vg, 230, 50, 110);
blueColor = vgFindColorIx(vg, 0,114,198);
darkBlueColor = vgFindColorIx(vg, 0,70,140);
greenColor = vgFindColorIx(vg, 28,206,40);
darkGreenColor = vgFindColorIx(vg, 28,140,40);

/* Start up client side map. */
hPrintf("<MAP Name=%s>\n", mapName);

/* Find colors to draw in. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->limitedVis != tvHide)
	{
	track->ixColor = vgFindRgb(vg, &track->color);
	track->ixAltColor = vgFindRgb(vg, &track->altColor);
        if (isCompositeTrack(track))
            {
	    struct track *subtrack;
            for (subtrack = track->subtracks; subtrack != NULL;
                         subtrack = subtrack->next)
                {
                if (!isSubtrackVisible(subtrack))
                    continue;
                subtrack->ixColor = vgFindRgb(vg, &subtrack->color);
                subtrack->ixAltColor = vgFindRgb(vg, &subtrack->altColor);
                }
            }
        }
    }

leftLabelX = gfxBorder;
leftLabelWidth = insideX - gfxBorder*3;

/* Draw mini-buttons. */
if (withLeftLabels && psOutput == NULL)
    {
    int butOff;
    boolean grayButtonGroup = FALSE;
    struct group *lastGroup = NULL;
    y = gfxBorder;
    if (rulerMode != tvHide)
        {
        /* draw button for Base Position pseudo-track */
        int height = basePositionHeight;
        if (rulerCds)
            height += rulerTranslationHeight;
        drawGrayButtonBox(vg, trackTabX, y, trackTabWidth, height, TRUE);
        mapBoxUi(trackTabX, y, trackTabWidth, height, RULER_TRACK_NAME, 
                                                      RULER_TRACK_LABEL);
        y += height + 1;
        }
    for (track = trackList; track != NULL; track = track->next)
        {
	int h, yStart = y, yEnd;
	if (track->limitedVis != tvHide)
	    {
	    y += trackPlusLabelHeight(track, fontHeight);
	    yEnd = y;
	    h = yEnd - yStart - 1;

            /* alternate button colors for track groups*/
            if (track->group != lastGroup)
                grayButtonGroup = !grayButtonGroup;
            lastGroup = track->group;
            if (grayButtonGroup) 
                drawGrayButtonBox(vg, trackTabX, yStart, trackTabWidth, 
	    	                h, track->hasUi); 
            else
                drawBlueButtonBox(vg, trackTabX, yStart, trackTabWidth, 
	    	                h, track->hasUi); 
	    if (track->hasUi)
                mapBoxTrackUi(trackTabX, yStart, trackTabWidth, h, track);
	    }
	}
    butOff = trackTabX + trackTabWidth;
    leftLabelX += butOff;
    leftLabelWidth -= butOff;
    }

if (withLeftLabels)
    {
    Color lightRed = vgFindColorIx(vg, 255, 180, 180);

    vgBox(vg, leftLabelX + leftLabelWidth, 0,
    	gfxBorder, pixHeight, lightRed);
    y = gfxBorder;
    if (rulerMode != tvHide)
	{
	if (baseTitle)
	    {
	    vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, titleHeight, 
			MG_BLACK, font, WIN_TITLE_LABEL);
	    y += titleHeight;
	    }
	if (baseShowPos||baseShowAsm)
	    {
	    vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, showPosHeight, 
			MG_BLACK, font, WIN_POS_LABEL);
	    y += showPosHeight;
	    }
	{
	char rulerLabel[64];
	safef(rulerLabel,ArraySize(rulerLabel),"%s:",chromName);
	vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, rulerHeight, 
		    MG_BLACK, font, rulerLabel);
	}
	y += rulerHeight;
	if (zoomedToBaseLevel || rulerCds)
	    {		    
	    drawComplementArrow(vg,leftLabelX, y,
				leftLabelWidth-1, baseHeight, font);
	    if (zoomedToBaseLevel)				    
    		y += baseHeight;
	    }
        if (rulerCds)
            y += rulerTranslationHeight;
	}
    for (track = trackList; track != NULL; track = track->next)
        {
	if (track->limitedVis == tvHide)
	    continue;
        if (isCompositeTrack(track))
            {
	    struct track *subtrack;
	    if (isWithCenterLabels(track))
		y += fontHeight;
            for (subtrack = track->subtracks; subtrack != NULL;
		 subtrack = subtrack->next)
                if (isSubtrackVisible(subtrack))
                    y = doLeftLabels(subtrack, vg, font, y);
            }
        else
            y = doLeftLabels(track, vg, font, y);
        }
    }
else
    {
    leftLabelX = leftLabelWidth = 0;
    }

/* Draw guidelines. */
if (withGuidelines)
    {
    int height = pixHeight - 2*gfxBorder;
    int x;
    Color lightBlue = vgFindRgb(vg, &guidelineColor);

    vgSetClip(vg, insideX, gfxBorder, insideWidth, height);
    y = gfxBorder;

    for (x = insideX+guidelineSpacing-1; x<pixWidth; x += guidelineSpacing)
	vgBox(vg, x, y, 1, height, lightBlue);
    vgUnclip(vg);
    }

/* Show ruler at top. */
if (rulerMode != tvHide)
    {
    struct dnaSeq *seq = NULL;
    int rulerClickY = 0;
    int rulerClickHeight = rulerHeight;

    y = rulerClickY;
    vgSetClip(vg, insideX, y, insideWidth, yAfterRuler-y+1);
    relNumOff = winStart;
    
    if (baseTitle)
	{
	vgTextCentered(vg, insideX, y, insideWidth, titleHeight, 
			    MG_BLACK, font, baseTitle);
	rulerClickHeight += titleHeight;
	y += titleHeight;
	}
    if (baseShowPos||baseShowAsm)
	{
	char txt[256];
	char numBuf[64];
	char *freezeName = NULL;
	freezeName = hFreezeFromDb(database);
	sprintLongWithCommas(numBuf, winEnd-winStart);
	if(freezeName == NULL)
	    freezeName = "Unknown";
	if (baseShowPos&&baseShowAsm)
    	    safef(txt,sizeof(txt),"%s %s   %s (%s bp)",organism,freezeName,addCommasToPos(position),numBuf);
	else if (baseShowPos)
    	    safef(txt,sizeof(txt),"%s (%s bp)",addCommasToPos(position),numBuf);
	else
    	    safef(txt,sizeof(txt),"%s %s",organism,freezeName);
	vgTextCentered(vg, insideX, y, insideWidth, showPosHeight, 
			    MG_BLACK, font, txt);
	rulerClickHeight += showPosHeight;
	freez(&freezeName);
	y += showPosHeight;
	}
    
    vgDrawRulerBumpText(vg, insideX, y, rulerHeight, insideWidth, MG_BLACK, 
                        font, relNumOff, winBaseCount, 0, 1);
    {
    /* Make hit boxes that will zoom program around ruler. */
    int boxes = 30;
    int winWidth = winEnd - winStart;
    int newWinWidth = winWidth;
    int i, ws, we = 0, ps, pe = 0;
    int mid, ns, ne;
    double wScale = (double)winWidth/boxes;
    double pScale = (double)insideWidth/boxes;
    char message[32];
    char *zoomType = cartCgiUsualString(cart, RULER_BASE_ZOOM_VAR, ZOOM_3X);

    safef(message, sizeof(message), "%s zoom", zoomType);
    if (sameString(zoomType, ZOOM_1PT5X))
        newWinWidth = winWidth/1.5;
    else if (sameString(zoomType, ZOOM_3X))
        newWinWidth = winWidth/3;
    else if (sameString(zoomType, ZOOM_10X))
        newWinWidth = winWidth/10;
    else if (sameString(zoomType, ZOOM_BASE))
        newWinWidth = insideWidth/tl.mWidth;
    else
        errAbort("invalid zoom type %s", zoomType);

    if (newWinWidth < 1)
	newWinWidth = 1;

    for (i=1; i<=boxes; ++i)
	{
	ps = pe;
	ws = we;
	pe = round(pScale*i);
	we = round(wScale*i);
	mid = (ws + we)/2 + winStart;
	ns = mid-newWinWidth/2;
	ne = ns + newWinWidth;
	if (ns < 0)
	    {
	    ns = 0;
	    ne -= ns;
	    }
	if (ne > seqBaseCount)
	    {
	    ns -= (ne - seqBaseCount);
	    ne = seqBaseCount;
	    }
	mapBoxJumpTo(ps+insideX,rulerClickY,pe-ps,rulerClickHeight,
		        chromName, ns, ne, message);
	}
    }
    if (zoomedToBaseLevel || rulerCds)
        {
        Color baseColor = MG_BLACK;
        int start, end, chromSize;
        struct dnaSeq *extraSeq;
	/* extraSeq has extra leading & trailing bases
	 * for translation in to amino acids */
        boolean complementRulerBases = 
                cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE);
        if (complementRulerBases)
            baseColor = MG_GRAY;

        /* get sequence, with leading & trailing 3 bases
         * used for amino acid translation */
        start = max(winStart - 3, 0);
        chromSize = hChromSize(chromName);
        end = min(winEnd + 3, chromSize);
        extraSeq = hDnaFromSeq(chromName, start, end, dnaUpper);
        if (start != winStart - 3 || end != winEnd + 3)
            {
            /* at chromosome boundaries, pad with N's to assure
             * leading & trailing 3 bases */
            char header[4] = "NNN", trailer[4] = "NNN";
            int size = winEnd - winStart + 6;
            char *padded = (char *)needMem(size+1);
            header[max(3 - winStart, 0)] = 0;
            trailer[max(winEnd - chromSize + 3, 0)] = 0;
            safef(padded, size+1, "%s%s%s", header, extraSeq->dna, trailer);
            extraSeq = newDnaSeq(padded, strlen(padded), extraSeq->name);
            }

        /* for drawing bases, must clip off leading and trailing 3 bases */
        seq = cloneDnaSeq(extraSeq);
        seq = newDnaSeq(seq->dna+3, seq->size-6, seq->name);

        if (zoomedToBaseLevel)
    	    drawBases(vg, insideX, y+rulerHeight, insideWidth, baseHeight, 
		  baseColor, font, complementRulerBases, seq);

        /* set up clickable area to toggle ruler visibility */
            {
            char newRulerVis[100];
            safef(newRulerVis, 100, "%s=%s", RULER_TRACK_NAME,
                         rulerMode == tvFull ?  
                                rulerMenu[tvDense] : 
                                rulerMenu[tvFull]);
            mapBoxReinvokeExtra(insideX, y+rulerHeight, insideWidth,baseHeight, 
                                NULL, NULL, 0, 0, "", newRulerVis);
            }
        if (rulerCds)
            {
            /* display codons */
            int frame;
            int firstFrame = 0;
            int mod;            // for determining frame ordering on display
            struct simpleFeature *sfList;
            double scale = scaleForWindow(insideWidth, winStart, winEnd);

            /* WARNING: tricky code to assure that an amino acid
             * stays in the same frame line on the browser during panning.
             * There may be a simpler way... */
            if (complementRulerBases)
                mod = (chromSize - winEnd) % 3;
            else
                mod = winStart % 3;
            if (mod == 0)
                firstFrame = 0;
            else if (mod == 1)
                firstFrame = 2;
            else if (mod == 2)
                firstFrame = 1;

            y = yAfterBases;
            if (complementRulerBases)
                reverseComplement(extraSeq->dna, extraSeq->size);
            for (frame = 0; frame < 3; frame++, y += codonHeight)
                {
                /* reference frame to start of chromosome */
                int refFrame = (firstFrame + frame) % 3;

                /* create list of codons in the specified coding frame */
                sfList = baseColorCodonsFromDna(refFrame, winStart, winEnd,
                                             extraSeq, complementRulerBases); 
                /* draw the codons in the list, with alternating colors */
                baseColorDrawRulerCodons(vg, sfList, scale, insideX, y,
                                    codonHeight, font, winStart, MAXPIXELS,
                                    zoomedToCodonLevel);
                }
            }
        }
    vgUnclip(vg);
    }

/* Draw center labels. */
if (withCenterLabels)
    {
    vgSetClip(vg, insideX, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);
    y = yAfterRuler;
    for (track = trackList; track != NULL; track = track->next)
        {
        struct track *subtrack;
	if (track->limitedVis == tvHide)
	    continue;
        if (isCompositeTrack(track))
            {
	    if (isWithCenterLabels(track))
		{
		y = doCenterLabels(track, track, vg, font, y);
		}
	    else
		{
		for (subtrack = track->subtracks; subtrack != NULL;
		     subtrack = subtrack->next)
		    if (isSubtrackVisible(subtrack) &&
			isWithCenterLabels(subtrack))
			y = doCenterLabels(subtrack, track, vg, font, y);
		}
            }
        else
            y = doCenterLabels(track, track, vg, font, y);
        }
    vgUnclip(vg);
    }

/* Draw tracks. */
{
    long lastTime = 0;
    y = yAfterRuler;
    if (measureTiming)
        lastTime = clock1000();
    for (track = trackList; track != NULL; track = track->next)
	{
	if (track->limitedVis == tvHide)
            continue;
        if (isCompositeTrack(track))
            {
            struct track *subtrack;
	    if (isWithCenterLabels(track))
		y += fontHeight;
            for (subtrack = track->subtracks; subtrack != NULL;
                         subtrack = subtrack->next)
                if (isSubtrackVisible(subtrack))
                    y = doDrawItems(subtrack, vg, font, y, &lastTime);
            }
        else
            y = doDrawItems(track, vg, font, y, &lastTime);
        }
}
/* if a track can draw its left labels, now is the time since it
 *	knows what exactly happened during drawItems
 */
if (withLeftLabels)
    {
    y = yAfterRuler;
    for (track = trackList; track != NULL; track = track->next)
	{
	if (track->limitedVis == tvHide)
            continue;
	if (isCompositeTrack(track))
	    {
	    struct track *subtrack;
	    if (isWithCenterLabels(track))
		y += fontHeight;
	    for (subtrack = track->subtracks; subtrack != NULL;
		 subtrack = subtrack->next)
		if (isSubtrackVisible(subtrack))
		    {
		    if (subtrack->drawLeftLabels != NULL)
			y = doOwnLeftLabels(subtrack, vg, font, y);
		    else
			y += trackPlusLabelHeight(subtrack, fontHeight);
		    }
	    }
        else if (track->drawLeftLabels != NULL)
	    {
	    y = doOwnLeftLabels(track, vg, font, y);
	    }
        else
            {
	    y += trackPlusLabelHeight(track, fontHeight);
            }
        }
    }


/* Make map background. */
y = yAfterRuler;
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->limitedVis != tvHide)
        y = doTrackMap(track, y, fontHeight, trackPastTabX, trackPastTabWidth);
    }

/* Finish map. */
hPrintf("</MAP>\n");

/* Save out picture and tell html file about it. */
vgClose(&vg);
hPrintf("<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s ",
    gifTn.forHtml, pixWidth, pixHeight, mapName);
hPrintf("><BR>\n");
}


void printEnsemblAnchor(char *database)
/* Print anchor to Ensembl display on same window. */
{
char *scientificName = hScientificName(database);
char *dir = ensOrgNameFromScientificName(scientificName);
struct dyString *ensUrl;
char *name;
int start, end;

if (sameWord(scientificName, "Takifugu rubripes"))
    {
    /* for Fugu, must give scaffold, not chr coordinates */
    /* Also, must give "chrom" as "scaffold_N", name below. */
    if (!hScaffoldPos(chromName, winStart, winEnd,
                        &name, &start, &end))
        /* position doesn't appear on Ensembl browser.
         * Ensembl doesn't show scaffolds < 2K */
        return;
    }
else
    {
    name = chromName;
    start = winStart;
    end = winEnd;
    }
start += 1;
ensUrl = ensContigViewUrl(dir, name, seqBaseCount, start, end);
hPrintf("<A HREF=\"%s\" TARGET=_blank class=\"topbar\">", ensUrl->string);
/* NOTE: probably should free mem from dir and scientificName ?*/
dyStringFree(&ensUrl);
}

typedef void (*TrackHandler)(struct track *tg);

struct hash *handlerHash;

void registerTrackHandler(char *name, TrackHandler handler)
/* Register a track handling function. */
{
if (handlerHash == NULL)
    handlerHash = newHash(6);
if (hashLookup(handlerHash, name))
    warn("handler duplicated for track %s", name);
else
    hashAdd(handlerHash, name, handler);
}

TrackHandler lookupTrackHandler(char *name)
/* Lookup handler for track of give name.  Return NULL if
 * none. */
{
if (handlerHash == NULL)
    return NULL;
return hashFindVal(handlerHash, name);
}

boolean colorsSame(struct rgbColor *a, struct rgbColor *b)
/* Return true if two colors are the same. */
{
return a->r == b->r && a->g == b->g && a->b == b->b;
}

void loadSimpleBed(struct track *tg)
/* Load the items in one track - just move beds in
 * window... */
{
struct bed *(*loader)(char **row);
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
char option[128]; /* Option -  score filter */
char *optionScoreVal;
int optionScore = 0;
char query[128] ;

if (tg->bedSize <= 3)
    loader = bedLoad3;
else if (tg->bedSize == 4)
    loader = bedLoad;
else if (tg->bedSize == 5)
    loader = bedLoad5;
else
    loader = bedLoad6;

/* limit to items above a specified score */
safef(option, sizeof(option), "%s.scoreFilter", tg->mapName);
optionScoreVal = trackDbSetting(tg->tdb, "scoreFilter");
if (optionScoreVal != NULL)
    optionScore = atoi(optionScoreVal);
optionScore = cartUsualInt(cart, option, optionScore);

if (optionScore > 0 && tg->bedSize >= 5)
    {
    safef(query, sizeof(query), "score >= %d", optionScore);
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, 
                         query, &rowOffset);
    }
else
    {
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, 
                         NULL, &rowOffset);
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = loader(row+rowOffset);
    slAddHead(&list, bed);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = list;
}

void bed8To12(struct bed *bed)
/* Turn a bed 8 into a bed 12 by defining one block. */
{
// Make up a block: the whole thing.
bed->blockCount  = 1;
bed->blockSizes  = needMem(bed->blockCount * sizeof(int));
bed->chromStarts = needMem(bed->blockCount * sizeof(int));
bed->blockSizes[0]  = bed->chromEnd - bed->chromStart;
bed->chromStarts[0] = 0;
// Some tracks overload thickStart and thickEnd -- catch garbage here.
if ((bed->thickStart != 0) &&
    ((bed->thickStart < bed->chromStart) ||
     (bed->thickStart > bed->chromEnd)))
    bed->thickStart = bed->chromStart;
if ((bed->thickEnd != 0) &&
    ((bed->thickEnd < bed->chromStart) ||
     (bed->thickEnd > bed->chromEnd)))
    bed->thickEnd = bed->chromEnd;
}

void loadBed9(struct track *tg)
/* Convert bed 9 info in window to linked feature.  (to handle itemRgb)*/
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));
boolean useItemRgb = FALSE;
char optionScoreStr[128];
int optionScore;
char extraWhere[128] ;

useItemRgb = bedItemRgb(tdb);

safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", tg->mapName);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0) 
    {
    safef(extraWhere, sizeof(extraWhere), "score >= %d", optionScore);
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
		     extraWhere, &rowOffset);
    }
else
    {
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
		     NULL, &rowOffset);
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 9);
    bed8To12(bed);
    lf = lfFromBedExtra(bed, scoreMin, scoreMax);
    if (useItemRgb)
	{
	lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
	lf->filterColor=bed->itemRgb;
	}
    slAddHead(&lfList, lf);
    bedFree(&bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}


void loadBed8(struct track *tg)
/* Convert bed 8 info in window to linked feature. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));
boolean useItemRgb = FALSE;
char optionScoreStr[128];
int optionScore;
char extraWhere[128] ;

useItemRgb = bedItemRgb(tdb);

safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", tg->mapName);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0) 
    {
    safef(extraWhere, sizeof(extraWhere), "score >= %d", optionScore);
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
		     extraWhere, &rowOffset);
    }
else
    {
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
		     NULL, &rowOffset);
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 8);
    bed8To12(bed);
    lf = lfFromBedExtra(bed, scoreMin, scoreMax);
    if (useItemRgb)
	{
	lf->extra = (void *)USE_ITEM_RGB;       /* signal for coloring */
	lf->filterColor=bed->itemRgb;
	}
    slAddHead(&lfList, lf);
    bedFree(&bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

static void filterBed(struct track *tg, struct linkedFeatures **pLfList)
/* Apply filters if any to mRNA linked features. */
{
struct linkedFeatures *lf, *next, *newList = NULL, *oldList = NULL;
struct mrnaUiData *mud = tg->extraUiData;
struct mrnaFilter *fil;
char *type;
boolean anyFilter = FALSE;
boolean colorIx = 0;
boolean isExclude = FALSE;
boolean andLogic = TRUE;

if (*pLfList == NULL || mud == NULL)
    return;

/* First make a quick pass through to see if we actually have
 * to do the filter. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    fil->pattern = cartUsualString(cart, fil->key, "");
    if (fil->pattern[0] != 0)
        anyFilter = TRUE;
    }
if (!anyFilter)
    return;

type = cartUsualString(cart, mud->filterTypeVar, "red");
if (sameString(type, "exclude"))
    isExclude = TRUE;
else if (sameString(type, "include"))
    isExclude = FALSE;
else
    colorIx = getFilterColor(type, MG_BLACK);
type = cartUsualString(cart, mud->logicTypeVar, "and");
andLogic = sameString(type, "and");

/* Make a pass though each filter, and start setting up search for
 * those that have some text. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    fil->pattern = cartUsualString(cart, fil->key, "");
    if (fil->pattern[0] != 0)
	{
	fil->hash = newHash(10);
	}
    }

/* Scan tables id/name tables to build up hash of matching id's. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    struct hash *hash = fil->hash;
    int wordIx, wordCount;
    char *words[128];

    if (hash != NULL)
	{
	boolean anyWild;
	char *dupPat = cloneString(fil->pattern);
	wordCount = chopLine(dupPat, words);
	for (wordIx=0; wordIx <wordCount; ++wordIx)
	    {
	    char *pattern = cloneString(words[wordIx]);
	    if (lastChar(pattern) != '*')
		{
		int len = strlen(pattern)+1;
		pattern = needMoreMem(pattern, len, len+1);
		pattern[len-1] = '*';
		}
	    anyWild = (strchr(pattern, '*') != NULL || strchr(pattern, '?') != NULL);
	    touppers(pattern);
	    for(lf = *pLfList; lf != NULL; lf=lf->next)
		{
		char copy[64];
		boolean gotMatch;
		safef(copy, sizeof(copy), "%s", lf->name);
		touppers(copy);
		if (anyWild)
		    gotMatch = wildMatch(pattern, copy);
		else
		    gotMatch = sameString(pattern, copy);
		if (gotMatch)
		    {
		    hashAdd(hash, lf->name, NULL);
		    }
		}
	    freez(&pattern);
	    }
	freez(&dupPat);
	}
    }

/* Scan through linked features coloring and or including/excluding ones that 
 * match filter. */
for (lf = *pLfList; lf != NULL; lf = next)
    {
    boolean passed = andLogic;
    next = lf->next;
    for (fil = mud->filterList; fil != NULL; fil = fil->next)
	{
	if (fil->hash != NULL)
	    {
	    if (hashLookup(fil->hash, lf->name) == NULL)
		{
		if (andLogic)    
		    passed = FALSE;
		}
	    else
		{
		if (!andLogic)
		    passed = TRUE;
		}
	    }
	}
    if (passed ^ isExclude)
	{
	slAddHead(&newList, lf);
	if (colorIx > 0)
	    lf->filterColor = colorIx;
	}
    else
        {
	slAddHead(&oldList, lf);
	}
    }

slReverse(&newList);
slReverse(&oldList);
if (colorIx > 0)
   {
   /* Draw stuff that passes filter first in full mode, last in dense. */
   if (tg->visibility == tvDense)
       {
       newList = slCat(oldList, newList);
       }
   else
       {
       newList = slCat(newList, oldList);
       }
   }
*pLfList = newList;
tg->limitedVisSet = FALSE;	/* Need to recalculate this after filtering. */

/* Free up hashes, etc. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    hashFree(&fil->hash);
    }
}

void loadGappedBed(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));
char optionScoreStr[128]; /* Option -  score filter */
int optionScore;
char extraWhere[128] ;
boolean useItemRgb = FALSE;

useItemRgb = bedItemRgb(tdb);

/* Use tg->tdb->tableName because subtracks inherit composite track's tdb 
 * by default, and the variable is named after the composite track. */
safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter",
      tg->tdb->tableName);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0) 
    {
    safef(extraWhere, sizeof(extraWhere), "score >= %d", optionScore);
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, extraWhere, &rowOffset);
    }
else
    {
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoad12(row+rowOffset);
    lf = lfFromBedExtra(bed, scoreMin, scoreMax);
    if (useItemRgb)
	{
	lf->extra = (void *)USE_ITEM_RGB;       /* signal for coloring */
	lf->filterColor=bed->itemRgb;
	}
    slAddHead(&lfList, lf);
    bedFree(&bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);
if(tg->extraUiData)
    filterBed(tg, &lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

void loadValAl(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct linkedFeatures *lfList = NULL, *lf;
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 15);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
//slReverse(&list);

for (bed = list; bed != NULL; bed = bed->next)
    {
    struct simpleFeature *sf;
    int i;
    lf = lfFromBed(bed);
    lf->grayIx = 9;
    slReverse(&lf->components);
    for (sf = lf->components, i = 0; sf != NULL && i < bed->expCount;
		sf = sf->next, i++)
	{
	sf->grayIx = bed->expIds[i];
	//sf->grayIx = grayInRange((int)(bed->expIds[i]),11,13);
	}
    slAddHead(&lfList,lf);
    }
tg->items = lfList;
}

void valAlDrawAt(struct track *tg, void *item,
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw the operon at position. */
{
struct linkedFeatures *lf = item; 
struct simpleFeature *sf;
int heightPer = tg->heightPer;
int x1,x2;
int s, e;
int midY = y + (heightPer>>1);
int w;

color = shadesOfGray[2];
x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
w = x2-x1;
innerLine(vg, x1, midY, w, color);
/*
if (vis == tvFull || vis == tvPack)
    {
    clippedBarbs(vg, x1, midY, w, tl.barbHeight, tl.barbSpacing, 
		 lf->orientation, color, FALSE);
    }
    */
//for(count=1; count < 4; count++)
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    int yOff; 
    s = sf->start; e = sf->end;
    /* shade ORF (exon) based on the grayIx value of the sf */
    switch(sf->grayIx)
	{
	case 1:
	    continue;
	    heightPer = tg->heightPer>>2;
	    yOff = y;
	    color = vgFindColorIx(vg, 204, 204,204);
	    break;
	case 2:
	    heightPer = tg->heightPer;
	    yOff = y;
	    color = vgFindColorIx(vg, 252, 90, 90);
	    break;
	case 3:
	    yOff = midY -(tg->heightPer>>2);
	    heightPer = tg->heightPer>>1;
	    color = vgFindColorIx(vg, 0, 0,0);
	    break;
	default:
	    continue;
	}
    //if (sf->grayIx == count)
    drawScaledBox(vg, s, e, scale, xOff, yOff, heightPer,
			color );
    }
}

void valAlMethods(struct track *tg)
{
linkedFeaturesMethods(tg);
tg->loadItems = loadValAl;
tg->colorShades = shadesOfGray;
tg->drawItemAt = valAlDrawAt;
}


void loadBlatz(struct track *tg)
{
enum trackVisibility vis = tg->visibility;
loadXenoPsl(tg);
if (vis != tvDense)
    {
    lookupProteinNames(tg);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
vis = limitVisibility(tg);
}

void loadBlast(struct track *tg)
{
enum trackVisibility vis = tg->visibility;
loadProteinPsl(tg);
if (vis != tvDense)
    {
    lookupProteinNames(tg);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
vis = limitVisibility(tg);
}

Color blastNameColor(struct track *tg, void *item, struct vGfx *vg)
{
return 1;
}

void blatzMethods(struct track *tg)
/* blatz track methods */
{
tg->loadItems = loadBlatz;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = blastColor;
tg->itemNameColor = blastNameColor;
}

void blastMethods(struct track *tg)
/* blast protein track methods */
{
tg->loadItems = loadBlast;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = blastColor;
tg->itemNameColor = blastNameColor;
}


static void drawTri(struct vGfx *vg, int x, int y1, int y2, Color color,
	char strand)
/* Draw traingle. */
{
struct gfxPoly *poly = gfxPolyNew();
int half = (y2 - y1) / 2;
if (strand == '-')
    {
    gfxPolyAddPoint(poly, x, y1+half);
    gfxPolyAddPoint(poly, x+half, y1);
    gfxPolyAddPoint(poly, x+half, y2);
    }
else
    {
    gfxPolyAddPoint(poly, x, y1);
    gfxPolyAddPoint(poly, x+half, y1+half);
    gfxPolyAddPoint(poly, x, y2);
    }
vgDrawPoly(vg, poly, color, TRUE);
gfxPolyFree(&poly);
}

static void triangleDrawAt(struct track *tg, void *item,
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a right- or left-pointing triangle at position. 
 * If item has width > 1 or block/cds structure, those will be ignored -- 
 * this only draws a triangle (direction depending on strand). */
{
struct bed *bed = item; 
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int y2 = y + tg->heightPer-1;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, vg);
else
    {
    if (tg->colorShades)
	color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
    }

drawTri(vg, x1, y, y2, color, bed->strand[0]);
}

void simpleBedTriangleMethods(struct track *tg)
/* Load up simple bed features methods, but use triangleDrawAt. */
{
bedMethods(tg);
tg->drawItemAt = triangleDrawAt;
}

void loadColoredExonBed(struct track *tg)
/* Load the items into a linkedFeaturesSeries. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bed;
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
char optionScoreStr[128]; /* Option -  score filter */
int optionScore;
char extraWhere[128] ;
/* Use tg->tdb->tableName because subtracks inherit composite track's tdb 
 * by default, and the variable is named after the composite track. */
safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter",
      tg->tdb->tableName);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0) 
    {
    safef(extraWhere, sizeof(extraWhere), "score >= %d", optionScore);
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, extraWhere, &rowOffset);
    }
else
    {
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 14);
    lfs = lfsFromColoredExonBed(bed);
    slAddHead(&lfsList, lfs);
    bedFree(&bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfsList);
slSort(&lfsList, linkedFeaturesSeriesCmp);
tg->items = lfsList;
}

void ctLoadColoredExon(struct track *tg)
/* Convert bed info in window to linked features series for custom track. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
    struct sqlResult *sr = NULL;
    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
		     NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	lfs = lfsFromColoredExonBed(bed);
	slAddHead(&lfsList, lfs);
	}
    hFreeOrDisconnect(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
	if (bed->chromStart < winEnd && bed->chromEnd > winStart 
		    && sameString(chromName, bed->chrom))
	    {
	    lfs = lfsFromColoredExonBed(bed);
	    slAddHead(&lfsList, lfs);
	    }
	}
    }
slReverse(&lfsList);
slSort(&lfsList, linkedFeaturesSeriesCmp);
tg->items = lfsList;
}

void coloredExonMethods(struct track *tg)
/* For BED 14 type "coloredExon" tracks. */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadColoredExonBed;
tg->canPack = TRUE;
}

void coloredExonMethodsFromCt(struct track *tg)
/* same as coloredExonMethods but different loader. */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = ctLoadColoredExon;
tg->canPack = TRUE;
}

boolean genePredClassFilter(struct track *tg, void *item)
/* Returns true if an item should be added to the filter. */
{
struct linkedFeatures *lf = item;
char *classString;
char *table = tg->mapName;
char *classType = NULL;
enum acemblyOptEnum ct;
struct sqlConnection *conn = NULL;
char query[256];
char **row = NULL;
struct sqlResult *sr;
char *classTable = NULL;
/* default is true then for no filtering */
boolean sameClass = TRUE;
classTable = trackDbSetting(tg->tdb, GENEPRED_CLASS_TBL);

AllocVar(classString);
if (classTable != NULL && hTableExists(classTable))
    {
    classString = addSuffix(table, ".type");
    if (sameString(table, "acembly"))
        {
        classType = cartUsualString(cart, classString, acemblyEnumToString(0));
        ct = acemblyStringToEnum(classType);
        if (ct == acemblyAll)
            return sameClass;
        }
    else if (classType == NULL)
        return TRUE;
    conn = hAllocConn();
    safef(query, sizeof(query),
         "select class from %s where name = \"%s\"", classTable, lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        /* check if this is the same as the class required */
        if (!sameString(row[0], classType))
            sameClass = FALSE;
        }
    sqlFreeResult(&sr);
    }
freeMem(classString);
hFreeConn(&conn);
return sameClass;
}

void loadGenePred(struct track *tg)
/* Convert gene pred in window to linked feature. */
{
tg->items = lfFromGenePredInRange(tg, tg->mapName, chromName, winStart, winEnd);
/* filter items on selected criteria if filter is available */
filterItems(tg, genePredClassFilter, "include");

    
}

void loadGenePredWithConfiguredName(struct track *tg)
/* Convert gene pred info in window to linked feature. Include name
 * in "extra" field (gene name, accession, or both, depending on UI) */
{
char buf[64];
char *geneLabel;
boolean useGeneName, useAcc;
struct linkedFeatures *lf;

safef(buf, sizeof buf, "%s.label", tg->mapName);
geneLabel = cartUsualString(cart, buf, "gene");
useGeneName = sameString(geneLabel, "gene") || sameString(geneLabel, "both");
useAcc = sameString(geneLabel, "accession") || sameString(geneLabel, "both");

loadGenePredWithName2(tg);
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(64);
    if (useGeneName && lf->extra)
        dyStringAppend(name, lf->extra);
    if (useGeneName && useAcc)
        dyStringAppendC(name, '/');
    if (useAcc)
        dyStringAppend(name, lf->name);
    lf->extra = dyStringCannibalize(&name);
    }
}

Color genePredItemAttrColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw a genePred in based on looking it up in a itemAttr
 * table. */
{
struct linkedFeatures *lf = item;
if (lf->itemAttr != NULL)
    return vgFindColorIx(vg, lf->itemAttr->colorR, lf->itemAttr->colorG, lf->itemAttr->colorB);
else
    return tg->ixColor;
}

Color genePredItemClassColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw a genePred based on looking up the gene class */
/* in an itemClass table. */
{
char *geneClasses = trackDbSetting(tg->tdb, GENEPRED_CLASS_VAR);
char *gClassesClone = NULL;
int class, classCt = 0;
char *classes[20];
char gClass[64];
char *classTable = trackDbSetting(tg->tdb, GENEPRED_CLASS_TBL);
struct linkedFeatures *lf = item;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row = NULL;
char query[256];
boolean found = FALSE;
char *colorString = NULL, *colorClone = NULL;
struct rgbColor gClassColor;
int color = tg->ixColor; /* default color in trackDb */
int size = 3;
char *rgbVals[5];
char *sep = ",";

if (geneClasses == NULL)
   errAbort(
      "Track %s missing required trackDb setting: geneClasses", tg->mapName);
if (geneClasses)
   {
   gClassesClone = cloneString(geneClasses);
   classCt = chopLine(gClassesClone, classes);
   }
if (hTableExists(classTable))
   {
   safef(query, sizeof(query), 
        "select class from %s where name = \"%s\"", classTable, lf->name);   
   sr = sqlGetResult(conn, query);
   if ((row = sqlNextRow(sr)) != NULL)
        {
        /* scan through groups to find a match */
        for (class = 0; class < classCt; class++)
           {
           if (sameString(classes[class], row[0]))
           /* get color from trackDb settings hash */
              {
              found = TRUE;
              safef(gClass, sizeof(gClass), "%s%s", GENEPRED_CLASS_PREFIX, classes[class]);
              colorString = trackDbSetting(tg->tdb, gClass);
              if (!colorString)
                  found = FALSE;
              break;
              }
           }
        }
   sqlFreeResult(&sr);
   if (found) 
      {
      /* need to convert color string to rgb */
      // check how these are found for trackDb
      colorClone = cloneString(colorString);     
      chopString(colorClone, sep, rgbVals, size);
      gClassColor.r = (sqlUnsigned(rgbVals[0]));
      gClassColor.g = (sqlUnsigned(rgbVals[1]));
      gClassColor.b = (sqlUnsigned(rgbVals[2]));
   
      /* find index for color */
      color = vgFindRgb(vg, &gClassColor);
      }
   }
hFreeConn(&conn);
/* return index for color to draw item */
return color;
}

void drawColorMethods(struct track *tg)
/* Fill in color track items based on chrom  */
{
char option[128]; /* Option -  rainbow chromosome color */
char *optionStr ;
safef( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "off");
tg->mapItemName = lfMapNameFromExtra;
if( sameString( optionStr, "on" )) /*use chromosome coloring*/
    tg->itemColor = lfChromColor;
else
    tg->itemColor = NULL;
linkedFeaturesMethods(tg);
tg->loadItems = loadGenePred;
}

Color gencodeIntronColorItem(struct track *tg, void *item, struct vGfx *vg)
/* Return color of ENCODE gencode intron track item.
 * Use recommended color palette pantone colors (level 4) for red, green, blue*/
{
struct gencodeIntron *intron = (struct gencodeIntron *)item;

if (sameString(intron->status, "not_tested"))
    return vgFindColorIx(vg, 214,214,216);       /* light grey */
if (sameString(intron->status, "RT_negative"))
    return vgFindColorIx(vg, 145,51,56);       /* red */
if (sameString(intron->status, "RT_positive") ||
        sameString(intron->status, "RACE_validated"))
    return vgFindColorIx(vg, 61,142,51);       /* green */
if (sameString(intron->status, "RT_wrong_junction"))
    return getOrangeColor(vg);                 /* orange */
if (sameString(intron->status, "RT_submitted"))
    return vgFindColorIx(vg, 102,109,112);       /* grey */
return vgFindColorIx(vg, 214,214,216);       /* light grey */
}

static void gencodeIntronLoadItems(struct track *tg)
/* Load up track items. */
{
bedLoadItem(tg, tg->mapName, (ItemLoader)gencodeIntronLoad);
}

static void gencodeIntronMethods(struct track *tg)
/* Load up custom methods for ENCODE Gencode intron validation track */
{
tg->loadItems = gencodeIntronLoadItems;
tg->itemColor = gencodeIntronColorItem;
}

static void gencodeGeneMethods(struct track *tg)
/* Load up custom methods for ENCODE Gencode gene track */
{
tg->loadItems = loadGenePredWithConfiguredName;
tg->itemName = gencodeGeneName;
}

void loadDless(struct track *tg) 
/* Load dless items */
{
struct sqlConnection *conn = hAllocConn();
struct dless *dless, *list = NULL;
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, 
                 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    dless = dlessLoad(row+rowOffset);
    slAddHead(&list, dless);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = list;
}

void freeDless(struct track *tg)
/* Free dless items. */
{
dlessFreeList(((struct dless **)(&tg->items)));
}

char *dlessName(struct track *tg, void *item) 
/* Get name to use for dless item */
{
struct dless *dl = item;
if (sameString(dl->type, "conserved"))
    return dl->type; 
else
    return dl->branch;
}

Color dlessColor(struct track *tg, void *item, struct vGfx *vg) 
/* Return color for dless item */
{
struct dless *dl = item;
char *rgb[4];
int count;
char *rgbStr;
static boolean gotColors = FALSE;
static Color consColor, gainColor, lossColor;

if (!gotColors) 
{
    consColor = tg->ixColor;
    gainColor = vgFindColorIx(vg, 0, 255, 0);
    lossColor = vgFindColorIx(vg, 255, 0, 0);

    if ((rgbStr = trackDbSetting(tg->tdb, "gainColor")) != NULL)
        {
        count = chopString(rgbStr, ",", rgb, ArraySize(rgb));
        if (count == 3 && isdigit(rgb[0][0]) && isdigit(rgb[1][0]) &&
            isdigit(rgb[2][0]))
            gainColor = vgFindColorIx(vg, atoi(rgb[0]), atoi(rgb[1]), atoi(rgb[2])); 
        }

    if ((rgbStr = trackDbSetting(tg->tdb, "lossColor")) != NULL)
        {
        count = chopString(rgbStr, ",", rgb, ArraySize(rgb));
        if (count == 3 && isdigit(rgb[0][0]) && isdigit(rgb[1][0]) &&
            isdigit(rgb[2][0]))
            lossColor = vgFindColorIx(vg, atoi(rgb[0]), atoi(rgb[1]), atoi(rgb[2])); 
        }

    gotColors = TRUE;
}

if (sameString(dl->type, "conserved"))
    return consColor;
else if (sameString(dl->type, "gain"))
    return gainColor;
else 
    return lossColor;
}

static void dlessMethods(struct track *tg)
/* Load custom methods for dless track */
{
tg->itemColor = dlessColor;
tg->loadItems = loadDless;
tg->itemName = dlessName;
tg->freeItems = freeDless;
}

char *vegaGeneName(struct track *tg, void *item)
{
static char cat[128];
struct linkedFeatures *lf = item;
if (lf->extra != NULL) 
    {
    sprintf(cat,"%s",(char *)lf->extra);
    return cat;
    }
else 
    return lf->name;
}

void vegaMethods(struct track *tg)
/* Special handling for vegaGene/vegaPseudoGene items. */
{
tg->loadItems = loadGenePredWithName2;
tg->itemColor = vegaColor;
tg->itemName = vegaGeneName;
}

Color gvColorByCount(struct track *tg, void *item, struct vGfx *vg)
/* color items by whether they are single position or multiple */
{
struct gvPos *el = item;
struct sqlConnection *conn = hAllocConn();
char *escId = NULL;
char *multColor = NULL, *singleColor = NULL;
int num = 0;
char query[256];
if (el->id != NULL)
    escId = sqlEscapeString(el->id);
else
    escId = sqlEscapeString(el->name);
safef(query, sizeof(query), "select count(*) from gvPos where name = '%s'",
    escId);
num = sqlQuickNum(conn, query);
hFreeConn(&conn);
freeMem(escId);
singleColor = cartUsualString(cart, "gvColorCountSingle", "blue");
multColor = cartUsualString(cart, "gvColorCountMult", "green");
if (num == 1) 
    {
    if (sameString(singleColor, "red"))
        return vgFindColorIx(vg, 221, 0, 0); /* dark red */
    else if (sameString(singleColor, "orange"))
        return vgFindColorIx(vg, 255, 153, 0);
    else if (sameString(singleColor, "green"))
        return vgFindColorIx(vg, 0, 153, 0); /* dark green */
    else if (sameString(singleColor, "gray"))
        return MG_GRAY;
    else if (sameString(singleColor, "purple"))
        return vgFindColorIx(vg, 204, 0, 255);
    else if (sameString(singleColor, "blue"))
        return MG_BLUE;
    else if (sameString(singleColor, "brown"))
        return vgFindColorIx(vg, 100, 50, 0); /* brown */
    else
        return MG_BLACK;
    }
else if (num > 1) 
    {
    if (sameString(multColor, "red"))
        return vgFindColorIx(vg, 221, 0, 0); /* dark red */
    else if (sameString(multColor, "orange"))
        return vgFindColorIx(vg, 255, 153, 0);
    else if (sameString(multColor, "green"))
        return vgFindColorIx(vg, 0, 153, 0); /* dark green */
    else if (sameString(multColor, "gray"))
        return MG_GRAY;
    else if (sameString(multColor, "purple"))
        return vgFindColorIx(vg, 204, 0, 255);
    else if (sameString(multColor, "blue"))
        return MG_BLUE;
    else if (sameString(multColor, "brown"))
        return vgFindColorIx(vg, 100, 50, 0); /* brown */
    else
        return MG_BLACK;
    }
else 
    return MG_BLACK;
}

Color gvColorByDisease(struct track *tg, void *item, struct vGfx *vg)
/* color items by whether they are known or likely to cause disease */
{
struct gvPos *el = item;
struct gvAttr *attr = NULL;
struct sqlConnection *conn = hAllocConn();
char *escId = NULL;
char *useColor = NULL;
int index = -1;
char query[256];
if (el->id != NULL)
    escId = sqlEscapeString(el->id);
else
    escId = sqlEscapeString(el->name);
safef(query, sizeof(query), "select * from hgFixed.gvAttr where id = '%s' and attrType = 'disease'", escId);
attr = gvAttrLoadByQuery(conn, query);
if (attr == NULL)
    {
    AllocVar(attr);
    attr->attrVal = cloneString("NULL");
    attr->id = NULL; /* so free will work */
    attr->attrType = NULL;
    }
index = stringArrayIx(attr->attrVal, gvColorDAAttrVal, gvColorDASize);
if (index < 0 || index >= gvColorDASize)
    {
    hFreeConn(&conn);
    return MG_BLACK;
    }
useColor = cartUsualString(cart, gvColorDAStrings[index], gvColorDADefault[index]);
gvAttrFreeList(&attr);
hFreeConn(&conn);
freeMem(escId);
if (sameString(useColor, "red"))
    return vgFindColorIx(vg, 221, 0, 0); /* dark red */
else if (sameString(useColor, "orange"))
    return vgFindColorIx(vg, 255, 153, 0);
else if (sameString(useColor, "green"))
    return vgFindColorIx(vg, 0, 153, 0); /* dark green */
else if (sameString(useColor, "gray"))
    return MG_GRAY;
else if (sameString(useColor, "purple"))
    return vgFindColorIx(vg, 204, 0, 255);
else if (sameString(useColor, "blue"))
    return MG_BLUE;
else if (sameString(useColor, "brown"))
    return vgFindColorIx(vg, 100, 50, 0); /* brown */
else
    return MG_BLACK;
}

Color gvColorByType(struct track *tg, void *item, struct vGfx *vg)
/* color items by type */
{
struct gvPos *el = item;
struct gv *details = NULL;
struct sqlConnection *conn = hAllocConn();
char *typeColor = NULL;
int index = 5;
char *escId = NULL; 
char query[256];
if (el->id != NULL)
    escId = sqlEscapeString(el->id);
else
    escId = sqlEscapeString(el->name);

safef(query, sizeof(query), "select * from hgFixed.gv where id = '%s'", escId);
details = gvLoadByQuery(conn, query);
index = stringArrayIx(details->baseChangeType, gvColorTypeBaseChangeType, gvColorTypeSize);
if (index < 0 || index >= gvColorTypeSize)
    {
    hFreeConn(&conn);
    return MG_BLACK;
    }
typeColor = cartUsualString(cart, gvColorTypeStrings[index], gvColorTypeDefault[index]);
gvFreeList(&details);
hFreeConn(&conn);
freeMem(escId);
if (sameString(typeColor, "purple"))
    return vgFindColorIx(vg, 204, 0, 255);
else if (sameString(typeColor, "green"))
    return vgFindColorIx(vg, 0, 153, 0); /* dark green */
else if (sameString(typeColor, "orange"))
    return vgFindColorIx(vg, 255, 153, 0); 
else if (sameString(typeColor, "blue"))
    return MG_BLUE;
else if (sameString(typeColor, "brown"))
    return vgFindColorIx(vg, 100, 50, 0); /* brown */
else if (sameString(typeColor, "gray"))
    return MG_GRAY;
else if (sameString(typeColor, "red"))
    return vgFindColorIx(vg, 221, 0, 0); /* dark red */
else 
    return MG_BLACK;
}

Color gvColor(struct track *tg, void *item, struct vGfx *vg)
/* color items, multiple choices for determination */
{
char *choice = NULL;
choice = cartOptionalString(cart, "gvPos.filter.colorby");
if (choice != NULL && sameString(choice, "type"))
    return gvColorByType(tg, item, vg);
else if (choice != NULL && sameString(choice, "count"))
    return gvColorByCount(tg, item, vg);
else if (choice != NULL && sameString(choice, "disease"))
    return gvColorByDisease(tg, item, vg);
else
    return gvColorByType(tg, item, vg);
}

boolean gvFilterAccuracy(struct gv *el)
/* Check to see if this element should be excluded. */
{
int cnt = 0;
for (cnt = 0; cnt < gvAccuracySize; cnt++)
    {
    if (cartVarExists(cart, gvAccuracyString[cnt]) &&
        cartString(cart, gvAccuracyString[cnt]) != NULL &&
        differentString(cartString(cart,gvAccuracyString[cnt]), "0") &&
        gvAccuracyDbValue[cnt] == el->coordinateAccuracy)
        {
        /* string 0/1 unselected/selected, unsigned 0/1 estimated/known */
        return FALSE;
        }
    }
return TRUE;
}

boolean gvFilterSrc(struct gv *el, struct gvSrc *srcList)
/* Check to see if this element should be excluded. */
{
int cnt = 0;
struct gvSrc *src = NULL;
char *srcTxt = NULL;
struct hashEl *filterList = NULL;

for (src = srcList; src != NULL; src = src->next)
    {
    if (sameString(el->srcId, src->srcId))
        {
        srcTxt = cloneString(src->src);
        break;
        }
    }
filterList = cartFindPrefix(cart, "gvPos.filter.src.");
if (srcTxt == NULL)
    errAbort("Bad value for srcId");
else if (filterList == NULL) 
    {
    /* if no src filters, set or unset, use defaults */
    cartSetInt(cart, gvSrcString[0], 1);
    }
hashElFreeList(&filterList);

for (cnt = 0; cnt < gvSrcSize; cnt++)
    {
    if (cartVarExists(cart, gvSrcString[cnt]) &&
        cartString(cart, gvSrcString[cnt]) != NULL &&
        differentString(cartString(cart,gvSrcString[cnt]), "0") &&
        sameString(gvSrcDbValue[cnt], srcTxt))
        {
        return FALSE;
        }
    }
return TRUE;
}

boolean gvFilterDA(struct gv *el)
/* Check to see if this element should be excluded (disease association attribute) */
{
int cnt = 0;
struct gvAttr *attr = NULL;
char query[256];
char *escId = NULL;
struct sqlConnection *conn = hAllocConn();

if (el->id != NULL)
    escId = sqlEscapeString(el->id);
else
    escId = sqlEscapeString(el->name);

safef(query, sizeof(query), "select * from hgFixed.gvAttr where id = '%s' and attrType = 'disease'", escId);
attr = gvAttrLoadByQuery(conn, query);
hFreeConn(&conn);
if (attr == NULL) 
    {
    AllocVar(attr);
    attr->attrVal = cloneString("NULL");
    attr->id = NULL; /* so free will work */
    attr->attrType = NULL;
    }
for (cnt = 0; cnt < gvFilterDASize; cnt++)
    {
    if (cartVarExists(cart, gvFilterDAString[cnt]) &&
        cartString(cart, gvFilterDAString[cnt]) != NULL &&
        differentString(cartString(cart, gvFilterDAString[cnt]), "0") &&
        sameString(gvFilterDADbValue[cnt], attr->attrVal))
        {
        gvAttrFree(&attr);
        return FALSE;
        }
    }
gvAttrFree(&attr);
return TRUE;
}

boolean gvFilterType(struct gv *el)
/* Check to see if this element should be excluded. */
{
int cnt = 0;
for (cnt = 0; cnt < gvTypeSize; cnt++)
    {
    if (cartVarExists(cart, gvTypeString[cnt]) &&
        cartString(cart, gvTypeString[cnt]) != NULL &&
        differentString(cartString(cart, gvTypeString[cnt]), "0") &&
        sameString(gvTypeDbValue[cnt], el->baseChangeType))
        {
        return FALSE;
        }
    }
return TRUE;
}

boolean gvFilterLoc(struct gv *el)
/* Check to see if this element should be excluded. */
{
int cnt = 0;
for (cnt = 0; cnt < gvLocationSize; cnt++)
    {
    if (cartVarExists(cart, gvLocationString[cnt]) &&
        cartString(cart, gvLocationString[cnt]) != NULL &&
        differentString(cartString(cart, gvLocationString[cnt]), "0") &&
        sameString(gvLocationDbValue[cnt], el->location))
        {
        return FALSE;
        }
    }
return TRUE;
}

void lookupGvName(struct track *tg)
/* give option on which name to display */
{
struct gvPos *el;
struct sqlConnection *conn = hAllocConn();
boolean useHgvs = FALSE;
boolean useId = FALSE;
boolean useCommon = FALSE;
boolean labelStarted = FALSE;

struct hashEl *gvLabels = cartFindPrefix(cart, "gvPos.label");
struct hashEl *label;
if (gvLabels == NULL)
    {
    useHgvs = TRUE; /* default to gene name */
    /* set cart to match what is being displayed */
    cartSetBoolean(cart, "gvPos.label.hgvs", TRUE);
    }
for (label = gvLabels; label != NULL; label = label->next)
    {
    if (endsWith(label->name, "hgvs") && differentString(label->val, "0"))
        useHgvs = TRUE;
    else if (endsWith(label->name, "common") && differentString(label->val, "0"))
        useCommon = TRUE;
    else if (endsWith(label->name, "dbid") && differentString(label->val, "0"))
        useId = TRUE;
    }
if (!useHgvs && !useCommon && !useId) 
    {
    /* assume noone really wants no names, squish will still remove names */
    useHgvs = TRUE;
    cartSetBoolean(cart, "gvPos.label.hgvs", TRUE);
    }

for (el = tg->items; el != NULL; el = el->next)
    {
    struct dyString *name = dyStringNew(64);
    labelStarted = FALSE; /* reset for each item */
    if (useHgvs) 
        {
        dyStringAppend(name, el->label);
        labelStarted = TRUE;
        }
    if (useCommon)
        {
        char query[256];
        char *commonName = NULL;
        char *escId = sqlEscapeString(el->name);
        safef(query, sizeof(query), "select attrVal from hgFixed.gvAttr where id = '%s' and attrType = 'commonName'", escId);
        commonName = sqlQuickString(conn, query);
        freeMem(escId);
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        if (commonName != NULL)
            dyStringAppend(name, commonName);
        else
            dyStringAppend(name, " ");
        }
    if (useId)
        {
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        dyStringAppend(name, el->name);
        }
    el->id = el->name; /* reassign ID */
    el->name = dyStringCannibalize(&name);
    }
hFreeConn(&conn);
}

void loadGV(struct track *tg)
/* Load human mutation with filter */
{
struct gvPos *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr;
struct gvSrc *srcList = NULL;
char **row;
int rowOffset;
enum trackVisibility vis = tg->visibility;

/* load as linked list once, outside of loop */
srcList = gvSrcLoadByQuery(conn, "select * from hgFixed.gvSrc");
/* load part need from gv table, outside of loop (load in hash?) */
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct gv *details = NULL;
    char query[256], *escId;
    struct gvPos *el = gvPosLoad(row);
    escId = sqlEscapeString(el->name);
    safef(query, sizeof(query), "select * from hgFixed.gv where id = '%s'", escId);
    details = gvLoadByQuery(conn2, query);
    if (!gvFilterType(details))
        gvPosFree(&el);
    else if (!gvFilterLoc(details))
        gvPosFree(&el);
    else if (!gvFilterSrc(details, srcList))
        gvPosFree(&el);
    else if (!gvFilterAccuracy(details))
        gvPosFree(&el);
    else if (!gvFilterDA(details))
        gvPosFree(&el);
    else
        slAddHead(&list, el);
    gvFreeList(&details);
    }
sqlFreeResult(&sr);
slReverse(&list);
gvSrcFreeList(&srcList);
tg->items = list;
/* change names here so not affected if change filters later 
   and no extra if when viewing dense                        */
if (vis != tvDense)
    {
    lookupGvName(tg);
    }
hFreeConn(&conn);
hFreeConn(&conn2);
}

boolean oregannoFilterType (struct oreganno *el)
/* filter of the type of region from the oregannoAttr table */
{
int cnt = 0;
struct oregannoAttr *attr = NULL;
char query[256];
struct sqlConnection *conn = hAllocConn();

safef(query, sizeof(query), "select * from oregannoAttr where id = '%s' and attribute = 'type'", el->id);
attr = oregannoAttrLoadByQuery(conn, query);
hFreeConn(&conn);
if (attr == NULL) 
    {
    AllocVar(attr);
    attr->attrVal = cloneString("NULL");
    attr->id = NULL; /* so free will work */
    attr->attribute = NULL;
    }
for (cnt = 0; cnt < oregannoTypeSize; cnt++)
    {
    if (cartVarExists(cart, oregannoTypeString[cnt]) &&        
        cartString(cart, oregannoTypeString[cnt]) != NULL &&        
        differentString(cartString(cart, oregannoTypeString[cnt]), "0") && 
        sameString(oregannoTypeDbValue[cnt], attr->attrVal))
        {
        oregannoAttrFree(&attr);
        return FALSE;
        }
    }
oregannoAttrFree(&attr);
return TRUE;
}

void loadOreganno (struct track *tg)
/* loads the oreganno track */
{
struct oreganno *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
                 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct oreganno *el = oregannoLoad(row);
    if (!oregannoFilterType(el)) 
        oregannoFree(&el);
    else
        slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
tg->items = list;
hFreeConn(&conn);
}

char *gvName(struct track *tg, void *item)
/* Get name to use for gv item. */
{
struct gvPos *el = item;
return el->name;
}

char *gvPosMapName (struct track *tg, void *item)
/* return id for item */
{
struct gvPos *el = item;
return el->id;
}

void gvMethods (struct track *tg)
/* Simple exclude/include filtering on human mutation items and color. */
{
tg->loadItems = loadGV;
tg->itemColor = gvColor;
tg->itemNameColor = gvColor;
tg->itemName = gvName;
tg->mapItemName = gvPosMapName;
}

void oregannoMethods (struct track *tg)
/* load so can allow filtering on type */
{
tg->loadItems = loadOreganno;
}

void loadBed12Source(struct track *tg)
/* Load bed 12 with extra "source" column as lf with extra value. */
{
struct linkedFeatures *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
                 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed12Source *el = bed12SourceLoad(row+rowOffset);
    struct linkedFeatures *lf = lfFromBed((struct bed *)el);
    lf->extra = (void *)cloneString(el->source);
    bed12SourceFree(&el);
    slAddHead(&list, lf);
    }
sqlFreeResult(&sr);
tg->items = list;
}

char *jaxAlleleName(struct track *tg, void *item)
/* Strip off initial NM_\d+ from jaxAllele item name. */
{
static char truncName[128];
struct linkedFeatures *lf = item;
/* todo: make this check a cart variable so it can be hgTrackUi-tweaked. */
assert(lf->name);
if (startsWith("NM_", lf->name) || startsWith("XM_", lf->name))
    {
    char *ptr = lf->name + 3;
    while (isdigit(ptr[0]))
	ptr++;
    if (ptr[0] == '_')
	ptr++;
    safef(truncName, sizeof(truncName), "%s", ptr);
    return truncName;
    }
else
    return lf->name;
}

void jaxAlleleMethods(struct track *tg)
/* Fancy name fetcher for jaxAllele. */
{
linkedFeaturesMethods(tg);
tg->loadItems = loadBed12Source;
tg->itemName = jaxAlleleName;
}

char *jaxPhenotypeName(struct track *tg, void *item)
/* Return name (or source) of jaxPhenotype item. */
{
struct linkedFeatures *lf = item;
/* todo: make this check a cart variable so it can be hgTrackUi-tweaked. */
return (char *)lf->extra;
}

void jaxPhenotypeMethods(struct track *tg)
/* Fancy name fetcher for jaxPhenotype. */
{
linkedFeaturesMethods(tg);
tg->loadItems = loadBed12Source;
tg->itemName = jaxPhenotypeName;
}


void getTransMapItemLabel(struct sqlConnection *defDbConn,
                          boolean useGeneName, boolean useAcc,
                          struct linkedFeatures *lf)
/* get label for a transMap item */
{
boolean labelStarted = FALSE;
struct dyString *label = dyStringNew(64);
char *org = NULL, acc[256], *dot;

/* remove version and qualifier */
safef(acc, sizeof(acc), "%s", lf->name);
dot = strchr(acc, '.');
if (dot != NULL)
    *dot = '\0';

org = getOrganismShort(defDbConn, acc);
if (org != NULL)
    dyStringPrintf(label, "%s ", org);

if (useGeneName)
    {
    char *gene = getGeneName(defDbConn, acc);
    if (gene != NULL)
        {
        dyStringAppend(label, gene);
        labelStarted = TRUE;
        }
    }
if (useAcc)
    {
    if (labelStarted)
        dyStringAppendC(label, '/');
    else
        labelStarted = TRUE;
    dyStringAppend(label, acc);
    }
lf->extra = dyStringCannibalize(&label);
}

void lookupTransMapLabels(struct track *tg)
/* This converts the transMap ids to labels. */
{
/* get organism prefix using defaultDb, since we might not have genbank,
 * or have xeno genbank.  However the db caching mechanism only handles
 * 2 databases, so just open a connection on first use and keep it open. */
struct sqlConnection *defDbConn = sqlConnect(hDefaultDb());

struct linkedFeatures *lf;
boolean useGeneName = FALSE;  /* FIXME: need to add track UI */
boolean useAcc =  TRUE;

for (lf = tg->items; lf != NULL; lf = lf->next)
    getTransMapItemLabel(defDbConn, useGeneName, useAcc, lf);
sqlDisconnect(&defDbConn);
}

void loadTransMap(struct track *tg)
/* Load up transMap gene predictions. */
{
enum trackVisibility vis = tg->visibility;
tg->items = lfFromGenePredInRange(tg, tg->mapName, chromName, winStart, winEnd);
if (vis != tvDense)
    {
    lookupTransMapLabels(tg);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
vis = limitVisibility(tg);
}

void transMapMethods(struct track *tg)
/* Make track of transMap gene predictions. */
{
tg->loadItems = loadTransMap;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
}

char *igtcName(struct track *tg, void *item)
/* Return name (stripping off the source suffix) of IGTC item. */
{
struct linkedFeatures *lf = (struct linkedFeatures *)item;
char *name = cloneString(lf->name);
char *ptr = strrchr(name, '_');
if (ptr != NULL)
    *ptr = '\0';
/* Some names contain spaces.  IGTC has cgi-encoded names to get them through 
 * BLAT and we keep that to get them through hgLoadPsl which splits on 
 * whitespace not tabs.  Decode for display: */
cgiDecode(name, name, strlen(name));
return name;
}

Color igtcColor(struct track *tg, void *item, struct vGfx *vg)
/* Color IGTC items by source. */
{
struct linkedFeatures *lf = (struct linkedFeatures *)item;
Color color = MG_BLACK;
char *source = strrchr(lf->name, '_');
if (source == NULL)
    return color;
source++;
/* reverse-alphabetical acronym rainbow: */
if (sameString(source, "BG"))
    color = vgFindColorIx(vg, 0x99, 0x00, 0xcc); /* purple */
else if (sameString(source, "CMHD"))
    color = vgFindColorIx(vg, 0x00, 0x00, 0xcc); /* dark blue */
else if (sameString(source, "EGTC"))
    color = vgFindColorIx(vg, 0x66, 0x99, 0xff); /* light blue */
else if (sameString(source, "ESDB"))
    color = vgFindColorIx(vg, 0x00, 0xcc, 0x00); /* green */
else if (sameString(source, "FHCRC"))
    color = vgFindColorIx(vg, 0xcc, 0x99, 0x00); /* dark yellow */
else if (sameString(source, "GGTC"))
    color = vgFindColorIx(vg, 0xff, 0x99, 0x00); /* orange */
else if (sameString(source, "SIGTR"))
    color = vgFindColorIx(vg, 0x99, 0x66, 0x00); /* brown */
else if (sameString(source, "TIGEM"))
    color = vgFindColorIx(vg, 0xcc, 0x00, 0x00); /* red */
return color;
}

void igtcMethods(struct track *tg)
/* International Gene Trap Consortium: special naming & coloring. */
{
tg->itemName = igtcName;
tg->itemColor = igtcColor;
tg->itemNameColor = igtcColor;
}

void logoLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
{
}

/* load data for a sequence logo */
static void logoLoad(struct track *tg)
{
struct dnaMotif *motif; 
int count = winEnd - winStart;
int ii;
char query[256];
struct sqlResult *sr;
long long offset = 0;
char *fileName = NULL;
struct sqlConnection *conn;
char **row;
FILE *f;
unsigned short *mem, *p;
boolean complementBases = cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE);

if (!zoomedToBaseLevel)
	return;

conn = hAllocConn();
safef(query, sizeof(query), 
	"select offset,fileName from %s where chrom = '%s'", tg->mapName,chromName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    offset = sqlLongLong(row[0]);
    fileName = cloneString(row[1]);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);

if (offset == 0)
    return; /* we should have found a non-zero offset  */

AllocVar(motif);
motif->name = NULL;
motif->columnCount = count;
motif->aProb = needMem(sizeof(float) * motif->columnCount);
motif->cProb = needMem(sizeof(float) * motif->columnCount);
motif->gProb = needMem(sizeof(float) * motif->columnCount);
motif->tProb = needMem(sizeof(float) * motif->columnCount);

/* get data from data file specified in db */
f = mustOpen(fileName, "r");
offset += winStart * 2; /* file has 2 bytes per base */
fseek(f, offset, 0);

p = mem = needMem(count * 2);

/* read in probability data from file */
fread(mem, sizeof(unsigned short), count, f);
fclose(f);

/* translate 5 bits for A,C,and G into real numbers for all bases */
for(ii=0; ii < motif->columnCount; ii++, p++)
    {
    motif->gProb[ii] = *p & 0x1f;
    motif->cProb[ii] = (*p >> 5)  & 0x1f;
    motif->aProb[ii] = (*p >> 10) & 0x1f;
    motif->tProb[ii] = 31 - motif->aProb[ii] - motif->cProb[ii] - motif->gProb[ii];
    }

if (complementBases) /* if bases are on '-' strand */
    {
    float *temp = motif->aProb;

    motif->aProb = motif->tProb;
    motif->tProb = temp;

    temp = motif->cProb;
    motif->cProb = motif->gProb;
    motif->gProb = temp;
    }

tg->items = motif;
}


int logoHeight(struct track *tg, enum trackVisibility vis)
/* set up size of sequence logo */
{
if (tg->items == NULL)
    tg->height = tg->lineHeight;
else
    tg->height = 52;

return tg->height;
}

void logoMethods(struct track *track, struct trackDb *tdb, 
	int argc, char *argv[])
/* Load up logo type methods. */
{
track->loadItems = logoLoad;
track->drawLeftLabels = logoLeftLabels;

track->drawItems = logoDrawSimple;
track->totalHeight = logoHeight; 
track->mapsSelf = TRUE;
}

void fillInFromType(struct track *track, struct trackDb *tdb)
/* Fill in various function pointers in track from type field of tdb. */
{
char *typeLine = tdb->type, *words[8], *type;
int wordCount;
if (typeLine == NULL)
    return;
wordCount = chopLine(cloneString(typeLine), words);
if (wordCount <= 0)
    return;
type = words[0];

if (sameWord(type, "bed"))
    {
    int fieldCount = 3;
    boolean useItemRgb = FALSE;

    useItemRgb = bedItemRgb(tdb);

    if (wordCount > 1)
        fieldCount = atoi(words[1]);
    track->bedSize = fieldCount;

    if (fieldCount < 8)
	{
	bedMethods(track);
	track->loadItems = loadSimpleBed;
	}
    else if (useItemRgb && fieldCount == 9)
	{
	linkedFeaturesMethods(track);
	track->loadItems = loadBed9;
	}
    else if (fieldCount < 12)
	{
	linkedFeaturesMethods(track);
	track->loadItems = loadBed8;
	}
    else 
	{
	linkedFeaturesMethods(track);
	track->extraUiData = newBedUiData(track->mapName);
	track->loadItems = loadGappedBed;
	}
    }
else if (sameWord(type, "bedGraph"))
    {
    bedGraphMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "wig"))
    {
    wigMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "wigMaf"))
    {
    wigMafMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "wigMafProt"))
    {
    wigMafMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "sample"))
    {
    sampleMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "genePred"))
    {
    linkedFeaturesMethods(track);
    track->loadItems = loadGenePred;
    track->colorShades = NULL;
    if (track->itemAttrTbl != NULL)
        track->itemColor = genePredItemAttrColor;
    else if (trackDbSetting(track->tdb, GENEPRED_CLASS_TBL) !=NULL)
        track->itemColor = genePredItemClassColor;
    }
else if (sameWord(type, "logo"))
    {
    logoMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "psl"))
    {
    pslMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "chain"))
    {
    chainMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "netAlign"))
    {
    netMethods(track);
    }
else if (sameWord(type, "maf"))
    {
    mafMethods(track);
    }
else if (sameWord(type, "coloredExon"))
    {
    coloredExonMethods(track);
    }
else if (sameWord(type, "axt"))
    {
    if (wordCount < 2)
        errAbort("Expecting 2 words in axt track type for %s", tdb->tableName);
    axtMethods(track, words[1]);
    }
else if (sameWord(type, "expRatio"))
    {
    expRatioMethodsFromDotRa(track);
    }
else if (sameWord(type, "bed5FloatScore"))
    {
    track->bedSize = 5;
    bedMethods(track);
    track->loadItems = loadSimpleBed;
    }
else if (sameWord(type, "bed6FloatScore"))
    {
    track->bedSize = 4;
    bedMethods(track);
    track->loadItems = loadSimpleBed;
    }
else if (sameWord(type, "chromGraph"))
    {
    chromGraphMethods(track);
    }
else if (sameWord(type, "altGraphX"))
    {
    altGraphXMethods(track);
    }
}

static void compositeLoad(struct track *track)
/* Load all subtracks */
{
struct track *subtrack;
long thisTime = 0, lastTime = 0;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	lastTime = clock1000();
        subtrack->loadItems(subtrack);
	if (measureTiming)
	    {
	    thisTime = clock1000();
	    subtrack->loadTime = thisTime - lastTime;
	    lastTime = thisTime;
	    }
	}
    else
	{
	subtrack->limitedVis = tvHide;
	subtrack->limitedVisSet = TRUE;
	}
    }
}

static int compositeTotalHeight(struct track *track, enum trackVisibility vis)
/* Return total height of composite track and set subtrack->height's. */
{
struct track *subtrack;
int height = 0;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    if (isSubtrackVisible(subtrack))
	{
	int h = subtrack->totalHeight(subtrack, vis);
	subtrack->height = h;
	height += h;
	}
track->height = height;
return height;
}

int trackPriCmp(const void *va, const void *vb)
/* Compare for sort based on priority */
{
const struct track *a = *((struct track **)va);
const struct track *b = *((struct track **)vb);

return (a->priority - b->priority);
}

static void makeCompositeTrack(struct track *track, struct trackDb *tdb)
/* Construct track subtrack list from trackDb entry.
 * Sets up color gradient in subtracks if requested */
{
unsigned char finalR = track->color.r, finalG = track->color.g, 
                            finalB = track->color.b;
unsigned char altR = track->altColor.r, altG = track->altColor.g, 
                            altB = track->altColor.b;
unsigned char deltaR = 0, deltaG = 0, deltaB = 0;
struct trackDb *subTdb;
/* number of possible subtracks for this track */
int subtrackCt = slCount(tdb->subtracks);
int altColors = subtrackCt - 1;
struct track *subtrack = NULL;
TrackHandler handler;
char table[64];
struct hashEl *hel, *hels = NULL;
int len;
boolean smart = FALSE;

/* ignore if no subtracks */
if (!subtrackCt)
    return;

/* look out for tracks that manage their own subtracks */
if (startsWith("wig", tdb->type) || startsWith("bedGraph", tdb->type) ||
    rStringIn("smart", trackDbSetting(tdb, "compositeTrack")))
        smart = TRUE;

/* setup function handlers for composite track */
handler = lookupTrackHandler(tdb->tableName);
if (smart && handler != NULL)
    /* handles it's own load and height */
    handler(track);
else
    {
    track->loadItems = compositeLoad;
    track->totalHeight = compositeTotalHeight;
    }

if (altColors && (finalR || finalG || finalB))
    {
    /* not black -- make a color gradient for the subtracks,
                from black, to the specified color */
    deltaR = (finalR - altR) / altColors;
    deltaG = (finalG - altG) / altColors;
    deltaB = (finalB - altB) / altColors;
    }
/* get cart variables for the composite, unless track handles it's own */
if (!smart)
    {
    safef(table, sizeof table, "%s.", track->mapName);
    hels = cartFindPrefix(cart, table);
    }

/* fill in subtracks of composite track */
for (subTdb = tdb->subtracks; subTdb != NULL; subTdb = subTdb->next)
{
    /* initialize from composite track settings */
    if (trackDbSetting(subTdb, "noInherit") == NULL)
	{
	/* install parent's track handler */
	subtrack = trackFromTrackDb(tdb);
	handler = lookupTrackHandler(tdb->tableName);
	}
    else 
	{
	subtrack = trackFromTrackDb(subTdb);
	handler = lookupTrackHandler(subTdb->tableName);
	}
    if (handler != NULL)
	handler(subtrack);

    if (trackDbSetting(subTdb, "noInherit") == NULL)
	{
        if (!smart)
            {
            /* add cart variables from parent */
            char cartVar[128];
            for (hel = hels; hel != NULL; hel = hel->next)
                {
                len = strlen(track->mapName);
                safef(cartVar, sizeof cartVar, "%s.%s",
                                   subTdb->tableName, hel->name+len+1);
                cartSetString(cart, cartVar, hel->val);
                }
            }
	/* add subtrack settings (table, colors, labels, vis & pri) */
	subtrack->mapName = subTdb->tableName;
	subtrack->shortLabel = subTdb->shortLabel;
	subtrack->longLabel = subTdb->longLabel;
	if (finalR || finalG || finalB)
	    {
	    subtrack->color.r = altR;
	    subtrack->altColor.r = (255+altR)/2;
	    altR += deltaR;
	    subtrack->color.g = altG;
	    subtrack->altColor.g = (255+altG)/2;
	    altG += deltaG;
	    subtrack->color.b = altB;
	    subtrack->altColor.b = (255+altB)/2;
	    altB += deltaB;
	    }
	else
	    {
	    subtrack->color.r = subTdb->colorR;
	    subtrack->color.g = subTdb->colorG;
	    subtrack->color.b = subTdb->colorB;
	    subtrack->altColor.r = subTdb->altColorR;
	    subtrack->altColor.g = subTdb->altColorG;
	    subtrack->altColor.b = subTdb->altColorB;
	    }
	subtrack->priority = subTdb->priority;
	}

    slAddHead(&track->subtracks, subtrack);
    }
slSort(&track->subtracks, trackPriCmp);

}

struct track *trackFromTrackDb(struct trackDb *tdb)
/* Create a track based on the tdb. */
{
struct track *track = trackNew();
char *iatName = NULL;
char *exonArrows;
char *nextItem;

track->mapName = cloneString(tdb->tableName);
track->visibility = tdb->visibility;
track->shortLabel = tdb->shortLabel;
track->longLabel = tdb->longLabel;
track->color.r = tdb->colorR;
track->color.g = tdb->colorG;
track->color.b = tdb->colorB;
track->altColor.r = tdb->altColorR;
track->altColor.g = tdb->altColorG;
track->altColor.b = tdb->altColorB;
track->lineHeight = tl.fontHeight+1;
track->heightPer = track->lineHeight - 1;
track->private = tdb->private;
track->priority = tdb->priority;
track->defaultPriority = tdb->priority;
track->groupName = tdb->grp;
track->canPack = tdb->canPack;
if (tdb->useScore)
    {
    /* Todo: expand spectrum opportunities. */
    if (colorsSame(&brownColor, &track->color))
        track->colorShades = shadesOfBrown;
    else if (colorsSame(&darkSeaColor, &track->color))
        track->colorShades = shadesOfSea;
    else
	track->colorShades = shadesOfGray;
    }
track->tdb = tdb;

exonArrows = trackDbSetting(tdb, "exonArrows");
nextItem = trackDbSetting(tdb, "nextItemButton");
/* default exonArrows to on, except for tracks in regulation/expression group */
if (exonArrows == NULL)
    {
    if (sameString(tdb->grp, "regulation"))
       exonArrows = "off";
    else
       exonArrows = "on";
    }
track->exonArrows = sameString(exonArrows, "on");
track->nextItemButtonable = TRUE;
if (nextItem && sameString(nextItem, "off"))
    track->nextItemButtonable = FALSE;
track->labelNextItemButtonable = track->nextItemButtonable;
iatName = trackDbSetting(tdb, "itemAttrTbl");
if (iatName != NULL)
    track->itemAttrTbl = itemAttrTblNew(iatName);
fillInFromType(track, tdb);
return track;
}


void loadFromTrackDb(struct track **pTrackList)
/* Load tracks from database, consulting handler list. */
{
struct trackDb *tdb, *tdbList = NULL;
struct track *track;
TrackHandler handler;

tdbList = hTrackDb(chromName);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    track = trackFromTrackDb(tdb);
    track->hasUi = TRUE;
    /* save default priority and group so we can reset it later */
    track->defaultPriority = track->priority;
    track->defaultGroupName = track->groupName;

    if (slCount(tdb->subtracks) != 0)
        makeCompositeTrack(track, tdb);
    else
        {
        handler = lookupTrackHandler(tdb->tableName);
        if (handler != NULL)
            handler(track);
        }
    if (track->loadItems == NULL)
        warn("No load handler for %s; possible missing trackDb `type' or `subTrack' attribute", tdb->tableName);
    else if (track->drawItems == NULL)
        warn("No draw handler for %s", tdb->tableName);
    else
        slAddHead(pTrackList, track);
    }
}

static int getScoreFilter(char *tableName)
/* check for score filter configuration setting */
{
char optionScoreStr[128];

safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", tableName);
return cartUsualInt(cart, optionScoreStr, 0);
}

void ctLoadSimpleBed(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct customTrack *ct = tg->customPt;
struct bed *bed, *nextBed, *list = NULL;
int scoreFilter = getScoreFilter(ct->tdb->tableName);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
    struct sqlResult *sr = NULL;

    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
		     NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	slAddHead(&list, bed);
	}
    hFreeOrDisconnect(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = nextBed)
	{
	nextBed = bed->next;
	if (bed->chromStart < winEnd && bed->chromEnd > winStart 
		    && sameString(chromName, bed->chrom))
	    {
            if (scoreFilter && bed->score < scoreFilter)
                continue;
	    slAddHead(&list, bed);
	    }
	}
    }
slSort(&list, bedCmp);
tg->items = list;
}

void ctLoadBed9(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
boolean useItemRgb = FALSE;
int scoreFilter = getScoreFilter(ct->tdb->tableName);

useItemRgb = bedItemRgb(ct->tdb);

if (ct->dbTrack)
    {
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
    struct sqlResult *sr = NULL;

    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
		     NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, 9);
	bed8To12(bed);
	lf = lfFromBed(bed);
	if (useItemRgb)
	    {
	    lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
	    lf->filterColor=bed->itemRgb;
	    }
	slAddHead(&lfList, lf);
	}
    hFreeOrDisconnect(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
        if (scoreFilter && bed->score < scoreFilter)
            continue;
	if (bed->chromStart < winEnd && bed->chromEnd > winStart 
		    && sameString(chromName, bed->chrom))
	    {
	    bed8To12(bed);
	    lf = lfFromBed(bed);
	    if (useItemRgb)
		{
		lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
		lf->filterColor=bed->itemRgb;
		}
	    slAddHead(&lfList, lf);
	    }
	}
    }
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}


void ctLoadBed8(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
int scoreFilter = getScoreFilter(ct->tdb->tableName);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
    struct sqlResult *sr = NULL;

    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
		     NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	bed8To12(bed);
	lf = lfFromBed(bed);
	slAddHead(&lfList, lf);
	}
    hFreeOrDisconnect(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
        if (scoreFilter && bed->score < scoreFilter)
            continue;
	if (bed->chromStart < winEnd && bed->chromEnd > winStart 
		    && sameString(chromName, bed->chrom))
	    {
	    bed8To12(bed);
	    lf = lfFromBed(bed);
	    slAddHead(&lfList, lf);
	    }
	}
    }
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

void ctLoadGappedBed(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
boolean useItemRgb = FALSE;
int scoreFilter = getScoreFilter(ct->tdb->tableName);

useItemRgb = bedItemRgb(ct->tdb);

if (ct->dbTrack)
    {
    int fieldCount = ct->fieldCount;
    int rowOffset;
    char **row;
    struct sqlConnection *conn = sqlCtConn(TRUE);
    struct sqlResult *sr = NULL;

    sr = hRangeQuery(conn, ct->dbTableName, chromName, winStart, winEnd,
		     NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, fieldCount);
	lf = lfFromBed(bed);
	if (useItemRgb)
	    {
	    lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
	    lf->filterColor=bed->itemRgb;
	    }
	slAddHead(&lfList, lf);
	}
    hFreeOrDisconnect(&conn);
    }
else
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
	{
        if (scoreFilter && bed->score < scoreFilter)
            continue;
	if (bed->chromStart < winEnd && bed->chromEnd > winStart 
		    && sameString(chromName, bed->chrom))
	    {
	    lf = lfFromBed(bed);
	    if (useItemRgb)
		{
		lf->extra = (void *)USE_ITEM_RGB; /* signal for coloring */
		lf->filterColor=bed->itemRgb;
		}
	    slAddHead(&lfList, lf);
	    }
	}
    }
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

char *ctMapItemName(struct track *tg, void *item)
/* Return composite item name for custom tracks. */
{
  char *itemName = tg->itemName(tg, item);
  static char buf[256];
  if (strlen(itemName) > 0)
      sprintf(buf, "%s %s", ctFileName, itemName);
  else
      sprintf(buf, "%s NoItemName", ctFileName);
  return buf;
}


struct track *newCustomTrack(struct customTrack *ct)
/* Make up a new custom track. */
{
struct track *tg = NULL;
struct trackDb *tdb = ct->tdb;
boolean useItemRgb = FALSE;
char *typeOrig = tdb->type;
char *typeDupe = cloneString(typeOrig);
char *typeParam = typeDupe;
char *type = nextWord(&typeParam);

if (ct->dbTrack)
    {
    struct sqlConnection *conn = sqlCtConn(FALSE);
    if ((struct sqlConnection *)NULL == conn)
	errAbort("can not connect to customTracks DB");
    else
	hFreeOrDisconnect(&conn);
    }

useItemRgb = bedItemRgb(tdb);

if (sameString(type, "wig"))
    {
    tg = trackFromTrackDb(tdb);
    if (ct->dbTrack)
	tg->loadItems = wigLoadItems;
    else
	tg->loadItems = ctWigLoadItems;
    tg->customPt = ct;
    }
else if (sameString(type, "bed"))
    {
    tg = trackFromTrackDb(tdb);
    if (ct->fieldCount < 8)
	{
	tg->loadItems = ctLoadSimpleBed;
	}
    else if (useItemRgb && ct->fieldCount == 9)
	{
	tg->loadItems = ctLoadBed9;
	}
    else if (ct->fieldCount < 12)
	{
	tg->loadItems = ctLoadBed8;
	}
    else if (ct->fieldCount == 15)
	{
	char *theType = trackDbSetting(tdb, "type");
	if (theType && sameString(theType, "expRatio"))
	    {
	    tg = trackFromTrackDb(tdb);
	    expRatioMethodsFromCt(tg);	    
	    }
	else
	    tg->loadItems = ctLoadGappedBed;
	}
    else 
	{
	tg->loadItems = ctLoadGappedBed;
	}
    tg->mapItemName = ctMapItemName;
    tg->canPack = TRUE;
    tg->customPt = ct;
    }
else if (sameString(type, "chromGraph"))
    {
    tdb->type = NULL;	/* Swap out type for the moment. */
    tg = trackFromTrackDb(tdb);
    chromGraphMethodsCt(tg);
    tdb->type = typeOrig;
    }
else if (sameString(type, "array"))
    {
    tg = trackFromTrackDb(tdb);
    expRatioMethodsFromCt(tg);
    tg->customPt = ct;
    }
else if (sameString(type, "coloredExon"))
    {
    tg = trackFromTrackDb(tdb);
    coloredExonMethodsFromCt(tg);
    tg->customPt = ct;
    }
else
    {
    errAbort("Unrecognized custom graph type %s", type);
    }
tg->labelNextItemButtonable = FALSE;
tg->hasUi = TRUE;
freez(&typeDupe);
return tg;
}

char *getPositionFromCustomTracks()
/* Parses custom track data to get the position variable
 * return - The first chromosome position variable found in the 
 * custom track data.  */
{
char *pos = NULL;
struct slName *bl = NULL;

ctList = customTracksParseCart(cart, &browserLines, &ctFileName);

for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    char *words[96];
    int wordCount;
    char *dupe = cloneString(bl->name);

    wordCount = chopLine(dupe, words);
    if (wordCount >= 3)
        {
        char *command = words[1];
        if (sameString(command, "position"))
            pos = cloneString(words[2]);
        }
    freez(&dupe);
    if (pos != NULL)
        break;
    }
return pos;
}

void loadCustomTracks(struct track **pGroupList)
/* Load up custom tracks and append to list. */
{
struct customTrack *ct;
struct track *tg;
struct slName *bl;

/* build up browser lines from cart variables set by hgCustom */
char *visAll = cartCgiUsualString(cart, "hgt.visAllFromCt", NULL);
if (visAll)
    {
    char buf[64];
    safef(buf, sizeof buf, "browser %s %s", visAll, "all");
    slAddTail(&browserLines, slNameNew(buf));
    }
struct hashEl *visEl;
struct hashEl *visList = cartFindPrefix(cart, "hgtct.");
for (visEl = visList; visEl != NULL; visEl = visEl->next)
    {
    char buf[128];
    safef(buf, sizeof buf, "browser %s %s", cartString(cart, visEl->name), 
                chopPrefix(cloneString(visEl->name)));
    slAddTail(&browserLines, slNameNew(buf));
    cartRemove(cart, visEl->name);
    }
hashElFreeList(&visList);

/* The loading is now handled by getPositionFromCustomTracks(). */
/* Process browser commands in custom track. */
for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    char *words[96];
    int wordCount;

    wordCount = chopLine(bl->name, words);
    if (wordCount > 1)
        {
	char *command = words[1];
	if (sameString(command, "hide") 
            || sameString(command, "dense") 
            || sameString(command, "pack") 
            || sameString(command, "squish") 
            || sameString(command, "full"))
	    {
	    if (wordCount > 2)
	        {
		int i;
		for (i=2; i<wordCount; ++i)
		    {
		    char *s = words[i];
		    struct track *tg;
		    boolean toAll = sameWord(s, "all");
		    for (tg = *pGroupList; tg != NULL; tg = tg->next)
		        {
			if (toAll || sameString(s, tg->mapName))
                            {
                            if (hTvFromString(command) == tg->tdb->visibility)
                                /* remove if setting to default vis */
                                cartRemove(cart, tg->mapName);
                            else
                                cartSetString(cart, tg->mapName, command);
                            }
			}
		    }
		}
	    }
	else if (sameString(command, "position"))
	    {
	    if (wordCount < 3)
	        errAbort("Expecting 3 words in browser position line");
	    if (!hgIsChromRange(words[2])) 
	        errAbort("browser position needs to be in chrN:123-456 format");
	    hgParseChromRange(words[2], &chromName, &winStart, &winEnd);
            /*Fix a start window of -1 that is returned when a custom track position
              begins at 0
            */
            if (winStart < 0) 
                {
                winStart = 0;
                }
	    }
	else if (sameString(command, "pix"))
	    {
	    if (wordCount != 3)
	        errAbort("Expecting 3 words in pix line");
	    trackLayoutSetPicWidth(&tl, words[2]);
	    }
	}
    }
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    hasCustomTracks = TRUE;
    tg = newCustomTrack(ct);
    slAddHead(pGroupList, tg);
    }
}

boolean restrictionEnzymesOk()
/* Check to see if it's OK to do restriction enzymes. */
{
return (hTableExistsDb("hgFixed", "cutters") &&
	hTableExistsDb("hgFixed", "rebaseRefs") &&
	hTableExistsDb("hgFixed", "rebaseCompanies"));
}

void hotLinks()
/* Put up the hot links bar. */
{
boolean gotBlat = hIsBlatIndexedDatabase(database);
struct dyString *uiVars = uiStateUrlPart(NULL);
char *orgEnc = cgiEncode(organism);

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#536ED3\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=CENTER><A HREF=\"/index.html?org=%s\" class=\"topbar\">Home</A></TD>", orgEnc);

hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s\" class=\"topbar\">Genomes</A></TD>", orgEnc, database);
if (hIsGsidServer())
    {
    hPrintf(
    "<TD ALIGN=CENTER><A HREF=\"../cgi-bin/gsidTable?gsidTable.do.advFilter=filter+%c28now+on%c29&fromProg=hgTracks\" class=\"topbar\">%s</A></TD>",
    '%', '%', "Select Subjects");
    } ;
if (gotBlat)
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgBlat?%s\" class=\"topbar\">Blat</A></TD>", uiVars->string);
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgTables?db=%s&position=%s:%d-%d&%s=%u\" class=\"topbar\">%s</A></TD>",
       database, chromName, winStart+1, winEnd, cartSessionVarName(),
       cartSessionId(cart), "Tables");
if (hgNearOk(database))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgNear?%s\" class=\"topbar\">%s</A></TD>",
                 uiVars->string, "Gene Sorter");
    }
if (hgPcrOk(database))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgPcr?%s\" class=\"topbar\">PCR</A></TD>", uiVars->string);
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&db=%s&%s\" class=\"topbar\">"
      " %s </A></TD>",  hgcNameAndSettings(),
      winStart, chromName, winStart, winEnd, database, uiVars->string, "DNA");

if (liftOverChainForDb(database) != NULL)
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"");
    hPrintf("../cgi-bin/hgConvert?%s&db=%s&position=%s:%d-%d", 
    	uiVars->string, database, chromName, winStart+1, winEnd);
    hPrintf("\" class=\"topbar\">Convert</A></TD>");
    }

/* Print Ensembl anchor for latest assembly of organisms we have
 * supported by Ensembl (human, mouse, rat, fugu) */
if (sameString(database, "hg18")
            || sameString(database, "mm8")
            || sameString(database, "rn4") 
            || sameString(database, "monDom4") 
            || sameString(database, "xenTro2") 
            || sameString(database, "danRer4") 
            || sameString(database, "canFam1") 
            || sameString(database, "dm2") 
            || sameString(database, "galGal2")
            || sameString(database, "panTro2")
            || sameString(database, "tetNig1"))
    {
    hPuts("<TD ALIGN=CENTER>");
    printEnsemblAnchor(database);
    hPrintf("%s</A></TD>", "Ensembl");
    }

/* Print NCBI MapView anchor */
if (sameString(database, "hg18"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "mm8"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=10090&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "danRer2"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7955&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "galGal3"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9031&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "canFam2"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9615&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "rheMac2"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9544&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "panTro2"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9598&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "anoGam1"))
    {
    hPrintf("<TD ALIGN=CENTER>");
    hPrintf("<A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7165&CHR=%s&BEG=%d&END=%d\" TARGET=_blank class=\"topbar\">",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", "NCBI");
    }
if (sameString(database, "oryLat1"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://medaka.utgenome.org/browser_ens_jump.php?revision=version1.0&chr=chromosome%s&start=%d&end=%d\" TARGET=_blank class=\"topbar\">%s</A></TD>",
        skipChr(chromName), winStart+1, winEnd, "UTGB");
    }
if (sameString(database, "ce2"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://ws120.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d\" TARGET=_blank class=\"topbar\">%s</A></TD>", 
        skipChr(chromName), winStart+1, winEnd, "WormBase");
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgTracks?%s=%u&hgt.psOutput=on\" class=\"topbar\">%s</A></TD>\n",cartSessionVarName(),
       cartSessionId(cart), "PDF/PS");
if (wikiLinkEnabled())
    {
    printf("<TD ALIGN=CENTER><A HREF=\"/cgi-bin/hgSession?%s=%u"
	   "&hgS_doMainPage=1\" class=\"topbar\">Session</A></TD>",
	   cartSessionVarName(), cartSessionId(cart));
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"../goldenPath/help/hgTracksHelp.html\" TARGET=_blank class=\"topbar\">%s</A></TD>\n", "Help");
hPuts("</TR></TABLE>");
hPuts("</TD></TR></TABLE>\n");
}

static void groupTracks(struct track **pTrackList, struct group **pGroupList)
/* Make up groups and assign tracks to groups. */
{
struct group *unknown = NULL;
struct group *group, *list = NULL;
struct hash *hash = newHash(8);
struct track *track;
struct trackRef *tr;
struct grp* grps = hLoadGrps();
struct grp *grp;
char cartVar[512];

/* build group objects from database. */
for (grp = grps; grp != NULL; grp = grp->next)
    {
    AllocVar(group);
    slAddHead(&list, group);
    hashAdd(hash, grp->name, group);
    group->name = cloneString(grp->name);
    group->label = cloneString(grp->label);
    if (withPriorityOverride)
        {
        float priority = 0;
        safef(cartVar, sizeof(cartVar), "%s.priority",group->name);
        priority = (float)cartUsualDouble(cart, cartVar, grp->priority);
        if (abs(priority - group->priority) < 0.00001)
            cartRemove(cart, cartVar);
        else
            {
            group->defaultPriority = grp->priority;
            group->priority = priority;
            }
        }
    else
        group->priority = grp->priority;
    }
grpFreeList(&grps);

/* Loop through tracks and fill in their groups. 
 * If necessary make up an unknown group. */
for (track = *pTrackList; track != NULL; track = track->next)
    {
    if (track->groupName == NULL)
        group = NULL;
    else
	{
	group = hashFindVal(hash, track->groupName);
	}
    if (group == NULL)
        {
	if (unknown == NULL)
	    {
	    AllocVar(unknown);
	    unknown->name = cloneString("other");
	    unknown->label = cloneString("other");
	    unknown->priority = 1000000;
	    slAddHead(&list, unknown);
	    }
	group = unknown;
	}
    track->group = group;
    }
slReverse(&list);  /* Postpone this til here so unknown will be last. */

/* Read priority from cart, if user has overriden priority from trackDb */
if (withPriorityOverride)
    for (track = *pTrackList; track != NULL; track = track->next)
        {
        int priority = 0;
        char *groupName = NULL;
        safef(cartVar, sizeof(cartVar), "%s.priority",track->mapName);
        priority = (float)cartUsualDouble(cart, cartVar, track->priority);
        /* remove cart variables that are the same as the trackDb settings */
        if (abs(priority - track->priority) < 0.00001)
            cartRemove(cart, cartVar);
        else
            track->defaultPriority = track->priority;
        track->priority = (float)cartUsualDouble(cart, cartVar, track->priority);
        safef(cartVar, sizeof(cartVar), "%s.group",track->mapName);
        groupName = cartUsualString(cart, cartVar, track->group->name);
        if (sameString(groupName, track->group->name))
            cartRemove(cart, cartVar);
        if (hashFindVal(hash, groupName)!=NULL)
            {
            track->groupName = cloneString(groupName);
            track->group = hashFindVal(hash, groupName);
            }
        }
/* Sort tracks by combined group/track priority, and
 * then add references to track to group. */
slSort(pTrackList, tgCmpPriority);
for (track = *pTrackList; track != NULL; track = track->next)
    {
    AllocVar(tr);
    tr->track = track;
    slAddHead(&track->group->trackList, tr);
    }

/* Straighten things out, clean up, and go home. */
for (group = list; group != NULL; group = group->next)
    slReverse(&group->trackList);
slSort(&list, gCmpPriority);
hashFree(&hash);
*pGroupList = list;
}

void topButton(char *var, char *label)
/* create a 3 or 4-char wide button for top line of display.
 * 3 chars wide for odd-length labels, 4 for even length.
 * Pad with spaces so label is centered */
{
char paddedLabel[5] = "    ";
int len = strlen(label);
if (len > 4)
    {
    /* truncate */
    /* or maybe errabort ? */
    label[3] = 0;
    len = 4;
    }
if (len % 2 != 0)
    paddedLabel[3] = 0;
if (len == strlen(paddedLabel))
    strcpy(paddedLabel, label);
else
    {
    int i;
    for (i=0; i<len; i++)
        paddedLabel[i+1] = label[i];
    }
hButton(var, paddedLabel);
}

void compositeTrackVis(struct track *track)
/* set visibilities of subtracks */
{
int subtrackCt = 0;
struct track *subtrack;

if (track->visibility == tvHide)
    return;

/* count number of visible subtracks for this track */
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    subtrackVisible(subtrack);
    /* visibilities have not been set yet, so don't use isSubtrackVisible 
     * which gates with visibility: */
    if (isSubtrackSelected(subtrack))
        subtrackCt++;
    }
/* if no subtracks are selected in cart, turn them all on */
if (subtrackCt == 0)
    for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
        setSubtrackVisible(subtrack->mapName, TRUE);
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    if(!subtrack->limitedVisSet)
        subtrack->visibility = track->visibility;
}

struct track *getTrackList( struct group **pGroupList)
/* Return list of all tracks. */
{
struct track *track, *trackList = NULL;
/* Register tracks that include some non-standard methods. */
registerTrackHandler("rgdGene", rgdGeneMethods);
registerTrackHandler("cgapSage", cgapSageMethods);
registerTrackHandler("cytoBand", cytoBandMethods);
registerTrackHandler("cytoBandIdeo", cytoBandIdeoMethods);
registerTrackHandler("bacEndPairs", bacEndPairsMethods);
registerTrackHandler("bacEndPairsBad", bacEndPairsBadMethods);
registerTrackHandler("bacEndPairsLong", bacEndPairsLongMethods);
registerTrackHandler("bacEndSingles", bacEndSinglesMethods);
registerTrackHandler("fosEndPairs", fosEndPairsMethods);
registerTrackHandler("fosEndPairsBad", fosEndPairsBadMethods);
registerTrackHandler("fosEndPairsLong", fosEndPairsLongMethods);
registerTrackHandler("earlyRep", earlyRepMethods);
registerTrackHandler("earlyRepBad", earlyRepBadMethods);
registerTrackHandler("genMapDb", genMapDbMethods);
registerTrackHandler("cgh", cghMethods);
registerTrackHandler("mcnBreakpoints", mcnBreakpointsMethods);
registerTrackHandler("fishClones", fishClonesMethods);
registerTrackHandler("mapGenethon", genethonMethods);
registerTrackHandler("stsMarker", stsMarkerMethods);
registerTrackHandler("stsMap", stsMapMethods);
registerTrackHandler("stsMapMouseNew", stsMapMouseMethods);
registerTrackHandler("stsMapRat", stsMapRatMethods);
registerTrackHandler("snpMap", snpMapMethods);
registerTrackHandler("snp", snpMethods);
registerTrackHandler("snp125", snp125Methods);
registerTrackHandler("snp126", snp125Methods);
registerTrackHandler("snp127", snp125Methods);
registerTrackHandler("ld", ldMethods);
registerTrackHandler("cnpSharp", cnpSharpMethods);
registerTrackHandler("cnpSharp2", cnpSharp2Methods);
registerTrackHandler("cnpIafrate", cnpIafrateMethods);
registerTrackHandler("cnpIafrate2", cnpIafrate2Methods);
registerTrackHandler("cnpSebat", cnpSebatMethods);
registerTrackHandler("cnpSebat2", cnpSebat2Methods);
registerTrackHandler("cnpFosmid", cnpFosmidMethods);
registerTrackHandler("cnpRedon", cnpRedonMethods);
registerTrackHandler("cnpLocke", cnpLockeMethods);
registerTrackHandler("cnpTuzun", cnpTuzunMethods);
registerTrackHandler("delConrad", delConradMethods);
registerTrackHandler("delConrad2", delConrad2Methods);
registerTrackHandler("delMccarroll", delMccarrollMethods);
registerTrackHandler("delHinds", delHindsMethods);
registerTrackHandler("delHinds2", delHindsMethods);
registerTrackHandler("hapmapLd", ldMethods);
registerTrackHandler("rertyHumanDiversityLd", ldMethods);
registerTrackHandler("rnaLpFold", rnaLpFoldMethods);
registerTrackHandler("recombRate", recombRateMethods);
registerTrackHandler("recombRateMouse", recombRateMouseMethods);
registerTrackHandler("recombRateRat", recombRateRatMethods);
registerTrackHandler("chr18deletions", chr18deletionsMethods);
registerTrackHandler("mouseSyn", mouseSynMethods);
registerTrackHandler("mouseSynWhd", mouseSynWhdMethods);
registerTrackHandler("ensRatMusHom", ensPhusionBlastMethods);
registerTrackHandler("ensRatMm4Hom", ensPhusionBlastMethods);
registerTrackHandler("ensRatMm5Hom", ensPhusionBlastMethods);
registerTrackHandler("ensRatMusHg17", ensPhusionBlastMethods);
registerTrackHandler("ensRn3MusHom", ensPhusionBlastMethods);
registerTrackHandler("syntenyMm4", syntenyMethods);
registerTrackHandler("syntenyMm3", syntenyMethods);
registerTrackHandler("syntenyRn3", syntenyMethods);
registerTrackHandler("syntenyHg15", syntenyMethods);
registerTrackHandler("syntenyHg16", syntenyMethods);
registerTrackHandler("syntenyHuman", syntenyMethods);
registerTrackHandler("syntenyMouse", syntenyMethods);
registerTrackHandler("syntenyRat", syntenyMethods);
registerTrackHandler("syntenyCow", syntenyMethods);
registerTrackHandler("synteny100000", syntenyMethods);
registerTrackHandler("syntenyBuild30", syntenyMethods);
registerTrackHandler("syntenyBerk", syntenyMethods);
registerTrackHandler("syntenyRatBerkSmall", syntenyMethods);
registerTrackHandler("syntenySanger", syntenyMethods);
registerTrackHandler("syntenyPevzner", syntenyMethods);
registerTrackHandler("syntenyBaylor", syntenyMethods);
registerTrackHandler("switchDbTss", switchDbTssMethods);
registerTrackHandler("zdobnovHg16", lfChromColorMethods);
registerTrackHandler("zdobnovMm3", lfChromColorMethods);
registerTrackHandler("zdobnovRn3", lfChromColorMethods);
registerTrackHandler("deweySyntHg16", deweySyntMethods);
registerTrackHandler("deweySyntMm3", deweySyntMethods);
registerTrackHandler("deweySyntRn3", deweySyntMethods);
registerTrackHandler("deweySyntPanTro1", deweySyntMethods);
registerTrackHandler("deweySyntGalGal2", deweySyntMethods);
registerTrackHandler("mouseOrtho", mouseOrthoMethods);
registerTrackHandler("mouseOrthoSeed", mouseOrthoMethods);
//registerTrackHandler("orthoTop4", drawColorMethods);
registerTrackHandler("humanParalog", humanParalogMethods);
registerTrackHandler("isochores", isochoresMethods);
registerTrackHandler("gcPercent", gcPercentMethods);
registerTrackHandler("gcPercentSmall", gcPercentMethods);
registerTrackHandler("ctgPos", contigMethods);
registerTrackHandler("ctgPos2", contigMethods);
registerTrackHandler("bactigPos", bactigMethods);
registerTrackHandler("gold", goldMethods);
registerTrackHandler("gap", gapMethods);
registerTrackHandler("genomicDups", genomicDupsMethods);
registerTrackHandler("clonePos", coverageMethods);
registerTrackHandler("genieKnown", genieKnownMethods);
registerTrackHandler("knownGene", knownGeneMethods);
registerTrackHandler("hg17Kg", hg17KgMethods);
registerTrackHandler("superfamily", superfamilyMethods);
registerTrackHandler("gad", gadMethods);
registerTrackHandler("rgdQtl", rgdQtlMethods);
registerTrackHandler("refGene", refGeneMethods);
registerTrackHandler("blastMm6", blastMethods);
registerTrackHandler("blastDm1FB", blastMethods);
registerTrackHandler("blastDm2FB", blastMethods);
registerTrackHandler("blastCe3WB", blastMethods);
registerTrackHandler("blastHg16KG", blastMethods);
registerTrackHandler("blastHg17KG", blastMethods);
registerTrackHandler("blastHg18KG", blastMethods);
registerTrackHandler("blatHg16KG", blastMethods);
registerTrackHandler("blatzHg17KG", blatzMethods);
registerTrackHandler("mrnaMapHg17KG", blatzMethods);
registerTrackHandler("blastSacCer1SG", blastMethods);
registerTrackHandler("tblastnHg16KGPep", blastMethods);
registerTrackHandler("xenoRefGene", xenoRefGeneMethods);
registerTrackHandler("sanger22", sanger22Methods);
registerTrackHandler("sanger22pseudo", sanger22Methods);
registerTrackHandler("vegaGene", vegaMethods);
registerTrackHandler("vegaPseudoGene", vegaMethods);
registerTrackHandler("vegaGeneZfish", vegaMethods);
registerTrackHandler("pseudoGeneLink", retroGeneMethods);
registerTrackHandler("pseudoGeneLink2", retroGeneMethods);
registerTrackHandler("retroMrnaInfo", retroGeneMethods);
registerTrackHandler("retroMrnaInfo2", retroGeneMethods);
registerTrackHandler("bdgpGene", bdgpGeneMethods);
registerTrackHandler("bdgpNonCoding", bdgpGeneMethods);
registerTrackHandler("bdgpLiftGene", bdgpGeneMethods);
registerTrackHandler("bdgpLiftNonCoding", bdgpGeneMethods);
registerTrackHandler("flyBaseGene", flyBaseGeneMethods);
registerTrackHandler("flyBaseNoncoding", flyBaseGeneMethods);
registerTrackHandler("sgdGene", sgdGeneMethods);
registerTrackHandler("genieAlt", genieAltMethods);
registerTrackHandler("ensGene", ensGeneMethods);
registerTrackHandler("ensGeneNonCoding", ensGeneMethods);
registerTrackHandler("ensEst", ensGeneMethods);
registerTrackHandler("mrna", mrnaMethods);
registerTrackHandler("intronEst", estMethods);
registerTrackHandler("est", estMethods);
registerTrackHandler("all_est", estMethods);
registerTrackHandler("all_mrna", mrnaMethods);
registerTrackHandler("interPro", interProMethods);
registerTrackHandler("tightMrna", mrnaMethods);
registerTrackHandler("tightEst", mrnaMethods);
registerTrackHandler("cpgIsland", cpgIslandMethods);
registerTrackHandler("cpgIslandExt", cpgIslandMethods);
registerTrackHandler("cpgIslandGgfAndy", cpgIslandMethods);
registerTrackHandler("cpgIslandGgfAndyMasked", cpgIslandMethods);
registerTrackHandler("exoMouse", exoMouseMethods);
registerTrackHandler("pseudoMrna", xenoMrnaMethods);
registerTrackHandler("pseudoMrna2", xenoMrnaMethods);
registerTrackHandler("mrnaBlastz", xenoMrnaMethods);
registerTrackHandler("mrnaBlastz2", xenoMrnaMethods);
registerTrackHandler("xenoBlastzMrna", xenoMrnaMethods);
registerTrackHandler("xenoBestMrna", xenoMrnaMethods);
registerTrackHandler("xenoMrna", xenoMrnaMethods);
registerTrackHandler("xenoEst", xenoMrnaMethods);
registerTrackHandler("exoFish", exoFishMethods);
registerTrackHandler("tet_waba", tetWabaMethods);
registerTrackHandler("wabaCbr", cbrWabaMethods);
registerTrackHandler("rnaGene", rnaGeneMethods);
registerTrackHandler("encodeRna", encodeRnaMethods);
registerTrackHandler("wgRna", wgRnaMethods);
registerTrackHandler("ncRna", ncRnaMethods);
registerTrackHandler("rmskLinSpec", repeatMethods);
registerTrackHandler("rmsk", repeatMethods);
registerTrackHandler("rmskNew", repeatMethods);
registerTrackHandler("rmskCensor", repeatMethods);
registerTrackHandler("simpleRepeat", simpleRepeatMethods);
registerTrackHandler("chesSimpleRepeat", simpleRepeatMethods);
registerTrackHandler("uniGene",uniGeneMethods);
registerTrackHandler("perlegen",perlegenMethods);
registerTrackHandler("haplotype",haplotypeMethods);
registerTrackHandler("encodeErge5race",encodeErgeMethods);
registerTrackHandler("encodeErgeBinding",encodeErgeMethods);
registerTrackHandler("encodeErgeExpProm",encodeErgeMethods);
registerTrackHandler("encodeErgeHssCellLines",encodeErgeMethods);
registerTrackHandler("encodeErgeInVitroFoot",encodeErgeMethods);
registerTrackHandler("encodeErgeMethProm",encodeErgeMethods);
registerTrackHandler("encodeErgeStableTransf",encodeErgeMethods);
registerTrackHandler("encodeErgeSummary",encodeErgeMethods);
registerTrackHandler("encodeErgeTransTransf",encodeErgeMethods);
registerTrackHandler("encodeStanfordNRSF",encodeStanfordNRSFMethods);
registerTrackHandler("nci60", nci60Methods);
registerTrackHandler("cghNci60", cghNci60Methods);
registerTrackHandler("rosetta", rosettaMethods);
registerTrackHandler("affy", affyMethods);
registerTrackHandler("ancientR", ancientRMethods );
registerTrackHandler("altGraphX", altGraphXMethods );
registerTrackHandler("triangle", triangleMethods );
registerTrackHandler("triangleSelf", triangleMethods );
registerTrackHandler("transfacHit", triangleMethods );
registerTrackHandler("esRegGeneToMotif", eranModuleMethods );
registerTrackHandler("leptin", mafMethods );
registerTrackHandler("igtc", igtcMethods );
/* Lowe lab related */
registerTrackHandler("refSeq", archaeaGeneMethods);
registerTrackHandler("gbProtCode", gbGeneMethods);
registerTrackHandler("tigrCmrORFs", tigrGeneMethods);
registerTrackHandler("BlastPEuk",llBlastPMethods);
registerTrackHandler("BlastPBac",llBlastPMethods);
registerTrackHandler("BlastPpyrFur2",llBlastPMethods);
registerTrackHandler("codeBlast",codeBlastMethods);
registerTrackHandler("tigrOperons",tigrOperonMethods);
registerTrackHandler("rabbitScore",valAlMethods);
registerTrackHandler("armadilloScore",valAlMethods);
registerTrackHandler("rnaGenes",rnaGenesMethods);
registerTrackHandler("sargassoSea",sargassoSeaMethods);
registerTrackHandler("llaPfuPrintC2", loweExpRatioMethods);

/*Test for my own MA data
registerTrackHandler("llaPfuPrintCExps",arrayMethods);*/
/* MGC related */
registerTrackHandler("mgcIncompleteMrna", mrnaMethods);
registerTrackHandler("mgcFailedEst", estMethods);
registerTrackHandler("mgcPickedEst", estMethods);
registerTrackHandler("mgcUnpickedEst", estMethods);

registerTrackHandler("HMRConservation", humMusLMethods);
registerTrackHandler("humMusL", humMusLMethods);
registerTrackHandler("regpotent", humMusLMethods);
registerTrackHandler("mm3Rn2L", humMusLMethods);
registerTrackHandler("hg15Mm3L", humMusLMethods);
registerTrackHandler("zoo", zooMethods);
registerTrackHandler("zooNew", zooMethods); 
registerTrackHandler("musHumL", humMusLMethods);
registerTrackHandler("mm3Hg15L", humMusLMethods);
registerTrackHandler("affyTranscriptome", affyTranscriptomeMethods);
registerTrackHandler("genomicSuperDups", genomicSuperDupsMethods);
registerTrackHandler("celeraDupPositive", celeraDupPositiveMethods);
registerTrackHandler("celeraCoverage", celeraCoverageMethods);
registerTrackHandler("jkDuplicon", jkDupliconMethods);
registerTrackHandler("affyTransfrags", affyTransfragsMethods);
registerTrackHandler("RfamSeedFolds", rnaSecStrMethods);
registerTrackHandler("RfamFullFolds", rnaSecStrMethods);
registerTrackHandler("rfamTestFolds", rnaSecStrMethods);
registerTrackHandler("rnaTestFolds", rnaSecStrMethods);
registerTrackHandler("rnaTestFoldsV2", rnaSecStrMethods);
registerTrackHandler("rnaTestFoldsV3", rnaSecStrMethods);
registerTrackHandler("evofold", rnaSecStrMethods);
registerTrackHandler("evofoldRaw", rnaSecStrMethods);
registerTrackHandler("encode_tba23EvoFold", rnaSecStrMethods);
registerTrackHandler("encodeEvoFold", rnaSecStrMethods);
registerTrackHandler("rnafold", rnaSecStrMethods);
registerTrackHandler("mcFolds", rnaSecStrMethods);
registerTrackHandler("rnaEditFolds", rnaSecStrMethods);
registerTrackHandler("altSpliceFolds", rnaSecStrMethods);
registerTrackHandler("chimpSimpleDiff", chimpSimpleDiffMethods);
registerTrackHandler("tfbsCons", tfbsConsMethods);
registerTrackHandler("tfbsConsSites", tfbsConsSitesMethods);
registerTrackHandler("pscreen", simpleBedTriangleMethods);
registerTrackHandler("dless", dlessMethods);
/* ENCODE related */
registerTrackHandler("encodeGencodeGene", gencodeGeneMethods);
registerTrackHandler("encodeGencodeGeneJun05", gencodeGeneMethods);
registerTrackHandler("encodeGencodeGeneOct05", gencodeGeneMethods);
registerTrackHandler("encodeGencodeGeneMar07", gencodeGeneMethods);
registerTrackHandler("encodeGencodeIntron", gencodeIntronMethods);
registerTrackHandler("encodeGencodeIntronJun05", gencodeIntronMethods);
registerTrackHandler("encodeGencodeIntronOct05", gencodeIntronMethods);
registerTrackHandler("affyTxnPhase2", affyTxnPhase2Methods);
registerTrackHandler("gvPos", gvMethods);
registerTrackHandler("oreganno", oregannoMethods);
registerTrackHandler("jaxAllele", jaxAlleleMethods);
registerTrackHandler("jaxPhenotype", jaxPhenotypeMethods);
registerTrackHandler("encodeDless", dlessMethods);

/* transMap */
registerTrackHandler("transMap", transMapMethods);
registerTrackHandler("transMapRefGene", transMapMethods);
registerTrackHandler("transMapRefAliGene", transMapMethods);
registerTrackHandler("transMapMRnaGene", transMapMethods);
registerTrackHandler("transMapMRnaAliGene", transMapMethods);
registerTrackHandler("transMapAnc", transMapMethods);
registerTrackHandler("transMapAncRefGene", transMapMethods);
registerTrackHandler("transMapAncRefAliGene", transMapMethods);
registerTrackHandler("transMapAncMRnaGene", transMapMethods);
registerTrackHandler("transMapAncMRnaAliGene", transMapMethods);

registerTrackHandler("retroposons", dbRIPMethods);

registerTrackHandler("hapmapSnps", hapmapMethods);

/* Load regular tracks, blatted tracks, and custom tracks. 
 * Best to load custom last. */
loadFromTrackDb(&trackList);
if (userSeqString != NULL) slSafeAddHead(&trackList, userPslTg());
slSafeAddHead(&trackList, oligoMatchTg());
if (restrictionEnzymesOk())
    {
    slSafeAddHead(&trackList, cuttersTg());
    }
loadCustomTracks(&trackList);
groupTracks(&trackList, pGroupList);

if (cgiOptionalString( "hideTracks"))
    changeTrackVis(groupList, NULL, tvHide, FALSE);

/* Get visibility values if any from ui. */
for (track = trackList; track != NULL; track = track->next)
    {
    char *s = cartOptionalString(cart, track->mapName);
    if (cgiOptionalString( "hideTracks"))
	{
	s = cgiOptionalString(track->mapName);
	if (s != NULL)
            {
	    cartSetString(cart, track->mapName, s);
            }
	}
    if (s != NULL)
	track->visibility = hTvFromString(s);
    if (isCompositeTrack(track))
        compositeTrackVis(track);
    }

return trackList;
}

void doNextPrevItem(char *whichWay, char *trackName)
/* In case a next/previous item arrow was clicked on a track, change */
/* position (i.e. winStart, winEnd, etc.) based on what track it was */
{
struct track *track = trackList;
if (trackName == NULL)
    return;
while ((track != NULL) && (!sameString(track->mapName, trackName)))
    track = track->next;
if (track == NULL)
    return;
if (track->labelNextPrevItem != NULL)
    track->labelNextPrevItem(track, sameString(whichWay, "nextItem"));
}

void doTrackForm(char *psOutput)
/* Make the tracks display form with the zoom/scroll
 * buttons and the active image. */
{
struct group *group;
struct track *track;
char *freezeName = NULL;
boolean hideAll = cgiVarExists("hgt.hideAll");
boolean defaultTracks = cgiVarExists("hgt.reset");
boolean showedRuler = FALSE;
boolean showTrackControls = cartUsualBoolean(cart, "trackControlsOnMain", TRUE);
long thisTime = 0, lastTime = 0;
char *clearButtonJavascript;

zoomedToBaseLevel = (winBaseCount <= insideWidth / tl.mWidth);
zoomedToCodonLevel = (ceil(winBaseCount/3) * tl.mWidth) <= insideWidth;
zoomedToCdsColorLevel = (winBaseCount <= insideWidth*3);

if (psOutput != NULL)
   {
   hPrintDisable();
   hideControls = TRUE;
   withNextItemArrows = FALSE;
   withNextExonArrows = FALSE;
   hgFindMatches = NULL;
   }

/* Tell browser where to go when they click on image. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackHeaderForm\" METHOD=GET>\n\n", hgTracksName());
cartSaveSession(cart);
clearButtonJavascript = "document.TrackHeaderForm.position.value=''";

/* See if want to include sequence search results. */
userSeqString = cartOptionalString(cart, "ss");
if (userSeqString && !ssFilesExist(userSeqString))
    {
    userSeqString = NULL;
    cartRemove(cart, "ss");
    }
if (!hideControls)
    hideControls = cartUsualBoolean(cart, "hideControls", FALSE);
if (measureTiming)
    uglyTime("Time before getTrackList");
trackList = getTrackList(&groupList);

if (measureTiming)
    uglyTime("getTrackList");

/* Honor hideAll and visAll variables */
if(hideAll || defaultTracks)
    {
    int vis = (hideAll ? tvHide : -1);
    changeTrackVis(groupList, NULL, vis, FALSE);
    }

/* Before loading items, deal with the next/prev item arrow buttons if pressed. */
if (cgiVarExists("hgt.nextItem"))       
    doNextPrevItem("nextItem", cgiUsualString("hgt.nextItem", NULL));
else if (cgiVarExists("hgt.prevItem"))
    doNextPrevItem("prevItem", cgiUsualString("hgt.prevItem", NULL));

/* Tell tracks to load their items. */
for (track = trackList; track != NULL; track = track->next)
    {
    /* remove cart priority variables if they are set  
       to the default values in the trackDb */
    if(!hTrackOnChrom(track->tdb, chromName)) 
	{
	track->limitedVis = tvHide;
	track->limitedVisSet = TRUE;
	}
    else if (track->visibility != tvHide)
	{
	if (measureTiming)
	    lastTime = clock1000();
	track->loadItems(track); 
	
	if (measureTiming)
	    {
	    thisTime = clock1000();
	    track->loadTime = thisTime - lastTime;
	    }
	}
    }

/* Center everything from now on. */
hPrintf("<CENTER>\n");

if (!hideControls)
    {
    char *browserName = (isPrivateHost ? "Test Browser" : "Genome Browser");
    char *organization = (hIsMgcServer() ? "MGC/ORFeome" : "UCSC");
    hotLinks();

    /* Show title . */
    freezeName = hFreezeFromDb(database);
    if(freezeName == NULL)
	freezeName = "Unknown";
    hPrintf("<FONT SIZE=5><B>");
    if (startsWith("zoo",database) )
	{
	hPrintf("%s %s on %s June 2002 Assembly %s target1",
		organization, browserName, organism, freezeName); 
	}
    else
	{

	if (sameString(organism, "Archaea"))
	    hPrintf("%s %s on Archaeon %s Assembly", 
	    	organization, browserName, freezeName);
	else
	    hPrintf("%s %s on %s %s Assembly", 
	    	organization, browserName, organism, freezeName); 
	}
    hPrintf("</B></FONT><BR>\n");

    /* This is a clear submit button that browsers will use by default when enter is pressed in position box. */
    hPrintf("<INPUT TYPE=IMAGE BORDER=0 NAME=\"hgt.dummyEnterButton\" src=\"../images/DOT.gif\">");
    /* Put up scroll and zoom controls. */
    hWrites("move ");
    hButton("hgt.left3", "<<<");
    hButton("hgt.left2", " <<");
    hButton("hgt.left1", " < ");
    hButton("hgt.right1", " > ");
    hButton("hgt.right2", ">> ");
    hButton("hgt.right3", ">>>");
    hWrites(" zoom in ");
    /* use button maker that determines padding, so we can share constants */
    topButton("hgt.in1", ZOOM_1PT5X);
    topButton("hgt.in2", ZOOM_3X);
    topButton("hgt.in3", ZOOM_10X);
    topButton("hgt.inBase", ZOOM_BASE);
    hWrites(" zoom out ");
    topButton("hgt.out1", ZOOM_1PT5X);
    topButton("hgt.out2", ZOOM_3X);
    topButton("hgt.out3", ZOOM_10X);
    hWrites("<BR>\n");

    if (showTrackControls)
	{
	/* Break into a second form so that zooming and scrolling
	 * can be done with a 'GET' so that user can back up from details
	 * page without Internet Explorer popping up an annoying dialog.
	 * Do rest of page as a 'POST' so that the ultra-long URL from
	 * all the track controls doesn't break things.  IE URL limit
	 * is 2000 bytes, but some firewalls impose a ~1000 byte limit.
	 * As a side effect of breaking up the page into two forms
	 * we need to repeat the position in a hidden variable here
	 * so that zoom/scrolling always has current position to work
	 * from. */
	hPrintf("<INPUT TYPE=HIDDEN NAME=\"position\" "
		"VALUE=\"%s:%d-%d\">", chromName, winStart+1, winEnd);
	hPrintf("</CENTER></FORM>\n");
	hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackForm\" METHOD=POST>\n\n", hgTracksName());
	cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */
	clearButtonJavascript = "document.TrackForm.position.value=''";
	hPrintf("<CENTER>");
	}


    /* Make line that says position. */
	{
	char buf[256];
        char *javascript = "onchange=\"document.location = '/cgi-bin/hgTracks?db=' + document.TrackForm.db.options[document.TrackForm.db.selectedIndex].value;\"";
        if (containsStringNoCase(database, "zoo"))
            {
            hPuts("Organism ");
            printAssemblyListHtmlExtra(database, javascript);
            }

	sprintf(buf, "%s:%d-%d", chromName, winStart+1, winEnd);
	position = cloneString(buf);
	hWrites("position/search ");
	hTextVar("position", addCommasToPos(position), 30);
	sprintLongWithCommas(buf, winEnd - winStart);
	hWrites(" ");
	hButton("submit", "jump");
	hOnClickButton(clearButtonJavascript,"clear");
	hPrintf(" size %s bp. ", buf);
        hButton("hgTracksConfigPage", "configure");
	hPutc('\n');
	}
    }

/* Make chromsome ideogram gif and map. */
makeChromIdeoImage(&trackList, psOutput);

/* Make clickable image and map. */
makeActiveImage(trackList, psOutput);
fflush(stdout);
if (!hideControls)
    {
    struct controlGrid *cg = NULL;

    hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1 WIDTH=%d COLS=%d><TR>\n", 
    	tl.picWidth, 27);
    hPrintf("<TD COLSPAN=6 ALIGN=CENTER NOWRAP>");
    hPrintf("move start<BR>");
    hButton("hgt.dinkLL", " < ");
    hTextVar("dinkL", cartUsualString(cart, "dinkL", "2.0"), 3);
    hButton("hgt.dinkLR", " > ");
    hPrintf("</TD><TD COLSPAN=15>");
    hWrites("Click on a feature for details. "
	  "Click on base position to zoom in around cursor. "
	  "Click on left mini-buttons for track-specific options." );
    hPrintf("</TD><TD COLSPAN=6 ALIGN=CENTER NOWRAP>");
    hPrintf("move end<BR>");
    hButton("hgt.dinkRL", " < ");
    hTextVar("dinkR", cartUsualString(cart, "dinkR", "2.0"), 3);
    hButton("hgt.dinkRR", " > ");
    hPrintf("</TD></TR></TABLE>\n");
    // smallBreak();

    /* Display bottom control panel. */
    hButton("hgt.reset", "default tracks");
    if (showTrackControls)
	{
	hPrintf(" ");
	hButton("hgt.hideAll", "hide all");
	}

    hPrintf(" ");
    hOnClickButton("document.customTrackForm.submit();return false;",
                        hasCustomTracks ? 
                            CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
    hPrintf(" ");
    hButton("hgTracksConfigPage", "configure");
    hPrintf(" ");
    hButton("submit", "refresh");

    hPrintf("<BR>\n");

    if( chromosomeColorsMade )
        {
        hPrintf("<B>Chromosome Color Key:</B><BR> ");
        hPrintf("<IMG SRC = \"../images/new_colorchrom.gif\" BORDER=1 WIDTH=596 HEIGHT=18 ><BR>\n");
        }

if (showTrackControls)
    {
    /* Display viewing options for each track. */
    /* Chuck: This is going to be wrapped in a table so that
     * the controls don't wrap around randomly */
    hPrintf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
    hPrintf("<tr><td colspan='5' align='CENTER' nowrap>"
	   "Use drop down controls below and press refresh to alter tracks "
	   "displayed.<BR>"
	   "Tracks with lots of items will automatically be displayed in "
	   "more compact modes.</td></tr>\n");
    cg = startControlGrid(MAX_CONTROL_COLUMNS, "left");
    for (group = groupList; group != NULL; group = group->next)
        {
	struct trackRef *tr;

	if (group->trackList == NULL)
	    continue;

	/* Print group label on left. */
	hPrintf("<TR>");
	cg->rowOpen = TRUE;
	hPrintf("<th colspan=%d BGCOLOR=#536ED3>", 
		MAX_CONTROL_COLUMNS);
	hPrintf("<B>%s</B>", wrapWhiteFont(group->label));
	hPrintf("\n<A NAME=\"#%s\"></A>\n",group->name);
	hPrintf("</th>\n", MAX_CONTROL_COLUMNS);
	controlGridEndRow(cg);

	/* First group gets ruler. */
	if (!showedRuler)
	    {
	    showedRuler = TRUE;
	    controlGridStartCell(cg);
            hPrintf("<A HREF=\"%s?%s=%u&c=%s&g=%s\">", hgTrackUiName(),
		    cartSessionVarName(), cartSessionId(cart),
		    chromName, RULER_TRACK_NAME);
	    hPrintf(" %s<BR> ", RULER_TRACK_LABEL);
            hPrintf("</A>");
	    hDropList("ruler", rulerMenu, sizeof(rulerMenu)/sizeof(char *), 
		      rulerMenu[rulerMode]);
	    controlGridEndCell(cg);
	    }

	for (tr = group->trackList; tr != NULL; tr = tr->next)
	    {
	    track = tr->track;
	    controlGridStartCell(cg);
	    if (track->hasUi)
		{
		char *encodedMapName = cgiEncode(track->mapName);
		hPrintf("<A HREF=\"%s?%s=%u&c=%s&g=%s\">", hgTrackUiName(),
		    cartSessionVarName(), cartSessionId(cart),
		    chromName, encodedMapName);
		freeMem(encodedMapName);
		}
	    hPrintf(" %s<BR> ", track->shortLabel);
	    if (track->hasUi)
		hPrintf("</A>");

	    /* If track is not on this chrom print an informational
	       message for the user. */
	    if(hTrackOnChrom(track->tdb, chromName)) 
		{
		/* check for option of limiting visibility to one mode */
		char *onlyVisibility =
			trackDbSetting(track->tdb, "onlyVisibility");
		hTvDropDownClassVisOnly(track->mapName, track->visibility,
		    track->canPack, (track->visibility == tvHide) ? 
			 "hiddenText" : "normalText", onlyVisibility );
		}
	    else 
		hPrintf("[No data-%s]", chromName);
	    controlGridEndCell(cg);
	    }
	/* now finish out the table */
	if (group->next != NULL)
	    controlGridEndRow(cg);
	}
    endControlGrid(&cg);
    }

    if (measureTiming)
        {
	hPrintf("track, load time, draw time, total<BR>\n");
	for (track = trackList; track != NULL; track = track->next)
	    {
	    if (track->visibility == tvHide)
                continue;
            if (isCompositeTrack(track))
                {
                struct track *subtrack;
                for (subtrack = track->subtracks; subtrack != NULL; 
                                                    subtrack = subtrack->next)
                    if (isSubtrackVisible(subtrack))
                        hPrintf("%s, %d, %d, %d<BR>\n", subtrack->shortLabel, 
                                subtrack->loadTime, subtrack->drawTime,
				subtrack->loadTime + subtrack->drawTime);
                }
            else 
                {
	        hPrintf("%s, %d, %d, %d<BR>\n", 
			track->shortLabel, track->loadTime, track->drawTime,
			track->loadTime + track->drawTime);
                if (startsWith("wigMaf", track->tdb->type))
                  if (track->subtracks)
                      if (track->subtracks->loadTime)
                         hPrintf("&nbsp; &nbsp; %s wiggle, load %d<BR>\n", 
                            track->shortLabel, track->subtracks->loadTime);
                }
	    }
	}
    }
if (showTrackControls)
    hButton("submit", "refresh");
hPrintf("</CENTER>\n");

#ifdef SLOW
/* We'll rely on the end of program to do the cleanup.
 * It turns out that the 'free' routine on Linux is
 * quite slow.  For chromosome level views the browser
 * spends about 1/3 of it's time doing the cleanup
 * below if it's enabled.  Since we really don't
 * need to reclaim this memory at this point I'm
 * taking this out.  Please don't delete the code though.
 * I'll like to keep it for testing now and then. -jk. */

/* Clean up. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->visibility != tvHide)
	{
	if (track->freeItems != NULL)
	    track->freeItems(track);
	lmCleanup(&track->lm);
	}
    }
#endif /* SLOW */
hPrintf("</FORM>\n");

/* hidden form for custom tracks CGI */
hPrintf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
cartSaveSession(cart);
hPrintf("</FORM>\n");
}

void zoomToSize(int newSize)
/* Zoom so that center stays in same place,
 * but window is new size.  If necessary move
 * center a little bit to keep it from going past
 * edges. */
{
int center = (winStart + winEnd)/2;
if (newSize > seqBaseCount)
    newSize = seqBaseCount;
winStart = center - newSize/2;
winEnd = winStart + newSize;
if (winStart <= 0)
    {
    winStart = 0;
    winEnd = newSize;
    }
else if (winEnd > seqBaseCount)
    {
    winEnd = seqBaseCount;
    winStart = winEnd - newSize;
    }
winBaseCount = winEnd - winStart;
}

void zoomAroundCenter(double amount)
/* Set ends so as to zoom around center by scaling amount. */
{
double newSizeDbl = (winBaseCount*amount + 0.5);
int newSize;
if (newSizeDbl > seqBaseCount)
    newSize = seqBaseCount;
else if (newSizeDbl < 1.0)
    newSize = 1;
else
    newSize = (int)newSizeDbl;
zoomToSize(newSize);
}

void zoomToBaseLevel()
/* Set things so that it's zoomed to base level. */
{
zoomToSize(insideWidth/tl.mWidth);
if (rulerMode == tvHide)
    cartSetString(cart, "ruler", "dense");
}

void relativeScroll(double amount)
/* Scroll percentage of visible window. */
{
int offset;
int newStart, newEnd;

offset = (int)(amount * winBaseCount);
/* Make sure don't scroll of ends. */
newStart = winStart + offset;
newEnd = winEnd + offset;
if (newStart < 0)
    offset = -winStart;
else if (newEnd > seqBaseCount)
    offset = seqBaseCount - winEnd;

/* Move window. */
winStart += offset;
winEnd += offset;
}

void dinkWindow(boolean start, int dinkAmount)
/* Move one end or other of window a little. */
{
if (start)
   {
   winStart += dinkAmount;
   if (winStart < 0) winStart = 0;
   }
else
   {
   winEnd += dinkAmount;
   if (winEnd > seqBaseCount)
       winEnd = seqBaseCount;
   }
}

int dinkSize(char *var)
/* Return size to dink. */
{
char *stringVal = cartOptionalString(cart, var);
double x;
int insideX = trackOffsetX(); /* The global versions of these are not yet set */
int insideWidth = tl.picWidth-gfxBorder-insideX;
double guideBases = (double)guidelineSpacing * (double)(winEnd - winStart) 
	/ ((double)insideWidth);

if (stringVal == NULL || !isdigit(stringVal[0]))
    {
    stringVal = "1";
    cartSetString(cart, var, stringVal);
    }
x = atof(stringVal);
return round(x*guideBases);
}

void handlePostscript()
/* Deal with Postscript output. */
{
struct tempName psTn;
char *pdfFile = NULL;
trashDirFile(&psTn, "hgt", "hgt", ".eps");
printf("<H1>PostScript/PDF Output</H1>\n");
printf("PostScript images can be printed at high resolution "
       "and edited by many drawing programs such as Adobe "
       "Illustrator.<BR>");
doTrackForm(psTn.forCgi);
printf("<A HREF=\"%s\">Click here</A> "
       "to download the current browser graphic in PostScript.  ", psTn.forCgi);
pdfFile = convertEpsToPdf(psTn.forCgi);
if(pdfFile != NULL) 
    {
    printf("<BR><BR>PDF can be viewed with Adobe Acrobat Reader.<BR>\n");
    printf("<A TARGET=_blank HREF=\"%s\">Click here</A> "
	   "to download the current browser graphic in PDF.", pdfFile);
    }
else
    printf("<BR><BR>PDF format not available");
freez(&pdfFile);
}

boolean isGenome(char *pos)
/* Return TRUE if pos is genome. */
{
pos = trimSpaces(pos);
return(sameWord(pos, "genome") || sameWord(pos, "hgBatch"));
}

void createHgFindMatchHash()
/* Read from the cart the string assocated with matches and 
   put the matching items into a hash for highlighting later. */
{
char *matchLine = NULL;
struct slName *nameList = NULL, *name = NULL;
matchLine = cartOptionalString(cart, "hgFind.matches");
if(matchLine == NULL)
    return;
nameList = slNameListFromString(matchLine,',');
hgFindMatches = newHash(5);
for(name = nameList; name != NULL; name = name->next)
    {
    hashAddInt(hgFindMatches, name->name, 1);
    }
slFreeList(&nameList);
}

void setRulerMode()
/* Set the rulerMode variable from cart. */
{
char *s = cartUsualString(cart, RULER_TRACK_NAME, "dense");
if (sameWord(s, "full") || sameWord(s, "on"))
    rulerMode = tvFull;
else if (sameWord(s, "dense"))
    rulerMode = tvDense;
else
    rulerMode = tvHide;
}

void tracksDisplay()
/* Put up main tracks display. This routine handles zooming and
 * scrolling. */
{
char newPos[256];
char *defaultPosition = hDefaultPos(database);
char titleVar[256];
position = getPositionFromCustomTracks();
if (NULL == position) 
    {
    position = cloneString(cartUsualString(cart, "position", NULL));
    }

/* default if not set at all, as would happen if it came from a URL with no
 * position. Otherwise tell them to go back to the gateway. Also recognize
 * "default" as specifying the default position. */
if (((position == NULL) || sameString(position, "default"))
    && (defaultPosition != NULL))
    position = cloneString(defaultPosition);
if (sameString(position, ""))
    {
    errAbort("Please go back and enter a coordinate range in the \"position\" field.<br>For example: chr22:20100000-20200000.\n");
    }

chromName = NULL;
winStart = 0;
if (isGenome(position) || NULL ==
    (hgp = findGenomePos(position, &chromName, &winStart, &winEnd, cart)))
    {
    if (winStart == 0)	/* number of positions found */
	hgp = findGenomePos(defaultPosition, &chromName, &winStart, &winEnd,
			    cart);
    }

if (NULL != hgp && NULL != hgp->tableList && NULL != hgp->tableList->name)
    {
    char *trackName = hgp->tableList->name;
    char *parent = hGetParent(trackName);
    if (parent)
        trackName = cloneString(parent);
    cartSetString(cart, trackName, hTrackOpenVis(trackName));
    }

/* After position is found set up hash of matches that should
   be drawn with names highlighted for easy identification. */
createHgFindMatchHash();

/* This means that no single result was found 
I.e., multiple results may have been found and are printed out prior to this code*/
if (NULL == chromName)
    {
    return;
    }

seqBaseCount = hChromSize(chromName);
winBaseCount = winEnd - winStart;

/* Figure out basic dimensions of display.  This
 * needs to be done early for the sake of the
 * zooming and dinking routines. */
withIdeogram = cartUsualBoolean(cart, "ideogram", TRUE);
withLeftLabels = cartUsualBoolean(cart, "leftLabels", TRUE);
withCenterLabels = cartUsualBoolean(cart, "centerLabels", TRUE);
withGuidelines = cartUsualBoolean(cart, "guidelines", TRUE);
withNextItemArrows = cartUsualBoolean(cart, "nextItemArrows", FALSE);
withNextExonArrows = cartUsualBoolean(cart, "nextExonArrows", FALSE);
withPriorityOverride = cartUsualBoolean(cart, configPriorityOverride, FALSE);
insideX = trackOffsetX();
insideWidth = tl.picWidth-gfxBorder-insideX;

baseShowPos = cartUsualBoolean(cart, BASE_SHOWPOS, FALSE);
baseShowAsm = cartUsualBoolean(cart, BASE_SHOWASM, FALSE);
safef(titleVar,sizeof(titleVar),"%s_%s", BASE_TITLE, database);
baseTitle = cartUsualString(cart, titleVar, "");
if (sameString(baseTitle, "")) 
    baseTitle = NULL;

setRulerMode();

/* Do zoom/scroll if they hit it. */
if (cgiVarExists("hgt.left3"))
    relativeScroll(-0.95);
else if (cgiVarExists("hgt.left2"))
    relativeScroll(-0.475);
else if (cgiVarExists("hgt.left1"))
    relativeScroll(-0.1);
else if (cgiVarExists("hgt.right1"))
    relativeScroll(0.1);
else if (cgiVarExists("hgt.right2"))
    relativeScroll(0.475);
else if (cgiVarExists("hgt.right3"))
    relativeScroll(0.95);
else if (cgiVarExists("hgt.inBase"))
    zoomToBaseLevel();
else if (cgiVarExists("hgt.in3"))
    zoomAroundCenter(1.0/10.0);
else if (cgiVarExists("hgt.in2"))
    zoomAroundCenter(1.0/3.0);
else if (cgiVarExists("hgt.in1"))
    zoomAroundCenter(1.0/1.5);
else if (cgiVarExists("hgt.out1"))
    zoomAroundCenter(1.5);
else if (cgiVarExists("hgt.out2"))
    zoomAroundCenter(3.0);
else if (cgiVarExists("hgt.out3"))
    zoomAroundCenter(10.0);
else if (cgiVarExists("hgt.dinkLL"))
    dinkWindow(TRUE, -dinkSize("dinkL"));
else if (cgiVarExists("hgt.dinkLR"))
    dinkWindow(TRUE, dinkSize("dinkL"));
else if (cgiVarExists("hgt.dinkRL"))
    dinkWindow(FALSE, -dinkSize("dinkR"));
else if (cgiVarExists("hgt.dinkRR"))
    dinkWindow(FALSE, dinkSize("dinkR"));

/* Clip chromosomal position to fit. */
if (winEnd < winStart)
    {
    int temp = winEnd;
    winEnd = winStart;
    winStart = temp;
    }
else if (winStart == winEnd)
    {
    winStart -= 1000;
    winEnd += 1000;
    }

if (winStart < 0)
    {
    winStart = 0;
    }

if (winEnd > seqBaseCount) 
    {
    winEnd = seqBaseCount;
    }

if (winStart > seqBaseCount)
    {
    winStart = seqBaseCount - 1000;
    }

winBaseCount = winEnd - winStart;
if (winBaseCount <= 0)
    errAbort("Window out of range on %s", chromName);
/* Save computed position in cart. */
sprintf(newPos, "%s:%d-%d", chromName, winStart+1, winEnd);
cartSetString(cart, "org", organism);
cartSetString(cart, "db", database);
cartSetString(cart, "position", newPos);
if (cgiVarExists("hgt.psOutput"))
    handlePostscript();
else
    doTrackForm(NULL);
}

void chromInfoTotalRow(long long total)
/* Make table row with total size from chromInfo. */
{
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
printf("Total");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
printLongWithCommas(stdout, total);
cgiTableFieldEnd();
cgiTableRowEnd();
}

void chromInfoRowsChrom()
/* Make table rows of chromosomal chromInfo name & size, sorted by name. */
{
struct slName *chromList = hAllChromNames();
struct slName *chromPtr = NULL;
long long total = 0;

slSort(&chromList, chrSlNameCmp);
for (chromPtr = chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    unsigned size = hChromSize(chromPtr->name);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    printf("<A HREF=\"%s?%s=%u&position=%s\">%s</A>",
	   hgTracksName(), cartSessionVarName(), cartSessionId(cart),
	   chromPtr->name, chromPtr->name);
    cgiTableFieldEnd();
    cgiSimpleTableFieldStart();
    printLongWithCommas(stdout, size);
    cgiTableFieldEnd();
    cgiTableRowEnd();
    total += size;
    }
chromInfoTotalRow(total);
slFreeList(&chromList);
}

void chromInfoRowsNonChrom(int limit)
/* Make table rows of non-chromosomal chromInfo name & size, sorted by size. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
long long total = 0;
char query[512];
char msg1[512], msg2[512];
int seqCount = 0;
boolean truncating;

seqCount = sqlQuickNum(conn, "select count(*) from chromInfo");
truncating = (limit > 0) && (seqCount > limit);

if (!truncating)
    {
    sr = sqlGetResult(conn, "select chrom,size from chromInfo order by size desc");
    }
else
    {

    safef(query, sizeof(query), "select chrom,size from chromInfo order by size desc limit %d", limit);
    sr = sqlGetResult(conn, query);
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned size = sqlUnsigned(row[1]);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    printf("<A HREF=\"%s?%s=%u&position=%s\">%s</A>",
	   hgTracksName(), cartSessionVarName(), cartSessionId(cart),
	   row[0], row[0]);
    cgiTableFieldEnd();
    cgiSimpleTableFieldStart();
    printLongWithCommas(stdout, size);
    cgiTableFieldEnd();
    cgiTableRowEnd();
    total += size;
    }
if (!truncating)
    {
    chromInfoTotalRow(total);
    }
else
    {
    safef(msg1, sizeof(msg1), "Limit reached");
    safef(msg2, sizeof(msg2), "%d rows displayed", limit);
    cgiSimpleTableRowStart();
    cgiSimpleTableFieldStart();
    printf(msg1);
    cgiTableFieldEnd();
    cgiSimpleTableFieldStart();
    printf(msg2);
    cgiTableFieldEnd();
    sqlFreeResult(&sr);
    safef(query, sizeof(query), "select count(*),sum(size) from chromInfo");
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	unsigned scafCount = sqlUnsigned(row[0]);
	unsigned totalSize = sqlUnsigned(row[1]);
	cgiTableRowEnd();
	safef(msg1, sizeof(msg1), "contig/scaffold<BR>count:");
	safef(msg2, sizeof(msg2), "total size:");
	cgiSimpleTableRowStart();
	cgiSimpleTableFieldStart();
	printf(msg1);
	cgiTableFieldEnd();
	cgiSimpleTableFieldStart();
	printf(msg2);
	cgiTableFieldEnd();
	cgiTableRowEnd();
	cgiSimpleTableRowStart();
	cgiSimpleTableFieldStart();
	printLongWithCommas(stdout, scafCount);
	cgiTableFieldEnd();
	cgiSimpleTableFieldStart();
	printLongWithCommas(stdout, totalSize);
	cgiTableFieldEnd();
	}
    cgiTableRowEnd();
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void chromInfoPage()
/* Show list of chromosomes (or scaffolds, etc) on which this db is based. */
{
char *position = cartUsualString(cart, "position", hDefaultPos(database));
char *defaultChrom = hDefaultChrom();
struct dyString *title = dyStringNew(512);
dyStringPrintf(title, "%s %s (%s) Browser Sequences",
	       hOrganism(database), hFreezeFromDb(database), database);
webStartWrapperDetailedNoArgs(cart, "", title->string, FALSE, FALSE, FALSE, FALSE);
printf("<FORM ACTION=\"%s\" NAME=\"posForm\" METHOD=GET>\n", hgTracksName());
cartSaveSession(cart);

puts("Enter a position, or click on a sequence name to view the entire "
     "sequence in the genome browser.<P>");
puts("position ");
hTextVar("position", addCommasToPos(position), 30);
cgiMakeButton("Submit", "submit");
puts("<P>");

hTableStart();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
puts("Sequence name &nbsp;");
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
puts("Length (bp) including gaps &nbsp;");
cgiTableFieldEnd();
cgiTableRowEnd();

if ((startsWith("chr", defaultChrom) || startsWith("Group", defaultChrom)) &&
    hChromCount() < 100)
    chromInfoRowsChrom();
else
    chromInfoRowsNonChrom(1000);

hTableEnd();

hgPositionsHelpHtml(organism, database);
puts("</FORM>");
dyStringFree(&title);
webEndSectionTables();
}


void resetVars()
/* Reset vars except for position and database. */
{
static char *except[] = {"db", "position", NULL};
char *cookieName = hUserCookie();
int sessionId = cgiUsualInt(cartSessionVarName(), 0);
char *hguidString = findCookieData(cookieName);
int userId = (hguidString == NULL ? 0 : atoi(hguidString));
struct cart *oldCart = cartNew(userId, sessionId, NULL, NULL);
cartRemoveExcept(oldCart, except);
cartCheckout(&oldCart);
cgiVarExcludeExcept(except);
}

void setKgVars()
/* Figure out which version of known genes we are on. */
{
if (hTableExists("kgColor"))
    gotKgColor = TRUE;
}

void doMiddle(struct cart *theCart)
/* Print the body of an html file.   */
{
char *debugTmp = NULL;
/* Uncomment this to see parameters for debugging. */
/* struct dyString *state = NULL; */
/* Initialize layout and database. */
cart = theCart;

/* Uncomment this to see parameters for debugging. */
/* Be careful though, it breaks if custom track
 * is more than 4k */
/*state = cgiUrlString(); printf("State: %s\n", state->string);   */
getDbAndGenome(cart, &database, &organism);
saveDbAndGenome(cart, database, organism);

protDbName = hPdbFromGdb(database);
debugTmp = cartUsualString(cart, "hgDebug", "off");
if(sameString(debugTmp, "on"))
    hgDebug = TRUE;
else
    hgDebug = FALSE;

hSetDb(database);
setKgVars();

hDefaultConnect();
initTl();

/* Do main display. */
if (cartVarExists(cart, "chromInfoPage"))
    {
    cartRemove(cart, "chromInfoPage");
    chromInfoPage();
    }
else if (cartVarExists(cart, "hgTracksConfigPage"))
    {
    cartRemove(cart, "hgTracksConfigPage");
    configPage();
    }
else if (cartVarExists(cart, configHideAll))
    {
    cartRemove(cart, configHideAll);
    configPageSetTrackVis(tvHide);
    }
else if (cartVarExists(cart, configShowAll))
    {
    cartRemove(cart, configShowAll);
    configPageSetTrackVis(tvDense);
    }
else if (cartVarExists(cart, configDefaultAll))
    {
    cartRemove(cart, configDefaultAll);
    configPageSetTrackVis(-1);
    }
else
    {
    tracksDisplay();
    }
}

void doDown(struct cart *cart)
{
printf("<H2>The Browser is Being Updated</H2>\n");
printf("The browser is currently unavailable.  We are in the process of\n");
printf("updating the database and the display software with a number of\n");
printf("new tracks, including some gene predictions.  Please try again tomorrow.\n");
}

/* Other than submit and Submit all these vars should start with hgt.
 * to avoid weeding things out of other program's namespaces.
 * Because the browser is a central program, most of it's cart 
 * variables are not hgt. qualified.  It's a good idea if other
 * program's unique variables be qualified with a prefix though. */
char *excludeVars[] = { "submit", "Submit", "hgt.reset",
			"hgt.in1", "hgt.in2", "hgt.in3", "hgt.inBase",
			"hgt.out1", "hgt.out2", "hgt.out3",
			"hgt.left1", "hgt.left2", "hgt.left3", 
			"hgt.right1", "hgt.right2", "hgt.right3", 
			"hgt.dinkLL", "hgt.dinkLR", "hgt.dinkRL", "hgt.dinkRR",
			"hgt.tui", "hgt.hideAll", "hgt.visAllFromCt", 
                        "hgt.psOutput", "hideControls",
			NULL };

int main(int argc, char *argv[])
{
enteredMainTime = clock1000();
uglyTime(NULL);
isPrivateHost = hIsPrivateHost();
/* Push very early error handling - this is just
 * for the benefit of the cgiVarExists, which 
 * somehow can't be moved effectively into doMiddle. */
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
htmlSetBackground(hBackgroundImage());
htmlSetStyle("<LINK REL=\"STYLESHEET\" HREF=\"/style/HGStyle.css\">"); 
oldVars = hashNew(8);
cartHtmlShell("UCSC Genome Browser v"CGI_VERSION, doMiddle, hUserCookie(), excludeVars, oldVars);
if (measureTiming)
    {
    fprintf(stdout, "Overall total time: %ld millis<BR>\n",
	clock1000() - enteredMainTime);
    }
return 0;
}
