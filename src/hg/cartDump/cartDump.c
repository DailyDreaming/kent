/* cartDump - Dump contents of cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"

static char const rcsid[] = "$Id: cartDump.c,v 1.4 2003/05/06 07:22:14 kate Exp $";

void doMiddle(struct cart *cart)
/* cartDump - Dump contents of cart. */
{
printf("<TT><PRE>");
cartDump(cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartHtmlShell("Cart Dump", doMiddle, "hguid", NULL, NULL);
return 0;
}
