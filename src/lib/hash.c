/* Hash.c - implements hashing. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "localmem.h"
#include "hash.h"
#include "obscure.h"

static char const rcsid[] = "$Id: hash.c,v 1.19 2004/03/03 18:14:03 sugnet Exp $";

bits32 hashCrc(char *string)
/* Returns a CRC value on string. */
{
unsigned char *us = (unsigned char *)string;
unsigned char c;
bits32 shiftAcc = 0;
bits32 addAcc = 0;

while ((c = *us++) != 0)
    {
    shiftAcc <<= 2;
    shiftAcc += c;
    addAcc += c;
    }
return shiftAcc + addAcc;
}

struct hashEl *hashLookup(struct hash *hash, char *name)
/* Looks for name in hash table. Returns associated element,
 * if found, or NULL if not. */
{
struct hashEl *el = hash->table[hashCrc(name)&hash->mask];
while (el != NULL)
    {
    if (strcmp(el->name, name) == 0)
        break;
    el = el->next;
    }
return el;
}

struct hashEl *hashLookupNext(struct hashEl *hashEl)
/* Find the next occurance of name that may occur in the table multiple times,
 * or NULL if not found. */
{
struct hashEl *el = hashEl->next;
while (el != NULL)
    {
    if (strcmp(el->name, hashEl->name) == 0)
        break;
    el = el->next;
    }
return el;
}

struct hashEl *hashAddN(struct hash *hash, char *name, int nameSize, void *val)
/* Add name of given size to hash (no need to be zero terminated) */
{
struct hashEl *el = lmAlloc(hash->lm, sizeof(*el));
int hashVal = (hashCrc(name)&hash->mask);
el->name = lmAlloc(hash->lm, nameSize+1);
memcpy(el->name, name, nameSize);
el->val = val;
el->next = hash->table[hashVal];
hash->table[hashVal] = el;
return el;
}

struct hashEl *hashAdd(struct hash *hash, char *name, void *val)
/* Add new element to hash table. */
{
return hashAddN(hash, name, strlen(name), val);
}

void *hashRemove(struct hash *hash, char *name)
/* Remove item of the given name from hash table. 
 * Returns value of removed item. */
{
struct hashEl *hel;
void *ret;
struct hashEl **pBucket = &hash->table[hashCrc(name)&hash->mask];
for (hel = *pBucket; hel != NULL; hel = hel->next)
    if (sameString(hel->name, name))
        break;
if (hel == NULL)
    {
    warn("Trying to remove non-existant %s from hash", name);
    return NULL;
    }
ret = hel->val;
slRemoveEl(pBucket, hel);
return ret;
}

struct hashEl *hashAddUnique(struct hash *hash, char *name, void *val)
/* Add new element to hash table. Squawk and die if not unique */
{
if (hashLookup(hash, name) != NULL)
    errAbort("%s duplicated, aborting", name);
return hashAdd(hash, name, val);
}

struct hashEl *hashAddSaveName(struct hash *hash, char *name, void *val, char **saveName)
/* Add new element to hash table.  Save the name of the element, which is now
 * allocated in the hash table, to *saveName.  A typical usage would be:
 *    AllocVar(el);
 *    hashAddSaveName(hash, name, el, &el->name);
 */
{
struct hashEl *hel = hashAdd(hash, name, val);
*saveName = hel->name;
return hel;
}

struct hashEl *hashStore(struct hash *hash, char *name)
/* If element in hash already return it, otherwise add it
 * and return it. */
{
struct hashEl *hel;
if ((hel = hashLookup(hash, name)) != NULL)
    return hel;
return hashAdd(hash, name, NULL);
}

char  *hashStoreName(struct hash *hash, char *name)
/* If element in hash already return it, otherwise add it
 * and return it. */
{
struct hashEl *hel;
if (name == NULL)
    return NULL;
if ((hel = hashLookup(hash, name)) != NULL)
    return hel->name;
return hashAdd(hash, name, NULL)->name;
}

int hashIntVal(struct hash *hash, char *name)
/* Find size of name in hash or die trying. */
{
void *val = hashMustFindVal(hash, name);
return ptToInt(val);
}

int hashIntValDefault(struct hash *hash, char *name, int defaultInt)
/* Return integer value associated with name in a simple 
 * hash of ints or defaultInt if not found. */
{
struct hashEl *hel = hashLookup(hash, name);
if(hel == NULL)
    return defaultInt;
return ptToInt(hel->val);
}

void *hashMustFindVal(struct hash *hash, char *name)
/* Lookup name in hash and return val.  Abort if not found. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
    errAbort("%s not found", name);
return hel->val;
}

void *hashFindVal(struct hash *hash, char *name)
/* Look up name in hash and return val or NULL if not found. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
    return NULL;
return hel->val;
}

char *hashMustFindName(struct hash *hash, char *name)
/* Return name as stored in hash table (in hel->name). 
 * Abort if not found. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
    errAbort("%s not found", name);
return hel->name;
}

void hashAddInt(struct hash *hash, char *name, int val)
/* Store integer value in hash */
{
char *pt = NULL;
hashAdd(hash, name, pt + val);
}

struct hash *newHash(int powerOfTwoSize)
/* Returns new hash table. */
{
struct hash *hash = needMem(sizeof(*hash));
int memBlockPower = 16;
if (powerOfTwoSize == 0)
    powerOfTwoSize = 12;
assert(powerOfTwoSize <= 24 && powerOfTwoSize > 0);
hash->powerOfTwoSize = powerOfTwoSize;
hash->size = (1<<powerOfTwoSize);
/* Make size of memory block for allocator vary between
 * 256 bytes and 64k depending on size of table. */
if (powerOfTwoSize < 8)
    memBlockPower = 8;
else if (powerOfTwoSize < 16)
    memBlockPower = powerOfTwoSize;
hash->lm = lmInit(1<<memBlockPower);
hash->mask = hash->size-1;
hash->table = lmAlloc(hash->lm, sizeof(struct hashEl *) * hash->size);
return hash;
}

void hashTraverseEls(struct hash *hash, void (*func)(struct hashEl *hel))
/* Apply func to every element of hash with hashEl as parameter. */
{
int i;
struct hashEl *hel;
for (i=0; i<hash->size; ++i)
    {
    for (hel = hash->table[i]; hel != NULL; hel = hel->next)
	func(hel);
    }
}

void hashTraverseVals(struct hash *hash, void (*func)(void *val))
/* Apply func to every element of hash with hashEl->val as parameter. */
{
int i;
struct hashEl *hel;
for (i=0; i<hash->size; ++i)
    {
    for (hel = hash->table[i]; hel != NULL; hel = hel->next)
	func(hel->val);
    }
}

int hashElCmp(const void *va, const void *vb)
/* Compare two hashEl by name. */
{
const struct hashEl *a = *((struct hashEl **)va);
const struct hashEl *b = *((struct hashEl **)vb);
return strcmp(a->name, b->name);
}

void *hashElFindVal(struct hashEl *list, char *name)
/* Look up name in hashEl list and return val or NULL if not found. */
{
struct hashEl *el;
for (el = list; el != NULL; el = el->next)
    {
    if (strcmp(el->name, name) == 0)
        return el->val;
    }
return NULL;
}

struct hashEl *hashElListHash(struct hash *hash)
/* Return a list of all elements of hash.   Free return with hashElFreeList. */
{
int i;
struct hashEl *hel, *dupe, *list = NULL;
for (i=0; i<hash->size; ++i)
    {
    for (hel = hash->table[i]; hel != NULL; hel = hel->next)
	{
	dupe = CloneVar(hel);
	slAddHead(&list, dupe);
	}
    }
return list;
}


void hashElFree(struct hashEl **pEl)
/* Free hash el list returned from hashListAll.  (Don't use
 * this internally.) */
{
freez(pEl);
}

void hashElFreeList(struct hashEl **pList)
/* Free hash el list returned from hashListAll.  (Don't use
 * this internally. */
{
slFreeList(pList);
}

struct hashCookie hashFirst(struct hash *hash)
/* Return an object to use by hashNext() to traverse the hash table.
 * The first call to hashNext will return the first entry in the table. */
{
struct hashCookie cookie;
cookie.hash = hash;
cookie.idx = 0;
cookie.nextEl = NULL;

/* find first entry */
for (cookie.idx = 0;
     (cookie.idx < hash->size) && (hash->table[cookie.idx] == NULL);
     cookie.idx++)
    continue;  /* empty body */
if (cookie.idx < hash->size)
    cookie.nextEl = hash->table[cookie.idx];
return cookie;
}

struct hashEl* hashNext(struct hashCookie *cookie)
/* Return the next entry in the hash table. Do not modify hash table
 * while this is being used. */
{
/* NOTE: if hashRemove were coded to track the previous entry during the
 * search and then use it to do the remove, it would be possible to
 * remove the entry returned by this method */
struct hashEl *retEl = cookie->nextEl;
if (retEl == NULL)
    return NULL;  /* no more */

/* find next entry */
cookie->nextEl = retEl->next;
if (cookie->nextEl == NULL)
    {
    for (cookie->idx++; (cookie->idx < cookie->hash->size)
             && (cookie->hash->table[cookie->idx] == NULL); cookie->idx++)
        continue;  /* empty body */
    if (cookie->idx < cookie->hash->size)
        cookie->nextEl = cookie->hash->table[cookie->idx];
    }
return retEl;
}

void freeHash(struct hash **pHash)
/* Free up hash table. */
{
struct hash *hash = *pHash;
if (hash == NULL)
    return;
lmCleanup(&hash->lm);
freez(pHash);
}

void freeHashAndVals(struct hash **pHash)
/* Free up hash table and all values associated with it.
 * (Just calls freeMem on each hel->val) */
{
struct hash *hash;
if ((hash = *pHash) != NULL)
    {
    hashTraverseVals(hash, freeMem);
    freeHash(pHash);
    }
}

