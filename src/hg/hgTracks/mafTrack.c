/* mafTrack - stuff to handle loading and display of
 * maf type tracks in browser. Mafs are multiple alignments. 
 * This also handles axt's, mostly by convincing them they
 * are really mafs.... */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "maf.h"
#include "scoredRef.h"
#include "hgMaf.h"

static char const rcsid[] = "$Id: mafTrack.c,v 1.29 2005/03/10 00:15:51 kate Exp $";

struct mafItem
/* A maf track item. */
    {
    struct mafItem *next;
    char *name;		/* Common name */
    char *db;		/* Database */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    };

static void mafItemFree(struct mafItem **pEl)
/* Free up a mafItem. */
{
struct mafItem *el = *pEl;
if (el != NULL)
    {
    freeMem(el->name);
    freeMem(el->db);
    freez(pEl);
    }
}

void mafItemFreeList(struct mafItem **pList)
/* Free a list of dynamically allocated mafItem's */
{
struct mafItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    mafItemFree(&el);
    }
*pList = NULL;
}

struct mafAli *mafOrAxtLoadInRegion(struct sqlConnection *conn, 
	struct track *tg, char *chrom, 
	int start, int end, boolean isAxt)
/* Load mafs from region, either from maf or axt file. */
{
if (isAxt)
    {
    struct hash *qSizeHash = hChromSizeHash(tg->otherDb);
    struct mafAli *mafList = 
            axtLoadAsMafInRegion(conn, tg->mapName, chrom, start, end,
                        database, tg->otherDb, hChromSize(chrom), qSizeHash);
    hashFree(&qSizeHash);
    return mafList;
    }
else
    return mafLoadInRegion(conn, tg->mapName, chrom, start, end);
}

static struct mafItem *baseByBaseItems(struct track *tg, int scoreHeight)
/* Make up base-by-base track items. */
{
struct mafItem *miList = NULL, *mi;
struct mafAli *maf;
char *myOrg = hOrganism(database);
char buf[64];
char *otherOrganism;

/* Make up item that will show inserts in this organism. */
AllocVar(mi);
mi->name = cloneString("Hidden Gaps");
mi->height = tl.fontHeight;
slAddHead(&miList, mi);

/* Make up item for this organism. */
AllocVar(mi);
mi->name = myOrg;
mi->height = tl.fontHeight;
mi->db = cloneString(database);
slAddHead(&miList, mi);

/* Make up items for other organisms by scanning through
 * all mafs and looking at database prefix to source. */
    {
    struct hash *hash = newHash(8);	/* keyed by database. */
    hashAdd(hash, mi->db, mi);		/* Add in current organism. */
    for (maf = tg->customPt; maf != NULL; maf = maf->next)
        {
	struct mafComp *mc;
	for (mc = maf->components; mc != NULL; mc = mc->next)
	    {
	    mafSrcDb(mc->src, buf, sizeof(buf));
	    if (hashLookup(hash, buf) == NULL)
	        {
		AllocVar(mi);
		mi->db = cloneString(buf);
                otherOrganism = hOrganism(mi->db);
                mi->name = 
                    (otherOrganism == NULL ? cloneString(buf) : otherOrganism);
		mi->height = tl.fontHeight;
		slAddHead(&miList, mi);
		hashAdd(hash, mi->db, mi);
		}
	    }
	}
    hashFree(&hash);
    }
/* Make up item that will show the score as grayscale. */
AllocVar(mi);
mi->name = cloneString("Score");
mi->height = scoreHeight;
slAddHead(&miList, mi);

slReverse(&miList);
return miList;
}

static struct mafItem *pairwiseItems(struct track *tg, int scoreHeight)
/* Make up pairwise alignment items for each 
   "other species" in the multiple alignment. */
{
struct mafItem *miList = NULL, *mi;
struct mafAli *maf;
char buf[64];
char *otherOrganism;

/* Make up item for wiggle */
AllocVar(mi);
mi->name = cloneString("Score");
mi->height = scoreHeight;
slAddHead(&miList, mi);

if (trackDbSetting(tg->tdb, "pairwise") != NULL)

/* Make up items for other organisms by scanning through
 * all mafs and looking at database prefix to source. */
    {
    // TODO: share code with baseByBaseItems
    struct hash *hash = newHash(8);	/* keyed by database. */
    for (maf = tg->customPt; maf != NULL; maf = maf->next)
        {
	struct mafComp *mc;
        boolean isMyOrg = TRUE;
	for (mc = maf->components; mc != NULL; mc = mc->next)
	    {
            if (isMyOrg) 
                {
                /* skip first maf component (this organism) */
                isMyOrg = FALSE;
                continue;
                }
	    mafSrcDb(mc->src, buf, sizeof(buf));
	    if (hashLookup(hash, buf) == NULL)
	        {
		AllocVar(mi);
		mi->db = cloneString(buf);
                otherOrganism = hOrganism(mi->db);
                mi->name = 
                    (otherOrganism == NULL ? cloneString(buf) : otherOrganism);
		mi->height = tl.fontHeight;
		slAddHead(&miList, mi);
		hashAdd(hash, mi->db, mi);
		}
	    }
	}
    hashFree(&hash);
    }
slReverse(&miList);
return miList;
}

static struct mafItem *mafItems(struct track *tg, int scoreHeight,
                                         boolean isBaseLevel, boolean isAxt)
/* Load up items for full mode */
{
struct mafAli *mafList = NULL;
struct mafItem *miList = NULL;
struct sqlConnection *conn = hAllocConn();

/* Load up mafs and store in track so drawer doesn't have
 * to do it again. */
mafList = mafOrAxtLoadInRegion(conn, tg, chromName, winStart, winEnd, isAxt);
tg->customPt = mafList;

/* Make up tracks for display. */
if (isBaseLevel)
    miList = baseByBaseItems(tg, scoreHeight);
else
    {
    miList = pairwiseItems(tg, scoreHeight);
    }
hFreeConn(&conn);
return miList;
}

static void mafOrAxtLoad(struct track *tg, boolean isAxt)
/* Load up maf or axt tracks.  What this will do depends on
 * the zoom level and the display density. */
{
struct mafItem *miList = NULL;
int scoreHeight = tl.fontHeight;

/* Create item list and set height depending
 * on display type. */
if (tg->visibility == tvFull)
    scoreHeight *= 4;
if (zoomedToBaseLevel)
    {
    miList = mafItems(tg, scoreHeight, zoomedToBaseLevel, isAxt);
    }
else
    {
    if (tg->visibility == tvFull && winEnd - winStart < 1000000)
        {
        /* currently implemented only for medium zoom out */
        miList = mafItems(tg, scoreHeight, FALSE, isAxt);
        }
    else
        {
        AllocVar(miList);
        miList->name = cloneString(tg->shortLabel);
        miList->height = scoreHeight;
        }
    }
tg->items = miList;
}

static void mafLoad(struct track *tg)
/* Load up maf tracks.  */
{
mafOrAxtLoad(tg, FALSE);
}



static int mafTotalHeight(struct track *tg, 
	enum trackVisibility vis)
/* Return total height of maf track.  */
{
struct mafItem *mi;
int total = 0;
for (mi = tg->items; mi != NULL; mi = mi->next)
    total += mi->height;
tg->height =  total;
return tg->height;
}

static int mafItemHeight(struct track *tg, void *item)
/* Return total height of maf track.  */
{
struct mafItem *mi = item;
return mi->height;
}

static void mafFree(struct track *tg)
/* Free up mafGroup items. */
{
mafAliFreeList((struct mafAli **)&tg->customPt);
mafItemFreeList((struct mafItem **)&tg->items);
}

static char *mafName(struct track *tg, void *item)
/* Return name of maf level track. */
{
struct mafItem *mi = item;
return mi->name;
}

static void processInserts(char *text, int textSize, 
	char *insertLine, int baseCount)
/* Make up insert line - it has a ' ' if there is no space
 * before char, and a '-' if there is.  */
{
int i, baseIx = 0;
char c;
for (i=0; i<textSize && baseIx < baseCount; ++i)
    {
    c = text[i];
    if (c == '-')
	{
	unsigned char b = insertLine[baseIx];
	if (b < 255)
	    insertLine[baseIx] = b+1;
	}
    else
        baseIx += 1;
    }
}

static void charifyInserts(char *insertLine, int size)
/* Convert insert line from counts to characters. */
{
int i;
char c;
for (i=0; i<size; ++i)
    {
    unsigned char b = insertLine[i];
    if (b == 0)
       c = ' ';
    else if (b <= 9)
       c = b + '0';
    else
       c = '+';
    insertLine[i] = c;
    }
}

static void processOtherSeq(char *text, char *masterText, int textSize,
	char *outLine, int outSize)
/* Copy text to outLine, suppressing copy where there are dashes
 * in masterText.  This effectively projects the alignment onto
 * the master genome. */
{
int i, outIx = 0;
for (i=0; i<textSize && outIx < outSize;  ++i)
    {
    if (masterText[i] != '-')
        {
	outLine[outIx] = text[i];
	outIx += 1;
	}
    }
}

static void setIxMafAlign(int ix, int *ixMafAli, int count)
/* make an array of alignment indices, one per base */
{
    int i;
    for (i = 0; i < count; i++)
        {
        *ixMafAli++ = ix;
        }
}

static int getIxMafAli(int *ixMafAli, int position, int maxPos)
/* get alignment index for a base position */
{
    if (position > maxPos)
        {
        return 0;
        }
    return *(ixMafAli + position);
}

static void getNormalizedScores(struct mafAli *maf, char *masterText,
	double *scores, int scoreCount)
/* Make an array of normalized scores, one per base. */
{
int textIx, outIx = 0;
double maxScore, minScore;
double scoreScale;
double score;

mafColMinMaxScore(maf, &minScore, &maxScore);
scoreScale = 1.0/(maxScore - minScore);
for (textIx = 0; textIx < maf->textSize; ++textIx)
    {
    if (masterText[textIx] != '-')
        {
	score = (mafScoreRangeMultiz(maf, textIx, 1) - minScore)*scoreScale;
	if (score < 0.0) score = 0.0;
	if (score > 1.0) score = 1.0;
	scores[outIx] = score;
	++outIx;
	}
    }
}


void mafFillInPixelScores(struct mafAli *maf, struct mafComp *mcMaster,
	double *scores, int numScores)
/* Calculate one score per pixel normalized to be between 0.0 and 1.0. */
{
int i,j;
double score, minScore, maxScore, scoreScale;
int textSize = maf->textSize;
int masterSize = mcMaster->size;
char *masterText = mcMaster->text;

mafColMinMaxScore(maf, &minScore, &maxScore);
if ((maxScore - minScore) < 0.0001)
    scoreScale = 0.0001;
else
    scoreScale = 1.0/(maxScore - minScore);
if (numScores >= masterSize)	 /* More pixels than bases */
    {
    int x1,x2;
    int masterPos = 0;
    for (i=0; i<textSize; ++i)
        {
	if (masterText[i] != '-')
	    {
	    score = mafScoreRangeMultiz(maf, i, 1);
	    score = (score - minScore) * scoreScale;
	    if (score < 0.0) score = 0.0;
	    if (score > 1.0) score = 1.0;
	    x1 = masterPos*numScores/masterSize;
	    x2 = (masterPos+1)*numScores/masterSize;
	    for (j=x1; j<x2; ++j)
	        scores[j] = score;
	    ++masterPos;
	    }
	}
    }
else	
    /* More bases than pixels. */
    /* This handles the case where you're
      fitting  M bases of alignment into N pixels, and
      M is not a perfect multiple of N.  That is it
      handles the case where you want to average over
      2 bases, then 3 bases, then 2 bases, then 3 bases
      (which would be the case where N = 4 and M = 10). */
    {
    int b1=0, b2;       /* start and end in the reference (master) genome */
    int deltaB;
    int t1=0, t2;       /* start and end in the maf text.  The spacing between
                         * them may be larger than b1,b2 due to '-' chars in 
                         * the * row of the alignment corresponding to 
                         * the master genome. */ 
    int deltaT;
    for (i=0; i<numScores; ++i)
        {
	b2 = (i+1)*masterSize/numScores;
	deltaB = b2 - b1;
	for (t2 = t1; t2 < textSize; ++t2)
	    {
	    if (deltaB <= 0)
		break;
	    if (masterText[t2] != '-')
		deltaB -= 1;
	    }
	deltaT = t2 - t1;

        /* Take the score over the relevant range of text symbols in the maf,
         * and divide it by the bases we cover in the master genome to 
         * get a normalized by base score. */ 
	score = mafScoreRangeMultiz(maf, t1, deltaT)/(b2-b1);

        /* Scale the score so that it is between 0 and 1 */ 
	score = (score - minScore) * scoreScale;
	if (score < 0.0) score = 0.0;
	if (score > 1.0) score = 1.0;
	scores[i] = score;
	b1 = b2;
	t1 = t2;
	}
    }
}

static void mafDrawOverview(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw wiggle-plot based on overall maf scores rather than
 * computing them for sections.  For this routine we don't
 * need to actually load the mafs, it's sufficient to load
 * the scoredRefs. */
{
char **row;
int rowOffset;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = hRangeQuery(conn, tg->mapName, chromName, 
    seqStart, seqEnd, NULL, &rowOffset);
double scale = scaleForPixels(width);
int x1,x2,y,w;
struct mafItem *mi = tg->items;
int height1 = mi->height-2;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct scoredRef ref;
    scoredRefStaticLoad(row + rowOffset, &ref);
    x1 = round((ref.chromStart - seqStart)*scale);
    x2 = round((ref.chromEnd - seqStart)*scale);
    w = x2-x1;
    if (w < 1) w = 1;
    if (vis == tvFull)
	{
	y = ref.score * height1;
	vgBox(vg, x1 + xOff, yOff + height1 - y, w, y+1, color);
	}
    else
	{
	int shade = ref.score * maxShade;
	Color c;
	if ((shade < 0) || (shade >= maxShade))
	    shade = 0;
	c = shadesOfGray[shade];
	vgBox(vg, x1 + xOff, yOff, w, tg->heightPer, c);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void drawMafRegionDetails(struct mafAli *mafList, int height,
        int seqStart, int seqEnd, struct vGfx *vg, int xOff, int yOff,
        int width, MgFont *font, Color color, Color altColor,
        enum trackVisibility vis, boolean isAxt)
/* Draw wiggle/density plot based on scoring things on the fly. */
{
struct mafAli *full, *sub = NULL, *maf = NULL;
struct mafComp *mcMaster;
char dbChrom[64];
int height1 = height-2;
int ixMafAli = 0;       /* alignment index, to allow alternating color */

safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);
for (full = mafList; full != NULL; full = full->next)
    {
    int mafPixelStart, mafPixelEnd, mafPixelWidth;
    int i;
    double *pixelScores = NULL;
    if (mafNeedSubset(full, dbChrom, seqStart, seqEnd))
        sub = maf = mafSubset(full, dbChrom, seqStart, seqEnd);
    else
        maf = full;
    if (maf != NULL)
	{
        ixMafAli++;
	mcMaster = mafFindComponent(maf, dbChrom);
	if (mcMaster->strand == '-')
	    mafFlipStrand(maf);
	mafPixelStart = (mcMaster->start - seqStart) * width/winBaseCount;
	mafPixelEnd = (mcMaster->start + mcMaster->size - seqStart) 
	    * width/winBaseCount;
	mafPixelWidth = mafPixelEnd-mafPixelStart;
	if (mafPixelWidth < 1) mafPixelWidth = 1;
	AllocArray(pixelScores, mafPixelWidth);
	mafFillInPixelScores(maf, mcMaster, pixelScores, mafPixelWidth);
	for (i=0; i<mafPixelWidth; ++i)
	    {
	    if (vis == tvFull)
		{
		int y = pixelScores[i] * height1;
		vgBox(vg, i+mafPixelStart+xOff, yOff + height1 - y, 
		    1, y+1, (ixMafAli % 2) ? color : altColor);
		}
	    else
	        {
		int shade = pixelScores[i] * maxShade;
		Color c = shadesOfGray[shade];
		vgBox(vg, i+mafPixelStart+xOff, yOff, 
		    1, height-1, c);
		}
	    }
	freez(&pixelScores);
	}
    mafAliFree(&sub);
    }
}

static void mafDrawDetails(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis, boolean isAxt)
/* Draw wiggle/density plot based on scoring things on the fly. */
{
struct mafAli *mafList;
struct sqlConnection *conn = hAllocConn();
struct mafItem *miList = tg->items, *mi = miList;
char *suffix;

mafList = tg->customPt;
if (mafList == NULL)
    mafList = mafOrAxtLoadInRegion(conn, tg, chromName, 
                                                seqStart, seqEnd, isAxt);

/* display the multiple alignment in this region */
drawMafRegionDetails(mafList, mi->height, seqStart, seqEnd, vg, xOff, yOff,
                             width, font, color, tg->ixAltColor,  vis, isAxt);
mafAliFreeList(&mafList);

yOff += mi->height + 1;
if (vis == tvFull)
    {
    while ((mi = mi->next) != NULL)
        {
        /* construct pairwise table name for this organism */
        /* if there's a value for the "pairwise" trackDb setting, use this
         * to construct the tablename, otherwise, use the track name */
        char mafTable[64];
        if ((suffix = trackDbSetting(tg->tdb, "pairwise")) == NULL ||
            *suffix == 0)
                suffix = tg->mapName;
        safef(mafTable, sizeof(mafTable), "%s_%s", mi->name, suffix);
        if (!hTableExistsDb(database, mafTable))
            continue;
        mafList = mafLoadInRegion(conn, mafTable, chromName, seqStart, seqEnd);
        /* display pairwise alignments in this region in dense format */
        drawMafRegionDetails(mafList, mi->height, seqStart, seqEnd, 
                                 vg, xOff, yOff, width, font,
                                 color, tg->ixAltColor, tvDense, isAxt);
        yOff += mi->height + 1;
        mafAliFreeList(&mafList);
        }
    }
hFreeConn(&conn);
}

static void mafDrawGraphic(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis, boolean isAxt)
/* Draw wiggle or density plot, not base-by-base. */
{
int seqSize = seqEnd - seqStart;
if (seqSize >= 1000000)
    {
    mafDrawOverview(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, 
            color, vis);
    }
else
    {
    mafDrawDetails(tg, seqStart, seqEnd, vg, 
                        xOff, yOff, width, font, color, vis, isAxt);
    }
// density gradient of blastz's
// mafDrawPairwise(tg, seqStart, seqEnd, vg, xOff, yOff, font, width, color, vis);
}

static void mafDrawBases(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw base-by-base view. */
{
struct mafItem *miList = tg->items, *mi;
struct mafAli *mafList = tg->customPt, *maf, *sub;
int lineCount = slCount(miList);
char **lines = NULL, *selfLine, *insertLine;
double *scores;  /* per base scores */
int *ixMafAli;   /* per base alignment index */
double scoreScale;
int i, y = yOff;
struct dnaSeq *seq = NULL;
struct hash *miHash = newHash(9);
char dbChrom[64];

/* Allocate a line of characters for each item. */
AllocArray(lines, lineCount-1);
lines[0] = needMem(winBaseCount+1);
for (i=1; i<lineCount-1; ++i)
    {
    lines[i] = needMem(winBaseCount+1);
    memset(lines[i], ' ', winBaseCount);
    }
AllocArray(scores, winBaseCount);
AllocArray(ixMafAli, winBaseCount);

/* Give nice names to first three. */
insertLine = lines[0];
selfLine = lines[1];

/* Load up self-line with DNA */
seq = hChromSeq(chromName, seqStart, seqEnd);
memcpy(selfLine, seq->dna, winBaseCount);
toUpperN(selfLine, winBaseCount);
freeDnaSeq(&seq);

/* Make hash of items keyed by database. */
i = 0;
for (mi = miList; mi != NULL; mi = mi->next)
    {
    mi->ix = i++;
    if (mi->db != NULL)
	hashAdd(miHash, mi->db, mi);
    }

/* Go through the mafs saving relevant info in lines. */
safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);
i = 0;
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    sub = mafSubset(maf, dbChrom, winStart, winEnd);
    if (sub != NULL)
        {
	struct mafComp *mc, *mcMaster;
	char db[64];
	int subStart,subEnd;
	int lineOffset, subSize;

        i++;
	mcMaster = mafFindComponent(sub, dbChrom);
	if (mcMaster->strand == '-')
	    mafFlipStrand(sub);
	subStart = mcMaster->start;
	subEnd = subStart + mcMaster->size;
	subSize = subEnd - subStart;
	lineOffset = subStart - seqStart;
	for (mc = sub->components; mc != NULL; mc = mc->next)
	    {
            mafSrcDb(mc->src, db, sizeof(db));
	    if (mc == mcMaster)
		{
		processInserts(mc->text, sub->textSize, 
			insertLine+lineOffset, subSize);
		}
	    else
	        {
		mi = hashMustFindVal(miHash, db);
		processOtherSeq(mc->text, mcMaster->text, sub->textSize, 
			lines[mi->ix] + lineOffset, subSize);
		}
	    }
	getNormalizedScores(sub, mcMaster->text, scores + lineOffset, subSize);
        setIxMafAlign(i, ixMafAli + lineOffset, subSize);
	}
    mafAliFree(&sub);
    }

/* Convert insert line from counts to characters. */
charifyInserts(insertLine, winBaseCount);
for (mi = miList, i=0; mi->next != NULL; mi = mi->next, ++i)
    {
    char *line = lines[i];
    int x = xOff;
    if (line == insertLine)
	x -= (width/winBaseCount)/2;
    spreadBasesString(vg, x, y, width, mi->height-1, color, font, 
    	line, winBaseCount);
    y += mi->height;
    }

/* Draw score line. */
if (vis == tvDense)
    scoreScale = (maxShade);
else
    scoreScale = (mi->height-2);
for (i=0; i<winBaseCount; ++i)
    {
    int x1 = i * width/winBaseCount;
    int x2 = (i+1) * width/winBaseCount;
    if (vis == tvDense)
	{
	int shade = scores[i] * scoreScale;
	int color = shadesOfGray[shade];
	vgBox(vg, x1+xOff, y, x2-x1, mi->height-1, color);
	}
    else
        {
	int wiggleH = scores[i] * scoreScale;
	vgBox(vg, x1+xOff, y + mi->height-2 - wiggleH, 
	    x2-x1, wiggleH+1, 
            getIxMafAli(ixMafAli, i, winBaseCount) % 2 ? color : tg->ixAltColor);
	}
    }
y += mi->height;

/* Clean up */
for (i=0; i<lineCount-1; ++i)
    freeMem(lines[i]);
freez(&lines);
freez(&scores);
hashFree(&miHash);
}

static void mafOrAxtDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis, 
	boolean isAxt)
/* Draw routine for maf or axt type tracks.  This will load
 * the items as well as drawing them. */
{
if (zoomedToBaseLevel)
    mafDrawBases(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
else 
    {
    mafDrawGraphic(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, 
                    color, vis, isAxt);
    }
mapBoxHc(seqStart, seqEnd, xOff, yOff, width, tg->height, tg->mapName, 
    tg->mapName, NULL);
}

static void mafDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for mafAlign type tracks.  This will load
 * the items as well as drawing them. */
{
mafOrAxtDraw(tg,seqStart,seqEnd,vg,xOff,yOff,width,font,color,vis,FALSE);
}

void mafMethods(struct track *tg)
/* Make track group for maf multiple alignment. */
{
tg->loadItems = mafLoad;
tg->freeItems = mafFree;
tg->drawItems = mafDraw;
tg->itemName = mafName;
tg->mapItemName = mafName;
tg->totalHeight = mafTotalHeight;
tg->itemHeight = mafItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
}

static void axtLoad(struct track *tg)
/* Load up axt tracks.  What this will do depends on
 * the zoom level and the display density. */
{
mafOrAxtLoad(tg, TRUE);
}

static void axtDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for axt type tracks.  This will load
 * the items as well as drawing them. */
{
mafOrAxtDraw(tg,seqStart,seqEnd,vg,xOff,yOff,width,font,color,vis,TRUE);
}

void axtMethods(struct track *tg, char *otherDb)
/* Make track group for axt alignments. */
{
tg->otherDb = cloneString(otherDb);
tg->loadItems = axtLoad;
tg->freeItems = mafFree;
tg->drawItems = axtDraw;
tg->itemName = mafName;
tg->mapItemName = mafName;
tg->totalHeight = mafTotalHeight;
tg->itemHeight = mafItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
}

