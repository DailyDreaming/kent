/* pathways - do pathways section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "hdb.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: pathways.c,v 1.8 2005/06/29 00:42:29 fanhsu Exp $";

struct pathwayLink
/* Info to link into a pathway. */
    {
    char *name;		/* Symbolic name */
    char *shortLabel;	/* Short label. */
    char *longLabel;	/* Long label. */
    char *tables;	/* Tables that must exist. */

    int (*count)(struct pathwayLink *pl, 
    	struct sqlConnection *conn, char *geneId);
    /* Count number of items referring to this gene. */

    void (*printLinks)(struct pathwayLink *pl, 
    	struct sqlConnection *conn, char *geneId);
    /* Print out links. */
    };

static void keggLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Print out kegg database link. */
{
char query[512], **row;
struct sqlResult *sr;
safef(query, sizeof(query), 
	"select keggPathway.locusID,keggPathway.mapID,keggMapDesc.description"
	" from keggPathway,keggMapDesc"
	" where keggPathway.kgID='%s'"
	" and keggPathway.mapID = keggMapDesc.mapID"
	, geneId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<A HREF=\"http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s\" TARGET=_blank>",
    	row[1], row[0]);
    hPrintf("%s</A> - %s<BR>", row[1], row[2]);
    }
sqlFreeResult(&sr);
}

static int keggCount(struct pathwayLink *pl, struct sqlConnection *conn,
	char *geneId)
/* Count up number of hits. */
{
char query[256];
safef(query, sizeof(query), 
	"select count(*) from keggPathway where kgID='%s'", geneId);
return sqlQuickNum(conn, query);
}

static void bioCycLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Print out bioCyc database link. */
{
char query[512], **row;
struct sqlResult *sr;
safef(query, sizeof(query),
	"select bioCycPathway.mapId,description"
	" from bioCycPathway,bioCycMapDesc"
	" where bioCycPathway.kgId='%s'"
	" and bioCycPathway.mapId = bioCycMapDesc.mapId"
	, geneId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<A HREF=\"http://biocyc.org/HUMAN/new-image?type=PATHWAY&object=%s&detail-level=2\" TARGET=_blank>",
    	row[0]);
    hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
    }
sqlFreeResult(&sr);
}

static int bioCycCount(struct pathwayLink *pl, struct sqlConnection *conn,
	char *geneId)
/* Count up number of hits. */
{
char query[256];
safef(query, sizeof(query), 
	"select count(*) from bioCycPathway where kgID='%s'", geneId);
return sqlQuickNum(conn, query);
}

static char *getCgapId(struct sqlConnection *conn)
/* Get cgap ID. */
{
char query[256];
safef(query, sizeof(query), 
	"select cgapId from cgapAlias where alias=\"%s\"", curGeneName);
return sqlQuickString(conn, query);
}

static void reactomeLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
{
char condStr[255];
char *protAccR;
char *reactomeId;
safef(condStr, sizeof(condStr), "kgID='%s'", geneId);
reactomeId = sqlGetField(conn, database, "kgReactome", "reactomeId", condStr);
if (reactomeId != NULL)
    {
    hPrintf("<BR>Reactome: ");
    hPrintf("<A href=\"http://www.reactome.org/cgi-bin/eventbrowser?DB=gk_current&ID=%s&\">%s</A><BR>",reactomeId, reactomeId);
    //hPrintf("<A href=\"http://www.reactome.org/cgi-bin/search?SUBMIT=1&QUERY_CLASS=ReferencePeptideSequence&QUERY=UniProt:%s\">%s</A><BR>",
    fflush(stdout);
    }
}

static void bioCartaLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Print out bioCarta database link. */
{
char *cgapId = getCgapId(conn);
if (cgapId != NULL)
    {
    struct hash *uniqHash = newHash(8);
    char query[512], **row;
    struct sqlResult *sr;
    safef(query, sizeof(query),
    	"select cgapBiocDesc.mapID,cgapBiocDesc.description "
	" from cgapBiocPathway,cgapBiocDesc"
	" where cgapBiocPathway.cgapID='%s'"
	" and cgapBiocPathway.mapID = cgapBiocDesc.mapID"
	, cgapId);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *name = row[0];
	if (!hashLookup(uniqHash, name))
	    {
	    hashAdd(uniqHash, name, NULL);
	    hPrintf("<A HREF=\"http://cgap.nci.nih.gov/Pathways/BioCarta/%s\" TARGET=_blank>", row[0]);
	    hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
	    }
	}
    freez(&cgapId);
    hashFree(&uniqHash);
    }
}

static int bioCartaCount(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Count up number of hits. */
{
int ret = 0;
char *cgapId = getCgapId(conn);
if (cgapId != NULL)
    {
    char query[256];
    safef(query, sizeof(query), 
	    "select count(*) from cgapBiocPathway where cgapID='%s'", cgapId);
    ret = sqlQuickNum(conn, query);
    freez(&cgapId);
    }
return ret;
}

static int reactomeCount(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Count up number of hits. */
{
int ret = 0;
char query[256];
safef(query, sizeof(query), 
	    "select count(*) from kgReactome where kgID='%s'", geneId);
ret = sqlQuickNum(conn, query);
return ret;
}

struct pathwayLink pathwayLinks[] =
{
   { "kegg", "KEGG", "KEGG - Kyoto Encyclopedia of Genes and Genomes", 
   	"keggPathway keggMapDesc", 
	keggCount, keggLink},
   { "bioCyc", "BioCyc", "BioCyc Knowledge Library",
        "bioCycPathway bioCycMapDesc", 
	bioCycCount, bioCycLink},
   { "bioCarta", "BioCarta", "BioCarta from NCI Cancer Genome Anatomy Project",
   	"cgapBiocPathway cgapBiocDesc cgapAlias",
	bioCartaCount, bioCartaLink},
   { "reactome", "Reactome", "Reactome from CSHL, EBI, and GO",
   	"kgReactome",
	reactomeCount, reactomeLink},
};

static boolean pathwayExists(struct pathwayLink *pl,
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if pathway exists and has data. */
{
if (!sqlTablesExist(conn, pl->tables))
    return FALSE;
return pl->count(pl, conn, geneId) > 0;
}

static boolean pathwaysExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if there's some pathway info on this one. */
{
int i;
for (i=0; i<ArraySize(pathwayLinks); ++i)
    {
    if (pathwayExists(&pathwayLinks[i], conn, geneId))
         return TRUE;
    }
return FALSE;
}

static void pathwaysPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out pathway links. */
{
int plIx;
struct pathwayLink *pl;
char **row;
struct sqlResult *sr;
boolean isFirst = TRUE;

for (plIx=0; plIx < ArraySize(pathwayLinks); ++plIx)
    {
    pl = &pathwayLinks[plIx];
    if (pathwayExists(pl, conn, geneId))
        {
	struct dyString *query = dyStringNew(1024);
	if (isFirst)
	    isFirst = !isFirst;
	else
	    hPrintf("<BR>\n");
	hPrintf("<B>%s</B><BR>", pl->longLabel);
	pl->printLinks(pl, conn, geneId);
	}
    }
}

struct section *pathwaysSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create pathways section. */
{
struct section *section = sectionNew(sectionRa, "pathways");
section->exists = pathwaysExists;
section->print = pathwaysPrint;
return section;
}

