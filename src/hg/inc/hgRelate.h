/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* hgRelate - Especially relational parts of browser data base. */
#ifndef HGRELATE_H
#define HGRELATE_H

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif 

#ifndef UNFIN_H
#include "unfin.h"
#endif

#ifndef JKSQL_H
#include "jksql.h"
#endif

typedef unsigned int HGID;	/* A database ID. */

void hgSetDb(char *dbName);
/* Set the database name. */

char *hgGetDb();
/* Return the current database name. */

struct sqlConnection *hgAllocConn();
/* Get free connection if possible. If not allocate a new one. */

void hgFreeConn(struct sqlConnection **pConn);
/* Put back connection for reuse. */


HGID hgIdQuery(struct sqlConnection *conn, char *query);
/* Return first field of first table as HGID. 0 return ok. */

HGID hgRealIdQuery(struct sqlConnection *conn, char *query);
/* Return first field of first table as HGID- abort if 0. */


struct sqlConnection *hgStartUpdate();
/* Open and connection and get next global id from the history table */

void hgEndUpdate(struct sqlConnection **pConn, char *comment, ...);
/* Finish up connection with a printf format comment. */

HGID hgNextId();
/* Get next unique id.  (Should only be called after hgStartUpdate). */

FILE *hgCreateTabFile(char *tmpDir, char *tableName);
/* Open a tab file with name corresponding to tableName in tmpDir. */

void hgLoadTabFile(struct sqlConnection *conn, char *tmpDir, char *tableName,
                   FILE **tabFh);
/* Load tab delimited file corresponding to tableName. close fh if not NULL */

HGID hgGetMaxId(struct sqlConnection *conn, char *tableName);
/* get the maximum value of the id column in a table or zero if empry  */

int hgAddToExtFile(char *path, struct sqlConnection *conn);
/* Add entry to ext file table.  Delete it if it already exists. 
 * Returns extFile id. */

#endif /* HGRELATE_H */

