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
#include "memgfx.h"
#include "vGfx.h"
#include "browserGfx.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
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
#include "cdsColors.h"
#include "cds.h"
#include "simpleNucDiff.h"
#include "tfbsCons.h"
#include "itemAttr.h"
#include "encode.h"
#include "variation.h"
#include "estOrientInfo.h"
#include "versionInfo.h"

static char const rcsid[] = "$Id: hgTracks.c,v 1.784 2004/08/25 18:15:08 braney Exp $";

#define MAX_CONTROL_COLUMNS 5
#define CHROM_COLORS 26
#define CDS_COLORS 6
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
boolean exprBedColorsMade = FALSE; /* Have the shades of Green, Red, and Blue been allocated? */
int maxRGBShade = EXPR_DATA_SHADES - 1;

Color chromColor[CHROM_COLORS+1];
/* Declare colors for chromosome coloring, +1 for unused chrom 0 color */

/* Have the 3 shades of 8 chromosome colors been allocated? */
boolean chromosomeColorsMade = FALSE; 

/* Declare colors for each CDS coloring possibility
 * {start,stop,splice,alternate codons} */
Color cdsColor[CDS_COLORS+1];
boolean cdsColorsMade = FALSE;


int z;
int maxCount;
int bestColor;
int maxItemsInFullTrack = 250;  /* Maximum number of items displayed in full */
int guidelineSpacing = 12;	/* Pixels between guidelines. */

struct cart *cart;	/* The cart where we keep persistent variables. */
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
static char *ctFileName = NULL;	/* Custom track file. */

int gfxBorder = hgDefaultGfxBorder;	/* Width of graphics border. */
int insideX;			/* Start of area to draw track in in pixels. */
int insideWidth;		/* Width of area to draw tracks in in pixels. */
int leftLabelX;			/* Start of area to draw left labels on. */
int leftLabelWidth;		/* Width of area to draw left labels on. */
boolean zoomedToBaseLevel; 	/* TRUE if zoomed so we can draw bases. */
boolean zoomedToCodonLevel; /* TRUE if zoomed so we can print codons text in genePreds*/
boolean zoomedToCdsColorLevel; /* TRUE if zoomed so we can color each codon*/

char *protDbName;               /* Name of proteome database for this genome. */

/* These variables are set by getPositionFromCustomTracks() at the very 
 * beginning of tracksDisplay(), and then used by loadCustomTracks(). */
struct customTrack *ctList = NULL;  /* Custom tracks. */
struct slName *browserLines = NULL; /* Custom track "browser" lines. */

boolean withIdeogram = TRUE;            /* Display chromosome ideogram? */
boolean ideogramAvail = FALSE;           /* Is the ideogram data available for this genome? */
boolean withLeftLabels = TRUE;		/* Display left labels? */
boolean withCenterLabels = TRUE;	/* Display center labels? */
boolean withGuidelines = TRUE;		/* Display guidelines? */
boolean hideControls = FALSE;		/* Hide all controls? */

#define RULER_MODE_OFF 0
#define RULER_MODE_ON 1
#define RULER_MODE_FULL 2

int rulerMode = RULER_MODE_ON;         /* on, off, full */

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

struct trackLayout tl;

boolean suppressHtml = FALSE;	
	/* If doing PostScript output we'll suppress most
         * of HTML output. */

void hvPrintf(char *format, va_list args)
/* Suppressable variable args printf. */
{
if (!suppressHtml)
    vprintf(format, args);
}

void hPrintf(char *format, ...)
/* Printf that can be suppressed if not making
 * html. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

void hPuts(char *string)
/* Puts that can be suppressed if not making
 * html. */
{
if (!suppressHtml)
    puts(string);
}

void hPutc(char c)
/* putc that can be suppressed if not making html. */
{
if (!suppressHtml)
    fputc(c, stdout);
}

void hWrites(char *string)
/* Write string with no '\n' if not suppressed. */
{
if (!suppressHtml)
    fputs(string, stdout);
}

void hButton(char *name, char *label)
/* Write out button if not suppressed. */
{
if (!suppressHtml)
    cgiMakeButton(name, label);
}

void hOnClickButton(char *command, char *label)
/* Write out push button if not suppressed. */
{
if (!suppressHtml)
    cgiMakeOnClickButton(command, label);
}

void hTextVar(char *varName, char *initialVal, int charSize)
/* Write out text entry field if not suppressed. */
{
if (!suppressHtml)
    cgiMakeTextVar(varName, initialVal, charSize);
}

void hIntVar(char *varName, int initialVal, int maxDigits)
/* Write out numerical entry field if not supressed. */
{
if (!suppressHtml)
    cgiMakeIntVar(varName, initialVal, maxDigits);
}

void hCheckBox(char *varName, boolean checked)
/* Make check box if not suppressed. */
{
if (!suppressHtml)
    cgiMakeCheckBox(varName, checked);
}

void hDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list with names if not suppressed. */
{
if (!suppressHtml)
    cgiMakeDropList(name, menu, menuSize, checked);
}

void printHtmlComment(char *format, ...)
/* Function to print output as a comment so it is not seen in the HTML
 * output but only in the HTML source. */
{
va_list(args);
va_start(args, format);
hWrites("\n<!-- DEBUG: ");
hvPrintf(format, args);
hWrites(" -->\n");
fflush(stdout); /* USED ONLY FOR DEBUGGING BECAUSE THIS IS SLOW - MATT */
va_end(args);
}

void setPicWidth(char *s)
/* Set pixel width from ascii string. */
{
if (s != NULL && isdigit(s[0]))
    {
    tl.picWidth = atoi(s);
    if (tl.picWidth > 5000)
        tl.picWidth = 5000;
    if (tl.picWidth < 320)
        tl.picWidth = 320;
    }
tl.trackWidth = tl.picWidth - tl.leftLabelWidth;
}

void initTl()
/* Initialize layout around small font and a picture about 600 pixels
 * wide. */
{
MgFont *font;

font = tl.font = mgSmallFont();
tl.mWidth = mgFontStringWidth(font, "M");
tl.nWidth = mgFontStringWidth(font, "N");
tl.fontHeight = mgFontLineHeight(font);
tl.leftLabelWidth = hgDefaultLeftLabelWidth;
tl.picWidth = hgDefaultPixWidth;
setPicWidth(cartOptionalString(cart, "pix"));
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

Color shadesOfSea[10+1];       /* Ten sea shades. */
struct rgbColor darkSeaColor = {0, 60, 120};
struct rgbColor lightSeaColor = {200, 220, 255};

void loadSampleIntoLinkedFeature(struct track *tg);

struct track *trackList = NULL;    /* List of all tracks. */
struct group *groupList = NULL;    /* List of all tracks. */


/* Some little functional stubs to fill in track group
 * function pointers with if we have nothing to do. */
boolean tgLoadNothing(struct track *tg){return TRUE;}
void tgDrawNothing(struct track *tg){}
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

int tgFixedItemHeight(struct track *tg, void *item)
/* Return item height for fixed height track. */
{
return tg->lineHeight;
}

int packCountRows(struct track *tg, int maxCount, boolean withLabels)
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
	if (spaceSaverAdd(ss, start, end, item) == NULL)
	    break;
	}
    }
spaceSaverFinish(ss);
return ss->rowCount;
}

int tgFixedTotalHeight(struct track *tg, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height 
 * they use. */
{
int rows;
tg->heightPer = tl.fontHeight;
tg->lineHeight = tl.fontHeight+1;
switch (vis)
    {
    case tvFull:
	rows = slCount(tg->items);
	break;
    case tvPack:
	{
	rows = packCountRows(tg, maxItemsInFullTrack+1, TRUE);
	break;
	}
    case tvSquish:
        {
	tg->heightPer = tl.fontHeight/2;
	if ((tg->heightPer & 1) == 0)
	    tg->heightPer -= 1;
	tg->lineHeight = tg->heightPer + 1;
	rows = packCountRows(tg, 3*maxItemsInFullTrack+1, FALSE);
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


enum trackVisibility limitVisibility(struct track *tg)
/* Return default visibility limited by number of items. 
 * This also sets tg->height. */
{
if (!tg->limitedVisSet)
    {
    enum trackVisibility vis = tg->visibility;
    int h;
    int maxHeight = maxItemsInFullTrack * tl.fontHeight;
    tg->limitedVisSet = TRUE;
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
	if (vis == tvDense)
	    {    
	    if (tg->canPack)
		vis = tvPack;
	    else
		vis = tvFull;
	    }
	else if (vis == tvFull || vis == tvPack)
	    vis = tvDense;
	dyStringPrintf(dy, "&%s=%s", tg->mapName, hStringFromTv(vis));
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
hPrintf(" ALT=\"");
hvPrintf(format, args);
hPutc('"');
va_end(args);
}

static void mapBoxUi(int x, int y, int width, int height,
                                char *name, char *shortLabel)
/* Print out image map rectangle that invokes hgTrackUi. */
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s?%s=%u&c=%s&g=%s\"", hgTrackUiName(), cartSessionVarName(),
                         cartSessionId(cart), chromName, name);
mapStatusMessage("%s controls", shortLabel);
hPrintf(">\n");
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
	hgTracksName(), !cartUsualBoolean(cart,"complement",FALSE));
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
	char *track, char *item, char *statusLine, boolean doHgGene)
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
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, xEnd, yEnd);
    if (doHgGene)
        {
	hPrintf("HREF=\"../cgi-bin/hgGene?%s&%s=%s&%s=%s&%s=%s&%s=%d&%s=%d\" ",
		cartSidUrlString(cart),
		"db", database,
		"hgg_gene", item,
		"hgg_chrom", chromName,
		"hgg_start", start,
		"hgg_end", end);
	}
    else
	{
	hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
	    hgcNameAndSettings(), start, end, track, encodedItem, 
	    chromName, winStart, winEnd, 
	    database, tl.picWidth);
	}
    if (statusLine != NULL)
	mapStatusMessage("%s", statusLine);
    hPrintf(">\n");
    freeMem(encodedItem);
    }
}

void mapBoxHc(int start, int end, int x, int y, int width, int height, 
	char *track, char *item, char *statusLine)
/* Print out image map rectangle that would invoke the hgc (human genome click)
 * program. */
{
mapBoxHgcOrHgGene(start, end, x, y, width, height, track, item, statusLine, FALSE);
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
        z = colorBin[i][color] ;  /*pick color of highest scoreing alignment  for this pixel */
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
enum trackVisibility vis = 0;	/* suppress compiler warning. */

if (sameWord(filterType, "none"))
    return;

if (sameWord(filterType, "include"))
    exclude = FALSE;
else if (sameWord(filterType, "exclude"))
    exclude = TRUE;
else
    {
    color = TRUE;
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
    slFreeList(&lf->components);
slFreeList(pList);
}

void linkedFeaturesFreeItems(struct track *tg)
/* Free up linkedFeaturesTrack items. */
{
linkedFeaturesFreeList((struct linkedFeatures**)(&tg->items));
}

enum {blackShadeIx=9,whiteShadeIx=0};


char *linkedFeaturesSeriesName(struct track *tg, void *item)
/* Return name of item */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->name;
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

int vgFindRgb(struct vGfx *vg, struct rgbColor *rgb)
/* Find color index corresponding to rgb color. */
{
return vgFindColorIx(vg, rgb->r, rgb->g, rgb->b);
}

void makeGrayShades(struct vGfx *vg)
/* Make eight shades of gray in display. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    shadesOfGray[i] = vgFindRgb(vg, &rgb);
    }
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

Color  makeOrangeColor(struct vGfx *vg)
{
return vgFindColorIx(vg, 230, 130, 0);
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
int range = maxVal - minVal;
int level;
level = ((val-minVal)*maxShade + (range>>1))/range;
if (level <= 0) level = 1;
if (level > maxShade) level = maxShade;
return level;
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
				|| (tg->subType == lfSubChain);
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

void linkedFeaturesDrawAt(struct track *tg, void *item,
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct linkedFeatures *lf = item; 
struct simpleFeature *sf;
int heightPer = tg->heightPer;
int x1,x2;
int shortOff = 2, shortHeight = heightPer-4;
int tallStart, tallEnd, s, e, e2, s2;
Color bColor;
int intronGap = 0;
boolean chainLines = ((vis != tvDense)&&(tg->subType == lfSubChain));
boolean hideLine = ((tg->subType == lfSubChain) || 
	        ((vis == tvDense) && (tg->subType == lfSubXeno)));
int midY = y + (heightPer>>1);
int midY1 = midY - (heightPer>>2);
int midY2 = midY + (heightPer>>2);
int w;
boolean exonArrows = tg->exonArrows;

//variables for genePred cds coloring
struct psl *psl = NULL;
struct dnaSeq *mrnaSeq = NULL;
boolean foundStart = FALSE;
boolean *foundStartPtr = &foundStart;
int drawOptionNum = 0; //off
boolean errorColor = FALSE;

/*if we are zoomed in far enough, look to see if we are coloring
  by codon, and setup if so.*/
if (zoomedToCdsColorLevel && (vis != tvDense))
    drawOptionNum = cdsColorSetup(vg, tg, cdsColor, &mrnaSeq, &psl,
            &errorColor, lf, cdsColorsMade);

if ((tg->tdb != NULL) && (vis != tvDense))
    intronGap = atoi(trackDbSettingOrDefault(tg->tdb, "intronGap", "0"));

if (chainLines && (vis == tvSquish))
    {
    midY1 = y;
    midY2 = y + heightPer - 1;
    }
lfColors(tg, lf, vg, &color, &bColor);

tallStart = lf->tallStart;
tallEnd = lf->tallEnd;
if (tallStart == 0 && tallEnd == 0)
    {
    // sometimes a bed <8 will get passed off as a bed 8, tsk tsk
    tallStart = lf->start;
    tallEnd   = lf->end;
    }
if (!hideLine)
    {
    x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
    x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
    w = x2-x1;
    innerLine(vg, x1, midY, w, color);
    if ((intronGap == 0) && (vis == tvFull || vis == tvPack))
	{
	clippedBarbs(vg, x1, midY, w, 2, 5, 
		 lf->orientation, bColor, FALSE);
	}
    }

for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    s = sf->start; e = sf->end;

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
    if (e > s)
	{
        if (zoomedToCdsColorLevel && drawOptionNum>0 && vis != tvDense &&
            e + 6 >= winStart && s - 6 < winEnd && e-s <= 3) 
                drawCdsColoredBox(tg, lf, sf->grayIx, cdsColor, vg, xOff, y, 
                                    scale, font, s, e, heightPer, 
                                    zoomedToCodonLevel, mrnaSeq, psl, 
                                    drawOptionNum, errorColor, foundStartPtr,
                                    MAXPIXELS, winStart, color);
        else
            {
            drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
                                color, lf->score );

            if (exonArrows && vis != tvDense &&
                /* Display barbs only if no intron is visible on the item.
                   This occurs when the exon completely spans the window,
                   or when it is the first or last intron in the feature and
                   the following/preceding intron isn't visible */
                (sf->start <= winStart || sf->start == lf->start) &&
                (sf->end >= winEnd || sf->end == lf->end))
                    {
                    x1 = round((double)((int)s-winStart)*scale) + xOff;
                    x2 = round((double)((int)e-winStart)*scale) + xOff;
                    w = x2-x1;
                    clippedBarbs(vg, x1+1, midY, x2-x1-2, 2, 5, lf->orientation,
                                 MG_WHITE, TRUE);
                    }
            }
	}

    if ((intronGap || chainLines) && sf->next != NULL)
	{
	int qGap, tGap;
	if (sf->start >= sf->next->end)
	    {
	    tGap = sf->start - sf->next->end;
	    s = sf->next->end + 1;
	    e = sf->start;
	    }
	else
	    {
	    tGap = sf->next->start - sf->start;
	    s = sf->end;
	    e = sf->next->start;
	    }

	x1 = round((double)((int)s-winStart)*scale) + xOff;
	x2 = round((double)((int)e-winStart)*scale) + xOff;
	if (chainLines)
	    {
    /* The idea here is to draw one or two lines
     * based on whether the gap in the target
     * is similar to the gap in the query.
     * If the gap in the target is more than 
     * GAPFACTOR times the gap in the query
     * we draw only one line, otherwise two.
     */
	    x1--; /* this causes some lines to overwrite one
		     pixel of the previous box */
	    w = x2-x1;
	    w++; /* innerLine subtracts 1 from the width */
	    qGap = sf->next->qStart - sf->qEnd;
	    tGap = sf->next->start - sf->end;
#define GAPFACTOR 5
	    if (tGap > GAPFACTOR * qGap)
		innerLine(vg, x1, midY, w, color);
	    else
		{
		innerLine(vg, x1, midY1, w, color);
		innerLine(vg, x1, midY2, w, color);
		}
	    }
	else /* checking for intronGap */
	    {
	    w = x2-x1;
	    qGap = sf->qStart - sf->next->qEnd;
	    if ((qGap == 0) && (tGap >= intronGap))
		clippedBarbs(vg, x1, midY, w, 2, 5, 
			 lf->orientation, bColor, FALSE);
	    }
	}
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
	  clippedBarbs(vg, x1, midY, w, 2, 5, lfs->orientation, bColor, TRUE);
	vgLine(vg, x1, midY, x2, midY, color);
	}
    }
}
	

static void linkedFeaturesSeriesDrawAt(struct track *tg, void *item, 
        struct vGfx *vg, int xOff, int y, double scale,
	    MgFont *font, Color color, enum trackVisibility vis)
/* Draw a linked features series item at position. */
{
struct linkedFeaturesSeries *lfs = item;
struct linkedFeatures *lf;
Color bColor;
int midY = y + (tg->heightPer>>1);
int prevEnd = lfs->start;

if ((lf = lfs->features) == NULL)
    return;
lfColors(tg, lf, vg, &color, &bColor);
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

boolean trackWantsHgGene(struct track *tg)
/* Return TRUE if track wants hgGene on details page. */
{
char *hgGene = trackDbSetting(tg->tdb, "hgGene");
return hgGene != NULL && sameString(hgGene, "on");
}

boolean highlightItem(struct track *tg, void *item)
/* Should this item be highlighted? */
{
char *mapName = NULL;
char *name = NULL;
boolean highlight = FALSE;
mapName = tg->mapItemName(tg, item);
name = tg->mapItemName(tg, item);
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
boolean doHgGene = trackWantsHgGene(tg);

if (vis == tvPack || vis == tvSquish)
    {
    struct spaceSaver *ss = tg->ss;
    struct spaceNode *sn;
    vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
    assert(ss);
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
	if(tg->itemNameColor != NULL) 
	    color = tg->itemNameColor(tg, item, vg);

	y = yOff + lineHeight * sn->row;
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, color, vis);
	drawNameInverted = highlightItem(tg, item);
        if (withLabels)
            {
            int nameWidth = mgFontStringWidth(font, name);
            int dotWidth = tl.nWidth/2;
            textX -= nameWidth + dotWidth;
            if (textX < insideX)        /* Snap label to the left. */
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
        if (!tg->mapsSelf)
            {
            int w = x2-textX;
            if (w > 0)
                mapBoxHgcOrHgGene(s, e, textX, y, w, heightPer, tg->mapName, 
				  tg->mapItemName(tg, item), name, doHgGene);
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
	if (isFull) y += lineHeight;
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


static void linkedFeaturesDrawAverage(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense items doing color averaging items. */
{
int baseWidth = seqEnd - seqStart;
UBYTE *useCounts;
int lineHeight = mgFontLineHeight(font);
struct linkedFeatures *lf;
struct simpleFeature *sf;
int x1, x2, w;

AllocArray(useCounts, width);
memset(useCounts, 0, width * sizeof(useCounts[0]));
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	x1 = roundingScale(sf->start-winStart, width, baseWidth);
	if (x1 < 0)
	  x1 = 0;
	x2 = roundingScale(sf->end-winStart, width, baseWidth);
	if (x2 >= width)
	  x2 = width-1;
	w = x2-x1;
	if (w >= 0)
	  {
	  if (w == 0)
	     w = 1;
	  incRange(useCounts+x1, w); 
	  }
	}
    }
grayThreshold(useCounts, width);
vgVerticalSmear(vg,xOff,yOff,width,lineHeight,useCounts,TRUE);
freeMem(useCounts);
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
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesItemStart;
tg->itemEnd = linkedFeaturesItemEnd;
tg->itemNameColor = linkedFeaturesNameColor;
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
tg->totalHeight = tgFixedTotalHeight;
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

safef( optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", table);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0) 
    {
    char extraWhere[128];
    safef(extraWhere, sizeof(extraWhere), "score >= %d",optionScore);
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
tg->longLabel = "Your Sequence from BLAT Search";
tg->shortLabel = "BLAT Sequence";
tg->loadItems = loadUserPsl;
tg->mapItemName = lfMapNameFromExtra;
tg->priority = 100;
tg->groupName = "map";
tdb->tableName = tg->mapName;
tdb->shortLabel = tg->shortLabel;
tdb->longLabel = tg->longLabel;
tdb->type = cloneString("psl");
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
        
struct linkedFeatures *connectedLfFromGenePredInRange(
        struct track *tg, struct sqlConnection *conn, char *table, 
	char *chrom, int start, int end)
/* Return linked features from range of a gene prediction table after 
 * we have already connected to database. */
{
struct linkedFeatures *lfList = NULL, *lf;
int grayIx = maxShade;
struct genePredReader *gpr = NULL;
struct genePred *gp = NULL;

int drawOptionNum = 0; //off
if (table != NULL)
    drawOptionNum = getCdsDrawOptionNum(tg);

if (tg->itemAttrTbl != NULL)
    itemAttrTblLoad(tg->itemAttrTbl, conn, chrom, start, end);

gpr = genePredReaderRangeQuery(conn, table, chrom, start, end, NULL);
while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    AllocVar(lf);
    lf->grayIx = grayIx;
    strncpy(lf->name, gp->name, sizeof(lf->name));
    lf->orientation = orientFromChar(gp->strand[0]);

    if (drawOptionNum>0 && zoomedToCdsColorLevel && gp->cdsStart != gp->cdsEnd)
        lf->components = splitGenePredByCodon(chrom, lf, gp,NULL,
                gp->optFields >= genePredExonFramesFld);
    else
        lf->components = sfFromGenePred(gp, grayIx);

    if ((tg->itemAttrTbl != NULL) && (gp->optFields & genePredIdFld))
        lf->itemAttr = itemAttrTblGet(tg->itemAttrTbl, gp->id);

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

char *knownGeneName(struct track *tg, void *item)
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

char *knownGeneMapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void lookupKnownGeneNames(struct linkedFeatures *lfList)
/* This converts the known gene name to a gene name where possible via refLink. */
{
struct linkedFeatures *lf;
char query[256];
struct sqlConnection *conn = hAllocConn();
char *seqType;
char *refSeqName;
char *proteinID;
char *hugoID;
char cond_str[256];

if (hTableExists("refLink") && hTableExists("knownGeneLink"))
    {
    struct sqlResult *sr;
    char **row;

    for (lf = lfList; lf != NULL; lf = lf->next)
	{
        sprintf(cond_str, "name='%s' and seqType='g'", lf->name);
        seqType = sqlGetField(conn, database, "knownGeneLink", "seqType", cond_str);

        if (seqType != NULL)
            {
	    // special processing for RefSeq DNA based genes
    	    sprintf(cond_str, "mrnaAcc = '%s'", lf->name);
    	    refSeqName = sqlGetField(conn, database, "refLink", "name", cond_str);
	    if (refSeqName != NULL)
		{
		lf->extra = cloneString(refSeqName);
		}
	    }
	else if (protDbName != NULL)
	    {
	    sprintf(cond_str, "mrnaID='%s'", lf->name);
	    proteinID = sqlGetField(conn, database, "spMrna", "spID", cond_str);
 
            sprintf(cond_str, "displayID = '%s'", proteinID);
	    hugoID = sqlGetField(conn, protDbName, "spXref3", "hugoSymbol", cond_str);
	    if (!((hugoID == NULL) || (*hugoID == '\0')) )
		{
		lf->extra = cloneString(hugoID);
		}
	    else
	    	{
	    	sprintf(query,"select refseq from %s.mrnaRefseq where mrna = '%s';",  
		        database, lf->name);

	    	sr = sqlGetResult(conn, query);
	    	row = sqlNextRow(sr);
	    	if (row != NULL)
    	    	    {
    	    	    sprintf(query, "select * from refLink where mrnaAcc = '%s'", row[0]);
    	    	    sqlFreeResult(&sr);
    	    	    sr = sqlGetResult(conn, query); 
    	    	    if ((row = sqlNextRow(sr)) != NULL)
        	    	{
                        if (strlen(row[0]) > 0)
                            lf->extra = cloneString(row[0]);
		    	}
            	    sqlFreeResult(&sr);
	    	    }
	    	else
            	    {
	    	    sqlFreeResult(&sr);
	    	    }
	    	}
	    }
	}
    } 
hFreeConn(&conn);
}

void loadKnownGene(struct track *tg)
/* Load up known genes. */
{
enum trackVisibility vis = tg->visibility;
tg->items = lfFromGenePredInRange(tg, "knownGene", chromName, winStart, winEnd);
if (vis != tvDense)
    {
    lookupKnownGeneNames(tg->items);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
limitVisibility(tg);
}

Color knownGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw known gene in. */
{
struct linkedFeatures *lf = item;
int col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter, lightest;
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
			or has a corresponding "reviewed" RefSeq entry
	Lighter blue:  	If the gene has a corresponding "provisional" RefSeq entry
	Lightest blue: 	Eveything else
*/

// set default to the lightest color
lightest.r = (1*normal->r + 2*255) / 3;
lightest.g = (1*normal->g + 2*255) / 3;
lightest.b = (1*normal->b + 2*255) / 3;
col = vgFindColorIx(vg, lightest.r, lightest.g, lightest.b);

// set color first according to RefSeq status (if there is a corresponding RefSeq)
sprintf(cond_str, "mrna='%s' ", lf->name);
refAcc = sqlGetField(conn, database, "mrnaRefseq", "refseq", cond_str);
if (refAcc != NULL)
    {
    if (hTableExists("refSeqStatus"))
    	{
    	sprintf(query, "select status from refSeqStatus where mrnaAcc = '%s'", refAcc);
    	sr = sqlGetResult(conn, query);
    	if ((row = sqlNextRow(sr)) != NULL)
            {
	    if (startsWith("Reviewed", row[0]))
	    	{
	    	/* Use the usual color */
	    	col = tg->ixColor;
	    	}
	    else
		{ 
	    	if (startsWith("Provisional", row[0]))
	    	    {
	    	    lighter.r = (6*normal->r + 4*255) / 10;
	    	    lighter.g = (6*normal->g + 4*255) / 10;
	    	    lighter.b = (6*normal->b + 4*255) / 10;
	    	    col = vgFindColorIx(vg, lighter.r, lighter.g, lighter.b);
	    	    }
		}	   
	    }
	sqlFreeResult(&sr);
	}
    }

/* if a corresponding SWISS-PROT entry exists, set it
	    	if (startsWith("Provisional", row[0]))
	    	    {
	    	    lighter.r = (6*normal->r + 4*255) / 10;
	    	    lighter.g = (6*normal->g + 4*255) / 10;
	    	    lighter.b = (6*normal->b + 4*255) / 10;
	    	    col = vgFindColorIx(vg, lighter.r, lighter.g, lighter.b);
	    	    }
		}	   
	    }
	sqlFreeResult(&sr);
	} to dark blue */
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

void knownGeneMethods(struct track *tg)
/* Make track of known genes. */
{
tg->loadItems 	= loadKnownGene;
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
if (hTableExists("ensemblXref2"))
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
	    	proteinName = strdup("");
		}
	    }
	}
    }
	
name = strdup(proteinName);
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
if (hTableExists("ensemblXref2"))
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
	    	proteinName = strdup("");
		}
            }
        }
    }

name = strdup(proteinName);
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
int heightPer = tg->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w;

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
    	char *s = superfamilyNameLong(tg, item);
        vgTextRight(vg, x1-mgFontStringWidth(font, s)-2, y, mgFontStringWidth(font, s),
                heightPer, MG_BLACK, font, s);
        }

    if (tg->drawName && vis != tvSquish)
	{
	/* Clip here so that text will tend to be more visible... */
	char *s = tg->itemName(tg, bed);
	w = x2-x1;
	if (w > mgFontStringWidth(font, s))
	    {
	    Color textColor = contrastingColor(vg, color);
	    vgTextCentered(vg, x1, y, w, heightPer, textColor, font, s);
	    }
	mapBoxHc(bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
		tg->mapName, tg->mapItemName(tg, bed), NULL);
	}
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
	Color textColor = contrastingColor(vg, color);
	clippedBarbs(vg, x1, midY, w, 2, 5, dir, textColor, TRUE);
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
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight 	= tgFixedItemHeight;
tg->itemStart 	= superfamilyItemStart;
tg->itemEnd 	= superfamilyItemEnd;
tg->drawName 	= FALSE;
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
/* Return un-abbreviated genie name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void lookupRefNames(struct track *tg)
/* This converts the refSeq accession to a gene name where possible. */
{
struct linkedFeatures *lf;
char query[256];
struct sqlConnection *conn = hAllocConn();
boolean isNative = sameString(tg->mapName, "refGene");
char *refGeneLabel = cartUsualString(cart, (isNative ? "refGene.label" : "xenoRefGene.label"), "gene");
boolean useGeneName = sameString(refGeneLabel, "gene")
    || sameString(refGeneLabel, "both");
boolean useAcc = sameString(refGeneLabel, "accession")
    || sameString(refGeneLabel, "both");

if (hTableExists("refLink"))
    {
    struct sqlResult *sr;
    char **row;

    for (lf = tg->items; lf != NULL; lf = lf->next)
	{
	sprintf(query, "select name from refLink where mrnaAcc = '%s'", lf->name);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    {
            if (strlen(row[0]) > 0)
                {
                /* allow space for both */
                int size = strlen(row[0]) + strlen(lf->name) + 2;
                lf->extra = needMem(size);
                if (useGeneName)
                    strcat(lf->extra, row[0]);
                if (useGeneName && useAcc)
                    strcat(lf->extra, "/");
                if (useAcc)
                    strcat(lf->extra, lf->name);
                }
            else
                {
                /* no reflink, use name unless none is selected  */
                if (useGeneName || useAcc)
                    lf->extra = cloneString(lf->name);
                else
                    lf->extra = cloneString("");
                }
	    }
	sqlFreeResult(&sr);
	}
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

safef(geneName, sizeof(geneName), "%s.geneLabel", tg->tdb->tableName);
safef(accName, sizeof(accName), "%s.accLabel", tg->tdb->tableName);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tg->tdb->tableName);
safef(posName, sizeof(posName), "%s.posLabel", tg->tdb->tableName);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);
blastRef = trackDbSettingOrDefault(tg->tdb, "blastRef", NULL);

if ((blastRef != NULL) && (hTableExists(blastRef)))
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
	if (ptr = strchr(buffer, '.'))
	    *ptr = 0;
	if (!sameString("blastDm1FB", tg->tdb->tableName))
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
		if (added)
		    strcat(lf->extra, "/");
		added = TRUE;
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
    return ;
    }

for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    char *acc, *prot = NULL;
    char *gene = NULL, *pos = NULL;
    char *buffer;
    boolean added = FALSE;

    lf->extra = needMem(strlen(lf->name));
    acc = buffer = cloneString(lf->name);
    if ((pos = strchr(acc, '.')) != NULL)
	{
	*pos++ = 0;
	if ((gene = strchr(pos, '.')) != NULL)
	    {
	    *gene++ = 0;
	    if ((prot = strchr(gene, '.')) != NULL)
		*prot++ = 0;
	    }
	}

    if (useGene && (gene != NULL))
	{
	added = TRUE;
	strcat(lf->extra, gene);
	}
    if (useAcc && (acc != NULL))
	{
	if (added)
	    strcat(lf->extra, "/");
	added = TRUE;
	strcat(lf->extra, acc);
	}
    if (useSprot && (prot != NULL))
	{
	if (added)
	    strcat(lf->extra, "/");
	strcat(lf->extra, prot);
	}
    if (usePos && (pos != NULL))
	{
	char *startPos = strchr(pos, ':');
	char *dash = strchr(pos, '-');

	if ((startPos != NULL) && (dash != NULL))
	    {
	    *startPos++ = 0;
	    dash -= 3; /* divide by 1000 */
	    *dash = 0;
	    if (added)
		strcat(lf->extra, "/");
	    strcat(lf->extra, pos);
	    strcat(lf->extra, " ");
	    strcat(lf->extra, startPos);
	    strcat(lf->extra, "k");
	    }
	}
    }
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

if (getCdsDrawOptionNum(tg)>0 && zoomedToCdsColorLevel)
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
	if ((blastRef != NULL) && hTableExists(blastRef))
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
		    col = getChromColor(row[0]+3, vg);
		    }
		}
	    sqlFreeResult(&sr);
	    hFreeConn(&conn);
	    }
	else
	    {
	    if ((pos = strchr(acc, '.')) != NULL)
		{
		pos +=4;
		if ((colon = strchr(pos, ':')) != NULL)
		    {
		    *colon = 0;
		    col = getChromColor(pos, vg);
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

Color refGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw refseq gene in. */
{
struct linkedFeatures *lf = item;
int col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter, lightest;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

/* If refSeqStatus is available, use it to determine the color.
 * Reviewed, Validated -> normal, Provisional -> lighter, 
 * Predicted, Inferred(other) -> lightest 
 * If no refSeqStatus, color it normally. 
 */
if (hTableExists("refSeqStatus"))
    {
    sprintf(query, "select status from refSeqStatus where mrnaAcc = '%s'",
	    lf->name);
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
    }
hFreeConn(&conn);
return(col);
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
sprintf(query, "select direction from mrna where acc='%s'", acc);
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
	estOrient = hashIntValDefault(orientHash, lf->name, BIGNUM); 
	if(estOrient < 0) 
	    lf->orientation = -1 * lf->orientation;
	else if(estOrient == BIGNUM)
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

void estMethods(struct track *tg)
/* Make track of EST methods - overrides color handler. */
{
tg->drawItems = linkedFeaturesAverageDenseOrientEst;
tg->extraUiData = newMrnaUiData(tg->mapName, FALSE);
}

void mrnaMethods(struct track *tg)
/* Make track of mRNA methods. */
{
tg->extraUiData = newMrnaUiData(tg->mapName, FALSE);
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

/* If vegaInfo is available, use it to determine the color:
 *  Known - black
 *  {Novel_CDS, Novel_Transcript} - dark blue
 *  {Putative,Ig_Segment} - medium blue
 *  {Predicted_gene,Pseudogene,Ig_Pseudogene_Segment} - light blue
 *  None of the above - gray
 * If no vegaGene, color it normally. 
 */
if (hTableExists("vegaInfo"))
    {
    sprintf(query, "select method from vegaInfo where transcriptId = '%s'",
	    lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	if (sameWord("Known", row[0]))
	    {
	    col = blackIndex();
	    }
	else if (sameWord("Novel_CDS", row[0]) ||
		 sameWord("Novel_Transcript", row[0]))
	    {
	    /* Use the usual color (dark blue) */
	    }
	else if (sameWord("Putative", row[0]) ||
		 sameWord("Ig_Segment", row[0]))
	    {
	    lighter.r = (6*normal->r + 4*255) / 10;
	    lighter.g = (6*normal->g + 4*255) / 10;
	    lighter.b = (6*normal->b + 4*255) / 10;
	    col = vgFindRgb(vg, &lighter);
	    }
	else if (sameWord("Predicted_gene", row[0]) ||
		 sameWord("Pseudogene", row[0]) ||
		 sameWord("Ig_Pseudogene_Segment", row[0]))
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

void vegaMethods(struct track *tg)
/* Special handling for vegaGene/vegaPseudoGene items. */
{
tg->itemColor = vegaColor;
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

Color contrastingColor(struct vGfx *vg, int backgroundIx)
/* Return black or white whichever would be more visible over
 * background. */
{
struct rgbColor c = vgColorIxToRgb(vg, backgroundIx);
int val = (int)c.r + c.g + c.g + c.b;
if (val > 512)
    return MG_BLACK;
else
    return MG_WHITE;
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
	/* Clip here so that text will tend to be more visible... */
	char *s = tg->itemName(tg, bed);
	w = x2-x1;
	if (w > mgFontStringWidth(font, s))
	    {
	    Color textColor = contrastingColor(vg, color);
	    vgTextCentered(vg, x1, y, w, heightPer, textColor, font, s);
	    }
	mapBoxHc(bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
		tg->mapName, tg->mapItemName(tg, bed), NULL);
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
	Color textColor = contrastingColor(vg, color);
	clippedBarbs(vg, x1, midY, w, 2, 5, dir, textColor, TRUE);
	}
    }
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
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = bedItemStart;
tg->itemEnd = bedItemEnd;
tg->freeItems = freeSimpleBed;
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

Color genomicSuperDupsColor(struct track *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
struct genomicSuperDups *dup = item;
int ppt = dup->score;
char *verdict=dup->verdict;


if ((verdict[0]=='B')&&(verdict[1]=='A')&&(verdict[2]=='D'))
    return vgFindColorIx(vg, 255,51,51);
return dupPptColor(ppt, vg);
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
int shortOff = 2, shortHeight = heightPer-4;
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
if (sameString(chromName, "chr22") && hIsPrivateHost())
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
char query[256];
char organism[256], *org;
sprintf(query, "select organism.name from mrna,organism where mrna.acc = '%s' and mrna.organism = organism.id", name);
org = sqlQuickQuery(conn, query, organism, sizeof(organism));
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
}

void xenoRefGeneMethods(struct track *tg)
/* Make track of known genes from xenoRefSeq. */
{
tg->loadItems = loadRefGene;
tg->itemName = xenoMrnaName;
tg->mapItemName = refGeneMapName;
tg->itemColor = refGeneColor;
}

boolean isNonChromColor(Color color)
/* test if color is a non-chrom color (black or gray) */
{
return color == chromColor[0];
}

Color getChromColor(char *name, struct vGfx *vg)
/* Return color index corresponding to chromosome name. */
{
int chromNum = 0;
Color colorNum = 0;
if(!chromosomeColorsMade)
    makeChromosomeShades(vg);
if (atoi(name) != 0)
    chromNum =  atoi(name);
else if (!strcmp(name,"X"))
    chromNum = 23;
else if (!strcmp(name,"X "))
    chromNum = 23;
else if (!strcmp(name,"Y"))
    chromNum = 24;
else if (!strcmp(name,"Y "))
    chromNum = 24;
else if (!strcmp(name,"M"))
    chromNum = 25;
else if (!strcmp(name,"M "))
    chromNum = 25;
else if (!strcmp(name,"Un"))
    chromNum = 26;
else if (!strcmp(name,"Un "))
    chromNum = 26;
else if (!strcmp(name,"I"))
    chromNum = 1;
else if (!strcmp(name,"I "))
    chromNum = 1;
else if (!strcmp(name,"II"))
    chromNum = 2;
else if (!strcmp(name,"II "))
    chromNum = 2;
else if (!strcmp(name,"III"))
    chromNum = 3;
else if (!strcmp(name,"III "))
    chromNum = 3;
else if (!strcmp(name,"IV"))
    chromNum = 4;
else if (!strcmp(name,"IV "))
    chromNum = 4;
else if (!strcmp(name,"V"))
    chromNum = 5;
else if (!strcmp(name,"V "))
    chromNum = 5;
if (chromNum > CHROM_COLORS) chromNum = 0;
colorNum = chromColor[chromNum];
return colorNum;
}

Color getScaffoldColor(char *scaffoldNumber, struct vGfx *vg)
/* assign fake chrom color to scaffold, based on number */
{
    int chromNum;
    chromNum = atoi(scaffoldNumber) % CHROM_COLORS;
    if (chromNum < 0 || chromNum > CHROM_COLORS)
        chromNum = 0;
    return chromColor[chromNum];
}

Color lfChromColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of chromosome for linked feature type items
 * where the chromosome is listed somewhere in the lf->name. */
{
struct linkedFeatures *lf = item;
char *chptr = strstr(lf->name, "chr");

if (chptr == NULL)
    {
    return(tg->ixColor);
    }
else
    {
    char chromStr[3];
    Color c;
    strncpy(chromStr, (char *)(chptr+3), 2);
    chromStr[2] = '\0';
    c = getChromColor(chromStr, vg);
    tg->ixAltColor = c;
    return c;
    }
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
if (stsMapMouseFilterItem(tg, item))
    return stsMapMouseFilterColor;
else
    {
    struct stsMapMouseNew *el = item;
    if (el->score >= 900)
        return MG_BLACK;
    else
        return MG_GRAY;
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
 * where the chromosome is listed somewhere in the bed->name. */
{
struct bed *bed = item;
char *chptr = strstr(bed->name, "chr");

if (chptr == NULL)
    {
    return(tg->ixColor);
    }
else
    {
    char chromStr[3];
    Color c;
    strncpy(chromStr, (char *)(chptr+3), 2);
    chromStr[2] = '\0';
    c = getChromColor(chromStr, vg);
    tg->ixAltColor = c;
    return c;
    }
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
char *ptr;
char chromStr[20];

if ((ptr = strstr(epb->name, "chr")) != NULL)
    {
    strncpy(chromStr, ptr+strlen("chr"), 2);
    chromStr[2] = '\0';
    return((Color)getChromColor(chromStr, vg));
    }
else
    {
    return(tg->ixColor);
    }
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

#ifdef OLD
void mapBoxHcWTarget(int start, int end, int x, int y, int width, int height, 
	char *track, char *item, char *statusLine, boolean target, char *otherFrame)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem = cgiEncode(item);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
    hgcNameAndSettings(), start, end, track, encodedItem, chromName, winStart, winEnd, 
    database, tl.picWidth);
if(target) 
    {
    hPrintf(" target=\"%s\" ", otherFrame);
    } 
hPrintf("ALT=\"%s\" TITLE=\"%s\">\n", statusLine, statusLine); 
freeMem(encodedItem);
}
#endif /* OLD */


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
tg->totalHeight = tgFixedTotalHeight;
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

Color alignInsertsColor()
/* Return color used for insert indicators in multiple alignments */
{
    return orangeColor;
}

int spreadStringCharWidth(int width, int count)
{
    return width/count;
}

void spreadAlignString(struct vGfx *vg, int x, int y, int width, int height,
		       Color color, MgFont *font, char *text, 
		       char *match, int count)
/* Draw evenly spaced letters in string.  For multiple alignments,
 * supply a non-NULL match string, and then matching letters will be colored
 * with the main color, mismatched letters will have alt color. 
 * Draw a vertical bar in orange where sequence lacks gaps that
 * are in reference sequence (possible insertion) -- this is indicated
 * by an escaped insert count in the sequence.  The escape char is backslash.
 * The count param is the number of bases to print, not length of
 * the input line (text) */
{
char c[2] = "";
int i,j,textPos=0;
int x1, x2;
char *motifString = cartOptionalString(cart,"hgt.motifs");
char **motifs = NULL;
boolean *inMotif = NULL;
int motifCount = 0;
Color noMatchColor = lighterColor(vg, color);
Color clr;
int textLength = strlen(text);

/* If we have motifs, look for them in the string. */
if(motifString != NULL && strlen(motifString) != 0)
    {
    touppers(motifString);
    motifString = cloneString(motifString);
    motifCount = chopString(motifString, ",", NULL, 0);
    AllocArray(motifs, motifCount);
    chopString(motifString, ",", motifs, motifCount);
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
    }

for (i=0; i<count; i++, text++, textPos++)
    {
    x1 = i * width / count;
    x2 = (i+1) * width/count;
    if (*text == '|')
        {
        /* insert count follows -- replace with a colored vertical bar */
        text++;
	textPos++;
        i--;
        vgBox(vg, x1+x, y, 1, height, alignInsertsColor());
        continue;
        }
    c[0] = *text;
    clr = color;
    if (match != NULL && match[i])
        if (*text != match[i])
            clr = noMatchColor;
    if(inMotif != NULL && textPos < textLength && inMotif[textPos])
	{
	vgBox(vg, x1+x, y, x2-x1, height, clr);
	vgTextCentered(vg, x1+x, y, x2-x1, height, MG_WHITE, font, c);
	}
    else
	vgTextCentered(vg, x1+x, y, x2-x1, height, clr, font, c);
    }
freez(&inMotif);
}

void spreadString(struct vGfx *vg, int x, int y, int width, int height,
	                Color color, MgFont *font, char *s, int count)
/* Draw evenly spaced letters in string. */
{
spreadAlignString(vg, x, y, width, height, color, font, s, NULL, count);
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
spreadString(vg, x, y, width, height, color, font, seq->dna, seq->size);

if (thisSeq == NULL)
    freeDnaSeq(&seq);
}

void drawComplementArrow( struct vGfx *vg, int x, int y, 
                                int width, int height, MgFont *font)
/* Draw arrow and create clickbox for complementing ruler bases */
{
if(cartUsualBoolean(cart, "complement", FALSE))
    vgTextRight(vg, x, y, width, height, MG_GRAY, font, "<---");
else
    vgTextRight(vg, x, y, width, height, MG_BLACK, font, "--->");
mapBoxToggleComplement(x, y, width, height, NULL, chromName, winStart, winEnd,
                                "complement bases");
}

void makeChromIdeoImage(struct track **pTrackList, char *psOutput)
/* Make an ideogram image of the chromsome and our position in
   it. */
{
struct track *track = NULL, *ideoTrack = NULL;
MgFont *font = tl.font;
char *mapName = "ideoMap";
struct vGfx *vg;
struct tempName gifTn;
boolean doIdeo = TRUE;
int ideoWidth = round(.65 *tl.picWidth);
int ideoHeight = 0;
int textWidth = 0;
/* Find the ideogram track. */
for(track = *pTrackList; track != NULL; track = track->next)
    {
    if(sameString(track->mapName, "cytoBandIdeo"))
	{
	ideoTrack = track;
	break;
	}
    }
/* If no ideogram don't draw. */
if(ideoTrack == NULL)
    doIdeo = FALSE;
else
    {
    struct trackRef *tr = NULL;
    /* Find and remove the track from the group and track list. */
    ideogramAvail = TRUE;
    for(tr = ideoTrack->group->trackList; tr != NULL; tr = tr->next)
	{
	if(tr->track == ideoTrack)
	    break;
	}
    slRemoveEl(&ideoTrack->group->trackList, tr);
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
    /* Draw the ideogram. */
    makeTempName(&gifTn, "hgtIdeo", ".gif");
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
    textWidth = mgFontStringWidth(font, chromName);
    vgTextCentered(vg, 2, gfxBorder, textWidth, ideoTrack->height, MG_BLACK, font, chromName);
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

void makeActiveImage(struct track *trackList, char *psOutput)
/* Make image and image map. */
{
struct track *track;
MgFont *font = tl.font;
struct vGfx *vg;
struct tempName gifTn;
char *mapName = "map";
int fontHeight = mgFontLineHeight(font);
int insideHeight = fontHeight-1;
int trackTabX = gfxBorder;
int trackTabWidth = 11;
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackPastTabWidth = tl.picWidth - trackPastTabX;
int pixWidth, pixHeight;
int y;
int rulerHeight = fontHeight;
int baseHeight = fontHeight;
int basePositionHeight = rulerHeight;
int codonHeight = fontHeight;
int rulerTranslationHeight = codonHeight * 3;        // 3 frames
int yAfterRuler = gfxBorder;
int yAfterBases = yAfterRuler;  // differs if base-level translation shown
int relNumOff;
int ymin, ymax;
double minRange, maxRange;
char minRangeStr[32];
char maxRangeStr[32];
int start;
int newy;
/* Figure out dimensions and allocate drawing space. */
pixWidth = tl.picWidth;

/* Figure out height of each visible track. */
pixHeight = gfxBorder;
if (rulerMode != RULER_MODE_OFF)
    {
    if (zoomedToBaseLevel)
	basePositionHeight += baseHeight;
    yAfterRuler += basePositionHeight;
    yAfterBases = yAfterRuler;
    pixHeight += basePositionHeight;
    if (rulerMode == RULER_MODE_FULL && 
            (zoomedToBaseLevel || zoomedToCdsColorLevel))
        {
        yAfterRuler += rulerTranslationHeight;
        pixHeight += rulerTranslationHeight;
        }
    }

trackHash = newHash(8);
for (track = trackList; track != NULL; track = track->next)
    {
    hashAddUnique(trackHash, track->mapName, track);
    if (track->visibility != tvHide)
	{
	int h;
	if (withCenterLabels)
	    pixHeight += fontHeight;
	limitVisibility(track);
	h = track->height;
	pixHeight += h;
	}
    else
        track->limitedVis = tvHide;
    }

imagePixelHeight = pixHeight;
if (psOutput)
    vg = vgOpenPostScript(pixWidth, pixHeight, psOutput);
else
    {
    makeTempName(&gifTn, "hgt", ".gif");
    vg = vgOpenGif(pixWidth, pixHeight, gifTn.forCgi);
    }
makeGrayShades(vg);
makeBrownShades(vg);
makeSeaShades(vg);
orangeColor = makeOrangeColor(vg);

if (rulerMode == RULER_MODE_FULL &&
        (zoomedToBaseLevel || zoomedToCdsColorLevel) && !cdsColorsMade)
    {
    makeCdsShades(vg, cdsColor);
    cdsColorsMade = TRUE;
    }

/* Start up client side map. */
hPrintf("<MAP Name=%s>\n", mapName);

/* Find colors to draw in. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->limitedVis != tvHide)
	{
	track->ixColor = vgFindRgb(vg, &track->color);
	track->ixAltColor = vgFindRgb(vg, &track->altColor);
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
    if (rulerMode != RULER_MODE_OFF)
        {
        /* draw button for Base Position pseudo-track */
        int height = basePositionHeight;
        if (rulerMode == RULER_MODE_FULL && 
                        (zoomedToBaseLevel || zoomedToCdsColorLevel))
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
	    y += track->height;
	    if (withCenterLabels)
		y += fontHeight;
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
    double min0, max0;
    Color lightRed = vgFindColorIx(vg, 255, 180, 180);

    vgBox(vg, leftLabelX + leftLabelWidth, 0,
    	gfxBorder, pixHeight, lightRed);
    y = gfxBorder;
    if (rulerMode != RULER_MODE_OFF)
	{
	vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, rulerHeight, 
		    MG_BLACK, font, RULER_TRACK_LABEL);
	if (zoomedToBaseLevel || 
                (zoomedToCdsColorLevel && rulerMode == RULER_MODE_FULL))
	    drawComplementArrow(vg,leftLabelX, y+rulerHeight,
				leftLabelWidth-1, baseHeight, font);
	y += basePositionHeight;
        if ((rulerMode == RULER_MODE_FULL) &&
                (zoomedToBaseLevel || zoomedToCdsColorLevel))
            y += rulerTranslationHeight;
	}
    for (track = trackList; track != NULL; track = track->next)
        {
	struct slList *prev = NULL;
	double minRangeCutoff, maxRangeCutoff;
	char o4[128];
	char o5[128];
	struct slList *item;
	enum trackVisibility vis = track->limitedVis;
        enum trackVisibility savedVis = vis;
	int tHeight;
	Color labelColor = (track->labelColor ? 
                                track->labelColor : track->ixColor);
	if (vis == tvHide)
	    continue;
	tHeight = track->height;
	if (withCenterLabels)
	    tHeight += fontHeight;
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
	/*  if a track can do its own left labels, do them after drawItems */
	if (track->drawLeftLabels != NULL)
	    {
	    if (withCenterLabels)
		y += fontHeight;
	    y += track->height;
	    vgUnclip(vg);
	    continue;
	    }
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
           labelling, temporarily set visibility to full.
           Restore savedVis later */
        {
        if (sameString(track->tdb->type, "wigMaf"))
            vis = tvFull;
        }
	switch (vis)
	    {
	    case tvHide:
		break;	/* Do nothing; */
	    case tvPack:
	    case tvSquish:
		if (withCenterLabels)
 		    y += fontHeight;
	        y += track->height;
		break;
	    case tvFull:
		if (withCenterLabels)
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
		    
		    /* Do some fancy stuff for sample tracks. 
		     * Draw y-value limits for 'sample' tracks. */
		    if(track->subType == lfSubSample )
			{
			
			if( prev == NULL )
			    newy += itemHeight;
			else
			    newy += sampleUpdateY( name, track->itemName(track, prev), itemHeight );
			if( newy == y )
			    continue;

			if( track->heightPer > (3 * fontHeight ) )
			    {
			    ymax = y - (track->heightPer / 2) + (fontHeight / 2);
			    ymin = y + (track->heightPer / 2) - (fontHeight / 2);
			    vgTextRight(vg, leftLabelX, ymin, leftLabelWidth-1, itemHeight,
					track->ixAltColor, font, minRangeStr );
			    vgTextRight(vg, leftLabelX, ymax, leftLabelWidth-1, itemHeight,
					track->ixAltColor, font, maxRangeStr );
			    }
			prev = item;

			rootName = cloneString( name );
			beforeFirstPeriod( rootName );
			if( sameString( track->mapName, "humMusL" ) || 
				 sameString( track->mapName, "hg15Mm3L" ))
			    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, itemHeight,
					track->ixColor, font, "Mouse Cons");
			else if( sameString( track->mapName, "musHumL" ) ||
				 sameString( track->mapName, "mm3Hg15L"))
			    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, itemHeight,
					track->ixColor, font, "Human Cons");
			else if( sameString( track->mapName, "mm3Rn2L" ))
			    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, itemHeight,
					track->ixColor, font, "Rat Cons");
			else
			    vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, itemHeight,
					track->ixColor, font, rootName );

			freeMem( rootName );
			start = 0;
			y = newy;
			}
		    else
			{
			vgTextRight(vg, leftLabelX, y, leftLabelWidth - 1, 
				itemHeight, labelColor, font, name);
			y += itemHeight;
			}
		    }
		break;
	    case tvDense:
		
		if (withCenterLabels)
		    y += fontHeight;
		
		/*draw y-value limits for 'sample' tracks. (always puts 0-100% range)*/
		if( track->subType == lfSubSample && track->heightPer > (3 * fontHeight ) )
		    {
		    ymax = y - (track->heightPer / 2) + (fontHeight / 2);
		    ymin = y + (track->heightPer / 2) - (fontHeight / 2);
		    vgTextRight(vg, leftLabelX, ymin, leftLabelWidth-1, track->lineHeight, 
				track->ixAltColor, font, minRangeStr );
		    vgTextRight(vg, leftLabelX, ymax, leftLabelWidth-1, track->lineHeight, 
				track->ixAltColor, font, maxRangeStr );
		    }
		vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, track->lineHeight, 
			    labelColor, font, track->shortLabel);
		y += track->height;
		break;
	    }
        /* NOTE: might want to just restore savedVis here for all track types,
           but I'm being cautious... */
        if (sameString(track->tdb->type, "wigMaf"))
            vis = savedVis;
	vgUnclip(vg);
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
if (rulerMode != RULER_MODE_OFF)
    {
    struct dnaSeq *seq = NULL;
    y = 0;
    vgSetClip(vg, insideX, y, insideWidth, yAfterRuler-y+1);
    relNumOff = winStart;
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
	mapBoxJumpTo(ps+insideX,y,pe-ps,rulerHeight,
		        chromName, ns, ne, message);
	}
    }
    if (zoomedToBaseLevel || 
            (zoomedToCdsColorLevel && rulerMode == RULER_MODE_FULL))
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
        if (start != winStart - 3 || winEnd != winEnd + 3)
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
        drawBases(vg, insideX, y+rulerHeight, insideWidth, baseHeight, 
		  baseColor, font, complementRulerBases, seq);

        /* set up clickable area to toggle ruler visibility */
            {
            char newRulerVis[100];
            safef(newRulerVis, 100, "%s=%s", RULER_TRACK_NAME,
                         rulerMode == RULER_MODE_FULL ?  
                                rulerMenu[RULER_MODE_ON] : 
                                rulerMenu[RULER_MODE_FULL]);
            mapBoxReinvokeExtra(insideX, y+rulerHeight, insideWidth,baseHeight, 
                                NULL, NULL, 0, 0, "", newRulerVis);
            }
        if (rulerMode == RULER_MODE_FULL && 
                                (zoomedToBaseLevel || zoomedToCdsColorLevel))
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
                sfList = splitDnaByCodon(refFrame, winStart, winEnd,
                                             extraSeq, complementRulerBases); 
                /* draw the codons in the list, with alternating colors */
                drawGenomicCodons(vg, sfList, scale, insideX, y, codonHeight,
                                    font, cdsColor, winStart, MAXPIXELS,
                                    zoomedToBaseLevel || zoomedToCodonLevel);
                }
            }
        }
    vgUnclip(vg);
    }


/* Draw center labels. */
if (withCenterLabels)
    {
    int clWidth = insideWidth;
    vgSetClip(vg, insideX, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);
    y = yAfterRuler;
    for (track = trackList; track != NULL; track = track->next)
        {
	if (track->limitedVis != tvHide)
	    {
	    Color labelColor = (track->labelColor ? 
                                track->labelColor : track->ixColor);
	    vgTextCentered(vg, insideX, y+1, clWidth, insideHeight, 
                                labelColor, font, track->longLabel);
	    mapBoxToggleVis(trackPastTabX, y+1, 
			    trackPastTabWidth, insideHeight, track);
	    y += fontHeight;
	    y += track->height;
	    }
        }
    vgUnclip(vg);
    }


/* Draw tracks. */
{
    y = yAfterRuler;
    for (track = trackList; track != NULL; track = track->next)
	{
	if (track->limitedVis != tvHide)
	    {
	    if (withCenterLabels)
		y += fontHeight;
	    if (track->limitedVis == tvPack)
		{
		vgSetClip(vg, gfxBorder+trackTabWidth+1, y, 
		    pixWidth-2*gfxBorder-trackTabWidth-1, track->height);
		}
	    else
		vgSetClip(vg, insideX, y, insideWidth, track->height);
	    track->drawItems(track, winStart, winEnd,
			     vg, insideX, y, insideWidth, 
			     font, track->ixColor, track->limitedVis);
	    vgUnclip(vg);
	    y += track->height;
	    }
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
	if (track->limitedVis != tvHide)
	    {
	    if (track->drawLeftLabels != NULL)
		{
		int tHeight = track->height;
		Color labelColor = (track->labelColor ? track->labelColor : track->ixColor);
		if (withCenterLabels)
		    tHeight += fontHeight;
		if (track->limitedVis == tvPack)
		    { /*XXX This needs to be looked at, no example yet*/
		    vgSetClip(vg, gfxBorder+trackTabWidth+1, y, 
			      pixWidth-2*gfxBorder-trackTabWidth-1, track->height);
		    }
		else
		    {
		    vgSetClip(vg, leftLabelX, y, leftLabelWidth, tHeight);
		    
		    /* when the limitedVis == tvPack is correct above,
		     *	this should be outside this else clause
		     */
		    track->drawLeftLabels(track, winStart, winEnd,
					  vg, leftLabelX, y, leftLabelWidth, tHeight,
					  withCenterLabels, font, labelColor, 
					  track->limitedVis);
		    }
		vgUnclip(vg);
		}
	    if (withCenterLabels)
		y += fontHeight;
	    y += track->height;
	    }
	}
    }

/* Make map background. */
y = yAfterRuler;
for (track = trackList; track != NULL; track = track->next)
    {
    struct slList *item;
    boolean doHgGene = FALSE;
    switch (track->limitedVis)
	{
	case tvHide:
	    break;	/* Do nothing; */
	case tvPack:
	case tvSquish:
	    if (withCenterLabels)
		y += fontHeight;
	    y += track->height;
	    break;
	case tvFull:
	    doHgGene = trackWantsHgGene(track);
	    if (withCenterLabels)
		y += fontHeight;
	    start = 1;
	    if( track->subType == lfSubSample && track->items == NULL )
		 y += track->lineHeight;

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
                        mapBoxHgcOrHgGene(track->itemStart(track, item), track->itemEnd(track, item),
				 trackPastTabX,y,trackPastTabWidth,height, track->mapName,
				 track->mapItemName(track, item),
				 track->itemName(track, item), doHgGene);
			}
		    y += height;
		    }
		}
	    break;
	case tvDense:
	    if (withCenterLabels)
		y += fontHeight;
	    mapBoxToggleVis(trackPastTabX,y,trackPastTabWidth,track->lineHeight,track);
	    y += track->height;
    
	    break;
	}
    }
hashFree(&trackHash);
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
char *scaffoldName;
char ensemblChrScaffoldName[64];
int start, end;

if (sameWord(scientificName, "Takifugu rubripes"))
    {
    /* for Fugu, must give scaffold, not chr coordinates */
    /* Also, must give "chrom" as "Chr_scaffold_N" */
    if (!hScaffoldPos(chromName, winStart, winEnd,
                        &scaffoldName, &start, &end))
        /* position doesn't appear on Ensembl browser.
         * Ensembl doesn't show scaffolds < 2K */
        return;
    strcpy(ensemblChrScaffoldName, "Chr_");
    strncat(ensemblChrScaffoldName, scaffoldName, 60);
    name = ensemblChrScaffoldName;
    }
else
    {
    name = chromName;
    start = winStart;
    end = winEnd;
    }
ensUrl = ensContigViewUrl(dir, name, seqBaseCount, start, end);
hPrintf("<A HREF=\"%s\" TARGET=_blank>", ensUrl->string);
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
    {
    hashAdd(handlerHash, name, handler);
    }
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
/* Load the items in one custom track - just move beds in
 * window... */
{
struct bed *(*loader)(char **row);
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
char optionScoreStr[128]; /* Option -  score filter */
int optionScore;
char extraWhere[128] ;

if (tg->bedSize <= 3)
    loader = bedLoad3;
else if (tg->bedSize == 4)
    loader = bedLoad;
else if (tg->bedSize == 5)
    loader = bedLoad5;
else
    loader = bedLoad6;
safef( optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", tg->mapName);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0) 
    {
    safef(extraWhere, sizeof(extraWhere), "score >= %d",optionScore);
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, extraWhere, &rowOffset);
    }
else
    {
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = loader(row+rowOffset);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
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

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 8);
    bed8To12(bed);
    lf = lfFromBedExtra(bed, scoreMin, scoreMax);
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

safef( optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", tg->mapName);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0) 
    {
    safef(extraWhere, sizeof(extraWhere), "score >= %d",optionScore);
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

void blastMethods(struct track *tg)
/* blast protein track methods */
{
tg->loadItems = loadBlast;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = blastColor;
tg->itemNameColor = blastNameColor;
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
int y2 = y + tg->heightPer;
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

if (bed->strand[0] == '-')
    vgTriLeft(vg, x1, y, y2, color);
else
    vgTriRight(vg, x1, y, y2, color);
}

void simpleBedTriangleMethods(struct track *tg)
/* Load up simple bed features methods, but use triangleDrawAt. */
{
bedMethods(tg);
tg->drawItemAt = triangleDrawAt;
}


void loadGenePred(struct track *tg)
/* Convert bed info in window to linked feature. */
{
tg->items = lfFromGenePredInRange(tg, tg->mapName, chromName, winStart, winEnd);
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


void fillInFromType(struct track *track, struct trackDb *tdb)
/* Fill in various function pointers in track from type field of tdb. */
{
char *typeLine = tdb->type, *words[8], *type;
int wordCount;
if (typeLine == NULL)
    return;
wordCount = chopLine(typeLine, words);
if (wordCount <= 0)
    return;
type = words[0];

if (sameWord(type, "bed"))
    {
    int fieldCount = 3;
    if (wordCount > 1)
        fieldCount = atoi(words[1]);
    track->bedSize = fieldCount;
    if (fieldCount < 8)
	{
	bedMethods(track);
	track->loadItems = loadSimpleBed;
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
else if (sameWord(type, "wig"))
    {
    wigMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "wigMaf"))
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
else if (sameWord(type, "axt"))
    {
    if (wordCount < 2)
        errAbort("Expecting 2 words in axt track type for %s", tdb->tableName);
    axtMethods(track, words[1]);
    }
else if (sameWord(type, "expRatio"))
    {
    expRatioMethods(track);
    }
}

struct track *trackFromTrackDb(struct trackDb *tdb)
/* Create a track based on the tdb. */
{
struct track *track = trackNew();
char *iatName = NULL;
char *exonArrows;

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
/* default exonArrows to on, except for tracks in regulation/expression group */
if (exonArrows == NULL)
    {
    if (sameString(tdb->grp, "regulation"))
       exonArrows = "off";
    else
       exonArrows = "on";
    }
track->exonArrows = sameString(exonArrows, "on");

iatName = trackDbSetting(tdb, "itemAttrIdTbl");
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
    handler = lookupTrackHandler(tdb->tableName);
    if (handler != NULL)
	handler(track);
    if (track->loadItems == NULL)
        warn("No load handler for %s", tdb->tableName);
    else if (track->drawItems == NULL)
        warn("No draw handler for %s", tdb->tableName);
    else
	{
	slAddHead(pTrackList, track);
	}
    }
}

void ctLoadSimpleBed(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct customTrack *ct = tg->customPt;
struct bed *bed, *nextBed, *list = NULL;
for (bed = ct->bedList; bed != NULL; bed = nextBed)
    {
    nextBed = bed->next;
    if (bed->chromStart < winEnd && bed->chromEnd > winStart 
    		&& sameString(chromName, bed->chrom))
	{
	slAddHead(&list, bed);
	}
    }
slSort(&list, bedCmp);
tg->items = list;
}

void ctLoadBed8(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;

for (bed = ct->bedList; bed != NULL; bed = bed->next)
    {
    if (bed->chromStart < winEnd && bed->chromEnd > winStart 
    		&& sameString(chromName, bed->chrom))
	{
	bed8To12(bed);
	lf = lfFromBed(bed);
	slAddHead(&lfList, lf);
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

for (bed = ct->bedList; bed != NULL; bed = bed->next)
    {
    if (bed->chromStart < winEnd && bed->chromEnd > winStart 
    		&& sameString(chromName, bed->chrom))
	{
	lf = lfFromBed(bed);
	slAddHead(&lfList, lf);
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
  sprintf(buf, "%s %s", ctFileName, itemName);
  return buf;
}

struct track *newCustomTrack(struct customTrack *ct)
/* Make up a new custom track. */
{
struct track *tg;
tg = trackFromTrackDb(ct->tdb);

if (ct->wiggle)
    tg->loadItems = ctWigLoadItems;
else
    {
    if (ct->fieldCount < 8)
	{
	tg->loadItems = ctLoadSimpleBed;
	}
    else if (ct->fieldCount < 12)
	{
	tg->loadItems = ctLoadBed8;
	}
    else
	{
	tg->loadItems = ctLoadGappedBed;
	}
    tg->mapItemName = ctMapItemName;
    tg->canPack = TRUE;
    }
tg->customPt = ct;
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
			    cartSetString(cart, tg->mapName, command);
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
	    setPicWidth(words[2]);
	    }
	}
    }

for (ct = ctList; ct != NULL; ct = ct->next)
    {
    char *vis;
    if (!ct->wiggle)
	{
	tg = newCustomTrack(ct);
	vis = cartOptionalString(cart, tg->mapName);
	if (vis != NULL)
	    tg->visibility = hTvFromString(vis);
	slAddHead(pGroupList, tg);
	}
    else
	{
	tg = newCustomTrack(ct);
	vis = cartOptionalString(cart, tg->mapName);
	if (vis != NULL)
	    tg->visibility = hTvFromString(vis);

	slAddHead(pGroupList, tg);
	}
    }
}	/*	void loadCustomTracks(struct track **pGroupList)	*/

void hideAllTracks(struct track *trackList)
/* hide all the tracks (and any in trackDb too) */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *trackDb = hTrackDbName();
char query[256];
assert(trackDb);
safef(query, sizeof(query), "select tableName from %s", trackDb);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    assert(row[0]);
    cartSetString(cart, row[0], "hide");
    }
freez(&trackDb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void hotLinks()
/* Put up the hot links bar. */
{
boolean gotBlat = hIsBlatIndexedDatabase(database);
struct dyString *uiVars = uiStateUrlPart(NULL);
char *orgEnc = cgiEncode(organism);

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#536ED3\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=CENTER><A HREF=\"/index.html?org=%s\">%s</A></TD>", orgEnc, wrapWhiteFont("Home"));

hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s&%s\">%s</A></TD>", orgEnc, database, uiVars->string, wrapWhiteFont("Genomes"));

if (gotBlat)
    {
    hPrintf("<TD><P ALIGN=CENTER><A HREF=\"../cgi-bin/hgBlat?%s\">%s</A></TD>", uiVars->string, wrapWhiteFont("BLAT"));
    }
if (hgPcrOk(database))
    {
    hPrintf("<TD><P ALIGN=CENTER><A HREF=\"../cgi-bin/hgPcr?%s\">%s</A></TD>", uiVars->string, wrapWhiteFont("PCR"));
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&db=%s&%s\">"
      " %s </A></TD>",  hgcNameAndSettings(),
      winStart, chromName, winStart, winEnd, database, uiVars->string, wrapWhiteFont(" DNA "));
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgText?db=%s&position=%s:%d-%d&phase=table&tbPosOrKeys=pos&%s=%u\">%s</A></TD>",
       database, chromName, winStart+1, winEnd, cartSessionVarName(),
       cartSessionId(cart), wrapWhiteFont("Tables"));

if (hgNearOk(database))
    {
    hPrintf("<TD><P ALIGN=CENTER><A HREF=\"../cgi-bin/hgNear?%s\">%s</A></TD>",
                 uiVars->string, wrapWhiteFont("Gene Sorter"));
    }

if (gotBlat)
    {
    /* the only zoo organism this currently works with is human.
     * blastz tables need to be generated for the other organisms. */
    if (!startsWith("zoo", database) || startsWith("zooHuman", database))
        hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgCoordConv?origDb=%s&position=%s:%d-%d&phase=table&%s\">%s</A></TD>", database, chromName, winStart+1, winEnd, uiVars->string, wrapWhiteFont("Convert"));
    }
/* Print Ensembl anchor for latest assembly of organisms we have
 * supported by Ensembl (human, mouse, rat, fugu) */
if (sameString(database, "hg16")
            || sameString(database, "mm3")
            /* Rat goes in when Ensembl rat annotations are ready ? */
            /* || sameString(database, "rn3") */
            || sameString(database, "fr1"))
    {
    hPuts("<TD ALIGN=CENTER>");
    printEnsemblAnchor(database);
    hPrintf("%s</A></TD>", wrapWhiteFont("Ensembl"));
    }
if (sameString(database, "hg16"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?CHR=%s&BEG=%d&END=%d\" TARGET=_blank>",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", wrapWhiteFont("NCBI"));
    }
if (sameString(database, "ce2"))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://ws120.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d\" TARGET=_blank>%s</A></TD>", 
        skipChr(chromName), winStart+1, winEnd, wrapWhiteFont("WormBase"));
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgTracks?%s=%u&hgt.psOutput=on\">%s</A></TD>\n",cartSessionVarName(),
       cartSessionId(cart), wrapWhiteFont("PDF/PS"));
hPrintf("<TD ALIGN=CENTER><A HREF=\"../goldenPath/help/hgTracksHelp.html\" TARGET=_blank>%s</A></TD>\n", wrapWhiteFont("Help"));
hPuts("</TR></TABLE>");
hPuts("</TD></TR></TABLE>\n");
}

void groupTracks(struct track **pTrackList, struct group **pGroupList)
/* Make up groups and assign tracks to groups. */
{
struct group *unknown = NULL;
struct group *group, *list = NULL;
struct hash *hash = newHash(8);
struct track *track;
struct trackRef *tr;
struct grp* grps = hLoadGrps();
struct grp *grp;

/* build group objects from database. */
for (grp = grps; grp != NULL; grp = grp->next)
    {
    AllocVar(group);
    slAddHead(&list, group);
    hashAdd(hash, grp->name, group);
    group->name = cloneString(grp->name);
    group->label = cloneString(grp->label);
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

void doTrackForm(char *psOutput)
/* Make the tracks display form with the zoom/scroll
 * buttons and the active image. */
{
struct group *group;
struct track *track;
char *freezeName = NULL;
boolean hideAll = cgiVarExists("hgt.hideAll");
boolean showedRuler = FALSE;

zoomedToBaseLevel = (winBaseCount <= insideWidth / tl.mWidth);
zoomedToCodonLevel = (ceil(winBaseCount/3) * tl.mWidth) <= insideWidth;
zoomedToCdsColorLevel = (winBaseCount <= insideWidth*3);

if (psOutput != NULL)
   {
   suppressHtml = TRUE;
   hideControls = TRUE;
   }

/* Tell browser where to go when they click on image. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackHeaderForm\" METHOD=GET>\n\n", hgTracksName());
cartSaveSession(cart);

/* See if want to include sequence search results. */
userSeqString = cartOptionalString(cart, "ss");
if (userSeqString && !ssFilesExist(userSeqString))
    {
    userSeqString = NULL;
    cartRemove(cart, "ss");
    }
if (!hideControls)
    hideControls = cartUsualBoolean(cart, "hideControls", FALSE);

/* Register tracks that include some non-standard methods. */
registerTrackHandler("rgdGene", rgdGeneMethods);
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
registerTrackHandler("synteny100000", syntenyMethods);
registerTrackHandler("syntenyBuild30", syntenyMethods);
registerTrackHandler("syntenyBerk", syntenyMethods);
registerTrackHandler("syntenyRatBerkSmall", syntenyMethods);
registerTrackHandler("syntenySanger", syntenyMethods);
registerTrackHandler("syntenyPevzner", syntenyMethods);
registerTrackHandler("syntenyBaylor", syntenyMethods);
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
registerTrackHandler("superfamily", superfamilyMethods);
registerTrackHandler("refGene", refGeneMethods);
registerTrackHandler("blastDm1FB", blastMethods);
registerTrackHandler("blastHg16KG", blastMethods);
registerTrackHandler("blastHg17KG", blastMethods);
registerTrackHandler("blatHg16KG", blastMethods);
registerTrackHandler("tblastnHg16KGPep", blastMethods);
registerTrackHandler("xenoRefGene", xenoRefGeneMethods);
registerTrackHandler("sanger22", sanger22Methods);
registerTrackHandler("sanger22pseudo", sanger22Methods);
registerTrackHandler("vegaGene", vegaMethods);
registerTrackHandler("vegaPseudoGene", vegaMethods);
registerTrackHandler("bdgpGene", bdgpGeneMethods);
registerTrackHandler("bdgpNonCoding", bdgpGeneMethods);
registerTrackHandler("sgdGene", sgdGeneMethods);
registerTrackHandler("genieAlt", genieAltMethods);
registerTrackHandler("ensGene", ensGeneMethods);
registerTrackHandler("ensEst", ensGeneMethods);
registerTrackHandler("mrna", mrnaMethods);
registerTrackHandler("intronEst", estMethods);
registerTrackHandler("est", estMethods);
registerTrackHandler("tightMrna", mrnaMethods);
registerTrackHandler("tightEst", mrnaMethods);
registerTrackHandler("cpgIsland", cpgIslandMethods);
registerTrackHandler("cpgIslandExt", cpgIslandMethods);
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
registerTrackHandler("rmsk", repeatMethods);
registerTrackHandler("rmskNew", repeatMethods);
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
registerTrackHandler("nci60", nci60Methods);
registerTrackHandler("cghNci60", cghNci60Methods);
registerTrackHandler("rosetta", rosettaMethods);
registerTrackHandler("affy", affyMethods);
registerTrackHandler("affyRatio", affyRatioMethods);
registerTrackHandler("affyUclaNorm", affyUclaNormMethods);
registerTrackHandler("ancientR", ancientRMethods );
registerTrackHandler("altGraphX", altGraphXMethods );
registerTrackHandler("altGraphXCon", altGraphXMethods );
registerTrackHandler("triangle", triangleMethods );
registerTrackHandler("triangleSelf", triangleMethods );
registerTrackHandler("transfacHit", triangleMethods );
registerTrackHandler("esRegGeneToMotif", eranModuleMethods );
registerTrackHandler("leptin", mafMethods );
/* Lowe lab related */
registerTrackHandler("gbProtCode", gbGeneMethods);
registerTrackHandler("tigrCmrORFs", tigrGeneMethods);
registerTrackHandler("llaPfuPrintA",llArrayMethods);
registerTrackHandler("llaPaePrintA",llArrayMethods);
registerTrackHandler("tigrOperons",tigrOperonMethods);
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
registerTrackHandler("altGraphXCon2", altGraphXMethods ); 
registerTrackHandler("altGraphXPsb2004", altGraphXMethods ); 
/* registerTrackHandler("altGraphXOrtho", altGraphXMethods ); */
/* registerTrackHandler("altGraphXT6Con", altGraphXMethods ); */
registerTrackHandler("affyTransfrags", affyTransfragsMethods);
registerTrackHandler("chimpSimpleDiff", chimpSimpleDiffMethods);
registerTrackHandler("tfbsCons", tfbsConsMethods);
registerTrackHandler("pscreen", simpleBedTriangleMethods);

/* Load regular tracks, blatted tracks, and custom tracks. 
 * Best to load custom last. */
loadFromTrackDb(&trackList);
if (userSeqString != NULL) slSafeAddHead(&trackList, userPslTg());
loadCustomTracks(&trackList);

groupTracks(&trackList, &groupList);

/* Get visibility values if any from ui. */
for (track = trackList; track != NULL; track = track->next)
    {
    char *s = cartOptionalString(cart, track->mapName);
    if (s != NULL)
	track->visibility = hTvFromString(s);
    }

/* If hideAll flag set, make all tracks hidden */
if(hideAll)
    {
    for (track = trackList; track != NULL; track = track->next)
	{
	track->visibility = tvHide;
	}
    hideAllTracks(trackList);
    }

/* Tell tracks to load their items. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->visibility != tvHide)
	{
	track->loadItems(track); 
	}
    }

/* Center everything from now on. */
hPrintf("<CENTER>\n");

if (!hideControls)
    {
    hotLinks();

    /* Show title . */
    freezeName = hFreezeFromDb(database);
    if(freezeName == NULL)
	freezeName = "Unknown";
    hPrintf("<FONT SIZE=5><B>");
    if (hIsMgcServer()) {
        hPrintf("MGC Genome Browser on %s %s Assembly", organism, freezeName); 
    } else {
	if( startsWith("zoo",database) ) {
/* HACK ALERT - same alert as in hgGateway - The Zoo needs its own
 * mechanism of producing this title with its date and target.
 */
	    hPrintf("UCSC Genome Browser on %s June 2002 Assembly %s target1", organism, freezeName); 
	} else {
	    hPrintf("UCSC Genome Browser on %s %s Assembly", organism, freezeName); 
	}
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
    hPrintf("<INPUT TYPE=HIDDEN NAME=\"position\""
            "VALUE=\"%s:%d-%d\">", chromName, winStart+1, winEnd);
    hPrintf("</FORM>\n");
    hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackForm\" METHOD=POST>\n\n", hgTracksName());
    cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */


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
	hWrites("position ");
	hTextVar("position", addCommasToPos(position), 30);
	sprintLongWithCommas(buf, winEnd - winStart);
#ifdef SORRY_GILL_I_HIT_INSTEAD_OF_SUBMIT_TOO_MANY_TIMES
	hOnClickButton("document.TrackForm.position.value=''","clear");
#endif /* SORRY_GILL_I_HIT_INSTEAD_OF_SUBMIT_TOO_MANY_TIMES */
	hPrintf(" size %s ", buf);
	hWrites(" bp. &nbsp;image width: ");
	hIntVar("pix", tl.picWidth, 4);
	hWrites(" ");
	hButton("submit", "jump");
	hPutc('\n');
	}
    }

/* Make chromsome ideogram gif and map. */
makeChromIdeoImage(&trackList, psOutput);
/* Make clickable image and map. */
makeActiveImage(trackList, psOutput);
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
	  "Click on left mini-buttons for track-specific options" );
    hPrintf("</TD><TD COLSPAN=6 ALIGN=CENTER NOWRAP>");
    hPrintf("move end<BR>");
    hButton("hgt.dinkRL", " < ");
    hTextVar("dinkR", cartUsualString(cart, "dinkR", "2.0"), 3);
    hButton("hgt.dinkRR", " > ");
    hPrintf("</TD></TR></TABLE>\n");
    smallBreak();

    /* Display bottom control panel. */
    hButton("hgt.reset", "reset all");
    hPrintf(" ");
    hButton("hgt.hideAll", "hide all");
    if(ideogramAvail)
	{
	hPrintf(" Chromosome");
	hCheckBox("ideogram", withIdeogram);
	}
    hPrintf(" Guidelines ");
    hCheckBox("guidelines", withGuidelines);
    hPrintf(" <B>Labels:</B> ");
    hPrintf("left ");
    hCheckBox("leftLabels", withLeftLabels);
    hPrintf("center ");
    hCheckBox("centerLabels", withCenterLabels);
    hPrintf(" ");
    hButton("submit", "refresh");
    hPrintf("<BR>\n");

    /* Display viewing options for each track. */
    /* Chuck: This is going to be wrapped in a table so that
     * the controls don't wrap around randomly
     */
    if( chromosomeColorsMade )
        {
        hPrintf("<B>Chromosome Color Key:</B><BR> ");
        hPrintf("<IMG SRC = \"../images/new_colorchrom.gif\" BORDER=1 WIDTH=596 HEIGHT=18 ><BR>\n");
        }
    hPrintf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
    // hPrintf("<tr><th colspan=%d>\n", MAX_CONTROL_COLUMNS);
    // smallBreak();
    // hPrintf("<B>Track Controls:</B>");
    // hPrintf("</th></tr>\n");
    hPrintf("<tr><td colspan='5' align='middle' nowrap>"
	   "Use drop down controls below and press refresh to alter tracks "
	   "displayed.<BR>"
	   "Tracks with lots of items will automatically be displayed in "
	   "more compact modes.</td></tr>\n");
    cg = startControlGrid(MAX_CONTROL_COLUMNS, "left");
    hPrintf("<TR>");
    for (group = groupList; group != NULL; group = group->next)
        {
	struct trackRef *tr;

	if (group->trackList == NULL)
	    continue;

	/* Print group label on left. */
	hPrintf("<th colspan=%d BGCOLOR=#536ED3>", 
		MAX_CONTROL_COLUMNS);
	hPrintf("<B>%s</B>", wrapWhiteFont(group->label));
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
            // TODO: perhaps replace with standard vis menu, below */
	    hDropList("ruler", rulerMenu, sizeof(rulerMenu)/sizeof(char *), 
                                                        rulerMenu[rulerMode]);
	    controlGridEndCell(cg);
	    }

	for (tr = group->trackList; tr != NULL; tr = tr->next)
	    {
	    track = tr->track;
	    controlGridStartCell(cg);
	    if (track->hasUi)
		hPrintf("<A HREF=\"%s?%s=%u&c=%s&g=%s\">", hgTrackUiName(),
		    cartSessionVarName(), cartSessionId(cart),
		    chromName, track->mapName);
	    hPrintf(" %s<BR> ", track->shortLabel);
	    if (track->hasUi)
		hPrintf("</A>");
	    hTvDropDownClass(track->mapName, track->visibility, track->canPack,
                                 (track->visibility == tvHide) ? 
                                        "hiddenText" : "normalText" );
	    controlGridEndCell(cg);
	    }
	/* now finish out the table */
	if (group->next != NULL)
	    controlGridEndRow(cg);
	}
    endControlGrid(&cg);
    }
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
hPrintf("</FORM>");
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
else if (newSizeDbl < 0)
    newSize = 0;
else
    newSize = (int)newSizeDbl;
if (newSize < 30) newSize = 30;
if (newSize > seqBaseCount)
    newSize = seqBaseCount;
zoomToSize(newSize);
}

void zoomToBaseLevel()
/* Set things so that it's zoomed to base level. */
{
zoomToSize(insideWidth/tl.mWidth);
if (rulerMode == RULER_MODE_OFF)
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

char * convertEpsToPdf(char *epsFile) 
/* Convert EPS to PDF and return filename, or NULL if failure. */
{
char *pdfTmpName = NULL, *pdfName=NULL;
char cmdBuffer[2048];
int sysVal = 0;
struct lineFile *lf = NULL;
char *line;
int lineSize=0;
float width=0, height=0;
pdfTmpName = cloneString(epsFile);

/* Get the dimensions of bounding box. */
lf = lineFileOpen(epsFile, TRUE);
while(lineFileNext(lf, &line, &lineSize)) 
    {
    if(strstr( line, "BoundingBox:")) 
	{
	char *words[5];
	chopLine(line, words);
	width = atof(words[3]);
	height = atof(words[4]);
	break;
	}
    }
lineFileClose(&lf);
	
/* Do conversion. */
chopSuffix(pdfTmpName);
pdfName = addSuffix(pdfTmpName, ".pdf");
safef(cmdBuffer, sizeof(cmdBuffer), "ps2pdf -dDEVICEWIDTHPOINTS=%d -dDEVICEHEIGHTPOINTS=%d %s %s", 
      round(width), round(height), epsFile, pdfName);
sysVal = system(cmdBuffer);
if(sysVal != 0)
    freez(&pdfName);
freez(&pdfTmpName);
return pdfName;
}

void handlePostscript()
/* Deal with Postscript output. */
{
struct tempName psTn;
char *pdfFile = NULL;
makeTempName(&psTn, "hgt", ".eps");
printf("<H1>PostScript/PDF Output</H1>\n");
printf("PostScript images can be printed at high resolution "
       "and edited by many drawing programs such as Adobe "
       "Illustrator.<BR>");
doTrackForm(psTn.forCgi);
printf("<A HREF=\"%s\">Click here to download</A> "
       "the current browser graphic in PostScript.  ", psTn.forCgi);
pdfFile = convertEpsToPdf(psTn.forCgi);
if(pdfFile != NULL) 
    {
    printf("<BR><BR>PDF can be viewed with Adobe Acrobat Reader.<BR>\n");
    printf("<A HREF=\"%s\">Click here to download</A> "
	   "the current browser graphic in PDF", pdfFile);
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

void tracksDisplay()
/* Put up main tracks display. This routine handles zooming and
 * scrolling. */
{
char newPos[256];
char *defaultPosition = hDefaultPos(database);
char *s;
char *motifString = cartOptionalString(cart,"hgt.motifs");
boolean complementRulerBases = cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE);
position = getPositionFromCustomTracks();
if (NULL == position) 
    {
    position = cloneString(cartUsualString(cart, "position", NULL));
    }

if((position == NULL) || sameString(position, ""))
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
insideX = trackOffsetX();
insideWidth = tl.picWidth-gfxBorder-insideX;

if (cgiVarExists("hgt.hideAll"))
    cartSetString(cart, RULER_TRACK_NAME, "dense");
s = cartUsualString(cart, RULER_TRACK_NAME, "full");
if (sameWord(s, "full") || sameWord(s, "on"))
    rulerMode = RULER_MODE_FULL;
else if (sameWord(s, "dense"))
    rulerMode = RULER_MODE_ON;
else
    rulerMode = RULER_MODE_OFF;

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
/* If are viewing the negative strand on ruler have
   to revsere the motifs to highlight. */
if(complementRulerBases && motifString != NULL)
    reverseBytes(motifString, strlen(motifString));
if (cgiVarExists("hgt.psOutput"))
    handlePostscript();
else
    doTrackForm(NULL);
/* If motifs were reversed, put them back in original
   order. */
if(complementRulerBases && motifString != NULL)
    reverseBytes(motifString, strlen(motifString));
}


void customTrackPage()
/* Put up page that lets user upload custom tracks. */
{
puts("<H2>Add Your Own Custom Track</H2>");
puts("<FORM ACTION=\"/cgi-bin/hgTracks\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);

puts(
"<P>Display your own annotation tracks in the browser using \n"
"the <A HREF=\"../goldenPath/help/customTrack.html\"> \n"
"procedure described here</A>.  Annotations may be uploaded from files or\n"
"pasted into the text box below. You can also paste a URL or a list of URLs \n"
"into the large text box that refer to \n"
"files in one of the supported formats.</P>\n"
"Click \n"
"<A HREF=\"../goldenPath/customTracks/custTracks.html\" TARGET=_blank>here</A> \n"
"to view a collection of custom annotation tracks submitted by Genome Browser users.</P> \n"
"\n"
"	Annotation File: <INPUT TYPE=FILE NAME=\"hgt.customFile\">\n"
);

cgiMakeButton("Submit", "Submit");

cgiSimpleTableStart();
cgiSimpleTableRowStart();
cgiSimpleTableFieldStart();
cgiMakeTextArea("hgt.customText", "", 14, 80);
cgiTableFieldEnd();
cgiSimpleTableFieldStart();
cgiMakeResetButton();
cgiTableFieldEnd();
cgiTableRowEnd();
cgiTableEnd();

#if defined(NOT)	/*	NOT YET	*/
puts("<BR>\n");
cgiMakeCheckBox("hgt.customAppend", cgiBooleanDefined("hgt.customAppend"));
puts("Check box to add this sequence to existing custom tracks<BR>If unchecked, all existing custom tracks will be cleared.</FORM>\n");
#endif

puts("</FORM>");
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
hSetDb(database);
protDbName = hPdbFromGdb(database);
debugTmp = cartUsualString(cart, "hgDebug", "off");
if(sameString(debugTmp, "on"))
    hgDebug = TRUE;
else
    hgDebug = FALSE;

hDefaultConnect();
initTl();

/* Do main display. */
if (cartVarExists(cart, "customTrackPage"))
    {
    cartRemove( cart, "customTrackPage");
    customTrackPage();
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
			"hgt.tui", "hgt.hideAll", "hgt.psOutput", "hideControls",
			NULL };

int main(int argc, char *argv[])
{
/* Push very early error handling - this is just
 * for the benefit of the cgiVarExists, which 
 * somehow can't be moved effectively into doMiddle. */
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
if (cgiVarExists("hgt.reset"))
    resetVars();
htmlSetBackground("../images/floret.jpg");
cartHtmlShell("UCSC Genome Browser v"CGI_VERSION, doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
