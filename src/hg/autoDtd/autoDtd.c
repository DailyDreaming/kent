/* autoDtd - Give this a XML document to look at and it will come up with a DTD to describe it.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "xap.h"

static char const rcsid[] = "$Id: autoDtd.c,v 1.5 2005/11/29 20:52:35 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "autoDtd - Give this a XML document to look at and it will come up with a DTD\n"
  "to describe it.\n"
  "usage:\n"
  "   autoDtd in.xml out.dtd out.stats\n"
  "options:\n"
  "   -tree=out.tree - Output tag tree.\n"
  );
}

static struct optionSpec options[] = {
   {"tree", OPTION_STRING},
   {NULL, 0},
};

struct type
/* Information on a type. */
    {
    struct type *next;
    char *name;		/* Name of type/field. */
    int count;	        /* Number of occurences of this tag. */
    struct hash *attHash;	/* Hash of all elements keyed by name */
    struct attribute *attributes;
    struct hash *elHash;	/* Hash of all elements keyed by type->name */
    struct element *elements;
    struct attribute *textAttribute;	/* Information on text. */
    };

struct attribute
/* Information on an attribute */
    {
    struct attribute *next;
    char *name;
    int count;		/* Number of times we've seen this attribute. */
    boolean isOptional;	/* True if it's not always there. */
    boolean nonInt;	/* True if not an int. */
    boolean nonFloat;	/* True if not a number. */
    boolean seenThisRound;  /* True if seen this round. */
    struct hash *values;	/* Hash of unique values. */
    int maxLen;		/* Maximum length */
    };

struct element
/* Information on an element */
    {
    struct element *next;
    struct type *type;	/* Element type */
    boolean isOptional;	/* True if it's optional. */
    boolean isList;	/* True if it's a list. */
    boolean seenThisRound;  /* True if seen this round. */
    };

struct hash *typeHash;	/* Keyed by struct type */
struct type *topType;	/* Highest level type */

boolean isAllInt(char *s)
/* Return true if it looks like an integer */
{
char c;
if (*s == '-')
   ++s;
while ((c = *s++) != 0)
    if (!isdigit(c))
        return FALSE;
return TRUE;
}

boolean isAllFloat(char *s)
/* Return true if it looks like an floating point */
{
char c;
if (*s == '-')
   ++s;
while ((c = *s++) != 0)
    if (!isdigit(c))
	break;
if (c == 0)
    return TRUE;
else if (c == '.')
    return isAllInt(s);
else
    return FALSE;
}


void *startHandler(struct xap *xap, char *name, char **atts)
/* Called at the start of a tag after attributes are parsed. */
{
int i;
struct type *type = hashFindVal(typeHash, name);
struct attribute *att;
struct element *el;

if (type == NULL)
    {
    AllocVar(type);
    hashAddSaveName(typeHash, name, type, &type->name);
    type->elHash = hashNew(6);
    type->attHash = hashNew(6);
    }

/* Zero out seenThisRound flags */
for (el = type->elements; el != NULL; el = el->next)
    el->seenThisRound = FALSE;
for (att = type->attributes; att != NULL; att = att->next)
    att->seenThisRound = FALSE;

for (i=0; atts[i] != NULL; i += 2)
    {
    char *name = atts[i], *val = atts[i+1];
    int valLen = strlen(val);
    att = hashFindVal(type->attHash, name);
    if (att == NULL)
        {
	AllocVar(att);
	hashAddSaveName(type->attHash, name, att, &att->name);
	att->values = hashNew(16);
	slAddTail(&type->attributes, att);
	if (type->count != 0)
	    att->isOptional = TRUE;
	}
    att->count += 1;
    hashStore(att->values, val);
    if (valLen > att->maxLen)
        att->maxLen = valLen;
    if (!att->nonInt)
	if (!isAllInt(val))
	    att->nonInt = TRUE;
    if (!att->nonFloat)
	if (!isAllFloat(val))
	    att->nonFloat = TRUE;
    att->seenThisRound = TRUE;
    }
for (att = type->attributes; att != NULL; att = att->next)
    {
    if (!att->seenThisRound)
        att->isOptional = TRUE;
    }

if (xap->stackDepth > 1)
    {
    struct xapStack *st = xap->stack+1;
    struct type *parent = st->object;
    el = hashFindVal(parent->elHash, name);
    if (el == NULL)
        {
	AllocVar(el);
	hashAdd(parent->elHash, name, el);
	el->type = type;
	slAddTail(&parent->elements, el);
	if (parent->count != 0)
	    el->isOptional = TRUE;
	}
    if (el->seenThisRound)
        el->isList = TRUE;
    el->seenThisRound = TRUE;
    }
return type;
}

void endHandler(struct xap *xap, char *name)
/* Called at end of a tag */
{
struct type *type = xap->stack->object;
char *text = skipLeadingSpaces(xap->stack->text->string);
struct element *el;
for (el = type->elements; el != NULL; el = el->next)
    {
    if (!el->seenThisRound)
        el->isOptional = TRUE;
    }
if (text[0] == 0)
    {
    if (type->textAttribute != NULL)
        type->textAttribute->isOptional = TRUE;
    }
else
    {
    int textLen = strlen(text);
    char head[33];
    struct attribute *att = type->textAttribute;
    if (att == NULL)
	{
	type->textAttribute = AllocVar(att);
	att->name = "<text>";
	att->values = hashNew(16);
	if (type->count != 0)
	    att->isOptional = TRUE;
	}
    if (att->maxLen < textLen)
        att->maxLen = textLen;
    hashStore(att->values, text);
    att->count += 1;
    if (!att->nonInt)
	if (!isAllInt(text))
	    att->nonInt = TRUE;
    if (!att->nonFloat)
	if (!isAllFloat(text))
	    att->nonFloat = TRUE;
    }
type->count += 1;
topType = type;
}

char *attDataType(struct attribute *att)
/* Return data type associated with attribute as a string */
{
if (!att->nonInt)
    return "int";
else if (!att->nonFloat)
    return "float";
else
    return "string";
}

void rWriteDtd(FILE *dtdFile, FILE *statsFile, struct type *type, 
	struct hash *uniqHash)
/* Recursively write out DTD. */
{
struct element *el;
struct attribute *att;
hashAdd(uniqHash, type->name, type);
fprintf(dtdFile, "<!ELEMENT %s (\n", type->name);
for (el = type->elements; el != NULL; el = el->next)
    {
    fprintf(dtdFile, "\t%s", el->type->name);
    if (el->isList)
        {
	if (el->isOptional)
	    fprintf(dtdFile, "*");
	else
	    fprintf(dtdFile, "+");
	}
    else
        {
	if (el->isOptional)
	    fprintf(dtdFile, "?");
	}
    if (el->next != NULL || type->textAttribute != NULL)
        fprintf(dtdFile, ",");
    fprintf(dtdFile, "\n");
    }
if (type->textAttribute != NULL)
    {
    fprintf(dtdFile, "\t");
    if (!type->textAttribute->nonInt)
        fprintf(dtdFile, "#INT");
    else if (!type->textAttribute->nonFloat)
        fprintf(dtdFile, "#FLOAT");
    else
        fprintf(dtdFile, "#PCDATA");
    fprintf(dtdFile, "\n");
    }
fprintf(dtdFile, ")>\n");
fprintf(statsFile, "%s %d\n", type->name, type->count);
if ((att = type->textAttribute) != NULL)
    {
    fprintf(statsFile, "\t%s\t%d\t%s\t%d\t%d\n", att->name, att->maxLen,
    	attDataType(att), att->count, att->values->elCount);
    }
else
    {
    fprintf(statsFile, "\t<text>\t0\tnone\t0\t0\n");
    }

for (att = type->attributes; att != NULL; att = att->next)
    {
    fprintf(dtdFile, "<!ATTLIST %s %s ", type->name, att->name);
    if (!att->nonInt)
        fprintf(dtdFile, "INT");
    else if (!att->nonFloat)
        fprintf(dtdFile, "FLOAT");
    else
        fprintf(dtdFile, "CDATA");
    if (att->isOptional)
        fprintf(dtdFile, " #IMPLIED");
    else
	fprintf(dtdFile, " #REQUIRED");
    fprintf(dtdFile, ">\n");
    fprintf(statsFile, "\t%s\t%d\t%s\t%d\t%d\n", att->name, att->maxLen,
    	attDataType(att), att->count, att->values->elCount);
    }
fprintf(dtdFile, "\n");
fprintf(statsFile, "\n");

/* Now recurse if we haven't written children yet. */
for (el = type->elements; el != NULL; el = el->next)
    {
    if (!hashLookup(uniqHash, el->type->name))
        {
	rWriteDtd(dtdFile, statsFile, el->type, uniqHash);
	}
    }
}

void writeDtd(char *dtdFileName, char *statsFileName, char *xmlFileName, 
	struct type *type)
/* Write out DTD. */
{
struct hash *uniqHash = newHash(0);  /* Prevent writing dup defs for shared types. */
FILE *dtdFile = mustOpen(dtdFileName, "w");
FILE *statsFile = mustOpen(statsFileName, "w");
fprintf(dtdFile, "<!-- This file was created by autoXml based on %s -->\n\n", xmlFileName);
fprintf(statsFile, "#Statistics on %s\n", xmlFileName);
fprintf(statsFile, "#Format is:\n");
fprintf(statsFile, "#<tag name>  <tag count>\n");
fprintf(statsFile, "#      <<text>> <max length> <type> <count> <unique count>\n");
fprintf(statsFile, "#      <attribute name> <max length> <type> <count> <unique count>\n");
fprintf(statsFile, "\n");
rWriteDtd(dtdFile, statsFile, type, uniqHash);
carefulClose(&dtdFile);
carefulClose(&statsFile);
}

void rWriteTree(FILE *f, struct type *type, boolean isOptional, boolean isList,
	struct hash *uniqHash, int level)
/* Write out type and it's children. */
{
struct attribute *att;
struct element *el;

spaceOut(f, level*2);
if (level > 20)
    {
    fprintf(f, "%s...\n", type->name);
    }
else
    {
    fprintf(f, "%s", type->name);
    if (isList)
        if (isOptional)
	    fprintf(f, "*");
	else
	    fprintf(f, "+");
    else
        if (isOptional)
	    fprintf(f, "?");
    fprintf(f, "\n");
    for (el = type->elements; el != NULL; el = el->next)
	rWriteTree(f, el->type, el->isOptional, el->isList, uniqHash, level+1);
    }
}

void writeTree(char *fileName, struct type *root)
/* Write out type tree to file. */
{
struct hash *uniqHash = newHash(0);  /* Prevent writing dup defs. */
FILE *f = mustOpen(fileName, "w");
rWriteTree(f, root, FALSE, FALSE, uniqHash, 0);
carefulClose(&f);
}


void autoDtd(char *inXml, char *outDtd, char *outStats, char *treeFileName)
/* autoDtd - Give this a XML document to look at and it will come up with a 
 * DTD to describe it.. */
{
struct xap *xap = xapNew(startHandler, endHandler, inXml);
typeHash = newHash(0);
xapParseFile(xap, inXml);
writeDtd(outDtd, outStats, inXml, topType);
if (treeFileName != NULL)
    writeTree(treeFileName, topType);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *treeFileName = NULL;
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
treeFileName = optionVal("tree", treeFileName);
autoDtd(argv[1], argv[2], argv[3], treeFileName);
return 0;
}
