#include "common.h"
#include "trackDb.h"
#include "cart.h"
#include "hgTables.h"
#include "pal.h"

static char const rcsid[] = "$Id: pal.c,v 1.12 2008/09/25 22:23:55 braney Exp $";

boolean isPalCompatible(struct sqlConnection *conn,
    struct trackDb *track, char *table)
/* Return TRUE if table is genePred and there is a maf. */
{
char setting[128], *p = setting;

if (track == NULL)
    return FALSE;
if (isEmpty(track->type))
    return FALSE;
safecpy(setting, sizeof setting, track->type);
char *type = nextWord(&p);

if (sameString(type, "genePred"))
    if (!sameString(track->tableName, table))
	return FALSE;

/* we also check for a maf table */
struct slName *list = hTrackTablesOfType(conn, "wigMaf%%");

if (list != NULL)
    {
    slFreeList(&list);
    return TRUE;
    }

return FALSE;
}

void doGenePredPal(struct sqlConnection *conn)
/* Output genePred protein alignment. */
{
if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
    {
    sendParamsToGalaxy(hgtaDoPalOut, "submit");
    return;
    }

/* get rid of pesky cookies that would bring us back here */
cartRemove(cart, hgtaDoPal);
cartRemove(cart, hgtaDoPalOut);
cartSaveSession(cart);

struct lm *lm = lmInit(64*1024);
int fieldCount;
struct bed *bed, *bedList = cookedBedsOnRegions(conn, curTable, getRegions(),
	lm, &fieldCount);

/* Make hash of all id's passing filters. */
struct hash *hash = newHash(18);
for (bed = bedList; bed != NULL; bed = bed->next)
    hashAdd(hash, bed->name, NULL);

//lmCleanup(&lm);

textOpen();

int outCount = palOutPredsInHash(conn, cart, hash, curTable);

/* Do some error diagnostics for user. */
if (outCount == 0)
    explainWhyNoResults(NULL);
hashFree(&hash);
}

void addOurButtons()
/* callback from options dialog to add navigation buttons */
{
printf("For information about output data format see "
  "<A HREF=\"../goldenPath/help/hgTablesHelp.html#FASTA\">Table Browser User's Guide</A><BR>");
cgiMakeButton(hgtaDoPalOut, "get output");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("<input type=\"hidden\" name=\"%s\" value=\"\">\n", hgtaDoPal);
}

void doOutPalOptions(struct sqlConnection *conn)
/* Output pal page. */
{
htmlOpen("Select multiple alignment and options for %s", curTrack->shortLabel);
palOptions(cart, conn, addOurButtons, hgtaDoPal);
htmlClose();
}
