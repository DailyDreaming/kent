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

struct hash *raFromString(char *string);
/* Return hash of key/value pairs from string.
 * As above freeHash this when done. */

#endif /* RA_H */

