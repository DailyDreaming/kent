static char const rcsid[] = "$Id: net.c,v 1.24 2003/05/05 06:45:35 kate Exp $";

/* net.c some stuff to wrap around net communications. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "internet.h"
#include "errabort.h"
#include "net.h"
#include "linefile.h"

/* Brought errno in to get more useful error messages */

extern int errno;

static int netStreamSocket()
/* Create a TCP/IP streaming socket.  Complain and return something
 * negative if can't */
{
int sd = socket(AF_INET, SOCK_STREAM, 0);
if (sd < 0)
    warn("Couldn't make AF_INET socket.");
return sd;
}


int netConnect(char *hostName, int port)
/* Start connection with a server. */
{
int sd, err;
struct sockaddr_in sai;		/* Some system socket info. */

if (hostName == NULL)
    {
    warn("NULL hostName in netConnect");
    return -1;
    }
if (!internetFillInAddress(hostName, port, &sai))
    return -1;
if ((sd = netStreamSocket()) < 0)
    return sd;
if ((err = connect(sd, (struct sockaddr*)&sai, sizeof(sai))) < 0)
   {
   warn("Couldn't connect to %s %d", hostName, port);
   close(sd);
   return err;
   }
return sd;
}

int netMustConnect(char *hostName, int port)
/* Start connection with server or die. */
{
int sd = netConnect(hostName, port);
if (sd < 0)
   noWarnAbort();
return sd;
}

int netMustConnectTo(char *hostName, char *portName)
/* Start connection with a server and a port that needs to be converted to integer */
{
if (!isdigit(portName[0]))
    errAbort("netConnectTo: ports must be numerical, not %s", portName);
return netMustConnect(hostName, atoi(portName));
}

static int netAcceptingSocketFrom(int port, int queueSize, char *host)
/* Create a socket that can accept connections from a particular
 * host.  If host is NULL then accept from anyone. */
{
struct sockaddr_in sai;
int sd;
int flag = 1;
 int len;

netBlockBrokenPipes();
if ((sd = netStreamSocket()) < 0)
    return sd;
if (!internetFillInAddress(host, port, &sai))
    return -1;
if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)))
    return -1;
if (getsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &flag, &len))
    warn("getsockopt error\n");
if (bind(sd, (struct sockaddr*)&sai, sizeof(sai)) == -1)
    {
    warn("Couldn't bind socket to %d: %s", port, strerror(errno));
    close(sd);
    return -1;
    }
listen(sd, queueSize);
return sd;
}

int netAcceptingSocket(int port, int queueSize)
/* Create a socket that can accept connections from
 * anywhere. */
{
return netAcceptingSocketFrom(port, queueSize, NULL);
}

int netAccept(int sd)
/* Accept incoming connection from socket descriptor. */
{
int fromLen;
return accept(sd, NULL, &fromLen);
}

static boolean plumberInstalled = FALSE;

void netBlockBrokenPipes()
/* Make it so a broken pipe doesn't kill us. */
{
if (!plumberInstalled)
    {
    signal(SIGPIPE, SIG_IGN);       /* Block broken pipe signals. */
    plumberInstalled = TRUE;
    }
}

int netReadAll(int sd, void *vBuf, size_t size)
/* Read given number of bytes into buffer.
 * Don't give up on first read! */
{
char *buf = vBuf;
size_t totalRead = 0;
int oneRead;

if (!plumberInstalled)
    netBlockBrokenPipes();
while (totalRead < size)
    {
    oneRead = read(sd, buf + totalRead, size - totalRead);
    if (oneRead < 0)
	return oneRead;
    if (oneRead == 0)
        break;
    totalRead += oneRead;
    }
return totalRead;
}

int netMustReadAll(int sd, void *vBuf, size_t size)
/* Read given number of bytes into buffer or die.
 * Don't give up if first read is short! */
{
int ret = netReadAll(sd, vBuf, size);
if (ret < 0)
    errnoAbort("Couldn't finish netReadAll");
return ret;
}

void netParseUrl(char *url, struct netParsedUrl *parsed)
/* Parse a URL into components.   A full URL is made up as so:
 *   http://hostName:port/file
 * This is set up so that the http:// and the port are optional. 
 */
{
char *s, *t, *u;
char buf[1024];

/* Make local copy of URL. */
if (strlen(url) >= sizeof(buf))
    errAbort("Url too long: '%s'", url);
strcpy(buf, url);
url = buf;

/* Find out protocol - default to http. */
s = trimSpaces(url);
s = stringIn("://", url);
if (s == NULL)
    {
    strcpy(parsed->protocol, "http");
    s = url;
    }
else
    {
    *s = 0;
    tolowers(url);
    strncpy(parsed->protocol, url, sizeof(parsed->protocol));
    s += 3;
    }

/* Split off file part. */
u = strchr(s, '/');
if (u == NULL)
   strcpy(parsed->file, "/");
else
   {
   strncpy(parsed->file, u, sizeof(parsed->file));
   *u = 0;
   }

/* Save port if it's there.  If not default to 80. */
t = strchr(s, ':');
if (t == NULL)
   {
   strcpy(parsed->port, "80");
   }
else
   {
   *t++ = 0;
   if (!isdigit(t[0]))
      errAbort("Non-numeric port name %s", t);
   strncpy(parsed->port, t, sizeof(parsed->port));
   }

/* What's left is the host. */
strncpy(parsed->host, s, sizeof(parsed->host));
}

static int netGetOpenHttp(char *url)
/* Return a file handle that will read the url.  This
 * skips past any header. */
{
struct netParsedUrl npu;
struct dyString *dy = newDyString(512);
int sd;

/* Parse the URL and connect. */
netParseUrl(url, &npu);
if (!sameString(npu.protocol, "http"))
    errAbort("Sorry, can only netOpen http's currently");
sd = netMustConnect(npu.host, atoi(npu.port));

/* Ask remote server for a file. */
dyStringPrintf(dy, "GET %s HTTP/1.0\r\n", npu.file);
dyStringPrintf(dy, "User-Agent: genome.ucsc.edu/net.c\r\n");
dyStringPrintf(dy, "Host: %s:%s\r\n", npu.host, npu.port);
dyStringPrintf(dy, "Accept: */*\r\n", npu.host, npu.port);
dyStringPrintf(dy, "\r\n", npu.host, npu.port);
write(sd, dy->string, dy->stringSize);

/* Clean up and return handle. */
dyStringFree(&dy);
return sd;
}

int netUrlOpen(char *url)
/* Return unix low-level file handle for url. 
 * Just close(result) when done. */
{
return netGetOpenHttp(url);
}

struct dyString *netSlurpUrl(char *url)
/* Go grab all of URL and return it as dynamic string. */
{
char buf[4*1024];
int readSize;
struct dyString *dy = newDyString(4*1024);
int sd = netUrlOpen(url);

/* Slurp file into dy and return. */
while ((readSize = read(sd, buf, sizeof(buf))) > 0)
    dyStringAppendN(dy, buf, readSize);
close(sd);
return dy;
}

static boolean netSkipHttpHeaderLines(struct lineFile *lf)
/* Skip http header lines. Return FALSE if there's a problem */
{
char *line;
if (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("HTTP/", line))
        {
	char *version, *code;
	version = nextWord(&line);
	code = nextWord(&line);
	if (code == NULL)
	    {
	    warn("Strange http header on %s\n", lf->fileName);
	    return FALSE;
	    }
	if (!sameString(code, "200"))
	    {
	    warn("%s: %s %s\n", lf->fileName, code, line);
	    return FALSE;
	    }
	while (lineFileNext(lf, &line, NULL))
	    {
	    if ((line[0] == '\r' && line[1] == 0) || line[0] == 0)
	        break;
	    }
	}
    else
        lineFileReuse(lf);
    }
return TRUE;
}

struct lineFile *netLineFileMayOpen(char *url)
/* Return a lineFile attatched to url. Skipp
 * http header.  Return NULL if there's a problem. */
{
int sd = netUrlOpen(url);
if (sd < 0)
    {
    warn("Couldn't open %s", url);
    return NULL;
    }
else
    {
    struct lineFile *lf = lineFileAttatch(url, TRUE, sd);
    if (!netSkipHttpHeaderLines(lf))
	lineFileClose(&lf);
    return lf;
    }
}

struct lineFile *netLineFileOpen(char *url)
/* Return a lineFile attatched to url.  This one
 * will skip any headers.   Free this with
 * lineFileClose(). */
{
struct lineFile *lf = netLineFileMayOpen(url);
if (lf == NULL)
    noWarnAbort();
return lf;
}

boolean netSendString(int sd, char *s)
/* Send a string down a socket - length byte first. */
{
int length = strlen(s);
UBYTE len;

if (length > 255)
    errAbort("Trying to send a string longer than 255 bytes (%d bytes)", length);
len = length;
if (write(sd, &len, 1)<0)
    {
    warn("Couldn't send string to socket");
    return FALSE;
    }
if (write(sd, s, length)<0)
    {
    warn("Couldn't send string to socket");
    return FALSE;
    }
return TRUE;
}

boolean netSendLongString(int sd, char *s)
/* Send a long string down socket: two bytes for length. */
{
unsigned length = strlen(s);
UBYTE b[2];

if (length >= 64*1024)
    {
    warn("Trying to send a string longer than 64k bytes (%d bytes)", length);
    return FALSE;
    }
b[0] = (length>>8);
b[1] = (length&0xff);
if (write(sd, b, 2) < 0)
    {
    warn("Couldn't send long string to socket");
    return FALSE;
    }
if (write(sd, s, length)<0)
    {
    warn("Couldn't send long string to socket");
    return FALSE;
    }
return TRUE;
}

boolean netSendHugeString(int sd, char *s)
/* Send a long string down socket: four bytes for length. */
{
unsigned long length = strlen(s);
unsigned long l = length;
UBYTE b[4];
int i;
for (i=3; i>=0; --i)
    {
    b[i] = l & 0xff;
    l >>= 8;
    }
if (write(sd, b, 4) < 0)
    {
    warn("Couldn't send huge string to socket");
    return FALSE;
    }
if (write(sd, s, length) < 0)
    {
    warn("Couldn't send huge string to socket");
    return FALSE;
    }
return TRUE;
}


char *netGetString(int sd, char buf[256])
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Print warning message
 * and return NULL if any problem. */
{
static char sbuf[256];
UBYTE len = 0;
int length;
if (buf == NULL) buf = sbuf;
if (read(sd, &len, 1)<0)
    {
    warn("Couldn't read string length");
    return NULL;
    }
length = len;
if (length > 0)
    if (netReadAll(sd, buf, length) < 0)
	{
	warn("Couldn't read string body");
	return NULL;
	}
buf[length] = 0;
return buf;
}

char *netGetLongString(int sd)
/* Read string and return it.  freeMem
 * the result when done. */
{
UBYTE b[2];
char *s = NULL;
int length = 0;
int sz;
b[0] = b[1] = 0;
sz = netReadAll(sd, b, 2);
if (sz == 0)
    return NULL;
if (sz < 0)
    {
    warn("Couldn't read long string length");
    return NULL;
    }
length = (b[0]<<8) + b[1];
s = needMem(length+1);
if (length > 0)
    if (netReadAll(sd, s, length) < 0)
	{
	warn("Couldn't read long string body");
	return NULL;
	}
s[length] = 0;
return s;
}

char *netGetHugeString(int sd)
/* Read string and return it.  freeMem
 * the result when done. */
{
UBYTE b[4];
char *s = NULL;
unsigned long length = 0;
int sz, i;
sz = netReadAll(sd, b, 4);
if (sz == 0)
    return NULL;
if (sz < 4)
    {
    warn("Couldn't read huge string length");
    return NULL;
    }
for (i=0; i<4; ++i)
    {
    length <<= 8;
    length += b[i];
    }
s = needMem(length+1);
if (length > 0)
    {
    if (netReadAll(sd, s, length) < 0)
	{
	warn("Couldn't read huge string body");
	return NULL;
	}
    }
s[length] = 0;
return s;
}


char *netRecieveString(int sd, char buf[256])
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Abort if any problem. */
{
char *s = netGetString(sd, buf);
if (s == NULL)
     noWarnAbort();   
return s;
}

char *netRecieveLongString(int sd)
/* Read string and return it.  freeMem
 * the result when done. Abort if any problem*/
{
char *s = netGetLongString(sd);
if (s == NULL)
     noWarnAbort();   
return s;
}

char *netRecieveHugeString(int sd)
/* Read string and return it.  freeMem
 * the result when done. Abort if any problem*/
{
char *s = netGetHugeString(sd);
if (s == NULL)
     noWarnAbort();   
return s;
}


struct lineFile *netHttpLineFileMayOpen(char *url, struct netParsedUrl **npu)
/* Parse URL and open an HTTP socket for it but don't send a request yet. */
{
int sd;
struct lineFile *lf;

/* Parse the URL and try to connect. */
AllocVar(*npu);
netParseUrl(url, *npu);
if (!sameString((*npu)->protocol, "http"))
    errAbort("Sorry, can only netOpen http's currently");
sd = netConnect((*npu)->host, atoi((*npu)->port));
if (sd < 0)
    return NULL;

/* Return handle. */
lf = lineFileAttatch(url, TRUE, sd);
return lf;
} /* netHttpLineFileMayOpen */


void netHttpGet(struct lineFile *lf, struct netParsedUrl *npu,
		boolean keepAlive)
/* Send a GET request, possibly with Keep-Alive. */
{
struct dyString *dy = newDyString(512);

/* Ask remote server for the file/query. */
dyStringPrintf(dy, "GET %s HTTP/1.1\r\n", npu->file);
dyStringPrintf(dy, "User-Agent: genome.ucsc.edu/net.c\r\n");
dyStringPrintf(dy, "Host: %s:%s\r\n", npu->host, npu->port);
dyStringAppend(dy, "Accept: */*\r\n");
if (keepAlive)
  {
    dyStringAppend(dy, "Connection: Keep-Alive\r\n");
    dyStringAppend(dy, "Connection: Persist\r\n");
  }
else
    dyStringAppend(dy, "Connection: close\r\n");
dyStringAppend(dy, "\r\n");
write(lf->fd, dy->string, dy->stringSize);
/* Clean up. */
dyStringFree(&dy);
} /* netHttpGet */

int netHttpGetMultiple(char *url, struct slName *queries, void *userData,
		       void (*responseCB)(void *userData, char *req,
					  char *hdr, struct dyString *body))
/* Given an URL which is the base of all requests to be made, and a 
 * linked list of queries to be appended to that base and sent in as 
 * requests, send the requests as a batch and read the HTTP response 
 * headers and bodies.  If not all the requests get responses (i.e. if 
 * the server is ignoring Keep-Alive or is imposing a limit), try again 
 * until we can't connect or until all requests have been served. 
 * For each HTTP response, do a callback. */
{
  struct slName *qStart;
  struct slName *qPtr;
  struct lineFile *lf;
  struct netParsedUrl *npu;
  struct dyString *dyQ    = newDyString(512);
  struct dyString *body;
  char *base;
  char *req;
  char *hdr;
  int qCount;
  int qTotal;
  int numParseFailures;
  int contentLength;
  boolean chunked;
  boolean done;
  boolean keepAlive;

  /* Find out how many queries we'll need to do so we know how many times 
   * it's OK to run into end of file in case server ignores Keep-Alive. */
  qTotal = 0;
  for (qPtr = queries;  qPtr != NULL;  qPtr = qPtr->next)
    {
      qTotal++;
    }

  done = FALSE;
  qCount = 0;
  numParseFailures = 0;
  qStart = queries;
  while ((! done) && (qStart != NULL))
    {
      lf = netHttpLineFileMayOpen(url, &npu);
      if (lf == NULL)
	{
	  done = TRUE;
	  break;
	}
      base = cloneString(npu->file);
      /* Send all remaining requests with keep-alive. */
      for (qPtr = qStart;  qPtr != NULL;  qPtr = qPtr->next)
	{
	  dyStringClear(dyQ);
	  dyStringAppend(dyQ, base);
	  dyStringAppend(dyQ, qPtr->name);
	  strcpy(npu->file, dyQ->string);
	  keepAlive = (qPtr->next == NULL) ? FALSE : TRUE;
	  netHttpGet(lf, npu, keepAlive);
	}
      /* Get as many responses as we can; call responseCB() and 
       * advance qStart for each. */
      for (qPtr = qStart;  qPtr != NULL;  qPtr = qPtr->next)
        {
	  if (lineFileParseHttpHeader(lf, &hdr, &chunked, &contentLength))
	    {
	      body = lineFileSlurpHttpBody(lf, chunked, contentLength);
	      dyStringClear(dyQ);
	      dyStringAppend(dyQ, base);
	      dyStringAppend(dyQ, qPtr->name);
	      responseCB(userData, dyQ->string, hdr, body);
	      qStart = qStart->next;
	      qCount++;
	    }
	  else
	    {
	      if (numParseFailures++ > qTotal) {
		done = TRUE;
	      }
	      break;
	    }
	}
    }

  return qCount;
} /* netHttpMultipleQueries */


