/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ggGraph - Third pass of routine that generates a representation
 * of possible splicing patterns of a gene as a graph from mrna
 * clusters. */

#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "ggPrivate.h"
#include "hdb.h"

static char const rcsid[] = "$Id: ggGraph.c,v 1.15 2005/01/27 02:21:06 sugnet Exp $";

static int maxEvidence = 500;

enum{ softEndBleedLimit = 5};  /* For soft ends allow some bleeding into next intron */

int ggEvidenceIx(struct ggEvidence *hayStack, struct ggEvidence *needle)
/* return index of needle in haystack or -1 if not present */
{
int count = 0;
struct ggEvidence *ge = NULL;
for(ge = hayStack; ge != NULL; ge = ge->next)
    {
    if(needle->id == ge->id)
	return count;
    count++;
    }
return -1;
}

void ggEvAddHeadWrapper(struct ggEvidence **list, struct ggEvidence **el)
/* Wrapper to avoid getting more than 50 pieces of evidence on an
   edge. 50 is enough that I believe it.*/
{
int count = slCount(*list);
if(count < maxEvidence)
    {
    slAddHead(list, *el);
    }
else
    ggEvidenceFree(el);
}

void moveEvidenceIfUnique(struct ggEvidence **currList, struct ggEvidence **newList)
/* add the new evidence to the old if not currently in in */
{
struct ggEvidence *ge = NULL, *geNext = NULL;
for(ge = *newList; ge != NULL; ge = geNext)
    {
    int index = -1;
    geNext = ge->next;
    index = ggEvidenceIx(*currList, ge);
    if(index == -1)
	ggEvAddHeadWrapper(currList, &ge);
    else
	ggEvidenceFree(&ge);
    }
*newList = NULL;
}

static int vertexIx(struct ggVertex *array, int arraySize, int pos, int type)
/* Return index of vertex in array, or -1 if not in array. */
{
int i;

/* Check for a hard vertex first. */
if(type == ggSoftStart || type == ggSoftEnd)
    {
    for (i=0; i<arraySize; ++i)
	{
	/* if position is right and can sub a soft vertex for a hard one
	   do so. */
	if(array[i].position == pos && 
	   ((array[i].type == ggHardStart && type == ggSoftStart) ||
	    (array[i].type == ggHardEnd && type == ggSoftEnd)))
	    return i;
	}
    }
for(i=0; i<arraySize; ++i)
    {
    if(array[i].position == pos && array[i].type == type)
	return i;
    }
return -1;
}

struct ggEvidence * ggEvidenceClone(struct ggEvidence *geList)
/* clone a list of evidence */
{
struct ggEvidence *retList = NULL, *ret=NULL;
struct ggEvidence *ge = NULL;
for(ge = geList; ge != NULL; ge = ge->next)
    {
    AllocVar(ret);
    ret->id = ge->id;
    ggEvAddHeadWrapper(&retList, &ret);
    }
slReverse(&retList);
return retList;
}

int findMrnaIdByName(char *name, char **mrnaRefs, int count)
/* find the index of the name in the mrnaRef array, return -1 if not found */
{
int i;
for(i = 0; i < count ; ++i)
    {
    if(sameString(name, mrnaRefs[i]))
	return i;
    }
return -1;
}

static struct geneGraph *makeInitialGraph(struct ggMrnaCluster *mc, struct ggMrnaInput *ci)
/* Create initial gene graph from input input. */
{
int vAlloc = 512;
int i;
struct ggVertex *vAll;
int vAllCount = 0;
struct ggAliInfo *da;
struct geneGraph *gg = NULL;
bool **em;
int totalWithDupes = 0;
struct maRef *ref;
char **mrnaRefs = NULL;
int mrnaRefCount;
struct ggEvidence ***evM = NULL;
AllocArray(vAll, vAlloc);

/* First fill up array with unique vertices. */
for (da = mc->mrnaList; da != NULL; da = da->next)
    {
    int countOne = da->vertexCount;
    struct ggVertex *vOne = da->vertices;
    int vix;
    totalWithDupes += countOne;
    for (i=0; i<countOne; ++i)
	{
	vix = vertexIx(vAll, vAllCount, vOne->position, vOne->type);
	/* if vOne not in vALL vix == -1 and we need to insert it */
	if (vix < 0)
	    {
	    if (vAllCount >= vAlloc)
		{
		vAlloc <<= 1;
		ExpandArray(vAll, vAllCount, vAlloc);
		}
	    vAll[vAllCount++] = *vOne;
	    }
	++vOne;
	}
    }

AllocVar(gg);
/* Fill in other info from ci and mc. */
snprintf(gg->strand, sizeof(gg->strand), "%s", mc->strand);
gg->tName = cloneString(ci->tName);
gg->tStart = mc->tStart;
gg->tEnd = mc->tEnd;
gg->mrnaRefCount = mrnaRefCount = slCount(mc->refList);
gg->mrnaRefs = needMem(sizeof(char *) * mrnaRefCount);
for (ref = mc->refList, i=0; ref != NULL; ref = ref->next, ++i)
     gg->mrnaRefs[i] = cloneString(ref->ma->qName);

/* Allocate gene graph and edge matrix. Also the evidence matrix */
gg->vertexCount = vAllCount;
AllocArray(gg->vertices, vAllCount);
memcpy(gg->vertices, vAll, vAllCount*sizeof(*vAll));
gg->edgeMatrix = AllocArray(em, vAllCount);
gg->evidence = AllocArray(evM, vAllCount);
for (i=0; i<vAllCount; ++i)
    {
    AllocArray(em[i], vAllCount);
    AllocArray(evM[i], vAllCount);
    }

/* Fill in edge matrix. */
/* for each alignment in cluster create edges from each exon to the next,
 *  as we have seen them in mRNAs we know that they are valid splicings. 
 *  Note that the id's stored in the evidences index into the mrnaRefs array. 
 */
for (da = mc->mrnaList; da != NULL; da = da->next)
    {
    int countOne = da->vertexCount;
    struct ggVertex *vOne = da->vertices;
    int vix, lastVix;
    struct ggEvidence *ev = NULL;
    vix = vertexIx(vAll, vAllCount, vOne->position, vOne->type);
    for (i=1; i<countOne; ++i)
	{
	AllocVar(ev);
	++vOne;
	lastVix = vix;
	vix = vertexIx(vAll, vAllCount, vOne->position, vOne->type);
	assert(vix >= 0 && lastVix >= 0);
	em[lastVix][vix] = TRUE;
	ev->id = findMrnaIdByName(da->ma->qName, gg->mrnaRefs, gg->mrnaRefCount);
	if(ev->id < 0)
	    errAbort("ggGraph::makeInitialGraph() - Couldn't find %s in mrnalist.", da->ma->tName);
	ggEvAddHeadWrapper(&evM[lastVix][vix], &ev); 
	}
    }
freeMem(vAll);

return gg;
}

void printEdges(struct geneGraph *gg, int position)
/* Print verices with position. */
{
int i;
for(i = 0; i < gg->vertexCount; i++)
    if(gg->vertices[i].position == position)
	fprintf(stderr, "%d\t%d\t%d\n", i, gg->vertices[i].position, gg->vertices[i].type);
}

static int findFurthestLinkedStart(struct geneGraph *gg, int endIx)
/* Find index furthest connected start vertex. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
int startIx;
int startPos;
int bestStartIx = 0;
int bestStartPos = 0x3fffffff;


for (startIx=0; startIx<vCount; ++startIx)
    {
    if (em[startIx][endIx])
	{
	startPos = vertices[startIx].position;
	if (startPos < bestStartPos)
	    {
	    bestStartPos = startPos;
	    bestStartIx = startIx;
	    }
	}
    }
return bestStartIx;
}

static boolean connectToAnything(struct geneGraph *gg, int vertIx)
/* Return TRUE if vertIx connects to anything. */
{
int i;
for(i = 0; i < gg->vertexCount; i++)
    if(gg->edgeMatrix[vertIx][i] == TRUE)
	return TRUE;
return FALSE;
}

static boolean anythingConnectsTo(struct geneGraph *gg, int vertIx)
/* Return TRUE if anything connects to vertIx. */
{
int i;
for(i = 0; i < gg->vertexCount; i++)
    if(gg->edgeMatrix[i][vertIx] == TRUE)
	return TRUE;
return FALSE;
}

static boolean softlyTrimStart(struct geneGraph *gg, int softIx)
/* Try and trim one soft start. Return TRUE if did trim. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
bool *edgesOut = em[softIx];
int endIx;
boolean anyTrim = FALSE;
boolean anyLeft = FALSE;
struct ggEvidence ***evid = gg->evidence;
for (endIx=0; endIx < vCount; ++endIx)
    {
    if (edgesOut[endIx])
	{
	int bestStart = findFurthestLinkedStart(gg, endIx);
	if (bestStart != softIx)
	    {
	    anyTrim = TRUE;
	    edgesOut[endIx] = FALSE;
	    moveEvidenceIfUnique(&gg->evidence[bestStart][endIx], &gg->evidence[softIx][endIx]);
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft && !anythingConnectsTo(gg, softIx))
    vertices[softIx].type = ggUnused;
return anyTrim;
}


static int findFurthestLinkedEnd(struct geneGraph *gg, int startIx)
/* Find index furthest connected end vertex. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
bool *edgesOut = em[startIx];
int endIx;
int endPos;
int bestEndIx = 0;
int bestEndPos = -0x3fffffff;


for (endIx=0; endIx<vCount; ++endIx)
    {
    if (edgesOut[endIx])
	{
	endPos = vertices[endIx].position;
	if (endPos > bestEndPos)
	    {
	    bestEndPos = endPos;
	    bestEndIx = endIx;
	    }
	}
    }
return bestEndIx;
}

static boolean softlyTrimEnd(struct geneGraph *gg, int softIx)
/* Try and trim one soft start. Return TRUE if did trim. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
int startIx;
boolean anyTrim = FALSE;
boolean anyLeft = FALSE;
struct ggEvidence ***evid = gg->evidence;
for (startIx=0; startIx < vCount; ++startIx)
    {
    if (em[startIx][softIx])
	{
	int bestEnd = findFurthestLinkedEnd(gg, startIx);
	if (bestEnd != softIx)
	    {
	    anyTrim = TRUE;
	    em[startIx][softIx] = FALSE; 
            // update evidence
	    moveEvidenceIfUnique(&gg->evidence[startIx][bestEnd], &gg->evidence[startIx][softIx]);
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft && !connectToAnything(gg, softIx))
    vertices[softIx].type = ggUnused;
return anyTrim;
}


static boolean softlyTrim(struct geneGraph *gg)
/* Trim all redundant soft starts and ends.  Return TRUE if trimmed 
 * any. */
{
int i;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
boolean result = FALSE;


for (i=0; i<vCount; ++i)
    {
    switch (vertices[i].type)
	{
	case ggSoftStart:
	    result |= softlyTrimStart(gg, i);
	    break;
	case ggSoftEnd:
	    result |= softlyTrimEnd(gg, i);
	    break;
	}
    }
return result;
}


static boolean arcsForSoftStartedExon(struct geneGraph *gg, int softStartIx, int endIx)
/* Put in arcs between hard start, and hard ends of any compatible
 * exons overlapping this one */
{
boolean result = FALSE;
int vCount = gg->vertexCount;
int sIx, eIx;
bool **em = gg->edgeMatrix;
struct ggVertex *vertices = gg->vertices;
int softStartPos = vertices[softStartIx].position;
int hardEndPos = vertices[endIx].position;
struct ggEvidence  ***evid = gg->evidence;
/* for each vertice */
for (eIx = 0; eIx < vCount; ++eIx)
    {
    struct ggVertex *ev = vertices+eIx;
    int type = ev->type;
    /* if the type is an end */
    if (type == ggSoftEnd || type == ggHardEnd)  
	{
	int evpos = ev->position;
	int softBleedAdjust = (type == ggHardEnd ? softEndBleedLimit : 0);
	/* if the position of the vertice is > the softStartPosition, assumes coordinates on + strand? */
	if (evpos > softStartPos)
            /* Why did jim comment this out? looks like now if there is a connection to any downstream exon we create the edge */
	    // if (evpos <= hardEndPos && evpos > softStartPos) 
	    {
	    for (sIx = 0; sIx < vCount; ++sIx)
		{
		if (sIx != softStartIx && em[sIx][eIx]) 
		    {
		    struct ggVertex *sv = vertices+sIx;
		    if (sv->position <= softStartPos + softBleedAdjust)
			{
			struct ggEvidence *ge = NULL;
			ge = ggEvidenceClone(gg->evidence[softStartIx][endIx]);
			em[sIx][endIx] = TRUE; 
			moveEvidenceIfUnique(&gg->evidence[sIx][endIx], &ge);
			result = TRUE;  
			}
		    }
		}
	    }
	}
    }
return result;
}

int blocksOverlap(struct ggMrnaBlock *block1, struct ggMrnaBlock *block2)
/** Return TRUE if block1 is inside block2. */
{
/* if(block1->tStart >= block2->tStart && block1->tEnd <= block2->tEnd) */
/*     return TRUE; */
if(rangeIntersection(block1->tStart, block1->tEnd, block2->tStart, block2->tEnd) > 0)
    return TRUE;
return FALSE;
}

boolean ggAliInfoOverlap(struct ggVertex *v, struct ggAliInfo *ai1, struct ggAliInfo *ai2, 
			 int startIx, int endIx, int start2Ix, int end2Ix)
/** Return TRUE if the two alignments have an overlap in the area
 * defined by starts and ends, return FALSE otherwise. */
{
struct ggMrnaBlock *blocks1 = NULL, *blocks2 = NULL;
int i=0,j=0;
blocks1 = ai1->ma->blocks; 
blocks2 = ai2->ma->blocks;
for(i=0; i<ai1->ma->blockCount; i++)
    {
    /* Find the matching block in the other mRNA. */
    for(j=0; j<ai2->ma->blockCount; j++)
	{
	if(blocksOverlap(&blocks1[i], &blocks2[j]) > 0)
	    {
	    return TRUE;
	    }
	}
    }
return FALSE;
}

boolean overlappingMRnas(struct geneGraph *gg, struct hash *aliHash, 
			 int startIx, int endIx, int start2Ix, int end2Ix)
/** Check to see if the mRNAs contained in the edges defined by
    these vertexes overlap. */
{
struct ggEvidence *edge1 = gg->evidence[startIx][endIx];
struct ggEvidence *edge2 = gg->evidence[start2Ix][end2Ix];
struct ggEvidence *ev1 = NULL, *ev2 = NULL;
struct ggAliInfo *ai1 = NULL;
struct ggAliInfo *ai2 = NULL;
char buff[256];
boolean overlap = FALSE;
for(ev1 = edge1; ev1 != NULL; ev1 = ev1->next)
    {
    safef(buff, sizeof(buff), "%s", gg->mrnaRefs[ev1->id]);
    ai1 = hashMustFindVal(aliHash, buff);
    ai1 = ai1;
    for(ev2 = edge2; ev2 != NULL; ev2 = ev2->next)
	{
	safef(buff, sizeof(buff), "%s", gg->mrnaRefs[ev2->id]);
	ai2 = hashMustFindVal(aliHash, buff);
	if(ggAliInfoOverlap(gg->vertices, ai1, ai2, startIx, endIx, start2Ix, end2Ix ))
	    return TRUE;
	}
    }
return overlap;
}


void getStartEdgesOverlapping(struct geneGraph *gg, struct hash *mRnaAli, int softStartIx, 
				       int endIx, enum ggVertexType startType, int **oIndex, int *oCount)
/** Loop through all the edges looking for ones that overlap
    em[softStartIx][endIx], start with hard edge, and have an mRNA that
    actually overlaps with the softStartix edge. */
{
bool **em = gg->edgeMatrix;
struct ggVertex *v = gg->vertices;
int i=0;
*oIndex =  needMem(sizeof(int)*gg->vertexCount);
for(i=0; i<gg->vertexCount; i++)
    {
    if(em[i][endIx] && v[i].type == startType) 
	{
	if(overlappingMRnas(gg, mRnaAli, softStartIx, endIx, i, endIx))
	    {
	    (*oIndex)[*oCount] = i;
	    (*oCount)++;
	    }
	}
    }
}

void getEndEdgesOverlapping(struct geneGraph *gg, struct hash *mRnaAli, int hardStartIx, 
				       int softEndIx, enum ggVertexType endType, int **oIndex, int *oCount)
/** Loop through all the edges looking for ones that overlap
    em[hardStartIx][softEndIx], end with hard vertex, and have an mRNA that
    actually overlaps with the softEndIx vertex. */
{
bool **em = gg->edgeMatrix;
struct ggVertex *v = gg->vertices;
int i=0;
*oIndex =  needMem(sizeof(int)*gg->vertexCount);
for(i=0; i<gg->vertexCount; i++)
    {
    if(em[hardStartIx][i] && v[i].type == endType) 
	{
	if(overlappingMRnas(gg, mRnaAli, hardStartIx, softEndIx, hardStartIx, i))
	    {
	    (*oIndex)[*oCount] = i;
	    (*oCount)++;
	    }
	}
    }
}

struct hash *newAlignmentHash(struct ggMrnaCluster *mc)
/** Hash the ggAliInfo's by the qName. */
{
struct hash *hash = newHash(4);
struct ggAliInfo *ai = NULL;
char buff[256];
for(ai=mc->mrnaList; ai != NULL; ai = ai->next)
    {
    safef(buff, sizeof(buff), "%s", ai->ma->qName);
    hashAdd(hash, buff, ai);
    }
return hash;
}

int consensusSoftStartVertex(struct geneGraph *gg, int softStartIx, int hardEndIx, 
			     int *oIndex, int oCount, boolean allowSmaller)
/** Pick the best soft start out of oIndex. Return index of best start
    into gg->vertices, -1 if no vertex found. Allow smaller exon only if allowSmaller == TRUE.
    Best is defined by:
    - Number of other alignments that support it.
    - Distance from softStartIx. */
{
int bestVertex = -1;
struct ggVertex *v = gg->vertices;
struct ggEvidence ***e = gg->evidence;
int longerBetterPrior = 5;
int maxDistance = 3000;
int minDistance = 20;
int i=0; 
int bestCount = -1;
int bestDist = -1;
boolean isHard = FALSE;
for(i=0; i<oCount; i++)
    {
    int curCount = slCount(e[oIndex[i]][hardEndIx]);
    int curDist = v[softStartIx].position - v[oIndex[i]].position;
    if( (curDist >= 0 || allowSmaller) &&                     /* Exon is larger or we're allowing smaller exons. */
	(!isHard || v[oIndex[i]].type  == ggHardStart) &&     /* Replace if not replacing hard with soft. */
	(curCount > bestCount ||                              /* and current count is better. */
	 (((curCount +longerBetterPrior) >= bestCount || curDist - bestDist <= minDistance) && /* end has decent evidence or is close */
	  curDist >= bestDist && curDist <= maxDistance) )) /* or start is farther upstream. */
	{
	if(v[oIndex[i]].type == ggHardStart)
	    isHard = TRUE;
	bestVertex = oIndex[i];
	bestCount = curCount;
	bestDist = curDist;
	}
    }
return bestVertex;
}


int consensusSoftEndVertex(struct geneGraph *gg, int hardStartIx, int softEndIx, 
			     int *oIndex, int oCount, boolean allowSmaller)
/** Pick the best hard end out of oIndex. Return index of best hard end index
    into gg->vertices, -1 if no vertex found. Allow smaller exon only if allowSmaller == TRUE.
    Best is defined by:
    - Number of other alignments that support it.    - Distance from softStartIx. */
{
int bestVertex = -1;
struct ggVertex *v = gg->vertices;
struct ggEvidence ***e = gg->evidence;
int i=0; 
int longerBetterPrior = 5;
int bestCount = -1;
int bestDist = -1;
int maxDist = 3000;
int minDist = 20;
boolean isHard = FALSE;
for(i=0; i<oCount; i++)
    {
    int curCount = slCount(e[hardStartIx][oIndex[i]]);
    int curDist = v[oIndex[i]].position - v[softEndIx].position;
    if( (curDist >= 0 || allowSmaller) &&                     /* Exon is larger or we're allowing smaller exons. */
        (!isHard || v[oIndex[i]].type == ggHardEnd) &&  /* Replace if not replacing hard with soft. */
	(curCount > bestCount ||                        /* and current count is better. */
	 (((curCount + longerBetterPrior )>= bestCount || curDist - bestDist <= minDist) && /* end has decent evidence or is close */
	 curDist >= bestDist && curDist <= maxDist) )) /* and end is farther downstream. */
	{
	if(v[oIndex[i]].type == ggHardEnd)
	    isHard = TRUE;
	bestVertex = oIndex[i];
	bestCount = curCount;
	bestDist = curDist;

	}
    }

return bestVertex;
}


static boolean softStartOverlapConsensusArcs(struct geneGraph *gg, struct hash *aliHash, 
					     int softStartIx, enum ggVertexType startType, boolean allowSmaller)
/** For each edge that contains softStartIx see if there are overlapping
    edges that have a hard start. If there are, find the consensus hard start
    and extend original edge to it. Only allow replacement of a softStart with a shorter
    exon when allowSmaller is TRUE. */
{
struct ggVertex *overlapping = NULL; /* Array of vertexes that could replace softStartIx. */
int *oIndex = NULL;                  /* Index into gg->vertexes array for overlapping. */
int oCount = 0;                      /* Number of items in oIndex and overlapping arrays. */
int bestEdge = -1;
bool **em = gg->edgeMatrix;
int vCount = gg->vertexCount;
int i;
int bestStart = -1;
boolean changed = FALSE;
for (i=0; i<vCount; ++i)
    {
    if (em[softStartIx][i])
	{
	oCount = 0;
	getStartEdgesOverlapping(gg, aliHash, softStartIx, i,  startType, &oIndex, &oCount);
	bestStart = consensusSoftStartVertex(gg, softStartIx, i, oIndex, oCount, allowSmaller);
	if(bestStart >= 0 && bestStart != softStartIx)
	    {
	    struct ggEvidence *ge = NULL;
	    em[softStartIx][i] = FALSE;
	    moveEvidenceIfUnique(&gg->evidence[bestStart][i], &gg->evidence[softStartIx][i]);
	    em[bestStart][i] = TRUE; 
	    changed=TRUE;
	    }
	freez(&oIndex);
	bestStart = -1;
	}
    }
return changed;
}

static boolean softEndOverlapConsensusArcs(struct geneGraph *gg, struct hash *aliHash, 
					   int softEndIx, enum ggVertexType endType, boolean allowSmaller)
/** For each edge that contains softEndIx see if there are overlapping
    edges that have a hard end. If there are, find the consensus hard start
    and extend original edge to it. */
{
struct ggVertex *overlapping = NULL; /* Array of vertexes that could replace softEndIx. */
int *oIndex = NULL;                  /* Index into gg->vertexes array for overlapping. */
int oCount = 0;                      /* Number of items in oIndex and overlapping arrays. */
int bestEdge = -1;
bool **em = gg->edgeMatrix;
int vCount = gg->vertexCount;
int i;
int bestEnd = -1;
boolean changed = FALSE;
for (i=0; i<vCount; ++i)
    {
    if (em[i][softEndIx])
	{
	oCount = 0;
	getEndEdgesOverlapping(gg, aliHash, i, softEndIx, endType, &oIndex, &oCount);
	bestEnd = consensusSoftEndVertex(gg, i, softEndIx, oIndex, oCount, allowSmaller);
	if(bestEnd >= 0 && bestEnd != softEndIx)
	    {
	    struct ggEvidence *ge = NULL;
	    em[i][softEndIx] = FALSE;
	    moveEvidenceIfUnique(&gg->evidence[i][bestEnd], &gg->evidence[i][softEndIx]);
	    em[i][bestEnd] = TRUE; 
	    changed=TRUE;
	    }
	freez(&oIndex);
	bestEnd = -1;
	}
    }
return changed;
}
    
static boolean softStartArcs(struct geneGraph *gg, int softStartIx)
/* Replace a soft start with all hard starts of overlapping,
 * but not conflicting exons. */
{
int myEndIx;
int vCount = gg->vertexCount;
bool **em = gg->edgeMatrix;
boolean result = FALSE;
boolean anyLeft = FALSE;
int i;
struct ggEvidence ***evid = gg->evidence;
for (i=0; i<vCount; ++i)
    {
    if (em[softStartIx][i])
	{
	int res;
	res = arcsForSoftStartedExon(gg, softStartIx, i);
	if (res)
	    {
	    em[softStartIx][i] = FALSE;
	    ggEvidenceFreeList(&gg->evidence[softStartIx][i]);
	    result = TRUE;
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft && !anythingConnectsTo(gg, softStartIx))
    {
    gg->vertices[softStartIx].type = ggUnused;
    }
return result;
}

static boolean arcsForSoftEndedExon(struct geneGraph *gg, int startIx, int softEndIx)
/* Put in arcs between hard start, and hard ends of any compatible
 * exons overlapping this one */
{
boolean result = FALSE;
int vCount = gg->vertexCount;
int sIx, eIx;
bool **em = gg->edgeMatrix;
struct ggVertex *vertices = gg->vertices;
int startPos = vertices[startIx].position;
int softEndPos = vertices[softEndIx].position;
struct ggEvidence ***evid = gg->evidence;
for (sIx = 0; sIx < vCount; ++sIx)
    {
    struct ggVertex *sv = vertices+sIx;
    int type = sv->type;
    if (type == ggSoftStart || type == ggHardStart)
	{
	int svpos = sv->position;
	int softBleedAdjust = (type == ggHardStart ? softEndBleedLimit : 0);
	// if (svpos >= startPos && svpos < softEndPos)
	if (svpos < softEndPos)
	    {
	    for (eIx = 0; eIx < vCount; ++eIx)
		{
		if (eIx != softEndIx && em[sIx][eIx])
		    {
		    struct ggVertex *ev = vertices+eIx;
		    if (ev->position >= softEndPos - softBleedAdjust)
			{
			struct ggEvidence *ge = NULL;
			ge = ggEvidenceClone(gg->evidence[startIx][softEndIx]);
			em[startIx][eIx] = TRUE; 
			moveEvidenceIfUnique(&gg->evidence[startIx][eIx], &ge);
			result = TRUE;
			}
		    }
		}
	    }
	}
    }
return result;
}

static boolean softEndArcs(struct geneGraph *gg, int softEndIx)
/* Replace a soft end with all hard ends of overlapping,
 * but not conflicting gexons. */
{
int myStartIx;
int vCount = gg->vertexCount;
bool **em = gg->edgeMatrix;
boolean result = FALSE;
boolean anyLeft = FALSE;
int i;
struct ggEvidence ***evid = gg->evidence;
for (i=0; i<vCount; ++i)
    {
    if (em[i][softEndIx])
	{
	int res;
	res = arcsForSoftEndedExon(gg, i, softEndIx);
	if (res)
	    {
	    em[i][softEndIx] = FALSE;
	    ggEvidenceFreeList(&gg->evidence[i][softEndIx]);
	    result = TRUE;
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft && !connectToAnything(gg, softEndIx))
    {
    gg->vertices[softEndIx].type = ggUnused;
    }
return result;
}

static boolean arcsForOverlaps(struct geneGraph *gg)
/* Add in arcs for exons with soft ends that overlap other
 * exons. */
{
int i;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
boolean result = FALSE;

for (i=0; i<vCount; ++i)
    {
    switch (vertices[i].type)
	{
	case ggSoftStart:
	    result |= softStartArcs(gg, i);
	    if(!matricesInSync(gg)) 
		errAbort("matrices are out of synch on vertex %d with ggSoftStart"); 	    
	    break;
	case ggSoftEnd:
	    result |= softEndArcs(gg, i);
	    if(!matricesInSync(gg)) 
		errAbort("matrices are out of synch on vertex %d with ggSoftEnd"); 	    
	    break;
	}
    }
return result;
}

static boolean exonEnclosed(struct geneGraph *gg, int startIx, int endIx)
/* See if defined by start to end is completely enclosed by another exon. */
{
int i,j;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
boolean result = FALSE;
int eStart = vertices[startIx].position;
int eEnd = vertices[endIx].position;

for (i=0; i<vCount; ++i)
    {
    struct ggVertex *sv = vertices+i;
    UBYTE type = sv->type;
    if ((type == ggSoftStart || type == ggHardStart) && sv->position <= eStart)
	{
	for (j=0; j<vCount; ++j)
	    {
	    if (em[i][j] && (i != startIx || j != endIx))
		{
		struct ggVertex *dv = vertices+j;
		type = dv->type;
		if (type == ggSoftEnd || type == ggHardEnd && dv->position >= endIx)
		    return TRUE;
		}
	    }
	}
    }
return FALSE;
}

static boolean hideLittleOrphans(struct geneGraph *gg)
/* Get rid of exons which are soft edged on both sides and
 * completely enclosed by another exon. 
 * (The arcsForOverlaps doesn't handle these. */
{
int i,j;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
boolean result = FALSE;
struct ggEvidence ***evid = gg->evidence;
for (i=0; i<vCount; ++i)
    {
    if (vertices[i].type == ggSoftStart)
	{
	bool *waysOut = em[i];
	boolean anyLeft = FALSE;
	for (j=0; j<vCount; ++j)
	    {
	    if (waysOut[j])
		{
		if (vertices[j].type == ggSoftEnd && exonEnclosed(gg, i, j))
		    {
		    waysOut[j] = FALSE;
		    ggEvidenceFreeList(&gg->evidence[i][j]);
		    result = TRUE;
		    }
		else
		    anyLeft = TRUE;
		}
	    }
	if (!anyLeft)
	    {
	    vertices[i].type = ggUnused;
	    }
	}
    }
return result;
}

void ggOutputDebug(struct geneGraph *gg, int offset)
/* convert to altGraphX and output to stdout */
{
struct altGraphX *ag = NULL;
ag =  ggToAltGraphX(gg);
altGraphXoffset(ag, offset);
altGraphXVertPosSort(ag);
altGraphXTabOut(ag, stdout);
altGraphXFree(&ag);
}

boolean softlyTrimConsensus(struct geneGraph *gg, struct hash *aliHash)
/** Extend soft starts and ends to a consensus start or end if it
    exists. */
{
int i;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
boolean result = FALSE;
for (i=0; i<vCount; ++i)
    {
    switch (vertices[i].type)
	{
	case ggSoftStart:
	    result |= softStartOverlapConsensusArcs(gg, aliHash, i, ggSoftStart, TRUE);
	    break;
	case ggSoftEnd:
	    result |= softEndOverlapConsensusArcs(gg, aliHash, i, ggSoftEnd, TRUE);
	    break;
	}
    }
return result;
}

boolean matchAsWellSoftStart(struct geneGraph *gg, struct ggMrnaCluster *mc,
			     int softStart, int hardEnd, int hardStart, int upStreamEnd)
/** Return TRUE if the changing the alignment to gap accross
    hardStart -- upStreamEnd produces as good of an alignment as
    softStart -- hardEnd. */
{
struct dnaSeq *seq = mc->genoSeq;
struct ggVertex *v = gg->vertices;
int i=0;
for(i=0; i <= v[hardStart].position - v[softStart].position; i++)
    {
    if(seq->dna[v[hardStart].position - i] != seq->dna[v[upStreamEnd].position -i])
	return FALSE;
    }
return TRUE;
}


void tryToFixSoftStartAlignment(struct geneGraph *gg, struct ggMrnaCluster *mc, 
				struct hash *aliHash, int softStart, int minOverlap)
/** Try to fix alignments where the EST/mRNA should reach into the next exon
    but it isn't big enough for blat to find it. */
{
int i, j, k;
int vCount = gg->vertexCount;
bool **em = gg->edgeMatrix;
struct ggVertex *v = gg->vertices;
for(i=0; i<vCount; i++)
    {
    /* For each hard end that this softstart connects to. */
    if(em[softStart][i] && v[i].type == ggHardEnd)
	{
	for(j=0; j<vCount; j++) 
	    {
	    /* For each hard start that connects to that hardEnd
	       before our softStart index and is within 5 base pairs. */
	    if(em[j][i] && v[j].type == ggHardStart && 
	       v[j].position - v[softStart].position > 0 &&
	       v[j].position - v[softStart].position <= 5)
		{
		for(k=0; k<vCount; k++)
		    {
		    /* If there is a hard end that connects to the hard start take a look */
		    if(em[k][j] && v[k].type == ggHardEnd)
			{
			/* If we get as good of an alignment by extending to the
			   next exon do it. */
			if(matchAsWellSoftStart(gg, mc, softStart, i, j, k))
			    {
			    em[softStart][i] = FALSE;
			    ggEvidenceFreeList(&gg->evidence[softStart][i]);
			    return;
			    }
			/* Else if we are less than the minimum distance drop the edge. */
			else if(abs(v[softStart].position - v[j].position) < minOverlap)
			    {
			    em[softStart][i] = FALSE;
			    ggEvidenceFreeList(&gg->evidence[softStart][i]);
			    return;
			    }
			}
		    }
		}
	    }
	}
    }
}
		
boolean matchAsWellSoftEnd(struct geneGraph *gg, struct ggMrnaCluster *mc,
			     int hardStart, int softEnd, int hardEnd, int downStreamStart)
/** Return TRUE if the changing the alignment to gap accross
    hardEnd -- downStreamStart produces as good of an alignment as
    hardStart -- softEnd. */
{
struct dnaSeq *seq = mc->genoSeq;
struct ggVertex *v = gg->vertices;
int i=0;
for(i=0; i < v[softEnd].position - v[hardEnd].position; i++)
    {
    if(seq->dna[v[hardEnd].position + i] != seq->dna[v[downStreamStart].position + i])
	return FALSE;
    }
return TRUE;
}


void tryToFixSoftEndAlignment(struct geneGraph *gg, struct ggMrnaCluster *mc, 
				struct hash *aliHash, int softEnd, int minOverlap)
/** Try to fix alignments where the EST/mRNA should reach into the next exon
    but it isn't big enough for blat to find it. If an overlap is less than minOverlap
    it will be removed anyhow. */
{
int i, j, k;
int vCount = gg->vertexCount;
bool **em = gg->edgeMatrix;
struct ggVertex *v = gg->vertices;
for(i=0; i<vCount; i++)
    {
    /* For each hard end that this softstart connects to. */
    if(em[i][softEnd] && v[i].type == ggHardStart)
	{
	for(j=0; j<vCount; j++) 
	    {
	    /* For each hard start that connects to that hardEnd
	       before our softStart index and is within 5 base pairs. */
	    if(em[i][j] && v[j].type == ggHardEnd && 
	       v[softEnd].position - v[j].position > 0 &&
	        v[softEnd].position - v[j].position  <= 5)
		{
		for(k=0; k<vCount; k++)
		    {
		    /* If there is a hard end that connects to the hard start take a look */
		    if(em[j][k] && v[k].type == ggHardStart)
			{
			/* If we get as good of an alignment by extending to the
			   next exon do it. */
			if(matchAsWellSoftEnd(gg, mc, i, softEnd, j, k))
			    {
			    em[i][softEnd] = FALSE;
			    ggEvidenceFreeList(&gg->evidence[i][softEnd]);
			    return;
			    }
			/* Else if we are less than the minimum distance drop the edge. */
			else if( abs(v[softEnd].position - v[j].position) < minOverlap)
			    {
			    em[i][softEnd] = FALSE;
			    ggEvidenceFreeList(&gg->evidence[i][softEnd]);
			    return;
			    }
			}
		    }
		}
	    }
	}
    }
}

void fixLargeMrnaInserts(struct geneGraph *gg)
/* If there is an insert in the mRNA can end up with cases where there
   is a soft end and soft start at the same genomic position. Splice
   these out and connect the two upstream and downstream vertices. */
{
struct ggVertex *v = gg->vertices;
int vStart = -1, vEnd = -1, vUp = -1, vDown = -1;
int vC = gg->vertexCount;
bool **em = gg->edgeMatrix;
for(vStart = 0; vStart < vC; vStart++)
    {
    for(vEnd = 0; vEnd < vC; vEnd++) 
	{
	/* If we have two vertices with same position
	   that connect to eachother try to find outside
	   things that they connect to and splice out self
	   connection. */
	if(v[vStart].position == v[vEnd].position &&
	   em[vStart][vEnd] &&
	   v[vStart].type == ggSoftEnd && v[vEnd].type == ggSoftStart)
	    {
	    /* Got one - Try to find all upstream and downstream connections. */
	    boolean foundOne = FALSE;
	    for(vUp = 0; vUp < vC; vUp++) 
		{
		for(vDown = 0; vDown < vC; vDown++)
		    {
		    if(em[vUp][vStart] && em[vEnd][vDown])
			{
			struct ggEvidence *newList = ggEvidenceClone(gg->evidence[vStart][vEnd]);
			moveEvidenceIfUnique(&gg->evidence[vUp][vDown], &newList);
			em[vUp][vDown] = TRUE;
			em[vUp][vStart] = FALSE;
			em[vEnd][vDown] = FALSE;
			foundOne = TRUE;
			}
		    }
		}
	    if(foundOne == TRUE)
		{
		em[vStart][vEnd] = FALSE;
		ggEvidenceFreeList(&gg->evidence[vStart][vEnd]);
		}
	    }
	}
    }
}
		
struct geneGraph *ggGraphConsensusCluster(struct ggMrnaCluster *mc, struct ggMrnaInput *ci, 
					  struct hash *tissLibHash, boolean fillInEvidence)
/* Make up a gene transcript graph out of the ggMrnaCluster. Only
 extending truncated exons to consensus splice sites. */
{
struct sqlConnection *conn = NULL;
struct geneGraph *gg = makeInitialGraph(mc, ci);
struct hash *aliHash = newAlignmentHash(mc);
int minOverlap = 6; /* Don't believe any soft start/end that is less
		     than 6bp from hard start/end.  Usually end up
		     being alignment oddities.*/
int i;
for(i=0; i<gg->vertexCount; i++)
    {
    if(gg->vertices[i].type == ggSoftStart)
	tryToFixSoftStartAlignment(gg, mc, aliHash, i, minOverlap);
    if(gg->vertices[i].type == ggSoftEnd)
	tryToFixSoftEndAlignment(gg, mc, aliHash, i, minOverlap);
    }

/* Try to fill in soft ends and starts with hard starts and ends. */
for(i=0; i<gg->vertexCount; i++)
    {
    if(gg->vertices[i].type == ggSoftStart)
	softStartOverlapConsensusArcs(gg, aliHash, i, ggHardStart, FALSE);
    else if(gg->vertices[i].type == ggSoftEnd)
	softEndOverlapConsensusArcs(gg, aliHash, i, ggHardEnd, FALSE);
    }
/* Fill in soft ends and starts with the "best" soft end or start. */
softlyTrimConsensus(gg, aliHash);
hideLittleOrphans(gg);
fixLargeMrnaInserts(gg); 
if(fillInEvidence)
    {
    if(tissLibHash == NULL)
	conn = hAllocConn();
    ggFillInTissuesAndLibraries(gg, tissLibHash, conn);
    }
else
    {
    AllocArray(gg->mrnaTissues, gg->mrnaRefCount);
    AllocArray(gg->mrnaLibs, gg->mrnaRefCount);
    }
hFreeConn(&conn);
hashFree(&aliHash);
return gg;
}


struct geneGraph *ggGraphCluster(struct ggMrnaCluster *mc, struct ggMrnaInput *ci)
/* Make up a gene transcript graph out of the ggMrnaCluster. */
{
struct sqlConnection *conn = hAllocConn();
struct geneGraph *gg = makeInitialGraph(mc, ci);
arcsForOverlaps(gg);
softlyTrim(gg);
hideLittleOrphans(gg);
ggFillInTissuesAndLibraries(gg, NULL, conn);
hFreeConn(&conn);
return gg;
}


void printEdgesWithVertex(struct geneGraph *gg, int queryIx)
/* Print all of the edges that contain a given vertex. Useful
   for debugging. */
{
int i = 0;
int vC = gg->vertexCount;
bool **em = gg->edgeMatrix;
struct ggVertex *query = &gg->vertices[queryIx];
for(i = 0; i < vC; i++) 
    {
    struct ggVertex *v = &gg->vertices[i];
    if(em[i][queryIx])
	printf("%d-%d %d-%d %d-%d\n", i, queryIx, v->position, query->position, v->type, query->type);
    if(em[queryIx][i])
	printf("%d-%d %d-%d %d-%d\n", queryIx, i, query->position, v->position, query->type, v->type);
    }
}
