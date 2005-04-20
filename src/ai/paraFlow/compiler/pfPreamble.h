/* pfPreamble - this gets included at the start of
 * paraFlow-generated C code. */

#include "../runtime/runType.h"

struct _pf_object;
struct _pf_string;
struct _pf_array;
struct _pf_list;
struct _pf_tree;
struct _pf_dir;
struct _pf_var;

typedef struct _pf_object *_pf_Object;
typedef struct _pf_string *_pf_String;
typedef struct _pf_array *_pf_Array;
typedef struct _pf_list *_pf_List;
typedef struct _pf_tree *_pf_Tree;
typedef struct _pf_dir *_pf_Dir;
typedef struct _pf_var _pf_Var;

typedef char _pf_Bit;
typedef unsigned char _pf_Byte;
typedef short _pf_Short;
typedef int _pf_Int;
typedef long long _pf_Long;
typedef float _pf_Float;
typedef double _pf_Double;

union _pf_varless
/* All the types a variable can take except the
 * typeless 'var' type. */
    {
    _pf_Bit Bit;
    _pf_Byte Byte;
    _pf_Short Short;
    _pf_Int Int;
    _pf_Long Long;
    _pf_Float Float;
    _pf_Double Double;
    _pf_Object Obj;
    _pf_String String;
    _pf_Array Array;
    _pf_List List;
    _pf_Tree Tree;
    _pf_Dir Dir;
    void *v;
    };

struct _pf_var
    {
    union _pf_varless val;	/* Typeless value. */
    int typeId;			/* Index in run time type table. */
    };

union _pf_stack
/* All the types a variable can take including the
 * typeless 'var' type. */
    {
    _pf_Bit Bit;
    _pf_Byte Byte;
    _pf_Short Short;
    _pf_Int Int;
    _pf_Long Long;
    _pf_Float Float;
    _pf_Double Double;
    _pf_Object Obj;
    _pf_String String;
    _pf_Array Array;
    _pf_List List;
    _pf_Tree Tree;
    _pf_Dir Dir;
    void *v;
    _pf_Var Var;
    };
typedef union _pf_stack _pf_Stack;

typedef void (*_pf_polyFunType)(_pf_Stack *stack);

struct _pf_iterator
/* Something to iterate over a collection */
    {
    	/* Get next item into *pItem.  Return FALSE if no more items.  */
    int (*next)(struct _pf_iterator *it, void *pItem);
    	/* Clean up iterator. */
    void (*cleanup)(struct _pf_iterator *it);
    void *data;	/* Iterator specific pointer data. */
    };

struct _pf_iterator _pf_list_iterator_init(_pf_List list);
struct _pf_iterator _pf_tree_iterator_init(_pf_Tree tree);
struct _pf_iterator _pf_dir_iterator_init(_pf_Dir dir);

#include "../runtime/object.h"
#include "../runtime/string.h"
#include "../runtime/dir.h"
#include "../runtime/initVar.h"


void print(_pf_Stack *stack);
/* Print to stdout, including a newline. */

void prin(_pf_Stack *stack);
/* Print to stdout, not including a newline. */
