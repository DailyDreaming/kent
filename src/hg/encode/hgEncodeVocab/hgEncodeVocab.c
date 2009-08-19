/* hgEncodeVocab - print table of controlled vocabulary from ENCODE configuration files */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "htmshell.h"
#include "ra.h"
#include "hui.h"

/* hgEncodeVocab - A CGI script to display the different types of encode controlled vocabulary.
 * usage:
 *   hgEncodeVocab [ra=cv.ra] type=[Antibody|"Cell Line"|localization|rnaExtract|"Gene Type"] [tier=(1|2|3)]
 * options:\n"
 *    ra=cv.ra       : Path to cv.ra file (default cv_file())
 *    type=TypeName  : Type to display
 *    tier=N         : If type="Cell Line" then this is the tier to display
 *    bgcolor=RRGGBB : Change background color (hex digits)
 */

//#define HANDLE_IMPLICIT_CONTROL

static char const rcsid[] = "$Id: hgEncodeVocab.c,v 1.24 2009/08/19 14:33:10 kate Exp $";

static char *cv_file()
{
/* return default location of cv.ra (can specify as cgi var: ra=cv.ra) */

static char filePath[PATH_LEN];
safef(filePath, sizeof(filePath), "%s/encode/cv.ra", hCgiRoot());
if(!fileExists(filePath))
    errAbort("Error: can't locate cv.ra; %s doesn't exist\n", filePath);
return filePath;
}


int termCmp(const void *va, const void *vb)
/* Compare controlled vocab based on term value */
{
const struct hash *a = *((struct hash **)va);
const struct hash *b = *((struct hash **)vb);
char *termA = hashMustFindVal((struct hash *)a, "term");
char *termB = hashMustFindVal((struct hash *)b, "term");
return (strcasecmp(termA, termB));
}

void doTypeHeader(char *type)
{
if (sameString(type,"Antibody"))
    {
    puts("  <TH>Term</TH><TH>Target Description</TH><TH>Antibody Description</TH><TH>Vendor ID</TH>");
    }
else if (sameString(type,"ripAntibody"))
    {
    puts("  <TH>Term</TH><TH>Antibody Description</TH><TH>Target Description</TH><TH>Vendor ID</TH>");
    }
else if (sameString(type,"ripTgtProtein"))
    {
    puts("  <TH>Term</TH><TH>Alternative Symbols</TH><TH>Description</TH>");
    }
else if (sameString(type,"localization"))
    {
    puts("  <TH>Term</TH><TH>Description</TH><TH>GO ID</TH>");
    }
else if (sameString(type,"Cell Line"))
    {
    puts("  <TH>Term</TH><TH>Tier</TH><TH>Description</TH><TH>Lineage</TH><TH>Karyotype</TH><TH>Documents</TH><TH>Vendor ID</TH><TH>Term ID</TH>");
    }
#ifdef HANDLE_IMPLICIT_CONTROL
else if (sameWord(type,"control") || sameWord(type,"input"))
    {
    puts("  <TH>Term</TH><TH>Description</TH></TR><TR>");
    printf("  <Td>%s</Td><Td>This data represents a control being compared with the other tracks in the set.</Td>\n",type);
    }
#endif//def HANDLE_IMPLICIT_CONTROL
else
    puts("  <TH>Term</TH><TH>Description</TH>");
}

void doTypeRow(struct hash *ra, char *type, int *total)
{
char *s, *u;

// Skip all rows that do not match term
char *term = cgiOptionalString("term");
if(term)
    {
    (void)stripChar(term,'\"');
    if(differentWord(term,hashMustFindVal(ra,"term")))
        return;
    }
else
    term = hashMustFindVal(ra,"term");

if (sameString(type,"Antibody"))
    {
    ++(*total);
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashFindVal(ra, "targetDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "antibodyDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");

    s = hashFindVal(ra, "vendorName");
    char *t = hashFindVal(ra, "vendorId");
    char *u = hashFindVal(ra, "orderUrl");
    printf("  <TD>");
    if (u)
        printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
    printf("%s %s", s ? s : "&nbsp;", t ? t : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");

    puts("</TR>");
    }
else if (sameString(type,"ripAntibody"))
    {
    ++(*total);
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashFindVal(ra, "antibodyDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "targetDescription");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "vendorName");
    char *t = hashFindVal(ra, "vendorId");
    char *u = hashFindVal(ra, "orderUrl");
    printf("  <TD>");
    if (u)
        printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
    printf("%s %s", s ? s : "&nbsp;", t ? t : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");

    puts("</TR>");
    }
else if (sameString(type,"ripTgtProtein"))
    {
    ++(*total);
    puts("<TR>");
    s = hashFindVal(ra, "url");
    if (s)
	printf("  <TD><A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=\"%s\">%s</A></TD>\n", s, term);
    else
	printf("  <TD>%s</TD>\n", term);
    s = hashFindVal(ra, "alternativeSymbols");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    s = hashFindVal(ra, "description");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;");
    puts("</TR>");
    }
else if (sameString(type,"localization"))
    {
    ++(*total);
    puts("<TR>");
    printf("  <TD>%s</TD>\n", term);
    s = hashMustFindVal(ra, "description");
    printf("  <TD>%s</TD>\n", s);
    s = hashFindVal(ra, "termId");
    u = hashFindVal(ra, "termUrl");
    printf("  <TD>");
    if (u)
        printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
    printf("%s", s ? s : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");

    puts("</TR>");
    }
else if (sameString(type,"Cell Line"))
    {
    printf("<!-- Cell Line table: contains links to protocol file and vendor description page -->");
    if (cgiOptionalInt("tier",0))
        {
        if (hashFindVal(ra,"tier") == NULL)
            return;
        if (atoi(hashFindVal(ra,"tier"))!=cgiOptionalInt("tier",0))
            return;
        }
    if (cgiOptionalString("tiers"))
        {
        if (hashFindVal(ra,"tier") == NULL)
            return;
        boolean found=FALSE;
        char *tiers=cloneString(cgiOptionalString("tiers"));
        char *tier;
        (void)strSwapChar(tiers,',',' ');
        while((tier=nextWord(&tiers)))
            {
            if (atoi(hashFindVal(ra,"tier"))==atoi(tier))
                {
                found=TRUE;
                break;
                }
            }
        if(!found)
            return;
        }
    ++(*total);
    puts("<TR>");

    printf("  <TD>%s</TD>\n", term);

    s = hashFindVal(ra, "tier");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
    s = hashFindVal(ra, "description");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
    s = hashFindVal(ra, "lineage");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );
    s = hashFindVal(ra, "karyotype");
    printf("  <TD>%s</TD>\n", s ? s : "&nbsp;" );

#define PROTOCOL_DIR "/ENCODE/protocols/cell/"
#define PROTOCOL_TITLE "protocol"
    // add links to protocol doc if it exists
    printf("  <TD>");
    char protocolUrl[PATH_LEN];
    char protocolFile[PATH_LEN];
    // parse Protocol setting
    s = hashFindVal(ra,"protocol");
    if(s != NULL)
        {
        char *protocolSetting = cloneString(s);
        char *settings=protocolSetting;
        while((s = nextWord(&settings)) != NULL)
            {
            char *title = NULL;
            char *fileName = NULL;
            if(strchr(s,':')) // lab Specific setting
                {
                title = strSwapChar(s,':',0);
                fileName = title + strlen(title) + 1;
                }
            else
                {
                title = PROTOCOL_TITLE;
                fileName = s;
                }
            safef(protocolUrl, sizeof(protocolUrl), "%s%s", PROTOCOL_DIR, fileName);
            safef(protocolFile, sizeof(protocolFile), "%s%s", hDocumentRoot(), protocolUrl);
            if (fileExists(protocolFile))
                printf(" <A STYLE=\"text-decoration:none\"HREF=%s TARGET=_BLANK>%s</A>\n", protocolUrl,title);
            }
        freeMem(protocolSetting);
        }
    else
        { // generate a standard name
        safef(protocolUrl, sizeof(protocolUrl), "%s%s_protocol.pdf", PROTOCOL_DIR, term);
        safef(protocolFile, sizeof(protocolFile), "%s%s", hDocumentRoot(), protocolUrl);
        if (fileExists(protocolFile))
            printf(" <A STYLE=\"text-decoration:none\"HREF=%s TARGET=_BLANK>%s</A>\n", protocolUrl,PROTOCOL_TITLE);
        }
    printf("  &nbsp;</TD>\n");

    s = hashFindVal(ra, "vendorName");
    char *t = hashFindVal(ra, "vendorId");
    char *u = hashFindVal(ra, "orderUrl");
    printf("  <TD>");
    if (u)
        printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
    printf("%s %s", s ? s : "&nbsp;", t ? t : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");

    s = hashFindVal(ra, "termId");
    u = hashFindVal(ra, "termUrl");
    printf("  <TD>");
    if (u)
        printf("<A STYLE=\"text-decoration:none\" TARGET=_BLANK HREF=%s>", u);
    printf("%s", s ? s : "&nbsp;");
    if (u)
        printf("</A>");
    puts("</TD>");
    puts("</TR>");
    }
else
    {
    s = hashFindVal(ra, "description");
    if(s != NULL)
        {
        ++(*total);
        puts("<TR>");
        printf("  <TD>%s</TD>\n", term);
        printf("  <TD>%s</TD>\n", s);
        puts("</TR>");
        }
    else
        {
        printf("<TR>\n  <TD>%s</TD>\n  <TD>Unrecognised term</TD>\n</TR>\n", term);
        errAbort("Error: Unrecognised type (%s)\n", type);
        }
    }
}

static char *normalizeType(char *type)
/* Strips any quotation marks and converts common synonyms */
{
(void)stripChar(type,'\"');
if ((sameWord(type,"Cell Line"))
||  (sameWord(type,"cellLine" ))
||  (sameWord(type,"Cell Type"))
||  (sameWord(type,"cellType" )))
    return cloneString("Cell Line");
else if (sameWord(type,"Factor"))
    return cloneString("Antibody");

return type;
}

static char *findType(struct hash *cvHash)
/* returns the type that was requested or else the type associated with the term requested */
{
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct hash *ra;
char *type = cgiOptionalString("type");

if(type==NULL)    // If not type, but term, then search for first term and use it's type
    {
    char *term = cgiOptionalString("term");
    if(term==NULL)
        errAbort("Error: Required 'term' or 'type' argument not found\n");
    (void)stripChar(term,'\"');
#ifdef HANDLE_IMPLICIT_CONTROL
    if(sameWord(term,"control") || sameWord(term,"input"))
        {
        type = term;
        }
    else
#endif//def HANDLE_IMPLICIT_CONTROL
        {
        while ((hEl = hashNext(&hc)) != NULL)
            {
            ra = (struct hash *)hEl->val;
            char *cmpTerm = hashFindVal(ra, "term");
            if (cmpTerm == NULL || differentWord(cmpTerm, term))
                continue;
            type = hashFindVal(ra, "type");
            break;
            }
        //hc = hashFirst(cvHash);
        }
    }
if(type==NULL)    // Still not type? abort
    errAbort("Error: Required 'type' or 'term' argument not found\n");
return normalizeType(type);
}

void doMiddle()
{
struct hash *cvHash = raReadAll(cgiUsualString("ra", cv_file()), "term");
struct hashCookie hc = hashFirst(cvHash);
struct hashEl *hEl;
struct slList *termList = NULL;
struct hash *ra;
char *type;
int total = 0;

puts("<TABLE BORDER=1 BGCOLOR=#FFFEE8 CELLSPACING=0 CELLPADDING=2>");
puts("<TR style=\"background:#D9E4F8\">");
type = findType(cvHash);
doTypeHeader(type);
puts("</TR>");

#ifdef HANDLE_IMPLICIT_CONTROL
if(differentWord(type,"control") && differentWord(type,"input"))
#endif//def HANDLE_IMPLICIT_CONTROL
    {
    while ((hEl = hashNext(&hc)) != NULL)
        {
        ra = (struct hash *)hEl->val;
        if (differentString(hashMustFindVal(ra, "type"), type))
            continue;
        slAddTail(&termList, ra);
        }
    slSort(&termList, termCmp);
    while((ra = slPopHead(&termList)) != NULL)
        {
        // TODO: Add check for unknown tags in cv.ra
        doTypeRow(ra, type, &total);
        }
    }
puts("</TABLE><BR>");
if(total > 1)
    printf("Total = %d\n", total);
}

int main(int argc, char *argv[])
/* Process command line */
{
cgiSpoof(&argc, argv);
char *bgColor = cgiOptionalString("bgcolor");
if (bgColor)
    htmlSetBgColor(strtol(bgColor, 0, 16));
htmShell("ENCODE Controlled Vocabulary", doMiddle, "get");
return 0;
}

