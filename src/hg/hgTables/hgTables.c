/* hgTables - Main and utility functions for table browser. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "obscure.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hCommon.h"
#include "hgColors.h"
#include "trackDb.h"
#include "grp.h"
#include "customTrack.h"
#include "pipeline.h"
#include "hgFind.h"
#include "hgTables.h"
#include "joiner.h"

static char const rcsid[] = "$Id: hgTables.c,v 1.87 2004/11/19 05:53:01 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTables - Get table data associated with tracks and intersect tracks\n"
  "usage:\n"
  "   hgTables XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Global variables. */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldVars;	/* The cart before new cgi stuff added. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *database;		/* Current genome database - hg17, mm5, etc. */
char *freezeName;	/* Date of assembly. */
struct grp *fullGroupList;	/* List of all groups. */
struct grp *curGroup;	/* Currently selected group. */
struct trackDb *fullTrackList;	/* List of all tracks in database. */
struct trackDb *curTrack;	/* Currently selected track. */
char *curTable;		/* Currently selected table. */
struct joiner *allJoiner;	/* Info on how to join tables. */

/* --------------- HTML Helpers ----------------- */

void hvPrintf(char *format, va_list args)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
vprintf(format, args);
if (ferror(stdout))
    noWarnAbort();
}

void hPrintf(char *format, ...)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

void hPrintSpaces(int count)
/* Print a number of non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    hPrintf("&nbsp;");
}

static void vaHtmlOpen(char *format, va_list args)
/* Start up a page that will be in html format. */
{
puts("Content-Type:text/html\n");
cartVaWebStart(cart, format, args);
}

void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
{
va_list args;
va_start(args, format);
vaHtmlOpen(format, args);
}

void htmlClose()
/* Close down html format page. */
{
cartWebEnd();
}

void explainWhyNoResults()
/* Put up a little explanation to user of why they got nothing. */
{
hPrintf("# No results");
if (identifierFileName() != NULL)
    hPrintf(" matching identifier list");
if (anyFilter())
    hPrintf(" passing filter");
if (!fullGenomeRegion())
    hPrintf(" in given region");
if (anyIntersection())
    hPrintf(" after intersection");
hPrintf(".");
}

char *curTableLabel()
/* Return label for current table - track short label if it's a track */
{
if (curTrack != NULL && sameString(curTrack->tableName, curTable))
    return curTrack->shortLabel;
else
    return curTable;
}

/* --------------- Text Mode Helpers ----------------- */

static void textWarnHandler(char *format, va_list args)
/* Text mode error message handler. */
{
char *hLine =
"---------------------------------------------------------------------------\n";
if (format != NULL) {
    fflush(stdout);
    fprintf(stdout, "%s", hLine);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fprintf(stdout, "%s", hLine);
    }
}

static void textAbortHandler()
/* Text mode abort handler. */
{
exit(-1);
}

void textOpen()
/* Start up page in text format. (No need to close this). */
{
char *fileName = cartUsualString(cart, hgtaOutFileName, "");
trimSpaces(fileName);
if (fileName[0] == 0)
    printf("Content-Type: text/plain\n\n");
else
    {
    printf("Content-Disposition: attachment; filename=%s\n", fileName);
    printf("Content-Type: application/octet-stream\n");
    printf("\n");
    }
pushWarnHandler(textWarnHandler);
pushAbortHandler(textAbortHandler);
}

/* --------- Utility functions --------------------- */

void dbOverrideFromTable(char buf[256], char **pDb, char **pTable)
/* If *pTable includes database, overrider *pDb with it, using
 * buf to hold string. */
{
char *s;
safef(buf, 256, "%s", *pTable);
s = strchr(buf, '.');
if (s != NULL)
    {
    *pDb = buf;
    *s++ = 0;
    *pTable = s;
    }
}

static struct trackDb *getFullTrackList()
/* Get all tracks including custom tracks if any. */
{
struct trackDb *list = hTrackDb(NULL), *tdb;
struct customTrack *ctList, *ct;

/* Change the mrna track to all_mrna to avoid confusion elsewhere. */
for (tdb = list; tdb != NULL; tdb = tdb->next)
    {
    if (sameString(tdb->tableName, "mrna"))
        {
	tdb->tableName = cloneString("all_mrna");
	}
    }

/* Create dummy group for custom tracks if any */
ctList = getCustomTracks();
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    slAddHead(&list, ct->tdb);
    }
return list;
}

boolean fullGenomeRegion()
/* Return TRUE if region is full genome. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
return sameString(regionType, "genome");
}

static int regionCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,start */
{
const struct region *a = *((struct region **)va);
const struct region *b = *((struct region **)vb);
int dif;
dif = chrStrippedCmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

struct region *getRegionsFullGenome()
/* Get a region list that covers all of each chromosome. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct region *region, *regionList = NULL;

sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->chrom = cloneString(row[0]);
    region->end = sqlUnsigned(row[1]);
    region->fullChrom = TRUE;
    slAddHead(&regionList, region);
    }
slSort(&regionList, regionCmp);
hFreeConn(&conn);
return regionList;
}

struct region *getEncodeRegions()
/* Get encode regions from encodeRegions table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct region *list = NULL, *region;

sr = sqlGetResult(conn, "select chrom,chromStart,chromEnd from encodeRegions order by name desc");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->chrom = cloneString(row[0]);
    region->start = atoi(row[1]);
    region->end = atoi(row[2]);
    slAddHead(&list, region);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return list;
}

boolean searchPosition(char *range, struct region *region)
/* Try and fill in region via call to hgFind. Return FALSE
 * if it can't find a single position. */
{
struct hgPositions *hgp = NULL;
char retAddr[512];
char position[512];
safef(retAddr, sizeof(retAddr), "%s", "../cgi-bin/hgTables");
hgp = findGenomePosWeb(range, &region->chrom, &region->start, &region->end,
	cart, TRUE, retAddr);
if (hgp != NULL && hgp->singlePos != NULL)
    {
    safef(position, sizeof(position),
	    "%s:%d-%d", region->chrom, region->start+1, region->end);
    cartSetString(cart, hgtaRange, position);
    return TRUE;
    }
else if (region->start == 0)	/* Confusing way findGenomePosWeb says pos not found. */
    {
    cartSetString(cart, hgtaRange, hDefaultPos(database));
    return FALSE;
    }
else
    return FALSE;
}

boolean lookupPosition()
/* Look up position (aka range) if need be.  Return FALSE if it puts
 * up multiple positions. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
char *range = cartUsualString(cart, hgtaRange, "");
boolean isSingle = TRUE;
range = trimSpaces(range);
if (range[0] != 0)
    {
    struct region r;
    isSingle = searchPosition(range, &r);
    }
else
    {
    cartSetString(cart, hgtaRange, hDefaultPos(database));
    }
return isSingle;
}

struct region *getRegions()
/* Consult cart to get list of regions to work on. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
struct region *regionList = NULL, *region;
if (sameString(regionType, "genome"))
    {
    regionList = getRegionsFullGenome();
    }
else if (sameString(regionType, "range"))
    {
    char *range = cartString(cart, hgtaRange);
    regionList = AllocVar(region);
    if ((region->chrom = hgOfficialChromName(range)) == NULL)
	{
	if (!hgParseChromRange(range, &region->chrom, &region->start, &region->end))
	    {
	    errAbort("Bad position %s, please enter something else in position box",
	    	range);
	    }
	}
    }
else if (sameString(regionType, "encode"))
    {
    regionList = getEncodeRegions();
    }
else
    {
    regionList = getRegionsFullGenome();
    }
return regionList;
}

char *getRegionName()
/* Get a name for selected region.  Don't free this. */
{
char *region = cartUsualString(cart, hgtaRegionType, "genome");
if (sameString(region, "range"))
    region = cartUsualString(cart, hgtaRange, "n/a");
return region;
}

struct sqlResult *regionQuery(struct sqlConnection *conn, char *table,
	char *fields, struct region *region, boolean isPositional,
	char *extraWhere)
/* Construct and execute query for table on region. Returns NULL if 
 * table doesn't exist (e.g. missing split table for region->chrom). */
{
struct sqlResult *sr;
if (isPositional)
    {
    /* Check for missing split tables before querying: */
    char *db = sqlGetDatabase(conn);
    struct hTableInfo *hti = hFindTableInfoDb(db, region->chrom, table);
    if (hti == NULL)
	return NULL;
    else if (hti->isSplit)
	{
	char fullTableName[256];
	safef(fullTableName, sizeof(fullTableName),
	      "%s_%s", region->chrom, table);
	if (!sqlTableExists(conn, fullTableName))
	    return NULL;
	}
    if (region->fullChrom) /* Full chromosome. */
	{
	sr = hExtendedChromQuery(conn, table, region->chrom, 
		extraWhere, FALSE, fields, NULL);
	}
    else
	{
	sr = hExtendedRangeQuery(conn, table, region->chrom, 
		region->start, region->end, 
		extraWhere, TRUE, fields, NULL);
	}
    }
else
    {
    struct dyString *query = dyStringNew(0);
    dyStringPrintf(query, "select %s from %s", fields, table);
    if (extraWhere)
         {
	 dyStringAppend(query, " where ");
	 dyStringAppend(query, extraWhere);
	 }
    sr = sqlGetResult(conn, query->string);
    dyStringFree(&query);
    }
return sr;
}

char *trackTable(char *rawTable)
/* Return table name for track, substituting all_mrna
 * for mRNA if need be. */
{
char *table = rawTable;
return table;
}

char *connectingTableForTrack(char *rawTable)
/* Return table name to use with all.joiner for track. 
 * You can freeMem this when done. */
{
if (sameString(rawTable, "mrna"))
    return cloneString("all_mrna");
else if (sameString(rawTable, "est"))
    return cloneString("all_est");
else 
    return cloneString(rawTable);
}

char *chromTable(struct sqlConnection *conn, char *table)
/* Get chr1_table if it exists, otherwise table. 
 * You can freeMem this when done. */
{
char *chrom = hDefaultChrom();
if (sqlTableExists(conn, table))
    return cloneString(table);
else
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s_%s", chrom, table);
    return cloneString(buf);
    }
}

char *chrnTable(struct sqlConnection *conn, char *table)
/* Return chrN_table if table is split, otherwise table. 
 * You can freeMem this when done. */
{
char buf[256];
char *splitTable = chromTable(conn, table);
if (!sameString(splitTable, table))
     {
     safef(buf, sizeof(buf), "chrN_%s", table);
     freez(&splitTable);
     return cloneString(buf);
     }
else
     return splitTable;
}

void checkTableExists(struct sqlConnection *conn, char *table)
/* Check that table exists, or put up an error message. */
{
char *splitTable = chromTable(conn, table);
if (!sqlTableExists(conn, table))
    errAbort("Table %s doesn't exist", table);
freeMem(splitTable);
}

struct hTableInfo *maybeGetHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti = NULL;

if (isCustomTrack(table))
    {
    struct customTrack *ct = lookupCt(table);
    hti = ctToHti(ct);
    }
else
    {
    char *track;
    if (startsWith("chrN_", table))
	track = table + strlen("chrN_");
    else
	track = table;
    hti = hFindTableInfoDb(db, NULL, track);
    }
return(hti);
}

struct hTableInfo *getHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti = maybeGetHti(db, table);

if (hti == NULL)
    {
    errAbort("Could not find table info for table %s in db %s",
	     table, db);
    }
return(hti);
}

boolean isPositional(char *db, char *table)
/* Return TRUE if it looks to be a positional table. */
{
boolean result = FALSE;
struct sqlConnection *conn = sqlConnect(db);
if (sqlTableExists(conn, "chromInfo"))
    {
    char chromName[64];
    struct hTableInfo *hti;
    sqlQuickQuery(conn, "select chrom from chromInfo limit 1", 
	chromName, sizeof(chromName));
    hti = hFindTableInfoDb(db, chromName, table);
    if (hti != NULL)
	{
	result = htiIsPositional(hti);
	}
    }
sqlDisconnect(&conn);
return result;
}

boolean isSqlStringType(char *type)
/* Return TRUE if it a a stringish SQL type. */
{
return strstr(type, "char") || strstr(type, "text") 
	|| strstr(type, "blob") || startsWith("enum", type);
}

boolean isSqlNumType(char *type)
/* Return TRUE if it is a numerical SQL type. */
{
return strstr(type, "int") || strstr(type, "float") || strstr(type, "double");
}

struct trackDb *findTrackInGroup(char *name, struct trackDb *trackList,
	struct grp *group)
/* Find named track that is in group (NULL for any group).
 * Return NULL if can't find it. */
{
struct trackDb *track;
if (group != NULL && sameString(group->name, "all"))
    group = NULL;
for (track = trackList; track != NULL; track = track->next)
    {
    if (sameString(name, track->tableName) &&
       (group == NULL || sameString(group->name, track->grp)))
       return track;
    }
return NULL;
}

struct trackDb *findTrack(char *name, struct trackDb *trackList)
/* Find track, or return NULL if can't find it. */
{
return findTrackInGroup(name, trackList, NULL);
}

struct trackDb *mustFindTrack(char *name, struct trackDb *trackList)
/* Find track or squawk and die. */
{
struct trackDb *track = findTrack(name, trackList);
if (track == NULL)
    {
    if (isCustomTrack(name))
        errAbort("Can't find custom track %s. "
	         "If it's been 8 hours since you accessed this track you "
		 "may just need to upload it again.", name);
    else
	errAbort("Track %s doesn't exist in database %s.", name, database);
    }
return track;
}

struct trackDb *findSelectedTrack(struct trackDb *trackList, 
	struct grp *group, char *varName)
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */
{
char *name = cartOptionalString(cart, varName);
struct trackDb *track = NULL;

if (name != NULL)
    track = findTrackInGroup(name, trackList, group);
if (track == NULL)
    {
    if (group == NULL || sameString(group->name, "all"))
        track = trackList;
    else
	{
	for (track = trackList; track != NULL; track = track->next)
	    if (sameString(track->grp, group->name))
	         break;
	if (track == NULL)
	    internalErr();
	}
    }
return track;
}

struct grp *makeGroupList(struct sqlConnection *conn, 
	struct trackDb *trackList, boolean allTablesOk)
/* Get list of groups that actually have something in them. */
{
struct sqlResult *sr;
char **row;
struct grp *groupsAll, *groupList = NULL, *group;
struct hash *groupsInTrackList = newHash(0);
struct hash *groupsInDatabase = newHash(0);
struct trackDb *track;

/* Stream throught track list building up hash of active groups. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInTrackList,track->grp))
        hashAdd(groupsInTrackList, track->grp, NULL);
    }

/* Scan through group table, putting in ones where we have data. */
groupsAll = hLoadGrps();
for (group = slPopHead(&groupsAll); group != NULL; group = slPopHead(&groupsAll))
    {
    if (hashLookup(groupsInTrackList, group->name))
	{
	slAddTail(&groupList, group);
	hashAdd(groupsInDatabase, group->name, group);
	}
    else
        grpFree(&group);
    }

/* Do some error checking for tracks with group names that are
 * not in database.  Just warn about them. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInDatabase, track->grp))
         warn("Track %s has group %s, which isn't in grp table",
	 	track->tableName, track->grp);
    }

/* Create dummy group for all tracks. */
AllocVar(group);
group->name = cloneString("allTracks");
group->label = cloneString("All Tracks");
slAddTail(&groupList, group);

/* Create another dummy group for all tables. */
if (allTablesOk)
    {
    AllocVar(group);
    group->name = cloneString("allTables");
    group->label = cloneString("All Tables");
    slAddTail(&groupList, group);
    }

hashFree(&groupsInTrackList);
hashFree(&groupsInDatabase);
return groupList;
}

static struct grp *findGroup(struct grp *groupList, char *name)
/* Return named group in list, or NULL if not found. */
{
struct grp *group;
for (group = groupList; group != NULL; group = group->next)
    if (sameString(name, group->name))
        return group;
return NULL;
}

struct grp *findSelectedGroup(struct grp *groupList, char *cgiVar)
/* Find user-selected group if possible.  If not then
 * go to various levels of defaults. */
{
char *defaultGroup = "genes";
char *name = cartUsualString(cart, cgiVar, defaultGroup);
struct grp *group = findGroup(groupList, name);
if (group == NULL)
    group = findGroup(groupList, defaultGroup);
if (group == NULL)
    group = groupList;
return group;
}


static void addTablesAccordingToTrackType(struct slName **pList, 
	struct hash *uniqHash, struct trackDb *track)
/* Parse out track->type and if necessary add some tables from it. */
{
char *trackDupe = cloneString(track->type);
if (trackDupe != NULL && trackDupe[0] != 0)
    {
    char *s = trackDupe;
    char *type = nextWord(&s);
    if (sameString(type, "wigMaf"))
        {
	char *wigTrack = trackDbSetting(track, "wiggle");
	if (wigTrack != NULL) 
	    {
	    struct slName *name = slNameNew(wigTrack);
	    slAddHead(pList, name);
	    hashAdd(uniqHash, wigTrack, NULL);
	    }
	}
    }
freez(&trackDupe);
}

struct slName *tablesForTrack(struct trackDb *track)
/* Return list of all tables associated with track. */
{
struct hash *uniqHash = newHash(8);
struct slName *name, *nameList = NULL;
struct joinerPair *jpList, *jp;
char *trackTable = track->tableName;

hashAdd(uniqHash, trackTable, NULL);
jpList = joinerRelate(allJoiner, database, trackTable);
for (jp = jpList; jp != NULL; jp = jp->next)
    {
    struct joinerDtf *dtf = jp->b;
    char buf[256];
    char *s;
    if (sameString(dtf->database, database))
	s = dtf->table;
    else
	{
	safef(buf, sizeof(buf), "%s.%s", dtf->database, dtf->table);
	s = buf;
	}
    if (!hashLookup(uniqHash, s))
	{
	hashAdd(uniqHash, s, NULL);
	name = slNameNew(s);
	slAddHead(&nameList, name);
	}
    }
slNameSort(&nameList);
name = slNameNew(trackTable);
slAddHead(&nameList, name);
addTablesAccordingToTrackType(&nameList, uniqHash, track);
hashFree(&uniqHash);
return nameList;
}

static char *findSelectedTable(struct sqlConnection *conn, struct trackDb *track, char *var)
/* Find selected table.  Default to main track table if none
 * found. */
{
if (track == NULL)
    return cartString(cart, var);
else if (isCustomTrack(track->tableName))
    return track->tableName;
else
    {
    struct slName *tableList = tablesForTrack(track);
    char *table = cartUsualString(cart, var, tableList->name);
    if (slNameInList(tableList, table))
	return table;
    return tableList->name;
    }
}



void addWhereClause(struct dyString *query, boolean *gotWhere)
/* Add where clause to query.  If already have a where clause
 * add 'and' to it. */
{
if (*gotWhere)
    {
    dyStringAppend(query, " and ");
    }
else
    {
    dyStringAppend(query, " where ");
    *gotWhere = TRUE;
    }
}

boolean htiIsPositional(struct hTableInfo *hti)
/* Return TRUE if hti looks like it's from a positional table. */
{
return hti->chromField[0] && hti->startField[0] && hti->endField[0];
}

char *getIdField(char *db, struct trackDb *track, char *table, 
	struct hTableInfo *hti)
/* Get ID field for table, or NULL if none.  FreeMem result when done */
{
char *idField = NULL;
if (hti != NULL && hti->nameField[0] != 0)
    idField = cloneString(hti->nameField);
else if (track != NULL)
    {
    struct hTableInfo *trackHti = getHti(db, track->tableName);
    if (hti != NULL && trackHti->nameField[0] != 0)
        {
	struct joinerPair *jp, *jpList;
	jpList = joinerRelate(allJoiner, db, track->tableName);
	for (jp = jpList; jp != NULL; jp = jp->next)
	    {
	    if (sameString(jp->a->field, trackHti->nameField))
	        {
		if ( sameString(jp->b->database, db) 
		  && sameString(jp->b->table, table) )
		    {
		    idField = cloneString(jp->b->field);
		    break;
		    }
		}
	    }
	joinerPairFreeList(&jpList);
	}
    }
return idField;
}

int countTableColumns(struct sqlConnection *conn, char *table)
/* Count columns in table. */
{
char *splitTable = chromTable(conn, table);
int count = sqlCountColumnsInTable(conn, splitTable);
freez(&splitTable);
return count;
}

static void doTabOutDb( char *db, char *table, 
	struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table. */
{
struct region *regionList = getRegions();
struct region *region;
struct hTableInfo *hti = NULL;
struct dyString *fieldSpec = newDyString(256);
struct hash *idHash = NULL;
int outCount = 0;
boolean isPositional;
boolean doIntersection;
int fieldCount;
int bedFieldsOffset, bedFieldCount;
char *idField;

hti = getHti(db, table);
idField = getIdField(db, curTrack, table, hti);

/* If they didn't pass in a field list assume they want all fields. */
if (fields != NULL)
    {
    dyStringAppend(fieldSpec, fields);
    fieldCount = countChars(fields, ',') + 1;
    }
else
    {
    dyStringAppend(fieldSpec, "*");
    fieldCount = countTableColumns(conn, table);
    }
bedFieldsOffset = fieldCount;

/* If can find id field for table then get
 * uploaded list of identifiers, create identifier hash
 * and add identifier column to end of result set. */
if (idField != NULL)
    {
    idHash = identifierHash();
    if (idHash != NULL)
	{
	dyStringAppendC(fieldSpec, ',');
	dyStringAppend(fieldSpec, idField);
	bedFieldsOffset += 1;
	}
    }
isPositional = htiIsPositional(hti);

/* If intersecting add fields needed to calculate bed as well. */
doIntersection = (anyIntersection() && isPositional);
if (doIntersection)
    {
    char *bedFields;
    bedSqlFieldsExceptForChrom(hti, &bedFieldCount, &bedFields);
    dyStringAppendC(fieldSpec, ',');
    dyStringAppend(fieldSpec, bedFields);
    freez(&bedFields);
    }

/* Loop through each region. */
for (region = regionList; region != NULL; region = region->next)
    {
    struct sqlResult *sr;
    char **row;
    int colIx, lastCol = fieldCount-1;
    char *filter = filterClause(db, table, region->chrom);

    sr = regionQuery(conn, table, fieldSpec->string, 
    	region, isPositional, filter);
    if (sr == NULL)
	continue;

    /* First time through print column names. */
    if (region == regionList)
        {
	if (filter != NULL)
	    hPrintf("#filter: %s\n", filter);
	hPrintf("#");
	for (colIx = 0; colIx < lastCol; ++colIx)
	    hPrintf("%s\t", sqlFieldName(sr));
	hPrintf("%s\n", sqlFieldName(sr));
	}
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (idHash == NULL || hashLookup(idHash, row[fieldCount]))
	    {
	    for (colIx = 0; colIx < lastCol; ++colIx)
		hPrintf("%s\t", row[colIx]);
	    hPrintf("%s\n", row[lastCol]);
	    ++outCount;
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;	/* No need to iterate across regions in this case. */
    freez(&filter);
    }

/* Do some error diagnostics for user. */
if (outCount == 0)
    explainWhyNoResults();
hashFree(&idHash);
}

void doTabOutTable( char *db, char *table, struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table. */
{
if (isCustomTrack(table))
    {
    struct trackDb *track = findTrack(table, fullTrackList);
    doTabOutCustomTracks(track, conn, fields);
    }
else
    {
    doTabOutDb(db, table, conn, fields);
    }
}

void doOutPrimaryTable(char *table, struct sqlConnection *conn)
/* Dump out primary table. */
{
if (anyIntersection())
    errAbort("Can't do all fields output when intersection is on. "
    "Please go back and select another output type, or clear the intersection.");
textOpen();
doTabOutTable(database, table, conn, NULL);
}

void doOutHyperlinks(char *table, struct sqlConnection *conn)
/* Output as genome browser hyperlinks. */
{
char *table2 = cartOptionalString(cart, hgtaIntersectTrack);
int outputPad = cartUsualInt(cart, hgtaOutputPad,0);
struct region *region, *regionList = getRegions();
char posBuf[64];
int count = 0;

htmlOpen("Hyperlinks to Genome Browser");
for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    struct bed *bedList, *bed;
    bedList = cookedBedList(conn, table, region, lm);
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	char *name;
        int start = max(0,bed->chromStart+1-outputPad);
        int end = min(hChromSize(bed->chrom),bed->chromEnd+outputPad);
	safef(posBuf, sizeof(posBuf), "%s:%d-%d",
		    bed->chrom, start, end);
	/* Construct browser anchor URL with tracks we're looking at open. */
	hPrintf("<A HREF=\"%s?%s", hgTracksName(), cartSidUrlString(cart));
	hPrintf("&db=%s", database);
	hPrintf("&position=%s", posBuf);
	hPrintf("&%s=%s", table, hTrackOpenVis(table));
	if (table2 != NULL)
	    hPrintf("&%s=%s", table2, hTrackOpenVis(table2));
	hPrintf("\" TARGET=_blank>");
	name = bed->name;
	if (bed->name == NULL)
	    name = posBuf;
	if (sameString(name, posBuf))
	    hPrintf("%s", posBuf);
	else
	    hPrintf("%s at %s", name, posBuf);
	hPrintf("</A><BR>\n");
	++count;
	}
    lmCleanup(&lm);
    }
if (count == 0)
    hPrintf("\n# No results returned from query.\n\n");
htmlClose();
}

void doLookupPosition(struct sqlConnection *conn)
/* Handle lookup button press.   The work has actually
 * already been done, so just call main page. */
{
doMainPage(conn);
}

void doTopSubmit(struct sqlConnection *conn)
/* Respond to submit button on top level page.
 * This basically just dispatches based on output type. */
{
char *output = cartString(cart, hgtaOutputType);
char *trackName = NULL;
char *table = cartString(cart, hgtaTable);
struct trackDb *track = NULL;
if (!sameString(curGroup->name, "allTables"))
    {
    trackName = cartString(cart, hgtaTrack);
    track = mustFindTrack(trackName, fullTrackList);
    }
if (sameString(output, outPrimaryTable))
    doOutPrimaryTable(table, conn);
else if (sameString(output, outSelectedFields))
    doOutSelectedFields(table, conn);
else if (sameString(output, outSequence))
    doOutSequence(conn);
else if (sameString(output, outBed))
    doOutBed(table, conn);
else if (sameString(output, outCustomTrack))
    doOutCustomTrack(table, conn);
else if (sameString(output, outGff))
    doOutGff(table, conn);
else if (sameString(output, outHyperlinks))
    doOutHyperlinks(table, conn);
else if (sameString(output, outWigData))
    doOutWigData(track, table, conn);
else if (sameString(output, outWigBed))
    doOutWigBed(track, table, conn);
else if (sameString(output, outGala))
    doOutGalaQuery(track, table, conn);
else if (sameString(output, outMaf))
    doOutMaf(track, table, conn);
else
    errAbort("Don't know how to handle %s output yet", output);
}

void dispatch(struct sqlConnection *conn)
/* Scan for 'do' variables and dispatch to appropriate page-generator.
 * By default head to the main page. */
{
struct hashEl *varList;
if (cartVarExists(cart, hgtaDoTest))
    doTest();
else if (cartVarExists(cart, hgtaDoTopSubmit))
    doTopSubmit(conn);
else if (cartVarExists(cart, hgtaDoSummaryStats))
    doSummaryStats(conn);
else if (cartVarExists(cart, hgtaDoSchema))
    doSchema(conn);
else if (cartVarExists(cart, hgtaDoIntersectPage))
    doIntersectPage(conn);
else if (cartVarExists(cart, hgtaDoClearIntersect))
    doClearIntersect(conn);
else if (cartVarExists(cart, hgtaDoIntersectMore))
    doIntersectMore(conn);
else if (cartVarExists(cart, hgtaDoIntersectSubmit))
    doIntersectSubmit(conn);
else if (cartVarExists(cart, hgtaDoPasteIdentifiers))
    doPasteIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoClearPasteIdentifierText))
    doClearPasteIdentifierText(conn);
/* Respond to clear within paste identifier page. */
else if (cartVarExists(cart, hgtaDoPastedIdentifiers))
    doPastedIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoUploadIdentifiers))
    doUploadIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoClearIdentifiers))
    doClearIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoFilterPage))
    doFilterPage(conn);
else if (cartVarExists(cart, hgtaDoFilterMore))
    doFilterMore(conn);
else if (cartVarExists(cart, hgtaDoFilterSubmit))
    doFilterSubmit(conn);
else if (cartVarExists(cart, hgtaDoClearFilter))
     doClearFilter(conn);
else if (cartVarExists(cart, hgtaDoSchemaTable))
    {
    doTableSchema( cartString(cart, hgtaDoSchemaDb), 
    	cartString(cart, hgtaDoSchemaTable), conn);
    }
else if (cartVarExists(cart, hgtaDoValueHistogram))
    doValueHistogram(cartString(cart, hgtaDoValueHistogram));
else if (cartVarExists(cart, hgtaDoValueRange))
    doValueRange(cartString(cart, hgtaDoValueRange));
else if (cartVarExists(cart, hgtaDoSelectFieldsMore))
    doSelectFieldsMore();
else if (cartVarExists(cart, hgtaDoPrintSelectedFields))
    doPrintSelectedFields();
else if ((varList = cartFindPrefix(cart, hgtaDoClearAllFieldPrefix)) != NULL)
    doClearAllField(varList->name + strlen(hgtaDoClearAllFieldPrefix));
else if ((varList = cartFindPrefix(cart, hgtaDoSetAllFieldPrefix)) != NULL)
    doSetAllField(varList->name + strlen(hgtaDoSetAllFieldPrefix));
else if (cartVarExists(cart, hgtaDoGenePredSequence))
    doGenePredSequence(conn);
else if (cartVarExists(cart, hgtaDoGenomicDna))
    doGenomicDna(conn);
else if (cartVarExists(cart, hgtaDoGetBed))
    doGetBed(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrackTb))
    doGetCustomTrackTb(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrackGb))
    doGetCustomTrackGb(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrackFile))
    doGetCustomTrackFile(conn);
else if (cartVarExists(cart, hgtaDoRemoveCustomTrack))
    doRemoveCustomTrack(conn);
else if (cartVarExists(cart, hgtaDoMainPage))
    doMainPage(conn);
else if (cartVarExists(cart, hgtaDoAllGalaQuery))
    doAllGalaQuery(conn);
else if (cartVarExists(cart, hgtaDoGetGalaQuery))
    doGetGalaQuery(conn);
else if (cartVarExists(cart, hgtaDoLookupPosition))
    doLookupPosition(conn);
else	/* Default - put up initial page. */
    doMainPage(conn);
cartRemovePrefix(cart, hgtaDo);
}

char *excludeVars[] = {"Submit", "submit", NULL};

void initGroupsTracksTables(struct sqlConnection *conn)
/* Get list of groups that actually have something in them. */
{
fullTrackList = getFullTrackList();
curTrack = findSelectedTrack(fullTrackList, NULL, hgtaTrack);
fullGroupList = makeGroupList(conn, fullTrackList, TRUE);
curGroup = findSelectedGroup(fullGroupList, hgtaGroup);
if (sameString(curGroup->name, "allTables"))
    curTrack = NULL;
curTable = findSelectedTable(conn, curTrack, hgtaTable);
}


void hgTables()
/* hgTables - Get table data associated with tracks and intersect tracks. 
 * Here we set up cart and some global variables, dispatch the command,
 * and put away the cart when it is done. */
{
struct sqlConnection *conn = NULL;

oldVars = hashNew(10);

/* Sometimes we output HTML and sometimes plain text; let each outputter 
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);

/* Set up global variables. */
allJoiner = joinerRead("all.joiner");
getDbAndGenome(cart, &database, &genome);
freezeName = hFreezeFromDb(database);
hSetDb(database);
conn = hAllocConn();

if (lookupPosition())
    {
    /* Init track and group lists and figure out what page to put up. */
    initGroupsTracksTables(conn);
    dispatch(conn);
    }

/* Save variables. */
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
pushCarefulMemHandler(1500000000);  /* enough for wiggle intersects on chr1 */
cgiSpoof(&argc, argv);
hgTables();
return 0;
}
