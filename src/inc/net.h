/* Net.h some stuff to wrap around net communications. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */


#ifndef NET_H
#define NET_H

#ifndef LINEFILE_H
#include "linefile.h"
#endif /* LINEFILE_H */

#ifndef DYSTRING_H
#include "dystring.h"
#endif /* DYSTRING_H */

int netConnect(char *hostName, int port);
/* Start connection with a server having resolved port. */

int netMustConnect(char *hostName, int port);
/* Start connection with server or die. */

int netMustConnectTo(char *hostName, char *portName);
/* Start connection with a server and a port that needs to be converted to integer */

int netAcceptingSocket(int port, int queueSize);
/* Create a socket for to accept connections. */

int netAcceptingSocketFrom(int port, int queueSize, char *host);
/* Create a socket that can accept connections from a 
 * IP address on the current machine if the current machine
 * has multiple IP addresses. */

int netAccept(int sd);
/* Accept incoming connection from socket descriptor. */

int netAcceptFrom(int sd, unsigned char subnet[4]);
/* Wait for incoming connection from socket descriptor
 * from IP address in subnet.  Subnet is something
 * returned from netParseDottedQuad.  */

FILE *netFileFromSocket(int socket);
/* Wrap a FILE around socket.  This should be fclose'd
 * and separately the socket close'd. */

void netBlockBrokenPipes();
/* Make it so a broken pipe doesn't kill us. */

int netReadAll(int sd, void *vBuf, size_t size);
/* Read given number of bytes into buffer.
 * Don't give up on first read! */

int netMustReadAll(int sd, void *vBuf, size_t size);
/* Read given number of bytes into buffer or die.
 * Don't give up if first read is short! */

boolean netSendString(int sd, char *s);
/* Send a string down a socket - length byte first. */

boolean netSendLongString(int sd, char *s);
/* Send a string down a socket - up to 64k characters. */

boolean netSendHugeString(int sd, char *s);
/* Send a string down a socket - up to 4G characters. */

char *netRecieveString(int sd, char buf[256]);
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Abort if any problem. */

char *netRecieveLongString(int sd);
/* Read string up to 64k and return it.  freeMem
 * the result when done. Abort if any problem*/

char *netRecieveHugeString(int sd);
/* Read string up to 4G and return it.  freeMem
 * the result when done. Abort if any problem*/

char *netGetString(int sd, char buf[256]);
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Print warning message
 * and return NULL if any problem. */

char *netGetLongString(int sd);
/* Read string up to 64k and return it.  freeMem
 * the result when done.  Print warning message and
 * return NULL if any problem. */

char *netGetHugeString(int sd);
/* Read string up to 4 gig and return it.  freeMem
 * the result when done.  Print warning message and
 * return NULL if any problem. */

void netCatchPipes();
/* Set up to catch broken pipe signals. */

boolean netPipeIsBroken();
/* Return TRUE if pipe is broken */

void  netClearPipeFlag();
/* Clear broken pipe flag. */

void netParseSubnet(char *in, unsigned char out[4]);
/* Parse subnet, which is a prefix of a normal dotted quad form.
 * Out will contain 255's for the don't care bits. */

struct netParsedUrl
/* A parsed URL. */
   {
   char protocol[16];	/* Protocol - http or ftp, etc. */
   char host[128];	/* Name of host computer - www.yahoo.com, etc. */
   char port[16];       /* Port, usually 80 or 8080. */
   char file[1024];	/* Remote file name/query string, starts with '/' */
   };

void netParseUrl(char *url, struct netParsedUrl *parsed);
/* Parse a URL into components.   A full URL is made up as so:
 *   http://hostName:port/file
 * This is set up so that the http:// and the port are optional. 
 */

int netUrlOpen(char *url);
/* Return unix low-level file handle for url. 
 * Just close(result) when done. */

struct hash;

int netUrlHead(char *url, struct hash *hash);
/* Go get head and return status.  Return negative number if
 * can't get head. If hash is non-null, fill it with header
 * lines, including hopefully Content-Type: */

struct lineFile *netLineFileOpen(char *url);
/* Return a lineFile attatched to url.  This one
 * will skip any headers.   Free this with
 * lineFileClose(). */

struct lineFile *netLineFileMayOpen(char *url);
/* Same as netLineFileOpen, but warns and returns
 * null rather than aborting on problems. */

struct dyString *netSlurpFile(int sd);
/* Slurp file into dynamic string and return. */

struct dyString *netSlurpUrl(char *url);
/* Go grab all of URL and return it as dynamic string. */

struct lineFile *netHttpLineFileMayOpen(char *url, struct netParsedUrl **npu);
/* Parse URL and open an HTTP socket for it but don't send a request yet. */

void netHttpGet(struct lineFile *lf, struct netParsedUrl *npu,
		boolean keepAlive);
/* Send a GET request, possibly with Keep-Alive. */

int netOpenHttpExt(char *url, char *method, boolean end);
/* Return a file handle that will read the url.  If end is not
 * set then can send cookies and other info to returned file  */

int netHttpGetMultiple(char *url, struct slName *queries, void *userData,
		       void (*responseCB)(void *userData, char *req,
					  char *hdr, struct dyString *body));
/* Given an URL which is the base of all requests to be made, and a 
 * linked list of queries to be appended to that base and sent in as 
 * requests, send the requests as a batch and read the HTTP response 
 * headers and bodies.  If not all the requests get responses (i.e. if 
 * the server is ignoring Keep-Alive or is imposing a limit), try again 
 * until we can't connect or until all requests have been served. 
 * For each HTTP response, do a callback. */

#endif /* NET_H */

