/* identifiers - handle identifier lists: uploading, pasting,
 * and restricting to just things on the list. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "portable.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: identifiers.c,v 1.1 2004/07/14 19:02:50 kent Exp $";


void doPasteIdentifiers(struct sqlConnection *conn)
/* Respond to paste identifiers button. */
{
htmlOpen("Paste In Identifiers for %s", curTrack->shortLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=POST>\n");
cartSaveSession(cart);
hPrintf("Please paste in the identifiers you want to include.<BR>\n");
cgiMakeTextArea(hgtaPastedIdentifiers, "", 10, 70);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoPastedIdentifiers, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>");
htmlClose();
}

void doUploadIdentifiers(struct sqlConnection *conn)
/* Respond to upload identifiers button. */
{
htmlOpen("Upload Identifiers for %s", curTrack->shortLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=POST ENCTYPE=\"multipart/form-data\">\n");
cartSaveSession(cart);
hPrintf("Please enter the name of a file from your computer that contains a ");
hPrintf("space, tab, or ");
hPrintf("line separated list of the items you want to include.<BR>");
hPrintf("<INPUT TYPE=FILE NAME=\"%s\"> ", hgtaPastedIdentifiers);
hPrintf("<BR>\n");
cgiMakeButton(hgtaDoPastedIdentifiers, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>");
htmlClose();
}

void doPastedIdentifiers(struct sqlConnection *conn)
/* Process submit in past identifiers page. */
{
char *idText = trimSpaces(cartString(cart, hgtaPastedIdentifiers));
htmlOpen("Table Browser (Input Identifiers)");
if (idText != NULL && idText[0] != 0)
    {
    /* Write variable to temp file and save temp
     * file name. */
    struct tempName tn;
    FILE *f;
    makeTempName(&tn, "tables", ".key");
    f = mustOpen(tn.forCgi, "w");
    mustWrite(f, idText, strlen(idText));
    carefulClose(&f);
    cartSetString(cart, hgtaIdentifierFile, tn.forCgi);
    }
else
    {
    cartRemove(cart, hgtaIdentifierFile);
    }
cartRemove(cart, hgtaPastedIdentifiers);
mainPageAfterOpen(conn);
htmlClose();
}


char *identifierFileName()
/* File name identifiers are in, or NULL if no such file. */
{
char *fileName = cartOptionalString(cart, hgtaIdentifierFile);
if (fileName == NULL)
    return NULL;
if (fileExists(fileName))
    return fileName;
else
    {
    cartRemove(cart, hgtaIdentifierFile);
    return NULL;
    }
}

struct hash *identifierHash()
/* Return hash full of identifiers. */
{
char *fileName = identifierFileName();
if (fileName == NULL)
    return NULL;
else
    {
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct hash *hash = hashNew(18);
    char *line, *word;
    while (lineFileNext(lf, &line, NULL))
        {
	while ((word = nextWord(&line)) != NULL)
	    hashAdd(hash, word, NULL);
	}
    return hash;
    }
}

void doClearIdentifiers(struct sqlConnection *conn)
/* Respond to clear identifiers button. */
{
char *fileName;

htmlOpen("Table Browser (Cleared Identifiers)");
fileName = identifierFileName();
if (fileName != NULL)
    remove(fileName);
cartRemove(cart, hgtaIdentifierFile);
mainPageAfterOpen(conn);
htmlClose();
}


