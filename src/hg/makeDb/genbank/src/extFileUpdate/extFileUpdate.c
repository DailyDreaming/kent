/* extFileUpdate - update gbSeq/gbExtFile to point to the latest data */
#include "common.h"
#include "options.h"
#include "hdb.h"
#include "gbVerb.h"
#include "gbIndex.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbEntry.h"
#include "gbGenome.h"
#include "gbProcessed.h"
#include "seqTbl.h"
#include "extFileTbl.h"
#include "raInfoTbl.h"
#include "gbSql.h"
#include "dbLoadOptions.h"
#include "dbLoadPartitions.h"
#include <signal.h>

static char const rcsid[] = "$Id: extFileUpdate.c,v 1.4 2006/03/16 21:36:46 markd Exp $";

/*
 * Algorithm:
 *  - foreach specified partition:
 *    - get list of all extFiles ids for this partition by matching file
 *      names.
 *    - select all gbSeqs in the partition that don't have extFile ids the set
 *      that was found.
 */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_INT},
    {"release", OPTION_STRING},
    {"srcDb", OPTION_STRING},
    {"type", OPTION_STRING},
    {"orgCat", OPTION_STRING},
    {"accPrefix", OPTION_STRING},
    {"dryRun", OPTION_BOOLEAN},
    {"gbdbGenBank", OPTION_STRING},
    {NULL, 0}
};

/* globals */
static boolean gStopSignaled = FALSE;  /* stop at the end of the current
                                        * partition */
static struct dbLoadOptions gOptions; /* options from cmdline and conf */
static char* gGbdbGenBank = NULL;     /* root file path to put in database */

static void sigStopSignaled(int sig)
/* signal handler that sets the stopWhenSafe flag */
{
gStopSignaled = TRUE;
}

static void checkForStop()
/* called at safe places to check for stop request from a signal */
{
if (gStopSignaled)
    {
    fprintf(stderr, "*** Stopped by SIGUSR1 request ***\n");
    exit(1);
    }
}

static void cleanExtFileTable(struct sqlConnection *conn)
/* clean up dangling references in the extFile table */
{
checkForStop();
gbVerbEnter(3, "cleaning extFileTbl");
extFileTblClean(conn, (gbVerbose >= 4));
gbVerbLeave(3, "cleaning extFileTbl");
}

static void mkGbdbPath(char *gbdbPath, char *relPath)
/* convert relative path to a gbdb path */
{
strcpy(gbdbPath, gGbdbGenBank);
strcat(gbdbPath, "/");
strcat(gbdbPath, relPath);
}

static void loadUpdateRaInfo(struct sqlConnection *conn,
                             struct extFileTbl* extFileTbl,
                             struct gbSelect *select,
                             struct raInfoTbl *rit)
/* load raInfo for an update of a partition */
{
unsigned cDnaFaId, pepFaId = 0;
char relPath[PATH_LEN], gbdbPath[PATH_LEN];

gbProcessedGetPath(select, "fa", relPath);
mkGbdbPath(gbdbPath, relPath);
if (fileExists(gbdbPath))
    {
    cDnaFaId = extFileTblGet(extFileTbl, conn, gbdbPath);
    if (gbProcessedGetPepFa(select, relPath))
        {
        mkGbdbPath(gbdbPath, relPath);
        if (fileExists(gbdbPath))
            pepFaId = extFileTblGet(extFileTbl, conn, gbdbPath);
        }
    gbProcessedGetPath(select, "ra.gz", relPath);
    raInfoTblRead(rit, relPath, cDnaFaId, pepFaId);
    }
}

static struct raInfoTbl *loadPartRaInfo(struct extFileTbl* extFileTbl,
                                        struct gbSelect *select)
/* load the ra files for all updates in a partition */
{
struct sqlConnection *conn = hAllocConn();
struct raInfoTbl *rit = raInfoTblNew();
struct gbSelect upSel = *select;

for (upSel.update = upSel.release->updates; upSel.update != NULL; upSel.update = upSel.update->next)
    loadUpdateRaInfo(conn, extFileTbl, &upSel, rit);

hFreeConn(&conn);
return rit;
}

static struct sqlResult *outdatedSeqQuery(struct sqlConnection *conn,
                                          struct gbSelect *select,
                                          struct extFileRef* extFiles)
/* start query for outdated sequences. Selects seq that are of the type
* in select and who's files are not in extFiles.  Note that extFiles
* can be NULL, if the table contains none of the current files. */
{
struct dyString *query = dyStringNew(0);
struct extFileRef* ef;
char *sep = "";
struct sqlResult *result;

dyStringPrintf(query, "select id, acc, version, type from gbSeq where (srcDb=\"%s\")",
               gbSrcDbName(select->release->srcDb));
if (select->release->srcDb == GB_REFSEQ)
    dyStringPrintf(query, " and ((type=\"mRna\") or (type=\"PEP\"))");
else
    dyStringPrintf(query, " and (type=\"%s\")", gbTypeName(select->type));
if (select->accPrefix != NULL)
    dyStringPrintf(query, " and (acc like \"%s%%\")", select->accPrefix);

if (extFiles != NULL)
    {
    dyStringPrintf(query, " and gbExtFile not in (");
    for (ef = extFiles; ef != NULL; ef = ef->next)
        {
        dyStringPrintf(query, "%s%d", sep, ef->extFile->id);
        sep = ",";
        }
    dyStringPrintf(query, ")");
    }
result = sqlGetResult(conn, query->string);
dyStringFree(&query);
return result;
}

static int checkGbStatusVer(char *acc)
/* check if acc is in gbStatus and get the version. return 0 if not in
 * status */
{
struct sqlConnection *conn = hAllocConn();
int ver;
char query[256];
safef(query, sizeof(query), "select version from gbStatus where acc=\"%s\"",
      acc);
ver = sqlQuickNum(conn, query);
hFreeConn(&conn);
return ver;
}

static void dropSeqEntry(struct seqTbl *seqTbl, unsigned seqId, char *acc, unsigned version, char *type)
/* drop an unexpected seq entry */
{
fprintf(stderr, "Warning: %s %s.%d not expected in gbSeqTbl, dropping\n",
        type, acc, version);
seqTblDelete(seqTbl, acc);
}

static struct raInfo *getRaInfo(struct raInfoTbl *rit, char *acc, unsigned version, char *type)
/* get RA info entry, or NULL if it can't be resolved */
{
struct raInfo *ri = NULL;
char accVer[GB_ACC_BUFSZ];
int statVer;
safef(accVer, sizeof(accVer), "%s.%d", acc, version);

ri = raInfoTblGet(rit, accVer);
if (ri != NULL)
    return ri;

/* there is a bug where if a sequence version is updated twice on the same
 * day, then gbExtFile can end up with the old version.  Check for this
 * and update gbSeq version if it occured */
statVer = checkGbStatusVer(acc);
if (statVer != 0)
    {
    fprintf(stderr, "Warning: %s %s.%d in gbSeqTbl, %s.%d in gbStatus, updating to new version\n",
            type, acc, version, acc, statVer);
    version = statVer;
    safef(accVer, sizeof(accVer), "%s.%d", acc, statVer);
    ri = raInfoTblGet(rit, accVer);
    if (ri != NULL)
        return ri;
    fprintf(stderr, "Warning: %s %s.%d in gbSeqTbl, %s.%d in gbStatus, neither ared in ra file, unchanged\n",
            type, acc, version, acc, statVer);
    }
else
    {
    fprintf(stderr, "Warning: %s %s.%d in seqTbl but not in a ra file, unchanged\n",
            type, acc, version);
    }
return NULL;
}

static void updateSeqEntry(struct raInfoTbl *rit, struct seqTbl *seqTbl,
                           unsigned seqId, char *acc, unsigned version, char *type)
/* add update for a given seq entry, skip with warning if not found in
 * table. */
{
struct raInfo *ri = NULL;
if (startsWith("YP_", acc))
    dropSeqEntry(seqTbl, seqId, acc, version, type);
else
    ri = getRaInfo(rit, acc, version, type);
if (ri != NULL)
    {
    gbVerbPr(4, "updateSeq %d %s.%d: ext: %d", seqId, acc, ri->version, ri->extFileId);
    seqTblMod(seqTbl, seqId, ri->version, ri->extFileId,
              ri->size, ri->offset, ri->fileSize);
    }
}

static void extFileUpdatePart(struct sqlConnection *conn, 
                              struct gbSelect *select,
                              struct extFileTbl* extFileTbl)
/* update gbSeq/gbExtFile entries for one partition of the database */
{
/* FIXME: hardcoded tmp*/
struct seqTbl *seqTbl;
struct extFileRef* extFiles;
struct raInfoTbl *rit = NULL;  /* lazy creation */
struct sqlResult *result;
char **row;
gbVerbEnter(3, "update %s", gbSelectDesc(select));
seqTbl = seqTblNew(conn, "/var/tmp", (gbVerbose >= 4));
extFiles = extFileTblMatch(extFileTbl, select);
result = outdatedSeqQuery(conn, select, extFiles);

while ((row = sqlNextRow(result)) != NULL)
    {
    if (rit == NULL)
        rit = loadPartRaInfo(extFileTbl, select);
    updateSeqEntry(rit, seqTbl, sqlUnsigned(row[0]), row[1], sqlUnsigned(row[2]), row[3]);
    }
sqlFreeResult(&result);
raInfoTblFree(&rit);
slFreeList(&extFiles);
if (gOptions.flags & DBLOAD_DRY_RUN)
    seqTblCancel(seqTbl);
else
    seqTblCommit(seqTbl, conn);
seqTblFree(&seqTbl);
gbVerbLeave(3, "update %s", gbSelectDesc(select));
checkForStop();
}

static void extFileUpdateDb(char *db)
/* update gbSeq/gbExtFile entries for one database */
{
struct sqlConnection *conn;
struct gbIndex* index;
struct gbSelect* selectList, *select;
struct extFileTbl* extFileTbl;

checkForStop();

gbVerbEnter(1, "extFileUpdate %s", db);
gOptions = dbLoadOptionsParse(db);
hgSetDb(db);
conn = hAllocConn();
gbLockDb(conn, NULL);
index = gbIndexNew(db, NULL);
selectList = dbLoadPartitionsGet(&gOptions, index);
extFileTbl = extFileTblLoad(conn);

for (select = selectList; select != NULL; select = select->next)
    extFileUpdatePart(conn, select, extFileTbl);

slFreeList(&selectList);
cleanExtFileTable(conn);
gbUnlockDb(conn, NULL);
hFreeConn(&conn);
gbVerbLeave(1, "extFileUpdate %s", db);
}


void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("Error: %s\n"
  "extFileUpdate - update gbSeq/gbExtFile to point to the latest data\n"
  "usage:\n"
  "   extFileUpdate [options] db1 [db2 ...]\n"
  "Options:\n"
  "     -release=relname - Just load release name (e.g. genbank.131.0),\n"
  "      If not specified, the newest aligned genbank and refseq is loaded.\n"
  "\n"
  "     -srcDb=srcDb - Just load srcDb (genbank or refseq).\n"
  "\n"
  "     -type=type - Just load type (mrna or est).\n"
  "\n"
  "     -orgCat=orgCat - Just load organism restriction (native or xeno).\n"
  "\n"
  "     -accPrefix=ac - Only process ESTs with this 2 char accession prefix.\n"
  "      Must specify -type=est, mostly useful for debugging.\n"
  "\n"
  "     -dryRun - go throught the selection process,  but don't update.\n"
  "      This will still remove ignored accessions.\n"
  "\n"
  "     -gbdbGenBank=dir - set gbdb path to dir, default /gbdb/genbank\n"
  "\n"
  "     -verbose=n - enable verbose output, values greater than 1 increase \n"
  "                  verbosity\n"
  "SIGUSR1 will cause process to stop after the current partition.  This\n"
  "will leave the database in a clean state, but the update incomplete.\n",
  msg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct sigaction sigSpec;
int argi;
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage("wrong number of arguments");
gbVerbInit(optionInt("verbose", 0));
if (gbVerbose >= 6)
    sqlMonitorEnable(JKSQL_TRACE);
gGbdbGenBank = optionVal("gbdbGenBank", "/gbdb/genbank");
ZeroVar(&sigSpec);
sigSpec.sa_handler = sigStopSignaled;
sigSpec.sa_flags = SA_RESTART;
if (sigaction(SIGUSR1, &sigSpec, NULL) < 0)
    errnoAbort("can't set SIGUSR1 handler");

for (argi = 1; argi < argc; argi++)
    extFileUpdateDb(argv[argi]);
return 0;
}
