/* hgLoadPsl - Load up a mySQL database with psl alignment tables. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "psl.h"
#include "xAli.h"
#include "hdb.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadPsl.c,v 1.20 2004/01/30 22:38:48 hartera Exp $";

unsigned pslCreateOpts = 0;
unsigned pslLoadOpts = 0;
boolean append = FALSE;
boolean exportOutput = FALSE;
char *clTableName = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadPsl - Load up a mySQL database with psl alignment tables\n"
  "usage:\n"
  "   hgLoadPsl database file1.psl ... fileN.psl\n"
  "This must be run in the same directory as the .psl files\n"
  "It will create a table for each psl file.\n"
  "options:\n"
  "   -table=tableName  Explicitly set tableName.  (Defaults to file name)\n"
  "   -tNameIx    add target name index\n"
  "   -xa         Include sequence info\n"
  "   -fastLoad   Browser will lock up if someone opens this table during loading\n"
  "   -onServer   This will speed things up if you're running in a directory that\n"
  "               the mysql server can access.\n"
  "   -export     create output in a manner similar to mysqlexport -T.\n"
  "      This create a sql script $table.sql and tab seperate file $table.txt\n"
  "      Useful when a psl is to be loaded in multiple databases or to avoid\n"
  "      writing an intermediate PSL file when used in a pipeline. The database is\n"
  "      is not loaded and db parameter is ignored\n"
  "   -append append data - don't drop table before loading\n"
  "   -nobin Repress binning");
}

void createTable(struct sqlConnection *conn, char* table)
{
char *sqlCmd = pslGetCreateSql(table, pslCreateOpts);
if (exportOutput)
    {
    char sqlFName[1024];
    FILE* fh;
    sprintf(sqlFName, "%s.sql", table);
    fh = mustOpen(sqlFName, "w");
    mustWrite(fh, sqlCmd, strlen(sqlCmd));
    carefulClose(&fh);
    }
else
    sqlRemakeTable(conn, table, sqlCmd);
freez(&sqlCmd);
}

void hgLoadPsl(char *database, int pslCount, char *pslNames[])
/* hgLoadPsl - Load up a mySQL database with psl alignment tables. */
{
int i;
char table[128];
char *pslName;
struct sqlConnection *conn = NULL;
char comment[256];

if (!exportOutput)
    conn = sqlConnect(database);

for (i = 0; i<pslCount; ++i)
    {
    char tabFile[1024];
    pslName = pslNames[i];
    printf("Processing %s\n", pslName);
    if (clTableName != NULL)
        strcpy(table, clTableName);
    else
	splitPath(pslName, NULL, table, NULL);
    if (!(pslCreateOpts & PSL_WITH_BIN))
        strcpy(tabFile, pslName);  /* not bin, load directly */
    else
        {
        FILE *f;
	struct lineFile *lf = NULL;
        if (exportOutput)
            sprintf(tabFile, "%s.txt", table);
        else
            strcpy(tabFile, "psl.tab");

	f = mustOpen(tabFile, "w");
	if (pslCreateOpts & PSL_XA_FORMAT)
	    {
	    struct xAli *xa;
	    char *row[23];
	    lf = lineFileOpen(pslName, TRUE);
	    while (lineFileRow(lf, row))
	        {
		xa = xAliLoad(row);
		fprintf(f, "%u\t", hFindBin(xa->tStart, xa->tEnd));
		xAliTabOut(xa, f);
		xAliFree(&xa);
		}
	    }
	else
	    {
	    struct psl *psl;
	    lf = pslFileOpen(pslName);
	    while ((psl = pslNext(lf)) != NULL)
		{
		fprintf(f, "%u\t", hFindBin(psl->tStart, psl->tEnd));
		pslTabOut(psl, f);
		pslFree(&psl);
		}
	    lineFileClose(&lf);
	    }
	carefulClose(&f);
        }
    if (!append)
        createTable(conn, table);
    if (!exportOutput)
        {
        sqlLoadTabFile(conn, tabFile, table, pslLoadOpts);
        /* add a comment and ids to the history table */
        safef(comment, sizeof(comment), "Add psl alignments to %s table", table);
        hgHistoryComment(conn, comment);
        }
    }

if (conn != NULL)
    sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (optionExists("tNameIx"))
    pslCreateOpts |= PSL_TNAMEIX;
if (!optionExists("nobin"))
    pslCreateOpts |= PSL_WITH_BIN;
if (optionExists("xa"))
    pslCreateOpts |= PSL_XA_FORMAT;
if (!optionExists("fastLoad"))
    pslLoadOpts |= SQL_TAB_FILE_CONCURRENT;
if (optionExists("onServer"))
    pslLoadOpts |= SQL_TAB_FILE_ON_SERVER;
clTableName = optionVal("table", NULL);
append = optionExists("append");
exportOutput = optionExists("export");
if ((!(pslCreateOpts & PSL_WITH_BIN)) && exportOutput)
    errAbort("-nobin not supported with -export\n");
if (argc < 3)
    usage();
/* force progress message out immediatly */
setlinebuf(stdout);
setlinebuf(stderr);
hgLoadPsl(argv[1], argc-2, argv+2);
return 0;
}
