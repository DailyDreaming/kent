/* Routines for reading and writing fasta format sequence files.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "errabort.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "linefile.h"

static char const rcsid[] = "$Id: fa.c,v 1.25 2003/11/15 21:14:09 kent Exp $";

boolean faReadNext(FILE *f, char *defaultName, boolean mustStartWithComment,
                         char **retCommentLine, struct dnaSeq **retSeq) 
/* Read next sequence from .fa file. Return sequence in retSeq.  
 * If retCommentLine is non-null
 * return the '>' line in retCommentLine.   
 * The whole thing returns FALSE at end of file.  
 * DNA chars are mapped to lower case.*/
{
    return faReadMixedNext(f, 0, defaultName, mustStartWithComment,
                                        retCommentLine, retSeq);
}

boolean faReadMixedNext(FILE *f, boolean preserveCase, char *defaultName, 
    boolean mustStartWithComment, char **retCommentLine, struct dnaSeq **retSeq)
/* Read next sequence from .fa file. Return sequence in retSeq.  
 * If retCommentLine is non-null return the '>' line in retCommentLine.
 * The whole thing returns FALSE at end of file. 
 * Contains parameter to preserve mixed case. */
{
char lineBuf[1024];
int lineSize;
char *words[1];
int c;
off_t offset = ftello(f);
size_t dnaSize = 0;
DNA *dna, *sequence, b;
int bogusChars = 0;
char *name = defaultName;

if (name == NULL)
    name = "";
dnaUtilOpen();
if (retCommentLine != NULL)
    *retCommentLine = NULL;
*retSeq = NULL;

/* Skip first lines until it starts with '>' */
for (;;)
    {
    if(fgets(lineBuf, sizeof(lineBuf), f) == NULL)
        {
        if (ferror(f))
            errnoAbort("read of fasta file failed");
        return FALSE;
        }
    lineSize = strlen(lineBuf);
    if (lineBuf[0] == '>')
        {
	if (retCommentLine != NULL)
            *retCommentLine = cloneString(lineBuf);
        offset = ftello(f);
        chopByWhite(lineBuf, words, ArraySize(words));
        name = words[0]+1;
        break;
        }
    else if (!mustStartWithComment)
        {
        if (fseeko(f, offset, SEEK_SET) < 0)
            errnoAbort("fseek on fasta file failed");
        break;
        }
    else
        offset += lineSize;
    }
/* Count up DNA. */
for (;;)
    {
    c = fgetc(f);
    if (c == EOF || c == '>')
        break;
    if (!isspace(c) && !isdigit(c))
        {
        ++dnaSize;
        }
    }

/* Allocate DNA and fill it up from file. */
dna = sequence = needHugeMem(dnaSize+1);
if (fseeko(f, offset, SEEK_SET) < 0)
    errnoAbort("fseek on fasta file failed");
for (;;)
    {
    c = fgetc(f);
    if (c == EOF || c == '>')
        break;
    if (!isspace(c) && !isdigit(c))
        {
        /* check for non-DNA char */
        if (ntChars[c] == 0)
            {
            *dna++ = preserveCase ? 'N' : 'n';
            }
        else
            {
            *dna++ = preserveCase ? c : ntChars[c];
            }
        }
    }
if (c == '>')
    ungetc(c, f);
*dna = 0;

*retSeq = newDnaSeq(sequence, dnaSize, name);
if (ferror(f))
    errnoAbort("read of fasta file failed");
return TRUE;
}


struct dnaSeq *faReadOneDnaSeq(FILE *f, char *defaultName, boolean mustStartWithComment)
/* Read sequence from FA file. Assumes positioned at or before
 * the '>' at start of sequence. */  
{
struct dnaSeq *seq;
if (!faReadNext(f, defaultName, mustStartWithComment, NULL, &seq))
    return NULL;
else
    return seq;
}

bioSeq *faNextSeqFromMemText(char **pText, boolean isDna)
/* Convert fa in memory to bioSeq.  Update *pText to point to next
 * record.  Returns NULL when no more sequences left. */
{
char *name = "";
char *s, *d;
struct dnaSeq *seq;
int size = 0;
char c;
char *filter = (isDna ? ntChars : aaChars);
char *text = *pText;

if (text == NULL)
    return NULL;
dnaUtilOpen();
if (text[0] == '>')
    {
    char *end;
    s = strchr(text, '\n');
    if (s != NULL) ++s;
    name = skipLeadingSpaces(text+1);
    end = skipToSpaces(name);
    if (end >= s || name >= s)
        errAbort("No name in line starting with '>'");
    if (end != NULL)
        *end = 0;
    }
else
    {
    s = skipLeadingSpaces(text);
    if (s == NULL || s[0] == 0)
        return NULL;
    }
name = cloneString(name);
    
d = text;
if (s != NULL)
    {
    for (;;)
	{
	c = *s;
	if (c == 0 || c == '>')
	    break;
	++s;
	if (isspace(c) || isdigit(c))
	    continue;
	if ((c = filter[c]) != 0) 
	    d[size++] = c;
	else
	    {
	    if (isDna)
		d[size++] = 'n';
	    else
		d[size++] = 'X';
	    }
	}
    }
d[size] = 0;

/* Put sequence into our little sequence structure. */
AllocVar(seq);
seq->name = name;
seq->dna = text;
seq->size = size;
*pText = s;
return seq;
}

bioSeq *faSeqListFromMemText(char *text, boolean isDna)
/* Convert fa's in memory into list of dnaSeqs. */
{
bioSeq *seqList = NULL, *seq;
while ((seq = faNextSeqFromMemText(&text, isDna)) != NULL)
    {
    slAddHead(&seqList, seq);
    }
slReverse(&seqList);
return seqList;
}

bioSeq *faSeqFromMemText(char *text, boolean isDna)
/* Convert fa in memory to bioSeq. */
{
return faNextSeqFromMemText(&text, isDna);
}

struct dnaSeq *faFromMemText(char *text)
/* Return a sequence from a .fa file that's been read into
 * a string in memory. This cannabalizes text, which should
 * be allocated with needMem.  This buffer becomes part of
 * the returned dnaSeq, which may be freed normally with
 * freeDnaSeq. */
{
return faNextSeqFromMemText(&text, TRUE);
}

struct dnaSeq *faReadSeq(char *fileName, boolean isDna)
/* Open fa file and read a single sequence from it. */
{
int maxSize = fileSize(fileName);
int size = 0;
int fd;
DNA *s;
struct dnaSeq *seq;

if (maxSize < 0)
    errAbort("can't open %s", fileName);
s = needHugeMem(maxSize+1);
fd = open(fileName, O_RDONLY);
read(fd, s, maxSize);
close(fd);
s[maxSize] = 0;
return faSeqFromMemText(s, isDna);
}

struct dnaSeq *faReadDna(char *fileName)
/* Open fa file and read a single nucleotide sequence from it. */
{
return faReadSeq(fileName, TRUE);
}

struct dnaSeq *faReadAa(char *fileName)
/* Open fa file and read a peptide single sequence from it. */
{
return faReadSeq(fileName, FALSE);
}

static int faFastBufSize = 0;
static DNA *faFastBuf;

static void expandFaFastBuf(int bufPos, int minExp)
/* Make faFastBuf bigger. */
{
if (faFastBufSize == 0)
    {
    faFastBufSize = 64 * 1024;
    while (minExp > faFastBufSize)
        faFastBufSize <<= 1;
    faFastBuf = needHugeMem(faFastBufSize);
    }
else
    {
    DNA *newBuf;
    int newBufSize = faFastBufSize + faFastBufSize;
    while (newBufSize < minExp)
        newBufSize <<= 1;
    newBuf = needHugeMem(newBufSize);
    memcpy(newBuf, faFastBuf, bufPos);
    freeMem(faFastBuf);
    faFastBuf = newBuf;
    faFastBufSize = newBufSize;
    }
}

void faFreeFastBuf()
/* Free up buffers used in fa fast and speedreading. */
{
freez(&faFastBuf);
faFastBufSize = 0;
}

boolean faFastReadNext(FILE *f, DNA **retDna, int *retSize, char **retName)
/* Read in next FA entry as fast as we can. Return FALSE at EOF. 
 * The returned DNA and name will be overwritten by the next call
 * to this function. */
{
int c;
int bufIx = 0;
static char name[256];
int nameIx = 0;
boolean gotSpace = FALSE;

/* Seek to next '\n' and save first word as name. */
dnaUtilOpen();
name[0] = 0;
for (;;)
    {
    if ((c = fgetc(f)) == EOF)
        {
        *retDna = NULL;
        *retSize = 0;
        *retName = NULL;
        return FALSE;
        }
    if (!gotSpace && nameIx < ArraySize(name)-1)
        {
        if (isspace(c))
            gotSpace = TRUE;
        else if (c != '>')
            {
            name[nameIx++] = c;
            }
        }
    if (c == '\n')
        break;
    }
name[nameIx] = 0;
/* Read until next '>' */
for (;;)
    {
    c = fgetc(f);
    if (isspace(c) || isdigit(c))
        continue;
    if (c == EOF || c == '>')
        c = 0;
    else
	{
	c = ntChars[c];
	if (c == 0) c = 'n';
	}
    if (bufIx >= faFastBufSize)
	expandFaFastBuf(bufIx, 0);
    faFastBuf[bufIx++] = c;
    if (c == 0)
        {
        *retDna = faFastBuf;
        *retSize = bufIx-1;
        *retName = name;
        return TRUE;
        }
    }
}


void faWriteNext(FILE *f, char *startLine, DNA *dna, int dnaSize)
/* Write next sequence to fa file. */
{
if (startLine != NULL)
    fprintf(f, ">%s\n", startLine);
writeSeqWithBreaks(f, dna, dnaSize, 50);
}

void faWrite(char *fileName, char *startLine, DNA *dna, int dnaSize)
/* Write out FA file or die trying. */
{
FILE *f = mustOpen(fileName, "w");
faWriteNext(f, startLine, dna, dnaSize);
if (fclose(f) != 0)
    errnoAbort("fclose failed");
}

void faWriteAll(char *fileName, bioSeq *seqList)
/* Write out all sequences in list to file. */
{
FILE *f = mustOpen(fileName, "w");
bioSeq *seq;

for (seq=seqList; seq != NULL; seq = seq->next)
    faWriteNext(f, seq->name, seq->dna, seq->size);
if (fclose(f) != 0)
    errnoAbort("fclose failed");
}

boolean faMixedSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName)
/* Read in DNA or Peptide FA record in mixed case.   Allow any upper or lower case
 * letter, or the dash character in. */
{
char c;
int bufIx = 0;
static char name[512];
int nameIx = 0;
boolean gotSpace = FALSE;
int lineSize, i;
char *line;

dnaUtilOpen();

/* Read first line, make sure it starts wiht '>', and read first word
 * as name of sequence. */
name[0] = 0;
if (!lineFileNext(lf, &line, &lineSize))
    {
    *retDna = NULL;
    *retSize = 0;
    return FALSE;
    }
if (line[0] == '>')
    {
    line = firstWordInLine(skipLeadingSpaces(line+1));
    if (line == NULL)
        errAbort("Expecting sequence name after '>' line %d of %s", lf->lineIx, lf->fileName);
    strncpy(name, line, sizeof(name));
    name[sizeof(name)-1] = '\0'; /* Just to make sure name is NULL terminated. */
    }
else
    {
    errAbort("Expecting '>' line %d of %s", lf->lineIx, lf->fileName);
    }
/* Read until next '>' */
for (;;)
    {
    if (!lineFileNext(lf, &line, &lineSize))
        break;
    if (line[0] == '>')
        {
	lineFileReuse(lf);
	break;
	}
    if (bufIx + lineSize >= faFastBufSize)
	expandFaFastBuf(bufIx, lineSize);
    for (i=0; i<lineSize; ++i)
        {
	c = line[i];
	if (isalpha(c) || c == '-')
	    faFastBuf[bufIx++] = c;
	}
    }
if (bufIx >= faFastBufSize)
    expandFaFastBuf(bufIx, 0);
faFastBuf[bufIx] = 0;
*retDna = faFastBuf;
*retSize = bufIx;
*retName = name;
return TRUE;
}

void faToProtein(char *poly, int size)
/* Convert possibly mixed-case protein to upper case.  Also
 * convert any strange characters to 'X'.  Does not change size.
 * of sequence. */
{
int i;
char c;
dnaUtilOpen();
for (i=0; i<size; ++i)
    {
    if ((c = aaChars[poly[i]]) == 0)
	c = 'X';
    poly[i] = c;
    }
}

void faToDna(char *poly, int size)
/* Convert possibly mixed-case DNA to lower case.  Also turn
 * any strange characters to 'n'.  Does not change size.
 * of sequence. */
{
int i;
char c;
dnaUtilOpen();
for (i=0; i<size; ++i)
    {
    if ((c = ntChars[poly[i]]) == 0)
	c = 'n';
    poly[i] = c;
    }
}

boolean faSomeSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName, boolean isDna)
/* Read in DNA or Peptide FA record. */
{
char *poly;
int size;

if (!faMixedSpeedReadNext(lf, retDna, retSize, retName))
    return FALSE;
size = *retSize;
poly = *retDna;
if (isDna)
    faToDna(poly, size);
else
    faToProtein(poly, size);
return TRUE;
}

boolean faPepSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName)
/* Read in next peptide FA entry as fast as we can.  */
{
return faSomeSpeedReadNext(lf, retDna, retSize, retName, FALSE);
}

boolean faSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName)
/* Read in next FA entry as fast as we can. Faster than that old,
 * pokey faFastReadNext. Return FALSE at EOF. 
 * The returned DNA and name will be overwritten by the next call
 * to this function. */
{
return faSomeSpeedReadNext(lf, retDna, retSize, retName, TRUE);
}

struct dnaSeq *faReadAllSeqMixable(char *fileName, boolean isDna, boolean mixed)
/* Return list of all sequences in FA file. 
 * Mixed case parameter overrides isDna.  If mixed is false then
 * will return DNA in lower case and non-DNA in upper case. */
{
struct lineFile *lf = lineFileOpen(fileName, FALSE);
struct dnaSeq *seqList = NULL, *seq;
DNA *dna;
char *name;
int size;
boolean ok;

for (;;)
    {
    if (mixed)
        ok = faMixedSpeedReadNext(lf, &dna, &size, &name);
    else
        ok = faSomeSpeedReadNext(lf, &dna, &size, &name, isDna);
    if (!ok)
        break;
    AllocVar(seq);
    seq->name = cloneString(name);
    seq->size = size;
    seq->dna = cloneMem(dna, size+1);
    slAddHead(&seqList, seq);
    }
lineFileClose(&lf);
slReverse(&seqList);
faFreeFastBuf();
return seqList;
}

struct dnaSeq *faReadAllSeq(char *fileName, boolean isDna)
/* Return list of all sequences in FA file. */
{
return faReadAllSeqMixable(fileName, isDna, FALSE);
}

struct dnaSeq *faReadAllDna(char *fileName)
/* Return list of all DNA sequences in FA file. */
{
return faReadAllSeq(fileName, TRUE);
}

struct dnaSeq *faReadAllPep(char *fileName)
/* Return list of all peptide sequences in FA file. */
{
return faReadAllSeq(fileName, FALSE);
}

struct dnaSeq *faReadAllMixed(char *fileName)
/* Read in mixed case fasta file, preserving case. */
{
return faReadAllSeqMixable(fileName, FALSE, TRUE);
}
