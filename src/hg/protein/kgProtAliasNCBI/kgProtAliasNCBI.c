/* kgProtAliasNCBI - generate alias list table for NCBI protein AC */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgProtAliasNCBI - create gene alias (mRNA part) .tab files "
  "usage:\n"
  "   kgProtAliasNCBI xxxx yyyy\n"
  "            xxxx is genome  database name\n"
  "example: kgProtAliasNCBI hg15\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;

char query[256], query2[256], query5[256];
struct sqlResult *sr, *sr2, *sr5;
char **row, **row2, **row5;
char *r1, *r2, *r3, *r5;
    
char *chp0, *chp;
char *kgID;
FILE *o1, *o2;
char cond_str[256];
char *database;

char *proteinID;
char *proteinAC;

if (argc != 2) usage();
database  = cloneString(argv[1]);

conn = hAllocConn();
conn2= hAllocConn();
o2 = fopen("jj.dat", "w");

sprintf(query2,"select name, proteinID from %s.knownGene;", database);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    kgID = row2[0];
    proteinID = row2[1];

    if (strstr(kgID, "NM_") != NULL)
	{
	sprintf(query,"select protAcc from %s.refLink where mrnaAcc = '%s';", database, kgID);
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	while (row != NULL)
    	    {
    	    proteinAC = row[0];
	    fprintf(o2, "%s\t%s\t%s\n", kgID, proteinID, proteinAC);
	    row = sqlNextRow(sr);
	    }
    	sqlFreeResult(&sr);
	}
    else
	{
	sprintf(query,"select proteinAC from %sTemp.locus2Acc0 where gbAC like '%s%c';", database, kgID, '%');
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	while (row != NULL)
    	    {
    	    proteinAC = row[0];

	    chp = strstr(proteinAC, ".");
	    if (chp != NULL)
		{
		*chp = '\0';
		}
	    if (proteinAC[0] != '-')
		{
		fprintf(o2, "%s\t%s\t%s\n", kgID, proteinID, proteinAC);
		}
	    row = sqlNextRow(sr);
	    }
    	sqlFreeResult(&sr);
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
fclose(o2);

system("cat jj.dat|sort|uniq  >kgProtAliasNCBI.tab");
system("rm jj.dat");
    
return(0);
}

