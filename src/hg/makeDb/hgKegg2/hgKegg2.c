/* hgKegg2 - creates keggPathway.tab and keggMapDesc.tab files for KG links to KEGG Pathway Map */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgKegg2 - creates keggPathway.tab and keggMapDesc.tab files for KG links to KEGG Pathway Map"
  "usage:\n"
  "   hgKegg2 kgTempDb roDb\n"
  "      kgTempDb is the KG build temp database name\n"
  "      roDb is the read only genome database name\n"
  "example: hgKegg2 kgMm6ATemp mm6\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
char query[256], query2[256], query3[256];
struct sqlResult *sr, *sr2, *sr3;
char **row, **row2, **row3;

char *r1, *r2, *r3, *r4;
    
char *chp;
FILE *o1, *o2;

char *locusID;	/* LocusLink ID */
char *gbAC;		/* GenBank accession.version */
char *seqType;	/* sequence type m=mRNA g=genomic u=undefined */
char *proteinAC;	/* protein accession.version */
char *taxID;	/* tax id */
    
char *locusID2;	/* LocusLink ID */
char *refAC;	/* Refseq accession.version */
char *revStatus;	/* review status */
char *proteinAC2;	/* protein accession.version */
char *taxID2;	/* tax id */
char *kgTempDbName, *roDbName; 
char cond_str[200];
char *kgID;
char *mapID;
char *desc;

if (argc != 3)  usage();
kgTempDbName    = argv[1];
roDbName 	= argv[2];

conn = hAllocConn();
conn2= hAllocConn();
conn3= hAllocConn();

o1 = fopen("j.dat",  "w");
o2 = fopen("jj.dat", "w");
    
sprintf(query, "select kgID, refseq from %s.kgXref", roDbName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
while (row != NULL)
    {
    kgID  = row[0];
    refAC = row[1];
	
    sprintf(cond_str, "refseq='%s'", refAC);
    locusID = sqlGetField(conn3, "entrez", "entrezRefProt", "geneID", cond_str);
    if (locusID != NULL)
	{
        sprintf(query3, "select * from %s.keggList where locusID = '%s'", kgTempDbName, locusID);
        sr3 = sqlGetResult(conn3, query3);
        while ((row3 = sqlNextRow(sr3)) != NULL)
            {
            mapID   = row3[1];
	    desc    = row3[2];
	    fprintf(o1, "%s\t%s\t%s\n", kgID, locusID, mapID);fflush(o1);
	    fprintf(o2, "%s\t%s\n", mapID, desc);
	    row3 = sqlNextRow(sr3);
            }
        sqlFreeResult(&sr3);
	}
    row = sqlNextRow(sr);
    }

fclose(o1);
fclose(o2);
hFreeConn(&conn);
hFreeConn(&conn2);

system("cat j.dat|sort|uniq >keggPathway.tab");
system("cat jj.dat|sort|uniq >keggMapDesc.tab");
system("rm j.dat");
system("rm jj.dat");
return(0);
}

