/* hgTables - shared data between hgTables modules. */

#ifndef HGTABLES_H
#define HGTABLES_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef GRP_H
#include "grp.h"
#endif

struct region
/* A part (or all) of a chromosome. */
    {
    struct region *next;
    char *chrom;		/* Chromosome. */
    int start;			/* Zero-based. */
    int end;			/* Non-inclusive.  If zero means full chromosome. */
    };

/* Global variables - generally set during initialization and then read-only. */
extern struct cart *cart;	/* This holds cgi and other variables between clicks. */
extern struct hash *oldVars;	/* The cart before new cgi stuff added. */
extern char *genome;		/* Name of genome - mouse, human, etc. */
extern char *database;		/* Name of genome database - hg15, mm3, or the like. */
extern struct trackDb *fullTrackList;	/* List of all tracks in database. */
extern struct trackDb *curTrack;	/* Currently selected track. */

/* --------------- HTML Helpers ----------------- */

void hPrintf(char *format, ...);
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */

void hPrintSpaces(int count);
/* Print a number of non-breaking spaces. */

void htmlOpen(char *format, ...);
/* Start up a page that will be in html format. */

void htmlClose();
/* Close down html format page. */

void textOpen();
/* Start up page in text format. (No need to close this). */

void hTableStart();
/* For some reason BORDER=1 does not work in our web.c nested table scheme.
 * So use web.c's trick of using an enclosing table to provide a border.   */

void hTableEnd();
/* Close out table started with hTableStart() */

/* --------- Utility functions --------------------- */

struct region *getRegions(struct sqlConnection *conn);
/* Consult cart to get list of regions to work on. */

struct trackDb *findTrack(char *name, struct trackDb *trackList);
/* Find track, or return NULL if can't find it. */

struct trackDb *findTrackInGroup(char *name, struct trackDb *trackList,
    struct grp *group);
/* Find named track that is in group (NULL for any group).
 * Return NULL if can't find it. */

struct trackDb *findSelectedTrack(struct trackDb *trackList, 
    struct grp *group);
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */

struct asObject *asForTable(struct sqlConnection *conn, char *table);
/* Get autoSQL description if any associated with table. */

struct asColumn *asColumnFind(struct asObject *asObj, char *name);
/* Return named column. */

char *connectingTableForTrack(struct trackDb *track);
/* Return table name to use with all.joiner for track. 
 * You can freeMem this when done. */

char *chromTable(struct sqlConnection *conn, char *table);
/* Get chr1_table if it exists, otherwise table. 
 * You can freeMem this when done. */

char *chrnTable(struct sqlConnection *conn, char *table);
/* Return chrN_table if table is split, otherwise table. 
 * You can freeMem this when done. */

void doTabOutTable(char *table, struct sqlConnection *conn, char *fields);
/* Do tab-separated output on table. */

/* --------- CGI/Cart Variables --------------------- */

/* Command type variables - control which page is up.  Get stripped from
 * cart. */
#define hgtaDo "hgta_do" /* All commands start with this and are removed
	 		  * from cart. */
#define hgtaDoIntersect "hgta_doIntersect"
#define hgtaDoMainPage "hgta_doMainPage"
#define hgtaDoTopSubmit "hgta_doTopSubmit"
#define hgtaDoPasteIdentifiers "hgta_doPasteIdentifiers"
#define hgtaDoPastedIdentifiers "hgta_doPastedIdentiers"
#define hgtaDoUploadIdentifiers "hgta_doUploadIdentifiers"
#define hgtaDoClearIdentifiers "hgta_doClearIdentifiers"
#define hgtaDoFilterPage "hgta_doFilterPage"
#define hgtaDoTest "hgta_doTest"
#define hgtaDoSchema "hgta_doSchema"
#define hgtaDoSchemaDb "hgta_doSchemaDb"
#define hgtaDoValueHistogram "hgta_doValueHistogram"
#define hgtaDoValueRange "hgta_doValueRange"
#define hgtaDoSelectedFields "hgta_doSelectedFields"

/* Other CGI variables. */
#define hgtaGroup "hgta_group"
#define hgtaTrack "hgta_track"
#define hgtaRegionType "hgta_regionType"
#define hgtaRange "hgta_range"
#define hgtaOffsetStart "hgta_offsetStart"
#define hgtaOffsetEnd "hgta_offsetEnd"
#define hgtaOffsetRelativeTo "hgta_offsetRelativeTo"
#define hgtaOutputType "hgta_outputType"
#define hgtaDatabase "hgta_database"  /* In most cases use "db" */
#define hgtaTable "hgta_table"
#define hgtaPastedIdentifiers "hgta_pastedIdentifiers"
#define hgtaIdentifierFile "hgta_identifierFile"

/* Prefix for variables managed by field selector. */
#define hgtaFieldSelectPrefix "hgta_fs_"


/* Output types. */
#define outPrimaryTable "primaryTable"
#define outSequence "sequence"
#define outSelectedFields "selectedFields"
#define outSchema "schema"
#define outStats "stats"
#define outBed "bed"
#define outGtf "gtf"
#define outCustomTrack "customTrack"

/* Identifier list handling stuff. */

char *identifierFileName();
/* File name identifiers are in, or NULL if no such file. */

struct hash *identifierHash();
/* Return hash full of identifiers. */


/* Stuff from other modules used in hgTables.h. */

void doMainPage(struct sqlConnection *conn);
/* Put up the first page user sees. */

void mainPageAfterOpen(struct sqlConnection *conn);
/* Put up main page assuming htmlOpen()/htmlClose()
 * will happen in calling routine. */

void doTableSchema(char *db, char *table, struct sqlConnection *conn);
/* Show schema around table. */

void doTrackSchema(struct trackDb *track, struct sqlConnection *conn);
/* Show schema around track. */

void doValueHistogram(char *field);
/* Put up value histogram. */

void doValueRange(char *field);
/* Put up value histogram. */

void doPasteIdentifiers(struct sqlConnection *conn);
/* Respond to paste identifiers button. */

void doPastedIdentifiers(struct sqlConnection *conn);
/* Process submit in past identifiers page. */

void doUploadIdentifiers(struct sqlConnection *conn);
/* Respond to upload identifiers button. */

void doClearIdentifiers(struct sqlConnection *conn);
/* Respond to clear identifiers button. */

void doSelectedFields();
/* Actually produce selected field output as text stream. */

void doOutSelectedFields(struct trackDb *track, struct sqlConnection *conn);
/* Put up select fields (for tab-separated output) page. */


void printMainHelp();
/* Put up main page help info. */
#endif /* HGTABLES_H */


