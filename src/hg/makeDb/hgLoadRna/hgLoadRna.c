/* hgLoadRna - load browser database with mRNA/EST info.. */

/* This is derived from a system that used to apply to the whole
 * database, but currently only applies to the mRNA/EST bits -
 * where each record has a globally uniq ID across the entire
 * database.  
 *
 * This system ended up being more complex than it was worth
 * in my judgement.  We couldn't simply suck in a tab-separated
 * file as a table without haveing to put in an extra ID field.
 *
 * However the system provides some useful services for the
 * mRNA/EST data.  In particular this data consists of many
 * fields such as library, tissue, which have values shared
 * by many records.  The "uniqTable" routines and the
 * id's in the "history table" that they depend upone
 * let us store each unique value once, and then just
 * reference these values in the larger records. */

#include "common.h"
#include "cheapcgi.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "hgRelate.h"

static char const rcsid[] = "$Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/hgLoadRna/Attic/hgLoadRna.c,v 1.27 2003/06/30 20:41:33 kate Exp $";

/* Command line options and defaults. */
char *abbr = NULL;
boolean noDbLoad = FALSE;
boolean ignore = FALSE;
boolean xenoDescriptions = FALSE;
boolean test = FALSE;

char historyTable[] =	
/* This contains a row for each update made to database.
 * (The idea is that this is just updated in batch.)
 * It keeps track of which id global ids are used
 * as well as providing a record of updates. */
"create table history ("
  "ix int not null auto_increment primary key,"  /* Update number. */
  "startId int unsigned not null,"              /* Start this session's ids. */
  "endId int unsigned not null,"                /* First id for next session. */
  "who varchar(255) not null,"         /* User who updated. */
  "what varchar(255) not null,"        /* What they did. */
  "modTime timestamp not null)";        /* Modification time. */

char extFileTable[] =
/* This keeps track of external files and directories. */
"create table extFile ("
  "id int unsigned not null primary key,"  /* Unique ID across all tables. */
  "name varchar(64) not null,"	  /* Symbolic name of file.  */
  "path varchar(255) not null,"   /* Full path. Ends in '/' if a dir. */
  "size bigint unsigned not null,"           /* Size of file (checked) */
                   /* Extra indices. */
  "index (name))";

char seqTable[] =
/* This keeps track of a sequence. */
"create table seq ("
  "id int unsigned not null primary key," /* Unique ID across all tables. */
  "acc varchar(24) not null ,"	 /* GenBank accession number or other ID. */
  "size int unsigned not null,"           /* Size of sequence in bases. */
  "gb_date date not null,"       /* GenBank last modified date. */
  "extFile int unsigned not null,"       /* File it is in. */
  "file_offset bigint not null,"         /* Offset in file. */
  "file_size int unsigned not null,"      /* Size in file. */
	       /* Extra indices. */
  "unique (acc))";

char mrnaTable[] =
/* This keeps track of mRNA. */
"create table mrna ("
  "id int unsigned not null primary key,"          /* Id, same as seq ID. */
  "acc char(12) not null,"		  /* Genbank accession. */
  "version char(12) not null,"		  /* Genbank version. */
  "type enum('EST','mRNA') not null,"	  /* Full length or EST. */
  "direction enum('5','3','0') not null," /* Read direction. */
  "source int unsigned not null,"	 	  /* Ref in source table. */
  "organism int unsigned not null," 		  /* Ref in organism table. */
  "library int unsigned not null,"		  /* Ref in library table. */
  "mrnaClone int unsigned not null,"              /* Ref in clone table. */
  "sex int unsigned not null,"                     /* Ref in sex table. */
  "tissue int unsigned not null,"                  /* Ref in tissue table. */
  "development int unsigned not null,"             /* Ref in development table. */
  "cell int unsigned not null,"                    /* Ref in cell table. */
  "cds int unsigned not null,"	                  /* Ref in CDS table. */
  "keyword int unsigned not null,"                /* Ref in key table. */
  "description int unsigned not null,"            /* Ref in description table. */
  "geneName int unsigned not null,"               /* Ref in geneName table. */
  "productName int unsigned not null,"            /* Ref in productName table. */
  "author int unsigned not null,"                 /* Ref in author table. */
	   /* Extra indices. */
  "unique (acc),"
  "unique (acc, version),"
  "index (type),"
  "index (library),"
  "index (mrnaClone),"
  "index (tissue),"
  "index (development),"
  "index (cell),"
  "index (keyword),"
  "index (description),"
  "index (geneName),"
  "index (productName),"
  "index (author))"
  ;

char *uniqueTableNames[] =
    {
    "development", "cell", "cds", "geneName", "productName",
    "source", "organism", "library", "mrnaClone", "sex", "tissue",
    "author", "keyword", "description",
    };

struct uniqueTable
/* Help manage a table that is simply unique. */
    {
    struct uniqueTable *next;
    char *tableName;
    char *raField;
    struct hash *hash;
    HGID curId;
    FILE *tabFile;
    };

void checkForGenBankIncr(char *database, char *funcMsg)
/* check to see if the database contains tables created by the incremental
 * genbank update process, and abort with useful message if so */
{
static char *CHK_TABLES[] = {
    "gbStatus", "gbSeq", "gbExtFile", NULL
};
int i;
struct sqlConnection *conn = sqlConnect(database);

for (i = 0; CHK_TABLES[i] != NULL; i++)
    {
    if (sqlTableExists(conn, CHK_TABLES[i]))
        errAbort("Table %s.%s exists, indicating that this database is managed\n"
                 "by the GenBank incremental update process.  This program should not be used\n"
                 "with this database.  %s",
                 database, CHK_TABLES[i], funcMsg);
    }

sqlDisconnect(&conn);
}

void createUniqTable(struct sqlConnection *conn, char *tableName)
/* Create a simple ID/Name pair table. */
{
char query[256];
sprintf(query,
    "create table %s (" 
       "id int not null primary key,"
       "name longtext not null,"
       "index (name(16)))",
    tableName); 
sqlUpdate(conn, query);
}

struct uniqueTable *uniqueTableList = NULL;

void clearUniqueIds()
/* Clear curId from all unique table entries. */
{
struct uniqueTable *uni;
for (uni = uniqueTableList; uni != NULL; uni = uni->next)
    uni->curId = 0;
}

void storeUniqueTables(struct sqlConnection *conn)
/* Close tab files, load tab files, free hashs from unique tables. */
{
struct uniqueTable *uni;
char *table;

for (uni = uniqueTableList; uni != NULL; uni = uni->next)
    {
    carefulClose(&uni->tabFile);
    table = uni->tableName;
    hgLoadTabFile(conn, ".", table, NULL);
    freeHash(&uni->hash);
    }
}

static char *nullPt = NULL;

struct uniqueTable *getUniqueTable(struct sqlConnection *conn, char *tableName, char *raField)
/* Return a new unique table.  Create it in database if it doesn't exist.  Load
 * up hash table with current values. */
{
struct uniqueTable *uni;
struct sqlResult *sr;
char **row;
char query[256];
struct hash *hash;
struct hashEl *hel;
int count = 0;

AllocVar(uni);
uni->tableName = tableName;
uni->hash = hash = newHash(0);
uni->raField = raField;
sprintf(query, "select id,name from %s", tableName);
sr = sqlGetResult(conn,query);
if (sr != NULL)
    {
    printf("Loading old %s values\n", tableName);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	HGID id = sqlUnsigned(row[0]);
	char *name = row[1];
        if (!test)
            hel = hashAdd(hash, name, nullPt + id);
	++count;
	}
    }
sqlFreeResult(&sr);
slAddHead(&uniqueTableList, uni);
uni->tabFile = hgCreateTabFile(".", tableName);
return uni;
}

HGID uniqueStore(struct sqlConnection *conn, struct uniqueTable *uni, char *name)
/* Store name in unique table.  Return id associated with name. */
{
struct hash *hash = uni->hash;
struct hashEl *hel = hashLookup(hash, name);

if (hel != NULL)
    {
    return (char *)(hel->val) - nullPt;
    }
else
    {
    HGID id = hgNextId();
    hashAdd(hash, name, nullPt + id);
    fprintf(uni->tabFile, "%u\t%s\n", id, name);
    return id;
    }
}

struct extFile
/* This stores info on an external file. */
    {
    HGID id;
    char *name;
    char *path;
    off_t size;
    };

struct hash *extFilesHash = NULL;

void loadExtFilesTable(struct sqlConnection *conn)
/* Convert external file table into extFilesHash. */
{
struct sqlResult *sr;
struct extFile *ex;
struct hash *hash;
struct hashEl *hel;
char **row;
char *name;
char lastChar;
int len;
off_t gotSize = 0;

if (extFilesHash == NULL)
    {
    extFilesHash = hash = newHash(8);
    sr = sqlGetResult(conn,"select id,name,path,size from extFile");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	AllocVar(ex);
	ex->id = sqlUnsigned(row[0]);
	hel = hashAdd(hash, row[1], ex);
	ex->name = name = hel->name;
	ex->path = cloneString(row[2]);
	ex->size = sqlLongLong(row[3]);
	len = strlen(name);
	lastChar = name[len-1];
        gotSize = fileSize(ex->path);
	if (lastChar != '/' && ex->size != gotSize)
	    {
            if(ignore) 
                {
                fprintf(stderr, "WARNING: External file %s out of sync.\n Expected size: %lld, got size %lld\n", ex->path, ex->size, gotSize);
                }
            else 
                {
                errAbort("ERROR: External file %s out of sync.\n Expected size: %ul, got size %ul\n", ex->path, ex->size, gotSize);
                }
	    }
	}
    sqlFreeResult(&sr);
    }
}

HGID hgStoreExtFilesTable(struct sqlConnection *conn, char *name, char *path)
/* Store name/path in external files table. */
{
struct hashEl *hel;
struct extFile *ex;

loadExtFilesTable(conn);
if ((hel = hashLookup(extFilesHash, name)) != NULL)
    {
    ex = hel->val;
    if (!sameString(ex->path, path))
	errAbort("external file %s path mismatch\n%s vs. %s\n", name, path, ex->path);
    return ex->id;
    }
else
    {
    char query[512];
    HGID id;
    long long size = fileSize(path);
    AllocVar(ex);
    ex->id = id = hgNextId();
    hel = hashAdd(extFilesHash, name, ex);
    ex->name = hel->name;
    ex->path = cloneString(path);
    ex->size = size;
    sprintf(query, "INSERT into extFile VALUES(%u,'%s','%s',%lld)",
	id, name, path, size);
    sqlUpdate(conn,query);
    return id;
    }
}

boolean faSeekNextRecord(struct lineFile *faLf)
/* Seeks to the next FA record.  Returns FALSE if seeks to EOF. */
{
char *faLine;
int faLineSize;
while (lineFileNext(faLf, &faLine, &faLineSize))
    {
    if (faLine[0] == '>')
	return TRUE;
    }
return FALSE;
}

void gbDateToSqlDate(char *gbDate, char *sqlDate)
/* Convert genbank to SQL date. */
{
static char *months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", };
int monthIx;
char *day = gbDate;
char *month = gbDate+3;
char *year = gbDate+7;

gbDate[2] = gbDate[6] = 0;
for (monthIx = 0; monthIx<12; ++monthIx)
    {
    if (sameString(months[monthIx], month))
	break;
    }
if (monthIx == 12)
    errAbort("Unrecognized month %s", month);
sprintf(sqlDate, "%s-%02d-%s", year, monthIx, day);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadRna - load browser database with mRNA/EST info.\n"
  "usage:\n"
  "   hgLoadRna new database\n"
  "This creates freshly the RNA part of the database\n"
  "   hgLoadRna add [-type=type] [-mrnaType=type] [-noDbLoad] database /full/path/mrna.fa mrna.ra [-ignore]\n"
  "      type can be mRNA, EST, xenoRNA or whatever goes into extFile.name\n"
  "      The type for the mrna table (mRNA or EST) will be guessed from type.\n"
  "      If it can't be guessed, it must be specified with -mrnaType=\n"
  "This adds mrna info to the database\n"
  "   hgLoadRna drop database\n"
  "This drops the tables created by hgLoadRna from database.\n"
  "   hgLoadRna addSeq [-abbr=junk] database file(s).fa [-ignore]\n"
  "This loads sequence files only, no auxiliarry info.  Typically\n"
  "these are more likely to be mouse reads than rna actually....\n"
  "The -ignore flag tells hgLoadRna not to validate the contents of the extFile table.\n"
  "It is only to be used if a normal load fails.\n"
  "Please notify someone responsible if an error is flagged.\n"
  "The -noDbLoad flag tells hgLoadRna to suppress loading the database.\n"
"            HgLoadRna will exit after generating the files (<table>.tab)\n"
  "The -test flag suppresses reading from and loading database.\n"
  "The -xenoDescriptions flag causes loading of xenoRna descriptions.\n"
  "          This is useful for non-model organisms with few mrna's.\n"
  );
}

void addRna(char *faName, char *symFaName, char *raName, char *type, char *mrnaType)
/* Add in RNA data from fa and ra files. */
{
struct sqlConnection *conn = hgStartUpdate();
struct hash *raFieldHash = newHash(8);
struct lineFile *raLf, *faLf;
char *faLine;
int faLineSize;
char faAcc[32];
char nameBuf[512];
DNA *faDna;
long extFileId = 0;
boolean gotFaStart = FALSE;
char *raTag;
char *raVal;
int raLineSize;
int maxMod = 200;
int mod = maxMod;
int lineMaxMod = 50;
int lineMod = lineMaxMod;
int count = 0;
FILE *mrnaTab = hgCreateTabFile(".", "mrna");
FILE *seqTab = hgCreateTabFile(".", "seq");
struct uniqueTable *uniSrc, *uniOrg, *uniLib, *uniClo, *uniSex,
                   *uniTis, *uniDev, *uniCel, *uniCds, *uniGen,
		   *uniPro, *uniAut, *uniKey, *uniDef;

if (!noDbLoad && !test)
    {
    extFileId= hgStoreExtFilesTable(conn, symFaName, faName);
    }
hashAdd(raFieldHash, "src", uniSrc = getUniqueTable(conn, "source", "src"));
hashAdd(raFieldHash, "org", uniOrg = getUniqueTable(conn, "organism", "org"));
hashAdd(raFieldHash, "lib", uniLib = getUniqueTable(conn, "library", "lib"));
hashAdd(raFieldHash, "clo", uniClo = getUniqueTable(conn, "mrnaClone", "clo"));
hashAdd(raFieldHash, "sex", uniSex = getUniqueTable(conn, "sex", "sex"));
hashAdd(raFieldHash, "tis", uniTis = getUniqueTable(conn, "tissue", "tis"));
hashAdd(raFieldHash, "dev", uniDev = getUniqueTable(conn, "development", "dev"));
hashAdd(raFieldHash, "cel", uniCel = getUniqueTable(conn, "cell", "cel"));
hashAdd(raFieldHash, "cds", uniCds = getUniqueTable(conn, "cds", "cds"));
hashAdd(raFieldHash, "gen", uniGen = getUniqueTable(conn, "geneName", "gen"));
hashAdd(raFieldHash, "pro", uniPro = getUniqueTable(conn, "productName", "pro"));
hashAdd(raFieldHash, "aut", uniAut = getUniqueTable(conn, "author", "aut"));
hashAdd(raFieldHash, "key", uniKey = getUniqueTable(conn, "keyword", "key"));
if (sameWord(type, "mRNA") || (xenoDescriptions && sameWord(type, "xenoRNA")))
    {
    printf("Adding xeno descriptions\n");
    hashAdd(raFieldHash, "def", uniDef = getUniqueTable(conn, "description", "def"));
    }
else
    uniDef = getUniqueTable(conn, "description", "def");

/* Open input files. */
faLf = lineFileOpen(faName, TRUE);
raLf = lineFileOpen(raName, TRUE);

/* Seek to first line starting with '>' in line file. */
if (!faSeekNextRecord(faLf))
    errAbort("%s doesn't appear to be an .fa file\n", faName);
lineFileReuse(faLf);

/* Loop around for each record of FA and RA */
for (;;)
    {
    char dir = '0';
    boolean gotRaAcc = FALSE, gotRaDate = FALSE;
    HGID id;
    off_t faOffset, faEndOffset;
    int faSize;
    char *s;
    int faNameSize;
    char sqlDate[32];
    int dnaSize = 0;
    char version [32] = "X"; // Init to obvious value

    ++count;
    if (--mod == 0)
	{
	printf(".");
	fflush(stdout);
	mod = maxMod;
	if (--lineMod == 0)
	    {
	    printf("%d\n", count);
	    lineMod = lineMaxMod;
	    }
	}
    /* Get Next FA record. */
    if (!lineFileNext(faLf, &faLine, &faLineSize))
	break;
    if (faLine[0] != '>')
	internalErr();
    faOffset = faLf->bufOffsetInFile + faLf->lineStart;
    s = firstWordInLine(faLine+1);
    faNameSize = strlen(s);
    if (faNameSize == 0)
	errAbort("Missing accession line %d of %s", faLf->lineIx, faLf->fileName);
    if (strlen(faLine+1) >= sizeof(faAcc))
	errAbort("FA name too long line %d of %s", faLf->lineIx, faLf->fileName);
    strcpy(faAcc, s);
    if (faSeekNextRecord(faLf))
	lineFileReuse(faLf);
    faEndOffset = faLf->bufOffsetInFile + faLf->lineStart;
    faSize = (int)(faEndOffset - faOffset); 

    /* Get next RA record. */
    clearUniqueIds();
    for (;;)
	{
	struct hashEl *hel = NULL;

	if (!lineFileNext(raLf, &raTag, &raLineSize))
	    errAbort("Unexpected eof in %s", raName);

	if (raTag[0] == 0)
	    break;

        /* null out trailing \, if any */
        /* occasionally Genbank lines have this unfortunate format */
        /* DEBUG
        printf("raTag=%s, raLineSize=%d, raTag[raLineSize-2]=%c\n", 
                        raTag, raLineSize, raTag[raLineSize-2]);
        */
        if (raTag[raLineSize-2] == '\\')
            {
            raLineSize--;
            raTag[raLineSize-1] = 0;
            }

	raVal = strchr(raTag, ' ');
	if (raVal == NULL)
	    errAbort("Badly formatted tag line %d of %s", raLf->lineIx, raLf->fileName);
	*raVal++ = 0;
	if ((hel = hashLookup(raFieldHash, raTag)) != NULL)
	    {
	    struct uniqueTable *uni = hel->val;
	    uni->curId = uniqueStore(conn,uni,raVal);
	    }
	else if (sameString(raTag, "acc"))
	    {
	    gotRaAcc = TRUE;
	    s = firstWordInLine(raVal);
	    if (!sameString(s, faAcc))
		errAbort("Accession mismatch %s and %s between %s and %s",
		    faAcc, raVal, faName, raName);
	    }
	else if (sameString(raTag, "dir"))
	    {
	    dir = raVal[0];
	    }
	else if (sameString(raTag, "dat"))
	    {
	    gbDateToSqlDate(raVal, sqlDate);
	    gotRaDate = TRUE;
	    }
	else if (sameString(raTag, "siz"))
	    {
	    dnaSize = sqlUnsigned(raVal);
	    }
	else if (sameString(raTag, "ver"))
	    {	    
            strcpy(version, firstWordInLine(raVal));
            }
	}

    /* Do a little error checking and then write out to tables. */
    if (!gotRaAcc)
	errAbort("No accession in %s\n", raName);
    if (!gotRaDate)
	errAbort("No date in %s\n", faAcc);
    if (!dnaSize)
	errAbort("No size in %s\n", faAcc);
    id = hgNextId();

    fprintf(seqTab, "%u\t%s\t%d\t%s\t%lu\t%lld\t%d\n",
	id, faAcc, dnaSize, sqlDate, extFileId, faOffset, faSize);
    fprintf(mrnaTab, "%u\t%s\t%s\t%s\t%c\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
            id, faAcc, version, mrnaType, dir,
            uniSrc->curId, uniOrg->curId, uniLib->curId, uniClo->curId,
            uniSex->curId, uniTis->curId, uniDev->curId, uniCel->curId,
            uniCds->curId, uniKey->curId, uniDef->curId, uniGen->curId, uniPro->curId, uniAut->curId);
    }
printf("%d\n", count);

if (noDbLoad || test) 
    {
    /* command-line option to suppress loading database */
    printf("Created tab files... exiting\n");
    exit(0);
    }
printf("Updating tissue, lib, etc. values\n");
storeUniqueTables(conn);
lineFileClose(&faLf);
lineFileClose(&raLf);
fclose(mrnaTab);
fclose(seqTab);
printf("Updating mrna table\n");
hgLoadTabFile(conn, ".", "mrna", NULL);
printf("Updating seq table\n");
hgLoadTabFile(conn, ".", "seq", NULL);
hgEndUpdate(&conn, "Add mRNA from %s,%s", faName, raName);
printf("All done\n");
}


void hgLoadRna(char *database, char *faPath, char *raFile)
/* hgLoadRna - load browser database with mRNA/EST info.. */
{
char *type, *symName, *mrnaType;

checkForGenBankIncr(database, "Use the GenBank incremental load tools.");

hgSetDb(database);

type = cgiOptionalString("type");
mrnaType = cgiOptionalString("mrnaType");
if (type == NULL)
    {
    if (strstrNoCase(raFile, "xenoRna"))
	type = "xenoRna";
    else if (strstrNoCase(raFile, "xenoEst"))
	type = raFile;
    else if (strstrNoCase(raFile, "est"))
	type = "EST";
    else
	type = "mRNA";
    }

if (mrnaType == NULL)
    {
    printf("guessing mrnatype\n");
    if (strstrNoCase(type, "rna") || strstrNoCase(type, "refseq"))
        mrnaType = "mRNA";
    else if (strstrNoCase(type, "est"))
        mrnaType = "EST";
    else
        errAbort("can't guess mrna type from \"%s\", specify with -mrnaType");
    }

printf("Adding data of type: %s, mrna type: %s\n", type, mrnaType);
addRna(faPath, type, raFile, type, mrnaType);
}

void abbreviate(char *s, char *fluff)
/* Cut out fluff from s. */
{
int len;
if (s != NULL && fluff != NULL)
    {
    s = strstr(s, fluff);
    if (s != NULL)
       {
       len = strlen(fluff);
       strcpy(s, s+len);
       }
    }
}

void hgLoadSeq(char *database, int fileCount, char *fileNames[])
/* Add a bunch of FA files to sequence and extFile tables of
 * database. */
{
struct sqlConnection *conn;
char *fileName;
int i, count=0;
DNA *faDna;
int faSize;
char *faLine;
int faLineSize;
FILE *seqTab;
struct lineFile *faLf;
int maxMod = 2000;
int mod = maxMod;
int lineMaxMod = 50;
int lineMod = lineMaxMod;
char symFaName[256+64], ext[64];
long extFileId;
char faAcc[64];

checkForGenBankIncr(database, "Use hgLoadSeq to load generic sequences.");

hgSetDb(database);
conn = hgStartUpdate();
seqTab = hgCreateTabFile(".", "seq");
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    splitPath(fileName, NULL, symFaName, ext);
    strcat(symFaName, ext);
    printf("Adding %s (%s)\n", fileName, symFaName);
    faLf = lineFileOpen(fileName, TRUE);
    extFileId = hgStoreExtFilesTable(conn, symFaName, fileName);


    /* Seek to first line starting with '>' in line file. */
    if (!faSeekNextRecord(faLf))
	errAbort("%s doesn't appear to be an .fa file\n", faLf->fileName);
    lineFileReuse(faLf);

    /* Loop around for each record of FA */
    for (;;)
	{
	HGID id;
	off_t faOffset, faEndOffset;
	int faSize;
	char *s;
	int faNameSize;
	static char sqlDate[32];
	int dnaSize = 0;

	++count;
	if (--mod == 0)
	    {
	    printf(".");
	    fflush(stdout);
	    mod = maxMod;
	    if (--lineMod == 0)
		{
		printf("%d\n", count);
		lineMod = lineMaxMod;
		}
	    }
	/* Get Next FA record. */
	if (!lineFileNext(faLf, &faLine, &faLineSize))
	    break;
	if (faLine[0] != '>')
	    internalErr();
	faOffset = faLf->bufOffsetInFile + faLf->lineStart;
	s = firstWordInLine(faLine+1);
	abbreviate(s, abbr);
	faNameSize = strlen(s);
	if (faNameSize == 0)
	    errAbort("Missing accession line %d of %s", faLf->lineIx, faLf->fileName);
	if (strlen(faLine+1) >= sizeof(faAcc))
	    errAbort("FA name too long line %d of %s", faLf->lineIx, faLf->fileName);
	strcpy(faAcc, s);
	if (faSeekNextRecord(faLf))
	    lineFileReuse(faLf);
	faEndOffset = faLf->bufOffsetInFile + faLf->lineStart;
	faSize = (int)(faEndOffset - faOffset); 
	id = hgNextId();

	fprintf(seqTab, "%u\t%s\t%d\t%s\t%lu\t%lld\t%d\n",
	    id, faAcc, dnaSize, sqlDate, extFileId, faOffset, faSize);
	}
    printf("%d\n", count);
    lineFileClose(&faLf);
    }
printf("Updating seq table\n");
fclose(seqTab);
hgLoadTabFile(conn, ".", "seq", NULL);
hgEndUpdate(&conn, "Add sequences");
printf("All done\n");
}

void createAll(char *database)
/* Create all tables in database. */
{
struct sqlConnection *conn = sqlConnect(database);
int i;
char *table;

checkForGenBankIncr(database, "Use the GenBank incremental load tools or hgLoadSeq.");

sqlUpdate(conn, historyTable);
sqlUpdate(conn, "INSERT into history VALUES(NULL,0,10,USER(),'New',NOW())");
sqlUpdate(conn, extFileTable);
sqlUpdate(conn, seqTable);
sqlUpdate(conn, mrnaTable);
for (i=0; i<ArraySize(uniqueTableNames); ++i)
    {
    char query[256];
    table = uniqueTableNames[i];
    createUniqTable(conn, table);
    sprintf(query, "INSERT into %s VALUES(0,'n/a')", table);
    sqlUpdate(conn, query);
    }
sqlDisconnect(&conn);
}

void dropAll(char *database)
/* Drop all hgLoadRna tables from database. */
{
struct sqlConnection *conn = sqlConnect(database);
char *table;
int i;

checkForGenBankIncr(database, "Use the GenBank incremental load tools.");

sqlUpdate(conn, "drop table history");
sqlUpdate(conn, "drop table extFile");
sqlUpdate(conn, "drop table seq");
sqlUpdate(conn, "drop table mrna");
for (i=0; i<ArraySize(uniqueTableNames); ++i)
    {
    char query[256];
    table = uniqueTableNames[i];
    sprintf(query, "drop table %s", table);
    sqlUpdate(conn, query);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;

cgiSpoof(&argc, argv);
if (argc < 2)
    {
    usage();
    }

abbr = cgiOptionalString("abbr");
ignore = (NULL != cgiOptionalString("ignore"));
noDbLoad = (NULL != cgiOptionalString("noDbLoad"));
test = cgiVarExists("test");
xenoDescriptions = cgiVarExists("xenoDescriptions");
command = argv[1];

if (sameString(command, "new"))
    {
    if (argc != 3)
        usage();
    createAll(argv[2]);
    }
else if (sameString(command, "drop"))
    {
    if (argc != 3)
        usage();
    dropAll(argv[2]);
    }
else if (sameString(command, "add"))
    {
    if (argc != 5)
        usage();
    hgLoadRna(argv[2], argv[3], argv[4]);
    }
else if (sameString(command, "addSeq"))
    {
    hgLoadSeq(argv[2], argc-3, argv+3);
    }
else
    usage();
return 0;
}
