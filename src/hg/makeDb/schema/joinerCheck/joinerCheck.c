/* joinerCheck - Parse and check joiner file. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "joiner.h"

static char const rcsid[] = "$Id: joinerCheck.c,v 1.18 2004/03/16 01:05:17 kent Exp $";

/* Variable that are set from command line. */
boolean parseOnly; 
char *fieldListIn;
char *fieldListOut;
char *identifier;
char *database;
boolean foreignKeys;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinerCheck - Parse and check joiner file\n"
  "usage:\n"
  "   joinerCheck file.joiner\n"
  "options:\n"
  "   -parseOnly just parse joiner file, don't check database.\n"
  "   -fieldListOut=file - List all fields in all databases to file.\n"
  "   -fieldListIn=file - Get list of fields from file rather than mysql.\n"
  "   -identifier=name - Just validate given identifier.\n"
  "   -database=name - Just validate given database.\n"
  "   -foreignKeys - Validate (foreign) keys.  Takes about an hour.\n"
  );
}

static struct optionSpec options[] = {
   {"parseOnly", OPTION_BOOLEAN},
   {"fieldListIn", OPTION_STRING},
   {"fieldListOut", OPTION_STRING},
   {"identifier", OPTION_STRING},
   {"database", OPTION_STRING},
   {"foreignKeys", OPTION_BOOLEAN},
   {NULL, 0},
};


static void printField(struct joinerField *jf, FILE *f)
/* Print out field info to f. */
{
struct slName *db;
for (db = jf->dbList; db != NULL; db = db->next)
    {
    if (db != jf->dbList)
        fprintf(f, ",");
    fprintf(f, "%s", db->name);
    }
fprintf(f, ".%s.%s", jf->table, jf->field);
}

static char *emptyForNull(char *s)
/* Return "" for NULL strings, otherwise string itself. */
{
if (s == NULL)
    s = "";
return s;
}

struct slName *getTablesForField(struct sqlConnection *conn, 
	char *splitPrefix, char *table, char *splitSuffix)
/* Get tables that match field. */
{
struct slName *list = NULL, *el;
if (splitPrefix != NULL || splitSuffix != NULL)
    {
    char query[256], **row;
    struct sqlResult *sr;
    safef(query, sizeof(query), "show tables like '%s%s%s'", 
    	emptyForNull(splitPrefix), table, emptyForNull(splitSuffix));
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	el = slNameNew(row[0]);
	slAddHead(&list, el);
	}
    sqlFreeResult(&sr);
    slReverse(&list);
    }
if (list == NULL)
    if (sqlTableExists(conn, table))
	list = slNameNew(table);
return list;
}

static boolean fieldExists(struct hash *fieldHash,
	struct joinerSet *js, struct joinerField *jf)
/* Make sure field exists in at least one database. */
{
struct slName *db;
boolean gotIt = FALSE;
for (db = jf->dbList; db != NULL && !gotIt; db = db->next)
    {
    struct sqlConnection *conn = sqlConnect(db->name);
    struct slName *table, *tableList = getTablesForField(conn,
    			jf->splitPrefix, jf->table, jf->splitSuffix);
    char fieldName[512];
    sqlDisconnect(&conn);
    for (table = tableList; table != NULL; table = table->next)
	{
	safef(fieldName, sizeof(fieldName), "%s.%s.%s", 
	    db->name, table->name, jf->field);
	if (hashLookup(fieldHash, fieldName))
	    {
	    gotIt = TRUE;
	    break;
	    }
	}
    slFreeList(&tableList);
    }
return gotIt;
}

#ifdef OLD
static struct hash *getDbChromHash()
/* Return hash with chromosome name list for each database
 * that has a chromInfo table. */
{
struct sqlConnection *conn = sqlConnect("mysql");
struct slName *dbList = sqlGetAllDatabase(conn), *db;
struct hash *dbHash = newHash(10);
struct slName *chromList, *chrom;
sqlDisconnect(&conn);

for (db = dbList; db != NULL; db = db->next)
     {
     conn = sqlConnect(db->name);
     if (sqlTableExists(conn, "chromInfo"))
          {
	  struct sqlResult *sr;
	  char **row;
	  struct slName *chromList = NULL, *chrom;
	  sr = sqlGetResult(conn, "select chrom from chromInfo");
	  while ((row = sqlNextRow(sr)) != NULL)
	      {
	      chrom = slNameNew(row[0]);
	      slAddHead(&chromList, chrom);
	      }
	  sqlFreeResult(&sr);
	  slReverse(&chromList);
	  hashAdd(dbHash, db->name, chromList);
	  }
     sqlDisconnect(&conn);
     }
slFreeList(&dbList);
return dbHash;
}
#endif /* OLD */


void joinerValidateFields(struct joiner *joiner, struct hash *fieldHash,
	char *oneIdentifier)
/* Make sure that joiner refers to fields that exist at
 * least somewhere. */
{
struct joinerSet *js;
struct joinerField *jf;
struct slName *db;

for (js=joiner->jsList; js != NULL; js = js->next)
    {
    if (oneIdentifier == NULL || sameString(oneIdentifier, js->name))
	{
	for (jf = js->fieldList; jf != NULL; jf = jf->next)
	    {
	    if (!fieldExists(fieldHash, js, jf))
		 {
		 if (!js->expanded)
		     {
		     printField(jf, stderr);
		     fprintf(stderr, " not found in %s line %d of %s\n",
			js->name, jf->lineIx, joiner->fileName);
		     }
		 }
	    }
	}
    }
}

struct slName *jsAllDb(struct joinerSet *js)
/* Get list of all databases referred to by set. */
{
struct slName *list = NULL, *db;
struct joinerField *jf;
for (jf = js->fieldList; jf != NULL; jf = jf->next)
    {
    for (db = jf->dbList; db != NULL; db = db->next)
	slNameStore(&list, db->name);
    }
return list;
}

int sqlTableRows(struct sqlConnection *conn, char *table)
/* REturn number of rows in table. */
{
char query[256];
safef(query, sizeof(query), "select count(*) from %s", table);
return sqlQuickNum(conn, query);
}

int totalTableRows(struct sqlConnection *conn, struct slName *tableList)
/* Return total number of rows in all tables in list. */
{
int rowCount = 0;
struct slName *table;
for (table = tableList; table != NULL; table = table->next)
    {
    rowCount += sqlTableRows(conn, table->name);
    }
return rowCount;
}

struct sqlConnection *sqlWarnConnect(char *db)
/* Connect to database, or warn and return NULL. */
{
struct sqlConnection *conn = sqlMayConnect(db);
if (conn == NULL)
    warn("Error: Couldn't connect to database %s", db);
return conn;
}

static char *doChops(struct joinerField *jf, char *id)
/* Return chopped version of id.  (This may insert a zero into s) */
{
struct slName *chop;
for (chop = jf->chopBefore; chop != NULL; chop = chop->next)
    {
    char *s = stringIn(chop->name, id);
    if (s != NULL)
	 id = s + strlen(chop->name);
    }
for (chop = jf->chopAfter; chop != NULL; chop = chop->next)
    {
    char *s = rStringIn(chop->name, id);
    if (s != NULL)
	*s = 0;
    }
return id;
}

struct hash *readKeyHash(char *db, struct joiner *joiner, 
	struct joinerField *keyField)
/* Read key-field into hash.  Check for dupes if need be. */
{
struct sqlConnection *conn = sqlWarnConnect(db);
struct hash *keyHash = NULL;
if (conn == NULL)
    {
    return NULL;
    }
else
    {
    struct slName *table;
    struct slName *tableList = getTablesForField(conn,keyField->splitPrefix,
    						 keyField->table, 
						 keyField->splitSuffix);
    int rowCount = totalTableRows(conn, tableList);
    int hashSize = digitsBaseTwo(rowCount)+1;
    char query[256], **row;
    struct sqlResult *sr;
    int itemCount = 0;
    int dupeCount = 0;
    char *dupe = NULL;

    if (rowCount > 0)
	{
	if (hashSize > hashMaxSize)
	    hashSize = hashMaxSize;
	keyHash = hashNew(hashSize);
	for (table = tableList; table != NULL; table = table->next)
	    {
	    safef(query, sizeof(query), "select %s from %s", 
		keyField->field, table->name);
	    sr = sqlGetResult(conn, query);
	    while ((row = sqlNextRow(sr)) != NULL)
		{
		char *id = doChops(keyField, row[0]);
		if (hashLookup(keyHash, id))
		    {
		    if (!keyField->dupeOk)
			{
			if (keyField->exclude == NULL || 
				!slNameInList(keyField->exclude, id))
			    {
			    if (dupeCount == 0)
				dupe = cloneString(id);
			    ++dupeCount;
			    }
			}
		    }
		else
		    {
		    hashAdd(keyHash, id, NULL);
		    ++itemCount;
		    }
		}
	    sqlFreeResult(&sr);
	    }
	if (dupe != NULL)
	    {
	    warn("Error: %d duplicates in %s.%s.%s including '%s'",
		    dupeCount, db, keyField->table, keyField->field, dupe);
	    freez(&dupe);
	    }
	verbose(1, "%s.%s.%s - %d unique identifiers\n", 
		db, keyField->table, keyField->field, itemCount);
	}
    slFreeList(&tableList);
    }
sqlDisconnect(&conn);
return keyHash;
}

static void addHitMiss(struct hash *keyHash, char *id, struct joinerField *jf,
	int *pHits, char **pMiss, int *pTotal)
/* Record info about one hit or miss to keyHash */
{
id = doChops(jf, id);
if (jf->exclude == NULL || !slNameInList(jf->exclude, id))
    {
    if (hashLookup(keyHash, id))
	++(*pHits);
    else
	{
	if (*pMiss == NULL)
	    *pMiss = cloneString(id);
	}
    ++(*pTotal);
    }
}

void doMinCheck(char *db, struct joiner *joiner, struct joinerSet *js, 
	struct hash *keyHash, struct joinerField *keyField,
	struct joinerField *jf)
{
struct sqlConnection *conn = sqlMayConnect(db);
if (conn != NULL)
    {
    int total = 0, hits = 0, hitsNeeded;
    char *miss = NULL;
    struct slName *table;
    struct slName *tableList = getTablesForField(conn,jf->splitPrefix,
    						 jf->table, jf->splitSuffix);
    for (table = tableList; table != NULL; table = table->next)
	{
	char query[256], **row;
	struct sqlResult *sr;
	safef(query, sizeof(query), "select %s from %s", 
		jf->field, table->name);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    if (jf->separator == NULL)
		{
		addHitMiss(keyHash, row[0], jf, &hits, &miss, &total);
		}
	    else 
	        {
		/* Do list. */
		struct slName *el, *list;
		int ix;
		list = slNameListFromString(row[0], jf->separator[0]);
		for (el = list, ix=0; el != NULL; el = el->next, ++ix)
		    {
		    char *id;
		    char buf[16];
		    if (jf->indexOf)
			{
		        safef(buf, sizeof(buf), "%d", ix);
			id = buf;
			}
		    addHitMiss(keyHash, el->name, jf, &hits, &miss, &total);
		    }
		slFreeList(&list);
		}
	    }
	sqlFreeResult(&sr);
	}
    if (tableList != NULL)
	{
	verbose(1, "%s.%s.%s - hits %d of %d\n", db, jf->table, jf->field, hits, total);
	hitsNeeded = round(total * jf->minCheck);
	if (hits < hitsNeeded)
	    {
	    warn("Error: %d of %d elements of %s.%s.%s are not in key %s.%s line %d of %s\n"
		 "Example miss: %s"
		, total - hits, total, db, jf->table, jf->field
		, keyField->table, keyField->field
		, jf->lineIx, joiner->fileName, miss);
	    }
	freez(&miss);
	sqlDisconnect(&conn);
	}
    slFreeList(&tableList);
    }
}

void jsValidateKeysOnDb(struct joiner *joiner, struct joinerSet *js,
	char *db, struct hash *keyHash)
/* Validate keys pertaining to this database. */
{
struct joinerField *jf;
struct hash *localKeyHash = NULL;
struct joinerField *keyField;

if (js->isFuzzy)
    return;
if ((keyField = js->fieldList) == NULL)
    return;
if (keyHash == NULL)
    {
    char *keyDb;
    if (slNameInList(keyField->dbList, db))
	{
	keyDb = db;
	}
    else
	{
	if (slCount(keyField->dbList) == 1)
	    keyDb = keyField->dbList->name;
	else
	    {
	    warn("Error line %d of %s:\n"
		 "Key (first) field contains multiple databases\n"
		 "but not all databases in other fields."
		 , keyField->lineIx, joiner->fileName);
	    return;
	    }
	}
    keyHash = localKeyHash = readKeyHash(keyDb, joiner, keyField);
    }
if (keyHash != NULL)
    {
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
	{
	if (jf != keyField && slNameInList(jf->dbList, db))
	    doMinCheck(db, joiner, js, keyHash, keyField, jf);
	}
    }
hashFree(&localKeyHash);
}

void jsValidateKeys(struct joiner *joiner, struct joinerSet *js, 
	char *preferredDb)
/* Validate keys on js.  If preferredDb is non-NULL then do it on
 * that database.  Otherwise do it on all databases. */
{
struct joinerField *keyField;
struct hash *keyHash = NULL;

/* If key is found in a single database then make hash here
 * rather than separately for each database. */
if ((keyField = js->fieldList) == NULL)
    return;
if (slCount(keyField->dbList) == 1)
    keyHash = readKeyHash(keyField->dbList->name, joiner, keyField);

/* Check key for database(s) */
if (preferredDb)
    jsValidateKeysOnDb(joiner, js, preferredDb, keyHash);
else
    {
    struct slName *db, *dbList = jsAllDb(js);
    for (db = dbList; db != NULL; db = db->next)
        jsValidateKeysOnDb(joiner, js, db->name, keyHash);
    slFreeList(&dbList);
    }
hashFree(&keyHash);
}

void joinerValidateKeys(struct joiner *joiner, 
	char *oneIdentifier, char *oneDatabase)
/* Make sure that joiner refers to fields that exist at
 * least somewhere. */
{
struct joinerSet *js;
int validations = 0;
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    if (oneIdentifier == NULL || sameString(oneIdentifier, js->name))
	{
        jsValidateKeys(joiner, js, oneDatabase);
	++validations;
	}
    }
if (validations < 1 && oneIdentifier)
    errAbort("Identifier %s not found in %s", oneIdentifier, joiner->fileName);
}

struct hash *processFieldHash(char *inName, char *outName)
/* Read in field hash from file if inName is non-NULL, 
 * else read from database.  If outName is non-NULL, 
 * save it to file. */
{
struct hash *fieldHash;
struct hashEl *el;

if (inName != NULL)
    fieldHash = hashWordsInFile(inName, 18);
else
    fieldHash = sqlAllFields();
if (outName != NULL)
    {
    struct hashEl *el, *list = hashElListHash(fieldHash);
    FILE *f = mustOpen(outName, "w");
    slSort(&list, hashElCmp);
    for (el = list; el != NULL; el = el->next)
	fprintf(f, "%s\n", el->name);
    slFreeList(&list);
    carefulClose(&f);
    }
return fieldHash;
}

void reportErrorList(struct slName **pList, char *message)
/* Report error on list of string references. */
/* Convert a list of string references to a comma-separated
 * slName.  Free up refList. */
{
if (*pList != NULL)
    {
    struct dyString *dy = dyStringNew(0);
    struct slName *name, *list;
    slReverse(pList);
    list = *pList;
    dyStringAppend(dy, list->name);
    for (name = list->next; name != NULL; name = name->next)
	{
        dyStringAppendC(dy, ',');
	dyStringAppend(dy, name->name);
	}
    warn("Error: %s %s", dy->string, message);
    slFreeList(pList);
    dyStringFree(&dy);
    }
}


static void joinerCheckDbCoverage(struct joiner *joiner)
/* Complain about databases that aren't in databasesChecked or ignored. */
{
struct slName *missList = NULL, *miss;
struct slName *db, *dbList = sqlListOfDatabases();

/* Keep a list of databases that aren't in either hash. */
for (db = dbList; db != NULL; db = db->next)
    {
    if (!hashLookup(joiner->databasesChecked, db->name) 
        && !hashLookup(joiner->databasesIgnored, db->name))
	{
	miss = slNameNew(db->name);
	slAddHead(&missList, miss);
	}
    }

/* If necessary report (in a single message) database not in joiner file. */
reportErrorList(&missList, "not in databasesChecked or databasesIgnored");
}

static void addTablesLike(struct hash *hash, struct sqlConnection *conn,
	char *spec)
/* Add tables like spec to hash. */
{
if (sqlWildcardIn(spec))
    {
    struct sqlResult *sr;
    char query[512], **row;
    safef(query, sizeof(query), 
	    "show tables like '%s'", spec);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	hashAdd(hash, row[0], NULL);
    sqlFreeResult(&sr);
    }
else
    {
    hashAdd(hash, spec, NULL);
    }
}


struct hash *getCoveredTables(struct joiner *joiner, char *db, 
	struct sqlConnection *conn)
/* Get list of tables covered in database. */
{
struct hash *hash = hashNew(0);
struct joinerIgnore *ig;
struct slName *spec;
struct joinerSet *js;
struct joinerField *jf;

/* First put in all the ignored tables. */
for (ig = joiner->tablesIgnored; ig != NULL; ig = ig->next)
    {
    if (slNameInList(ig->dbList, db))
        {
	for (spec = ig->tableList; spec != NULL; spec = spec->next)
	    {
	    addTablesLike(hash, conn, spec->name);
	    }
	}
    }

/* Now put in tables that are in one of the identifiers. */
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
        {
	if (slNameInList(jf->dbList, db))
	    {
	    char spec[512];
	    safef(spec, sizeof(spec), "%s%s%s",
	        emptyForNull(jf->splitPrefix), jf->table,
		emptyForNull(jf->splitSuffix));
	    addTablesLike(hash, conn, spec);
	    }
	}
    }
return hash;
}

void joinerCheckTableCoverage(struct joiner *joiner)
/* Check that all tables either are part of an identifier or
 * are in the tablesIgnored statements. */
{
struct slName *miss, *missList = NULL;
struct hashEl *dbList, *db;

uglyf("Checking tables are covered\n");
dbList = hashElListHash(joiner->databasesChecked);
for (db = dbList; db != NULL; db = db->next)
    {
    struct sqlConnection *conn = sqlMayConnect(db->name);
    if (conn == NULL)
        warn("Error: database %s doesn't exist", db->name);
    else
        {
	struct slName *table;
	struct slName *tableList = sqlListTables(conn);
	struct hash *hash = getCoveredTables(joiner, db->name, conn);
	for (table = tableList; table != NULL; table = table->next)
	    {
	    if (!hashLookup(hash, table->name))
	        {
		char fullName[256];
		safef(fullName, sizeof(fullName), "%s.%s", 
			db->name, table->name);
		miss = slNameNew(fullName);
		slAddHead(&missList, miss);
		}
	    }
	slFreeList(&tableList);
	freeHash(&hash);
	reportErrorList(&missList, "tables not in .joiner file");
	}
    sqlDisconnect(&conn);
    }
slFreeList(&dbList);
}

void joinerCheck(char *fileName)
/* joinerCheck - Parse and check joiner file. */
{
struct joiner *joiner = joinerRead(fileName);
if (!parseOnly)
    {
    struct hash *fieldHash;
    joinerCheckDbCoverage(joiner);
    joinerCheckTableCoverage(joiner);
    fieldHash = processFieldHash(fieldListIn, fieldListOut);
    joinerValidateFields(joiner, fieldHash, identifier);
    if (foreignKeys)
	joinerValidateKeys(joiner, identifier, database);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
parseOnly = optionExists("parseOnly");
fieldListIn = optionVal("fieldListIn", NULL);
fieldListOut = optionVal("fieldListOut", NULL);
identifier = optionVal("identifier", NULL);
database = optionVal("database", NULL);
foreignKeys = optionExists("foreignKeys");
joinerCheck(argv[1]);
return 0;
}
