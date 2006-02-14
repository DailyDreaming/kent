/* pfParse build up parse tree from token stream */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#ifndef PFPARSE_H
#define PFPARSE_H

#ifndef PFCOMPILE_H
#include "pfCompile.h"
#endif

enum pfParseType
/* Parse type */
    {
    pptNone = 0,
    pptProgram,
    pptScope,
    pptInclude,
    pptModule,
    pptMainModule,
    pptModuleRef,
    pptNop,
    pptCompound,
    pptTuple,
    pptDot,
    pptKeyVal,
    pptOf,
    pptIf,
    pptElse,
    pptWhile,
    pptFor,
    pptForeach,
    pptForEachCall,
    pptBreak,
    pptContinue,
    pptClass,
    pptInterface,
    pptVarDec,
    pptNameUse,
    pptVarUse,
    pptSelfUse,
    pptFieldUse,
    pptConstUse,
    pptPolymorphic,
    pptToDec,
    pptFlowDec,
    pptReturn,
    pptCall,
    pptIndirectCall,
    pptAssignment,
    pptPlusEquals,
    pptMinusEquals,
    pptMulEquals,
    pptDivEquals,
    pptIndex,
    pptPlus,
    pptMinus,
    pptMul,
    pptDiv,
    pptShiftLeft,
    pptShiftRight,
    pptMod,
    pptComma,
    pptSame,
    pptNotSame,
    pptGreater,
    pptLess,
    pptGreaterOrEquals,
    pptLessOrEquals,
    pptNegate,
    pptNot,
    pptFlipBits,
    pptBitAnd,
    pptBitOr,
    pptBitXor,
    pptLogAnd,
    pptLogOr,
    pptRoot,
    pptParent,
    pptSys,		/* (Unused) Represents system include path. */
    pptUser,		/* (Unused) Represents user include path. */
    pptSysOrUser,	/* (Unused) Represent either include path. */
    pptVarInit,
    pptFormalParameter,
    pptPlaceholder,
    pptSymName,
    pptTypeName,
    pptTypeTuple,
    pptTypeFlowPt,
    pptTypeToPt,
    pptStringCat,
    pptParaDo,
    pptParaAdd,
    pptParaMultiply,
    pptParaAnd,
    pptParaOr,
    pptParaMin,
    pptParaMax,
    pptParaArgMin,
    pptParaArgMax,
    pptParaGet,
    pptParaFilter,
    pptUntypedElInCollection,	
    pptOperatorDec,
    pptArrayAppend,
    pptIndexRange,

    /* Note the casts must be in this order relative to pptCastBitToBit */
    pptCastBitToBit,	/* Never emitted. */
    pptCastBitToByte,	
    pptCastBitToShort,
    pptCastBitToInt,
    pptCastBitToLong,
    pptCastBitToFloat,
    pptCastBitToDouble,
    pptCastBitToString,

    pptCastByteToBit,
    pptCastByteToByte,	/* Never emitted. */
    pptCastByteToShort,
    pptCastByteToInt,
    pptCastByteToLong,
    pptCastByteToFloat,
    pptCastByteToDouble,
    pptCastByteToString,

    pptCastShortToBit,
    pptCastShortToByte,
    pptCastShortToShort,	/* NEver emitted. */
    pptCastShortToInt,
    pptCastShortToLong,
    pptCastShortToFloat,
    pptCastShortToDouble,
    pptCastShortToString,

    pptCastIntToBit,
    pptCastIntToByte,
    pptCastIntToShort,
    pptCastIntToInt,		/* Never emitted. */
    pptCastIntToLong,
    pptCastIntToFloat,
    pptCastIntToDouble,
    pptCastIntToString,

    pptCastLongToBit,
    pptCastLongToByte,
    pptCastLongToShort,
    pptCastLongToInt,
    pptCastLongToLong,		/* Never emitted. */
    pptCastLongToFloat,
    pptCastLongToDouble,
    pptCastLongToString,

    pptCastFloatToBit,
    pptCastFloatToByte,
    pptCastFloatToShort,
    pptCastFloatToInt,
    pptCastFloatToLong,
    pptCastFloatToFloat,	/* Never emitted. */
    pptCastFloatToDouble,
    pptCastFloatToString,

    pptCastDoubleToBit,
    pptCastDoubleToByte,
    pptCastDoubleToShort,
    pptCastDoubleToInt,
    pptCastDoubleToLong,
    pptCastDoubleToFloat,
    pptCastDoubleToDouble,	/* Never emitted. */
    pptCastDoubleToString,

    pptCastStringToBit,
    pptCastObjectToBit,
    pptCastClassToInterface,
    pptCastFunctionToPointer,
    pptCastTypedToVar,		
    pptCastVarToTyped,
    pptCastCallToTuple,
    pptUniformTuple,

    pptConstBit,
    pptConstByte,
    pptConstShort,
    pptConstInt,
    pptConstLong,
    pptConstFloat,
    pptConstDouble,
    pptConstString,
    pptConstZero,

#ifdef OLD
    pptStatic,
#endif /* OLD */

    pptTypeCount,
    };

char *pfParseTypeAsString(enum pfParseType type);
/* Return string corresponding to pfParseType */

struct pfParse
/* The para parse tree. */
    {
    struct pfParse *next;	/* Next in list */
    UBYTE type;			/* Node type */
    UBYTE isConst;		/* Is this all constant? */
    UBYTE isStatic;		/* Is this a static declaration?. */
    UBYTE reserved2;		/* For expansion 2. */
    char *name;			/* Node name - not allocated here */
    struct pfToken *tok;	/* Token associated with node. */
    struct pfScope *scope;	/* Associated scope. */
    struct pfType *ty;		/* Type of expression associated with parse. */
    struct pfVar *var;		/* Associated variable if any.  */
    struct pfParse *parent;	/* Parent statement if any. */
    struct pfParse *children;	/* subparts. */
    };

struct pfParse *pfParseNew(enum pfParseType type,
	struct pfToken *tok, struct pfParse *parent, struct pfScope *scope);
/* Return new parse node.  It's up to caller to fill in
 * children later. */

struct pfParse *pfParseEnclosingClass(struct pfParse *pp);
/* Find enclosing class if any. */

void pfParsePutChildrenInPlaceOfSelf(struct pfParse **pPp);
/* Replace self with children. */

void pfParseTypeSub(struct pfParse *pp, enum pfParseType oldType,
	enum pfParseType newType);
/* Convert type of pp and any children that are of oldType to newType */

struct pfParse *pfSingleTuple(struct pfParse *parent, struct pfToken *tok, 
	struct pfParse *single);
/* Wrap tuple around single and return it.  . */

void pfParseDump(struct pfParse *pp, int level, FILE *f);
/* Write out pp (and it's children) to file at given level of indent */

void pfParseDumpOne(struct pfParse *pp, int level, FILE *f);
/* Dump out single pfParse record at given level of indent. */

#ifdef OLD
struct pfParse *pfParseSource(struct pfCompile *pfc, struct pfSource *source,
	struct pfParse *parent, struct pfScope *scope, enum pfParseType modType);
/* Tokenize and parse given source. */
#endif /* OLD */

struct pfParse *pfParseModule(struct pfCompile *pfc, struct pfModule *module,
      struct pfParse *parent, struct pfScope *scope, enum pfParseType modType);
/* Parse a module and return parse tree associated with it. */

#endif /* PFPARSE_H */
