/* asParse - parse out an autoSql .as file. */

#include "common.h"
#include "linefile.h"
#include "tokenizer.h"
#include "asParse.h"

static char const rcsid[] = "$Id: asParse.c,v 1.3 2004/07/14 05:47:51 kent Exp $";

struct asTypeInfo asTypes[] = {
    {t_double,  "double",  FALSE, FALSE, "double",           "double",        "Double", "Double", "%f"},
    {t_float,   "float",   FALSE, FALSE, "float",            "float",         "Float",  "Float",  "%f"},
    {t_char,    "char",    FALSE, FALSE, "char",             "char",          "Char",   "Char",   "%c"},
    {t_int,     "int",     FALSE, FALSE, "int",              "int",           "Signed", "Signed", "%d"},
    {t_uint,    "uint",    TRUE,  FALSE, "int unsigned",     "unsigned",      "Unsigned","Unsigned", "%u"},
    {t_short,   "short",   FALSE, FALSE, "smallint",         "short",         "Short",  "Signed", "%d"},
    {t_ushort,  "ushort",  TRUE,  FALSE, "smallint unsigned","unsigned short","Ushort", "Unsigned", "%u"},
    {t_byte,    "byte",    FALSE, FALSE, "tinyint",          "signed char",   "Byte",   "Signed", "%d"},
    {t_ubyte,   "ubyte",   TRUE,  FALSE, "tinyint unsigned", "unsigned char", "Ubyte",  "Unsigned", "%u"},
    {t_off,     "bigint",  FALSE,  FALSE,"bigint",           "long long",     "LongLong", "LongLong", "%lld"},
    {t_string,  "string",  FALSE, TRUE,  "varchar(255)",     "char *",        "String", "String", "%s"},
    {t_lstring,    "lstring",    FALSE, TRUE,  "longblob",   "char *",        "String", "String", "%s"},
    {t_object,  "object",  FALSE, FALSE, "longblob",         "!error!",       "Object", "Object", NULL},
    {t_object,  "table",   FALSE, FALSE, "longblob",         "!error!",       "Object", "Object", NULL},
    {t_simple,  "simple",  FALSE, FALSE, "longblob",         "!error!",       "Simple", "Simple", NULL},
};

static struct asTypeInfo *findLowType(struct tokenizer *tkz)
/* Return low type info.  Squawk and die if s doesn't
 * correspond to one. */
{
char *s = tkz->string;
int i;
for (i=0; i<ArraySize(asTypes); ++i)
    {
    if (sameWord(asTypes[i].name, s))
	return &asTypes[i];
    }
tokenizerErrAbort(tkz, "Unknown type '%s'", s);
return NULL;
}

static struct asColumn *mustFindColumn(struct asObject *table, char *colName)
/* Return column or die. */
{
struct asColumn *col;

for (col = table->columnList; col != NULL; col = col->next)
    {
    if (sameWord(col->name, colName))
	return col;
    }
errAbort("Couldn't find column %s", colName);
return NULL;
}

static struct asObject *findObType(struct asObject *objList, char *obName)
/* Find object with given name. */
{
struct asObject *obj;
for (obj = objList; obj != NULL; obj = obj->next)
    {
    if (sameWord(obj->name, obName))
	return obj;
    }
return NULL;
}


static struct asObject *asParseTokens(struct tokenizer *tkz)
/* Parse file into a list of objects. */
{
struct asObject *objList = NULL;
struct asObject *obj;
struct asColumn *col;

for (;;)
    {
    if (!tokenizerNext(tkz))
	break;
    AllocVar(obj);
    if (sameWord(tkz->string, "table"))
	obj->isTable = TRUE;
    else if (sameWord(tkz->string, "simple"))
	obj->isSimple = TRUE;
    else if (sameWord(tkz->string, "object"))
	;
    else
	tokenizerErrAbort(tkz, "Expecting 'table' or 'object' got '%s'", tkz->string);
    tokenizerMustHaveNext(tkz);
    if (findObType(objList, tkz->string))
	tokenizerErrAbort(tkz, "Duplicate definition of %s", tkz->string);
    obj->name = cloneString(tkz->string);
    tokenizerMustHaveNext(tkz);
    obj->comment = cloneString(tkz->string);
    tokenizerMustHaveNext(tkz);
    tokenizerMustMatch(tkz, "(");
    for (;;)
	{
	int ltt;
	if (tkz->string[0] == ')')
	    break;
	AllocVar(col);

	col->lowType = findLowType(tkz);
	tokenizerMustHaveNext(tkz);
	ltt = col->lowType->type;

	if (ltt == t_object || ltt == t_simple)
	    {
	    col->obName = cloneString(tkz->string);
	    tokenizerMustHaveNext(tkz);
	    }
	
	if (tkz->string[0] == '[')
	    {
	    if (ltt == t_simple)
	    	col->isArray = TRUE;
	    else
		col->isList = TRUE;
	    tokenizerMustHaveNext(tkz);
	    if (isdigit(tkz->string[0]))
		{
		col->fixedSize = atoi(tkz->string);
		tokenizerMustHaveNext(tkz);
		}
	    else if (isalpha(tkz->string[0]))
		{
		if (obj->isSimple)
		    {
		    tokenizerErrAbort(tkz, "simple objects can't include variable length arrays\n");
		    }
		col->linkedSizeName = cloneString(tkz->string);
		col->linkedSize = mustFindColumn(obj, col->linkedSizeName);
		col->linkedSize->isSizeLink = TRUE;
		tokenizerMustHaveNext(tkz);
		}
	    else
		{
		tokenizerErrAbort(tkz, "must have column name or integer inside []'s\n");
		}
	     tokenizerMustMatch(tkz, "]");
	    }

	col->name = cloneString(tkz->string);
	tokenizerMustHaveNext(tkz);
	tokenizerMustMatch(tkz, ";");
	col->comment = cloneString(tkz->string);
	tokenizerMustHaveNext(tkz);
	if (col->lowType->type == t_char && col->fixedSize != 0)
	    {
	    col->isList = FALSE;	/* It's not really a list... */
	    }
	slAddHead(&obj->columnList, col);
	}
    slReverse(&obj->columnList);
    slAddTail(&objList, obj);
    }
/* Look up any embedded objects. */
for (obj = objList; obj != NULL; obj = obj->next)
    {
    for (col = obj->columnList; col != NULL; col = col->next)
	{
	if (col->obName != NULL)
	    {
	    if ((col->obType = findObType(objList, col->obName)) == NULL)
		errAbort("%s used but not defined", col->obName);
	    if (obj->isSimple)
		{
		if (!col->obType->isSimple)
		    errAbort("Simple object %s with embedded non-simple object %s",
			obj->name, col->name);
		}
	    }
	}
    }
return objList;
}

static struct asObject *asParseLineFile(struct lineFile *lf)
/* Parse open line file.  Closes lf as a side effect. */
{
struct tokenizer *tkz = tokenizerOnLineFile(lf);
struct asObject *objList = asParseTokens(tkz);
tokenizerFree(&tkz);
return objList;
}

struct asObject *asParseFile(char *fileName)
/* Parse autoSql .as file. */
{
return asParseLineFile(lineFileOpen(fileName, TRUE));
}


struct asObject *asParseText(char *text)
/* Parse autoSql from text (as opposed to file). */
{
char *dupe = cloneString(text);
struct lineFile *lf = lineFileOnString("text", TRUE, dupe);
struct asObject *objList = asParseLineFile(lf);
return objList;
}

