/* motifLogo - Make a sequence logo out of a motif.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: motifLogo.c,v 1.2 2003/05/06 07:22:18 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "motifLogo - Make a sequence logo out of a motif.\n"
  "usage:\n"
  "   motifLogo XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void motifLogo(char *XXX)
/* motifLogo - Make a sequence logo out of a motif.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
motifLogo(argv[1]);
return 0;
}
