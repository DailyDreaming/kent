/*
  File: pslAnal.c
  Author: Terry Furey
  Date: 5/2/2003
  Description: Calculates characteristics of a file of psl alignments
*/

#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "fuzzyFind.h"
#include "genePred.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "psl.h"
#include "snp.h"
#include "fa.h"
#include "psl.h"
#include "options.h"
#include "hgConfig.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"ver", OPTION_STRING},
    {"der", OPTION_STRING},
    {"xeno", OPTION_BOOLEAN},
    {"indels", OPTION_BOOLEAN},
    {"unaligned", OPTION_BOOLEAN},
    {"mismatches", OPTION_BOOLEAN},
    {"codonsub", OPTION_BOOLEAN},
    {"noVersions", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean indelReport = FALSE;
boolean unaliReport = FALSE;
boolean mismatchReport = FALSE;
boolean codonSubReport = FALSE;
boolean xeno = FALSE;
boolean noVersions = FALSE;

struct acc
{
  struct acc *next;
  char *name;
  char *version;
  char *organism;
};

struct clone
{
  struct clone *next;
  int id;
  int imageId;
};

struct refseq
{
  struct refseq *next;
  struct acc *accList;
};

/* indel object is now used for tracking several, these constants
 * defined the type */
#define INDEL    1
#define MISMATCH 2
#define CODONSUB 3
#define UNALIGNED 4

struct evid
{
  struct evid *next;
  int genMrna;
  struct acc *genMrnaAcc;
  int genEst;
  struct acc *genEstAcc;
  int mrnaMrna;
  struct acc *mrnaMrnaAcc;
  int mrnaEst;
  struct acc *mrnaEstAcc;
  int noMrna;
  struct acc *noMrnaAcc;
  int noEst;
  struct acc *noEstAcc;
  int unMrna;
  struct acc *unMrnaAcc;
  int unEst;
  struct acc *unEstAcc;
};

struct indel
{
  struct indel *next;
  int size;
  char *chrom;
  int chromStart;  /* note: [1..n] coordinates */
  int chromEnd;
  struct acc *mrna;
  int mrnaStart;
  int mrnaEnd;
  struct clone *clones;
  struct acc *accs;
  struct evid *hs;
  struct evid *xe;
  char* mrnaSeq;
  char* genSeq;
  boolean insertion;
  boolean cds;

  /* fields used if this is tracking codon substitutions*/
  int codonGenPos[3];  /* position of the codon bases */
  char genCodon[4];
  char mrnaCodon[4];
  boolean knownSnp;
  boolean validatedSnp;
};

struct pslInfo
{
  struct pslInfo *next;
  struct psl *psl;
  short splice[512];
  struct acc *mrna;
  float pctId;
  float coverage;
  int cdsStart;
  int cdsEnd;
  int cdsSize;
  float cdsPctId;
  float cdsCoverage;
  int introns;
  int stdSplice;
  int cdsMatch;
  int cdsMismatch;
  struct indel *mmList;
  int cdsGap;
  int unalignedCds;
  int singleIndel;
  int tripleIndel;
  int totalIndel;
  int indelCount;
  int indels[256];
  struct indel *indelList;
  struct indel *unaliList;
  int snp;
  int thirdPos;
  int synSub;
  int nonSynSub;
  int synSubSnp;
  int nonSynSubSnp;
  int loci;
  struct indel *codonSubList;
  struct clone *mrnaCloneId;
  struct refseq *refseq;
} *piList = NULL;

struct hash *cdsStarts = NULL;
struct hash *cdsEnds = NULL;
struct hash *loci = NULL;
int nextFakeLoci = 1;
struct hash *rnaSeqs = NULL;
struct hash *version = NULL;
struct hash *derived = NULL;

int indels[128];

void cloneFree(struct clone **clone)
/* Free a dynamically allocated acc */
{
struct clone *c;

if ((c = *clone) == NULL) return;
freez(clone);
}

void cloneFreeList(struct clone **cList)
/* Free a dynamically allocated list of acc's */
{
struct clone *c = NULL, *next = NULL;

for (c = *cList; c != NULL; c = next)
    {
    next = c->next;
    cloneFree(&c);
    }
*cList = NULL;
}

char* accFmt(struct acc* acc)
/* format acc with optional version */
{
static char accVer[64];

if (acc->version == NULL)
    return acc->name;
else
    {
    safef(accVer, sizeof(accVer), "%s.%s", acc->name, acc->version);
    return accVer;
    }
}

void accFree(struct acc **acc)
/* Free a dynamically allocated acc */
{
struct acc *a;

if ((a = *acc) == NULL) return;
freez(&(a->organism));
freez(&(a->name));
freez(&(a->version));
freez(acc);
}

void accFreeList(struct acc **aList)
/* Free a dynamically allocated list of acc's */
{
struct acc *a = NULL, *next = NULL;

for (a = *aList; a != NULL; a = next)
    {
    next = a->next;
    accFree(&a);
    }
*aList = NULL;
}

void evidFree(struct evid **ev)
/* Free a dynamically allocated evid */
{
struct evid *e;

if ((e = *ev) == NULL) return;
accFreeList(&(e->genMrnaAcc));
accFreeList(&(e->genEstAcc));
accFreeList(&(e->mrnaMrnaAcc));
accFreeList(&(e->mrnaEstAcc));
accFreeList(&(e->noMrnaAcc));
accFreeList(&(e->noEstAcc));
accFreeList(&(e->unMrnaAcc));
accFreeList(&(e->unEstAcc));
freez(ev);
}

void evidListFree(struct evid **iList)
/* Free a dynamically allocated list of evid's */
{
struct evid *i, *next;

for (i = *iList; i != NULL; i = next)
    {
    next = i->next;
    evidFree(&i);
    }
*iList = NULL;
}

void indelFree(struct indel **in)
/* Free a dynamically allocated indel */
{
struct indel *i;

if ((i = *in) == NULL) return;
/*accFree(&(i->mrna));*/
evidListFree(&(i->hs));
evidListFree(&(i->xe));
cloneFreeList(&(i->clones));
/* accFreeList(&(i->accs)); - freed by evidFreeList */
free(i->mrnaSeq);
free(i->genSeq);
freez(in);
}

void indelListFree(struct indel **iList)
/* Free a dynamically allocated list of indel's */
{
struct indel *i, *next;

for (i = *iList; i != NULL; i = next)
    {
    next = i->next;
    indelFree(&i);
    }
*iList = NULL;
}

void pslInfoFree(struct pslInfo **pi)
/* Free a single dynamically allocated pslInfo element */
{
struct pslInfo *el;

if ((el = *pi) == NULL) return;
pslFree(&(el->psl));
accFree(&(el->mrna));
indelListFree(&(el->indelList));
indelListFree(&(el->mmList));
indelListFree(&(el->codonSubList));
cloneFree(&(el->mrnaCloneId));
freez(pi);
}

void readCds(struct lineFile *cf)
/* Read in file of coding region starts and stops 
   Convert start to 0-based to make copmarison with psl easier */
{
int lineSize, wordCount;
char *words[4];
char *name;
int start;
int end;

cdsStarts = newHash(16);
cdsEnds = newHash(16);

while (lineFileChopNext(cf, words, 3))
    {
    name = cloneString(words[0]);
    start = sqlUnsigned(words[1]) - 1;
    end = sqlUnsigned(words[2]);
    hashAddInt(cdsStarts, name, start);
    hashAddInt(cdsEnds, name, end);
    }
}

void readRnaSeq(char *filename)
/* Read in file of mRNA fa sequences */
{
struct dnaSeq *seqList, *oneSeq;
int start, end, size;

rnaSeqs = newHash(16);
seqList = faReadAllDna(filename);
for (oneSeq = seqList; oneSeq != NULL; oneSeq = oneSeq->next)
    hashAdd(rnaSeqs, oneSeq->name, oneSeq);
}

void readLoci(struct lineFile *lf)
/* Read in file of loci id's, primarily from LocusLink */
{
int lineSize, wordCount;
char *words[4];
char *name;
int thisLoci;
int numLoci = 0;

loci = newHash(16);

while (lineFileChopNext(lf, words, 2))
    {
    name = cloneString(words[0]);
    thisLoci = sqlUnsigned(words[1]);
    hashAddInt(loci, name, thisLoci);
    numLoci++;
    }

/* if loci files was empty, no loci will be used */
if (numLoci == 0)
    hashFree(&loci);
}

void readVersion(struct lineFile *lf)
/* Read in file of version numbers for mrnas */
{
int lineSize, wordCount;
char *words[4];
char *name;
char *v;

version = newHash(16);

while (lineFileChopNext(lf, words, 2))
    {
    name = cloneString(words[0]);
    v = cloneString(words[1]);
    hashAdd(version, name, v);
    }
}

char *findVersion(char *name)
/* Determine the version for an mrna/est accession */
{
struct sqlConnection *conn = hAllocConn();
char *ret = NULL;
char query[256];
struct sqlResult *sr;
char **row;

safef(query, sizeof(query), "select version from mrna where acc = '%s'", name); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    ret = cloneString(row[0]);
sqlFreeResult(&sr);
hFreeConn(&conn);

return(ret);
}


struct acc *createAcc(char *name)
{
struct acc *ret;
char *a;
char *accs[4];
int wordCount, id = -1;
  
AllocVar(ret);
if (noVersions)
    {
    ret->name = name;
    }
else
    {
    wordCount = chopByChar(name, '.', accs, ArraySize(accs)); 
    if (wordCount > 2) 
        errAbort("Accession not standard, %s\n", name);
    ret->name = accs[0];
    if (wordCount = 1)
        {
        if ((!version) || (!hashLookup(version, name)))
            ret->version = findVersion(name);
        else 
            ret->version = cloneString(hashFindVal(version, name));
        }
    else
        ret->version = accs[1];
    }
/* fprintf(stderr, "accession %s created\n", accFmt(ret));*/

return(ret);
}

void readRsDerived(struct lineFile *lf)
/* Read in file of derived accessions for refseq mrnas */
{
char *words[4];
char *rs, *acc;
struct refseq *r;
struct acc *a;

derived = newHash(16);

while (lineFileChopNext(lf, words, 2))
    {
    rs = cloneString(words[0]);
    acc = cloneString(words[1]);
    a = createAcc(acc);
    if (!hashLookup(derived, rs))
      {
	AllocVar(r);
	r->next = NULL;
	r->accList = NULL;
	hashAdd(derived, rs, r);
      }
    r = hashFindVal(derived, rs);
    slAddHead(&r->accList, a);
    }
}

int countIntrons(unsigned count, unsigned *sizes, unsigned *starts)
/* Count the number of introns in an alignment where an intron is
   defined as a gap greater than 30 bases */
{
int ret = 0, i;

for (i = 1; i < count; i++) 
    if (starts[i] - (starts[i-1] + sizes[i-1]) > 30)
	ret++;
return(ret);
}

int countStdSplice(struct psl *psl, DNA *seq, struct pslInfo *pi)
/* For each intron, determine whether it has a canonical splice site
   Return the number of introns that do */
{
int count=0, i;

for (i=1; i<psl->blockCount; ++i)
    {
    int iStart, iEnd, blockSize = psl->blockSizes[i-1];
    pi->splice[i] = 0;
    /*    if ((psl->qStarts[i-1] + blockSize == psl->qStarts[i]) && 
	(psl->tStarts[i] - (psl->tStarts[i-1] + psl->blockSizes[i-1]) > 30))
	{*/
	iStart = psl->tStarts[i-1] + psl->blockSizes[i-1] - psl->tStart;
	iEnd = psl->tStarts[i] - psl->tStart;
	if (abs(iEnd - iStart) > 15)
	  {
	    /*if (psl->strand[0] == '+')
	      {*/
		if ((seq[iStart] == 'g' && seq[iStart+1] == 't' && seq[iEnd-2] == 'a' && seq[iEnd-1] == 'g') ||
		    (seq[iStart] == 'g' && seq[iStart+1] == 'c' && seq[iEnd-2] == 'a' && seq[iEnd-1] == 'g'))
		  {
		    count++;
		    pi->splice[i] = 1;
		    if  (abs(iEnd - iStart) <= 30)
		      pi->introns++;
		  }
		/* }
	    else 
	      {
		if ((seq[iStart] == 'c' && seq[iStart+1] == 't' && seq[iEnd-2] == 'a' && seq[iEnd-1] == 'c') ||
		    (seq[iStart] == 'c' && seq[iStart+1] == 't' && seq[iEnd-2] == 'g' && seq[iEnd-1] == 'c'))
		  {
		    count++;
		    pi->splice[i] = 1;
		  }
		  }*/
	  }
	/*}*/
    }
return(count);
}

int snpBase(struct sqlConnection *conn, char *chr, int position)
/* Determine whether a given position is known to be a SNP */ 
{
struct sqlResult *sr;
char **row;
int rowOff;
int ret = 0;
static boolean checked = FALSE;
static boolean haveSnp = FALSE;

if (!checked)
    {
    haveSnp = sqlTableExists(conn, "snp");
    checked = TRUE;
    if (!haveSnp)
        fprintf(stderr, "warning: no snp table in this databsae\n");
    }

/* the new table is snp, replacing snpTsc and snpNih+hgFixed.dsSnpRS */
if (haveSnp)
    {
    sr = hRangeQuery(conn, "snp", chr, position, position+1, NULL, &rowOff);
    while ((row = sqlNextRow(sr)) != NULL) 
        {
        struct snp snp;
        snpStaticLoad(row+rowOff, &snp);
	/* Check if this is a snp, not a indel */
        if (sameString(snp.class, "snp"))
	  /* Check if the snp has been validated*/
	  if (differentString(snp.valid, "no-information"))
	    ret = 2;
	  else
	    if (ret < 2)
	      ret = 1;	
        }
    sqlFreeResult(&sr);
    }
return(ret);
}

void findOrganism(struct sqlConnection *conn, struct acc *acc)
/* Determine organism for each non-human mrna/est in the list */
{
char query[256];
struct sqlResult *sr;
char **row;
char *a;
char *accs[4];
int wordCount, id = -1;


/*a = cloneString(acc->name);
wordCount = chopByChar(a, '.', accs, ArraySize(accs)); 
if (wordCount > 2) 
errAbort("Accession not standard, %s\n", acc->name);*/
safef(query, sizeof(query), "select organism from mrna where acc = '%s'", acc->name); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    id = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
if (id != -1)
    {
    safef(query, sizeof(query), "select name from organism where id = %d", id);   
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
      acc->organism = cloneString(row[0]);
    else
      printf("Could not find organism for id %d\n", id);
    sqlFreeResult(&sr);
    } 
else 
    printf("Could not find mrna record for %s\n", acc->name);
}

struct clone *getMrnaCloneId(struct sqlConnection *conn, char *acc)
/* Find the clone id for an mrna accession */
{
char query[256];
struct sqlResult *sr, *sr1;
char **row;
char *a;
char *accs[4];
int wordCount, i;
struct clone *ret = NULL;

AllocVar(ret);
ret->next = NULL;

safef(query, sizeof(query), "select mrnaClone from mrna where acc = '%s'", acc); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    ret->id = sqlUnsigned(row[0]);
    ret->imageId = 0;
    }
sqlFreeResult(&sr);
safef(query, sizeof(query), "select imageId from imageClone where acc = '%s'", acc); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    ret->imageId = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
return(ret);
}

boolean refseqAcc(struct refseq *r, char *name, char* rs)
/* Check if accession was used to create refseq sequence */
{
  /*struct refseq *r;*/
struct acc *a;
boolean ret = FALSE;

/*if (hashLookup(derived, rs))
  {
  r = hashFindVal(derived, rs);*/
  for (a = r->accList; a != NULL; a = a->next)
    if (sameString(a->name, name))
      ret = TRUE;
  /*}*/
return(ret);
}

boolean sameAcc(struct acc *a1, struct acc *a2)
/* Determine if two accessions are the same */
{
if ((a1 == NULL) || (a2 == NULL))
    return(0);
 if (sameString(a1->name,a2->name))
    return(1);
return(0);
}

boolean usedAcc(struct acc *a, struct acc *alist) 
/* Determine if acc is in list */
{
struct acc *el;

if ((a == NULL) || (alist == NULL))
    return(0);
for (el = alist; el != NULL; el = el->next)
    if (sameAcc(a, el))
       return(1);
return(0);
}

boolean sameClone(struct clone *c1, struct clone *c2)
/* Determine if two clones are the same */
{
if ((c1 == NULL) || (c2 == NULL))
    return(0);
 if (((c1->id) && (c2->id) && (c1->id == c2->id)) || ((c1->imageId) && (c2->imageId) && (c1->imageId == c2->imageId)))
    return(1);
return(0);
}

boolean usedClone(struct clone *c, struct clone *clist) 
/* Determine if clone is in list */
{
struct clone *el;

if ((c == NULL) || (clist == NULL))
    return(0);
for (el = clist; el != NULL; el = el->next)
    if (sameClone(c, el))
       return(1);
return(0);
}

int shiftIndel(char *indel, int size, int dir, int start, int end, struct dnaSeq *seq)
{
/* Determine how far insertion in seq can be shifted with changing an alignment */
  int ret = 0, offset = 0, i = 0;
  char *test;

  test = needMem(size+1);
  offset = start + (size*dir);
  if ((offset >= 0) && (offset < seq->size))
    {
      strncpy(test, seq->dna + offset, size);
      test[size] = '\0';
    }
  else
    test[0] = '\0';

  /* printf("Testing %s vs %s\n", test, indel); */
  while (sameString(indel, test)) {
    ret += size;
    offset = start + (ret*dir) + (size*dir);
    if ((offset >= 0) && (offset < seq->size))
      strncpy(test, seq->dna + offset, size);
    else
      test[0] = '\0';
    /* printf("Testing %s vs %s\n", test, indel); */
  }
  /* Check for pathological cases */
  for (i = 0; i < size; i++) 
    if (test[i] == indel[i]) 
      ret++;
    else
      i = size;
  freez(&test);
  /* printf("Shifting %d in direction %d\n", ret, dir); */
  return(ret);
}

void getCoords(struct psl *psl, int gstart, int gend, int *start, int *end, char *strand, boolean *nogap)
/* Get the genomic DNA that corresponds to an indel, and determine the corresponding \
   start and end positions for this sequence in the query sequence */ 
{
int gs = 0, ge = 0, off, i, bStart = -1, bEnd = -1;

/* Check that alignment covers the range */
if ((psl->tStart < gstart) && (psl->tEnd > gend))
  {
    /* Reverse complement xeno alignments if done on target - strand */
    if (psl->strand[1] == '-')
      pslRcBoth(psl);
    
    /* Look in all blocks for the indel */
    for (i = 0; i < psl->blockCount; i++) 
      {
	/* If the block contains the indel */   
	if (((psl->tStarts[i] + psl->blockSizes[i]) >= gstart) && (psl->tStarts[i] < gend))
	  {
	    /* Determine the start position offset */
	    if (gstart >= psl->tStarts[i])
	      {
		*start = psl->qStarts[i] + (gstart - psl->tStarts[i]);
		/*gs = gstart;*/
		bStart = i;
	      }
	    /* Determine the end position offset */
	    if (gend <= (psl->tStarts[i]+psl->blockSizes[i]))
	      {
		*end = psl->qStarts[i] + gend - psl->tStarts[i];
		/*ge = gend;*/
		bEnd = i;
	      }
	  }
	if ((gstart < psl->tStarts[i]) && (bStart < 0))
	  {
	    *start = psl->qStarts[i];
	    bStart = i;
	  }
	if ((gend > (psl->tStarts[i] + psl->blockSizes[i])) && (!*end))
	  bEnd = i;
      }
    
    if ((bEnd >= 0) && (!*end))
      *end = psl->qStarts[bEnd] + psl->blockSizes[bEnd];
    
    /* If opposite strand alignment, reverse the start and end positions in the mRNA */
    if (((psl->strand[0] == '-')  && (psl->strand[1] != '-')) 
	|| ((psl->strand[0] == '+') && (psl->strand[1] == '-')))
      {
	int tmp = *start;
	*start = psl->qSize - *end;
	*end = psl->qSize - tmp;
	/* reverseComplement(ret->dna, ret->size); */
	sprintf(strand, "-");
      }
    else
      sprintf(strand, "+");
    
    /* Check if mrna aligns completely in this region */
    if (((*end - *start) == (gend - gstart)) && (bStart > 0))
      *nogap = TRUE;
    else if ((bStart < bEnd) && (bStart > 0))
      {
	/*nogap = TRUE;
	  for (i = bStart; i < bEnd; i++) 
	  if ((psl->qStarts[i] + psl->blockSizes[i]) < psl->qStarts[i+1]) */
	*nogap = FALSE;
      }
  }
 else 
   {
     *start = 0;
     *end = 0;
   }

}

void searchTransPsl(char *table, DNA *mdna, struct indel *ni, char *strand, unsigned type, struct psl* psl, struct dnaSeq *gseq, struct acc *acc, int left, int right)
/* process one mRNA or EST for searchTrans */
{
int start = 0, end = 0;
boolean nogap = FALSE;
struct dnaSeq *dummy = NULL, *mseq = NULL;
char *dna = NULL, *dnaStart = NULL, *dnaEnd = NULL;
char thisStrand[2];
struct sqlConnection *conn2 = hAllocConn();
int mrnaSize = ni->mrnaEnd - ni->mrnaStart + left + right;
char accBuf[64];
char *p;

/* Get the start and end coordinates for the mRNA or EST sequence */
if ((type == INDEL) || (type == UNALIGNED))
    if (ni->chromStart == ni->chromEnd)
      getCoords(psl, ni->chromStart-left, ni->chromEnd+right, &start, &end, thisStrand, &nogap);
    else
      getCoords(psl, ni->chromStart-left, ni->chromEnd+right, &start, &end, thisStrand, &nogap);      
/*if (ni->chromStart == ni->chromEnd)
      getCoords(psl, ni->chromStart-3, ni->chromEnd+2, &start, &end, thisStrand, &nogap);
    else
    getCoords(psl, ni->chromStart-2, ni->chromEnd+2, &start, &end, thisStrand, &nogap);*/      
else
    getCoords(psl, ni->chromStart-1, ni->chromEnd, &start, &end, thisStrand, &nogap);

/* Get the corresponding mRNA or EST  sequence; db doesn't have versions,
 * so strip them. */
safef(accBuf, sizeof(accBuf),"%s", psl->qName);
p = strrchr(accBuf, '.');
if (p != NULL)
    *p = '\0';
struct dnaSeq *seq = hRnaSeq(accBuf);
if (thisStrand[0] != strand[0])
    {
    int temp = start;
    start = seq->size - end;
    end = seq->size - temp;
    reverseComplement(seq->dna, seq->size);
    }
if ((end-start) > 0)
    {
    dna = needMem((end-start)+1);
    strncpy(dna,seq->dna + start, end-start);
    dna[end-start] = '\0';
    }
else
    dna = cloneString("");
if ((type == INDEL) || (type == UNALIGNED))
  {

    if ((seq->size > (start + left + right)) && (start >= 0))
      {      
	dnaStart = needMem(mrnaSize+1);
	strncpy(dnaStart, seq->dna + start, mrnaSize);
	dnaStart[mrnaSize] = '\0';
      }
    if  ((end - mrnaSize) > 0)
      {
	dnaEnd = needMem(mrnaSize+1);
	strncpy(dnaEnd, seq->dna + end - mrnaSize, mrnaSize);
	dnaEnd[mrnaSize] = '\0';
      }    
    /*if ((seq->size > (start + 4)) && (start >= 0))
      {      
	dnaStart = needMem(mrnaSize+5);
	strncpy(dnaStart, seq->dna + start, mrnaSize + 4);
	dnaStart[mrnaSize+4] = '\0';
      }
    if  ((end - mrnaSize - 4) > 0)
      {
	dnaEnd = needMem(mrnaSize+5);
	strncpy(dnaEnd, seq->dna + end - mrnaSize - 4, mrnaSize + 4);
	dnaEnd[mrnaSize+4] = '\0';
	}*/    
  }
if (!dnaStart)
   dnaStart = cloneString("");
if (!dnaEnd)
   dnaEnd = cloneString("");


/* fprintf(stderr, "Comparing genomic %s at %d vs. %s %s vs. %s %s (%d-%d, %s vs. %s)\n", gseq->dna, ni->chromStart, ni->mrna->name, mdna, psl->qName, dna, start, end, thisStrand, strand);*/
/*fprintf(stderr, "Comparing genomic at %d-%d\n%s\n vs. %s\n%s\n vs. %s\n%s\n (%d-%d, %s vs. %s)\n", ni->chromStart, ni->chromEnd, gseq->dna, ni->mrna->name, mdna, psl->qName, dna, start, end, thisStrand, strand);*/
/*fprintf(stderr, "\tfrom start - %s\n\tfrom end - %s\n", dnaStart, dnaEnd);*/

/* If it doesn't align to this region */
if (start == end)
    {
    if (sameString(table, "mrna"))
        {
	  ni->hs->unMrna++;
	  slAddHead(&ni->hs->unMrnaAcc, acc);
        } 
    else if (sameString(table, "xenoMrna"))
        {
	  ni->xe->unMrna++;
	  findOrganism(conn2, acc);
	  slAddHead(&ni->xe->unMrnaAcc, acc);
	}
    else if (sameString(table, "est"))
        {
	  ni->hs->unEst++;
	  slAddHead(&ni->hs->unEstAcc, acc);
	}
     else if (sameString(table, "xenoEst"))
        {
	  ni->xe->unEst++;
	  findOrganism(conn2, acc);
	  slAddHead(&ni->xe->unEstAcc, acc);
	}
    }
/* If it agrees with the genomic sequence */
else if ((sameString(gseq->dna, dna)) || (((type == INDEL) || (type == UNALIGNED)) && (nogap))) 
    {
    if (sameString(table, "mrna"))
        {
	  ni->hs->genMrna++;
	  slAddHead(&ni->hs->genMrnaAcc, acc);
	} 
    else if (sameString(table, "xenoMrna"))
        {
	  ni->xe->genMrna++;
	  findOrganism(conn2, acc);
	  slAddHead(&ni->xe->genMrnaAcc, acc);
	}
    else if (sameString(table, "est"))
        {
	  ni->hs->genEst++;
	  slAddHead(&ni->hs->genEstAcc, acc);
	}
     else if (sameString(table, "xenoEst"))
        {
	  ni->xe->genEst++;
	  findOrganism(conn2, acc);
	  slAddHead(&ni->xe->genEstAcc, acc);
        }
    }
/* If it agrees with the mrna sequence */
else if ((sameString(mdna, dna)) || 
	 (((type == INDEL) || (type == UNALIGNED)) && 
	  ((strlen(dna) == strlen(mdna)) || (sameString(mdna, dnaStart)) || (sameString(mdna, dnaEnd)))))
  {
    if (sameString(table, "mrna"))
        {
	  ni->hs->mrnaMrna++;
	  slAddHead(&ni->hs->mrnaMrnaAcc, acc);
        } 
    else if (sameString(table, "xenoMrna"))
        {
	  ni->xe->mrnaMrna++;
	  findOrganism(conn2, acc);
	  slAddHead(&ni->xe->mrnaMrnaAcc, acc);
	}
    else if (sameString(table, "est"))
        {
	  ni->hs->mrnaEst++;
	  slAddHead(&ni->hs->mrnaEstAcc, acc);
	}
     else if (sameString(table, "xenoEst"))
        {
	  ni->xe->mrnaEst++;
	  findOrganism(conn2, acc);
	  slAddHead(&ni->xe->mrnaEstAcc, acc);
        }
    }
/* if it agrees with neither */
else 
    {
    if (sameString(table, "mrna"))
        {
	  ni->hs->noMrna++;
	  slAddHead(&ni->hs->noMrnaAcc, acc);
	}
    else if (sameString(table, "xenoMrna"))
        {
	  ni->xe->noMrna++;
	  findOrganism(conn2, acc);
	  slAddHead(&ni->xe->noMrnaAcc, acc);
	}
    else if (sameString(table, "est"))
        {
	  ni->hs->noEst++;
	  slAddHead(&ni->hs->noEstAcc, acc);
	}
     else if (sameString(table, "xenoEst"))
        {
	  ni->xe->noEst++;
	  findOrganism(conn2, acc);
	  slAddHead(&ni->xe->noEstAcc, acc);
	}
    }  

if (dnaStart)
   {
     freez(&dnaStart);
     freez(&dnaEnd);
   }
freez(&dna);
dnaSeqFree(&seq);
hFreeConn(&conn2);
}

void searchTrans(struct sqlConnection *conn, char *table, struct dnaSeq *rna, struct indel *ni, char *strand, unsigned type, struct clone *mrnaCloneId, int left, int right)
/* Find all mRNA's or EST's that align in the region of an indel, mismatch, or codon */
{
struct sqlResult *sr;
char **row;
int offset, start, end;
struct clone *cloneId;
struct psl *psl;
DNA mdna[10000];
struct sqlConnection *conn2 = hAllocConn();
struct dnaSeq *gseq;
char thisStrand[2];
struct acc *acc;
char *name;
struct refseq *rs = NULL;

if (type == CODONSUB)
    assert(((ni->mrnaEnd-ni->mrnaStart)+1) == 3);
if ((derived) && (hashLookup(derived, ni->mrna->name)))
     rs = hashFindVal(derived, ni->mrna->name);

/* Determine the sequence, If indel, add one base on each side */
/* if ((type == INDEL) || (type == UNALIGNED))
    {
	assert((ni->mrnaEnd-ni->mrnaStart+left+right+1) < sizeof(mdna));
        strncpy(mdna,rna->dna + ni->mrnaStart - left,ni->mrnaEnd-ni->mrnaStart+left+right);
        mdna[ni->mrnaEnd-ni->mrnaStart+left+right] = '\0';
      if (ni->mrnaStart == ni->mrnaEnd) 
	{
	assert((ni->mrnaEnd-ni->mrnaStart+5) < sizeof(mdna));
        strncpy(mdna,rna->dna + ni->mrnaStart - 2,ni->mrnaEnd-ni->mrnaStart+4);
        mdna[ni->mrnaEnd-ni->mrnaStart+4] = '\0';
	}
      else
      {
	assert((ni->mrnaEnd-ni->mrnaStart+5) < sizeof(mdna));
        strncpy(mdna,rna->dna + ni->mrnaStart - 2,ni->mrnaEnd-ni->mrnaStart+4);
        mdna[ni->mrnaEnd-ni->mrnaStart+4] = '\0';
      }
    }
else
    {*/
    int len = ni->mrnaEnd-ni->mrnaStart+left+right;
    assert((len+1) < sizeof(mdna));
    strncpy(mdna,rna->dna + ni->mrnaStart - left,len);
    mdna[len] = '\0';
    /* printf("Mismatch/Indel at %d-%d (%d) in %s:%d-%d bases %s, left=%d, right=%d\n", ni->mrnaStart, ni->mrnaEnd, rna->size, ni->chrom, ni->chromStart, ni->chromEnd, mdna, left, right); */
    /*printf("Mismatch/indel in %s at %d in %s:%d bases %s\n", ni->mrna->name, ni->mrnaStart, ni->chrom, ni->chromStart, mdna);*/
    /*    }*/

/* Get dna sequence */
 if ((type == INDEL) || (type == UNALIGNED))
   if (ni->chromStart == ni->chromEnd)
     gseq = hDnaFromSeq(ni->chrom, ni->chromStart-left, ni->chromEnd+right, dnaLower);
   else
     gseq = hDnaFromSeq(ni->chrom, ni->chromStart-left, ni->chromEnd+right, dnaLower);
 /*if (ni->chromStart == ni->chromEnd)
     gseq = hDnaFromSeq(ni->chrom, ni->chromStart-3, ni->chromEnd+2, dnaLower);
   else
   gseq = hDnaFromSeq(ni->chrom, ni->chromStart-2, ni->chromEnd+2, dnaLower);*/
else
  gseq = hDnaFromSeq(ni->chrom, ni->chromStart-1, ni->chromEnd, dnaLower);
if (strand[0] == '-')
  reverseComplement(gseq->dna, gseq->size);


/* Find all sequences that span this region */
 if ((type == INDEL) || (type == UNALIGNED))
   if (ni->chromStart == ni->chromEnd)
     sr = hRangeQuery(conn, table, ni->chrom, ni->chromStart-left, ni->chromEnd+right, NULL, &offset);
   else
     sr = hRangeQuery(conn, table, ni->chrom, ni->chromStart-left, ni->chromEnd+right, NULL, &offset);
 /*if (ni->chromStart == ni->chromEnd)
     sr = hRangeQuery(conn, table, ni->chrom, ni->chromStart-3, ni->chromEnd+2, NULL, &offset);
   else
   sr = hRangeQuery(conn, table, ni->chrom, ni->chromStart-2, ni->chromEnd+2, NULL, &offset);*/
else 
    sr = hRangeQuery(conn, table, ni->chrom, ni->chromStart-1, ni->chromEnd, NULL, &offset);
while ((row = sqlNextRow(sr)) != NULL) 
    {
    psl = pslLoad(row+offset);
    name = cloneString(psl->qName);
    acc = createAcc(name);
    cloneId = getMrnaCloneId(conn2, acc->name);
    if ((!sameAcc(acc,ni->mrna)) && (!sameClone(cloneId,mrnaCloneId)) && 
	((!rs) || (!refseqAcc(rs, acc->name, ni->mrna->name))) &&
	(!usedClone(cloneId, ni->clones)) && (!usedAcc(acc, ni->accs)))
      {
	slAddHead(&(ni->clones), cloneId);
	slAddHead(&(ni->accs), acc);	
	searchTransPsl(table, mdna, ni, strand, type, psl, gseq, acc, left, right);
      }
    else 
      {
	accFree(&acc);
	cloneFree(&cloneId);
      }
    pslFree(&psl);
    }
dnaSeqFree(&gseq);
sqlFreeResult(&sr);
hFreeConn(&conn2);
}

struct evid *createEvid()
/* Create an instance of an evid struct */
{
struct evid *ev;

 AllocVar(ev);
 ev->next = NULL;
 ev->genMrna = 0;
 ev->genMrnaAcc = NULL;
 ev->genEst = 0;
 ev->genEstAcc = NULL;
 ev->mrnaMrna = 0;
 ev->mrnaMrnaAcc = NULL;
 ev->mrnaEst = 0;
 ev->mrnaEstAcc = NULL;
 ev->noMrna = 0;
 ev->noMrnaAcc = NULL;
 ev->noEst = 0;
 ev->noEstAcc = NULL;
 ev->unMrna = 0;
 ev->unMrnaAcc = NULL;
 ev->unEst = 0;
 ev->unEstAcc = NULL;
 return(ev);
}

struct indel *createMismatch(struct sqlConnection *conn, char *mrna, int mbase, char* chr, int gbase, 
			     struct dnaSeq *rna, char *strand, struct clone *cloneId, struct acc *acc, 
			     boolean snp, boolean valid, char r, char g)
/* Create a record of a mismatch */
{
  struct indel *mi;

  AllocVar(mi);
  mi->next = NULL;
  mi->size = 1;
  mi->chrom = chr;
  mi->chromStart = mi->chromEnd = gbase;
  mi->mrna = acc;
  mi->mrnaStart = mi->mrnaEnd = mbase;
  mi->hs = createEvid();
  mi->xe = createEvid();
  mi->knownSnp = snp;
  mi->validatedSnp = valid;
  mi->insertion = FALSE;
  mi->mrnaSeq = needMem(2);
  mi->mrnaSeq[0] = r;
  mi->mrnaSeq[1] = '\0';
  mi->genSeq = needMem(2);
  mi->genSeq[0] = g;
  mi->genSeq[1] = '\0';

 /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in mismatch */
  searchTrans(conn, "mrna", rna, mi, strand, MISMATCH, cloneId, 0, 1);
  searchTrans(conn, "est", rna, mi, strand, MISMATCH, cloneId, 0, 1);
  if (xeno)
      {
      searchTrans(conn, "xenoMrna", rna, mi, strand, MISMATCH, cloneId, 0, 1);
      searchTrans(conn, "xenoEst", rna, mi, strand, MISMATCH, cloneId, 0, 1);
      }

  return(mi);
}

struct indel *createCodonSub(struct sqlConnection *conn, int mrnaStart,
                             char *mCodon, char* chr, int genPos[3], char* gCodon,
                             struct dnaSeq *rna, char *strand, struct clone *cloneId,
			     boolean knownSnp, struct acc *acc)
/* Create a record of a mismatch */
{
  struct indel *mi;
#if 0
  printf("codonSub: mrna=%s mrnaStart=%d  genPos=%d,%d,%d mCodon=%s,gCodon=%s, aa=%c %c\n",
         acc->name, mrnaStart, genPos[0], genPos[1], genPos[2], mCodon, gCodon,
         lookupCodon(mCodon), lookupCodon(gCodon));
#endif
 
  AllocVar(mi);
  mi->next = NULL;
  mi->size = 1;
  mi->chrom = chr;
  mi->chromStart = genPos[0];
  mi->chromEnd = genPos[2];
  mi->mrna = acc;
  mi->mrnaStart = mrnaStart-2;
  mi->mrnaEnd = mrnaStart;
  memcpy(mi->codonGenPos, genPos, sizeof(mi->codonGenPos));
  strcpy(mi->genCodon, gCodon);
  strcpy(mi->mrnaCodon, mCodon);
  mi->hs = createEvid();
  mi->xe = createEvid();
  mi->knownSnp = knownSnp;
  mi->validatedSnp = FALSE;
    
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in mismatch */
  searchTrans(conn, "mrna", rna, mi, strand, CODONSUB, cloneId, 0, 1);
  searchTrans(conn, "est", rna, mi, strand, CODONSUB, cloneId, 0, 1);
  if (xeno)
      {
      searchTrans(conn, "xenoMrna", rna, mi, strand, CODONSUB, cloneId, 0, 1);
      searchTrans(conn, "xenoEst", rna, mi, strand, CODONSUB, cloneId, 0, 1);
      }
  return(mi);
}

void cdsCompare(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna, struct dnaSeq *dna)
/* Compare the alignment of the coding and UTR regions of an mRNA to the genomic sequence */
{
int i,j;
DNA *r, *d; 
DNA rCodon[4], dCodon[4];
int codonSnps = 0, codonMismatches = 0, valid = 0;
int codonGenPos[3];
int codonMrnaStart = 0;
int nCodonBases = 0;   /* to deal with partial codons */
struct indel *mi, *miList=NULL;
struct indel *codonSub, *codonSubList=NULL;
ZeroVar(codonGenPos);
boolean knownSnp = FALSE, mmSnp = FALSE;

strcpy(rCodon, "---");
strcpy(dCodon, "---");
r = rna->dna;
d = dna->dna;
pi->cdsMatch = pi->cdsMismatch = 0;
/* Look at all alignment blocks */
for (i = 0; i < pi->psl->blockCount; i++) 
    {
    int qstart = pi->psl->qStarts[i];
    int tstart = pi->psl->tStarts[i] - pi->psl->tStarts[0];

    /* Compare each base */
    for (j = 0; j < pi->psl->blockSizes[i]; j++) 
        {
        /* Check if it is in the coding region */
	if (((qstart+j) >= pi->cdsStart) && ((qstart+j) < pi->cdsEnd))
            {
	    int tPosition = tstart + j + pi->psl->tStarts[0];
            int iCodon = ((qstart+j-pi->cdsStart)%3);
            if (iCodon == 0) {
                codonSnps = 0;
                codonMismatches = 0;
                codonMrnaStart = qstart+j;
            }
	    if (pi->psl->strand[0] == '-')
                {
	        tPosition = pi->psl->tSize - tPosition - 1;
                codonGenPos[2-iCodon] = tPosition + 1;
                }
            else
                {
                codonGenPos[iCodon] = tPosition + 1;
                }
            rCodon[iCodon] = r[qstart+j];
            dCodon[iCodon] = d[tstart+j];
            nCodonBases++;
            /* Bases match */
            if ((char)r[qstart+j] == (char)d[tstart+j])
                pi->cdsMatch++;
            /* Check if mismatch is due to a SNP */
            else if ((valid = snpBase(conn,pi->psl->tName,tPosition)) > 0)
                {
                pi->cdsMatch++;
                pi->snp++;
                codonSnps++;
		valid--;
		if (mismatchReport)
		    {
		    mi = createMismatch(conn, pi->mrna->name, qstart+j, pi->psl->tName, tPosition+1, rna, pi->psl->strand, pi->mrnaCloneId, pi->mrna, TRUE, valid, r[qstart+j], d[tstart+j]);
		    slAddHead(&miList,mi);
		    }
                }
            else
                {
                pi->cdsMismatch++;
		if (mismatchReport)
		    {
		    mi = createMismatch(conn, pi->mrna->name, qstart+j, pi->psl->tName, tPosition+1, rna, pi->psl->strand, pi->mrnaCloneId, pi->mrna, FALSE, FALSE, r[qstart+j], d[tstart+j]);
		    slAddHead(&miList,mi);
		    }
                codonMismatches++;
                /* Check if mismatch is in a codon wobble position.*/
                if (iCodon==2)
                    pi->thirdPos++;
                }
            /* If third base, check codon for mismatch */
            if ((iCodon==2) && (nCodonBases == 3) && !sameString(rCodon, dCodon))
                {
		if ((codonSnps) && (codonMismatches == 0))
		    knownSnp = TRUE;
		else
		    knownSnp = FALSE;
                if (lookupCodon(rCodon) == lookupCodon(dCodon))
                    {
                    pi->synSub++;
                    if ((codonSnps > 0) && (codonMismatches == 0))
			pi->synSubSnp++;
                    }		
                else
                    {
                    pi->nonSynSub++;
                    if ((codonSnps > 0) && (codonMismatches == 0))
                        pi->nonSynSubSnp++;
                    }
		if (codonSubReport)
                    {
                    codonSub = createCodonSub(conn, qstart+j,
                                              rCodon, pi->psl->tName, codonGenPos,
                                              dCodon, rna, pi->psl->strand, 
					      pi->mrnaCloneId, knownSnp, pi->mrna);
                    slAddHead(&codonSubList, codonSub);
                    }
                }
            if (iCodon == 2) 
                nCodonBases = 0;
            }
        }
    }
if (mismatchReport)
    { 
    slReverse(&miList);
    pi->mmList = miList;
    }
if (codonSubReport)
    {
    slReverse(&codonSubList);
    pi->codonSubList = codonSubList;
    }
}

struct indel *createUnali(struct sqlConnection *conn, int mstart, int mend, char* chr, int gstart, int gend, 
			  struct dnaSeq *rna, char *strand, struct clone *cloneId, struct acc *acc, int left, int right, char* insert)
/* Create a record of an unaligned region of cds */
{
  struct indel *ni;
 
  AllocVar(ni);
  ni->next = NULL;
  ni->size = mend - mstart;
  ni->chrom = chr;
  ni->chromStart = gstart;
  ni->chromEnd = gend;
  ni->mrna = acc;
  ni->mrnaStart = mstart;
  ni->mrnaEnd = mend;
  ni->hs = createEvid();
  ni->xe = createEvid();
  ni->insertion = FALSE;
  ni->mrnaSeq = insert;
  ni->genSeq = NULL;
 
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in indel region */
  searchTrans(conn, "mrna", rna, ni, strand, UNALIGNED, cloneId, left, right);
  searchTrans(conn, "est", rna, ni, strand, UNALIGNED, cloneId, right, left);
  if (xeno)
      {
      searchTrans(conn, "xenoMrna", rna, ni, strand, UNALIGNED, cloneId, left, right);
      searchTrans(conn, "xenoEst", rna, ni, strand, UNALIGNED, cloneId, right, left);
      }
  
  return(ni);
}

struct indel *createIndel(struct sqlConnection *conn, int mstart, int mend, char* chr, int gstart, int gend, 
			  struct dnaSeq *rna, char *strand, struct clone *cloneId, struct acc *acc, int left, int right, char* seq, boolean insert)
/* Create a record of an indel */
{
  struct indel *ni;
 
  AllocVar(ni);
  ni->next = NULL;
  if ((mend - mstart) > 0) 
    ni->size = mend - mstart;
  else
    ni->size = gend - gstart;
  ni->chrom = chr;
  ni->chromStart = gstart;
  ni->chromEnd = gend;
  ni->mrna = acc;
  ni->mrnaStart = mstart;
  ni->mrnaEnd = mend;
  ni->hs = createEvid();
  ni->xe = createEvid();
  ni->insertion = insert;
  ni->genSeq = NULL;
  ni->mrnaSeq = NULL;
  if (insert)
    ni->mrnaSeq = seq;
  else
    ni->genSeq = seq;
  ni->knownSnp = FALSE;
  ni->validatedSnp = FALSE;
 
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in indel region */
  searchTrans(conn, "mrna", rna, ni, strand, INDEL, cloneId, left, right);
  searchTrans(conn, "est", rna, ni, strand, INDEL, cloneId, left, right);
  if (xeno)
      {
      searchTrans(conn, "xenoMrna", rna, ni, strand, INDEL, cloneId, left, right);
      searchTrans(conn, "xenoEst", rna, ni, strand, INDEL, cloneId, left, right);
      }

  return(ni);
}

void cdsIndels(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna)
/* Find indels in coding region of mRNA alignment */
{
  int i, j;
  int unaligned=0, prevqend=0, prevtend=0, qstart=0, tstart=0;
  int leftShift = 0, rightShift = 0, tLeft = 0, tRight = 0, startIndel = 0;
  struct indel *ni=NULL, *niList=NULL, *uiList=NULL;
  DNA *insert;
  int cdsS = pi->cdsStart, cdsE = pi->cdsEnd;
  boolean unali = FALSE;
  struct dnaSeq *gseq;
  
  /* Check all blocks for indels */
  for (i = 1; i < pi->psl->blockCount; i++)
  {  
    /* Find insertions in the mRNA sequence */
    unaligned = 0;
    prevqend = 0;
    prevtend = 0;
    qstart = pi->psl->qStarts[i];
    tstart = pi->psl->tStarts[i];
    /* If block is in the cds region */
    if ((qstart >= cdsS) && (qstart <= cdsE))
      {
        /* Determine where previous block left off in the alignment */
	prevqend = pi->psl->qStarts[i-1] + pi->psl->blockSizes[i-1];
	prevtend = pi->psl->tStarts[i-1] + pi->psl->blockSizes[i-1];
	/* Adjust if previous end is not in coding region */
	if (prevqend < pi->cdsStart)
	  {
	  prevqend = cdsS;
	  prevtend = tstart;
	  }
	unaligned = qstart - prevqend;
	/*if (((tstart - prevtend) != 0) && (!pi->splice[i]) && ((tstart - prevtend) < 30))*/
	if (((tstart - prevtend) != 0) && (!pi->splice[i]))
	  unali = TRUE;
	else
	  unali = FALSE;
	/* Check if there is an indel */
	if (unaligned > 0)
	  {
	    /* Check if unaligned part is a gap in the mRNA alignment, not an insertion */
	    if (unaligned > 30)
	      pi->cdsGap += unaligned;
	    if (!unali)
	        {
	        if (unaligned == 1) 
	            pi->singleIndel++;
		if ((unaligned%3) == 0) 
	            pi->tripleIndel += unaligned/3;
	        pi->totalIndel += unaligned;
	        pi->indels[pi->indelCount] = unaligned;
	        pi->indelCount++;
		}
	    pi->unalignedCds += unaligned;
	    /*if ((pi->cdsPctId >= 0.98) && (pi->cdsCoverage >= 0.80))
	      indels[unaligned]++;*/
	    if (pi->indelCount > 256) 
		errAbort("Indel count too high");
	    /* Determine boundaries of the indel */
	    if (pi->psl->strand[0] == '-') {
	      int temp = tstart;
	      tstart = pi->psl->tSize - prevtend;
	      prevtend = pi->psl->tSize - temp;
	      tLeft = pi->psl->tSize - pi->psl->tStarts[i] - pi->psl->blockSizes[i];
	      tRight = pi->psl->tSize - pi->psl->tStarts[i-1];
	      gseq = hDnaFromSeq(pi->psl->tName, tLeft, tRight, dnaLower);
	      reverseComplement(gseq->dna, gseq->size);
	      startIndel = tRight - tstart;
	    } else {
	      tLeft = pi->psl->tStarts[i-1];
	      tRight = pi->psl->tStarts[i] + pi->psl->blockSizes[i];
	      gseq = hDnaFromSeq(pi->psl->tName, tLeft, tRight, dnaLower);
	      startIndel = prevtend-tLeft;
	    }
	    insert = needMem(unaligned+1);
	    strncpy(insert, rna->dna + prevqend, unaligned);
	    insert[unaligned] = '\0';
	    leftShift = shiftIndel(insert, unaligned, -1, startIndel, tRight-tLeft, gseq);
	    rightShift = shiftIndel(insert, unaligned, 1, startIndel-1, tRight-tLeft, gseq);
	    /*free(insert);*/
	    if (pi->psl->strand[0] == '-') 
	      {
		int temp = leftShift;
		prevqend += rightShift;
		qstart += rightShift;
		prevtend += rightShift;
		tstart += rightShift;
		leftShift = rightShift;
		rightShift = temp;
	      }
	    prevqend -= leftShift;
	    qstart -= leftShift;
	    prevtend -= leftShift;
	    tstart -= leftShift;
            /* Create an unali record for this */	
	    if ((unaliReport) && (unali)) 
	      {
		ni = createUnali(conn, prevqend, qstart, pi->psl->tName, prevtend, tstart, rna, pi->psl->strand, pi->mrnaCloneId, pi->mrna, 1, leftShift + rightShift + 1, insert); 
		slAddHead(&uiList, ni);
	      }
            /* Create an indel record for this */	
	    if ((indelReport) && (!unali)) 
	      {
		ni = createIndel(conn, prevqend, qstart, pi->psl->tName, prevtend, prevtend, rna, pi->psl->strand, pi->mrnaCloneId, pi->mrna, 1, leftShift + rightShift + 1, insert, TRUE); 
		slAddHead(&niList,ni);
	      }
	  }
      }
    
    /* Find deletions in the mRNA sequence */
    unaligned = 0;
    qstart = pi->psl->qStarts[i];
    tstart = pi->psl->tStarts[i];
    prevqend = 0;
    prevtend = 0;
    /* Check if in the coding region */
    if ((qstart >= cdsS) && (qstart <= cdsE))
      {
        /* Determine where previous block left off in the alignment */
	prevqend = pi->psl->qStarts[i-1] + pi->psl->blockSizes[i-1];
	prevtend = pi->psl->tStarts[i-1] + pi->psl->blockSizes[i-1];
	/* Adjust if previous end is not in coding region */
	if (prevqend < pi->cdsStart)
	  {
	  prevqend = qstart;
	  prevtend = tstart;
	  }
	unaligned = tstart - prevtend;
	if ((qstart - prevqend) > 0)
	    unali = TRUE;
	else
	    unali = FALSE;
	/* Check if unaligned part is an intron */
	if ((unaligned > 30) || (pi->splice[i]))
	  /*pi->cdsGap += unaligned;*/
	  pi->cdsGap += 0;
	/* Check if there is an indel */
	else if ((unaligned != 0) && (!unali)) 
	  {
	    if (unaligned == 1) 
	      pi->singleIndel++;
	    if ((unaligned%3) == 0) 
	      pi->tripleIndel++;
	    pi->totalIndel -= unaligned;
	    pi->indels[pi->indelCount] = unaligned;
	    pi->indelCount++;
	    if (pi->indelCount > 256) 
	      errAbort("Indel count too high");
	    /*if ((pi->cdsPctId >= 0.98) && (pi->cdsCoverage >= 0.80))
	      indels[unaligned]++;*/
	    /* Create an indel record for this */
	    if (pi->psl->strand[0] == '-') {
	      int temp = tstart;
	      tstart = pi->psl->tSize - prevtend;
	      prevtend = pi->psl->tSize - temp;
	      tLeft = pi->psl->tSize - pi->psl->tStarts[i] - pi->psl->blockSizes[i];
	      tRight = pi->psl->tSize - pi->psl->tStarts[i-1];
	      gseq = hDnaFromSeq(pi->psl->tName, tLeft, tRight, dnaLower);
	      reverseComplement(gseq->dna, gseq->size);
	      startIndel = tRight - tstart;
	    } else {
	      tLeft = pi->psl->tStarts[i-1];
	      tRight = pi->psl->tStarts[i] + pi->psl->blockSizes[i];
	      gseq = hDnaFromSeq(pi->psl->tName, tLeft, tRight, dnaLower);
	      startIndel = prevtend - tLeft;
	    }
	    insert = needMem(unaligned+1);
	    strncpy(insert, gseq->dna + startIndel, unaligned);
	    insert[unaligned] = '\0';
	    leftShift = shiftIndel(insert, unaligned, -1, startIndel, tLeft+tRight, gseq);
	    rightShift = shiftIndel(insert, unaligned, 1, startIndel, tLeft+tRight, gseq);
	    /* free(insert); */
	    if (pi->psl->strand[0] == '-') 
	      {
		int temp = leftShift;
		prevqend += rightShift;
		qstart += rightShift;
		prevtend += rightShift;
		tstart += rightShift;
		leftShift = rightShift;
		rightShift = temp;
	      }
	    prevqend -= leftShift;
	    qstart -= leftShift;
	    prevtend -= leftShift;
	    tstart -= leftShift;
	    if (indelReport)
	        {
		ni = createIndel(conn, prevqend, qstart, pi->psl->tName, prevtend, tstart, rna, pi->psl->strand, pi->mrnaCloneId, pi->mrna, 1, rightShift + leftShift + 1, insert, FALSE); 
		slAddHead(&niList,ni);
		}
	  }
      }
  }
  if (indelReport)
      { 
      slReverse(&niList);
      pi->indelList = niList;
      }
  if (unaliReport)
      { 
      slReverse(&uiList);
      pi->unaliList = uiList;
      }
}

void processCds(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna, struct dnaSeq *dna)
/* Examine closely the alignment of the coding region */
{
struct acc *acc;
char *name = cloneString(pi->psl->qName);

verbose(1, "Processing %s\n", name);
/* Create the accession for the query */
acc = createAcc(name);
/* Compare the actual aligned parts */
cdsCompare(conn, pi, rna, dna);
pi->cdsPctId = (float)(pi->cdsMatch)/(pi->cdsMatch + pi->cdsMismatch);
pi->cdsCoverage = (float)(pi->cdsMatch + pi->cdsMismatch)/(pi->cdsSize);

/* Determine indels in the alignment */
cdsIndels(conn, pi, rna);
} 

struct pslInfo *processPsl(struct sqlConnection *conn, struct psl *psl)
/* Analyze an alignment of a mRNA to the genomic sequence */
{
struct pslInfo *pi;
struct dnaSeq *rnaSeq;
struct dnaSeq *dnaSeq;
char *name = cloneString(psl->qName);

AllocVar(pi);
pi->psl = psl;
pi->mrna = createAcc(name);
pi->pctId = (float)(psl->match + psl->repMatch)/(psl->match + psl->repMatch + psl->misMatch);
pi->coverage = (float)(psl->match + psl->misMatch + psl->repMatch)/(psl->qSize);
if (!hashLookup(cdsStarts, pi->mrna->name))
    pi->cdsStart = 0;  
else
    pi->cdsStart = hashIntVal(cdsStarts, pi->mrna->name);
if (!hashLookup(cdsEnds, pi->mrna->name))
    pi->cdsEnd = psl->qSize; 
else
    pi->cdsEnd = hashIntVal(cdsEnds, pi->mrna->name);
pi->cdsSize = pi->cdsEnd - pi->cdsStart;
pi->introns = countIntrons(psl->blockCount, psl->blockSizes, psl->tStarts);
if (loci != NULL)
    pi->loci = hashIntVal(loci, pi->mrna->name);
else
    pi->loci = nextFakeLoci++;
pi->indelCount = 0;
pi->totalIndel = 0;
pi->mrnaCloneId = getMrnaCloneId(conn, pi->mrna->name);
if (derived && hashLookup(derived, pi->mrna->name))
   pi->refseq = hashFindVal(derived, pi->mrna->name);
else 
  pi->refseq = NULL;

/* Get the corresponding sequences */
rnaSeq = hashMustFindVal(rnaSeqs, pi->mrna->name);
dnaSeq = hDnaFromSeq(psl->tName, psl->tStart, psl->tEnd, dnaLower);

/* Reverse compliment genomic and psl record if aligned on opposite strand */
if (psl->strand[0] == '-') 
  {
   reverseComplement(dnaSeq->dna, dnaSeq->size);
   pslRcBoth(pi->psl);
  }

/* Analyze the coding region */
pi->stdSplice = countStdSplice(psl, dnaSeq->dna, pi);
processCds(conn, pi, rnaSeq, dnaSeq);

/* Revert back to original psl record for printing */
if (psl->strand[0] == '-') 
  {
   pslRcBoth(pi->psl);
  }

freeDnaSeq(&dnaSeq);
return(pi);
}

void writeList(FILE *of, struct indel *iList, int type, struct psl *psl, struct acc *a)
/* Write out an list of indel/mismatches/codon subs*/
{
struct indel *indel;
struct acc *acc;

for (indel = iList; indel != NULL; indel=indel->next)
    {
      /*printf("Indel of size %d in %s:%d-%d vs. %s:%d-%d\n",
    indel->size, indel->mrna, indel->mrnaStart, indel->mrnaEnd,
    indel->chrom, indel->chromStart, indel->chromEnd);*/
    if (type == INDEL)
      {
        fprintf(of, "Indel of size %d in %s:%d-%d vs. %s:%d-%d, %s vs. %s",
		indel->size, accFmt(indel->mrna), indel->mrnaStart, indel->mrnaEnd,
		indel->chrom, indel->chromStart, indel->chromEnd, indel->mrnaSeq, 
		indel->genSeq);
	if (indel->insertion)
	  fprintf(of, ", INSERTION\n");
	else
	  fprintf(of, ", DELETION\n");
      }
    else if (type == UNALIGNED)
        fprintf(of, "Unaligned bases of size %d in %s:%d-%d vs. %s:%d-%d, %s vs. %s\n",
                indel->size, accFmt(indel->mrna), indel->mrnaStart, indel->mrnaEnd,
                indel->chrom, indel->chromStart, indel->chromEnd, indel->mrnaSeq,
		indel->genSeq);
    else if (type == MISMATCH)
      {
      fprintf(of, "Mismatch at %s:%d vs. %s:%d, %s vs. %s",
              accFmt(indel->mrna), indel->mrnaStart, indel->chrom, indel->chromStart,
	      indel->mrnaSeq, indel->genSeq);
      if (indel->knownSnp)
	if (indel->validatedSnp)
	  fprintf(of, ", SNP-validated");
	else
          fprintf(of, ", SNP\n");
      else
          fprintf(of, "\n");	   
      }
    else if (type == CODONSUB)
       {
       char mrnaAA = lookupCodon(indel->mrnaCodon);
       char genAA = lookupCodon(indel->genCodon);
       bool isSyn = (mrnaAA == genAA);
       fprintf(of, "%s codon substitution at %s:%d vs. %s:%d,%d,%d, %s vs. %s, ",
	       (isSyn ? "synonymous" : "non-synonymous"),
	       accFmt(indel->mrna), indel->mrnaStart, indel->chrom, indel->codonGenPos[0],
	       indel->codonGenPos[1], indel->codonGenPos[2],
	       indel->mrnaCodon, indel->genCodon);
       if (mrnaAA == 0)
	   fprintf(of, "STOP vs. ");
       else
           fprintf(of, "%c vs. ", mrnaAA);
       if (genAA == 0)
           fprintf(of, "STOP");
       else
           fprintf(of, "%c", genAA);
       if (indel->knownSnp)
	   fprintf(of, ", SNP\n");
       else
	   fprintf(of, "\n");	   
       fprintf(of, "\tpsl %s %d %d\t%s %d %d\n",
	       a->name, psl->qStart, psl->qEnd,
	       psl->tName, psl->tStart, psl->tEnd);
       } 
    fprintf(of, "\t%d human mRNAs support genomic: ", indel->hs->genMrna);
    slReverse(&(indel->hs->genMrnaAcc));
    for (acc = indel->hs->genMrnaAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", accFmt(acc));
    fprintf(of, "\n\t%d human ESTs support genomic: ",indel->hs->genEst);
    slReverse(&(indel->hs->genEstAcc));
    for (acc = indel->hs->genEstAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", accFmt(acc));
    fprintf(of, "\n\t%d human mRNAs support %s: ", indel->hs->mrnaMrna, accFmt(indel->mrna));
    slReverse(&(indel->hs->mrnaMrnaAcc));
    for (acc = indel->hs->mrnaMrnaAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", accFmt(acc));
    fprintf(of, "\n\t%d human ESTs support %s: ",indel->hs->mrnaEst, accFmt(indel->mrna));
    slReverse(&(indel->hs->mrnaEstAcc));
    for (acc = indel->hs->mrnaEstAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", accFmt(acc));
    fprintf(of, "\n\t%d human mRNAs support neither: ", indel->hs->noMrna);
    slReverse(&(indel->hs->noMrnaAcc));
    for (acc = indel->hs->noMrnaAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", accFmt(acc));
    fprintf(of, "\n\t%d human ESTs support neither: ",indel->hs->noEst);
    slReverse(&(indel->hs->noEstAcc));
    for (acc = indel->hs->noEstAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", accFmt(acc));
    fprintf(of, "\n\t%d human mRNAs do not align: ", indel->hs->unMrna);
    slReverse(&(indel->hs->unMrnaAcc));
    for (acc = indel->hs->unMrnaAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", accFmt(acc));
    fprintf(of, "\n\t%d human ESTs do not align: ",indel->hs->unEst);
    slReverse(&(indel->hs->unEstAcc));
    for (acc = indel->hs->unEstAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", accFmt(acc));

    if (xeno)
        {
        fprintf(of, "\n\n\t%d xeno mRNAs support genomic: ", indel->xe->genMrna);
	slReverse(&(indel->xe->genMrnaAcc));
	for (acc = indel->xe->genMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", accFmt(acc), acc->organism);
	fprintf(of, "\n\t%d xeno ESTs support genomic: ",indel->xe->genEst);
	slReverse(&(indel->xe->genEstAcc));
	for (acc = indel->xe->genEstAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, accFmt(acc));
	fprintf(of, "\n\t%d xeno mRNAs support %s: ", indel->xe->mrnaMrna, accFmt(indel->mrna));
	slReverse(&(indel->xe->mrnaMrnaAcc));
	for (acc = indel->xe->mrnaMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", accFmt(acc), acc->organism);
	fprintf(of, "\n\t%d xeno ESTs support %s: ",indel->xe->mrnaEst, accFmt(indel->mrna));
	slReverse(&(indel->xe->mrnaEstAcc));
	for (acc = indel->xe->mrnaEstAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", accFmt(acc), acc->organism);
	fprintf(of, "\n\t%d xeno mRNAs support neither: ", indel->xe->noMrna);
	slReverse(&(indel->xe->noMrnaAcc));
	for (acc = indel->xe->noMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", accFmt(acc), acc->organism);
	fprintf(of, "\n\t%d xeno ESTs support neither: ",indel->xe->noEst);
	slReverse(&(indel->xe->noEstAcc));
	for (acc = indel->xe->noEstAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", accFmt(acc), acc->organism);
	fprintf(of, "\n\t%d xeno mRNAs do not align: ", indel->xe->unMrna);
	slReverse(&(indel->xe->unMrnaAcc));
	for (acc = indel->xe->unMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", accFmt(acc), acc->organism);
	fprintf(of, "\n\t%d xeno ESTs do not align: ",indel->xe->unEst);
	slReverse(&(indel->xe->unEstAcc));
	for (acc = indel->xe->unEstAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", accFmt(acc), acc->organism);
	}
    fprintf(of, "\n\n");
    }
}

void writeOut(FILE *of, FILE *in, FILE *mm, FILE* cs, FILE* un, struct pslInfo *pi)
/* Output results of the mRNA alignment analysis */
{
int i;

fprintf(of, "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t", accFmt(pi->mrna), pi->psl->tName, pi->psl->tStart, 
	pi->psl->tEnd,pi->psl->qStart,pi->psl->qEnd,pi->psl->qSize,pi->loci);
fprintf(of, "%.4f\t%.4f\t%d\t%d\t%.4f\t%.4f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t", 
	pi->coverage, pi->pctId, pi->cdsStart+1, 
	pi->cdsEnd, pi->cdsCoverage, pi->cdsPctId, pi->cdsMatch, 
	pi->cdsMismatch, pi->snp, pi->thirdPos, pi->synSub, pi->nonSynSub,
        pi->synSubSnp, pi->nonSynSubSnp, pi->introns, pi->stdSplice);
fprintf(of, "%d\t%d\t%d\t%d\t", pi->unalignedCds, pi->singleIndel, pi->tripleIndel, pi->totalIndel);
for (i = 0; i < pi->indelCount; i++)
    fprintf(of, "%d,", pi->indels[i]);
fprintf(of, "\t%d", pi->cdsGap);
fprintf(of, "\n");

/* Write out detailed records of indels, if requested */
if (indelReport) 
    {
    verbose(1, "Writing out indels\n");
    writeList(in, pi->indelList, INDEL, NULL, NULL);
    }

/* Write out detailed records of indels, if requested */
if (unaliReport) 
    {
    verbose(1, "Writing out unaligned regions\n");
    writeList(un, pi->unaliList, UNALIGNED, NULL, NULL);
    }

/* Write out detailed records of mismatches, if requested */
if (mismatchReport) 
    {
    verbose(1, "Writing out mismatches\n");
    writeList(mm, pi->mmList, MISMATCH, NULL, NULL);
    }
/* Write out detailed records of codon substitutions, if requested */
if (codonSubReport) 
    {
    verbose(1, "Writing out codon substitutions\n");
    writeList(cs, pi->codonSubList, CODONSUB, pi->psl, pi->mrna);
    }
}
 
void doFile(struct lineFile *pf, FILE *of, FILE *in, FILE *mm, FILE* cs, FILE* un)
/* Process all records in a psl file of mRNA alignments */
{
int lineSize;
char *line;
char *words[32];
int wordCount;
struct psl *psl;
struct pslInfo *pi;
struct sqlConnection *conn = hAllocConn();

while (lineFileNext(pf, &line, &lineSize))
    {
    wordCount = chopTabs(line, words);
    if (wordCount != 21)
	errAbort("Bad line %d of %s\n", pf->lineIx, pf->fileName);
    psl = pslLoad(words);
    pi = processPsl(conn, psl);
    slAddHead(&piList, pi);
    writeOut(of, in, mm, cs, un, pi);
    pslInfoFree(&pi);
    }
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{
struct lineFile *pf, *cf, *lf, *vf=NULL, *df=NULL;
FILE *of, *in=NULL, *mm=NULL, *cs=NULL, *un=NULL;
char *faFile, *db, filename[PATH_LEN], *vfName = NULL, *dfName = NULL;
char *user, *password;
int verb = 0;

/* try read-only first */
user = cfgOption("ro.user");
if (user == NULL) 
    user = cfgOption("db.user");
password = cfgOption("ro.password");
if (password == NULL)
    password = cfgOption("db.password");

verboseSetLevel(0);
optionInit(&argc, argv, optionSpecs);
if (argc != 6)
    {
    fprintf(stderr, "USAGE: pslAnal [-db=db -ver=<mrna versions> -noVersions -der=<refseq derived accs> -verbose=<level> -xeno -indels -unaligned -mismatches -codonsub] <psl file> <cds file> <loci file> <fa file> <out file prefix>\n");
    return 1;
    }
db = optionVal("db", "hg15");
vfName = optionVal("ver", NULL);
dfName = optionVal("der", NULL);
verb = optionInt("verbose", 0);
verboseSetLevel(verb);
indelReport = optionExists("indels");
unaliReport = optionExists("unaligned");
mismatchReport = optionExists("mismatches");
codonSubReport = optionExists("codonsub");
xeno = optionExists("xeno");
noVersions = optionExists("noVersions");
pf = pslFileOpen(argv[1]);
cf = lineFileOpen(argv[2], FALSE);
lf = lineFileOpen(argv[3], FALSE);
faFile = argv[4];
safef(filename, sizeof(filename), "%s.anal", argv[5]);
of = mustOpen(filename, "w");
fprintf(of, "Acc\tChr\tStart\tEnd\tmStart\tmEnd\tSize\tLoci\tCov\tID\tCdsStart\tCdsEnd\tCdsCov\tCdsID\tCdsMatch\tCdsMismatch\tSnp\tThirdPos\tSyn\tNonSyn\tSynSnp\tNonSynSnp\tIntrons\tStdSplice\tUnCds\tSingle\tTriple\tTotal\tIndels\tGaps\n");
fprintf(of, "10\t10\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10\t10N\n");
if (vfName) 
  vf = lineFileOpen(vfName, FALSE);
if (dfName) 
  df = lineFileOpen(dfName, FALSE);
if (indelReport) 
    {
    safef(filename, sizeof(filename), "%s.indel", argv[5]);
    in = mustOpen(filename, "w");
    }
if (unaliReport) 
    {
    safef(filename, sizeof(filename), "%s.unali", argv[5]);
    un = mustOpen(filename, "w");
    }
if (mismatchReport)
    {
    safef(filename, sizeof(filename), "%s.mismatch", argv[5]);
    mm = mustOpen(filename, "w");
    }

if (codonSubReport)
    {
    safef(filename, sizeof(filename), "%s.codonsub", argv[5]);
    cs = mustOpen(filename, "w");
    }

hSetDb(db);
 if (getenv("HGDB_HOST") == NULL)
     hSetDbConnect("hgwdev.cse.ucsc.edu", db, user, password);
verbose(1, "Reading CDS file\n");
readCds(cf);
verbose(1, "Reading FA file\n");
readRnaSeq(faFile);
verbose(1, "Reading loci file\n");
readLoci(lf);
if (vf) 
    {
    verbose(1, "Reading version file\n");
    readVersion(vf);
    }
if (df) 
    {
    verbose(1, "Reading refseq derived accessions file\n");
    readRsDerived(df);
    }
verbose(1, "Processing psl file\n");
doFile(pf, of, in, mm, cs, un);

if (indelReport)
    fclose(in);
if (unaliReport)
    fclose(un);
if (mismatchReport)
    fclose(mm);
if (codonSubReport)
    fclose(cs);
fclose(of);
lineFileClose(&pf);

return 0;
}
