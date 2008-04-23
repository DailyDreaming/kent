/* pcrResult -- support for internal track of hgPcr results. */

#include "common.h"
#include "hdb.h"
#include "hui.h"
#include "obscure.h"
#include "targetDb.h"
#include "pcrResult.h"

static char const rcsid[] = "$Id: pcrResult.c,v 1.5 2008/04/23 18:31:33 angie Exp $";

char *pcrResultCartVar(char *db)
/* Returns the cart variable name for PCR result track info for db. 
 * Don't free the result! */
{
static char buf[1024];
safef(buf, sizeof(buf), "hgPcrResult_%s", db);
return buf;
}

#define setPtIfNotNull(pt, val) if (pt != NULL) *pt = val

boolean pcrResultParseCart(struct cart *cart, char **retPslFile,
			   char **retPrimerFile,
			   struct targetDb **retTarget)
/* Parse out hgPcrResult cart variable into components and make sure
 * they are valid.  If so, set *ret's and return TRUE.  Otherwise, null out 
 * *ret's and return FALSE (and clear the cart variable if it exists).  
 * ret's are ignored if NULL. */
{
char *cartVar = pcrResultCartVar(cartString(cart, "db"));
char *hgPcrResult = cartOptionalString(cart, cartVar);
if (isEmpty(hgPcrResult))
    {
    setPtIfNotNull(retPslFile, NULL);
    setPtIfNotNull(retPrimerFile, NULL);
    setPtIfNotNull(retTarget, NULL);
    return FALSE;
    }
static char buf[2048];
char *words[3];
int wordCount;
safecpy(buf, sizeof(buf), hgPcrResult);
wordCount = chopLine(buf, words);
if (wordCount < 2)
    errAbort("Badly formatted hgPcrResult variable: %s", hgPcrResult);
char *pslFile = words[0];
char *primerFile = words[1];
char *targetName = (wordCount > 2) ? words[2] : NULL;
struct targetDb *target = NULL;
if (isNotEmpty(targetName))
    target = targetDbLookup(hGetDb(), targetName);

if (!fileExists(pslFile) || !fileExists(primerFile) ||
    (wordCount > 2 && target == NULL))
    {
    cartRemove(cart, "hgPcrResult");
    setPtIfNotNull(retPslFile, NULL);
    setPtIfNotNull(retPrimerFile, NULL);
    setPtIfNotNull(retTarget, NULL);
    return FALSE;
    }
setPtIfNotNull(retPslFile, cloneString(pslFile));
setPtIfNotNull(retPrimerFile, cloneString(primerFile));
setPtIfNotNull(retTarget, target);
if (retTarget == NULL)
    targetDbFreeList(&target);
return TRUE;
}

void pcrResultGetPrimers(char *fileName, char **retFPrimer, char **retRPrimer)
/* Given a file whose first line is 2 words (forward primer, reverse primer)
 * set the ret's to upper-cased primer sequences.  
 * Do not free the statically allocated ret's. */
{
static char fPrimer[1024], rPrimer[1024];;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[2];
if (! lineFileRow(lf, words))
    lineFileAbort(lf, "Couldn't read primers");
if (retFPrimer != NULL)
    {
    safecpy(fPrimer, sizeof(fPrimer), words[0]);
    touppers(fPrimer);
    *retFPrimer = fPrimer;
    }
if (retRPrimer != NULL)
    {
    safecpy(rPrimer, sizeof(rPrimer), words[1]);
    touppers(rPrimer);
    *retRPrimer = rPrimer;
    }
lineFileClose(&lf);
}

void pcrResultGetPsl(char *fileName, struct targetDb *target, char *item,
		     char *chrom, int itemStart, int itemEnd,
		     struct psl **retItemPsl, struct psl **retOtherPsls)
/* Read in psl from file.  If a psl matches the given item position, set 
 * retItemPsl to that; otherwise add it to retOtherPsls.  Die if no psl
 * matches the given item position. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct psl *itemPsl = NULL, *otherPsls = NULL;
char *pslFields[21];
while (lineFileRow(lf, pslFields))
    {
    struct psl *psl = pslLoad(pslFields);
    boolean gotIt = FALSE;
    if (target != NULL)
	{
	if (sameString(psl->tName, item))
	    gotIt = TRUE;
	}
    else if (sameString(psl->tName, chrom) && psl->tStart == itemStart &&
	     psl->tEnd == itemEnd)
	gotIt = TRUE;
    if (gotIt)
	itemPsl = psl;
    else
	slAddHead(&otherPsls, psl);
    }
lineFileClose(&lf);
if (itemPsl == NULL)
    {
    if (target != NULL)
	errAbort("Did not find record for amplicon in %s sequence %s",
		 target->description, item);
    else
	errAbort("Did not find record for amplicon at %s:%d-%d",
		 chrom, itemStart, itemEnd);
    }
if (retItemPsl != NULL)
    *retItemPsl = itemPsl;
else
    pslFree(&itemPsl);
if (retOtherPsls != NULL)
    {
    slSort(&otherPsls, pslCmpTarget);
    *retOtherPsls = otherPsls;
    }
else
    pslFreeList(&otherPsls);
}

struct trackDb *pcrResultFakeTdb()
/* Construct a trackDb record for PCR Results track. */
{
struct trackDb *tdb;
char helpName[PATH_LEN], *helpBuf;
safef(helpName, sizeof(helpName), "%s%s/%s.html", hDocumentRoot(), HELP_DIR,
      "hgPcrResult");
readInGulp(helpName, &helpBuf, NULL);
AllocVar(tdb);
tdb->tableName = cloneString("hgPcrResult");
tdb->shortLabel = cloneString("PCR Results");
tdb->longLabel = cloneString("Your Sequence from PCR Search");
tdb->grp = cloneString("map");
tdb->type = cloneString("psl .");
tdb->priority = 100.01;
tdb->canPack = TRUE;
tdb->visibility = tvPack;
tdb->grp = cloneString("map");
tdb->html = helpBuf;
trackDbPolish(tdb);
if (tdb->settingsHash == NULL)
    tdb->settingsHash = hashNew(0);
hashAdd(tdb->settingsHash, "baseColorDefault", cloneString("diffBases"));
hashAdd(tdb->settingsHash, "baseColorUseSequence", cloneString("hgPcrResult"));
hashAdd(tdb->settingsHash, "showDiffBasesAllScales", cloneString("."));
hashAdd(tdb->settingsHash, "indelDoubleInsert", cloneString("on"));
hashAdd(tdb->settingsHash, "indelQueryInsert", cloneString("on"));
hashAdd(tdb->settingsHash, "indelPolyA", cloneString("on"));
hashAdd(tdb->settingsHash, "nextItemButton", cloneString("off"));
return tdb;
}

