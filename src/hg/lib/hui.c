/* hui - human genome user interface common controls. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "hCommon.h"

static char const rcsid[] = "$Id: hui.c,v 1.32 2004/02/02 19:52:25 hiram Exp $";

char *hUserCookie()
/* Return our cookie name. */
{
if (hIsMgcServer())
    return "mgcuid";
else    
    return "hguid";
}

char *wrapWhiteFont(char *s)
/* Write white font around s */
{
static char buf[256];
sprintf(buf, "<FONT COLOR=\"#FFFFFF\">%s</FONT>", s);
return buf;
}


/******  Some stuff for tables of controls ******/

struct controlGrid *startControlGrid(int columns, char *align)
/* Start up a control grid. */
{
struct controlGrid *cg;
AllocVar(cg);
cg->columns = columns;
cg->align = cloneString(align);
return cg;
}

void controlGridEndRow(struct controlGrid *cg)
/* Force end of row. */
{
printf("</tr>\n<tr>");
cg->columnIx = 0;
}

void controlGridStartCell(struct controlGrid *cg)
/* Start a new cell in control grid. */
{
if (cg->columnIx == cg->columns)
    controlGridEndRow(cg);
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
	    printf("<td>&nbsp</td>\n");
    printf("</tr>\n</table>\n");
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
return stringArrayIx(s, hTvStrings, ArraySize(hTvStrings));
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

void hTvDropDown(char *varName, enum trackVisibility vis, boolean canPack)
/* Make track visibility drop down for varName 
 * uses style "normalText" */
{
    hTvDropDownClass(varName, vis, canPack, "normalText");
}

void hTvDropDownClass(char *varName, enum trackVisibility vis, boolean canPack, char *class)
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
    cgiMakeDropListClass(varName, pack, ArraySize(pack), pack[packIx[vis]], class);
else
    cgiMakeDropListClass(varName, noPack, ArraySize(noPack), noPack[vis], class);
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
    "MGD Geneic Map",
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

/****** Some stuff for snp type related controls *******/

static char *snpTypeLabels[] = {
    "Single Nucleotide Polymorphisms",
    "Insertions and Deletions",
    "Segmental Duplications",
};

static char *snpTypes[] = {
    "snpSingle.type",
    "snpIndels.type",
    "snpSegmental.type",
};

static char *snpTypeDataName[] = {
    "SNP",
    "INDEL",
    "SEGMENTAL",
};

static char *snpTypeStates[] = {
    "include",
    "exclude",
    "exclude",
};

enum snpTypeEnum snpTypeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, snpTypes);
if (x < 0)
   errAbort("Unknown option '%s' in snpTypeStringToEnum", string);
return x;
}

char *snpTypeEnumToString(enum snpTypeEnum x)
/* Convert from enum to string representation. */
{
return snpTypes[x];
}

enum snpTypeEnum snpTypeLabelStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, snpTypeLabels);
if (x < 0)
   errAbort("Unknown option '%s' in snpTypeLabelStringToEnum", string);
return x;
}

char *snpTypeLabelEnumToString(enum snpTypeEnum x)
/* Convert from enum to string representation. */
{
return snpTypeLabels[x];
}

enum snpTypeEnum snpTypeStateStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, snpTypeStates);
if (x < 0)
   errAbort("Unknown option '%s' in snpTypeStateStringToEnum", string);
return x;
}

char *snpTypeStateEnumToString(enum snpTypeEnum x)
/* Convert from enum to string representation. */
{
return snpTypeStates[x];
}

enum snpTypeEnum snpTypeDataStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, snpTypeDataName);
if (x < 0)
   errAbort("Unknown option '%s' in snpTypeDataStringToEnum", string);
return x;
}

char *snpTypeDataEnumToString(enum snpTypeEnum x)
/* Convert from enum to string representation. */
{
return snpTypeDataName[x];
}

/****** Some stuff for snp source related controls *******/

static char *snpSourceLabels[] = {
    "BAC Overlaps",
    "Mixed",
    "Random",
    "Other",
    "Affymetrix Genotyping Array 10K",
    "Affymetrix Genotyping Array 120K",
};

static char *snpSourceStrings[] = {
    "snpBAC.source",
    "snpMIXED.source",
    "snpRANDOM.source",
    "snpOTHER.source",
    "snpAffy10K.source",
    "snpAffy120K.source",
};

static char *snpSourceDataName[] = {
    "BAC_OVERLAP",
    "MIXED",
    "RANDOM",
    "OTHER",
    "Affy10K",
    "Affy120K",
};

static char *snpSourceColor[] = {
    "red",
    "green",
    "blue",
    "black",
    "exclude",
    "exclude",
};

enum snpSourceEnum snpSourceStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, snpSourceStrings);
if (x < 0)
   errAbort("Unknown option '%s' in snpSourceStringToEnum", string);
return x;
}

char *snpSourceEnumToString(enum snpSourceEnum x)
/* Convert from enum to string representation. */
{
return snpSourceStrings[x];
}

enum snpSourceEnum snpSourceLabelStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, snpSourceLabels);
if (x < 0)
   errAbort("Unknown option '%s' in snpSourceLabelStringToEnum", string);
return x;
}

char *snpSourceLabelEnumToString(enum snpSourceEnum x)
/* Convert from enum to string representation. */
{
return snpSourceLabels[x];
}

enum snpSourceEnum snpSourceColorStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, snpSourceColor);
if (x < 0)
   errAbort("Unknown option '%s' in snpSourceColorStringToEnum", string);
return x;
}

char *snpSourceColorEnumToString(enum snpSourceEnum x)
/* Convert from enum to string representation. */
{
return snpSourceColor[x];
}

enum snpSourceEnum snpSourceDataStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, snpSourceDataName);
if (x < 0)
   errAbort("Unknown option '%s' in snpSourceDataStringToEnum", string);
return x;
}

char *snpSourceDataEnumToString(enum snpSourceEnum x)
/* Convert from enum to string representation. */
{
return snpSourceDataName[x];
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

/****** Some stuff for cdsColoring of mrna track related controls *******/

static char *cdsColorOptions[] = {
    "off",
    "genomic codons",
    "genomic bases",
    "mRNA codons",
    "mRNA bases",
    "different mRNA codons",
    "different mRNA bases"
    };

enum cdsColorOptEnum cdsColorStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, cdsColorOptions);
if (x < 0)
   errAbort("hui::cdsColorOptEnum() - Unknown option %s", string);
return x;
}

char *cdsColorEnumToString(enum cdsColorOptEnum x)
/* Convert from enum to string representation. */
{
return cdsColorOptions[x];
}

void cdsColorDropDown(char *var, char* curVal, int size)
/* Make drop down of options for coloring mRNAs and printing bases. */
{
if(size<0)
    cgiMakeDropList(var, cdsColorOptions, ArraySize(cdsColorOptions), curVal);
else
    cgiMakeDropList(var, cdsColorOptions, size, curVal);

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

/****** Options for the wiggle track Windowing *******/

static char *wiggleWindowingOptions[] = {
    "Maximum",
    "Mean",
    "Minimum"
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
    "OFF", "OFF", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
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

/****** Options for the wiggle track AutoScale *******/

static char *wiggleScaleOptions[] = {
    "Use Vertical Viewing Range Setting",
    "Auto-Scale to data view"
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
    "Points",
    "Bar"
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
    "Only samples",
    "Linear interpolation"
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
    "Only samples",
    "Linear interpolation"
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
    "Only samples",
    "Linear interpolation"
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
char buf[64];
AllocVar(fil);
fil->label = label;
sprintf(buf, "%s_%s", track, key);
fil->key = cloneString(buf);
fil->table = table;
slAddTail(&mud->filterList, fil);
}

struct mrnaUiData *newMrnaUiData(char *track, boolean isXeno)
/* Make a new  in extra-ui data structure for mRNA. */
{
struct mrnaUiData *mud;
char buf[64];
AllocVar(mud);
sprintf(buf, "%sFt", track);
mud->filterTypeVar = cloneString(buf);
sprintf(buf, "%sLt", track);
mud->logicTypeVar = cloneString(buf);
if (isXeno)
    addMrnaFilter(mud, track, "organism", "org", "organism");
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

