/* Stuff to parse .ra files. Ra files are simple text databases.
 * The database is broken into records by blank lines. 
 * Each field takes a line.  The name of the field is the first
 * word in the line.  The value of the field is the rest of the line. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef RA_H

struct hash *raNextRecord(struct lineFile *lf);
/* Return a hash containing next record.   
 * Returns NULL at end of file.  freeHash this
 * when done.  Note this will free the hash
 * keys and values as well, so you'll have to
 * cloneMem them if you want them for later. */

struct slPair *raNextRecordAsSlPairList(struct lineFile *lf);
/* Return ra record as a slPair list instead of a hash.  Handy if you want to preserve the order. 
 * Do a slPairFreeValsAndList on result when done. */

boolean raSkipLeadingEmptyLines(struct lineFile *lf);
/* Skip leading empty lines and comments.  Returns FALSE at end of file. 
 * Together with raNextTagVal you can construct your own raNextRecord.... */

boolean raNextTagVal(struct lineFile *lf, char **retTag, char **retVal);
/* Read next line.  Return FALSE at end of file or blank line.  Otherwise
 * fill in *retTag and *retVal and return TRUE */

struct hash *raFromString(char *string);
/* Return hash of key/value pairs from string.
 * As above freeHash this when done. */

boolean raFoldInOne(struct lineFile *lf, struct hash *hashOfHash);
/* Fold in one record from ra file into hashOfHash. 
 * This will add ra's and ra fields to whatever already
 * exists in the hashOfHash,  overriding fields of the
 * same name if they exist already. */

void raFoldIn(char *fileName, struct hash *hashOfHash);
/* Read ra's in file name and fold them into hashOfHash. 
 * This will add ra's and ra fields to whatever already
 * exists in the hashOfHash,  overriding fields of the
 * same name if they exist already. */

struct hash *raReadSingle(char *fileName);
/* Read in first ra record in file and return as hash. */

struct hash *raReadAll(char *fileName, char *keyField);
/* Return hash that contains all ra records in file keyed
 * by given field, which must exist.  The values of the
 * hash are themselves hashes. */

#endif /* RA_H */

