/* bam -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifdef USE_BAM

#include "common.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgConfig.h"
#include "udc.h"
#include "bamFile.h"

static char const rcsid[] = "$Id: bamFile.c,v 1.22 2010/03/04 05:14:13 angie Exp $";

char *bamFileNameFromTable(struct sqlConnection *conn, char *table, char *bamSeqName)
/* Return file name from table.  If table has a seqName column, then grab the 
 * row associated with bamSeqName (which is not nec. in chromInfo, e.g. 
 * bam file might have '1' not 'chr1'). */
{
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
if (fileName == NULL && checkSeqName)
    {
    if (startsWith("chr", bamSeqName))
	safef(query, sizeof(query), "select fileName from %s where seqName = '%s'",
	      table, bamSeqName+strlen("chr"));
    else
	safef(query, sizeof(query), "select fileName from %s where seqName = 'chr%s'",
	      table, bamSeqName);
    fileName = sqlQuickString(conn, query);
    }
if (fileName == NULL)
    {
    if (checkSeqName)
	errAbort("Missing fileName for seqName '%s' in %s table", bamSeqName, table);
    else
	errAbort("Missing fileName in %s table", table);
    }
return fileName;
}

static boolean isRegularFile(char *filename)
/* File not only exists, but is also a file not a directory. */
{
struct stat mystat;
if (stat(filename, &mystat) != 0)
    return FALSE;
return S_ISREG(mystat.st_mode);
}

static char *samtoolsFileName(char *fileOrUrl)
/* If udcFuse is configured, and we have a URL, convert it into a filename in
 * the udcFuse filesystem for use by samtools.  Thus samtools will think it's
 * working on a local file, but udcFuse will pass access requests to udc and
 * we'll get the benefits of sparse-file local caching and https support.
 * udcFuse needs us to open udc files before invoking udcFuse paths, so open
 * both the .bam and .bai (index) URLs with udc here.  
 * If udcFuse is not configured, or fileOrUrl is not an URL, just pass through fileOrUrl. */
{
char *udcFuseRoot = cfgOption("udcFuse.mountPoint");
char *protocol = NULL, *afterProtocol = NULL, *colon = NULL, *auth = NULL;
udcParseUrlFull(fileOrUrl, &protocol, &afterProtocol, &colon, &auth);
if (udcFuseRoot != NULL && afterProtocol != NULL)
    {
    struct dyString *dy = dyStringNew(0);
    if (auth == NULL)
	auth = "";
    dyStringPrintf(dy, "%s/%s/%s%s", udcFuseRoot, protocol, auth, afterProtocol);
    char *bamFileName = dyStringCannibalize(&dy);
    if (!isRegularFile(bamFileName))
	{
	verbose(2, "going to call udcFileMayOpen(%s).\n", fileOrUrl);
	struct udcFile *udcf = udcFileMayOpen(fileOrUrl, NULL);
	if (udcf != NULL)
	    {
	    udcFileClose(&udcf);
	    verbose(2, "closed udcf. testing existence of %s.\n", bamFileName);
	    if (!isRegularFile(bamFileName))
		{
		warn("Cannot find %s -- remount udcFuse?", bamFileName);
		freeMem(bamFileName);
		return cloneString(fileOrUrl);
		}
	    }
	else
	    {
	    warn("Failed to open BAM URL \"%s\" with udc", fileOrUrl);
	    freeMem(bamFileName);
	    return cloneString(fileOrUrl);
	    }
	}
    // Look for index file: xxx.bam.bai or xxx.bai.  Look for both in udcFuse,
    // and only open the URL with udc if neither udcFuse file exists.
    int urlLen = strlen(fileOrUrl), fLen = strlen(bamFileName);
    char *indexFileName = needMem(fLen+5);
    safef(indexFileName, fLen+5, "%s.bai", bamFileName);
    if (!isRegularFile(indexFileName))
	{
	verbose(2, "%s does not already exist\n", indexFileName);
	char *altIndexFileName = NULL;
	if (endsWith(fileOrUrl, ".bam"))
	    {
	    altIndexFileName = cloneString(indexFileName);
	    strcpy(altIndexFileName+fLen-1, "i");
	    }
	if (!(altIndexFileName && isRegularFile(altIndexFileName)))
	    {
	    char *indexUrl = needMem(urlLen+5);
	    safef(indexUrl, urlLen+5, "%s.bai", fileOrUrl);
	    verbose(2, "going to call udcFileMayOpen(%s).\n", indexUrl);
	    struct udcFile *udcf = udcFileMayOpen(indexUrl, NULL);
	    if (udcf != NULL)
		udcFileClose(&udcf);
	    else if (altIndexFileName != NULL)
		{
		char *altIndexUrl = cloneString(indexUrl);
		strcpy(altIndexUrl+urlLen-1, "i");
		verbose(2, "going to call udcFileMayOpen(%s).\n", altIndexUrl);
		udcf = udcFileMayOpen(altIndexUrl, NULL);
		if (udcf == NULL)
		    {
		    warn("Cannot find BAM index file (%s or %s)", indexUrl, altIndexUrl);
		    return cloneString(fileOrUrl);
		    }
		udcFileClose(&udcf);
		freeMem(altIndexUrl);
		}
	    else
		{
		warn("Cannot find BAM index file for \"%s\"", fileOrUrl);
		return cloneString(fileOrUrl);
		}
	    freeMem(indexUrl);
	    }
	freeMem(altIndexFileName);
	}
    freeMem(indexFileName);
    return bamFileName;
    }
return cloneString(fileOrUrl);
}

#ifndef KNETFILE_HOOKS
static char *getSamDir()
/* Return the name of a trash dir for samtools to run in (it creates files in current dir)
 * and make sure the directory exists. */
{
static char *samDir = NULL;
char *dirName = "samtools";
if (samDir == NULL)
    {
    mkdirTrashDirectory(dirName);
    size_t len = strlen(trashDir()) + 1 + strlen(dirName) + 1;
    samDir = needMem(len);
    safef(samDir, len, "%s/%s", trashDir(), dirName);
    }
return samDir;
}
#endif//ndef KNETFILE_HOOKS

boolean bamFileExists(char *fileOrUrl)
/* Return TRUE if we can successfully open the bam file and its index file. */
{
char *bamFileName = samtoolsFileName(fileOrUrl);
samfile_t *fh = samopen(bamFileName, "rb", NULL);
if (fh != NULL)
    {
#ifndef KNETFILE_HOOKS
    // When file is an URL, this caches the index file in addition to validating:
    // Since samtools's url-handling code saves the .bai file to the current directory,
    // chdir to a trash directory before calling bam_index_load, then chdir back.
    char *runDir = getCurrentDir();
    char *samDir = getSamDir();
    setCurrentDir(samDir);
#endif//ndef KNETFILE_HOOKS
    bam_index_t *idx = bam_index_load(bamFileName);
#ifndef KNETFILE_HOOKS
    setCurrentDir(runDir);
#endif//ndef KNETFILE_HOOKS
    samclose(fh);
    if (idx == NULL)
	{
	warn("bamFileExists: failed to read index corresponding to %s", bamFileName);
	return FALSE;
	}
    free(idx); // Not freeMem, freez etc -- sam just uses malloc/calloc.
    return TRUE;
    }
return FALSE;
}

void bamFetch(char *fileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData)
/* Open the .bam file, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData.
 * This handles BAM files with "chr"-less sequence names, e.g. from Ensembl. */
{
char *bamFileName = samtoolsFileName(fileOrUrl);
samfile_t *fh = samopen(bamFileName, "rb", NULL);
if (fh == NULL)
    {
    boolean usingUrl = (strstr(fileOrUrl, "tp://") || strstr(fileOrUrl, "https://"));
    struct dyString *urlWarning = dyStringNew(0);
    if (usingUrl)
	{
	char *udcFuseRoot = cfgOption("udcFuse.mountPoint");
	boolean usingUdc = (udcFuseRoot != NULL && startsWith(udcFuseRoot, bamFileName));
	if (usingUdc)
	    dyStringAppend(urlWarning, " (using udcFuse)");
	dyStringAppend(urlWarning,
		       ". If you are able to access the URL with your web browser, "
		       "please try reloading this page.");
	}
    warn("failed to open %s%s", fileOrUrl, urlWarning->string);
    return;
    }

int chromId, start, end;
int ret = bam_parse_region(fh->header, position, &chromId, &start, &end);
if (ret != 0 && startsWith("chr", position))
    ret = bam_parse_region(fh->header, position+strlen("chr"), &chromId, &start, &end);
if (ret != 0)
    // If the bam file does not cover the current chromosome, OK
    return;

#ifndef KNETFILE_HOOKS
// Since samtools' url-handling code saves the .bai file to the current directory,
// chdir to a trash directory before calling bam_index_load, then chdir back.
char *runDir = getCurrentDir();
char *samDir = getSamDir();
setCurrentDir(samDir);
#endif//ndef KNETFILE_HOOKS
bam_index_t *idx = bam_index_load(bamFileName);
#ifndef KNETFILE_HOOKS
setCurrentDir(runDir);
#endif//ndef KNETFILE_HOOKS
if (idx == NULL)
    warn("bam_index_load(%s) failed.", bamFileName);
else
    {
    ret = bam_fetch(fh->x.bam, idx, chromId, start, end, callbackData, callbackFunc);
    if (ret != 0)
	warn("bam_fetch(%s, %s (chromId=%d) failed (%d)", bamFileName, position, chromId, ret);
    free(idx); // Not freeMem, freez etc -- sam just uses malloc/calloc.
    }
samclose(fh);
}

boolean bamIsRc(const bam1_t *bam)
/* Return TRUE if alignment is on - strand. */
{
const bam1_core_t *core = &bam->core;
return (core->flag & BAM_FREVERSE);
}

void bamGetSoftClipping(const bam1_t *bam, int *retLow, int *retHigh, int *retClippedQLen)
/* If retLow is non-NULL, set it to the number of "soft-clipped" (skipped) bases at
 * the beginning of the query sequence and quality; likewise for retHigh at end.
 * For convenience, retClippedQLen is the original query length minus soft clipping
 * (and the length of the query sequence that will be returned). */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
char op;
int n = bamUnpackCigarElement(cigarPacked[0], &op);
int low = (op == 'S') ? n : 0;
n = bamUnpackCigarElement(cigarPacked[core->n_cigar-1], &op);
int high = (op == 'S') ? n : 0;
if (retLow != NULL)
    *retLow = low;
if (retHigh != NULL)
    *retHigh = high;
if (retClippedQLen != NULL)
    *retClippedQLen = (core->l_qseq - low - high);
}

char *bamGetQuerySequence(const bam1_t *bam, boolean useStrand)
/* Return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so if useStrand is given we rev-comp it back to restore the original query 
 * sequence. */
{
const bam1_core_t *core = &bam->core;
int qLen = core->l_qseq;
char *qSeq = needMem(qLen+1);
uint8_t *packedQSeq = bam1_seq(bam);
int i;
for (i = 0; i < qLen; i++)
    qSeq[i] = bam_nt16_rev_table[bam1_seqi(packedQSeq, i)];
qSeq[i] = '\0';
if (useStrand && bamIsRc(bam))
    reverseComplement(qSeq, qLen);
return qSeq;
}

UBYTE *bamGetQueryQuals(const bam1_t *bam, boolean useStrand)
/* Return the base quality scores encoded in bam as an array of ubytes. */
{
const bam1_core_t *core = &bam->core;
int qLen = core->l_qseq;
UBYTE *arr = needMem(qLen);
boolean isRc = useStrand && bamIsRc(bam);
UBYTE *qualStr = bam1_qual(bam);
int i;
for (i = 0;  i < core->l_qseq;  i++)
    {
    int offset = isRc ? (qLen - 1 - i) : i;
    arr[i] = (qualStr[0] == 255) ? 255 : qualStr[offset];
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
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tLength += n;
	    break;
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("bamGetTargetLength: unrecognized CIGAR op %c -- update me", op);
	}
    }
return tLength;
}

struct ffAli *bamToFfAli(const bam1_t *bam, struct dnaSeq *target, int targetOffset,
			 boolean useStrand, char **retQSeq)
/* Convert from bam to ffAli format.  If retQSeq is non-null, set it to the 
 * query sequence into which ffAli needle pointers point. (Adapted from psl.c's pslToFfAli.) */
{
struct ffAli *ffList = NULL, *ff;
const bam1_core_t *core = &bam->core;
boolean isRc = useStrand && bamIsRc(bam);
DNA *needle = (DNA *)bamGetQuerySequence(bam, useStrand);
if (retQSeq)
    *retQSeq = needle;
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

char *bamGetTagString(const bam1_t *bam, char *tag, char *buf, size_t bufSize)
/* If bam's tags include the given 2-character tag, place the value into 
 * buf (zero-terminated, trunc'd if nec) and return a pointer to buf,
 * or NULL if tag is not present. */
{
if (tag == NULL)
    errAbort("NULL tag passed to bamGetTagString");
if (! (isalpha(tag[0]) && isalnum(tag[1]) && tag[2] == '\0'))
    errAbort("bamGetTagString: invalid tag '%s'", htmlEncode(tag));
char *val = NULL;
// adapted from part of bam.c bam_format1:
uint8_t *s = bam1_aux(bam);
while (s < bam->data + bam->data_len)
    {
    uint8_t type, key[2];
    key[0] = s[0]; key[1] = s[1];
    s += 2; type = *s; ++s;
    if (key[0] == tag[0] && key[1] == tag[1])
	{
	if (type == 'A') { snprintf(buf, bufSize, "%c", *s);}
	else if (type == 'C') { snprintf(buf, bufSize, "%u", *s); }
	else if (type == 'c') { snprintf(buf, bufSize, "%d", *s); }
	else if (type == 'S') { snprintf(buf, bufSize, "%u", *(uint16_t*)s); }
	else if (type == 's') { snprintf(buf, bufSize, "%d", *(int16_t*)s); }
	else if (type == 'I') { snprintf(buf, bufSize, "%u", *(uint32_t*)s); }
	else if (type == 'i') { snprintf(buf, bufSize, "%d", *(int32_t*)s); }
	else if (type == 'f') { snprintf(buf, bufSize, "%g", *(float*)s); }
	else if (type == 'd') { snprintf(buf, bufSize, "%lg", *(double*)s); }
	else if (type == 'Z' || type == 'H') strncpy(buf, (char *)s, bufSize);
	else buf[0] = '\0';
	buf[bufSize-1] = '\0'; // TODO: is this nec?? see man pages
	val = buf;
	break;
	}
    else
	{
	if (type == 'A' || type == 'C' || type == 'c') { ++s; }
	else if (type == 'S' || type == 's') { s += 2; }
	else if (type == 'I' || type == 'i' || type == 'f') { s += 4; }
	else if (type == 'd') { s += 8; }
	else if (type == 'Z' || type == 'H')
	    {
	    while (*s++);
	    }
	}
    }
return val;
}

#endif//def USE_BAM
