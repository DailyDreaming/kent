/* autoSql - automatically generate SQL and C code for saving
 * and loading entities in database. 
 *
 * This module is pretty ugly.  It seems to work within some
 * limitations (described in the autoSql.doc).  Rather than do major
 * changes from here I'd rewrite it though.  The tokenizer is
 * pretty sound, the parser isn't bad (though the language it
 * parses is quirky), but the code generator is u-g-l-y.
 */

#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: autoSql.c,v 1.19 2003/05/06 07:22:14 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort("autoSql - create SQL and C code for permanently storing\n"
         "a structure in database and loading it back into memory\n"
	 "based on a specification file\n"
	 "usage:\n"
	 "    autoSql specFile outRoot {optional: -dbLink} \n"
	 "This will create outRoot.sql outRoot.c and outRoot.h based\n"
	 "on the contents of specFile. The -dbLink flag optionally\n"
	 "generates code to execute queries and updates of the table.\n");
}

enum lowTypes
/* Different low level types (not including lists and objects) */
   {
   t_double,   /* double precision floating point. */
   t_float,    /* single precision floating point. */
   t_char,     /* character or fixed size character array. */
   t_int,      /* signed 32 bit integer */
   t_uint,     /* unsigned 32 bit integer */
   t_short,    /* signed 16 bit integer */
   t_ushort,   /* unsigned 16 bit integer */
   t_byte,     /* signed 8 bit integer */
   t_ubyte,    /* unsigned 8 bit integer */
   t_off,      /* 64 bit integer. */
   t_string,   /* varchar/char * (variable size string up to 255 chars)  */
   t_lstring,     /* variable sized large string. */
   t_object,   /* composite object - object/table - forms lists. */
   t_simple,   /* simple composite object - forms arrays. */
   };

struct lowTypeInfo
    {
    enum lowTypes type;		   /* Numeric ID of low level type. */
    char *name;                    /* Text ID of low level type. */
    bool isUnsigned;               /* True if an unsigned int of some type. */
    bool stringy;                  /* True if a string or blob. */
    char *sqlName;                 /* SQL type name. */
    char *cName;                   /* C type name. */
    char *listyName;               /* What functions that load a list are called. */
    char *nummyName;               /* What functions that load a number are called. */
    char *outFormat;		   /* Output format for printf. %d, %u, etc. */
    };

struct lowTypeInfo lowTypes[] = {
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

struct column
/* Info on one column/field */
    {
    struct column *next;           /* Next column. */
    char *name;                    /* Column name. */
    char *comment;		   /* Comment string on column. */
    struct lowTypeInfo *lowType;   /* Root type info. */
    char *obName;                  /* Name of object or table. */
    struct dbObject *obType;       /* Name of composite object. */
    int fixedSize;		   /* 0 if not fixed size, otherwise size of list. */
    char *linkedSizeName;          /* Points to variable that holds size of list. */
    struct column *linkedSize;     /* Column for linked size. */
    bool isSizeLink;               /* Flag to tell if have read link. */
    bool isList;                   /* TRUE if a list. */
    bool isArray;                  /* TRUE if an array. */
    };

struct dbObject
/* Info on whole dbObject. */
    {
    struct dbObject *next;
    char *name;			/* Name of object. */
    char *comment;		/* Comment describing object. */
    struct column *columnList;  /* List of columns. */
    bool isTable;	        /* True if a table. */
    bool isSimple;	        /* True if a simple object. */
    };

struct tokenizer
/* This handles reading in tokens. */
    {
    bool reuse;	         /* True if want to reuse this token. */
    bool eof;            /* True at end of file. */
    struct lineFile *lf; /* Underlying file. */
    char *curLine;       /* Current line of text. */
    char *linePt;        /* Start position within current line. */
    char *string;        /* String value of token */
    int sSize;           /* Size of string. */
    int sAlloc;          /* Allocated string size. */
    };

struct tokenizer *newTokenizer(char *fileName)
/* Return a new tokenizer. */
{
struct tokenizer *tkz;
AllocVar(tkz);
tkz->sAlloc = 128;
tkz->string = needMem(tkz->sAlloc);
tkz->lf = lineFileOpen(fileName, TRUE);
tkz->curLine = tkz->linePt = "";
return tkz;
}

void freeTokenizer(struct tokenizer **pTkz)
/* Tear down a tokenizer. */
{
struct tokenizer *tkz;
if ((tkz = *pTkz) != NULL)
    {
    freeMem(tkz->string);
    lineFileClose(&tkz->lf);
    freez(pTkz);
    }
}

void tkzReuse(struct tokenizer *tkz)
/* Reused token. */
{
tkz->reuse = TRUE;
}

int tkzLineCount(struct tokenizer *tkz)
/* Return line of current token. */
{
return tkz->lf->lineIx;
}

char *tkzFileName(struct tokenizer *tkz)
/* Return name of file. */
{
return tkz->lf->fileName;
}

char *tkzNext(struct tokenizer *tkz)
/* Return token's next string (also available as tkz->string) or
 * NULL at EOF. */
{
char *start, *end;
char c, *s;
int size;
if (tkz->reuse)
    {
    tkz->reuse = FALSE;
    return tkz->string;
    }
for (;;)	/* Skip over white space. */
    {
    int lineSize;
    s = start = skipLeadingSpaces(tkz->linePt);
    if ((c = start[0]) != 0)
	break;
    if (!lineFileNext(tkz->lf, &tkz->curLine, &lineSize))
	{
	tkz->eof = TRUE;
	return NULL;
	}
    tkz->linePt = tkz->curLine;
    }
if (isalnum(c) || (c == '_'))
    {
    for (;;)
	{
        s++;
	if (!(isalnum(*s) || (*s == '_')))
	    break;
	}
    end = s;
    }
else if (c == '"')
    {
    start = s+1;
    for (;;)
	{
	c = *(++s);
	if (c == '"')
	    break;
	}
    end = s;
    ++s;
    }
else
    {
    end = ++s;
    }
tkz->linePt = s;
size = end - start;
if (size >= tkz->sAlloc)
    {
    tkz->sAlloc = size+128;
    tkz->string = needMoreMem(tkz->string, 0, tkz->sAlloc);
    }
memcpy(tkz->string, start, size);
tkz->string[size] = 0;
return tkz->string;
}

void tkzErrAbort(struct tokenizer *tkz, char *format, ...)
/* Print error message followed by file and line number and
 * abort. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
errAbort("line %d of %s:\n%s", 
	tkzLineCount(tkz), tkzFileName(tkz), tkz->curLine);
}

void tkzNotEnd(struct tokenizer *tkz)
/* Squawk if at end. */
{
if (tkz->eof)
    errAbort("Unexpected end of file");
}

void tkzMustHaveNext(struct tokenizer *tkz)
/* Get next token, which must be there. */
{
if (tkzNext(tkz) == NULL)
    errAbort("Unexpected end of file");
}

void tkzMustMatch(struct tokenizer *tkz, char *string)
/* Require next token to match string.  Return next token
 * if it does, otherwise abort. */
{
if (sameWord(tkz->string, string))
    tkzMustHaveNext(tkz);
else
    tkzErrAbort(tkz, "Expecting %s got %s", string, tkz->string);
}

struct lowTypeInfo *findLowType(struct tokenizer *tkz)
/* Return low type info.  Squawk and die if s doesn't
 * correspond to one. */
{
char *s = tkz->string;
int i;
for (i=0; i<ArraySize(lowTypes); ++i)
    {
    if (sameWord(lowTypes[i].name, s))
	return &lowTypes[i];
    }
tkzErrAbort(tkz, "Unknown type '%s'", s);
return NULL;
}

struct column *mustFindColumn(struct dbObject *table, char *colName)
/* Return column or die. */
{
struct column *col;

for (col = table->columnList; col != NULL; col = col->next)
    {
    if (sameWord(col->name, colName))
	return col;
    }
errAbort("Couldn't find column %s", colName);
}

struct dbObject *findObType(struct dbObject *objList, char *obName)
/* Find object with given name. */
{
struct dbObject *obj;
for (obj = objList; obj != NULL; obj = obj->next)
    {
    if (sameWord(obj->name, obName))
	return obj;
    }
return NULL;
}


struct dbObject *parseObjects(struct tokenizer *tkz)
/* Parse file into a list of objects. */
{
struct dbObject *objList = NULL;
struct dbObject *obj;
struct column *col;

for (;;)
    {
    if (!tkzNext(tkz))
	break;
    AllocVar(obj);
    if (sameWord(tkz->string, "table"))
	obj->isTable = TRUE;
    else if (sameWord(tkz->string, "simple"))
	obj->isSimple = TRUE;
    else if (sameWord(tkz->string, "object"))
	;
    else
	tkzErrAbort(tkz, "Expecting 'table' or 'object' got '%s'", tkz->string);
    tkzMustHaveNext(tkz);
    if (findObType(objList, tkz->string))
	tkzErrAbort(tkz, "Duplicate definition of %s", tkz->string);
    obj->name = cloneString(tkz->string);
    tkzMustHaveNext(tkz);
    obj->comment = cloneString(tkz->string);
    tkzMustHaveNext(tkz);
    tkzMustMatch(tkz, "(");
    for (;;)
	{
	int ltt;
	if (tkz->string[0] == ')')
	    break;
	AllocVar(col);

	col->lowType = findLowType(tkz);
	tkzMustHaveNext(tkz);
	ltt = col->lowType->type;

	if (ltt == t_object || ltt == t_simple)
	    {
	    col->obName = cloneString(tkz->string);
	    tkzMustHaveNext(tkz);
	    }
	
	if (tkz->string[0] == '[')
	    {
	    if (ltt == t_simple)
	    	col->isArray = TRUE;
	    else
		col->isList = TRUE;
	    tkzMustHaveNext(tkz);
	    if (isdigit(tkz->string[0]))
		{
		col->fixedSize = atoi(tkz->string);
		tkzMustHaveNext(tkz);
		}
	    else if (isalpha(tkz->string[0]))
		{
		if (obj->isSimple)
		    {
		    tkzErrAbort(tkz, "simple objects can't include variable length arrays\n");
		    }
		col->linkedSizeName = cloneString(tkz->string);
		col->linkedSize = mustFindColumn(obj, col->linkedSizeName);
		col->linkedSize->isSizeLink = TRUE;
		tkzMustHaveNext(tkz);
		}
	    else
		{
		tkzErrAbort(tkz, "must have column name or integer inside []'s\n");
		}
	     tkzMustMatch(tkz, "]");
	    }

	col->name = cloneString(tkz->string);
	tkzMustHaveNext(tkz);
	tkzMustMatch(tkz, ";");
	col->comment = cloneString(tkz->string);
	tkzMustHaveNext(tkz);
	if (col->lowType->type == t_char)
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

void sqlTable(struct dbObject *table, FILE *f)
/* Print out structure of table in SQL. */
{
struct column *col;
struct lowTypeInfo *lt;

fprintf(f, "\n#%s\n", table->comment);
fprintf(f, "CREATE TABLE %s (\n", table->name);
for (col = table->columnList; col != NULL; col = col->next)
    {
    lt = col->lowType;
    fprintf(f, "    %s ", col->name);
    if (col->isList || col->isArray)
	fprintf(f, "longblob not null");
    else if (lt->type == t_char)
	fprintf(f, "char(%d) not null", col->fixedSize ? col->fixedSize : 1);
    else
	fprintf(f, "%s not null", lt->sqlName);
    fputc(',', f);
    fprintf(f, "\t# %s\n", col->comment);
    }
fprintf(f,"              #Indices\n");
fprintf(f, "    PRIMARY KEY(%s)\n", table->columnList->name);
fprintf(f, ");\n");
}

void cTable(struct dbObject *dbObj, FILE *f)
/* Print out structure of dbObj in C. */
{
struct column *col;
struct lowTypeInfo *lt;
struct dbObject *obType;
char defineName[256];

safef(defineName, sizeof(defineName), "%s", dbObj->name);
touppers(defineName);
fprintf(f, "#define %s_NUM_COLS %d\n\n", defineName,
        slCount(dbObj->columnList));

fprintf(f, "struct %s\n", dbObj->name);
fprintf(f, "/* %s */\n", dbObj->comment);
fprintf(f, "    {\n");
if (!dbObj->isSimple)
    fprintf(f, "    struct %s *next;  /* Next in singly linked list. */\n", dbObj->name);
for (col = dbObj->columnList; col != NULL; col = col->next)
    {
    lt = col->lowType;
    if ((obType = col->obType) != NULL)
	{
	if (lt->type == t_object)
	    fprintf(f, "    struct %s *%s", obType->name, col->name);
	else if (lt->type == t_simple)
	    {
	    if (col->isArray)
		{
		if (col->fixedSize)
		    {
		    fprintf(f, "    struct %s %s[%d]", 
		    	obType->name, col->name, col->fixedSize);
		    }
		else
		    {
		    fprintf(f, "    struct %s *%s", 
		    	obType->name, col->name);
		    }
		}
	    else
		{
		fprintf(f, "    struct %s %s", obType->name, col->name);
		}
	    }
	else
	    {
	    assert(FALSE);
	    }
	}
    else
	{
	fprintf(f, "    %s", lt->cName);
	if (lt->type == t_char)
	    {
	    if (col->fixedSize > 0)
		fprintf(f, " %s[%d]", col->name, col->fixedSize+1); 
	    else
		fprintf(f, " %s", col->name); 
	    }
	else
	    {
	    if (!lt->stringy)
		fputc(' ',f);
	    if (col->isList && !col->fixedSize)
		fputc('*', f);
	    fprintf(f, "%s", col->name);
	    if (col->isList && col->fixedSize)
		{
		fprintf(f, "[%d]", col->fixedSize);
		}
	    }
	}
    fprintf(f, ";\t/* %s */\n", col->comment);
    }
fprintf(f, "    };\n\n");
}

static boolean trueFalse[] = {TRUE, FALSE};

void makeCommaInColumn(char *indent, struct column *col,  FILE *f, bool isArray)
/* Make code to read in one column from a comma separated set. */
{
struct dbObject *obType = col->obType;
struct lowTypeInfo *lt = col->lowType;
char *arrayRef = (isArray ? "[i]" : "");
    
if (obType != NULL)
    {
    if (lt->type == t_object)
	{
	fprintf(f, "%ss = sqlEatChar(s, '{');\n", indent);
	fprintf(f, "%sif(s[0] != '}')",indent);
	fprintf(f, "%s    slSafeAddHead(&ret->%s, %sCommaIn(&s,NULL));\n", indent,
	     col->name, obType->name);
	fprintf(f, "%ss = sqlEatChar(s, '}');\n", indent);
	fprintf(f, "%ss = sqlEatChar(s, ',');\n", indent);
	}
    else if (lt->type == t_simple)
	{
	fprintf(f, "%ss = sqlEatChar(s, '{');\n", indent);
	fprintf(f, "%sif(s[0] != '}')",indent);
	fprintf(f, "%s    %sCommaIn(&s, &ret->%s%s);\n", indent,
	    obType->name, col->name, arrayRef);
	fprintf(f, "%ss = sqlEatChar(s, '}');\n", indent);
	fprintf(f, "%ss = sqlEatChar(s, ',');\n", indent);
	}
    else
	{
	assert(FALSE);
	}
    }
else if (lt->stringy)
    fprintf(f, "%sret->%s%s = sqlStringComma(&s);\n", indent, col->name, arrayRef);
else if (lt->isUnsigned)
    fprintf(f, "%sret->%s%s = sqlUnsignedComma(&s);\n", indent, col->name, arrayRef);
else if (lt->type == t_char)
    {
    if (col->fixedSize > 0)
	fprintf(f, "%ssqlFixedStringComma(&s, ret->%s%s, sizeof(ret->%s%s));\n",
		indent, col->name, arrayRef, col->name, arrayRef);
    else
	fprintf(f, "%ssqlFixedStringComma(&s, &(ret->%s), sizeof(ret->%s));\n",
		indent, col->name, col->name);
    }
else
    fprintf(f, "%sret->%s%s = sql%sComma(&s);\n", indent, col->name, arrayRef, lt->nummyName);
}


void loadColumn(struct column *col, int colIx, boolean isDynamic, boolean isSizeLink,
	FILE *f)
/* Print statement to load column. */
{
char *staDyn = (isDynamic ? "Dynamic" : "Static");

if (col->isSizeLink == isSizeLink)
    {
    struct lowTypeInfo *lt = col->lowType;
    enum lowTypes type = lt->type;
    struct dbObject *obType = col->obType;
    if (col->isList || col->isArray)
	{
	char *lName;
	if ((lName = lt->listyName) == NULL)
	    errAbort("Sorry, lists of %s not implemented.", lt->name);
	if (obType)
	    {
	    fprintf(f, "s = row[%d];\n", colIx);
	    if (col->fixedSize)
		{
		fprintf(f, "for (i=0; i<%d; ++i)\n", col->fixedSize);
		}
	    else
		{
		if (type == t_simple)
		    {
		    fprintf(f, "AllocArray(ret->%s, ret->%s);\n", col->name, 
		    	col->linkedSize->name);
		    }
		fprintf(f, "for (i=0; i<ret->%s; ++i)\n", col->linkedSize->name);
		}
	    fprintf(f, "    {\n");
	    fprintf(f, "    s = sqlEatChar(s, '{');\n");
	    if (type == t_object)
		{
		fprintf(f, "    slSafeAddHead(&ret->%s, %sCommaIn(&s, NULL));\n",
		    col->name, obType->name);
		}
	    else if (type == t_simple)
		{
		fprintf(f, "    %sCommaIn(&s, &ret->%s[i]);\n", obType->name, col->name);
		}
	    else
		assert(FALSE);
	    fprintf(f, "    s = sqlEatChar(s, '}');\n");
	    fprintf(f, "    s = sqlEatChar(s, ',');\n");
	    fprintf(f, "    }\n");
	    if (type == t_object)
		fprintf(f, "slReverse(&ret->%s);\n", col->name);
	    }
	else
	    {
	    if (col->fixedSize)
		{
		if (isDynamic && lt->stringy)
		    {
		    fprintf(f, "s = cloneString(row[%d]);\n", colIx);
		    fprintf(f, "sql%sArray(s, ret->%s, %d);\n",
			       lName, col->name, col->fixedSize);
		    }
		else
		    {
		    fprintf(f, "sql%sArray(row[%d], ret->%s, %d);\n",
			       lName, colIx, col->name, col->fixedSize);
		    }
		}
	    else
		{
		struct column *ls;
		fprintf(f, "sql%s%sArray(row[%d], &ret->%s, &sizeOne);\n",
			   lName, staDyn, colIx, col->name); 
		if ((ls = col->linkedSize) != NULL)
		    fprintf(f, "assert(sizeOne == ret->%s);\n", ls->name);
		}
	    }
	}
    else
	{
	switch (type)
	    {
	    case t_float:
	    case t_double:
		fprintf(f, "ret->%s = atof(row[%d]);\n",
		    col->name, colIx);
		break;
	    case t_string:
	    case t_lstring:
		if (isDynamic)
		    fprintf(f, "ret->%s = cloneString(row[%d]);\n", col->name, colIx);
		else
		    fprintf(f, "ret->%s = row[%d];\n", col->name, colIx);
		break;
	    case t_char:
		if (col->fixedSize > 0)
		    fprintf(f, "strcpy(ret->%s, row[%d]);\n", col->name, colIx);
		else
		    fprintf(f, "ret->%s = row[%d][0];\n", col->name, colIx);
		break;
	    case t_object:
		{
		struct dbObject *obj = col->obType;
		fprintf(f, "s = row[%d];\n", colIx);
		fprintf(f, "if(s != NULL && differentString(s, \"\"))\n");
		fprintf(f, "   ret->%s = %sCommaIn(&s, NULL);\n", col->name, obj->name);
		break;
		}
	    case t_simple:
		{
		struct dbObject *obj = col->obType;
		fprintf(f, "s = row[%d];\n", colIx);
		fprintf(f, "if(s != NULL && differentString(s, \"\"))\n");
		fprintf(f, "   %sCommaIn(&s, &ret->%s);\n", obj->name, col->name);
		break;
		}
	    default:
	        {
		fprintf(f, "ret->%s = sql%s(row[%d]);\n", 
		    col->name, lt->nummyName, colIx);
		break;
		}
	    }
	}
    }
}

void makeCommaIn(struct dbObject *table, FILE *f, FILE *hFile)
/* Make routine that loads object from comma separated file. */
{
char *tableName = table->name;
struct column *col;

fprintf(hFile, "struct %s *%sCommaIn(char **pS, struct %s *ret);\n", 
	tableName, tableName, tableName);
fprintf(hFile, "/* Create a %s out of a comma separated string. \n", tableName);
fprintf(hFile, " * This will fill in ret if non-null, otherwise will\n");
fprintf(hFile, " * return a new %s */\n\n", tableName);

fprintf(f, "struct %s *%sCommaIn(char **pS, struct %s *ret)\n", 
	tableName, tableName, tableName);
fprintf(f, "/* Create a %s out of a comma separated string. \n", tableName);
fprintf(f, " * This will fill in ret if non-null, otherwise will\n");
fprintf(f, " * return a new %s */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "char *s = *pS;\n");
fprintf(f, "int i;\n");
fprintf(f, "\n");
fprintf(f, "if (ret == NULL)\n");
fprintf(f, "    AllocVar(ret);\n");

for (col = table->columnList; col != NULL; col = col->next)
    {
    if (col->isList)
	{
	fprintf(f, "s = sqlEatChar(s, '{');\n");
	if (col->fixedSize)
	    {
	    fprintf(f, "for (i=0; i<%d; ++i)\n", col->fixedSize);
	    }
	else
	    {
	    if (!col->obType)
		fprintf(f, "AllocArray(ret->%s, ret->%s);\n", col->name, col->linkedSizeName);
	    fprintf(f, "for (i=0; i<ret->%s; ++i)\n", col->linkedSizeName);
	    }
	fprintf(f, "    {\n");
	makeCommaInColumn("    ", col, f, col->obType == NULL);
	fprintf(f, "    }\n");
	if (col->obType)
	    fprintf(f, "slReverse(&ret->%s);\n", col->name);
	fprintf(f, "s = sqlEatChar(s, '}');\n");
	fprintf(f, "s = sqlEatChar(s, ',');\n");
	}
    else if (col->isArray)
	{
	fprintf(f, "s = sqlEatChar(s, '{');\n");
	if (col->fixedSize)
	    {
	    fprintf(f, "for (i=0; i<%d; ++i)\n", col->fixedSize);
	    }
	else
	    {
	    fprintf(f, "AllocArray(ret->%s, ret->%s);\n", col->name, col->linkedSizeName);
	    fprintf(f, "for (i=0; i<ret->%s; ++i)\n", col->linkedSizeName);
	    }
	fprintf(f, "    {\n");
	makeCommaInColumn("    ", col, f, TRUE);
	fprintf(f, "    }\n");
	fprintf(f, "s = sqlEatChar(s, '}');\n");
	fprintf(f, "s = sqlEatChar(s, ',');\n");
	}
    else
	{
	makeCommaInColumn("",col, f, FALSE);
	}
    }
fprintf(f, "*pS = s;\n");
fprintf(f, "return ret;\n");
fprintf(f, "}\n\n");
}


boolean objectHasVariableLists(struct dbObject *table)
/* Returns TRUE if object has any list members. */
{
struct column *col;
for (col = table->columnList; col != NULL; col = col->next)
    {
    if ((col->isList || col->isArray) && !col->fixedSize)
	return TRUE;
    }
return FALSE;
}

boolean objectHasSubObjects(struct dbObject *table)
/* Returns TRUE if object has any object members. */
{
struct column *col;
for (col = table->columnList; col != NULL; col = col->next)
    {
    if (col->lowType->type == t_object)
	return TRUE;
    }
return FALSE;
}

void staticLoadRow(struct dbObject *table, FILE *f, FILE *hFile)
/* Create C code to load a static instance from a row.
 * Only generated if no lists... */
{
int i;
char *tableName = table->name;
struct column *col;
boolean isSizeLink;
int tfIx;


fprintf(hFile, "void %sStaticLoad(char **row, struct %s *ret);\n", tableName, tableName);
fprintf(hFile, "/* Load a row from %s table into ret.  The contents of ret will\n", tableName);
fprintf(hFile, " * be replaced at the next call to this function. */\n\n");

fprintf(f, "void %sStaticLoad(char **row, struct %s *ret)\n", tableName, tableName);
fprintf(f, "/* Load a row from %s table into ret.  The contents of ret will\n", tableName);
fprintf(f, " * be replaced at the next call to this function. */\n");
fprintf(f, "{\n");
fprintf(f, "int sizeOne,i;\n");
fprintf(f, "char *s;\n");
fprintf(f, "\n");
for (tfIx = 0; tfIx < 2; ++tfIx)
    {
    isSizeLink = trueFalse[tfIx];
    for (i=0,col = table->columnList; col != NULL; col = col->next, ++i)
	{
	loadColumn(col, i, FALSE, isSizeLink, f);
	}
    }
fprintf(f, "}\n\n");
}

void dynamicLoadRow(struct dbObject *table, FILE *f, FILE *hFile)
/* Create C code to load an instance from a row into dynamically
 * allocated memory. */
{
int i;
char *tableName = table->name;
struct column *col;
boolean isSizeLink;
int tfIx;

fprintf(hFile, "struct %s *%sLoad(char **row);\n", tableName, tableName);
fprintf(hFile, "/* Load a %s from row fetched with select * from %s\n", tableName, tableName);
fprintf(hFile, " * from database.  Dispose of this with %sFree(). */\n\n", tableName);

fprintf(f, "struct %s *%sLoad(char **row)\n", tableName, tableName);
fprintf(f, "/* Load a %s from row fetched with select * from %s\n", tableName, tableName);
fprintf(f, " * from database.  Dispose of this with %sFree(). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "struct %s *ret;\n", tableName);
fprintf(f, "int sizeOne,i;\n");
fprintf(f, "char *s;\n");
fprintf(f, "\n");
fprintf(f, "AllocVar(ret);\n");
for (tfIx = 0; tfIx < 2; ++tfIx)
    {
    isSizeLink = trueFalse[tfIx];
    for (i=0,col = table->columnList; col != NULL; col = col->next, ++i)
	{
	loadColumn(col, i, TRUE, isSizeLink, f);
	}
    }
fprintf(f, "return ret;\n");
fprintf(f, "}\n\n");
}

void dynamicLoadAll(struct dbObject *table, FILE *f, FILE *hFile)
/* Create C code to load a all objects from a tab separated file. */
{
char *tableName = table->name;

fprintf(hFile, "struct %s *%sLoadAll(char *fileName);\n", tableName, tableName);
fprintf(hFile, "/* Load all %s from whitespace-separated file.\n", tableName);
fprintf(hFile, " * Dispose of this with %sFreeList(). */\n\n", tableName);

fprintf(f, "struct %s *%sLoadAll(char *fileName) \n", tableName, tableName);
fprintf(f, "/* Load all %s from a whitespace-separated file.\n", tableName);
fprintf(f, " * Dispose of this with %sFreeList(). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "struct %s *list = NULL, *el;\n", tableName);
fprintf(f, "struct lineFile *lf = lineFileOpen(fileName, TRUE);\n");
fprintf(f, "char *row[%d];\n", slCount(table->columnList));
fprintf(f, "\n");
fprintf(f, "while (lineFileRow(lf, row))\n");
fprintf(f, "    {\n");
fprintf(f, "    el = %sLoad(row);\n", tableName);
fprintf(f, "    slAddHead(&list, el);\n");
fprintf(f, "    }\n");
fprintf(f, "lineFileClose(&lf);\n");
fprintf(f, "slReverse(&list);\n");
fprintf(f, "return list;\n");
fprintf(f, "}\n\n");
}

void dynamicLoadAllByChar(struct dbObject *table, FILE *f, FILE *hFile)
/* Create C code to load a all objects from a tab separated file. */
{
char *tableName = table->name;

fprintf(hFile, "struct %s *%sLoadAllByChar(char *fileName, char chopper);\n", tableName, tableName);
fprintf(hFile, "/* Load all %s from chopper separated file.\n", tableName);
fprintf(hFile, " * Dispose of this with %sFreeList(). */\n\n", tableName);

fprintf(hFile, "#define %sLoadAllByTab(a) %sLoadAllByChar(a, '\\t');\n", tableName, tableName);
fprintf(hFile, "/* Load all %s from tab separated file.\n", tableName);
fprintf(hFile, " * Dispose of this with %sFreeList(). */\n\n", tableName);

fprintf(f, "struct %s *%sLoadAllByChar(char *fileName, char chopper) \n", tableName, tableName);
fprintf(f, "/* Load all %s from a chopper separated file.\n", tableName);
fprintf(f, " * Dispose of this with %sFreeList(). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "struct %s *list = NULL, *el;\n", tableName);
fprintf(f, "struct lineFile *lf = lineFileOpen(fileName, TRUE);\n");
fprintf(f, "char *row[%d];\n", slCount(table->columnList));
fprintf(f, "\n");
fprintf(f, "while (lineFileNextCharRow(lf, chopper, row, ArraySize(row)))\n");
fprintf(f, "    {\n");
fprintf(f, "    el = %sLoad(row);\n", tableName);
fprintf(f, "    slAddHead(&list, el);\n");
fprintf(f, "    }\n");
fprintf(f, "lineFileClose(&lf);\n");
fprintf(f, "slReverse(&list);\n");
fprintf(f, "return list;\n");
fprintf(f, "}\n\n");
}

void dynamicLoadByQueryPrintPrototype(char *tableName, FILE *f, boolean addSemi)
/* Print out function prototype and opening comment. */
{
fprintf(f, 
   "struct %s *%sLoadByQuery(struct sqlConnection *conn, char *query)%s\n", 
    tableName, tableName,
    (addSemi ? ";" : ""));
fprintf(f, "/* Load all %s from table that satisfy the query given.  \n", tableName);
fprintf(f, " * Where query is of the form 'select * from example where something=something'\n");
fprintf(f, " * or 'select example.* from example, anotherTable where example.something = \n");
fprintf(f, " * anotherTable.something'.\n");
fprintf(f, " * Dispose of this with %sFreeList(). */\n", tableName);
}

void dynamicLoadByQuery(struct dbObject *table, FILE *f, FILE *hFile)
/* Create C code to build a list from a query to database. */
{
char *tableName = table->name;
dynamicLoadByQueryPrintPrototype(tableName, hFile, TRUE);
fprintf(hFile, "\n");
dynamicLoadByQueryPrintPrototype(tableName, f, FALSE);
fprintf(f, "{\n");
fprintf(f, "struct %s *list = NULL, *el;\n", tableName);
fprintf(f, "struct sqlResult *sr;\n");
fprintf(f, "char **row;\n");
fprintf(f, "\n");
fprintf(f, "sr = sqlGetResult(conn, query);\n");
fprintf(f, "while ((row = sqlNextRow(sr)) != NULL)\n");
fprintf(f, "    {\n");
fprintf(f, "    el = %sLoad(row);\n", tableName);
fprintf(f, "    slAddHead(&list, el);\n");
fprintf(f, "    }\n");
fprintf(f, "slReverse(&list);\n");
fprintf(f, "sqlFreeResult(&sr);\n");
fprintf(f, "return list;\n");
fprintf(f, "}\n\n");
}

void dynamicSaveToDbPrintPrototype(char *tableName, FILE *f, boolean addSemi)
/* Print out function prototype and opening comment. */
{
fprintf(f,
	"void %sSaveToDb(struct sqlConnection *conn, struct %s *el, char *tableName, int updateSize)%s\n", 
	tableName, tableName, (addSemi ? ";" : ""));
fprintf(f,
	"/* Save %s as a row to the table specified by tableName. \n", tableName);
fprintf(f, " * As blob fields may be arbitrary size updateSize specifies the approx size\n");
fprintf(f, " * of a string that would contain the entire query. Arrays of native types are\n");
fprintf(f, " * converted to comma separated strings and loaded as such, User defined types are\n");
fprintf(f, " * inserted as NULL. Note that strings must be escaped to allow insertion into the database.\n");
fprintf(f, " * For example \"autosql's features include\" --> \"autosql\\'s features include\" \n");
fprintf(f, " * If worried about this use %sSaveToDbEscaped() */\n", tableName);
}

boolean lastArrayType(struct column *colList)
/* if there are any more string types returns TRUE else returns false */
{
struct column *col;
for(col = colList; col != NULL; col = col->next)
    {
    struct lowTypeInfo *lt = col->lowType;
    enum lowTypes type = lt->type;
    if((col->isArray || col->isList) && type != t_object && type != t_simple)
	return FALSE;
    }
return TRUE;
}

boolean noMoreColumnsToInsert(struct column *colList)
{
struct column *col;
for(col = colList; col != NULL; col = col->next)
    {
    if(col->obType == NULL)
	return FALSE;
    }
return TRUE;
}

void dynamicSaveToDb(struct dbObject *table, FILE *f, FILE *hFile)
/* create C code that will save a table structure to the database */
{
char *tableName = table->name;
struct column *col;
struct dyString *colInsert = newDyString(1024);           /* code to associate columns with printf format characters the insert statement */
struct dyString *stringDeclarations = newDyString(1024);  /* code to declare necessary strings */
struct dyString *stringFrees = newDyString(1024);         /* code to free necessary strings */
struct dyString *update = newDyString(1024);              /* code to do the update statement itself */
struct dyString *stringArrays = newDyString(1024);        /* code to convert arrays to strings */
boolean hasArray = FALSE;
dynamicSaveToDbPrintPrototype(tableName, hFile, TRUE);
fprintf(hFile, "\n");
dynamicSaveToDbPrintPrototype(tableName, f, FALSE);
fprintf(f, "{\n");
fprintf(f, "struct dyString *update = newDyString(updateSize);\n");
dyStringPrintf(update, "dyStringPrintf(update, \"insert into %%s values ( ");
dyStringPrintf(stringDeclarations, "char ");
for (col = table->columnList; col != NULL; col = col->next)
    {
    char *colName = col->name;
    char *outString = NULL; /* printf formater for column, i.e. %d for int, '%s' for string */
    struct dbObject *obType = col->obType;
    struct lowTypeInfo *lt = col->lowType;
    enum lowTypes type = lt->type;
    char colInsertBuff[256]; /* what variable name  matches up with the printf format character in outString */
    boolean colInsertFlag = TRUE; /* if column is not a native type insert NULL with no associated variable */
    switch(type)
	{
	case t_char:
	    outString = (col->fixedSize > 0) ? "'%s'" : "'%c'";
	    break;
	case t_string:
	    outString = "'%s'";
	    break;
	default:
	    outString = lt->outFormat;
	    break;
	}

    sprintf(colInsertBuff, " el->%s", colName);

    /* it gets pretty ugly here as we have to handle arrays of objects.. */
    if(col->isArray || col->isList || type == t_object || type == t_simple)
	{
	/* if we have a basic array type convert it to a string representation and insert into db */
	if(type != t_object && type != t_simple )
	    {
	    hasArray = TRUE;
	    outString = "'%s'";
	    /* if this is the last array put a semi otherwise a comment */
	    if(lastArrayType(col->next))
		dyStringPrintf(stringDeclarations, " *%sArray;", colName);
	    else 
		dyStringPrintf(stringDeclarations, " *%sArray,", colName);
	    /* set up call to convert array to char * */
	    if(col->fixedSize)
		dyStringPrintf(stringArrays, "%sArray = sql%sArrayToString(el->%s, %d);\n", 
			       colName, lt->listyName, colName, col->fixedSize);
	    else
		dyStringPrintf(stringArrays, "%sArray = sql%sArrayToString(el->%s, el->%s);\n", 
			       colName, lt->listyName, colName, col->linkedSizeName);
	    /* code to free allocated strings */
	    dyStringPrintf(stringFrees, "freez(&%sArray);\n", colName);
	    sprintf(colInsertBuff, " %sArray ", colName);
	    }
	/* if we have an object, or simple data type just insert NULL,
	 * don't wrap the whole thing up into one string.*/
	else
	    {
	    warn("The user defined type \"%s\" in table \"%s\" will be saved to the database as NULL.", 
		 col->obType->name, tableName);
	    outString = " NULL ";
	    colInsertFlag = FALSE;
	    }
	}
    /* can't have a comma at the end of the list */
    if(col->next == NULL)
	dyStringPrintf(update, "%s", outString);
    else
	dyStringPrintf(update, "%s,",outString);

    /* if we still have more columns to insert add a comma */
    if(!noMoreColumnsToInsert(col->next))
	strcat(colInsertBuff, ", ");

    /* if we have a column to append do so */
    if(colInsertFlag)
	dyStringPrintf(colInsert, "%s", colInsertBuff);
    }
if(hasArray)
    {
    fprintf(f, "%s\n", stringDeclarations->string);
    }
fprintf(f, "%s", stringArrays->string);
fprintf(f, "%s", update->string);
fprintf(f, ")\", \n\ttableName, ");
fprintf(f, "%s);\n", colInsert->string);
fprintf(f, "sqlUpdate(conn, update->string);\n");
fprintf(f, "freeDyString(&update);\n");
if(hasArray)
    {
    fprintf(f, "%s", stringFrees->string);
    }
fprintf(f, "}\n\n");
dyStringFree(&colInsert);
dyStringFree(&stringDeclarations);
dyStringFree(&stringFrees);
dyStringFree(&update);
}

void dynamicSaveToDbEscapedPrintPrototype(char *tableName, FILE *f, boolean addSemi)
/* Print out function prototype and opening comment. */
{
fprintf(f,
	"void %sSaveToDbEscaped(struct sqlConnection *conn, struct %s *el, char *tableName, int updateSize)%s\n", 
	tableName, tableName, (addSemi ? ";" : ""));
fprintf(f,
	"/* Save %s as a row to the table specified by tableName. \n", tableName);
fprintf(f, " * As blob fields may be arbitrary size updateSize specifies the approx size.\n");
fprintf(f, " * of a string that would contain the entire query. Automatically \n");
fprintf(f, " * escapes all simple strings (not arrays of string) but may be slower than %sSaveToDb().\n", tableName);
fprintf(f, " * For example automatically copies and converts: \n");
fprintf(f, " * \"autosql's features include\" --> \"autosql\\'s features include\" \n");
fprintf(f, " * before inserting into database. */ \n");
}

boolean lastStringType(struct column *colList)
/* if there are any more string types returns TRUE else returns false */
{
struct column *col;
for(col = colList; col != NULL; col = col->next)
    {
    struct lowTypeInfo *lt = col->lowType;
    enum lowTypes type = lt->type;
    if(type == t_char || type == t_string || type == t_lstring || ((col->isArray || col->isList) && type != t_object && type != t_simple))
	return FALSE;
    }
return TRUE;
}

void dynamicSaveToDbEscaped(struct dbObject *table, FILE *f, FILE *hFile)
/* create C code that will save a table structure to the database with 
 * all strings escaped. */
{
char *tableName = table->name;
struct column *col;
/* We need to do a lot of things with the string datatypes use
 * these buffers to only cycle through columns once */
struct dyString *colInsert = newDyString(1024);          /* code to associate columns with printf format characters the insert statement */
struct dyString *stringDeclarations = newDyString(1024); /* code to declare necessary strings */
struct dyString *stringFrees = newDyString(1024);        /* code to free necessary strings */
struct dyString *update = newDyString(1024);             /* code to do the update statement itself */
struct dyString *stringEscapes = newDyString(1024);      /* code to escape strings */
struct dyString *stringArrays = newDyString(1024);       /* code to convert arrays to strings */
boolean hasString = FALSE;
boolean hasArray = FALSE;
dynamicSaveToDbEscapedPrintPrototype(tableName, hFile, TRUE);
fprintf(hFile, "\n");
dynamicSaveToDbEscapedPrintPrototype(tableName, f, FALSE);
fprintf(f, "{\n");
fprintf(f, "struct dyString *update = newDyString(updateSize);\n");
dyStringPrintf(update, "dyStringPrintf(update, \"insert into %%s values ( ");
dyStringPrintf(stringDeclarations, "char ");

/* loop through each of the columns and add things appropriately */
for (col = table->columnList; col != NULL; col = col->next)
    {
    char *colName = col->name;
    char *outString = NULL; /* printf formater for column, i.e. %d for int, '%s' for string */
    struct lowTypeInfo *lt = col->lowType;
    enum lowTypes type = lt->type;
    char colInsertBuff[256]; /* what variable name  matches up with the printf format character in outString */
    struct dbObject *obType = col->obType;
    boolean colInsertFlag = TRUE; /* if column is not a native type insert NULL with no associated variable */
    switch(type)
	{
	case t_char:
	case t_string:
	case t_lstring:
	    /* if of string type have to do all the work of declaring, escaping,
	     * and freeing */
	    if(!col->isArray && !col->isList)
		{
		hasString = TRUE;
		outString = "'%s'";
		/* code to escape strings */
		dyStringPrintf(stringEscapes, "%s = sqlEscapeString(el->%s);\n", colName, colName);
		/* code to free strings */
		dyStringPrintf(stringFrees, "freez(&%s);\n", colName);
		sprintf(colInsertBuff, " %s", colName);
		if(lastStringType(col->next))
		    dyStringPrintf(stringDeclarations, " *%s;", colName);
		else 
		    dyStringPrintf(stringDeclarations, " *%s,", colName);
		}
	    break;
	default:
	    outString = lt->outFormat;
	    sprintf(colInsertBuff, "el->%s ", colName);
	    break;
	}
    if(col->isArray || col->isList || type == t_object || type == t_simple)
	{
	/* if we have a basic array type convert it to a string representation and insert into db */
	if(type != t_object && type != t_simple )
	    {
	    hasArray = TRUE;
	    outString = "'%s'";
	    if(lastStringType(col->next) && lastArrayType(col->next))
		dyStringPrintf(stringDeclarations, " *%sArray;", colName);
	    else 
		dyStringPrintf(stringDeclarations, " *%sArray,", colName);
	    if(col->fixedSize)
		dyStringPrintf(stringArrays, "%sArray = sql%sArrayToString(el->%s, %d);\n", colName, lt->listyName, colName, col->fixedSize);
	    else
		dyStringPrintf(stringArrays, "%sArray = sql%sArrayToString(el->%s, el->%s);\n", colName, lt->listyName, colName, col->linkedSizeName);
	    dyStringPrintf(stringFrees, "freez(&%sArray);\n", colName);
	    sprintf(colInsertBuff, " %sArray ", colName);
	    }
	/* if we have an object, or simple data type just insert NULL, don't wrap the whole thing up into one string.*/
	else
	    {
	    warn("The user defined type \"%s\" in table \"%s\" will be saved to the database as NULL.", col->obType->name, tableName);
	    outString = " NULL ";
	    colInsertFlag = FALSE;
	    }
	}
    /* can't have comma at the end of the insert */
    if(col->next == NULL)
	dyStringPrintf(update, "%s", outString);
    else
	dyStringPrintf(update, "%s,",outString);

    /* if we still have more columns to insert add a comma */
    if(!noMoreColumnsToInsert(col->next))
	strcat(colInsertBuff, ", ");
    /* if we have a column to append do so */
    if(colInsertFlag)
	dyStringPrintf(colInsert, "%s", colInsertBuff);
    }
if(hasString || hasArray)
    {
    fprintf(f, "%s\n", stringDeclarations->string);
    fprintf(f, "%s\n", stringEscapes->string);
    }
fprintf(f, "%s", stringArrays->string);
fprintf(f, "%s", update->string);
fprintf(f, ")\", \n\ttableName, ");
fprintf(f, "%s);\n", colInsert->string);
fprintf(f, "sqlUpdate(conn, update->string);\n");
fprintf(f, "freeDyString(&update);\n");
if(hasString)
    {
    fprintf(f, "%s", stringFrees->string);
    }
fprintf(f, "}\n\n");
dyStringFree(&colInsert);
dyStringFree(&stringDeclarations);
dyStringFree(&stringEscapes);
dyStringFree(&stringFrees);
dyStringFree(&update);
}



void makeFree(struct dbObject *table, FILE *f, FILE *hFile)
/* Make function that frees a dynamically allocated table. */
{
char *tableName = table->name;
struct column *col;

fprintf(hFile, "void %sFree(struct %s **pEl);\n", tableName, tableName);
fprintf(hFile, "/* Free a single dynamically allocated %s such as created\n", tableName);
fprintf(hFile, " * with %sLoad(). */\n\n", tableName);

fprintf(f, "void %sFree(struct %s **pEl)\n", tableName, tableName);
fprintf(f, "/* Free a single dynamically allocated %s such as created\n", tableName);
fprintf(f, " * with %sLoad(). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "struct %s *el;\n", tableName);
fprintf(f, "\n");
fprintf(f, "if ((el = *pEl) == NULL) return;\n");
for (col = table->columnList; col != NULL; col = col->next)
    {
    struct lowTypeInfo *lt = col->lowType;
    enum lowTypes type = lt->type;
    char *colName = col->name;
    struct dbObject *obj;

    if ((obj = col->obType) != NULL)
	{
	if (type == t_object)
	    fprintf(f, "%sFreeList(&el->%s);\n", obj->name, colName);
	else if (type == t_simple)
	    {
	    if (col->isArray && !col->fixedSize)
		fprintf(f, "freeMem(el->%s);\n", colName);
	    }
	}
    else
	{
	if (col->isList)
	    {
	    if (lt->stringy)
		{
		fprintf(f, "/* All strings in %s are allocated at once, so only need to free first. */\n", colName);
		fprintf(f, "if (el->%s != NULL)\n", colName);
		fprintf(f, "    freeMem(el->%s[0]);\n", colName);
		}
	    if (!col->fixedSize)
		fprintf(f, "freeMem(el->%s);\n", colName);
	    }
	else
	    {
	    if (lt->stringy)
		{
		fprintf(f, "freeMem(el->%s);\n", colName);
		}
	    }
	}
    }
fprintf(f, "freez(pEl);\n");
fprintf(f, "}\n\n");
}

void makeFreeList(struct dbObject *table, FILE *f, FILE *hFile)
/* Make function that frees a list of dynamically table. */
{
char *name = table->name;

fprintf(hFile, "void %sFreeList(struct %s **pList);\n", name, name);
fprintf(hFile, "/* Free a list of dynamically allocated %s's */\n\n", name);

fprintf(f, "void %sFreeList(struct %s **pList)\n", name, name);
fprintf(f, "/* Free a list of dynamically allocated %s's */\n", name);
fprintf(f, "{\n");
fprintf(f, "struct %s *el, *next;\n", name);
fprintf(f, "\n");
fprintf(f, "for (el = *pList; el != NULL; el = next)\n");
fprintf(f, "    {\n");
fprintf(f, "    next = el->next;\n");
fprintf(f, "    %sFree(&el);\n", name);
fprintf(f, "    }\n");
fprintf(f, "*pList = NULL;\n");
fprintf(f, "}\n\n");
}


void makeOutput(struct dbObject *table, FILE *f, FILE *hFile)
/* Make function that prints table to tab delimited file. */
{
char *tableName = table->name;
struct column *col;
char *outString = NULL;

fprintf(hFile, 
  "void %sOutput(struct %s *el, FILE *f, char sep, char lastSep);\n", tableName, tableName);
fprintf(hFile, 
  "/* Print out %s.  Separate fields with sep. Follow last field with lastSep. */\n\n", 
  tableName);

fprintf(f, 
  "void %sOutput(struct %s *el, FILE *f, char sep, char lastSep) \n", tableName, tableName);
fprintf(f, 
  "/* Print out %s.  Separate fields with sep. Follow last field with lastSep. */\n", 
  tableName);

fprintf(hFile, 
   "#define %sTabOut(el,f) %sOutput(el,f,'\\t','\\n');\n", tableName, tableName);
fprintf(hFile, 
   "/* Print out %s as a line in a tab-separated file. */\n\n", tableName);

fprintf(hFile, 
   "#define %sCommaOut(el,f) %sOutput(el,f,',',',');\n", tableName, tableName);
fprintf(hFile, 
   "/* Print out %s as a comma separated list including final comma. */\n\n", 
   	tableName);

fprintf(f, "{\n");
fprintf(f, "int i;\n");
for (col = table->columnList; col != NULL; col = col->next)
    {
    char *colName = col->name;
    struct lowTypeInfo *lt = col->lowType;
    enum lowTypes type = lt->type;
    struct dbObject *obType = col->obType;
    char *lineEnd = (col->next != NULL ? "sep" : "lastSep");
    char outChar = 0;
    boolean mightNeedQuotes = FALSE;

    switch(type)
	{
	case t_char:
	    outString = (col->fixedSize > 0) ? "%s" : "%c";
	    mightNeedQuotes = TRUE;
	    break;
	case t_string:
	case t_lstring:
	    outString = "%s";
	    mightNeedQuotes = TRUE;
	    break;
	default:
	    outString = lt->outFormat;
	    break;
	}

    if (col->isList || col->isArray)
	{
	char *indent = "";
	if (obType != NULL)
	    {
	    fprintf(f, "/* Loading %s list. */\n", obType->name);
	    fprintf(f, "    {\n    struct %s *it = el->%s;\n", 
	    	obType->name, col->name);
	    indent = "    ";
	    }
	fprintf(f, "%sif (sep == ',') fputc('{',f);\n", indent);
	if (col->fixedSize)
	    fprintf(f, "%sfor (i=0; i<%d; ++i)\n", indent, col->fixedSize);
	else
	    fprintf(f, "%sfor (i=0; i<el->%s; ++i)\n", indent, col->linkedSize->name);
	fprintf(f, "%s    {\n", indent);
	if (type == t_object)
	    {
	    fprintf(f, "%s    fputc('{',f);\n", indent);
	    fprintf(f, "%s    %sCommaOut(it,f);\n", indent, obType->name);
	    fprintf(f, "%s    it = it->next;\n", indent);
	    fprintf(f, "%s    fputc('}',f);\n", indent);
	    fprintf(f, "%s    fputc(',',f);\n", indent);
	    }
	else if (type == t_simple)
	    {
	    fprintf(f, "%s    fputc('{',f);\n", indent);
	    fprintf(f, "%s    %sCommaOut(&it[i],f);\n", indent, obType->name);
	    fprintf(f, "%s    fputc('}',f);\n", indent);
	    fprintf(f, "%s    fputc(',',f);\n", indent);
	    }
	else
	    {
	    if (mightNeedQuotes)
		fprintf(f, "%s    if (sep == ',') fputc('\"',f);\n", indent);
	    fprintf(f, "%s    fprintf(f, \"%s\", el->%s[i]);\n", indent, outString, 
		colName);
	    if (mightNeedQuotes)
		fprintf(f, "%s    if (sep == ',') fputc('\"',f);\n", indent);
	    fprintf(f, "%s    fputc(',', f);\n", indent);
	    }
	fprintf(f, "%s    }\n", indent);
	fprintf(f, "%sif (sep == ',') fputc('}',f);\n", indent);
	if (obType != NULL)
	    fprintf(f, "    }\n");
	fprintf(f, "fputc(%s,f);\n", lineEnd);
	}
    else
	{
	if (type == t_object)
	    {
	    struct dbObject *obj = col->obType;
	    fprintf(f, "if (sep == ',') fputc('{',f);\n");
	    fprintf(f, "if(el->%s != NULL)", colName);
	    fprintf(f, "    %sCommaOut(el->%s,f);\n", obj->name, col->name);
	    fprintf(f, "if (sep == ',') fputc('}',f);\n");
	    fprintf(f, "fputc(%s,f);\n", lineEnd);
	    }
	else if (type == t_simple)
	    {
	    struct dbObject *obj = col->obType;
	    fprintf(f, "if (sep == ',') fputc('{',f);\n");
	    fprintf(f, "if(el->%s != NULL)", colName);
	    fprintf(f, "    %sCommaOut(&el->%s,f);\n", obj->name, col->name);
	    fprintf(f, "if (sep == ',') fputc('}',f);\n");
	    fprintf(f, "fputc(%s,f);\n", lineEnd);
	    }
	else
	    {
	    if (mightNeedQuotes)
		fprintf(f, "if (sep == ',') fputc('\"',f);\n");
	    fprintf(f, "fprintf(f, \"%s\", el->%s);\n", outString, 
		colName);
	    if (mightNeedQuotes)
		fprintf(f, "if (sep == ',') fputc('\"',f);\n");
	    fprintf(f, "fputc(%s,f);\n", lineEnd);
	    }
	}
    }
fprintf(f, "}\n\n");
}

int main(int argc, char *argv[])
{
struct tokenizer *tkz;
struct dbObject *objList, *obj;
char *outRoot;
char dotC[256];
char dotH[256];
char dotSql[256];
FILE *cFile;
FILE *hFile;
FILE *sqlFile;
char defineName[256];
boolean doDbLoadAndSave = FALSE;
cgiSpoof(&argc, argv);
doDbLoadAndSave = cgiBoolean("dbLink");
if (argc != 3)
    usage();

tkz = newTokenizer(argv[1]);
objList = parseObjects(tkz);
freeTokenizer(&tkz);
outRoot = argv[2];

sprintf(dotC, "%s.c", outRoot);
cFile = mustOpen(dotC, "w");
sprintf(dotH, "%s.h", outRoot);
hFile = mustOpen(dotH, "w");
sprintf(dotSql, "%s.sql", outRoot);
sqlFile = mustOpen(dotSql, "w");

/* Print header comment in all files. */
fprintf(hFile, 
   "/* %s was originally generated by the autoSql program, which also \n"
   " * generated %s and %s.  This header links the database and\n"
   " * the RAM representation of objects. */\n\n",
   dotH, dotC, dotSql);
fprintf(cFile, 
   "/* %s was originally generated by the autoSql program, which also \n"
   " * generated %s and %s.  This module links the database and\n"
   " * the RAM representation of objects. */\n\n",
   dotC, dotH, dotSql);
fprintf(sqlFile, 
   "# %s was originally generated by the autoSql program, which also \n"
   "# generated %s and %s.  This creates the database representation of\n"
   "# an object which can be loaded and saved from RAM in a fairly \n"
   "# automatic way.\n",
   dotSql, dotC, dotH);

/* Bracket H file with definition that keeps it from being included twice. */
sprintf(defineName, "%s_H", outRoot);
touppers(defineName);
fprintf(hFile, "#ifndef %s\n", defineName);
fprintf(hFile, "#define %s\n\n", defineName);
if(doDbLoadAndSave)
    {
    fprintf(hFile, "#ifndef JKSQL_H\n");
    fprintf(hFile, "#include \"jksql.h\"\n");
    fprintf(hFile, "#endif\n\n");
    }

/* Put the usual includes in .c file, and also include .h file we are
 * generating. */
fprintf(cFile, "#include \"common.h\"\n");
fprintf(cFile, "#include \"linefile.h\"\n");
fprintf(cFile, "#include \"dystring.h\"\n");
fprintf(cFile, "#include \"jksql.h\"\n");
fprintf(cFile, "#include \"%s\"\n", dotH);
fprintf(cFile, "\n");

/* Process each object in specification file and output to .c, 
 * .h, and .sql. */
for (obj = objList; obj != NULL; obj = obj->next)
    {
    cTable(obj, hFile);
    if (obj->isTable)
	{
	sqlTable(obj, sqlFile);
	if (!objectHasVariableLists(obj) && !objectHasSubObjects(obj))
	    staticLoadRow(obj, cFile, hFile);
	dynamicLoadRow(obj, cFile, hFile);
	dynamicLoadAll(obj, cFile, hFile);
	dynamicLoadAllByChar(obj, cFile, hFile);
	if(doDbLoadAndSave)
	    {
	    dynamicLoadByQuery(obj, cFile, hFile);
	    dynamicSaveToDb(obj, cFile, hFile);
	    dynamicSaveToDbEscaped(obj, cFile, hFile);
	    }
	}
    makeCommaIn(obj, cFile, hFile);
    if (!obj->isSimple)
	{
	makeFree(obj, cFile, hFile);
	makeFreeList(obj, cFile, hFile);
	}
    makeOutput(obj, cFile, hFile);
    printf("Made %s object\n", obj->name);
    }

fprintf(cFile, "/* -------------------------------- End autoSql Generated Code -------------------------------- */\n\n");
fprintf(hFile, "/* -------------------------------- End autoSql Generated Code -------------------------------- */\n\n");
/* Finish off H file bracket. */
fprintf(hFile, "#endif /* %s */\n\n", defineName);
return 0;
}

