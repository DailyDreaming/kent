/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* nib.h - interface to nucleotides stored 4 bits per base (so have
 * room for N. */
#ifndef NIB_H
#define NIB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif 

/** Options for controlling masking  */
#define NIB_MASK_MIXED    0x01 /* Read uses case to indicate masking.
                                * Write sets mask bit for lower-case */
#define NIB_MASK_MAP      0x02 /* Read builds dnaSeq->mask bit map. Write
                                * uses mask to set mask bases.  Note: the
                                * bit map indicates which bases are not repeats
                                */
#define NIB_BASE_NAME     0x04 /* Return a sequence name that is the base name
                                * the file. */

void nibOpenVerify(char *fileName, FILE **retFile, int *retSize);
/* Open file and verify it's in good nibble format. */

struct dnaSeq *nibLoadPart(char *fileName, int start, int size);
/* Load part of an .nib file. */

struct dnaSeq *nibLoadPartMasked(int options, char *fileName, int start, int size);
/* Load part of an .nib file, with control over handling of masked positions */

struct dnaSeq *nibLdPart(char *fileName, FILE *f, int seqSize, int start, int size);
/* Load part of an open .nib file. */

struct dnaSeq *nibLdPartMasked(int options, char *fileName, FILE *f, int seqSize, int start, int size);
/* Load part of an open .nib file, with control over handling of masked
 * positions. */

struct dnaSeq *nibLoadAll(char *fileName);
/* Load all of a nib file. */

struct dnaSeq *nibLoadAllMasked(int options, char *fileName);
/* Load part of a .nib file, with control over handling of masked
 * positions. Subranges of nib files may specified in the file name
 * using the syntax:
 *    /path/file.nib:seqid:start-end
 * or\n"
 *    /path/file.nib:start-end
 * With the first form, seqid becomes the id of the subrange, with the second
 * form, a sequence id of file:start-end will be used.
 */

void nibWrite(struct dnaSeq *seq, char *fileName);
/* Write out file in format of four bits per nucleotide. */

void nibWriteMasked(int options, struct dnaSeq *seq, char *fileName);
/* Write out file in format of four bits per nucleotide, with control over
 * handling of masked positions. */

boolean isNib(char *fileName);
/* Return TRUE if file is a nib file. */

boolean isNibSubrange(char *fileName);
/* Return TRUE if file specifies a subrange of a nib file. */

struct nibStream *nibStreamOpen(char *fileName);
/* Create a new nib stream.  Open file and stuff. */

void nibStreamClose(struct nibStream **pNs);
/* Close a nib stream.  Flush last nibble if need be.  Fix up header. */

void nibStreamOne(struct nibStream *ns, DNA base);
/* Write out one base to nibStream. */

void nibStreamMany(struct nibStream *ns, DNA *dna, int size);
/* Write many bases to nibStream. */

#endif /* NIB_H */

