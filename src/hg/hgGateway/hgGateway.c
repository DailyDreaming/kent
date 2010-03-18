/* hgGateway - Human Genome Browser Gateway. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "obscure.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "dbDb.h"
#include "hgFind.h"
#include "hCommon.h"
#include "hui.h"
#include "customTrack.h"
#include "hgConfig.h"
#include "jsHelper.h"
#include "hPrint.h"
#include "suggest.h"

static char const rcsid[] = "$Id: hgGateway.c,v 1.114.4.1 2010/03/18 23:28:55 galt Exp $";

boolean isPrivateHost;		/* True if we're on genome-test. */
struct cart *cart = NULL;
struct hash *oldVars = NULL;
char *clade = NULL;
char *organism = NULL;
char *db = NULL;

void hgGateway()
/* hgGateway - Human Genome Browser Gateway. */
{
char *defaultPosition = hDefaultPos(db);
char *position = cloneString(cartUsualString(cart, "position", defaultPosition));
boolean gotClade = hGotClade();
char *survey = cfgOptionEnv("HGDB_SURVEY", "survey");
char *surveyLabel = cfgOptionEnv("HGDB_SURVEY_LABEL", "surveyLabel");
boolean supportsSuggest = assemblySupportsGeneSuggest(db);

/* JavaScript to copy input data on the change genome button to a hidden form
This was done in order to be able to flexibly arrange the UI HTML
*/
char *onChangeDB = "onchange=\"document.orgForm.db.value = document.mainForm.db.options[document.mainForm.db.selectedIndex].value; document.orgForm.submit();\"";
char *onChangeOrg = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.db.value = 0; document.orgForm.submit();\"";
char *onChangeClade = "onchange=\"document.orgForm.clade.value = document.mainForm.clade.options[document.mainForm.clade.selectedIndex].value; document.orgForm.org.value = 0; document.orgForm.db.value = 0; document.orgForm.submit();\"";

/* 
   If we are changing databases via explicit cgi request,
   then remove custom track data which will 
   be irrelevant in this new database .
   If databases were changed then use the new default position too.
*/

if (sameString(position, "genome") || sameString(position, "hgBatch"))
    position = defaultPosition;

hPrintf("<link href='../style/autocomplete.css' rel='stylesheet' type='text/css' />\n");
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("jquery.autocomplete.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("autocomplete.js", NULL);
jsIncludeFile("hgGateway.js", NULL);
jsIncludeFile("utils.js", NULL);

puts(
"<CENTER>"
"<TABLE BGCOLOR=\"FFFEF3\" BORDERCOLOR=\"cccc99\" BORDER=0 CELLPADDING=1>\n"
"<TR><TD>\n"
"<CENTER><FONT SIZE=\"2\">\n"
"The UCSC Genome Browser was created by the \n"
"<A HREF=\"../staff.html\">Genome Bioinformatics Group of UC Santa Cruz</A>.\n"
"<BR>"
"Software Copyright (c) The Regents of the University of California.\n"
"All rights reserved.\n"
"</FONT></CENTER>\n"
"</TD></TR></TABLE></CENTER>\n"
);

puts(
"<center>\n"
"<table bgcolor=\"cccc99\" border=\"0\" CELLPADDING=1 CELLSPACING=0>\n"
"<tr><td>\n"
"<table BGCOLOR=\"FEFDEF\" BORDERCOLOR=\"CCCC99\" BORDER=0 CELLPADDING=0 CELLSPACING=0>\n"  
"<tr><td>\n"
"<table bgcolor=\"fffef3\" border=0>\n"
"<tr>\n"
"<td>\n");

puts(
"<FORM ACTION=\"../cgi-bin/hgTracks\" NAME=\"mainForm\" METHOD=\"GET\">\n"
"<input TYPE=\"IMAGE\" BORDER=\"0\" NAME=\"hgt.dummyEnterButton\" src=\"../images/DOT.gif\" WIDTH=1 HEIGHT=1 ALT=dot>\n");
cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */
puts("<table><tr>");
if (gotClade)
    puts("<td align=center valign=baseline>clade</td>");
puts(
"<td align=center valign=baseline>genome</td>\n"
"<td align=center valign=baseline>assembly</td>\n"
"<td align=center valign=baseline>position or search term</td>\n");
if(supportsSuggest)
    puts("<td align=center valign=baseline>gene</td>\n");
puts(
"<td align=center valign=baseline>image width</td>\n"
"<td align=center valign=baseline> &nbsp; </td>\n"
"</tr>\n<tr>"
);

if (gotClade)
    {
    puts("<td align=center>\n");
    printCladeListHtml(organism, onChangeClade);
    puts("</td>\n");
    }

puts("<td align=center>\n");
if (gotClade)
    printGenomeListForCladeHtml(db, onChangeOrg);
else
    printGenomeListHtml(db, onChangeOrg);
puts("</td>\n");

puts("<td align=center>\n");
printAssemblyListHtml(db, onChangeDB);
puts("</td>\n");

puts("<td align=center>\n");
cgiMakeTextVar("position", addCommasToPos(db, position), 30);
printf("</td>\n");

if(supportsSuggest)
    {
    puts("<td align=center>\n");
    hWrites("<input type='text' size='5' id='suggest' />\n");
    printf("</td>\n");
    }

cartSetString(cart, "position", position);
cartSetString(cart, "db", db);
cartSetString(cart, "org", organism);
if (gotClade)
    cartSetString(cart, "clade", clade);

freez(&defaultPosition);
position = NULL;

puts("<td align=center>\n");
cgiMakeIntVar("pix", cartUsualInt(cart, "pix", hgDefaultPixWidth), 4);
puts("</td>\n");
puts("<td align=center>");
cgiMakeButton("Submit", "submit");
puts(
"</td>\n"
"</tr></table>\n"
"</FORM></td></tr>\n");

puts(
"<tr><td><center>\n"
"<a HREF=\"../cgi-bin/cartReset\">Click here to reset</a> the browser user interface settings to their defaults.");

#define SURVEY 1
#ifdef SURVEY
if (survey && differentWord(survey, "off"))
    printf("&nbsp;&nbsp;&nbsp;<FONT STYLE=\"background-color:yellow;\"><A HREF=\"%s\" TARGET=_BLANK><EM><B>%s</EM></B></A></FONT>", survey, surveyLabel ? surveyLabel : "Take survey");
#endif

puts(
"<BR>\n"
"</center>\n"
"</td></tr><tr><td><center>\n"
);

puts("<TABLE BORDER=\"0\">");
puts("<TR>");

// custom track button. disable hgCustom button on GSID server, until
// necessary additional work is authorized.
puts("<TD VALIGN=\"TOP\">");

/* disable CT for CGB servers for the time being */
if (!hIsGsidServer() && !hIsCgbServer())
    {
    printf(
	"<FORM ACTION=\"%s\" METHOD=\"GET\"><INPUT TYPE=SUBMIT VALUE=\"%s\">",
        hgCustomName(), customTracksExist(cart, NULL) ? 
                        CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
    cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */
    puts("</FORM>");
    }
puts("</TD>");

// configure button
puts("<TD VALIGN=\"TOP\">");
puts("<FORM ACTION=\"../cgi-bin/hgTracks\" NAME=\"buttonForm\" METHOD=\"GET\">\n");
cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */
cgiMakeButton("hgTracksConfigPage", "configure tracks and display");
puts("</FORM></TD>");

// clear possition button
puts("<TD VALIGN=\"TOP\">");
puts("<FORM ACTION=\"../cgi-bin/hgTracks\" NAME=\"buttonForm\" METHOD=\"GET\">\n");
cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */
cgiMakeOnClickButton("document.mainForm.position.value=''","clear position");
puts("</FORM></TD>");

puts("</TR>");
puts("</TABLE>");

puts("</center>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
);
puts("</center>");
if (isPrivateHost)
puts("<P>This is just our test site.  It usually works, but it is filled with tracks in various "
"stages of construction, and others of little interest to people outside of our local group. "
"It is usually slow because we are building databases on it. The documentation is poor. "
 "More data than usual is flat out wrong.  Maybe you want to go to "
	 "<A HREF=\"http://genome.ucsc.edu\">genome.ucsc.edu</A> instead.");

if (hIsGsidServer())
    {
    webNewSection("%s", "Sequence View\n");
    printf("%s", 
	   "Sequence View is a customized version of the UCSC Genome Browser, which is specifically tailored to provide functions needed for the GSID HIV Data Browser.\n");
    }

hgPositionsHelpHtml(organism, db);

puts("<FORM ACTION=\"../cgi-bin/hgGateway\" METHOD=\"GET\" NAME=\"orgForm\">");
cartSaveSession(cart);	/* Put up hgsid= as hidden variable. */
if (gotClade)
    printf("<input type=\"hidden\" name=\"clade\" value=\"%s\">\n", clade);
else
    printf("<input type=\"hidden\" name=\"clade\" value=\"%s\">\n", "mammal");

printf("<input type=\"hidden\" name=\"org\" value=\"%s\">\n", organism);
printf("<input type=\"hidden\" name=\"db\" value=\"%s\">\n", db);
puts("</FORM><BR>");
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
char *scientificName = NULL;
cart = theCart;

getDbGenomeClade(cart, &db, &organism, &clade, oldVars);
if (! hDbIsActive(db))
    {
    db = hDefaultDb();
    organism = hGenome(db);
    clade = hClade(organism);
    }
scientificName = hScientificName(db);
if (hIsGsidServer())
    cartWebStart(theCart, db, "GSID %s Sequence View (UCSC Genome Browser) Gateway \n", organism);
else
    {
    char buffer[128];
    char *browserName = (isPrivateHost ? "TEST Genome Browser" : "Genome Browser");

    /* tell html routines *not* to escape htmlOut strings*/
    htmlNoEscape();
    buffer[0] = 0;
    if (*scientificName != 0)
	{
	if (sameString(clade,"ancestor"))
	    safef(buffer, sizeof(buffer), "(<I>%s</I> Ancestor) ", scientificName);
	else
	    safef(buffer, sizeof(buffer), "(<I>%s</I>) ", scientificName);
	}
    cartWebStart(theCart, db, "%s %s%s Gateway\n", organism, buffer, browserName);
    htmlDoEscape();
    }
hgGateway();
cartWebEnd();
}

char *excludeVars[] = {NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
isPrivateHost = hIsPrivateHost();
oldVars = hashNew(10);
cgiSpoof(&argc, argv);

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
