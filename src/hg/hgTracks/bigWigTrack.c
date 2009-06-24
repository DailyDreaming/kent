/* bigWigTrack - stuff to handle loading and display of bigWig type tracks in browser.   */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
// #include "wiggle.h"
// #include "scoredRef.h"
// #include "customTrack.h"
#include "localmem.h"
#include "wigCommon.h"
#include "bbiFile.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bigWigTrack.c,v 1.6 2009/06/24 18:48:00 angie Exp $";

static void bigWigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
/* Allocate predraw area. */
int preDrawZero, preDrawSize;
struct preDrawElement *preDraw = initPreDraw(width, &preDrawSize, &preDrawZero);

/* Get summary info from bigWig */
int summarySize = width;
struct bbiSummaryElement *summary;
AllocArray(summary, summarySize);
char *wigFileName = tg->bbiFileName;
if (bigWigSummaryArrayExtended(wigFileName, chromName, winStart, winEnd, summarySize, summary))
    {
    /* Convert format to predraw */
    int i;
    for (i=0; i<summarySize; ++i)
        {
	struct preDrawElement *pe = &preDraw[i + preDrawZero];
	struct bbiSummaryElement *be = &summary[i];
	pe->count = be->validCount;
	pe->min = be->minVal;
	pe->max = be->maxVal;
	pe->sumData = be->sumData;
	pe->sumSquares = be->sumSquares;
	}

    /* Call actual graphing routine. */
    wigDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
		   preDraw, preDrawZero, preDrawSize, &tg->graphUpperLimit, &tg->graphLowerLimit);
    }

freeMem(preDraw);
freeMem(summary);
}

static void bigWigLoadItems(struct track *tg)
/* Fill up tg->items with bedGraphItems derived from a bigWig file */
{
if (tg->bbiFileName == NULL)
    {
    /* Figure out bigWig file name. */
    struct sqlConnection *conn = hAllocConn(database);
    tg->bbiFileName = bbiNameFromTable(conn, tg->mapName);
    hFreeConn(&conn);
    }
}

void bigWigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Set up bigWig methods. */
{
bedGraphMethods(track, tdb, wordCount, words);
track->loadItems = bigWigLoadItems;
track->drawItems = bigWigDrawItems;
}
