/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* fuzzyFind.h - This is the interface to the fuzzyFind
 * DNA sequence alligner.  This just returns a single
 * allignment - the one the algorithm thinks is best.
 * The algorithm is heuristic, but pretty good.  (See
 * comments in the fuzzyFind.c file for more details.) It
 * is not good for finding distant homologies, but it
 * will fairly reliably allign a somewhat noisy cDNA
 * sequence with genomic sequence.
 *
 * The main data structure is the ffAli - which is
 * a node in a doubly linked list.  The finder algorithm
 * returns a pointer to the leftmost node in the list.
 * When you're done with the alignment you can dispose
 * of it via ffFreeAli.
 *
 * The finder supports three levels of stringency.
 * Generally you're best off using "ffTight".  "ffLoose"
 * will allow for more distant matches, but at the 
 * expense of very often taking several seconds to
 * return a garbage allignment.  "ffExact" requires
 * an exact match - which is quick, but in the
 * real world not so often useful.
 *
 * If you want to compare allignments use ffScore.
 */

#ifndef FUZZYFIND_H
#define FUZZYFIND_H

#ifndef MEMGFX_H
#include "memgfx.h"
#endif 

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef LOCALMEM_H
#include "localmem.h"
#endif

#ifndef ALITYPE_H
#include "aliType.h"
#endif

struct ffAli
/* Node of a doubly linked list that will contain one
 * allignment. Contains information on a matching
 * set of DNA between needle and haystack. */
    {
    struct ffAli *left;   /* Neighboring intervals. */
    struct ffAli *right;
    char *nStart, *nEnd;          /* Needle start and end. (1/2 open interval) */
    char *hStart, *hEnd;          /* Haystack start and end. */
    int startGood, endGood; /* Number that match perfectly on ends. */
    };

#define ffIntronMax 750000	/* Maximum intron size */

/************* lib/ffAli.c routines - using alignments ************/

/* FFA */void ffFreeAli(struct ffAli **pAli);
/* Dispose of memory gotten from fuzzyFind(). */

/* FFA */int ffOneIntronOrientation(struct ffAli *left, struct ffAli *right);
/* Return 1 for GT/AG intron between left and right, -1 for CT/AC, 0 for no
 * intron. */

/* FFA */int ffIntronOrientation(struct ffAli *ali);
/* Return + for positive orientation overall, - for negative,
 * 0 if can't tell. */

/* FFA */struct ffAli *ffRightmost(struct ffAli *ff);
/* Return rightmost block of alignment. */

/* FFA */struct ffAli *ffMakeRightLinks(struct ffAli *rightMost);
/* Given a pointer to the rightmost block in an alignment
 * which has all of the left pointers filled in, fill in
 * the right pointers and return the leftmost block. */
 
/* FFA */void ffCountGoodEnds(struct ffAli *aliList);
/* Fill in the goodEnd and badEnd scores. */

/* FFA */int ffAliCount(struct ffAli *d);
/* How many blocks in alignment? */

/* FFA */struct ffAli *ffAliFromSym(int symCount, char *nSym, char *hSym,
	struct lm *lm, char *nStart, char *hStart);
/* Convert symbol representation of alignments (letters plus '-')
 * to ffAli representation.  If lm is nonNULL, ffAli result 
 * will be lmAlloced, else it will be needMemed. This routine
 * depends on nSym/hSym being zero terminated. */

/************* lib/ffScore.c routines - scoring alignments ************/

/* FFS */int ffScoreCdna(struct ffAli *ali);
/* Return score of alignment.  A perfect allignment score will
 * be the number of bases in needle. */

/* FFS */int ffScore(struct ffAli *ali, enum ffStringency stringency);
/* Score DNA based alignment. */

/* FFS */int ffScoreProtein(struct ffAli *ali, enum ffStringency stringency);
/* Figure out overall score of protein alignment. */

/* FFS */int ffScoreSomething(struct ffAli *ali, enum ffStringency stringency,
   boolean isProt);
/* Score any alignment. */

/* FFS */int ffScoreSomeAlis(struct ffAli *ali, int count, enum ffStringency stringency);
/* Figure out score of count consecutive alis. */

/* FFS */int ffCalcGapPenalty(int hGap, int nGap, enum ffStringency stringency);
/* Return gap penalty for given h and n gaps. */

/* FFS */int ffCalcCdnaGapPenalty(int hGap, int nGap);
/* Return gap penalty for given h and n gaps in cDNA. */

/* FFS */int ffGapPenalty(struct ffAli *ali, struct ffAli *right, enum ffStringency stringency);
/* Calculate gap penaltly for alignment. */

/* FFS */int ffCdnaGapPenalty(struct ffAli *ali, struct ffAli *right);
/* Calculate gap penaltly for cdna alignment. */

/************* jkOwnLib/ffAliHelp -helpers for alignment producers. ****************/

/* FAA */boolean ffSlideIntrons(struct ffAli *ali);
/* Slide introns (or spaces between aligned blocks)
 * to match consensus.  Return TRUE if any slid. */

/* FAA */boolean ffSlideOrientedIntrons(struct ffAli *ali, int orient);
/* Slide introns (or spaces between aligned blocks)
 * to match consensus on given strand (usually from ffIntronOrientation). */

/* FAA */struct ffAli *ffRemoveEmptyAlis(struct ffAli *ali, boolean doFree);
/* Remove empty blocks from list. Optionally free empties too. */

/* FAA */struct ffAli *ffMergeHayOverlaps(struct ffAli *ali);
/* Remove overlaps in haystack that perfectly abut in needle.
 * These are transformed into perfectly abutting haystacks
 * that have a gap in the needle. */

/* FAA */struct ffAli *ffMergeNeedleAlis(struct ffAli *ali, boolean doFree);
/* Remove overlapping areas needle in alignment. Assumes ali is sorted on
 * ascending nStart field. Also merge perfectly abutting neighbors.*/

/* FAA */void ffExpandExactRight(struct ffAli *ali, DNA *needleEnd, DNA *hayEnd);
/* Expand aligned segment to right as far as can exactly. */

/* FAA */void ffExpandExactLeft(struct ffAli *ali, DNA *needleStart, DNA *hayStart);
/* Expand aligned segment to left as far as can exactly. */

/* FAA */struct ffAli *ffMergeClose(struct ffAli *aliList);
/* Remove overlapping areas needle in alignment. Assumes ali is sorted on
 * ascending nStart field. Also merge perfectly abutting neighbors or
 * ones that could be merged at the expense of just a few mismatches.*/

/* FAA */void ffAliSort(struct ffAli **pList, 
	int (*compare )(const void *elem1,  const void *elem2));
/* Sort a doubly linked list of ffAlis. */

/* FAA */void ffCat(struct ffAli **pA, struct ffAli **pB);
/* Concatenate B to the end of A. Eat up second list
 * in process. */

/* FAA */int ffCmpHitsHayFirst(const void *va, const void *vb);
/* Compare function to sort hit array by ascending
 * target offset followed by ascending query offset. */

/* FAA */int ffCmpHitsNeedleFirst(const void *va, const void *vb);
/* Compare function to sort hit array by ascending
 * query offset followed by ascending target offset. */

/************* jkOwnLib/fuzzyFind - old local cDNA alignment. ****************/

/* FZF */struct ffAli *ffFind(DNA *needleStart, DNA *needleEnd, DNA *hayStart, DNA *hayEnd,
    enum ffStringency stringency);
/* Return an allignment of needle in haystack. (Returns left end of doubly
 * linked allignment list.) The input DNA is all expected to be lower case
 * characters - a, c, g, t, or n. */

/* FZF */boolean ffFindEitherStrand(DNA *needle, DNA *haystack, enum ffStringency stringency,
    struct ffAli **pAli, boolean *pRcNeedle);
/* Return TRUE if find an alignment using needle, or reverse complement of 
 * needle to search haystack. DNA must be lower case. Needle and haystack
 * are zero terminated. */

/* FZF */boolean ffFindEitherStrandN(DNA *needle, int needleSize, DNA *haystack, int haySize,
    enum ffStringency stringency, struct ffAli **pAli, boolean *pRcNeedle);
/* Return TRUE if find an allignment using needle, or reverse complement of 
 * needle to search haystack. DNA must be lower case. */

/* FZF */boolean ffFindAndScore(DNA *needle, int needleSize, DNA *haystack, int haySize,
    enum ffStringency stringency, struct ffAli **pAli, boolean *pRcNeedle, int *pScore);
/* Return TRUE if find an allignment using needle, or reverse complement of 
 * needle to search haystack. DNA must be lower case. If pScore is non-NULL returns
 * score of alignment. */

/************* lib/fuzzyShow - display alignments. ****************/

/* FSH */int ffShAliPart(FILE *f, struct ffAli *aliList, 
    char *needleName, DNA *needle, int needleSize, int needleNumOffset,
    char *haystackName, DNA *haystack, int haySize, int hayNumOffset,
    int blockMaxGap, boolean rcNeedle, boolean rcHaystack,
    boolean showJumpTable, 
    boolean showNeedle, boolean showHaystack,
    boolean showSideBySide, boolean upcMatch);
/* Display parts of alignment on html page.  Returns number of blocks (after
 * merging blocks separated by blockMaxGap or less). */

/* FSH */int ffShAli(FILE *f, struct ffAli *aliList, 
    char *needleName, DNA *needle, int needleSize, int needleNumOffset,
    char *haystackName, DNA *haystack, int haySize, int hayNumOffset,
    int blockMaxGap,
    boolean rcNeedle);
/* Display alignment on html page.  Returns number of blocks (after
 * merging blocks separated by blockMaxGap or less). */

/* FSH */void ffShowAli(struct ffAli *aliList, 
    char *needleName, DNA *needle, int needleNumOffset,
    char *haystackName, DNA *haystack, int hayNumOffset,
    boolean rcNeedle);
/* Display allignment on html page to stdout. */

#endif /* FUZZYFIND_H */

