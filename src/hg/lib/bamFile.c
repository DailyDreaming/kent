/* bam -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifdef USE_BAM

#include "common.h"
#include "hdb.h"
#include "bamFile.h"

static char const rcsid[] = "$Id: bamFile.c,v 1.9 2009/09/24 04:33:50 angie Exp $";

static boolean ignoreStrand = FALSE;

void bamIgnoreStrand()
/* Change the behavior of this lib to disregard item strand. 
 * If called, this should be called before any other bam functions. */
{
ignoreStrand = TRUE;
}

char *bamFileNameFromTable(char *db, char *table, char *bamSeqName)
/* Return file name from table.  If table has a seqName column, then grab the 
 * row associated with bamSeqName (which is not nec. in chromInfo, e.g. 
 * bam file might have '1' not 'chr1'). */
{
struct sqlConnection *conn = hAllocConn(db);
boolean checkSeqName = (sqlFieldIndex(conn, table, "seqName") >= 0);
if (checkSeqName && bamSeqName == NULL)
    errAbort("bamFileNameFromTable: table %s has seqName column, but NULL seqName passed in",
	     table);
char query[512];
if (checkSeqName)
    safef(query, sizeof(query), "select fileName from %s where seqName = '%s'",
	  table, bamSeqName);
else
    safef(query, sizeof(query), "select fileName from %s", table);
char *fileName = sqlQuickString(conn, query);
if (fileName == NULL)
    {
    if (checkSeqName)
	errAbort("Missing fileName for seqName '%s' in %s table", bamSeqName, table);
    else
	errAbort("Missing fileName in %s table", table);
    }
hFreeConn(&conn);
return fileName;
}

void bamFetch(char *bamFileName, char *position, bam_fetch_f callbackFunc, void *callbackData)
/* Open the .bam file, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData. 
 * Note: if sequences in .bam file don't begin with "chr" but cart position does, pass in 
 * cart position + strlen("chr") to match the .bam file sequence names. */
{
samfile_t *fh = samopen(bamFileName, "rb", NULL);
if (fh == NULL)
    errAbort("samopen(%s, \"rb\") returned NULL", bamFileName);

int chromId, start, end;
int ret = bam_parse_region(fh->header, position, &chromId, &start, &end);
if (ret != 0)
    errAbort("bam_parse_region(%s) failed (%d)", position, ret);
//?? Could this happen if there is no data on some _random?  can avoid with tdb chromosomes...

bam_index_t *idx = bam_index_load(bamFileName);
if (idx == NULL)
    errAbort("bam_index_load(%s) failed.", bamFileName);
ret = bam_fetch(fh->x.bam, idx, chromId, start, end, callbackData, callbackFunc);
if (ret != 0)
    errAbort("bam_fetch(%s, %s (chromId=%d) failed (%d)", bamFileName, position, chromId, ret);
samclose(fh);
}

boolean bamIsRc(const bam1_t *bam)
/* Return TRUE if alignment is on - strand.  If bamIgnoreStrand has been called,
 * then this always returns FALSE. */
{
const bam1_core_t *core = &bam->core;
return (core->flag & BAM_FREVERSE) && !ignoreStrand;
}

char *bamGetQuerySequence(const bam1_t *bam)
/* Return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so here we rev-comp it back to restore the original query sequence. */
{
const bam1_core_t *core = &bam->core;
char *qSeq = needMem(core->l_qseq + 1);
uint8_t *s = bam1_seq(bam);
int i;
for (i = 0; i < core->l_qseq; i++)
    qSeq[i] = bam_nt16_rev_table[bam1_seqi(s, i)];
if (bamIsRc(bam))
    reverseComplement(qSeq, core->l_qseq);
return qSeq;
}

UBYTE *bamGetQueryQuals(const bam1_t *bam)
/* Return the base quality scores encoded in bam as an array of ubytes. */
{
const bam1_core_t *core = &bam->core;
int qLen = core->l_qseq;
UBYTE *arr = needMem(qLen);
boolean isRc = bamIsRc(bam);
UBYTE *qualStr = bam1_qual(bam);
int i;
for (i = 0;  i < qLen;  i++)
    {
    int offset = isRc ? (qLen - 1 - i) : i;
    arr[offset] = (qualStr[0] == 255) ? 255 : qualStr[i];
    }
return arr;
}

char *bamGetCigar(const bam1_t *bam)
/* Return a BAM-enhanced CIGAR string, decoded from the packed encoding in bam. */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
struct dyString *dyCigar = dyStringNew(min(8, core->n_cigar*4));
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    dyStringPrintf(dyCigar, "%d", n);
    dyStringAppendC(dyCigar, op);
    }
return dyStringCannibalize(&dyCigar);
}

void bamShowCigarEnglish(const bam1_t *bam)
/* Print out cigar in English e.g. "20 (mis)Match, 1 Deletion, 3 (mis)Match" */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    if (i > 0)
	printf(", ");
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    printf("%d (mis)Match", n);
	    break;
	case 'I': // inserted in query
	    printf("%d Insertion", n);
	    break;
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    printf("%d Skipped", n);
	    break;
	case 'D': // deleted from query
	    printf("%d Deletion", n);
	    break;
	case 'N': // long deletion from query (intron as opposed to small del)
	    printf("%d deletioN", n);
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	    printf("%d Hard clipped query", n);
	    break;
	case 'P': // P="silent deletion from padded reference sequence"
	    printf("%d Padded / silent deletion", n);
	    break;
	default:
	    errAbort("bamShowCigarEnglish: unrecognized CIGAR op %c -- update me", op);
	}
    }
}

static void descFlag(unsigned flag, unsigned bitMask, char *desc, boolean makeRed,
	      boolean *retFirst)
/* Describe a flag bit (or multi-bit mask) if it is set in flag. */
{
if ((flag & bitMask) == bitMask) // *all* bits in bitMask are set in flag
    {
    if (!*retFirst)
	printf(" | ");
    printf("<span%s>(<TT>0x%02x</TT>) %s</span>",
	   (makeRed ? " style='color: red'" : ""), bitMask, desc);
    *retFirst = FALSE;
    }
}

void bamShowFlagsEnglish(const bam1_t *bam)
/* Print out flags in English, e.g. "Mate is on '-' strand; Properly paired". */
{
const bam1_core_t *core = &bam->core;
unsigned flag = core->flag;
boolean first = TRUE;
descFlag(flag, BAM_FDUP, "Optical or PCR duplicate", TRUE, &first);
descFlag(flag, BAM_FQCFAIL, "QC failure", TRUE, &first);
descFlag(flag, BAM_FSECONDARY, "Not primary alignment", TRUE, &first);
descFlag(flag, BAM_FREAD2, "Read 2 of pair", FALSE, &first);
descFlag(flag, BAM_FREAD1, "Read 1 of pair", FALSE, &first);
descFlag(flag, BAM_FMREVERSE, "Mate is on '-' strand", FALSE, &first);
descFlag(flag, BAM_FREVERSE, "Read is on '-' strand", FALSE, &first);
descFlag(flag, BAM_FMUNMAP, "Mate is unmapped", TRUE, &first);
if (flag & BAM_FUNMAP)
    errAbort("Read is unmapped (what is it doing here?!?)");
descFlag(flag, (BAM_FPROPER_PAIR | BAM_FPAIRED), "Properly paired", FALSE, &first);
if ((flag & BAM_FPAIRED) && !(flag & BAM_FPROPER_PAIR))
    descFlag(flag, BAM_FPAIRED, "Not properly paired", TRUE, &first);
}

int bamGetTargetLength(const bam1_t *bam)
/* Tally up the alignment's length on the reference sequence from
 * bam's packed-int CIGAR representation. */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
int tLength=0;
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    tLength += n;
	    break;
	case 'I': // inserted in query
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tLength += n;
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("bamGetTargetLength: unrecognized CIGAR op %c -- update me", op);
	}
    }
return tLength;
}

struct ffAli *bamToFfAli(const bam1_t *bam, struct dnaSeq *target, int targetOffset)
/* Convert from bam to ffAli format.  (Adapted from psl.c's pslToFfAli.) */
{
struct ffAli *ffList = NULL, *ff;
const bam1_core_t *core = &bam->core;
boolean isRc = bamIsRc(bam);
DNA *needle = (DNA *)bamGetQuerySequence(bam);
if (isRc)
    reverseComplement(target->dna, target->size);
DNA *haystack = target->dna;
unsigned int *cigarPacked = bam1_cigar(bam);
int tStart = targetOffset, qStart = 0, i;
// If isRc, need to go through the CIGAR ops backwards, but sequence offsets still count up.
int iStart = isRc ? (core->n_cigar - 1) : 0;
int iIncr = isRc ? -1 : 1;
for (i = iStart;  isRc ? (i >= 0) : (i < core->n_cigar);  i += iIncr)
    {
    char op;
    int size = bamUnpackCigarElement(cigarPacked[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    AllocVar(ff);
	    ff->left = ffList;
	    ffList = ff;
	    ff->nStart = needle + qStart;
	    ff->nEnd = ff->nStart + size;
	    ff->hStart = haystack + tStart - targetOffset;
	    ff->hEnd = ff->hStart + size;
	    tStart += size;
	    qStart += size;
	    break;
	case 'I': // inserted in query
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    qStart += size;
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tStart += size;
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("bamToFfAli: unrecognized CIGAR op %c -- update me", op);
	}
    }
ffList = ffMakeRightLinks(ffList);
ffCountGoodEnds(ffList);
return ffList;
}

bam1_t *bamClone(const bam1_t *bam)
/* Return a newly allocated copy of bam. */
{
// Using typecasts to get around compiler complaints about bam being const:
bam1_t *newBam = cloneMem((void *)bam, sizeof(*bam));
newBam->data = cloneMem((void *)bam->data, bam->data_len*sizeof(bam->data[0]));
return newBam;
}

void bamShowTags(const bam1_t *bam)
/* Print out tags in HTML: bold key, no type indicator for brevity. */
{
// adapted from part of bam.c bam_format1:
uint8_t *s = bam1_aux(bam);
while (s < bam->data + bam->data_len)
    {
    uint8_t type, key[2];
    key[0] = s[0]; key[1] = s[1];
    s += 2; type = *s; ++s;
    printf(" <B>%c%c</B>:", key[0], key[1]);
    if (type == 'A') { printf("%c", *s); ++s; }
    else if (type == 'C') { printf("%u", *s); ++s; }
    else if (type == 'c') { printf("%d", *s); ++s; }
    else if (type == 'S') { printf("%u", *(uint16_t*)s); s += 2; }
    else if (type == 's') { printf("%d", *(int16_t*)s); s += 2; }
    else if (type == 'I') { printf("%u", *(uint32_t*)s); s += 4; }
    else if (type == 'i') { printf("%d", *(int32_t*)s); s += 4; }
    else if (type == 'f') { printf("%g", *(float*)s); s += 4; }
    else if (type == 'd') { printf("%lg", *(double*)s); s += 8; }
    else if (type == 'Z' || type == 'H')
	{
	while (*s) putc(*s++, stdout);
	++s;
	}
    }
putc('\n', stdout);
}

#endif//def USE_BAM
