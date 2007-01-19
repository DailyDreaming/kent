/* hdb - human genome browser database. */

#ifndef HDB_H
#define HDB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef SUBTEXT_H
#include "subText.h"
#endif

#ifndef JKSQL_H
#include "jksql.h"
#endif 

#ifndef TRACKDB_H
#include "trackDb.h"
#endif

#ifndef HGRELATE_H
#include "hgRelate.h"
#endif

#ifndef BED_H
#include "bed.h"
#endif

/* At or below this number of sequences, allow split tables: */
#define HDB_MAX_SEQS_FOR_SPLIT 100

/* Statically-allocated string lengths (max supported len incl. final \0): */
#define HDB_MAX_CHROM_STRING 32
#define HDB_MAX_BAND_STRING 64
#define HDB_MAX_FIELD_STRING 32
#define HDB_MAX_TABLE_STRING 128
#define HDB_MAX_PATH_STRING 512

struct blatServerTable
/* Information about a BLAT server. */
{
    char *db;		/* Database name. */
    char *genome;	/* Genome name. */
    boolean isTrans;	/* Is tranlated to protein? */
    char *host;		/* Name of machine hosting server. */
    char *port;		/* Port that hosts server. */
    char *nibDir;	/* Directory of sequence files. */
};

struct hTableInfo
/* Some info on a track table, extracted from its field names. */
    {
    struct hTableInfo *next;	/* Next in list. */
    char *rootName;		/* Name without chrN_. */
    boolean isPos;		/* True if table is positional. */
    boolean isSplit;		/* True if table is split. */
    boolean hasBin;		/* True if table starts with field. */
    char chromField[HDB_MAX_FIELD_STRING];	/* Name of chromosome field. */
    char startField[HDB_MAX_FIELD_STRING];	/* Name of chromosome start field. */
    char endField[HDB_MAX_FIELD_STRING];		/* Name of chromosome end field. */
    char nameField[HDB_MAX_FIELD_STRING];		/* Name of item name field. */
    char scoreField[HDB_MAX_FIELD_STRING];	/* Name of score field. */
    char strandField[HDB_MAX_FIELD_STRING];	/* Name of strand field. */
    char cdsStartField[HDB_MAX_FIELD_STRING];	/* Name of cds(thick)Start field. */
    char cdsEndField[HDB_MAX_FIELD_STRING];	/* Name of cds(thick)End field. */
    char countField[HDB_MAX_FIELD_STRING];	/* Name of exon(block)Count field. */
    char startsField[HDB_MAX_FIELD_STRING];	/* Name of exon(block)Starts field. */
    char endsSizesField[HDB_MAX_FIELD_STRING];	/* Name of exon(block)Ends(Sizes) field. */
    char spanField[HDB_MAX_FIELD_STRING];		/* Name of span field. (wiggle) */
    boolean hasCDS;		/* True if it has cdsStart,cdsEnd fields. */
    boolean hasBlocks;		/* True if it has count,starts,endsSizes. */
    char *type;			/* A guess at the trackDb type for this. */
    };

void hDefaultConnect();
/* read the default settings from the config file */

char *hTrackDbName();
/* return the name of the track database from the config file. Freez when done */

struct slName *hTrackDbList();
/* return the names of the track databases from the config file. Freez when done */

void hSetTrackDbName(char *trackDbName);
/* Override the hg.conf db.trackDb setting. */

char *hPdbFromGdb(char *genomeDb);
/* return the name of the proteome database given the genome database name */

void hSetDbConnect(char* host, char *db, char *user, char *password);
/* set the connection information for the database */

void hSetDbConnect2(char* host, char *db, char *user, char *password);
/* set the connection information for the database */

boolean hDbExists(char *database);
/* Function to check if this is a valid db name */

boolean hDbIsActive(char *database);
/* Function to check if this is a valid and active db name */

void hSetDb(char *dbName);
/* Set the database name. */

void hSetDb2(char *dbName);
/* Set the database name. */

char *hDefaultDb();
/* Return the default db if all else fails */

char *hDbForTaxon(struct sqlConnection *conn, int taxon);
/* Get database associated with NCBI taxon number if any. */

char *hDefaultChrom();
/* Return some sequence named in chromInfo from the current db, or NULL if db 
 * has no chromInfo. */

char *hDefaultChromDb(char *db);
/* Return some sequence named in chromInfo from the given db, or NULL if db 
 * has no chromInfo. */

int hChromCount();
/* Return the number of chromosomes (scaffolds etc.) in the current db. */

int hChromCountDb(char *db);
/* Return the number of chromosomes (scaffolds etc.) in the given db. */

char *hGetDb();
/* Return the current database name, setting to default if not defined. */

char *hGetDbUsual(char *usual);
/* Return the current database name, setting to usual if not defined. */

char *hGetDb2();
/* Return the secondary database name, setting to default if not defined. */

char *hGetDb2Usual(char *usual);
/* Return the secondary database name, setting to usual if not defined. */

char *hGetDbHost();
/* Return the current database host. */

char *hGetDbName();
/* Return the current database name. */

char *hGetDbUser();
/* Return the current database user. */

char *hGetDbPassword();
/* Return the current database password. */

struct sqlConnection *hAllocConn();
/* Get free connection if possible. If not allocate a new one. */

struct sqlConnection *hAllocConn2();
/* Get free connection if possible. If not allocate a new one. */

struct sqlConnection *hAllocConnDb(char *db);
/* Get free cached connection for a database if possible. If not allocate 
 * a new one. */

struct sqlConnection *hAllocOrConnect(char *db);
/* Get available cached connection if possible. If not, just connect. */

void hFreeConn(struct sqlConnection **pConn);
/* Put back connection for reuse. */

void hFreeConn2(struct sqlConnection **pConn);
/* Put back secondary db connection for reuse. */

void hFreeOrDisconnect(struct sqlConnection **pConn);
/* Free cached or non-cached connection. */

struct sqlConnection *hConnectCentral();
/* Connect to central database where user info and other info
 * not specific to a particular genome lives.  Free this up
 * with hDisconnectCentral(). */

void hDisconnectCentral(struct sqlConnection **pConn);
/* Put back connection for reuse. */

struct sqlConnection *hConnectCart();
/* Connect to cart database.  Defaults to the central connection
 * unless cart.db or cart.host are configured. Free this
 * up with hDisconnectCart(). */

void hDisconnectCart(struct sqlConnection **pConn);
/* Put back connection for reuse. */

char *hgOfficialChromName(char *name);
/* Returns "cannonical" name of chromosome or NULL
 * if not a chromosome. */

boolean hgIsOfficialChromName(char *name);
/* Determine if name is exact (case-sensitive) match with
 * a chromosome in the current assembly */

boolean hgNearOk(char *database);
/* Return TRUE if ok to put up familyBrowser (hgNear) 
 * on this database. */

boolean hgPbOk(char *database);
/* Return TRUE if ok to put up Proteome Browser (pbTracks)
 * on this database. */

boolean hgPcrOk(char *database);
/* Return TRUE if ok to put up hgPcr on this database. */

boolean hTableExists(char *table);
/* Return TRUE if a table exists in database. */

boolean hTableExists2(char *table);
/* Return TRUE if a table exists in secondary database. */

boolean hTableExistsDb(char *db, char *table);
/* Return TRUE if a table exists in db. */

boolean hTableOrSplitExists(char *table);
/* Return TRUE if table (or a chrN_table) exists in database. */

char *hTableForTrack(char *db, char *trackName);
/* Return a table for a track in db. Returns one of the split
 * tables, or main table if not split */

boolean hColExists(char *table, char *column);
/* Return TRUE if a column exists in table. */

boolean hColExistsDb(char *db, char *table, char *column);
/* Return TRUE if a column exists in table in db. */


void hParseTableName(char *table, char trackName[HDB_MAX_TABLE_STRING],
		     char chrom[HDB_MAX_CHROM_STRING]);
/* Parse an actual table name like "chr17_random_blastzWhatever" into 
 * the track name (blastzWhatever) and chrom (chr17_random). */

int hdbChromSize(char *db, char *chromName);
/* Get chromosome size from given database . */

int hChromSize(char *chromName);
/* Return size of chromosome. */

int hChromSize2(char *chromName);
/* Return size of chromosome from secondary database. */

struct dnaSeq *hFetchSeq(char *fileName, char *seqName, int start, int end);
/* Fetch sequence from file.  If it is a .2bit file then fetch the named sequence.
If it is .nib then just ignore seqName. */

struct dnaSeq *hFetchSeqMixed(char *fileName, char *seqName, int start, int end);
/* Fetch mixed case sequence. */

struct dnaSeq *hChromSeq(char *chrom, int start, int end);
/* Return lower case DNA from chromosome. */

struct dnaSeq *hChromSeq2(char *chrom, int start, int end);
/* Return lower case DNA from chromosome in db2.*/

struct dnaSeq *hChromSeqMixed(char *chrom, int start, int end);
/* Return mixed case (repeats in lower case) DNA from chromosome. */

struct dnaSeq *hChromSeqMixed2(char *chrom, int start, int end);
/* Return mixed case (repeats in lower case) DNA from chromosome. */

struct dnaSeq *hDnaFromSeq2(char *seqName, int start, int end, enum dnaCase dnaCase);
/* Fetch DNA */

struct dnaSeq *hChromSeqNib(char *chrom, int start, int end, char *nibDir);
/* Return lower case DNA from chromosome and nib directory. */

struct dnaSeq *hSeqForBed(struct bed *bed);
/* Get the sequence associated with a particular bed concatenated together. */

boolean hChromBand(char *chrom, int pos, char retBand[HDB_MAX_BAND_STRING]);
/* Fill in text string that says what band pos is on. 
 * Return FALSE if not on any band, or table missing. */

boolean hChromBandConn(struct sqlConnection *conn, 
	char *chrom, int pos, char retBand[HDB_MAX_BAND_STRING]);
/* Fill in text string that says what band pos is on. 
 * Return FALSE if not on any band, or table missing. */

boolean hChromBand(char *chrom, int pos, char retBand[HDB_MAX_BAND_STRING]);
/* Fill in text string that says what band pos is on. 
 * Return FALSE if not on any band, or table missing. */

boolean hScaffoldPos(char *chrom, int start, int end,
                        char **retScaffold, int *retStart, int *retEnd);
/* Return the scaffold, and start end coordinates on a scaffold, for
 * a chromosome range.  If the range extends past end of a scaffold,
 * it is truncated to the scaffold end.
 * Return FALSE if unable to convert */

struct dnaSeq *hDnaFromSeq(char *seqName, 
	int start, int end, enum dnaCase dnaCase);
/* Fetch DNA in a variety of cases.  */

struct dnaSeq *hLoadChrom(char *chromName);
/* Fetch entire chromosome into memory. */

void hNibForChrom(char *chromName, char retNibName[HDB_MAX_PATH_STRING]);
/* Get .nib file associated with chromosome. */

void hNibForChrom2(char *chromName, char retNibName[HDB_MAX_PATH_STRING]);
/* Get .nib file associated with chromosome. */

struct slName *hAllChromNames();
/* Get list of all chromosomes. */

struct slName *hAllChromNamesDb(char *db);
/* Get list of all chromosomes in database. */

char *hExtFileName(char *extFileTable, unsigned extFileId);
/* Get external file name from table and ID.  Typically
 * extFile table will be 'extFile' or 'gbExtFile'
 * Abort if the id is not in the table or if the file
 * fails size check.  Please freeMem the result when you 
 * are done with it. */

int hRnaSeqAndIdx(char *acc, struct dnaSeq **retSeq, HGID *retId, char *gbdate, struct sqlConnection *conn);
/* Return sequence for RNA, it's database ID, and optionally genbank 
 * modification date. Return -1 if not found. */

char* hGetSeqAndId(struct sqlConnection *conn, char *acc, HGID *retId);
/* Return sequence as a fasta record in a string and it's database ID, or 
 * NULL if not found. */

struct dnaSeq *hExtSeq(char *acc);
/* Return sequence for external seq. */

struct dnaSeq *hExtSeqPart(char *acc, int start, int end);
/* Return part of external sequence. */

struct dnaSeq *hRnaSeq(char *acc);
/* Return sequence for RNA. */

aaSeq *hPepSeq(char *acc);
/* Return sequence for a peptide. */

boolean hGenBankHaveSeq(char *acc, char *compatTable);
/* Get a GenBank or RefSeq mRNA or EST sequence or NULL if it doesn't exist.
 * This handles compatibility between pre-incremental genbank databases where
 * refSeq sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */

struct dnaSeq *hGenBankGetMrna(char *acc, char *compatTable);
/* Get a GenBank or RefSeq mRNA or EST sequence or NULL if it doesn't exist.
 * This handles compatibility between pre-incremental genbank databases where
 * refSeq sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */

struct dnaSeq *hGenBankGetMrnaC(struct sqlConnection *conn, char *acc, char *compatTable);
/* Same as above, but can pass in connection to any db */

aaSeq *hGenBankGetPep(char *acc, char *compatTable);
/* Get a RefSeq peptide sequence or NULL if it doesn't exist.  This handles
 * compatibility between pre-incremental genbank databases where refSeq
 * sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */

aaSeq *hGenBankGetPepC(struct sqlConnection *conn, char *acc, char *compatTable);
/* Same as above, but can pass in connection to any db */

char *hGenBankGetDesc(char *acc, boolean native);
/* Get a description for a genbank or refseq mRNA. If native is TRUE, an
 * attempt is made to get a more compact description that doesn't include
 * species name. Acc may optionally include the version.  NULL is returned if
 * a description isn't available.  Free string when done. */

struct bed *hGetBedRange(char *table, char *chrom, int chromStart,
			 int chromEnd, char *sqlConstraints);
/* Return a bed list of all items (that match sqlConstraints, if nonNULL) 
 * in the given range in table.  If chromEnd is 0, omit the range (whole chrom).
 * WARNING: this does not use the bin column and maybe slower than you would like.
 */

struct bed *hGetBedRangeDb(char *db, char *table, char *chrom, int chromStart,
			   int chromEnd, char *sqlConstraints);
/* Return a bed list of all items (that match sqlConstraints, if nonNULL) 
 * in the given range in table.  If chromEnd is 0, omit the range (whole chrom).
 * WARNING: this does not use the bin column and maybe slower than you would like.*/

struct hash *hCtgPosHash();
/* Return hash of ctgPos from current database keyed by contig name. */

char *hFreezeFromDb(char *database);
/* return the freeze for the database version. 
   For example: "hg6" returns "Dec 12, 2000". If database
   not recognized returns NULL */

char *hDbFromFreeze(char *freeze);
/* Return database version from freeze name. */

struct slName *hDbList();
/* List of all database versions that are online (database
 * names only).  See also hDbDbList. */

struct dbDb *hDbDb(char *database);
/* Return dbDb entry for a database */

struct dbDb *hDbDbList();
/* Return list of databases that are actually online. 
 * The list includes the name, description, and where to
 * find the nib-formatted DNA files. Free this with dbDbFree. */

struct dbDb *hArchiveDbDbList();
/* Return list of databases in archive central dbDb.
 * Free this with dbDbFree. */

struct sqlConnection *hMaybeConnectArchiveCentral();
/* Connect to central database for archives.
 * Free this up with hDisconnectCentralArchive(). */

boolean hIsPrivateHost();
/* Return TRUE if this is running on private web-server. */

boolean hIsMgscHost();
/* Return TRUE if this is running on web server only
 * accessible to Mouse Genome Sequencing Consortium. */

boolean hTrackOnChrom(struct trackDb *tdb, char *chrom);
/* Return TRUE if track exists on this chromosome. */

struct trackDb *hTrackDb(char *chrom);
/* Load tracks associated with current chromosome (which may be NULL for
 * all). If trackDbLocal exists, then it's row either override or are added to
 * the standard trackDb. */

struct trackDb *hTrackDbForTrack(char *track);
/* Load trackDb object for a track. If trackDbLocal exists, then its row is
 * used if it exists.  If track is composite, its subtracks will also be 
 * loaded and inheritance will be handled; if track is a subtrack then 
 * inheritance will be handled.  (Unless a subtrack has "noInherit on"...)
 * This will die if the current database does not have a trackDb, but will 
 * return NULL if track is not found. */

struct trackDb *hCompositeTrackDbForSubtrack(struct trackDb *sTdb);
/* Given a trackDb that may be for a subtrack of a composite track, 
 * return the trackDb for the composite track if we can find it, else NULL.
 * Note: if the composite trackDb is found and returned, then its subtracks 
 * member will contain a newly allocated tdb like sTdb (but not ==). */

struct hTableInfo *hFindTableInfo(char *chrom, char *rootName);
/* Find table information.  Return NULL if no table. */

struct hTableInfo *hFindTableInfoDb(char *db, char *chrom, char *rootName);
/* Find table information in specified db.  Return NULL if no table. */

int hTableInfoBedFieldCount(struct hTableInfo *hti);
/* Return number of BED fields needed to save hti. */

boolean hFindChromStartEndFields(char *table, 
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING],
	char retEnd[HDB_MAX_FIELD_STRING]);
/* Given a table return the fields for selecting chromosome, start, and end. */

boolean hFindChromStartEndFieldsDb(char *db, char *table, 
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING],
	char retEnd[HDB_MAX_FIELD_STRING]);
/* Given a table return the fields for selecting chromosome, start, and end. */

boolean hFindBed12Fields(char *table, 
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING],
	char retEnd[HDB_MAX_FIELD_STRING], char retName[HDB_MAX_FIELD_STRING],
	char retScore[HDB_MAX_FIELD_STRING],
	char retStrand[HDB_MAX_FIELD_STRING],
        char retCdsStart[HDB_MAX_FIELD_STRING],
	char retCdsEnd[HDB_MAX_FIELD_STRING],
	char retCount[HDB_MAX_FIELD_STRING],
	char retStarts[HDB_MAX_FIELD_STRING],
	char retEndsSizes[HDB_MAX_FIELD_STRING]);
/* Given a table return the fields corresponding to all the bed 12 
 * fields, if they exist.  Fields that don't exist in the given table 
 * will be set to "". */

boolean hIsBinned(char *table);
/* Return TRUE if a table is binned. */

int hFieldIndex(char *table, char *field)
/* Return index of field in table or -1 if it doesn't exist. */;

boolean hHasField(char *table, char *field);
/* Return TRUE if table has field */

boolean hFieldHasIndex(char *table, char *field);
/* Return TRUE if a SQL index exists for table.field. */

boolean hFindFieldsAndBin(char *table, 
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING],
	char retEnd[HDB_MAX_FIELD_STRING], boolean *retBinned);
/* Given a table return the fields for selecting chromosome, start, end,
 * and whether it's binned . */

boolean hFindSplitTable(char *chrom, char *rootName, 
	char retTableBuf[HDB_MAX_TABLE_STRING], boolean *hasBin);
/* Find name of table that may or may not be split across chromosomes. 
 * Return FALSE if table doesn't exist.  */

boolean hFindSplitTableDb(char *db, char *chrom, char *rootName, 
	char retTableBuf[HDB_MAX_TABLE_STRING], boolean *hasBin);
/* Find name of table in a given database that may or may not 
 * be split across chromosomes. Return FALSE if table doesn't exist.  */

struct slName *hSplitTableNames(char *rootName);
/* Return a list of all split tables for rootName, or of just rootName if not 
 * split, or NULL if no such tables exist. */

int hBinLevels();
/* Return number of levels to bins. */

int hBinFirstShift();
/* Return amount to shift a number to get to finest bin. */

int hBinNextShift();
/* Return amount to shift a numbe to get to next coarser bin. */

int hFindBin(int start, int end);
/* Given start,end in chromosome coordinates assign it
 * a bin.   There's a bin for each 128k segment, for each
 * 1M segment, for each 8M segment, for each 64M segment,
 * and for each chromosome (which is assumed to be less than
 * 512M.)  A range goes into the smallest bin it will fit in. */

void hAddBinToQueryGeneral(char *binField, int start, int end, struct dyString *query);
/* Add clause that will restrict to relevant bins to query. allow bin field name to be specified */

void hAddBinToQuery(int start, int end, struct dyString *query);
/* Add clause that will restrict to relevant bins to query. */

struct sqlResult *hRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset);
/* Construct and make a query to tables that may be split and/or
 * binned. */

struct sqlResult *hOrderedRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset);
/* Construct and make a query to tables that may be split and/or
 * binned. Forces return values to be sorted by chromosome start. */

struct sqlResult *hExtendedRangeQuery(
	struct sqlConnection *conn,  /* Open SQL connection. */
	char *rootTable, 	     /* Table (not including any chrN_) */
	char *chrom, int start, int end,  /* Range. */
	char *extraWhere,            /* Extra things to add to where clause. */
	boolean order, 	   /* If true order by start position (can be slow). */
	char *fields,      /* If non-NULL comma separated field list. */
	int *retRowOffset); /* Returns offset past bin field. */
/* Range query with lots of options. */

struct sqlResult *hChromQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	char *extraWhere, int *retRowOffset);
/* Construct and make a query across whole chromosome to tables 
 * that may be split and/or
 * binned. */

struct sqlResult *hExtendedChromQuery(
	struct sqlConnection *conn,  /* Open SQL connection. */
	char *rootTable, 	     /* Table (not including any chrN_) */
	char *chrom,  		     /* Chromosome. */
	char *extraWhere,            /* Extra things to add to where clause. */
	boolean order, 	   /* If true order by start position (can be slow). */
	char *fields,      /* If non-NULL comma separated field list. */
	int *retRowOffset); /* Returns offset past bin field. */
/* Chromosome query fields for tables that may be split and/or binned, 
 * with lots of options. */

int hOffsetPastBin(char *chrom, char *table);
/* Return offset into a row of table that skips past bin
 * field if any. */

boolean hgParseChromRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd);
/* Parse something of form chrom:start-end into pieces. 
 * assumes a current database */

boolean hgParseChromRangeDb(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, boolean haveDb);
/* Parse something of form chrom:start-end into pieces. 
 * if haveDb then check with chromInfo for names */

boolean hgIsChromRange(char *spec);
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */

struct trackDb *hMaybeTrackInfo(struct sqlConnection *conn, char *trackName);
/* Load trackDb object for a track. If trackDbLocal exists, then its row is
 * used if it exists.  If track is composite, its subtracks will also be 
 * loaded and inheritance will be handled; if track is a subtrack then 
 * inheritance will be handled.  (Unless a subtrack has "noInherit on"...)
 * Don't die if conn has no trackDb table.  Return NULL if trackName is not 
 * found. */

struct trackDb *hTrackInfo(struct sqlConnection *conn, char *trackName);
/* Look up track in database, errAbort if it's not there. */

boolean hTrackCanPack(char *trackName);
/* Return TRUE if this track can be packed. */

char *hTrackOpenVis(char *trackName);
/* Return "pack" if track is packable, otherwise "full". */

struct dbDb *hGetIndexedDatabases();
/* Get list of all active databases. 
 * Dispose of this with dbDbFreeList. */

struct dbDb *hGetIndexedDatabasesForClade(char *db);
/* Get list of active databases in db's clade.
 * Dispose of this with dbDbFreeList. */

struct slName *hLiftOverFromDbs();
/* Return a list of names of the DBs in the 
 * fromDb column of the liftOverChain.*/

struct slName *hLiftOverToDbs(char *fromDb);
/* Return a list of names of the DBs in the 
 * toDb column of the liftOverChain.
 * If fromDb!=NULL, return only those with that
 * fromDb. */

struct slName *hLiftOverFromOrgs();
/* Return a list of names of organisms that 
 * have databases in the fromDb column of
 * liftOverChain.*/

struct slName *hLiftOverToOrgs(char *fromDb);
/* Return a list of names of the organisms with
 * databases in the toDb column of the liftOverChain.
 * If fromDb!=NULL, return only those with that
 * fromDb. */

struct hash *hGetDatabaseRank();
/* Get list of databases and make a hash of order rank
 * Dispose of this with hashFree. */ 

struct dbDb *hGetLiftOverFromDatabases();
/* Get list of databases for which there is at least one liftOver chain file
 * Dispose of this with dbDbFreeList. */

struct dbDb *hGetLiftOverToDatabases(char *fromDb);
/* Get list of databases for which there are liftOver chain files 
 * to convert from the fromDb assembly.
 * Dispose of this with dbDbFreeList. */

struct dbDb *hGetAxtInfoDbs();
/* Get list of db's where we have axt files listed in axtInfo . 
 * The db's with the same organism as organism go last.
 * Dispose of this with dbDbFreeList. */

struct axtInfo *hGetAxtAlignments(char *db);
/* Get list of alignments where we have axt files listed in axtInfo . 
 * Dispose of this with axtInfoFreeList. */

struct axtInfo *hGetAxtAlignmentsChrom(char *otherDb, char *chrom);
/* Get list of alignments where we have axt files listed in axtInfo for a specified chromosome . 
 * Dispose of this with axtInfoFreeList. */

struct dbDb *hGetBlatIndexedDatabases();
/* Get list of databases for which there is a BLAT index. 
 * Dispose of this with dbDbFreeList. */

boolean hIsBlatIndexedDatabase(char *db);
/* Return TRUE if have a BLAT server on sequence corresponding 
 * to give database. */

struct blatServerTable *hFindBlatServer(char *db, boolean isTrans);
/* return the blat server information corresponding to the database */


char *hDefaultPos(char *database);
/* param database - The database within which to look up this position.
   return - default chromosome position associated with database.
    Use freeMem on return value when done.
 */

char *hOrganism(char *database);
/* Return organism associated with database.   Use freeMem on
 * return value when done. */

char *hArchiveOrganism(char *database);
/* Return organism associated with database.   Use freeMem on
 * return value when done. This one checks the normal central
 * DB first, then the archive dbDb. */

int hOrganismID(char *database);
/* Get organism ID from relational organism table */
/* Return -1 if not found */

char *hScientificName(char *database);
/* Return scientific name for organism represented by this database */
/* Return NULL if unknown database */
/* NOTE: must free returned string after use */

char *hHtmlPath(char *database);
/* Return /gbdb path name to html description for this database */
/* Return NULL if unknown database */
/* NOTE: must free returned string after use */

char *hFreezeDate(char *database);
/* Return freeze date of database. Use freeMem when done. */

char *hGenomeOrArchive(char *database);
/* Return genome name associated from the regular or the archive database. */

char *hGenome(char *database);
/* Return genome associated with database.   Use freeMem on
 * return value when done. */

char *hPreviousAssembly(char *database);
/* Return previous assembly for the genome associated with database. */

boolean hGotClade();
/* Return TRUE if central db contains clade info tables. */

char *hClade(char *genome);
/* If central database has clade tables, return the clade for the 
 * given genome; otherwise return NULL. */

void hAddDbSubVars(char *prefix, char *database, struct subText **pList);
/* Add substitution variables associated with database to list. */

void hLookupStringsInTdb(struct trackDb *tdb, char *database);
/* Lookup strings in track database. */

char *hDefaultDbForGenome(char *genome);
/*
Purpose: Return the default database matching the organism.

param organism - The organism for which we are trying to get the 
    default database.
return - The default database name for this organism
 */

char *hDefaultGenomeForClade(char *clade);
/* Return highest relative priority genome for clade. */

char *sqlGetField(struct sqlConnection *connIn, 
   	          char *dbName, char *tblName, char *fldName, 
  	          char *condition);
/* Return a single field from the database, given database name, 
   table name, field name, and a condition string */

struct hash *hChromSizeHash(char *db);
/* Get hash of chromosome sizes for database.  Just hashFree it when done. */

struct mafAli *mafLoadInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end);
/* Return list of alignments in region. */

struct mafAli *axtLoadAsMafInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end, 
	char *tPrefix, char *qPrefix, int tSize,  struct hash *qSizeHash);
/* Return list of alignments in region from axt external file as a maf. */

char *hgDirForOrg(char *org);
/* Make directory name from organism name - getting
 * rid of dots and spaces. */

struct hash *hgReadRa(char *genome, char *database, char *rootDir, 
	char *rootName, struct hash **retHashOfHash);
/* Read in ra in root, root/org, and root/org/database. 
 * Returns a list of hashes, one for each ra record.  Optionally
 * if retHashOfHash is non-null it returns there a
 * a hash of hashes keyed by the name field in each
 * ra sub-hash. */

char *addCommasToPos(char *position);
/* add commas to the numbers in a position 
 * returns pointer to static */

struct grp* hLoadGrps();
/* load the grp and optional grpLocal tables from the databases.  If grpLocal
 * exists, then entries in this table will override or supplement the grp
 * table.  The names of these tables can be configured in the hg.conf file
 * with db.grp and db.grpLocal variables.  List will be returned sorted by
 * priority. */

int hGetMinIndexLength();
/* get the minimum index size for the current database that won't smoosh together chromNames
 * such that any group of smooshed entries has a cumulative size greater than the
 * the largest chromosome.  Allow one exception cuz we're nice
 */

int chrStrippedCmp(char *chrA, char *chrB);
/*	compare chrom names after stripping chr, Scaffold_ or ps_ prefix */

int chrNameCmp(char *str1, char *str2);
/* Compare chromosome names by number, then suffix.  str1 and str2 must 
 * match the regex "chr([0-9]+|[A-Za-z0-9]+)(_[A-Za-z0-9_]+)?". */

int chrSlNameCmp(const void *el1, const void *el2);
/* Compare chromosome names by number, then suffix.  el1 and el2 must be 
 * slName **s (as passed in by slSort) whose names match the regex 
 * "chr([0-9]+|[A-Za-z0-9]+)(_[A-Za-z0-9_]+)?". */

int compareDbs(char *dbA, char *dbB);
/* Compare two org# e.g. mm6 vs. mm16 or mm6 vs. hg17
 * Return > 0 if dbA > dbB, < 0 if less than, and 0 if equal */

int getTableSize(char *table);
/* Get count of rows in a table in the primary database */

boolean isNewChimp(char *database) ;
/* database is panTro2 or later */

#endif /* HDB_H */
