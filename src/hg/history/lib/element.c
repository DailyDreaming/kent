#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"

#include "element.h"

static char buffer[10*1024*1024];

static struct element *newEdge(struct element *parent, struct element *child);

struct element *newElement(struct genome *g, char *name, char *version)
{
struct element *e;

AllocVar(e);
e->genome = g;
e->species = g->name;
e->name = name;
e->version = version;
slAddHead(&g->elements, e);

return e;
}

void printGenomes(struct genome *genomes)
{
struct genome *genome;
for(genome=genomes; genome; genome = genome->next)
    {
    struct element *element;

    printf(">%s\n",genome->name);
    for(element = genome->elements; element; element = element->next)
	printf("%s.%s \n",element->name, element->version);
    printf("\n");
    }
}

char *bigWords[10*10*1024];

struct phyloTree *readEleTree(struct lineFile *lf, struct element **parents, 
	int numParents, boolean addStartStop)
{
int ii;
struct phyloTree *node = NULL;
struct genome *genome = NULL;
int wordsRead, elementsLeft = 0, numChildren = 0;
boolean needGenome = TRUE;
int count = 0;
//char *words[10*1024];
struct element **elements = NULL;
struct possibleEdge *p;
struct element *element = NULL;

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {

    if (needGenome)
	{
	if ((wordsRead != 4) || (bigWords[0][0] != '>'))
	    errAbort("elTree genomes are named starting with a '>' with 4 total words");

	AllocVar(node);
	AllocVar(node->ident);
	AllocVar(genome);
	node->priv = genome;
	genome->node = node;
	//slAddHead(&allGenomes, genome);

	node->ident->name = genome->name = cloneString(&bigWords[0][1]);
	genome->elementHash = newHash(8);
	elementsLeft = atoi(bigWords[1]);
	numChildren = atoi(bigWords[2]);
	node->ident->length =  atof(bigWords[3]);
	elements = NULL;
	if (elementsLeft)
	    {
	    needGenome = FALSE;
	    elements = needMem((elementsLeft + 2) * sizeof(struct element *));
	    }
	else 
	    break;

	verbose(2, "adding genome %s with %d elements\n",genome->name, elementsLeft);
	}
    else
	{
	int ii;
	struct chromBreak *chromBreak;

	if (genome == NULL)
	    errAbort("must specify genome name before listing elements");

	if (wordsRead /2> elementsLeft)
	    errAbort("too many elements in genome (%d vs %d)  %s",wordsRead/2,elementsLeft,genome->name);
	if ( (bigWords[0][0] == '>'))
	    errAbort("too few elements in genome %s elementsLeft %d\n",genome->name,elementsLeft);
	elementsLeft -= wordsRead/2;

	if (addStartStop)
	    {
	    AllocVar(chromBreak);
	    AllocVar(element);
	    chromBreak->element = element;
	    slAddHead(&genome->breaks, chromBreak);
	    slAddHead(&genome->elements, element);
	    element->genome = genome;
	    element->species = genome->name;
	    element->name = cloneString("START");
	    elements[count++] = element;
	    if (parents)
		{
		element->parent = parents[0];
		newEdge(element->parent, element);
		}
	    }

	for(ii=0; ii < wordsRead; ii+=2)
	    {
	    char *ptr;

	    AllocVar(element);
	    slAddHead(&genome->elements, element);
	    elements[count++] = element;

	    element->genome = genome;
	    element->species = genome->name;
	    element->name = cloneString(bigWords[ii]);
	    if (parents && !sameString(bigWords[ii+1], "0"))
		{
		if (addStartStop)
		    element->parent = parents[atoi(bigWords[ii+1]) ];
		else
		    element->parent = parents[atoi(bigWords[ii+1]) - 1 ];

		if (element->parent == NULL)
		    errAbort("parent is null %s",bigWords[ii+1]);

		newEdge(element->parent, element);

		if ((ptr = strchr(bigWords[ii+1], '*')) == NULL)
		    {
		    struct element *parent = element->parent;

		    if (parent->edges[0] != element)
			{
			assert (parent->numEdges > 1);
			int jj;

			for(jj=0; jj < parent->numEdges; jj++)
			    if (parent->edges[jj] == element)
				break;
				
			assert(jj < parent->numEdges);

			struct element *swap = parent->edges[0];
			parent->edges[0] = element;
			parent->edges[jj] = swap;
			}
		    }
		}
	    else
		{
		element->parent = NULL;
		}
	    if (*element->name == '-')
		{
		element->name++;
		element->isFlipped = 1;
		}
	    hashAdd(genome->elementHash, element->name, element);

	    if ((ptr = strchr(element->name, '.')) == NULL)
		errAbort("elements must be of the format name.version");
	    *ptr++ = 0;
	    element->version = ptr;
	    if(element->parent && !sameString(element->name, element->parent->name))
		warn("element named %s doesn't have same name as parent %s: line %d",
		    element->name, element->parent->name, lf->lineIx);

	    verbose(2, "added element %s.%s\n",element->name,element->version);
	    }
	if (elementsLeft == 0)
	    {
	    if (addStartStop)
		{
		AllocVar(chromBreak);
		AllocVar(element);
		elements[count++] = element;
	    //    AllocVar(p);
	     //   p->count = 1;
	      //  p->element = genome->elements;
	       // element->calced.prev = p;

	       // AllocVar(p);
	       // p->count = 1;
	       // p->element = element;
	       // genome->elements->calced.next = p;

		chromBreak->element = element;
		slAddHead(&genome->breaks, chromBreak);
		slAddHead(&genome->elements, element);
		element->genome = genome;
		element->species = genome->name;
		element->name = cloneString("END");
		if (parents)
		    {
		    element->parent = parents[numParents-1];
		    newEdge(element->parent, element);
		    if (element->parent == NULL)
			errAbort("bad eleTree");
		    }
		}
	    break;
	    }
	}
    }
    if (genome)
	{
	slReverse(&genome->breaks);
	slReverse(&genome->elements);
	}
    //AllocVar(p);
    //p->element = genome->elements->next;
    //p->count = 1;
    //genome->elements->calced.next = p;
    //AllocVar(p);
    //p->count = 1;
    //p->element = genome->elements;
    //genome->elements->next->calced.prev = p;

    if (addStartStop)
	{
	AllocVar(p);
	p->element = element;
	p->count = 1;
	genome->elements->calced.prev = p;

	AllocVar(p);
	p->count = 1;
	p->element = genome->elements;
	element->calced.next = p;
	}
    for(ii=0; ii < count; ii++)
	{
	if (elements[ii] == NULL)
	    errAbort("elemen %d isnull\n",ii);
	verbose(2, "elem count %d\n",elements[ii]->count);
	}

    for(ii=0; ii < numChildren; ii++)
	{
	struct phyloTree *childNode = readEleTree(lf,elements, count, addStartStop);
	if (childNode != NULL)
	    phyloAddEdge(node, childNode);
	}

    freez(&elements);
    return node;
}

struct phyloTree *eleReadTree(char *fileName, boolean addStartStop)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);

return readEleTree(lf, NULL, 0, addStartStop);
}

struct genome *readGenomes(char *fileName)
{
struct genome *allGenomes = NULL, *genome = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[100*2048];
int wordsRead, wordsLeft = 0;
boolean needGenome = TRUE;

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    {
    if (needGenome) 
	{
	if ((wordsRead != 2) || (words[0][0] != '>'))
	    errAbort("genomes are named starting with a '>' and followed by a count");

	AllocVar(genome);
	slAddHead(&allGenomes, genome);
	genome->name = cloneString(&words[0][1]);
	wordsLeft = atoi(words[1]);
	verbose(2, "adding genome %s\n",genome->name);
	genome->elementHash = newHash(8);
	if (wordsLeft)
	    needGenome = FALSE;
	}
    else
	{
	int ii;

	if (genome == NULL)
	    errAbort("must specify genome name before listing elements");

	if (wordsRead > wordsLeft)
	    errAbort("too many elements in genome (%d vs %d)  %s",wordsRead/2,wordsLeft/2,genome->name);
	if ( (words[0][0] == '>'))
	    errAbort("too few elements in genome %s\n",genome->name);
	wordsLeft -= wordsRead;

	if (wordsLeft == 0)
	    needGenome = TRUE;

	for(ii=0; ii < wordsRead; ii++)
	    {
	    char *ptr;
	    struct element *element;

	    AllocVar(element);
	    slAddHead(&genome->elements, element);

	    element->genome = genome;
	    element->species = genome->name;
	    element->name = cloneString(words[ii]);
	    if (*element->name == '-')
		{
		element->name++;
		element->isFlipped = 1;
		}
	    hashAdd(genome->elementHash, element->name, element);

	    if ((ptr = strchr(element->name, '.')) == NULL)
		errAbort("elements must be of the format name.version");
	    *ptr++ = 0;
	    element->version = ptr;
	    verbose(2, "added element %s.%s\n",element->name,element->version);
	    }
	}
    }

if ( wordsLeft)
    errAbort("too few elements in genome %s\n",genome->name);

for(genome=allGenomes; genome; genome = genome->next)
    slReverse(&genome->elements);

return allGenomes;
}

struct genome *getGenome(char *file, char *name)
{
    struct genome *list = readGenomes(file);
    struct genome *g;

    for(g = list; g ; g = g->next)
	{
	if (sameString(g->name, name))
	    return g;
	}

    return NULL;
}

static struct element *getElement(char *name, struct hash *genomeHash)
{
char *s = cloneString(name);
struct element *e;
char *ptr;
struct genome *genome;
char *species, *ename;// *version;

if ((ptr = strchr(s, '.')) == NULL)
    errAbort("element names must be species.element.version");
species = s;

*ptr++ = 0;
ename = ptr;
if ((ptr = strchr(ptr, '.')) == NULL)
    errAbort("element names must be species.element.version");

if ((genome = hashFindVal(genomeHash, species)) == NULL)
    errAbort("can't find element list for %s\n",species);

if ((e = hashFindVal(genome->elementHash, ename)) == NULL)
    printf("Warn: can't find element with name %s in genome %s\n",ename,species);

return e;
}


void setElementDist(struct element *e1, struct element *e2, double dist,
    struct distance **distanceList, struct hash **pDistHash, struct hash **pDistEleHash)
{
int val = 0;
// char buffer[512];
char *str = "%s.%s.%s-%s.%s.%s";
float *pdist;

if (!(e1 && e2))
    {
    errAbort("setElementDist: both elements NULL");
    return;
    }

if (!sameString(e1->name, e2->name))
    errAbort("can't set element distance on elements with different names");

if ((val = strcmp(e1->species, e2->species)) == 0)
    {
    //printf("ignoring orthologs\n");
    //return;
    val = strcmp(e1->version , e2->version);
    }

if (val < 0)
    safef(buffer, sizeof(buffer), str, e1->species, e1->name, e1->version,
    	e2->species, e2->name, e2->version);
else
    safef(buffer, sizeof(buffer), str, e2->species, e2->name, e2->version,
    	e1->species, e1->name, e1->version);

if (*pDistHash == NULL)
    *pDistHash = newHash(5);
if (*pDistEleHash == NULL)
    *pDistEleHash = newHash(5);

if ((pdist = (float *)hashFindVal(*pDistHash, buffer)) != NULL)
    {
#define EPSILON 0.01
    if (fabs(dist - *pdist) > EPSILON)
	{
	errAbort("attempting to set dist for %s to different value (%g) (%g)\n",
	    buffer, dist, *pdist);
	return;
	}
    }
else
    {
	struct distance *distance;
	struct eleDistance *list;
	struct eleDistance *ed;
	char *p;

	AllocVar(pdist);
	*pdist = dist;
	hashAdd(*pDistHash, buffer, pdist);

	AllocVar(distance);
	slAddHead(distanceList, distance);

	distance->e1 = e1;
	distance->e2 = e2;
	distance->distance = dist;
	distance->new = 1;
	
	AllocVar(ed);
	ed->distance = distance;
	//safef(buffer, sizeof(buffer), "%s.%s.%s", e1->species, e1->name, e1->version);
	p = eleName(e1);
	if ((list = hashFindVal(*pDistEleHash, p)) == NULL)
	    hashAdd(*pDistEleHash, p, ed);
	else
	    slAddTail(&list, ed);

	AllocVar(ed);
	ed->distance = distance;
	//safef(buffer, sizeof(buffer), "%s.%s.%s", e2->species, e2->name, e2->version);
	p = eleName(e2);
	if ((list = hashFindVal(*pDistEleHash, p)) == NULL)
	    hashAdd(*pDistEleHash, p, ed);
	else
	    slAddTail(&list, ed);
    }
}

int sortByDistance(const void *p1, const void *p2)
{
const struct distance *d1 = *((struct distance **)p1);
const struct distance *d2 = *((struct distance **)p2);
double diff;

if ((diff = d1->distance - d2->distance) > 0)
    return 1;
else if (diff < 0)
    return -1;

return d1->e1->numEdges + d1->e2->numEdges - d2->e1->numEdges - d2->e2->numEdges;
//return strlen(eleName(d1->e1)) + strlen(eleName(d1->e2)) - strlen(eleName(d2->e1))- strlen(eleName(d2->e2));
//return 0;
}

struct distance *readDistances(char *fileName, struct hash *genomeHash,
    struct hash **pDistHash, struct hash **pDistEleHash)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[128 * 1024];
int wordsRead;
struct distance *distances = NULL;

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *))))
    {
    int count = 0;
    char **rowNames;
    double **rowValues;
    int ii, jj;

    if ((wordsRead != 1) || ((count = atoi(words[0])) <= 0))
	errAbort("first line of dist matrix is count of rows");

    rowNames = (char **)needMem(count * sizeof(char *));
    rowValues = (double **)needMem(count * sizeof(double *));
    if (count + 1 > sizeof(words)/sizeof(char *))
	errAbort("buffer not big enough for %d words in readDistances", count + 1);

    for(ii=0; ii < count; ii++)
	{
	wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *));
	if (wordsRead != count + 1)
	    errAbort("number of words in distance matrix row must be %d line %d\n",count+1,lf->lineIx);

	rowNames[ii] = cloneString(words[0]);
	rowValues[ii] = needMem(sizeof(double) * count);

	for(jj=0; jj < count ; jj++)
	    rowValues[ii][jj] = atof(words[jj+1]);
	}
    for(ii=0; ii < count; ii++)
	{
	for(jj=0; jj < count ; jj++)
	    {
	    char *s1 = rowNames[ii];
	    char *s2 = rowNames[jj];
	    struct element *e1, *e2;

	    if (ii == jj)
		continue;

	    if (strcmp(s1, s2) > 0)
		{
		s2 = rowNames[ii];
		s1 = rowNames[jj];
		}
	    e1 = getElement(s1, genomeHash);
	    e2 = getElement(s2, genomeHash);

	    setElementDist(e1, e2, rowValues[ii][jj], &distances,
		pDistHash, pDistEleHash);
	    }
	}
    }

slSort(&distances, sortByDistance);
return distances;
}


static struct element *newEdge(struct element *parent, struct element *child)
{
parent->numEdges++;

if (parent->numEdges > parent->allocedEdges)
    {
    int oldSize = parent->allocedEdges * sizeof (struct element *);
    int newSize;

    parent->allocedEdges += 5;
    newSize = parent->allocedEdges * sizeof (struct element *);
    parent->edges = needMoreMem(parent->edges, oldSize, newSize);
    }

child->parent = parent;
return parent->edges[parent->numEdges -1 ] = child;
}

struct element *eleAddEdge(struct element *parent, struct element *child)
/* add an edge to an element */
{
//if (parent->numEdges == 2)
    //errAbort("adding a third edge");
return newEdge(parent, child);
}

void eleDeleteEdge(struct element *tree, struct element *edge)
/* delete an edge from a node.  Aborts on error */
{
int ii;

for (ii=0; ii < tree->numEdges; ii++)
    if (tree->edges[ii] == edge)
	{
	memcpy(&tree->edges[ii], &tree->edges[ii+1], sizeof(tree) * (tree->numEdges - ii - 1));
	tree->numEdges--;
	//phyloFreeTree(edge);
	return;
	}

errAbort("tried to delete non-existant edge");
}

char *eleFullName(struct element *e, boolean doNeg)
{
// static char buffer[512];

if (e->isFlipped ^ doNeg)
    //safef(buffer,sizeof buffer, "-%s.%s.%s",e->species,e->name, e->version);
    safef(buffer,sizeof buffer, "-%s.%s",e->name,e->version);
else
    safef(buffer,sizeof buffer, "%s.%s",e->name,e->version);

return buffer;
}

char *eleName(struct element *e)
{
// static char buffer[512];

safef(buffer,sizeof buffer, "%s.%s.%s",e->species,e->name, e->version);

return cloneString(buffer);
}

void printElementTrees(struct phyloTree *node, int depth)
{
struct genome *g = node->priv;
struct element *e;
int ii;

for(ii=0; ii < depth; ii++)
    printf(" ");
printf("%s %g %d: ",g->name, node->ident->length, node->numEdges);
for(e = g->elements; e; e = e->next)
    {
    if (e->isFlipped)
	printf("-");
    printf("%s.%s %d ",e->name,e->version,e->numEdges);
    }
printf("\n");

for(ii=0; ii < node->numEdges; ii++)
    printElementTrees(node->edges[ii], depth+1);
}

static void assignElemNums(struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e;
int ii;

for(ii=0, e = g->elements; e ; ii++, e= e->next)
    {
    int jj;
    if(e->genome != g)
	errAbort("element genome not equal to host genome");
    for(jj=0; jj < e->numEdges; jj++)
	assert(e->edges[jj]->parent == e);
    //assert(e->numEdges < 3);
    //if ((e->parent == NULL) && g->node->parent)
	//errAbort("more mess");
    //if (e->parent)// && e->genome->node)
	//if (e->genome->node->parent->priv != e->parent->genome)
	    //errAbort("parent mess");
    e->count = ii;
    }

for(ii=0; ii < node->numEdges; ii++)
    assignElemNums(node->edges[ii]);
}

static void outElems(FILE *f, struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e;
int ii;

//for(ii = 0, e = g->elements; e; ii++,e = e->next)
    //;
fprintf(f, ">%s %d %d %g\n",g->name, slCount(g->elements), node->numEdges, node->ident->length);

for(e = g->elements, ii = 0; e; e = e->next, ii++)
    {
    struct element *parent = e->parent;
    int jj = 0;

    if (e->isFlipped)
	fprintf(f, "-");
    if ((parent) && (parent->numEdges > 1))
	{
	for(jj=0; jj < parent->numEdges; jj++)
	    if (parent->edges[jj] == e)
		break;
	assert(jj < parent->numEdges);
	}

    if (parent && (node->parent->numEdges == 1) && (jj != 0))
	fprintf(f,"%s.%s %d* ",e->name,e->version, (e->parent) ? e->parent->count + 1 : 0);
    else
	fprintf(f,"%s.%s %d ",e->name,e->version, (e->parent) ? e->parent->count + 1 : 0 );
    if (((ii + 1) % 8) == 0)
	fprintf(f,"\n");
    }
fprintf(f,"\n");

for(ii=0; ii < node->numEdges; ii++)
    outElems(f, node->edges[ii]);
}

void outElementTrees(FILE *f, struct phyloTree *node)
{
assignElemNums(node);
outElems(f, node);
}

char *nextVersion()
{
static int count = 0;
static char buffer[5];

if ((count / (26*26 *26)) > 25)
    errAbort("version overflow");

buffer[0] = count / (26*26*26) + 'A';
buffer[1] = (count % (26*26*26)) / (26*26) + 'A';
buffer[2] = (count % (26 * 26)) / 26 + 'A';
buffer[3] = count % 26 + 'A';
buffer[4] = 0;

count++;
return buffer;
}

char *nextGenome()
{
static int count = 0;
static char buffer[3];

buffer[0] = count / 26 + 'A';
buffer[1] = count % 26 + 'A';
buffer[2] = 0;
count++;
return buffer;
}

void removeXs(struct genome *list, char ch)
{
for(; list; list = list->next)
    {
    char *ptr, *lastPtr;

    lastPtr = &list->name[strlen(list->name)];
    for(ptr = list->name; *ptr; )
	if (*ptr == ch)
	    memcpy(ptr, ptr+1, (lastPtr - ptr)); 
	else 
	    ptr++;
    if (list->node)
	list->node->ident->name = list->name;
    }
}

void removeStartStop(struct genome *g)
{
struct element *e = g->elements;
struct element *prev = NULL;

if (!sameString(e->name, "START"))
    errAbort("first element not START");

g->elements = e->next;

prev = NULL;
for(e = g->elements; e->next ; prev = e, e = e->next)
    ;

if (!sameString(e->name, "END"))
    errAbort("last element not END");

if (prev == NULL)
    g->elements = NULL;
else
    prev->next = NULL;
}

void removeAllStartStop(struct phyloTree *node)
{
int ii;

for(ii=0; ii < node->numEdges; ii++)
    removeAllStartStop(node->edges[ii]);

removeStartStop(node->priv);
}

int workCountLeaves(struct phyloTree *node, int count )
{
int ii;

for(ii = 0; ii < node->numEdges; ii++)
    count = workCountLeaves(node->edges[ii], count);

if (node->numEdges == 0)
    count++;

return count;
}

int countLeaves(struct phyloTree *node)
{
    return workCountLeaves(node, 0);
}
