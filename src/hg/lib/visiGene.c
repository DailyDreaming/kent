/* visiGene.c  - Interface to visiGene database. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "visiGene.h"

static char *cloneOrNull(char *s)
/* Return copy of string, or NULL if it is empty */
{
if (s == NULL || s[0] == 0)
    return NULL;
return cloneString(s);
}

void visiGeneImageSize(struct sqlConnection *conn, int id, int *imageWidth, int *imageHeight)
/* Get width and height of image with given id */
{
char query[256];
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), 
	"select imageFile.imageWidth, imageFile.imageHeight from image,imageFile"
	" where image.id = %d and image.imageFile = imageFile.id "
	, id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("No image of id %d", id);
*imageWidth = atoi(row[0]);
*imageHeight = atoi(row[1]);
sqlFreeResult(&sr);
}

static char *somePath(struct sqlConnection *conn, int id, char *locationField)
/* Fill in path based on given location field */
{
char query[256], path[PATH_LEN];
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), 
	"select fileLocation.name,imageFile.fileName from image,imageFile,fileLocation"
	" where image.id = %d and image.imageFile = imageFile.id "
	" and fileLocation.id=imageFile.%s", id, locationField);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("No image of id %d", id);
safef(path, PATH_LEN, "%s/%s", row[0], row[1]);
sqlFreeResult(&sr);
return cloneString(path);
}

char *visiGeneFullSizePath(struct sqlConnection *conn, int id)
/* Fill in path for full image visiGene of given id.  FreeMem
 * this when done */
{
return somePath(conn, id, "fullLocation");
}

char *visiGeneThumbSizePath(struct sqlConnection *conn, int id)
/* Fill in path for thumbnail image (~200x200) visiGene of given id. 
 * FreeMem when done. */
{
return somePath(conn, id, "thumbLocation");
}

#define TIME_FORMAT_SIZE 32

static void formatDays(double days, char out[TIME_FORMAT_SIZE])
/* Write time in hours, days, weeks, or years depending on
 * what seems appropriate. */
{
if (days < 1.0)
    safef(out, TIME_FORMAT_SIZE, "%3.1f hour", 24.0 * days);
else if (days < 2*7.0)
    safef(out, TIME_FORMAT_SIZE, "%3.1f day", days);
else if (days < 2*30.0)
    safef(out, TIME_FORMAT_SIZE, "%3.1f week", days/7);
else if (days < 2*365.0)
    safef(out, TIME_FORMAT_SIZE, "%3.1f month", days/30);
else 
    safef(out, TIME_FORMAT_SIZE, "%3.1f year", days/365);
}

char *visiGeneOrganism(struct sqlConnection *conn, int id)
/* Return binomial scientific name of organism associated with given image. 
 * FreeMem this when done. */
{
char query[256], buf[256];
safef(query, sizeof(query),
	"select uniProt.taxon.binomial from image,specimen,uniProt.taxon"
         " where image.id = %d and"
	 " image.specimen = specimen.id and"
	 " specimen.taxon = uniProt.taxon.id",
	 id);
return cloneOrNull(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

char *visiGeneStage(struct sqlConnection *conn, int id, boolean doLong)
/* Return string describing growth stage of organism.  The description
 * will be 5 or 6 characters wide if doLong is false, and about
 * 25-50 characters wide if it is true. FreeMem this when done. */
{
char query[256];
double age;	/* Days since conception */
int taxon;	/* Species id. */
struct sqlResult *sr;
char **row;
double stageAge; /* Days since developmental milestone */
char *stageName;	/* Name of stage. */
char *stageLetter = "";	/* Letter for stage. */
struct dyString *dy = dyStringNew(256);


/* Figure out days since conception and species. */
safef(query, sizeof(query),
    "select specimen.age,genotype.taxon "
    "from image,specimen,genotype "
    "where image.id = %d "
    "and image.specimen = specimen.id "
    "and specimen.genotype = genotype.id"
    , id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    internalErr();
age = atof(row[0]);
taxon = atoi(row[1]);
sqlFreeResult(&sr);

/* Try and look things up in lifeTime table, figuring
 * out stage name, age, and letter. */
safef(query, sizeof(query),
    "select birth,adult from lifeTime where taxon = %d", taxon);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    double birth = atof(row[0]);
    double adult = atof(row[1]);
    if (age < birth)
        {
	stageName = "embryo";
	stageAge = age;
	stageLetter = "e";
	}
    else if (age < adult)
        {
	stageName = "pup";
	stageAge = age - birth;
	stageLetter = "p";
	}
    else
        {
	stageName = "adult";
	stageAge = age - birth;
	stageLetter = "p";
	}
    }
else
    {
    stageName = "conception";
    stageAge = age;
    stageLetter = "";
    }
sqlFreeResult(&sr);

/* Format output. */
if (doLong)
    {
    char timeBuf[TIME_FORMAT_SIZE];
    char *stageSchemeName = NULL;
    int stageScheme = 0;
    formatDays(stageAge, timeBuf);
    dyStringPrintf(dy, "%s old %s", timeBuf, stageName);

    /* See if have nice developmental staging scheme for this organism. */
    safef(query, sizeof(query), 
       "select id,name from lifeStageScheme where taxon = %d", taxon);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	stageScheme = atoi(row[0]);
	stageSchemeName = cloneString(row[1]);
	}
    sqlFreeResult(&sr);

    /* Add developmental stage info. */
    if (stageScheme != 0)
        {
	safef(query, sizeof(query),
	    "select name from lifeStage "
	    "where lifeStageScheme = %d and age < %f "
	    "order by age desc"
	    , stageScheme, age);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    dyStringPrintf(dy, " (%s %s)", stageSchemeName, row[0]);
	sqlFreeResult(&sr);
	}
    freez(&stageSchemeName);
    }
else
    {
    if (stageLetter[0] == 'p')
	dyStringPrintf(dy, "p%d", round(stageAge));
    else
	dyStringPrintf(dy, "%s%3.1f", stageLetter, stageAge);
    }
return dyStringCannibalize(&dy);
}

struct slName *visiGeneGeneName(struct sqlConnection *conn, int id)
/* Return list of gene names  - HUGO if possible, RefSeq/GenBank, etc if not. 
 * slNameFreeList this when done. */
{
struct slName *list = NULL, *el = NULL;
struct sqlResult *sr;
char **row;
char query[256], buf[256];
safef(query, sizeof(query),
      "select gene.name,gene.locusLink,gene.refSeq,gene.genbank,gene.uniProt"
      " from imageProbe,probe,gene"
      " where imageProbe.image = %d"
      " and imageProbe.probe = probe.id"
      " and probe.gene = gene.id", id);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    char *locusLink = row[1];
    char *refSeq = row[2];
    char *genbank = row[3];
    char *uniProt = row[4];
    if (name[0] != 0)
        el = slNameNew(name);
    else if (refSeq[0] != 0)
        el = slNameNew(refSeq);
    else if (genbank[0] != 0)
        el = slNameNew(genbank);
    else if (uniProt[0] != 0)
        el = slNameNew(uniProt);
    else if (locusLink[0] != 0)
	{
	safef(buf, sizeof(buf), "Entrez Gene %s", locusLink);
        el = slNameNew(buf);
	}
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

static struct slName *extNamesForId(struct sqlConnection *conn, int id, 
	char *field)
/* Return list of external identifiers of type described in field
 * associated with image. */
{
struct slName *list = NULL, *el;
struct sqlResult *sr;
char **row;
char query[256];
safef(query, sizeof(query),
      "select gene.%s from imageProbe,probe,gene"
      " where imageProbe.image = %d"
      " and imageProbe.probe = probe.id"
      " and probe.gene = gene.id", field, id);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

struct slName *visiGeneRefSeq(struct sqlConnection *conn, int id)
/* Return RefSeq accession or n/a if none available. 
 * slNameFreeList this when done. */
{
return extNamesForId(conn, id, "refSeq");
}

struct slName *visiGeneGenbank(struct sqlConnection *conn, int id)
/* Return Genbank accession or n/a if none available. 
 * slNameFreeList this when done. */
{
return extNamesForId(conn, id, "genbank");
}

struct slName *visiGeneUniProt(struct sqlConnection *conn, int id)
/* Return Genbank accession or n/a if none available. 
 * slNameFreeList this when done. */
{
return extNamesForId(conn, id, "uniProt");
}

char *visiGeneSubmitId(struct sqlConnection *conn, int id)
/* Return submitId  for image.  (Internal id for data set)
 * FreeMem this when done. */
{
char query[256];
safef(query, sizeof(query),
    "select imageFile.submitId from image,imageFile "
    "where image.id = %d and image.imageFile = imageFile.id", id);
return sqlQuickString(conn, query);
}

char *visiGeneBodyPart(struct sqlConnection *conn, int id)
/* Return body part if any.  FreeMem this when done. */
{
char query[256];
safef(query, sizeof(query),
    "select bodyPart.name from image,specimen,bodyPart "
    "where image.id = %d "
    "and image.specimen = specimen.id "
    "and specimen.bodyPart = bodyPart.id"
    , id);
return sqlQuickString(conn, query);
}

char *visiGeneSliceType(struct sqlConnection *conn, int id)
/* Return slice type if any.  FreeMem this when done. */
{
char query[256];
safef(query, sizeof(query),
    "select sliceType.name from image,preparation,sliceType "
    "where image.id = %d "
    "and image.preparation = preparation.id "
    "and preparation.sliceType = sliceType.id"
    , id);
return sqlQuickString(conn, query);
}

char *visiGeneFixation(struct sqlConnection *conn, int id)
/* Return fixation conditions if any.  FreeMem this when done.*/
{
char query[256];
safef(query, sizeof(query),
    "select fixation.description from image,preparation,fixation "
    "where image.id = %d "
    "and image.preparation = preparation.id "
    "and preparation.fixation = fixation.id"
    , id);
return sqlQuickString(conn, query);
}

char *visiGeneEmbedding(struct sqlConnection *conn, int id)
/* Return fixation conditions if any.  FreeMem this when done.*/
{
char query[256];
safef(query, sizeof(query),
    "select embedding.description from image,preparation,embedding "
    "where image.id = %d "
    "and image.preparation = preparation.id "
    "and preparation.embedding = embedding.id"
    , id);
return sqlQuickString(conn, query);
}

char *visiGenePermeablization(struct sqlConnection *conn, int id)
/* Return permeablization conditions if any.  FreeMem this when done.*/
{
char query[256];
safef(query, sizeof(query),
    "select permeablization.description from image,preparation,permeablization "
    "where image.id = %d "
    "and image.preparation = preparation.id "
    "and preparation.permeablization = permeablization.id"
    , id);
return sqlQuickString(conn, query);
}

static char *stringFieldInSubmissionSet(struct sqlConnection *conn, int id, 
	char *field)
/* Return some string field in submissionSet table when you have image id. */
{
char query[256];
safef(query, sizeof(query),
     "select submissionSet.%s from image,imageFile,submissionSet"
     " where image.id = %d"
     " and image.imageFile = imageFile.id "
     " and imageFile.submissionSet = submissionSet.id", field, id);
return sqlQuickString(conn, query);
}

char *visiGeneContributors(struct sqlConnection *conn, int id)
/* Return comma-separated list of contributors in format Kent W.H, Wu F.Y. 
 * FreeMem this when done. */
{
return stringFieldInSubmissionSet(conn, id, "contributors");
}

char *visiGenePublication(struct sqlConnection *conn, int id)
/* Return name of publication associated with image if any.  
 * FreeMem this when done. */
{
return stringFieldInSubmissionSet(conn, id, "publication");
}

char *visiGenePubUrl(struct sqlConnection *conn, int id)
/* Return url of publication associated with image if any.
 * FreeMem this when done. */
{
return stringFieldInSubmissionSet(conn, id, "pubUrl");
}

char *visiGeneSetUrl(struct sqlConnection *conn, int id)
/* Return contributor url associated with image set if any. 
 * FreeMem this when done. */
{
return stringFieldInSubmissionSet(conn, id, "setUrl");
}

char *visiGeneItemUrl(struct sqlConnection *conn, int id)
/* Return contributor url associated with this image. 
 * Substitute in submitId for %s before using.  May be null.
 * FreeMem when done. */
{
return stringFieldInSubmissionSet(conn, id, "itemUrl");
}

char *visiGeneCopyright(struct sqlConnection *conn, int id)
/* Return copyright statement if any,  NULL if none.
 * FreeMem this when done. */
{
char query[256];
safef(query, sizeof(query),
    "select copyright.notice from image,imageFile,submissionSet,copyright "
    "where image.id = %d "
    "and image.imageFile = imageFile.id "
    "and imageFile.submissionSet = submissionSet.id "
    "and submissionSet.copyright = copyright.id"
    , id);
return sqlQuickString(conn, query);
}

static void appendMatchHow(struct dyString *dy, char *pattern,
	enum visiGeneSearchType how)
/* Append clause to do search on pattern according to how on dy */
{
switch (how)
    {
    case vgsExact:
        dyStringPrintf(dy, " = \"%s\"", pattern);
	break;
    case vgsPrefix:
        dyStringPrintf(dy, " like \"%s%%\"", pattern);
	break;
    case vgsLike:
        dyStringPrintf(dy, " like \"%s\"", pattern);
	break;
    default:
        internalErr();
	break;
    }
}

struct slInt *visiGeneSelectNamed(struct sqlConnection *conn,
	char *name, enum visiGeneSearchType how)
/* Return ids of images that have probes involving gene that match name. */
{
struct hash *uniqHash = newHash(0);
struct slName *geneList = NULL, *geneEl;
struct slInt *imageList = NULL, *imageEl;
struct dyString *dy = dyStringNew(0);
char **row;
struct sqlResult *sr;

dyStringPrintf(dy, "select id from gene where name ");
appendMatchHow(dy, name, how);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    geneEl = slNameNew(row[0]);
    slAddHead(&geneList, geneEl);
    }
sqlFreeResult(&sr);

dyStringClear(dy);
dyStringPrintf(dy, "select gene from geneSynonym where name ");
appendMatchHow(dy, name, how);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    geneEl = slNameNew(row[0]);
    slAddHead(&geneList, geneEl);
    }
slReverse(&geneList);
sqlFreeResult(&sr);

for (geneEl = geneList; geneEl != NULL; geneEl = geneEl->next)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "select imageProbe.image from probe,imageProbe");
    dyStringPrintf(dy, " where probe.gene = %s ", geneEl->name);
    dyStringAppend(dy, " and probe.id = imageProbe.probe");
    sr = sqlGetResult(conn, dy->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *ids = row[0];
	if (!hashLookup(uniqHash, ids))
	    {
	    hashAdd(uniqHash, ids, NULL);
	    imageEl = slIntNew(sqlUnsigned(ids));
	    slAddHead(&imageList, imageEl);
	    }
	}
    sqlFreeResult(&sr);
    }
dyStringFree(&dy);
hashFree(&uniqHash);
slReverse(&imageList);
return imageList;
}

struct slInt *matchingExtId(struct sqlConnection *conn, char *acc, char *field)
/* Return ids of images that have probes involving external id. */
{
struct slInt *imageList = NULL, *imageEl;
struct sqlResult *sr;
char query[256], **row;

if (acc[0] == 0)
    return NULL;
safef(query, sizeof(query),
    "select imageProbe.image from gene,probe,imageProbe"
    " where gene.%s = '%s'"
    " and gene.id = probe.gene"
    " and probe.id = imageProbe.probe",
    field, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    imageEl = slIntNew(sqlUnsigned(row[0]));
    slAddHead(&imageList, imageEl);
    }
sqlFreeResult(&sr);
slReverse(&imageList);
return imageList;
}

struct slInt *visiGeneSelectRefSeq(struct sqlConnection *conn, char *acc)
/* Return ids of images that have probes involving refSeq mRNA seq. */
{
return matchingExtId(conn, acc, "refSeq");
}

struct slInt *visiGeneSelectLocusLink(struct sqlConnection *conn, char *id)
/* Return ids of images that have probes involving locusLink (entrez gene)
 * id. */
{
return matchingExtId(conn, id, "locusLink");
}

struct slInt *visiGeneSelectGenbank(struct sqlConnection *conn, char *acc)
/* Return ids of images that have probes involving genbank accessioned 
 * sequence */
{
return matchingExtId(conn, acc, "genbank");
}

struct slInt *visiGeneSelectUniProt(struct sqlConnection *conn, char *acc)
/* Return ids of images that have probes involving UniProt accessioned
 * sequence. */
{
return matchingExtId(conn, acc, "uniProt");
}



