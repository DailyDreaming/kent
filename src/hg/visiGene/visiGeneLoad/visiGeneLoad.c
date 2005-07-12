/* visiGeneLoad - Load data into visiGene database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"
#include "jksql.h"
#include "dystring.h"

/* Variables you can override from command line. */
char *database = "visiGene";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "visiGeneLoad - Load data into visiGene database\n"
  "usage:\n"
  "   visiGeneLoad setInfo.ra itemInfo.tab\n"
  "Please see visiGeneLoad.doc for description of the .ra and .tab files\n"
  "Options:\n"
  "   -database=%s - Specifically set database\n"
  , database
  );
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING,},
   {NULL, 0},
};

struct hash *hashRowOffsets(char *line)
/* Given a space-delimited line, create a hash keyed by the words in 
 * line with values the position of the word (0 based) in line */
{
struct hash *hash = hashNew(0);
char *word;
int wordIx = 0;
while ((word = nextWord(&line)) != 0)
    {
    hashAdd(hash, word, intToPt(wordIx));
    wordIx += 1;
    }
return hash;
}

char *getVal(char *fieldName, struct hash *raHash, struct hash *rowHash, char **row, char *defaultVal)
/* Return value in row if possible, else in ra, else in default.  
 * If no value and no default return an error. */
{
char *val = NULL;
struct hashEl *hel = hashLookup(rowHash, fieldName);
if (hel != NULL)
    {
    int rowIx = ptToInt(hel->val);
    val = trimSpaces(row[rowIx]);
    }
else
    {
    val = hashFindVal(raHash, fieldName);
    if (val == NULL)
	{
	if (defaultVal != NULL)
	    val = defaultVal;
	else
	    errAbort("Can't find value for field %s", fieldName);
	}
    }
return val;
}

static char *requiredItemFields[] = {"fileName", "submitId"};
static char *requiredSetFields[] = {"contributor", "submitSet"};
static char *requiredFields[] = {"fullDir", "screenDir", "thumbDir", "taxon", 
	"age", "probeColor", };
static char *optionalFields[] = {
    "abName", "abDescription", "abTaxon", "bodyPart", "copyright",
    "embedding", "fixation", "fPrimer", "genbank", "gene",
    "genotype", "imagePos", "itemUrl", "minAge", "maxAge",
    "journal", "journalUrl",
    "locusLink", "paneLabel", "permeablization", 
    "preparationNotes", "priority", "refSeq",
    "rPrimer", "sectionIx", "sectionSet", "sex", "sliceType", "specimenName",
    "specimenNotes", "strain", "publication", "pubUrl",
    "setUrl", "seq", "uniProt",
    };

char *hashValOrDefault(struct hash *hash, char *key, char *defaultVal)
/* Lookup key in hash and return value, or return default if it doesn't exist. */
{
char *val = hashFindVal(hash, key);
if (val == NULL)
    val = defaultVal;
return val;
}

int findExactSubmissionId(struct sqlConnection *conn,
	char *submitSetName,
	char *contributors)
/* Find ID of submissionSet that matches all parameters.  Return 0 if none found. */
{
char query[1024];
safef(query, sizeof(query),
      "select id from submissionSet "
      "where contributors = \"%s\" "
      "and name = \"%s\" "
      , contributors, submitSetName);
verbose(2, "query %s\n", query);
return sqlQuickNum(conn, query);
}

int findOrAddIdTable(struct sqlConnection *conn, char *table, char *field, 
	char *value)
/* Get ID associated with field.value in table.  */
{
char query[256];
int id;
char *escValue = makeEscapedString(value, '"');
safef(query, sizeof(query), "select id from %s where %s = \"%s\"",
	table, field, escValue);
id = sqlQuickNum(conn, query);
if (id == 0)
    {
    safef(query, sizeof(query), "insert into %s values(default, \"%s\")",
    	table, escValue);
    verbose(2, "%s\n", query);
    sqlUpdate(conn, query);
    id = sqlLastAutoId(conn);
    }
freeMem(escValue);
return id;
}

int doJournal(struct sqlConnection *conn, char *name, char *url)
/* Update journal table if need be.  Return journal ID. */
{
if (name[0] == 0)
    return 0;
else
    {
    struct dyString *dy = dyStringNew(0);
    int id = 0;

    dyStringPrintf(dy, "select id from journal where name = '%s'", name);
    id = sqlQuickNum(conn, dy->string);
    if (id == 0)
	{
	dyStringClear(dy);
	dyStringPrintf(dy, "insert into journal set");
	dyStringPrintf(dy, " id=default,\n");
	dyStringPrintf(dy, " name=\"%s\",\n", name);
	dyStringPrintf(dy, " url=\"%s\"\n", url);
	verbose(2, "%s\n", dy->string);
	sqlUpdate(conn, dy->string);
	id = sqlLastAutoId(conn);
	}
    dyStringFree(&dy);
    return id;
    }
}

int createSubmissionId(struct sqlConnection *conn,
	char *name,
	char *contributors, char *publication, 
	char *pubUrl, char *setUrl, char *itemUrl,
	char *journal, char *journalUrl, int copyright)
/* Add submission and contributors to database and return submission ID */
{
struct slName *slNameListFromString(char *s, char delimiter);
struct slName *contribList = NULL, *contrib;
int submissionSetId;
int journalId = doJournal(conn, journal, journalUrl);
struct dyString *dy = dyStringNew(0);
char query[1024];

dyStringPrintf(dy, "insert into submissionSet set");
dyStringPrintf(dy, " id = default,\n");
dyStringPrintf(dy, " name = \"%s\",\n", name);
dyStringPrintf(dy, " contributors = \"%s\",\n", contributors);
dyStringPrintf(dy, " publication = \"%s\",\n", publication);
dyStringPrintf(dy, " pubUrl = \"%s\",\n", pubUrl);
dyStringPrintf(dy, " journal = %d,\n", journalId);
dyStringPrintf(dy, " copyright = %d,\n", copyright);
dyStringPrintf(dy, " setUrl = \"%s\",\n", setUrl);
dyStringPrintf(dy, " itemUrl = \"%s\"\n", itemUrl);
verbose(2, "%s\n", dy->string);
sqlUpdate(conn, dy->string);
submissionSetId = sqlLastAutoId(conn);

contribList = slNameListFromComma(contributors);
for (contrib = contribList; contrib != NULL; contrib = contrib->next)
    {
    int contribId = findOrAddIdTable(conn, "contributor", "name", 
    	skipLeadingSpaces(contrib->name));
    safef(query, sizeof(query),
          "insert into submissionContributor values(%d, %d)",
	  submissionSetId, contribId);
    verbose(2, "%s\n", query);
    sqlUpdate(conn, query);
    }
slFreeList(&contribList);
dyStringFree(&dy);
return submissionSetId;
}

int saveSubmissionSet(struct sqlConnection *conn, struct hash *raHash)
/* Create submissionSet, submissionContributor, and contributor records. */
{
char *contributor = hashMustFindVal(raHash, "contributor");
char *name = hashMustFindVal(raHash, "submitSet");
char *publication = hashValOrDefault(raHash, "publication", "");
char *pubUrl = hashValOrDefault(raHash, "pubUrl", "");
char *setUrl = hashValOrDefault(raHash, "setUrl", "");
char *itemUrl = hashValOrDefault(raHash, "itemUrl", "");
char *journal = hashValOrDefault(raHash, "journal", "");
char *journalUrl = hashValOrDefault(raHash, "journalUrl", "");
char *copyright = hashFindVal(raHash, "copyright");
int copyrightId = 0;
int submissionId = findExactSubmissionId(conn, name, contributor);
if (copyright != NULL)
    copyrightId = findOrAddIdTable(conn, "copyright", "notice", copyright);

if (submissionId != 0)
     return submissionId;
else
     return createSubmissionId(conn, name, contributor, 
     	publication, pubUrl, setUrl, itemUrl, journal, journalUrl, copyrightId);
}

int cachedId(struct sqlConnection *conn, char *tableName, char *fieldName,
	struct hash *cache, char *raFieldName, struct hash *raHash, 
	struct hash *rowHash, char **row)
/* Get value for named field, and see if it exists in table.  If so
 * return associated id, otherwise create new table entry and return 
 * that id. */
{
char *value = getVal(raFieldName, raHash, rowHash, row, "");
if (value[0] == 0)
    return 0;
return findOrAddIdTable(conn, tableName, fieldName, value);
}


boolean optionallyAddOr(struct dyString *dy, char *field, char *val, 
	boolean needOr)
/* Add field = 'val', optionally preceded by or to dy. */
{
if (val[0] == 0)
    return needOr;
if (needOr)
    dyStringAppend(dy, " or ");
dyStringPrintf(dy, "%s = '%s'", field, val);
return TRUE;
}

int doSectionSet(struct sqlConnection *conn, struct hash *sectionSetHash, 
	char *sectionSet)
/* Update section set table if necessary.  Return section set id */
{
int id = 0;
if (sectionSet[0] != 0 && !sameString(sectionSet, "0"))
    {
    char query[256];
    id = atoi(sectionSet);
    safef(query, sizeof(query), "select id from sectionSet where id=%d", id);
    if (!sqlQuickNum(conn, query))
        {
	safef(query, sizeof(query), "insert into sectionSet values(%d)", id);
	sqlUpdate(conn, query);
	}
    }
return id;
}

int doGene(struct lineFile *lf, struct sqlConnection *conn,
	char *gene, char *locusLink, char *refSeq, 
	char *uniProt, char *genbank, char *taxon)
/* Update gene table if necessary, and return gene ID. */
{
struct dyString *dy = dyStringNew(0);
boolean needOr = FALSE;
int geneId = 0;
int bestScore = 0;
struct sqlResult *sr;
char **row;

verbose(3, "doGene %s %s %s %s %s %s\n", gene, locusLink, refSeq, uniProt, genbank, taxon);
/* Make sure that at least one field specifying gene is
 * specified. */
if (gene[0] == 0 && locusLink[0] == 0 && refSeq[0] == 0
    && uniProt[0] == 0 && genbank[0] == 0)
    errAbort("No gene, locusLink, refSeq, uniProt, or genbank "
	     " specified line %d of %s", lf->lineIx, lf->fileName);

/* Create query string that will catch relevant existing genes. */
dyStringPrintf(dy, "select id,name,locusLink,refSeq,genbank,uniProt from gene ");
dyStringPrintf(dy, "where taxon = %s and (",  taxon);
needOr = optionallyAddOr(dy, "name", gene, needOr);
needOr = optionallyAddOr(dy, "locusLink", locusLink, needOr);
needOr = optionallyAddOr(dy, "refSeq", refSeq, needOr);
needOr = optionallyAddOr(dy, "uniProt", uniProt, needOr);
needOr = optionallyAddOr(dy, "genbank", genbank, needOr);
verbose(2, "gene query: %s\n", dy->string);
dyStringPrintf(dy, ")");

/* Loop through query results finding ID that best matches us.
 * The locusLink/refSeq ID's are worth 8,  the genbank 4, the
 * uniProt 2, and the name 1.  This scheme will allow different
 * genes with the same name and even the same uniProt ID to 
 * cope with alternative splicing.  */
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int id = sqlUnsigned(row[0]);
    int score = 0;
    char *nameOne = row[1];
    char *locusLinkOne = row[2];
    char *refSeqOne = row[3];
    char *genbankOne = row[4];
    char *uniProtOne = row[5];

    verbose(3, "id %d, nameOne %s, locusLinkOne %s, refSeqOne %s, genbankOne %s, uniProtOne %s\n", id, nameOne, locusLinkOne, refSeqOne, genbankOne, uniProtOne);
    /* If disagree on locusLink, or refSeq ID, then this is not the
     * gene for us */
    verbose(3, "test locusLink '%s' & locusLinkOne '%s'\n", locusLink, locusLinkOne);
    if (locusLink[0] != 0 && locusLinkOne[0] != 0 
	&& differentString(locusLink, locusLinkOne))
	continue;
    verbose(3, "test refSeq '%s' & refSeqOne '%s'\n", refSeq, refSeqOne);
    if (refSeq[0] != 0 && refSeqOne[0] != 0 
	&& differentString(refSeq, refSeqOne))
	continue;
    verbose(3, "past continues\n");

    /* If we've agreed on locusLink or refSeq ID, then this is the
     * gene for us. */
    if (locusLink[0] != 0 || refSeq[0] != 0)
	score += 8;

    if (genbank[0] != 0 && genbankOne[0] != 0
	&& sameString(genbank, genbankOne))
	score += 4;
    if (uniProt[0] != 0 && uniProtOne[0] != 0
	&& sameString(uniProt, uniProtOne))
	score += 2;
    if (gene[0] != 0 && nameOne[0] != 0
	&& sameString(gene, nameOne))
	score += 1;
    verbose(3, "score %d, bestScore %d\n", score, bestScore);
    if (score > bestScore)
	{
	bestScore = score;
	geneId = id;
	}
    }
sqlFreeResult(&sr);

if (geneId == 0)
   {
   dyStringClear(dy);
   dyStringAppend(dy, "insert into gene set\n");
   dyStringPrintf(dy, " id = default,\n");
   dyStringPrintf(dy, " name = '%s',\n", gene);
   dyStringPrintf(dy, " locusLink = '%s',\n", locusLink);
   dyStringPrintf(dy, " refSeq = '%s',\n", refSeq);
   dyStringPrintf(dy, " genbank = '%s',\n", genbank);
   dyStringPrintf(dy, " uniProt = '%s',\n", uniProt);
   dyStringPrintf(dy, " taxon = %s\n", taxon);
   verbose(2, "%s\n", dy->string);
   sqlUpdate(conn, dy->string);
   geneId = sqlLastAutoId(conn);
   }
else
   {
   char *oldName;

   /* Handle updating name field - possibly creating a synonym. */
   dyStringClear(dy);
   dyStringPrintf(dy, "select name from gene where id = %d", geneId);
   oldName = sqlQuickString(conn, dy->string);
   if (gene[0] != 0)
       {
       if (oldName[0] == 0)
	   {
	   dyStringClear(dy);
	   dyStringPrintf(dy, "update gene set" );
	   dyStringPrintf(dy, " name = '%s'", gene);
	   dyStringPrintf(dy, " where id = %d", geneId);
	   verbose(2, "%s\n", dy->string);
	   sqlUpdate(conn, dy->string);
	   }
       else if (differentString(oldName, gene))
	   {
	   dyStringClear(dy);
	   dyStringAppend(dy, "select count(*) from geneSynonym where ");
	   dyStringPrintf(dy, "gene = %d and name = '%s'", geneId, gene);
	   if (sqlQuickNum(conn, dy->string) == 0)
	       {
	       dyStringClear(dy);
	       dyStringPrintf(dy, "insert into geneSynonym "
			     "values(%d,\"%s\")", geneId, gene);
	       verbose(2, "%s\n", dy->string);
	       sqlUpdate(conn, dy->string);
	       }
	   }
       }

   /* Update other fields. */
   dyStringClear(dy);
   dyStringAppend(dy, "update gene set\n");
   if (locusLink[0] != 0)
       dyStringPrintf(dy, " locusLink = '%s',", locusLink);
   if (refSeq[0] != 0)
       dyStringPrintf(dy, " refSeq = '%s',", refSeq);
   if (genbank[0] != 0)
       dyStringPrintf(dy, " genbank = '%s',", genbank);
   if (uniProt[0] != 0)
       dyStringPrintf(dy, " uniProt = '%s',", uniProt);
   dyStringPrintf(dy, " taxon = %s", taxon);
   dyStringPrintf(dy, " where id = %d", geneId);
   verbose(2, "%s\n", dy->string);
   sqlUpdate(conn, dy->string);
   geneId = geneId;
   freez(&oldName);
   }
dyStringFree(&dy);
return geneId;
}

int doAntibody(struct sqlConnection *conn, char *abName, char *abDescription,
	char *abTaxon)
/* Update antibody table if necessary and return antibody ID. */
{
int antibodyId = 0;

if (abName[0] != 0)
    {
    int bestScore = 0;
    struct sqlResult *sr;
    char **row;
    struct dyString *dy = dyStringNew(0);

    /* Try to hook up with existing antibody record. */
    dyStringAppend(dy, "select id,name,description,taxon from antibody");
    dyStringPrintf(dy, " where name = '%s'", abName);
    sr = sqlGetResult(conn, dy->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	int id = sqlUnsigned(row[0]);
	char *name = row[1];
	char *description = row[2];
	char *taxon = row[3];
	int score = 1;
	if (abTaxon[0] != '0' && taxon[0] != '0')
	    {
	    if (differentString(taxon, abTaxon))
		continue;
	    else
		score += 2;
	    }
	if (description[0] != 0 && abDescription[0] != 0)
	    {
	    if (differentString(description, abDescription))
		continue;
	    else
		score += 4;
	    }
	if (score > bestScore)
	    {
	    bestScore = score;
	    antibodyId = id;
	    }
	}
    sqlFreeResult(&sr);

    if (antibodyId == 0)
	{
	dyStringClear(dy);
	dyStringAppend(dy, "insert into antibody set");
	dyStringPrintf(dy, " id = default,\n");
	dyStringPrintf(dy, " name = '%s',\n", abName);
	dyStringPrintf(dy, " description = \"%s\",\n", abDescription);
	if (abTaxon[0] == 0)
	    abTaxon = "0";
	dyStringPrintf(dy, " taxon = %s", abTaxon);
	verbose(2, "%s\n", dy->string);
	sqlUpdate(conn, dy->string);
        antibodyId = sqlLastAutoId(conn);
	}
    else
	{
	if (abDescription[0] != 0)
	    {
	    dyStringClear(dy);
	    dyStringPrintf(dy, "update antibody set description = \"%s\"",
		    abDescription);
	    dyStringPrintf(dy, " where id = %d", antibodyId);
	    verbose(2, "%s\n", dy->string);
	    sqlUpdate(conn, dy->string);
	    }
	if (abTaxon[0] != 0)
	    {
	    dyStringClear(dy);
	    dyStringPrintf(dy, "update antibody set taxon = %s", abTaxon);
	    dyStringPrintf(dy, " where id = %d", antibodyId);
	    verbose(2, "%s\n", dy->string);
	    sqlUpdate(conn, dy->string);
	    }
	}
    dyStringFree(&dy);
    }
return antibodyId;
}

int doProbe(struct lineFile *lf, struct sqlConnection *conn, 
	int geneId, int antibodyId,
	char *fPrimer, char *rPrimer, char *seq)
/* Update probe table and probeType table if need be and return probe ID. */
{
struct sqlResult *sr;
char **row;
int probeTypeId = 0;
int probeId = 0;
int bestScore = 0;
struct dyString *dy = dyStringNew(0);
char *probeType = "RNA";
boolean gotPrimers = (rPrimer[0] != 0 && fPrimer[0] != 0);

verbose(3, "doProbe %d %d %s %s %s\n", geneId, antibodyId, fPrimer, rPrimer, seq);
touppers(fPrimer);
touppers(rPrimer);
touppers(seq);
if (gotPrimers || seq[0] != 0)
    {
    if (antibodyId != 0)
        errAbort("Probe defined by antibody and RNA line %d of %s",
		lf->lineIx, lf->fileName);
    }
else if (antibodyId)
    {
    probeType = "antibody";
    }

/* Handle probe type */
dyStringPrintf(dy, "select id from probeType where name = '%s'", probeType);
probeTypeId = sqlQuickNum(conn, dy->string);
if (probeTypeId == 0)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "insert into probeType values(default, '%s')", 
    	probeType);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    probeTypeId  = sqlLastAutoId(conn);
    }

dyStringClear(dy);
dyStringAppend(dy, "select id,gene,antibody,fPrimer,rPrimer,seq ");
dyStringAppend(dy, "from probe ");
dyStringPrintf(dy, "where gene=%d and antibody=%d", geneId, antibodyId);
verbose(2, "query: %s\n", dy->string);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int idOne = sqlUnsigned(row[0]);
    int geneOne = sqlUnsigned(row[1]);
    int antibodyOne = sqlUnsigned(row[2]);
    char *fPrimerOne = row[3];
    char *rPrimerOne = row[4];
    char *seqOne = row[5];
    int score = 1;
    verbose(3, "idOne %d, geneOne %d, antibodyOne %d, fPrimerOne %s, rPrimerOne %s\n", idOne, geneOne, antibodyOne, fPrimerOne, rPrimerOne);
    if (antibodyId != 0)
        {
	if (antibodyOne == antibodyId)
	    score += 10;
	else
	    continue;
	}
    else
        {
	if (antibodyOne != 0)
	    continue;
	}
    if (gotPrimers && fPrimerOne[0] != 0 && rPrimerOne[0] != 0)
        {
	if (sameString(fPrimerOne, fPrimer) && 
	    sameString(rPrimerOne, rPrimer))
	    score += 2;
	else
	    continue;
	}
    if (seq[0] != 0  && seqOne[0] != 0)
        {
	if (sameString(seq, seqOne))
	    score += 4;
	else
	    continue;
	}
    verbose(3, "score = %d, bestScore %d\n", score, bestScore);
    if (score > bestScore)
	{
        probeId = idOne;
	bestScore = score;
	}
    }
sqlFreeResult(&sr);


if (probeId == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into probe set");
    dyStringPrintf(dy, " id=default,\n");
    dyStringPrintf(dy, " gene=%d,\n", geneId);
    dyStringPrintf(dy, " antibody=%d,\n", antibodyId);
    dyStringPrintf(dy, " probeType=%d,\n", probeTypeId);
    dyStringPrintf(dy, " fPrimer='%s',\n", fPrimer);
    dyStringPrintf(dy, " rPrimer='%s',\n", rPrimer);
    dyStringPrintf(dy, " seq='%s'\n", seq);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    probeId = sqlLastAutoId(conn);
    }
else
    {
    if (antibodyId != 0)
	{
	dyStringClear(dy);
	dyStringPrintf(dy, "update probe set antibody=%d", antibodyId);
	dyStringPrintf(dy, " where id=%d", probeId);
	verbose(2, "%s\n", dy->string);
	sqlUpdate(conn, dy->string);
	}
    if (gotPrimers)
	{
	dyStringClear(dy);
	dyStringAppend(dy, "update probe set ");
	dyStringPrintf(dy, "fPrimer = '%s', ", fPrimer);
	dyStringPrintf(dy, "rPrimer = '%s'", rPrimer);
	dyStringPrintf(dy, " where id=%d", probeId);
	verbose(2, "%s\n", dy->string);
	sqlUpdate(conn, dy->string);
	}
    if (seq[0] != 0)
        {
	dyStringClear(dy);
	dyStringAppend(dy, "update probe set seq = '");
	dyStringAppend(dy, seq);
	dyStringAppend(dy, "'");
	dyStringPrintf(dy, " where id=%d", probeId);
	verbose(2, "%s\n", dy->string);
	sqlUpdate(conn, dy->string);
	}
    }

dyStringFree(&dy);
return probeId;
}

int doImageFile(struct lineFile *lf, struct sqlConnection *conn, 
	char *fileName, int fullDir, int screenDir, int thumbDir,
	int submissionSetId, char *submitId, char *priority)
/* Update image file record if necessary and return image file ID. */
{
int imageFileId = 0;
struct dyString *dy = newDyString(0);

dyStringAppend(dy, "select id from imageFile where ");
dyStringPrintf(dy, "fileName='%s' and fullLocation=%d", 
	fileName, fullDir);
imageFileId = sqlQuickNum(conn, dy->string);
if (imageFileId == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into imageFile set");
    dyStringPrintf(dy, " id = default,\n");
    dyStringPrintf(dy, " fileName = '%s',\n", fileName);
    dyStringPrintf(dy, " priority = %s,\n", priority);
    dyStringPrintf(dy, " fullLocation = %d,\n", fullDir);
    dyStringPrintf(dy, " screenLocation = %d,\n", screenDir);
    dyStringPrintf(dy, " thumbLocation = %d,\n", thumbDir);
    dyStringPrintf(dy, " submissionSet = %d,\n", submissionSetId);
    dyStringPrintf(dy, " submitId = '%s'\n", submitId);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    imageFileId = sqlLastAutoId(conn);
    }
dyStringFree(&dy);
return imageFileId;
}

int doStrain(struct lineFile *lf, struct sqlConnection *conn, char *taxon,
	char *strain)
/* Return strain id, creating a new table entry if necessary. */
{
int id = 0;
struct dyString *dy = newDyString(0);

dyStringAppend(dy, "select id from strain where ");
dyStringPrintf(dy, "taxon=%s and name='%s'", taxon, strain);
id = sqlQuickNum(conn, dy->string);
if (id == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into strain set");
    dyStringPrintf(dy, " id = default,\n");
    dyStringPrintf(dy, " taxon = %s,\n", taxon);
    dyStringPrintf(dy, " name = '%s'", strain);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    id = sqlLastAutoId(conn);
    }
dyStringFree(&dy);
return id;
}

int doGenotype(struct lineFile *lf, struct sqlConnection *conn,
	char *taxon, int strainId, char *genotype)
/* Unpack genotype string, alphabatize it, return id associated
 * with it if possible, otherwise create associated genes and
 * alleles then genotype, and return genotypeId. */
{
int id = 0;
struct dyString *dy = newDyString(0);
struct slName *geneAlleleList = commaSepToSlNames(genotype), *el;
struct dyString *alphabetical = newDyString(0);

slSort(&geneAlleleList, slNameCmp);
for (el = geneAlleleList; el != NULL; el = el->next)
    dyStringPrintf(alphabetical, "%s,", el->name);

dyStringAppend(dy, "select id from genotype where ");
dyStringPrintf(dy, "taxon=%s and strain=%d and alleles='%s'", 
	taxon, strainId, alphabetical->string);
id = sqlQuickNum(conn, dy->string);
if (id == 0)
    {
    /* Create main genotype record. */
    dyStringClear(dy);
    dyStringAppend(dy, "insert into genotype set");
    dyStringPrintf(dy, " id = default,\n");
    dyStringPrintf(dy, " taxon = %s,\n", taxon);
    dyStringPrintf(dy, " strain = %d,\n", strainId);
    dyStringPrintf(dy, " alleles = \"%s\"", alphabetical->string);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    id = sqlLastAutoId(conn);

    /* Create additional records for each component of genotype. */
    if (!sameString(genotype, "wild type"))
	{
	for (el = geneAlleleList; el != NULL; el = el->next)
	    {
	    int geneId = 0, alleleId = 0;
	    char *gene, *allele;

	    /* Parse gene:allele */
	    gene = el->name;
	    allele = strchr(gene, ':');
	    if (allele == NULL)
		errAbort("Malformed genotype %s (missing :)", genotype);
	    *allele++ = 0;

	    /* Get or make gene ID. */
	    dyStringClear(dy);
	    dyStringPrintf(dy, "select id from gene where ");
	    dyStringPrintf(dy, "name = \"%s\" and taxon=%s", gene, taxon);
	    geneId = sqlQuickNum(conn, dy->string);
	    if (geneId == 0)
	        {
		dyStringClear(dy);
		dyStringAppend(dy, "insert into gene set");
		dyStringPrintf(dy, " id = default,\n");
		dyStringPrintf(dy, " name = \"%s\",\n", gene);
		dyStringPrintf(dy, " locusLink = '',\n");
		dyStringPrintf(dy, " refSeq = '',\n");
		dyStringPrintf(dy, " genbank = '',\n");
		dyStringPrintf(dy, " uniProt = '',\n");
		dyStringPrintf(dy, " taxon = %s", taxon);
		verbose(2, "%s\n", dy->string);
		sqlUpdate(conn, dy->string);
		geneId = sqlLastAutoId(conn);
		}

	    /* Get or make allele ID. */
	    dyStringClear(dy);
	    dyStringPrintf(dy, "select id from allele where ");
	    dyStringPrintf(dy, "gene = %d and name = \"%s\"", geneId, allele);
	    alleleId = sqlQuickNum(conn, dy->string);
	    if (alleleId == 0)
	        {
		dyStringClear(dy);
		dyStringAppend(dy, "insert into allele set");
		dyStringPrintf(dy, " id = default,\n");
		dyStringPrintf(dy, " gene = %d,\n", geneId);
		dyStringPrintf(dy, " name = \"%s\"", allele);
		verbose(2, "%s\n", dy->string);
		sqlUpdate(conn, dy->string);
		alleleId = sqlLastAutoId(conn);
		}

	    /* Add genotypeAllele record. */
	    dyStringClear(dy);
	    dyStringAppend(dy, "insert into genotypeAllele set ");
	    dyStringPrintf(dy, "genotype = %d, allele=%d\n", id, alleleId);
	    verbose(2, "%s\n", dy->string);
	    sqlUpdate(conn, dy->string);
	    }
	}
    }
slFreeList(&geneAlleleList);
dyStringFree(&alphabetical);
dyStringFree(&dy);
return id;
}

int doSpecimen(struct lineFile *lf, struct sqlConnection *conn,
	char *name, char *taxon, int genotype, int bodyPart, int sex,
	char *age, char *minAge, char *maxAge, char *notes)
/* Add specimen to table if it doesn't already exist. */
{
int id = 0;
struct dyString *dy = newDyString(0);

if (minAge[0] == 0) minAge = age;
if (maxAge[0] == 0) maxAge = age;
dyStringAppend(dy, "select id from specimen where ");
dyStringPrintf(dy, "name = \"%s\" and ", name);
dyStringPrintf(dy, "taxon = %s and ", taxon);
dyStringPrintf(dy, "genotype = %d and ", genotype);
dyStringPrintf(dy, "bodyPart = %d and ", bodyPart);
dyStringPrintf(dy, "age = %s and ", age);
dyStringPrintf(dy, "minAge = %s and ", minAge);
dyStringPrintf(dy, "maxAge = %s and ", maxAge);
dyStringPrintf(dy, "notes = \"%s\"", notes);
id = sqlQuickNum(conn, dy->string);
if (id == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into specimen set");
    dyStringPrintf(dy, " id = default,\n");
    dyStringPrintf(dy, "name = \"%s\",\n", name);
    dyStringPrintf(dy, "taxon = %s,\n", taxon);
    dyStringPrintf(dy, "genotype = %d,\n", genotype);
    dyStringPrintf(dy, "bodyPart = %d,\n", bodyPart);
    dyStringPrintf(dy, "age = %s,\n", age);
    dyStringPrintf(dy, "minAge = %s,\n", minAge);
    dyStringPrintf(dy, "maxAge = %s,\n", maxAge);
    dyStringPrintf(dy, "notes = \"%s\"", notes);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    id = sqlLastAutoId(conn);
    }
dyStringFree(&dy);
return id;
}
	
int doPreparation(struct lineFile *lf, struct sqlConnection *conn, 
	int fixation, int embedding, int permeablization, 
	int sliceType, char *notes)
/* Add preparation to table if it doesn't already exist. */
{
int id = 0;
struct dyString *dy = newDyString(0);

dyStringAppend(dy, "select id from preparation where ");
dyStringPrintf(dy, "fixation = %d and ", fixation);
dyStringPrintf(dy, "embedding = %d and ", embedding);
dyStringPrintf(dy, "permeablization = %d and ", permeablization);
dyStringPrintf(dy, "sliceType = %d and ", sliceType);
dyStringPrintf(dy, "notes = \"%s\"", notes);
id = sqlQuickNum(conn, dy->string);
if (id == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into preparation set");
    dyStringPrintf(dy, " id = default,\n");
    dyStringPrintf(dy, " fixation = %d,\n", fixation);
    dyStringPrintf(dy, " embedding = %d,\n", embedding);
    dyStringPrintf(dy, " permeablization = %d,\n", permeablization);
    dyStringPrintf(dy, " sliceType = %d,\n", sliceType);
    dyStringPrintf(dy, " notes = '%s'", notes);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    id = sqlLastAutoId(conn);
    }
dyStringFree(&dy);
return id;
}

int doImage(struct lineFile *lf, struct sqlConnection *conn,
	int submissionSet, int imageFile, int imagePos, char *paneLabel,
	int sectionSet, int sectionIx, int specimen, int preparation)
/* Update image table. */
{
int id = 0;
struct dyString *dy = newDyString(0);

dyStringAppend(dy, "select id from image where ");
dyStringPrintf(dy, "submissionSet = %d and ", submissionSet);
dyStringPrintf(dy, "imageFile = %d and ", imageFile);
dyStringPrintf(dy, "imagePos = %d and ", imagePos);
dyStringPrintf(dy, "paneLabel = \"%s\" and ", paneLabel);
dyStringPrintf(dy, "sectionSet = %d and ", sectionSet);
dyStringPrintf(dy, "sectionIx = %d and ", sectionIx);
dyStringPrintf(dy, "specimen = %d and ", specimen);
dyStringPrintf(dy, "preparation = %d", preparation);
id = sqlQuickNum(conn, dy->string);
if (id == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into image set ");
    dyStringPrintf(dy, " id=default,");
    dyStringPrintf(dy, "submissionSet = %d,", submissionSet);
    dyStringPrintf(dy, "imageFile = %d,", imageFile);
    dyStringPrintf(dy, "imagePos = %d,", imagePos);
    dyStringPrintf(dy, "paneLabel = \"%s\",", paneLabel);
    dyStringPrintf(dy, "sectionSet = %d,", sectionSet);
    dyStringPrintf(dy, "sectionIx = %d,", sectionIx);
    dyStringPrintf(dy, "specimen = %d,", specimen);
    dyStringPrintf(dy, "preparation = %d", preparation);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    id = sqlLastAutoId(conn);
    }
dyStringFree(&dy);
return id;
}


int doImageProbe(struct sqlConnection *conn,
	int image, int probe, int probeColor)
/* Update image probe table if need be */
{
struct dyString *dy = dyStringNew(0);
int id = 0;
dyStringAppend(dy, "select id from imageProbe where ");
dyStringPrintf(dy, "image=%d and probe=%d and probeColor=%d",
    image, probe, probeColor);
id = sqlQuickNum(conn, dy->string);
if (id == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into imageProbe set");
    dyStringPrintf(dy, " id=default,");
    dyStringPrintf(dy, " image=%d,", image);
    dyStringPrintf(dy, " probe=%d,", probe);
    dyStringPrintf(dy, " probeColor=%d\n", probeColor);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    id = sqlLastAutoId(conn);
    }
dyStringFree(&dy);
return id;
}

void doExpression(struct lineFile *lf, struct sqlConnection *conn, 
	int imageProbe, char *bodyPart, char *level)
/* Add item to expression table, possibly body part table too. */
{
struct dyString *dy = dyStringNew(0);
int bodyPartId = findOrAddIdTable(conn, "bodyPart", "name", bodyPart);
double lev = atof(level);
if (lev < 0 || lev > 1.0)
    errAbort("expression level out of range (0 to 1) line %d of %s", 
    	lf->lineIx, lf->fileName);
dyStringAppend(dy, "select count(*) from expressionLevel where ");
dyStringPrintf(dy, "imageProbe=%d and bodyPart=%d", imageProbe, bodyPartId);
if (sqlQuickNum(conn, dy->string) == 0)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "insert into expressionLevel set");
    dyStringPrintf(dy, " imageProbe=%d,", imageProbe);
    dyStringPrintf(dy, " bodyPart=%d,", bodyPartId);
    dyStringPrintf(dy, " level=%f", lev);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);
    }
dyStringFree(&dy);
}


void visiGeneLoad(char *setRaFile, char *itemTabFile)
/* visiGeneLoad - Load data into visiGene database. */
{
struct hash *raHash = raReadSingle(setRaFile);
struct hash *rowHash;
struct lineFile *lf = lineFileOpen(itemTabFile, TRUE);
char *line, *words[256];
struct sqlConnection *conn = sqlConnect(database);
int rowSize;
int submissionSetId;
struct hash *fullDirHash = newHash(0);
struct hash *screenDirHash = newHash(0);
struct hash *thumbDirHash = newHash(0);
struct hash *bodyPartHash = newHash(0);
struct hash *sexHash = newHash(0);
struct hash *copyrightHash = newHash(0);
struct hash *embeddingHash = newHash(0);
struct hash *fixationHash = newHash(0);
struct hash *permeablizationHash = newHash(0);
struct hash *probeColorHash = newHash(0);
struct hash *sliceTypeHash = newHash(0);
struct hash *sectionSetHash = newHash(0);
struct dyString *dy = dyStringNew(0);
int imageProbeId = 0;

/* Read first line of tab file, and from it get all the field names. */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s appears to be empty", lf->fileName);
if (line[0] != '#')
    errAbort("First line of %s needs to start with #, and then contain field names",
    	lf->fileName);
rowHash = hashRowOffsets(line+1);
rowSize = rowHash->elCount;
if (rowSize >= ArraySize(words))
    errAbort("Too many fields in %s", lf->fileName);

/* Check that have all required fields */
    {
    char *fieldName;
    int i;

    for (i=0; i<ArraySize(requiredSetFields); ++i)
        {
	fieldName = requiredSetFields[i];
	if (!hashLookup(raHash, fieldName))
	    errAbort("Field %s is not in %s", fieldName, setRaFile);
	}

    for (i=0; i<ArraySize(requiredItemFields); ++i)
        {
	fieldName = requiredItemFields[i];
	if (!hashLookup(rowHash, fieldName))
	    errAbort("Field %s is not in %s", fieldName, itemTabFile);
	}

    for (i=0; i<ArraySize(requiredFields); ++i)
        {
	fieldName = requiredFields[i];
	if (!hashLookup(rowHash, fieldName) && !hashLookup(raHash, fieldName))
	    errAbort("Field %s is not in %s or %s", fieldName, setRaFile, itemTabFile);
	}
    }

/* Create/find submission record. */
submissionSetId = saveSubmissionSet(conn, raHash);

/* Process rest of tab file. */
while (lineFileNextReal(lf, &line))
    {
    int wordCount;
    if (isspace(line[0])) /* Add to info on previous line. */
        {
	char *command;
	if (imageProbeId == 0)
	    errAbort("Can't begin %s with an indented line", lf->fileName);
	line = skipLeadingSpaces(line);
	wordCount = chopTabs(line, words);
	command = words[0];
	if (sameString(command, "expression"))
	    {
	    lineFileExpectWords(lf, 3, wordCount);
	    doExpression(lf, conn, imageProbeId, words[1], words[2]);
	    }
	else
	    {
	    errAbort("Unknown command %s line %d of %s", 
	    	command, lf->lineIx, lf->fileName);
	    }
	}
    else	/* Fresh imageProbe. */
        {
	wordCount = chopTabs(line, words);
	lineFileExpectWords(lf, rowSize, wordCount);
	/* Find/add fields that are in simple id/name type tables. */
	int fullDir = cachedId(conn, "fileLocation", "name", 
	    fullDirHash, "fullDir", raHash, rowHash, words);
	int screenDir = cachedId(conn, "fileLocation", "name", 
	    screenDirHash, "screenDir", raHash, rowHash, words);
	int thumbDir = cachedId(conn, "fileLocation", 
	    "name", thumbDirHash, "thumbDir", raHash, rowHash, words);
	int bodyPart = cachedId(conn, "bodyPart", 
	    "name", bodyPartHash, "bodyPart", raHash, rowHash, words);
	int embedding = cachedId(conn, "embedding", "description",
	    embeddingHash, "embedding", raHash, rowHash, words);
	int fixation = cachedId(conn, "fixation", "description",
	    fixationHash, "fixation", raHash, rowHash, words);
	int permeablization = cachedId(conn, "permeablization", "description",
	    permeablizationHash, "permeablization", raHash, rowHash, words);
	int probeColor = cachedId(conn, "probeColor", 
	    "name", probeColorHash, "probeColor", raHash, rowHash, words);
	int sex = cachedId(conn, "sex", 
	    "name", sexHash, "sex", raHash, rowHash, words);
	int sliceType = cachedId(conn, "sliceType", 
	    "name", sliceTypeHash, "sliceType", raHash, rowHash, words);
	
	/* Get required fields in tab file */
	char *fileName = getVal("fileName", raHash, rowHash, words, NULL);
	char *submitId = getVal("submitId", raHash, rowHash, words, NULL);

	/* Get required fields that can live in tab or .ra file. */
	char *taxon = getVal("taxon", raHash, rowHash, words, NULL);
	char *age = getVal("age", raHash, rowHash, words, NULL);

	/* Get other fields from input (either tab or ra file) */
	char *abName = getVal("abName", raHash, rowHash, words, "");
	char *abDescription = getVal("abDescription", raHash, rowHash, words, "");
	char *abTaxon = getVal("abTaxon", raHash, rowHash, words, "0");
	char *fPrimer = getVal("fPrimer", raHash, rowHash, words, "");
	char *genbank = getVal("genbank", raHash, rowHash, words, "");
	char *gene = getVal("gene", raHash, rowHash, words, "");
	char *genotype = getVal("genotype", raHash, rowHash, words, "wild type");
	char *imagePos = getVal("imagePos", raHash, rowHash, words, "0");
	char *journal = getVal("journal", raHash, rowHash, words, "");
	char *journalUrl = getVal("journalUrl", raHash, rowHash, words, "");
	char *locusLink = getVal("locusLink", raHash, rowHash, words, "");
	char *minAge = getVal("minAge", raHash, rowHash, words, "");
	char *maxAge = getVal("maxAge", raHash, rowHash, words, "");
	char *paneLabel = getVal("paneLabel", raHash, rowHash, words, "");
	char *preparationNotes = getVal("preparationNotes", raHash, rowHash, words, "");
	char *priority = getVal("priority", raHash, rowHash, words, "200");
	char *refSeq = getVal("refSeq", raHash, rowHash, words, "");
	char *rPrimer = getVal("rPrimer", raHash, rowHash, words, "");
	char *sectionSet = getVal("sectionSet", raHash, rowHash, words, "");
	char *sectionIx = getVal("sectionIx", raHash, rowHash, words, "0");
	char *seq = getVal("seq", raHash, rowHash, words, "");
	char *specimenName = getVal("specimenName", raHash, rowHash, words, "");
	char *specimenNotes = getVal("specimenNotes", raHash, rowHash, words, "");
	char *strain = getVal("strain", raHash, rowHash, words, "");
	char *uniProt = getVal("uniProt", raHash, rowHash, words, "");

	/* Some ID's we have to calculate individually. */
	int sectionId = 0;
	int geneId = 0;
	int antibodyId = 0;
	int probeId = 0;
	int imageFileId = 0;
	int strainId = 0;
	int genotypeId = 0;
	int specimenId = 0;
	int preparationId = 0;
	int imageId = 0;

	verbose(3, "line %d of %s: gene %s, fileName %s\n", lf->lineIx, lf->fileName, gene, fileName);
	sectionId = doSectionSet(conn, sectionSetHash, sectionSet);
	geneId = doGene(lf, conn, gene, locusLink, refSeq, uniProt, genbank, taxon);
	antibodyId = doAntibody(conn, abName, abDescription, abTaxon);
	probeId = doProbe(lf, conn, geneId, antibodyId, fPrimer, rPrimer, seq);
	imageFileId = doImageFile(lf, conn, fileName, fullDir, screenDir, thumbDir,
	    submissionSetId, submitId, priority);
	strainId = doStrain(lf, conn, taxon, strain);
	genotypeId = doGenotype(lf, conn, taxon, strainId, genotype);
	specimenId = doSpecimen(lf, conn, specimenName, taxon, 
	    genotypeId, bodyPart, sex, 
	    age, minAge, maxAge, specimenNotes);
	preparationId = doPreparation(lf, conn, fixation, embedding, 
	    permeablization, sliceType, preparationNotes);
	imageId = doImage(lf, conn, submissionSetId, imageFileId, 
	    atoi(imagePos), paneLabel,
	    sectionId, atoi(sectionIx), specimenId, preparationId);
	imageProbeId = doImageProbe(conn, imageId, probeId, probeColor);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
database = optionVal("database", database);
visiGeneLoad(argv[1], argv[2]);
return 0;
}
