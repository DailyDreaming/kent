/* hCommon.h - routines used by many files in hgap project. */

#ifndef HCOMMON_H
#define HCOMMON_H

char *hgcName();
/* Relative URL to click processing program. */

char *hgTracksName();
/* Relative URL to browser. */

char *hgTrackUiName();
/* Relative URL to extended track UI. */

char *hgcFullName();
/* Absolute URL to click processing program. */

char *hgTracksFullName();
/* Absolute URL to browser. */

char *hgTextName();
/* Relative URL to text browser. */

char *hgTracksFullName();
/* Absolute URL to text browser. */

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

boolean hIsMgcServer();
/* Is this the MGC-customized server? Change for config variable
 * mgc.server=yes */

#define hgDefaultPixWidth 620

#endif /* HCOMMON_H */
