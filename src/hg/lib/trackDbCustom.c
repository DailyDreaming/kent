/* trackDbCustom - custom (not autoSQL generated) code for working
 * with trackDb. */

#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "hui.h"
#include "ra.h"
#include "hash.h"

/* ----------- End of AutoSQL generated code --------------------- */

int trackDbCmp(const void *va, const void *vb);
/* Sort track by priority. */

struct trackDb *trackDbFromRa(char *raFile);
/* Load track info from ra file into list. */

void trackDbAddInfo(struct trackDb *bt, 
	char *var, char *value, struct lineFile *lf);
/* Add info from a variable/value pair to browser table. */

void trackDbPolish(struct trackDb *bt);
/* Fill in missing values with defaults. */



int trackDbCmp(const void *va, const void *vb)
/* Compare to sort based on priority. */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return 0;
else
   return 1;
}

struct trackDb *trackDbFromRa(char *raFile)
/* Load track info from ra file into list. */
{
struct lineFile *lf = lineFileOpen(raFile, TRUE);
char *line, *word;
struct trackDb *btList = NULL, *bt;
boolean done = FALSE;

for (;;)
    {
    /* Seek to next line that starts with 'track' */
    for (;;)
	{
	if (!lineFileNext(lf, &line, NULL))
	   {
	   done = TRUE;
	   break;
	   }
	if (startsWith("track", line))
	   {
	   lineFileReuse(lf);
	   break;
	   }
	}
    if (done)
        break;

    /* Allocate track structure and fill it in until next blank line. */
    AllocVar(bt);
    bt->canPack = 2;	/* Unknown value */
    slAddHead(&btList, bt);
    for (;;)
        {
	/* Break at blank line or EOF. */
	if (!lineFileNext(lf, &line, NULL))
	    break;
	line = skipLeadingSpaces(line);
	if (line == NULL || line[0] == 0)
	    break;

	/* Skip comments. */
	if (line[0] == '#')
	    continue;

	/* Parse out first word and decide what to do. */
	word = nextWord(&line);
	if (line == NULL)
	    errAbort("No value for %s line %d of %s", word, lf->lineIx, lf->fileName);
	line = trimSpaces(line);
	trackDbAddInfo(bt, word, line, lf);
	}
    }
lineFileClose(&lf);
for (bt = btList; bt != NULL; bt = bt->next)
    {
    trackDbPolish(bt);
    }
slReverse(&btList);
return btList;
}

static void parseColor(struct lineFile *lf, char *text, 
	unsigned char *r, unsigned char *g, unsigned char *b)
/* Turn comma-separated string of three numbers into three 
 * color components. */
{
char *words[4];
int wordCount;
wordCount = chopString(text, ", \t", words, ArraySize(words));
if (wordCount != 3)
    errAbort("Expecting 3 comma separated values line %d of %s",
    		lf->lineIx, lf->fileName);
*r = atoi(words[0]);
*g = atoi(words[1]);
*b = atoi(words[2]);
}

static unsigned char parseVisibility(char *value, struct lineFile *lf)
/* Parse a visibility value */
{
if (sameString(value, "hide") || sameString(value, "0"))
    return tvHide;
else if (sameString(value, "dense") || sameString(value, "1"))
    return tvDense;
else if (sameString(value, "full") || sameString(value, "2"))
    return tvFull;
else if (sameString(value, "pack") || sameString(value, "3"))
    return tvPack;
else if (sameString(value, "squish") || sameString(value, "4"))
    return tvSquish;
else
    errAbort("Unknown visibility %s line %d of %s", 
             value, lf->lineIx, lf->fileName);
return tvHide;  /* never reached */
}

void trackDbAddInfo(struct trackDb *bt, 
	char *var, char *value, struct lineFile *lf)
/* Add info from a variable/value pair to browser table. */
{
if (sameString(var, "track"))
    {
    bt->tableName = cloneString(value);
    }
else if (sameString(var, "shortLabel") || sameString(var, "name"))
    bt->shortLabel = cloneString(value);
else if (sameString(var, "longLabel") || sameString(var, "description"))
    bt->longLabel = cloneString(value);
else if (sameString(var, "priority"))
    bt->priority = atof(value);
else if (sameWord(var, "url"))
    bt->url = cloneString(value);
else if (sameString(var, "visibility"))
    {
    bt->visibility =  parseVisibility(value, lf);
    }
else if (sameWord(var, "color"))
    {
    parseColor(lf, value, &bt->colorR, &bt->colorG, &bt->colorB);
    }
else if (sameWord(var, "altColor"))
    {
    parseColor(lf, value, &bt->altColorR, &bt->altColorG, &bt->altColorB);
    }
else if (sameWord(var, "type"))
    {
    bt->type = cloneString(value);
    }
else if (sameWord(var, "spectrum") || sameWord(var, "useScore"))
    {
    bt->useScore = TRUE;
    }
else if (sameWord(var, "canPack"))
    {
    bt->canPack = !(sameString(value, "0") || sameString(value, "off"));
    }
else if (sameWord(var, "chromosomes"))
    sqlStringDynamicArray(value, &bt->restrictList, &bt->restrictCount);
else if (sameWord(var, "private"))
    bt->private = TRUE;
else if (sameWord(var, "group"))
    {
    bt->grp = cloneString(value);
    }
}

static boolean packableType(char *type)
/* Return TRUE if we can pack this type. */
{
char *t = cloneString(type);
char *s = firstWordInLine(t);
boolean canPack = (sameString("psl", s) || sameString("chain", s) 
    || sameString("bed", s) || sameString("genePred", s));
freeMem(t);
return canPack;
}

void trackDbPolish(struct trackDb *bt)
/* Fill in missing values with defaults. */
{
if (bt->shortLabel == NULL)
    bt->shortLabel = cloneString(bt->tableName);
if (bt->longLabel == NULL)
    bt->longLabel = cloneString(bt->shortLabel);
if (bt->altColorR == 0 && bt->altColorG == 0 && bt->altColorB == 0)
    {
    bt->altColorR = (255+bt->colorR)/2;
    bt->altColorG = (255+bt->colorG)/2;
    bt->altColorB = (255+bt->colorB)/2;
    }
if (bt->type == NULL)
    bt->type = cloneString("");
if (bt->priority == 0)
    bt->priority = 100.0;
if (bt->url == NULL)
    bt->url = cloneString("");
if (bt->html == NULL)
    bt->html = cloneString("");
if (bt->grp == NULL)
    bt->grp = cloneString("x");
if (bt->canPack == 2)
    bt->canPack = packableType(bt->type);
}

void trackDbOverrideVisbility(struct hash *tdHash, char *visibilityRa)
/* override visbility settings using a ra file */
{
struct hashEl *hel;
struct hashCookie cookie;
struct lineFile *lf;
struct hash *raRecord;

/* Set visibility to hide on all entries */
cookie = hashFirst(tdHash);
while ((hel = hashNext(&cookie)) != NULL)
    ((struct trackDb *)hel->val)->visibility = tvHide;

/* Parse the ra file, adjusting visibility accordingly */
lf = lineFileOpen(visibilityRa, TRUE);
while ((raRecord = raNextRecord(lf)) != NULL)
    {
    char *trackName = hashFindVal(raRecord, "track");
    char *visibility = hashFindVal(raRecord, "visibility");
    if ((trackName != NULL) && (visibility != NULL))
        {
        struct trackDb *td = hashFindVal(tdHash, trackName);
        if (td != NULL)
            td->visibility = parseVisibility(visibility, lf);
        }
    hashFree(&raRecord);
    }
lineFileClose(&lf);
}

