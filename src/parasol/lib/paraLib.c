#include <sys/utsname.h>
#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include "common.h"
#include "options.h"
#include "errabort.h"
#include "net.h"
#include "paraLib.h"

time_t now;	/* Time when started processing current message */

void findNow()
/* Just set now to current time. */
{
now = time(NULL);
}

char paraSig[17] = "0d2f070562685f29";  /* Mild security measure. */
int paraSigSize = 16;
int paraPort = 0x46DC;		      /* Our port */

boolean sendWithSig(int fd, char *string)
/* Send a string with the signature prepended.  Warn 
 * but don't abort if there's a problem.  Return TRUE
 * on success. */
{
if (write(fd, paraSig, paraSigSize) < paraSigSize)
    {
    warn("Couldn't write signature to socket");
    return FALSE;
    }
return netSendLongString(fd, string);
}

void mustSendWithSig(int fd, char *string)
/* Send a string with the signature prepended. 
 * Abort on failure. */
{
if (!sendWithSig(fd, string))
    noWarnAbort();
}

char *getMachine()
/* Return host name. */
{
static char *host = NULL;
if (host == NULL)
    {
    host = getenv("HOST");
    if (host == NULL)
	{
	struct utsname unamebuf;
	if (uname(&unamebuf) < 0)
	    errAbort("Couldn't find HOST environment variable or good uname");
	host = unamebuf.nodename;
	}
    host = cloneString(host);
    }
return host;
}

char *getUser()
/* Get user name */
{
uid_t uid = geteuid();
struct passwd *pw = getpwuid(uid);
if (pw == NULL)
    {
    warn("Can't getUser");
    return "nobody";
    }
return pw->pw_name;
}

boolean parseRunJobMessage(char *line, struct runJobMessage *rjm)
/* Parse runJobMessage as paraNodes sees it. */
{
bool ret;
rjm->managingHost = nextWord(&line);
rjm->jobIdString = nextWord(&line);
rjm->reserved = nextWord(&line);
rjm->user = nextWord(&line);
rjm->dir = nextWord(&line);
rjm->in = nextWord(&line);
rjm->out = nextWord(&line);
rjm->err = nextWord(&line);
rjm->command = line = skipLeadingSpaces(line);
ret = (line != NULL && line[0] != 0);
if (!ret)
    warn("Badly formatted jobMessage");
return ret;
}

void fillInErrFile(char errFile[512], int jobId, char *tempDir )
/* Fill in error file name */
{
sprintf(errFile, "%s/para%d.err", tempDir, jobId);
}


static FILE *logFile = NULL;  /* Log file - if NULL no logging. */
boolean logFlush;

void vLogIt(char *format, va_list args)
/* Variable args logit. */
{
if (logFile != NULL)
    {
    fprintf(logFile, "%lu ", now);
    vfprintf(logFile, format, args);
    if (logFlush)
	fflush(logFile);
    }
}

void logIt(char *format, ...)
/* Print message to log file. */
{
if (logFile != NULL)
    {
    va_list args;
    va_start(args, format);
    vLogIt(format, args);
    va_end(args);
    }
}


static void warnToLog(char *format, va_list args)
/* Warn handler that prints to log file. */
{
if (logFile != NULL)
    {
    fputs("warn: ", logFile);
    vfprintf(logFile, format, args);
    fputs("\n", logFile);
    if (logFlush)
	fflush(logFile);
    }
}

void flushLog()
/* Flush log file */
{
if (logFile != NULL)
   fflush(logFile);
}

static char *logName = NULL;

static void reopenLog(int s)
/* Close and reopen log files in response to signal. */
{
if (logName != NULL)
    {
    carefulClose(&logFile);
    logFile = fopen(logName, "w");
    }
}

void setupDaemonLog(char *fileName)
/* Setup log file, and warning handler that goes to this
 * file.  If fileName is NULL then no log, and warning
 * messages go into the bit bucket. */
{
if (fileName != NULL)
    {
    logFile = mustOpen(fileName, "w");
    logName = cloneString(fileName);
    }
if (!logFlush)
    logFlush = optionExists("logFlush");
signal(SIGHUP, reopenLog);
pushWarnHandler(warnToLog);
}

void logClose()
/* Close log file. */
{
carefulClose(&logFile);
}
