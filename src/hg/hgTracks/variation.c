/* variation.c - hgTracks routines that are specific to the tracks in
 * the variation group */

#include "variation.h"

static char const rcsid[] = "$Id: variation.c,v 1.110 2006/10/09 03:10:19 daryl Exp $";

void filterSnpMapItems(struct track *tg, boolean (*filter)
		       (struct track *tg, void *item))
/* Filter out items from track->itemList. */
{
filterSnpItems(tg, filter);
}

void filterSnpItems(struct track *tg, boolean (*filter)
		    (struct track *tg, void *item))
/* Filter out items from track->itemList. */
{
struct slList *newList = NULL, *el, *next;

for (el = tg->items; el != NULL; el = next)
    {
    next = el->next;
    if (filter(tg, el))
 	slAddHead(&newList, el);
    }
slReverse(&newList);
tg->items = newList;
}

boolean snpMapSourceFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snpMap *el = item;
int    snpMapSource = 0;

for (snpMapSource=0; snpMapSource<snpMapSourceCartSize; snpMapSource++)
    if (containsStringNoCase(el->source,snpMapSourceDataName[snpMapSource]))
 	if (sameString(snpMapSourceCart[snpMapSource], "exclude"))
 	    return FALSE;
return TRUE;
}

boolean snpMapTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snpMap *el = item;
int    snpMapType = 0;

for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    if (containsStringNoCase(el->type,snpMapTypeDataName[snpMapType]))
 	if (sameString(snpMapTypeCart[snpMapType], "exclude"))
 	    return FALSE;
return TRUE;
}

boolean snpAvHetFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;

if (el->avHet < atof(cartUsualString(cart, "snpAvHetCutoff", "0.0")))
    return FALSE;
return TRUE;
}

boolean snp125AvHetFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;

if (el->avHet < atof(cartUsualString(cart, "snp125AvHetCutoff", "0.0")))
    return FALSE;
return TRUE;
}

boolean snp125WeightFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;

if (el->weight > atoi(cartUsualString(cart, "snp125WeightCutoff", "3")))
    return FALSE;
return TRUE;
}

boolean snpSourceFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpSource = 0;

for (snpSource=0; snpSource<snpSourceCartSize; snpSource++)
    if (containsStringNoCase(el->source,snpSourceDataName[snpSource]))
 	if (sameString(snpSourceCart[snpSource], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snpMolTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpMolType = 0;

for (snpMolType=0; snpMolType<snpMolTypeCartSize; snpMolType++)
    if (containsStringNoCase(el->molType,snpMolTypeDataName[snpMolType]))
 	if ( sameString(snpMolTypeCart[snpMolType], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snp125MolTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;
int i;

for (i=0; i<snp125MolTypeLabelsSize; i++)
    {
    if (!sameString(snp125MolTypeDataName[i], el->molType)) 
	continue;
    return snp125MolTypeIncludeCart[i];
    }
return TRUE;
}

boolean snpClassFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpClass = 0;

for (snpClass=0; snpClass<snpClassCartSize; snpClass++)
    if (containsStringNoCase(el->class,snpClassDataName[snpClass]))
 	if ( sameString(snpClassCart[snpClass], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snp125ClassFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;
int i;

for (i=0; i<snp125ClassLabelsSize; i++)
    {
    if (!sameString(snp125ClassDataName[i], el->class)) 
	continue;
    return snp125ClassIncludeCart[i];
    }
return TRUE;
}

boolean snpValidFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpValid = 0;

for (snpValid=0; snpValid<snpValidCartSize; snpValid++)
    if (containsStringNoCase(el->valid,snpValidDataName[snpValid]))
 	if ( sameString(snpValidCart[snpValid], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snp125ValidFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;
int i;

for (i=0; i<snp125ValidLabelsSize; i++)
    {
    if (!containsStringNoCase(el->valid, snp125ValidDataName[i])) 
	continue;
    return snp125ValidIncludeCart[i];
    }
return TRUE;
}

boolean snpFuncFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpFunc = 0;

for (snpFunc=0; snpFunc<snpFuncCartSize; snpFunc++)
    if (containsStringNoCase(el->func,snpFuncDataName[snpFunc]))
 	if ( sameString(snpFuncCart[snpFunc], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snp125FuncFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;
int i;

for (i=0; i<snp125FuncLabelsSize; i++)
    {
    if (!containsStringNoCase(el->func, snp125FuncDataName[i])) 
	continue;
    return snp125FuncIncludeCart[i];
    }
return TRUE;
}

boolean snpLocTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpLocType = 0;

for (snpLocType=0; snpLocType<snpLocTypeCartSize; snpLocType++)
    if (containsStringNoCase(el->locType,snpLocTypeDataName[snpLocType]))
 	if ( sameString(snpLocTypeCart[snpLocType], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snp125LocTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;
int i;

for (i=0; i<snp125LocTypeLabelsSize; i++)
    {
    if (!sameString(snp125LocTypeDataName[i], el->locType)) 
	continue;
    return snp125LocTypeIncludeCart[i];
    }
return TRUE;
}

struct orthoBed *orthoBedLoad(char **row)
/* Load a bed from row fetched with select * from bed
 * from database.  Dispose of this with bedFree(). */
{
struct orthoBed *ret;
AllocVar(ret);
ret->chrom      = cloneString(row[0]);
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd   = sqlUnsigned(row[2]);
ret->name       = cloneString(row[3]);
ret->chimp      = cloneString(row[10]);
return ret;
}

int snpOrthoCmp(const void *va, const void *vb)
/* Compare for sort based on bed4 */
{
struct snp125Extended *a = *((struct snp125Extended **)va);
struct orthoBed       *b = *((struct orthoBed       **)vb);
int dif;

if(a==0||b==0)
    return 0;
dif = differentWord(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = a->chromEnd - b->chromEnd;
if (dif == 0)
    dif = differentWord(a->name, b->name);
return dif;
}

void setSnp125ExtendedNameExtra(struct track *tg)
/* add extra text to be drawn in snp name field.  This works by
   walking through two sorted lists and updating the nameExtra value
   for the SNP list with data from a table of orthologous state
   information */
{
struct sqlConnection *conn          = hAllocConn();
int                   rowOffset     = 0;
char                **row           = NULL;      
struct slList        *snpItemList   = tg->items; /* list of SNPs */
struct slList        *snpItem       = snpItemList;
struct slList        *orthoItemList = NULL;      /* list of orthologous state info */
struct slList        *orthoItem     = orthoItemList;
char                 *orthoTable    = cloneString("snp126orthoPanTro2RheMac2"); /* could be a trackDb option */
struct sqlResult     *sr            = NULL;
int                   cmp           = 0;
struct dyString      *extra         = newDyString(256);

/* if orthologous info is not available, don't add it! */
if(!sqlTableExists(conn,orthoTable))
    {
    hFreeConn(&conn);
    return;
    }
/* get list of orthologous alleles */
sr = hRangeQuery(conn, orthoTable, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    orthoItem = (struct slList *)orthoBedLoad(row + rowOffset);
    slAddHead(&orthoItemList, orthoItem);
    }

/* List of SNPs is already sorted, so sort list of Ortho info */
slSort(&orthoItemList, bedCmp);

/* Walk through two sorted lists together */
snpItem   = snpItemList;
orthoItem = orthoItemList;
while(snpItem!=NULL && orthoItem!=NULL)
    {
    /* check to see that we're not at the end of either list and that
     * the two list elements represent the same human position */
    while ( snpItem!=NULL && orthoItem!=NULL && (cmp = snpOrthoCmp(snpItem, orthoItem))!=0 )
	if (cmp<0)
	    snpItem = snpItem->next;
	else if (cmp>0)
	    orthoItem = orthoItem->next;
    /* either one of the lists is null or the two items refer to the
     * same human position */
    if (snpItem==NULL || orthoItem==NULL)
	break;
    /* update the snp->extraName with the ortho data */
    dyStringPrintf(extra, " %s>%s", ((struct orthoBed *)orthoItem)->chimp, ((struct snp125Extended *)snpItem)->observed);
    ((struct snp125Extended *)snpItem)->nameExtra = cloneString(extra->string);
    dyStringClear(extra);
    /* increment the list pointers */
    snpItem = snpItem->next;
    orthoItem = orthoItem->next;
    }
tg->items=snpItemList;
freeDyString(&extra);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

Color snp125ExtendedColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of snp track item. */
{
return ((struct snp125Extended *)item)->color;
}

int snp125ExtendedColorCmp(const void *va, const void *vb)
/* Compare to sort based on color -- black first, red last */
{
const struct snp125Extended *a = *((struct snp125Extended **)va);
const struct snp125Extended *b = *((struct snp125Extended **)vb);
const Color ca = a->color;
const Color cb = b->color;

/* order is important here */
if (ca==MG_RED)
    return 1;
if (cb==MG_RED)
    return -1;
if (ca==MG_GREEN)
    return 1;
if (cb==MG_GREEN)
    return -1;
if (ca==MG_BLUE)
    return 1;
if (cb==MG_BLUE)
    return -1;
if (ca==MG_GRAY)
    return 1;
if (cb==MG_GRAY)
    return -1;
if (ca==MG_BLACK)
    return 1;
if (cb==MG_BLACK)
    return -1;
hPrintComment("SNP track: colors %d (%s) and %d (%s) not known", ca, a->name, cb, b->name);
return 0;
}

void sortSnp125ExtendedByColor(struct track *tg)
/* Sort snps so that more functional snps (non-synonymous, splice site) are printed last.
 * Color calculation is used as an intermediate step to represent severity. */
{
/* snp and snpMap have different loaders that do not support the color
 * attribute of the snp125Extended struct */
if(differentString(tg->mapName,"snp") && differentString(tg->mapName,"snpMap"))
    slSort(&tg->items, snp125ExtendedColorCmp);
}

void loadSnp125Extended(struct track *tg)
/* load snps from snp125 table, ortho alleles from snpXXXortho table,
 * and return in extended struct */
{
struct sqlConnection   *conn      = hAllocConn();
int                     rowOffset = 0;
char                  **row       = NULL;
struct slList          *itemList  = tg->items;
struct slList          *item      = itemList;
struct sqlResult       *sr        = NULL;
struct snp125Extended  *se        = NULL;
enum   trackVisibility  vis       = tg->visibility;
enum   trackVisibility  visLim    = tg->limitedVis;
int                     i         = 0;

snp125AvHetCutoff = atof(cartUsualString(cart, "snp125AvHetCutoff", "0.0"));
snp125WeightCutoff = atoi(cartUsualString(cart, "snp125WeightCutoff", "3"));
snp125ExtendedNames = cartUsualBoolean(cart, "snp125ExtendedNames", FALSE);

for (i=0; i < snp125MolTypeCartSize; i++)
    {
    snp125MolTypeCart[i] = cartUsualString(cart, snp125MolTypeStrings[i], snp125MolTypeDefault[i]);
    snp125MolTypeIncludeCart[i] = cartUsualBoolean(cart, snp125MolTypeIncludeStrings[i], snp125MolTypeIncludeDefault[i]);
    }
for (i=0; i < snp125ClassCartSize; i++)
    {
    snp125ClassCart[i] = cartUsualString(cart, snp125ClassStrings[i], snp125ClassDefault[i]);
    snp125ClassIncludeCart[i] = cartUsualBoolean(cart, snp125ClassIncludeStrings[i], snp125ClassIncludeDefault[i]);
    }
for (i=0; i < snp125ValidCartSize; i++)
    {
    snp125ValidCart[i] = cartUsualString(cart, snp125ValidStrings[i], snp125ValidDefault[i]);
    snp125ValidIncludeCart[i] = cartUsualBoolean(cart, snp125ValidIncludeStrings[i], snp125ValidIncludeDefault[i]);
    }
for (i=0; i < snp125FuncCartSize; i++)
    {
    snp125FuncCart[i] = cartUsualString(cart, snp125FuncStrings[i], snp125FuncDefault[i]);
    snp125FuncIncludeCart[i] = cartUsualBoolean(cart, snp125FuncIncludeStrings[i], snp125FuncIncludeDefault[i]);
    }
for (i=0; i < snp125LocTypeCartSize; i++)
    {
    snp125LocTypeCart[i] = cartUsualString(cart, snp125LocTypeStrings[i], snp125LocTypeDefault[i]);
    snp125LocTypeIncludeCart[i] = cartUsualBoolean(cart, snp125LocTypeIncludeStrings[i], snp125LocTypeIncludeDefault[i]);
    }

/* load SNPs */
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);

if(differentString(tg->mapName,"snp") && differentString(tg->mapName,"snpMap"))
    while ((row = sqlNextRow(sr)) != NULL)
	{
	/* use loader for snp125 table format */
	item = (struct slList *)snp125ExtendedLoad(row + rowOffset);
	se = (struct snp125Extended *)item;
	se->color = snp125Color(tg, se, NULL);
	slAddHead(&itemList, item);
	}
else
    while ((row = sqlNextRow(sr)) != NULL)
	{
	/* use loader for pre-snp125 table format */
	item = (struct slList *)snpExtendedLoad(row + rowOffset);
	se = (struct snp125Extended *)item;
	se->color = snp125Color(tg, se, NULL);
	slAddHead(&itemList, item);
	}
sqlFreeResult(&sr);
hFreeConn(&conn);

slSort(&itemList, bedCmp);
tg->items = itemList;

filterSnpItems(tg, snp125AvHetFilterItem);
filterSnpItems(tg, snp125WeightFilterItem);
filterSnpItems(tg, snp125MolTypeFilterItem);
filterSnpItems(tg, snp125ClassFilterItem);
filterSnpItems(tg, snp125ValidFilterItem);
filterSnpItems(tg, snp125FuncFilterItem);
filterSnpItems(tg, snp125LocTypeFilterItem);

if(withLeftLabels && snp125ExtendedNames && (vis==tvPack||vis==tvFull) && !(visLim==tvDense||visLim==tvSquish) )
    setSnp125ExtendedNameExtra(tg);
}

void loadSnpMap(struct track *tg)
/* Load up snpMap from database table to track items. */
{
int  snpMapSource, snpMapType;

for (snpMapSource=0; snpMapSource<snpMapSourceCartSize; snpMapSource++)
    snpMapSourceCart[snpMapSource] = cartUsualString(cart, 
       snpMapSourceStrings[snpMapSource], snpMapSourceDefault[snpMapSource]);
for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    snpMapTypeCart[snpMapType] = cartUsualString(cart, 
       snpMapTypeStrings[snpMapType], snpMapTypeDefault[snpMapType]);
bedLoadItem(tg, "snpMap", (ItemLoader)snpMapLoad);
if (!startsWith("hg",database))
    return;
filterSnpMapItems(tg, snpMapSourceFilterItem);
filterSnpMapItems(tg, snpMapTypeFilterItem);
}

char *snp125ExtendedName(struct track *tg, void *item)
{
struct dyString *ds = newDyString(256);
struct snp125Extended *se = item;
char *ret = NULL;

dyStringPrintf(ds, "%s", se->name);
if (se!=NULL && se->nameExtra != NULL)
    dyStringAppend(ds, se->nameExtra);
ret = cloneString(ds->string);
freeDyString(&ds);
return ret;
}

void loadSnp(struct track *tg)
/* Load up snp from database table to track items. */
{
int  i = 0;

snpAvHetCutoff = atof(cartUsualString(cart, "snpAvHetCutoff", "0.0"));

for (i=0; i < snpSourceCartSize;  i++)
    snpSourceCart[i]  = cartUsualString(cart, snpSourceStrings[i],  snpSourceDefault[i]);
for (i=0; i < snpMolTypeCartSize; i++)
    snpMolTypeCart[i] = cartUsualString(cart, snpMolTypeStrings[i], snpMolTypeDefault[i]);
for (i=0; i < snpClassCartSize;   i++)
    snpClassCart[i]   = cartUsualString(cart, snpClassStrings[i],   snpClassDefault[i]);
for (i=0; i < snpValidCartSize;   i++)
    snpValidCart[i]   = cartUsualString(cart, snpValidStrings[i],   snpValidDefault[i]);
for (i=0; i < snpFuncCartSize;    i++)
    snpFuncCart[i]    = cartUsualString(cart, snpFuncStrings[i],    snpFuncDefault[i]);
for (i=0; i < snpLocTypeCartSize; i++)
    snpLocTypeCart[i] = cartUsualString(cart, snpLocTypeStrings[i], snpLocTypeDefault[i]);

bedLoadItem(tg, "snp", (ItemLoader)snpLoad);

filterSnpItems(tg, snpAvHetFilterItem);
filterSnpItems(tg, snpSourceFilterItem);
filterSnpItems(tg, snpMolTypeFilterItem);
filterSnpItems(tg, snpClassFilterItem);
filterSnpItems(tg, snpValidFilterItem);
filterSnpItems(tg, snpFuncFilterItem);
filterSnpItems(tg, snpLocTypeFilterItem);
}

void loadSnp125(struct track *tg)
/* Load up snp125 from database table to track items. */
{
int i = 0;

snp125AvHetCutoff = atof(cartUsualString(cart, "snp125AvHetCutoff", "0.0"));
snp125WeightCutoff = atoi(cartUsualString(cart, "snp125WeightCutoff", "3"));

for (i=0; i < snp125MolTypeCartSize; i++)
    {
    snp125MolTypeCart[i] = cartUsualString(cart, snp125MolTypeStrings[i], snp125MolTypeDefault[i]);
    snp125MolTypeIncludeCart[i] = cartUsualBoolean(cart, snp125MolTypeIncludeStrings[i], snp125MolTypeIncludeDefault[i]);
    }
for (i=0; i < snp125ClassCartSize; i++)
    {
    snp125ClassCart[i] = cartUsualString(cart, snp125ClassStrings[i], snp125ClassDefault[i]);
    snp125ClassIncludeCart[i] = cartUsualBoolean(cart, snp125ClassIncludeStrings[i], snp125ClassIncludeDefault[i]);
    }
for (i=0; i < snp125ValidCartSize; i++)
    {
    snp125ValidCart[i] = cartUsualString(cart, snp125ValidStrings[i], snp125ValidDefault[i]);
    snp125ValidIncludeCart[i] = cartUsualBoolean(cart, snp125ValidIncludeStrings[i], snp125ValidIncludeDefault[i]);
    }
for (i=0; i < snp125FuncCartSize; i++)
    {
    snp125FuncCart[i] = cartUsualString(cart, snp125FuncStrings[i], snp125FuncDefault[i]);
    snp125FuncIncludeCart[i] = cartUsualBoolean(cart, snp125FuncIncludeStrings[i], snp125FuncIncludeDefault[i]);
    }
for (i=0; i < snp125LocTypeCartSize; i++)
    {
    snp125LocTypeCart[i] = cartUsualString(cart, snp125LocTypeStrings[i], snp125LocTypeDefault[i]);
    snp125LocTypeIncludeCart[i] = cartUsualBoolean(cart, snp125LocTypeIncludeStrings[i], snp125LocTypeIncludeDefault[i]);
    }

bedLoadItem(tg, tg->mapName, (ItemLoader)snp125Load);

filterSnpItems(tg, snp125AvHetFilterItem);
filterSnpItems(tg, snp125WeightFilterItem);
filterSnpItems(tg, snp125MolTypeFilterItem);
filterSnpItems(tg, snp125ClassFilterItem);
filterSnpItems(tg, snp125ValidFilterItem);
filterSnpItems(tg, snp125FuncFilterItem);
filterSnpItems(tg, snp125LocTypeFilterItem);
}

void freeSnpMap(struct track *tg)
/* Free up snpMap items. */
{
snpMapFreeList((struct snpMap**)&tg->items);
}

void freeSnp(struct track *tg)
/* Free up snp items. */
{
snpFreeList((struct snp**)&tg->items);
}

void freeSnp125(struct track *tg)
/* Free up snp125 items. */
{
snp125FreeList((struct snp125**)&tg->items);
}


Color snpMapColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of snpMap track item. */
{
struct snpMap *el = item;
enum   snpColorEnum thisSnpColor = stringArrayIx( snpMapSourceCart[stringArrayIx(el->source,snpMapSourceDataName,snpMapSourceDataNameSize)],
						  snpColorLabel,snpColorLabelSize);

switch (thisSnpColor)
    {
    case snpColorRed:
 	return MG_RED;
 	break;
    case snpColorGreen:
 	return MG_GREEN;
 	break;
    case snpColorBlue:
 	return MG_BLUE;
 	break;
    default:
 	return MG_BLACK;
 	break;
    }
}

Color snp125Color(struct track *tg, void *item, struct vGfx *vg)
/* Return color of snp track item. */
{
struct snp125 *el = item;
enum   snp125ColorEnum thisSnpColor = snp125ColorBlack;
char  *snpColorSource = cartUsualString(cart, snp125ColorSourceDataName[0], snp125ColorSourceDefault[0]);
char  *validString = NULL;
char  *funcString = NULL;
int    snpValid = 0;
int    snpFunc = 0;
int    index1 = 0;
int    index2 = 0;

index1 = stringArrayIx(snpColorSource, snp125ColorSourceLabels, snp125ColorSourceLabelsSize);
switch (stringArrayIx(snpColorSource, snp125ColorSourceLabels, snp125ColorSourceLabelsSize))
    {
    case snp125ColorSourceMolType:
	index2 = stringArrayIx(el->molType,snp125MolTypeDataName,snp125MolTypeDataNameSize);
	if (index2 < 0) 
	    index2 = 0;
	thisSnpColor=(enum snp125ColorEnum)stringArrayIx(snp125MolTypeCart[index2],snp125ColorLabel,snp125ColorLabelSize);
	break;
    case snp125ColorSourceClass:
	index2 = stringArrayIx(el->class,snp125ClassDataName,snp125ClassDataNameSize);
	if (index2 < 0) 
	    index2 = 0;
	thisSnpColor=(enum snp125ColorEnum)stringArrayIx(snp125ClassCart[index2],snp125ColorLabel,snp125ColorLabelSize);
	break;
	/* valid is a set */
    case snp125ColorSourceValid:
	validString = cloneString(el->valid);
	for (snpValid=0; snpValid<snp125ValidCartSize; snpValid++)
	    if (containsStringNoCase(validString, snp125ValidDataName[snpValid]))
		thisSnpColor = (enum snp125ColorEnum) stringArrayIx(snp125ValidCart[snpValid],snp125ColorLabel,snp125ColorLabelSize);
	break;
	/* func is a set */
    case snp125ColorSourceFunc:
        funcString = cloneString(el->func);
	for (snpFunc=0; snpFunc<snp125FuncCartSize; snpFunc++)
	    if (containsStringNoCase(funcString, snp125FuncDataName[snpFunc]))
	        thisSnpColor = (enum snp125ColorEnum) stringArrayIx(snp125FuncCart[snpFunc],snp125ColorLabel,snp125ColorLabelSize);
	break;
    case snp125ColorSourceLocType:
	index2 = stringArrayIx(el->locType,snp125LocTypeDataName,snp125LocTypeDataNameSize);
	if (index2 < 0) 
	    index2 = 0;
	thisSnpColor=(enum snp125ColorEnum)stringArrayIx(snp125LocTypeCart[index2],snp125ColorLabel,snp125ColorLabelSize);
	break;
    default:
	thisSnpColor = snp125ColorBlack;
	break;
    }
switch (thisSnpColor)
    {
    case snp125ColorRed:
 	return MG_RED;
 	break;
    case snp125ColorGreen:
        return MG_GREEN;
 	break;
    case snp125ColorBlue:
 	return MG_BLUE;
 	break;
    case snp125ColorGray:
        return MG_GRAY;
	break;
    case snp125ColorBlack:
    default:
 	return MG_BLACK;
 	break;
    }
}

Color snpColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of snp track item. */
{
struct snp *el = item;
enum   snpColorEnum thisSnpColor = snpColorBlack;
char  *snpColorSource = cartUsualString(cart, snpColorSourceDataName[0], snpColorSourceDefault[0]);
char  *validString = NULL;
char  *funcString = NULL;
int    snpValid = 0;
int    snpFunc = 0;
int    index1 = 0;
int    index2 = 0;

index1 =  stringArrayIx(snpColorSource, snpColorSourceStrings, snpColorSourceStringsSize);

switch (stringArrayIx(snpColorSource, snpColorSourceStrings, snpColorSourceStringsSize))
    {
    case snpColorSourceSource:
	index2 = stringArrayIx(el->source,snpSourceDataName,snpSourceDataNameSize);
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpSourceCart[index2],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceMolType:
	index2 = stringArrayIx(el->molType,snpMolTypeDataName,snpMolTypeDataNameSize);
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpMolTypeCart[index2],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceClass:
	index2 = stringArrayIx(el->class,snpClassDataName,snpClassDataNameSize);
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpClassCart[index2],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceValid:
	validString = cloneString(el->valid);
	for (snpValid=0; snpValid<snpValidCartSize; snpValid++)
	    if (containsStringNoCase(validString, snpValidDataName[snpValid]))
		thisSnpColor = (enum snpColorEnum) stringArrayIx(snpValidCart[snpValid],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceFunc:
	funcString = cloneString(el->func);
	for (snpFunc=0; snpFunc<snpFuncCartSize; snpFunc++)
	    if (containsStringNoCase(funcString, snpFuncDataName[snpFunc]))
		thisSnpColor = (enum snpColorEnum) stringArrayIx(snpFuncCart[snpFunc],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceLocType:
	index2 = stringArrayIx(el->locType,snpLocTypeDataName,snpLocTypeDataNameSize);
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpLocTypeCart[index2],snpColorLabel,snpColorLabelSize);
	break;
    default:
	thisSnpColor = snpColorBlack;
	break;
    }
switch (thisSnpColor)
    {
    case snpColorRed:
 	return MG_RED;
 	break;
    case snpColorGreen:
 	return MG_GREEN;
 	break;
    case snpColorBlue:
 	return MG_BLUE;
 	break;
    case snpColorBlack:
    default:
 	return MG_BLACK;
 	break;
    }
}

void snpMapDrawItemAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single snpMap item at position. */
{
struct snpMap *sm = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)sm->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)sm->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
Color itemColor = tg->itemColor(tg, sm, vg);

if ( w<1 )
    w = 1;
vgBox(vg, x1, y, w, heightPer, itemColor);
/* Clip here so that text will tend to be more visible... */
if (tg->drawName && vis != tvSquish)
    mapBoxHc(sm->chromStart, sm->chromEnd, x1, y, w, heightPer, tg->mapName, tg->mapItemName(tg, sm), NULL);
}

void snpDrawItemAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single snp item at position. */
{
struct snp *s = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)s->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)s->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
Color itemColor = tg->itemColor(tg, s, vg);

if ( w<1 )
    w = 1;
vgBox(vg, x1, y, w, heightPer, itemColor);
/* Clip here so that text will tend to be more visible... */
if (tg->drawName && vis != tvSquish)
    mapBoxHc(s->chromStart, s->chromEnd, x1, y, w, heightPer,
	     tg->mapName, tg->mapItemName(tg, s), NULL);
}

void snp125DrawItemAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single snp125 item at position. */
{
struct snp125 *s = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)s->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)s->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
Color itemColor = tg->itemColor(tg, s, vg);

if ( w<1 )
    w = 1;
vgBox(vg, x1, y, w, heightPer, itemColor);
/* Clip here so that text will tend to be more visible... */
if (tg->drawName && vis != tvSquish)
    mapBoxHc(s->chromStart, s->chromEnd, x1, y, w, heightPer,
	     tg->mapName, tg->mapItemName(tg, s), NULL);
}

static void snpMapDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw snpMap items. */
{
double scale = scaleForPixels(width);
int lineHeight = tg->lineHeight;
int heightPer = tg->heightPer;
int w, y;
boolean withLabels = (withLeftLabels && vis == tvPack && !tg->drawName);

if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->mapName);

if (vis == tvPack || vis == tvSquish)
    {
    struct spaceSaver *ss = tg->ss;
    struct spaceNode *sn = NULL;
    vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
        {
        struct slList *item = sn->val;
        int s = tg->itemStart(tg, item);
        int e = tg->itemEnd(tg, item);
        int x1 = round((s - winStart)*scale) + xOff;
        int x2 = round((e - winStart)*scale) + xOff;
        int textX = x1;
        char *name = tg->itemName(tg, item);
	Color itemColor = tg->itemColor(tg, item, vg);
	Color itemNameColor = tg->itemNameColor(tg, item, vg);
	
        y = yOff + lineHeight * sn->row;
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, itemColor, vis);
        if (withLabels)
            {
            int nameWidth = mgFontStringWidth(font, name);
            int dotWidth = tl.nWidth/2;
            textX -= nameWidth + dotWidth;
            if (textX < insideX)        /* Snap label to the left. */
		{
		textX = leftLabelX;
		vgUnclip(vg);
		vgSetClip(vg, leftLabelX, yOff, insideWidth, tg->height);
		vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, heightPer,
			    itemNameColor, font, name);
		vgUnclip(vg);
		vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
		}
            else
		vgTextRight(vg, textX, y, nameWidth, heightPer, 
			    itemNameColor, font, name);
            }
        if (!tg->mapsSelf && ( ( w = x2-textX ) > 0 ))
	    mapBoxHgcOrHgGene(s, e, textX, y, w, heightPer, tg->mapName, tg->mapItemName(tg, item), name, NULL, FALSE);
        }
    vgUnclip(vg);
    }
else
    {
    struct slList *item;
    y = yOff;
    for (item = tg->items; item != NULL; item = item->next)
        {
	Color itemColor = tg->itemColor(tg, item, vg);
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, itemColor, vis);
        if (vis == tvFull) 
	    y += lineHeight;
        } 
    }
}

static void snpDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw snp items. */
{
double scale = scaleForPixels(width);
int lineHeight = tg->lineHeight;
int heightPer = tg->heightPer;
int y, w;
boolean withLabels = (withLeftLabels && vis == tvPack && !tg->drawName);

if(vis==tvDense)
    sortSnp125ExtendedByColor(tg);
if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->mapName);
if (vis == tvPack || vis == tvSquish)
    {
    struct spaceSaver *ss = tg->ss;
    struct spaceNode *sn = NULL;
    vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
        {
        struct slList *item = sn->val;
        int s = tg->itemStart(tg, item);
        int e = tg->itemEnd(tg, item);
        int x1 = round((s - winStart)*scale) + xOff;
        int x2 = round((e - winStart)*scale) + xOff;
        int textX = x1;
        char *name = tg->itemName(tg, item);
	Color itemColor = tg->itemColor(tg, item, vg);
	Color itemNameColor = tg->itemNameColor(tg, item, vg);
	boolean drawNameInverted = FALSE;
	
        y = yOff + lineHeight * sn->row;
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, itemColor, vis);
        if (withLabels)
            {
            int nameWidth = mgFontStringWidth(font, name);
            int dotWidth = tl.nWidth/2;
	    boolean snapLeft = FALSE;
	    drawNameInverted = highlightItem(tg, item);
            textX -= nameWidth + dotWidth;
	    snapLeft = (textX < insideX);
            if (snapLeft)        /* Snap label to the left. */
		{
		textX = leftLabelX;
		vgUnclip(vg);
		vgSetClip(vg, leftLabelX, yOff, insideWidth, tg->height);
		if (drawNameInverted)
		    {
		    int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
		    vgBox(vg, boxStart, y, nameWidth+1, heightPer - 1, color);
		    vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, heightPer,
		                MG_WHITE, font, name);
		    }
		else
		    vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, heightPer,
			    itemNameColor, font, name);
		vgUnclip(vg);
		vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
		}
            else
	        {
		if (drawNameInverted)
		    {
		    vgBox(vg, textX - 1, y, nameWidth+1, heightPer-1, color);
		    vgTextRight(vg, textX, y, nameWidth, heightPer, MG_WHITE, font, name);
		    }
		else
		    vgTextRight(vg, textX, y, nameWidth, heightPer, 
			    itemNameColor, font, name);
		}
            }
        if (!tg->mapsSelf && ( ( w = x2-textX ) > 0 ) )
	    mapBoxHgcOrHgGene(s, e, textX, y, w, heightPer, tg->mapName, tg->mapItemName(tg, item), name, NULL, FALSE);
        }
    vgUnclip(vg);
    }
else
    {
    struct slList *item;
    y = yOff;
    for (item = tg->items; item != NULL; item = item->next)
        {
	Color itemColor = tg->itemColor(tg, item, vg);
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, itemColor, vis);
        if (vis == tvFull) 
	    y += lineHeight;
        } 
    }
}

void snpMapMethods(struct track *tg)
/* Make track for snps. */
{
tg->drawItems = snpMapDrawItems;
tg->drawItemAt = snpMapDrawItemAt;
tg->loadItems = loadSnpMap;
tg->freeItems = freeSnpMap;
tg->itemColor = snpMapColor;
tg->itemNameColor = snpMapColor;
}

void snpMethods(struct track *tg)
/* Make track for snps. */
{
tg->drawItems     = snpDrawItems;
tg->drawItemAt    = snpDrawItemAt;
tg->loadItems     = loadSnp;
tg->freeItems     = freeSnp;
tg->itemColor     = snpColor;
tg->itemNameColor = snpColor;
}

void snp125Methods(struct track *tg)
{
struct sqlConnection *conn = hAllocConn();

tg->drawItems     = snpDrawItems;
tg->drawItemAt    = snp125DrawItemAt;
tg->freeItems     = freeSnp125;
tg->loadItems     = loadSnp125Extended;
tg->itemNameColor = snp125ExtendedColor;
tg->itemColor     = snp125ExtendedColor;
if (sqlTableExists(conn,"snp126orthoPanTro2RheMac2"))
    tg->itemName  = snp125ExtendedName;
hFreeConn(&conn);
}

char *perlegenName(struct track *tg, void *item)
/* return the actual perlegen name, in form xx/yyyy:
      cut off xx/ and return yyyy */
{
char *name;
struct linkedFeatures *lf = item;
name = strstr(lf->name, "/");
if (name != NULL)
    name ++;
if (name != NULL)
    return name;
return "unknown";
}

int haplotypeHeight(struct track *tg, struct linkedFeatures *lf,
                    struct simpleFeature *sf)
/* if the item isn't the first or the last make it smaller */
{
if(sf == lf->components || sf->next == NULL)
    return tg->heightPer;
return (tg->heightPer-4);
}

static void haplotypeLinkedFeaturesDrawAt(struct track *tg, void *item,
               struct vGfx *vg, int xOff, int y, double scale, 
	       MgFont *font, Color color, enum trackVisibility vis)
/* draws and individual haplotype and a given location */
{
struct linkedFeatures *lf = item;
struct simpleFeature *sf = NULL;
int x1 = 0;
int x2 = 0;
int w = 0;
int heightPer = tg->heightPer;
int shortHeight = heightPer / 2;
int s = 0;
int e = 0;
Color *shades = tg->colorShades;
boolean isXeno = (tg->subType == lfSubXeno) || (tg->subType == lfSubChain);
boolean hideLine = (vis == tvDense && isXeno);

/* draw haplotype thick line ... */
if (lf->components != NULL && !hideLine)
    {
    x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
    x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
    w = x2 - x1;
    color = shades[lf->grayIx+isXeno];

    vgBox(vg, x1, y+3, w, shortHeight-2, color);
    if (vis==tvSquish)
	vgBox(vg, x1, y+1, w, tg->heightPer/2, color);
    else
	vgBox(vg, x1, y+3, w, shortHeight-1, color);
    }
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    s = sf->start;
    e = sf->end;
    heightPer = haplotypeHeight(tg, lf, sf);
    if (vis==tvSquish)
	drawScaledBox(vg, s, e, scale, xOff, y, tg->heightPer, blackIndex());
    else
	drawScaledBox(vg, s, e, scale, xOff, y+((tg->heightPer-heightPer)/2),
		      heightPer, blackIndex());
    }
}

void haplotypeMethods(struct track *tg)
/* setup special methods for haplotype track */
{
tg->drawItemAt = haplotypeLinkedFeaturesDrawAt;
tg->colorShades = shadesOfSea;
}

void perlegenMethods(struct track *tg)
/* setup special methods for Perlegen haplotype track */
{
haplotypeMethods(tg);
tg->itemName = perlegenName;
}


/*******************************************************************/

/* Declare our color gradients and the the number of colors in them */
Color ldShadesPos[LD_DATA_SHADES];
Color ldHighLodLowDprime; /* pink */
Color ldHighDprimeLowLod; /* blue */
int colorLookup[256];

void ldShadesInit(struct track *tg, struct vGfx *vg, boolean isDprime) 
/* Allocate the LD for positive and negative values, and error cases */
{
static struct rgbColor white = {255, 255, 255};
static struct rgbColor red   = {255,   0,   0};
static struct rgbColor green = {  0, 255,   0};
static struct rgbColor blue  = {  0,   0, 255};
struct dyString *dsLdPos = newDyString(32);
char *ldPos = NULL;

if (startsWith("hapmapLd", tg->mapName))
    dyStringPrintf(dsLdPos, "hapmapLd_pos");
else
    dyStringPrintf(dsLdPos, "%s_pos", tg->mapName);
ldPos = cartUsualString(cart, dsLdPos->string, ldPosDefault);
ldHighLodLowDprime = vgFindColorIx(vg, 255, 224, 224); /* pink */
ldHighDprimeLowLod = vgFindColorIx(vg, 192, 192, 240); /* blue */
if (sameString(ldPos,"red")) 
    vgMakeColorGradient(vg, &white, &red,   LD_DATA_SHADES, ldShadesPos);
else if (sameString(ldPos,"blue"))
    vgMakeColorGradient(vg, &white, &blue,  LD_DATA_SHADES, ldShadesPos);
else if (sameString(ldPos,"green"))
    vgMakeColorGradient(vg, &white, &green, LD_DATA_SHADES, ldShadesPos);
else
    errAbort("LD fill color must be 'red', 'blue', or 'green'; '%s' is not recognized", ldPos);
}

void bedLoadLdItemByQuery(struct track *tg, char *table, 
			char *query, ItemLoader loader)
/* LD specific tg->item loader, as we need to load items beyond
   the current window to load the chromEnd positions for LD values. */
{
struct sqlConnection *conn = hAllocConn();
int rowOffset = 0;
int chromEndOffset = min(winEnd-winStart, 250000); /* extended chromEnd range */
struct sqlResult *sr = NULL;
char **row = NULL;
struct slList *itemList = NULL, *item = NULL;

if(query == NULL)
    sr = hRangeQuery(conn, table, chromName, winStart, winEnd+chromEndOffset, NULL, &rowOffset);
else
    sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    item = loader(row + rowOffset);
    slAddHead(&itemList, item);
    }
slSort(&itemList, bedCmp);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

void bedLoadLdItem(struct track *tg, char *table, ItemLoader loader)
/* LD specific tg->item loader. */
{
bedLoadLdItemByQuery(tg, table, NULL, loader);
}

void ldLoadItems(struct track *tg)
/* loadItems loads up items for the chromosome range indicated.   */
{
int count = 0;

bedLoadLdItem(tg, tg->mapName, (ItemLoader)ldLoad);
count = slCount((struct sList *)(tg->items));
tg->canPack = FALSE;
}

void mapDiamondUi(int xl, int yl, int xt, int yt, 
		  int xr, int yr, int xb, int yb, 
		  char *name, char *shortLabel)
/* Print out image map rectangle that invokes hgTrackUi. */
{
hPrintf("<AREA SHAPE=POLY COORDS=\"%d,%d,%d,%d,%d,%d,%d,%d\" ", 
	xl, yl, xt, yt, xr, yr, xb, yb);
/* change 'hapmapLd' to use parent composite table name */
/* move this to hgTracks when finished */
hPrintf("HREF=\"%s?%s=%u&c=%s&g=hapmapLd&i=%s\"", hgTrackUiName(), 
	cartSessionVarName(), cartSessionId(cart), chromName, name);
mapStatusMessage("%s controls", shortLabel);
hPrintf(">\n");
}

void mapTrackBackground(struct track *tg, int xOff, int yOff)
/* Print out image map rectangle that invokes hgTrackUi. */
{
struct dyString *name = newDyString(32);

if (startsWith("hapmapLd", tg->mapName))
    dyStringPrintf(name, "hapmapLd");
else
    dyStringPrintf(name, "%s", tg->mapName);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", 
	xOff, yOff, xOff+insideWidth, yOff+tg->height);
/* change 'hapmapLd' to use parent composite table name */
/* move this to hgTracks when finished */
hPrintf("HREF=\"%s?%s=%u&c=%s&g=%s&i=%s\"", hgTrackUiName(), 
	cartSessionVarName(), cartSessionId(cart), chromName, 
	name->string, name->string);
mapStatusMessage("%s controls", tg->mapName);
hPrintf(">\n");
}

int ldTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height. Called before and after drawItems. 
 * Must set height, lineHeight, heightPer */ 
{
tg->lineHeight = tl.fontHeight + 1;
tg->heightPer  = tg->lineHeight - 1;
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    tg->height = tg->lineHeight;
else if (winEnd-winStart<250000)
    tg->height = insideWidth/2;
else
    tg->height = max((int)(insideWidth*(250000.0/2.0)/(winEnd-winStart)),tg->lineHeight);
return tg->height;
}

void initColorLookup(struct track *tg, struct vGfx *vg, boolean isDprime)
{
ldShadesInit(tg, vg, isDprime);
colorLookup[(int)'a'] = ldShadesPos[0];
colorLookup[(int)'b'] = ldShadesPos[1];
colorLookup[(int)'c'] = ldShadesPos[2];
colorLookup[(int)'d'] = ldShadesPos[3];
colorLookup[(int)'e'] = ldShadesPos[4];
colorLookup[(int)'f'] = ldShadesPos[5];
colorLookup[(int)'g'] = ldShadesPos[6];
colorLookup[(int)'h'] = ldShadesPos[7];
colorLookup[(int)'i'] = ldShadesPos[8];
colorLookup[(int)'j'] = ldShadesPos[9];
colorLookup[(int)'y'] = ldHighLodLowDprime; /* LOD error case */
colorLookup[(int)'z'] = ldHighDprimeLowLod; /* LOD error case */
}

void drawDiamond(struct vGfx *vg, 
	 int xl, int yl, int xt, int yt, int xr, int yr, int xb, int yb, 
	 Color fillColor, Color outlineColor)
/* Draw diamond shape. */
{
struct gfxPoly *poly = gfxPolyNew();
gfxPolyAddPoint(poly, xl, yl);
gfxPolyAddPoint(poly, xt, yt);
gfxPolyAddPoint(poly, xr, yr);
gfxPolyAddPoint(poly, xb, yb);
vgDrawPoly(vg, poly, fillColor, TRUE);
if (outlineColor != 0)
    vgDrawPoly(vg, poly, outlineColor, FALSE);
gfxPolyFree(&poly);
}

void ldDrawDiamond(struct vGfx *vg, struct track *tg, int width, 
		   int xOff, int yOff, int a, int b, int c, int d, 
		   Color shade, Color outlineColor, double scale, 
		   boolean drawMap, char *name, enum trackVisibility vis,
		   boolean trim)
/* Draw and map a single pairwise LD box */
{
int yMax = ldTotalHeight(tg, vis)+yOff;
/* convert from genomic coordinates to vg coordinates */
int xl = round((double)(scale*((c+a)/2-winStart))) + xOff;
int xt = round((double)(scale*((d+a)/2-winStart))) + xOff;
int xr = round((double)(scale*((d+b)/2-winStart))) + xOff;
int xb = round((double)(scale*((c+b)/2-winStart))) + xOff;
int yl = round((double)(scale*(c-a)/2)) + yOff;
int yt = round((double)(scale*(d-a)/2)) + yOff;
int yr = round((double)(scale*(d-b)/2)) + yOff;
int yb = round((double)(scale*(c-b)/2)) + yOff;
boolean ldInv;
struct dyString *dsLdInv = newDyString(32);

dyStringPrintf(dsLdInv, "%s_inv", tg->mapName);
ldInv = cartUsualBoolean(cart, dsLdInv->string, ldInvDefault);
if (!ldInv)
    {
    yl = yMax - yl + yOff;
    yt = yMax - yt + yOff;
    yr = yMax - yr + yOff;
    yb = yMax - yb + yOff;
    }
/* correct bottom coordinate if necessary */
if (yb<=0)
    yb=1;
if (yt>yMax && trim)
    yt=yMax;
drawDiamond(vg, xl, yl, xt, yt, xr, yr, xb, yb, shade, outlineColor);
return; /* mapDiamondUI is working well, but there is a bug with 
	   AREA=POLY on the Mac browsers, so this will be 
	   postponed for now by not using this code */
if (drawMap && xt-xl>5 && xb-xl>5)
    mapDiamondUi(xl, yl, xt, yt, xr, yr, xb, yb, name, tg->mapName);
}

Color getOutlineColor(struct track *tg, int itemCount)
/* get outline color from cart and set outlineColor*/
{
struct dyString *dsLdOut = newDyString(32);
char *outColor = NULL;

if(startsWith("hapmapLd", tg->mapName))
    dyStringPrintf(dsLdOut, "hapmapLd_out");
else
    dyStringPrintf(dsLdOut, "%s_out", tg->mapName);
outColor = cartUsualString(cart, dsLdOut->string, ldOutDefault);
if (winEnd-winStart > 100000)
    return 0;
if (sameString(outColor,"yellow"))
    return MG_YELLOW;
else if (sameString(outColor,"red"))
    return MG_RED;
else if (sameString(outColor,"blue"))
    return MG_BLUE;
else if (sameString(outColor,"green"))
    return MG_GREEN;
else if (sameString(outColor,"white"))
    return MG_WHITE;
else if (sameString(outColor,"black"))
    return MG_BLACK;
else
    return 0;
}

boolean notInWindow(int a, int b, int c, int d, boolean trim)
/* determine if the LD diamond is within the drawing window */
{
if ((b+d)/2 <= winStart)        /* outside window on the left */
    return TRUE;
if ((a+c)/2 >= winEnd)          /* outside window on the right */
    return TRUE;
if ((c-b)/2 >= winEnd-winStart) /* bottom is outside window */
    return TRUE;
if (trim && d >= winEnd)        /* trim right edge */
    return TRUE;
if (trim && a <= winStart)      /* trim left edge */
    return TRUE;
if (!trim && b <= winStart-(winEnd-winStart))
    return TRUE;
if (!trim && c >= winEnd+(winEnd-winStart))
    return TRUE;
return FALSE;
}

int ldIndexCharToInt(char charValue)
/* convert from character encoding to intensity index */
{
switch (charValue)
    {
    case 'a': return 0;
    case 'b': return 1;
    case 'c': return 2;
    case 'd': return 3;
    case 'e': return 4;
    case 'f': return 5;
    case 'g': return 6;
    case 'h': return 7;
    case 'i': return 8;
    case 'j': return 9;
    case 'y': return -100;
    case 'z': return -101;
    }
return -102;
}

int ldIndexIntToChar(int index)
/* convert from intensity index to character encoding */
{
switch (index)
    {
    case 0: return 'a';
    case 1: return 'b';
    case 2: return 'c';
    case 3: return 'd';
    case 4: return 'e';
    case 5: return 'f';
    case 6: return 'g';
    case 7: return 'h';
    case 8: return 'i';
    case 9: return 'j';
    case -100: return 'y';
    case -101: return 'z';
    }
return 'x';
}

void ldAddToDenseValueHash(struct hash *ldHash, unsigned a, char charValue)
/* Add new values to LD hash or update existing values.
   Values are averaged along the diagonals. */
{
struct ldStats *stats;
char name[16];
int indexValue = ldIndexCharToInt(charValue);
int incrementN = 1;

if (a<winStart || a>winEnd)
    return;
/* Include SNPs with error codes, but don't add their values. 
 * This will allow the marker to be drawn, even if all values
 *   are missing. */
if (indexValue<0)
    {
    incrementN = 0;
    indexValue = 0;
    }
safef(name, sizeof(name), "%d", a);
stats = hashFindVal(ldHash, name);
if (!stats)
    {
    AllocVar(stats);
    stats->name      = name;
    stats->n         = incrementN;
    stats->sumValues = indexValue;
    hashAddSaveName(ldHash, name, stats, &stats->name);
    }
else
    {
    stats->n         += incrementN;
    stats->sumValues += indexValue;
    }
}

void ldDrawDenseValue(struct vGfx *vg, struct track *tg, int xOff, int y1, 
		      double scale, Color outlineColor, struct ldStats *d)
/* Draw single dense LD value */
{
int   colorInt  = (d->n > 0) ? round(d->sumValues/d->n) : -100;
char  colorChar = ldIndexIntToChar(colorInt);
Color shade     = (d->n > 0) ? colorLookup[(int)colorChar] : ldHighDprimeLowLod;
int   w         = 3; /* width of box */
int   w2        = w/2;
int   x         = round((atoi(d->name)-winStart)*scale) + xOff - w2;
int   x1=x-w2, x2=x1+w, y2=y1+tg->heightPer-1;

vgBox(vg, x1, y1, w, tg->heightPer, shade);
if (outlineColor!=0)
    {
    vgLine(vg, x1, y1, x2, y1, outlineColor);
    vgLine(vg, x1, y2, x2, y2, outlineColor);
    vgLine(vg, x1, y1, x1, y2, outlineColor);
    vgLine(vg, x2, y1, x2, y2, outlineColor);
    }
}

void ldDrawDenseValueHash(struct vGfx *vg, struct track *tg, int xOff, int yOff, 
			  double scale, Color outlineColor, struct hash *ldHash)
/* Draw all dense LD values */
{
struct hashEl *hashEl, *stats=hashElListHash(ldHash);
Color yellow = vgFindRgb(vg, &undefinedYellowColor);

vgBox(vg, insideX, yOff, insideWidth, tg->height-1, yellow);
for (hashEl=stats; hashEl!=NULL; hashEl=hashEl->next)
    ldDrawDenseValue(vg, tg, xOff, yOff, scale, outlineColor, hashEl->val);
hashElFreeList(&stats);
}

void ldDrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width, int height, 
	boolean withCenterLabels, MgFont *font,
	Color color, enum trackVisibility vis)
/* Draw left labels. */
{
char  label[16];
char *ldVal = cartUsualString(cart, "hapmapLd_val", ldValDefault);
char *pop         = tg->mapName;
int   yVisOffset  = ( vis == tvDense ? 0 : tg->heightPer + height/2 );
struct dyString *dsLdVal = newDyString(32);
if (!startsWith("hapmapLd", tg->mapName))
    {
    dyStringPrintf(dsLdVal, "%s_val", tg->mapName);
    ldVal = cartUsualString(cart, dsLdVal->string, ldValDefault);
    }
if (startsWith("hapmapLd", pop) && strlen(tg->mapName)>8)
    {
    pop += 8;
    if (sameString(tg->mapName, "hapmapLdChbJpt"))
	pop = cloneString("Jpt+Chb");
    }
else
    pop = "";

if (sameString(ldVal, "lod"))
    ldVal = cloneString("LOD");
else if (sameString(ldVal, "rsquared"))
    ldVal = cloneString("R^2");
else if (sameString(ldVal, "dprime"))
    ldVal = cloneString("D'");
else
    errAbort("%s values are not recognized", ldVal);

safef(label, sizeof(label), "LD %s %s", pop, ldVal);

toUpperN(label, sizeof(label));
vgUnclip(vg);
vgSetClip(vg, leftLabelX, yOff+yVisOffset, leftLabelWidth, tg->heightPer);
vgTextRight(vg, leftLabelX, yOff+yVisOffset, leftLabelWidth, tg->heightPer, color, font, label);
vgUnclip(vg);
vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
}


/* ldDrawItems -- lots of disk and cpu optimizations here.  
 * There are three data fields, (lod, rsquared, and dprime) in each
   item, and each is a list of pairwise values.  The values are
   encoded in ascii to save disk space and color calculation (drawing)
   time.  As these are pairwise values and it is expensive to store
   the second coordinate for each value, each item has only a
   chromosome coordinate for the first of the pair (chromStart), and
   the second coordinate is inferred from the list of items.  This
   means that two pointers are necessary: a data pointer (dPtr) to
   keep track of the current data, and a second pointer (sPtr) to
   retrieve the second coordinate.
 * The length of each of the three value vectors (rsquared, dprime,
   and lod) is the same and is stored in the score field.
 * The ascii values are mapped to colors in the colorLookup[] array.
 * The four points of each diamond are calculated from the chromosomal 
   coordinates of four SNPs:
     a: the SNP at dPtr->chromStart
     b: the SNP immediately 3' of the chromStart (dPtr->next->chromStart)
     c: the SNP immediately 5' of the second position's chromStart (sPtr->chromStart)
     d: the SNP at the second position's chromStart (sPtr->next->chromStart) 
 * The chromosome coordinates are converted into vg coordinates in
   ldDrawDiamond.
 * A counter (i) is used to keep from reading beyond the end of the 
   value array.  */
void ldDrawItems(struct track *tg, int seqStart, int seqEnd,
		 struct vGfx *vg, int xOff, int yOff, int width,
		 MgFont *font, Color color, enum trackVisibility vis)
/* Draw item list, one per track. */
{
struct ld   *dPtr      = NULL, *sPtr = NULL; /* pointers to 5' and 3' ends */
boolean      isLod     = FALSE, isRsquared = FALSE, isDprime = FALSE;
double       scale     = scaleForPixels(insideWidth);
int          itemCount = slCount((struct slList *)tg->items);
Color        shade     = 0, outlineColor = getOutlineColor(tg, itemCount);
int          a, b, c, d, i; /* chromosome coordinates and counter */
boolean      drawMap   = FALSE; /* ( itemCount<1000 ? TRUE : FALSE ); */
struct hash *ldHash    = newHash(20);
Color        yellow    = vgFindRgb(vg, &undefinedYellowColor);
char        *ldVal     = NULL;
boolean      ldTrm;
struct dyString *dsLdVal = newDyString(32);
struct dyString *dsLdTrm = newDyString(32);

if(startsWith("hapmapLd",tg->mapName))
    {
    dyStringPrintf(dsLdVal, "hapmapLd_val");
    dyStringPrintf(dsLdTrm, "hapmapLd_trm");
    }
else
    {
    dyStringPrintf(dsLdVal, "%s_val", tg->mapName);
    dyStringPrintf(dsLdTrm, "%s_trm", tg->mapName);
    }
ldVal = cartUsualString( cart, dsLdVal->string, ldValDefault);
ldTrm = cartUsualBoolean(cart, dsLdTrm->string, ldTrmDefault);
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    vgBox(vg, insideX, yOff, insideWidth, tg->height-1, yellow);
mapTrackBackground(tg, xOff, yOff);
if (tg->items==NULL)
    return;

if (sameString(ldVal, "lod")) /* only one value can be drawn at a time, so figure it out here */
    isLod = TRUE;
else if (sameString(ldVal, "dprime"))
    isDprime = TRUE;
else if (sameString(ldVal, "rsquared"))
    isRsquared = TRUE;
else
    errAbort ("LD score value must be 'rsquared', 'dprime', or 'lod'; '%s' is not known", ldVal);

/* initialize arrays to convert from ascii encoding to color values */
initColorLookup(tg, vg, isDprime);

/* Loop through all items to get values and initial coordinates (a and b) */
for (dPtr=tg->items; dPtr!=NULL && dPtr->next!=NULL; dPtr=dPtr->next)
    {
    a = dPtr->chromStart;
    b = dPtr->next->chromStart;
    i = 0;
    if ((ldTrm && a <=winStart)||(!ldTrm && b <=winStart-(winEnd-winStart))) /* return if this item is not to be drawn */
	continue;
    if (isLod) /* point to the right data values to be drawn.  'ldVal' variable is reused */
	ldVal = dPtr->lod;
    else if (isRsquared)
	ldVal = dPtr->rsquared;
    else if (isDprime)
	ldVal = dPtr->dprime;
    /* Loop through all items again to get end coordinates (c and d): used to be 'drawNecklace' */
    for ( sPtr=dPtr; i<dPtr->score && sPtr!=NULL && sPtr->next!=NULL; sPtr=sPtr->next )
	{
	c = sPtr->chromStart;
	d = sPtr->next->chromStart;
	if (notInWindow(a, b, c, d, ldTrm)) /* Check to see if this diamond needs to be drawn, or if it is out of the window */
	    {
	    if ((c-b)/2 >= winEnd-winStart || (ldTrm&&d>=winEnd) || (!ldTrm&&c>=winEnd+(winEnd-winStart)))
		i = dPtr->score;
	    continue;
	    }
	if ( d-a > 250000 ) /* Check to see if we are trying to reach across a window that is too wide (centromere) */
	    continue;
	shade = colorLookup[(int)ldVal[i]];
	if ( vis==tvFull && ( !tg->limitedVisSet || ( tg->limitedVisSet && tg->limitedVis==tvFull ) ) )
	    ldDrawDiamond(vg, tg, width, xOff, yOff, a, b, c, d, shade, outlineColor, scale, drawMap, dPtr->name, vis, ldTrm);
	else if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
	    {
	    ldAddToDenseValueHash(ldHash, a, ldVal[i]);
	    ldAddToDenseValueHash(ldHash, d, ldVal[i]);
	    }
	else
	    errAbort("Visibility '%s' is not supported for the LD track yet.", hStringFromTv(vis));
	i++;
	}
    /* reached end of chromosome, so sPtr->next is null; draw last diamond in list */
    if (sPtr->next==NULL)
	{
	a = dPtr->chromStart;
	b = dPtr->chromEnd;
	c = sPtr->chromStart;
	d = sPtr->chromEnd;
	if (notInWindow(a, b, c, d, ldTrm)) /* Check to see if this diamond needs to be drawn, or if it is out of the window */
	    continue;
	shade = colorLookup[(int)ldVal[i]];
	if ( vis==tvFull && ( !tg->limitedVisSet || ( tg->limitedVisSet && tg->limitedVis==tvFull ) ) )
	    ldDrawDiamond(vg, tg, width, xOff, yOff, a, b, c, d, shade, outlineColor, scale, drawMap, dPtr->name, vis, ldTrm);
	else if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
	    {
	    ldAddToDenseValueHash(ldHash, a, ldVal[i]);
	    ldAddToDenseValueHash(ldHash, d, ldVal[i]);
	    }
	else
	    errAbort("Visibility '%s' is not supported for the LD track yet.", hStringFromTv(vis));
	}
    }
/* starting at last snp on chromosome, so dPtr->next is null; draw this diamond */
if (dPtr->next==NULL)
    {
    a = dPtr->chromStart;
    b = dPtr->chromEnd;
    if (!notInWindow(a, b, a, b, ldTrm)) /* Continue only if this diamond needs to be drawn */
	{
	if (isLod) /* point to the right data values to be drawn.  'ldVal' variable is reused */
	    ldVal = dPtr->lod;
	else if (isRsquared)
	    ldVal = dPtr->rsquared;
	else if (isDprime)
	    ldVal = dPtr->dprime;
	shade = colorLookup[(int)ldVal[0]];
	if ( vis==tvFull && ( !tg->limitedVisSet || ( tg->limitedVisSet && tg->limitedVis==tvFull ) ) )
	    ldDrawDiamond(vg, tg, width, xOff, yOff, a, b, a, b, shade, outlineColor, scale, drawMap, dPtr->name, vis, ldTrm);
	else if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
	    {
	    ldAddToDenseValueHash(ldHash, a, ldVal[0]);
	    ldAddToDenseValueHash(ldHash, b, ldVal[0]);
	    }
	else
	    errAbort("Visibility '%s' is not supported for the LD track yet.", hStringFromTv(vis));
	}
    }
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    ldDrawDenseValueHash(vg, tg, xOff, yOff, scale, outlineColor, ldHash);
}

void ldFreeItems(struct track *tg)
/* Free item list. */
{
ldFreeList((struct ld**)&tg->items);
}

void ldMethods(struct track *tg)
/* setup special methods for Linkage Disequilibrium track */
{
if(tg->subtracks != 0) /* Only load subtracks, not top level track. */
    return;
tg->loadItems      = ldLoadItems;
tg->totalHeight    = ldTotalHeight;
tg->drawItems      = ldDrawItems;
tg->freeItems      = ldFreeItems;
tg->drawLeftLabels = ldDrawLeftLabels;
tg->mapsSelf       = TRUE;
tg->canPack        = FALSE;
}

void cnpSharpLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpSharp", (ItemLoader)cnpSharpLoad);
}

void cnpIafrateLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpIafrate", (ItemLoader)cnpIafrateLoad);
}

void cnpSebatLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpSebat", (ItemLoader)cnpSebatLoad);
}

void cnpFosmidLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpFosmid", (ItemLoader)bedLoad);
}

void cnpSharpFreeItems(struct track *tg)
{
cnpSharpFreeList((struct cnpSharp**)&tg->items);
}

void cnpIafrateFreeItems(struct track *tg)
{
cnpIafrateFreeList((struct cnpIafrate**)&tg->items);
}

void cnpSebatFreeItems(struct track *tg)
{
cnpSebatFreeList((struct cnpSebat**)&tg->items);
}

void cnpFosmidFreeItems(struct track *tg)
{
bedFreeList((struct bed**)&tg->items);
}

Color cnpSharpItemColor(struct track *tg, void *item, struct vGfx *vg)
{
struct cnpSharp *cnpSh = item;

if (sameString(cnpSh->variationType, "Gain"))
    return MG_GREEN;
if (sameString(cnpSh->variationType, "Loss"))
    return MG_RED;
if (sameString(cnpSh->variationType, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

Color cnpIafrateItemColor(struct track *tg, void *item, struct vGfx *vg)
{
struct cnpIafrate *cnpIa = item;

if (sameString(cnpIa->variationType, "Gain"))
    return MG_GREEN;
if (sameString(cnpIa->variationType, "Loss"))
    return MG_RED;
if (sameString(cnpIa->variationType, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

Color cnpSebatItemColor(struct track *tg, void *item, struct vGfx *vg)
{
struct cnpSebat *cnpSe = item;

if (sameString(cnpSe->name, "Gain"))
    return MG_GREEN;
if (sameString(cnpSe->name, "Loss"))
    return MG_RED;
if (sameString(cnpSe->name, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

Color cnpFosmidItemColor(struct track *tg, void *item, struct vGfx *vg)
{
struct bed *cnpFo = item;

if (cnpFo->name[0] == 'I')
    return MG_GREEN;
if (cnpFo->name[0] == 'D')
    return MG_RED;
return MG_BLACK;
}

Color delConradItemColor (struct track *tg, void *item, struct vGfx *vg)
{
return MG_RED;
}

Color delMccarrollItemColor (struct track *tg, void *item, struct vGfx *vg)
{
return MG_RED;
}

Color delHindsItemColor (struct track *tg, void *item, struct vGfx *vg)
{
return MG_RED;
}


void cnpSharpMethods(struct track *tg)
{
tg->loadItems = cnpSharpLoadItems;
tg->freeItems = cnpSharpFreeItems;
tg->itemColor = cnpSharpItemColor;
tg->itemNameColor = cnpSharpItemColor;
}

void cnpIafrateMethods(struct track *tg)
{
tg->loadItems = cnpIafrateLoadItems;
tg->freeItems = cnpIafrateFreeItems;
tg->itemColor = cnpIafrateItemColor;
tg->itemNameColor = cnpIafrateItemColor;
}

void cnpSebatMethods(struct track *tg)
{
tg->loadItems = cnpSebatLoadItems;
tg->freeItems = cnpSebatFreeItems;
tg->itemColor = cnpSebatItemColor;
tg->itemNameColor = cnpSebatItemColor;
}

void cnpFosmidMethods(struct track *tg)
{
tg->loadItems = cnpFosmidLoadItems;
tg->freeItems = cnpFosmidFreeItems;
tg->itemColor = cnpFosmidItemColor;
tg->itemNameColor = cnpFosmidItemColor;
}

void delConradMethods(struct track *tg)
{
tg->itemColor = delConradItemColor;
tg->itemNameColor = delConradItemColor;
}

void delMccarrollMethods(struct track *tg)
{
tg->itemColor = delMccarrollItemColor;
tg->itemNameColor = delMccarrollItemColor;
}

void delHindsMethods(struct track *tg)
{
tg->itemColor = delHindsItemColor;
tg->itemNameColor = delHindsItemColor;
}
