/* axtSort - Sort axt files. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"

static char const rcsid[] = "$Id: axtSort.c,v 1.5 2003/05/06 07:22:27 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtSort - Sort axt files\n"
  "usage:\n"
  "   axtSort in.axt out.axt\n"
  "options:\n"
  "   -query - Sort by query position, not target\n"
  );
}

void axtSort(char *in, char *out, boolean byQuery)
/* axtSort - Sort axt files. */
{
struct axt *axtList = NULL, *axt;
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f;

while ((axt = axtRead(lf)) != NULL)
    {
    slAddHead(&axtList, axt);
    }
if (byQuery)
    slSort(&axtList, axtCmpQuery);
else
    slSort(&axtList, axtCmpTarget);
lineFileClose(&lf);
f = mustOpen(out, "w");
for (axt = axtList; axt != NULL; axt = axt->next)
    axtWrite(axt, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
axtSort(argv[1], argv[2], optionExists("query"));
return 0;
}
