/* axtToBed - Convert axt alignments to simple bed format. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: axtToBed.c,v 1.4 2003/05/06 07:22:27 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToBed - Convert axt alignments to simple bed format\n"
  "usage:\n"
  "   axtToBed in.axt out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtToBed(char *inName, char *outName)
/* axtToBed - Convert axt alignments to simple bed format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *row[8], *line;
int s, e;

for (;;)
    {
    if (!lineFileRow(lf, row))
        break;
    s = lineFileNeedNum(lf, row, 2) - 1;
    e = lineFileNeedNum(lf, row, 3);
    fprintf(f, "%s\t%d\t%d\n", row[1], s, e);
    lineFileSkip(lf, 3);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
axtToBed(argv[1],argv[2]);
return 0;
}
