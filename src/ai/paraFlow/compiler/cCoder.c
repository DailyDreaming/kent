/* cCoder - produce C code from type-checked parse tree. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "recodedType.h"
#include "codedType.h"
#include "cCoder.h"

#define prefix "pf_"

static void codeStatement(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp);
/* Emit code for one statement. */

static int codeExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef);
/* Emit code for one expression.  Returns how many items added
 * to stack. */

static void codeScope( struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean printPfInit, boolean checkForExterns);
/* Print types and then variables from scope. */

static void codeScopeVars(struct pfCompile *pfc, FILE *f, 
	struct pfScope *scope, boolean zeroUnitialized);
/* Print out variable declarations associated with scope. */

static void printPreamble(struct pfCompile *pfc, FILE *f, char *fileName, boolean doExtern)
/* Print out C code for preamble. */
{
fprintf(f, "/* This file is a translation of %s by paraFlow. */\n", 
	fileName);
fprintf(f, "\n");
fprintf(f, "#include \"pfPreamble.h\"\n");
fprintf(f, "\n");
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_Var _pf_var_zero;   /* Helps initialize vars to zero. */\n");
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_Array %sargs;	    /* Command line arguments go here. */\n",
	prefix);
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_String %sprogramName; /* Name of program (argv[0]) */\n",
	prefix);
fprintf(f, "\n");
}

static void printSysVarsAndPrototypes(FILE *f)
/* Print stuff needed for main() */
{
fprintf(f, 
"void _pf_init_args(int argc, char **argv, _pf_String *retProg, _pf_Array *retArgs, char *environ[]);\n");
fprintf(f, "\n");
}

static char *localTypeTableType = "_pf_local_type_info";
static char *localTypeTableName = "_pf_lti";

static void codeLocalTypeRef(FILE *f, int ref)
/* Print out local type reference. */
{
fprintf(f, "%s[%d].id", localTypeTableName, ref);
}

static void codeLocalTypeTableName(FILE *f, char *module)
/* Print out local type table name. */
{
fprintf(f, "struct %s %s_%s", localTypeTableType, localTypeTableName, module);
}

static char *typeKey(struct pfCompile *pfc, struct pfBaseType *base)
/* Return key for type if available, or NULL */
{
if (base == pfc->bitType)
    return "Bit";
else if (base == pfc->byteType)
    return "Byte";
else if (base == pfc->shortType)
    return "Short";
else if (base == pfc->intType)
    return "Int";
else if (base == pfc->longType)
    return "Long";
else if (base == pfc->floatType)
    return "Float";
else if (base == pfc->doubleType)
    return "Double";
else if (base == pfc->stringType)
    return "String";
else if (base == pfc->varType)
    return "Var";
else if (base == pfc->arrayType)
    return "Array";
else if (base == pfc->listType)
    return "List";
else if (base == pfc->dirType)
    return "Dir";
else if (base == pfc->treeType)
    return "Tree";
else 
    return NULL;
}

static void printType(struct pfCompile *pfc, FILE *f, struct pfBaseType *base)
/* Print out type info for C. */
{
char *s = typeKey(pfc, base);
if (s == NULL)
    fprintf(f, "struct %s*", base->name);
else
    fprintf(f, "_pf_%s", s);
}

static char *stackName = "_pf_stack";
static char *stackType = "_pf_Stack";

static void codeStackAccess(FILE *f, int offset)
/* Print out code to access stack at offset. */
{
fprintf(f, "%s[%d]", stackName, offset);
}

static void codeStackFieldAccess(FILE *f, char *field, int offset)
/* Code access to field on stack at offset. */
{
codeStackAccess(f, offset);
fprintf(f, ".%s", field);
}

struct dyString *varName(struct pfCompile *pfc, struct pfVar *var)
/* Return  variable name from C point of view.  (Easy unless it's static). */
{
struct dyString *name = dyStringNew(256);
struct pfType *type = var->ty;
if (type->isStatic)
    {
    /* Find enclosing function. */
    struct pfParse *toDec, *classDec;
    char *className = "";
    for (toDec = var->parse; toDec != NULL; toDec = toDec->parent)
	if (toDec->type == pptToDec)
	    break;
    if (toDec == NULL)
        internalErr();
    for (classDec = toDec->parent; classDec!=NULL; classDec = classDec->parent)
	if (toDec->type == pptClass)
	    break;
    if (classDec != NULL)
        className = classDec->name;
    dyStringPrintf(name, "_pf_sta_%d_%s_%s_%s", 
		var->scope->id, className, toDec->name, var->name);
    }
else
    {
    dyStringAppend(name, prefix);
    dyStringAppend(name, var->name);
    }
return name;
}

static void printVarName(struct pfCompile *pfc, FILE *f, struct pfVar *var)
/* Print variable name from C point of view.  (Easy unless it's static). */
{
struct dyString *name = varName(pfc, var);
fprintf(f, "%s", name->string);
dyStringFree(&name);
}

static char *vTypeKey(struct pfCompile *pfc, struct pfBaseType *base)
/* Return typeKey, or "v" is that would be NULL.  The v is used
 * to access void pointer on stack. */
{
char *s = typeKey(pfc, base);
if (s == NULL)
    s = "v";
return s;
}

static void codeParamAccess(struct pfCompile *pfc, FILE *f, 
	struct pfBaseType *base, int offset)
/* Print out code to access paramater of given type at offset. */
{
char *s = vTypeKey(pfc, base);
codeStackFieldAccess(f, s, offset);
}

static void rPrintFields(struct pfCompile *pfc, 
	FILE *f, struct pfBaseType *base)
/* Print out fields - parents first */
{
struct pfType *type;
if (base->parent != NULL)
    rPrintFields(pfc, f, base->parent);
for (type = base->fields; type != NULL; type = type->next)
    {
    printType(pfc, f, type->base);
    fprintf(f, " %s;\n", type->fieldName);
    }
}


static boolean isInitialized(struct pfVar *var)
/* Return TRUE if variable is initialized on declaration in  parse tree. */
{
struct pfParse *pp = var->parse;
if (pp->type != pptVarInit)
    errAbort("Expecting pptVarInit got %s for var %s",
    	pfParseTypeAsString(pp->type), var->name);
return pp->children->next->next != NULL;
}

static void codeMethodName(struct pfCompile *pfc, struct pfToken *tok, 
	FILE *f, struct pfBaseType *base, char *name, int stack)
/* Find method in current class or one of it's parents, and print
 * call to it */
{
if (base == pfc->stringType)
    {
    fprintf(f, "_pf_cm_string_%s", name);
    }
else
    {
    struct pfType *method = NULL;
    while (base != NULL)
	{
	for (method = base->methods; method != NULL; method = method->next)
	    {
	    if (sameString(method->fieldName, name))
		break;
	    }
	if (method != NULL)
	     break;
	base = base->parent;
	}
    if (base != NULL)
	{
	if (method->tyty == tytyVirtualFunction)
	    {
	    fprintf(f, "%s[%d].Obj->_pf_polyTable[%d]",
	    	stackName, stack, method->polyOffset);
	    }
	else
	    fprintf(f, "_pf_cm%d_%s_%s", base->scope->id, base->name, name);
	}
    else
	internalErrAt(tok);
    }
}

static int codeCall(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for a function call. */
{
struct pfParse *function = pp->children;
struct pfParse *inTuple = function->next;
struct pfType *outTuple = function->ty->children->next;
int outCount = slCount(outTuple->children);
struct pfParse *in;

if (function->type == pptDot)
    {
    struct pfParse *dotList = function->children;
    struct pfParse *class = NULL, *method = NULL;
    int dotCount = slCount(dotList);
    assert(dotCount >= 2);

    /* Push object pointer on stack. */
    if (dotCount == 2)
        {
	/* Slightly optimize simple case. */
	class = dotList;
	method = dotList->next;
	codeExpression(pfc, f, class, stack, FALSE);
	}
    else
        {
	/* More general case here. */
	struct pfParse *p;
	for (p = dotList->next; p->next != NULL; p = p->next)
	    class = p;
	method = class->next;

	/* Swap out method for null in dot-chain so that codeExpression
	 * will end up pushing class on stack. */
	class->next = NULL;
	codeExpression(pfc, f, function, stack, FALSE);
	class->next = method;
	}
    /* Put rest of input on the stack, and print call with mangled function name. */
    codeExpression(pfc, f, inTuple, stack+1, TRUE);
    codeMethodName(pfc, pp->tok, f, class->ty->base, method->name, stack);
    fprintf(f, "(%s+%d);\n", stackName, stack);
    }
else
    {
    codeExpression(pfc, f, inTuple, stack, TRUE);
    assert(function->type == pptVarUse);
    if (stack == 0)
	fprintf(f, "%s%s(%s);\n", prefix, pp->name, stackName);
    else
	fprintf(f, "%s%s(%s+%d);\n", prefix, pp->name, stackName, stack);
    }
return outCount;
}

static void codeRunTimeError(struct pfCompile *pfc, FILE *f,
	struct pfToken *tok, char *message)
/* Print code for a run time error message. */
{
char *file;
int line, col;
pfSourcePos(tok->source, tok->text, &file, &line, &col);
fprintf(f, "errAbort(\"\\nRun time error line %d col %d of %s: %s\");\n", 
	line+1, col+1, file, message);
}

static void startCleanTemp(FILE *f)
/* Declare a temp variable to assist in cleanup */
{
fprintf(f, " {\n struct _pf_object *_pf_tmp = (struct _pf_object *)\n  ");
}

static void codeForType(struct pfCompile *pfc, FILE *f, struct pfType *type)
/* Print out code to access type ID */
{
codeLocalTypeRef(f, recodedTypeId(pfc, type));
}

static void endCleanTemp(struct pfCompile *pfc, FILE *f, struct pfType *type)
{
fprintf(f, ";\n");
fprintf(f, " if (_pf_tmp != 0 && --_pf_tmp->_pf_refCount <= 0)\n");
fprintf(f, "   _pf_tmp->_pf_cleanup(_pf_tmp, ");
codeForType(pfc, f, type);
fprintf(f, ");\n");
fprintf(f, " }\n");
}

static void bumpStackRefCount(struct pfCompile *pfc,
	FILE *f, struct pfType *type, int stack)
/* If type is reference counted, bump up refCount of top of stack. */
{
struct pfBaseType *base = type->base;

if (base == pfc->varType)
    {
    fprintf(f, "_pf_var_link(");
    codeStackAccess(f, stack);
    fprintf(f, ".Var");
    fprintf(f, ");\n");
    }
else if (base->needsCleanup)
    {
    fprintf(f, "if (0 != ");
    codeStackAccess(f, stack);
    fprintf(f, ".Obj) ");
    codeStackAccess(f, stack);
    fprintf(f, ".Obj->_pf_refCount+=1;\n");
    }
}

static int pushArrayIndexAndBoundsCheck(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Put collection and index on stack.  Check index is in range. 
 * Return offset to index. */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfBaseType *colBase = collection->ty->base;
struct pfParse *index = collection->next;

/* Push array and index onto expression stack */
int offset = codeExpression(pfc, f, collection, stack, FALSE);
codeExpression(pfc, f, index, stack+offset, FALSE);

/* Do bounds checking */
fprintf(f, "if (");
codeParamAccess(pfc, f, pfc->intType, stack+offset);
fprintf(f, "< 0 || ");
codeParamAccess(pfc, f, pfc->intType, stack+offset);
fprintf(f, " >= ");
codeParamAccess(pfc, f, colBase, stack);
fprintf(f, "->size)\n  ");
codeRunTimeError(pfc, f, pp->tok, "array access out of bounds");
return offset;
}

static void codeArrayAccess(struct pfCompile *pfc, FILE *f,
	struct pfBaseType *base, int stack, int indexOffset)
/* Print out code to access array (on either left or right
 * side */
{
fprintf(f, "((");
printType(pfc, f, base);
fprintf(f, "*)(");
codeParamAccess(pfc, f, pfc->arrayType, stack);
fprintf(f, "->elements + %d * ",  base->size);
codeParamAccess(pfc, f, pfc->intType, stack+indexOffset);
fprintf(f, "))[0]");
}

static void codeAccessToByteInString(struct pfCompile *pfc, FILE *f,
	struct pfBaseType *base, int stack, int indexOffset)
/* Print out code to a byte in string. */
{
codeParamAccess(pfc, f, pfc->stringType, stack);
fprintf(f, "->s[");
codeParamAccess(pfc, f, pfc->intType, stack+indexOffset);
fprintf(f, "]");
}


static int codeIndexRval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Generate code for index expression (not on left side of
 * assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfParse *index = collection->next;
struct pfBaseType *colBase = collection->ty->base;
if (colBase == pfc->arrayType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, stack);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, " = ");
    codeArrayAccess(pfc, f, outType->base, stack, indexOffset);
    fprintf(f, ";\n");
    if (addRef) 
    	bumpStackRefCount(pfc, f, outType, stack);
    }
else if (colBase == pfc->stringType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, stack);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, " = ");
    codeAccessToByteInString(pfc, f, outType->base, stack, indexOffset);
    fprintf(f, ";\n");
    }
else if (colBase == pfc->dirType)
    {
    int offset = codeExpression(pfc, f, collection, stack, FALSE);
    codeExpression(pfc, f, index, stack+offset, FALSE);
    if (outType->base->needsCleanup)
	{
	fprintf(f, "%s[%d].Obj", stackName, stack);
	fprintf(f, " = ");
        fprintf(f, "_pf_dir_lookup_object(%s+%d, %d);\n", stackName, stack,
		addRef);
	}
    else 
	{
	fprintf(f, "_pf_dir_lookup_number(%s+%d);\n",  stackName, stack);
	}
    }
else
    {
    internalErr();
    }
return 1;
}

static void codeIndexLval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op, int expSize, 
	boolean cleanupOldVal)
/* Generate code for index expression  on left side of assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *collection = pp->children;
struct pfParse *index = collection->next;
struct pfBaseType *colBase = collection->ty->base;
int emptyStack = stack + expSize;
if (colBase == pfc->arrayType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, emptyStack);
    if (outType->base == pfc->varType)
        {
	fprintf(f, "_pf_var_cleanup(");
	codeArrayAccess(pfc, f, outType->base, emptyStack, indexOffset);
	fprintf(f, ");\n");
	}
    else if (outType->base->needsCleanup)
        {
	startCleanTemp(f);
	codeArrayAccess(pfc, f, outType->base, emptyStack, indexOffset);
	endCleanTemp(pfc, f, outType);
	}
    codeArrayAccess(pfc, f, outType->base, emptyStack, indexOffset);
    fprintf(f, " %s ", op);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, ";\n");
    }
else if (colBase == pfc->stringType)
    {
    int indexOffset = pushArrayIndexAndBoundsCheck(pfc, f, pp, emptyStack);
    codeAccessToByteInString(pfc, f, outType->base, emptyStack, indexOffset);
    fprintf(f, " %s ", op);
    codeParamAccess(pfc, f, outType->base, stack);
    fprintf(f, ";\n");
    }
else if (colBase == pfc->dirType)
    {
    int offset = codeExpression(pfc, f, collection, emptyStack, FALSE);
    codeExpression(pfc, f, index, emptyStack+offset, FALSE);
    if (outType->base->needsCleanup)
	{
        fprintf(f, "_pf_dir_add_object(%s+%d, %d);\n", stackName, stack, expSize);
	}
    else 
	{
	fprintf(f, "_pf_dir_add_number(%s+%d, %d);\n",  stackName, stack, expSize);
	}
    }
else
    {
    internalErr();
    }
}

static void codeDotAccess(struct pfCompile *pfc, FILE *f, 
	struct pfParse *pp, int stack)
/* Print out code to access field (on either left or right
 * side */
{
struct pfParse *class = pp->children;
struct pfParse *field = class->next;
struct pfBaseType *base = class->ty->base;
if (base == pfc->stringType || base == pfc->arrayType)
    fprintf(f, "(");
else
    fprintf(f, "((struct %s *)", base->name);
codeParamAccess(pfc, f, base, stack);
fprintf(f, ")");
while (field != NULL)
    {
    fprintf(f, "->%s", field->name);
    field = field->next;
    }
}
	

static int codeDotRval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for . expression (not on left side of
 * assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *class = pp->children;
codeExpression(pfc, f, class, stack, FALSE);
codeParamAccess(pfc, f, outType->base, stack);
fprintf(f, " = ");
codeDotAccess(pfc, f, pp, stack);
fprintf(f, ";\n");
bumpStackRefCount(pfc, f, outType, stack);
return 1;
}

static void codeDotLval(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op, int expSize, 
	boolean cleanupOldVal)
/* Generate code for dot expression  on left side of assignment */
{
struct pfType *outType = pp->ty;
struct pfParse *class = pp->children;
struct pfParse *field = class->next;
int emptyStack = stack + expSize;
codeExpression(pfc, f, class, emptyStack, FALSE);
if (outType->base->needsCleanup)
    {
    startCleanTemp(f);
    codeDotAccess(pfc, f, pp, emptyStack);
    endCleanTemp(pfc, f, outType);
    }
codeDotAccess(pfc, f, pp, emptyStack);
fprintf(f, " %s ", op);
codeParamAccess(pfc, f, outType->base, stack);
fprintf(f, ";\n");
}


static void codeCleanupVarNamed(struct pfCompile *pfc, FILE *f, 
	struct pfType *type, char *name)
/* Emit cleanup code for variable of given type and name. */
{
if (type->base->needsCleanup)
    {
    if (type->base == pfc->varType)
	fprintf(f, "_pf_var_cleanup(%s);\n", name);
    else
	{
	fprintf(f, "if (0!=%s && (%s->_pf_refCount-=1) <= 0)\n", name, name);
	fprintf(f, "   %s->_pf_cleanup(%s, ", name, name);
	codeForType(pfc, f, type);
	fprintf(f, ");\n");
	}
    }
}

static void codeCleanupVar(struct pfCompile *pfc, FILE *f, 
        struct pfVar *var)
/* Emit cleanup code for variable of given type and name. */
{
struct dyString *name = varName(pfc, var);
codeCleanupVarNamed(pfc, f, var->ty, name->string);
dyStringFree(&name);
}

static void codeInitOfType(struct pfCompile *pfc, FILE *f, struct pfType *type)
/* Print out default initialization of type. */
{
if (type->base == pfc->varType)
    fprintf(f, "=_pf_var_zero");
else
    fprintf(f, "=0");
}

static void codeVarsInHelList(struct pfCompile *pfc, FILE *f,
	struct hashEl *helList, boolean zeroUninit)
/* Print out variable declarations in helList. */
{
struct hashEl *hel;
boolean gotVar = FALSE;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct pfVar *var = hel->val;
    struct pfType *type = var->ty;
    if (type->tyty == tytyVariable && !type->isStatic)
        {
	if (var->isExternal)
	    fprintf(f, "extern ");
	printType(pfc, f, type->base);
	fprintf(f, " ");
	printVarName(pfc, f, var);
	if (zeroUninit && !var->isExternal && !isInitialized(var))
	    {
	    codeInitOfType(pfc, f, type);
	    }
	fprintf(f, ";\n");
	gotVar = TRUE;
	}
    }
if (gotVar)
    fprintf(f, "\n");
}

static void codeCleanupVarsInHelList(struct pfCompile *pfc, FILE *f,
	struct hashEl *helList)
/* Print out variable cleanups for helList. */
{
struct hashEl *hel;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct pfVar *var = hel->val;
    if (!var->isExternal && !var->ty->isStatic)
	codeCleanupVar(pfc, f, var);
    }
}

static void codeScopeVars(struct pfCompile *pfc, FILE *f, struct pfScope *scope,
	boolean zeroUninitialized)
/* Print out variable declarations associated with scope. */
{
struct hashEl *helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);
codeVarsInHelList(pfc, f, helList, zeroUninitialized);
hashElFreeList(&helList);
}

static void cleanupScopeVars(struct pfCompile *pfc, FILE *f, 
	struct pfScope *scope)
/* Print out variable declarations associated with scope. */
{
struct hashEl *helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);
slReverse(&helList);
codeCleanupVarsInHelList(pfc, f, helList);
hashElFreeList(&helList);
}



static int lvalOffStack(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op, int expSize,
	boolean cleanupOldVal)
/* Take an lval off of stack. */
{
switch (pp->type)
    {
    case pptVarUse:
    case pptVarInit:
	{
	struct pfBaseType *base = pp->ty->base;
	if (cleanupOldVal && base->needsCleanup)
	    codeCleanupVar(pfc, f, pp->var);
	printVarName(pfc, f, pp->var);
	fprintf(f, " %s ", op);
	codeParamAccess(pfc, f, base, stack);
	fprintf(f, ";\n");
	return 1;
	}
    case pptTuple:
        {
	int total = 0;
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    total += lvalOffStack(pfc, f, p, stack+total, op, expSize-total,
	    	cleanupOldVal);
	return total;
	}
    case pptIndex:
        {
	codeIndexLval(pfc, f, pp, stack, op, expSize, cleanupOldVal);
	return 1;
	}
    case pptDot:
        {
	codeDotLval(pfc, f, pp, stack, op, expSize, cleanupOldVal);
	return 1;
	}
    default:
        fprintf(f, "using %s %s as an lval\n", pp->name, pfParseTypeAsString(pp->type));
	internalErr();
	return 0;
    }
}

static void rCodeTupleType(struct pfCompile *pfc, FILE *f, struct pfType *type)
/* Recursively encode tuple type to output. */
{
struct pfType *t;
fprintf(f, "(");
for (t = type->children; t != NULL; t = t->next)
    {
    struct pfType *tk = t;
    if (t->base == pfc->keyValType)
        tk = t->children->next;
    if (tk->tyty == tytyTuple)
        rCodeTupleType(pfc, f, tk);
    else
        fprintf(f, "x");
    }
fprintf(f, ")");
}

static void codeTupleIntoCollection(struct pfCompile *pfc, FILE *f,
	struct pfType *type, struct pfParse *rval, int stack, int tupleSize)
/* Shovel tuple into array, list, dir, tree. */
{
struct pfBaseType *base = type->base;
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = ");
if (base == pfc->arrayType || base == pfc->listType || base == pfc->treeType
	|| base == pfc->dirType)
    {
    struct pfBaseType *base = type->children->base;
    if (base == pfc->bitType || base == pfc->byteType || base == pfc->shortType
	|| base == pfc->intType || base == pfc->longType || base == pfc->floatType
	|| base == pfc->doubleType || base == pfc->stringType || base == pfc->varType)
	{
	fprintf(f, "_pf_%s_%s_from_tuple(%s+%d, %d, ",
		base->name, type->base->name, stackName, stack, tupleSize);
	codeForType(pfc, f, type), 
	fprintf(f, ", ");
	codeForType(pfc, f, type->children);
	fprintf(f, ");\n");
	}
    else
	{
	fprintf(f, "_pf_tuple_to_%s(%s+%d, ", type->base->name,
		stackName, stack);
	codeForType(pfc, f, type->children);
	fprintf(f, ", \"");
	rCodeTupleType(pfc, f, rval->ty);
	fprintf(f, "\");\n");
	}
    }
else
    {
    internalErr();
    }
}

static void codeTupleIntoClass(struct pfCompile *pfc, FILE *f,
	struct pfParse *lval, struct pfParse *rval, int stack, int tupleSize)
/* Shovel tuple into class. */
{
struct pfBaseType *base = lval->ty->base;
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = ");
fprintf(f, "_pf_tuple_to_class(%s+%d, ",
	stackName, stack);
codeForType(pfc, f, lval->ty);
fprintf(f, ", \"");
rCodeTupleType(pfc, f, rval->ty);
fprintf(f, "\");\n");
}

static int codeInitOrAssign(struct pfCompile *pfc, FILE *f,
	struct pfParse *lval, struct pfParse *rval,
	int stack)
{
int count = codeExpression(pfc, f, rval, stack, TRUE);
if (lval->ty->base->isClass)
    {
    if (rval->ty->base == pfc->tupleType)
	codeTupleIntoClass(pfc, f, lval, rval, stack, count);
    }
else
    {
    if (rval->type == pptUniformTuple)
	codeTupleIntoCollection(pfc, f, lval->ty, rval, stack, count);
    }
return count;
}

static void cantCopeParamType(struct pfParse *pp, int code)
/* Explain that can't deal with parameterized type complications.
 * Hopefully this is temporary. */
{
errAt(pp->tok, "Don't know how to deal with this parameterized type (code %d)", 
	code);
}

static void codeParameterizedType(struct pfCompile *pfc, FILE *f, struct pfParse *varInit,
	struct pfParse *type, boolean isInitialized, int stack)
/* Deal with things like array[10] of ... */
{
boolean gotOne = FALSE;
if (type->type != pptOf)
    cantCopeParamType(type, 1);
for (type = type->children; type != NULL; type = type->next)
    {
    if (type->children != NULL)
	{
	if (isInitialized)
	    errAt(type->tok, "Sorry right now you can't both dimension an array and initialize it.");
	if (type->type != pptTypeName)
	    cantCopeParamType(type, 2);
	if (type->ty->base != pfc->arrayType)
	    cantCopeParamType(type, 3);
	if (gotOne)
	    errAt(type->tok, "Sorry right now you can only parameterize simple arrays");
	gotOne = TRUE;
	codeExpression(pfc, f, type->children, stack, FALSE);
	fprintf(f, "%s[%d].Array = ", stackName, stack);
	fprintf(f, "_pf_dim_array(%s[%d].Int, ", stackName, stack);
	codeForType(pfc, f, type->next->ty);
	fprintf(f, ");\n");
	lvalOffStack(pfc, f, varInit, stack, "=", 1, FALSE);
	}
    }
}

static void codeVarInit(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Generate code for an assignment. */
{
struct pfParse *lval = pp;
struct pfParse *type = pp->children;
struct pfParse *name = type->next;
struct pfParse *rval = name->next;
if (type->children != NULL)
    codeParameterizedType(pfc, f, lval, type, rval != NULL, stack);
if (rval != NULL)
    {
    int count = codeInitOrAssign(pfc, f, lval, rval, stack);
    lvalOffStack(pfc, f, lval, stack, "=", count, FALSE);
    }
}

static int codeAssignment(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Generate code for an assignment. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;

if (lval->type == pptTuple && rval->type == pptTuple)
    {
    for (lval = lval->children, rval = rval->children; 
    	lval != NULL; lval = lval->next, rval = rval->next)
        {
	int count = codeInitOrAssign(pfc, f, lval, rval, stack);
	lvalOffStack(pfc, f, lval, stack, op, count, TRUE);
	}
    }
else
    {
    int count = codeInitOrAssign(pfc, f, lval, rval, stack);
    lvalOffStack(pfc, f, lval, stack, op, count, TRUE);
    }
return 0;
}

static int codeVarUse(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Generate code for moving a variable onto stack. */
{
struct pfBaseType *base = pp->ty->base;
struct dyString *name = varName(pfc, pp->var);
if (addRef && base->needsCleanup)
    {
    if (base == pfc->varType)
	fprintf(f, "_pf_var_link(%s);\n", name->string);
    else
	{
	fprintf(f, "if (0 != %s) ", name->string);
	fprintf(f, "%s->_pf_refCount+=1;\n", name->string);
	}
    }
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = %s;\n", name->string);
dyStringFree(&name);
return 1;
}
	
static int codeBinaryOp(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Emit code for some sort of binary op. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;
codeExpression(pfc, f, lval, stack, TRUE);
codeExpression(pfc, f, rval, stack+1, TRUE);
codeParamAccess(pfc, f, pp->ty->base, stack);
fprintf(f, " = ");
if (lval->ty->base == pfc->stringType)
    {
    fprintf(f, " (_pf_strcmp(%s+%d) %s 0);\n", stackName, stack, op);
    }
else
    {
    codeParamAccess(pfc, f, lval->ty->base, stack);
    fprintf(f, " %s ", op);
    codeParamAccess(pfc, f, rval->ty->base, stack+1);
    fprintf(f, ";\n");
    }
return 1;
}

static int codeUnaryOp(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, char *op)
/* Emit code for some sort of unary op. */
{
struct pfParse *child = pp->children;
struct pfBaseType *base = child->ty->base;
codeExpression(pfc, f, child, stack, TRUE);
codeParamAccess(pfc, f, base, stack);
fprintf(f, " = ");
fprintf(f, "%s", op);
codeParamAccess(pfc, f, base, stack);
fprintf(f, ";\n");
return 1;
}

static int codeStringCat(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Generate code for a string concatenation. */
{
int total = 0;
struct pfParse *p;
for (p = pp->children; p != NULL; p = p->next)
    total += codeExpression(pfc, f, p, stack+total, addRef);
codeParamAccess(pfc, f, pp->ty->base, stack);
fprintf(f, " = ");
fprintf(f, "_pf_string_cat(%s+%d, %d);\n", stackName, stack, total);
return 1;
}

static void castStack(struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	int stack);
/* Cast stack location. */


static void castCallToTuple(struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	int stack)
/* Cast function call results, which are on stack, to types specified by
 * cast list. */
{
struct pfParse *call = pp->children;
struct pfParse *castList = call->next;
struct pfParse *cast;
for (cast = castList; cast != NULL; cast = cast->next)
    {
    if (cast->type != pptPlaceholder)
	castStack(pfc, f, cast, stack);
    stack += 1;
    }
}

static void castStack(struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	int stack)
/* Cast stack location. */
{
switch(pp->type)
    {
    case pptCastBitToBit:
    case pptCastBitToByte:
    case pptCastBitToShort:
    case pptCastBitToInt:
    case pptCastBitToLong:
    case pptCastBitToFloat:
    case pptCastBitToDouble:
    case pptCastByteToByte:
    case pptCastByteToShort:
    case pptCastByteToInt:
    case pptCastByteToLong:
    case pptCastByteToFloat:
    case pptCastByteToDouble:
    case pptCastShortToByte:
    case pptCastShortToShort:
    case pptCastShortToInt:
    case pptCastShortToLong:
    case pptCastShortToFloat:
    case pptCastShortToDouble:
    case pptCastIntToByte:
    case pptCastIntToShort:
    case pptCastIntToInt:
    case pptCastIntToLong:
    case pptCastIntToFloat:
    case pptCastIntToDouble:
    case pptCastLongToByte:
    case pptCastLongToShort:
    case pptCastLongToInt:
    case pptCastLongToLong:
    case pptCastLongToFloat:
    case pptCastLongToDouble:
    case pptCastFloatToByte:
    case pptCastFloatToShort:
    case pptCastFloatToInt:
    case pptCastFloatToLong:
    case pptCastFloatToFloat:
    case pptCastFloatToDouble:
    case pptCastDoubleToByte:
    case pptCastDoubleToShort:
    case pptCastDoubleToInt:
    case pptCastDoubleToLong:
    case pptCastDoubleToFloat:
    case pptCastDoubleToDouble:
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ";\n");
	break;
    case pptCastByteToBit:
    case pptCastShortToBit:
    case pptCastIntToBit:
    case pptCastLongToBit:
    case pptCastFloatToBit:
    case pptCastDoubleToBit:
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, " != 0");
	fprintf(f, ";\n");
	break;
    case pptCastObjectToBit:
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = (0 != ");
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ");\n");
	break;
    case pptCastStringToBit:
        {
	fprintf(f, "{_pf_String _pf_tmp = %s[%d].String; ", stackName, stack);
	fprintf(f, "%s[%d].Bit = (_pf_tmp != 0 && _pf_tmp->size != 0);", stackName, stack);
	fprintf(f, "}\n");
	break;
	}
    case pptCastBitToString:
    case pptCastByteToString:
    case pptCastShortToString:
    case pptCastIntToString:
    case pptCastLongToString:
    case pptCastFloatToString:
    case pptCastDoubleToString:
	{
	char *dest;
	if (pp->type == pptCastFloatToString || pp->type == pptCastDoubleToString)
	    dest = "double";
	else
	    dest = "long";
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = _pf_string_from_%s(", dest);
        codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ");\n");
	break;
	}
    case pptCastTypedToVar:
	{
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".val.%s = ", vTypeKey(pfc, pp->children->ty->base));
	codeParamAccess(pfc, f, pp->children->ty->base, stack);
	fprintf(f, ";\n");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".typeId = ");
	codeForType(pfc, f, pp->children->ty);
	fprintf(f, ";\n");
	break;
	}
    case pptCastVarToTyped:
        {
	fprintf(f, "if (");
	codeForType(pfc, f, pp->ty);
	fprintf(f, " != ");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".typeId)\n");
	fprintf(f, "if (!_pf_check_types(");
	codeForType(pfc, f, pp->ty);
	fprintf(f, ", ");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".typeId))\n");
	codeRunTimeError(pfc, f, pp->tok, "run-time type mismatch");
        codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
        codeParamAccess(pfc, f, pfc->varType, stack);
	fprintf(f, ".val.%s;\n", vTypeKey(pfc, pp->ty->base));
	break;
	}
    case pptCastCallToTuple:
        castCallToTuple(pfc, f, pp, stack);
	break;
    default:
	{
	internalErr();
	break;
	}
    }
}

static int codeStringAppend(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack)
/* Emit code to append to a string. */
{
struct pfParse *lval = pp->children;
struct pfParse *rval = lval->next;

codeExpression(pfc, f, lval, stack, FALSE);
fprintf(f, "if (%s[%d].String == 0)\n", stackName, stack);
codeRunTimeError(pfc, f, lval->tok, "string is nil");
codeExpression(pfc, f, rval, stack+1, TRUE);
fprintf(f, "_pf_cm_string_append(%s+%d);\n", stackName, stack);
return 0;
}

static int codeExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef)
/* Emit code for one expression.  Returns how many items added
 * to stack. */
{
switch (pp->type)
    {
    case pptTuple:
    case pptUniformTuple:
    case pptKeyVal:
        {
	int total = 0;
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    total += codeExpression(pfc, f, p, stack+total, addRef);
	return total;
	}
    case pptCall:
	return codeCall(pfc, f, pp, stack);
    case pptAssignment:
	return codeAssignment(pfc, f, pp, stack, "=");
    case pptPlusEquals:
	if (pp->ty->base == pfc->stringType)
	    return codeStringAppend(pfc, f, pp, stack);
	else
	    return codeAssignment(pfc, f, pp, stack, "+=");
    case pptMinusEquals:
	return codeAssignment(pfc, f, pp, stack, "-=");
    case pptMulEquals:
	return codeAssignment(pfc, f, pp, stack, "*=");
    case pptDivEquals:
	return codeAssignment(pfc, f, pp, stack, "/=");
    case pptVarInit:
    	codeVarInit(pfc, f, pp, stack);
	return 0;
    case pptVarUse:
	return codeVarUse(pfc, f, pp, stack, addRef);
    case pptIndex:
         return codeIndexRval(pfc, f, pp, stack, addRef);
    case pptDot:
         return codeDotRval(pfc, f, pp, stack);
    case pptPlus:
        return codeBinaryOp(pfc, f, pp, stack, "+");
    case pptMinus:
        return codeBinaryOp(pfc, f, pp, stack, "-");
    case pptMul:
        return codeBinaryOp(pfc, f, pp, stack, "*");
    case pptDiv:
        return codeBinaryOp(pfc, f, pp, stack, "/");
    case pptMod:
        return codeBinaryOp(pfc, f, pp, stack, "%");
    case pptGreater:
        return codeBinaryOp(pfc, f, pp, stack, ">");
    case pptGreaterOrEquals:
        return codeBinaryOp(pfc, f, pp, stack, ">=");
    case pptSame:
        return codeBinaryOp(pfc, f, pp, stack, "==");
    case pptNotSame:
        return codeBinaryOp(pfc, f, pp, stack, "!=");
    case pptLess:
        return codeBinaryOp(pfc, f, pp, stack, "<");
    case pptLessOrEquals:
        return codeBinaryOp(pfc, f, pp, stack, "<=");
    case pptLogAnd:
        return codeBinaryOp(pfc, f, pp, stack, "&&");
    case pptLogOr:
        return codeBinaryOp(pfc, f, pp, stack, "||");
    case pptBitAnd:
        return codeBinaryOp(pfc, f, pp, stack, "&");
    case pptBitOr:
        return codeBinaryOp(pfc, f, pp, stack, "|");
    case pptShiftLeft:
        return codeBinaryOp(pfc, f, pp, stack, "<<");
    case pptShiftRight:
        return codeBinaryOp(pfc, f, pp, stack, ">>");
    case pptNegate:
         return codeUnaryOp(pfc, f, pp, stack, "-");
    case pptFlipBits:
         return codeUnaryOp(pfc, f, pp, stack, "~");
    case pptNot:
         return codeUnaryOp(pfc, f, pp, stack, "!");
    case pptConstBit:
    case pptConstByte:
    case pptConstShort:
    case pptConstInt:
    case pptConstLong:
    case pptConstFloat:
    case pptConstDouble:
        {
	struct pfToken *tok = pp->tok;
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = ");
	switch (tok->type)
	    {
	    case pftString:
		assert(pp->type == pptConstBit);
	        fprintf(f, "1;\n");
		break;
	    case pftFloat:
	        fprintf(f, "%f;\n", pp->tok->val.x);
		break;
	    case pftLong:
		fprintf(f, "%lldLL;\n", pp->tok->val.l);
		break;
	    case pftInt:
		fprintf(f, "%d;\n", pp->tok->val.i);
		break;
	    }
	return 1;
	}
    case pptConstString:
        {
	assert(pp->tok->type == pftString);
	codeParamAccess(pfc, f, pp->ty->base, stack);
	fprintf(f, " = _pf_string_from_const(");
	printEscapedString(f, pp->tok->val.s);
	fprintf(f, ");\n");
	return 1;
	}
    case pptConstZero:
        {
	if (pp->ty->base == pfc->varType)
	    {
	    fprintf(f, "%s[%d].Var.typeId = 0;\n", stackName, stack);
	    fprintf(f, "%s[%d].Var.val.Int = 0;\n", stackName, stack);
	    }
	else
	    {
	    codeParamAccess(pfc, f, pp->ty->base, stack);
	    fprintf(f, " = 0;\n");
	    }
	return 1;
	}
    case pptStringCat:
        {
	return codeStringCat(pfc, f, pp, stack, addRef);
	}
    case pptCastBitToBit:
    case pptCastBitToByte:
    case pptCastBitToShort:
    case pptCastBitToInt:
    case pptCastBitToLong:
    case pptCastBitToFloat:
    case pptCastBitToDouble:
    case pptCastByteToBit:
    case pptCastByteToByte:
    case pptCastByteToShort:
    case pptCastByteToInt:
    case pptCastByteToLong:
    case pptCastByteToFloat:
    case pptCastByteToDouble:
    case pptCastShortToBit:
    case pptCastShortToByte:
    case pptCastShortToShort:
    case pptCastShortToInt:
    case pptCastShortToLong:
    case pptCastShortToFloat:
    case pptCastShortToDouble:
    case pptCastIntToBit:
    case pptCastIntToByte:
    case pptCastIntToShort:
    case pptCastIntToInt:
    case pptCastIntToLong:
    case pptCastIntToFloat:
    case pptCastIntToDouble:
    case pptCastLongToBit:
    case pptCastLongToByte:
    case pptCastLongToShort:
    case pptCastLongToInt:
    case pptCastLongToLong:
    case pptCastLongToFloat:
    case pptCastLongToDouble:
    case pptCastFloatToBit:
    case pptCastFloatToByte:
    case pptCastFloatToShort:
    case pptCastFloatToInt:
    case pptCastFloatToLong:
    case pptCastFloatToFloat:
    case pptCastFloatToDouble:
    case pptCastDoubleToBit:
    case pptCastDoubleToByte:
    case pptCastDoubleToShort:
    case pptCastDoubleToInt:
    case pptCastDoubleToLong:
    case pptCastDoubleToFloat:
    case pptCastDoubleToDouble:
    case pptCastObjectToBit:
    case pptCastStringToBit:
    case pptCastBitToString:
    case pptCastByteToString:
    case pptCastShortToString:
    case pptCastIntToString:
    case pptCastLongToString:
    case pptCastFloatToString:
    case pptCastDoubleToString:
    case pptCastTypedToVar:
    case pptCastVarToTyped:
    case pptCastCallToTuple:
	{
	codeExpression(pfc, f, pp->children, stack, addRef);
	castStack(pfc, f, pp, stack);
	return 1;
	}
    default:
	{
	fprintf(f, "(%s expression)\n", pfParseTypeAsString(pp->type));
	return 0;
	}
    }
}

static void expandDottedName(struct pfCompile *pfc,
	char *fullName, int maxSize, struct pfParse *pp)
/* Fill in fullName with this.that.whatever. */
{
switch (pp->type)
    {
    case pptVarUse:
	{
	struct dyString *name = varName(pfc, pp->var);
        safef(fullName, maxSize, "%s", name->string);
	dyStringFree(&name);
	break;
	}
    case pptDot:
        {
	int curSize = 0, itemSize;
	boolean isFirst = TRUE;
	for (pp = pp->children; pp != NULL; pp = pp->next)
	    {
	    if (isFirst)
	        {
		struct dyString *name = varName(pfc, pp->var);
		safef(fullName, maxSize, "%s", name->string);
		curSize = name->stringSize;
		if (curSize >= maxSize)
		    errAt(pp->tok, "Dotted name too long");
		strcpy(fullName, name->string);
		dyStringFree(&name);
		isFirst = FALSE;
		}
	    else
		{
		itemSize = strlen(pp->name);
		if (itemSize + curSize + 2 >= maxSize)
		    errAt(pp->tok, "Dotted name too long");
		fullName[curSize] = '-';
		fullName[curSize+1] = '>';
		curSize += 2;
		strcpy(fullName + curSize, pp->name);
		curSize += itemSize;
		}
	    }
	fullName[curSize] = 0;
	break;
	}
    default:
        internalErr();
	break;
    }
}


static void codeForeach(struct pfCompile *pfc, FILE *f,
	struct pfParse *foreach)
/* Emit C code for foreach statement. */
{
struct pfParse *elPp = foreach->children;
struct pfParse *collectionPp = elPp->next;
struct pfBaseType *base = collectionPp->ty->base;
struct pfParse *body = collectionPp->next;
struct dyString *elName = varName(pfc, elPp->var);
char collectionName[512];

/* Print element variable in a new scope. */
expandDottedName(pfc, collectionName, sizeof(collectionName), collectionPp);
fprintf(f, "{\n");
codeScopeVars(pfc, f, foreach->scope, FALSE);

/* Also print some iteration stuff. */
if (base == pfc->arrayType)
    {
    struct pfBaseType *elBase = collectionPp->ty->children->base;
    fprintf(f, "int _pf_offset;\n");
    fprintf(f, "int _pf_elSize = %s->elSize;\n", collectionName);
    fprintf(f, "int _pf_endOffset = %s->size * _pf_elSize;\n", collectionName);
    fprintf(f, "for (_pf_offset=0; _pf_offset<_pf_endOffset; _pf_offset += _pf_elSize)\n");
    fprintf(f, "{\n");
    fprintf(f, "%s = *((", elName->string);
    printType(pfc, f, elBase);
    fprintf(f, "*)(%s->elements + _pf_offset));\n", collectionName);
    codeStatement(pfc, f, body);
    fprintf(f, "}\n");
    }
else if (base == pfc->stringType)
    {
    fprintf(f, "int _pf_offset;\n");
    fprintf(f, "int _pf_endOffset = %s->size;\n", collectionName);
    fprintf(f, "for (_pf_offset=0; _pf_offset<_pf_endOffset; _pf_offset += 1)\n");
    fprintf(f, "{\n");
    fprintf(f, "%s = %s->s[_pf_offset];\n", elName->string, collectionName);
    codeStatement(pfc, f, body);
    fprintf(f, "}\n");
    }
else
    {
    fprintf(f, "struct _pf_iterator _pf_ix = _pf_%s_iterator_init(%s);\n",
    	base->name, collectionName);
    fprintf(f, "while (_pf_ix.next(&_pf_ix, &%s))\n", elName->string);
    fprintf(f, "{\n");
    codeStatement(pfc, f, body);
    fprintf(f, "}\n");
    fprintf(f, "_pf_ix.cleanup(&_pf_ix);\n");
    }
cleanupScopeVars(pfc, f, foreach->scope);
fprintf(f, "}\n");
dyStringFree(&elName);
}

static void codeFor(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for for statement. */
{
struct pfParse *init = pp->children;
struct pfParse *end = init->next;
struct pfParse *next = end->next;
struct pfParse *body = next->next;

fprintf(f, "{\n");
codeScopeVars(pfc, f, pp->scope, FALSE);
codeStatement(pfc, f, init);
fprintf(f, "for(;;)\n");
fprintf(f, "{\n");
if (next->type != pptNop)
    {
    codeExpression(pfc, f, end, 0, TRUE);
    fprintf(f, "if (!");
    codeParamAccess(pfc, f, pfc->bitType, 0);
    fprintf(f, ") break;\n");
    }
codeStatement(pfc, f, body);
codeStatement(pfc, f, next);
fprintf(f, "}\n");
fprintf(f, "}\n");
}

static void codeForEachCall(struct pfCompile *pfc, FILE *f, struct pfParse *foreach)
/* Emit C code for foreach call statement. */
{
struct pfParse *elPp = foreach->children;
struct pfParse *callPp = elPp->next;
struct pfBaseType *base = callPp->ty->base;
struct pfParse *body = callPp->next;
struct pfParse *cast = body->next;
int expSize;

/* Print element variable in a new scope. */
fprintf(f, "{\n");
codeScopeVars(pfc, f, foreach->scope, TRUE);
fprintf(f, "for(;;)\n");
fprintf(f, "{\n");
expSize = codeInitOrAssign(pfc, f, elPp, callPp, 0);
lvalOffStack(pfc, f, elPp, 0, "=", expSize, TRUE);
if (cast != NULL)
    castStack(pfc, f, cast, 0);
fprintf(f, "if (%s[0].Bit == 0)", stackName);
fprintf(f, "   break;\n");
codeStatement(pfc, f, body);
fprintf(f, "}\n");
cleanupScopeVars(pfc, f, foreach->scope);
fprintf(f, "}\n");
}


static void codeWhile(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for while statement. */
{
struct pfParse *end = pp->children;
struct pfParse *body = end->next;
fprintf(f, "for(;;)\n");
fprintf(f, "{\n");
codeExpression(pfc, f, end, 0, TRUE);
fprintf(f, "if (!");
codeParamAccess(pfc, f, pfc->bitType, 0);
fprintf(f, ") break;\n");
codeStatement(pfc, f, body);
fprintf(f, "}\n");
}

static void codeIf(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Emit C code for for statement. */
{
struct pfParse *cond = pp->children;
struct pfParse *trueBody = cond->next;
struct pfParse *falseBody = trueBody->next;
codeExpression(pfc, f, cond, 0, TRUE);
fprintf(f, "if (");
codeParamAccess(pfc, f, pfc->bitType, 0);
fprintf(f, ")\n");
fprintf(f, "{\n");
codeStatement(pfc, f, trueBody);
fprintf(f, "}\n");
if (falseBody != NULL)
    {
    fprintf(f, "else\n");
    fprintf(f, "{\n");
    codeStatement(pfc, f, falseBody);
    fprintf(f, "}\n");
    }
}

static void codeStatement(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp)
/* Emit C code for one statement. */
{
switch (pp->type)
    {
    case pptCompound:
        {
	fprintf(f, "{\n");
	codeScope(pfc, f, pp, FALSE, FALSE);
	fprintf(f, "}\n");
	break;
	}
    case pptCall:
    case pptAssignment:
    case pptPlusEquals:
    case pptMinusEquals:
    case pptMulEquals:
    case pptDivEquals:
        codeExpression(pfc, f, pp, 0, TRUE);
	break;
    case pptTuple:
        {
	struct pfParse *p;
	for (p = pp->children; p != NULL; p = p->next)
	    codeStatement(pfc, f, p);
	break;
	}
    case pptVarInit:
	codeVarInit(pfc, f, pp, 0);
        break;
    case pptForeach:
	codeForeach(pfc, f, pp);
	break;
    case pptForEachCall:
        codeForEachCall(pfc, f, pp);
	break;
    case pptFor:
	codeFor(pfc, f, pp);
	break;
    case pptWhile:
        codeWhile(pfc, f, pp);
	break;
    case pptIf:
        codeIf(pfc, f, pp);
	break;
    case pptNop:
        break;
    case pptBreak:
        fprintf(f, "break;\n");
        break;
    case pptContinue:
        fprintf(f, "continue;\n");
        break;
    case pptReturn:
        fprintf(f, "goto _pf_cleanup;\n");
        break;
    case pptInclude:
        fprintf(f, "_pf_entry_%s(%s);\n", pp->name, stackName);
	break;
    default:
        fprintf(f, "[%s statement];\n", pfParseTypeAsString(pp->type));
	break;
    }
}

static void printPrototype(FILE *f, struct pfParse *funcDec, struct pfParse *class)
/* Print prototype for function call. */
{
/* Put out function prototype and opening brace.  */
if (class)
    {
    fprintf(f, "void _pf_cm%d_%s_%s(", class->scope->id, 
    	class->name, funcDec->name);
    fprintf(f, "%s *%s)",  stackType, stackName);
    }
else
    fprintf(f, "void %s%s(%s *%s)", prefix, funcDec->name, 
    	stackType, stackName);
}

static void rPrintPrototypes(FILE *f, struct pfParse *pp, struct pfParse *class)
/* Recursively print out function prototypes in C. */
{
switch (pp->type)
    {
    case pptClass:
        class = pp;
	break;
    case pptToDec:
    case pptParaDec:
    case pptFlowDec:
	printPrototype(f, pp, class);
	fprintf(f, ";\n");
        break;
    }
for (pp=pp->children; pp != NULL; pp = pp->next)
    rPrintPrototypes(f, pp, class);
}

static void declareStaticVars(struct pfCompile *pfc, FILE *f, 
	struct pfParse *funcDec, struct pfParse *pp,
	struct pfParse *class)
/* Declare static variables inside of function */
{
if (pp->type == pptVarInit)
    {
    struct pfType *type = pp->ty;
    if (type->isStatic)
        {
	fprintf(f, "static ");
	printType(pfc, f, type->base);
	fprintf(f, " ");
	printVarName(pfc, f, pp->var);
	fprintf(f, ";\n");
	}
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    declareStaticVars(pfc, f, funcDec, pp, class);
}

static void codeFunction(struct pfCompile *pfc, FILE *f, 
	struct pfParse *funcDec, struct pfParse *class)
/* Emit C code for function.  If class is non-null
 * then decorate it with 'self' so as to be a method. */
{
struct pfVar *funcVar = funcDec->var;
struct pfType *funcType = funcVar->ty;
struct pfType *inTuple = funcType->children;
struct pfType *outTuple = inTuple->next;
struct pfParse *body = funcDec->children->next->next->next;

if (body == NULL)
    return;

declareStaticVars(pfc, f, funcDec, body, class);
printPrototype(f, funcDec, class);
if (class)
    pfScopeAddVar(funcDec->scope, "self", class->ty, NULL);
fprintf(f, "\n{\n");


/* Print out input parameters. */
    {
    struct pfType *in;
    int inIx = 0;
    if (class)
        {
	fprintf(f, "struct %s *%sself = ", class->name, prefix);
	codeParamAccess(pfc, f, class->ty->base, 0);
	fprintf(f, ";\n");
	inIx += 1;
	}
    for (in = inTuple->children; in != NULL; in = in->next)
        {
	printType(pfc, f, in->base);
	fprintf(f, " %s%s = ", prefix, in->fieldName);
	codeParamAccess(pfc, f, in->base, inIx);
	fprintf(f, ";\n");
	inIx += 1;
	}
    }

/* Print out output parameters. */
    {
    struct pfType *out;
    for (out = outTuple->children; out != NULL; out = out->next)
        {
	printType(pfc, f, out->base);
	fprintf(f, " %s%s", prefix, out->fieldName);
	codeInitOfType(pfc, f, out);
	fprintf(f, ";\n");
	}
    }

/* Print out body (which is a compound statement) */
codeStatement(pfc, f, body);

/* Print exit label for returns. */
fprintf(f, "_pf_cleanup: ;\n");

/* Decrement ref counts on input variables. */
    {
    struct pfType *in;
    struct dyString *name = dyStringNew(0);
    for (in = inTuple->children; in != NULL; in = in->next)
	{
	dyStringClear(name);
	dyStringPrintf(name, "%s%s", prefix, in->fieldName);
	codeCleanupVarNamed(pfc, f, in, name->string);
	}
    dyStringFree(&name);
    }

/* Save the output. */
    {
    int outIx = 0;
    struct pfType *out;
    for (out = outTuple->children; out != NULL; out = out->next)
        {
	codeParamAccess(pfc, f, out->base, outIx);
	fprintf(f, " = %s%s;\n", prefix, out->fieldName);
	outIx += 1;
	}
    }

/* Close out function. */
fprintf(f, "}\n\n");
}

static void codeMethods(struct pfCompile *pfc, FILE *f, struct pfParse *class)
/* Print out methods in class. */
{
struct pfParse *classCompound = class->children->next;
struct pfParse *statement;
for (statement = classCompound->children; statement != NULL; 
     statement = statement->next)
    {
    switch (statement->type)
        {
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	    codeFunction(pfc, f, statement, class);
	    break;
	}
    }
}


static void printPolyFunTable(struct pfCompile *pfc, FILE *f, 
	struct pfBaseType *base)
/* Print polymorphic function table. */
{
if (base->polyList != NULL)
    {
    struct pfPolyFunRef *pfr;
    fprintf(f, "_pf_polyFunType _pf_pf%d_%s[] = {\n", base->scope->id, base->name);
    for (pfr = base->polyList; pfr != NULL; pfr = pfr->next)
        {
	struct pfBaseType *b = pfr->class;
	fprintf(f, "  _pf_cm%d_%s_%s,\n", b->scope->id, b->name, 
		pfr->method->fieldName);
	}
    fprintf(f, "};\n");
    }
}

static void codeStaticCleanups(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Print out any static assignments in parse tree. */
{
if (pp->type == pptVarInit && pp->ty->isStatic)
    codeCleanupVar(pfc, f, pp->var);
for (pp = pp->children; pp != NULL; pp = pp->next)
    codeStaticCleanups(pfc, f, pp);
}

static void codeStaticAssignments(struct pfCompile *pfc, FILE *f, struct pfParse *pp)
/* Print out any static assignments in parse tree. */
{
if (pp->type == pptVarInit && pp->ty->isStatic)
    codeStatement(pfc, f, pp);
for (pp = pp->children; pp != NULL; pp = pp->next)
    codeStaticAssignments(pfc, f, pp);
}

static void rPrintClasses(struct pfCompile *pfc, FILE *f, struct pfParse *pp,
	boolean printPolyFun)
/* Print out class definitions. */
{
if (pp->type == pptClass)
    {
    struct pfBaseType *base = pp->ty->base;
    fprintf(f, "struct %s {\n", base->name);
    fprintf(f, "int _pf_refCount;\n");
    fprintf(f, "void (*_pf_cleanup)(struct %s *obj, int typeId);\n", base->name);
    fprintf(f, "_pf_polyFunType *_pf_polyFun;\n");
    rPrintFields(pfc, f, base); 
    fprintf(f, "};\n");
    if (printPolyFun)
	printPolyFunTable(pfc, f, base);
    fprintf(f, "\n");
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    rPrintClasses(pfc, f, pp, printPolyFun);
}

static boolean isInside(struct pfParse *outside, struct pfParse *inside)
/* Return TRUE if inside is a child of outside. */
{
while (inside != NULL)
    {
    if (inside == outside)
	return TRUE;
    inside = inside->parent;
    }
return FALSE;
}


static void codeScope(
	struct pfCompile *pfc, FILE *f, struct pfParse *pp, 
	boolean printMain, boolean checkForExterns)
/* Print types and then variables from scope. */
{
struct pfScope *scope = pp->scope;
struct hashEl *hel, *helList;
struct pfParse *p;
boolean gotFunc = FALSE, gotVar = FALSE;


/* Get declaration list and sort it. */
helList = hashElListHash(scope->vars);
slSort(&helList, hashElCmp);

/* Print out variables. */
if (checkForExterns)
    {
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	struct pfVar *var = hel->val;
	var->isExternal = !isInside(pp, var->parse);
	}
    }
codeVarsInHelList(pfc, f, helList, !scope->isModule);

/* Print out function declarations */
for (p = pp->children; p != NULL; p = p->next)
    {
    switch (p->type)
        {
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	    codeFunction(pfc, f, p, NULL);
	    break;
	case pptClass:
	    codeMethods(pfc, f, p);
	    break;
	}
    }

/* Print out other statements */
if (printMain)
    {
    fprintf(f, "void _pf_entry_%s(%s *%s)\n{\n", pp->name, 
    	stackType, stackName);
    fprintf(f, "static int firstTime = 1;\n");
    fprintf(f, "if (firstTime)\n");
    fprintf(f, "{\n");
    fprintf(f, "firstTime = 0;\n");
    codeStaticAssignments(pfc, f, pp);
    }
for (p = pp->children; p != NULL; p = p->next)
    {
    switch (p->type)
        {
	case pptToDec:
	case pptParaDec:
	case pptFlowDec:
	case pptNop:
	case pptClass:
	    break;
	case pptTuple:
	case pptVarInit:
	    if (!p->isStatic)
		codeStatement(pfc, f, p);
	    break;
	default:
	    codeStatement(pfc, f, p);
	    break;
	}
    }
/* Print out any needed cleanups. */
codeCleanupVarsInHelList(pfc, f, helList);
if (printMain)
    codeStaticCleanups(pfc, f, pp);

if (printMain)
    {
    fprintf(f, "}\n");
    fprintf(f, "}\n");
    }

hashElFreeList(&helList);
}

static void printPolyFuncConnections(struct pfCompile *pfc,
	struct pfScope *scopeList, struct pfParse *module, FILE *f)
/* Print out poly_info table that connects polymorphic function
 * tables to the classes they belong to. */
{
struct pfScope *scope;
fprintf(f, "struct _pf_poly_info _pf_poly_info_%s[] = {\n", module->name);
for (scope = scopeList; scope != NULL; scope = scope->next)
    {
    struct pfBaseType *class = scope->class;
    if (class != NULL && class->polyList != NULL && isInside(module, class->def))
        {
	fprintf(f, "  {\"%s\", _pf_pf%d_%s},\n", class->name, class->scope->id, class->name);
	}
    }
fprintf(f, "  {0, 0},\n");  /* Make sure have at least one. */
fprintf(f, "};\n");
}

static void printConclusion(struct pfCompile *pfc, FILE *f, struct pfParse *mainModule)
/* Print out C code for end of program. */
{
fprintf(f, 
"\n"
"int main(int argc, char *argv[], char *environ[])\n"
"{\n"
"static _pf_Stack stack[16*1024];\n"
"_pf_init_types(_pf_base_info, _pf_base_info_count,\n"
"               _pf_type_info, _pf_type_info_count,\n"
"               _pf_field_info, _pf_field_info_count,\n"
"               _pf_module_info, _pf_module_info_count);\n"
"_pf_init_args(argc, argv, &%sprogramName, &%sargs, environ);\n"
"_pf_entry_%s(stack);\n"
"return 0;\n"
"}\n", prefix, prefix, mainModule->name);
}

static void printLocalTypeInfo(struct pfCompile *pfc, char *moduleName, FILE *f)
/* Print out local type info table. */
{
codeLocalTypeTableName(f, moduleName);
fprintf(f, "[] = {\n");
printModuleTypeTable(pfc, f);
fprintf(f, "};\n");
fprintf(f, "\n");
}

static void printModuleTable(struct pfCompile *pfc, FILE *f, struct pfParse *program)
/* Print out table with basic info on each module */
{
struct pfParse *module;
int moduleCount = 0;
for (module = program->children; module != NULL; module = module->next)
    {
    if (module->name[0] != '<')
        {
	fprintf(f, "extern struct %s %s_%s[];\n", 
	    localTypeTableType, localTypeTableName, module->name);
        fprintf(f, "extern struct _pf_poly_info _pf_poly_info_%s[];\n",
		module->name);
	fprintf(f, "void _pf_entry_%s(%s *%s);\n", module->name, 
	    stackType, stackName);
	}
    }
fprintf(f, "\n");
fprintf(f, "struct _pf_module_info _pf_module_info[] = {\n");
for (module = program->children; module != NULL; module = module->next)
    {
    if (module->name[0] != '<')
        {
	fprintf(f, "  {\"%s\", %s_%s, _pf_poly_info_%s, _pf_entry_%s,},\n",
	    module->name, localTypeTableName, module->name, module->name,
	    module->name);
	++moduleCount;
	}
    }
fprintf(f, "};\n");
fprintf(f, "int _pf_module_info_count = %d;\n\n", moduleCount);
}


void pfCodeC(struct pfCompile *pfc, struct pfParse *program, char *baseDir, char *mainName)
/* Generate C code for program. */
{
FILE *f;
struct pfParse *toCode, *module;
struct pfScope *scope;
struct pfParse *mainModule = NULL;

pfc->runTypeHash = hashNew(0);

/* Generate code for each module that is not already compiled. */
for (toCode = program->children; toCode != NULL; toCode = toCode->next)
    {
    if (toCode->type == pptModule || toCode->type == pptMainModule)
	{
	char fileName[PATH_LEN];
	if (toCode->type == pptMainModule)
	    mainModule = toCode;
	safef(fileName, sizeof(fileName), "%s%s.c", baseDir, toCode->name);
	f = mustOpen(fileName, "w");

	pfc->moduleTypeHash = hashNew(0);
	printPreamble(pfc, f, fileName, TRUE);
	fprintf(f, "extern struct %s %s_%s[];\n", localTypeTableType, localTypeTableName,
		toCode->name);
	fprintf(f, "static struct %s *%s = %s_%s;\n\n",
		localTypeTableType, localTypeTableName, localTypeTableName, toCode->name);

	/* Print function prototypes and class definitions for all modules */
	for (module = program->children; module != NULL; module = module->next)
	    {
	    fprintf(f, "/* Prototypes in ParaFlow module %s */\n\n", module->name);
	    if (module->name[0] != '<')
		fprintf(f, "void _pf_entry_%s(%s *stack);\n", module->name, stackType);
	    rPrintPrototypes(f, module, NULL);
	    fprintf(f, "\n");
	    fprintf(f, "/* Class definitions in ParaFlow module %s */\n\n", module->name);
	    rPrintClasses(pfc, f, module, toCode == module);
	    fprintf(f, "\n");
	    }

	for (module = program->children; module != NULL; module = module->next)
	    {
	    if (module == toCode)
		{
		verbose(3, "Coding %s\n", module->name);
		fprintf(f, "/* ParaFlow module %s */\n\n", module->name);
		fprintf(f, "\n");
		codeScope(pfc, f, module, TRUE, TRUE);
		fprintf(f, "\n");
		printPolyFuncConnections(pfc, pfc->scopeList, module, f);
		}
	    }
	printLocalTypeInfo(pfc, toCode->name, f);
	freeHashAndVals(&pfc->moduleTypeHash);
	carefulClose(&f);
	}
    }
f = mustOpen(mainName, "w");
printPreamble(pfc, f, mainName, FALSE);
printSysVarsAndPrototypes(f);
printModuleTable(pfc, f, program);
codedTypesCalcAndPrintAsC(pfc, program, f);
printConclusion(pfc, f, mainModule);
carefulClose(&f);
}

