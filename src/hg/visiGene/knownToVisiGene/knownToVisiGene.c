/* knownToVisiGene - Create knownToVisiGene table by riffling through various other knownTo tables. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "hgRelate.h"
#include "visiGene.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "knownToVisiGene - Create knownToVisiGene table by riffling through various other knownTo tables\n"
  "usage:\n"
  "   knownToVisiGene database\n"
  "options:\n"
  "   -table=XXX - give another name to table other than knownToVisiGene\n"
  "   -visiDb=XXX - used a VisiGene database other than 'visiGene'\n"
  );
  /*
  "options:\n"
  "   -xxx=XXX\n"
  */
}

char *outTable = "knownToVisiGene";
char *visiDb = "visiGene";

static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {"visiDb", OPTION_STRING},
   {NULL, 0},
};

struct prioritizedImage 
    {
    int imageId;	/* ID of image */
    float priority;	/* Image priority - smaller is more urgent */
    };

void createTable(struct sqlConnection *conn, char *tableName)
/* Create our name/value table, dropping if it already exists. */
{
struct dyString *dy = dyStringNew(512);
dyStringPrintf(dy, 
"CREATE TABLE  %s (\n"
"    name varchar(255) not null,\n"
"    value varchar(255) not null,\n"
"              #Indices\n"
"    PRIMARY KEY(name(16)),\n"
"    INDEX(value(16))\n"
")\n",  tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

void addPrioritizedImage(struct hash *hash, int id, float priority, char *key)
/* Add image to hash, replacing what's already there if we have a better priority */
{
struct prioritizedImage *pi = hashFindVal(hash, key);
if (pi == NULL)
    {
    AllocVar(pi);
    pi->imageId = id;
    pi->priority = priority;
    hashAdd(hash, key, pi);
    }
else if (pi->priority > priority)
    {
    pi->imageId = id;
    pi->priority = priority;
    }
}

void foldIntoHash(struct sqlConnection *conn, char *table, char *keyField, char *valField,
	struct hash *hash, struct hash *uniqHash, boolean secondary)
/* Add key/value pairs from table into hash */
{
struct sqlResult *sr;
char query[256];
char **row;
safef(query, sizeof(query), "select %s,%s from %s", keyField, valField, table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (uniqHash != NULL)
        {
	if (hashLookup(uniqHash, row[1]))
	    {
	    if (secondary)
		continue;
	    }
	else
	    hashAdd(uniqHash, row[1], NULL);
	}
    hashAdd(hash, row[0], cloneString(row[1]));
    }
sqlFreeResult(&sr);
}

int bestImage(char *kgId, struct hash *kgToHash, struct hash *imageHash)
/* Return best image id if possible, otherwise 0 */
{
struct hashEl *extId = hashLookup(kgToHash, kgId);

while (extId != NULL)
    {
    struct prioritizedImage *pi = hashFindVal(imageHash, extId->val);
    if (pi != NULL)
	return pi->imageId;
    extId = hashLookupNext(extId);
    }
return 0;
}

void knownToVisiGene(char *database)
/* knownToVisiGene - Create knownToVisiGene table by riffling through various other knownTo tables. */
{
char *tempDir = ".";
FILE *f = hgCreateTabFile(tempDir, outTable);
struct sqlConnection *hConn = sqlConnect(database);
struct sqlConnection *iConn = sqlConnect(visiDb);
struct sqlResult *sr;
char query[512];
char **row;
struct hash *geneImageHash = newHash(18);
struct hash *locusLinkImageHash = newHash(18);
struct hash *refSeqImageHash = newHash(18);
struct hash *genbankImageHash = newHash(18);
struct hash *knownToLocusLinkHash = newHash(18);
struct hash *knownToRefSeqHash = newHash(18);
struct hash *knownToGeneHash = newHash(18);
struct hash *favorHugoHash = newHash(18);
struct slName *knownList = NULL, *known;
struct hash *dupeHash = newHash(17);

/* Go through and make up hashes of images keyed by various fields. */
sr = sqlGetResult(iConn,
        "select image.id,imageFile.priority,gene.name,gene.locusLink,gene.refSeq,gene.genbank,submissionSet.privateUser "
	"from image,imageFile,imageProbe,probe,gene,submissionSet "
	"where image.imageFile = imageFile.id "
	"and image.id = imageProbe.image "
	"and imageProbe.probe = probe.id "
	"and probe.gene = gene.id "
	"and image.submissionSet=submissionSet.id");

while ((row = sqlNextRow(sr)) != NULL)
    {
    int id = sqlUnsigned(row[0]);
    float priority = atof(row[1]);
    int privateUser = sqlSigned(row[6]);
    if (privateUser == 0)
	{
	addPrioritizedImage(geneImageHash, id, priority, row[2]);
	addPrioritizedImage(locusLinkImageHash, id, priority, row[3]);
	addPrioritizedImage(refSeqImageHash, id, priority, row[4]);
	addPrioritizedImage(genbankImageHash, id, priority, row[5]);
	}
    }
verbose(2, "Made hashes of image: geneImageHash %d, locusLinkImageHash %d, refSeqImageHash %d, genbankImageHash %d\n", geneImageHash->elCount, locusLinkImageHash->elCount, refSeqImageHash->elCount, genbankImageHash->elCount);
sqlFreeResult(&sr);

/* Build up list of known genes. */
sr = sqlGetResult(hConn, "select name from knownGene");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if (!hashLookup(dupeHash, name))
        {
	hashAdd(dupeHash, name, NULL);
	known = slNameNew(name);
	slAddHead(&knownList, known);
	}
    }
slReverse(&knownList);
sqlFreeResult(&sr);

/* Build up hashes from knownGene to other things. */
foldIntoHash(hConn, "knownToLocusLink", "name", "value", knownToLocusLinkHash, NULL, FALSE);
foldIntoHash(hConn, "knownToRefSeq", "name", "value", knownToRefSeqHash, NULL, FALSE);
foldIntoHash(hConn, "kgXref", "kgID", "geneSymbol", knownToGeneHash, favorHugoHash, FALSE);
foldIntoHash(hConn, "kgAlias", "kgID", "alias", knownToGeneHash, favorHugoHash, TRUE);
foldIntoHash(hConn, "kgProtAlias", "kgID", "alias", knownToGeneHash, favorHugoHash, TRUE);
verbose(2, "knownToLocusLink %d, knownToRefSeq %d, knownToGene %d\n", knownToLocusLinkHash->elCount, knownToRefSeqHash->elCount, knownToGeneHash->elCount);

/* Try and find an image for each gene. */
for (known = knownList; known != NULL; known = known->next)
    {
    char *name = known->name;
    int imageId = 0;

    imageId = bestImage(name, knownToLocusLinkHash, locusLinkImageHash);
    if (imageId == 0)
        imageId = bestImage(name, knownToRefSeqHash, refSeqImageHash);
    if (imageId == 0)
	{
	struct prioritizedImage *pi = hashFindVal(genbankImageHash, name);
	if (pi != NULL)
	    imageId = pi->imageId;
	}
    if (imageId == 0)
        imageId = bestImage(name, knownToGeneHash, geneImageHash);
    if (imageId != 0)
        {
	fprintf(f, "%s\t%d\n", name, imageId);
	}
    }

createTable(hConn, outTable);
hgLoadTabFile(hConn, tempDir, outTable, &f);
hgRemoveTabFile(tempDir, outTable);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
outTable = optionVal("table", outTable);
visiDb = optionVal("visiDb", visiDb);
if (argc != 2)
    usage();
knownToVisiGene(argv[1]);
return 0;
}
