/* clusterGenes - Cluster genes from genePred tracks. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "verbose.h"
#include "dlist.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"
#include "binRange.h"
#include "bits.h"
#include "hdb.h"

static char const rcsid[] = "$Id: clusterGenes.c,v 1.27 2005/07/04 18:44:44 markd Exp $";

/* Command line driven variables. */
char *clChrom = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterGenes - Cluster genes from genePred tracks\n"
  "usage:\n"
  "   clusterGenes [options] outputFile database table1 ... tableN\n"
  "   clusterGenes [options] -trackNames outputFile database track1 table1 ... trackN tableN\n"
  "\n"
  "Where outputFile is a tab-separated file describing the clustering,\n"
  "database is a genome database such as mm4 or hg16,\n"
  "and the table parameters are either tables in genePred format in that\n"
  "database or genePred tab seperated files.\n"
  "options:\n"
  "   -verbose=N - Print copious debugging info. 0 for none, 3 for loads\n"
  "   -chrom=chrN - Just work this chromosome, maybe repeated.\n"
  "   -cds - cluster only on CDS exons\n"
  "   -trackNames - If specified, input are pairs of track names and files.\n"
  "    This is useful when the file names don't reflact the desired track\n"
  "    names.\n"
  "   -clusterBed=bed - output BED file for each cluster\n"
  "   -flatBed=bed - output BED file that contains the exons of all genes\n"
  "    flattned into a single record.\n"
  "   -joinContained - join genes that are contained within a larger loci\n"
  "    into that loci. Intended as a way to handled fragments and exon-level\n"
  "    predictsions, as genes-in-introns on the same strand are very rare.\n"
  "\n"
  "The cdsConflicts and exonConflicts columns contains `y' if the cluster has\n"
  "conficts. A conflict is a cluster where all of the genes don't share exons. \n"
  "Conflicts maybe either internal to a table or between tables.\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING|OPTION_MULTI},
   {"cds", OPTION_BOOLEAN},
   {"trackNames", OPTION_BOOLEAN},
   {"clusterBed", OPTION_STRING},
   {"flatBed", OPTION_STRING},
   {"cds", OPTION_BOOLEAN},
   {"joinContained", OPTION_BOOLEAN},
   {NULL, 0},
};

/* from command line  */
boolean gUseCds;
boolean gTrackNames;
struct track *gTracks = NULL;  /* all tracks */
boolean gJoinContained = FALSE;

struct track
/*  Object representing a track. */
{
    struct track *next;
    char *name;            /* name to use */
    char *table;           /* table or file */
    boolean isDb;          /* is this a database table or file? */
};

struct track* trackNew(char* name,
                       char *table)
/* create a new track, adding it to the global list, if name is NULL,
 * name is derived from table. */
{
struct track* track;
AllocVar(track);

/* determin if table or file */
if (fileExists(table))
    {
    track->isDb = FALSE;
    /* can't read pipes, due to read per chromsome and per strand */
    if (sameString(table, "stdin") || sameString(table, "/dev/stdin"))
        errAbort("can't read track from stdin");
    }
else if (hTableExists(table))
    track->isDb = TRUE;
else
    errAbort("table %s.%s or file %s doesn't exist", hGetDb(), table, table);

/* either default or save name */
if (name == NULL)
    {
    if (!track->isDb)
        {
        /* will load from file, strip directories and trailing extensions */
        char trackName[256];
        splitPath(table, NULL, trackName, NULL);
        if (endsWith(table, ".gz"))
            {
            char *ext2 = strrchr(trackName, '.');
            if (ext2 != NULL)
                *ext2 = '\0';
            }
        track->name = cloneString(trackName);
        }
    else
        {
        /* will load from db table */
        track->name = cloneString(table);
        }
    }
else
    track->name = cloneString(name);
track->table = cloneString(table);
return track;
}

boolean gpGetExon(struct genePred* gp, int exonIx, boolean cdsOnly, 
                  int *exonStartRet, int *exonEndRet)
/* Get the start and end of an exon, adjusting if we are only examining CDS.
 * Return false if exon should not be used.  */
{
int exonStart = gp->exonStarts[exonIx];
int exonEnd = gp->exonEnds[exonIx];
if (cdsOnly)
    {
    if (exonStart < gp->cdsStart)
        exonStart = gp->cdsStart;
    if (exonEnd > gp->cdsEnd)
        exonEnd = gp->cdsEnd;
    }
*exonStartRet = exonStart;
*exonEndRet = exonEnd;
return exonStart < exonEnd;
}

struct clusterGene
/* A gene in a cluster */
{
    struct clusterGene* next;
    struct track* track;         /* aka table for gene */
    struct genePred* gp;
    struct slRef *exonConflicts; /* list of genes in cluster that don't share
                                  * exons with this gene */
    struct slRef *cdsConflicts; /* list of genes in cluster that don't share
                                 * CDS with this gene */
};

struct clusterGene* clusterGeneNew(struct track* track, struct genePred* gp)
/* create a new clusterGene. */
{
struct clusterGene* cg;
AllocVar(cg);
cg->track = track;
cg->gp = gp;
return cg;
}

void clusterGeneFree(struct clusterGene** cgp)
/* Free a clusterGene. */
{
struct clusterGene* cg = *cgp;
if (cg != NULL)
    {
    slFreeList(&cg->exonConflicts);
    slFreeList(&cg->cdsConflicts);
    freez(cgp);
    }
}

int clusterGeneRefCmp(const void *clr1, const void *clr2)
/* compare two slRef objects reference clusterGene objects */
{
struct clusterGene *cg1 = (*((struct slRef**)clr1))->val;
struct clusterGene *cg2 = (*((struct slRef**)clr2))->val;
int cmp = strcmp(cg1->track->name, cg2->track->name);
if (cmp == 0)
    strcmp(cg1->gp->name, cg2->gp->name);
return cmp;
}

struct cluster
/* A cluster of overlapping genes. */
    {
    struct cluster *next;	/* Next in list. */
    int id;                     /* id assigned to cluster */
    struct clusterGene *genes;  /* Associated genes. */
    char *chrom;                /* chrom, memory not owned */
    int start,end;		/* Range covered by cluster. */
    boolean hasExonConflicts;   /* does this cluster have conflicts? */
    boolean hasCdsConflicts;
    };

boolean clusterHaveTrack(struct cluster *cluster,
                         struct track *track)
/* check if the cluster has a track */
{
struct clusterGene *gene;
for (gene = cluster->genes; gene != NULL; gene = gene->next)
    if (gene->track == track)
        return TRUE;
return FALSE;
}

void clusterDump(struct cluster *cluster)
/* Dump contents of cluster to stderr. */
{
struct clusterGene *gene;
fprintf(stderr, "%d-%d", cluster->start, cluster->end);
for (gene = cluster->genes; gene != NULL; gene = gene->next)
    fprintf(stderr, " %s", gene->gp->name);
fprintf(stderr, "\n");
}

struct cluster *clusterNew()
/* Create new cluster. */
{
struct cluster *cluster;
AllocVar(cluster);
return cluster;
}

void clusterFree(struct cluster **pCluster)
/* Free up a cluster. */
{
struct cluster *cluster = *pCluster;
if (cluster != NULL)
    {
    while (cluster->genes != NULL)
        {
        struct clusterGene *cg = cluster->genes;
        cluster->genes = cluster->genes->next;
        clusterGeneFree(&cg);
        }
    freez(pCluster);
    }
}

void clusterFreeList(struct cluster **pList)
/* Free a list of dynamically allocated cluster's */
{
struct cluster *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    clusterFree(&el);
    }
*pList = NULL;
}

int clusterCmp(const void *va, const void *vb)
/* Compare to sort based on start and end. assumes they are on the chrom */
{
const struct cluster *a = *((struct cluster **)va);
const struct cluster *b = *((struct cluster **)vb);
int dif = a->start - b->start;
if (dif == 0)
    dif = a->end - b->end;
return dif;
}

struct clusterGene *clusterFindGene(struct cluster *cluster, struct track *track, struct genePred *gp)
/* search for a gene in a cluster.  */
{
struct clusterGene *cg;

for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    if ((cg->gp == gp) && (cg->track == track))
        return cg;
    }
return NULL;
}

void clusterAddExon(struct cluster *cluster,
	int start, int end, struct track *track, struct genePred *gp)
/* Add exon to cluster. */
{
struct clusterGene *cg = clusterFindGene(cluster, track, gp);
if (cg == NULL)
    {
    cg = clusterGeneNew(track, gp);
    slSafeAddHead(&cluster->genes, cg);
    }
if (cluster->start == cluster->end)
    {
    cluster->chrom = gp->chrom;
    cluster->start = start;
    cluster->end = end;
    }
else
    {
    if (start < cluster->start) cluster->start = start;
    if (cluster->end < end) cluster->end = end;
    }
}

void addExon(struct binKeeper *bk, struct dlNode *clusterNode,
	int start, int end, struct track *track, struct genePred *gp)
/* Add exon to cluster and binKeeper. */
{
clusterAddExon(clusterNode->val, start, end, track, gp);
binKeeperAdd(bk, start, end, clusterNode);
}

void mergeClusters(struct binKeeper *bk, struct binElement *bkRest,
	struct dlNode *aNode, struct dlNode *bNode)
/* Move bNode into aNode. */
{
struct cluster *aCluster = aNode->val;
struct cluster *bCluster = bNode->val;
struct binElement *bkEl;

if (verboseLevel() >= 3) 
    {
    fprintf(stderr, " a: ");
    clusterDump(aCluster);
    fprintf(stderr, " b: ");
    clusterDump(bCluster);
    }

/* First change references to bNode. */
binKeeperReplaceVal(bk, bCluster->start, bCluster->end, bNode, aNode);
for (bkEl = bkRest; bkEl != NULL; bkEl = bkEl->next)
    if (bkEl->val == bNode) 
        bkEl->val = aNode;

/* Add b's genes to a. */
while (bCluster->genes != NULL)
    {
    struct clusterGene *cg = bCluster->genes;
    bCluster->genes = bCluster->genes->next;
    cg->next = NULL;
    slSafeAddHead(&aCluster->genes, cg);
    }

/* Adjust start/end. */
if (bCluster->start < aCluster->start) 
    aCluster->start = bCluster->start;
if (aCluster->end < bCluster->end)
    aCluster->end = bCluster->end;

/* Remove all traces of bNode. */
dlDelete(&bNode);
clusterFree(&bCluster);
if (verboseLevel() >= 3) 
    {
    fprintf(stderr, " ab: ");
    clusterDump(aCluster);
    }
}

boolean shareExons(struct genePred *gp1, struct genePred *gp2, boolean cdsOnly)
/* determine if two genes share exons or CDS exons */
{
int exonIx1, exonStart1, exonEnd1;
int exonIx2, exonStart2, exonEnd2;

for (exonIx1 = 0; exonIx1 < gp1->exonCount; exonIx1++)
    {
    if (gpGetExon(gp1, exonIx1, cdsOnly, &exonStart1, &exonEnd1))
        {
        /* exonStart2 >= exon1End indicates there can't be overlap on this
         * exon */
        for (exonIx2 = 0, exonStart2 = 0;
             (exonIx2 < gp2->exonCount) && (exonStart2 < exonEnd1);
             exonIx2++)
            {
            if (gpGetExon(gp2, exonIx2, cdsOnly, &exonStart2, &exonEnd2))
                {
                if ((exonStart2 < exonEnd1) && (exonEnd2 > exonStart1))
                    return TRUE; /* overlaps */
                }
            }
        }
    }
return FALSE;
}

struct slRef *getGeneConflicts(struct cluster *cluster, struct clusterGene *gene,
                               boolean cdsOnly)
/* get list of genes in this cluster that don't share exons/CDS */
{
struct slRef *conflicts = NULL;
struct clusterGene *cg;

/* check all other genes */
for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    if ((cg != gene) && !shareExons(cg->gp, gene->gp, cdsOnly))
        refAdd(&conflicts, cg);
    }
return conflicts;
}

void getClusterConflicts(struct cluster *cluster)
/* determine if the cluster has conflicts and fill in clusterGene
 * list of conflicts */
{
struct clusterGene *cg;

for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    cg->exonConflicts = getGeneConflicts(cluster, cg, FALSE);
    if (cg->exonConflicts != NULL)
        {
        slSort(&cg->exonConflicts, clusterGeneRefCmp);
        cluster->hasExonConflicts = TRUE;
        }
    cg->cdsConflicts = getGeneConflicts(cluster, cg, TRUE);
    if (cg->cdsConflicts != NULL)
        {
        slSort(&cg->cdsConflicts, clusterGeneRefCmp);
        cluster->hasCdsConflicts = TRUE;
        }
    }
}

void getConflicts(struct cluster *clusters)
/* search for conflicst in clusters and fill in the data structs */
{
struct cluster *cl;
for (cl = clusters; cl != NULL; cl = cl->next)
    getClusterConflicts(cl);
}

int totalGeneCount = 0;
int totalClusterCount = 0;

struct clusterMaker
/* Something that helps us make clusters. */
    {
    struct clusterMaker *next;	/* Next in list */
    struct dlList *clusters;	/* Doubly linked list of clusters. */
    struct binKeeper *bk;	/* Bin-keeper that tracks exons. */
    };

struct clusterMaker *clusterMakerStart(int chromSize)
/* Allocate a new clusterMaker */
{
struct clusterMaker *cm;
AllocVar(cm);
cm->bk = binKeeperNew(0, chromSize);
cm->clusters = dlListNew();
return cm;
}

int nextClusterId = 1;  /* next cluster id to assign */

struct cluster *clusterMakerFinish(struct clusterMaker **pCm)
/* Finish up cluster - free up and return singly-linked list of clusters. */
{
struct dlNode *node;
struct cluster *clusterList = NULL, *cluster;
struct clusterMaker *cm = *pCm;

if (cm == NULL)
    errAbort("Null cluster in clusterMakerFinish");

/* We build up the cluster list as a doubly-linked list
 * to make it faster to remove clusters that get merged
 * into another cluster.  At the end though we make
 * a singly-linked list out of it. */
for (node = cm->clusters->tail; !dlStart(node); node=node->prev)
    {
    cluster = node->val;
    slAddHead(&clusterList, cluster);
    }
slSort(&clusterList, clusterCmp);

getConflicts(clusterList);

/* assign ids */
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    cluster->id = nextClusterId++;

/* Clean up and go home. */
binKeeperFree(&cm->bk);
dlListFree(&cm->clusters);
freez(pCm);
return clusterList;
}

void clusterMakerAddExon(struct clusterMaker *cm, struct track *track, struct genePred *gp,
                         int exonStart, int exonEnd, struct dlNode **oldNodePtr)
/* Add a gene exon to clusterMaker. */
{
struct dlNode *oldNode = *oldNodePtr;
struct binElement *bEl, *bList = binKeeperFind(cm->bk, exonStart, exonEnd);
verbose(4, "  %s %d-%d\n", track->name, exonStart, exonEnd);
if (bList == NULL)
    {
    if (oldNode == NULL)
        {
        struct cluster *cluster = clusterNew();
        oldNode = dlAddValTail(cm->clusters, cluster);
        }
    addExon(cm->bk, oldNode, exonStart, exonEnd, track, gp);
    }
else
    {
    for (bEl = bList; bEl != NULL; bEl = bEl->next)
        {
        struct dlNode *newNode = bEl->val;
        if (newNode != oldNode)
            {
            if (oldNode == NULL)
                {
                /* Add to existing cluster. */
                oldNode = newNode;
                }
            else
                {
                /* Merge new cluster into old one. */
                verbose(3, "Merging %p %p\n", oldNode, newNode);
                mergeClusters(cm->bk, bEl->next, oldNode, newNode);
                }
            }
        }
    addExon(cm->bk, oldNode, exonStart, exonEnd, track, gp);
    slFreeList(&bList);
    }
*oldNodePtr = oldNode;
}

void clusterMakerAdd(struct clusterMaker *cm, struct track *track, struct genePred *gp)
/* Add gene to clusterMaker. */
{
int exonIx;
struct dlNode *oldNode = NULL;

/* Build up cluster list with aid of binKeeper.  For each exon look to see if
 * it overlaps an existing cluster.  If so put it into existing cluster,
 * otherwise make a new cluster.  The hard case is where part of the gene is
 * already in one cluster and the exon overlaps with a new cluster.  In this
 * case merge the new cluster into the old one.  If we are joining contained
 * genes, the only gene range is added as if it was a single exon. */

verbose(2, "%s %s %d-%d\n", track->name, gp->name, gp->txStart, gp->txEnd);
if (gJoinContained)
    {
    int start =(gUseCds) ? gp->cdsStart : gp->txStart;
    int end =(gUseCds) ? gp->cdsEnd : gp->txEnd;
    if (end > start) 
        clusterMakerAddExon(cm, track, gp, start, end, &oldNode);
    }
else
    {
    for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
        {
        int exonStart, exonEnd;
        if (gpGetExon(gp, exonIx, gUseCds, &exonStart, &exonEnd))
            clusterMakerAddExon(cm, track, gp, exonStart, exonEnd, &oldNode);
        }
    }
}

void loadGenes(struct clusterMaker *cm, struct sqlConnection *conn,
               struct track* track, char *chrom, char strand,
               struct genePred **gpList)
/* load genes into cluster from a table or file */
{
struct genePredReader *gpr;
struct genePred *gp;
verbose(2, "%s %s %c\n", track->table, chrom, strand);

/* setup reader for file or table */
if (track->isDb)
    {
    char where[128];
    safef(where, sizeof(where), "chrom = '%s' and strand = '%c'", chrom, strand);
    gpr = genePredReaderQuery(conn, track->table,  where);
    }
else 
    {
    gpr = genePredReaderFile(track->table, chrom);
    }

/* read and add to cluster and deletion list */
while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    if (gp->strand[0] == strand)
        {
        slAddHead(gpList, gp);
        clusterMakerAdd(cm, track, gp);
        ++totalGeneCount;
        }
    else
        {
        genePredFree(&gp);
        }
    }
genePredReaderFree(&gpr);
}

void prConflicts(FILE *f, struct slRef* conflicts)
/* print list of conflicts as comma-seperated list */
{
struct slRef* cl;
fprintf(f, "\t");
for (cl = conflicts; cl != NULL; cl = cl->next)
    {
    struct clusterGene *cg = cl->val;
    fprintf(f, "%s:%s,", cg->track->name, cg->gp->name);
    }
}

void prGene(FILE *f, struct cluster *cluster, struct clusterGene *cg)
/* output info on one gene */
{
fprintf(f, "%d\t%s\t%s\t%s\t%d\t%d\t%s\t%c\t%c", cluster->id, cg->track->name, cg->gp->name, cg->gp->chrom, cg->gp->txStart, cg->gp->txEnd,
        cg->gp->strand,
        (cluster->hasExonConflicts ? 'y' : 'n'),
        (cluster->hasCdsConflicts ? 'y' : 'n'));
prConflicts(f, cg->exonConflicts);
prConflicts(f, cg->cdsConflicts);
fprintf(f, "\n");
}

Bits* mkClusterMap(struct cluster *cluster)
/* make a bit map of the exons in a cluster */
{
int len = (cluster->end - cluster->start);
Bits *map = bitAlloc(len);
struct clusterGene *cg;
int iExon;

for (cg = cluster->genes; cg != NULL; cg = cg->next)
    {
    for (iExon = 0; iExon < cg->gp->exonCount; iExon++)
        bitSetRange(map, (cg->gp->exonStarts[iExon]-cluster->start),
                    (cg->gp->exonEnds[iExon]-cg->gp->exonStarts[iExon]));
    }
return map;
}

void outputFlatBed(struct cluster *cluster, char strand, FILE *flatBedFh)
/* output a clusters a a single bed record */
{
static struct bed bed;  /* bed buffer */
static int capacity = 0;

int len = (cluster->end - cluster->start);
Bits *map = mkClusterMap(cluster); 
int startIdx = 0;
char nameBuf[64];

/* setup bed */
if (capacity == 0)
    {
    ZeroVar(&bed);
    capacity = 16;
    bed.blockSizes = needMem(capacity*sizeof(int));
    bed.chromStarts = needMem(capacity*sizeof(int));
    }
bed.chrom = cluster->chrom;
bed.chromStart = cluster->start;
bed.chromEnd = cluster->end;
bed.blockCount = 0;
safef(nameBuf, sizeof(nameBuf), "cl%d", cluster->id);
bed.name = nameBuf;
bed.strand[0] = strand;
bed.thickStart = cluster->start;
bed.thickEnd = cluster->end;

/* add blocks */
while ((startIdx = bitFindSet(map, startIdx, len)) < len)
    {
    int endIdx = bitFindClear(map, startIdx, len);
    if (bed.blockCount == capacity)
        {
        /* grouw memory in bed buffer */
        int oldSize = capacity*sizeof(int);
        int newSize = capacity*capacity*sizeof(int);
        bed.blockSizes = needMoreMem(bed.blockSizes, oldSize, newSize);
        bed.chromStarts = needMoreMem(bed.chromStarts, oldSize, newSize);
        capacity *= capacity;
        }
    bed.blockSizes[bed.blockCount] = endIdx-startIdx;
    bed.chromStarts[bed.blockCount] = startIdx; 
    bed.blockCount++;
    startIdx = endIdx;
    }
bedTabOutN(&bed, 12, flatBedFh);

bitFree(&map);
}

void outputClusters(struct cluster *clusterList, char strand, FILE *outFh,
                    FILE *clBedFh, FILE *flatBedFh)
/* output clusters */
{
struct cluster *cluster;
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    if (cluster->id >= 0)
        {
        struct clusterGene *cg;
        for (cg = cluster->genes; cg != NULL; cg = cg->next)
            prGene(outFh, cluster, cg);
        ++totalClusterCount;
        if (clBedFh != NULL)
            fprintf(clBedFh, "%s\t%d\t%d\tcl%d\n",
                    cluster->chrom, cluster->start, cluster->end,
                    cluster->id);
        if (flatBedFh != NULL)
            outputFlatBed(cluster, strand, flatBedFh);
        }
}

void clusterGenesOnStrand(struct sqlConnection *conn,
                          char *chrom, char strand, FILE *outFh,
                          FILE *clBedFh, FILE *flatBedFh)
/* Scan through genes on this strand, cluster, and write clusters to file. */
{
struct genePred *gpList = NULL;
struct cluster *clusterList = NULL;
struct track* track;
struct clusterMaker *cm = clusterMakerStart(hChromSize(chrom));

for (track = gTracks; track != NULL; track = track->next)
    loadGenes(cm, conn, track, chrom, strand, &gpList);

clusterList = clusterMakerFinish(&cm);
outputClusters(clusterList, strand, outFh, clBedFh, flatBedFh);

genePredFreeList(&gpList);
clusterFreeList(&clusterList);
}

struct track *buildTrackList(int specCount, char *specs[])
/* build list of tracks, consisting of list of tables, files, or
 * pairs of trackNames and files */
{
struct track* tracks = NULL;
int i;
if (gTrackNames)
    {
    for (i = 0; i < specCount; i += 2)
        slSafeAddHead(&tracks, trackNew(specs[i], specs[i+1]));
    }
else
    {
    for (i = 0; i < specCount; i++)
        slSafeAddHead(&tracks, trackNew(NULL, specs[i]));
    }
slReverse(&tracks);
return tracks;
}

FILE *openOutput(char *outFile)
/* open the output file and write the header */
{
FILE *f = mustOpen(outFile, "w");
fputs("#"
      "cluster\t"
      "table\t"
      "gene\t"
      "chrom\t"
      "txStart\t"
      "txEnd\t"
      "strand\t"
      "hasExonConflicts\t"
      "hasCdsConflicts\t"
      "exonConflicts\t"
      "cdsConflicts\n", f);
return f;
}

void clusterGenes(char *outFile, char *database, int specCount, char *specs[])
/* clusterGenes - Cluster genes from genePred tracks. */
{
struct slName *chromList, *chrom;
struct sqlConnection *conn;
FILE *outFh = NULL;
FILE *clBedFh = NULL;
FILE *flatBedFh = NULL;

hSetDb(database);
chromList = optionMultiVal("chrom", NULL);
if (chromList == NULL)
    chromList = hAllChromNames();
slNameSort(&chromList);

gTracks  = buildTrackList(specCount, specs);

conn = hAllocConn();

outFh = openOutput(outFile);
if (optionExists("clusterBed"))
    clBedFh = mustOpen(optionVal("clusterBed", NULL), "w");
if (optionExists("flatBed"))
    flatBedFh = mustOpen(optionVal("flatBed", NULL), "w");

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    clusterGenesOnStrand(conn, chrom->name, '+', outFh, clBedFh, flatBedFh);
    clusterGenesOnStrand(conn, chrom->name, '-', outFh, clBedFh, flatBedFh);
    }
carefulClose(&clBedFh);
carefulClose(&flatBedFh);
carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
gUseCds = optionExists("cds");
gTrackNames = optionExists("trackNames");
gJoinContained = optionExists("joinContained");

if (!gTrackNames)
    {
    if (argc < 4)
        usage();
    }
else
    {
    if ((argc < 5) || !(argc & 1))
        usage();
    }
clusterGenes(argv[1], argv[2], argc-3, argv+3);
return 0;
}
