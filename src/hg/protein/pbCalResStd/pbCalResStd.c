/* pbCalResStd- Calculate the avg frequency and standard deviation of each AA residue for the proteins in a specific genome*/

#define MAXN 100000
#define MAXRES 23

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"
#include "math.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pbCalResStd calculates the avg frequency and standard deviation of every AA residues of the proteins in a specific genome\n"
  "usage:\n"
  "   pbCalResStd sss xxx hhh\n"
  "      sss is the SWISS-PROT database name\n"
  "      xxx is the taxnomy number of the genome\n"
  "      sss is the genome database name\n"
  "Example: pbCalResStd sp031112 9606 hg16\n"
  );
}

double measure[MAXN];
double freq[MAXN][MAXRES];
double avg[MAXRES];
double sumJ[MAXRES];
double sigma[MAXRES];
double sumJ[MAXRES];

int recordCnt;
double recordCntDouble;
double lenDouble;
double sum;

int calDist(double *measure, int nInput, int nDist, double xMin, double xDelta, char *oFileName)
/* calculate histogram distribution of a double array of nInput elements */ 
{
int distCnt[1000];
double xDist[1000];
FILE *o3;
int i,j;
int highestCnt, totalCnt;

assert(nDist < ArraySize(distCnt));

o3 = mustOpen(oFileName, "w");
for (j=0; j<=nDist; j++)
    {
    distCnt[j] = 0;
    xDist[j] = xMin + xDelta * (double)j;
    }

for (i=0; i<nInput; i++)
    {
    if (measure[i] <= xDist[0])
	{
	distCnt[0]++;
	}
    for (j=1; j<nDist; j++)
	{
	if ((measure[i] > xDist[j-1]) && (measure[i] <= xDist[j]))
 	    {
	    distCnt[j]++;
	    }
	}
    if (measure[i] > xDist[nDist-1])
	{
	distCnt[nDist]++;
	}
    }

highestCnt = 0;
totalCnt   = 0;
for (j=0; j<=nDist; j++)
    {
    if (distCnt[j] > highestCnt) highestCnt = distCnt[j];
    totalCnt = totalCnt + distCnt[j];
    }
    
if (totalCnt != nInput)
    errAbort("nInput %d is not equal totalCnt %d, aborting ...\n", nInput, totalCnt);
   
// do not print out count of the last inteval, which is everything beyond xMax 
for (j=0; j<nDist; j++)
    {
    fprintf(o3, "%f\t%d\n", xDist[j], distCnt[j]);
    }
carefulClose(&o3);
   
return(highestCnt);
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;
char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
char *r1, *r2;
char cond_str[255];
char *proteinDatabaseName;
char emptyStr[1] = {""};
FILE *o1, *o2, *o3;
FILE *fh[23];
char temp_str[1000];;
char *accession;
char *aaSeq;
char *chp;
int i, j, len;
int ihi, ilow;
char *answer;
char *protDisplayId;
int aaResCnt[30];
double aaResCntDouble[30];
char aaAlphabet[30];
int aaResFound;
int totalResCnt;
float fvalue1, fvalue2;
float p1, p2;
int icnt, jcnt;
double aaLenDouble[100000];
char *taxon;
char *database;
int sortedCnt;

if (argc != 4) usage();

strcpy(aaAlphabet, "WCMHYNFIDQKRTVPGEASLXZB");

proteinDatabaseName = argv[1];
taxon = argv[2];
database = argv[3];

//o1 = mustOpen("resData.tab", "w");
o2 = mustOpen("pbResAvgStd.tab", "w");

for (i=0; i<20; i++)
    {
    sprintf(temp_str, "%c.txt", aaAlphabet[i]);
    fh[i] = mustOpen(temp_str, "w");
    }

conn  = hAllocConn();
conn2 = hAllocConn();

sprintf(query2,"select proteinID from %s.knownGene;", database);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
icnt = 0;
jcnt = 0;

for (j=0; j<MAXRES; j++)
    {
    sumJ[j] = 0;
    }

while (row2 != NULL)
    {
    protDisplayId = row2[0];   
  
    sprintf(cond_str, "val='%s'", protDisplayId);
    accession = sqlGetField(conn, proteinDatabaseName, "displayId", "acc", cond_str);

    if (accession == NULL)
	{
        sprintf(cond_str, "acc='%s'", protDisplayId);
    	accession = sqlGetField(conn, proteinDatabaseName, "displayId", "acc", cond_str);
	if (accession == NULL)
	    {
	    //printf("%s not found.\n", protDisplayId);fflush(stdout);
	    goto skip;
	    }
	}
    
    sprintf(cond_str, "accession='%s'", accession);
    answer = sqlGetField(conn, "proteins040115", "spXref2", "biodatabaseID", cond_str);
    if (answer[0] != '1')
	{
	//printf("%s not in SWISS-PROT\n", protDisplayId);fflush(stdout);
	goto skip;
	}
    
    sprintf(cond_str, "acc='%s'", accession);
    aaSeq = sqlGetField(conn, proteinDatabaseName, "protein", "val", cond_str);

    len  = strlen(aaSeq);
    if (len < 100) goto skip;

    lenDouble = (double)len;

    for (j=0; j<MAXRES; j++)
    	{
    	aaResCnt[j] = 0;
	}

    chp = aaSeq;
    for (i=0; i<len; i++)
	{
	aaResFound = 0;
	for (j=0; j<MAXRES; j++)
	    {
	    if (*chp == aaAlphabet[j])
		{
		aaResFound = 1;
		aaResCnt[j] ++;
		}
	    }
	if (!aaResFound)
	    {
	    fprintf(stderr, "%c %d not a valid AA residue.\n", *chp, *chp);
	    }
	chp++;
	}

    for (j=0; j<MAXRES; j++)
	{	
	freq[icnt][j] = (double)aaResCnt[j]/lenDouble;
	sumJ[j] = sumJ[j] + freq[icnt][j];
	}
    /*
    fprintf(o1, "%s\t%s\t%d", accession, protDisplayId, len);
    for (j=0; j<MAXRES; j++)
	{
	fprintf(o1, "\t%7.5f", freq[icnt][j]);
	}
    fprintf(o1, "\n");
    */

    for (j=0; j<20; j++)
	{
	fprintf(fh[j], "%15.7f\t%s\n", freq[icnt][j], accession);fflush(fh[j]);
	}
    icnt++;
skip:
    row2 = sqlNextRow(sr2);
    }

recordCnt = icnt;
recordCntDouble = (double)recordCnt;

for (j=0; j<20; j++)
    {
    carefulClose(&(fh[j]));
    }

//carefulClose(&o1);
sqlFreeResult(&sr2);
hFreeConn(&conn);
hFreeConn(&conn2);

for (j=0; j<MAXRES; j++)
    {
    avg[j] = sumJ[j]/recordCntDouble;
    }

for (j=0; j<20; j++)
    {
    sum = 0.0;
    for (i=0; i<recordCnt; i++)
     	{
	sum = sum + (freq[i][j] - avg[j]) * (freq[i][j] - avg[j]);
	}
    sigma[j] = sqrt(sum/(double)(recordCnt-1));
    fprintf(o2, "%c\t%f\t%f\n", aaAlphabet[j], avg[j], sigma[j]);
    }

//carefulClose(&o1);
carefulClose(&o2);

o1 = mustOpen("pbAnomLimit.tab", "w");
for (j=0; j<20; j++)
    {
    sprintf(temp_str, "cat %c.txt|sort|uniq > %c.srt",  aaAlphabet[j], aaAlphabet[j]);
    system(temp_str);

    // figure out how many unique entries 
    sprintf(temp_str, "wc %c.srt > %c.tmp",  aaAlphabet[j], aaAlphabet[j]);
    system(temp_str);
    sprintf(temp_str, "%c.tmp",  aaAlphabet[j]);
    o3 = mustOpen(temp_str, "r");
    fgets(temp_str, 1000, o3);
    chp = temp_str;
    while (*chp == ' ') chp++;
    while (*chp != ' ') chp++;
    *chp = '\0';
    sscanf(temp_str, "%d", &sortedCnt);
    sprintf(temp_str, "rm %c.tmp", aaAlphabet[j]);
    system(temp_str);

    // cal hi and low cutoff threshold
    ilow = (int)((float)sortedCnt * 0.025);
    ihi  = (int)((float)sortedCnt * 0.975);
    
    sprintf(temp_str, "%c.srt",  aaAlphabet[j]);
    o2 = mustOpen(temp_str, "r");
    i=0;
    for (i=0; i<ilow; i++)
	{
	fgets(temp_str, 1000, o2);
	}
    sscanf(temp_str, "%f", &fvalue1);

    fgets(temp_str, 1000, o2);
    sscanf(temp_str, "%f", &fvalue2);
    p1 = (fvalue1 + fvalue2)/2.0;

    for (i=ilow+1; i<ihi; i++)
	{
	fgets(temp_str, 1000, o2);
	}
    sscanf(temp_str, "%f", &fvalue1);

    fgets(temp_str, 1000, o2);
    sscanf(temp_str, "%f", &fvalue2);
    p2 = (fvalue1 + fvalue2)/2.0;
    carefulClose(&o2);

    fprintf(o1, "%c\t%f\t%f\n", aaAlphabet[j], p1, p2);fflush(stdout);

    for (i=0; i<recordCnt; i++)
	{
	measure[i] = freq[i][j]; 
	}
    sprintf(temp_str, "pbAaDist%c.tab", aaAlphabet[j]);
    calDist(measure,  recordCnt,    51,     0.0, 0.005, temp_str);
    }
    
carefulClose(&o1);

return(0);
}
