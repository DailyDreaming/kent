/* index - blatz module that builds indexes using spaced seeds. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "localmem.h"
#include "blatz.h"
#include "spacedSeed.h"

int blatzIndexKey(DNA *dna, int *seedOffsets, int seedWeight)
/* Calculate which slot in index to look into.  Returns -1 if
 * dna contains lower case or N or other junk. */
{
int i;
int key=0;
for (i=0; i<seedWeight; ++i)
    {
    int nt = ntValUpper[dna[seedOffsets[i]]];
    if (nt < 0)
        return -1;
    key <<= 2;
    key += nt;
    }
return key;
}

void blatzIndexFree(struct blatzIndex **pIndex)
/* Free up memory associated with index. */
{
struct blatzIndex *index = *pIndex;
if (index != NULL)
    {
    freeMem(index->slots);
    freeMem(index->seedOffsets);
    freeMem(index->posBuf);
    freez(pIndex);
    }
}

struct seqPos 
/* A reference to a position within a sequence.  Part of a non-compacted index. */
    {
    struct seqPos *next;        /* Next in list. */
    int pos;                        /* Position in sequence. */
    };


struct blatzIndex *blatzIndexOne(struct dnaSeq *seq, int weight)
/* Create a new index of given seed weight populated by seq. */
{
struct lm *lm = lmInit(0);
int seedSpan = spacedSeedSpan(weight);
int *seedOffsets = spacedSeedOffsets(weight);
int slotCount = (1<<(2*weight));
struct seqPos **slots, *pos;
DNA *dna = seq->dna;
int lastBase = seq->size - seedSpan;
int i;
struct blatzIndex *index;
size_t posCount = 0;
bits32 *posBuf;
struct blatzIndexPos *compactSlots;

/* Build up uncompacted list of target positions in slots. */
lmAllocArray(lm, slots, slotCount);
for (i=0; i<=lastBase; ++i)
    {
    int key = blatzIndexKey(dna + i, seedOffsets, weight);
    if (key >= 0)
        {
        struct seqPos *pos, **slot;
        lmAllocVar(lm, pos);
        pos->pos = i;
        slot = &slots[key];
        slAddHead(slot, pos);
        ++posCount;
        }
    }

/* Allocate index structure and fill in basics. */
AllocVar(index);
compactSlots = AllocArray(index->slots, slotCount);
index->seedWeight = weight;
index->seedSpan = seedSpan;
index->seedOffsets = seedOffsets;
index->target = seq;
if (posCount > 0)
    {
    index->posBuf = posBuf = needHugeMem(posCount * sizeof(posBuf[0]));

    /* Copy over and compact the slots. */
    for (i=0; i<slotCount; ++i)
        {
        int slotSize = 0;
        struct seqPos *pos;
        compactSlots->pos = posBuf;
        for (pos = slots[i]; pos != NULL; pos = pos->next)
            {
            posBuf[slotSize] = pos->pos;
            ++slotSize;
            }
        compactSlots->count = slotSize;
        posBuf += slotSize;
        ++compactSlots;
        }
    }

lmCleanup(&lm);
return index;
}

struct blatzIndex *blatzIndexAll(struct dnaSeq *seqList, int seedWeight)
/* Return a list of indexes, one for each seq on seqList */
{
struct blatzIndex *indexList = NULL, *index;
struct dnaSeq *seq;
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    index = blatzIndexOne(seq, seedWeight);
    slAddHead(&indexList, index);
    }
slReverse(&indexList);
return indexList;
}
