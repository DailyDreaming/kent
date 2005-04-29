/* wigMafTrack - display multiple alignment files with score wiggle
 * and base-level alignment, or else density plot of pairwise alignments
 * (zoomed out) */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "maf.h"
#include "scoredRef.h"
#include "wiggle.h"
#include "hgMaf.h"
#include "mafTrack.h"
#include "mafSummary.h"

#define ANNOT_DEBUG 1
#undef ANNOT_DEBUG

static char const rcsid[] = "$Id: wigMafTrack.c,v 1.77 2005/04/29 21:59:28 kate Exp $";

struct wigMafItem
/* A maf track item -- 
 * a line of bases (base level) or pairwise density gradient (zoomed out). */
    {
    struct wigMafItem *next;
    char *name;		/* Common name */
    char *db;		/* Database */
    int group;          /* number of species group/clade */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    };

static void wigMafItemFree(struct wigMafItem **pEl)
/* Free up a wigMafItem. */
{
struct wigMafItem *el = *pEl;
if (el != NULL)
    {
    freeMem(el->name);
    freeMem(el->db);
    freez(pEl);
    }
}

void wigMafItemFreeList(struct wigMafItem **pList)
/* Free a list of dynamically allocated wigMafItem's */
{
struct wigMafItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wigMafItemFree(&el);
    }
*pList = NULL;
}

Color wigMafItemLabelColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw a maf item based on the species group it is in */
{
return (((struct wigMafItem *)item)->group % 2 ? 
                        tg->ixAltColor : tg->ixColor);
}

struct mafAli *wigMafLoadInRegion(struct sqlConnection *conn, 
	char *table, char *chrom, int start, int end)
/* Load mafs from region */
{
    return mafLoadInRegion(conn, table, chrom, start, end);
}

static struct wigMafItem *newMafItem(char *s, int g)
/* Allocate and initialize a maf item. Species param can be a db or name */
{
struct wigMafItem *mi;
char *val;

AllocVar(mi);
if ((val = hGenome(s)) != NULL)
    {
    /* it's a database name */
    mi->db = cloneString(s);
    mi->name = val;
    }
else
    {
    mi->db = cloneString(s);
    mi->name = cloneString(s);
    }
mi->name = hgDirForOrg(mi->name);
*mi->name = tolower(*mi->name);
mi->height = tl.fontHeight;
mi->group = g;
return mi;
}

struct wigMafItem *newSpeciesItems(struct track *track, int height)
/* Make up item list for all species configured in track settings */
{
char option[64];
char *species[100];
char *groups[20];
char *defaultOff[100];
char sGroup[24];
struct wigMafItem *mi = NULL, *miList = NULL;
int group;
int i;
int speciesCt = 0, groupCt = 1;
int speciesOffCt = 0;
struct hash *speciesOffHash = newHash(0);

/* either speciesOrder or speciesGroup is specified in trackDb */
char *speciesOrder = trackDbSetting(track->tdb, SPECIES_ORDER_VAR);
char *speciesGroup = trackDbSetting(track->tdb, SPECIES_GROUP_VAR);
char *speciesOff = trackDbSetting(track->tdb, SPECIES_DEFAULT_OFF_VAR);

if (speciesOrder == NULL && speciesGroup == NULL)
    errAbort(
      "Track %s missing required trackDb setting: speciesOrder or speciesGroup",
                track->mapName);
if (speciesGroup)
    groupCt = chopLine(cloneString(speciesGroup), groups);

/* keep track of species configured off initially for track */
if (speciesOff)
    {
    speciesOffCt = chopLine(cloneString(speciesOff), defaultOff);
    for (i = 0; i < speciesOffCt; i++)
        hashAdd(speciesOffHash, defaultOff[i], NULL);
    }

/* Make up items for other organisms by scanning through group & species 
   track settings */
for (group = 0; group < groupCt; group++)
    {
    if (groupCt != 1 || !speciesOrder)
        {
        safef(sGroup, sizeof sGroup, "%s%s", 
                                SPECIES_GROUP_PREFIX, groups[group]);
        speciesOrder = trackDbRequiredSetting(track->tdb, sGroup);
        }
    speciesCt = chopLine(cloneString(speciesOrder), species);
    for (i = 0; i < speciesCt; i++)
        {
        /* skip this species if UI checkbox was unchecked */
        safef(option, sizeof(option), "%s.%s", track->mapName, species[i]);
        if (!cartVarExists(cart, option))
            if (hashLookup(speciesOffHash, species[i]))
                cartSetBoolean(cart, option, FALSE);
        if (!cartUsualBoolean(cart, option, TRUE))
            continue;
        mi = newMafItem(species[i], group);
        slAddHead(&miList, mi);
        }
    }
for (mi = miList; mi != NULL; mi = mi->next)
    mi->height = height;
return miList;
}

static struct wigMafItem *scoreItem(int scoreHeight)
/* Make up item that will show the score */
{
struct wigMafItem *mi;

AllocVar(mi);
mi->name = cloneString("Conservation");
mi->height = scoreHeight;
return mi;
}

static void loadMafsToTrack(struct track *track)
/* load mafs in region to track custom pointer */
{
struct sqlConnection *conn;

if (winBaseCount > MAF_SUMMARY_VIEW)
    return;
conn = hAllocConn();
track->customPt = wigMafLoadInRegion(conn, track->mapName, 
                                        chromName, winStart, winEnd);
hFreeConn(&conn);
}

static struct wigMafItem *loadBaseByBaseItems(struct track *track)
/* Make up base-by-base track items. */
{
struct wigMafItem *miList = NULL, *speciesList = NULL, *mi;
int scoreHeight = 0;

loadMafsToTrack(track);

/* Add item for score wiggle base alignment */
if (track->subtracks)
    {
    enum trackVisibility wigVis = 
    	(track->visibility == tvDense ? tvDense : tvFull);
    scoreHeight = wigTotalHeight(track->subtracks, wigVis);
    }
else
    scoreHeight = 
        (track->visibility == tvDense ? tl.fontHeight : tl.fontHeight * 4);
mi = scoreItem(scoreHeight);
slAddHead(&miList, mi);

/* Make up item that will show gaps in this organism. */
AllocVar(mi);
mi->name = "Gaps";
mi->height = tl.fontHeight;
slAddHead(&miList, mi);

/* Make up item for this organism. */
mi = newMafItem(database, 0);
slAddHead(&miList, mi);

/* Make items for other species */
speciesList = newSpeciesItems(track, tl.fontHeight);
miList = slCat(speciesList, miList);

slReverse(&miList);
return miList;
}

static char *summarySetting(struct track *track)
/* Return the setting for the MAF summary table
 * or NULL if none set  */
{
return trackDbSetting(track->tdb, SUMMARY_VAR);
}

static char *pairwiseSuffix(struct track *track)
/* Return the suffix for the wiggle tables for the pairwise alignments,
 * or NULL if none set  */
{
char *suffix = trackDbSetting(track->tdb, PAIRWISE_VAR);
if (suffix != NULL)
    suffix = firstWordInLine(cloneString(suffix));
return suffix;
}

static int pairwiseWigHeight(struct track *track)
/* Return the height of a pairwise wiggle for this track, or 0 if n/a
 * NOTE: set one pixel smaller than we actually want, to 
 * leave a border at the bottom of the wiggle.
 */
{
char *words[2];
int wordCount;
char *settings;
struct track *wigTrack = track->subtracks;
int pairwiseHeight = tl.fontHeight;
int consWigHeight = 0;

if (wigTrack)
    {
    consWigHeight = wigTotalHeight(wigTrack, tvFull);
    pairwiseHeight = max(consWigHeight/3 - 1, pairwiseHeight);
    }

settings = cloneString(trackDbSetting(track->tdb, PAIRWISE_VAR));
if (settings == NULL)
    return pairwiseHeight;

/* get height for pairwise wiggles */
if ((wordCount = chopLine(settings, words)) > 1)
    {
    int settingsHeight = atoi(words[1]);
    if (settingsHeight < tl.fontHeight)
       pairwiseHeight = tl.fontHeight;
    else if (settingsHeight > consWigHeight && consWigHeight > 0)
        pairwiseHeight = consWigHeight;
    else
        pairwiseHeight = settingsHeight;
    }
freez(&settings);
return pairwiseHeight;
}

static char *getWigTablename(char *species, char *suffix)
/* generate tablename for wiggle pairwise: "<species>_<table>_wig" */
{
char table[64];

safef(table, sizeof(table), "%s_%s_wig", species, suffix);
return cloneString(table);
}

static char *getMafTablename(char *species, char *suffix)
/* generate tablename for wiggle maf:  "<species>_<table>" */
{
char table[64];

safef(table, sizeof(table), "%s_%s", species, suffix);
return cloneString(table);
}

static boolean displayPairwise(struct track *track)
/* determine if tables are present for pairwise display */
{
return winBaseCount < MAF_SUMMARY_VIEW || 
        pairwiseSuffix(track) || summarySetting(track);
}

static boolean displayZoomedIn(struct track *track)
/* determine if mafs are loaded -- zoomed in display */
{
return track->customPt != (char *)-1;
}

static void markNotPairwiseItem(struct wigMafItem *mi)
{
    mi->ix = -1;
}
static boolean isPairwiseItem(struct wigMafItem *mi)
{
    return mi->ix != -1;
}

static struct wigMafItem *loadPairwiseItems(struct track *track)
/* Make up items for modes where pairwise data are shown.
   First an item for the score wiggle, then a pairwise item
   for each "other species" in the multiple alignment.
   These may be density plots (pack) or wiggles (full).
   Return item list.  Also set customPt with mafList if
   zoomed in */
{
struct wigMafItem *miList = NULL, *speciesItems = NULL, *mi;
struct track *wigTrack = track->subtracks;
int scoreHeight = tl.fontHeight * 4;

if (winBaseCount < MAF_SUMMARY_VIEW)
    {
    /* "close in" display uses actual alignments from file */
    struct sqlConnection *conn = hAllocConn();
    track->customPt = wigMafLoadInRegion(conn, track->mapName, 
                                        chromName, winStart, winEnd);
#ifdef DEBUG
    slSort(&track->customPt, mafCmp);
#endif
    hFreeConn(&conn);
    }
if (wigTrack != NULL)
    scoreHeight = wigTotalHeight(wigTrack, tvFull);
mi = scoreItem(scoreHeight);
/* mark this as not a pairwise item */
markNotPairwiseItem(mi);
slAddHead(&miList, mi);
if (displayPairwise(track))
    /* make up items for other organisms by scanning through
     * all mafs and looking at database prefix to source. */
    {
    speciesItems = newSpeciesItems(track, track->visibility == tvFull ?
                                                pairwiseWigHeight(track) :
                                                tl.fontHeight);
    miList = slCat(speciesItems, miList);
    }
slReverse(&miList);
return miList;
}

static struct wigMafItem *loadWigMafItems(struct track *track,
                                                 boolean isBaseLevel)
/* Load up items */
{
struct wigMafItem *miList = NULL;
int scoreHeight = tl.fontHeight * 4;

track->customPt = (char *)-1;   /* no maf's loaded or attempted to load */

/* Load up mafs and store in track so drawer doesn't have
 * to do it again. */
/* Make up tracks for display. */
if (isBaseLevel)
    {
    miList = loadBaseByBaseItems(track);
    }
/* zoomed out */
else if (track->visibility == tvFull || track->visibility == tvPack)
    {
    miList = loadPairwiseItems(track);
    }
else if (track->visibility == tvSquish)
    {
    if (track->subtracks)
        /* have a wiggle */
        scoreHeight = wigTotalHeight(track->subtracks, tvFull);
    else
        {
        scoreHeight = tl.fontHeight * 4;
        if (winBaseCount < MAF_SUMMARY_VIEW)
            loadMafsToTrack(track);
        }
    miList = scoreItem(scoreHeight);
    }
else 
    {
    /* dense mode, zoomed out - show density plot, with track label */
    if (!track->subtracks)
        /* no wiggle -- use mafs if close in */
        if (winBaseCount < MAF_SUMMARY_VIEW)
            loadMafsToTrack(track);
    AllocVar(miList);
    miList->name = cloneString(track->shortLabel);
    miList->height = tl.fontHeight;
    }
return miList;
}

static void wigMafLoad(struct track *track)
/* Load up maf tracks.  What this will do depends on
 * the zoom level and the display density. */
{
struct wigMafItem *miList = NULL;
struct track *wigTrack = track->subtracks;

miList = loadWigMafItems(track, zoomedToBaseLevel);
track->items = miList;

if (wigTrack != NULL) 
    // load wiggle subtrack items
    {
    /* update track visibility from parent track,
     * since hgTracks will update parent vis before loadItems */
    wigTrack->visibility = track->visibility;
    wigTrack->loadItems(wigTrack);
    }
}

static int wigMafTotalHeight(struct track *track, enum trackVisibility vis)
/* Return total height of maf track.  */
{
struct wigMafItem *mi;
int total = 0;
for (mi = track->items; mi != NULL; mi = mi->next)
    total += mi->height;
track->height =  total;
return track->height;
}

static int wigMafItemHeight(struct track *track, void *item)
/* Return total height of maf track.  */
{
struct wigMafItem *mi = item;
return mi->height;
}


static void wigMafFree(struct track *track)
/* Free up maf items. */
{
if (track->customPt != NULL && track->customPt != (char *)-1)
    mafAliFreeList((struct mafAli **)&track->customPt);
if (track->items != NULL)
    wigMafItemFreeList((struct wigMafItem **)&track->items);
}

static char *wigMafItemName(struct track *track, void *item)
/* Return name of maf level track. */
{
struct wigMafItem *mi = item;
return mi->name;
}

static void processInserts(char *text, struct mafAli *maf, 
                                struct hash *itemHash,
	                        int insertCounts[], int baseCount)
/* Make up insert line from sequence of reference species.  
   It has a gap count at each displayed base position, and is generated by
   counting up '-' chars in the sequence, where  */
{
int i, baseIx = 0;
struct mafComp *mc;
char c;

for (i=0; i < maf->textSize && baseIx < baseCount; i++)
    {
    c = text[i];
    if (c == '-')
        {
        for (mc = maf->components; mc != NULL; mc = mc->next)
            {
            char buf[64];
            mafSrcDb(mc->src, buf, sizeof buf);
            if (hashLookup(itemHash, buf) == NULL)
                continue;
            if (mc->size == 0)
                /* empty row annotation */
                continue;
            if (mc->text[i] != '-')
                {
                insertCounts[baseIx]++;
                break;
                }
            }
        }
    else
        baseIx++;
    }
}

static void charifyInserts(char *insertLine, int insertCounts[], int size)
/* Create insert line from insert counts */
{
int i;
char c;
for (i=0; i<size; ++i)
    {
    int b = insertCounts[i];
    if (b == 0)
       c = ' ';
    else if (b <= 9)
       c = b + '0';
    else if (b % 3)
        /* multiple of 3 gap */
       c = '+';
    else
       c = '*';
    insertLine[i] = c;
    }
}

static void processSeq(char *text, char *masterText, int textSize,
                            char *outLine, int offset, int outSize)
/* Add text to outLine, suppressing copy where there are dashes
 * in masterText.  This effectively projects the alignment onto
 * the master genome.
 * If no dash exists in this sequence, count up size
 * of the insert and save in the line.
 */
{
int i, outIx = 0, outPositions = 0;
int insertSize = 0, previousInserts = 0;
int previousBreaks = 0;

/* count up insert counts in the existing line -- need to 
   add these to determine where to start this sequence in the line */
for (i=0; outLine[i]; i++)
    {
    if (outLine[i] == '|')
        {
        previousInserts++;
        i++;    /* skip count after escape char */
        }
    if (outLine[i] == MAF_FULL_BREAK_BEFORE ||
        outLine[i] == MAF_FULL_BREAK_AFTER ||
        outLine[i] == MAF_PARTIAL_BREAK_BEFORE ||
        outLine[i] == MAF_PARTIAL_BREAK_AFTER ||
        outLine[i] == MAF_FULL_MAYBE_BREAK_BEFORE ||
        outLine[i] == MAF_FULL_MAYBE_BREAK_AFTER ||
        outLine[i] == MAF_PARTIAL_MAYBE_BREAK_BEFORE ||
        outLine[i] == MAF_PARTIAL_MAYBE_BREAK_AFTER)
            previousBreaks++;
    }
outLine = outLine + offset + previousBreaks + (previousInserts * 2);
for (i=0; i < textSize && outPositions < outSize;  i++)
    {
    if (masterText[i] != '-')
        {
        if (insertSize != 0)
            {
            outLine[outIx++] = '|';// escape to indicate following is count
            outLine[outIx++] = (unsigned char) max(255, insertSize);
            insertSize = 0;
            }
	outLine[outIx++] = text[i];
        outPositions++;
	}
    else
        {
        /* gap in master (reference) sequence but not in this species */
        if (text[i] != '-' && text[i] != '=')
            insertSize++;
        }
    }
}

static int drawScore(float score, int chromStart, int chromEnd, int seqStart,
                        double scale, struct vGfx *vg, int xOff, int yOff,
                        int height, Color color, enum trackVisibility vis)
/* Draw density plot or graph based on score. Return last X drawn  */
{
int x1,x2,y,w;
int height1 = height-1;

x1 = round((chromStart - seqStart)*scale);
x2 = round((chromEnd - seqStart)*scale);
w = x2-x1+1;
if (vis == tvFull)
    {
    y = score * height1;
    vgBox(vg, x1 + xOff, yOff + height1 - y, w, y+1, color);
    }
else
    {
    Color c;
    int shade = (score * maxShade) + 1;
    if (shade < 0)
        shade = 0;
    else if (shade > maxShade)
        shade = maxShade;
    c = shadesOfGray[shade];
    vgBox(vg, x1 + xOff, yOff, w, height1, c);
    }
return xOff + x1 + w - 1;
}

static void drawScoreSummary(struct mafSummary *summaryList, int height,
                             int seqStart, int seqEnd, 
                            struct vGfx *vg, int xOff, int yOff,
                            int width, MgFont *font, 
                            Color color, Color altColor,
                            enum trackVisibility vis, boolean chainBreaks)
/* Draw density plot or graph for summary maf scores */
{
struct mafSummary *ms;
double scale = scaleForPixels(width);
boolean isDouble = FALSE;
int x1, x2;
int w = 0;
int chromStart = seqStart;
int lastX;

/* draw chain before first alignment */
ms = summaryList;
if (chainBreaks && ms->chromStart > chromStart && 
    (ms->leftStatus[0] == MAF_CONTIG_STATUS || 
     ms->leftStatus[0] == MAF_INSERT_STATUS))
    {
    isDouble = (ms->leftStatus[0] == MAF_INSERT_STATUS);
    x1 = xOff;
    x2 = round((double)((int)ms->chromStart-1 - seqStart) * scale) + xOff;
    w = x2 - x1;
    if (w > 0)
        drawMafChain(vg, x1, yOff, w, height, isDouble);
    }
for (ms = summaryList; ms != NULL; ms = ms->next)
    {
    lastX = drawScore(ms->score, ms->chromStart, ms->chromEnd, seqStart,
                        scale, vg, xOff, yOff, height, color, vis);

    /* draw chain after alignment */
    if (chainBreaks && ms->chromEnd < seqEnd && ms->next != NULL &&
        (ms->rightStatus[0] == MAF_CONTIG_STATUS || 
         ms->rightStatus[0] == MAF_INSERT_STATUS))
        {
        isDouble = (ms->rightStatus[0] == MAF_INSERT_STATUS);
        x1 = round((double)((int)ms->chromEnd+1 - seqStart) * scale) + xOff;
        x2 = round((double)((int)ms->next->chromStart-1 - seqStart) * scale) 
                + xOff;
        w = x2 - x1;
        if (w == 1 && x1 == lastX)
            continue;
        if (w > 0);
            drawMafChain(vg, x1, yOff, w, height, isDouble);
        }
    }
}

static void drawScoreOverview(char *tableName, int height,
                             int seqStart, int seqEnd, 
                            struct vGfx *vg, int xOff, int yOff,
                            int width, MgFont *font, 
                            Color color, Color altColor,
                            enum trackVisibility vis)
/* Draw density plot or graph for overall maf scores rather than computing
 * by sections, for speed.  Don't actually load the mafs -- just
 * the scored refs from the table.
 * TODO: reuse code in mafTrack.c 
 */
{
char **row;
int rowOffset;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = hRangeQuery(conn, tableName, chromName, 
                                        seqStart, seqEnd, NULL, &rowOffset);
double scale = scaleForPixels(width);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct scoredRef ref;
    scoredRefStaticLoad(row + rowOffset, &ref);
    drawScore(ref.score, ref.chromStart, ref.chromEnd, seqStart, scale,
                vg, xOff, yOff, height, color, vis);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}


static boolean drawPairsFromSummary(struct track *track, 
        int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
{
/* Draw pairwise display for this multiple alignment */
char *summary;
struct wigMafItem *miList = track->items, *mi = miList;
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;
struct mafSummary *ms, *summaryList;
struct hash *componentHash = newHash(6);
struct hashEl *hel;
struct hashCookie cookie;
struct dyString *where = dyStringNew(256);
boolean useIrowChains = FALSE;
char option[64];

if (miList == NULL)
    return FALSE;

/* get summary table name from trackDb */
if ((summary = summarySetting(track)) == NULL)
    return FALSE;

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_CHAIN_VAR);
if (cartCgiUsualBoolean(cart, option, FALSE) && 
    trackDbSetting(track->tdb, "irows") != NULL)
        useIrowChains = TRUE;

/* Create SQL where clause that will load up just the
 * summaries for the species that we are including. */ 
conn = hAllocConn();
dyStringAppend(where, "src in (");
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (!isPairwiseItem(mi))
        /* exclude non-species items (e.g. conservation wiggle */
        continue;
    dyStringPrintf(where, "'%s'", mi->db);
    if (mi->next != NULL)
        dyStringAppend(where, ",");
    }
dyStringAppend(where, ")");
sr = hOrderedRangeQuery(conn, summary, chromName, seqStart, seqEnd,
                        where->string, &rowOffset);

/* Loop through result creating a hash of lists of maf summary blocks.
 * The hash is keyed by species. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (hHasField(summary, "leftStatus"))
        ms = mafSummaryLoad(row + rowOffset);
    else
        /* previous table schema didn't have status fields */
        ms = mafSummaryMiniLoad(row + rowOffset);
    /* prune to fit in window bounds */
    if (ms->chromStart < seqStart)
        ms->chromStart = seqStart;
    if (ms->chromEnd > seqEnd)
        ms->chromEnd = seqEnd;
    if ((hel = hashLookup(componentHash, ms->src)) == NULL)
        hashAdd(componentHash, ms->src, ms);
    else
        slAddHead(&(hel->val), ms);
    }
sqlFreeResult(&sr);

/* reverse summary lists */
cookie = hashFirst(componentHash);
while ((hel = hashNext(&cookie)) != NULL)
    slReverse(&hel->val);
hFreeConn(&conn);

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (mi->ix < 0)
        /* ignore item for the score */
        continue;
    summaryList = (struct mafSummary *)hashFindVal(componentHash, mi->db);
    if (summaryList == NULL)
        summaryList = 
            (struct mafSummary *)hashMustFindVal(componentHash, mi->name);
    if (vis == tvFull)
        {
        vgSetClip(vg, xOff, yOff, width, 16);
        drawScoreSummary(summaryList, mi->height, seqStart, seqEnd, vg, 
                                xOff, yOff, width, font, track->ixAltColor,
                                track->ixAltColor, tvFull, FALSE);
        vgUnclip(vg);
        }
    else 
        {
        /* pack */
        /* get maf table, containing pairwise alignments for this organism */
        /* display pairwise alignments in this region in dense format */
        vgSetClip(vg, xOff, yOff, width, mi->height);
        drawScoreSummary(summaryList, mi->height, seqStart, seqEnd, vg, 
                            xOff, yOff, width, font, color, color, tvDense,
                            useIrowChains);
        vgUnclip(vg);
        }
    yOff += mi->height;
    }
return TRUE;
}

static boolean drawPairsFromPairwiseMafScores(struct track *track, 
        int seqStart, int seqEnd, struct vGfx *vg, int xOff, int yOff, 
        int width, MgFont *font, Color color, enum trackVisibility vis)
/* Draw pairwise display for this multiple alignment */
{
char *suffix;
char *tableName;
Color pairColor;
struct track *wigTrack = track->subtracks;
int pairwiseHeight = pairwiseWigHeight(track);
struct wigMafItem *miList = track->items, *mi = miList;

if (miList == NULL)
    return FALSE;

/* get pairwise table suffix from trackDb */
suffix = pairwiseSuffix(track);

if (vis == tvFull)
    {
    double minY = 50.0;
    double maxY = 100.0;

    /* NOTE: later, remove requirement for wiggle */
    if (wigTrack == NULL)
        return FALSE;
    /* swap colors for pairwise wiggles */
    pairColor = wigTrack->ixColor;
    wigTrack->ixColor = wigTrack->ixAltColor;
    wigTrack->ixAltColor = pairColor;
    wigSetCart(wigTrack, MIN_Y, (void *)&minY);
    wigSetCart(wigTrack, MAX_Y, (void *)&maxY);
    }

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (mi->ix < 0)
        /* ignore item for the score */
        continue;
    if (vis == tvFull)
        {
        /* get wiggle table, of pairwise 
           for example, percent identity */
        tableName = getWigTablename(mi->name, suffix);
        if (!hTableExists(tableName))
            tableName = getWigTablename(mi->db, suffix);
        if (hTableExists(tableName))
            {
            /* reuse the wigTrack for pairwise tables */
            wigTrack->mapName = tableName;
            wigTrack->loadItems(wigTrack);
            wigTrack->height = wigTrack->lineHeight = wigTrack->heightPer =
                                                    pairwiseHeight - 1;
            /* clip, but leave 1 pixel border */
            vgSetClip(vg, xOff, yOff, width, wigTrack->height);
            wigTrack->drawItems(wigTrack, seqStart, seqEnd, vg, xOff, yOff,
                             width, font, color, tvFull);
            vgUnclip(vg);
            }
        else
            {
            /* no wiggle table for this -- compute a graph on-the-fly 
               from mafs */
            vgSetClip(vg, xOff, yOff, width, mi->height);
            tableName = getMafTablename(mi->name, suffix);
            if (!hTableExists(tableName))
                tableName = getMafTablename(mi->db, suffix);
            if (hTableExists(tableName))
                drawScoreOverview(tableName, mi->height, seqStart, seqEnd, vg, 
                                xOff, yOff, width, font, track->ixAltColor, 
                                track->ixAltColor, tvFull);
            vgUnclip(vg);
            }
        /* need to add extra space between wiggles (for now) */
        mi->height = pairwiseHeight;
        }
    else 
        {
        /* pack */
        /* get maf table, containing pairwise alignments for this organism */
        /* display pairwise alignments in this region in dense format */
        vgSetClip(vg, xOff, yOff, width, mi->height);
        tableName = getMafTablename(mi->name, suffix);
	if (!hTableExists(tableName))
	    tableName = getMafTablename(mi->db, suffix);
        if (hTableExists(tableName))
            drawScoreOverview(tableName, mi->height, seqStart, seqEnd, vg, 
                                xOff, yOff, width, font, color, color, tvDense);
        vgUnclip(vg);
        }
    yOff += mi->height;
    }
return TRUE;
}

static boolean drawPairsFromMultipleMaf(struct track *track, 
        int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
/* Draw pairwise display from maf of multiple alignment.
 * Extract pairwise alignments from maf and rescore.
 * This is used only when zoomed-in.
 */
{
struct wigMafItem *miList = track->items, *mi = miList;
int graphHeight = 0;
Color pairColor = (vis == tvFull ? track->ixAltColor : color);
boolean useIrowChains = FALSE;
char option[64];

if (miList == NULL || track->customPt == NULL)
    return FALSE;

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_CHAIN_VAR);
if (cartCgiUsualBoolean(cart, option, FALSE) && 
    trackDbSetting(track->tdb, "irows") != NULL)
        useIrowChains = TRUE;

if (vis == tvFull)
    graphHeight = pairwiseWigHeight(track);

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    struct mafAli *mafList = NULL, *maf, *pairMaf;
    struct mafComp *mcThis, *mcPair = NULL, *mcMaster = NULL;

    if (mi->ix < 0)
        /* ignore item for the score */
        continue;

    /* using maf sequences from file */
    /* create pairwise maf list from the multiple maf */
#ifdef ANNOT_DEBUG
{
struct mafComp *mc;
struct mafAli *mafList = (struct mafAli *)track->customPt;

for(maf=mafList; maf; maf=maf->next)
    {
    mc = maf->components;
    printf("<BR> maf %d %d - ",mc->start,mc->size);
    for (mc=maf->components->next; mc ; mc=mc->next)
	{
	printf("%s %d %c %c ",mc->src,mc->size, mc->leftStatus,mc->rightStatus);
	}
    }
printf("end ||");
fflush(stdout);
}
#endif
    for (maf = (struct mafAli *)track->customPt; maf != NULL; maf = maf->next)
        {
        if ((mcThis = mafMayFindCompPrefix(maf, mi->db, "")) == NULL)
            continue;
	//if (mcPair->srcSize != 0)
        // TODO: replace with a cloneMafComp()
        AllocVar(mcPair);
        mcPair->src = cloneString(mcThis->src);
        mcPair->srcSize = mcThis->srcSize;
        mcPair->strand = mcThis->strand;
        mcPair->start = mcThis->start;
        mcPair->size = mcThis->size;
        mcPair->text = cloneString(mcThis->text);
        mcPair->leftStatus = mcThis->leftStatus;
        mcPair->leftLen = mcThis->leftLen;
        mcPair->rightStatus = mcThis->rightStatus;
        mcPair->rightLen = mcThis->rightLen;

        mcThis = mafFindCompPrefix(maf, database, "");
        AllocVar(mcMaster);
        mcMaster->src = cloneString(mcThis->src);
        mcMaster->srcSize = mcThis->srcSize;
        mcMaster->strand = mcThis->strand;
        mcMaster->start = mcThis->start;
        mcMaster->size = mcThis->size;
        mcMaster->text = cloneString(mcThis->text);
        mcMaster->next = mcPair;

        AllocVar(pairMaf);
        pairMaf->components = mcMaster;
        pairMaf->textSize = maf->textSize;
        slAddHead(&mafList, pairMaf);
        }
    slReverse(&mafList);

    /* compute a graph or density on-the-fly from mafs */
    vgSetClip(vg, xOff, yOff, width, mi->height);
    drawMafRegionDetails(mafList, mi->height, seqStart, seqEnd, vg, xOff, yOff,
                         width, font, pairColor, pairColor, vis, FALSE, 
                         useIrowChains);
    vgUnclip(vg);

    /* need to add extra space between graphs ?? (for now) */
    if (vis == tvFull)
        mi->height = graphHeight;

    yOff += mi->height;
    mafAliFreeList(&mafList);
    }
return TRUE;
}

static boolean wigMafDrawPairwise(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
/* Draw pairwise display for this multiple alignment
 * When zoomed in, use on-the-fly scoring of alignments extracted from multiple
 * When zoomed out:
 *  if "pairwise" setting is on, use pairwise tables (maf or wiggle)
 *      <species>_<suffix> for maf, <species>_<suffix>_wig for wiggle
 *              For full mode, display graph.
 *              for pack mode, display density plot.
 *  if "summary" setting is on, use maf summary table
 *      (saves space, and performs better) */
{
    if (displayZoomedIn(track))
        return drawPairsFromMultipleMaf(track, seqStart, seqEnd, vg,
                                        xOff, yOff, width, font, color, vis);
    if (pairwiseSuffix(track))
        return drawPairsFromPairwiseMafScores(track, seqStart, seqEnd, vg,
                                        xOff, yOff, width, font, color, vis);
    if (summarySetting(track))
        return drawPairsFromSummary(track, seqStart, seqEnd, vg,
                                        xOff, yOff, width, font, color, vis);
    return FALSE;
}

static void alternateBlocksBehindChars(struct vGfx *vg, int x, int y, 
	int width, int height, int charWidth, int charCount, 
	int stripeCharWidth, Color a, Color b)
/* Draw blocks that alternate between color a and b. */
{
int x1,x2 = x + width;
int color = a;
int i;
for (i=0; i<charCount; i += stripeCharWidth)
    {
    x1 = i * width / charCount;
    x2 = (i+stripeCharWidth) * width/charCount;
    vgBox(vg, x1+x, y, x2-x1, height, color);
    if (color == a)
        color = b;
    else
        color = a;
    }
}

void alignSeqToUpperN(char *line)
/* force base chars to upper, ignoring insert counts */
{
int i;
for (i=0; line[i] != 0; i++)
    if (*line == '|')
        /* escape char, indicating insert count */
        i += 2;
    else
        line[i] = toupper(line[i]);
}

void complementUpperAlignSeq(DNA *dna, int size)
/* Complement DNA (not reverse), ignoring insert counts.
 * Assumed to be all upper case bases */
{
int i;
for (i = 0; i < size; i++, dna++)
    {
    if (*dna == 'A')
        *dna = 'T';
    else if (*dna == 'C')
        *dna = 'G';
    else if (*dna == 'G')
        *dna = 'C';
    else if (*dna == 'T')
        *dna = 'A';
    else if (*dna == '|')
        {
        /* escape indicates skip next char -- it is an insert count */
        dna++;
        i++;
        }
    }
}

static int wigMafDrawBases(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis,
	struct wigMafItem *miList)
/* Draw base-by-base view, return new Y offset. */
{
struct wigMafItem *mi;
struct mafAli *mafList, *maf, *sub;
struct mafComp *mc, *mcMaster;
int lineCount = slCount(miList);
char **lines = NULL, *selfLine, *insertLine;
int *insertCounts;
int i, x = xOff, y = yOff;
struct dnaSeq *seq = NULL;
struct hash *miHash = newHash(9);
struct hash *srcHash = newHash(0);
char dbChrom[64];
char buf[1024];
char option[64];
int alignLineLength = winBaseCount * 2;
        /* doubled to allow space for insert counts */
boolean complementBases = cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE);
bool dots;         /* configuration option */
/* this line must be longer than the longest base-level display */
char noAlignment[2000];
boolean useIrowChains = FALSE;
boolean useSpanningChains = FALSE;

/* initialize "no alignment" string to o's */
for (i = 0; i < sizeof noAlignment - 1; i++)
    noAlignment[i] = UNALIGNED_SEQ;

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_DOT_VAR);
dots = cartCgiUsualBoolean(cart, option, FALSE);

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_CHAIN_VAR);
if (cartCgiUsualBoolean(cart, option, FALSE))
    {
    if (trackDbSetting(track->tdb, "irows") != NULL)
        useIrowChains = TRUE;
    else
        useSpanningChains = TRUE;
    }

/* Allocate a line of characters for each item. */
AllocArray(lines, lineCount);
lines[0] = needMem(alignLineLength);
for (i=1; i<lineCount; ++i)
    {
    lines[i] = needMem(alignLineLength);
    memset(lines[i], ' ', alignLineLength - 1);
    }

/* Give nice names to first two. */
insertLine = lines[0];
selfLine = lines[1];

/* Allocate a line for recording gap sizes in reference */
AllocArray(insertCounts, alignLineLength);

/* Load up self-line with DNA */
seq = hChromSeq(chromName, seqStart, seqEnd);
memcpy(selfLine, seq->dna, winBaseCount);
toUpperN(selfLine, winBaseCount);
freeDnaSeq(&seq);

/* Make hash of species items keyed by database. */
i = 0;
for (mi = miList; mi != NULL; mi = mi->next)
    {
    mi->ix = i++;
    if (mi->db != NULL)
	hashAdd(miHash, mi->db, mi);
    }

/* Go through the mafs saving relevant info in lines. */
mafList = track->customPt;
safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);

#ifdef ANNOT_DEBUG
{
struct mafComp *mc;

for(maf=mafList; maf; maf=maf->next)
    {
    mc = maf->components;
    printf("<BR>maf %d %d ",mc->start,mc->size);
    for (mc=maf->components->next; mc ; mc=mc->next)
	{
	printf("%s %d %c %c |",mc->src,mc->size,mc->leftStatus,mc->rightStatus);
	}
    }
fflush(stdout);
}
#endif
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    /* get info about sequences from full alignment,
       for use later, when determining if sequence is unaligned or missing */
    for (mc = maf->components; mc != NULL; mc = mc->next)
        if (!hashFindVal(srcHash, mc->src))
            hashAdd(srcHash, mc->src, maf);

    /* get portion of maf in this window */
    sub = mafSubset(maf, dbChrom, winStart, winEnd);
    if (sub != NULL)
        {
	int subStart,subEnd;
	int lineOffset, subSize;

        /* process alignment for reference ("master") species */
	mcMaster = mafFindComponent(sub, dbChrom);
	if (mcMaster->strand == '-')
	    mafFlipStrand(sub);
	subStart = mcMaster->start;
	subEnd = subStart + mcMaster->size;
	subSize = subEnd - subStart;
	lineOffset = subStart - seqStart;
        processInserts(mcMaster->text, sub, miHash,
                                &insertCounts[lineOffset], subSize);

        /* fill in bases for each species */
        for (mi = miList; mi != NULL; mi = mi->next)
            {
            char *seq;
            bool needToFree = FALSE;
            int size = sub->textSize;
            if (mi->ix == 1)
                /* reference */
                continue;
            if (mi->db == NULL)
                /* not a species line -- it's the gaps line, or... */
                continue;
            if ((mc = mafMayFindCompPrefix(sub, mi->db, "")) == NULL)
                {
                /* no alignment for this species */
                char chainTable[64];
                char *dbUpper;

                if (!useSpanningChains)
                    continue;
                 /* no irows annotation and user has requested chaining, 
                  * so see if a * chain spans this region; 
                  * use it to "extend" alignment */
                 dbUpper = cloneString(mi->db);
                 dbUpper[0] = toupper(dbUpper[0]);
                 safef(chainTable, sizeof chainTable, "%s_chain%s", 
                                             chromName, dbUpper);
                 if (hTableExistsDb(database, chainTable))
                    {
                    struct sqlConnection *conn;
                    char query[128];

                     conn = hAllocConn();
                     safef(query, sizeof query,
                        "SELECT count(*) from %s WHERE tStart < %d AND tEnd > %d",
                                     chainTable, subStart, subEnd);
                     if (sqlQuickNum(conn, query) > 0)
                         processSeq(noAlignment, noAlignment,
                                     sub->textSize, lines[mi->ix],
                                     lineOffset, subSize);
                     hFreeConn(&conn);
                     continue;
                     }
                }
            seq = mc->text;
            if (mc->size == 0)
                {
                /* if no alignment here, but MAF annotation indicates continuity
                 * of flanking alignments, fill with dashes or ='s */
               if (!useIrowChains)
                   continue;
                if ((mc->leftStatus == MAF_CONTIG_STATUS &&
                    mc->rightStatus == MAF_CONTIG_STATUS) ||
                    (mc->leftStatus == MAF_INSERT_STATUS &&
                    mc->rightStatus == MAF_INSERT_STATUS))
                    {
                    char fill = (mc->leftStatus == MAF_CONTIG_STATUS ? 
                                             '-' : MAF_DOUBLE_GAP);
                    seq = needMem(size+1);
                    needToFree = TRUE;
                    memset(seq, fill, size);
                    }
                else
                    continue;
                }
            if (mc->leftStatus == MAF_NEW_STATUS ||
                    mc->rightStatus == MAF_NEW_STATUS ||
                    mc->leftStatus == MAF_MAYBE_NEW_STATUS ||
                    mc->rightStatus == MAF_MAYBE_NEW_STATUS)
                {
                int i;
                char *p;
                boolean maybe = (mc->leftStatus == MAF_MAYBE_NEW_STATUS ||
                                 mc->rightStatus == MAF_MAYBE_NEW_STATUS);
                /* determine if alignment ends on chrom/contig
                 * boundary */
                boolean full = (mc->start == 0 || 
                            (mc->start + mc->size == mc->srcSize));
                if (mc->leftStatus == MAF_NEW_STATUS ||
                    mc->leftStatus == MAF_MAYBE_NEW_STATUS)
                        size++;
                if (mc->rightStatus == MAF_NEW_STATUS ||
                    mc->rightStatus == MAF_MAYBE_NEW_STATUS)
                        size++;
                seq = needMem(size+1);
                needToFree = TRUE;
                for (p = seq, i = 0; i < size; p++, i++)
                    *p = ' ';
                p = seq;
                if (mc->leftStatus == MAF_NEW_STATUS ||
                    mc->leftStatus == MAF_MAYBE_NEW_STATUS)
                    {
                    if (full)
                        *seq = (maybe ? MAF_FULL_MAYBE_BREAK_BEFORE :
                                        MAF_FULL_BREAK_BEFORE);
                    else
                        *seq = (maybe ? MAF_PARTIAL_MAYBE_BREAK_BEFORE :
                                        MAF_PARTIAL_BREAK_BEFORE);
                    subSize++;
                    p++;
                    }
                if (mc->size != 0)
                    strcpy(p, mc->text);
                if (mc->rightStatus == MAF_NEW_STATUS ||
                    mc->rightStatus == MAF_MAYBE_NEW_STATUS)
                    {
                    if (full)
                        *(seq+size-1) = (maybe ? MAF_FULL_MAYBE_BREAK_AFTER :
                                                  MAF_FULL_BREAK_AFTER);
                    else
                        *(seq+size-1) = (maybe ? MAF_PARTIAL_MAYBE_BREAK_AFTER: 
                                                  MAF_PARTIAL_BREAK_AFTER);
                    subSize++;
                    }
                }
            processSeq(seq, mcMaster->text, size,
                                lines[mi->ix], lineOffset, subSize);
            if (needToFree)
                freeMem(seq);
	    }
	}
    mafAliFree(&sub);
    }
/* draw inserts line */
charifyInserts(insertLine, insertCounts, winBaseCount);
mi = miList;
spreadBasesString(vg, x - (width/winBaseCount)/2, y, width, mi->height-1, 
                getOrangeColor(), font, insertLine, winBaseCount);
y += mi->height;

/* draw alternating colors behind base-level alignments */
    {
    int alternateColorBaseCount, alternateColorBaseOffset;
    safef(buf, sizeof(buf), "%s.%s", track->mapName, BASE_COLORS_VAR);
    alternateColorBaseCount = cartCgiUsualInt(cart, buf, 0);
    safef(buf, sizeof(buf), "%s.%s", track->mapName, BASE_COLORS_OFFSET_VAR);
    alternateColorBaseOffset = cartCgiUsualInt(cart, buf, 0);
    if (alternateColorBaseCount != 0)
        {
        int baseWidth = spreadStringCharWidth(width, winBaseCount);
        int colorX = x + alternateColorBaseOffset * baseWidth;
        alternateBlocksBehindChars(vg, colorX, y-1, width, 
                mi->height*(lineCount-2), tl.mWidth, winBaseCount, 
                alternateColorBaseCount, shadesOfSea[0], MG_WHITE);
        }
    }

/* draw base-level alignments */
for (mi = miList->next, i=1; mi != NULL && mi->db != NULL; mi = mi->next, i++)
    {
    char *line;
    line  = lines[i];
    /* TODO: leave lower case in to indicate masking ?
       * NOTE: want to make sure that all sequences are soft-masked
       * if we do this */
    alignSeqToUpperN(line);
    if (complementBases)
        {
        seq = newDnaSeq(line, strlen(line), "");
        complementUpperAlignSeq(seq->dna, seq->size);
        line = seq->dna;
        }
    /* draw sequence letters for alignment */
    vgSetClip(vg, x, y, width, mi->height-1);
    spreadAlignString(vg, x, y, width, mi->height-1, color,
                        font, line, selfLine, winBaseCount, dots);
    vgUnclip(vg);
    y += mi->height;
    }

/* Clean up */
for (i=0; i<lineCount-1; ++i)
    freeMem(lines[i]);
freez(&lines);
hashFree(&miHash);
return y;
}

static int wigMafDrawScoreGraph(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
/* Draw routine for score graph, returns new Y offset */
struct track *wigTrack = track->subtracks;
enum trackVisibility scoreVis;

scoreVis = (vis == tvDense ? tvDense : tvFull);
if (wigTrack != NULL)
    {
    /* draw conservation wiggle */
    wigTrack->ixColor = vgFindRgb(vg, &wigTrack->color);
    wigTrack->ixAltColor = vgFindRgb(vg, &wigTrack->altColor);
    vgSetClip(vg, xOff, yOff, width, wigTotalHeight(wigTrack, scoreVis) - 1);
    wigTrack->drawItems(wigTrack, seqStart, seqEnd, vg, xOff, yOff,
                         width, font, color, scoreVis);
    vgUnclip(vg);
    yOff += wigTotalHeight(wigTrack, scoreVis);
    }
else
    {
    /* draw some kind of graph from multiple alignment */
    int height = tl.fontHeight * 4;
    if (track->customPt != (char *)-1 && track->customPt != NULL)
        {
        /* use mafs */
        drawMafRegionDetails(track->customPt, height, seqStart, seqEnd,
                                vg, xOff, yOff, width, font,
                                color, color, scoreVis, FALSE, FALSE);
        }
    else
        {
        /* use or scored refs from maf table*/
        drawScoreOverview(track->mapName, height, seqStart, seqEnd, vg, 
                            xOff, yOff, width, font, color, color, scoreVis);
        yOff++;
        }
    yOff += height;
    }
return yOff;
}


static void wigMafDraw(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for wigmaf type tracks */
{
int y = yOff;
y = wigMafDrawScoreGraph(track, seqStart, seqEnd, vg, xOff, y, width,
                                font, color, vis);
if (zoomedToBaseLevel)
    {
    struct wigMafItem *wiList = track->items;
    if (track->subtracks != NULL)
        wiList = wiList->next;
    y = wigMafDrawBases(track, seqStart, seqEnd, vg, xOff, y, width, font,
                                color, vis, wiList);
    }
else 
    {
    if (vis == tvFull || vis == tvPack)
        wigMafDrawPairwise(track, seqStart, seqEnd, vg, xOff, y, 
                                width, font, color, vis);
    }
mapBoxHc(seqStart, seqEnd, xOff, yOff, width, track->height, track->mapName, 
            track->mapName, NULL);
}

void wigMafMethods(struct track *track, struct trackDb *tdb,
                                        int wordCount, char *words[])
/* Make track for maf multiple alignment. */
{
char *wigTable;
struct track *wigTrack;
int i;
char *savedType;
char option[64];
struct dyString *wigType = newDyString(64);
track->loadItems = wigMafLoad;
track->freeItems = wigMafFree;
track->drawItems = wigMafDraw;
track->itemName = wigMafItemName;
track->mapItemName = wigMafItemName;
track->totalHeight = wigMafTotalHeight;
track->itemHeight = wigMafItemHeight;
track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
track->itemLabelColor = wigMafItemLabelColor;
track->mapsSelf = TRUE;
//track->canPack = TRUE;

safef(option, sizeof(option), "%s.%s", track->mapName, MAF_CHAIN_VAR);

if ((wigTable = trackDbSetting(tdb, "wiggle")) != NULL)
    if (hTableExists(wigTable))
        {
        //  manufacture and initialize wiggle subtrack
        /* CAUTION: this code is very interdependent with
           hgTracks.c:fillInFromType()
           Also, both the main track and subtrack share the same tdb */
        // restore "type" line, but change type to "wig"
        savedType = tdb->type;
	dyStringClear(wigType);
        dyStringPrintf(wigType, "type wig ");
        for (i = 1; i < wordCount; i++)
            {
            dyStringPrintf(wigType, "%s ", words[i]);
            }
        dyStringPrintf(wigType, "\n");
        tdb->type = cloneString(wigType->string);
        wigTrack = trackFromTrackDb(tdb);
        tdb->type = savedType;

        // replace tablename with wiggle table from "wiggle" setting
        wigTrack->mapName = cloneString(wigTable);

        // setup wiggle methods in subtrack
        wigMethods(wigTrack, tdb, wordCount, words);

        wigTrack->mapsSelf = FALSE;
        wigTrack->drawLeftLabels = NULL;
        track->subtracks = wigTrack;
        track->subtracks->next = NULL;
        }
dyStringFree(&wigType);
}

