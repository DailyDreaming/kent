/* hui - human genome user interface common controls. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "jsHelper.h"
#include "sqlNum.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "hCommon.h"
#include "hgConfig.h"
#include "chainCart.h"
#include "obscure.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: hui.c,v 1.101 2008/06/25 19:55:47 tdreszer Exp $";

char *hUserCookie()
/* Return our cookie name. */
{
if (hIsMgcServer())
    return "mgcuid";
else    
    return cfgOptionDefault("central.cookie", "hguid");
}

char *wrapWhiteFont(char *s)
/* Write white font around s */
{
static char buf[256];
safef(buf, sizeof(buf), "<FONT COLOR=\"#FFFFFF\">%s</FONT>", s);
return buf;
}

char *hDocumentRoot()
/* get the path to the DocumentRoot, or the default */
{
return cfgOptionDefault("browser.documentRoot", DOCUMENT_ROOT);
}

char *hHelpFile(char *fileRoot)
/* Given a help file root name (e.g. "hgPcrResult" or "cutters"),
 * prepend the complete help directory path and add .html suffix. 
 * Do not free the statically allocated result. */
{
static char helpName[PATH_LEN];
/* This cfgOption comes from Todd Lowe's hgTrackUi.c addition (r1.230): */
char *helpDir = cfgOption("help.html");
if (helpDir != NULL)
    safef(helpName, sizeof(helpName), "%s/%s.html", helpDir, fileRoot);
else
    safef(helpName, sizeof(helpName), "%s%s/%s.html", hDocumentRoot(),
	  HELP_DIR, fileRoot);
return helpName;
}

char *hFileContentsOrWarning(char *file)
/* Return the contents of the html file, or a warning message. 
 * The file path may begin with hDocumentRoot(); if it doesn't, it is
 * assumed to be relative and hDocumentRoot() will be prepended. */
{
if (isEmpty(file))
    return cloneString("<BR>Program Error: Empty file name for include file"
		       "<BR>\n");
char path[PATH_LEN];
char *docRoot = hDocumentRoot();
if (startsWith(docRoot, file))
    safecpy(path, sizeof path, file);
else
    safef(path, sizeof path, "%s/%s", docRoot, file);
if (! fileExists(path))
    {
    char message[1024];
    safef(message, sizeof(message), "<BR>Program Error: Missing file %s</BR>",
	  path);
    return cloneString(message);
    }
/* If the file is there but not readable, readInGulp will errAbort,
 * but I think that is serious enough that errAbort is OK. */
char *result;
readInGulp(path, &result, NULL);
return result;
}

char *hCgiRoot()
/* get the path to the CGI directory.
 * Returns NULL when not running as a CGI (unless specified by browser.cgiRoot) */
{
static char defaultDir[PATH_LEN];
char *scriptFilename = getenv("SCRIPT_FILENAME");
if(scriptFilename)
    {
    char dir[PATH_LEN], name[FILENAME_LEN], extension[FILEEXT_LEN];
    dir[0] = 0;
    splitPath(scriptFilename, dir, name, extension);
    safef(defaultDir, sizeof(defaultDir), "%s", dir);
    int len = strlen(defaultDir);
    // Get rid of trailing slash to be consistent with hDocumentRoot
    if(defaultDir[len-1] == '/')
        defaultDir[len-1] = 0;
    }
else
    {
    defaultDir[0] = 0;
    }
return cfgOptionDefault("browser.cgiRoot", defaultDir);
}

char *hBackgroundImage()
/* get the path to the configured background image to use, or the default */
{
return cfgOptionDefault("browser.background", DEFAULT_BACKGROUND);
}

/******  Some stuff for tables of controls ******/

struct controlGrid *startControlGrid(int columns, char *align)
/* Start up a control grid. */
{
struct controlGrid *cg;
AllocVar(cg);
cg->columns = columns;
cg->align = cloneString(align);
cg->rowOpen = FALSE;
return cg;
}

void controlGridEndRow(struct controlGrid *cg)
/* Force end of row. */
{
printf("</tr>");
cg->rowOpen = FALSE;
cg->columnIx = 0;
}

void controlGridStartCell(struct controlGrid *cg)
/* Start a new cell in control grid. */
{
if (cg->columnIx == cg->columns)
    controlGridEndRow(cg);
if (!cg->rowOpen)
    {
    printf("<tr>");
    cg->rowOpen = TRUE;
    }
if (cg->align)
    printf("<td align=%s>", cg->align);
else
    printf("<td>");
}

void controlGridEndCell(struct controlGrid *cg)
/* End cell in control grid. */
{
printf("</td>");
++cg->columnIx;
}

void endControlGrid(struct controlGrid **pCg)
/* Finish up a control grid. */
{
struct controlGrid *cg = *pCg;
if (cg != NULL)
    {
    int i;
    if (cg->columnIx != 0 && cg->columnIx < cg->columns)
	for( i = cg->columnIx; i <= cg->columns; i++)
	    printf("<td>&nbsp;</td>\n");
    if (cg->rowOpen)
	printf("</tr>\n");
    printf("</table>\n");
    freeMem(cg->align);
    freez(pCg);
    }
}

/******  Some stuff for hide/dense/full controls ******/

static char *hTvStrings[] = 
/* User interface strings for track visibility controls. */
    {
    "hide",
    "dense",
    "full",
    "pack",
    "squish"
    };

enum trackVisibility hTvFromStringNoAbort(char *s)
/* Given a string representation of track visibility, return as
 * equivalent enum. */
{
int vis = stringArrayIx(s, hTvStrings, ArraySize(hTvStrings));
if (vis < 0)
    vis = 0;  // don't generate bogus value on invalid input
return vis;
}

enum trackVisibility hTvFromString(char *s)
/* Given a string representation of track visibility, return as
 * equivalent enum. */
{
enum trackVisibility vis = hTvFromStringNoAbort(s);
if ((int)vis < 0)
   errAbort("Unknown visibility %s", s);
return vis;
}

char *hStringFromTv(enum trackVisibility vis)
/* Given enum representation convert to string. */
{
return hTvStrings[vis];
}

void hTvDropDownClassWithJavascript(char *varName, enum trackVisibility vis, boolean canPack, char *class,char * javascript)
/* Make track visibility drop down for varName with style class */
{
static char *noPack[] = 
    {
    "hide",
    "dense",
    "full",
    };
static char *pack[] = 
    {
    "hide",
    "dense",
    "squish",
    "pack",
    "full",
    };
static int packIx[] = {tvHide,tvDense,tvSquish,tvPack,tvFull};
if (canPack)
    cgiMakeDropListClassWithStyleAndJavascript(varName, pack, ArraySize(pack), 
                            pack[packIx[vis]], class, TV_DROPDOWN_STYLE,javascript);
else
    cgiMakeDropListClassWithStyleAndJavascript(varName, noPack, ArraySize(noPack), 
                            noPack[vis], class, TV_DROPDOWN_STYLE,javascript);
}

void hTvDropDownClassVisOnly(char *varName, enum trackVisibility vis,
	boolean canPack, char *class, char *visOnly)
/* Make track visibility drop down for varName with style class,
	and potentially limited to visOnly */
{
static char *denseOnly[] = 
    {
    "hide",
    "dense",
    };
static char *squishOnly[] = 
    {
    "hide",
    "squish",
    };
static char *packOnly[] = 
    {
    "hide",
    "pack",
    };
static char *fullOnly[] = 
    {
    "hide",
    "full",
    };
static char *noPack[] = 
    {
    "hide",
    "dense",
    "full",
    };
static char *pack[] = 
    {
    "hide",
    "dense",
    "squish",
    "pack",
    "full",
    };
static int packIx[] = {tvHide,tvDense,tvSquish,tvPack,tvFull};
if (visOnly != NULL)
    {
    int visIx = (vis > 0) ? 1 : 0;
    if (sameWord(visOnly,"dense"))
	cgiMakeDropListClassWithStyle(varName, denseOnly, ArraySize(denseOnly),
		denseOnly[visIx], class, TV_DROPDOWN_STYLE);
    else if (sameWord(visOnly,"squish"))
	cgiMakeDropListClassWithStyle(varName, squishOnly, 
                ArraySize(squishOnly), squishOnly[visIx], 
                class, TV_DROPDOWN_STYLE);
    else if (sameWord(visOnly,"pack"))
	cgiMakeDropListClassWithStyle(varName, packOnly, ArraySize(packOnly),
		packOnly[visIx], class, TV_DROPDOWN_STYLE);
    else if (sameWord(visOnly,"full"))
	cgiMakeDropListClassWithStyle(varName, fullOnly, ArraySize(fullOnly),
		fullOnly[visIx], class, TV_DROPDOWN_STYLE);
    else			/* default when not recognized */
	cgiMakeDropListClassWithStyle(varName, denseOnly, ArraySize(denseOnly),
		denseOnly[visIx], class, TV_DROPDOWN_STYLE);
    }
    else
    {
    if (canPack)
	cgiMakeDropListClassWithStyle(varName, pack, ArraySize(pack), 
                            pack[packIx[vis]], class, TV_DROPDOWN_STYLE);
    else
	cgiMakeDropListClassWithStyle(varName, noPack, ArraySize(noPack), 
                            noPack[vis], class, TV_DROPDOWN_STYLE);
    }
}

void hideShowDropDown(char *varName, boolean show, char *class)
/* Make hide/show dropdown for varName */
{
static char *hideShow[] =
    {
    "hide",
    "show"
    };
cgiMakeDropListClassWithStyle(varName, hideShow, ArraySize(hideShow), 
                    hideShow[show], class, TV_DROPDOWN_STYLE);
}


/****** Some stuff for stsMap related controls *******/

static char *stsMapOptions[] = {
    "All Genetic",
    "Genethon",
    "Marshfield",
    "deCODE",
    "GeneMap 99",
    "Whitehead YAC",
    "Whitehead RH",
    "Stanford TNG",
};

enum stsMapOptEnum smoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, stsMapOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *smoeEnumToString(enum stsMapOptEnum x)
/* Convert from enum to string representation. */
{
return stsMapOptions[x];
}

void smoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, stsMapOptions, ArraySize(stsMapOptions), 
	curVal);
}

/****** Some stuff for stsMapMouseNew related controls *******/

static char *stsMapMouseOptions[] = {
    "All Genetic",
    "WICGR Genetic Map",
    "MGD Genetic Map",
    "RH",
};

enum stsMapMouseOptEnum smmoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, stsMapMouseOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *smmoeEnumToString(enum stsMapMouseOptEnum x)
/* Convert from enum to string representation. */
{
return stsMapMouseOptions[x];
}

void smmoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, stsMapMouseOptions, ArraySize(stsMapMouseOptions), 
	curVal);
}

/****** Some stuff for stsMapRat related controls *******/

static char *stsMapRatOptions[] = {
    "All Genetic",
    "FHHxACI",
    "SHRSPxBN",
    "RH",
};

enum stsMapRatOptEnum smroeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, stsMapRatOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *smroeEnumToString(enum stsMapRatOptEnum x)
/* Convert from enum to string representation. */
{
return stsMapRatOptions[x];
}

void smroeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, stsMapRatOptions, ArraySize(stsMapRatOptions), 
	curVal);
}

/****** Some stuff for fishClones related controls *******/

static char *fishClonesOptions[] = {
    "Fred Hutchinson CRC",
    "National Cancer Institute",
    "Sanger Centre",
    "Roswell Park Cancer Institute",
    "Cedars-Sinai Medical Center",
    "Los Alamos National Lab",
    "UC San Francisco",
};

enum fishClonesOptEnum fcoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, fishClonesOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *fcoeEnumToString(enum fishClonesOptEnum x)
/* Convert from enum to string representation. */
{
return fishClonesOptions[x];
}

void fcoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, fishClonesOptions, ArraySize(fishClonesOptions), 
	curVal);
}

/****** Some stuff for recombRate related controls *******/

static char *recombRateOptions[] = {
    "deCODE Sex Averaged Distances",
    "deCODE Female Distances",
    "deCODE Male Distances",
    "Marshfield Sex Averaged Distances",
    "Marshfield Female Distances",
    "Marshfield Male Distances",
    "Genethon Sex Averaged Distances",
    "Genethon Female Distances",
    "Genethon Male Distances",
};

enum recombRateOptEnum rroeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, recombRateOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *rroeEnumToString(enum recombRateOptEnum x)
/* Convert from enum to string representation. */
{
return recombRateOptions[x];
}

void rroeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, recombRateOptions, ArraySize(recombRateOptions), 
	curVal);
}

/****** Some stuff for recombRateRat related controls *******/

static char *recombRateRatOptions[] = {
    "SHRSPxBN Sex Averaged Distances",
    "FHHxACI Sex Averaged Distances",
};

enum recombRateRatOptEnum rrroeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, recombRateRatOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *rrroeEnumToString(enum recombRateRatOptEnum x)
/* Convert from enum to string representation. */
{
return recombRateRatOptions[x];
}

void rrroeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, recombRateRatOptions, ArraySize(recombRateRatOptions), 
	curVal);
}

/****** Some stuff for recombRateMouse related controls *******/

static char *recombRateMouseOptions[] = {
    "WI Genetic Map Sex Averaged Distances",
    "MGD Genetic Map Sex Averaged Distances",
};

enum recombRateMouseOptEnum rrmoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, recombRateMouseOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *rrmoeEnumToString(enum recombRateMouseOptEnum x)
/* Convert from enum to string representation. */
{
return recombRateMouseOptions[x];
}

void rrmoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, recombRateMouseOptions, ArraySize(recombRateMouseOptions), 
	curVal);
}

/****** Some stuff for CGH NCI60 related controls *******/

static char *cghNci60Options[] = {
    "Tissue Averages",
    "BREAST",
    "CNS",
    "COLON",
    "LEUKEMIA",
    "LUNG",
    "MELANOMA",
    "OVARY",
    "PROSTATE",
    "RENAL",
    "All Cell Lines",
};

enum cghNci60OptEnum cghoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, cghNci60Options);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *cghoeEnumToString(enum cghNci60OptEnum x)
/* Convert from enum to string representation. */
{
return cghNci60Options[x];
}

void cghoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, cghNci60Options, ArraySize(cghNci60Options), 
	curVal);
}

/****** Some stuff for nci60 related controls *******/

static char *nci60Options[] = {
    "Tissue Averages",
    "All Cell Lines",
    "BREAST",
    "CNS",
    "COLON",
    "LEUKEMIA",
    "MELANOMA",
    "OVARIAN",
    "PROSTATE",
    "RENAL",
    "NSCLC",
    "DUPLICATE",
    "UNKNOWN"
    };

enum nci60OptEnum nci60StringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, nci60Options);
if (x < 0)
   errAbort("hui::nci60StringToEnum() - Unknown option %s", string);
return x;
}

char *nci60EnumToString(enum nci60OptEnum x)
/* Convert from enum to string representation. */
{
return nci60Options[x];
}

void nci60DropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, nci60Options, ArraySize(nci60Options), 
	curVal);
}


/*** Control of base/codon coloring code: ***/

/* All options (parallel to enum baseColorDrawOpt): */
static char *baseColorDrawAllOptionLabels[] =
    {
    BASE_COLOR_DRAW_OFF_LABEL,
    BASE_COLOR_DRAW_GENOMIC_CODONS_LABEL,
    BASE_COLOR_DRAW_ITEM_CODONS_LABEL,
    BASE_COLOR_DRAW_DIFF_CODONS_LABEL,
    BASE_COLOR_DRAW_ITEM_BASES_LABEL,
    BASE_COLOR_DRAW_DIFF_BASES_LABEL,
    };
static char *baseColorDrawAllOptionValues[] =
    {
    BASE_COLOR_DRAW_OFF,
    BASE_COLOR_DRAW_GENOMIC_CODONS,
    BASE_COLOR_DRAW_ITEM_CODONS,
    BASE_COLOR_DRAW_DIFF_CODONS,
    BASE_COLOR_DRAW_ITEM_BASES,
    BASE_COLOR_DRAW_DIFF_BASES,
    };

/* Subset of options for tracks with CDS info but not item sequence: */
static char *baseColorDrawGenomicOptionLabels[] =
    {
    BASE_COLOR_DRAW_OFF_LABEL,
    BASE_COLOR_DRAW_GENOMIC_CODONS_LABEL,
    };
static char *baseColorDrawGenomicOptionValues[] =
    {
    BASE_COLOR_DRAW_OFF,
    BASE_COLOR_DRAW_GENOMIC_CODONS,
    };

/* Subset of options for tracks with aligned item sequence but not CDS: */
static char *baseColorDrawItemOptionLabels[] =
    {
    BASE_COLOR_DRAW_OFF_LABEL,
    BASE_COLOR_DRAW_ITEM_BASES_LABEL,
    BASE_COLOR_DRAW_DIFF_BASES_LABEL,
    };
static char *baseColorDrawItemOptionValues[] =
    {
    BASE_COLOR_DRAW_OFF,
    BASE_COLOR_DRAW_ITEM_BASES,
    BASE_COLOR_DRAW_DIFF_BASES,
    };

enum baseColorDrawOpt baseColorDrawOptStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, baseColorDrawAllOptionValues);
if (x < 0)
   errAbort("hui::baseColorDrawOptStringToEnum() - Unknown option %s", string);
return x;
}

static boolean baseColorGotCds(struct trackDb *tdb)
/* Return true if this track has CDS info according to tdb (or is genePred). */
{
boolean gotIt = FALSE;
char *setting = trackDbSetting(tdb, BASE_COLOR_USE_CDS);
if (isNotEmpty(setting))
    {
    if (sameString(setting, "all") || sameString(setting, "given") ||
	sameString(setting, "genbank") || startsWith("table", setting))
	gotIt = TRUE;
    else if (! sameString(setting, "none"))
	errAbort("trackDb for %s, setting %s: unrecognized value \"%s\".  "
		 "must be one of {none,all,given,genbank,table}.",
		 tdb->tableName, BASE_COLOR_USE_CDS, setting);
    }
else if (startsWith("genePred", tdb->type))
    gotIt = TRUE;
return gotIt;
}

static boolean baseColorGotSequence(struct trackDb *tdb)
/* Return true if this track has aligned sequence according to tdb. */
{
boolean gotIt = FALSE;
char *setting = trackDbSetting(tdb, BASE_COLOR_USE_SEQUENCE);
if (isNotEmpty(setting))
    {
    if (sameString(setting, "genbank") || sameString(setting, "seq") ||
	sameString(setting, "ss") || startsWith("extFile", setting) ||
	sameString(setting, "hgPcrResult"))
	gotIt = TRUE;
    else if (sameString(setting, "none"))
	errAbort("trackDb for %s, setting %s: unrecognized value \"%s\".  "
		 "must be one of {none,genbank,seq,extFile}.",
		 tdb->tableName, BASE_COLOR_USE_SEQUENCE, setting);
    }
return gotIt;
}

void baseColorDrawOptDropDown(struct cart *cart, struct trackDb *tdb)
/* Make appropriately labeled drop down of options if any are applicable.*/
{
enum baseColorDrawOpt curOpt = baseColorDrawOptEnabled(cart, tdb);
char *curValue = baseColorDrawAllOptionValues[curOpt];
char var[512];
boolean gotCds = baseColorGotCds(tdb);
boolean gotSeq = baseColorGotSequence(tdb);

safef(var, sizeof(var), "%s." BASE_COLOR_VAR_SUFFIX, tdb->tableName);
if (gotCds && gotSeq)
    {
    puts("<P><B>Color track by codons or bases:</B>");
    cgiMakeDropListFull(var, baseColorDrawAllOptionLabels,
			baseColorDrawAllOptionValues,
			ArraySize(baseColorDrawAllOptionLabels),
			curValue, NULL);
    printf("<BR><A HREF=\"%s\">Help on mRNA coloring</A><BR>",
	   CDS_MRNA_HELP_PAGE);
    }
else if (gotCds)
    {
    puts("<P><B>Color track by codons:</B>");
    cgiMakeDropListFull(var, baseColorDrawGenomicOptionLabels,
			baseColorDrawGenomicOptionValues,
			ArraySize(baseColorDrawGenomicOptionLabels),
			curValue, NULL);
    printf("<BR><A HREF=\"%s\">Help on codon coloring</A><BR>",
	   CDS_HELP_PAGE);
    }
else if (gotSeq)
    {
    puts("<P><B>Color track by bases:</B>");
    cgiMakeDropListFull(var, baseColorDrawItemOptionLabels,
			baseColorDrawItemOptionValues,
			ArraySize(baseColorDrawItemOptionLabels),
			curValue, NULL);
    printf("<BR><A HREF=\"%s\">Help on mRNA coloring</A><BR>",
	   CDS_MRNA_HELP_PAGE);
    }
}

enum baseColorDrawOpt baseColorDrawOptEnabled(struct cart *cart,
					      struct trackDb *tdb)
/* Query cart & trackDb to determine what drawing mode (if any) is enabled. */
{
char *stringVal = NULL;
char optionStr[256];
assert(cart);
assert(tdb);

/* trackDb can override default of OFF; cart can override trackDb. */
stringVal = trackDbSettingOrDefault(tdb, BASE_COLOR_DEFAULT,
				    BASE_COLOR_DRAW_OFF);
safef(optionStr, sizeof(optionStr), "%s." BASE_COLOR_VAR_SUFFIX,
      tdb->tableName);
stringVal = cartUsualString(cart, optionStr, stringVal);

return baseColorDrawOptStringToEnum(stringVal);
}


/*** Control of fancy indel display code: ***/

static boolean indelAppropriate(struct trackDb *tdb)
/* Return true if it makes sense to offer indel display options for tdb. */
{
return (tdb && startsWith("psl", tdb->type) &&
	(cfgOptionDefault("browser.indelOptions", NULL) != NULL));
}

void indelShowOptions(struct cart *cart, struct trackDb *tdb)
/* Make HTML inputs for indel display options if any are applicable. */
{
if (indelAppropriate(tdb))
    {
    boolean showDoubleInsert, showQueryInsert, showPolyA;
    char var[512];
    indelEnabled(cart, tdb, 0.0, &showDoubleInsert, &showQueryInsert, &showPolyA);
    printf("<P><B>Alignment Gap/Insertion Display Options</B><BR>\n");
    safef(var, sizeof(var), "%s_%s", INDEL_DOUBLE_INSERT, tdb->tableName);
    cgiMakeCheckBox(var, showDoubleInsert);
    printf("Draw double horizontal lines when both genome and query have "
	   "an insertion "
	   "<BR>\n");
    safef(var, sizeof(var), "%s_%s", INDEL_QUERY_INSERT, tdb->tableName);
    cgiMakeCheckBox(var, showQueryInsert);
    printf("Draw a vertical blue line for an insertion at the beginning or "
	   "end of the query, orange for insertion in the middle of the query"
	   "<BR>\n");
    safef(var, sizeof(var), "%s_%s", INDEL_POLY_A, tdb->tableName);
    /* We can highlight valid polyA's only if we have query sequence -- 
     * so indelPolyA code piggiebacks on baseColor code: */
    if (baseColorGotSequence(tdb))
	{
	cgiMakeCheckBox(var, showPolyA);
	printf("Draw a vertical green line where query has a polyA tail "
	       "insertion"
	       "<BR>\n");
	}
	
    printf("<A HREF=\"%s\">Help on alignment gap/insertion display options</A>"
	   "<BR>\n",
	   INDEL_HELP_PAGE);
    }
}

static boolean tdbOrCartBoolean(struct cart *cart, struct trackDb *tdb,
				char *settingName, char *defaultOnOff)
/* Query cart & trackDb to determine if a boolean variable is set. */
{
boolean alreadySet;
char optionStr[512];
alreadySet = !sameString("off",
		trackDbSettingOrDefault(tdb, settingName, defaultOnOff));
safef(optionStr, sizeof(optionStr), "%s_%s",
      settingName, tdb->tableName);
alreadySet = cartUsualBoolean(cart, optionStr, alreadySet);
return alreadySet;
}

void indelEnabled(struct cart *cart, struct trackDb *tdb, float basesPerPixel,
		  boolean *retDoubleInsert, boolean *retQueryInsert,
		  boolean *retPolyA)
/* Query cart & trackDb to determine what indel display (if any) is enabled. Set
 * basesPerPixel to 0.0 to disable check for zoom level.  */
{
boolean apropos = indelAppropriate(tdb);
if (apropos && (basesPerPixel > 0.0))
    {
    // check indel max zoom
    char *setting = trackDbSetting(tdb, "showIndelMaxZoom");
    if (setting != NULL)
        {
        float showIndelMaxZoom = sqlFloat(trimSpaces(setting));
        if ((basesPerPixel > showIndelMaxZoom) || (showIndelMaxZoom == 0.0))
            apropos = FALSE;
        }
    }

if (retDoubleInsert)
    *retDoubleInsert = apropos &&
	tdbOrCartBoolean(cart, tdb, INDEL_DOUBLE_INSERT, "off");
if (retQueryInsert)
    *retQueryInsert = apropos &&
	tdbOrCartBoolean(cart, tdb, INDEL_QUERY_INSERT, "off");
if (retPolyA)
    *retPolyA = apropos &&
	tdbOrCartBoolean(cart, tdb, INDEL_POLY_A, "off");
}


/****** base position (ruler) controls *******/

static char *zoomOptions[] = {
    ZOOM_1PT5X,
    ZOOM_3X,
    ZOOM_10X,
    ZOOM_BASE
    };

void zoomRadioButtons(char *var, char *curVal)
/* Make a list of radio buttons for all zoom options */
{
int i;
int size = ArraySize(zoomOptions);
for (i = 0; i < size; i++)
    {
    char *s = zoomOptions[i];
    cgiMakeRadioButton(var, s, sameString(s, curVal));
    printf(" %s &nbsp;&nbsp;", s);   
    }
}

/****** Some stuff for affy related controls *******/

static char *affyOptions[] = {
    "Chip Type",
    "Chip ID",
    "Tissue Averages"
    };

enum affyOptEnum affyStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, affyOptions);
if (x < 0)
   errAbort("hui::affyStringToEnum() - Unknown option %s", string);
return x;
}

char *affyEnumToString(enum affyOptEnum x)
/* Convert from enum to string representation. */
{
return affyOptions[x];
}

void affyDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, affyOptions, ArraySize(affyOptions), 
	curVal);
}

/****** Some stuff for affy all exon related controls *******/

static char *affyAllExonOptions[] = {
    "Chip",
    "Tissue Averages"
    };

enum affyAllExonOptEnum affyAllExonStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, affyAllExonOptions);
if (x < 0)
   errAbort("hui::affyAllExonStringToEnum() - Unknown option %s", string);
return x;
}

char *affyAllExonEnumToString(enum affyAllExonOptEnum x)
/* Convert from enum to string representation. */
{
return affyAllExonOptions[x];
}

void affyAllExonDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, affyAllExonOptions, ArraySize(affyAllExonOptions), 
	curVal);
}

/****** Some stuff for Rosetta related controls *******/

static char *rosettaOptions[] = {
    "All Experiments",
    "Common Reference and Other",
    "Common Reference",
    "Other Exps"
};

static char *rosettaExonOptions[] = {
    "Confirmed Only",
    "Predicted Only",
    "All",
};

enum rosettaOptEnum rosettaStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, rosettaOptions);
if (x < 0)
   errAbort("hui::rosettaStringToEnum() - Unknown option %s", string);
return x;
}

char *rosettaEnumToString(enum rosettaOptEnum x)
/* Convert from enum to string representation. */
{
return rosettaOptions[x];
}

void rosettaDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, rosettaOptions, ArraySize(rosettaOptions), 
	curVal);
}

enum rosettaExonOptEnum rosettaStringToExonEnum(char *string)
/* Convert from string to enum representation of exon types. */
{
int x = stringIx(string, rosettaExonOptions);
if (x < 0)
   errAbort("hui::rosettaStringToExonEnum() - Unknown option %s", string);
return x;
}

char *rosettaExonEnumToString(enum rosettaExonOptEnum x) 
/* Convert from enum to string representation of exon types. */
{
return rosettaExonOptions[x];
}

void rosettaExonDropDown(char *var, char *curVal)
/* Make drop down of exon type options. */
{
cgiMakeDropList(var, rosettaExonOptions, ArraySize(rosettaExonOptions), curVal);
}

/****** Options for the chain track color options *******/
static char *chainColorOptions[] = {
    CHROM_COLORS,
    SCORE_COLORS,
    NO_COLORS
    };

enum chainColorEnum chainColorStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, chainColorOptions);
if (x < 0)
   errAbort("hui::chainColorStringToEnum() - Unknown option %s", string);
return x;
}

char *chainColorEnumToString(enum chainColorEnum x)
/* Convert from enum to string representation. */
{
return chainColorOptions[x];
}

void chainColorDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, chainColorOptions, ArraySize(chainColorOptions), curVal);
}

/****** Options for the wiggle track Windowing *******/

static char *wiggleWindowingOptions[] = {
    "maximum",
    "mean",
    "minimum"
    };

enum wiggleWindowingEnum wiggleWindowingStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleWindowingOptions);
if (x < 0)
   errAbort("hui::wiggleWindowingStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleWindowingEnumToString(enum wiggleWindowingEnum x)
/* Convert from enum to string representation. */
{
return wiggleWindowingOptions[x];
}

void wiggleWindowingDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleWindowingOptions, ArraySize(wiggleWindowingOptions), 
	curVal);
}

/****** Options for the wiggle track Smoothing *******/

static char *wiggleSmoothingOptions[] = {
    "OFF", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
    "12", "13", "14", "15", "16"
    };

enum wiggleSmoothingEnum wiggleSmoothingStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleSmoothingOptions);
if (x < 0)
   errAbort("hui::wiggleSmoothingStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleSmoothingEnumToString(enum wiggleSmoothingEnum x)
/* Convert from enum to string representation. */
{
return wiggleSmoothingOptions[x];
}

void wiggleSmoothingDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleSmoothingOptions, ArraySize(wiggleSmoothingOptions), 
	curVal);
}

/****** Options for the wiggle track y Line Mark On/Off *******/

static char *wiggleYLineMarkOptions[] = {
    "OFF",
    "ON"
    };

enum wiggleYLineMarkEnum wiggleYLineMarkStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleYLineMarkOptions);
if (x < 0)
   errAbort("hui::wiggleYLineMarkStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleYLineMarkEnumToString(enum wiggleYLineMarkEnum x)
/* Convert from enum to string representation. */
{
return wiggleYLineMarkOptions[x];
}

void wiggleYLineMarkDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleYLineMarkOptions, ArraySize(wiggleYLineMarkOptions), 
	curVal);
}

/****** Options for the wiggle track AutoScale *******/

static char *wiggleScaleOptions[] = {
    "use vertical viewing range setting",
    "auto-scale to data view"
    };

enum wiggleScaleOptEnum wiggleScaleStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleScaleOptions);
if (x < 0)
   errAbort("hui::wiggleScaleStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleScaleEnumToString(enum wiggleScaleOptEnum x)
/* Convert from enum to string representation. */
{
return wiggleScaleOptions[x];
}

void wiggleScaleDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleScaleOptions, ArraySize(wiggleScaleOptions), 
	curVal);
}

/****** Options for the wiggle track type of graph *******/

static char *wiggleGraphOptions[] = {
    "points",
    "bar"
    };

enum wiggleGraphOptEnum wiggleGraphStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleGraphOptions);
if (x < 0)
   errAbort("hui::wiggleGraphStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleGraphEnumToString(enum wiggleGraphOptEnum x)
/* Convert from enum to string representation. */
{
return wiggleGraphOptions[x];
}

void wiggleGraphDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleGraphOptions, ArraySize(wiggleGraphOptions), 
	curVal);
}

/****** Options for the wiggle track horizontal grid lines *******/

static char *wiggleGridOptions[] = {
    "ON",
    "OFF"
    };

enum wiggleGridOptEnum wiggleGridStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleGridOptions);
if (x < 0)
   errAbort("hui::wiggleGridStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleGridEnumToString(enum wiggleGridOptEnum x)
/* Convert from enum to string representation. */
{
return wiggleGridOptions[x];
}

void wiggleGridDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleGridOptions, ArraySize(wiggleGridOptions), 
	curVal);
}

/****** Some stuff for wiggle track related controls *******/

static char *wiggleOptions[] = {
    "samples only",
    "linear interpolation"
    };

enum wiggleOptEnum wiggleStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleOptions);
if (x < 0)
   errAbort("hui::wiggleStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleEnumToString(enum wiggleOptEnum x)
/* Convert from enum to string representation. */
{
return wiggleOptions[x];
}

void wiggleDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleOptions, ArraySize(wiggleOptions), 
	curVal);
}


/****** Some stuff for GCwiggle track related controls *******/

static char *GCwiggleOptions[] = {
    "samples only",
    "linear interpolation"
    };

enum GCwiggleOptEnum GCwiggleStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, GCwiggleOptions);
if (x < 0)
   errAbort("hui::GCwiggleStringToEnum() - Unknown option %s", string);
return x;
}

char *GCwiggleEnumToString(enum GCwiggleOptEnum x)
/* Convert from enum to string representation. */
{
return GCwiggleOptions[x];
}

void GCwiggleDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, GCwiggleOptions, ArraySize(GCwiggleOptions), 
	curVal);
}

/****** Some stuff for chimp track related controls *******/

static char *chimpOptions[] = {
    "samples only",
    "linear interpolation"
    };

enum chimpOptEnum chimpStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, chimpOptions);
if (x < 0)
   errAbort("hui::chimpStringToEnum() - Unknown option %s", string);
return x;
}

char *chimpEnumToString(enum chimpOptEnum x)
/* Convert from enum to string representation. */
{
return chimpOptions[x];
}

void chimpDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, chimpOptions, ArraySize(chimpOptions), 
	curVal);
}



/****** Some stuff for mRNA and EST related controls *******/

static void addMrnaFilter(struct mrnaUiData *mud, char *track, char *label, char *key, char *table)
/* Add an mrna filter */
{
struct mrnaFilter *fil;
char buf[128];
AllocVar(fil);
fil->label = label;
safef(buf, sizeof(buf), "%s_%s", track, key);
fil->key = cloneString(buf);
fil->table = table;
slAddTail(&mud->filterList, fil);
}

struct mrnaUiData *newBedUiData(char *track)
/* Make a new  in extra-ui data structure for a bed. */
{
struct mrnaUiData *mud;
char buf[128];  /* Expand me here */
AllocVar(mud);
safef(buf, sizeof(buf), "%sFt", track);
mud->filterTypeVar = cloneString(buf);
safef(buf, sizeof(buf), "%sLt", track);
mud->logicTypeVar = cloneString(buf);
addMrnaFilter(mud, track, "name", "name",track);
return mud;
}

struct mrnaUiData *newMrnaUiData(char *track, boolean isXeno)
/* Make a new  in extra-ui data structure for mRNA. */
{
struct mrnaUiData *mud;
char buf[128];
AllocVar(mud);
safef(buf, sizeof(buf), "%sFt", track);
mud->filterTypeVar = cloneString(buf);
safef(buf, sizeof(buf), "%sLt", track);
mud->logicTypeVar = cloneString(buf);
if (isXeno)
    addMrnaFilter(mud, track, "organism", "org", "organism");
addMrnaFilter(mud, track, "accession", "acc", "acc");
addMrnaFilter(mud, track, "author", "aut", "author");
addMrnaFilter(mud, track, "library", "lib", "library");
addMrnaFilter(mud, track, "tissue", "tis", "tissue");
addMrnaFilter(mud, track, "cell", "cel", "cell");
addMrnaFilter(mud, track, "keyword", "key", "keyword");
addMrnaFilter(mud, track, "gene", "gen", "geneName");
addMrnaFilter(mud, track, "product", "pro", "productName");
addMrnaFilter(mud, track, "description", "des", "description");
return mud;
}

int trackNameAndLabelCmp(const void *va, const void *vb)
/* Compare to sort on label. */
{
const struct trackNameAndLabel *a = *((struct trackNameAndLabel **)va);
const struct trackNameAndLabel *b = *((struct trackNameAndLabel **)vb);
return strcmp(a->label, b->label);
}

char *trackFindLabel(struct trackNameAndLabel *list, char *label)
/* Try to find label in list. Return NULL if it's
 * not there. */
{
struct trackNameAndLabel *el;
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(el->label, label))
        return label;
    }
return NULL;
}

char *genePredDropDown(struct cart *cart, struct hash *trackHash,  
                                        char *formName, char *varName)
/* Make gene-prediction drop-down().  Return track name of
 * currently selected one.  Return NULL if no gene tracks.
 * If formName isn't NULL, it's the form for auto submit (onchange attr).
 * If formName is NULL, no submit occurs when menu is changed */
{
char *cartTrack = cartOptionalString(cart, varName);
struct hashEl *trackList, *trackEl;
char *selectedName = NULL;
struct trackNameAndLabel *nameList = NULL, *name;
char *trackName = NULL;

/* Make alphabetized list of all genePred track names. */
trackList = hashElListHash(trackHash);
for (trackEl = trackList; trackEl != NULL; trackEl = trackEl->next)
    {
    struct trackDb *tdb = trackEl->val;
    char *dupe = cloneString(tdb->type);
    char *type = firstWordInLine(dupe);
    if ((sameString(type, "genePred")) && (!sameString(tdb->tableName, "tigrGeneIndex") && !trackDbIsComposite(tdb)))
	{
	AllocVar(name);
	name->name = tdb->tableName;
	name->label = tdb->shortLabel;
	slAddHead(&nameList, name);
	}
    freez(&dupe);
    }
slSort(&nameList, trackNameAndLabelCmp);

/* No gene tracks - not much we can do. */
if (nameList == NULL)
    {
    slFreeList(&trackList);
    return NULL;
    }

/* Try to find current track - from cart first, then
 * knownGenes, then refGenes. */
if (cartTrack != NULL)
    selectedName = trackFindLabel(nameList, cartTrack);
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "Known Genes");
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "SGD Genes");
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "BDGP Genes");
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "WormBase Genes");
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "RefSeq Genes");
if (selectedName == NULL)
    selectedName = nameList->name;

/* Make drop-down list. */
    {
    char javascript[64], *autoSubmit;
    int nameCount = slCount(nameList);
    char **menu;
    int i;

    AllocArray(menu, nameCount);
    for (name = nameList, i=0; name != NULL; name = name->next, ++i)
	{
	menu[i] = name->label;
	}
    if (formName == NULL)
        autoSubmit = NULL;
    else
        { 
        safef(javascript, sizeof(javascript), 
                "onchange=\"document.%s.submit();\"", formName);
        autoSubmit = javascript;
        }
    cgiMakeDropListFull(varName, menu, menu, 
    	                        nameCount, selectedName, autoSubmit);
    freez(&menu);
    }

/* Convert to track name */
for (name = nameList; name != NULL; name = name->next)
    {
    if (sameString(selectedName, name->label))
        trackName = name->name;
    }

/* Clean up and return. */
slFreeList(&nameList);
slFreeList(&trackList);
return trackName;
}

struct hash *makeTrackHashWithComposites(char *database, char *chrom, 
                                        bool withComposites)
/* Make hash of trackDb items for this chromosome, including composites,
   not just the subtracks. */
{
struct trackDb *tdbs = hTrackDb(chrom);
struct hash *trackHash = newHash(7);

while (tdbs != NULL)
    {
    struct trackDb *tdb = slPopHead(&tdbs);
    hLookupStringsInTdb(tdb, database);
    if (hTrackOnChrom(tdb, chrom))
        {
        if (tdb->subtracks)
            {
            struct trackDb *subtrack;
            for (subtrack = tdb->subtracks; subtrack != NULL; 
                        subtrack = subtrack->next)
                {
                /* need track description for hgc */
                subtrack->html = cloneString(tdb->html);
                hashAdd(trackHash, subtrack->tableName, subtrack);
                }
            if (withComposites)
                hashAdd(trackHash, tdb->tableName, tdb);
            }
        else
            hashAdd(trackHash, tdb->tableName, tdb);
        }
    else
        trackDbFree(&tdb);
    }

return trackHash;
}

struct hash *makeTrackHash(char *database, char *chrom)
/* Make hash of trackDb items for this chromosome. */
{
    return makeTrackHashWithComposites(database, chrom, FALSE);
}

/****** Stuff for acembly related options *******/

static char *acemblyOptions[] = {
    "all genes",
    "main",
    "putative",
};

enum acemblyOptEnum acemblyStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, acemblyOptions);
if (x < 0)
   errAbort("hui::acemblyStringToEnum() - Unknown option %s", string);
return x;
}

char *acemblyEnumToString(enum acemblyOptEnum x)
/* Convert from enum to string representation. */
{
return acemblyOptions[x];
}

void acemblyDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, acemblyOptions, ArraySize(acemblyOptions), 
	curVal);
}

boolean parseAssignment(char *words, char **name, char **value)
/* parse <name>=<value>, destroying input words in the process */
{
char *p;
if ((p = index(words, '=')) == NULL)
    return FALSE;
*p++ = 0;
if (name)
    *name = words;
if (value)
    *value = p;
return TRUE;
}

static char *getPrimaryType(char *primarySubtrack, struct trackDb *tdb)
/* Do not free when done. */
{
struct trackDb *subtrack = NULL;
char *type = NULL;
if (primarySubtrack)
    for (subtrack = tdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
	{
	if (sameString(subtrack->tableName, primarySubtrack))
	    {
	    type = subtrack->type;
	    break;
	    }
	}
return type;
}

static boolean dimensionsExist(struct trackDb *parentTdb)
/* Does this parent track contain dimensions? */
{
    return (trackDbSetting(parentTdb, "dimensions") != NULL);
}
static boolean dimensionsSubtrackOf(struct trackDb *childTdb)
/* Does this child belong to a parent  with dimensions? */
{
    return (childTdb->parent != NULL && dimensionsExist(childTdb->parent));
}

static int dimensionSettingsGet(struct trackDb *parentTdb,char ***dimensions,char ***subgroups)
/* Parses any dimemnions setting in parent of subtracks */
{
int cnt,ix,dIx=0;
*dimensions = (void *)NULL;
*subgroups  = (void *)NULL;
char *setting = trackDbSetting(parentTdb, "dimensions");
if(setting != NULL)
    {
    char *words[64];
    cnt = chopByWhite(cloneString(setting),words,64);
    assert(cnt<=64);
    if(cnt > 0)
        {
        *dimensions = needMem(cnt*sizeof(char*));
        *subgroups  = needMem(cnt*sizeof(char*));
        char *name,*value;
        for (ix = 0,dIx=0; ix < cnt; ix++)
            {
            if (parseAssignment(words[ix], &name, &value))
                {
                (*dimensions)[dIx] = name;
                (*subgroups)[dIx]  = value;
                dIx++;
                }
            }
        }
    }
return dIx;  // NOTE cloneString:words[0]==*dimensions[0] and will be freed when *dimemsions is freed
}
static void dimensionsFree(char ***dimensions,char ***subgroups)
/* frees any previously obtained dividers setting */
{
if(dimensions && *dimensions)
    {
    freeMem((*dimensions)[0]); // NOTE cloneString:words[0]==*dimensions[0] and is freed now
    freez(dimensions);
    }
if(subgroups && *subgroups)
    freez(subgroups);
}

static int dividersSettingGet(struct trackDb *parentTdb,char ***dividers)
/* Parses any dividers setting in parent of subtracks */
{
int cnt=0;
*dividers = (void *)NULL;
char *setting = trackDbSetting(parentTdb, "dividers");
if(setting != NULL)
    {
    *dividers = needMem(24*sizeof(char*));
    cnt = chopByWhite(cloneString(setting), *dividers,24);
    }
return cnt;
}
static void dividersFree(char ***dividers)
/* frees any previously obtained dividers setting */
{
if(dividers && *dividers)
    freez(dividers);
}

static int hierarchySettingGet(struct trackDb *parentTdb,char ***labels,int **indents)
/* Parses any list hierachy instructions setting in parent of subtracks */
{
int cnt,ix,hIx=0;
*labels  = (void *)NULL;
*indents = (void *)NULL;
char *setting = trackDbSetting(parentTdb, "hierarchy");
if(setting != NULL)
    {
    char *words[64];
    cnt = chopByWhite(cloneString(setting), words,64);
    assert(cnt<=64);
    if(cnt > 1)
        {
        *labels  = needMem(cnt*sizeof(char*));
        *indents = needMem(cnt*sizeof(int));
        char *name,*value;
        for (ix = 1,hIx=0; ix < cnt; ix++)
            {
            if (parseAssignment(words[ix], &name, &value))
                {
                (*labels)[hIx]  = name;
                (*indents)[hIx] = atoi(value);
                hIx++;
                }
            }
        (*labels)[hIx] = words[0]; // so that it can be freed later 
        }
    }
return hIx;  // NOTE cloneString:words[0]==*label[hCnt] and will be freed when *labels are freed
}
static void hierarchyFree(int hCnt,char***labels,int **indents)
/* frees any previously obtained hierachy settings */
{
if(labels && *labels)
    {
    freeMem((*labels)[hCnt]); // NOTE cloneString:words[0]==*label[hCnt] and is freed now
    freez(labels);
    }
if(indents && *indents)
    freez(indents);
}

#define SUBGROUP_MAX 9
static int subgroupMembersGet(struct trackDb *parentTdb, char *settingName,char **tag,char **title,char ***names,char ***values)
/* Parse a subGroup setting line into tag,title, names(optional) and values(optional), returning the count of members or 0 */
{
int ix,cnt,members=0;
char *target = NULL;

*tag    = NULL;
*title  = NULL;
if(names != (void*)NULL)
    *names  = NULL;
if(values != (void*)NULL)
    *values = NULL;
char subGrp[16];
if(sameStringN(settingName,"subGroup",8))
    target = cloneString(trackDbSetting(parentTdb, settingName));
if(target == NULL)
    {    
    for(ix=1;ix<=SUBGROUP_MAX;ix++) // How many do we support?
        {
        safef(subGrp, ArraySize(subGrp), "subGroup%d",ix);
        char *setting = trackDbSetting(parentTdb, subGrp);
        if(setting != NULL)
            {
            if(startsWithWord(settingName,setting))
                {
                target = cloneString(setting);
                break;
                }
            }
        }
    }
if(target == NULL)
    return 0;

char *words[64];
cnt = chopLine(target, words);
assert(cnt <= 64);
*tag = words[0];
*title = strSwapChar(words[1],'_',' '); // Titles replace '_' with space
if(names != (void*)NULL)
    *names  = needMem(cnt*sizeof(char*));
if(values != (void*)NULL)
    *values = needMem(cnt*sizeof(char*));
for (ix = 2,members=0; ix < cnt; ix++)
    {
    char *name,*value;
    if (parseAssignment(words[ix], &name, &value))
        {
        if(names != (void*)NULL && *names != (void*)NULL)
            (*names)[members] = name;
        if(values != (void*)NULL && *values != (void*)NULL)
            (*values)[members] = strSwapChar(value,'_',' ');
        members++;
        }
    }
return members;
}

static int subgroupMembersGetByDimension(struct trackDb *parentTdb, char dimension,char **tag,char **title,char ***names,char ***values)
/* Finds the dimension requested and return its associated 
 * subgroupMembership, returning the count of members or 0 */
{
int ix,sCnt=0;
char **dimensions,**subgroups;
*tag    = NULL;
*title  = NULL;
if(names != (void*)NULL)
    *names  = NULL;
if(names != (void*)NULL)
    *values = NULL;
int dCnt = dimensionSettingsGet(parentTdb,&dimensions,&subgroups);
for(ix=0;ix<dCnt;ix++)
    {
    if(startsWith("dimension",dimensions[ix]) 
    && strlen(dimensions[ix]) == 10
    && lastChar(dimensions[ix]) == dimension)
        {
        sCnt = subgroupMembersGet(parentTdb, subgroups[ix],tag,title,names,values);
        break;
        }
    }
if(dCnt>0)
    dimensionsFree(&dimensions,&subgroups);
return sCnt;
}

static void subgroupMembersFree(char **tag,char **title,char ***names,char ***values)
/* frees memory for subgroupMembers lists */
{
if(tag && *tag)
    freez(tag); // checks for NULL and sets to NULL after free
if(names && *names)
    freez(names);
if(values && *values)
    freez(values);
}

boolean subgroupFind(struct trackDb *childTdb, char *name,char **value)
/* looks for a single tag in a child tracks subGroups setting */
{
if(value != (void*)NULL)
    *value = NULL;
char *subGroups = trackDbSetting(childTdb, "subGroups");
if(subGroups == (void*)NULL)
    return FALSE;
char *found = stringIn(name, subGroups);
if(found == (void*)NULL)
    return FALSE;
if(*(found+strlen(name)) != '=')
    return FALSE;
if(value != (void*)NULL)
    *value = firstWordInLine(cloneString(found+strlen(name)+1));
return TRUE;
}
void subgroupFree(char **value)
/* frees subgroup memory */
{
if(value && *value)
    freez(value);
}

static int subgroupMembershipGet(struct trackDb *childTdb, char ***names,char ***values)
/* gets all the subgroup membership for a child track */
{
int ix,members,cnt;
char *name,*value;
    
if(names != (void*)NULL)
    *names = NULL;
if(values != (void*)NULL)
    *values = NULL;
char *subGroups = trackDbSetting(childTdb, "subGroups");
if(subGroups == NULL)
    return 0;

char *words[64];
cnt = chopLine(cloneString(subGroups), words);
assert(cnt <= 64);
if(names != (void*)NULL)
    {
    *names  = needMem((cnt+1)*sizeof(char*));
    }
if(values != (void*)NULL)
    *values = needMem(cnt*sizeof(char*));
for (ix = 0,members=0; ix < cnt; ix++)
    {
    if (parseAssignment(words[ix], &name, &value))
        {
        if(names != (void*)NULL && *names != (void*)NULL)
            (*names)[members] = name;
        if(values != (void*)NULL && *values != (void*)NULL)
            (*values)[members] = strSwapChar(value,'_',' ');
        members++;
        }
    }
   if(names != (void*)NULL && *names != (void*)NULL)
        (*names)[members] = words[0]; // so that it can be freed later 
return members;
}
static void subgroupMembershipFree(int members,char ***names,char ***values)
/* frees subgroupMembership memory */
{
if(names && *names)
    {
    freeMem((*names)[members]); // NOTE cloneString:words[0]==*names[gCnt] and is freed now
    freez(names);
    }
if(values && *values)
    freez(values);
}

#define COLOR_BG_DEFAULT        "#FFFEE8"
#define COLOR_BG_ALTDEFAULT      "#FFF9D2"
#define COLOR_BG_DEFAULT_IX     0
#define COLOR_BG_ALTDEFAULT_IX  1
#define COLOR_DARKGREEN         "#008800"
#define COLOR_DARKBLUE          "#000088"
static void compositeUiSubtracks(struct cart *cart, struct trackDb *parentTdb,
				 boolean selectedOnly, char *primarySubtrack)
/* Show checkboxes for subtracks. */
{
struct trackDb *subtrack;
char *primaryType = getPrimaryType(primarySubtrack, parentTdb);
char option[64];
char *words[5];
char *colors[2]   = { COLOR_BG_DEFAULT,
                      COLOR_BG_ALTDEFAULT };
int colorIx = COLOR_BG_DEFAULT_IX; // Start with non-default allows alternation

char **dividers;
char **lastDivide = NULL;
int ix;
int dCnt = dividersSettingGet(parentTdb,&dividers);
if(dCnt> 0) 
    lastDivide = needMem(sizeof(char*)*dCnt);
char **hLabels;
int *hIndents;
int hCnt = hierarchySettingGet(parentTdb,&hLabels,&hIndents);
char *tagX,*titleX;
char *tagY,*titleY;
subgroupMembersGetByDimension(parentTdb,'X',&tagX,&titleX,NULL,NULL);
subgroupMembersGetByDimension(parentTdb,'Y',&tagY,&titleY,NULL,NULL);
boolean isMatrix = (tagX || tagY);

puts("\n<TABLE CELLSPACING=\"2\" CELLPADDING=\"1\" border=\"0\" >");

slSort(&(parentTdb->subtracks), trackDbCmp);

for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    boolean alreadySet = TRUE;
    boolean isPrimary = FALSE;
    char *setting;

    safef(option, sizeof(option), "%s_sel", subtrack->tableName);
    if ((setting = trackDbSetting(subtrack, "subTrack")) != NULL)
        {
        if (chopLine(cloneString(setting), words) >= 2)
            alreadySet = differentString(words[1], "off");
        }
    alreadySet = cartUsualBoolean(cart, option, alreadySet);
    isPrimary = (primarySubtrack &&
		 sameString(subtrack->tableName, primarySubtrack));

    if (primarySubtrack)
        {
        if (isPrimary)
            {
            puts("<TR><TD>");
            cgiMakeHiddenBoolean(option, TRUE);
            puts("[on] ");
            printf ("</TD><TD>%s [selected on main page]</TD></TR>\n",
                subtrack->longLabel);
            }
        else if (sameString(primaryType, subtrack->type) && hTableExists(subtrack->tableName))
            {
            puts("<TR><TD>");
            cgiMakeCheckBox(option, alreadySet);
            printf ("</TD><TD>%s</TD></TR>\n", subtrack->longLabel);
            }
        }
    else
        {
        if(!isMatrix) // TODO Remove this check when show/hide subtracks by js no longer restricted to cfgByMatrix
            {
            if(selectedOnly && !alreadySet)
                continue;
            }
        char **subgroups,**membership;
        int sCnt = subgroupMembershipGet(subtrack,&subgroups,&membership);
        if (hTableExists(subtrack->tableName))
            {
            char id[64] = "";
            if(isMatrix)
                {  // What is wanted: id="cb_ES_K4_SIG_cb"
                safecpy(id,sizeof(id),"cb_");
                if(tagX != NULL)
                    {
                    ix = stringArrayIx(tagX, subgroups, sCnt);
                    if(ix >= 0)
                        safef(id+strlen(id), sizeof(id)-strlen(id), "%s_", membership[ix]);
                    }
                if(tagY != NULL)
                    {
                    ix = stringArrayIx(tagY, subgroups, sCnt);
                    if(ix >= 0)
                        safef(id+strlen(id), sizeof(id)-strlen(id), "%s_", membership[ix]);
                    }
                ix = stringArrayIx("view", subgroups, sCnt);
                if(ix >= 0)
                    safef(id+strlen(id), sizeof(id)-strlen(id), "%s_", membership[ix]);
                safecat(id+strlen(id), sizeof(id)-strlen(id), "cb");
                }
            if(strlen(id) <= 5)
                safef(id, sizeof(id), "cb_%s", subtrack->shortLabel);

            boolean division = FALSE;
            if(lastDivide != NULL)
                {
                for(ix=0;ix<dCnt;ix++)
                    {
                    int sIx = stringArrayIx(dividers[ix],subgroups, sCnt);
                    if((lastDivide[ix] == (void*)NULL && sIx >= 0)
                    || (lastDivide[ix] != (void*)NULL && sIx <  0)
                    || (strcmp(lastDivide[ix],membership[sIx]) != 0) )
                        {
                        division = TRUE;
                        if(lastDivide[ix] != (void*)NULL)
                            freeMem(lastDivide[ix]);
                        lastDivide[ix] = ( sIx<0 ? (void*)NULL : cloneString(membership[sIx]) );
                        }
                    }
                }
            int indent = 0;
            if(hCnt>0)
                {
                for(ix=0;ix<sCnt;ix++)
                    {
                    int iIx = stringArrayIx(membership[ix], hLabels, hCnt);
                    if(iIx >= 0)
                        indent = hIndents[iIx];
                    }
                }
            if(division)
                {
                colorIx = (colorIx == COLOR_BG_DEFAULT_IX ? COLOR_BG_ALTDEFAULT_IX : COLOR_BG_DEFAULT_IX);
#define DIVIDING_LINE "<TR valign=\"CENTER\" line-height=\"1\" BGCOLOR=\"%s\"><TH colspan=\"4\" align=\"CENTER\"><hr noshade color=\"%s\" width=\"100%%\"></TD></TR>\n"                
#define DIVIDER_PRINT(color) printf(DIVIDING_LINE,COLOR_BG_DEFAULT,(color))
                DIVIDER_PRINT(COLOR_DARKGREEN);
                }

            printf("<TR BGCOLOR=\"%s\" id=\"tr_%s\" valign=\"CENTER\">\n<TD>",colors[colorIx],id);
            if(isMatrix) // TODO Remove this check when show/hide subtracks by js no longer restricted to cfgByMatrix
                cgiMakeCheckBoxIdAndJS(option,alreadySet,id,"onclick=\"hideOrShowSubtrack(this);\"");
            else
                cgiMakeCheckBoxWithId(option,alreadySet,id);
            printf ("</TD><TD nowrap=\"true\">");
            for(;indent>0;indent--) 
                puts ("&nbsp;&nbsp;&nbsp;"); 
            printf ("%s&nbsp;</TD><TD nowrap=\"true\">&nbsp;%s", subtrack->shortLabel, subtrack->longLabel);
            puts("</TD><TD nowrap=\"true\">&nbsp;");
#define SCHEMA_LINK "<A HREF=\"../cgi-bin/hgTables?db=%s&hgta_group=%s&hgta_track=%s&hgta_table=%s&hgta_doSchema=describe+table+schema\" TARGET=_BLANK> View table schema</A>\n"
            printf(SCHEMA_LINK, hGetDb(), parentTdb->grp, parentTdb->tableName, subtrack->tableName);
            puts("&nbsp;</TD></TR>");
            }
        subgroupMembershipFree(sCnt,&subgroups,&membership);
        } 
    }
if(!primarySubtrack && isMatrix && parentTdb->subtracks != NULL && dCnt>0)
    DIVIDER_PRINT(COLOR_DARKGREEN);
puts("</TABLE>");
puts("<P>");
subgroupMembersFree(&tagX,&titleX,NULL,NULL);
subgroupMembersFree(&tagY,&titleY,NULL,NULL);
dividersFree(&dividers);
hierarchyFree(hCnt,&hLabels,&hIndents);  
}

static void compositeUiAllSubtracks(struct cart *cart, struct trackDb *tdb,
				    char *primarySubtrack)
/* Show checkboxes for all subtracks, not just selected ones. */
{
compositeUiSubtracks(cart, tdb, FALSE, primarySubtrack);
}

static void compositeUiSelectedSubtracks(struct cart *cart, struct trackDb *tdb,
					 char *primarySubtrack)
/* Show checkboxes only for selected subtracks. */
{
compositeUiSubtracks(cart, tdb, TRUE, primarySubtrack);
}

#define MAX_SUBGROUP 9
#define ADD_BUTTON_LABEL        "add" 
#define CLEAR_BUTTON_LABEL      "clear" 
#define JBUFSIZE 2048

static void makeAddClearSubmitTweak(char javascript[JBUFSIZE], char *formName,
				    char *buttonVar, char *label)
/* safef into javascript a sequence of commands that will force a refresh 
 * of this same form, updating the values of whatever variables are necessary 
 * to say what we want to do. */
{
safef(javascript, JBUFSIZE*sizeof(char), 
      "document.%s.action = '%s'; document.%s.%s.value='%s'; "
      "document.%s.submit();",
      formName, cgiScriptName(), formName, buttonVar, label,
      formName);
}

static void makeRadioSubmitTweak(char javascript[JBUFSIZE], char *formName)
/* safef into javascript a sequence of commands that will force a refresh 
 * of this same form, updating the values of whatever variables are necessary 
 * to say what we want to do. */
{
// TODO Remove this subroutine when show/hide subtracks by js no longer restricted to cfgByMatrix
safef(javascript, JBUFSIZE*sizeof(char), 
      "onclick=\"document.%s.action = '%s'; "
      "document.%s.submit();\"",
      formName, cgiScriptName(), formName);
}

#define MANY_SUBTRACKS  8

#define WIGGLE_HELP_PAGE  "../goldenPath/help/hgWiggleTrackHelp.html"
static void wigCfgUi(struct cart *cart, struct trackDb *parentTdb,char *name)
/* UI for the wiggle track */
{
char *typeLine = NULL;  /*  to parse the trackDb type line  */
char *words[8];     /*  to parse the trackDb type line  */
int wordCount = 0;  /*  to parse the trackDb type line  */
char options[14][256];  /*  our option strings here */
double minY;        /*  from trackDb or cart    */
double maxY;        /*  from trackDb or cart    */
double tDbMinY;     /*  data range limits from trackDb type line */
double tDbMaxY;     /*  data range limits from trackDb type line */
int defaultHeight;  /*  pixels per item */
char *horizontalGrid = NULL;    /*  Grid lines, off by default */
char *lineBar;  /*  Line or Bar graph */
char *autoScale;    /*  Auto scaling on or off */
char *windowingFunction;    /*  Maximum, Mean, or Minimum */
char *smoothingWindow;  /*  OFF or [2:16] */
char *yLineMarkOnOff;   /*  user defined Y marker line to draw */
double yLineMark;       /*  from trackDb or cart    */
int maxHeightPixels = atoi(DEFAULT_HEIGHT_PER);
int minHeightPixels = MIN_HEIGHT_PER;
boolean bedGraph = FALSE;   /*  working on bedGraph type ? */

typeLine = cloneString(parentTdb->type);
wordCount = chopLine(typeLine,words);

if (sameString(words[0],"bedGraph"))
    bedGraph = TRUE;

snprintf( &options[0][0], 256, "%s.%s", name, HEIGHTPER );
snprintf( &options[4][0], 256, "%s.%s", name, MIN_Y );
snprintf( &options[5][0], 256, "%s.%s", name, MAX_Y );
snprintf( &options[7][0], 256, "%s.%s", name, HORIZGRID );
snprintf( &options[8][0], 256, "%s.%s", name, LINEBAR );
snprintf( &options[9][0], 256, "%s.%s", name, AUTOSCALE );
snprintf( &options[10][0], 256, "%s.%s", name, WINDOWINGFUNCTION );
snprintf( &options[11][0], 256, "%s.%s", name, SMOOTHINGWINDOW );
snprintf( &options[12][0], 256, "%s.%s", name, YLINEMARK );
snprintf( &options[13][0], 256, "%s.%s", name, YLINEONOFF );

wigFetchMinMaxPixelsWithCart(cart,parentTdb,name,&minHeightPixels, &maxHeightPixels, &defaultHeight);
if (bedGraph)
    wigFetchMinMaxLimitsWithCart(cart,parentTdb,name, &minY, &maxY, &tDbMinY, &tDbMaxY);
else
    wigFetchMinMaxYWithCart(cart,parentTdb,name, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
(void) wigFetchHorizontalGridWithCart(cart,parentTdb,name, &horizontalGrid);
(void) wigFetchAutoScaleWithCart(cart,parentTdb,name, &autoScale);
(void) wigFetchGraphTypeWithCart(cart,parentTdb,name, &lineBar);
(void) wigFetchWindowingFunctionWithCart(cart,parentTdb,name, &windowingFunction);
(void) wigFetchSmoothingWindowWithCart(cart,parentTdb,name, &smoothingWindow);
(void) wigFetchYLineMarkWithCart(cart,parentTdb,name, &yLineMarkOnOff);
wigFetchYLineMarkValueWithCart(cart,parentTdb,name, &yLineMark);

puts("<TABLE BORDER=0><TR><TD ALIGN=LEFT>");

if (bedGraph)
    printf("<b>Type of graph:&nbsp;</b>");
else
    printf("<b>Type of graph:&nbsp;</b>");
wiggleGraphDropDown(&options[8][0], lineBar);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

puts("<b>y&nbsp;=&nbsp;0.0 line:&nbsp;</b>");
wiggleGridDropDown(&options[7][0], horizontalGrid);
puts(" </TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Track height:&nbsp;</b>");
cgiMakeIntVar(&options[0][0], defaultHeight, 5);
printf("&nbsp;pixels&nbsp;&nbsp;(range: %d to %d)",
    minHeightPixels, maxHeightPixels);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Vertical viewing range</b>:&nbsp;&nbsp;\n<b>min:&nbsp;</b>");
cgiMakeDoubleVar(&options[4][0], minY, 6);
printf("&nbsp;&nbsp;&nbsp;<b>max:&nbsp;</b>");
cgiMakeDoubleVar(&options[5][0], maxY, 6);
printf("&nbsp;(range: %g to %g)",
    tDbMinY, tDbMaxY);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Data view scaling:&nbsp;</b>");
wiggleScaleDropDown(&options[9][0], autoScale);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Windowing function:&nbsp;</b>");
wiggleWindowingDropDown(&options[10][0], windowingFunction);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Smoothing window:&nbsp;</b>");
wiggleSmoothingDropDown(&options[11][0], smoothingWindow);
puts("&nbsp;pixels</TD></TR><TR><TD ALIGN=LEFT COLSPAN=1>");

puts("<b>Draw indicator line at y&nbsp;=&nbsp;</b>&nbsp;");
cgiMakeDoubleVar(&options[12][0], yLineMark, 6);
printf("&nbsp;&nbsp;");
wiggleYLineMarkDropDown(&options[13][0], yLineMarkOnOff);
printf("</TD><TD align=\"RIGHT\"><A HREF=\"%s\" TARGET=_blank>Graph configuration help</A>",
    WIGGLE_HELP_PAGE);
puts("</TD></TR></TABLE>");

freeMem(typeLine);
}

void scoreCfgUi(struct cart *cart, struct trackDb *parentTdb, int maxScore, char *name)
/* Put up UI for filtering bed track based on a score */
{
char option[256];

char *scoreValString = trackDbSetting(parentTdb, "scoreFilter");
int scoreSetting;
int scoreVal = 0;
char *words[2];

/* filter top-scoring N items in track */
char *scoreCtString = trackDbSetting(parentTdb, "filterTopScorers");
char *scoreFilterCt = NULL;
bool doScoreCtFilter = FALSE;

/* initial value of score theshold is 0, unless
 * overridden by the scoreFilter setting in the track */
if (scoreValString != NULL)
    scoreVal = atoi(scoreValString);
printf("<p><b>Show only items with score at or above:</b> ");
snprintf(option, sizeof(option), "%s.scoreFilter", name);
scoreSetting = cartUsualInt(cart,  option,  scoreVal);
cgiMakeIntVar(option, scoreSetting, 11);
printf("&nbsp;&nbsp;(range: 0&nbsp;to&nbsp;%d)", maxScore);

if (scoreCtString != NULL)
    {
    /* show only top-scoring items. This option only displayed if trackDb
     * setting exists.  Format:  filterTopScorers <on|off> <count> <table> */
    chopLine(cloneString(scoreCtString), words);
    safef(option, sizeof(option), "%s.filterTopScorersOn", name);
    doScoreCtFilter =
        cartCgiUsualBoolean(cart, option, sameString(words[0], "on"));
    puts("<P>");
    cgiMakeCheckBox(option, cartCgiUsualBoolean(cart, option, doScoreCtFilter));
    safef(option, sizeof(option), "%s.filterTopScorersCt", name);
    scoreFilterCt = cartCgiUsualString(cart, option, words[1]);

    puts("&nbsp; <B> Show only items in top-scoring </B>");
    cgiMakeTextVar(option, scoreFilterCt, 5);
    /* Only check size of table if track does not have subtracks */
    if (!trackDbIsComposite(parentTdb))
        printf("&nbsp; (range: 1 to 100000, total items: %d)",
                getTableSize(parentTdb->tableName));
    }
}

static boolean hCompositeDisplayViewDropDowns(struct cart *cart, struct trackDb *parentTdb)
/* UI for composite view drop down selections. */
{
int ix;
struct trackDb *subtrack;
char objName[64];
char javascript[JBUFSIZE];
#define VIEW_CFG_TITLELINK
#ifdef VIEW_CFG_TITLELINK
    #define CFG_LINK  "<B><A NAME=\"a_cfg_%s\"></A><A HREF=\"#a_cfg_%s\" onclick=\"return (showConfigControls('%s') == false);\" title=\"Configure View Settings\">%s</A></B>\n"
#else//ifndef VIEW_CFG_TITLELINK
    #define CFG_LINK  "<A NAME=\"a_cfg_%s\"></A><A HREF=\"#a_cfg_%s\" onclick=\"return (showConfigControls('%s') == false);\">Configure %s</A>\n"
#endif//ndef VIEW_CFG_TITLELINK
#define MAKE_CFG_LINK(name,title) printf(CFG_LINK, (name),(name),(name),(title))

char *tagV,*titleV;
char **nameV,**valueV;
int membersOfV = subgroupMembersGet(parentTdb,"view",&tagV,&titleV,&nameV,&valueV);
if(membersOfV <= 0)
    return FALSE;
    
// Should create an aray of subTrackDbs to build appropriate controls
struct trackDb **subtrkCfg = needMem(sizeof(struct trackDb *)*membersOfV);
int cntOfCfgs;
for (subtrack = parentTdb->subtracks,cntOfCfgs=0; subtrack != NULL; subtrack = subtrack->next)
    {
    char *stView;
    if(subgroupFind(subtrack,"view",&stView))
        {
        // identify view, set ptr for appropriate view, when all are set, break;
        for (ix = 0; ix < membersOfV; ix++)
            {
            if(!differentString(stView,nameV[ix]) && subtrkCfg[ix] == NULL)
                {
                    subtrkCfg[ix] = subtrack;
                    cntOfCfgs++;
                    break;
                }
            }
        }
        if(cntOfCfgs == membersOfV)
            break; 
    }
// Now check for configurable types    
for (ix = 0; ix < membersOfV; ix++)
    {
    if(subtrkCfg[ix] != NULL)
        {
        if(differentString(subtrkCfg[ix]->type,"wig") && differentString(subtrkCfg[ix]->type,"bed 5 +"))
            {
            subtrkCfg[ix] = NULL;
            cntOfCfgs--;
            }
        }
    }

printf("<EM><B>Select %s:</EM></B><BR>\n", titleV);
puts("<TABLE><TR align=\"LEFT\">");
for (ix = 0; ix < membersOfV; ix++)
    {
#ifdef VIEW_CFG_TITLELINK
    printf("<TD>");
    if(subtrkCfg[ix] != NULL)
        MAKE_CFG_LINK(nameV[ix],valueV[ix]);
    else
        printf("<B>%s</B>\n",valueV[ix]);
#else//ifndef VIEW_CFG_TITLELINK
    printf("<B>%s</B>\n",valueV[ix]);
#endif//ndef VIEW_CFG_TITLELINK
    puts("</TD>");
    
    safef(objName, sizeof(objName), "%s_dd_%s", parentTdb->tableName,nameV[ix]);
    enum trackVisibility tv = 
        hTvFromString(cartUsualString(cart, objName,hStringFromTv(parentTdb->visibility)));

    safef(javascript, sizeof(javascript), "onchange=\"matSelectViewForSubTracks(this,'_%s');\"", nameV[ix]);

    printf("<TD>");
    hTvDropDownWithJavascript(objName, tv, parentTdb->canPack,javascript);
    puts(" &nbsp; &nbsp; &nbsp;</TD>");
    }
puts("</TR>");
if(cntOfCfgs>0)
    {
#ifndef VIEW_CFG_TITLELINK
    printf("<TR>");
    for (ix = 0; ix < membersOfV; ix++)
        {
            if(subtrkCfg[ix] != NULL)
                {
                printf("<TD colspan=\"2\"><i>");
                MAKE_CFG_LINK(nameV[ix],valueV[ix]);
                puts("</i></TD>");
                }
            else
                printf("<TD colspan=\"2\">&nbsp;</TD>\n");
        }
    puts("</TR>");
#endif//ndef VIEW_CFG_TITLELINK
    puts("</TABLE><TABLE>");
    for (ix = 0; ix < membersOfV; ix++)
        {
            if(subtrkCfg[ix] != NULL)
                {
                printf("<TR id=\"tr_cfg_%s\" style=\"display:none\"><TD>&nbsp;&nbsp;&nbsp;&nbsp;</TD><TD>",nameV[ix]);
                printf("<TABLE CELLSPACING=\"3\" CELLPADDING=\"0\" border=\"4\" bgcolor=\"%s\" borderColor=\"%s\"><TR><TD>\n",
                       COLOR_DARKBLUE,COLOR_DARKBLUE);
                printf("<TABLE border=\"0\" bgcolor=\"%s\" borderColor=\"%s\"><TR><TD>",COLOR_BG_ALTDEFAULT,COLOR_BG_ALTDEFAULT);
                printf("<CENTER><B>%s Configuration</B></CENTER>\n",valueV[ix]);
                safef(objName, sizeof(objName), "%s_%s", parentTdb->tableName,nameV[ix]);
                if(!differentString(subtrkCfg[ix]->type,"wig"))
                    wigCfgUi(cart,subtrkCfg[ix],objName);
                else if(!differentString(subtrkCfg[ix]->type,"bed 5 +"))
                    scoreCfgUi(cart,parentTdb, 1000,objName);
                puts("</td></tr></table></td></tr></table>");
                puts("</TD></TR>");
                }
        }
    }
puts("</TABLE><BR>");
subgroupMembersFree(&tagV,&titleV,&nameV,&valueV);
return TRUE;
}

static boolean hCompositeUiByMatrix(struct cart *cart, struct trackDb *parentTdb, char *formName)
/* UI for composite tracks: matrix of checkboxes. */
{
//int ix;
char objName[64];
char javascript[JBUFSIZE];
char option[64];
boolean alreadySet = TRUE;
struct trackDb *subtrack;
char *tagX,*titleX;
char **nameX,**valueX;
char *tagY,*titleY;
char **nameY,**valueY;
int membersOfX=0,membersOfY=0,ixX,ixY,subTrkCount;
    
if(!dimensionsExist(parentTdb))
    return FALSE;

membersOfX = subgroupMembersGetByDimension(parentTdb,'X',&tagX,&titleX,&nameX,&valueX);
membersOfY = subgroupMembersGetByDimension(parentTdb,'Y',&tagY,&titleY,&nameY,&valueY);
if(membersOfX == 0 && membersOfY == 0) // Must be an X or Y dimension
    return FALSE;
    
// use array of char determine all the cells (in X,Y dimensions) that are actually populated
char *value;
char cells[membersOfX+1][membersOfY+1]; // If membersOfX or Y==0 then there needs to be atleast one element in dimension
memset(cells, 0, sizeof(cells));
for (subtrack = parentTdb->subtracks,subTrkCount=0; subtrack != NULL; subtrack = subtrack->next,subTrkCount++)
    {
    ixX= (membersOfX == 0?0:-1);
    ixY= (membersOfY == 0?0:-1);
    if(membersOfX > 0 && subgroupFind(subtrack,tagX,&value))
        {
        ixX = stringArrayIx(value,nameX,membersOfX);
        subgroupFree(&value);
        }
    if(membersOfY > 0 && subgroupFind(subtrack,tagY,&value))
        {
        ixY = stringArrayIx(value,nameY,membersOfY);
        subgroupFree(&value);
        }
    if(ixX > -1 && ixY > -1)
        cells[ixX][ixY]++;
    }

hCompositeDisplayViewDropDowns(cart,parentTdb);  // If there is a view dimension, it is at top
    
    // Regardless of whethere there is a dimension X or Y, there will be 'all' buttons
    puts("<B>Select subtracks:</B><BR>");
    printf("<TABLE CELLSPACING=\"3\" CELLPADDING=\"0\" border=\"4\" bgcolor=\"%s\" borderColor=\"%s\"><TR><TD>\n",COLOR_DARKGREEN,COLOR_DARKGREEN);
    printf("<TABLE border=\"0\" bgcolor=\"%s\" borderColor=\"%s\">\n",COLOR_BG_DEFAULT,COLOR_BG_DEFAULT);

    printf("<TR ALIGN=CENTER BGCOLOR=\"%s\">\n",COLOR_BG_ALTDEFAULT);
#define PM_BUTTON  "<A NAME=\"%s\"></A><A HREF=\"#%s\"><IMG height=18 width=18 onclick=\"return (setCheckBoxesThatContain('name',%s,true,'%s','%s') == false);\" id=\"btn_%s\" src=\"../images/%s\" alt=\"%s\"></A>\n"
#define  PLUS_BUTTON(anc,str1,str2) printf(PM_BUTTON, (anc),(anc),"true",(str1),(str2),(anc),"add_sm.gif","+")
#define MINUS_BUTTON(anc,str1,str2) printf(PM_BUTTON, (anc),(anc),"false",(str1),(str2),(anc),"remove_sm.gif","-")
    if(membersOfX > 0 && membersOfY > 0)
        {
        printf("<TH ALIGN=LEFT WIDTH=\"100\">All&nbsp;&nbsp;");
        PLUS_BUTTON(  "plus_all","mat_","_cb");
        MINUS_BUTTON("minus_all","mat_","_cb");
        puts("</TH>");
        }
    else if(membersOfX > 0)
        printf("<TH WIDTH=\"100\"><EM><B>%s</EM></B></TH>", titleX);
    else //if(membersOfY > 0)
        printf("<TH ALIGN=RIGHT WIDTH=\"100\"><EM><B>%s</EM></B></TH>", titleY);
    
    // If there is an X dimension, then titles go across the top
    if(membersOfX > 0)
        {
        if(membersOfY > 0)
            printf("<TH WIDTH=\"100\"><EM><B>%s</EM></B></TH>", titleX);
        for (ixX = 0; ixX < membersOfX; ixX++)
            printf("<TH WIDTH=\"100\">%s</TH>",valueX[ixX]);
        }
    else
        {
        printf("<TH ALIGN=CENTER WIDTH=\"100\">");
        PLUS_BUTTON(  "plus_all","mat_","_cb");
        MINUS_BUTTON("minus_all","mat_","_cb");
        puts("</TH>");
        }
    puts("</TR>\n");
    
    // If there are both X and Y dimensions, then there is a row of buttons in X
    if(membersOfX > 0 && membersOfY > 0)
        {
        printf("<TR ALIGN=CENTER BGCOLOR=\"#FFF9D2\"><TH ALIGN=RIGHT><EM><B>%s</EM></B></TH><TD>&nbsp;</TD>", titleY);
        for (ixX = 0; ixX < membersOfX; ixX++)    // Special row of +- +- +-
            {
            puts("<TD>");
            safef(option, sizeof(option), "mat_%s_", nameX[ixX]);
            safef(objName, sizeof(objName), "plus_%s_all", nameX[ixX]);
            PLUS_BUTTON(  objName,option,"_cb");
            safef(objName, sizeof(objName), "minus_%s_all", nameX[ixX]);
            MINUS_BUTTON(objName,option,"_cb");
            puts("</TD>");    
            }
        puts("</TR>\n");
        }
    
    // Now the Y by X matrix
    for (ixY = 0; ixY <= membersOfY; ixY++)
        {
        printf("<TR ALIGN=CENTER BGCOLOR=\"#FFF9D2\">");
        if(membersOfY == 0) // 'All' buttons go here if no Y dimension
            {
            printf("<TH ALIGN=CENTER WIDTH=\"100\">");
            PLUS_BUTTON(  "plus_all","mat_","_cb");
            MINUS_BUTTON("minus_all","mat_","_cb");
            puts("</TH>");
            }
        else if(ixY < membersOfY)
            printf("<TH ALIGN=RIGHT>%s</TH>\n",valueY[ixY]);
        else
            break;
        
        if(membersOfX > 0 && membersOfY > 0) // Both X and Y, then column of buttons
            {
            puts("<TD>");    
            safef(option, sizeof(option), "_%s_cb", nameY[ixY]);    
            safef(objName, sizeof(objName), "plus_all_%s", nameY[ixY]);
            PLUS_BUTTON(  objName,"mat_",option);
            safef(objName, sizeof(objName), "minus_all_%s", nameY[ixY]);
            MINUS_BUTTON(objName,"mat_",option);
            puts("</TD>");
            }
        for (ixX = 0; ixX <= membersOfX; ixX++)
            {
            if(membersOfX > 0 && ixX == membersOfX)
                break; 
            if(cells[ixX][ixY] > 0)
                {
                if(membersOfX > 0 && membersOfY > 0)
                    {
                    safef(objName, sizeof(objName), "mat_%s_%s_cb", nameX[ixX],nameY[ixY]);
                    safef(javascript, sizeof(javascript), "onclick=\"matSetCheckBoxesThatContain('id',this.checked,true,'cb_%s_%s_');\"", 
                          nameX[ixX],nameY[ixY]);
                    }
                else
                    {
                    safef(objName, sizeof(objName), "mat_%s_cb", (membersOfX > 0 ? nameX[ixX] : nameY[ixY]));
                    safef(javascript, sizeof(javascript), "onclick=\"matSetCheckBoxesThatContain('id',this.checked,true,'cb_%s_');\"", 
                          (membersOfX > 0 ? nameX[ixX] : nameY[ixY]));
                    }
                alreadySet = cartUsualBoolean(cart, objName, alreadySet);
                puts("<TD>");
                cgiMakeCheckBoxJS(objName,alreadySet,javascript);
                puts("</TD>");
                }
            else
                puts("<TD>&nbsp;</TD>");
            }
        puts("</TR>\n");
        }
    puts("</TABLE>");
    puts("</TD></TR></TABLE>");
    subgroupMembersFree(&tagX,&titleX,&nameX,&valueX);
    subgroupMembersFree(&tagY,&titleY,&nameY,&valueY);
    puts("<BR>\n");
    return TRUE;
}

static boolean hCompositeUiNoMatrix(struct cart *cart, struct trackDb *parentTdb,
          char *primarySubtrack, char *formName)
/* UI for composite tracks: subtrack selection.  This is the default UI 
without matrix controls. */
{
int i, j, k;
char *words[64];
char option[64];
int wordCnt;
char javascript[JBUFSIZE];
char *primaryType = getPrimaryType(primarySubtrack, parentTdb);
char *name, *value;
char buttonVar[32];
char setting[] = "subGroupN";
char *button;
struct trackDb *subtrack;
bool hasSubgroups = (trackDbSetting(parentTdb, "subGroup1") != NULL);

if(dimensionsExist(parentTdb))
    return FALSE;

puts ("<TABLE>");
if (hasSubgroups)
    {
    puts("<TR><B>Select subtracks:</B></TR>");
    puts("<TR><TD><EM><B>&nbsp; &nbsp; All</B></EM>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; </TD><TD>");
    }
else
    {
    puts("<TR><TD><B>All subtracks:</B></TD><TD>");
    }
safef(buttonVar, sizeof buttonVar, "%s", "button_all");
if (formName)
    {
    cgiMakeHiddenVar(buttonVar, "");
    makeAddClearSubmitTweak(javascript, formName, buttonVar,
                ADD_BUTTON_LABEL);
    cgiMakeOnClickButton(javascript, ADD_BUTTON_LABEL);
    puts("</TD><TD>");
    makeAddClearSubmitTweak(javascript, formName, buttonVar,
                CLEAR_BUTTON_LABEL);
    cgiMakeOnClickButton(javascript, CLEAR_BUTTON_LABEL);
    }
else
    {
    cgiMakeButton(buttonVar, ADD_BUTTON_LABEL);
    puts("</TD><TD>");
    cgiMakeButton(buttonVar, CLEAR_BUTTON_LABEL);
    }
button = cgiOptionalString(buttonVar);
if (isNotEmpty(button))
    {
    for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
        {
        boolean newVal = FALSE;
        safef(option, sizeof(option), "%s_sel", subtrack->tableName);
        newVal = sameString(button, ADD_BUTTON_LABEL);
        if (primarySubtrack)
            {
            if (sameString(subtrack->tableName, primarySubtrack))
                newVal = TRUE;
            if (sameString(subtrack->type, primaryType))
                cartSetBoolean(cart, option, newVal);
            }
        else
            cartSetBoolean(cart, option, newVal);
        }
    }
puts("</TD></TR>");
puts ("</TABLE>");
/* generate set & clear buttons for subgroups */
for (i = 0; i < MAX_SUBGROUP; i++)
    {
    char *subGroup;
    safef(setting, sizeof setting, "subGroup%d", i+1);
    if (trackDbSetting(parentTdb, setting) == NULL)
        break;
    wordCnt = chopLine(cloneString(trackDbSetting(parentTdb, setting)), words);
    if (wordCnt < 2)
        continue;
    subGroup = cloneString(words[0]);
    puts ("<TABLE>");
    printf("<TR><TD><EM><B>&nbsp; &nbsp; %s</EM></B></TD></TR>", words[1]);
    for (j = 2; j < wordCnt; j++)
        {
        if (!parseAssignment(words[j], &name, &value))
            continue;
        printf("<TR><TD>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; %s</TD><TD>",
           value);
        safef(buttonVar, sizeof buttonVar, "%s_%s", subGroup, name);
        if (formName)
            {
            cgiMakeHiddenVar(buttonVar, "");
            makeAddClearSubmitTweak(javascript, formName, buttonVar,
                        ADD_BUTTON_LABEL);
            cgiMakeOnClickButton(javascript, ADD_BUTTON_LABEL);
            puts("</TD><TD>");
            makeAddClearSubmitTweak(javascript, formName, buttonVar,
                        CLEAR_BUTTON_LABEL);
            cgiMakeOnClickButton(javascript, CLEAR_BUTTON_LABEL);
            }
        else
            {
            cgiMakeButton(buttonVar, ADD_BUTTON_LABEL);
            puts("</TD><TD>");
            cgiMakeButton(buttonVar, CLEAR_BUTTON_LABEL);
            }
        puts("</TD></TR>");
        button = cgiOptionalString(buttonVar);
        if (isEmpty(button))
            continue;
        for (subtrack = parentTdb->subtracks; subtrack != NULL; 
                subtrack = subtrack->next)
            {
            char *p;
            int n;
            if ((p = trackDbSetting(subtrack, "subGroups")) == NULL)
                continue;
            n = chopLine(cloneString(p), words);
            for (k = 0; k < n; k++)
                {
                char *subName, *subValue;
                if (!parseAssignment(words[k], &subName, &subValue))
                    continue;
                if (sameString(subName, subGroup) && sameString(subValue, name))
                    {
                    boolean newVal = FALSE;
                            safef(option, sizeof(option), 
                                    "%s_sel", subtrack->tableName);
                    newVal = sameString(button, ADD_BUTTON_LABEL);
                    if (primarySubtrack)
                        {
                        if (sameString(subtrack->tableName, primarySubtrack))
                            newVal = TRUE;
                        if (sameString(subtrack->type, primaryType))
                            cartSetBoolean(cart, option, newVal);
                        }
                    else
                        cartSetBoolean(cart, option, newVal);
                    }
                }
            }
        }
    puts ("</TABLE>");
    }
    return TRUE;
}

void hCompositeUi(struct cart *cart, struct trackDb *tdb,
		  char *primarySubtrack, char *fakeSubmit, char *formName)
/* UI for composite tracks: subtrack selection.  If primarySubtrack is
 * non-NULL, don't allow it to be cleared and only offer subtracks
 * that have the same type.  If fakeSubmit is non-NULL, add a hidden
 * var with that name so it looks like it was pressed. */
{
int i;
char javascript[JBUFSIZE];
int nGroups;
bool hasSubgroups = (trackDbSetting(tdb, "subGroup1") != NULL);
boolean displayAll = 
    sameString(cartUsualString(cart, "displaySubtracks", "all"), "all");
boolean isMatrix = dimensionsExist(tdb);


puts("<P>");
if (slCount(tdb->subtracks) < MANY_SUBTRACKS && !hasSubgroups)
    {
    compositeUiAllSubtracks(cart, tdb, primarySubtrack);
    return;
    }
if (fakeSubmit)
    cgiMakeHiddenVar(fakeSubmit, "submit");
    
jsIncludeFile("utils.js",NULL);
jsIncludeFile("hui.js",NULL);

if (!hasSubgroups || !isMatrix || primarySubtrack)
    hCompositeUiNoMatrix(cart,tdb,primarySubtrack,formName);
else 
    hCompositeUiByMatrix(cart,tdb,formName);

if(primarySubtrack == NULL)
    {
    puts("<TABLE><TR><TD><B>Show checkboxes for:</B></TD><TD>");
    if (formName)
        {
        if(isMatrix) // TODO Remove this check when show/hide subtracks by js no longer restricted to cfgByMatrix
            safef(javascript, sizeof(javascript), "onclick=\"showSubTrackCheckBoxes(true);\"");
        else
        makeRadioSubmitTweak(javascript, formName);
        
        cgiMakeOnClickRadioButton("displaySubtracks", "selected", !displayAll,
                                    javascript);
        puts("Only selected subtracks</TD><TD>");
        if(isMatrix) // TODO Remove this check when show/hide subtracks by js no longer restricted to cfgByMatrix
            safef(javascript, sizeof(javascript), "onclick=\"showSubTrackCheckBoxes(false);\"");
        cgiMakeOnClickRadioButton("displaySubtracks", "all", displayAll,
                    javascript);
        }
    else
        {
        cgiMakeRadioButton("displaySubtracks", "selected", !displayAll);
        puts("Only selected subtracks</TD><TD>");
        cgiMakeRadioButton("displaySubtracks", "all", displayAll);
        }
    puts("All subtracks</TD>");
    puts("</TR></TABLE>");
}
cartSaveSession(cart);
cgiContinueHiddenVar("g");
nGroups = i;
if (displayAll)
    compositeUiAllSubtracks(cart, tdb, primarySubtrack);
else
    compositeUiSelectedSubtracks(cart, tdb, primarySubtrack);
}

boolean superTrackDropDown(struct cart *cart, struct trackDb *tdb,
                                int visibleChild)
/* Displays hide/show dropdown for supertrack.  
 * Set visibleChild to indicate whether 'show' should be grayed
 * out to indicate that no supertrack members are visible:
 *    0 to gray out (no visible children)
 *    1 don't gray out (there are visible children)
 *   -1 don't know (this function should determine)
 * If -1,i the subtracks field must be populated with the child trackDbs.
 * Returns false if not a supertrack */
{
if (!tdb->isSuper)
    return FALSE;

/* determine if supertrack is show/hide */
boolean show = FALSE;
char *setting = 
        cartUsualString(cart, tdb->tableName, tdb->isShow ? "show" : "hide");
if (sameString("show", setting))
    show = TRUE;

/* Determine if any tracks in supertrack are visible; if not,
 * the 'show' is grayed out */
if (show && (visibleChild == -1))
    {
    visibleChild = 0;
    struct trackDb *cTdb;
    for (cTdb = tdb->subtracks; cTdb != NULL; cTdb = tdb->next)
        {
        cTdb->visibility = 
                hTvFromString(cartUsualString(cart, cTdb->tableName,
                                      hStringFromTv(cTdb->visibility)));
        if (cTdb->visibility != tvHide)
            visibleChild = 1;
        }
    }
hideShowDropDown(tdb->tableName, show, (show && visibleChild) ? 
                            "normalText" : "hiddenText");
return TRUE;
}

int tvConvertToNumericOrder(enum trackVisibility v)
{
return ((v) == tvFull   ? 4 : \
        (v) == tvPack   ? 3 : \
        (v) == tvSquish ? 2 : \
        (v) == tvDense  ? 1 : 0);
}
int tvCompare(enum trackVisibility a, enum trackVisibility b)
/* enum trackVis isn't in numeric order by visibility, so compare
 * symbolically: */
{
return (tvConvertToNumericOrder(b) - tvConvertToNumericOrder(a)); 
}

enum trackVisibility tvMin(enum trackVisibility a, enum trackVisibility b)
/* Return the less visible of a and b. */
{
if (tvCompare(a, b) >= 0)
    return a;
else
    return b;
}

char *compositeViewControlNameFromTdb(struct trackDb *tdb)
/* Returns a string with the composite view control name if one exists */
{
int len;
char *stView;
char *name;
char *parentName = trackDbSetting(tdb, "subTrack");
if(parentName 
&& dimensionsSubtrackOf(tdb) // TODO: Remove when no longer restricted to dimensions
&& subgroupFind(tdb,"view",&stView))  
    {
    len = strlen(parentName) + 10;
    name = needMem(len);
    safef(name,len,"%s_%s",parentName,stView);
    subgroupFree(&stView);
    }
else
    {
    len = strlen(tdb->tableName) + 10;
    name = needMem(len);
    safef(name,len,"%s",tdb->tableName);
    }
return name;
}
void compositeViewControlNameFree(char **name)
/* frees a string allocated by compositeViewControlNameFromTdb */
{
if(name && *name)
    freez(name);
}

