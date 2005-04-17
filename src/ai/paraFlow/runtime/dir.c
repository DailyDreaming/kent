/* dir - Implement ParaFlow dir - a collection keyed by strings
 * which internally is a hash table. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "runType.h"
#include "../compiler/pfPreamble.h"
#include "object.h"
#include "string.h"

void suckItemOffStack(_pf_Stack **pStack, char **pEncoding, 
	struct _pf_type *fieldType, void *output);

int countOurLevel(char *encoding);

static void dirCleanup(struct _pf_dir *dir, int typeId)
/* Clean up resources associated with dir. */
{
struct hash *hash = dir->hash;
struct _pf_type *elType = dir->elType;
struct _pf_base *base = elType->base;
if (base->needsCleanup)
    {
    struct hashCookie it = hashFirst(hash);
    struct hashEl *hel;
    while ((hel = hashNext(&it)) != NULL)
        {
	struct _pf_object *obj = hel->val;
	if (--obj->_pf_refCount <= 0)
	    obj->_pf_cleanup(obj, elType->typeId);
	}
    }
else
    {
    freeHashAndVals(&hash);
    }
freeMem(dir);
}


struct _pf_dir *_pf_dir_new(int estimatedSize, struct _pf_type *type)
/* Create a dir.  The estimatedSize is just a guideline.
 * Generally you want this to be about the same as the
 * number of things going into the dir for optimal
 * performance.  If it's too small it will go slower.
 * If it's too big it will use up more memory.
 * Still, it's fine to be pretty approximate with it. */
{
int sizeLog2 = digitsBaseTwo(estimatedSize + (estimatedSize>>1));
struct _pf_dir *dir;
AllocVar(dir);
dir->_pf_refCount = 1;
dir->_pf_cleanup = dirCleanup;
dir->elType = type;
dir->hash = hashNew(sizeLog2);
return dir;
}

struct _pf_dir *_pf_int_dir_from_tuple(_pf_Stack *stack, int count, int typeId,
	int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = _pf_dir_new(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_Int val = stack[1].Int;
    stack += 2;
    hashAdd(hash, key->s, CloneVar(&val) );
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}

struct _pf_dir *_pf_string_dir_from_tuple(_pf_Stack *stack, int count, int typeId,
	int elTypeId)
/* Return a directory of ints from a tuple of key/val pairs.  Note the count
 * is twice the number of elements.  (It represents the number of items put
 * on the stack, and we have one each for key and value. */
{
struct _pf_dir *dir = _pf_dir_new(0, _pf_type_table[elTypeId]);
int i, elCount = (count>>1);
struct hash *hash = dir->hash;

for (i=0; i<elCount; ++i)
    {
    _pf_String key = stack[0].String;
    _pf_String val = stack[1].String;
    stack += 2;
    hashAdd(hash, key->s, val);
    if (--key->_pf_refCount <= 0)
         key->_pf_cleanup(key, 0);
    }
return dir;
}

 
struct _pf_object *_pf_dir_lookup_object(_pf_Stack *stack)
/* Stack contains directory, keyword.  Return value associated
 * with keyword on top of stack.  Neither one of the inputs is
 * pushed with a reference, so you don't need to deal with 
 * decrementing the ref counts on the input side.  The output
 * does get an extra refcount though. */
{
struct _pf_dir *dir = stack[0].Dir;
struct _pf_string *string = stack[1].String;
struct _pf_object *obj = hashFindVal(dir->hash, string->s);
if (obj != NULL)
    obj->_pf_refCount += 1;
return obj;
}

void _pf_dir_lookup_number(_pf_Stack *stack)
/* Stack contains directory, keyword.  Return number of
 * some sort back on the stack. */
{
struct _pf_dir *dir = stack[0].Dir;
struct _pf_string *string = stack[1].String;
void *v = hashFindVal(dir->hash, string->s);
struct _pf_base *base = dir->elType->base;
uglyf("dir_lookup_number type %s\n", base->name);
switch (base->singleType)
    {
    case pf_stBit:
	if (v == NULL) stack[0].Bit = 0;
	else stack[0].Bit = *((_pf_Bit *)v);
        break;
    case pf_stByte:
	if (v == NULL) stack[0].Byte = 0;
	else stack[0].Byte = *((_pf_Byte *)v);
        break;
    case pf_stShort:
	if (v == NULL) stack[0].Short = 0;
	else stack[0].Short = *((_pf_Short *)v);
        break;
    case pf_stInt:
	if (v == NULL) stack[0].Int = 0;
	else stack[0].Int = *((_pf_Int *)v);
        break;
    case pf_stLong:
	if (v == NULL) stack[0].Long = 0;
	else stack[0].Long = *((_pf_Long *)v);
        break;
    case pf_stFloat:
	if (v == NULL) stack[0].Float = 0;
	else stack[0].Float = *((_pf_Float *)v);
        break;
    case pf_stDouble:
	if (v == NULL) stack[0].Double = 0;
	else stack[0].Double = *((_pf_Double *)v);
        break;
    }
}

void _pf_dir_add_object(_pf_Stack *stack)
/* Stack contains object, directory, keyword.  Add object to
 * directory. */
{
struct _pf_object *obj = stack[0].Obj;
struct _pf_dir *dir = stack[1].Dir;
struct _pf_string *string = stack[2].String;
struct hash *hash = dir->hash;
struct hashEl *hel;
struct _pf_object *oldObj;

hel = hashLookup(hash, string->s);
if (hel != NULL)
    {
    oldObj = hel->val;
    if (--oldObj->_pf_refCount <= 0)
        oldObj->_pf_cleanup(oldObj, dir->elType->typeId);
    hel->val = obj;
    }
else
    {
    hashAdd(hash, string->s, obj);
    }
}

void *cloneStackNum(_pf_Stack *stack, struct _pf_type *type)
/* Return clone of number. */
{
switch (type->base->singleType)
    {
    case pf_stBit:
	return CloneVar(&stack[0].Bit);
    case pf_stByte:
	return CloneVar(&stack[0].Byte);
    case pf_stShort:
	return CloneVar(&stack[0].Short);
    case pf_stInt:
	return CloneVar(&stack[0].Int);
    case pf_stLong:
	return CloneVar(&stack[0].Long);
    case pf_stFloat:
	return CloneVar(&stack[0].Float);
    case pf_stDouble:
	return CloneVar(&stack[0].Double);
    default:
    	internalErr();
	return NULL;
    }
}

void _pf_dir_add_number(_pf_Stack *stack)
/* Stack contains number, directory, keyword.  Add number to
 * directory. */
{
struct _pf_dir *dir = stack[1].Dir;
struct _pf_string *string = stack[2].String;
struct hash *hash = dir->hash;
struct hashEl *hel;
struct _pf_object *oldObj;
void *clonedNum = cloneStackNum(&stack[0], dir->elType);

hel = hashLookup(hash, string->s);
if (hel != NULL)
    {
    freeMem(hel->val);
    hel->val = clonedNum;
    }
else
    hashAdd(hash, string->s, clonedNum);
}

_pf_Dir _pf_r_tuple_to_dir(_pf_Stack *stack, struct _pf_type *elType, 
	char *encoding, _pf_Stack **retStack, char **retEncoding)
/* Convert tuple to dir. */
{
struct _pf_base *base = elType->base;
int elSize = elType->base->size;
int i, count = countOurLevel(encoding);
struct _pf_dir *dir = _pf_dir_new(count, elType);

if (encoding[0] != '(')
    errAbort("Expecting ( in array tuple encoding, got %c", encoding[0]);
encoding += 1;
for (i=0; i<count; ++i)
    {
    _pf_String key = stack[0].String;
    stack += 1;
    if (base->needsCleanup)
        {
	struct _pf_object *obj;
	suckItemOffStack(&stack, &encoding, elType, &obj);
	hashAdd(dir->hash, key->s, obj);
	}
    else
        {
	switch (base->singleType)
	    {
	    case pf_stBit:
		{
	        _pf_Bit x;
		suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stByte:
		{
	        _pf_Byte x;
		suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stShort:
		{
	        _pf_Short x;
		suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stInt:
		{
	        _pf_Int x;
		suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stLong:
		{
	        _pf_Long x;
		suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stFloat:
		{
	        _pf_Float x;
		suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    case pf_stDouble:
		{
	        _pf_Double x;
		suckItemOffStack(&stack, &encoding, elType, &x);
		hashAdd(dir->hash, key->s, CloneVar(&x));
		break;
		}
	    default:
		internalErr();
	    }
	}
    }
if (encoding[0] != ')')
    errAbort("Expecting ) in array tuple encoding, got %c", encoding[0]);
encoding += 1;
*retStack = stack;
*retEncoding = encoding;
return dir;
}

_pf_Dir _pf_tuple_to_dir(_pf_Stack *stack, int typeId, char *encoding)
/* Convert tuple to dir. */
{
return _pf_r_tuple_to_dir(stack, _pf_type_table[typeId], encoding, &stack, &encoding);
}

