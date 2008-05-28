/* parasol - Parasol program - for launching programs in parallel on a computer cluster. */
#include "paraCommon.h"
#include "net.h"
#include "linefile.h"
#include "errabort.h"
#include "dystring.h"
#include "hash.h"
#include "options.h"
#include "paraLib.h"
#include "paraMessage.h"

static char const rcsid[] = "$Id: parasol.c,v 1.39 2008/05/28 05:17:05 galt Exp $";

char *version = PARA_VERSION;   /* Version number. */

struct rudp *hubRudp;	/* Network connection to paraHub. */
char *userName;	/* Name of user. */

void mustBeRoot()
/* Abort if we're not root. */
{
if (!sameString(userName, "root"))
    errAbort("That command can only be run by root.");
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "Parasol version %s\n"
  "Parasol is the name given to the overall system for managing jobs on\n"
  "a computer cluster and to this specific command.  This command is\n"
  "intended primarily for system administrators.  The 'para' command\n"
  "is the primary command for users.\n"
  "Usage in brief:\n"
  "   parasol add machine machineFullHostName localTempDir  - Add new machine to pool.\n"
  "    or \n"
  "   parasol add machine machineFullHostName cpus ramSizeMB localTempDir localDir localSizeMB switchName\n"
  "   parasol remove machine machineFullHostName \"reason why\"  - Remove machine from pool.\n"
  "   parasol add spoke  - Add a new spoke daemon.\n"
  "   parasol [options] add job command-line   - Add job to list.\n"
  "         options: -out=out -in=in -dir=dir -results=file -verbose\n"
  "   parasol remove job id  - Remove job of given ID.\n"
  "   parasol ping [count]  - Ping hub server to make sure it's alive.\n"
  "   parasol remove jobs userName [jobPattern]  - Remove jobs submitted by user that\n"
  "         match jobPattern (which may include ? and * escaped for shell).\n"
  "   parasol list machines  - List machines in pool.\n"
  "   parasol [-extended] list jobs  - List jobs one per line.\n"
  "   parasol list users  - List users one per line.\n"
  "   parasol list batches  - List batches one per line.\n"
  "   parasol list sick  - List sick nodes one per line.\n"
  "   parasol status  - Summarize status of machines, jobs, and spoke daemons.\n"
  , version
  );
}

boolean commandHub(char *command)
/* Send a command to hub. */
{
struct paraMessage pm;
pmInitFromName(&pm, "localhost", paraHubPort);
return pmSendString(&pm, hubRudp, command);
}

char *hubCommandGetReciept(char *command)
/* Send command to hub,  wait for one line respons and
 * return it.  freeMem return value when done. */
{
struct paraMessage pm;
if (!commandHub(command))
    return NULL;
if (pmReceive(&pm, hubRudp))
    return cloneString(pm.data);
else
    return NULL;
}

void hubCommandCheckReceipt(char *command)
/* Send command to hub, and wait for one line response which should
 * be 'ok'. */
{
char *line = hubCommandGetReciept(command);
if (line == NULL || !sameString(line, "ok"))
    errAbort("Hub didn't acknowledge %s", command);
freeMem(line);
}

void hubCommandAndPrint(char *command)
/* Send command to hub, and print response until blank line. */
{
char *line = NULL;
struct slRef *list = NULL, *ref;
struct paraMessage pm;
int count = 0;
char *row[256];

/* Issue command and suck down response quickly into memory. */
if (!commandHub(command))
    errAbort("Couldn't send '%s' command to paraHub", command);
for (;;)
    {
    if (!pmReceive(&pm, hubRudp))
	break;
    if (pm.size == 0)
        break;

    count = chopByChar(pm.data, '\n', row, sizeof(row));

    if (count > 1) --count;  /* for multiline, count is inflated by one */

    int i;
    for(i=0; i<count; ++i)
        refAdd(&list, cloneString(row[i]));

    }
slReverse(&list);

/* Print results. */
for (ref = list; ref != NULL; ref = ref->next)
    {
    line = ref->val;
    puts(line);
    }
}

void addMachine(int argc, char *argv[])
/* Tell hub about a new machine. */
{
char buf[1024];
mustBeRoot();
if (argc == 3)
    safef(buf, sizeof(buf), "addMachine %s %s", argv[1], argv[2]);
else /* argc == 7 */
    safef(buf, sizeof(buf), "addMachine %s %s %s %s %s %s %s", 
	argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7] );
commandHub(buf);
}

void addJob(int argc, char *argv[], boolean printId)
/* Tell hub about a new job. */
{
struct dyString *dy = newDyString(1024);
char curDir[512];
char defaultResults[512];
char *in = optionVal("in", "/dev/null"), 
     *out = optionVal("out", "/dev/null"), 
     *dir = optionVal("dir", curDir),
     *results = optionVal("results", defaultResults);
char *jobIdString;
int i;

getcwd(curDir, sizeof(curDir));
sprintf(defaultResults, "%s/results", curDir);
dyStringPrintf(dy, "addJob %s %s %s %s %s", userName, dir, in, out, results);
for (i=0; i<argc; ++i)
    dyStringPrintf(dy, " %s", argv[i]);
jobIdString = hubCommandGetReciept(dy->string);
dyStringFree(&dy);
if (printId)
    {
    printf("your job %s (\"%s", jobIdString, argv[0]);
    for (i=1; i<argc; ++i)
	 printf(" %s", argv[i]);
    printf("\") has been submitted\n");
    }
freez(&jobIdString);
}

void addSpoke()
/* Tell hub to add a spoke. */
{
mustBeRoot();
commandHub("addSpoke");
}

void removeMachine(char *machine, char *reason)
/* Tell hub to get rid of machine.
   Log the user and reason.
*/
{
char buf[512];
sprintf(buf, "removeMachine %s %s %s", machine, getUser(), reason);
hubCommandAndPrint(buf);
}

void removeJob(char *job)
/* Tell hub to remove job. */
{
char buf[512];
if (!isdigit(job[0]))
    errAbort("remove job requires a numerical job id");
sprintf(buf, "%s %s", "removeJob", job);
hubCommandCheckReceipt(buf);
}

struct jobInfo
/* Info on a job. */
    {
    struct jobInfo *next;   /* Next in list. */
    char *id;		    /* Job id. */
    char *machine;	    /* Machine name (or NULL) */
    char *user;		    /* Job user. */
    char *command;	    /* Job command line. */
    };

void jobInfoFree(struct jobInfo **pJob)
/* Free up a job info. */
{
struct jobInfo *job;
if ((job = *pJob) != NULL)
    {
    freeMem(job->id);
    freeMem(job->machine);
    freeMem(job->user);
    freeMem(job->command);
    freez(pJob);
    }
}

void jobInfoFreeList(struct jobInfo **pList)
/* Free up list of jobInfo */
{
struct jobInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    jobInfoFree(&el);
    }
*pList = NULL;
}

struct jobInfo *jobInfoParse(char *line)
/* Create a job info from parsing line.  Eats line.  Returns
 * NULL if there's a problem. */
{
char *id, *user, *machine, *command;
struct jobInfo *job;
if ((id = nextWord(&line)) == NULL) return NULL;
if ((machine = nextWord(&line)) == NULL) return NULL;
if ((user = nextWord(&line)) == NULL) return NULL;
line = skipLeadingSpaces(line);
if (line == NULL || line[0] == 0) return NULL;
command = line;
AllocVar(job);
job->id = cloneString(id);
job->user = cloneString(user);
job->machine = cloneString(machine);
job->command = cloneString(command);
return job;
}

struct jobInfo *jobInfoMustParse(char *line)
/* Create job info from parsing line.  Eats line.  Aborts
 * with error message if a problem. */
{
struct jobInfo *job = jobInfoParse(line);
if (job == NULL)
    errAbort("Couldn't parse job %s", line);
return job;
}


struct jobInfo *getJobList()
/* Read job list from server. */
{
struct paraMessage pm;
struct jobInfo *jobList = NULL, *job;
commandHub("listJobs");
for (;;)
    {
    if (!pmReceive(&pm, hubRudp))
	break;
    if (pm.size == 0)
        break;
    job = jobInfoMustParse(pm.data);
    slAddHead(&jobList, job);
    }
slReverse(&jobList);
return jobList;
}

void removeUserJobs(char *user, int argc, char *argv[])
/* Remove jobs associated with user. */
{
struct jobInfo *jobList = getJobList(), *job;
char *wildCard = NULL;
if (!sameString(user, userName) && !sameString("root", userName))
    errAbort("You can only remove your own jobs (unless you are root).");
if (argc > 0) wildCard = argv[0];
slReverse(&jobList);	/* Improves performance to remove from tail first. */
for (job = jobList; job != NULL; job = job->next)
    {
    if (sameString(user, job->user) && 
    	(wildCard == NULL || wildMatch(wildCard, job->command)))
	{
        removeJob(job->id);
	}
    }
jobInfoFreeList(&jobList);
}

void status()
/* Send status command to hub and print. */
{
hubCommandAndPrint("status");
}

void pstat()
/* Send status command to hub and print. */
{
hubCommandAndPrint("pstat");
}

void pstat2()
/* Send status command to hub and print. */
{
hubCommandAndPrint("pstat2");
}


void ping(int count)
/* Ping hub server given number of times. */
{
int i;
for (i=0; i<count; ++i)
    {
    hubCommandCheckReceipt("ping");
    }
printf("Pinged hub %d times\n", count);
}


void parasol(char *command, int argc, char *argv[])
/* parasol - Parasol program - for launching programs in parallel on a computer cluster. */
{
char *subType = argv[0];

hubRudp = rudpMustOpen();
if (sameString(command, "add"))
    {
    if (argc < 1)
        usage();
    if (sameString(subType, "machine"))
	{
	if (argc != 3 && argc != 8)
	    usage();
	addMachine(argc, argv);
	}
    else if (sameString(subType, "job"))
	{
	if (argc < 2)
	    usage();
        addJob(argc-1, argv+1, optionExists("verbose"));
	}
    else if (sameString(subType, "spoke"))
        addSpoke();
    else
        usage();
    }
else if (sameString(command, "remove"))
    {
    if (argc < 2)
        usage();
    if (sameString(subType, "machine"))
	{
	if (argc < 3)
	    usage();
        removeMachine(argv[1],argv[2]);
	}
    else if (sameString(subType, "job"))
        removeJob(argv[1]);
    else if (sameString(subType, "jobs"))
        removeUserJobs(argv[1], argc-2, argv+2);
    else
        usage();
    }
else if (sameString(command, "list"))
    {
    if (argc != 1)
        usage();
    if (sameString(subType, "machine") || sameString(subType, "machines"))
        hubCommandAndPrint("listMachines");
    else if (sameString(subType, "job") || sameString(subType, "jobs"))
	{
        if (optionExists("extended"))
	    hubCommandAndPrint("listJobsExtended");
        else
	    hubCommandAndPrint("listJobs");
	}
    else if (sameString(subType, "user") || sameString(subType, "users"))
        hubCommandAndPrint("listUsers");
    else if (sameString(subType, "batch") || sameString(subType, "batches"))
        hubCommandAndPrint("listBatches");
    else if (sameString(subType, "sick"))
        hubCommandAndPrint("listSick");
    else
        usage();
    }
else if (sameString(command, "pstat"))
    {
    pstat();
    }
else if (sameString(command, "pstat2"))
    {
    pstat2();
    }
else if (sameString(command, "ping"))
    {
    int count = 1;
    if (argc >= 1)
        {
	if (!isdigit(argv[0][0]))
	    usage();
	count = atoi(argv[0]);
	}
    ping(count);
    }
else if (sameString(command, "status"))
    status();
else
    usage();
}

int main(int argc, char *argv[])
/* Process command line. */
{
userName = cloneString(getUser());
optionHashSome(&argc, argv, TRUE);
if (argc < 2)
    usage();
parasol(argv[1], argc-2, argv+2);
return 0;
}
