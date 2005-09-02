/* pseudoGene descriptions. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hdb.h"
#include "genePred.h"
#include "bed.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: pseudoGene.c,v 1.1 2005/09/02 17:35:42 fanhsu Exp $";

static boolean pseudoGeneExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if mrna  on this one. */
{
boolean result;

result = FALSE;
if (hTableExists("pseudoGeneLink"))
    {
    struct sqlResult *sr;
    char **row;
    struct psl *psl;
    int rowOffset;
    char query[255];
    safef(query, sizeof(query),
          "select name from pseudoGeneLink where name='%s' or kgName='%s' or refseq='%s'",
	  geneId, geneId, geneId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL) 
    	{
	result = TRUE;
	}
	
    sqlFreeResult(&sr);
    }
return(result);
}

static void pseudoGenePrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out mrna descriptions annotations. */
{
struct sqlResult *sr;
char **row;
struct psl *psl;
int rowOffset;
char condStr[255];
char *descID, *desc;    
char *emptyStr;
char query[255];
char *name, *chrom, *chromStart, *chromEnd, *refseq;

emptyStr = strdup("");
safef(query, sizeof(query),
      "select distinct name, chrom, chromStart, chromEnd, refseq from pseudoGeneLink where name='%s' or kgName='%s' or refseq='%s'",
      geneId, geneId, geneId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL) 
    {
    name 	= row[0];
    chrom	= row[1];
    chromStart  = row[2];
    chromEnd	= row[3];
    refseq	= row[4];
   
    desc = emptyStr;
    sprintf(condStr, "acc='%s'", name);
    descID= sqlGetField(NULL, database, "gbCdnaInfo", "description", condStr);
    if (descID != NULL)
    	{
    	sprintf(condStr, "id=%s", descID);
    	desc = sqlGetField(NULL, database, "description", "name", condStr);
	if (desc == NULL) desc = emptyStr;
	}
   	
    hPrintf("<br><A HREF=\"hgTracks?position=%s:%s-%s&pseudoGeneLink=pack&hgFind.matches=%s,\">%s at %s:%s-%s</A> %s\n",
           chrom, chromStart, chromEnd, name, name, chrom, chromStart, chromEnd, desc);
    }
sqlFreeResult(&sr);
}

struct section *pseudoGeneSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create pseudoGene section. */
{
struct section *section = sectionNew(sectionRa, "pseudoGene");
section->exists = pseudoGeneExists;
section->print = pseudoGenePrint;
return section;
}
