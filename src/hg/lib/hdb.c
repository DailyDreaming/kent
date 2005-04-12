/* hdb - human genome browser database. */
#include "common.h"
#include "obscure.h"
#include "hash.h"
#include "portable.h"
#include "linefile.h"
#include "binRange.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "hdb.h"
#include "hgRelate.h"
#include "fa.h"
#include "hgConfig.h"
#include "ctgPos.h"
#include "trackDb.h"
#include "hCommon.h"
#include "hgFind.h"
#include "dbDb.h"
#include "axtInfo.h"
#include "subText.h"
#include "blatServers.h"
#include "bed.h"
#include "defaultDb.h"
#include "scoredRef.h"
#include "maf.h"
#include "ra.h"
#include "liftOver.h"
#include "liftOverChain.h"
#include "grp.h"
#include "twoBit.h"
#include "genbank.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: hdb.c,v 1.245 2005/04/12 22:28:18 angie Exp $";


#define DEFAULT_PROTEINS "proteins"
#define DEFAULT_GENOME "Human"


static struct sqlConnCache *hdbCc = NULL;  /* cache for primary database connection */
static struct sqlConnCache *hdbCc2 = NULL;  /* cache for second database connection (ortholog) */
static struct sqlConnCache *centralCc = NULL;
static struct sqlConnCache *centralArchiveCc = NULL;
static struct sqlConnCache *cartCc = NULL;  /* cache for cart; normally same as centralCc */
                                               

static char *hdbHost = NULL;
static char *hdbName = NULL;
static char *hdbName2 = NULL;
static char *hdbUser = NULL;
static char *hdbPassword = NULL;
static char *hdbTrackDb = NULL;
static char *hdbTrackDbLocal = NULL;

static char* getCfgValue(char* envName, char* cfgName)
/* get a configuration value, from either the environment or the cfg file,
 * with the env take precedence.
 */
{
char *val = getenv(envName);
if (val == NULL)
    val = cfgOption(cfgName);
return val;
}


static struct chromInfo *lookupChromInfo(char *db, char *chrom)
/* Query db.chromInfo for the first entry matching chrom. */
{
struct chromInfo *ci = NULL;
struct sqlConnection *conn = hAllocOrConnect(db);
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];
safef(query, sizeof(query), "select * from chromInfo where chrom like '%s'",
      chrom);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    ci = chromInfoLoad(row);
    }
sqlFreeResult(&sr);
hFreeOrDisconnect(&conn);
return ci;
}

static struct chromInfo *getChromInfo(char *db, char *chrom)
/* Get chromInfo for named chromosome (case-insens.) from db.  
 * Return NULL if no such chrom. */
/* Cache results, but build up the hash incrementally instead of in one slurp 
 * from chromInfo because that takes a *long* time for scaffold-based dbs and 
 * is usually not necessary. */
{
static struct hash *dbToInfo = NULL;
struct hash *infoHash = NULL;
struct hashEl *dHel = NULL;
struct chromInfo *ci = NULL;
char upcName[HDB_MAX_CHROM_STRING];
safef(upcName, sizeof(upcName), chrom);
touppers(upcName);

if (dbToInfo == NULL)
    dbToInfo = hashNew(0);
dHel = hashLookup(dbToInfo, db);
if (dHel == NULL)
    {
    infoHash = hashNew(0);
    hashAdd(dbToInfo, db, infoHash);
    }
else
    infoHash = (struct hash *)(dHel->val);
ci = (struct chromInfo *)hashFindVal(infoHash, upcName);
if (ci == NULL)
    {
    ci = lookupChromInfo(db, chrom);
    hashAdd(infoHash, upcName, ci);
    }
return ci;
}

static struct chromInfo *mustGetChromInfo(char *db, char *chrom)
/* Get chromInfo for named chrom from primary database or
 * die trying. */
{
struct chromInfo *ci = getChromInfo(db, chrom);
if (ci == NULL)
    errAbort("Couldn't find chromosome/scaffold %s in database", chrom);
return ci;
}

char *hgOfficialChromName(char *name)
/* Returns "canonical" name of chromosome or NULL
 * if not a chromosome. (Case-insensitive search w/sameWord()) */
{
struct chromInfo *ci = getChromInfo(hGetDb(), name);
if (ci != NULL)
    return cloneString(ci->chrom);
else
    return NULL;
}

int hGetMinIndexLength()
/* get the minimum index size for the current database that won't smoosh 
 * together chromNames. */
{
static boolean minLen = 0;
if (minLen <= 0)
    {
    struct slName *nameList = hAllChromNames();
    struct slName *name, *last;
    int len = 4;
    slSort(&nameList, slNameCmp);
    last = nameList;
    if (last != NULL)
        {
	for (name = nameList->next; name != NULL; name = name->next)
	    {
	    while (strncmp(name->name, last->name, len) == 0)
	        ++len;
	    last = name;
	    }
	}
    slFreeList(&nameList);
    minLen = len;
    }
return minLen;
}

void hDefaultConnect()
/* read in the connection options from config file */
{
hdbHost 	= getCfgValue("HGDB_HOST", "db.host");
hdbUser 	= getCfgValue("HGDB_USER", "db.user");
hdbPassword	= getCfgValue("HGDB_PASSWORD", "db.password");
if (hdbTrackDb == NULL)
    hdbTrackDb      = getCfgValue("HGDB_TRACKDB", "db.trackDb");
if(hdbHost == NULL || hdbUser == NULL || hdbPassword == NULL)
    errAbort("cannot read in connection setting from configuration file.");
}

char *hTrackDbName()
/* return the name of the track database from the config file. Freez when done */
{
if(hdbTrackDb == NULL)
    hdbTrackDb = getCfgValue("HGDB_TRACKDB", "db.trackDb");
if(hdbTrackDb == NULL)
    errAbort("Please set the db.trackDb field in the hg.conf config file.");
return cloneString(hdbTrackDb);
}

char *hTrackDbLocalName()
/* return the name of the trackDbLocal from the config file, or NULL if none.
 * Freez when done */
{
static boolean first = TRUE;
if (first)
    {
    hdbTrackDbLocal = getCfgValue("HGDB_TRACKDB_LOCAL", "db.trackDbLocal");
    first = FALSE;
    }
if (hdbTrackDbLocal == NULL)
    return NULL;
else
    return cloneString(hdbTrackDbLocal);
}

void hSetTrackDbName(char *trackDbName)
/* Override the hg.conf db.trackDb setting. */
{
hdbTrackDb = cloneString(trackDbName);
}

void hSetDbConnect(char* host, char *db, char *user, char *password)
/* set the connection information for the database */
{
    hdbHost = cloneString(host);
    hdbName = cloneString(db);
    hdbUser = cloneString(user);
    hdbPassword = cloneString(password);
}
void hSetDbConnect2(char* host, char *db, char *user, char *password)
/* set the connection information for the database */
{
    hdbHost = cloneString(host);
    hdbName2 = cloneString(db);
    hdbUser = cloneString(user);
    hdbPassword = cloneString(password);
}

boolean hDbExists(char *database)
/*
  Function to check if this is a valid db name
*/
{
struct sqlConnection *conn = hConnectCentral();
char buf[128];
char query[256];
boolean res = FALSE;
sprintf(query, "select name from dbDb where name = '%s'", database);
res = (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL);
hDisconnectCentral(&conn);
return res;
}

boolean hDbIsActive(char *database)
/* Function to check if this is a valid and active db name */
{
struct sqlConnection *conn = hConnectCentral();
char buf[128];
char query[256];
boolean res = FALSE;
sprintf(query, "select name from dbDb where name = '%s' and active = 1",
	database);
res = (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL);
hDisconnectCentral(&conn);
return res;
}

void hSetDb(char *dbName)
/* Set the database name. */
{
if ((hdbCc != NULL) && !sameString(hdbName, dbName))
    errAbort("Can't hSetDb(%s) after an hAllocConn(%s), sorry.",
	     dbName, hdbName);
hdbName = cloneString(dbName);
}

void hSetDb2(char *dbName)
/* Set the database name. */
{
if ((hdbCc2 != NULL) && !sameString(hdbName2, dbName))
    errAbort("Can't hSetDb2(%s) after an hAllocConn2(%s), sorry.",
	     dbName, hdbName2);
hdbName2 = cloneString(dbName);
}

char *hDefaultDbForGenome(char *genome)
/* Purpose: Return the default database matching the Genome.
 * param Genome - The Genome for which we are trying to get the 
 *    default database.
 * return - The default database name for this Genome
 * Free the returned database name. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row;
struct defaultDb *db = NULL;
char query [256];
char *result = NULL;

if (NULL == genome)
    {
    genome = DEFAULT_GENOME;
    }

/* Get proper default from defaultDb table */
sprintf(query, "select * from defaultDb where genome = '%s'", genome);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    db = defaultDbLoad(row);
    }
if (db == NULL)
    {
    /* Can't find any of specified ones ?  Then use the first
     *	This is for the product browser which may have none of
     *	the usual UCSC genomes, but it needs to be able to function.
     */
    sprintf(query, "select * from defaultDb");
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	db = defaultDbLoad(row);
	}
    if (db == NULL)
	errAbort("Can't find genome \"%s\" in central database table defaultDb.\n", genome);
    }

sqlFreeResult(&sr);
hDisconnectCentral(&conn);
AllocArray(result, strlen(db->name) + 1);
strcpy(result, db->name);
defaultDbFree(&db);
return result;
}

char *hDefaultGenomeForClade(char *clade)
/* Return highest relative priority genome for clade. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
char *genome = NULL;
/* Get the top-priority genome *with an active database* so if genomeClade 
 * gets pushed from hgwdev to hgwbeta/RR with genomes whose dbs haven't been 
 * pushed yet, they'll be ignored. */
safef(query, sizeof(query),
      "select genomeClade.genome from genomeClade,dbDb "
      "where genomeClade.clade = '%s' and genomeClade.genome = dbDb.genome "
      "and dbDb.active = 1 "
      "order by genomeClade.priority limit 1",
      clade);
genome = sqlQuickString(conn, query);
hDisconnectCentral(&conn);
return genome;
}

char *hDefaultDb()
/* Return the default db if all else fails */
{
return hDefaultDbForGenome(DEFAULT_GENOME);
}

char *hDefaultChromDb(char *db)
/* Return some sequence named in chromInfo from the given db. */
{
static struct hash *hash = NULL;
char *chrom = NULL;

if (hash == NULL)
    hash = hashNew(0);
chrom = (char *)hashFindVal(hash, db);
if (chrom == NULL)
    {
    struct sqlConnection *conn = hAllocOrConnect(db);
    char buf[HDB_MAX_CHROM_STRING];
    chrom = sqlQuickQuery(conn, "select chrom from chromInfo limit 1",
			  buf, sizeof(buf));
    if (chrom == NULL)
	errAbort("hDefaultChromDb: database %s has no chromInfo", db);
    hFreeOrDisconnect(&conn);
    hashAdd(hash, db, cloneString(chrom));
    }
return cloneString(chrom);
}

char *hDefaultChrom()
/* Return the first chrom in chromInfo from the current db. */
{
return hDefaultChromDb(hGetDb());
}

int hChromCountDb(char *db)
/* Return the number of chromosomes (scaffolds etc.) in the given db. */
{
struct sqlConnection *conn = hAllocOrConnect(db);
int count = sqlQuickNum(conn, "select count(*) from chromInfo");
hFreeOrDisconnect(&conn);
return count;
}

int hChromCount()
/* Return the number of chromosomes (scaffolds etc.) in the current db. */
{
return hChromCountDb(hGetDb());
}

char *hGetDb()
/* Return the current database name, setting to default if not defined. */
{
if (NULL == hdbName)
    {
    hdbName = hDefaultDb();
    }

return hdbName;
}

char *hGetDbUsual(char *usual)
/* Return the current database name, setting to usual if not defined. */
{
if (NULL == hdbName)
    {
    hdbName = cloneString(usual);
    }

return hdbName;
}

char *hGetDb2()
/* Return the secondary database name, setting to default if not defined. */
{
if (NULL == hdbName2)
    {
    hdbName2 = hDefaultDbForGenome("Mouse");
    }

return hdbName2;
}

char *hGetDb2Usual(char *usual)
/* Return the secondary database name, setting to usual if not defined. */
{
if (NULL == hdbName2)
    {
    hdbName2 = cloneString(usual);
    }

return hdbName2;
}

char *hGetDbHost()
/* Return the current database host. */
{
return hdbHost;
}

char *hGetDbName()
/* Return the current database name. */
{
return hdbName;
}

char *hGetDbUser()
/* Return the current database user. */
{
return hdbUser;
}

char *hGetDbPassword()
/* Return the current database password. */
{
return hdbPassword;
}

struct sqlConnection *hAllocConn()
/* Get free connection if possible. If not allocate a new one. */
{
if (hdbHost == NULL)
    hDefaultConnect();
if (hdbCc == NULL)
    hdbCc = sqlNewRemoteConnCache(hdbName, hdbHost, hdbUser, hdbPassword);
return sqlAllocConnection(hdbCc);
}

struct sqlConnection *hAllocConn2()
/* Get free connection if possible. If not allocate a new one. */
{
if (hdbHost == NULL)
    hDefaultConnect();
if (hdbCc2 == NULL)
    hdbCc2 = sqlNewRemoteConnCache(hdbName2, hdbHost, hdbUser, hdbPassword);
return sqlAllocConnection(hdbCc2);
}

struct sqlConnection *hAllocConnDb(char *db)
/* Get free connection if possible. If not allocate a new one. */
{
if (hdbHost == NULL)
    hDefaultConnect();
if (hdbCc == NULL)
    hdbCc = sqlNewRemoteConnCache(hdbName, hdbHost, hdbUser, hdbPassword);
if ( sameString( db, connGetDatabase(hdbCc)))
    return sqlAllocConnection(hdbCc);
if (hdbCc2 == NULL)
    hdbCc2 = sqlNewRemoteConnCache(hdbName2, hdbHost, hdbUser, hdbPassword);
if (sameString(connGetDatabase(hdbCc2),db))
    return sqlAllocConnection(hdbCc2);
else
    errAbort("cannot find a connection to %s\n",db);
return NULL;
}

struct sqlConnection *hAllocOrConnect(char *db)
/* Get available cached connection if possible. If not, just connect. */
{
struct sqlConnection *conn;
if (sameString(db, hGetDbUsual(db)))
    conn = hAllocConn();
else if (sameString(db, hGetDb2Usual(db)))
    conn = hAllocConn2();
else
    {
    conn = sqlConnect(db);
    }
return conn;
}

void hFreeConn(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(hdbCc, pConn);
}

void hFreeConn2(struct sqlConnection **pConn)
/* Put back connection for reuse into second pool for second database connection */
{
sqlFreeConnection(hdbCc2, pConn);
}

void hFreeOrDisconnect(struct sqlConnection **pConn)
/* Free cached or non-cached connection. */
{
char *db = sqlGetDatabase(*pConn);
if (sameString(db, hGetDbUsual(db)))
    hFreeConn(pConn);
else if (sameString(db, hGetDb2Usual(db)))
    hFreeConn2(pConn);
else
    sqlDisconnect(pConn);
}

static struct sqlConnCache *getCentralCcFromCfg(char *prefix)
/* Given a prefix for config settings for a central database connection, 
 * get the settings and make a connection cache for that database. */
{
char *database, *host, *user, *password;
char setting[128];
safef(setting, sizeof(setting), "%s.db", prefix);
database = cfgOption(setting);
safef(setting, sizeof(setting), "%s.host", prefix);
host = cfgOption(setting);
safef(setting, sizeof(setting), "%s.user", prefix);
user = cfgOption(setting);
safef(setting, sizeof(setting), "%s.password", prefix);
password = cfgOption(setting);;

if (database == NULL || host == NULL || user == NULL || password == NULL)
    errAbort("Please set %s options in the hg.conf file.", prefix);
return sqlNewRemoteConnCache(database, host, user, password);
}

struct sqlConnection *hConnectCentral()
/* Connect to central database where user info and other info
 * not specific to a particular genome lives.  Free this up
 * with hDisconnectCentral(). */
{
struct sqlConnection *conn = NULL;
if (centralCc == NULL)
    centralCc = getCentralCcFromCfg("central");
conn = sqlMayAllocConnection(centralCc, FALSE);
if (conn == NULL)
    {
    centralCc = getCentralCcFromCfg("backupcentral");
    conn = sqlAllocConnection(centralCc);
    }
return(conn);
}

void hDisconnectCentral(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(centralCc, pConn);
}

struct sqlConnection *hConnectArchiveCentral()
/* Connect to central database for archives.
 * Free this up with hDisconnectCentralArchive(). */
{
if (centralArchiveCc == NULL)
    centralArchiveCc = getCentralCcFromCfg("archivecentral");
return sqlAllocConnection(centralArchiveCc);
}

void hDisconnectArchiveCentral(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(centralArchiveCc, pConn);
}

struct sqlConnection *hConnectCart()
/* Connect to cart database.  Defaults to the central connection
 * unless cart.db or cart.host are configured. Free this
 * up with hDisconnectCart(). */
{
if (cartCc == NULL)
    {
    if ((cfgOption("cart.db") != NULL) || (cfgOption("cart.host") != NULL)
        || (cfgOption("cart.user") != NULL) || (cfgOption("cart.password") != NULL))
        {
        /* use explict cart options */
        char *database = cfgOption("cart.db");
        char *host = cfgOption("cart.host");
        char *user = cfgOption("cart.user");
        char *password = cfgOption("cart.password");;

        if (database == NULL || host == NULL || user == NULL || password == NULL)
            errAbort("Must specify either all or none of the cart options in the hg.conf file.");
        cartCc = sqlNewRemoteConnCache(database, host, user, password);
        }
    else
        {
        /* use centralCc */
        if (centralCc == NULL)
            {
            /* force creation of central cache */
            struct sqlConnection *conn = hConnectCentral();
            hDisconnectCentral(&conn);
            }
        cartCc = centralCc;
        }
    }
return sqlAllocConnection(cartCc);
}

void hDisconnectCart(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(cartCc, pConn);
}


boolean hCanHaveSplitTables(char *db)
/* Return TRUE if split tables are allowed in database. */
{
struct sqlConnection *conn = hAllocOrConnect(db);
int count = sqlTableSizeIfExists(conn, "chromInfo");
hFreeOrDisconnect(&conn);
return (count >= 0 && count <= HDB_MAX_SEQS_FOR_SPLIT);
}

static struct hash *buildTableListHash(char *db)
/* Return a hash that maps a track/table name (unsplit) to an slName list 
 * of actual table names (possibly split) -- we can compute this once and 
 * cache it to save a lot of querying if we will check existence of 
 * lots of tables. */
{
struct hash *hash = hashNew(14);
struct sqlConnection *conn = hAllocOrConnect(db);
struct slName *allTables = sqlListTables(conn);

if (hCanHaveSplitTables(db))
    {
    /* Consolidate split tables into one list per track: */
    struct slName *tbl = NULL, *nextTbl = NULL;
    for (tbl = allTables;  tbl != NULL;  tbl = nextTbl)
	{
	struct hashEl *tHel = NULL;
	char trackName[HDB_MAX_TABLE_STRING];
	char chrom[HDB_MAX_CHROM_STRING];
	nextTbl = tbl->next;
	tbl->next = NULL;
	hParseTableName(tbl->name, trackName, chrom);
	tHel = hashLookup(hash, trackName);
	if (tHel == NULL)
	    hashAdd(hash, trackName, tbl);
	else if (! sameString(tbl->name, trackName))
	    slAddHead(&(tHel->val), tbl);
	}
    }
else
    {
    /* Just hash all table names: */
    struct slName *tbl = NULL, *nextTbl = NULL;
    for (tbl = allTables;  tbl != NULL;  tbl = nextTbl)
	{
	nextTbl = tbl->next;
	tbl->next = NULL;
	hashAdd(hash, tbl->name, tbl);
	}
    }
hFreeOrDisconnect(&conn);
return hash;
}

static struct hash *tableListHash(char *db)
/* Retrieve (or build if necessary) the cached hash of split-consolidated 
 * tables for db. */
{
static struct hash *dbToTables = NULL;
struct hashEl *dHel = NULL;
if (dbToTables == NULL)
    dbToTables = hashNew(0);
dHel = hashLookup(dbToTables, db);
if (dHel == NULL)
    {
    struct hash *tableHash = buildTableListHash(db);
    hashAdd(dbToTables, db, tableHash);
    return tableHash;
    }
else
    return (struct hash *)(dHel->val);
}


boolean hTableExistsDb(char *db, char *table)
/* Return TRUE if a table exists in db. */
{
struct hash *hash = tableListHash(db);
struct slName *tableNames = NULL, *tbl = NULL;
char trackName[HDB_MAX_TABLE_STRING];
char chrom[HDB_MAX_CHROM_STRING];
hParseTableName(table, trackName, chrom);
tableNames = (struct slName *)hashFindVal(hash, trackName);
for (tbl = tableNames;  tbl != NULL;  tbl = tbl->next)
    {
    if (sameString(table, tbl->name))
	return TRUE;
    }
return FALSE;
}

static void parseDbTable(char *dbTable, char retDb[HDB_MAX_TABLE_STRING],
			 char retTable[HDB_MAX_TABLE_STRING])
/* If dbTable has a '.', split around the . into retDb and retTable.  
 * Otherwise copy hGetDb() into retDb and copy dbTable into retTable. */
{
char *ptr = strchr(dbTable, '.');
if (ptr != NULL)
    {
    snprintf(retDb, min(HDB_MAX_TABLE_STRING, (ptr - dbTable + 1)),
	     dbTable);
    retDb[HDB_MAX_TABLE_STRING-1] = 0;
    safef(retTable, HDB_MAX_TABLE_STRING, ptr+1);
    }
else
    {
    safef(retDb, HDB_MAX_TABLE_STRING, hGetDb());
    safef(retTable, HDB_MAX_TABLE_STRING, dbTable);
    }
}

boolean hTableExists(char *table)
/* Return TRUE if a table exists in database. */
{
char db[HDB_MAX_TABLE_STRING];
char justTable[HDB_MAX_TABLE_STRING];
parseDbTable(table, db, justTable);
return hTableExistsDb(db, justTable);
}

boolean hTableExists2(char *table)
/* Return TRUE if a table exists in secondary database. */
{
return(hTableExistsDb(hGetDb2(), table));
}

boolean hTableOrSplitExistsDb(char *db, char *track)
/* Return TRUE if track table (or split table) exists in db. */
{
struct hash *hash = tableListHash(db);
return (hashLookup(hash, track) != NULL);
}

boolean hTableOrSplitExists(char *track)
/* Return TRUE if table (or a chrN_table) exists in database. */
{
char db[HDB_MAX_TABLE_STRING];
char justTable[HDB_MAX_TABLE_STRING];
parseDbTable(track, db, justTable);
return hTableOrSplitExistsDb(db, justTable);
}

void hParseTableName(char *table, char trackName[HDB_MAX_TABLE_STRING],
		     char chrom[HDB_MAX_CHROM_STRING])
/* Parse an actual table name like "chr17_random_blastzWhatever" into 
 * the track name (blastzWhatever) and chrom (chr17_random). */
/* Note: for the sake of speed, this does not consult chromInfo 
 * because that would be extremely slow for scaffold-based dbs.
 * Instead this makes some assumptions about chromosome names and split 
 * table names in databases that support split tables, and just parses text.
 * When chromosome/table name conventions change, this will need an update! */
{
/* It might not be a split table; provide defaults: */
safef(trackName, HDB_MAX_TABLE_STRING, table);
safef(chrom, HDB_MAX_CHROM_STRING, hDefaultChrom());
if (startsWith("chr", table) || startsWith("Group", table))
    {
    char *ptr = strrchr(table, '_');
    if (ptr != NULL)
	{
	int chromLen = min(HDB_MAX_CHROM_STRING-1, (ptr - table));
	strncpy(chrom, table, chromLen);
	chrom[chromLen] = 0;
	safef(trackName, HDB_MAX_TABLE_STRING, ptr+1);
	}
    }
}

int hChromSize(char *chromName)
/* Return size of chromosome. */
{
struct chromInfo *ci = mustGetChromInfo(hGetDb(), chromName);
return ci->size;
}

int hChromSize2(char *chromName)
/* Return size of chromosome on db2. */
{
struct chromInfo *ci = mustGetChromInfo(hGetDb2(), chromName);
return ci->size;
}

void hNibForChrom(char *chromName, char retNibName[HDB_MAX_PATH_STRING])
/* Get .nib file associated with chromosome. */
{
struct chromInfo *ci = mustGetChromInfo(hGetDb(), chromName);
safef(retNibName, HDB_MAX_PATH_STRING, "%s", ci->fileName);
}

static void hNibForChrom2(char *chromName, char retNibName[HDB_MAX_PATH_STRING])
/* Get .nib file associated with chromosome on db2. */
{
struct chromInfo *ci = mustGetChromInfo(hGetDb2(), chromName);
safef(retNibName, HDB_MAX_PATH_STRING, ci->fileName);
}

struct hash *hCtgPosHash()
/* Return hash of ctgPos from current database keyed by contig name. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(10);
struct ctgPos *ctg;

sr = sqlGetResult(conn, "select * from ctgPos");
while ((row = sqlNextRow(sr)) != NULL)
    {
    ctg = ctgPosLoad(row);
    hashAdd(hash, ctg->contig, ctg);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return hash;
}

struct dnaSeq *hFetchSeqMixed(char *fileName, char *seqName, int start, int end)
/* Fetch mixed case sequence. */
{
if (twoBitIsFile(fileName))
    {
    struct twoBitFile *tbf;
    struct dnaSeq *seq;
    tbf = twoBitOpen(fileName);
    seq = twoBitReadSeqFrag(tbf, seqName, start, end);
    twoBitClose(&tbf);
    return seq;
    }
return nibLoadPartMasked(NIB_MASK_MIXED, fileName, start, end-start);
}

struct dnaSeq *hFetchSeq(char *fileName, char *seqName, int start, int end)
/* Fetch sequence from file.  If it is a .2bit file then fetch the named sequence.
   If it is .nib then just ignore seqName. */
{
if (twoBitIsFile(fileName))
    {
    struct twoBitFile *tbf;
    struct dnaSeq *seq;
    tbf = twoBitOpen(fileName);
    seq = twoBitReadSeqFrag(tbf, seqName, start, end);
    tolowers(seq->dna);
    twoBitClose(&tbf);
    return seq;
    }
return nibLoadPart(fileName, start, end-start);
}

struct dnaSeq *hChromSeqMixed(char *chrom, int start, int end)
/* Return mixed case (repeats in lower case) DNA from chromosome. */
{
char fileName[HDB_MAX_PATH_STRING];
hNibForChrom(chrom, fileName);
return hFetchSeqMixed(fileName, chrom, start, end);
}

struct dnaSeq *hChromSeq(char *chrom, int start, int end)
/* Return lower case DNA from chromosome. */
{
char fileName[HDB_MAX_PATH_STRING];
hNibForChrom(chrom, fileName);
return hFetchSeq(fileName, chrom, start, end);
}

struct dnaSeq *hChromSeq2(char *chrom, int start, int end)
/* Return lower case DNA from chromosome in db2.*/
{
char fileName[HDB_MAX_PATH_STRING];
hNibForChrom2(chrom, fileName);
return hFetchSeq(fileName, chrom, start, end);
}

struct dnaSeq *hSeqForBed(struct bed *bed)
/* Get the sequence associated with a particular bed concatenated together. */
{
char fileName[HDB_MAX_PATH_STRING];
struct dnaSeq *block = NULL;
struct dnaSeq *bedSeq = NULL;
int i = 0 ;
assert(bed);
/* Handle very simple beds and beds with blocks. */
if(bed->blockCount == 0)
    {
    bedSeq = hChromSeq(bed->chrom, bed->chromStart, bed->chromEnd);
    freez(&bedSeq->name);
    bedSeq->name = cloneString(bed->name);
    }
else
    {
    int offSet = bed->chromStart;
    struct dyString *currentSeq = newDyString(2048);
    hNibForChrom(bed->chrom, fileName);
    for(i=0; i<bed->blockCount; i++)
	{
	block = hFetchSeq(fileName, bed->chrom,
			  offSet+bed->chromStarts[i], offSet+bed->chromStarts[i]+bed->blockSizes[i]);
	dyStringAppendN(currentSeq, block->dna, block->size);
	dnaSeqFree(&block);
	}
    AllocVar(bedSeq);
    bedSeq->name = cloneString(bed->name);
    bedSeq->dna = cloneString(currentSeq->string);
    bedSeq->size = strlen(bedSeq->dna);
    dyStringFree(&currentSeq);
    }
if(bed->strand[0] == '-')
    reverseComplement(bedSeq->dna, bedSeq->size);
return bedSeq;
}

boolean hChromBandConn(struct sqlConnection *conn, 
	char *chrom, int pos, char retBand[HDB_MAX_BAND_STRING])
/* Return text string that says what band pos is on. 
 * Return FALSE if not on any band, or table missing. */
{
char query[256];
char buf[HDB_MAX_BAND_STRING];
char *s;
boolean ok = TRUE;
boolean isDmel = startsWith("dm", hGetDb());

sprintf(query, 
	"select name from cytoBand where chrom = '%s' and chromStart <= %d and chromEnd > %d", 
	chrom, pos, pos);
buf[0] = 0;
s = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (s == NULL)
   {
   s = "";
   ok = FALSE;
   }
sprintf(retBand, "%s%s", (isDmel ? "" : skipChr(chrom)), buf);
return ok;
}

boolean hChromBand(char *chrom, int pos, char retBand[HDB_MAX_BAND_STRING])
/* Return text string that says what band pos is on. 
 * Return FALSE if not on any band, or table missing. */
{
if (!hTableExists("cytoBand"))
    return FALSE;
else
    {
    struct sqlConnection *conn = hAllocConn();
    boolean ok = hChromBandConn(conn, chrom, pos, retBand);
    hFreeConn(&conn);
    return ok;
    }
}

boolean hScaffoldPos(char *chrom, int start, int end,
                            char **retScaffold, int *retStart, int *retEnd)
/* Return the scaffold, and start end coordinates on a scaffold, for
 * a chromosome range.  If the range extends past end of a scaffold,
 * it is truncated to the scaffold end.
 * Return FALSE if unable to convert */
{
int ret = FALSE;
char table[HDB_MAX_TABLE_STRING];
safef(table, sizeof(table), "%s_gold", chrom);
if (!hTableExists(table))
    return FALSE;
else
    {
    char query[256];
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    int chromStart, chromEnd;
    int scaffoldStart, scaffoldEnd;
    sprintf(query, 
	"SELECT frag, chromStart, chromEnd FROM %s WHERE chromStart <= %d ORDER BY chromStart DESC LIMIT 1", table, start);
    sr = sqlGetResult(conn, query);
    if (sr != NULL)
        {
        row = sqlNextRow(sr);
        if (row != NULL)
            {
            chromStart = sqlUnsigned(row[1]);
            chromEnd = sqlUnsigned(row[2]);

            scaffoldStart = start - chromStart;
            if (retStart != NULL)
                *retStart = scaffoldStart;

            if (end > chromEnd)
                end = chromEnd;
            scaffoldEnd = end - chromStart;
            if (retEnd != NULL)
                *retEnd = scaffoldEnd;

            if (scaffoldStart < scaffoldEnd)
                {
                /* check for "reversed" endpoints -- e.g.
                 * if printing scaffold itself */
                if (retScaffold != NULL)
                    *retScaffold = cloneString(row[0]);
                 ret = TRUE;
                }
            }
        sqlFreeResult(&sr);
        }
    hFreeConn(&conn);
    return ret;
    }
}

struct dnaSeq *hDnaFromSeq(char *seqName, int start, int end, enum dnaCase dnaCase)
/* Fetch DNA */
{
struct dnaSeq *seq;
if (dnaCase == dnaMixed)
    seq = hChromSeqMixed(seqName, start, end);
else
    {
    seq = hChromSeq(seqName, start, end);
	if (dnaCase == dnaUpper)
	  touppers(seq->dna);
	}
return seq;
}

struct dnaSeq *hLoadChrom(char *chromName)
/* Fetch entire chromosome into memory. */
{
int size = hChromSize(chromName);
return hDnaFromSeq(chromName, 0, size, dnaLower);
}

struct slName *hAllChromNames()
/* Get list of all chromosome names. */
{
return hAllChromNamesDb(hdbName);
}

struct slName *hAllChromNamesDb(char *db)
/* Get list of all chromosome names in database. */
{
struct slName *list = NULL;
struct sqlConnection *conn = hAllocOrConnect(db);
struct sqlResult *sr;
char **row;

sr = sqlGetResult(conn, "select chrom from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct slName *el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hFreeOrDisconnect(&conn);
return list;
}


static char *hExtFileNameC(struct sqlConnection *conn, char *extFileTable, unsigned extFileId)
/* Get external file name from table and ID.  Typically
 * extFile table will be 'extFile' or 'gbExtFile'
 * Abort if the id is not in the table or if the file
 * fails size check.  Please freeMem the result when you 
 * are done with it. (requires conn passed in) */
{
char query[256];
struct sqlResult *sr;
char **row;
long long dbSize, diskSize;
char *path;

safef(query, sizeof(query), 
	"select path,size from %s where id = %u", extFileTable, extFileId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Database inconsistency - no external file with id %lu", extFileId);
path = cloneString(row[0]);
dbSize = sqlLongLong(row[1]);
diskSize = fileSize(path);
if (dbSize != diskSize)
    {
    errAbort("External file %s cannot be opened or has wrong size.  Old size %lld, new size %lld, error %s", 
   	path, dbSize, diskSize, strerror(errno));
    }
sqlFreeResult(&sr);
return path;
}


char *hExtFileName(char *extFileTable, unsigned extFileId)
/* Get external file name from table and ID.  Typically
 * extFile table will be 'extFile' or 'gbExtFile'
 * Abort if the id is not in the table or if the file
 * fails size check.  Please freeMem the result when you 
 * are done with it. */
{
struct sqlConnection *conn = hgAllocConn();
char *path=hExtFileNameC(conn,extFileTable,extFileId);
hgFreeConn(&conn);
return path;
}





/* Constants for selecting seq/extFile or gbSeq/gbExtFile */
#define SEQ_TBL_SET   1
#define GBSEQ_TBL_SET 2

struct largeSeqFile
/* Manages our large external sequence files.  Typically there will
 * be around four of these.  This basically caches the file handle
 * so don't have to keep opening and closing them. */
{
    struct largeSeqFile *next;  /* Next in list. */
    char *path;                 /* Path name for file. */
    unsigned seqTblSet;         /* extFile or gbExtFile */
    char *db;                   /* database this is associated with */
    HGID id;                    /* Id in extFile table. */
    int fd;                     /* File handle. */
    };

static struct largeSeqFile *largeFileList;  /* List of open large files. */


static struct largeSeqFile *largeFileHandle(struct sqlConnection *conn, HGID extId, int seqTblSet)
/* Return handle to large external file. */
{
struct largeSeqFile *lsf;
char *extTable = (seqTblSet == GBSEQ_TBL_SET) ? "gbExtFile" : "extFile";
char *db = sqlGetDatabase(conn); 

/* Search for it on existing list and return it if found. */
for (lsf = largeFileList; lsf != NULL; lsf = lsf->next)
    {
    if ((lsf->id == extId) && (lsf->seqTblSet == seqTblSet) && sameString(lsf->db, db))
        return lsf;
    }

/* Open file and put it on list. */
    {
    struct largeSeqFile *lsf;
    AllocVar(lsf);
    lsf->path = hExtFileNameC(conn, extTable, extId);
    lsf->seqTblSet = seqTblSet;
    lsf->db = cloneString(db);
    lsf->id = extId;
    if ((lsf->fd = open(lsf->path, O_RDONLY)) < 0)
        errAbort("Couldn't open external file %s", lsf->path);
    slAddHead(&largeFileList, lsf);
    return lsf;
    }
}

static void *readOpenFileSection(int fd, off_t offset, size_t size, char *fileName)
/* Allocate a buffer big enough to hold a section of a file,
 * and read that section into it. */
{
void *buf;
buf = needMem(size+1);
if (lseek(fd, offset, SEEK_SET) < 0)
        errAbort("Couldn't seek to %ld in %s", offset, fileName);
if (read(fd, buf, size) < size)
        errAbort("Couldn't read %u bytes at %ld in %s", size, offset, fileName);
return buf;
}

static char* getSeqAndId(struct sqlConnection *conn, char *acc, HGID *retId, char *gbDate)
/* Return sequence as a fasta record in a string and it's database ID, or 
 * NULL if not found. Optionally get genbank modification date. */
{
struct sqlResult *sr = NULL;
char **row;
char query[256];
HGID extId;
size_t size;
off_t offset;
char *buf;
int seqTblSet = SEQ_TBL_SET;
struct largeSeqFile *lsf;

row = NULL;
if (sqlTableExists(conn, "seq"))
    {
    sprintf(query,
       "select id,extFile,file_offset,file_size,gb_date from seq where acc = '%s'",
       acc);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    }

if ((row == NULL) && sqlTableExists(conn, "gbSeq"))
    {
    /* try gbSeq table */
    if (sr)
	sqlFreeResult(&sr);
    if (gbDate != NULL)
        sprintf(query,
                "select gbSeq.id,gbExtFile,file_offset,file_size,moddate from gbSeq,gbCdnaInfo where (gbSeq.acc = '%s') and (gbCdnaInfo.acc = gbSeq.acc)",
                acc);
    else
        sprintf(query,
                "select id,gbExtFile,file_offset,file_size from gbSeq where acc = '%s'",
                acc);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    seqTblSet = GBSEQ_TBL_SET;
    }
if (row == NULL)
    {
    sqlFreeResult(&sr);
    return NULL;
    }
if (retId != NULL)
    *retId = sqlUnsigned(row[0]);
extId = sqlUnsigned(row[1]);
offset = sqlLongLong(row[2]);
size = sqlUnsigned(row[3]);
if (gbDate != NULL)
    strcpy(gbDate, row[4]);
    
sqlFreeResult(&sr);

lsf = largeFileHandle(conn, extId, seqTblSet);
buf = readOpenFileSection(lsf->fd, offset, size, lsf->path);
return buf; 
}

static char* mustGetSeqAndId(struct sqlConnection *conn, char *acc,
                             HGID *retId)
/* Return sequence as a fasta record in a string and it's database ID,
 * abort if not found */
{
char *buf= getSeqAndId(conn, acc, retId, NULL);
if (buf == NULL)
    errAbort("No sequence for %s in seq or gbSeq tables", acc);
return buf;
}

char* hGetSeqAndId(struct sqlConnection *conn, char *acc, HGID *retId)
/* Return sequence as a fasta record in a string and it's database ID, or 
 * NULL if not found. */
{
return getSeqAndId(conn, acc, retId, NULL);
}

int hRnaSeqAndIdx(char *acc, struct dnaSeq **retSeq, HGID *retId, char *gbdate, struct sqlConnection *conn)
/* Return sequence for RNA, it's database ID, and optionally genbank 
 * modification date. Return -1 if not found. */
{
char *buf = getSeqAndId(conn, acc, retId, gbdate);
if (buf == NULL)
    return -1;
*retSeq = faFromMemText(buf);
return 0;
}

void hRnaSeqAndId(char *acc, struct dnaSeq **retSeq, HGID *retId)
/* Return sequence for RNA and it's database ID. */
{
struct sqlConnection *conn = hAllocConn();
char *buf = mustGetSeqAndId(conn, acc, retId);
*retSeq = faFromMemText(buf);
hFreeConn(&conn);
}

struct dnaSeq *hExtSeq(char *acc)
/* Return sequence for external sequence. */
{
struct dnaSeq *seq;
HGID id;
hRnaSeqAndId(acc, &seq, &id);
return seq;
}

struct dnaSeq *hExtSeqPart(char *acc, int start, int end)
/* Return part of external sequence. */
{
struct dnaSeq *seq = hExtSeq(acc);
//FIXME: freeing this won't free up the entire DNA seq
if (end > seq->size)
    errAbort("Can't extract partial seq: acc=%s, end=%d, size=%d",
                acc, end, seq->size);
return newDnaSeq(seq->dna + start, end - start, acc);
}

struct dnaSeq *hRnaSeq(char *acc)
/* Return sequence for RNA. */
{
return hExtSeq(acc);
}

aaSeq *hPepSeq(char *acc)
/* Return sequence for a peptide. */
{
struct sqlConnection *conn = hAllocConn();
char *buf = mustGetSeqAndId(conn, acc, NULL);
hFreeConn(&conn);
return faSeqFromMemText(buf, FALSE);
}

static boolean checkIfInTable(struct sqlConnection *conn, char *acc,
                              char *column, char *table)
/* check if a a sequences exists in a table */
{
boolean inTable = FALSE;
char query[256];
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), "select 0 from %s where %s = \"%s\"",
      table, column, acc);
sr = sqlGetResult(conn, query);
inTable = ((row = sqlNextRow(sr)) != NULL);
sqlFreeResult(&sr);
return inTable;
}

boolean hGenBankHaveSeq(char *acc, char *compatTable)
/* Get a GenBank or RefSeq mRNA or EST sequence or NULL if it doesn't exist.
 * This handles compatibility between pre-incremental genbank databases where
 * refSeq sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */
{
struct sqlConnection *conn = hAllocConn();
boolean haveSeq = FALSE;

/* Check compatTable if we have it, otherwise check seq and gbSeq */
if ((compatTable != NULL) && sqlTableExists(conn, compatTable))
    {
    haveSeq = checkIfInTable(conn, acc, "name", compatTable);
    }
else
    {
    if (sqlTableExists(conn, "gbSeq"))
        haveSeq = checkIfInTable(conn, acc, "acc", "gbSeq");
    if ((!haveSeq) && sqlTableExists(conn, "seq"))
        haveSeq = checkIfInTable(conn, acc, "acc", "seq");
    }

hFreeConn(&conn);
return haveSeq;
}

static struct dnaSeq *loadSeqFromTable(struct sqlConnection *conn,
                                       char *acc, char *table)
/* load a sequence from table. */
{
struct dnaSeq *seq = NULL;
struct sqlResult *sr;
char **row;
char query[256];

safef(query, sizeof(query), "select name,seq from %s where name = '%s'",
      table, acc);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    seq = newDnaSeq(cloneString(row[1]), strlen(row[1]), row[0]);

sqlFreeResult(&sr);
return seq;
}



struct dnaSeq *hGenBankGetMrnaC(struct sqlConnection *conn, char *acc, char *compatTable)
/* Get a GenBank or RefSeq mRNA or EST sequence or NULL if it doesn't exist.
 * This handles compatibility between pre-incremental genbank databases where
 * refSeq sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */
{
struct dnaSeq *seq = NULL;

/* If we have the compat table, get the sequence from there, otherwise from
 * seq or gbSeq. */
if ((compatTable != NULL) && sqlTableExists(conn, compatTable))
    {
    seq = loadSeqFromTable(conn, acc, compatTable);
    }
else 
    {
    char *buf = getSeqAndId(conn, acc, NULL, NULL);
    if (buf != NULL)
        seq = faFromMemText(buf);
    }

return seq;
}


struct dnaSeq *hGenBankGetMrna(char *acc, char *compatTable)
/* Get a GenBank or RefSeq mRNA or EST sequence or NULL if it doesn't exist.
 * This handles compatibility between pre-incremental genbank databases where
 * refSeq sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */
{
struct sqlConnection *conn = hAllocConn();
struct dnaSeq *seq = hGenBankGetMrnaC(conn, acc, compatTable);
hFreeConn(&conn);
return seq;
}



aaSeq *hGenBankGetPepC(struct sqlConnection *conn, char *acc, char *compatTable)
/* Get a RefSeq peptide sequence or NULL if it doesn't exist.  This handles
 * compatibility between pre-incremental genbank databases where refSeq
 * sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */
{
aaSeq *seq = NULL;


/* If we have the compat table, get the sequence from there, otherwise from
 * gbSeq. */
if ((compatTable != NULL) && sqlTableExists(conn, compatTable))
    {
    seq = loadSeqFromTable(conn, acc, compatTable);
    }
else
    {
    char *buf = getSeqAndId(conn, acc, NULL, NULL);
    if (buf != NULL)
        seq = faSeqFromMemText(buf, FALSE);
    }
return seq;
}


aaSeq *hGenBankGetPep(char *acc, char *compatTable)
/* Get a RefSeq peptide sequence or NULL if it doesn't exist.  This handles
 * compatibility between pre-incremental genbank databases where refSeq
 * sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */
{
struct sqlConnection *conn = hAllocConn();
aaSeq *seq = hGenBankGetPepC(conn, acc, compatTable);
hFreeConn(&conn);
return seq;
}

char *hGenBankGetDesc(char *acc, boolean native)
/* Get a description for a genbank or refseq mRNA. If native is TRUE, an
 * attempt is made to get a more compact description that doesn't include
 * species name. Acc may optionally include the version.  NULL is returned if
 * a description isn't available.  Free string when done. */
{
struct sqlConnection *conn = hAllocConn();
char *desc =  NULL;
char accId[GENBANK_ACC_BUFSZ], query[256];

genbankDropVer(accId, acc);

if (native && genbankIsRefSeqAcc(accId))
    {
    safef(query, sizeof(query), "select product from refLink where mrnaAcc = \"%s\"", accId);
    desc = sqlQuickString(conn, query);
    }

if (desc == NULL)
    {
    safef(query, sizeof(query), "select description.name from description,gbCdnaInfo "
          "where gbCdnaInfo.acc = \"%s\" "
          "and gbCdnaInfo.description = description.id", accId);
    desc = sqlQuickString(conn, query);
    }
hFreeConn(&conn);
return desc;
}

struct bed *hGetBedRangeDb(char *db, char *table, char *chrom, int chromStart,
			   int chromEnd, char *sqlConstraints)
/* Return a bed list of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table. */
{
struct dyString *query = newDyString(512);
struct sqlConnection *conn = hAllocOrConnect(db);
struct sqlResult *sr;
struct hTableInfo *hti;
struct bed *bedList=NULL, *bedItem;
char **row;
char parsedChrom[HDB_MAX_CHROM_STRING];
char rootName[256];
char fullTableName[256];
char rangeStr[32];
int count;
boolean canDoUTR, canDoIntrons;
boolean useSqlConstraints = sqlConstraints != NULL && sqlConstraints[0] != 0;
char tStrand = '?', qStrand = '?';
int i;

/* Caller can give us either a full table name or root table name. */
hParseTableName(table, rootName, parsedChrom);
hti = hFindTableInfoDb(db, chrom, rootName);
if (hti == NULL)
    errAbort("Could not find table info for table %s (%s)",
	     rootName, table);
if (hti->isSplit)
    safef(fullTableName, sizeof(fullTableName), "%s_%s", chrom, rootName);
else
    safef(fullTableName, sizeof(fullTableName), rootName);
canDoUTR = hti->hasCDS;
canDoIntrons = hti->hasBlocks;

dyStringClear(query);
// row[0], row[1] -> start, end
dyStringPrintf(query, "SELECT %s,%s", hti->startField, hti->endField);
// row[2] -> name or placeholder
if (hti->nameField[0] != 0)
    dyStringPrintf(query, ",%s", hti->nameField);
else
    dyStringPrintf(query, ",%s", hti->startField);  // keep the same #fields!
// row[3] -> score or placeholder
if (hti->scoreField[0] != 0)
    dyStringPrintf(query, ",%s", hti->scoreField);
else
    dyStringPrintf(query, ",%s", hti->startField);  // keep the same #fields!
// row[4] -> strand or placeholder
if (hti->strandField[0] != 0)
    dyStringPrintf(query, ",%s", hti->strandField);
else
    dyStringPrintf(query, ",%s", hti->startField);  // keep the same #fields!
// row[5], row[6] -> cdsStart, cdsEnd or placeholders
if (hti->cdsStartField[0] != 0)
    dyStringPrintf(query, ",%s,%s", hti->cdsStartField, hti->cdsEndField);
else
    dyStringPrintf(query, ",%s,%s", hti->startField, hti->startField);  // keep the same #fields!
// row[7], row[8], row[9] -> count, starts, ends/sizes or empty.
if (hti->startsField[0] != 0)
    dyStringPrintf(query, ",%s,%s,%s", hti->countField, hti->startsField,
		   hti->endsSizesField);
else
    dyStringPrintf(query, ",%s,%s,%s", hti->startField, hti->startField,
		   hti->startField);  // keep same #fields!
// row[10] -> tSize for PSL '-' strand coord-swizzling only:
if (sameString("tStarts", hti->startsField))
    dyStringAppend(query, ",tSize");
else
    dyStringPrintf(query, ",%s", hti->startField);  // keep the same #fields!
dyStringPrintf(query, " FROM %s WHERE %s < %d AND %s > %d",
	       fullTableName,
	       hti->startField, chromEnd, hti->endField, chromStart);
if (hti->chromField[0] != 0)
    dyStringPrintf(query, " AND %s = '%s'", hti->chromField, chrom);
if (useSqlConstraints)
    dyStringPrintf(query, " AND %s", sqlConstraints);

sr = sqlGetResult(conn, query->string);

while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(bedItem);
    bedItem->chrom      = cloneString(chrom);
    bedItem->chromStart = atoi(row[0]);
    bedItem->chromEnd   = atoi(row[1]);
    if (hti->nameField[0] != 0)
	bedItem->name   = cloneString(row[2]);
    else
	{
	snprintf(rangeStr, sizeof(rangeStr), "%s:%d-%d", chrom,
		 bedItem->chromStart+1,  bedItem->chromEnd);
	bedItem->name   = cloneString(rangeStr);
	}
    if (hti->scoreField[0] != 0)
	bedItem->score  = atoi(row[3]);
    else
	bedItem->score  = 0;
    if (hti->strandField[0] != 0)
	if (sameString("tStarts", hti->startsField))
	    {
	    // psl: use XOR of qStrand,tStrand if both are given.
	    qStrand = row[4][0];
	    tStrand = row[4][1];
	    if ((tStrand != '+') && (tStrand != '-'))
		bedItem->strand[0] = qStrand;
	    else if ((qStrand == '-' && tStrand == '+') ||
		     (qStrand == '+' && tStrand == '-'))
		strncpy(bedItem->strand, "-", 2);
	    else
		strncpy(bedItem->strand, "+", 2);
	    }
	else
	    strncpy(bedItem->strand, row[4], 2);
    else
	strcpy(bedItem->strand, ".");
    if (canDoUTR)
	{
	bedItem->thickStart = atoi(row[5]);
	bedItem->thickEnd   = atoi(row[6]);
	/* thickStart, thickEnd fields are sometimes used for other-organism 
	   coords (e.g. synteny100000, syntenyBuild30).  So if they look 
	   completely wrong, fake them out to start/end.  */
	if (bedItem->thickStart < bedItem->chromStart)
	    bedItem->thickStart = bedItem->chromStart;
	else if (bedItem->thickStart > bedItem->chromEnd)
	    bedItem->thickStart = bedItem->chromStart;
	if (bedItem->thickEnd < bedItem->chromStart)
	    bedItem->thickEnd = bedItem->chromEnd;
	else if (bedItem->thickEnd > bedItem->chromEnd)
	    bedItem->thickEnd = bedItem->chromEnd;
	}
    else
	{
	bedItem->thickStart = bedItem->chromStart;
	bedItem->thickEnd   = bedItem->chromEnd;
	}
    if (canDoIntrons)
	{
	bedItem->blockCount = atoi(row[7]);
	sqlSignedDynamicArray(row[8], &bedItem->chromStarts, &count);
	if (count != bedItem->blockCount)
	    errAbort("Data error: block count (%d) must be the same as the number of block starts (%d) for table %s item %s %s:%d-%d",
		     bedItem->blockCount, count, table,
		     bedItem->name, bedItem->chrom,
		     bedItem->chromStart, bedItem->chromEnd);
	sqlSignedDynamicArray(row[9], &bedItem->blockSizes, &count);
	if (count != bedItem->blockCount)
	    errAbort("Data error: block count (%d) must be the same as the number of block ends/sizes (%d) for table %s item %s %s:%d-%d",
		     bedItem->blockCount, count, table,
		     bedItem->name, bedItem->chrom,
		     bedItem->chromStart, bedItem->chromEnd);
	if (sameString("exonEnds", hti->endsSizesField))
	    {
	    // genePred: translate ends to sizes
	    for (i=0;  i < bedItem->blockCount;  i++)
		{
		bedItem->blockSizes[i] -= bedItem->chromStarts[i];
		}
	    }
	if (sameString("tStarts", hti->startsField)) // psls
	    {
	    if (tStrand == '-')
		{
		int tSize = atoi(row[10]);
		// if protein then blockSizes are in protein space
		if (bedItem->chromStart == 
			tSize - (3*bedItem->blockSizes[bedItem->blockCount - 1]  + 
			bedItem->chromStarts[bedItem->blockCount - 1]))
		    {
		    for (i=0; i<bedItem->blockCount; ++i)
			bedItem->blockSizes[i] *= 3;
		    }

		// psl: if target strand is '-', flip the coords.
		// (this is the target part of pslRcBoth from src/lib/psl.c)
		for (i=0; i<bedItem->blockCount; ++i)
		    {
		    bedItem->chromStarts[i] = tSize - (bedItem->chromStarts[i] +
						       bedItem->blockSizes[i]);
		    }
		reverseInts(bedItem->chromStarts, bedItem->blockCount);
		reverseInts(bedItem->blockSizes, bedItem->blockCount);
		}
	    else
		{
		// if protein then blockSizes are in protein space
		if (bedItem->chromEnd == 
			3*bedItem->blockSizes[bedItem->blockCount - 1]  + 
			bedItem->chromStarts[bedItem->blockCount - 1])
		    {
		    for (i=0; i<bedItem->blockCount; ++i)
			bedItem->blockSizes[i] *= 3;
		    }
		}
	if (bedItem->chromStart != bedItem->chromStarts[0])
	    errAbort("Data error: start (%d) must be the same as first block start (%d) for table %s item %s %s:%d-%d",
		     bedItem->chromStart, bedItem->chromStarts[0],
		     table, bedItem->name,
		     bedItem->chrom, bedItem->chromStart, bedItem->chromEnd);
	    }
	if (! (sameString("chromStarts", hti->startsField) ||
	       sameString("blockStarts", hti->startsField)) )
	    {
	    // non-bed: translate absolute starts to relative starts
	    for (i=0;  i < bedItem->blockCount;  i++)
		{
		bedItem->chromStarts[i] -= bedItem->chromStart;
		}
	    }
	}
    else
	{
	bedItem->blockCount  = 0;
	bedItem->chromStarts = NULL;
	bedItem->blockSizes  = NULL;
	}
    slAddHead(&bedList, bedItem);
    }
dyStringFree(&query);
sqlFreeResult(&sr);
hFreeOrDisconnect(&conn);
slReverse(&bedList);
return(bedList);
}


struct bed *hGetBedRange(char *table, char *chrom, int chromStart,
			 int chromEnd, char *sqlConstraints)
/* Return a bed list of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table. */
{
return(hGetBedRangeDb(hGetDb(), table, chrom, chromStart, chromEnd,
		      sqlConstraints));
}

char *hPdbFromGdb(char *genomeDb)
/* Find proteome database name given genome database name */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
char *ret = NULL;
struct dyString *dy = newDyString(128);

if (sqlTableExists(conn, "gdbPdb"))
    {
    if (genomeDb != NULL)
	dyStringPrintf(dy, "select proteomeDb from gdbPdb where genomeDb = '%s';", genomeDb);
    else
	internalErr();
    sr = sqlGetResult(conn, dy->string);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	ret = cloneString(row[0]);
	}
    else
	{
	// if a corresponding protein DB is not found, get the default one from the gdbPdb table
        sqlFreeResult(&sr);
    	sr = sqlGetResult(conn,  "select proteomeDb from gdbPdb where genomeDb = 'default';");
    	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    ret = cloneString(row[0]);
	    }
	else
	    {
	    errAbort("No protein database defined for %s.", genomeDb);
	    }
	}
	
    sqlFreeResult(&sr);
    }
hDisconnectCentral(&conn);
freeDyString(&dy);
return(ret);
}

static char *hFreezeDbConversion(char *database, char *freeze)
/* Find freeze given database or vice versa.  Pass in NULL
 * for parameter that is unknown and it will be returned
 * as a result.  This result can be freeMem'd when done. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
char *ret = NULL;
struct dyString *dy = newDyString(128);

if (database != NULL)
    dyStringPrintf(dy, "select description from dbDb where name = '%s'", database);
else if (freeze != NULL)
    dyStringPrintf(dy, "select name from dbDb where description = '%s'", freeze);
else
    internalErr();
sr = sqlGetResult(conn, dy->string);
if ((row = sqlNextRow(sr)) != NULL)
    ret = cloneString(row[0]);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
freeDyString(&dy);
return ret;
}


char *hFreezeFromDb(char *database)
/* return the freeze for the database version. 
   For example: "hg6" returns "Dec 12, 2000". If database
   not recognized returns NULL */
{
return hFreezeDbConversion(database, NULL);
}

char *hDbFromFreeze(char *freeze)
/* Return database version from freeze name. */
{
return hFreezeDbConversion(NULL, freeze);
}

boolean hgNearOk(char *database)
/* Return TRUE if ok to put up familyBrowser (hgNear) 
 * on this database. */
{
struct sqlConnection *conn = hConnectCentral();
char query[256];
boolean ok;
safef(query, sizeof(query), 
	"select hgNearOk from dbDb where name = '%s'", database);
ok = sqlQuickNum(conn, query);
hDisconnectCentral(&conn);
return ok;
}

boolean hgPbOk(char *database)
/* Return TRUE if ok to put up Proteome Browser (pbTracks)
 * on this database. */
{
struct sqlConnection *conn = hConnectCentral();
char query[256];
char **row;
struct sqlResult *sr = NULL;
boolean ok;
boolean dbDbHasPbOk;

dbDbHasPbOk = FALSE;
safef(query, sizeof(query), "describe dbDb");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameWord(row[0], "hgPbOk"))
        {
        dbDbHasPbOk = TRUE;
        }
    }
sqlFreeResult(&sr);
if (!dbDbHasPbOk) return(FALSE);

safef(query, sizeof(query),
        "select hgPbOk from dbDb where name = '%s'", database);
ok = sqlQuickNum(conn, query);
hDisconnectCentral(&conn);
return ok;
}

boolean hgPcrOk(char *database)
/* Return TRUE if ok to put up hgPcr on this database. */
{
struct sqlConnection *conn = hConnectCentral();
char query[256];
boolean ok;
safef(query, sizeof(query), 
	"select canPcr from blatServers where db = '%s' and isTrans=0", database);
ok = sqlQuickNum(conn, query);
hDisconnectCentral(&conn);
return ok;
}



char *hArchiveOrCentralDbDbOptionalField(char *database, char *field, boolean archive)
/* Look up field in dbDb table keyed by database,
 * Return NULL if database doesn't exist. 
 * Free this string when you are done. Look in 
 * either the regular or the archive database. 
 * The name for this function may be a little silly. */
{
struct sqlConnection *conn;
char buf[128];
char query[256];
char *res = NULL;
conn = (archive) ? hConnectArchiveCentral() : hConnectCentral();
sprintf(query, "select %s from dbDb where name = '%s'", field, database);
if (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL)
    res = cloneString(buf);
if (archive)
    hDisconnectArchiveCentral(&conn);
else 
    hDisconnectCentral(&conn);
return res;
}

char *hArchiveDbDbOptionalField(char *database, char *field)
/* Wrapper for hArchiveOrCentralDbDbOptionalField to 
 * look up in the archive database. */
{
return hArchiveOrCentralDbDbOptionalField(database, field, TRUE);
}

char *hDbDbOptionalField(char *database, char *field)
/* Wrapper for hArchiveOrCentralDbDbOptionalField to 
 * look up in the regular central database. */
{
return hArchiveOrCentralDbDbOptionalField(database, field, FALSE);
}

char *hDbDbField(char *database, char *field)
/* Look up field in dbDb table keyed by database.
 * Free this string when you are done. */
{
char *res = hDbDbOptionalField(database, field);
if (res == NULL)
    errAbort("Can't find %s for %s", field, database);
return res;
}

char *hDefaultPos(char *database)
/* Return default chromosome position for the 
  organism associated with database.   use freeMem on
 * this when done. */
{
return hDbDbField(database, "defaultPos");
}

char *hOrganism(char *database)
/* Return organism associated with database.   use freeMem on
 * this when done. */
{
if (sameString(database, "rep"))    /* bypass dbDb if repeat */
    return cloneString("Repeat");
return hDbDbOptionalField(database, "organism");
}

char *hArchiveOrganism(char *database)
/* Return organism name from the archive database.  E.g. "hg12". */
{
char *organism = hOrganism(database);
if (!organism)
    organism = hArchiveDbDbOptionalField(database, "organism");
return organism;
}

char *hGenome(char *database)
/* Return genome associated with database.   
 * use freeMem on this when done. */
{
return hDbDbOptionalField(database, "genome");
}

char *hScientificName(char *database)
/* Return scientific name for organism represented by this database */
/* Return NULL if unknown database */
/* NOTE: must free returned string after use */
{
return hDbDbOptionalField(database, "scientificName");
}

char *hHtmlPath(char *database)
/* Return /gbdb path name to html description for this database */
/* Return NULL if unknown database */
/* NOTE: must free returned string after use */
{
return hDbDbOptionalField(database, "htmlPath");
}

char *hFreezeDate(char *database)
/* Return freeze date of database. Use freeMem when done. */
{
return hDbDbField(database, "description");
}

int hOrganismID(char *database)
/* Get organism ID from relational organism table */
/* Return -1 if not found */
{
char query[256];
struct sqlConnection *conn = hAllocOrConnect(database);
int ret;

sprintf(query, "select id from organism where name = '%s'",
				    hScientificName(database));
ret = sqlQuickNum(conn, query);
hFreeOrDisconnect(&conn);
return ret;
}

static boolean hGotCladeConn(struct sqlConnection *conn)
/* Return TRUE if central db contains clade info tables. */
{
return (sqlTableExists(conn, "clade") && sqlTableExists(conn, "genomeClade"));
}

boolean hGotClade()
/* Return TRUE if central db contains clade info tables. */
{
struct sqlConnection *conn = hConnectCentral();
boolean gotClade = hGotCladeConn(conn);
hDisconnectCentral(&conn);
return gotClade;
}

char *hClade(char *genome)
/* If central database has clade tables, return the clade for the 
 * given genome; otherwise return NULL. */
{
struct sqlConnection *conn = hConnectCentral();
if (hGotCladeConn(conn))
    {
    char query[512];
    char *clade;
    safef(query, sizeof(query),
	  "select clade from genomeClade where genome = '%s'", genome);
    clade = sqlQuickString(conn, query);
    hDisconnectCentral(&conn);
    if (clade == NULL)
	{
	warn("Warning: central database genomeClade doesn't contain "
	     "genome \"%s\"", genome);
	return cloneString("other");
	}
    else
	return clade;
    }
else
    {
    hDisconnectCentral(&conn);
    return NULL;
    }
}

static void addSubVar(char *prefix, char *name, 
	char *value, struct subText **pList)
/* Add substitution to list. */
{
struct subText *sub;
char fullName[HDB_MAX_TABLE_STRING];
safef(fullName, sizeof(fullName), "$%s%s", prefix, name);
sub = subTextNew(fullName, value);
slAddHead(pList, sub);
}

static boolean isAbbrevScientificName(char *name)
/* Return true if name looks like an abbreviated scientific name 
* (e.g. D. yakuba). */
{
return (name != NULL && strlen(name) > 4 &&
	isalpha(name[0]) &&
	name[1] == '.' && name[2] == ' ' &&
	isalpha(name[3]));
}

void hAddDbSubVars(char *prefix, char *database, struct subText **pList)
/* Add substitution variables associated with database to list. */
{
char *organism = hOrganism(database);
if (organism != NULL)
    {
    char *lcOrg = cloneString(organism);
    char *ucOrg = cloneString(organism);
    char *date = hFreezeDate(database);
    if (! isAbbrevScientificName(organism))
	tolowers(lcOrg);
    touppers(ucOrg);
    addSubVar(prefix, "Organism", organism, pList);
    addSubVar(prefix, "ORGANISM", ucOrg, pList);
    addSubVar(prefix, "organism", lcOrg, pList);
    addSubVar(prefix, "date", date, pList);
    freez(&date);
    freez(&ucOrg);
    freez(&lcOrg);
    freez(&organism);
    }
addSubVar(prefix, "db", database, pList);
}

static void subOut(struct trackDb *tdb, char **pString, struct subText *subList)
/* Substitute one string. */
{
char *old = *pString;
*pString = subTextString(subList, old);
freeMem(old);
}

static void subOutAll(struct trackDb *tdb, struct subText *subList)
/* Substitute all strings that need substitution. */
{
subOut(tdb, &tdb->shortLabel, subList);
subOut(tdb, &tdb->longLabel, subList);
subOut(tdb, &tdb->html, subList);
}

void hLookupStringsInTdb(struct trackDb *tdb, char *database)
/* Lookup strings in track database. */
{
static struct subText *subList = NULL;
static char *oldDatabase = NULL;

if (oldDatabase != NULL && !sameString(database, oldDatabase))
    {
    subTextFreeList(&subList);
    freez(&oldDatabase);
    oldDatabase = cloneString(database);
    }
if (subList == NULL)
    hAddDbSubVars("", database, &subList);
subOutAll(tdb, subList);

if (tdb->settings != NULL && tdb->settings[0] != 0)
    {
    struct subText *subList = NULL;
    char *otherDb = trackDbSetting(tdb, "otherDb");
    char *blurb = trackDbSetting(tdb, "blurb");
    if (blurb != NULL)
	addSubVar("", "blurb", blurb, &subList);
    if (otherDb != NULL)
	hAddDbSubVars("o_", otherDb, &subList);
    subOutAll(tdb, subList);
    subTextFreeList(&subList);
    }
}


struct dbDb *hDbDbList()
/* Return list of databases that are actually online. 
 * The list includes the name, description, and where to
 * find the nib-formatted DNA files. Free this with dbDbFree. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
struct dbDb *dbList = NULL, *db;
struct hash *hash = sqlHashOfDatabases();

sr = sqlGetResult(conn, "select * from dbDb order by orderKey,name desc");
while ((row = sqlNextRow(sr)) != NULL)
    {
    db = dbDbLoad(row);
    if (hashLookup(hash, db->name))
        {
	slAddHead(&dbList, db);
	}
    else
        dbDbFree(&db);
    }
sqlFreeResult(&sr);
hashFree(&hash);
hDisconnectCentral(&conn);
slReverse(&dbList);
return dbList;
}

struct dbDb *archiveDbDbLoad(char **row)
/* Load a archive dbDb from row fetched with select * from dbDb
         from database.  Dispose of this with dbDbFree().
  NOTE: this table schema is now detached from the
  main production dbDb, so we are not using the autoSql functions */
{
    struct dbDb *ret;

    AllocVar(ret);
    ret->name = cloneString(row[0]);
    ret->description = cloneString(row[1]);
    ret->nibPath = cloneString(row[2]);
    ret->organism = cloneString(row[3]);
    ret->defaultPos = cloneString(row[4]);
    ret->active = sqlSigned(row[5]);
    ret->orderKey = sqlSigned(row[6]);
    ret->genome = cloneString(row[7]);
    ret->scientificName = cloneString(row[8]);
    ret->htmlPath = cloneString(row[9]);
    ret->hgNearOk = sqlSigned(row[10]);
    return ret;
}

struct dbDb *hArchiveDbDbList()
/* Return list of databases in archive central dbDb.
 * Free this with dbDbFree. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct dbDb *dbList = NULL, *db;
char *assembly;
char *next;

conn = hConnectArchiveCentral();
if (conn)
    {
    /* NOTE: archive orderKey convention is opposite of production server! */
    sr = sqlGetResult(conn, "select * from dbDb order by orderKey desc,name desc");
    while ((row = sqlNextRow(sr)) != NULL)
        {
        db = archiveDbDbLoad(row);
        /* strip organism out of assembly description if it's there
         * (true in hg6-hg11 entries) */
        next = assembly = cloneString(db->description);
        if (sameString(nextWord(&next), db->genome))
            {
            freez(&db->description);
            db->description = cloneString(next);
            }
        freez(&assembly);
        slAddHead(&dbList, db);
        }
    sqlFreeResult(&sr);
    hDisconnectArchiveCentral(&conn);
    slReverse(&dbList);
    }
return dbList;
}

struct slName *hDbList()
/* List of all database versions that are online (database
 * names only).  See also hDbDbList. */
{
struct slName *nList = NULL, *n;
struct dbDb *dbList, *db;

dbList = hDbDbList();
for (db = dbList; db != NULL; db = db->next)
    {
    n = newSlName(db->name);
    slAddTail(&nList, n);
    }
dbDbFree(&dbList);
return nList;
}

char *hPreviousAssembly(char *database)
/* Return previous assembly for the genome associated with database, or NULL.
 * Must free returned string */

{
struct dbDb *dbList = NULL, *db, *prevDb;
char *prev = NULL;

/* NOTE: relies on this list being ordered descendingly */
dbList = hDbDbList();
for (db = dbList; db != NULL; db = db->next)
    {
    if (sameString(db->name, database))
        {
        prevDb = db->next;
        if (prevDb)
            prev = cloneString(prevDb->name);
        break;
        }
    }
if (dbList)
    dbDbFreeList(&dbList);
return prev;
}


static boolean fitField(struct hash *hash, char *fieldName,
	char retField[HDB_MAX_FIELD_STRING])
/* Return TRUE if fieldName is in hash.  
 * If so copy it to retField.
 * Helper routine for findMoreFields below. */
{
if (hashLookup(hash, fieldName))
    {
    strcpy(retField, fieldName);
    return TRUE;
    }
else
    {
    retField[0] = 0;
    return FALSE;
    }
}

static boolean fitFields(struct hash *hash, char *chrom, char *start, char *end,
	char retChrom[HDB_MAX_FIELD_STRING], char retStart[HDB_MAX_FIELD_STRING], char retEnd[HDB_MAX_FIELD_STRING])
/* Return TRUE if chrom/start/end are in hash.  
 * If so copy them to retChrom, retStart, retEnd. 
 * Helper routine for findChromStartEndFields below. */
{
if (hashLookup(hash, chrom) && hashLookup(hash, start) && hashLookup(hash, end))
    {
    strcpy(retChrom, chrom);
    strcpy(retStart, start);
    strcpy(retEnd, end);
    return TRUE;
    }
else
    return FALSE;
}

boolean hIsBinned(char *table)
/* Return TRUE if a table is binned. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
boolean binned = FALSE;

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], "bin"))
        binned = TRUE;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return binned;
}

int hFieldIndex(char *table, char *field)
/* Return index of field in table or -1 if it doesn't exist. */
{
struct sqlConnection *conn = hAllocConn();
int result = sqlFieldIndex(conn, table, field);
hFreeConn(&conn);
return result;
}

boolean hHasField(char *table, char *field)
/* Return TRUE if table has field */
{
return hFieldIndex(table, field) >= 0;
}

boolean hFieldHasIndexDb(char *db, char *table, char *field)
/* Return TRUE if a SQL index exists for table.field. */
{
struct sqlConnection *conn = hAllocOrConnect(db);
struct sqlResult *sr = NULL;
char **row = NULL;
boolean gotIndex = FALSE;
char query[512];

safef(query, sizeof(query), "show index from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[4], field))
	{
	gotIndex = TRUE;
	break;
	}
    }
sqlFreeResult(&sr);
hFreeOrDisconnect(&conn);
return(gotIndex);
}

boolean hFieldHasIndex(char *table, char *field)
/* Return TRUE if a SQL index exists for table.field. */
{
return(hFieldHasIndexDb(hGetDb(), table, field));
}

boolean hFindBed12FieldsAndBinDb(char *db, char *table, 
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING],
	char retEnd[HDB_MAX_FIELD_STRING],
	char retName[HDB_MAX_FIELD_STRING],
	char retScore[HDB_MAX_FIELD_STRING],
	char retStrand[HDB_MAX_FIELD_STRING],
        char retCdsStart[HDB_MAX_FIELD_STRING],
	char retCdsEnd[HDB_MAX_FIELD_STRING],
	char retCount[HDB_MAX_FIELD_STRING],
	char retStarts[HDB_MAX_FIELD_STRING],
	char retEndsSizes[HDB_MAX_FIELD_STRING],
        char retSpan[HDB_MAX_FIELD_STRING], boolean *retBinned)
/* Given a table return the fields corresponding to all the bed 12 
 * fields, if they exist.  Fields that don't exist in the given table 
 * will be set to "". */
{
char query[256];
struct sqlConnection *conn = NULL;
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(5);
boolean gotIt = TRUE, binned = FALSE;

if (! hTableExistsDb(db, table))
    return FALSE;
conn = hAllocOrConnect(db);

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], "bin"))
        binned = TRUE;
    hashAdd(hash, row[0], NULL);
    }
sqlFreeResult(&sr);

/* Look for bed-style names. */
if (fitFields(hash, "chrom", "chromStart", "chromEnd", retChrom, retStart, retEnd))
    {
    if (!fitField(hash, "name", retName))
	if (!fitField(hash, "acc", retName))
	     fitField(hash, "frag", retName);
    fitField(hash, "score", retScore);
    fitField(hash, "strand", retStrand);
    fitField(hash, "thickStart", retCdsStart);
    fitField(hash, "thickEnd", retCdsEnd);
    fitField(hash, "blockCount", retCount);
    fitField(hash, "chromStarts", retStarts) ||
	fitField(hash, "blockStarts", retStarts);
    fitField(hash, "blockSizes", retEndsSizes);
    fitField(hash, "span", retSpan);
    }
/* Look for psl-style names. */
else if (fitFields(hash, "tName", "tStart", "tEnd", retChrom, retStart, retEnd))
    {
    fitField(hash, "qName", retName);
    fitField(hash, "strand", retStrand);
    retScore[0] = 0;
    retCdsStart[0] = 0;
    retCdsEnd[0] = 0;
    fitField(hash, "blockCount", retCount);
    fitField(hash, "tStarts", retStarts);
    fitField(hash, "blockSizes", retEndsSizes);
    retSpan[0] = 0;
    }
/* Look for gene prediction names. */
else if (fitFields(hash, "chrom", "txStart", "txEnd", retChrom, retStart, retEnd))
    {
    fitField(hash, "geneName", retName) ||  // tweak for refFlat type
	fitField(hash, "name", retName);
    fitField(hash, "score", retScore);      // some variants might have it...
    fitField(hash, "strand", retStrand);
    fitField(hash, "cdsStart", retCdsStart);
    fitField(hash, "cdsEnd", retCdsEnd);
    fitField(hash, "exonCount", retCount);
    fitField(hash, "exonStarts", retStarts);
    fitField(hash, "exonEnds", retEndsSizes);
    retSpan[0] = 0;
    }
/* Look for repeatMasker names. */
else if (fitFields(hash, "genoName", "genoStart", "genoEnd", retChrom, retStart, retEnd))
    {
    fitField(hash, "repName", retName);
    fitField(hash, "swScore", retScore);
    fitField(hash, "strand", retStrand);
    retCdsStart[0] = 0;
    retCdsEnd[0] = 0;
    retCount[0] = 0;
    retStarts[0] = 0;
    retEndsSizes[0] = 0;
    retSpan[0] = 0;
    }
else if (startsWith("chr", table) && endsWith(table, "_gl") && hashLookup(hash, "start") && hashLookup(hash, "end"))
    {
    strcpy(retChrom, "");
    strcpy(retStart, "start");
    strcpy(retEnd, "end");
    fitField(hash, "frag", retName);
    fitField(hash, "strand", retStrand);
    retScore[0] = 0;
    retCdsStart[0] = 0;
    retCdsEnd[0] = 0;
    retCount[0] = 0;
    retStarts[0] = 0;
    retEndsSizes[0] = 0;
    retSpan[0] = 0;
    }
else
    {
    if (hashLookup(hash, "acc"))
         strcpy(retName, "acc");
    else if (hashLookup(hash, "id"))
         strcpy(retName, "id");
    else if (hashLookup(hash, "name"))
         strcpy(retName, "name");
    gotIt = FALSE;
    }
freeHash(&hash);
hFreeOrDisconnect(&conn);
*retBinned = binned;
return gotIt;
}

boolean hFindFieldsAndBin(char *table, 
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING], char retEnd[HDB_MAX_FIELD_STRING],
	boolean *retBinned)
/* Given a table return the fields for selecting chromosome, start, end,
 * and whether it's binned . */
{
char retName[HDB_MAX_FIELD_STRING];
char retScore[HDB_MAX_FIELD_STRING];
char retStrand[HDB_MAX_FIELD_STRING];
char retCdsStart[HDB_MAX_FIELD_STRING];
char retCdsEnd[HDB_MAX_FIELD_STRING];
char retCount[HDB_MAX_FIELD_STRING];
char retStarts[HDB_MAX_FIELD_STRING];
char retEndsSizes[HDB_MAX_FIELD_STRING];
char retSpan[HDB_MAX_FIELD_STRING];
return hFindBed12FieldsAndBinDb(hGetDb(), table,
				retChrom, retStart, retEnd,
				retName, retScore, retStrand,
				retCdsStart, retCdsEnd,
				retCount, retStarts, retEndsSizes,
				retSpan, retBinned);
}

boolean hFindChromStartEndFields(char *table, 
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING], char retEnd[HDB_MAX_FIELD_STRING])
/* Given a table return the fields for selecting chromosome, start, and end. */
{
char retName[HDB_MAX_FIELD_STRING];
char retScore[HDB_MAX_FIELD_STRING];
char retStrand[HDB_MAX_FIELD_STRING];
char retCdsStart[HDB_MAX_FIELD_STRING];
char retCdsEnd[HDB_MAX_FIELD_STRING];
char retCount[HDB_MAX_FIELD_STRING];
char retStarts[HDB_MAX_FIELD_STRING];
char retEndsSizes[HDB_MAX_FIELD_STRING];
char retSpan[HDB_MAX_FIELD_STRING];
boolean isBinned;
return hFindBed12FieldsAndBinDb(hGetDb(), table,
				retChrom, retStart, retEnd,
				retName, retScore, retStrand,
				retCdsStart, retCdsEnd,
				retCount, retStarts, retEndsSizes,
				retSpan, &isBinned);
}


boolean hFindChromStartEndFieldsDb(char *db, char *table, 
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING], char retEnd[HDB_MAX_FIELD_STRING])
/* Given a table return the fields for selecting chromosome, start, and end. */
{
char retName[HDB_MAX_FIELD_STRING];
char retScore[HDB_MAX_FIELD_STRING];
char retStrand[HDB_MAX_FIELD_STRING];
char retCdsStart[HDB_MAX_FIELD_STRING];
char retCdsEnd[HDB_MAX_FIELD_STRING];
char retCount[HDB_MAX_FIELD_STRING];
char retStarts[HDB_MAX_FIELD_STRING];
char retEndsSizes[HDB_MAX_FIELD_STRING];
char retSpan[HDB_MAX_FIELD_STRING];
boolean isBinned;
return hFindBed12FieldsAndBinDb(db, table,
				retChrom, retStart, retEnd,
				retName, retScore, retStrand,
				retCdsStart, retCdsEnd,
				retCount, retStarts, retEndsSizes,
				retSpan, &isBinned);
}

int hdbChromSize(char *db, char *chromName)
/* Get chromosome size from given database . */
{
if (sameString(db, hGetDb()))
    return hChromSize(chromName);
else if ((hGetDb2() != NULL) && sameString(db, hGetDb2()))
    return hChromSize2(chromName);
else
    {
    warn("hdbChromSize not handling this case well");
    return 0;
    }
}

struct hTableInfo *hFindTableInfoDb(char *db, char *chrom, char *rootName)
/* Find table information.  Return NULL if no table.  */
{
static struct hash *dbHash = NULL;	/* Values are hashes of tables. */
struct hash *hash;
struct hTableInfo *hti;
char fullName[HDB_MAX_TABLE_STRING];
boolean isSplit = FALSE;

if (chrom == NULL)
    chrom = hDefaultChromDb(db);
if (dbHash == NULL)
    dbHash = newHash(8);
hash = hashFindVal(dbHash, db);
if (hash == NULL)
    {
    hash = newHash(8);
    hashAdd(dbHash, db, hash);
    }
if ((hti = hashFindVal(hash, rootName)) == NULL)
    {
    if (chrom != NULL)
	{
	safef(fullName, sizeof(fullName), "%s_%s", chrom, rootName);
	if (hTableExistsDb(db, fullName))
	    isSplit = TRUE;
	}
    if (!isSplit)
        {
	safef(fullName, sizeof(fullName), rootName);
	if (!hTableExistsDb(db, fullName))
	    return NULL;
	}
    AllocVar(hti);
    hashAddSaveName(hash, rootName, hti, &hti->rootName);
    hti->isSplit = isSplit;
    hti->isPos = hFindBed12FieldsAndBinDb(db, fullName,
	hti->chromField, hti->startField, hti->endField,
	hti->nameField, hti->scoreField, hti->strandField,
	hti->cdsStartField, hti->cdsEndField,
	hti->countField, hti->startsField, hti->endsSizesField,
	hti->spanField, &hti->hasBin);
    hti->hasCDS = (hti->cdsStartField[0] != 0);
    hti->hasBlocks = (hti->startsField[0] != 0);
    if (hti->isPos)
	{
	if (sameString(hti->startsField, "exonStarts"))
	    hti->type = cloneString("genePred");
	else if (sameString(hti->startsField, "chromStarts") ||
		 sameString(hti->startsField, "blockStarts"))
	    hti->type = cloneString("bed 12");
	else if (sameString(hti->startsField, "tStarts"))
	    hti->type = cloneString("psl");
	else if (hti->cdsStartField[0] != 0)
	    hti->type = cloneString("bed 8");
	else if (hti->strandField[0] !=0  &&  hti->chromField[0] == 0)
	    hti->type = cloneString("gl");
	else if (hti->strandField[0] !=0)
	    hti->type = cloneString("bed 6");
	else if (hti->spanField[0] !=0)
	    hti->type = cloneString("wiggle");
	else if (hti->nameField[0] !=0)
	    hti->type = cloneString("bed 4");
	else
	    hti->type = cloneString("bed 3");
	}
    else
	hti->type = NULL;
    }
return hti;
}

int hTableInfoBedFieldCount(struct hTableInfo *hti)
/* Return number of BED fields needed to save hti. */
{
if (hti->hasBlocks)
    return 12;
else if (hti->hasCDS)
    return 8;
else if (hti->strandField[0] != 0)
    return 6;
else if (hti->scoreField[0] != 0)
    return 5;
else if (hti->nameField[0] != 0)
    return 4;
else
    return 3;
}



struct hTableInfo *hFindTableInfo(char *chrom, char *rootName)
/* Find table information.  Return NULL if no table. */
{
return hFindTableInfoDb(hGetDb(), chrom, rootName);
}


boolean hFindSplitTableDb(char *db, char *chrom, char *rootName, 
	char retTableBuf[HDB_MAX_TABLE_STRING], boolean *hasBin)
/* Find name of table in a given database that may or may not 
 * be split across chromosomes. Return FALSE if table doesn't exist.  */
{
struct hTableInfo *hti = hFindTableInfoDb(db, chrom, rootName);
if (hti == NULL)
    return FALSE;
if (retTableBuf != NULL)
    {
    if (chrom == NULL)
	chrom = hDefaultChromDb(db);
    if (hti->isSplit)
	safef(retTableBuf, HDB_MAX_TABLE_STRING, "%s_%s", chrom, rootName);
    else
	safef(retTableBuf, HDB_MAX_TABLE_STRING, rootName);
    }
if (hasBin != NULL)
    *hasBin = hti->hasBin;
return TRUE;
}

boolean hFindSplitTable(char *chrom, char *rootName, 
	char retTableBuf[HDB_MAX_TABLE_STRING], boolean *hasBin)
/* Find name of table that may or may not be split across chromosomes. 
 * Return FALSE if table doesn't exist.  */
{
return hFindSplitTableDb(hGetDb(), chrom, rootName, retTableBuf, hasBin);
}

struct slName *hSplitTableNames(char *rootName)
/* Return a list of all split tables for rootName, or of just rootName if not 
 * split, or NULL if no such tables exist. */
{
struct hash *hash = NULL;
struct hashEl *hel = NULL;
char db[HDB_MAX_TABLE_STRING];
char justTable[HDB_MAX_TABLE_STRING];

parseDbTable(rootName, db, justTable);
hash = tableListHash(db);
hel = hashLookup(hash, justTable);
if (hel == NULL)
    return NULL;
else
    return slNameCloneList((struct slName *)(hel->val));
}

boolean hIsMgscHost()
/* Return TRUE if this is running on web server only
 * accessible to Mouse Genome Sequencing Consortium. */
{
static boolean gotIt = FALSE;
static boolean priv = FALSE;
if (!gotIt)
    {
    char *t = getenv("HTTP_HOST");
    if (t != NULL && (startsWith("hgwdev-mgsc", t)))
        priv = TRUE;
    gotIt = TRUE;
    }
return priv;
}

boolean hIsPrivateHost()
/* Return TRUE if this is running on private web-server. */
{
static boolean gotIt = FALSE;
static boolean priv = FALSE;
if (!gotIt)
    {
    char *t = getenv("HTTP_HOST");
    if (t != NULL && (startsWith("genome-test", t) || startsWith("hgwdev", t)))
        priv = TRUE;
    gotIt = TRUE;
    }
return priv;
}


int hOffsetPastBin(char *chrom, char *table)
/* Return offset into a row of table that skips past bin
 * field if any. */
{
struct hTableInfo *hti = hFindTableInfo(chrom, table);
if (hti == NULL)
    return 0;
return hti->hasBin;
}

/* Stuff to handle binning - which helps us restrict our
 * attention to the parts of database that contain info
 * about a particular window on a chromosome. This scheme
 * will work without modification for chromosome sizes up
 * to half a gigaBase.  The finest sized bin is 128k (1<<17).
 * The next coarsest is 8x as big (1<<3).  There's a hierarchy
 * of bins with the chromosome itself being the final bin.
 * Features are put in the finest bin they'll fit in. */

int hFindBin(int start, int end)
/* Given start,end in chromosome coordinates assign it
 * a bin.   There's a bin for each 128k segment, for each
 * 1M segment, for each 8M segment, for each 64M segment,
 * and for each chromosome (which is assumed to be less than
 * 512M.)  A range goes into the smallest bin it will fit in. */
{
return binFromRange(start, end);
}

void hAddBinToQueryGeneral(char *binField, int start, int end, 
	struct dyString *query)
/* Add clause that will restrict to relevant bins to query. */
{
int bFirstShift = binFirstShift(), bNextShift = binNextShift();
int startBin = (start>>bFirstShift), endBin = ((end-1)>>bFirstShift);
int i, levels = binLevels();

dyStringAppend(query, "(");
for (i=0; i<levels; ++i)
    {
    int offset = binOffset(i);
    if (i != 0)
        dyStringAppend(query, " or ");
    if (startBin == endBin)
        dyStringPrintf(query, "%s=%u", binField, startBin + offset);
    else
        dyStringPrintf(query, "%s>=%u and %s<=%u", 
		binField, startBin + offset, binField, endBin + offset);
    startBin >>= bNextShift;
    endBin >>= bNextShift;
    }
dyStringAppend(query, ")");
dyStringAppend(query, " and ");
}

void hAddBinToQuery(int start, int end, struct dyString *query)
/* Add clause that will restrict to relevant bins to query. */
{
hAddBinToQueryGeneral("bin", start, end, query);
}

struct sqlResult *hExtendedRangeQuery(
	struct sqlConnection *conn,  /* Open SQL connection. */
	char *rootTable, 	     /* Table (not including any chrN_) */
	char *chrom, int start, int end,  /* Range. */
	char *extraWhere,            /* Extra things to add to where clause. */
	boolean order, 	   /* If true order by start position (can be slow). */
	char *fields,      /* If non-NULL comma separated field list. */
	int *retRowOffset) /* Returns offset past bin field. */
/* Range query with lots of options. */
{
char *db = sqlGetDatabase(conn);
struct hTableInfo *hti = hFindTableInfoDb(db, chrom, rootTable);
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(1024);
char *table = NULL;
int rowOffset = 0;

if (fields == NULL) fields = "*";
if (hti == NULL)
    {
    warn("table %s doesn't exist", rootTable);
    }
else
    {
    dyStringPrintf(query, "select %s from ", fields);
    if (hti->isSplit)
	{
	char fullTable[HDB_MAX_TABLE_STRING];
	safef(fullTable, sizeof(fullTable), "%s_%s", chrom, rootTable);
	if (!hTableExistsDb(db, fullTable))
	     warn("%s doesn't exist", fullTable);
	else
	    {
	    table = fullTable;
	    dyStringPrintf(query, "%s where ", table);
	    }
	}
    else
        {
	table = rootTable;
	dyStringPrintf(query, "%s where %s='%s' and ", 
	    table, hti->chromField, chrom);
	}
    }
if (table != NULL)
    {
    if (hti->hasBin)
        {
	hAddBinToQuery(start, end, query);
	rowOffset = 1;
	}
    dyStringPrintf(query, "%s<%u and %s>%u", 
    	hti->startField, end, hti->endField, start);
    if (extraWhere)
        {
        /* allow more flexible additions to where clause */
        if (!startsWith("order", extraWhere) && 
            !startsWith("limit", extraWhere))
                dyStringAppend(query, " and ");
        dyStringPrintf(query, " %s", extraWhere);
        }
    if (order)
        dyStringPrintf(query, " order by %s", hti->startField);
    sr = sqlGetResult(conn, query->string);
    }
freeDyString(&query);
if (retRowOffset != NULL)
    *retRowOffset = rowOffset;
return sr;
}


struct sqlResult *hRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset)
/* Construct and make a query to tables that may be split and/or
 * binned. */
{
return hExtendedRangeQuery(conn, rootTable, chrom, start, end, 
	extraWhere, FALSE, NULL, retRowOffset);
}

struct sqlResult *hOrderedRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset)
/* Construct and make a query to tables that may be split and/or
 * binned. Forces return values to be sorted by chromosome start. */
{
return hExtendedRangeQuery(conn, rootTable, chrom, start, end, 
	extraWhere, TRUE, NULL, retRowOffset);
}

struct sqlResult *hExtendedChromQuery(
	struct sqlConnection *conn,  /* Open SQL connection. */
	char *rootTable, 	     /* Table (not including any chrN_) */
	char *chrom,  		     /* Chromosome. */
	char *extraWhere,            /* Extra things to add to where clause. */
	boolean order, 	   /* If true order by start position (can be slow). */
	char *fields,      /* If non-NULL comma separated field list. */
	int *retRowOffset) /* Returns offset past bin field. */
/* Chromosome query fields for tables that may be split and/or binned, 
 * with lots of options. */
{
char *db = sqlGetDatabase(conn);
struct hTableInfo *hti = hFindTableInfoDb(db, chrom, rootTable);
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(1024);
int rowOffset = 0;

if (fields == NULL) fields = "*";
if (hti == NULL)
    {
    warn("table %s doesn't exist", rootTable);
    }
else
    {
    rowOffset = hti->hasBin;
    if (hti->isSplit)
	{
        dyStringPrintf(query, "select %s from %s_%s", fields, chrom, rootTable);
	if (extraWhere != NULL)
	    dyStringPrintf(query, " where %s", extraWhere);
	}
    else
	{
        dyStringPrintf(query, "select %s from %s where %s='%s'", 
		fields, rootTable, hti->chromField, chrom);
	if (extraWhere != NULL)
	    dyStringPrintf(query, " and (%s)", extraWhere);
	}
    if (order)
        dyStringPrintf(query, " order by %s", hti->startField);
    sr = sqlGetResult(conn, query->string);
    }
freeDyString(&query);
if (retRowOffset != NULL)
    *retRowOffset = rowOffset;
return sr;
}

struct sqlResult *hChromQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	char *extraWhere, int *retRowOffset)
/* Construct and make a query across whole chromosome to tables 
 * that may be split and/or * binned. */
{
return hExtendedChromQuery(conn, rootTable, chrom, extraWhere, 
	FALSE, NULL, retRowOffset);
}

boolean hTrackOnChrom(struct trackDb *tdb, char *chrom)
/* Return TRUE if track exists on this chromosome. */
{
boolean chromOk = TRUE;
if (tdb->restrictCount > 0 && chrom != NULL)
    chromOk =  (stringArrayIx(chrom, tdb->restrictList, tdb->restrictCount)) >= 0;
return chromOk;
}

static struct trackDb* loadTrackDb(struct sqlConnection *conn, char* where)
/* load list of trackDb objects, with optional where */
{
char *trackDb = hTrackDbName();
struct trackDb *tdbList = trackDbLoadWhere(conn, trackDb, where);
freez(&trackDb);
return tdbList;
}

static struct trackDb* loadTrackDbLocal(struct sqlConnection *conn, char* where)
/* load list of trackDbLocal objects, with optional where */
{
char *trackDbLocal = hTrackDbLocalName();
struct trackDb *tdbList = NULL;
if ((trackDbLocal != NULL) && sqlTableExists(conn, trackDbLocal))
    tdbList = trackDbLoadWhere(conn, trackDbLocal, where);
freez(&trackDbLocal);
return tdbList;
}

static struct trackDb* findTrackDb(struct trackDb** tdbList, char *table)
/* search a list of trackDb objects for a object associated with a particular
 * track, and remove from list.  Return NULL if not found  */
{
struct trackDb *tdb, *prevTdb = NULL;
for (tdb = *tdbList; tdb != NULL; prevTdb = tdb, tdb = tdb->next)
    {
    if (sameString(tdb->tableName, table))
        {
        if (prevTdb == NULL)
            *tdbList = tdb->next;
        else
            prevTdb->next = tdb->next;
        return tdb;
        }
    }
return NULL;
}

static void processTrackDb(char *database, struct trackDb *tdb, char *chrom,
                           boolean privateHost, struct trackDb **tdbRetList)
/* check if a trackDb entry should be included in display, and if so
 * add it to the list, otherwise free it */
{
char splitTable[HDB_MAX_TABLE_STRING];
hLookupStringsInTdb(tdb, database);
if ((!tdb->private || privateHost) &&
    hFindSplitTable(chrom, tdb->tableName, splitTable, NULL) 
#ifdef NEEDED_UNTIL_GB_CDNA_INFO_CHANGE
	&& !sameString(splitTable, "mrna") /* Long ago we reused this name badly. */
#endif /* NEEDED_UNTIL_GB_CDNA_INFO_CHANGE */
    )

    slAddHead(tdbRetList, tdb);
else
    trackDbFree(&tdb);
}

struct trackDb *hTrackDb(char *chrom)
/* Load tracks associated with current chromosome (which may be NULL for
 * all). If trackDbLocal exists, then it's row either override or are added to
 * the standard trackDb. */
{
struct sqlConnection *conn = hAllocConn();
struct trackDb *tdbList = loadTrackDb(conn, NULL);
struct trackDb *tdbLocalList = loadTrackDbLocal(conn, NULL);
struct trackDb *tdbFullList = NULL, *tdbSubtrackedList = NULL;
struct trackDb *tdbRetList = NULL;
char *database = hGetDb();
boolean privateHost = hIsPrivateHost();
struct hash *compositeHash = newHash(0);
struct trackDb *tdb, *compositeTdb;
struct trackDb *nextTdb;

while (tdbList != NULL)
    {
    struct trackDb *tdbLoc;
    tdb = slPopHead(&tdbList);
    tdbLoc = findTrackDb(&tdbLocalList, tdb->tableName);
    if (tdbLoc != NULL)
        {
        /* use local */
        trackDbFree(&tdb);
        tdb = tdbLoc;
        }
    if (trackDbSetting(tdb, "compositeTrack"))
        {
        slAddHead(&tdbFullList, tdb);
        hashAdd(compositeHash, tdb->tableName, tdb);
        }
    else
        processTrackDb(database, tdb, chrom, privateHost, &tdbFullList);
    }

/* add remaing local trackDbs */
while (tdbLocalList != NULL)
    {
    tdb = slPopHead(&tdbLocalList);
    if (trackDbSetting(tdb, "compositeTrack"))
        {
        hashAdd(compositeHash, tdb->tableName, tdb);
        slAddHead(&tdbRetList, tdb);
        }
    else
        processTrackDb(database, tdb, chrom, privateHost, &tdbFullList);
    }

/* create new list with subtrack entries in subtracks field of composite track*/
nextTdb = tdbFullList;
for (tdb = tdbFullList; nextTdb != NULL; tdb = nextTdb)
    {
    char *words[1];
    char *setting;

    nextTdb = tdb->next;
    if ((setting = trackDbSetting(tdb, "subTrack")) != NULL)
        {
        if (chopLine(cloneString(setting), words) >= 1)
            {
            compositeTdb = 
                (struct trackDb *)hashFindVal(compositeHash, words[0]);
            if (compositeTdb)
                {
                /* should be a short list -- we can shortcut and add to tail
                 * rather than reversing later */
                tdb->type = cloneString(compositeTdb->type);
                slAddTail(&compositeTdb->subtracks, tdb);
                }
            }
        }
    else
        slAddHead(&tdbSubtrackedList, tdb);
    }
/* Prune composite tracks that have empty subtracks lists because their 
 * tables do not exist in the database. */
slReverse(&tdbSubtrackedList);
for (nextTdb = tdb = tdbSubtrackedList; nextTdb != NULL; tdb = nextTdb)
    {
    nextTdb = tdb->next;
    if (! (trackDbSetting(tdb, "compositeTrack") && tdb->subtracks == NULL))
        {
	slAddHead(&tdbRetList, tdb);
	}
    }
hFreeConn(&conn);
return tdbRetList;
}

static struct trackDb *loadTrackDbForTrack(struct sqlConnection *conn, char *track)
/* Load trackDb object for a track. If trackDbLocal exists, then it's row is
 * used if it exists. this is common code for two external functions. */
{
struct trackDb *tdb, *nextTdb, *tdbList, *compositeTdb = NULL;
char where[256];

safef(where, sizeof(where), "tableName = '%s' or settings like '%%subTrack %s%%'", track, track);

tdbList = loadTrackDbLocal(conn, where);
if (tdbList == NULL)
    tdbList = loadTrackDb(conn, where);
if (tdbList != NULL)
    hLookupStringsInTdb(tdbList, hGetDb());

/* create new entry with subtrack entries in subtracks field 
 * of composite track*/
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (trackDbSetting(tdb, "compositeTrack"))
        {
        compositeTdb = tdb;
        break;
        }
    }
if (compositeTdb)
    {
    nextTdb = tdbList;
    for (tdb = tdbList; nextTdb != NULL; tdb = nextTdb)
        {
        nextTdb = tdb->next;
        if (trackDbSetting(tdb, "subTrack"))
            {
            slAddHead(&compositeTdb->subtracks, tdb);
            tdb->type = cloneString(compositeTdb->type);
            }
        }
    return compositeTdb;
    }
else
    return tdbList;
}

struct trackDb *hTrackDbForTrack(char *track)
/* Load trackDb object for a track. If trackDbLocal exists, then it's row is
 * used if it exists. */
{
struct sqlConnection *conn = hAllocConn();
struct trackDb *tdb = loadTrackDbForTrack(conn, track);
hFreeConn(&conn);
return tdb;
}

boolean hgParseChromRangeDb(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd, boolean haveDb)
/* Parse something of form chrom:start-end into pieces. 
 * if haveDb then check with chromInfo for names */
{
char *chrom, *start, *end;
char buf[256];
int iStart, iEnd;

strncpy(buf, spec, 256);
stripChar(buf, ',');
chrom = buf;
start = strchr(chrom, ':');

if (start == NULL)
    {
    /* If just chromosome name cover all of it. */
    if (!haveDb || ((chrom = hgOfficialChromName(chrom)) == NULL))
	return FALSE;
    else
       {
       iStart = 0;
       iEnd = hChromSize(chrom);
       }
    }
else 
    {
    *start++ = 0;
    end = strchr(start, '-');
    if (end == NULL)
	return FALSE;
    else
    *end++ = 0;
    chrom = trimSpaces(chrom);
    start = trimSpaces(start);
    end = trimSpaces(end);
    if (!isdigit(start[0]))
	return FALSE;
    if (!isdigit(end[0]))
	return FALSE;
    if (haveDb && ((chrom = hgOfficialChromName(chrom)) == NULL))
	return FALSE;
    iStart = atoi(start)-1;
    iEnd = atoi(end);
    }
if (retChromName != NULL)
    *retChromName = (haveDb)? chrom : cloneString(chrom);
if (retWinStart != NULL)
    *retWinStart = iStart;
if (retWinEnd != NULL)
    *retWinEnd = iEnd;
return TRUE;
}

boolean hgParseChromRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Parse something of form chrom:start-end into pieces. */
{
return hgParseChromRangeDb(spec, retChromName, retWinStart, retWinEnd, TRUE);
}


boolean hgIsChromRange(char *spec)
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */
{
return hgParseChromRange(spec, NULL, NULL, NULL);
}

#ifdef UNUSED
boolean hgParseContigRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Parse something of form contig:start-end into pieces. */
{
char *contig, *start,*end;
char buf[256];
char contigName[256];
int iStart, iEnd;
size_t colinspot;
char *chrom;
int contigstart,contigend;
int spot;

strncpy(buf, spec, 256);
contig = buf;
start = strchr(contig, ':');
if (start == NULL)
    return FALSE;
if (startsWith("ctg",contig) == FALSE && startsWith("NT_", contig) == FALSE)
    return FALSE;
spot = strcspn(contig,":");
strncpy(contigName,contig,spot);
contigName[spot] = '\0';
*start++ = 0;
end = strchr(start, '-');
if (end == NULL)
    return FALSE;
else
*end++ = 0;
contig = trimSpaces(contig);
start = trimSpaces(start);
end = trimSpaces(end);
if (!isdigit(start[0]))
    return FALSE;
if (!isdigit(end[0]))
    return FALSE;
iStart = atoi(start)-1;
iEnd = atoi(end);
if (retChromName != NULL)
    *retChromName = chrom;
if (retWinStart != NULL)
    *retWinStart = contigstart + iStart;
if (retWinEnd != NULL)
    *retWinEnd = contigstart + iEnd;
return TRUE; 
}

boolean hgIsContigRange(char *spec)
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */
{
return hgParseContigRange(spec, NULL, NULL, NULL);
}  
#endif /* UNUSED */

struct trackDb *hMaybeTrackInfo(struct sqlConnection *conn, char *trackName)
/* Look up track in database, return NULL if it's not there. */
{
if (sqlTableExists(conn, hTrackDbName()))
    return loadTrackDbForTrack(conn, trackName);
else
    return NULL;
}

struct trackDb *hTrackInfo(struct sqlConnection *conn, char *trackName)
/* Look up track in database, errAbort if it's not there. */
{
struct trackDb *tdb;

tdb = hMaybeTrackInfo(conn, trackName);
if (tdb == NULL)
    errAbort("Track %s not found", trackName);
return tdb;
}


boolean hTrackCanPack(char *trackName)
/* Return TRUE if this track can be packed. */
{
struct sqlConnection *conn = hAllocConn();
struct trackDb *tdb = hMaybeTrackInfo(conn, trackName);
boolean ret = FALSE;
if (tdb != NULL)
    {
    ret = tdb->canPack;
    trackDbFree(&tdb);
    }
hFreeConn(&conn);
return ret;
}

char *hTrackOpenVis(char *trackName)
/* Return "pack" if track is packable, otherwise "full". */
{
return hTrackCanPack(trackName) ? "pack" : "full";
}

static struct dbDb *hGetIndexedDbsMaybeClade(char *theDb)
/* Get list of active databases, in theDb's clade if theDb is not NULL.
 * Dispose of this with dbDbFreeList. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row;
struct dbDb *dbList = NULL, *db;
char *theClade = theDb ? hClade(hGenome(theDb)) : NULL;

/* Scan through dbDb table, loading into list */
sr = sqlGetResult(conn,
	   "select * from dbDb where active = 1 order by orderKey,name desc");
while ((row = sqlNextRow(sr)) != NULL)
    {
    db = dbDbLoad(row);
    if (theClade == NULL ||
	sameString(hClade(hGenome(db->name)), theClade))
	slAddHead(&dbList, db);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
slReverse(&dbList);
return dbList;
}

struct dbDb *hGetIndexedDatabases()
/* Get list of all active databases. 
 * Dispose of this with dbDbFreeList. */
{
return hGetIndexedDbsMaybeClade(NULL);
}

struct dbDb *hGetIndexedDatabasesForClade(char *db)
/* Get list of active databases in db's clade.
 * Dispose of this with dbDbFreeList. */
{
return hGetIndexedDbsMaybeClade(db);
}

struct slName *hLiftOverFromDbs() 
/* Return a list of names of the DBs in the 
 * fromDb column of the liftOverChain.*/
{
struct slName *names = NULL;
struct liftOverChain *chainList = liftOverChainList(), *chain;
for (chain = chainList; chain != NULL; chain = chain->next)
    slNameStore(&names, chain->fromDb);
liftOverChainFreeList(&chainList);
return names;
}

struct slName *hLiftOverToDbs(char *fromDb) 
/* Return a list of names of the DBs in the 
 * toDb column of the liftOverChain.
 * If fromDb!=NULL, return only those with that
 * fromDb. */
{
struct slName *names = NULL;
struct liftOverChain *chainList = liftOverChainList(), *chain;
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    if (!fromDb || (fromDb && sameString(fromDb,chain->fromDb)))
	slNameStore(&names,chain->toDb);
    }
liftOverChainFreeList(&chainList);
return names;
}

struct slName *hLiftOverOrgs(boolean from, char *fromDb)
/* Just a function hLiftOverFromOrgs and
 * hLiftOverToOrgs call. */
{
struct slName *dbs = (from) ? hLiftOverFromDbs() : hLiftOverToDbs(fromDb);
struct slName *names = NULL, *org;
for (org = dbs; org != NULL; org = org->next)
    slNameStore(&names, hArchiveOrganism(org->name));
slReverse(&names);
slFreeList(&dbs);
return names;
}

struct slName *hLiftOverFromOrgs()
/* Return a list of names of organisms that 
 * have databases in the fromDb column of
 * liftOverChain.*/
{
return hLiftOverOrgs(TRUE,NULL);
}

struct slName *hLiftOverToOrgs(char *fromDb)
/* Return a list of names of the organisms with
 * databases in the toDb column of the liftOverChain.
 * If fromDb!=NULL, return only those with that
 * fromDb. */
{
return hLiftOverOrgs(FALSE,fromDb);
}

struct dbDb *hGetLiftOverFromDatabases()
/* Get list of databases for which there is at least one liftOver chain file
 * from this assembly to another.
 * Dispose of this with dbDbFreeList. */
{
struct dbDb *allDbList = NULL;
struct dbDb *liftOverDbList = NULL, *dbDb, *nextDbDb;
struct liftOverChain *chainList = NULL, *chain;
struct hash *hash = newHash(0), *dbNameHash = newHash(3);

/* Get list of all liftOver chains in central database */
chainList = liftOverChainList();

/* Create hash of databases having liftOver chains from this database */
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    if (!hashFindVal(hash, chain->fromDb))
        hashAdd(hash, chain->fromDb, chain->fromDb);
    }

/* Get list of all current and archived databases */
allDbList = slCat(hDbDbList(),hArchiveDbDbList());

/* Create a new dbDb list of all entries in the liftOver hash */
for (dbDb = allDbList; dbDb != NULL; dbDb = nextDbDb)
    {
    /* current dbDb entries */
    nextDbDb = dbDb->next;
    if (hashFindVal(hash, dbDb->name) && !hashFindVal(dbNameHash, dbDb->name))
	{
        slAddHead(&liftOverDbList, dbDb);
	hashAdd(dbNameHash, dbDb->name, dbDb->name);
	}
    else
        dbDbFree(&dbDb);
    }

hashFree(&hash);
hashFree(&dbNameHash);
liftOverChainFreeList(&chainList);
slReverse(&liftOverDbList);
return liftOverDbList;
}

struct dbDb *hGetLiftOverToDatabases(char *fromDb)
/* Get list of databases for which there are liftOver chain files 
 * to convert from the fromDb assembly.
 * Dispose of this with dbDbFreeList. */
{
struct dbDb *allDbList = NULL, *liftOverDbList = NULL, *dbDb, *nextDbDb;
struct liftOverChain *chainList = NULL, *chain;
struct hash *hash = newHash(0);
struct hash *dbNameHash = newHash(3);

/* Get list of all liftOver chains in central database */
chainList = liftOverChainList();

/* Create hash of databases having liftOver chains from the fromDb */
for (chain = chainList; chain != NULL; chain = chain->next)
    if (sameString(chain->fromDb,fromDb))
	hashAdd(hash, chain->toDb, chain->toDb);

/* Get list of all current databases */
allDbList = slCat(hDbDbList(),hArchiveDbDbList());

/* Create a new dbDb list of all entries in the liftOver hash */
for (dbDb = allDbList; dbDb != NULL; dbDb = nextDbDb)
    {
    nextDbDb = dbDb->next;
    if (hashFindVal(hash, dbDb->name) && !hashFindVal(dbNameHash, dbDb->name))
	{	
        slAddHead(&liftOverDbList, dbDb);
	/* to avoid duplicates in the returned list. */
	hashAdd(dbNameHash, dbDb->name, dbDb->name);
	}
    else
        dbDbFree(&dbDb);
    }
hashFree(&hash);
liftOverChainFreeList(&chainList);
slReverse(&liftOverDbList);
return liftOverDbList;
}

struct dbDb *hGetAxtInfoDbs()
/* Get list of db's where we have axt files listed in axtInfo . 
 * The db's with the same organism as current db go last.
 * Dispose of result with dbDbFreeList. */
{
struct dbDb *dbDbList = NULL, *dbDb;
struct hash *hash = hashNew(7); // 2^^7 entries = 128
struct slName *dbNames = NULL, *dbName;
struct dyString *query = newDyString(256);
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
char *organism = hOrganism(hdbName);
int count;

if (! hTableExists("axtInfo"))
    {
    dyStringFree(&query);
    hashFree(&hash);
    hFreeConn(&conn);
    return NULL;
    }

/* "species" is a misnomer, we're really looking up database names. */
sr = sqlGetResult(conn, "select species from axtInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    // uniquify database names and make sure the databases still exist
    if ((hashLookup(hash, row[0]) == NULL) && hDbExists(row[0]))
	{
	struct slName *sln = newSlName(cloneString(row[0]));
	slAddHead(&dbNames, sln);
	hashStoreName(hash, cloneString(row[0]));
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

/* Traverse the uniquified list of databases twice: first for db's with 
 * a different organism, then for db's with this organism. */
conn = hConnectCentral();
dyStringClear(query);
dyStringAppend(query, "SELECT * from dbDb");
count = 0;
for (dbName = dbNames;  dbName != NULL;  dbName = dbName->next)
    {
    char *dbOrg = hOrganism(dbName->name);
    if (! sameString(dbOrg, organism))
	{
	count++;
	if (count == 1)
	    dyStringPrintf(query, " where active = 1 and (name = '%s'",
			   dbName->name);
	else
	    dyStringPrintf(query, " or name = '%s'", dbName->name);
	}
    }
dyStringPrintf(query, ") order by orderKey desc");
if (count > 0)
    {
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	dbDb = dbDbLoad(row);
	slAddHead(&dbDbList, dbDb);
	}
    sqlFreeResult(&sr);
    }
dyStringClear(query);
dyStringAppend(query, "SELECT * from dbDb");
count = 0;
for (dbName = dbNames;  dbName != NULL;  dbName = dbName->next)
    {
    char *dbOrg = hOrganism(dbName->name);
    if (sameString(dbOrg, organism))
	{
	count++;
	if (count == 1)
	    dyStringPrintf(query, " where active = 1 and (name = '%s'",
			   dbName->name);
	else
	    dyStringPrintf(query, " or name = '%s'", dbName->name);
	}
    }
dyStringPrintf(query, ") order by orderKey, name desc");
if (count > 0)
    {
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	dbDb = dbDbLoad(row);
	slAddHead(&dbDbList, dbDb);
	}
    sqlFreeResult(&sr);
    }
hDisconnectCentral(&conn);
slFreeList(&dbNames);
dyStringFree(&query);
hashFree(&hash);

slReverse(&dbDbList);
return(dbDbList);
}

struct axtInfo *hGetAxtAlignments(char *otherDb)
/* Get list of alignments where we have axt files listed in axtInfo . 
 * Dispose of this with axtInfoFreeList. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct axtInfo *aiList = NULL, *ai;
char query[256];

sprintf(query, "select * from axtInfo where species = '%s' and chrom = '%s' order by sort",
	otherDb, hDefaultChrom());
/* Scan through axtInfo table, loading into list */
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ai = axtInfoLoad(row);
    slAddHead(&aiList, ai);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&aiList);
return aiList;
}

struct axtInfo *hGetAxtAlignmentsChrom(char *otherDb, char *chrom)
/* Get list of alignments where we have axt files listed in axtInfo for a specified chromosome . 
 * Dispose of this with axtInfoFreeList. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct axtInfo *aiList = NULL, *ai;
char query[256];

sprintf(query, "select * from axtInfo where species = '%s' and chrom = '%s'",
	otherDb, chrom);
/* Scan through axtInfo table, loading into list */
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ai = axtInfoLoad(row);
    slAddHead(&aiList, ai);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&aiList);
return aiList;
}
struct dbDb *hGetBlatIndexedDatabases()
/* Get list of databases for which there is a BLAT index. 
 * Dispose of this with dbDbFreeList. */
{
struct hash *hash=newHash(5);
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
struct dbDb *dbList = NULL, *db;

/* Get hash of active blat servers. */
sr = sqlGetResult(conn, "select db from blatServers");
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], NULL);
sqlFreeResult(&sr);

/* Scan through dbDb table, keeping ones that are indexed. */
sr = sqlGetResult(conn, "select * from dbDb order by orderKey,name desc");
while ((row = sqlNextRow(sr)) != NULL)
    {
    db = dbDbLoad(row);
    if (hashLookup(hash, db->name))
        {
	slAddHead(&dbList, db);
	}
    else
        dbDbFree(&db);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
hashFree(&hash);
slReverse(&dbList);
return dbList;
}

boolean hIsBlatIndexedDatabase(char *db)
/* Return TRUE if have a BLAT server on sequence corresponding 
 * to give database. */
{
struct sqlConnection *conn = hConnectCentral();
boolean gotIx;
char query[256];

sprintf(query, "select name from dbDb where name = '%s'", db);
gotIx = sqlExists(conn, query);
hDisconnectCentral(&conn);
return gotIx;
}

struct blatServerTable *hFindBlatServer(char *db, boolean isTrans)
/* Return server for given database.  Db can either be
 * database name or description. Ponter returned is owned
 * by this function and shouldn't be modified */
{
static struct blatServerTable st;
struct sqlConnection *conn = hConnectCentral();
char query[256];
struct sqlResult *sr;
char **row;
char dbActualName[32];

/* If necessary convert database description to name. */
sprintf(query, "select name from dbDb where name = '%s'", db);
if (!sqlExists(conn, query))
    {
    sprintf(query, "select name from dbDb where description = '%s'", db);
    if (sqlQuickQuery(conn, query, dbActualName, sizeof(dbActualName)) != NULL)
        db = dbActualName;
    }

/* Do a little join to get data to fit into the blatServerTable. */
sprintf(query, "select dbDb.name,dbDb.description,blatServers.isTrans"
               ",blatServers.host,blatServers.port,dbDb.nibPath "
	       "from dbDb,blatServers where blatServers.isTrans = %d and "
	       "dbDb.name = '%s' and dbDb.name = blatServers.db", 
	       isTrans, db);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    errAbort("Can't find a server for %s database %s\n",
	    (isTrans ? "translated" : "DNA"), db);
    }
st.db = cloneString(row[0]);
st.genome = cloneString(row[1]);
st.isTrans = atoi(row[2]);
st.host = cloneString(row[3]);
st.port = cloneString(row[4]);
st.nibDir = cloneString(row[5]);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return &st;
}

char *sqlGetField(struct sqlConnection *connIn, 
   	          char *dbName, char *tblName, char *fldName, 
  	          char *condition)
/* get a single field from the database, given database name, 
 * table name, field name, and a condition string */
{
struct sqlConnection *conn;
char query[256];
struct sqlResult *sr;
char **row;
char *answer;

// allocate connection if given NULL
if (connIn == NULL)
    {
    conn = hAllocConn();
    }
else
    {
    conn = connIn;
    }

answer = NULL;
sprintf(query, "select %s from %s.%s  where %s;",
	fldName, dbName, tblName, condition);
//printf("<br>%s\n", query); fflush(stdout);
sr  = sqlGetResult(conn, query);
row = sqlNextRow(sr);
	    
if (row != NULL)
    {
    answer = strdup(row[0]);
    }

sqlFreeResult(&sr);
if (connIn == NULL) hFreeConn(&conn);
		    
return(answer);
}

struct hash *hChromSizeHash(char *db)
/* Get hash of chromosome sizes for database.  Just hashFree it when done. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(0);

sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    hashAddInt(hash, row[0], sqlUnsigned(row[1]));
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return hash;
}

char *hgDirForOrg(char *org)
/* Make directory name from organism name - getting
 * rid of dots and spaces. */
{
org = cloneString(org);
stripChar(org, '.');
subChar(org, ' ', '_');
return org;
}

struct hash *hgReadRa(char *genome, char *database, char *rootDir, 
	char *rootName, struct hash **retHashOfHash)
/* Read in ra in root, root/org, and root/org/database. 
 * Returns a list of hashes, one for each ra record.  Optionally
 * if retHashOfHash is non-null it returns there a
 * a hash of hashes keyed by the name field in each
 * ra sub-hash. */
{
struct hash *hashOfHash = newHash(10);
char *org = hgDirForOrg(genome);
char fileName[HDB_MAX_PATH_STRING];
struct hashEl *helList, *hel;
struct hash *raList = NULL, *ra;

/* Create hash of hash. */
safef(fileName, sizeof(fileName), "%s/%s", rootDir, rootName);
raFoldIn(fileName, hashOfHash);
safef(fileName, sizeof(fileName), "%s/%s/%s", rootDir, org, rootName);
raFoldIn(fileName, hashOfHash);
safef(fileName, sizeof(fileName), "%s/%s/%s/%s", rootDir, org, database, rootName);
raFoldIn(fileName, hashOfHash);
freez(&org);

/* Create list. */
helList = hashElListHash(hashOfHash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    ra = hel->val;
    slAddHead(&raList, ra);
    }

if (retHashOfHash)
    *retHashOfHash = hashOfHash;
else 
    hashFree(&hashOfHash);

return raList;
}

char *addCommasToPos(char *position)
/* add commas to the numbers in a position 
 * returns pointer to static */
{
static char buffer[256];
int winStart, winEnd;
char *chromName;
char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */

if (position == NULL)
    return NULL;

buffer[sizeof(buffer) - 1] = 0;
if (!hgParseChromRangeDb(position, &chromName, &winStart, &winEnd, FALSE))
    strncpy(buffer, position, sizeof(buffer) - 1);
else
    {
    sprintLongWithCommas(num1Buf, winStart + 1);
    sprintLongWithCommas(num2Buf, winEnd);
    safef(buffer, sizeof(buffer) - 1, "%s:%s-%s",chromName, num1Buf,  num2Buf);
    }

return buffer;
}

static struct grp* loadGrp(struct sqlConnection *conn, char *confName, char *defaultTbl)
/* load all of the grp rows from a table.  The table name is first looked up
 * in hg.conf with confName. If not there, use defaultTbl.  If the table
 * doesn't exist, return NULL */
{
char query[128];
struct grp *grps = NULL;
char *tbl = cfgOption(confName);
if (tbl == NULL)
    tbl = defaultTbl;
if (sqlTableExists(conn, tbl))
    {
    safef(query, sizeof(query), "select * from %s", tbl);
    grps = grpLoadByQuery(conn, query);
    }
return grps;
}

static struct grp* findGrp(struct grp** grpList, char* name)
/* search a list of grp objects for a object of the particular name and remove
 * from list.  Return NULL if not found  */
{
struct grp *grp, *prevGrp = NULL;
for (grp = *grpList; grp != NULL; prevGrp = grp, grp = grp->next)
    {
    if (sameString(grp->name, name))
        {
        if (prevGrp == NULL)
            *grpList = grp->next;
        else
            prevGrp->next = grp->next;
        return grp;
        }
    }
return NULL;
}

struct grp* hLoadGrps()
/* load the grp and optional grpLocal tables from the databases.  If grpLocal
 * exists, then entries in this table will override or supplement the grp
 * table.  The names of these tables can be configured in the hg.conf file
 * with db.grp and db.grpLocal variables.  List will be returned sorted by
 * priority. */
{
struct sqlConnection *conn = hAllocConn();
struct grp *grpList = loadGrp(conn, "db.grp", "grp");
struct grp *grpLocalList = loadGrp(conn, "db.grpLocal", "grpLocal");
struct grp *grps = NULL;

/* check each object from grp table to see if it's grpLocal overrides it */
while (grpList != NULL)
    {
    struct grp *grp = slPopHead(&grpList);
    struct grp *grpLocal = findGrp(&grpLocalList, grp->name);
    if (grpLocal != NULL)
        {
        grpFree(&grp);
        grp = grpLocal;
        }
    slAddHead(&grps, grp);
    }

/* add remainder of grpLocal */
grps = slCat(grps, grpLocalList);

slSort(&grps, grpCmpPriority);
hFreeConn(&conn);
return grps;
}


int chrStrippedCmp(char *chrA, char *chrB)
/*	compare chrom names after stripping chr, Scaffold_ or ps_ prefix
 *	database ci1 has the Scaffold_ prefix, cioSav1 has the ps_
 *	prefix, dp2 has an unusual ContigN_ContigN pattern
 *	all the rest are prefixed chr
 *	This can be used in sort compare functions to order the chroms
 *	by number  (the _random's come out conveniently after everything
 *	else)
 */
{
int dif;
int lenA = 0;
int lenB = 0;

if (startsWith("chr", chrA))
    chrA += strlen("chr");
else if (startsWith("Scaffold_",chrA))
    chrA += strlen("Scaffold_");
else if (startsWith("ps_",chrA))
    chrA += strlen("ps_");

if (startsWith("chr",chrB))
    chrB += strlen("chr");
else if (startsWith("Scaffold_",chrB))
    chrB += strlen("Scaffold_");
else if (startsWith("ps_",chrB))
    chrB += strlen("ps_");

lenA = strlen(chrA);
lenB = strlen(chrB);

dif = lenA - lenB;

if (dif == 0)
    dif = strcmp(chrA, chrB);

return dif;
}


int chrNameCmp(char *str1, char *str2)
/* Compare chromosome or linkage group names by number, then suffix.  
 * str1 and str2 must match the regex 
 * "(chr|Group)([0-9]+|[A-Za-z0-9]+)(_[A-Za-z0-9_]+)?". */
{
int num1 = 0, num2 = 0;
int match1 = 0, match2 = 0;
char suffix1[512], suffix2[512];

/* get past "chr" or "Group" prefix: */
if (startsWith("chr", str1))
    str1 += 3;
else if (startsWith("Group", str1))
    str1 += 5;
else
    return -1;
if (startsWith("chr", str2))
    str2 += 3;
else if (startsWith("Group", str2))
    str2 += 5;
else
    return 1;
/* If only one is numeric, that one goes first. */
/* If both are numeric, compare by number; if same number, look at suffix. */
/* Otherwise go alph. but put M and U/Un/Un_random at end. */
match1 = sscanf(str1, "%d%s", &num1, suffix1);
match2 = sscanf(str2, "%d%s", &num2, suffix2);
if (match1 && !match2)
    return -1;
else if (!match1 && match2)
    return 1;
else if (match1 && match2)
    {
    int diff = num1 - num2;
    if (diff != 0)
	return diff;
    /* same chrom number... got suffix? */
    if (match1 > 1 && match2 <= 1)
	return 1;
    else if (match1 <= 1 && match2 > 1)
	return -1;
    else if (match1 > 1 && match2 > 1)
	return strcmp(suffix1, suffix2);
    else
	/* This shouldn't happen (duplicate chrom name passed in) */
	return 0;
    }
else if (sameString(str1, "M") && !sameString(str2, "M"))
    return 1;
else if (!sameString(str1, "M") && sameString(str2, "M"))
    return -1;
else if (str1[0] == 'U' && str2[0] != 'U')
    return 1;
else if (str1[0] != 'U' && str2[0] == 'U')
    return -1;
else
    return strcmp(str1, str2);
}

int chrSlNameCmp(const void *el1, const void *el2)
/* Compare chromosome names by number, then suffix.  el1 and el2 must be 
 * slName **s (as passed in by slSort) whose names match the regex 
 * "chr([0-9]+|[A-Za-z0-9]+)(_[A-Za-z0-9_]+)?". */
{
struct slName *sln1 = *(struct slName **)el1;
struct slName *sln2 = *(struct slName **)el2;
return chrNameCmp(sln1->name, sln2->name);
}

int getTableSize(char *table)
/* Get count of rows in a table in the primary database */
{
struct sqlConnection *conn = hAllocConn();
int ct = sqlTableSize(conn, table);
hFreeConn(&conn);
return ct;
}


