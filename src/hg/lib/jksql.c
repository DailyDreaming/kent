/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* jksql.c - Stuff to manage interface with SQL database. */
#include "common.h"
#include "portable.h"
#include "errabort.h"
#include <mysql.h>
#include "dlist.h"
#include "jksql.h"
#include "sqlNum.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: jksql.c,v 1.46 2004/01/17 09:35:13 markd Exp $";

boolean sqlTrace = FALSE;  /* setting to true prints each query */
int sqlTraceIndent = 0;    /* number of spaces to indent traces */

struct sqlConnection
/* This is an item on a list of sql open connections. */
    {
    MYSQL *conn;			    /* Connection. */
    struct dlNode *node;		    /* Pointer to list node. */
    struct dlList *resultList;		    /* Any open results. */
    };

struct sqlResult
/* This is an item on a list of sql open results. */
    {
    MYSQL_RES *result;			/* Result. */
    struct dlNode *node;		/* Pointer to list node we're on. */
    struct sqlConnection *conn;		/* Pointer to connection. */
    };

static struct dlList *sqlOpenConnections;

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

void sqlFreeResult(struct sqlResult **pRes)
/* Free up a result. */
{
struct sqlResult *res = *pRes;
if (res != NULL)
    {
    if (res->result != NULL)
	mysql_free_result(res->result);
    if (res->node != NULL)
	{
	dlRemove(res->node);
	freeMem(res->node);
	}
    freez(pRes);
    }
}

void sqlDisconnect(struct sqlConnection **pSc)
/* Close down connection. */
{
struct sqlConnection *sc = *pSc;
if (sc != NULL)
    {
    MYSQL *conn = sc->conn;
    struct dlList *resList = sc->resultList;
    struct dlNode *node = sc->node;
    if (resList != NULL)
	{
	struct dlNode *resNode, *resNext;
	for (resNode = resList->head; resNode->next != NULL; resNode = resNext)
	    {
	    struct sqlResult *res = resNode->val;
	    resNext = resNode->next;
	    sqlFreeResult(&res);
	    }
	freeDlList(&resList);
	}
    if (conn != NULL)
	{
	mysql_close(conn);
	}
    if (node != NULL)
	{
	dlRemove(node);
	freeMem(node);
	}
    freez(pSc);
    }
}

char* sqlGetDatabase(struct sqlConnection *sc)
/* Get the database associated with an connection. */
{
return sc->conn->db;
}

struct slName *sqlGetAllDatabase(struct sqlConnection *sc)
/* Get a list of all database on the server */
{
struct sqlResult *sr = sqlGetResult(sc, "show databases");
char **row;
struct slName *databases = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    slSafeAddHead(&databases, slNameNew(row[0]));
    }
sqlFreeResult(&sr);
return databases;
}

void sqlCleanupAll()
/* Cleanup all open connections and resources. */
{
if (sqlOpenConnections)
    {
    struct dlNode *conNode, *conNext;
    struct sqlConnection *conn;
    for (conNode = sqlOpenConnections->head; conNode->next != NULL; conNode = conNext)
	{
	conn = conNode->val;
	conNext = conNode->next;
	sqlDisconnect(&conn);
	}
    freeDlList(&sqlOpenConnections);
    }
}

static void sqlInitTracking()
/* Initialize tracking and freeing of resources. */
{
if (sqlOpenConnections == NULL)
    {
    sqlOpenConnections = newDlList();
    atexit(sqlCleanupAll);
    }
}

struct sqlConnection *sqlConnRemote(char *host, 
	char *user, char *password, char *database, boolean abort)
/* Connect to database somewhere as somebody.  
 * If abort is set display error message and abort on error. */
{
struct sqlConnection *sc;
MYSQL *conn;

sqlInitTracking();

AllocVar(sc);
sc->resultList = newDlList();
sc->node = dlAddValTail(sqlOpenConnections, sc);

if ((sc->conn = conn = mysql_init(NULL)) == NULL)
    errAbort("Couldn't connect to mySQL.");
if (mysql_real_connect(
	conn,
	host, /* host */
	user,	/* user name */
	password,	/* password */
	database, /* database */
	0,	/* port */
	NULL,	/* socket */
	0)	/* flags */  == NULL)
    {
    if (abort)
	{
	errAbort("Couldn't connect to database %s on %s as %s.\n%s", 
	    database, host, user, mysql_error(conn));
        }
    return NULL;
    }
return sc;
}

struct sqlConnection *sqlConnectRemote(char *host, 
	char *user, char *password, char *database)
/* Connect to database somewhere as somebody. */
{
return sqlConnRemote(host, user, password, database, TRUE);
}

struct sqlConnection *sqlConn(char *database, boolean abort)
/* Connect to database on default host as default user. 
 * Optionally abort on failure. */
{
char* host = getCfgValue("HGDB_HOST", "db.host");
char* user = getCfgValue("HGDB_USER", "db.user");
char* password = getCfgValue("HGDB_PASSWORD", "db.password");

if(host == 0 || user == 0 || password == 0)
    sqlConnectReadOnly(database);
return sqlConnRemote(host, user, password, database, abort);
}

struct sqlConnection *sqlMayConnect(char *database)
/* Connect to database on default host as default user. 
 * Return NULL (don't abort) on failure. */
{
return sqlConn(database, FALSE);
}

struct sqlConnection *sqlConnect(char *database)
/* Connect to database on default host as default user. */
{
return sqlConn(database, TRUE);
}

struct sqlConnection *sqlConnectReadOnly(char *database)
/* Connect to database on default host as default user. */
{
char* host = getCfgValue("HGDB_HOST", "ro.host");
char* user = getCfgValue("HGDB_USER", "ro.user");
char* password = getCfgValue("HGDB_PASSWORD", "ro.password");

if(host == 0 || user == 0 || password == 0)
    {
    errAbort("Could not read hostname, user, or password to the database from configuration file.");
    }
return sqlConnectRemote(host, user, password, database);
}

void sqlVaWarn(struct sqlConnection *sc, char *format, va_list args)
/* Default error message handler. */
{
MYSQL *conn = sc->conn;
if (format != NULL) {
    vaWarn(format, args);
    }
warn("mySQL error %d: %s", mysql_errno(conn), mysql_error(conn));
}

void sqlWarn(struct sqlConnection *sc, char *format, ...)
/* Printf formatted error message that adds on sql 
 * error message. */
{
va_list args;
va_start(args, format);
sqlVaWarn(sc, format, args);
va_end(args);
}

void sqlAbort(struct sqlConnection  *sc, char *format, ...)
/* Printf formatted error message that adds on sql 
 * error message and abort. */
{
va_list args;
va_start(args, format);
sqlVaWarn(sc, format, args);
va_end(args);
noWarnAbort();
}

typedef MYSQL_RES *	STDCALL ResGetter(MYSQL *mysql);

static struct sqlResult *sqlUseOrStore(struct sqlConnection *sc, 
	char *query, ResGetter *getter, boolean abort)
/* Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on. */
{
MYSQL *conn = sc->conn;
if (sqlTrace)
    fprintf(stderr, "%.*squery: %s\n", sqlTraceIndent,
            "                                                       ",
            query);
if (mysql_real_query(conn, query, strlen(query)) != 0)
    {
    if (abort)
	sqlAbort(sc, "Can't start query:\n%s\n", query);
    return NULL;
    }
else
    {
    struct sqlResult *res;
    MYSQL_RES *resSet;
    if ((resSet = getter(conn)) == NULL)
	{
	if (mysql_errno(conn) != 0)
	    {
	    sqlAbort(sc, "Can't use query:\n%s", query);
	    }
	return NULL;
	}
    AllocVar(res);
    res->conn = sc;
    res->result = resSet;
    res->node = dlAddValTail(sc->resultList, res);
    return res;
    }
}

void sqlDropTable(struct sqlConnection *sc, char *table)
/* Drop table if it exists. */
{
if (sqlTableExists(sc, table))
    {
    char query[256];
    sprintf(query, "drop table %s", table);
    sqlUpdate(sc, query);
    }
}

boolean sqlMaybeMakeTable(struct sqlConnection *sc, char *table, char *query)
/* Create table from query if it doesn't exist already. 
 * Returns FALSE if didn't make table. */
{
if (sqlTableExists(sc, table))
    return FALSE;
sqlUpdate(sc, query);
return TRUE;
}

void sqlRemakeTable(struct sqlConnection *sc, char *table, char *create)
/* Drop table if it exists, and recreate it. */
{
sqlDropTable(sc, table);
sqlUpdate(sc, create);
}

boolean sqlDatabaseExists(char *database)
/* Return TRUE if database exists. */
{
struct sqlConnection *conn = sqlMayConnect(database);
boolean exists = (conn != NULL);
sqlDisconnect(&conn);
return exists;
}

boolean sqlTableExists(struct sqlConnection *sc, char *table)
/* Return TRUE if a table exists. */
{
char query[256];
struct sqlResult *sr;

sprintf(query, "select count(*) from %s", table);
if ((sr = sqlUseOrStore(sc,query,mysql_use_result, FALSE)) == NULL)
    return FALSE;
sqlNextRow(sr);	/* Just discard. */
sqlFreeResult(&sr);
return TRUE;
}

boolean sqlTablesExist(struct sqlConnection *conn, char *tables)
/* Check all tables in space delimited string exist. */
{
char *dupe = cloneString(tables);
char *s = dupe, *word;
boolean ok = TRUE;
while ((word = nextWord(&s)) != NULL)
     {
     if (!sqlTableExists(conn, word))
         {
	 ok = FALSE;
	 break;
	 }
     }
freeMem(dupe);
return ok;
}


struct sqlResult *sqlGetResult(struct sqlConnection *sc, char *query)
/* Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on. */
{
return sqlUseOrStore(sc,query,mysql_use_result, TRUE);
}

struct sqlResult *sqlMustGetResult(struct sqlConnection *sc, char *query)
/* Query database. If result empty squawk and die. */
{
struct sqlResult *res = sqlGetResult(sc,query);
if (res == NULL)
	errAbort("Object not found in database.\nQuery was %s", query);
return res;
}


void sqlUpdate(struct sqlConnection *conn, char *query)
/* Tell database to do something that produces no results table. */
{
struct sqlResult *sr;
sr = sqlGetResult(conn,query);
sqlFreeResult(&sr);
}

int sqlUpdateRows(struct sqlConnection *conn, char *query, int* matched)
/* Execute an update query, returning the number of rows change.  If matched
 * is not NULL, it gets the total number matching the query. */
{
int numChanged, numMatched;
const char *info;
int numScan = 0;
struct sqlResult *sr = sqlGetResult(conn,query);

/* Rows matched: 40 Changed: 40 Warnings: 0 */
info = mysql_info(conn->conn);
if (info != NULL)
    numScan = sscanf(info, "Rows matched: %d Changed: %d Warnings: %*d",
                     &numMatched, &numChanged);
if ((info == NULL) || (numScan < 2))
    errAbort("can't get info (maybe not an sql UPDATE): %s", query);
sqlFreeResult(&sr);
if (matched != NULL)
    *matched = numMatched;
return numChanged;
}

static boolean isMySql4(struct sqlConnection *conn)
/* determine if this is at least mysql 4.0 or newer */
{
char majorVerBuf[64];
char *dotPtr = strchr(conn->conn->server_version, '.');
int len = (dotPtr - conn->conn->server_version);
assert(dotPtr != NULL);
strncpy(majorVerBuf, conn->conn->server_version, len);
majorVerBuf[len] = '\0';

return (sqlUnsigned(majorVerBuf) >= 4);
}

void sqlLoadTabFile(struct sqlConnection *conn, char *path, char *table,
                    unsigned options)
/* Load a tab-seperated file into a database table, checking for errors. 
 * Options are the SQL_TAB_* bit set. */
{
char tabPath[PATH_LEN];
char query[PATH_LEN+256];
int numScan, numRecs, numSkipped, numWarnings;
char *localOpt, *concurrentOpt, *dupOpt;
const char *info;
struct sqlResult *sr;
boolean mysql4 = isMySql4(conn);

/* Doing an "alter table disable keys" command implicitly commits the current
   transaction. Don't want to use that optimization if we need to be transaction
   safe. */
/* FIXME: markd 2003/01/05: mysql 4.0.17 - the alter table enable keys hangs,
 * disable this optimization for now. Verify performance on small loads
 * before re-enabling*/
# if 0
boolean doDisableKeys = !(options & SQL_TAB_TRANSACTION_SAFE);
#else
boolean doDisableKeys = FALSE;
#endif

/* determine if tab file can be accessed directly by the database, or send
 * over the network */
if (options & SQL_TAB_FILE_ON_SERVER)
    {
    /* tab file on server requiries full path */
    strcpy(tabPath, "");
    if (path[0] != '/')
        {
        getcwd(tabPath, sizeof(tabPath));
        strcat(tabPath, "/");
        }
    strcat(tabPath, path);
    localOpt = "";
    }
else
    {
    strcpy(tabPath, path);
    localOpt = "LOCAL";
    }

/* optimize for concurrent to others to access the table. */
if (options & SQL_TAB_FILE_CONCURRENT)
    concurrentOpt = "CONCURRENT";
else
    {
    concurrentOpt = "";
    if (mysql4 && doDisableKeys)
        {
        /* disable update of indexes during load. Inompatible with concurrent,
         * since enable keys locks other's out. */
        safef(query, sizeof(query), "ALTER TABLE %s DISABLE KEYS", table);
        sqlUpdate(conn, query);
        }
    }

if (options & SQL_TAB_REPLACE)
    dupOpt = "REPLACE";
else
    dupOpt = "";

safef(query, sizeof(query),  "LOAD DATA %s %s %s INFILE '%s' INTO TABLE %s",
      concurrentOpt, localOpt, dupOpt, tabPath, table);
sr = sqlGetResult(conn, query);
info = mysql_info(conn->conn);
if (info == NULL)
    errAbort("no info available for result of sql query: %s", query);
numScan = sscanf(info, "Records: %d Deleted: %*d  Skipped: %d  Warnings: %d",
                 &numRecs, &numSkipped, &numWarnings);
if (numScan != 3)
    errAbort("can't parse sql load info: %s", info);
sqlFreeResult(&sr);

if ((numSkipped > 0) || (numWarnings > 0))
    {
    boolean doAbort = TRUE;
    if ((numSkipped > 0) && (options & SQL_TAB_FILE_WARN_ON_ERROR))
        doAbort = FALSE;  /* don't abort on errors */
    else if ((numWarnings > 0) &&
             (options & (SQL_TAB_FILE_WARN_ON_ERROR|SQL_TAB_FILE_WARN_ON_WARN)))
        doAbort = FALSE;  /* don't abort on warnings */

    if (doAbort)
        errAbort("load of %s did not go as planned: %d record(s), "
                 "%d row(s) skipped, %d warning(s) loading %s",
                 table, numRecs, numSkipped, numWarnings, path);
    else
        warn("Warning: load of %s did not go as planned: %d record(s), "
             "%d row(s) skipped, %d warning(s) loading %s",
             table, numRecs, numSkipped, numWarnings, path);
    }
if (((options & SQL_TAB_FILE_CONCURRENT) == 0) && mysql4 && doDisableKeys)
    {
    /* reenable update of indexes */
    safef(query, sizeof(query), "ALTER TABLE %s ENABLE KEYS", table);
    sqlUpdate(conn, query);
    }
}

boolean sqlExists(struct sqlConnection *conn, char *query)
/* Query database and return TRUE if it had a non-empty result. */
{
struct sqlResult *sr;
if ((sr = sqlGetResult(conn,query)) == NULL)
    return FALSE;
else
    {
    if(sqlNextRow(sr) == NULL)
	{
	sqlFreeResult(&sr);
	return FALSE;
	}
    else
	{
	sqlFreeResult(&sr);
	return TRUE;
	}
    }
}

boolean sqlRowExists(struct sqlConnection *conn,
	char *table, char *field, char *key)
/* Return TRUE if row where field = key is in table. */
{
char query[256];
safef(query, sizeof(query), "select count(*) from %s where %s = '%s'",
	table, field, key);
return sqlQuickNum(conn, query) > 0;
}


struct sqlResult *sqlStoreResult(struct sqlConnection *sc, char *query)
/* Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on.  Same interface as sqlGetResult,
 * but internally this keeps the entire result in memory. */
{
return sqlUseOrStore(sc,query,mysql_store_result, TRUE);
}

char **sqlNextRow(struct sqlResult *sr)
/* Get next row from query result. */
{
char** row = NULL;
if (sr != NULL)
    {
    row = mysql_fetch_row(sr->result);
    if (mysql_errno(sr->conn->conn) != 0)
        {
        sqlAbort(sr->conn, "nextRow failed");
        }
    }
return row;
}

/* repeated calls to this function returns the names of the fields 
 * the given result */
char* sqlFieldName(struct sqlResult *sr)
{
MYSQL_FIELD *field;

field = mysql_fetch_field(sr->result);
if(field == 0)
	return 0;

return field->name;
}

int sqlCountColumns(struct sqlResult *sr)
/* Count the number of columns in result. */
{
if(sr != 0)
	return mysql_field_count(sr->conn->conn);
return 0;
}

#ifdef SOMETIMES  /* Not available for all MYSQL environments. */
int sqlFieldCount(struct sqlResult *sr)
/* Return number of fields in a row of result. */
{
if (sr == NULL)
    return 0;
return mysql_field_count(sr->result);
}
#endif /* SOMETIMES */

int sqlFieldCount(struct sqlResult *sr)
/* Return number of fields in a row of result. */
{
if (sr == NULL)
    return 0;
return mysql_num_fields(sr->result);
}

int sqlCountRows(struct sqlConnection *sc, char *table)
/* Return the number of fields in a table */
{
char query[256];
struct sqlResult *sr;
char **row;
int count;

/* Read table description and count rows. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(sc, query);
count = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    count++;
    }
sqlFreeResult(&sr);
return count;
}

char *sqlQuickQuery(struct sqlConnection *sc, char *query, char *buf, int bufSize)
/* Does query and returns first field in first row.  Meant
 * for cases where you are just looking up one small thing.  
 * Returns NULL if query comes up empty. */
{
struct sqlResult *sr;
char **row;
char *ret = NULL;

if ((sr = sqlGetResult(sc, query)) == NULL)
    return NULL;
row = sqlNextRow(sr);
if (row != NULL)
    {
    strncpy(buf, row[0], bufSize);
    ret = buf;
    }
sqlFreeResult(&sr);
return ret;
}

char *sqlNeedQuickQuery(struct sqlConnection *sc, char *query, 
	char *buf, int bufSize)
/* Does query and returns first field in first row.  Meant
 * for cases where you are just looking up one small thing.  
 * Prints error message and aborts if query comes up empty. */
{
char *s = sqlQuickQuery(sc, query, buf, bufSize);
if (s == NULL)
    errAbort("query not found: %s", query);
return s;
}


int sqlQuickNum(struct sqlConnection *conn, char *query)
/* Get numerical result from simple query */
{
struct sqlResult *sr;
char **row;
int ret = 0;

sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    ret = sqlSigned(row[0]);
sqlFreeResult(&sr);
return ret;
}

int sqlNeedQuickNum(struct sqlConnection *conn, char *query)
/* Get numerical result or die trying. */
{
char buf[32];
sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
return sqlSigned(buf);
}

char *sqlQuickString(struct sqlConnection *sc, char *query)
/* Return result of single-row/single column query in a
 * string that should eventually be freeMem'd. */
{
struct sqlResult *sr;
char **row;
char *ret = NULL;

if ((sr = sqlGetResult(sc, query)) == NULL)
    return NULL;
row = sqlNextRow(sr);
if (row != NULL)
    ret = cloneString(row[0]);
sqlFreeResult(&sr);
return ret;
}

char *sqlNeedQuickString(struct sqlConnection *sc, char *query)
/* Return result of single-row/single column query in a
 * string that should eventually be freeMem'd.  This will
 * print an error message and abort if result returns empty. */
{
char *s = sqlQuickString(sc, query);
if (s == NULL)
    errAbort("query not found: %s", query);
return s;
}

struct slName *sqlQuickList(struct sqlConnection *conn, char *query)
/* Return a list of slNames for a single column query.
 * Do a slFreeList on result when done. */
{
struct slName *list = NULL, *n;
struct sqlResult *sr;
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    n = slNameNew(row[0]);
    slAddHead(&list, n);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

int sqlTableSize(struct sqlConnection *conn, char *table)
/* Find number of rows in table. */
{
char query[128];
sprintf(query, "select count(*) from %s", table);
return sqlQuickNum(conn, query);
}

int sqlFieldIndex(struct sqlConnection *conn, char *table, char *field)
/* Returns index of field in a row from table, or -1 if it 
 * doesn't exist. */
{
char query[256];
struct sqlResult *sr;
char **row;
int i = 0, ix=-1;

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], field))
        {
	ix = i;
	break;
	}
    ++i;
    }
sqlFreeResult(&sr);
return ix;
}

unsigned int sqlLastAutoId(struct sqlConnection *conn)
/* Return last automatically incremented id inserted into database. */
{
return mysql_insert_id(conn->conn);
}

/* Stuff to manage and cache up to 16 open connections on 
 * a database.  Typically you only need 3.
 * MySQL takes about 2 milliseconds on a local
 * host to open a connection.  On a remote host it can be more
 * and this caching is probably actually necessary. */

enum {maxConn = 16};

struct sqlConnCache
    {
    char *database;				/* SQL database. */
    char *host;					/* Host machine of database. */
    char *user;					/* Database user name */
    char *password;				/* Password. */
    int connAlloced;                            /* # open connections. */
    struct sqlConnection *connArray[maxConn];   /* Open connections. */
    boolean connUsed[maxConn];                  /* Tracks used conns. */
    };

struct sqlConnCache *sqlNewRemoteConnCache(char *database, 
	char *host, char *user, char *password)
/* Set up a cache on a remote database. */
{
struct sqlConnCache *cache;
AllocVar(cache);
cache->database = cloneString(database);
cache->host = cloneString(host);
cache->user = cloneString(user);
cache->password = cloneString(password);
return cache;
}

struct sqlConnCache *sqlNewConnCache(char *database)
/* Return a new connection cache. */
{
char* host = getCfgValue("HGDB_HOST", "db.host");
char* user = getCfgValue("HGDB_USER", "db.user");
char* password = getCfgValue("HGDB_PASSWORD", "db.password");
if (password == NULL || user == NULL || host == NULL)
    errAbort("Could not read hostname, user, or password to the database from configuration file.");
return sqlNewRemoteConnCache(database, host, user, password);
}

void sqlFreeConnCache(struct sqlConnCache **pCache)
/* Dispose of a connection cache. */
{
struct sqlConnCache *cache;
if ((cache = *pCache) != NULL)
    {
    int i;
    for (i=0; i<cache->connAlloced; ++i)
	sqlDisconnect(&cache->connArray[i]);
    freeMem(cache->database);
    freeMem(cache->host);
    freeMem(cache->user);
    freeMem(cache->password);
    freez(pCache);
    }
}

struct sqlConnection *sqlAllocConnection(struct sqlConnCache *cache)
/* Allocate a cached connection. */
{
int i;
int connAlloced = cache->connAlloced;
boolean *connUsed = cache->connUsed;
for (i=0; i<connAlloced; ++i)
    {
    if (!connUsed[i])
	{
	connUsed[i] = TRUE;
	return cache->connArray[i];
	}
    }
if (connAlloced >= maxConn)
   errAbort("Too many open sqlConnections to %s for cache", cache->database);
cache->connArray[connAlloced] = sqlConnectRemote(cache->host, 
	cache->user, cache->password, cache->database);
connUsed[connAlloced] = TRUE;
++cache->connAlloced;
return cache->connArray[connAlloced];
}

void sqlFreeConnection(struct sqlConnCache *cache, struct sqlConnection **pConn)
/* Free up a cached connection. */
{
struct sqlConnection *conn;
int connAlloced = cache->connAlloced;
struct sqlConnection **connArray = cache->connArray;

if ((conn = *pConn) != NULL)
    {
    int ix;
    for (ix = 0; ix < connAlloced; ++ix)
	{
	if (connArray[ix] == conn)
	    {
	    cache->connUsed[ix] = FALSE;
	    break;
	    }
	}
    *pConn = NULL;
    }
}


char *sqlEscapeString2(char *to, const char* from)
/* Prepares a string for inclusion in a sql statement.  Output string
 * must be 2*strlen(from)+1 */
{
mysql_escape_string(to, from, strlen(from));
return to; 
}

char *sqlEscapeString(const char* from)
/* Prepares string for inclusion in a SQL statement . Remember to free
 * returned string.  Returned string contains strlen(length)*2+1 as many bytes
 * as orig because in worst case every character has to be escaped.*/
{
int size = (strlen(from)*2) +1;
char *to = needMem(size * sizeof(char));
return sqlEscapeString2(to, from);
}

char *sqlEscapeTabFileString2(char *to, const char *from)
/* Escape a string for including in a tab seperated file. Output string
 * must be 2*strlen(from)+1 */
{
const char *fp = from;
char *tp = to;
while (*fp != '\0')
    {
    switch (*fp)
        {
        case '\\':
            *tp++ = '\\';
            *tp++ = '\\';
            break;
        case '\n':
            *tp++ = '\\';
            *tp++ = 'n';
            break;
        case '\t':
            *tp++ = '\\';
            *tp++ = 't';
            break;
        default:
            *tp++ = *fp;
            break;
        }
    fp++;
    }
*tp = '\0'; 
return to; 
}

char *sqlEscapeTabFileString(const char *from)
/* Escape a string for including in a tab seperated file. Output string
 * must be 2*strlen(from)+1 */
{
int size = (strlen(from)*2) +1;
char *to = needMem(size * sizeof(char));
return sqlEscapeTabFileString2(to, from);
}


struct hash *sqlHashOfDatabases()
/* Get hash table with names of all databases that are online. */
{
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = sqlConnect("mysql");
struct hash *hash = newHash(8);

sr = sqlGetResult(conn, "show databases");
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[0], NULL);
    }
sqlDisconnect(&conn);
return hash;
}


char *connGetDatabase(struct sqlConnCache *conn)
/* return database for a connection cache */
{
    return conn->database;
}

char *sqlLikeFromWild(char *wild)
/* Convert normal wildcard string to SQL wildcard by
 * mapping * to % and ? to _.  Escape any existing % and _'s. */
{
int escCount = countChars(wild, '%') + countChars(wild, '_');
int size = strlen(wild) + escCount + 1;
char *retVal = needMem(size);
char *s = retVal, c;

while ((c = *wild++) != 0)
    {
    switch (c)
	{
	case '%':
	case '_':
	    *s++ = '\\';
	    *s++ = c;
	    break;
	case '*':
	    *s++ = '%';
	    break;
	case '?':
	    *s++ = '_';
	    break;
	default:
	    *s++ = c;
	    break;
	}
    }
return retVal;
}

