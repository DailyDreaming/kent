/* hgsql - Execute some sql code using passwords in .hg.conf. */
#include "common.h"
#include "dystring.h"
#include "options.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: hgsql.c,v 1.5 2004/02/24 18:47:09 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsql - Execute some sql code using passwords in .hg.conf\n"
  "usage:\n"
  "   hgsql database\n"
  "or:\n"
  "   hgsql database < file.sql\n"
  "or:\n"
  "   hgsql -h host database\n"
  "Generally anything in command line is passed to mysql\n"
  "after an implicit '-A -u user -ppassword"
  );
}

boolean stringHasSpace(char *s)
/* Return TRUE if white space in string */
{
char c;
while ((c = *s++) != 0)
    {
    if (isspace(c))
        return TRUE;
    }
return FALSE;
}

void hgsql(int argc, char *argv[])
/* hgsql - Execute some sql code using passwords in .hg.conf. */
{
int i;
struct dyString *command = newDyString(1024);
char *password = cfgOption("db.password");
char *user = cfgOption("db.user");
char *host = cfgOption("db.host");
dyStringPrintf(command, "mysql -A -u %s -p%s -h%s", user, password, host);
for (i=0; i<argc; ++i)
    {
    boolean hasSpace = stringHasSpace(argv[i]);
    dyStringAppendC(command, ' ');
    if (hasSpace)
	dyStringAppendC(command, '\'');
    dyStringAppend(command, argv[i]);
    if (hasSpace)
	dyStringAppendC(command, '\'');
    }
system(command->string);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc <= 1)
    usage();
hgsql(argc-1, argv+1);
return 0;
}
