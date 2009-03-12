/* chainCart.h - cart settings for chain color options */

#ifndef CHAINCART_H
#define CHAINCART_H

/*	Option strings for TrackUi settings	*/
#define MAX_OPT_STRLEN	128

#define OPT_CHROM_COLORS "chainColor"
/*	TrackUi menu items, completing the sentence:
 *	Color chains by:
 */
#define CHROM_COLORS "Chromosome"
#define SCORE_COLORS "Normalized Score"
#define NO_COLORS "Black"

extern enum chainColorEnum chainFetchColorOption(struct cart *cart,
    struct trackDb *tdb, char **optString);
/******	ColorOption - Chrom colors by default **************************/

#endif /* CHAINCART_H */
