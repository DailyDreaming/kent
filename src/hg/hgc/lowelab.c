/* Lowe Lab additions for Archaea and Bacterial details pages*/
#include "common.h"
#include "obscure.h"
#include "hCommon.h"
#include "hash.h"
#include "bits.h"
#include "memgfx.h"
#include "portable.h"
#include "errabort.h"
#include "dystring.h"
#include "nib.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "jksql.h"
#include "dnautil.h" 
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "seqOut.h"
#include "hdb.h"
#include "spDb.h"
#include "hui.h"
#include "hgRelate.h"
#include "htmlPage.h"
#include "psl.h"
#include "cogs.h"
#include "cogsxra.h"
#include "bed.h"
#include "xenalign.h"
#include "isochores.h"
#include "genePred.h"
#include "genePredReader.h"
#include "pepPred.h"
#include "wabAli.h"
#include "minGeneInfo.h"
#include "rnaGene.h"
#include "tRNAs.h"
#include "snoRNAs.h"
#include "gbRNAs.h"
#include "hgMaf.h"
#include "maf.h"
#include "hgConfig.h"
#include "web.h"
#include "dbDb.h"
#include "sample.h"
#include "axt.h"
#include "gbProtAnn.h"
#include "hgSeq.h"
#include "chain.h"
#include "chainNet.h"
#include "chainDb.h"
#include "chainToPsl.h"
#include "chainToAxt.h"
#include "netAlign.h"
#include "tigrCmrGene.h"
#include "jgiGene.h"
#include "sargassoSeaXra.h"
#include "codeBlastScore.h"
#include "codeBlast.h"
#include "tigrOperon.h"
#include "easyGene.h"
#include "llaInfo.h"
#include "blastTab.h"
#include "hdb.h"
#include "hgc.h"
#include "genbank.h"
#include "pseudoGeneLink.h"
#include "axtLib.h"
#include "tfbsCons.h"
#include "tfbsConsMap.h"
#include "tfbsConsSites.h"
#include "tfbsConsFactors.h"
#include "simpleNucDiff.h"
#include "botDelay.h"
#include "bed5FloatScore.h"
#include "bed6FloatScore.h"
#include "pscreen.h"
#include "gapCalc.h"
#include "chainConnect.h"
#include "ec.h"
#include "transMapClick.h"
#include "ccdsClick.h"
#include "memalloc.h"

static char const rcsid[] = "$Id: lowelab.c,v 1.5 2007/05/04 21:21:18 pchan Exp $";

extern char *uniprotFormat;

static void goPrint( struct sqlConnection *conn, char *geneId, char *acc)
/* Print out GO annotations. */
{
struct sqlConnection *goConn = sqlConnect("go");
char query[512];
struct sqlResult *sr;
char **row;
static char *aspects[3] = {"F", "P", "C"};
static char *aspectNames[3] = {
    "Molecular Function",
    "Biological Process",
    "Cellular Component",
};
int aspectIx;

if (sqlTableExists(goConn,"goaPart") && 
    sqlTableExists(goConn,"term")) 
for (aspectIx = 0; aspectIx < ArraySize(aspects); ++aspectIx)
    {
    boolean hasFirst = FALSE;
    safef(query, sizeof(query),
          "select term.acc,term.name"
          " from goaPart,term"
          " where goaPart.dbObjectId = '%s'"
          " and goaPart.goId = term.acc"
          " and goaPart.aspect = '%s'"
          , acc, aspects[aspectIx]);
    sr = sqlGetResult(goConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *goID = row[0];
	char *goTermName = row[1];
	if (!hasFirst)
	    {
	    printf("<B>%s:</B><BR>", aspectNames[aspectIx]);
	    hasFirst = TRUE;
	    }
        printf("<A HREF = \"");
	printf("http://godatabase.org/cgi-bin/go.cgi?view=details&depth=1&query=%s", goID);
	printf("\" TARGET=_blank>%s</A> %s<BR>\n", goID, goTermName);
	}
    if (hasFirst)
        printf("<BR>");
    sqlFreeResult(&sr);
    }
sqlDisconnect(&goConn);
}

void keggOtherGenes(struct sqlConnection *conn, char *geneId, 
        char *table, char *mapId)
/* Print out genes linked to a kegg pathway mapId. */
{
char query[512], **row;
struct sqlResult *sr;
char *extraTable = "gbProtCodeXra";
char *keggTable = "keggPathway";
if (hTableExists(extraTable)) 
    {
    safef(query, sizeof(query), 
            "select x.name, x.gene, x.product from %s k1, %s x  "
            "where k1.mapID = '%s' and "
            "k1.kgID = x.name ;"
            , keggTable,  extraTable, mapId );
    sr = sqlGetResult(conn, query);
    printf("<table>\n");
    while ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<tr><td>");
        hgcAnchorPosition(table,row[0]);
	printf("%s</A> <BR>\n",row[0]); 
        if (differentString(row[0],row[1]) && differentString(row[1], "none"))
            printf("</td><td>%s</td><td>%s</td></tr>\n",
                row[1], row[2]);
        else
            printf("</td><td> </td><td>%s</td></tr>\n",
                row[2]);
        }
    sqlFreeResult(&sr);
    printf("</table>\n");
    }
}
void keggLink(struct sqlConnection *conn, char *geneId, 
        char *table, char *title)
/* Print out kegg database link. */
{
char query[512], **row;
struct sqlResult *sr;
struct sqlConnection *conn2 = hAllocConn();
safef(query, sizeof(query), 
	"select keggPathway.locusID,keggPathway.mapID,keggMapDesc.description"
	" from keggPathway,keggMapDesc"
	" where keggPathway.kgID='%s'"
	" and keggPathway.mapID = keggMapDesc.mapID"
	, geneId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("%s",title);
    printf("<A HREF=\"http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s\" TARGET=_blank>",
    	row[1], row[0]);
    printf("%s</A> - %s<BR>", row[1], row[2]);
    printf("<B>Other Genes in Kegg Pathway: </b><BR>");
    keggOtherGenes(conn2, geneId, table, row[1]);
    printf("<BR>\n");
    }
sqlFreeResult(&sr);
}

int keggCount(struct sqlConnection *conn, char *geneId)
/* Count up number of hits. */
{
char query[256];
char *keggTable = "keggPathway";
if (!hTableExists(keggTable)) 
    return 0;
safef(query, sizeof(query), 
	"select count(*) from %s where kgID='%s'", keggTable, geneId);
return sqlQuickNum(conn, query);
}

void modBaseAnchor(char *swissProtAcc)
/* Print out anchor to modBase. */
{
printf("<A HREF=\"http://salilab.org/modbase-cgi/model_search.cgi?searchkw=name&kword=%s\" TARGET=_blank>", swissProtAcc);
}

void doRefSeq(struct trackDb *tdb, char *item, 
		     char *pepTable, char *extraTable)
/* Handle click on gene track. */
{
struct minGeneInfo ginfo;
char query[256];
char query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
char *words[16], *dupe = cloneString(tdb->type);
int wordCount, x, length;
int num = 0;
int hits = 0;
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn2;
struct sqlConnection *spConn = NULL; 
struct COG *COG=NULL;
struct COGXra *COGXra=NULL;
char *temparray[160];
char *giwords[5];
char *spAcc = NULL;
struct slName *el, *list;
char *table = tdb->tableName;
char *pdb = hPdbFromGdb(database);

spConn = sqlConnect( pdb);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
if (extraTable != NULL && hTableExists(extraTable)) 
    {
    sprintf(query, "select * from %s where name = '%s'", extraTable, item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL) 
	{
	minGeneInfoStaticLoad(row, &ginfo);
        if (ginfo.gene != NULL && differentString(ginfo.gene,"none"))
            printf("<B>Gene: </B>%s<BR>\n", ginfo.gene);
        if (ginfo.product != NULL && differentString(ginfo.product,"none"))
            medlineLinkedLine("Product", ginfo.product, ginfo.product);
        if (ginfo.note != NULL && differentString(ginfo.note,"none"))
            printf("<B>Note: </B>%s<BR>\n", ginfo.note);
        if (ginfo.protein != NULL && differentString(ginfo.protein,"none"))
            printf("<B>Protein: </B>%s<BR>\n", ginfo.protein);
        if (ginfo.ec != NULL && differentString(ginfo.ec,"none"))
            {
            printf("<B>EC (Enzyme Commission) number:</B> "
               "<A HREF=\"http://us.expasy.org/enzyme/%s\" "
	       "TARGET=_BLANK>%s</A>\n", ginfo.ec, ginfo.ec);

            getEcHtml(ginfo.ec);
            medlineLinkedTermLine("EC PubMed Search", ginfo.ec, ginfo.ec, "EC/RN Number");
            printf(" <B>Brenda : </B> <A HREF=\"http://www.brenda.uni-koeln.de/php/result_flat.php4?ecno=%s&organism=\" "
	       "TARGET=_BLANK>%s</A><BR>\n", ginfo.ec, ginfo.ec);
            }
	}
    sqlFreeResult(&sr);
    }
/* lookup swissprot acc */
spAcc = uniProtFindPrimAccFromGene(item, database);
if (spAcc != NULL)
    {
    printf("<B>UniProt:</B> ");
    printf("<A HREF=");
    printf(uniprotFormat, spAcc);
    if (spAcc == NULL)
        {
        printf(" TARGET=_blank>%s</A></B><BR>\n", item);
        }
    else
        {
        printf(" TARGET=_blank>%s</A></B><BR>\n", spAcc);
        }
    }
/* ncbi blast hits */
    hits = chopString(ginfo.gi,":",giwords,sizeof(giwords));
    if (hits > 0)
        {
	printf("<B>NCBI Blast Hits:</B> "
		   "<A HREF=\"http://www.ncbi.nlm.nih.gov/sutils/blink.cgi?pid=%s\" "
		   "TARGET=_BLANK>%s</A><BR>\n", (hits == 2) ? giwords[1] : giwords[0],
		   (hits == 2) ? giwords[1] : giwords[0]);
	}
/* cog description */
if (hTableExists("COG"))
    { 
    sprintf(query, "select * from COG where name = '%s'", item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL) 
	{
	COG = COGLoad(row);
	if(COG!=NULL)
	    { 
     	    length=chopString(COG->COG, "," , temparray, 999);
   	    for(x=0; x<length; x++)
		{
		conn2 = hAllocConn();
		sprintf(query2, "select * from COGXra where name = '%s'", temparray[x]);
                sr2 = sqlGetResult(conn2, query2);
		while ((row2 = sqlNextRow(sr2)) != NULL) 
	    	    {
		    COGXra=COGXraLoad(row2);
		    if(COGXra!=NULL)
    printf("<B>COG: </B> "
               "<A HREF=\"http://www.ncbi.nlm.nih.gov/COG/old/palox.cgi?%s\" "
	       "TARGET=_BLANK>%s</A>\n", COGXra->name, COGXra->name);
		        printf(" %s<BR><BR>\n", COGXra->info); 
		    }
		    sqlFreeResult(&sr2);
		    hFreeConn(&conn2);
	        }
	     }
  	 }
    }
/* interpro domains */
list = spExtDbAcc1List(spConn, spAcc, "InterPro");
if (list != NULL)
    {
    char query[256], **row, **row2;
    struct sqlResult *sr, *sr2;
    printf("<B>InterPro Domains: </B> ");
    printf("<A HREF=\"http://www.ebi.ac.uk/interpro/ISpy?mode=single&ac=%s\" TARGET=_blank>",
    	spAcc);
    printf("Graphical view of domain structure</A><BR>");
    safef(query, sizeof(query),
    	"select extAcc1,extAcc2 from extDbRef,extDb"
	" where extDbRef.acc = '%s'"
	" and extDb.val = 'Interpro' and extDb.id = extDbRef.extDb"
	, spAcc);
    sr = sqlGetResult(spConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char interPro[256];
        char *pdb = hPdbFromGdb(hGetDb());
        safef(interPro, 128, "%s.interProXref", pdb);
            if (hTableExists(interPro))
                {
                safef(query, sizeof(query),
                        "select description from %s where accession = '%s' and interProId = '%s'",
                        interPro, spAcc, row[0]);
                sr2 = sqlGetResult(conn, query);
                if ((row2 = sqlNextRow(sr2)) != NULL)
                    {
                    printf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\" TARGET=_blank>", row[0]);
                    printf("%s</A> - %s <BR>\n", row[0], row2[0]);
                    }
                sqlFreeResult(&sr2);
                }
            else
                {
                printf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\" TARGET=_blank>", row[0]);
                printf("%s</A> - %s<BR>\n", row[0], row[1]);
                }
	}
    printf("<BR>\n");
    slFreeList(&list);
    }

/* pfam domains */
list = spExtDbAcc1List(spConn, spAcc, "Pfam");
if (list != NULL)
    {
    printf("<B>Pfam Domains:</B><BR>");
    for (el = list; el != NULL; el = el->next)
	{
	char query[256];
	char *description;
	safef(query, sizeof(query), "select description from proteome.pfamDesc where pfamAC='%s'", el->name);
	if (!sqlTableExists(spConn,"pfamDesc")) 
	    {
	    safef(query, sizeof(query), 
	    "select extDbRef.extAcc1 from extDbRef,extDb "
	    "where extDbRef.acc = '%s' "
	    "and extDbRef.extDb = extDb.id "
	    "and extDb.val = '%s'"
	    , spAcc,el->name);
	    }
	description = sqlQuickString(spConn, query);
	if (description == NULL)
	    description = cloneString("n/a");
	printf("<A HREF=\"http://www.sanger.ac.uk/cgi-bin/Pfam/getacc?%s\" TARGET=_blank>", 
	    el->name);
	printf("%s</A> - %s<BR>\n", el->name, description);
	freez(&description);
	}
    slFreeList(&list);
    printf("<BR>\n");
    }

list = spExtDbAcc1List(spConn, spAcc, "PDB");
if (list != NULL)
    {
    char query[256], **row;
    struct sqlResult *sr;
    int column = 0, maxColumn=4, rowCount=0;
    printf("<B>Protein Data Bank (PDB) 3-D Structure</B><BR>");
    safef(query, sizeof(query),
    	"select extAcc1,extAcc2 from extDbRef,extDb"
	" where extDbRef.acc = '%s'"
	" and extDb.val = 'PDB' and extDb.id = extDbRef.extDb"
	, spAcc);
    sr = sqlGetResult(spConn, query);
    printf("<TABLE><TR>\n");
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (++column > maxColumn)
	    {
	    printf("</TR><TR>");
	    column = 1;
	    if (rowCount == 0)
	        {
		printf("<TD ALIGN=CENTER COLSPAN=4><I>To conserve bandwidth, only the images from the first %d structures are shown.</I>", maxColumn);
		printf("</TR><TR>");
		}
	    ++rowCount;
	    }
	printf("<TD>");
	printf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?pdbId=%s\" TARGET=_blank>", row[0]);
	if (rowCount < 1)
	    printf("<IMG SRC=\"http://www.rcsb.org/pdb/cgi/pdbImage.cgi/%sx150.jpg\"><BR>", row[0]);
	printf("%s</A> - %s<BR>\n", row[0], row[1]);
	printf("</TD>");
	}
    printf("</TR></TABLE>\n");
    printf("<BR>\n");
    slFreeList(&list);
    }

/* kegg pathway links */
if (keggCount(conn, item) > 0)
    {
    keggLink(conn, item, table, "<B>Kegg Pathway: </b><BR>");
    }
/* Do SAM-T02 sub-section */
//doSamT02(spAcc, database);

/* print go terms */
goPrint( conn, item, spAcc);

/* Do modBase link. */
    {
    printf("<B>ModBase Predicted Comparative 3D Structure on ");
    modBaseAnchor(spAcc);
    printf("%s", spAcc);
    printf("</A></B><BR>\n");
    printf("<TABLE><TR>");
    printf("<TD>");
    modBaseAnchor(spAcc);
    printf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s\"></A></TD>", spAcc);
    printf("<TD>");
    modBaseAnchor(spAcc);
    printf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s&axis=x&degree=90\"></A></TD>", spAcc);
    printf("<TD>");
    modBaseAnchor(spAcc);
    printf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s&axis=y&degree=90\"></A></TD>", spAcc);
    printf("</TR><TR>\n");
    printf("<TD ALIGN=CENTER>Front</TD>");
    printf("<TD ALIGN=CENTER>Top</TD>");
    printf("<TD ALIGN=CENTER>Side</TD>");
    printf("</TR></TABLE>\n");
    printf("<I>The pictures above may be empty if there is no "
            "ModBase structure for the protein.  The ModBase structure "
	    "frequently covers just a fragment of the protein.  You may "
	    "be asked to log onto ModBase the first time you click on the "
	    "pictures. It is simplest after logging in to just click on "
	    "the picture again to get to the specific info on that model.</I><p>");
    }

geneShowPosAndLinks(item, item, tdb, pepTable, "htcTranslatedProtein",
		    "htcGeneMrna", "htcGeneInGenome", "Predicted mRNA");
printTrackHtml(tdb);
hFreeConn(&conn);
}


/*Code to display Sargasso Sea Information*/
void doSargassoSea(struct trackDb *tdb, char *trnaName)
{
struct bed *cb=NULL;
struct sargassoSeaXra *cbs=NULL, *cbs2, *list=NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char *track = tdb->tableName;
char query[512];
char *dupe, *words[16];
char **row;
char tempstring[255]="";
int flag,  z, dashes, wordCount, rowOffset;
int start = cartInt(cart, "o"), num = 0, flag2=0;
float sequenceLength, dashlength=60;

genericHeader(tdb,trnaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, trnaName, start, num);
rowOffset = hOffsetPastBin(seqName, track);

sprintf(query, "select * from %s where name = '%s'", track, trnaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    cb=bedLoadN(row+1, 6);
sequenceLength=(cb->chromEnd - cb->chromStart);
if(sequenceLength<0){ sequenceLength=sequenceLength*-1;}
sequenceLength=sequenceLength/3;
dashlength=sequenceLength/60;

/*Query the database for the extrainfo file for sargassoSea*/
conn=hAllocConn();/*sqlConnect(dupe);*/  
safef(tempstring, sizeof(tempstring),"select * from sargassoSeaXra where qname = '%s'", trnaName);
sr = sqlGetResult(conn, tempstring);
 
/*Load the required data from the database*/
while ((row = sqlNextRow(sr)) != NULL)
    {
    cbs=sargassoSeaXraLoad(row);
    slAddHead(&list, cbs);
    }
slReverse(&list);

flag=0;
flag2=0;

/*Print out table with Blast information*/
 printf("   </tbody>\n</table>\n<br><br>");

printf("<table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" style=\"text-align: left; width: 100%%;\">");
printf(" <tbody>\n    <tr>\n");
printf("     <td style=\"vertical-align: top;\"><b>Organism</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Nucleotides \naligned begin</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Nucelotides \naligned end</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\"><b>NCBI Link</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Evalue</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Percent Identity</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Alignment Length</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Gap openings</b>");
printf("</td>\n    </tr>\n    <tr>");

flag=0;
for(cbs2=list;cbs2!=NULL;cbs2=cbs2->next)
    {
    printf("\n      \n      <td style=\"vertical-align: top;\">");
    printf("<a name=\"%i-desc\"></a>",cbs2->GI);	
    printf("<a\nhref=\"#%i-align\">%s</a>",cbs2->GI,cbs2->species);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%i",cbs2->thisseqstart);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%i",cbs2->thisseqend);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("<a\nhref=\"http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?db=protein&amp;val=%i\">NCBI Link</a>",cbs2->GI);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%s",cbs2->evalue);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%.2f",cbs2->PI);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%i",cbs2->length);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%i",cbs2->gap);
    printf("<br></td>\n    </tr>\n");
    flag=0;	    
    }	
/* printf("  <br><br></tbody>\n</table>  \n"); */



/*Print out the table for the alignments*/
printf("<table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" style=\"width: 100%%;\">");
printf(" <tbody>\n    <tr>\n");
printf("     <td style=\"vertical-align: top;\"><b>Organism</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\"><b>Alignment</b>");
printf("</td>\n    </tr>\n    <tr>");
printf("     <td style=\"vertical-align: top;\">%s where each plus(+) is approx. %.1f amino acids", trnaName, dashlength);
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<code>\n");
for(z=0; z<60; z++)
{
	printf("+");
}
printf("</code>");

printf("</td>\n    </tr>\n    <tr>");
flag=0;
flag2=0;

for(cbs2=list;cbs2!=NULL;cbs2=cbs2->next)
    {		    
    printf("\n      </td>\n      <td style=\"vertical-align: top;\">");
    dashes=cbs2->queryseqstart-cbs2->queryseqend;
    if(dashes<0) dashes=dashes*-1;          
    printf("<a name=\"%i-align\"></a>",cbs2->GI);
    printf("<a\nhref=\"#%i-desc\">%s</a>, %s",cbs2->GI, cbs2->species,cbs2->qName);
    printf("\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("<code>\n");
    dashes=dashes/dashlength;
    if(cbs2->queryseqstart>cbs2->queryseqend)
	for(z=0; z<((cbs2->queryseqend)/dashlength); z++)
	    {
	    printf("&nbsp;");
	    }
    else
        for(z=0; z<((cbs2->queryseqstart)/dashlength); z++)
	    {
	    printf("&nbsp;");
	    }
    if(dashes<1) printf("+");
    for(z=0; z<dashes; z++) printf("+");
    printf("</code>");
    printf("</td>\n    </tr>\n");
    flag=0;
	
                     
    }
   

/*Free the data*/

hFreeConn(&conn);
sargassoSeaXraFree(&cbs);
printTrackHtml(tdb);
}


/*Function to print out full code name from code letter*/
void printCode(char code)
{
    switch(code)
	{
	case 'a':    
	    printf("Aerobe");
	    break;
	case 'b':
	    printf("Bacteria");
	    break;
	case 'c':
	    printf("Crenarchaea");
	    break;
	case 'd':
	    printf("Acidophile");
	    break;
	case 'e':
	    printf("Euryarchaea");
	    break;
	case 'g':
	    printf("Within Genus");
	    break;
	case 'h':
	    printf("Hyperthermophile");
	    break;	
	case 'm':
	    printf("Methanogen");
	    break;
	case 'n':
	     printf("Anaerobe");
	    break;
	case 'o':
	    printf("Nanoarchaea");
	    break;
	case 't':
	    printf("Thermophile");
	    break;
	case 'u':
	    printf("Eukarya");
	    break;
	case 'v':
	    printf("Viral");
    	    break;
	case 'k':
	    printf("Alkaliphile");
    	    break;	
	case 'l':
	    printf("Halophile");
    	    break;
	case 'r':
	    printf("Facultative Aerobe");
    	    break;
	default:
	    break;
       }
}


void doTrnaGenes(struct trackDb *tdb, char *trnaName)
{
char *track = tdb->tableName;
struct tRNAs *trna;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char *dupe, *words[16];
char **row;
int wordCount;
int rowOffset;

genericHeader(tdb,trnaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);

rowOffset = hOffsetPastBin(seqName, track);
sprintf(query, "select * from %s where name = '%s'", track, trnaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
  {
    trna = tRNAsLoad(row+rowOffset);
    
    printf("<img align=right src=\"/RNA-img/%s/%s-%s-%s.gif\" alt='tRNA secondary structure for %s'>\n",
	   database,database,trna->chrom,trna->name,trna->name);
    
    printf("<B>tRNA name: </B> %s<BR>\n",trna->name);
    printf("<B>tRNA Isotype: </B> %s<BR>\n",trna->aa);
    printf("<B>tRNA anticodon: </B> %s<BR>\n",trna->ac);
    printf("<B>tRNAscan-SE score: </B> %.2f<BR>\n",trna->trnaScore);    
    printf("<B>Intron(s): </B> %s<BR>\n",trna->intron);

    printf("<BR><B>Genomic size: </B> %d nt<BR>\n",trna->chromEnd-trna->chromStart);
    printf("<B>Position:</B> "
	   "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
	   hgTracksPathAndSettings(), database, trna->chrom, trna->chromStart+1, trna->chromEnd);
    printf("%s:%d-%d</A><BR>\n", trna->chrom, trna->chromStart+1, trna->chromEnd);
    printf("<B>Strand:</B> %s<BR>\n", trna->strand);
    
    if (trna->next != NULL)
      printf("<hr>\n");
  }
 sqlFreeResult(&sr);
 hFreeConn(&conn);
 printTrackHtml(tdb);
 tRNAsFree(&trna);
}

void doSnornaGenes(struct trackDb *tdb, char *snornaName)
{
char *track = tdb->tableName;
struct snoRNAs *snorna;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char *dupe, *words[16];
char **row;
int wordCount;
int rowOffset;

genericHeader(tdb,snornaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);

rowOffset = hOffsetPastBin(seqName, track);
sprintf(query, "select * from %s where name = '%s'", track, snornaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
  {
    snorna = snoRNAsLoad(row+rowOffset);
    
    printf("<B>sRNA name: </B> %s<BR>\n",snorna->name);
    printf("<B>Snoscan score: </B> %.2f<BR>\n",snorna->snoScore);    
    printf("<B>HMM snoRNA score: </B> %.2f<BR>\n",snorna->hmmScore);    
    printf("<B>Predicted targets: </B> %s<BR>\n",snorna->targetList);
    printf("<B>Predicted guide interactions:</B><pre>%s</pre>\n",snorna->guideStr);
    printf("<B>Possible sRNA homolog(s): </B> %s<BR>\n",snorna->orthologs);

    printf("<BR><B>Genomic size: </B> %d nt<BR>\n",snorna->chromEnd-snorna->chromStart);
    printf("<B>Position:</B> "
	   "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
	   hgTracksPathAndSettings(), database, snorna->chrom, snorna->chromStart+1, snorna->chromEnd);
    printf("%s:%d-%d</A><BR>\n", snorna->chrom, snorna->chromStart+1, snorna->chromEnd);
    printf("<B>Strand:</B> %s<BR><BR>\n", snorna->strand);
    printf("<B>Snoscan program output: </B><BR><pre> %s</pre><BR>\n",snorna->snoscanOutput);
    if (snorna->next != NULL)
      printf("<hr>\n");
  }
 sqlFreeResult(&sr);
 hFreeConn(&conn);
 printTrackHtml(tdb);
 snoRNAsFree(&snorna);
}

void doGbRnaGenes(struct trackDb *tdb, char *gbRnaName)
{
char *track = tdb->tableName;
struct gbRNAs *gbRna;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char *dupe, *words[16];
char **row;
int wordCount;
int rowOffset;

genericHeader(tdb,gbRnaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);


rowOffset = hOffsetPastBin(seqName, track);
sprintf(query, "select * from %s where name = '%s'", track, gbRnaName);
sr = sqlGetResult(conn, query);


while ((row = sqlNextRow(sr)) != NULL)
  {
    
    gbRna = gbRNAsLoad(row+rowOffset);
    
    printf("<B>Genbank ncRNA name: </B> %s<BR>\n",gbRna->name);
    printf("<B>Product Description/Note: </B> %s<BR>\n",gbRna->product);
    printf ("<B>Intron(s): </B> %s<BR>\n",gbRna->intron);
    
    printf("<BR><B>Genomic size: </B> %d nt<BR>\n",gbRna->chromEnd-gbRna->chromStart);
    printf("<B>Position:</B> "
	   "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
	   hgTracksPathAndSettings(), database, gbRna->chrom, gbRna->chromStart+1, gbRna->chromEnd);
    printf("%s:%d-%d</A><BR>\n", gbRna->chrom, gbRna->chromStart+1, gbRna->chromEnd); 
    printf("<B>Strand:</B> %s<BR>\n", gbRna->strand);
     
    if (gbRna->next != NULL)
      printf("<hr>\n");
    
  }
 sqlFreeResult(&sr);
 hFreeConn(&conn);
 printTrackHtml(tdb);
 gbRNAsFree(&gbRna);

}

void doEasyGenes(struct trackDb *tdb, char *egName)
{
char *track = tdb->tableName;
struct easyGene *egList = NULL, *eg;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;

genericHeader(tdb,egName);
rowOffset = hOffsetPastBin(seqName, track);
sprintf(query, "select * from %s where name = '%s'", track, egName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    slAddTail(&egList,easyGeneLoad(row+rowOffset));
slReverse(&egList);
sqlFreeResult(&sr);
hFreeConn(&conn);
for (eg = egList; eg != NULL; eg = eg->next)
    {
    if (eg->genbank[0] == 'Y')
	printf("<FONT COLOR=\"#FF0000\">\n");
    else
	printf("<FONT COLOR=\"#000000\">\n");
    printf("<B>Item:</B> %s<BR>\n",eg->name);
    printf("<B>Feature identifier:</B> %s<BR>\n",eg->feat);
    printf("<B>Start codon:</B> %s<BR>\n",eg->startCodon);
    printf("<B>EasyGene descriptor:</B> %s<BR>\n",eg->descriptor);
    if (eg->R >= 0.001)
	printf("<B>R value:</B> %.3f<BR>\n",eg->R);
    else 
	printf("<B>R value:</B> %.2e<BR>\n",eg->R);
    printf("<B>Log-odds :</B> %.1f<BR>\n",eg->logOdds);
    printf("<B>Frame:</B> %d<BR>\n",eg->frame);
    printf("<B>Swiss-Prot match:</B> %s<BR>\n",eg->swissProt);
    printf("<B>ORF identifier:</B> %s<BR>\n",eg->orf);
    printPos(eg->chrom, eg->chromStart, eg->chromEnd, eg->strand, TRUE, eg->name);
    printf("</FONT>\n");
    if (eg->next != NULL)
	printf("<hr>\n");
    }
printTrackHtml(tdb);
easyGeneFreeList(&egList);
}

void doCodeBlast(struct trackDb *tdb, char *trnaName)
{
struct pepPred *pp=NULL;
struct codeBlast *cb=NULL;
struct codeBlastScore *cbs=NULL, *cbs2, *list=NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char *track = tdb->tableName;
char query[512];
char *dupe, *words[16];
char **row;
char tempstring[255]="";
int flag, z, dashes, wordCount, rowOffset, currentGI=0;
int start = cartInt(cart, "o"), num = 0, flag2=0;
float sequenceLength, dashlength=60;

genericHeader(tdb,trnaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, trnaName, start, num);
rowOffset = hOffsetPastBin(seqName, track);

sprintf(query, "select * from %s where name = '%s'", track, trnaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cb=codeBlastLoad(row);
    }
    
sequenceLength=(cb->chromEnd - cb->chromStart);
if(sequenceLength<0){ sequenceLength=sequenceLength*-1;}
sequenceLength=sequenceLength/3;
dashlength=sequenceLength/60;

conn=hAllocConn();/*sqlConnect(dupe);*/  
sprintf(query, "select * from gbProtCodePep where name = '%s'", trnaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    pp=pepPredLoad(row);
    }


/*Query the database for the extrainfo file for codeBlast*/
conn=hAllocConn();/*sqlConnect(dupe);*/  
safef(tempstring, sizeof(tempstring), "select * from codeBlastScore where qname = '%s'", trnaName);

sr = sqlGetResult(conn, tempstring);
 
/*Load the required data from the database*/
while ((row = sqlNextRow(sr)) != NULL)
    {
    cbs=codeBlastScoreLoad(row);
    slAddHead(&list, cbs);
    }

if (pp!=NULL)
    printf(
"<br><a\nhref=\"http://www.ncbi.nlm.nih.gov/BLAST/Blast.cgi?ALIGNMENTS=250&ALIGNMENT_VIEW=Pairwise&AUTO_FORMAT=Semiauto&CDD_SEARCH=on&CLIENT=web&DATABASE=nr&DESCRIPTIONS=500&ENTREZ_QUERY=All+organisms&EXPECT=10&FILTER=L&FORMAT_BLOCK_ON_RESPAGE=None&FORMAT_ENTREZ_QUERY=All+organisms&FORMAT_OBJECT=Alignment&FORMAT_TYPE=HTML&GAPCOSTS=11+1&GET_SEQUENCE=on&I_THRESH=0.005&LAYOUT=TwoWindows&MASK_CHAR=0&MASK_COLOR=0&MATRIX_NAME=BLOSUM62&NCBI_GI=on&NEW_FORMATTER=on&PAGE=Proteins&PROGRAM=blastp&QUERY=%s&SERVICE=plain&SET_DEFAULTS=Yes&SET_DEFAULTS.x=25&SET_DEFAULTS.y=11&SHOW_LINKOUT=on&SHOW_OVERVIEW=on&WORD_SIZE=3&END_OF_HTTPGET=Yes\">Query NCBI Blast",pp->seq);

/*Print out table with Blast information*/
printf("   </tbody>\n</table>\n<br><br><table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" style=\"text-align: left; width: 100%%;\">");
printf(" <tbody>\n    <tr>\n");
printf("     <td style=\"vertical-align: top;\"><b>Organism</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Phylogenetic/Functional Category</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Gene Name</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Product</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\"><b>NCBI Entry</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Evalue</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Percent Identity</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Alignment Length(AA)</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Gap openings</b>");
printf("</td>\n    </tr>\n    <tr>");

flag=0;
for(cbs2=list;cbs2!=NULL;cbs2=cbs2->next)
    {
    if(sameString(trnaName, cbs2->qName))
        {
	if(flag==0)
	    {
	    currentGI=cbs2->GI;
	    printf("\n      \n      <td style=\"vertical-align: top;\">");
	    printf("<a name=\"%i-desc\"></a>",cbs2->GI);
	
	    printf("<a\nhref=\"#%i-align\">%s</a>",cbs2->GI,cbs2->species);
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");

	    flag=1;
	    }
			
	if((cbs2->next!=NULL) && (currentGI== cbs2->GI) && (currentGI== cbs2->next->GI)  )
	    {
	    printCode(cbs2->code[0]);
            printf(", ");
	    } 
			
	else
	    {
	   
	    printCode(cbs2->code[0]);
	    
	    /*See which database to link to*/
	    if((sameString(cbs2->species, "Pyrococcus furiosus"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=pyrFur2&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Pyrococcus abyssi"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=pyrAby1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Pyrococcus horikoshii"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=pyrHor1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Thermococcus kodakaraensis"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=therKoda1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Sulfolobus solfataricus"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=sulSol1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Sulfolobus tokodaii"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=sulfToko1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Sulfolobus acidocaldarius"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=sulfAcid1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Pyrobaculum aerophilum"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=pyrAer1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Nanoarchaeum equitans"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=nanEqu1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanosarcina acetivorans"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=metAce1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanosarcina barkeri"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=methBark1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanosarcina mazei"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=methMaze1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanothermobacter thermautotrophicus"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=methTher1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanococcoides burtonii"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=methBurt1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanopyrus kandleri"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=methKand1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Haloarcula marismortui"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=halMar1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Halobacterium sp."))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=haloHalo1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Aeropyrum pernix"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=aerPer1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Archaeoglobus fulgidus"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=archFulg1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Thermoplasma acidophilum"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=therAcid1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Thermoplasma volcanium"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=therVolc1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanocaldococcus jannaschii"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=methJann1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanococcus maripaludis"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=metMar1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanosphaera stadtmanae"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=methStad1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Methanospirillum hungatei"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=methHung1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Aquifex aeolicus"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=aquiAeol1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Thermotoga maritima"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=therMari1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Escherichia coli K12"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=eschColi_K12_1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Bacillus subtilis"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=baciSubt1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else if((sameString(cbs2->species, "Shewanella oneidensis"))){
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"%s?db=shewOnei1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
	    }
	    else{
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("%s",cbs2->name);
	    }
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("%s",cbs2->product);
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<a\nhref=\"http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?db=protein&amp;val=%i\">NCBI Link</a>",cbs2->GI);
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("%s",cbs2->evalue);
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("%.2f",cbs2->PI);
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("%i",cbs2->length);
	    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("%i",cbs2->gap);
	    printf("<br></td>\n    </tr>\n");
	    flag=0;
	    
	    }               
        }               
    }	
    
    /*Print out the table for the alignments*/
printf("</td></td></td><br>\n<table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" style=\"width: 100%%;\">");
printf(" <tbody>\n    <tr>\n");
printf("     <td style=\"vertical-align: top;\"><b>Organism</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\"><b>Alignment</b>");
printf("</td>\n    </tr>\n    <tr>");
printf("     <td style=\"vertical-align: top;\">%s where each plus(+) is approx. %f amino acids", trnaName, dashlength);
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<code>\n");
for(z=0; z<60; z++)
{
	printf("+");
}
printf("</code>");

printf("</td>\n    </tr>\n    <tr>");
flag=0;
flag2=0;

for(cbs2=list;cbs2!=NULL;cbs2=cbs2->next)
    {		    
    if(sameString(trnaName, cbs2->qName));
        {
	if(flag==0)
	    {
	    currentGI=cbs2->GI;
	    printf("\n      </td>\n      <td style=\"vertical-align: top;\">");
	    flag=1;
	    }
	if((cbs2->next!=NULL) && (currentGI== cbs2->GI) && (currentGI== cbs2->next->GI) )
	    {
	    }  		
	else
	    {
	    dashes=cbs2->seqend-cbs2->seqstart;
	    if(cbs2->length<dashes/2){dashes=dashes/3;}
	    if(dashes<0) dashes=dashes*-1;           
	    printf("<a name=\"%i-align\"></a>",cbs2->GI);
            printf("<a\nhref=\"#%i-desc\">%s</a>, %s",cbs2->GI, cbs2->species, cbs2->name);
	    printf("\n      </td>\n      <td style=\"vertical-align: top;\">");
	    printf("<code>\n");
	    dashes=dashes/dashlength;
	    if(cbs2->seqstart>cbs2->seqend)
		for(z=0; z<((cbs2->seqend)/dashlength); z++)
		    {
		    printf("&nbsp;");
		    }
	    else
		for(z=0; z<((cbs2->seqstart)/dashlength); z++)
		    {
		    printf("&nbsp;");
		    }
	    if(dashes<1) printf("+");
	    for(z=0; z<dashes; z++) printf("+");
	    printf("</code>");
	    printf("</td>\n    </tr>\n");
	    flag=0;
	
	    }
                        
        }
                     
    }
   
printf("  <br><br></tbody>\n</table>  \n");
/*\printf("  </tbody>\n</table>  \n");
*/
/*Free the data*/

hFreeConn(&conn);
codeBlastScoreFree(&cbs);
codeBlastFree(&cb);
printTrackHtml(tdb);
}


void llDoCodingGenes(struct trackDb *tdb, char *item, 
		     char *pepTable, char *extraTable)
/* Handle click on gene track. */
{
struct minGeneInfo ginfo;
char query[256];
char query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
char *words[16], *dupe = cloneString(tdb->type);
int wordCount, x, length;
int start = cartInt(cart, "o"), num = 0;
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn2;
struct COG *COG=NULL;
struct COGXra *COGXra=NULL;
char *temparray[160];

genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, item, start, num);
if (pepTable != NULL && hTableExists(pepTable))
    {
    char *pepNameCol = sameString(pepTable, "gbSeq") ? "acc" : "name";
    conn = hAllocConn();
    /* simple query to see if pepName has a record in pepTable: */
    safef(query, sizeof(query), "select 0 from %s where %s = '%s'",
	  pepTable, pepNameCol, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	hgcAnchorSomewhere("htcTranslatedProtein", item, pepTable, seqName);
	printf("Predicted Protein</A> <BR>\n"); 
	}
    sqlFreeResult(&sr);
    }
if (extraTable != NULL && hTableExists(extraTable)) 
    {
    conn = hAllocConn();
    sprintf(query, "select * from %s where name = '%s'", extraTable, item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL) 
	{
	minGeneInfoStaticLoad(row, &ginfo);
	printf("<B>Product: </B>%s<BR>\n", ginfo.product);
	printf("<B>Note: </B>%s<BR>\n", ginfo.note);
	}
    sqlFreeResult(&sr);
    }
if (hTableExists("COG"))
    { 
    conn = hAllocConn();
    sprintf(query, "select * from COG where name = '%s'", item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL) 
	{
	COG = COGLoad(row);
	if(COG!=NULL)
	    { 
     	    length=chopString(COG->COG, "," , temparray, 999);
   	    for(x=0; x<length; x++)
		{
		conn2 = hAllocConn();
		sprintf(query2, "select * from COGXra where name = '%s'", temparray[x]);
                sr2 = sqlGetResult(conn2, query2);
		while ((row2 = sqlNextRow(sr2)) != NULL) 
	    	    {
		    COGXra=COGXraLoad(row2);
		    if(COGXra!=NULL)
		        printf("<B>COG: </B>%s <B>INFO: </B>%s<BR>\n", COGXra->name, COGXra->info); 
		    }
		    sqlFreeResult(&sr2);
		    hFreeConn(&conn2);
	        }
	     }
  	 }
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doTigrOperons(struct trackDb *tdb, char *opName)
/* track handler for the TIGR operon predictions */
{
char *track = tdb->tableName;
struct tigrOperon *op;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char *dupe, *words[16];
char **row;
int wordCount;
int start = cartInt(cart, "o"), num = 0;

genericHeader(tdb,opName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, opName, start, num);
sprintf(query, "select * from %sInfo where name = '%s'", track, opName);
sr = sqlGetResult(conn, query);
/* Make the operon table like on the TIGR web page. */
if ((row = sqlNextRow(sr)) != NULL)
    {
    int i,j;
    char *infos[30];
    op = tigrOperonLoad(row);
    chopCommas(op->info,infos);
    printf("<P>\n<TABLE BORDER=1 ALIGN=\"CENTER\">\n");
    for (i = 0; i <= op->size; i++)
	{
	printf("  <TR ALIGN=\"CENTER\">");
	for (j = 0; j <= op->size; j++)
	    {
	    printf("<TD>");
	    if ((i == 0 || j == 0) && !(i == 0 && j == 0))
		(i > j) ? printf("%s",op->genes[i-1]) : printf("%s",op->genes[j-1]);
	    else if (i + j > 0)
		{
		char *data = infos[((i-1)*op->size)+(j-1)];
		if (!sameString(data,"---"))
		    {
		    char *n, *conf;
		    n = chopPrefixAt(data,'|');
		    conf = data;
		    printf("confidence = %.2f, n = %d",atof(conf),atoi(n));
		    }
		else
		    printf("%s",data);
		}
	    printf("</TD>");
	    }
	printf("</TR>\n");
	}
    printf("</TABLE>\n</P>\n");
    }
printTrackHtml(tdb);
/* clean up */
sqlFreeResult(&sr);
hFreeConn(&conn);
tigrOperonFree(&op);
}

void doTigrCmrGene(struct trackDb *tdb, char *tigrName)
/* Handle the TIRG CMR gene track. */
{
  char *track = tdb->tableName;
  struct tigrCmrGene *tigr;
  char query[512];
  struct sqlConnection *conn = hAllocConn();
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;
  /* int start = cartInt(cart, "o"), num = 0; */
  
  genericHeader(tdb,tigrName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(seqName, track);
  sprintf(query, "select * from %s where name = '%s'", track, tigrName);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
      tigr = tigrCmrGeneLoad(row);
      if (tigr != NULL)
	{
	  printf("<B>TIGR locus name: </B> %s<BR>\n",tigrName);
	  printf("<B>TIGR gene description: </B> %s<BR>\n",tigr->tigrCommon);
	  printf("<B>Alternate TIGR gene name: </B> ");
	  if (strlen(tigr->tigrGene) >0) {
	    printf("%s<BR>\n",tigr->tigrGene);
	  }
	  else {
	    printf("None<BR>");
	  }
	  printf("<B>Genbank locus name: </B> %s<BR>\n",tigr->primLocus);
	  printf("<B>Protein length: </B> %d aa<BR>\n",tigr->tigrPepLength);
	  if (strlen(tigr->tigrECN)>0) {
	    printf("<B>Enzyme comission number: </B> %s<BR>\n",tigr->tigrECN);
	  }
	  printf("<B>Main role: </B> %s<BR>\n",tigr->tigrMainRole);
	  printf("<B>Subrole: </B> %s<BR>\n",tigr->tigrSubRole);
	  if (tigr->tigrMw > 0) {
	    printf("<B>Molecular weight: </B> %.2f Da<BR>\n",tigr->tigrMw);
	  }
	  if (tigr->tigrPi > 0) {
	    printf("<B>Isoelectric point: </B> %.2f<BR>\n",tigr->tigrPi);
	  }
	  printf("<B>GC content: </B> %.2f %%<BR>\n",tigr->tigrGc);
	  
	  printf("<BR><B>Position:</B> "
		 "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
		 hgTracksPathAndSettings(), database, tigr->chrom, tigr->chromStart, tigr->chromEnd);
	  printf("%s:%d-%d</A><BR>\n", tigr->chrom, tigr->chromStart, tigr->chromEnd);
	  printf("<B>Strand:</B> %s<BR>\n", tigr->strand);
	  printf("<B>Genomic size: </B> %d nt<BR>\n",tigr->tigrLength);
	  if (tigr->next != NULL)
	    printf("<hr>\n");
	}
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  printTrackHtml(tdb);
  tigrCmrGeneFree(&tigr);
}

void doJgiGene(struct trackDb *tdb, char *jgiName)
/* Handle the JGI gene track. */
{
  char *track = tdb->tableName;
  struct jgiGene *jgi;
  char query[512];
  struct sqlConnection *conn = hAllocConn();
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;

  genericHeader(tdb,jgiName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(seqName, track);
  sprintf(query, "select * from %s where name = '%s'", track, jgiName);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
      jgi = jgiGeneLoad(row+rowOffset);
      printf("<B>JGI locus name: </B> %s<BR>\n",jgiName);
      printf("<B>JGI gene symbol: </B> %s<BR>\n",jgi->jgiSymbol);
      printf("<B>JGI gene description: </B> %s<BR>\n",jgi->jgiDescription);
      printf("<B>JGI gene id:</B> "
             "<A HREF=\"http://img.jgi.doe.gov/cgi-bin/pub/main.cgi?section=GeneDetail&page=geneDetail&gene_oid=%s\" TARGET=_blank>",
             jgi->jgiGeneId);
      printf("%s</A><BR>\n", jgi->jgiGeneId);
      printf("<B>GC content: </B> %.0f %%<BR>\n",jgi->jgiGc);
      
      printf("<BR><B>Position:</B> "
             "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
             hgTracksPathAndSettings(), database, jgi->chrom, jgi->chromStart + 1, jgi->chromEnd);
      printf("%s:%d-%d</A><BR>\n", jgi->chrom, jgi->chromStart + 1, jgi->chromEnd);
      printf("<B>Strand:</B> %s<BR>\n", jgi->strand);
      printf("<B>Genomic size: </B> %d nt<BR>\n", (jgi->chromEnd - jgi->chromStart));
      if (jgi->next != NULL)
        printf("<hr>\n");
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  printTrackHtml(tdb);
  jgiGeneFree(&jgi);
}

int parseDelimitedString(char *inString, char delimiter, char *outString[], int outSize)
{
    int arrayCount = 0;
    int charCount = 0;
    int start = 0;
    char *out = NULL;
    
    for (charCount = 0; charCount < strlen(inString); charCount++)
    {
        if (inString[charCount] == delimiter)
        {
            if (arrayCount < outSize)
            {
                out = malloc(sizeof(char) * (charCount - start + 1));
                memcpy(out, inString + start, sizeof(char) * (charCount - start));
                out[charCount - start] = '\0';
                outString[arrayCount++] = out;               
                start = charCount + 1;
            }
        }
    }
    if (arrayCount < outSize)
    {
        out = malloc(sizeof(char) * (charCount - start + 1));
        memcpy(out, inString + start, sizeof(char) * (charCount - start));
        out[charCount - start] = '\0';
        outString[arrayCount++] = out;               
    }
    
    return arrayCount;
}

struct bed * getBlastpTrackRecord(struct sqlConnection *conn, struct trackDb *tdb, char *targetName)
/* Get blastp track record clicked by user*/
{
    char *track = tdb->tableName;
    struct bed *blastpTrack = NULL;
    char *dupe, *words[16];
    int wordCount;
    int bedSize = 0;
    char query[512];
    struct sqlResult *sr;
    char **row;
    int rowOffset;
   
    int start = cartInt(cart, "o");
    int end = cartInt(cart, "t");
    char *chrom = cartString(cart, "c");

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;
    
    rowOffset = hOffsetPastBin(seqName, track);
    sprintf(query, "select distinct * from %s where name = '%s' and chrom = '%s' and chromStart = %d and chromEnd = %d",
            track, targetName, chrom, start, end);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        blastpTrack = bedLoadN(row+rowOffset, bedSize);

    freez(&dupe);
    sqlFreeResult(&sr);
    
    return blastpTrack;
}

struct minGeneInfo* getGbProtCodeInfo(struct sqlConnection *conn, char* dbName, char *geneName)
/* Get refseq protein annotation by given gene name in a given database */
{
    char query[512];
    struct sqlResult *sr;
    char **row;
    struct minGeneInfo* ginfo = NULL;
    char gbProtCodeXra[] = "gbProtCodeXra";
    
    if (hTableExists(gbProtCodeXra))
    {
        sprintf(query, "select * from %s.%s where name = '%s'", dbName, gbProtCodeXra, geneName);
        sr = sqlGetResult(conn, query);
        if ((row = sqlNextRow(sr)) != NULL) 
            ginfo = minGeneInfoLoad(row);
    }
    
    sqlFreeResult(&sr);
    return ginfo;
}

void printQueryGeneInfo(struct sqlConnection *conn, struct bed *blastpTrack, char *queryName, unsigned int *querySeqLength)
/* Get and print blastp query gene info */
{
    char query[512];
    struct sqlResult *srRefSeq;
    char **row;
    int geneCount;
    char **buffer = NULL;
    char *targetGeneName[2];
    struct minGeneInfo* ginfo;
    
    char refSeq[] = "refSeq";
    char blastpHits[] = "blastpHits";
    
    unsigned int queryStart = 0;
    unsigned int queryEnd = 0;
    
    parseDelimitedString(blastpTrack->name, ':', targetGeneName, 2); 
        
    if (hTableExists(refSeq) && hTableExists(blastpHits))
    {
        /* Get query gene from refSeq */
        sprintf(query, "select count(*) from %s where chrom = '%s' and strand = '%s' and cdsStart <= %u and cdsEnd >= %u",
                refSeq, blastpTrack->chrom, blastpTrack->strand, blastpTrack->chromStart, blastpTrack->chromEnd);
        srRefSeq = sqlGetResult(conn, query);
        if ((row = sqlNextRow(srRefSeq)) != NULL)
        {
            geneCount = atoi(row[0]);
            sqlFreeResult(&srRefSeq);
            
            if (geneCount == 1)
            {
                sprintf(query, "select name, cdsStart, cdsEnd from %s where chrom = '%s' and strand = '%s' and cdsStart <= %u and cdsEnd >= %u",
                        refSeq, blastpTrack->chrom, blastpTrack->strand, blastpTrack->chromStart, blastpTrack->chromEnd);
                srRefSeq = sqlGetResult(conn, query);
                if ((row = sqlNextRow(srRefSeq)) != NULL)
                {
                    strcpy(queryName, row[0]);
                    queryStart = strtoul(row[1], buffer, 10);
                    queryEnd = strtoul(row[2], buffer, 10);
                }
                sqlFreeResult(&srRefSeq);
            }
            else
            {
                /* Check blastpHits if more than 1 query gene is found within the region */
                sprintf(query, "select a.name, a.cdsStart, a.cdsEnd from %s a, %s b where a.chrom = '%s' and a.strand = '%s' and a.cdsStart <= %u and a.cdsEnd >= %u and a.name = b.query and b.target like '%%%s'",
                        refSeq, blastpHits,
                        blastpTrack->chrom, blastpTrack->strand, blastpTrack->chromStart, blastpTrack->chromEnd, targetGeneName[0]);
                srRefSeq = sqlGetResult(conn, query);
                if ((row = sqlNextRow(srRefSeq)) != NULL)
                {
                    strcpy(queryName, row[0]);
                    queryStart = strtoul(row[1], buffer, 10);
                    queryEnd = strtoul(row[2], buffer, 10);
                }
                sqlFreeResult(&srRefSeq);
            }
            
            if ((queryStart == 0) && (queryEnd == 0))
                printf("Query gene not found for %s at %s:%u-%u\n", blastpTrack->name, blastpTrack->chrom, blastpTrack->chromStart, blastpTrack->chromEnd);
            else
            {
                *querySeqLength = queryEnd - queryStart;
                
                /* Print query gene info */
                printf("<B>Gene: </B>%s<BR>\n", queryName);
                
                ginfo = getGbProtCodeInfo(conn, database, queryName);
                if (ginfo != NULL)
                {
                    if (ginfo->product != NULL && differentString(ginfo->product,"none"))
                        medlineLinkedLine("Product", ginfo->product, ginfo->product);
                }
                
                printf("<B>Position:</B> "
                       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                       hgTracksPathAndSettings(), database, blastpTrack->chrom, queryStart + 1, queryEnd);
                printf("%s:%d-%d</A><BR>\n", blastpTrack->chrom, queryStart + 1, queryEnd);
                printf("<B>Strand:</B> %s<BR>\n", blastpTrack->strand);
                printf("<B>Genomic size: </B> %d nt<BR>\n", (queryEnd - queryStart));
            }
        }
        else
            printf("<BR>Query gene not found for %s at %s:%u-%u<BR>\n", blastpTrack->name, blastpTrack->chrom, blastpTrack->chromStart, blastpTrack->chromEnd);            
    }
 
    sqlFreeResult(&srRefSeq);
    free(targetGeneName[0]);
    free(targetGeneName[1]);
}

void getGenomeClade(struct sqlConnection *conn, char *dbName, char *genome, char *clade)
/* Get genome and clade for a given database name */
{
    char query[512];
    struct sqlResult *srDb;
    char **rowDb;

    sprintf(query, "select a.genome, c.label from centraldb.genomeClade a, centraldb.dbDb b, centraldb.clade c where a.genome = b.genome and a.clade = c.name and b.name = '%s'", dbName);
    srDb = sqlGetResult(conn, query);
    if ((rowDb = sqlNextRow(srDb)) != NULL)
    {
        strcpy(genome, rowDb[0]);
        strcpy(clade, rowDb[1]);
    }
    sqlFreeResult(&srDb);
}

struct slName* getAllClades(struct sqlConnection *conn)
/* Get all available clades in database */
{
    char query[512];
    struct sqlResult *srDb;
    char **rowDb;
    struct slName *list = NULL;
    char clade[50];

    sprintf(query, "select label from centraldb.clade");
    srDb = sqlGetResult(conn, query);
    while ((rowDb = sqlNextRow(srDb)) != NULL)
    {
        strcpy(clade, rowDb[0]);
        slNameAddTail(&list, clade);
    }
    sqlFreeResult(&srDb);
    
    return list;
}

struct blastTab* loadBlastpHits(struct sqlConnection *conn, char* queryName)
/* Load all blastp hits of the given query gene into a list */
{
    char query[512];
    struct sqlResult *srBlastpHits;
    struct blastTab *list = NULL;
    struct blastTab *blastpHits;
    char **row;
    char blastpHitsTable[] = "blastpHits";

    if (hTableExists(blastpHitsTable))
    {
        sprintf(query, "select * from %s where query = '%s'", blastpHitsTable, queryName);
        srBlastpHits = sqlGetResult(conn, query);
        while ((row = sqlNextRow(srBlastpHits)) != NULL)
        {
            blastpHits = blastTabLoad(row);
            slAddTail(&list, blastpHits);
        }
    }
    sqlFreeResult(&srBlastpHits);
    return list;
}

void printBlastpResult(struct sqlConnection *conn, struct blastTab *blastpHitsList, char* sectionClade, unsigned int querySeqLength)
/* Print Blastp result of given clade */
{
    char query[512];
    struct sqlResult *sr;
    char **row;
    char refSeq[] = "refSeq";
    struct blastTab *blastpHits;
    struct minGeneInfo *ginfo;
    char *blastpTarget[2];
    char genome[50] = "";
    char clade[50] = "";
    unsigned int hitStart = 0;
    unsigned int hitEnd = 0;
    char **buffer = NULL;

    int tStart = cartInt(cart, "o");
    int tEnd = cartInt(cart, "t");
    char *tChrom = cartString(cart, "c");

    printf("<br><a name=\"%s\"><b>BLASTP Alignment Hits to %sl Proteins</b></a><br>\n", sectionClade, sectionClade);
 
    /* Print table */
    printf("<table style=\"width: 100%%;\" bgcolor=\"#%s\" border=\"0\" cellpadding=\"1\" cellspacing=\"0\">", HG_COL_BORDER);
    printf("<tbody><tr><td>\n");
    printf("<table style=\"width: 100%%; text-align: left;\" bgcolor=\"%s\" border=\"1\" cellpadding=\"2\" cellspacing=\"2\">\n", HG_COL_INSIDE);
    printf("<tbody>\n");

    /* Print table column heading */
    printf("<tr style=\"vertical-align: top;\">\n");
    printf("<td width=\"18%%\"><b>Organism</b></td>\n");
    printf("<td width=\"7%%\"><b>Gene</b></td>\n");
    printf("<td><b>Product</b></td>\n");
    printf("<td width=\"5%%\"><b>Percentage of Full Length</b></td>\n"); 
    printf("<td width=\"8%%\"><b>Query Position</b></td>\n");
    printf("<td width=\"5%%\"><b>Identity (%%)</b></td>\n");
    printf("<td width=\"5%%\"><b>E-value</b></td>\n");
    printf("<td width=\"5%%\"><b>Log of E-value</b></td>\n");
    printf("<td width=\"5%%\"><b>Bit Score</b></td>\n");
    printf("<td width=\"6%%\"><b>Alignment Length</b></td>\n");
    printf("<td width=\"5%%\"><b>Mismatch</b></td>\n");
    printf("<td width=\"5%%\"><b>Gap</b></td>\n");
    printf("</tr>\n");
    
    blastpHits = blastpHitsList;
    while (blastpHits != NULL)
    {
        parseDelimitedString(blastpHits->target, ':', blastpTarget, 2);
        
        /* Get species info */
        getGenomeClade(conn, blastpTarget[0], genome, clade);

        if (sameWord(clade, sectionClade))
        {
            printf("<tr style=\"vertical-align: top;\">\n");
           
            printf("<td><a name=\"%s:%s:%u-%u\"><i>%s</i></td>\n", blastpTarget[1], tChrom, tStart, tEnd, genome);
            
            if (hTableExists(refSeq))
            {
                sprintf(query, "select chrom, cdsStart, cdsEnd from %s.%s where name = '%s'",
                        blastpTarget[0], refSeq, blastpTarget[1]);
                sr = sqlGetResult(conn, query);
                if ((row = sqlNextRow(sr)) != NULL)
                {
                    hitStart = strtoul(row[1], buffer, 10) + blastpHits->tStart * 3 + 1;
                    hitEnd = strtoul(row[1], buffer, 10) + blastpHits->tEnd * 3;
                    printf("<td><a href=\"http://archdev.cse.ucsc.edu/cgi-bin/hgTracks\?position=%s:%u-%u&db=%s\" TARGET=_blank>%s</a></td>\n",
                           row[0], hitStart, hitEnd, blastpTarget[0], blastpTarget[1]);
                }
                else
                    printf("<td>%s</td>\n", blastpTarget[1]);
                sqlFreeResult(&sr);
            }
            else
                printf("<td>%s</td>\n", blastpTarget[1]);

            ginfo = getGbProtCodeInfo(conn, blastpTarget[0], blastpTarget[1]);
            if (ginfo != NULL && ginfo->product != NULL && differentString(ginfo->product,"none"))
                printf("<td>%s</td>\n", ginfo->product);
            else
                printf("<td>%s</td>\n", "N/A");                
            
            printf("<td style=\"text-align: center;\">%0.2f</td>\n", ((double) (blastpHits->qEnd - blastpHits->qStart) / ((double) querySeqLength / 3.0f)) * 100.0f);
            printf("<td style=\"text-align: center;\">%u - %u</td>\n", blastpHits->qStart + 1, blastpHits->qEnd);
            printf("<td style=\"text-align: right;\">%0.2f</td>\n", blastpHits->identity);
            printf("<td style=\"text-align: right;\">%0.0e</td>\n", blastpHits->eValue);
            printf("<td style=\"text-align: right;\">e%0.0f</td>\n", (blastpHits->eValue == 0)? 0 : log(blastpHits->eValue) / log(10));
            printf("<td style=\"text-align: right;\">%0.1f</td>\n", blastpHits->bitScore);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastpHits->aliLength);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastpHits->mismatch);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastpHits->gapOpen);
    
            printf("</tr>\n");
        }
        free(blastpTarget[0]);        
        free(blastpTarget[1]);
        blastpHits = blastpHits->next;
    }
    /* Close table */
    printf("</tbody>\n");
    printf("</table>\n");
    printf("</td></tr></tbody>\n");
    printf("</table>\n");
}

void doBlastP(struct trackDb *tdb, char *targetName)
/* Handle the BlastP Archaea and BlastP Bacteria tracks. */
{
    char queryName[50];
    unsigned int querySeqLength = 0;
    struct sqlConnection *conn = hAllocConn();
    struct bed *blastpTrack;
    struct blastTab *blastpHitsList;
    struct slName *cladeList;
    struct slName *thisClade;
    char genome[100];
    char clade[50];
 
    cartWebStart(cart, "%s", "BlastP Alignment Hits");
    
    blastpTrack = getBlastpTrackRecord(conn, tdb, targetName);
    printQueryGeneInfo(conn, blastpTrack, queryName, &querySeqLength);

    blastpHitsList = loadBlastpHits(conn, queryName);
    
    cladeList = getAllClades(conn);
    getGenomeClade(conn, database, genome, clade);

    printf("<br><b>Jump to:</b> [<a href=\"#%s\">%sl proteins</a>]", clade, clade);
    thisClade = cladeList;
    while (thisClade != NULL)
    {
        if (differentWord(thisClade->name, clade))
            printf(" [<a href=\"#%s\">%sl proteins</a>]", thisClade->name, thisClade->name);
        thisClade = thisClade->next;
    }
    printf("<br>\n");
    
    printBlastpResult(conn, blastpHitsList, clade, querySeqLength);
    
    thisClade = cladeList;
    while (thisClade != NULL)
    {
        if (differentWord(thisClade->name, clade))
            printBlastpResult(conn, blastpHitsList, thisClade->name, querySeqLength);
        thisClade = thisClade->next;
    }

    printf("<BR>Note: All measurements are counted by the number of amino acids.<BR>\n");
    printf("<hr>\n");
    hFreeConn(&conn);
    printTrackHtml(tdb);
}

void doWiki(struct trackDb *tdb, char *itemName)
{
  char strand[2]; 

  printf("<HEAD>");

  if(startsWith("Make", itemName))
  {
    strand[0] = itemName[strlen(itemName)-1];
    strand[1] = 0;
    printf("<META HTTP-EQUIV=\"REFRESH\" content=\"0; URL=http://cali.cse.ucsc.edu/wikiea/index.php/BED:%s:%s:%d-%d:%s\"</META>", database, seqName, winStart, winEnd, strand);
  }
  else
  {   
    printf("<META HTTP-EQUIV=\"REFRESH\" content=\"0; URL=http://cali.cse.ucsc.edu/wikiea/index.php/BED:%s:%s:%s\"</META>", database, seqName, itemName);
  }

  printf("</HEAD>");
}

bool loweLabClick(char *track, char *item, struct trackDb *tdb)
/* check if we have one of the lowelab tracks */
{
if (sameWord(track, "gbProtCode"))
    {
    llDoCodingGenes(tdb, item,"gbProtCodePep","gbProtCodeXra");
    }
else if (sameWord(track, "refSeq"))
    {
    doRefSeq(tdb, item,"gbProtCodePep","gbProtCodeXra");
    }
else if (sameWord(track, "sargassoSea"))
    {
    doSargassoSea(tdb, item);
    }
else if (sameWord(track, "tigrCmrORFs"))
    {
    doTigrCmrGene(tdb,item);
    }
else if (sameWord(track, "jgiGene"))
    {
    doJgiGene(tdb,item);
    }
else if (sameWord(track, "tigrOperons"))
    {
    doTigrOperons(tdb,item);
    }
else if (sameWord(track,"codeBlast"))
    {
    doCodeBlast(tdb, item);
    }
else if (sameWord(track,"gbRNAs"))
    {
    doGbRnaGenes(tdb, item);
    }
else if (sameWord(track,"tRNAs"))
    {
    doTrnaGenes(tdb, item);
    }
else if (sameWord(track,"snoRNAs"))
    {
    doSnornaGenes(tdb, item);
    }
else if (sameWord(track,"easyGene"))
    {
    doEasyGenes(tdb, item);
    }
else if (startsWith("BlastP_", track)
        && differentWord(track, "BlastP_Crenarchaea") && differentWord(track, "BlastP_Nanoarch") && differentWord(track, "BlastP_Euryarch")
        && differentWord(track, "BlastP_Bacteria"))
    {
    doBlastP(tdb, item);
    }
else if (sameWord(track,"wiki"))
   {
   doWiki(tdb,item);
   }  
else 
    return FALSE;
return TRUE;
}
