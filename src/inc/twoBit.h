/* twoBit - DNA sequence represented as two bits per pixel
 * with associated list of regions containing N's, and
 * masked regions. */

#ifndef TWOBIT_H
#define TWOBIT_H

struct twoBit
/* Two bit representation of DNA. */
    {
    struct twoBit *next;	/* Next sequence in list */
    char *name;			/* Name of sequence. */
    UBYTE *data;		/* DNA at two bits per base. */
    bits32 size;			/* Size of this sequence. */
    bits32 nBlockCount;		/* Count of blocks of Ns. */
    bits32 *nStarts;		/* Starts of blocks of Ns. */
    bits32 *nSizes;		/* Sizes of blocks of Ns. */
    bits32 maskBlockCount;		/* Count of masked blocks. */
    bits32 *maskStarts;		/* Starts of masked regions. */
    bits32 *maskSizes;		/* Sizes of masked regions. */
    bits32 reserved;		/* Reserved for future expansion. */
    };

struct twoBitIndex
/* An entry in twoBit index. */
    {
    struct twoBitIndex *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash */
    bits32 offset;		/* Offset in file. */
    };

struct twoBitFile
/* Holds header and index info from .2bit file. */
    {
    struct twoBitFile *next;
    char *fileName;	/* Name of this file, for error reporting. */
    FILE *f;		/* Open file. */
    boolean isSwapped;	/* Is byte-swapping needed. */
    bits32 version;	/* Version of .2bit file */
    bits32 seqCount;	/* Number of sequences. */
    bits32 reserved;	/* Reserved, always zero for now. */
    struct twoBitIndex *indexList;	/* List of sequence. */
    struct hash *hash;	/* Hash of sequences. */
    };

struct twoBitFile *twoBitOpen(char *fileName);
/* Open file, read in header and index.  
 * Squawk and die if there is a problem. */

void twoBitClose(struct twoBitFile **pTbf);
/* Free up resources associated with twoBitFile. */

struct dnaSeq *twoBitReadSeqFrag(struct twoBitFile *tbf, char *name,
	int fragStart, int fragEnd);
/* Read part of sequence from .2bit file.  To read full
 * sequence call with start=end=0.  Note that sequence will
 * be mixed case, with repeats in lower case and rest in
 * upper case. */

struct dnaSeq *twoBitLoadAll(char *spec);
/* Return list of all sequences matching spec.  If
 * spec is a simple file name then this will be
 * all sequence in file. Otherwise it will be
 * the sequence in the file specified by spec,
 * which is in format
 *    file/path/name:seqName:start-end
 * or
 *    file/path/name:seqName */

struct twoBit *twoBitFromDnaSeq(struct dnaSeq *seq, boolean doMask);
/* Convert dnaSeq representation in memory to twoBit representation.
 * If doMask is true interpret lower-case letters as masked. */

void twoBitWriteOne(struct twoBit *twoBit, FILE *f);
/* Write out one twoBit sequence to binary file. 
 * Note this does not include the name, which is
 * stored only in index. */

void twoBitWriteHeader(struct twoBit *twoBitList, FILE *f);
/* Write out header portion of twoBit file, including initial
 * index */

boolean twoBitIsFile(char *fileName);
/* Return TRUE if file is in .2bit format. */

boolean twoBitParseRange(char *rangeSpec, char **retFile, 
	char **retSeq, int *retStart, int *retEnd);
/* Parse out something in format
 *    file/path/name:seqName:start-end
 * or
 *    file/path/name:seqName
 * This will destroy the input 'rangeSpec' in the process.
 * Returns FALSE if it doesn't fit this format. 
 * If it is the shorter form then start and end will both
 * be returned as zero, which is ok by twoBitReadSeqFrag. */

boolean twoBitIsRange(char *rangeSpec);
/* Return TRUE if it looks like a two bit range specifier. */

boolean twoBitIsFileOrRange(char *spec);
/* Return TRUE if it is a two bit file or subrange. */

#endif /* TWOBIT_H */
