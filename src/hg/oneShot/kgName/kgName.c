/* kgName - builds association table between knownPep and gene common name. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"

char *database = "hg16";
void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgName - builds association table between knownPep and gene common name\n"
  "usage:\n"
  "   kgName database idList outAssoc\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *protDbName = NULL;

char* lookupName(  struct sqlConnection *conn , char *id)
{
char query[256];
char *newName;
char *seqType;
char *refSeqName;
char *proteinID;
char *hugoID;
char cond_str[256];
char *name = id;

if (hTableExists("refLink") && hTableExists("knownGeneLink"))
    {
    struct sqlResult *sr;
    char **row;

    sprintf(cond_str, "name='%s' and seqType='g'", id);
    seqType = sqlGetField(conn, database, "knownGeneLink", "seqType", cond_str);

    if (seqType != NULL)
	{
	// special processing for RefSeq DNA based genes
	sprintf(cond_str, "mrnaAcc = '%s'", id);
	refSeqName = sqlGetField(conn, database, "refLink", "name", cond_str);
	if (refSeqName != NULL)
	    {
	    name=  cloneString(refSeqName);
	    }
	}
    else if (protDbName != NULL)
	{
	sprintf(cond_str, "mrnaID='%s'", id);
	proteinID = sqlGetField(conn, database, "spMrna", "spID", cond_str);

	sprintf(cond_str, "displayID = '%s'", proteinID);
	hugoID = sqlGetField(conn, protDbName, "spXref3", "hugoSymbol", cond_str);
	if (!((hugoID == NULL) || (*hugoID == '\0')) )
	    {
	    name= cloneString(hugoID);
	    }
	else
	    {
	    sprintf(query,"select refseq from %s.mrnaRefseq where mrna = '%s';",  
		    database, id);

	    sr = sqlGetResult(conn, query);
	    row = sqlNextRow(sr);
	    if (row != NULL)
		{
		sprintf(query, "select * from refLink where mrnaAcc = '%s'", row[0]);
		sqlFreeResult(&sr);
		sr = sqlGetResult(conn, query); 
		if ((row = sqlNextRow(sr)) != NULL)
		    {
		    if (strlen(row[0]) > 0)
			name= cloneString(row[0]);
		    }
		sqlFreeResult(&sr);
		}
	    else
		{
		sqlFreeResult(&sr);
		}
	    }
	}
    }
return name;
}

char *getSwiss( struct sqlConnection *conn , char *id)
{
char cond_str[256];

sprintf(cond_str, "mrnaID='%s'", id);
return sqlGetField(conn, database, "spMrna", "spID", cond_str);
}

void kgName(char *database, char *protDb,  char *inList, char *outPsl)
/* kgName - builds association table between knownPep and gene common name. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn2 = sqlConnect("swissProt");
char *words[1], **row;
FILE *f = mustOpen(outPsl, "w");
struct lineFile *lf = lineFileOpen(inList, TRUE);
int count = 0, found = 0;
char query[256];
while (lineFileRow(lf, words))
    {
    fprintf(f,"%s\t%s\t%s\n",words[0], lookupName(conn,words[0]), getSwiss(conn, words[0]));
    }
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
database=argv[1];
hSetDb(argv[1]);
protDbName = hPdbFromGdb(database);
kgName(argv[1], protDbName, argv[2], argv[3]);
return 0;
}
