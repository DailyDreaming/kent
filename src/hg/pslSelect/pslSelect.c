/* pslSelect - select records from a PSL file  */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "localmem.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslSelect.c,v 1.3 2004/05/05 22:39:52 kate Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"qtPairs", OPTION_STRING},
    {"queries", OPTION_STRING},
    {"queryPairs", OPTION_STRING},
    {NULL, 0}
};

#define QT_PAIRS_MODE   1
#define QUERY_MODE      2
#define QUERY_PAIRS_MODE 3

static int mode = 0;
static int isPairs = TRUE;

/* global data from command line */
static char *selectFile;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslSelect - select records from a PSL file.\n"
  "\n"
  "usage:\n"
  "   pslSelect [options] inPsl outPsl\n"
  "\n"
  "Must specify a selection option\n"
  "\n"
  "Options:\n"
  "   -qtPairs=file - file is tab-separated query and query to select\n"
  "   -query=file - file is list of query to select\n"
  "   -queryPairs=file - file is list of queries, with substitution value\n"
  );
}

struct hash *loadSelect(char *selectFile)
/* load select file. */
{
struct hash *hash = hashNew(20);
char *row[2];
struct lineFile *lf = lineFileOpen(selectFile, TRUE);
int wordCount = isPairs ? 2 : 1;
while (lineFileNextRowTab(lf, row, wordCount))
    {
    char *value = isPairs ? row[1] : "";
    hashAdd(hash, row[0], lmCloneString(hash->lm, value));
    }
lineFileClose(&lf);
return hash;
}

struct hashEl *selectedItem(struct hash* selectHash, char *qName, char *tName)
/* determine if the item is selected.  Handle the query
 * being paired to multiple query */
{
struct hashEl *hel = hashLookup(selectHash, qName);
while (hel != NULL)
    {
    char *target = hel->val;
    if (mode == QUERY_MODE || mode == QUERY_PAIRS_MODE)
        return hel;
    if (mode == QT_PAIRS_MODE && sameString(target, tName))
        return hel;
    hel = hashLookupNext(hel);
    }
return NULL;
}

void pslSelect(char *inPsl, char *outPsl)
/* select psl */
{
struct hash *selectHash = loadSelect(selectFile);
struct lineFile *inPslLf = pslFileOpen(inPsl);
FILE *outPslFh = mustOpen(outPsl, "w");
struct psl* psl;
struct hashEl *hel;

while ((psl = pslNext(inPslLf)) != NULL)
    {
    if ((hel = selectedItem(selectHash, psl->qName, psl->tName)) != NULL)
        {
        if (mode == QUERY_PAIRS_MODE)
            {
            freeMem(psl->qName);
            psl->qName = cloneString(hel->val);
            }
        pslTabOut(psl, outPslFh);
        }
    pslFree(&psl);
    }

carefulClose(&outPslFh);
lineFileClose(&inPslLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
if ((selectFile = optionVal("qtPairs", NULL)) != NULL)
    mode = QT_PAIRS_MODE;
else if ((selectFile = optionVal("queries", NULL)) != NULL)
    {
    mode = QUERY_MODE;
    isPairs = FALSE;
    }
else if ((selectFile = optionVal("queryPairs", NULL)) != NULL)
    mode = QUERY_PAIRS_MODE;
else
    errAbort("must specify option");

pslSelect(argv[1], argv[2]);

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

