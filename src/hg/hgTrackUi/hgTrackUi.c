#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "jsHelper.h"
#include "trackDb.h"
#include "hgTrackUi.h"
#include "hdb.h"
#include "hCommon.h"
#include "hui.h"
#include "ldUi.h"
#include "snpUi.h"
#include "snp125Ui.h"
#include "snp125.h"
#include "sample.h"
#include "wiggle.h"
#include "hgMaf.h"
#include "obscure.h"
#include "chainCart.h"
#include "chainDb.h"
#include "gvUi.h"
#include "oregannoUi.h"
#include "chromGraph.h"
#include "hgConfig.h"
#include "customTrack.h"
#include "dbRIP.h"
#include "tfbsConsSites.h"
#include "hapmapSnps.h"
#include "nonCodingUi.h"
#include "expRecord.h"
#include "wikiTrack.h"
#include "pcrResult.h"
#include "dgv.h"
#include "transMapStuff.h"

#ifdef USE_BAM
#include "bamFile.h"
#endif

#define MAIN_FORM "mainForm"
#define WIGGLE_HELP_PAGE  "../goldenPath/help/hgWiggleTrackHelp.html"

static char const rcsid[] = "$Id: hgTrackUi.c,v 1.509.2.4 2009/12/17 08:38:31 kent Exp $";

struct cart *cart = NULL;	/* Cookie cart with UI settings */
char *database = NULL;		/* Current database. */
char *chromosome = NULL;	        /* Chromosome. */

void superTrackUi(struct trackDb *superTdb);



/* Even more of a mess. */



void tfbsConsSitesUi(struct trackDb *tdb)
{
float tfbsConsSitesCutoff =
    sqlFloat(cartUsualString(cart,TFBS_SITES_CUTOFF,TFBS_SITES_CUTOFF_DEFAULT));
printf("<BR><B>Z score cutoff (default %s, minimum %s):&nbsp;</B>",
	TFBS_SITES_CUTOFF_DEFAULT, TFBS_SITES_CUTOFF_MINIMUM);
cgiMakeDoubleVar(TFBS_SITES_CUTOFF,tfbsConsSitesCutoff,5);
}

void stsMapUi(struct trackDb *tdb)
/* Put up UI stsMarkers. */
{
char *stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
char *stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
filterButtons("stsMap.filter", stsMapFilter, TRUE);
printf(" ");
smoeDropDown("stsMap.type", stsMapMap);
}

void stsMapMouseUi(struct trackDb *tdb)
/* Put up UI stsMapMouse. */
{
char *stsMapMouseFilter = cartUsualString(cart, "stsMapMouse.filter", "blue");
char *stsMapMouseMap = cartUsualString(cart, "stsMapMouse.type", smmoeEnumToString(0));
filterButtons("stsMapMouse.filter", stsMapMouseFilter, TRUE);
printf(" ");
smmoeDropDown("stsMapMouse.type", stsMapMouseMap);
}

void stsMapRatUi(struct trackDb *tdb)
/* Put up UI stsMapRat. */
{
char *stsMapRatFilter = cartUsualString(cart, "stsMapRat.filter", "blue");
char *stsMapRatMap = cartUsualString(cart, "stsMapRat.type", smroeEnumToString(0));
filterButtons("stsMapRat.filter", stsMapRatFilter, TRUE);
printf(" ");
smroeDropDown("stsMapRat.type", stsMapRatMap);
}

void snpMapTypeFilterButtons(char *filterTypeVar, char *filterTypeVal)
/* Put up some filter buttons. */
{
radioButton(filterTypeVar, filterTypeVal, "include");
radioButton(filterTypeVar, filterTypeVal, "exclude");
}

void snpColorFilterButtons(char *filterTypeVar, char *filterTypeVal)
/* Put up some filter buttons. */
{
int i;
for (i=0; i<snpColorSourceLabelsSize; i++)
    {
    cgiMakeRadioButton(filterTypeVar, snpColorSourceStrings[i],
		       sameString(snpColorSourceStrings[i], filterTypeVal));
    printf("%s &nbsp;", snpColorSourceLabels[i]);
    }
printf("<BR>\n");
}

void snpFilterButtons(char *filterTypeVar, char *filterTypeVal)
/* Put up some filter buttons. */
{
printf("&nbsp;&nbsp;&nbsp;&nbsp;");
cgiMakeRadioButton(filterTypeVar, "exclude", sameString("exclude", filterTypeVal));
printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>|</B>&nbsp;&nbsp;");
radioButton(filterTypeVar, filterTypeVal, "red");
radioButton(filterTypeVar, filterTypeVal, "green");
radioButton(filterTypeVar, filterTypeVal, "blue");
radioButton(filterTypeVar, filterTypeVal, "black");
}

void snpMapUi(struct trackDb *tdb)
/* Put up UI snpMap data. */
{
int   snpMapSource = 0;
int   snpMapType = 0;

if (strncmp(database,"hg",2))
    return;
printf("<BR><B>Variant Sources:</B><BR>\n");
for (snpMapSource=0; snpMapSource<snpMapSourceCartSize; snpMapSource++)
    {
    snpMapSourceCart[snpMapSource] =
	cartUsualString(cart, snpMapSourceStrings[snpMapSource], snpMapSourceDefault[snpMapSource]);
    snpFilterButtons(snpMapSourceStrings[snpMapSource], snpMapSourceCart[snpMapSource]);
    printf(" - <B>%s</B><BR>\n", snpMapSourceLabels[snpMapSource]);
    }
printf("<BR><B>Variant Types:</B><BR>\n");
for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    {
    snpMapTypeCart[snpMapType] =
	cartUsualString(cart, snpMapTypeStrings[snpMapType], snpMapTypeDefault[snpMapType]);
    snpMapTypeFilterButtons(snpMapTypeStrings[snpMapType], snpMapTypeCart[snpMapType]);
    printf(" - <B>%s</B><BR>\n", snpMapTypeLabels[snpMapType]);
    }
}

/* A comment for the purposes of brancht-tag-move demo. */

void snp125OfferGeneTracksForFunction(struct trackDb *tdb)
{
struct sqlConnection *conn = hAllocConn(database);
struct slName *genePredTables = hTrackTablesOfType(conn, "genePred%%"), *gt;
if (genePredTables != NULL)
    {
    struct trackDb *geneTdbList = NULL, *gTdb;
    for (gt = genePredTables;  gt != NULL;  gt = gt->next)
	{
	gTdb = hTrackDbForTrack(database, gt->name);
	if (gTdb && sameString(gTdb->grp, "genes"))
	    {
	    if (gTdb->parent)
		gTdb->priority = (gTdb->parent->priority + gTdb->priority/1000);
	    slAddHead(&geneTdbList, gTdb);
	    }
	}
    slSort(&geneTdbList, trackDbCmp);
    printf("<BR><B>On details page, show function and coding differences relative to: </B> ");
    char cartVar[256];
    safef(cartVar, sizeof(cartVar), "%s_geneTrack", tdb->tableName);
    jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
    jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
    struct slName *selectedGeneTracks = cartOptionalSlNameList(cart, cartVar);
    if (selectedGeneTracks == NULL)
	{
	char *defaultGeneTracks = trackDbSetting(tdb, "defaultGeneTracks");
	if (isNotEmpty(defaultGeneTracks))
	    selectedGeneTracks = slNameListFromComma(defaultGeneTracks);
	}
    int numCols = 4, i;
    int menuSize = slCount(geneTdbList);
    char **values = needMem(menuSize*sizeof(char *));
    char **labels = needMem(menuSize*sizeof(char *));
    for (i = 0, gTdb = geneTdbList;  i < menuSize && gTdb != NULL;  i++, gTdb = gTdb->next)
	{
	values[i] = gTdb->tableName;
	labels[i] = gTdb->shortLabel;
	}
    cgiMakeCheckboxGroupWithVals(cartVar, labels, values, menuSize, selectedGeneTracks, numCols);
    cgiMakeHiddenVar(cartVar, "persistentShadow");
    }
hFreeConn(&conn);
}

#define SNP125_FILTER_COLUMNS 4
#define SNP125_SET_ALL "snp125SetAll"
#define SNP125_CLEAR_ALL "snp125ClearAll"
#define SNP125_DEFAULTS "snp125Defaults"

void snp125PrintFilterColGroup()
/* Print the fancy COLGROUP for the table enclosing all filter checkbox
 * groups. */
{
int i;
/* Each colgroup has a skinny column for checkboxes followed by a proportional
 * column for labels. */
for (i = 0;  i < SNP125_FILTER_COLUMNS;  i++)
    printf("<COLGROUP><COL><COL width=\"%d%%\"></COLGROUP>\n",
	   round(100 / SNP125_FILTER_COLUMNS));
}

void snp125PrintFilterControls(char *attributeName,
			       char *vars[], char *labels[], boolean checked[],
			       boolean defaults[], int varCount)
/* Print two or more rows (attribute name header and row(s) of checkboxes)
 * of a table displaying snp125 attribute filter checkboxes. */
{
struct slName *varList = slNameListFromStringArray(vars, varCount);
char buttonVar[1024];
int i;
printf("<TR><TD colspan=%d><B>%s:</B>&nbsp;\n",
       SNP125_FILTER_COLUMNS*2, attributeName);
safef(buttonVar, sizeof(buttonVar), "%s_%s", SNP125_SET_ALL, attributeName);
stripChar(buttonVar, ' ');
jsMakeSetClearButton(cart, MAIN_FORM, buttonVar, JS_SET_ALL_BUTTON_LABEL, "",
		     varList, NULL, TRUE, TRUE);
printf("&nbsp;\n");
safef(buttonVar, sizeof(buttonVar), "%s_%s", SNP125_CLEAR_ALL, attributeName);
stripChar(buttonVar, ' ');
jsMakeSetClearButton(cart, MAIN_FORM, buttonVar, JS_CLEAR_ALL_BUTTON_LABEL, "",
		     varList, NULL, TRUE, FALSE);
printf("</TD></TR>\n");
for (i=0; i < varCount; i++)
    {
    checked[i] = cartUsualBoolean(cart, vars[i], defaults[i]);
    if (i % SNP125_FILTER_COLUMNS == 0)
	{
	if (i > 0)
	    printf("</TR>\n");
	printf("<TR>");
	}
    printf("<TD>");
    cgiMakeCheckBox(vars[i], checked[i]);
    printf("</TD><TD>%s</TD>\n", labels[i]);
    }
printf("</TR>\n");
}

void snp125PrintColorSpec(char *vars[], char *labels[], char *selected[],
			  char *defaults[], int varCount)
/* Print a table displaying snp125 attribute color selects. */
{
int i;
printf("<TABLE border=0 cellspacing=0 cellpadding=1>\n");
for (i=0; i < varCount; i++)
    {
    if (i % SNP125_FILTER_COLUMNS == 0)
	{
	if (i > 0)
	    printf("</TR>\n");
	printf("<TR>");
	}
    printf("<TD align=right>%s</TD><TD>", labels[i]);
    selected[i] = cartUsualString(cart, vars[i], defaults[i]);
    cgiMakeDropListWithVals(vars[i], snp125ColorLabel, snp125ColorLabel,
			    snp125ColorLabelSize, selected[i]);
    printf("</TD><TD>&nbsp;&nbsp;&nbsp;</TD>");
    }
printf("</TABLE>\n");
}

void cartSetStringArray(struct cart *cart, char *vars[], char *defaults[],
			int varCount)
/* Given parallel arrays of variable names and default values, set those
 * cart variables to the default values.  If a NULL is encountered in
 * vars[], assume vars[] is NULL-terminated even if varCount has not
 * been reached. */
{
if (vars == NULL)
    return;
int i;
for (i = 0;  i < varCount;  i++)
    {
    if (vars[i] == NULL)
	break;
    cartSetString(cart, vars[i], defaults[i]);
    }
}

void snp125Ui(struct trackDb *tdb)
/* UI for dbSNP version 125 and later. */
{
char autoSubmit[2048];
char *orthoTable = snp125OrthoTable(tdb, NULL);
int version = snpVersion(tdb->tableName);

if (version < 130)
    snp125ValidLabelsSize--; // no by-1000genomes

if (isNotEmpty(orthoTable) && hTableExists(database, orthoTable))
    {
    snp125ExtendedNames = cartUsualBoolean(cart, "snp125ExtendedNames", FALSE);
    printf("<BR><B>Include Chimp state and observed human alleles in name: </B>&nbsp;");
    cgiMakeCheckBox("snp125ExtendedNames",snp125ExtendedNames);
    printf("<BR>(If enabled, chimp allele is displayed first, then '>', then human alleles). </B>&nbsp;");
    printf("<BR>\n");
    }

snp125OfferGeneTracksForFunction(tdb);

snp125AvHetCutoff = atof(cartUsualString(cart, "snp125AvHetCutoff", "0"));
printf("<BR><B>Minimum <A HREF=\"#AvHet\">Average Heterozygosity</A>:</B>&nbsp;");
cgiMakeDoubleVar("snp125AvHetCutoff",snp125AvHetCutoff,6);

snp125WeightCutoff = atoi(cartUsualString(cart, "snp125WeightCutoff", "3"));
printf("<BR><B>Maximum <A HREF=\"#Weight\">Weight</A>:</B>&nbsp;");
cgiMakeIntVar("snp125WeightCutoff",snp125WeightCutoff,4);
printf("<I>SNPs with higher weights are less reliable</I><BR><BR>\n");

printf("<A name=\"filterControls\"><HR>\n"
       "<B>Filter by Attribute</B><BR>\n"
       "Check the boxes below to include SNPs with those attributes.  "
       "In order to be displayed, a SNP must pass the filter for each "
       "category.  \n"
       "Some assemblies may not contain any SNPs that have some of the "
       "listed attributes.\n"
       "<BR><BR>\n");

printf("<TABLE border=0 cellspacing=0 cellpadding=0>\n");
snp125PrintFilterColGroup();
if (version <= 127)
    snp125PrintFilterControls("Location Type", snp125LocTypeIncludeStrings,
			 snp125LocTypeLabels, snp125LocTypeIncludeCart,
			 snp125LocTypeIncludeDefault, snp125LocTypeLabelsSize);
snp125PrintFilterControls("Class", snp125ClassIncludeStrings,
			  snp125ClassLabels, snp125ClassIncludeCart,
			  snp125ClassIncludeDefault, snp125ClassLabelsSize);
snp125PrintFilterControls("Validation", snp125ValidIncludeStrings,
			  snp125ValidLabels, snp125ValidIncludeCart,
			  snp125ValidIncludeDefault, snp125ValidLabelsSize);
int size = (version < 130) ? snp125FuncLabelsSize : (snp125FuncLabelsSize - 1);
snp125PrintFilterControls("Function", snp125FuncIncludeStrings,
			  snp125FuncLabels, snp125FuncIncludeCart,
			  snp125FuncIncludeDefault, size);
snp125PrintFilterControls("Molecule Type", snp125MolTypeIncludeStrings,
			  snp125MolTypeLabels, snp125MolTypeIncludeCart,
			  snp125MolTypeIncludeDefault, snp125MolTypeLabelsSize);
printf("</TABLE><BR>\n");


jsInit();
safef(autoSubmit, sizeof(autoSubmit), "onchange=\""
      "document."MAIN_FORM".action = '%s'; %s"
      "document."MAIN_FORM".submit();\"",
      cgiScriptName(), jsSetVerticalPosition(MAIN_FORM));
cgiContinueHiddenVar("g");
cgiContinueHiddenVar("c");

/* The actual set defaults button is below, but we need to handle it here: */
char defaultButton[1024];
safef(defaultButton, sizeof(defaultButton), "%s_coloring", SNP125_DEFAULTS);
stripChar(defaultButton, ' ');
boolean defaultColoring = isNotEmpty(cgiOptionalString(defaultButton));
if (defaultColoring)
    {
    cartSetString(cart,
		  snp125ColorSourceDataName[0], snp125ColorSourceDefault[0]);
    cartSetStringArray(cart, snp125LocTypeStrings, snp125LocTypeDefault,
		       snp125LocTypeLabelsSize);
    cartSetStringArray(cart, snp125ClassStrings, snp125ClassDefault,
		       snp125ClassLabelsSize);
    cartSetStringArray(cart, snp125ValidStrings, snp125ValidDefault,
		       snp125ValidLabelsSize);
    cartSetStringArray(cart, snp125FuncStrings, snp125FuncDefault,
		       snp125FuncLabelsSize);
    cartSetStringArray(cart, snp125MolTypeStrings, snp125MolTypeDefault,
		       snp125MolTypeLabelsSize);
    }
/* Something in the middle of a function. */
printf("<A name=\"colorSpec\"><HR>\n");
printf("<B>SNP Feature for Color Specification:</B>\n");
snp125ColorSourceCart[0] = cartUsualString(cart, snp125ColorSourceDataName[0],
					   snp125ColorSourceDefault[0]);
if (version <= 127)
    cgiMakeDropListFull(snp125ColorSourceDataName[0], snp125ColorSourceLabels,
			snp125ColorSourceLabels, snp125ColorSourceLabelsSize,
			snp125ColorSourceCart[0], autoSubmit);
else
    {
    if (stringArrayIx(snp125ColorSourceCart[0], snp128ColorSourceLabels,
		      snp128ColorSourceLabelsSize) < 0)
	snp125ColorSourceCart[0] = snp125ColorSourceDefault[0];
    cgiMakeDropListFull(snp125ColorSourceDataName[0], snp128ColorSourceLabels,
			snp128ColorSourceLabels, snp128ColorSourceLabelsSize,
			snp125ColorSourceCart[0], autoSubmit);
    }
printf("&nbsp;\n");
char javascript[2048];
safef(javascript, sizeof(javascript),
      "document."MAIN_FORM".action='%s'; %s document."MAIN_FORM".submit();",
      cgiScriptName(), jsSetVerticalPosition(MAIN_FORM));
cgiMakeOnClickSubmitButton(javascript, defaultButton, JS_DEFAULTS_BUTTON_LABEL);
printf("<BR><BR>\n");
printf("The selected feature above has the following values below.  \n");
printf("For each value, a selection of colors is available.\n");
printf("If a SNP has more than one of these properties, resulting in\n");
printf("more than one color, then the stronger color will override the\n");
printf("weaker color.  In order from strongest to weakest, the colors are\n");
printf("red, green, blue, gray, black.<BR><BR>\n");

if (sameString(snp125ColorSourceCart[0], "Location Type"))
    {
    if (version <= 127)
	snp125PrintColorSpec(snp125LocTypeStrings, snp125LocTypeLabels,
			     snp125LocTypeCart, snp125LocTypeDefault,
			     snp125LocTypeLabelsSize);
    }
else if (sameString(snp125ColorSourceCart[0], "Class"))
    snp125PrintColorSpec(snp125ClassStrings, snp125ClassLabels,
			 snp125ClassCart, snp125ClassDefault,
			 snp125ClassLabelsSize);
else if (sameString(snp125ColorSourceCart[0], "Validation"))
    snp125PrintColorSpec(snp125ValidStrings, snp125ValidLabels,
			 snp125ValidCart, snp125ValidDefault,
			 snp125ValidLabelsSize);
else if (sameString(snp125ColorSourceCart[0], "Function"))
    snp125PrintColorSpec(snp125FuncStrings, snp125FuncLabels,
			 snp125FuncCart, snp125FuncDefault,
			 snp125FuncLabelsSize);
else if (sameString(snp125ColorSourceCart[0], "Molecule Type"))
    snp125PrintColorSpec(snp125MolTypeStrings, snp125MolTypeLabels,
			 snp125MolTypeCart, snp125MolTypeDefault,
			 snp125MolTypeLabelsSize);
printf("<HR>\n");
}

void snpUi(struct trackDb *tdb)
/* Put up UI snp data. */
{
int   snpSource  = 0;
int   snpMolType = 0;
int   snpClass   = 0;
int   snpValid   = 0;
int   snpFunc    = 0;
int   snpLocType = 0;

/* It would be nice to add a 'reset' button here to reset the snp
 * variables to their defaults.
 * I'd also like to see 'submit' buttons at several places along the
 * page, as the page is very tall and scrolling is tedious. */

printf("<BR><B>Colors and Filters:</B>\n");
printf("<BR>Use the &quot;Color Specification&quot; buttons to specify a group to direct coloring for the track display.\n");
printf("<BR>Variants can optionally be excluded based on their values in each of the subsequent categories by choosing &quot;exclude&quot;.\n");
printf("<BR>\n");

printf("<BR><B>Color Specification:</B><BR>\n");
snpColorSourceCart[0] = cartUsualString(cart, snpColorSourceDataName[0], snpColorSourceDefault[0]);
snpColorFilterButtons(snpColorSourceDataName[0], snpColorSourceCart[0]);

snpAvHetCutoff = atof(cartUsualString(cart, "snpAvHetCutoff", "0"));
printf("<BR><B>Minimum <A HREF=\"#AvHet\">Average Heterozygosity</A>:</B>&nbsp;");
cgiMakeDoubleVar("snpAvHetCutoff",snpAvHetCutoff,6);

printf("<BR><BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#Source\">Sources</A>:</B><BR>\n");
for (snpSource=0; snpSource < snpSourceCartSize; snpSource++)
    {
    snpSourceCart[snpSource] = cartUsualString(cart, snpSourceStrings[snpSource], snpSourceDefault[snpSource]);
    snpFilterButtons(snpSourceStrings[snpSource], snpSourceCart[snpSource]);
    printf(" - <B>%s</B><BR>\n", snpSourceLabels[snpSource]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#MolType\">Molecule Types</A>:</B><BR>\n");
for (snpMolType=0; snpMolType<snpMolTypeCartSize; snpMolType++)
    {
    snpMolTypeCart[snpMolType] = cartUsualString(cart, snpMolTypeStrings[snpMolType], snpMolTypeDefault[snpMolType]);
    snpFilterButtons(snpMolTypeStrings[snpMolType], snpMolTypeCart[snpMolType]);
    printf(" - <B>%s</B><BR>\n", snpMolTypeLabels[snpMolType]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#Class\">Variant Classes</A>:</B><BR>\n");
for (snpClass=0; snpClass<snpClassCartSize; snpClass++)
    {
    snpClassCart[snpClass] = cartUsualString(cart, snpClassStrings[snpClass], snpClassDefault[snpClass]);
    snpFilterButtons(snpClassStrings[snpClass], snpClassCart[snpClass]);
    printf(" - <B>%s</B><BR>\n", snpClassLabels[snpClass]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#Valid\">Validation Status</A>:</B><BR>\n");
for (snpValid=0; snpValid<snpValidCartSize; snpValid++)
    {
    snpValidCart[snpValid] = cartUsualString(cart, snpValidStrings[snpValid], snpValidDefault[snpValid]);
    snpFilterButtons(snpValidStrings[snpValid], snpValidCart[snpValid]);
    printf(" - <B>%s</B><BR>\n",snpValidLabels[snpValid]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#Func\">Functional Classes<A>:</B><BR>\n");
for (snpFunc=0; snpFunc<snpFuncCartSize; snpFunc++)
    {
    snpFuncCart[snpFunc] = cartUsualString(cart, snpFuncStrings[snpFunc], snpFuncDefault[snpFunc]);
    snpFilterButtons(snpFuncStrings[snpFunc], snpFuncCart[snpFunc]);
    printf(" - <B>%s</B><BR>\n",snpFuncLabels[snpFunc]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#LocType\">Location Type</A>:</B><BR>\n");
for (snpLocType=0; snpLocType<snpLocTypeCartSize; snpLocType++)
    {
    snpLocTypeCart[snpLocType] = cartUsualString(cart, snpLocTypeStrings[snpLocType], snpLocTypeDefault[snpLocType]);
    snpFilterButtons(snpLocTypeStrings[snpLocType], snpLocTypeCart[snpLocType]);
    printf(" - <B>%s</B><BR>\n",snpLocTypeLabels[snpLocType]);
    }
}

void ldUi(struct trackDb *tdb)
/* Put up UI snp data. */
{
char var[512];
char *val;

/* It would be nice to add a 'reset' button to reset the ld variables to their defaults. */

printf("<BR><B>LD Values:</B><BR>&nbsp;&nbsp;\n");

safef(var, sizeof(var), "%s_val", tdb->tableName);
val = cartUsualString(cart,  var, ldValDefault);
cgiMakeRadioButton(var, "rsquared", sameString("rsquared", val));
printf("&nbsp;r<sup>2</sup>&nbsp;&nbsp;");
cgiMakeRadioButton(var, "dprime",   sameString("dprime",   val));
printf("&nbsp;D'&nbsp;&nbsp;");
cgiMakeRadioButton(var, "lod",      sameString("lod",      val));
printf("&nbsp;LOD<BR>");

printf("<BR><B>Track Geometry:</B><BR>&nbsp;&nbsp;\n");

safef(var, sizeof(var), "%s_trm", tdb->tableName);
cgiMakeCheckBox(var, cartUsualBoolean(cart, var, ldTrmDefault));
printf("&nbsp;Trim to triangle<BR>\n");

if (tdbIsComposite(tdb))
    {
    printf("<BR>&nbsp;&nbsp;&nbsp;");
    struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(tdb->subtracks);
    slSort(tdbRefList, trackDbRefCmp);
    struct slRef *tdbRef;
    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
	{
	struct trackDb *subTdb = tdbRef->val;
	if (hTableExists(database, subTdb->tableName))
	    {
	    safef(var, sizeof(var), "%s_inv", subTdb->tableName);
	    cgiMakeCheckBox(var, cartUsualBoolean(cart, var, ldInvDefault));
	    printf("&nbsp;Invert display for %s<BR>&nbsp;&nbsp;\n",
		   subTdb->longLabel);
	    }
	}
    slFreeList(&tdbRefList);
    }
else
    {
    safef(var, sizeof(var), "%s_inv", tdb->tableName);
    printf("&nbsp;&nbsp;&nbsp;");
    cgiMakeCheckBox(var, cartUsualBoolean(cart, var, ldInvDefault));
    printf("&nbsp;Invert the display<BR>&nbsp;&nbsp;\n");
    }
printf("<BR><B>Colors:</B>\n");

safef(var, sizeof(var), "%s_pos", tdb->tableName);
val = cartUsualString(cart, var, ldPosDefault);
printf("<TABLE>\n ");
printf("<TR>\n  <TD>&nbsp;&nbsp;LD values&nbsp;&nbsp;</TD>\n  <TD>- ");
radioButton(var, val, "red");
printf("</TD>\n  <TD>");
radioButton(var, val, "green");
printf("</TD>\n  <TD>");
radioButton(var, val, "blue");
printf("</TD>\n </TR>\n ");

safef(var, sizeof(var), "%s_out", tdb->tableName);
val = cartUsualString(cart, var, ldOutDefault);
printf("<TR>\n  <TD>&nbsp;&nbsp;Outlines&nbsp;&nbsp;</TD>\n  <TD>- ");
radioButton(var, val, "red");
printf("</TD>\n  <TD>");
radioButton(var, val, "green");
printf("</TD>\n  <TD>");
radioButton(var, val, "blue");
printf("</TD>\n  <TD>");
radioButton(var, val, "yellow");
printf("</TD>\n  <TD>");
radioButton(var, val, "black");
printf("</TD>\n  <TD>");
radioButton(var, val, "white");
printf("</TD>\n  <TD>");
radioButton(var, val, "none");
printf("</TD>\n </TR>\n ");
printf("</TABLE>\n");

if (tdb->type && sameString(tdb->type, "ld2"))
    {
    safef(var, sizeof(var), "%s_gap", tdb->tableName);
    printf("&nbsp;&nbsp;");
    cgiMakeCheckBox(var, cartUsualBoolean(cart, var, ldGapDefault));
    printf("&nbsp;In dense mode, shade gaps between markers by T-int<BR>\n");
    }

if (tdbIsComposite(tdb))
    printf("<BR><B>Populations:</B>\n");
}

void oregannoUi (struct trackDb *tdb)
/* print the controls */
{
int i = 0; /* variable to walk through array */

printf("<BR><B>Type of region to display: </B> ");
printf("&nbsp;(Click <A HREF=\"http://www.oreganno.org/oregano/help/records.html\" target=\"_blank\">here</A> for detailed information on these element types)<BR>");
for (i = 0; i < oregannoTypeSize; i++)
    {
    cartMakeCheckBox(cart, oregannoTypeString[i], TRUE);
    printf (" %s<BR>", oregannoTypeLabel[i]);
    }
}

void labelMakeCheckBox(struct trackDb *tdb, char *sym, char *desc, boolean dflt)
/* add a checkbox use to choose labels to enable. */
{
char varName[64];
safef(varName, sizeof(varName), "%s.label.%s", tdb->tableName, sym);
boolean option = cartUsualBoolean(cart, varName, dflt);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", desc);
}

void gvIdControls (struct trackDb *tdb)
/* print the controls for the label choice */
{
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "hgvs", "HGVS name", FALSE);
labelMakeCheckBox(tdb, "common", "Common name", FALSE);
printf("<BR>\n");
}

void gvUi(struct trackDb *tdb)
/* print UI for human mutation filters */
{
int i = 0; /* variable to walk through arrays */
//char *def;

gvIdControls(tdb);

/*
printf("<BR><B>Exclude data source</B><BR>");
for (i = 0; i < gvSrcSize; i++)
    {
    if (differentString(gvSrcDbValue[i], "LSDB"))
        {
        if (sameString(gvSrcDbValue[i], "UniProt (Swiss-Prot/TrEMBL)"))
            {
            // exclude Swiss-Prot data by default, can be misleading
            cartMakeCheckBox(cart, gvSrcString[i], TRUE);
            }
        else
            {
            cartMakeCheckBox(cart, gvSrcString[i], FALSE);
            }
        printf (" %s<BR>", gvSrcDbValue[i]); // label with db value
        }
    else
        {
        cartMakeCheckBox(cart, gvSrcString[i], FALSE);
        printf (" Locus Specific Databases<BR>");
        }
    }
*/
printf("<BR><B>Exclude</B><BR>");
for (i = 0; i < gvAccuracySize; i++)
    {
    cartMakeCheckBox(cart, gvAccuracyString[i], FALSE);
    printf (" %s<BR>", gvAccuracyLabel[i]);
    }

printf("<BR><B>Exclude mutation type</B><BR>");
for (i = 0; i < gvTypeSize; i++)
    {
    cartMakeCheckBox(cart, gvTypeString[i], FALSE);
    printf (" %s<BR>", gvTypeLabel[i]);
    }

printf("<BR><B>Exclude mutation location</B><BR>");
for (i = 0; i < gvLocationSize; i++)
    {
    cartMakeCheckBox(cart, gvLocationString[i], FALSE);
    printf (" %s<BR>", gvLocationLabel[i]);
    }

/*
printf("<BR><B>Exclude phenotype association</B><BR>");
for (i = 0; i < gvFilterDASize; i++)
    {
    cartMakeCheckBox(cart, gvFilterDAString[i], FALSE);
    printf (" %s<BR>", gvFilterDALabel[i]);
    }
*/
printf("<BR>");
cartMakeRadioButton(cart, "gvPos.filter.colorby", "type", "type");
printf("<B>Color mutations by type</B><BR>");
for (i = 0; i < gvColorTypeSize; i++)
    {
    char *defaultVal = cartUsualString(cart, gvColorTypeStrings[i], gvColorTypeDefault[i]);
    printf (" %s ", gvColorTypeLabels[i]);
    cgiMakeDropList(gvColorTypeStrings[i], gvColorLabels, gvColorLabelSize, defaultVal);
    }

printf("<BR>");
/*
cartMakeRadioButton(cart, "gvPos.filter.colorby", "disease", "type");
printf("<B>Color mutations by phenotype association</B> (determination is described <A HREF=#phenoassoc>below</A>)<BR>");
for (i = 0; i < gvColorDASize; i++)
    {
    char *defaultVal = cartUsualString(cart, gvColorDAStrings[i], gvColorDADefault[i]);
    printf (" %s ", gvColorDALabels[i]);
    cgiMakeDropList(gvColorDAStrings[i], gvColorLabels, gvColorLabelSize, defaultVal);
    }
printf("<BR>");
*/
/* only on development page for now, but need to check in other changes */
/*
if (startsWith("hgwdev-giardine", cgiServerName()))
    {
    cartMakeRadioButton(cart, "gvPos.filter.colorby", "count", "type");
    printf("<B>Color mutations by count of positions associated with a mutation</B><BR>");
    def = cartUsualString(cart, "gvColorCountSingle", "blue");
    printf (" Single position ");
    cgiMakeDropList("gvColorCountSingle", gvColorLabels, gvColorLabelSize, def);
    def = cartUsualString(cart, "gvColorCountMult", "green");
    printf (" Multiple positions ");
    cgiMakeDropList("gvColorCountMult", gvColorLabels, gvColorLabelSize, def);
    printf("<BR>");
    }
*/
}

void retroposonsUi(struct trackDb *tdb)
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
char **row;
struct sqlResult *sr;
struct slName *sList = NULL, *item;
int menuSize = 0;
char **menu;
int i;
char *tableList[3];

i = 0;
tableList[i++] = "dbRIPAlu";
tableList[i++] = "dbRIPL1";
tableList[i++] = "dbRIPSVA";

safef(query, sizeof(query),
"SELECT genoRegion FROM dbRIPAlu GROUP BY genoRegion ORDER BY genoRegion DESC");
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    slNameAddHead(&sList, row[0]);
    }
sqlFreeResult(&sr);

menuSize = slCount(sList) + 1;
menu = needMem((size_t)(menuSize * sizeof(char *)));

i = 0;
menu[i++] = GENO_REGION_DEFAULT;
for (item = sList; item != NULL; item = item->next)
    menu[i++] = item->name;

puts("<P><B>Add filters:</B><BR><BR>\n" );
puts("<B>Gene Context:</B>&nbsp;" );
cgiMakeDropList(GENO_REGION, menu, menuSize,
    cartCgiUsualString(cart, GENO_REGION, menu[0]));
slFreeList(&sList);
freez(&menu);

menuSize = 3;
menu = needMem((size_t)(menuSize * sizeof(char *)));
i = 0;
menu[i++] = POLY_SOURCE_DEFAULT;
menu[i++] = "yes";
menu[i++] = "no";

puts("<BR><BR>\n<B>Insertion found in reference sequence:</B>&nbsp;");
cgiMakeDropList(POLY_SOURCE, menu, menuSize,
    cartCgiUsualString(cart, POLY_SOURCE, menu[0]));
freez(&menu);

for (i = 0; i < 3; ++i)
    {
    safef(query, sizeof(query),
    "SELECT polySubfamily FROM %s GROUP BY polySubfamily ORDER BY polySubfamily DESC", tableList[i]);
    sr = sqlGetResult(conn, query);

    while ((row = sqlNextRow(sr)) != NULL)
	{
	slNameStore(&sList, row[0]);
	}
    sqlFreeResult(&sr);
    }
slNameSortCase(&sList);

menuSize = slCount(sList) + 1;
menu = needMem((size_t)(menuSize * sizeof(char *)));

i = 0;
menu[i++] = POLY_SUBFAMILY_DEFAULT;
for (item = sList; item != NULL; item = item->next)
    menu[i++] = item->name;

puts("<BR><BR>\n<B>Insertion identified in sub-family:</B>&nbsp;");
cgiMakeDropList(POLY_SUBFAMILY, menu, menuSize,
    cartCgiUsualString(cart, POLY_SUBFAMILY, menu[0]));
slFreeList(&sList);
freez(&menu);

safef(query, sizeof(query),
"SELECT ethnicGroup FROM polyGenotype GROUP BY ethnicGroup ORDER BY ethnicGroup DESC");
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
{
    slNameStore(&sList, row[0]);
    }
sqlFreeResult(&sr);

menuSize = slCount(sList) + 1;
menu = needMem((size_t)(menuSize * sizeof(char *)));

i = 0;
menu[i++] = ETHNIC_GROUP_DEFAULT;
for (item = sList; item != NULL; item = item->next)
    menu[i++] = item->name;

puts("<BR><BR>\n<B>Ethnic group:</B>&nbsp;");
cgiMakeDropList(ETHNIC_GROUP, menu, menuSize,
    cartCgiUsualString(cart, ETHNIC_GROUP, menu[0]));
slFreeList(&sList);
freez(&menu);
puts("&nbsp;");

radioButton(ETHNIC_GROUP_EXCINC,
	cartUsualString(cart, ETHNIC_GROUP_EXCINC, ETHNIC_NOT_DEFAULT),
	"include");
radioButton(ETHNIC_GROUP_EXCINC,
	cartUsualString(cart, ETHNIC_GROUP_EXCINC, ETHNIC_NOT_DEFAULT),
	"exclude");
puts("<B>this ethnic group</B><BR>\n");

#ifdef HAS_NO_MEANING
menuSize = 10;
menu = needMem((size_t)(menuSize * sizeof(char *)));
i = 0;
menu[i++] = "0.0"; menu[i++] = "0.1"; menu[i++] = "0.2"; menu[i++] = "0.3";
menu[i++] = "0.4"; menu[i++] = "0.5"; menu[i++] = "0.6"; menu[i++] = "0.7";
menu[i++] = "0.8"; menu[i++] = "0.9";

/*	safety check on bad user input, they may have set them illegally
 *	in which case reset them to defaults 0.0 <= f <= 1.0
 */
double freqLow = sqlFloat(cartCgiUsualString(cart, ALLELE_FREQ_LOW, menu[0]));
double freqHi = sqlFloat(cartCgiUsualString(cart, ALLELE_FREQ_HI, menu[9]));

puts("<BR>\n<B>Restrict polymorphism frequency to:</B>&nbsp;");
if (freqLow < freqHi)
    cgiMakeDropList(ALLELE_FREQ_LOW, menu, menuSize,
	cartCgiUsualString(cart, ALLELE_FREQ_LOW, menu[0]));
else
    cgiMakeDropList(ALLELE_FREQ_LOW, menu, menuSize, menu[0]);

i = 0;
menu[i++] = "0.1"; menu[i++] = "0.2"; menu[i++] = "0.3"; menu[i++] = "0.4";
menu[i++] = "0.5"; menu[i++] = "0.6"; menu[i++] = "0.7"; menu[i++] = "0.8";
menu[i++] = "0.9"; menu[i++] = "1.0";

puts("&nbsp;&lt;=&nbsp;f&nbsp;&lt;=&nbsp;");
if (freqLow < freqHi)
    cgiMakeDropList(ALLELE_FREQ_HI, menu, menuSize,
	cartCgiUsualString(cart, ALLELE_FREQ_HI, menu[9]));
else
    cgiMakeDropList(ALLELE_FREQ_HI, menu, menuSize, menu[9]);
freez(&menu);
#endif	/*	HAS_NO_MEANING	*/

menuSize = 3;
menu = needMem((size_t)(menuSize * sizeof(char *)));
i = 0;
menu[i++] = DISEASE_DEFAULT;
menu[i++] = "yes";
menu[i++] = "no";

puts("<BR><BR>\n<B>Associated disease state known:</B>&nbsp;");
cgiMakeDropList(dbRIP_DISEASE, menu, menuSize,
    cartCgiUsualString(cart, dbRIP_DISEASE, menu[0]));
freez(&menu);

puts("\n</P>\n");
hFreeConn(&conn);
}

void cbrWabaUi(struct trackDb *tdb)
/* Put up UI cbrWaba. */
{
/*   This link is disabled in the external browser
char *cbrWabaFilter = cartUsualString(cart, "cbrWaba.filter", "red");
char *cbrWabaMap = cartUsualString(cart, "cbrWaba.type", fcoeEnumToString(0));
int start = cartInt(cart, "cbrWaba.start");
int end = cartInt(cart, "cbrWaba.end");
printf(
"<P><A HREF=\"http://genome-test.cse.ucsc.edu/cgi-bin/tracks.exe?where=%s%%3A%d-%d\"> Temporary Intronerator link: %s:%d-%d</A> <I>for testing purposes only</I> \n</P>", chromosome+3, start, end, chromosome+3, start, end );
*/
}

void fishClonesUi(struct trackDb *tdb)
/* Put up UI fishClones. */
{
char *fishClonesFilter = cartUsualString(cart, "fishClones.filter", "red");
char *fishClonesMap = cartUsualString(cart, "fishClones.type", fcoeEnumToString(0));
filterButtons("fishClones.filter", fishClonesFilter, TRUE);
printf(" ");
fcoeDropDown("fishClones.type", fishClonesMap);
}

void recombRateUi(struct trackDb *tdb)
/* Put up UI recombRate. */
{
char *recombRateMap = cartUsualString(cart, "recombRate.type", rroeEnumToString(0));
printf("<b>Map Distances: </b>");
rroeDropDown("recombRate.type", recombRateMap);
}

void recombRateRatUi(struct trackDb *tdb)
/* Put up UI recombRateRat. */
{
char *recombRateRatMap = cartUsualString(cart, "recombRateRat.type", rrroeEnumToString(0));
printf("<b>Map Distances: </b>");
rrroeDropDown("recombRateRat.type", recombRateRatMap);
}

void recombRateMouseUi(struct trackDb *tdb)
/* Put up UI recombRateMouse. */
{
char *recombRateMouseMap = cartUsualString(cart, "recombRateMouse.type", rrmoeEnumToString(0));
printf("<b>Map Distances: </b>");
rrmoeDropDown("recombRateMouse.type", recombRateMouseMap);
}

void cghNci60Ui(struct trackDb *tdb)
/* Put up UI cghNci60. */
{
char *cghNci60Map = cartUsualString(cart, "cghNci60.type", cghoeEnumToString(0));
char *col = cartUsualString(cart, "cghNci60.color", "gr");
printf(" <b>Cell Lines: </b> ");
cghoeDropDown("cghNci60.type", cghNci60Map);
printf(" ");
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("cghNci60.color", "gr", sameString(col, "gr"));
printf(" green/red ");
cgiMakeRadioButton("cghNci60.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("cghNci60.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void affyUi(struct trackDb *tdb)
/* put up UI for the affy track from stanford track */
{
char *affyMap;
char *col;
char varName[128];

safef(varName, sizeof(varName), "%s.%s", tdb->tableName, "type");
affyMap = cartUsualString(cart, varName, affyEnumToString(affyTissue));
col = cartUsualString(cart, "exprssn.color", "rg");
printf("<p><b>Experiment Display: </b> ");
affyDropDown(varName, affyMap);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void expRatioCombineDropDown(char *trackName, char *groupSettings, struct hash *allGroupings)
/* Make a drop-down of all the main combinations. */
{
int size = 0;
int i;
char **menuArray;
char **valArray;
char dropDownName[512];
struct hash *groupGroup = hashMustFindVal(allGroupings, groupSettings);
char *combineList = hashFindVal(groupGroup, "combine");
char *allSetting = hashMustFindVal(groupGroup, "all");
char *defaultSetting = hashFindVal(groupGroup, "combine.default");
char *cartSetting = NULL;
struct slName *combineNames = slNameListFromComma(combineList);
struct slName *aName;
safef(dropDownName, sizeof(dropDownName), "%s.combine", trackName);
size = slCount(combineNames) + 1;
AllocArray(menuArray, size);
AllocArray(valArray, size);
slNameAddHead(&combineNames, allSetting);
for (i = 0, aName = combineNames; i < size && aName != NULL; i++, aName = aName->next)
    {
    struct hash *oneGroupSetting = hashMustFindVal(allGroupings, aName->name);
    char *descrip = hashMustFindVal(oneGroupSetting, "description");
    menuArray[i] = descrip;
    valArray[i] = aName->name;
    }
if (defaultSetting == NULL)
    defaultSetting = allSetting;
cartSetting = cartUsualString(cart, dropDownName, defaultSetting);
printf(" <b>Combine arrays</b>: ");
cgiMakeDropListWithVals(dropDownName, menuArray, valArray,
                         size, cartSetting);
}

void expRatioDrawExonOption(struct trackDb *tdb)
/* Add option to show exons if possible. */
{
char checkBoxName[512];
char *drawExons = trackDbSetting(tdb, "expDrawExons");
boolean checked = FALSE;
if (!drawExons || differentWord(drawExons, "on"))
    return;
safef(checkBoxName, sizeof(checkBoxName), "%s.expDrawExons", tdb->tableName);
checked = cartCgiUsualBoolean(cart, checkBoxName, FALSE);
puts("<B>Draw intron lines/arrows and exons: </B> ");
cgiMakeCheckBox(checkBoxName, checked);
puts("<BR>\n");
}

void expRatioColorOption(struct trackDb *tdb)
/* Radio button for red/green or blue/yellow */
{
char radioName[256];
char *colorSetting = NULL;
boolean rgChecked  = FALSE;
boolean rgwChecked = FALSE;
safef(radioName, sizeof(radioName), "%s.color", tdb->tableName);
colorSetting = cartUsualString(cart, radioName, "redGreen");
if (sameString(colorSetting, "redGreen"))
    rgChecked = TRUE;
if (sameString(colorSetting, "redBlueOnWhite"))
    rgwChecked = TRUE;
puts("<BR><B>Color: </B> ");
cgiMakeRadioButton(radioName, "redGreen", rgChecked);
puts("red/green");
cgiMakeRadioButton(radioName, "yellowBlue", !(rgChecked || rgwChecked));
puts("yellow/blue\n");
cgiMakeRadioButton(radioName, "redBlueOnWhite", rgwChecked);
puts("red/blue on white background<BR>");
}

void expRatioUi(struct trackDb *tdb)
/* UI options for the expRatio tracks. */
{
char *groupings = trackDbRequiredSetting(tdb, "groupings");
struct hash *gHashOfHashes = NULL;
struct hash *ret =
    hgReadRa(hGenome(database), database, "hgCgiData",
	     "microarrayGroups.ra", &gHashOfHashes);
if ((ret == NULL) && (gHashOfHashes == NULL))
    errAbort("Could not get group settings for track.");
expRatioDrawExonOption(tdb);
expRatioCombineDropDown(tdb->tableName, groupings, gHashOfHashes);
expRatioColorOption(tdb);
}

void expRatioCtUi(struct trackDb *tdb)
/* UI options for array custom tracks. */
{
expRatioColorOption(tdb);
}

void affyAllExonUi(struct trackDb *tdb)
/* put up UI for the affy all exon tracks. */
{
char *affyAllExonMap;
char *col;
char varName[128];

safef(varName, sizeof(varName), "%s.%s", tdb->tableName, "type");
affyAllExonMap = cartUsualString(cart, varName, affyAllExonEnumToString(affyAllExonTissue));
col = cartUsualString(cart, "exprssn.color", "rg");
printf("<p><b>Experiment Display: </b> ");
affyAllExonDropDown(varName, affyAllExonMap);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void rosettaUi(struct trackDb *tdb)
/* put up UI for the rosetta track */
{
char *rosettaMap = cartUsualString(cart, "rosetta.type", rosettaEnumToString(0));
char *col = cartUsualString(cart, "exprssn.color", "rg");
char *exonTypes = cartUsualString(cart, "rosetta.et",  rosettaExonEnumToString(0));

printf("<p><b>Reference Sample: </b> ");
rosettaDropDown("rosetta.type", rosettaMap);
printf("  ");
printf("<b>Exons Shown:</b> ");
rosettaExonDropDown("rosetta.et", exonTypes);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void switchDbScoreUi(struct trackDb *tdb)
/* Put up UI for filtering switchDb track based on a score */
/* The scores use a drop-box to set scoreFilter at several */
/* thresholds. */
{
char *option = "switchDbTss.scoreFilter";
char *pseudo = "switchDbTss.pseudo";
char *menu[3] = {"All TSSs (no filter)", "Lower stringency (score >= 10)", "Higher stringency (score >= 20)"};
char *values[3] = {"0", "10", "20"};
char *scoreValString = trackDbSetting(tdb, "scoreFilter");
int scoreSetting;
int scoreVal = SWITCHDBTSS_FILTER;
char tempScore[256];
if (scoreValString != NULL)
    scoreVal = atoi(scoreValString);
printf("<p><b>Filter TSSs by score:</b> ");
scoreSetting = cartUsualInt(cart,  option,  scoreVal);
safef(tempScore, sizeof(tempScore), "%d",scoreSetting);
cgiMakeDropListWithVals(option, menu, values,
			ArraySize(menu), tempScore);
printf("<p><b>Include TSSs for predicted pseudogenes</b> ");
cartMakeCheckBox(cart, pseudo, FALSE);
}

void blastSGUi(struct trackDb *tdb)
{
char geneName[64];
char accName[64];
char sprotName[64];
char posName[64];
char cModeStr[64];
boolean useGene, useAcc, useSprot, usePos;
int cMode;
char *cModes[3] = {"0", "1", "2"};

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->tableName);
safef(accName, sizeof(accName), "%s.accLabel", tdb->tableName);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->tableName);
safef(posName, sizeof(posName), "%s.posLabel", tdb->tableName);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->tableName);
cMode = cartUsualInt(cart, cModeStr, 0);

printf("<P><B>Color elements: </B> ");
cgiMakeRadioButton(cModeStr, cModes[0], cMode == 0);
printf("by score ");
cgiMakeRadioButton(cModeStr, cModes[1], cMode == 1);
printf("by yeast chromosome ");
cgiMakeRadioButton(cModeStr, cModes[2], cMode == 2);
printf("black");

printf("<P><B>Label elements by: </B> ");
cgiMakeCheckBox(geneName, useGene);
printf("Yeast Gene ");
cgiMakeCheckBox(accName, useAcc);
printf("Yeast mRNA ");
cgiMakeCheckBox(sprotName, useSprot);
printf("SwissProt ID ");
cgiMakeCheckBox(posName, usePos);
printf("Yeast Position");

baseColorDrawOptDropDown(cart, tdb);
}

void blastFBUi(struct trackDb *tdb)
{
char geneName[64];
char accName[64];
char sprotName[64];
char posName[64];
char cModeStr[64];
boolean useGene, useAcc, usePos;
int cMode;
char *cModes[3] = {"0", "1", "2"};

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->tableName);
safef(accName, sizeof(accName), "%s.accLabel", tdb->tableName);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->tableName);
safef(posName, sizeof(posName), "%s.posLabel", tdb->tableName);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->tableName);
cMode = cartUsualInt(cart, cModeStr, 0);

printf("<P><B>Color elements: </B> ");
cgiMakeRadioButton(cModeStr, cModes[0], cMode == 0);
printf("by score ");
cgiMakeRadioButton(cModeStr, cModes[1], cMode == 1);
printf("by D. mel. chromosome ");
cgiMakeRadioButton(cModeStr, cModes[2], cMode == 2);
printf("black");
printf("<P><B>Label elements by: </B> ");
cgiMakeCheckBox(geneName, useGene);
printf("FlyBase Gene ");
cgiMakeCheckBox(accName, useAcc);
printf("D. melanogaster mRNA ");
cgiMakeCheckBox(posName, usePos);
printf("D. melanogaster Position");

baseColorDrawOptDropDown(cart, tdb);
}

void blastUi(struct trackDb *tdb)
{
char geneName[64];
char accName[64];
char sprotName[64];
char posName[64];
char cModeStr[64];
boolean useGene, useAcc, useSprot, usePos;
int cMode;
char *cModes[3] = {"0", "1", "2"};

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->tableName);
safef(accName, sizeof(accName), "%s.accLabel", tdb->tableName);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->tableName);
safef(posName, sizeof(posName), "%s.posLabel", tdb->tableName);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->tableName);
cMode = cartUsualInt(cart, cModeStr, 0);

printf("<P><B>Color elements: </B> ");
cgiMakeRadioButton(cModeStr, cModes[0], cMode == 0);
printf("by score ");
cgiMakeRadioButton(cModeStr, cModes[1], cMode == 1);
printf("by human chromosome ");
cgiMakeRadioButton(cModeStr, cModes[2], cMode == 2);
printf("black");

printf("<P><B>Label elements by: </B> ");
cgiMakeCheckBox(geneName, useGene);
printf("Human Gene ");
cgiMakeCheckBox(accName, useAcc);
printf("Human mRNA ");
cgiMakeCheckBox(sprotName, useSprot);
printf("UniProt(Swiss-Prot/TrEMBL) ID ");
cgiMakeCheckBox(posName, usePos);
printf("Human Position");

baseColorDrawOptDropDown(cart, tdb);
}

void hg17KgIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->tableName);
geneLabel = cartUsualString(cart, varName, "gene symbol");
printf("<B>Label:</B> ");
radioButton(varName, geneLabel, "gene symbol");
radioButton(varName, geneLabel, "UCSC Known Gene ID");
radioButton(varName, geneLabel, "all");
radioButton(varName, geneLabel, "none");
}

void hg17KgUI(struct trackDb *tdb)
/* Put up refGene-specific controls */
{
hg17KgIdConfig(tdb);
baseColorDrawOptDropDown(cart, tdb);
}

void omimGeneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->tableName);
geneLabel = cartUsualString(cart, varName, "OMIM ID");
printf("<BR><B>Label:</B> ");
radioButton(varName, geneLabel, "OMIM ID");
radioButton(varName, geneLabel, "OMIM gene or syndrome");
radioButton(varName, geneLabel, "UCSC gene symbol");
}

void knownGeneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
char *omimAvail = NULL;
safef(query, sizeof(query), "select kgXref.kgID from kgXref,refLink where kgXref.refseq = refLink.mrnaAcc and refLink.omimId != 0 limit 1");
omimAvail = sqlQuickString(conn, query);
hFreeConn(&conn);

printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "gene", "gene symbol", FALSE);
labelMakeCheckBox(tdb, "kgId", "UCSC Known Gene ID", FALSE);
labelMakeCheckBox(tdb, "prot", "UniProt Display ID", FALSE);

if (omimAvail != NULL)
    {
    char sym[32];
    safef(sym, sizeof(sym), "omim%s", cartString(cart, "db"));
    labelMakeCheckBox(tdb, sym, "OMIM ID", FALSE);
    }
printf("<BR>\n");
}

void knownGeneShowWhatUi(struct trackDb *tdb)
/* Put up line of controls that describe what parts to show. */
{
char varName[64];
printf("<B>Show:</B> ");
safef(varName, sizeof(varName), "%s.show.noncoding", tdb->tableName);
boolean option = cartUsualBoolean(cart, varName, TRUE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "non-coding genes");
safef(varName, sizeof(varName), "%s.show.spliceVariants", tdb->tableName);
option = cartUsualBoolean(cart, varName, TRUE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "splice variants");
printf("<BR>\n");
}

void knownGeneUI(struct trackDb *tdb)
/* Put up refGene-specific controls */
{
/* This is incompatible with adding Protein ID to lf->extra */
knownGeneIdConfig(tdb);
knownGeneShowWhatUi(tdb);
baseColorDrawOptDropDown(cart, tdb);
}

void omimGeneUI(struct trackDb *tdb)
/* Put up omimGene-specific controls */
{
omimGeneIdConfig(tdb);
}

void geneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->tableName);
geneLabel = cartUsualString(cart, varName, "gene");
printf("<B>Label:</B> ");
radioButton(varName, geneLabel, "gene");
radioButton(varName, geneLabel, "accession");
radioButton(varName, geneLabel, "both");
radioButton(varName, geneLabel, "none");
}

static void hideNoncodingOpt(struct trackDb *tdb)
/* Put up hide-noncoding options. */
{
/* Put up option to hide non-coding elements. */
printf("<B>Hide non-coding genes:</B> ");
char varName[64];
safef(varName, sizeof(varName), "%s.%s", tdb->tableName, HIDE_NONCODING_SUFFIX);
cartMakeCheckBox(cart, varName, HIDE_NONCODING_DEFAULT);
}

void refGeneUI(struct trackDb *tdb)
/* Put up refGene or xenoRefGene gene ID track controls, with checkboxes */
{
/* Figure out if OMIM database is available. */
int omimAvail = 0;
if (sameString(tdb->tableName, "refGene"))
    {
    struct sqlConnection *conn = hAllocConn(database);
    char query[128];
    safef(query, sizeof(query), "select refLink.omimId from refLink, refGene where refLink.mrnaAcc = refGene.name and refLink.omimId != 0 limit 1");
    omimAvail = sqlQuickNum(conn, query);
    hFreeConn(&conn);
    }

/* Put up label line  - boxes for gene, accession or maybe OMIM. */
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "gene", "gene", FALSE);
labelMakeCheckBox(tdb, "acc", "accession", FALSE);
if (omimAvail != 0)
    {
    char sym[32];
    safef(sym, sizeof(sym), "omim%s", cartString(cart, "db"));
    labelMakeCheckBox(tdb, sym, "OMIM ID", FALSE);
    }
printf("<BR>\n");

/* Put up noncoding option and codon coloring stuff. */
hideNoncodingOpt(tdb);
baseColorDrawOptDropDown(cart, tdb);
}

void transMapUI(struct trackDb *tdb)
/* Put up transMap-specific controls */
{
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "orgCommon", "common name", FALSE);
labelMakeCheckBox(tdb, "orgAbbrv", "organism abbreviation", FALSE);
labelMakeCheckBox(tdb, "db", "assembly database", FALSE);
labelMakeCheckBox(tdb, "gene", "gene", FALSE);
labelMakeCheckBox(tdb, "acc", "accession", FALSE);

baseColorDrawOptDropDown(cart, tdb);
indelShowOptions(cart, tdb);
}

void retroGeneUI(struct trackDb *tdb)
/* Put up retroGene-specific controls */
{
geneIdConfig(tdb);
baseColorDrawOptDropDown(cart, tdb);
}

void vegaGeneUI(struct trackDb *tdb)
/* Put up Vega Gene track-specific controls */
{
geneIdConfig(tdb); 
printf("<BR>\n");

/* Put up codon coloring stuff. */
baseColorDrawOptDropDown(cart, tdb);
}

void ensemblNonCodingTypeConfig(struct trackDb *tdb)
{
int i = 0;

printf("<BR><B>Non-coding RNA type:</B> ");
printf("<BR>\n");

for (i=0; i < nonCodingTypeLabelsSize; i++)
    {
    nonCodingTypeIncludeCart[i] = cartUsualBoolean(cart, nonCodingTypeIncludeStrings[i], nonCodingTypeIncludeDefault[i]);
    cgiMakeCheckBox(nonCodingTypeIncludeStrings[i], nonCodingTypeIncludeCart[i]);
    printf(" %s", nonCodingTypeLabels[i]);
    }
}

void ensemblNonCodingUI(struct trackDb *tdb)
/* Put up Ensembl Non-Coding genes-specific controls */
{
ensemblNonCodingTypeConfig(tdb);
}

void mrnaUi(struct trackDb *tdb, boolean isXeno)
/* Put up UI for an mRNA (or EST) track. */
{
struct mrnaUiData *mud = newMrnaUiData(tdb->tableName, isXeno);
struct mrnaFilter *fil;
struct controlGrid *cg = NULL;
char *filterTypeVar = mud->filterTypeVar;
char *filterTypeVal = cartUsualString(cart, filterTypeVar, "red");
char *logicTypeVar = mud->logicTypeVar;
char *logicTypeVal = cartUsualString(cart, logicTypeVar, "and");

/* Define type of filter. */
filterButtons(filterTypeVar, filterTypeVal, FALSE);
printf("  <B>Combination Logic:</B> ");
radioButton(logicTypeVar, logicTypeVal, "and");
radioButton(logicTypeVar, logicTypeVal, "or");
printf("<BR>\n");

/* List various fields you can filter on. */
printf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
cg = startControlGrid(4, NULL);
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    oneMrnaFilterUi(cg, fil->label, fil->key, cart);
endControlGrid(&cg);
baseColorDrawOptDropDown(cart, tdb);
indelShowOptions(cart, tdb);
}



static void filterByChrom(struct trackDb *tdb)
{
char *filterSetting;
char filterVar[256];
char *filterVal = "";

printf("<p><b>Filter by chromosome (e.g. chr10):</b> ");
snprintf(filterVar, sizeof(filterVar), "%s.chromFilter", tdb->tableName);
filterSetting = cartUsualString(cart, filterVar, filterVal);
cgiMakeTextVar(filterVar, cartUsualString(cart, filterVar, ""), 15);
}

void crossSpeciesUi(struct trackDb *tdb)
/* Put up UI for selecting rainbow chromosome color or intensity score. */
{
char colorVar[256];
char *colorSetting;
/* initial value of chromosome coloring option is "on", unless
 * overridden by the colorChromDefault setting in the track */
char *colorDefault = trackDbSettingOrDefault(tdb, "colorChromDefault", "on");

printf("<p><b>Color track based on chromosome:</b> ");
snprintf(colorVar, sizeof(colorVar), "%s.color", tdb->tableName);
colorSetting = cartUsualString(cart, colorVar, colorDefault);
cgiMakeRadioButton(colorVar, "on", sameString(colorSetting, "on"));
printf(" on ");
cgiMakeRadioButton(colorVar, "off", sameString(colorSetting, "off"));
printf(" off ");
printf("<br><br>");
filterByChrom(tdb);
}

void transRegCodeUi(struct trackDb *tdb)
/* Put up UI for transcriptional regulatory code - not
 * much more than score UI. */
{
printf("%s",
	"<P>The scoring ranges from 0 to 1000 and is based on the number of lines "
	"of evidence that support the motif being active.  Each of the two "
	"<I>sensu stricto</I> species in which the motif was conserved counts "
	"as a line of evidence.  If the ChIP-chip data showed good (P &le; 0.001) "
	"evidence of binding to the transcription factor associated with the "
	"motif, that counts as two lines of evidence.  If the ChIP-chip data "
	"showed weaker (P &le; 0.005) evidence of binding, that counts as just one line "
	"of evidence.  The following table shows the relationship between lines "
	"of evidence and score:");
printf("<P>");
hTableStart();
printf("%s",
   "<BLOCKQUOTE>\n"
   "<TR><TH>Evidence</TH><TH>Score</TH></TR>\n"
   "<TR><TD>4</TD><TD>1000</TD></TR>\n"
   "<TR><TD>3</TD><TD>500</TD></TR>\n"
   "<TR><TD>2</TD><TD>333</TD></TR>\n"
   "<TR><TD>1</TD><TD>250</TD></TR>\n"
   "<TR><TD>0</TD><TD>200</TD></TR>\n"
   "</BLOCKQUOTE>\n"
   );
hTableEnd();
}


void zooWiggleUi(struct trackDb *tdb )
/* put up UI for zoo track with one species on each line
 * and checkboxes to toggle each of them on/off*/
{
char options[7][256];
int thisHeightPer, thisLineGap;
float thisMinYRange, thisMaxYRange;
char *interpolate, *fill;

char **row;
int rowOffset;
struct sample *sample;
struct sqlResult *sr;

char option[64];
struct sqlConnection *conn = hAllocConn(database);

char newRow = 0;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "1000.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

printf("<p><b>Toggle Species on/off</b><br>" );
sr = hRangeQuery(conn, tdb->tableName, chromosome, 0, 1877426, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row + rowOffset);
    snprintf( option, sizeof(option), "zooSpecies.%s", sample->name );
    if( cartUsualBoolean(cart, option, TRUE ) )
	cgiMakeCheckBox(option, TRUE );
    else
	cgiMakeCheckBox(option, FALSE );
    printf("%s&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;", sample->name );

    newRow++;
    if( newRow % 5 == 0 ) printf("<br>");

    sampleFree(&sample);
    }

}

void chainColorUi(struct trackDb *tdb)
/* UI for the chain tracks */
{
boolean normScoreAvailable = chainDbNormScoreAvailable(tdb);
boolean compositeTrack = tdbIsComposite(tdb);

if (compositeTrack)
    return;	// configuration taken care of by hCompositeUi() later
else if (normScoreAvailable)
    chainCfgUi(database, cart, tdb, tdb->tableName, NULL, FALSE, chromosome);
else
    crossSpeciesUi(tdb);
}

void chromGraphUi(struct trackDb *tdb)
/* UI for the wiggle track */
{
char varName[chromGraphVarNameMaxSize];
struct sqlConnection *conn = NULL;
char *track = tdb->tableName;
if (!isCustomTrack(track))
    conn = hAllocConn(database);
double minVal,maxVal;
struct chromGraphSettings *cgs = chromGraphSettingsGet(track,
	conn, tdb, cart);

printf("<b>Track height:&nbsp;</b>");
chromGraphVarName(track, "pixels", varName);
cgiMakeIntVar(varName, cgs->pixels, 3);
printf("&nbsp;pixels&nbsp;&nbsp;(range:&nbsp;%d&nbsp;to&nbsp;%d)<BR>",
	cgs->minPixels, cgs->maxPixels);

printf("<b>Vertical viewing range</b>:&nbsp;&nbsp;\n<b>min:&nbsp;</b>");
chromGraphVarName(track, "minVal", varName);
cgiMakeDoubleVar(varName, cgs->minVal, 6);
printf("&nbsp;&nbsp;&nbsp;&nbsp;<b>max:&nbsp;</b>");
chromGraphVarName(track, "maxVal", varName);
cgiMakeDoubleVar(varName, cgs->maxVal, 6);
if (conn)
    chromGraphDataRange(track, conn, &minVal, &maxVal);
else
    {
    char *fileName = trackDbRequiredSetting(tdb, "binaryFile");
    struct chromGraphBin *cgb = chromGraphBinOpen(fileName);
    minVal = cgb->minVal;
    maxVal = cgb->maxVal;
    chromGraphBinFree(&cgb);
    }
printf("\n&nbsp; &nbsp;(range: &nbsp;%g&nbsp;to&nbsp;%g)<BR>",
    minVal, maxVal);

hFreeConn(&conn);
}

void wikiTrackUi(struct trackDb *tdb)
/* UI for wiki track user annotations */
{
return;	// currently empty
}

void rulerUi(struct trackDb *tdb)
/* UI for base position (ruler) */
{
boolean showScaleBar = cartUsualBoolean(cart, BASE_SCALE_BAR, TRUE);
boolean showRuler = cartUsualBoolean(cart, BASE_SHOWRULER, TRUE);
boolean complementsToo = cartUsualBoolean(cart, MOTIF_COMPLEMENT, FALSE);
boolean showPos = cartUsualBoolean(cart, BASE_SHOWPOS, FALSE);
boolean showAsm = cartUsualBoolean(cart, BASE_SHOWASM, FALSE);
/* title var is assembly-specific */
char titleVar[256];
char *title = NULL;
/* Configure zoom when click occurs */
char *currentZoom = cartCgiUsualString(cart, RULER_BASE_ZOOM_VAR, ZOOM_3X);
char *motifString = cartCgiUsualString(cart, BASE_MOTIFS, "");
safef(titleVar,sizeof(titleVar),"%s_%s",BASE_TITLE,database);
title = cartUsualString(cart, titleVar, "");
puts("<P>");
cgiMakeCheckBox(BASE_SHOWRULER, showRuler);
puts("&nbsp;<B>Show ruler</B></P>");
puts("<P>");
cgiMakeCheckBox(BASE_SCALE_BAR, showScaleBar);
puts("&nbsp;<B>Show scale bar</B>");
puts("<P><B>Zoom factor:&nbsp;</B>");
zoomRadioButtons(RULER_BASE_ZOOM_VAR, currentZoom);
puts("<P><B>Motifs to highlight:&nbsp;</B>");
cgiMakeTextVar(BASE_MOTIFS, motifString, 20);
puts("&nbsp;(Comma separated list, i.e.: GT,AG for splice sites)");
puts("<P>");
cgiMakeCheckBox(MOTIF_COMPLEMENT, complementsToo);
puts("&nbsp;<B>Show reverse complements of motifs also</B>");
puts("<P>Options for slides and presentations:");
puts("<P><B>Title:&nbsp;</B>");
cgiMakeTextVar(titleVar, title, 30);
puts("<P><B>Display:&nbsp;</B>");
cgiMakeCheckBox(BASE_SHOWASM, showAsm);
puts("&nbsp;<B>assembly&nbsp;</B>");
cgiMakeCheckBox(BASE_SHOWPOS, showPos);
puts("&nbsp;<B>position</B>");

}

void oligoMatchUi(struct trackDb *tdb)
/* UI for oligo match track */
{
char *oligo = cartUsualString(cart, oligoMatchVar, oligoMatchDefault);
puts("<P><B>Short (2-30 base) sequence:</B>");
cgiMakeTextVar(oligoMatchVar, oligo, 45);
}

void cutterUi(struct trackDb *tdb)
/* UI for restriction enzyme track */
{
char *enz = cartUsualString(cart, cutterVar, cutterDefault);
puts("<P><B>Enzymes (separate with commas):</B><BR>");
cgiMakeTextVar(cutterVar, enz, 100);
}

void genericWiggleUi(struct trackDb *tdb, int optionNum )
/* put up UI for any standard wiggle track (a.k.a. sample track)*/
{

char options[7][256];
int thisHeightPer, thisLineGap;
float thisMinYRange, thisMaxYRange;
char *interpolate, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "1000.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

if( optionNum >= 7 )
    {
    printf("<p><b>Maximum Interval to Interpolate Across</b>:&nbsp;&nbsp;");
    cgiMakeIntVar(&options[6][0], thisLineGap, 10 );
    printf("&nbsp;bases");
    }
}

void affyTxnPhase2Ui(struct trackDb *tdb)
/* Ui for affymetrix transcriptome phase 2 data. */
{
enum trackVisibility tnfgVis = tvHide;
char *visString = cartUsualString(cart, "hgt.affyPhase2.tnfg", "hide");
tnfgVis = hTvFromString(visString);
printf("<b>Transfrags Display Mode: </b>");
hTvDropDown("hgt.affyPhase2.tnfg", tnfgVis, TRUE);

wigCfgUi(cart,tdb,tdb->tableName,"<u>Graph Plotting options:</u>",FALSE);
printf("<p><b><u>View/Hide individual cell lines:</u></b>");
}

void humMusUi(struct trackDb *tdb, int optionNum )
/* put up UI for human/mouse conservation sample tracks (humMusL and musHumL)*/
{

char options[7][256];
int thisHeightPer, thisLineGap;
float thisMinYRange, thisMaxYRange;
char *interpolate, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "8.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

if( optionNum >= 7 )
{
	printf("<p><b>Maximum Interval to Interpolate Across</b>:&nbsp;&nbsp;");
	cgiMakeIntVar(&options[6][0], thisLineGap, 10 );
	printf("&nbsp;bases");
}
}




void affyTranscriptomeUi(struct trackDb *tdb)
	/* put up UI for the GC percent track (a sample track)*/
{
	int affyTranscriptomeHeightPer = atoi(cartUsualString(cart, "affyTranscriptome.heightPer", "100"));
	char *fill = cartUsualString(cart, "affyTranscriptome.fill", "1");

	printf("<br><br>");
	printf(" <b>Fill Blocks</b>: ");
	cgiMakeRadioButton("affyTranscriptome.fill", "1", sameString(fill, "1"));
	printf(" on ");

	cgiMakeRadioButton("affyTranscriptome.fill", "0", sameString(fill, "0"));
	printf(" off ");

	printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
	cgiMakeIntVar("affyTranscriptome.heightPer", affyTranscriptomeHeightPer, 5 );
	printf("&nbsp;pixels");

}


void ancientRUi(struct trackDb *tdb)
	/* put up UI for the ancient repeats track to let user enter an
	 * integer to filter out those repeats with less aligned bases.*/
{
	int ancientRMinLength = atoi(cartUsualString(cart, "ancientR.minLength", "50"));
	printf("<p><b>Length Filter</b><br>Exclude aligned repeats with less than ");
	cgiMakeIntVar("ancientR.minLength", ancientRMinLength, 4 );
	printf("aligned bases (not necessarily identical). Enter 0 for no filtering.");
}


void affyTransfragUi(struct trackDb *tdb)
/* Options for filtering affymetrix transfrag track based on score. */
{
boolean skipPseudos = cartUsualBoolean(cart, "affyTransfrags.skipPseudos", TRUE);
boolean skipDups = cartUsualBoolean(cart, "affyTransfrags.skipDups", FALSE);
printf("<br>");
cgiMakeCheckBox("affyTransfrags.skipPseudos", skipPseudos);
printf(" Remove transfrags that overlap pseudogenes from display.<br>");
cgiMakeCheckBox("affyTransfrags.skipDups", skipDups);
printf(" Remove transfrags that have a BLAT match elsewhere in the genome from display.<br>");
}

void ucsfdemoUi(struct trackDb *tdb)
{
char **menu;
int menuSize = 0;
int menuPos = 0;

puts("<BR><B>ER filter:</B>&nbsp;");
menuSize = 3;
menu = needMem((size_t)(menuSize * sizeof(char *)));
menuPos = 0;
menu[menuPos++] = "no filter";
menu[menuPos++] = "pos";
menu[menuPos++] = "neg";
cgiMakeDropList(UCSF_DEMO_ER, menu, menuSize,
    cartCgiUsualString(cart, UCSF_DEMO_ER, UCSF_DEMO_ER_DEFAULT));
freez(&menu);

puts("<BR><B>PR filter:</B>&nbsp;");
menuSize = 3;
menu = needMem((size_t)(menuSize * sizeof(char *)));
menuPos = 0;
menu[menuPos++] = "no filter";
menu[menuPos++] = "pos";
menu[menuPos++] = "neg";
cgiMakeDropList(UCSF_DEMO_PR, menu, menuSize,
    cartCgiUsualString(cart, UCSF_DEMO_PR, UCSF_DEMO_PR_DEFAULT));
freez(&menu);

}

void hapmapSnpsUi(struct trackDb *tdb)
/* Options for filtering hapmap snps */
/* Default is always to not filter (include all data) */
/* snp track puts the menu options in kent/src/hg/lib/snp125Ui.h */
/* snp track also sets global variables, don't actually need this? */
/* Consider using radio buttons */
{
struct sqlConnection *conn = hAllocConn(database);
boolean isPhaseIII = sameString(trackDbSettingOrDefault(tdb, "hapmapPhase", "II"), "III");

if ((isPhaseIII && !sqlTableExists(conn, "hapmapPhaseIIISummary")) ||
    (!isPhaseIII & !sqlTableExists(conn, "hapmapAllelesSummary")))
    return;

puts("<P>");
puts("<B>Display filters (applied to all subtracks):</B>");
puts("<BR>\n");

puts("<BR><B>Population availability:</B>&nbsp;");
static char *popAvailMenuPhaseIII[] =
    { "no filter",
      "all 11 Phase III populations",
      "all 4 Phase II populations" };
static char *popAvailMenuPhaseII[] =
    { "no filter",
      "all 4 populations",
      "1-3 populations" };
char **menu = isPhaseIII ? popAvailMenuPhaseIII : popAvailMenuPhaseII;
cgiMakeDropList(HAP_POP_COUNT, menu, 3,
		cartUsualString(cart, HAP_POP_COUNT, HAP_FILTER_DEFAULT));

puts("<BR><B>Major allele mixture between populations:</B>&nbsp;");
static char *mixedMenu[] = { "no filter", "mixed", "not mixed" };
cgiMakeDropList(HAP_POP_MIXED, mixedMenu, 3,
		cartUsualString(cart, HAP_POP_MIXED, HAP_FILTER_DEFAULT));

puts("<BR><B>Monomorphism:</B><BR>");
static char *noYesNoMenu[] = { "no filter", "yes", "no" };
char **pops = isPhaseIII ? hapmapPhaseIIIPops : hapmapPhaseIIPops;
int popCount = isPhaseIII ? HAP_PHASEIII_POPCOUNT : HAP_PHASEII_POPCOUNT;
puts("<TABLE BORDERWITH=0>");
int cellCount = 0, i;
char cartVar[128];
for (i = 0;  i < popCount;  i++)
    {
    char table[HDB_MAX_TABLE_STRING];
    if (endsWith(tdb->tableName, "PhaseII"))
	safef(table, sizeof(table), "hapmapSnps%sPhaseII", pops[i]);
    else
	safef(table, sizeof(table), "hapmapSnps%s", pops[i]);
    if (sqlTableExists(conn, table))
	{
	if (cellCount == 0)
	    puts("<TR>");
	printf("<TD align=right><B>%s:</B></TD><TD>", pops[i]);
	safef(cartVar, sizeof(cartVar), "%s_%s", HAP_MONO_PREFIX, pops[i]);
	cgiMakeDropList(cartVar, noYesNoMenu, 3,
			cartUsualString(cart, cartVar, HAP_FILTER_DEFAULT));
	printf("</TD>\n");
	cellCount += 2;
	if (cellCount == 12)
	    {
	    puts("</TR>");
	    cellCount = 0;
	    }
	}
    }
if (cellCount != 0)
    {
    while (cellCount++ < 12)
	puts("<TD></TD>");
    puts("</TR>");
    }
puts("</TABLE>");

puts("<BR><B>Polymorphism type:</B>&nbsp;");
static char *typeMenu[] = { "no filter", "bi-allelic", "transition", "transversion", "complex" };
cgiMakeDropList(HAP_TYPE, typeMenu, 5,
		cartUsualString(cart, HAP_TYPE, HAP_FILTER_DEFAULT));

puts("<BR><BR><B>Minor allele frequency in any population:  min:</B>&nbsp;");
float minFreq = atof(cartUsualString(cart, HAP_MIN_FREQ, HAP_MIN_FREQ_DEFAULT));
cgiMakeDoubleVar(HAP_MIN_FREQ, minFreq, 6);

puts("<B>max:</B>&nbsp;");
float maxFreq = atof(cartUsualString(cart, HAP_MAX_FREQ, HAP_MAX_FREQ_DEFAULT));
cgiMakeDoubleVar(HAP_MAX_FREQ, maxFreq, 6);
puts("&nbsp;(range: 0.0 to 0.5)\n");

if (isPhaseIII)
    puts("<BR><B>Average of populations' observed heterozygosities: </B>");
else
    puts("<BR><B>Expected heterozygosity (from total allele frequencies): </B>");
puts("<B>min:</B>&nbsp;");
float minHet = atof(cartUsualString(cart, HAP_MIN_HET, HAP_MIN_HET_DEFAULT));
cgiMakeDoubleVar(HAP_MIN_HET, minHet, 6);

puts("<B>max:</B>&nbsp;");
if (isPhaseIII)
    {
    float maxHet = atof(cartUsualString(cart, HAP_MAX_OBSERVED_HET, HAP_MAX_OBSERVED_HET_DEFAULT));
    cgiMakeDoubleVar(HAP_MAX_OBSERVED_HET, maxHet, 6);
    puts("&nbsp;(range: 0.0 to 1.0)\n");
    }
else
    {
    float maxHet = atof(cartUsualString(cart, HAP_MAX_EXPECTED_HET, HAP_MAX_EXPECTED_HET_DEFAULT));
    cgiMakeDoubleVar(HAP_MAX_EXPECTED_HET, maxHet, 6);
    puts("&nbsp;(range: 0.0 to 0.5)\n");
    }

static char *orthoMenu[] =
    { "no filter",
      "available",
      "matches major human allele",
      "matches minor human allele",
      "matches neither human allele" };

puts("<P><TABLE>");
for (i = 0;  hapmapOrthoSpecies[i] != NULL;  i++)
    {
    printf("<TR><TD><B>%s allele:</B></TD>\n<TD>", hapmapOrthoSpecies[i]);
    safef(cartVar, sizeof(cartVar), "%s_%s", HAP_ORTHO_PREFIX, hapmapOrthoSpecies[i]);
    cgiMakeDropList(cartVar, orthoMenu, 5,
		    cartUsualString(cart, cartVar, HAP_FILTER_DEFAULT));
    puts("</TD>");
    safef(cartVar, sizeof(cartVar), "%s_%s", HAP_ORTHO_QUAL_PREFIX, hapmapOrthoSpecies[i]);
    int minQual = atoi(cartUsualString(cart, cartVar, HAP_ORTHO_QUAL_DEFAULT));
    puts("<TD><B>Minimum quality score:</B></TD>\n<TD>");
    cgiMakeIntVar(cartVar, minQual, 4);
    puts("&nbsp;(range: 0 to 100)</TD></TR>\n");
    }
puts("</TABLE>");
puts("</P>\n");
printf("<B>Select subtracks to display:</B><BR>\n");
hFreeConn(&conn);
}

void pcrResultUi(struct trackDb *tdb)
/* Result from hgPcr query. */
{
struct targetDb *target;
if (! pcrResultParseCart(database, cart, NULL, NULL, &target))
    return;
if (target != NULL)
    {
    char *chosen = cartUsualString(cart, PCR_RESULT_TARGET_STYLE,
				   PCR_RESULT_TARGET_STYLE_DEFAULT);
    cgiMakeRadioButton(PCR_RESULT_TARGET_STYLE, PCR_RESULT_TARGET_STYLE_TRIM,
		       sameString(chosen, PCR_RESULT_TARGET_STYLE_TRIM));
    printf("Show only the amplified part of %s item&nbsp;&nbsp;&nbsp;\n",
	   target->description);
    cgiMakeRadioButton(PCR_RESULT_TARGET_STYLE, PCR_RESULT_TARGET_STYLE_TALL,
		       sameString(chosen, PCR_RESULT_TARGET_STYLE_TALL));
    printf("Show the whole %s item with amplified part tall",
	   target->description);
    }
baseColorDrawOptDropDown(cart, tdb);
}

void dgvUi(struct trackDb *tdb)
/* Database of Genomic Variants: filter by publication. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
safef(query, sizeof(query),
      "select reference,pubMedId from %s group by pubMedId order by reference;", tdb->tableName);
sr = sqlGetResult(conn, query);
printf("<BR><B>Filter by publication reference:</B>\n");
char cartVarName[256];
safef (cartVarName, sizeof(cartVarName), "hgt_%s_filterType", tdb->tableName);
boolean isInclude = sameString("include", cartUsualString(cart, cartVarName, "include"));
cgiMakeRadioButton(cartVarName, "include", isInclude);
printf("include\n");
cgiMakeRadioButton(cartVarName, "exclude", !isInclude);
printf("exclude<BR>\n");
safef (cartVarName, sizeof(cartVarName), "hgt_%s_filterPmId", tdb->tableName);
struct slName *checked = cartOptionalSlNameList(cart, cartVarName);
boolean setAll = (checked == NULL && isInclude);
#define MAX_DGV_REFS 128
char *labelArr[MAX_DGV_REFS], *valueArr[MAX_DGV_REFS];
int refCount = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *ref = row[0];
    char *pmId = row[1];
    char label[512];
    safef(label, sizeof(label),
	  "<A HREF=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=PubMed"
	  "&list_uids=%s&dopt=Abstract&tool=genome.ucsc.edu\" TARGET=_BLANK>%s</A>", pmId, ref);
    labelArr[refCount] = cloneString(label);
    valueArr[refCount++] = cloneString(pmId);
    if (refCount >= MAX_DGV_REFS)
	errAbort("dgvUi: %s has too many references (max %d)", tdb->tableName, MAX_DGV_REFS);
    if (setAll)
	slAddHead(&checked, slNameNew(pmId));
    }
sqlFreeResult(&sr);
jsMakeCheckboxGroupSetClearButton(cartVarName, TRUE);
jsMakeCheckboxGroupSetClearButton(cartVarName, FALSE);
cgiMakeCheckboxGroupWithVals(cartVarName, labelArr, valueArr, refCount, checked, 4);
hFreeConn(&conn);
}

#ifdef USE_BAM
static char *grayLabels[] =
    { "alignment quality",
      "base qualities",
      "unpaired ends",
    };
static char *grayValues[] =
    { BAM_GRAY_MODE_ALI_QUAL,
      BAM_GRAY_MODE_BASE_QUAL,
      BAM_GRAY_MODE_UNPAIRED,
    };

// When a child input of a radio set is changed, click its radio button:
#define UPDATE_RADIO_FORMAT "%s=\"\
    var inputs = document.getElementsByName('%s'); \
    if (inputs) { \
      for (var i=0; i < inputs.length; i++) { \
        if (inputs[i].type == 'radio') { \
          inputs[i].checked = (inputs[i].value == '%s'); \
        } \
      } \
    }\""

void bamUi(struct trackDb *tdb)
/* BAM: short-read-oriented alignment file format. */
{
char cartVarName[1024];
boolean canPair = (trackDbSetting(tdb, BAM_PAIR_ENDS_BY_NAME) != NULL);
puts("<BR>");
if (canPair)
    {
    printf("<B>Attempt to join paired end reads by name:</B>\n");
    safef(cartVarName, sizeof(cartVarName), "%s." BAM_PAIR_ENDS_BY_NAME, tdb->tableName);
    cartMakeCheckBox(cart, cartVarName, TRUE);
    puts("<BR>");
    }
printf("<B>Minimum alignment quality:</B>\n");
safef(cartVarName, sizeof(cartVarName), "%s." BAM_MIN_ALI_QUAL, tdb->tableName);
cartMakeIntVar(cart, cartVarName,
	       atoi(trackDbSettingOrDefault(tdb, BAM_MIN_ALI_QUAL, BAM_MIN_ALI_QUAL_DEFAULT)), 4);
puts("<BR>");
baseColorDrawOptDropDown(cart, tdb);
printf("<BR>\n");
printf("<B>Additional coloring modes:</B><BR>\n");
safef(cartVarName, sizeof(cartVarName), "%s." BAM_COLOR_MODE, tdb->tableName);
char *selected = cartUsualString(cart, cartVarName,
				 trackDbSettingOrDefault(tdb, BAM_COLOR_MODE, BAM_COLOR_MODE_DEFAULT));
cgiMakeRadioButton(cartVarName, BAM_COLOR_MODE_STRAND, sameString(selected, BAM_COLOR_MODE_STRAND));
printf("Color by strand (blue for +, red for -)<BR>\n");
cgiMakeRadioButton(cartVarName, BAM_COLOR_MODE_GRAY, sameString(selected, BAM_COLOR_MODE_GRAY));
printf("Use gray for\n");
char cartVarName2[1024];
safef(cartVarName2, sizeof(cartVarName2), "%s." BAM_GRAY_MODE, tdb->tableName);
int grayMenuSize = canPair ? ArraySize(grayLabels) : ArraySize(grayLabels)-1;
char *sel2 = cartUsualString(cart, cartVarName2,
			     trackDbSettingOrDefault(tdb, BAM_GRAY_MODE, BAM_GRAY_MODE_DEFAULT));
char onChange[2048];
safef(onChange, sizeof(onChange), UPDATE_RADIO_FORMAT,
      "onChange", cartVarName, BAM_COLOR_MODE_GRAY);
cgiMakeDropListFull(cartVarName2, grayLabels, grayValues, grayMenuSize, sel2, onChange);
printf("<BR>\n");
cgiMakeRadioButton(cartVarName, BAM_COLOR_MODE_TAG, sameString(selected, BAM_COLOR_MODE_TAG));
printf("Use R,G,B colors specified in user-defined tag ");
safef(cartVarName2, sizeof(cartVarName2), "%s." BAM_COLOR_TAG, tdb->tableName);
sel2 = cartUsualString(cart, cartVarName2,
		       trackDbSettingOrDefault(tdb, BAM_COLOR_TAG, BAM_COLOR_TAG_DEFAULT));
safef(onChange, sizeof(onChange), UPDATE_RADIO_FORMAT,
      "onkeypress", cartVarName, BAM_COLOR_MODE_TAG);
cgiMakeTextVarWithExtraHtml(cartVarName2, sel2, 30, onChange);
printf("<BR>\n");
cgiMakeRadioButton(cartVarName, BAM_COLOR_MODE_OFF, sameString(selected, BAM_COLOR_MODE_OFF));
printf("No additional coloring<BR>\n");

//TODO: include / exclude flags
}
#endif//def USE_BAM


void superTrackUi(struct trackDb *superTdb)
/* List tracks in this collection, with visibility controls and UI links */
{
struct trackDb *tdb;
printf("<P><TABLE CELLPADDING=2>");
for (tdb = superTdb->subtracks; tdb != NULL; tdb = tdb->next)
    {
    if (!hTableOrSplitExists(database, tdb->tableName) && trackDbSetting(tdb, "compositeTrack") == NULL) // NOTE: tdb if composite, is not yet populated with it's own subtracks!
        continue;
    printf("<TR>");
    printf("<TD NOWRAP><A HREF=\"%s?%s=%u&c=%s&g=%s\">%s</A>&nbsp;</TD>",
                hgTrackUiName(), cartSessionVarName(), cartSessionId(cart),
                chromosome, cgiEncode(tdb->tableName), tdb->shortLabel);
    printf("<TD>");
    enum trackVisibility tv =
                    hTvFromString(cartUsualString(cart, tdb->tableName,
                                            hStringFromTv(tdb->visibility)));
    hTvDropDownClassVisOnly(tdb->tableName, tv, tdb->canPack,
                            tv == tvHide ?  "hiddenText" : "normalText",
                            trackDbSetting(tdb, "onlyVisibility"));
    printf("<TD>%s", tdb->longLabel);
    char *dataVersion = trackDbSetting(tdb, "dataVersion");
    if (dataVersion)
        printf("&nbsp&nbsp;<EM><FONT COLOR=#666666 SIZE=-1>%s</FONT></EM>", dataVersion);
    printf("</TD></TR>");
    }
printf("</TABLE>");
}

void specificUi(struct trackDb *tdb, struct customTrack *ct)
	/* Draw track specific parts of UI. */
{
char *track = tdb->tableName;

if (sameString(track, "stsMap"))
        stsMapUi(tdb);
else if (sameString(track, "affyTxnPhase2"))
    affyTxnPhase2Ui(tdb);
else if (sameString(track, "cgapSage"))
    cgapSageUi(tdb);
else if (sameString(track, "stsMapMouseNew"))
        stsMapMouseUi(tdb);
else if (sameString(track, "stsMapRat"))
        stsMapRatUi(tdb);
else if (sameString(track, "snpMap"))
        snpMapUi(tdb);
else if (sameString(track, "snp"))
        snpUi(tdb);
else if (snpVersion(track) >= 125)
	snp125Ui(tdb);
else if (sameString(track, "rertyHumanDiversityLd") ||
	 startsWith("hapmapLd", track) ||
	 sameString(tdb->type, "ld2"))
        ldUi(tdb);
else if (sameString(track, "cbr_waba"))
        cbrWabaUi(tdb);
else if (sameString(track, "fishClones"))
        fishClonesUi(tdb);
else if (sameString(track, "recombRate"))
        recombRateUi(tdb);
else if (sameString(track, "recombRateRat"))
        recombRateRatUi(tdb);
else if (sameString(track, "recombRateMouse"))
        recombRateMouseUi(tdb);
else if (sameString(track, "cghNci60"))
        cghNci60Ui(tdb);
else if (sameString(track, "xenoRefGene"))
        refGeneUI(tdb);
else if (startsWith("transMapAln", track))
        transMapUI(tdb);
else if (sameString(track, "refGene"))
        refGeneUI(tdb);
else if (sameString(track, "knownGene"))
        knownGeneUI(tdb);
else if (sameString(track, "omimGene"))
        omimGeneUI(tdb);
else if (sameString(track, "hg17Kg"))
        hg17KgUI(tdb);
else if (sameString(track, "pseudoGeneLink") || startsWith("retroMrnaInfo", track))
        retroGeneUI(tdb);
else if (sameString(track, "ensGeneNonCoding"))
        ensemblNonCodingUI(tdb);
else if (sameString(track, "vegaGeneComposite"))
        vegaGeneUI(tdb);
else if (sameString(track, "all_mrna"))
    mrnaUi(tdb, FALSE);
else if (sameString(track, "mrna"))
    mrnaUi(tdb, FALSE);
else if (sameString(track, "splicesP"))
    bedUi(tdb, cart, "", FALSE);
else if(sameString(track,"FantomCageBedForward") || sameString(track,"FantomCageBedReverse") ||
	sameString(track,"FantomCageReadForward") || sameString(track,"FantomCageReadReverse"))
    bedUi(tdb, cart, "", FALSE);
else if (sameString(track, "all_est"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "est"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "tightMrna"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "tightEst"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "intronEst"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "xenoMrna"))
        mrnaUi(tdb, TRUE);
else if (sameString(track, "xenoEst"))
        mrnaUi(tdb, TRUE);
else if (sameString(track, "rosetta"))
        rosettaUi(tdb);
else if (startsWith("blastDm", track))
        blastFBUi(tdb);
else if (sameString(track, "blastSacCer1SG"))
        blastSGUi(tdb);
else if (sameString(track, "blastHg17KG") || sameString(track, "blastHg16KG")
        || sameString(track, "blastCe3WB") || sameString(track, "blastHg18KG")
        || sameString(track, "blatzHg17KG")|| startsWith("mrnaMap", track)|| startsWith("mrnaXeno", track))
        blastUi(tdb);
else if (sameString(track, "hgPcrResult"))
    pcrResultUi(tdb);
else if (startsWith("bedGraph", tdb->type) || startsWith("bigWig", tdb->type))
    wigCfgUi(cart,tdb,tdb->tableName,NULL, FALSE);
else if (startsWith("wig", tdb->type))
        {
        if (startsWith("wigMaf", tdb->type))
            wigMafCfgUi(cart, tdb, tdb->tableName, NULL, FALSE, database);
        else
            wigCfgUi(cart,tdb,tdb->tableName, NULL, FALSE);
        }
else if (startsWith("chromGraph", tdb->type))
        chromGraphUi(tdb);
/* else if (sameString(track, "affyHumanExon")) */
/*         affyAllExonUi(tdb); */
else if (sameString(track, "ancientR"))
        ancientRUi(tdb);
else if (sameString(track, "zoo") || sameString(track, "zooNew" ))
         zooWiggleUi(tdb);
else if (sameString(track, "humMusL") ||
         sameString( track, "musHumL") ||
         sameString( track, "regpotent") ||
         sameString( track, "mm3Rn2L" )	 ||
         sameString( track, "mm3Hg15L" ) ||
         sameString( track, "hg15Mm3L" ))
            humMusUi(tdb,7);
/* NOTE: type psl xeno <otherDb> tracks use crossSpeciesUi, so
 * add explicitly here only if track has another type (bed, chain).
 * For crossSpeciesUi, the
 * default for chrom coloring is "on", unless track setting
 * colorChromDefault is set to "off" */
else if (startsWith("net", track))
    netAlignCfgUi(database, cart, tdb, tdb->tableName, NULL, FALSE);
else if (startsWith("chain", track) || endsWith("chainSelf", track))
    chainColorUi(tdb);
else if (sameString(track, "orthoTop4"))
    /* still used ?? */
    crossSpeciesUi(tdb);
else if (sameString(track, "mouseOrtho"))
    crossSpeciesUi(tdb);
else if (sameString(track, "mouseSyn"))
    crossSpeciesUi(tdb);
else if (sameString(track, "affyTranscriptome"))
    affyTranscriptomeUi(tdb);

else if (startsWith("sample", tdb->type))
    genericWiggleUi(tdb,7);
else if (sameString(track, WIKI_TRACK_TABLE))
    wikiTrackUi(tdb);
else if (sameString(track, RULER_TRACK_NAME))
    rulerUi(tdb);
else if (sameString(track, OLIGO_MATCH_TRACK_NAME))
    oligoMatchUi(tdb);
else if (sameString(track, CUTTERS_TRACK_NAME))
    cutterUi(tdb);
else if(sameString(track, "affyTransfrags"))
    affyTransfragUi(tdb);
else if (sameString(track, "gvPos"))
    gvUi(tdb);
else if (sameString(track, "oreganno"))
    oregannoUi(tdb);
else if (startsWith("retroposons", track))
    retroposonsUi(tdb);
else if (sameString(track, "tfbsConsSites"))
    tfbsConsSitesUi(tdb);
else if (sameString(track, "CGHBreastCancerUCSF"))
    ucsfdemoUi(tdb);
else if (startsWith("hapmapSnps", track))
    hapmapSnpsUi(tdb);
else if (sameString(track, "switchDbTss"))
    switchDbScoreUi(tdb);
else if (sameString(track, "dgv") || (startsWith("dgvV", track) && isdigit(track[4])))
    dgvUi(tdb);
#ifdef USE_BAM
else if (sameString(tdb->type, "bam"))
    bamUi(tdb);
#endif
else if (tdb->type != NULL)
    {
    /* handle all tracks with type genePred or bed or "psl xeno <otherDb>" */
    char *typeLine = cloneString(tdb->type);
    char *words[8];
    int wordCount = 0;
    wordCount = chopLine(typeLine, words);
    if (wordCount > 0)
        {
	    if (sameWord(words[0], "genePred"))
            {
            genePredCfgUi(cart,tdb,tdb->tableName,NULL,FALSE);
            }
        else if(sameWord(words[0], "encodePeak") || sameWord(words[0], "narrowPeak")
             || sameWord(words[0], "broadPeak")  || sameWord(words[0], "gappedPeak"))
	       {
	       encodePeakUi(tdb, ct);
	       }
        else if (sameWord(words[0], "expRatio"))
	       {
	       expRatioUi(tdb);
	       }
        else if (sameWord(words[0], "array")) /* not quite the same as an "expRatio" type (custom tracks) */
            {
            expRatioCtUi(tdb);
            }
        /* if bed has score then show optional filter based on score */
        else if (sameWord(words[0], "bed") && wordCount == 3)
            {
            /* Note: jaxQTL3 is a bed 8 format track because of
                thickStart/thickStart, but there is no valid score.
                Similarly, the score field for wgRna track is no long used either.
                It originally was usd to depict different RNA types.  But the new
                wgRna table has a new field 'type', which is used to store RNA
                type info and from which to determine the display color of each entry.
            */
            int bedFieldCount = atoi(words[1]);
            if ((bedFieldCount >= 5 || trackDbSetting(tdb, "scoreMin") != NULL)
            &&  !sameString(track, "jaxQTL3") && !sameString(track, "wgRna")
            &&  !startsWith("encodeGencodeIntron", track))
                {
                if (trackDbSetting(tdb, "scoreFilterMax"))
                    scoreCfgUi(database, cart,tdb,tdb->tableName,NULL,
                        sqlUnsigned(trackDbSetting(tdb, "scoreFilterMax")),FALSE);
                else
                    scoreCfgUi(database, cart,tdb,tdb->tableName,NULL,1000,FALSE);
                }
            }
        else if (sameWord(words[0], "bed5FloatScore") || sameWord(words[0], "bed5FloatScoreWithFdr"))
            scoreCfgUi(database, cart,tdb,tdb->tableName,NULL,1000,FALSE);
        else if (sameWord(words[0], "psl"))
            {
            if (wordCount == 3)
            if (sameWord(words[1], "xeno"))
                crossSpeciesUi(tdb);
            baseColorDrawOptDropDown(cart, tdb);
            }
        }
        freeMem(typeLine);
    }
if (tdbIsSuperTrack(tdb))
    superTrackUi(tdb);
else if (tdbIsComposite(tdb))
    {
     hCompositeUi(database, cart, tdb, NULL, NULL, MAIN_FORM);
    }
extraUiLinks(database,tdb);
}


void trackUi(struct trackDb *tdb, struct customTrack *ct)
/* Put up track-specific user interface. */
{
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("utils.js",NULL);
#define SUPPORT_RESET_TO_DEFAULTS
#ifdef SUPPORT_RESET_TO_DEFAULTS
#define RESET_TO_DEFAULTS "defaults"
char setting[128];

// NOTE: Currently only composite multi-view tracks because
// reset relies upon all cart vars following naming convention:
//   {tableName}.{varName}...  ( One exception supported: {tableName}_sel ).
if(tdbIsComposite(tdb))
    {
    safef(setting,sizeof(setting),"%s.%s",tdb->tableName,RESET_TO_DEFAULTS);
    // NOTE: if you want track vis to not be reset, move to after vis dropdown
    if(1 == cartUsualInt(cart, setting, 0))
        cartRemoveAllForTdbAndChildren(cart,tdb);
    }
#endif//def SUPPORT_RESET_TO_DEFAULTS

printf("<FORM ACTION=\"%s\" NAME=\""MAIN_FORM"\" METHOD=%s>\n\n",
       hgTracksName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
printf("<H1>%s%s</H1>\n", tdb->longLabel, tdbIsSuper(tdb) ? " Tracks" : "");

/* Print link for supertrack */
if (tdbIsSuperTrackChild(tdb))
    {
    struct trackDb *superTdb = tdb->parent;
    assert(superTdb != NULL);
    if (superTdb)
        {
        char *encodedMapName = cgiEncode(superTdb->tableName);
        printf("<H3>Member of super-track: <A HREF=\"%s?%s=%u&c=%s&g=%s\">%s</A></H3>",
                    hgTrackUiName(), cartSessionVarName(), cartSessionId(cart),
                    chromosome, encodedMapName, superTdb->shortLabel);
        freeMem(encodedMapName);
        }
    }

if (ct && sameString(tdb->type, "maf"))
    tdb->canPack = TRUE;

/* Display visibility menu */
if (tdbIsComposite(tdb) && multViewCount(tdb) > 0)
    printf("<B>Maximum&nbsp;display&nbsp;mode:&nbsp;</B>");
else
    printf("<B>Display&nbsp;mode:&nbsp;</B>");
if (tdbIsSuper(tdb))
    {
    /* This is a supertrack -- load its members and show hide/show dropdown */
    hTrackDbLoadSuper(database, tdb);
    superTrackDropDown(cart, tdb, 1);
    }
else
    {
    /* normal visibility control dropdown */
    char *vis = hStringFromTv(tdb->visibility);
    hTvDropDownClassVisOnly(tdb->tableName,
        hTvFromString(cartUsualString(cart,tdb->tableName, vis)),
        tdb->canPack, "normalText", trackDbSetting(tdb, "onlyVisibility"));
    }
printf("&nbsp;");
cgiMakeButton("Submit", "Submit");

#ifdef SUPPORT_RESET_TO_DEFAULTS
if(tdbIsComposite(tdb))
    printf("\n&nbsp;&nbsp;<a href='#' onclick='setVarAndPostForm(\"%s\",\"1\",\"mainForm\"); return false;'>Reset to defaults</a>\n",setting);
#endif//def SUPPORT_RESET_TO_DEFAULTS

if (ct)
    {
    puts("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
    cgiMakeButton(CT_DO_REMOVE_VAR, "Remove custom track");
    cgiMakeHiddenVar(CT_SELECTED_TABLE_VAR, tdb->tableName);
    puts("&nbsp;");
    if (differentString(tdb->type, "chromGraph"))
        cgiMakeOnClickButton("document.customTrackForm.submit();return false;",
                                "Update custom track");
    }
printf("<BR>\n");

specificUi(tdb, ct);
puts("</FORM>");

if (ct)
    {
    /* hidden form for custom tracks CGI */
    printf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
    cartSaveSession(cart);
    cgiMakeHiddenVar(CT_SELECTED_TABLE_VAR, tdb->tableName);
    puts("</FORM>\n");
    }
else
    {
    /* Print data version trackDB setting, if any */
    char *version = trackDbSetting(tdb, "dataVersion");
    if (version)
        printf("<B>Data version:</B> %s<BR>\n", version);

   /* Print lift information from trackDb, if any */
   trackDbPrintOrigAssembly(tdb, database);

   if (hTableOrSplitExists(database, tdb->tableName))
        {
        /* Print update time of the table (or one of the components if split) */
        char *tableName = hTableForTrack(database, tdb->tableName);
	struct sqlConnection *conn = hAllocConnProfile(getTrackProfileName(tdb), database);

	char *date = firstWordInLine(sqlTableUpdate(conn, tableName));
	if (date != NULL && !startsWith("wigMaf", tdb->type))
	    printf("<B>Data last updated:</B> %s<BR>\n", date);
	hFreeConn(&conn);
	}
    }
if (tdb->html != NULL && tdb->html[0] != 0)
    {
    htmlHorizontalLine();
    puts(tdb->html);
    }
}	/*	void trackUi(struct trackDb *tdb)	*/

struct trackDb *trackDbForPseudoTrack(char *tableName, char *shortLabel,
	char *longLabel, int defaultVis, boolean canPack)
/* Create trackDb for a track without a corresponding table. */
{
struct trackDb *tdb;

AllocVar(tdb);
tdb->tableName = tableName;
tdb->shortLabel = shortLabel;
tdb->longLabel = longLabel;
tdb->visibility = defaultVis;
tdb->priority = 1.0;

tdb->html = hFileContentsOrWarning(hHelpFile(tableName));
tdb->type = "none";
tdb->grp = "map";
tdb->canPack = canPack;
return tdb;
}

struct trackDb *trackDbForWikiTrack()
/* Create a trackDb entry for the wikiTrack.
   It is not a real track, so doesn't appear in trackDb */
{
return trackDbForPseudoTrack(WIKI_TRACK_TABLE,
	WIKI_TRACK_LABEL, WIKI_TRACK_LONGLABEL, tvFull, FALSE);
}

struct trackDb *trackDbForRuler()
/* Create a trackDb entry for the base position ruler.
   It is not (yet?) a real track, so doesn't appear in trackDb */
{
return trackDbForPseudoTrack(RULER_TRACK_NAME,
	RULER_TRACK_LABEL, RULER_TRACK_LONGLABEL, tvFull, FALSE);
}

struct trackDb *trackDbForOligoMatch()
/* Create a trackDb entry for the oligo matcher pseudo-track. */
{
return trackDbForPseudoTrack(OLIGO_MATCH_TRACK_NAME,
	OLIGO_MATCH_TRACK_LABEL, OLIGO_MATCH_TRACK_LONGLABEL, tvHide, TRUE);
}

void doMiddle(struct cart *theCart)
/* Write body of web page. */
{
struct trackDb *tdb = NULL;
char *track;
struct customTrack *ct = NULL, *ctList = NULL;
char *ignored;
cart = theCart;
track = cartString(cart, "g");
getDbAndGenome(cart, &database, &ignored, NULL);
chromosome = cartUsualString(cart, "c", hDefaultChrom(database));
if (sameWord(track, WIKI_TRACK_TABLE))
    tdb = trackDbForWikiTrack();
else if (sameWord(track, RULER_TRACK_NAME))
    /* special handling -- it's not a full-fledged track */
    tdb = trackDbForRuler();
else if (sameWord(track, OLIGO_MATCH_TRACK_NAME))
    tdb = trackDbForOligoMatch();
else if (sameWord(track, CUTTERS_TRACK_NAME))
    tdb = trackDbForPseudoTrack(CUTTERS_TRACK_NAME, CUTTERS_TRACK_LABEL, CUTTERS_TRACK_LONGLABEL, tvHide, TRUE);
else if (isCustomTrack(track))
    {
    ctList = customTracksParseCart(database, cart, NULL, NULL);
    for (ct = ctList; ct != NULL; ct = ct->next)
        {
        if (sameString(track, ct->tdb->tableName))
            {
            tdb = ct->tdb;
            break;
            }
        }
    }
else if (sameString(track, "hgPcrResult"))
    tdb = pcrResultFakeTdb();
else
    {
    tdb = hTrackDbForTrack(database, track);
    }
if (tdb == NULL)
   errAbort("Can't find %s in track database %s chromosome %s",
	    track, database, chromosome);
char *super = trackDbGetSupertrackName(tdb);
if (super)
    {
    /* configured as a supertrack member in trackDb */
    if (tdb->parent)
        {
        /* the supertrack is also configured, so use supertrack defaults */
        tdbMarkAsSuperTrack(tdb->parent);
        trackDbSuperMemberSettings(tdb);
        }
    }
char *title = (tdbIsSuper(tdb) ? "Super-track Settings" : "Track Settings");
cartWebStart(cart, database, "%s %s", tdb->shortLabel, title);
trackUi(tdb, ct);
printf("<BR>\n");
webEnd();
}

char *excludeVars[] = { "submit", "Submit", "g", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetBackground(hBackgroundImage());
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
