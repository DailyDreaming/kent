/* hCommon.h - routines used by many files in hgap project. */

#ifndef HCOMMON_H
#define HCOMMON_H

#ifndef HGCOLORS_H
#include "hgColors.h"
#endif

char *hgcName();
/* Relative URL to click processing program. */

char *hgTracksName();
/* Relative URL to browser. */

char *hgTrackUiName();
/* Relative URL to extended track UI. */

char *hgTextName();
/* Relative URL to old table browser. */

char *hgTablesName();
/* Relative URL to table browser. */

char *hgCustomName();
/* Relative URL to custom tracks manager. */

char *hgSessionName();
/* Relative URL to session manager. */

char *hgPalName();
/* Relative URL to click processing program. */

void fragToCloneName(char *fragName, char cloneName[128]);
/* Convert fragment name to clone name. */

void fragToCloneVerName(char *fragName, char cloneVerName[128]);
/* Convert fragment name to clone.version name. */

void recNameToFileName(char *dir, char *recName, char *fileName, char *suffix);
/* Convert UCSC style fragment name to name of file for a clone. */

void faRecNameToFaFileName(char *dir, char *recName, char *fileName);
/* Convert fa record name to file name. */

void faRecNameToQacFileName(char *dir, char *recName, char *fileName);
/* Convert fa record name to file name. */

void gsToUcsc(char *gsName, char *ucscName);
/* Convert from 
 *    AC020585.5~1.2 Fragment 2 of 29 (AC020585.5:1..1195)
 * to
 *    AC020585.5_1_2
 */

char *skipChr(char *s);
/* Skip leading 'chr' in string (to get the actual chromosome part). */

int chromToInt(char *s);
/* converts a chrom name chrXX into an integer from 1 to 54. 
 *  X = 23 Y = 24 Un = 25 M = 26 random = chr + 26;*/

boolean hIsMgcServer();
/* Is this the MGC-customized server? Change for config variable
 * mgc.server=yes */

boolean hIsGsidServer();
/* Is this the GSID-customized server? Change for config variable
 * gsid.server=yes */

boolean hIsCgbServer();
/* Is this a cancer genome browser server? Change for config variable
 * cgb.server=yes */

void hTableStart();
/* Output a table with solid borders. */

void hTableEnd();
/* Close out table started with hTableStart() */

#define hgDefaultPixWidth 800
#define DEFAULT_PIX_WIDTH "800"
/* if this default pix width is changed, also note relationship in
 * CONTROL_TABLE_WIDTH in hui.h */
#define	hgDefaultLeftLabelWidth	120
#define	hgDefaultGfxBorder	1

#endif /* HCOMMON_H */
