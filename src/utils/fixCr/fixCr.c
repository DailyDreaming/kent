#include "common.h"
#include "linefile.h"

static char const rcsid[] = "$Id: fixCr.c,v 1.2 2003/05/06 07:41:06 kate Exp $";

int main(int argc, char *argv[])
/* Fix carraige returns. */
{
char dir[256], name[128], extension[64];
char bakName[512];
char *fileName;
int i;
FILE *out;
struct lineFile *lf;
char *line;
char *end;
int lineSize;


if (argc<2)
    errAbort("fixCr - strip <CR>s from ends of lines");
for (i=1; i<argc; ++i)
    {
    fileName = argv[i];
    printf("%s ", fileName);
    fflush(stdout);
    splitPath(fileName, dir, name, extension);
    sprintf(bakName, "%s%s%s", dir, name, ".bak");
    remove(bakName);
    rename(fileName, bakName);
    lf = lineFileOpen(bakName, FALSE);
    out = mustOpen(fileName, "w");
    while (lineFileNext(lf, &line, &lineSize))
	{
	if (lineSize > 1)
	    {
	    end = line + lineSize - 2;
	    if (*end == '\r')
		{
		*end = '\n';
		lineSize -= 1;
		}
	    }
	mustWrite(out, line, lineSize);
	}
    fclose(out);
    lineFileClose(&lf);
    }
printf("\n");
}
