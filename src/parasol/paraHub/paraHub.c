/* paraHub - Parasol hub server.  This is the heart of the parasol system
 * and consists of several threads - sucketSucker, heartbeat, a collection
 * of spokes, as well as the main hub thread.  The system is synchronized
 * around a message queue.
 *
 * The purpose of socketSucker is to move messages from the UDP
 * socket, which has a limited queue size, to the message queue, which
 * can be much larger.  The spoke daemons exist to send messages to compute
 * nodes.  Since sending a message to a node can take a while depending on
 * the network conditions, the multiple spokes allow the system to be
 * delivering messages to multiple nodes simultaniously.  The heartbeat
 * daemon simply sits in a loop adding a heartbeat message to the message
 * queue every 15 seconds or so. The hub thead is responsible for
 * keeping track of everything. The hub thread puts jobs 
 * on the job list, moves machines from the busy list to the free list,  
 * and calls the 'runner' routines, and appends job results to results
 * files in batch directories.
 *
 * The runner routine looks to see if there is a free machine, a free spoke,
 * and a job to run.  If so it will send a message to the spoke telling
 * it to run the job on the machine,  and then move the job from the 'pending'
 * to the 'running' list,  the spoke from the freeSpoke to the busySpoke list, 
 * and the machine from the freeMachine to the busyMachine list.   This
 * indirection of starting jobs via a separate spoke process avoids the
 * hub daemon itself having to wait for a response from a compute node
 * over the network.
 *
 * When a spoke is done assigning a job, the spoke sends a 'recycleSpoke'
 * message to the hub, which puts the spoke back on the freeSpoke list.
 * Likewise when a job is done the machine running the jobs sends a 
 * 'job done' message to the hub, which puts the machine back on the
 * free list,  writes the job exit code to a file, and removes the job
 * from the system.
 *
 * Sometimes a spoke will find that a machine is down.  In this case it
 * sends a 'node down' message to the hub as well as the 'spoke free'
 * message.   The hub will then move the machine to the deadMachines list,
 * and put the job back on the top of the pending list.
 *
 * The heartbeat messages stimulate the hub to do various background
 * chores.  When the hub gets a heartbeat message it
 * does a few things:
 *     o - It calls runner to try and start some more jobs.  (Runner
 *         is also called at the end of processing a recycleSpoke, 
 *         jobDone, addJob or addMachine message.  Typically runner
 *         won't find anything new to run in the heartbeat, but this
 *         is put here mostly just in case of unforseen issues.)
 *    o -  It calls graveDigger, a routine which sees if machines
 *         on the dead list have come back to life.
 *    o -  It calls hangman, a routine which sees if jobs the system
 *         thinks have been running for a long time are still 
 *         running on the machine they have been assigned to.
 *         If the machine has gone down it is moved to the dead list
 *         and the job is reassigned. 
 */

#include "paraCommon.h"
#include "options.h"
#include "linefile.h"
#include "hash.h"
#include "errabort.h"
#include "dystring.h"
#include "dlist.h"
#include "net.h"
#include "internet.h"
#include "paraHub.h"
#include "machSpec.h"

int version = 8;	/* Version number. */

/* Some command-line configurable quantities and their defaults. */
int jobCheckPeriod = 10;	/* Minutes between checking running jobs. */
int machineCheckPeriod = 20; /* Minutes between checking dead machines. */
int assumeDeadPeriod = 60;   /* If haven't heard from job in this long assume
                                 * machine running it is dead. */
int initialSpokes = 30;		/* Number of spokes to start with. */
unsigned char hubSubnet[4] = {255,255,255,255};   /* Subnet to check. */
int nextJobId = 0;		/* Next free job id. */
time_t startupTime;		/* Clock tick of paraHub startup. */

void usage()
/* Explain usage and exit. */
{
errAbort("paraHub - parasol hub server version %d\n"
         "usage:\n"
	 "    paraHub machineList\n"
	 "Where machine list is a file with machine names in the\n"
	 "first column, and number of CPUs in the second column.\n"
	 "options:\n"
	 "    spokes=N  Number of processes that feed jobs to nodes - default %d\n"
	 "    jobCheckPeriod=N  Minutes between checking on job - default %d\n"
	 "    machineCheckPeriod=N Minutes between checking on machine - default %d\n"
	 "    subnet=XXX.YYY.ZZZ Only accept connections from subnet (example 192.168)\n"
	 "    nextJobId=N  Starting job ID number\n"
	 "    logFacility=facility log to the specified syslog facility.\n"
	 "    noResume  Don't try to reconnect with jobs running on nodes\n"
	               ,
	 version, initialSpokes, jobCheckPeriod, machineCheckPeriod
	 );
}

struct spoke *spokeList;	/* List of all spokes. */
struct dlList *freeSpokes;      /* List of free spokes. */
struct dlList *busySpokes;	/* List of busy spokes. */
struct dlList *deadSpokes;	/* List of dead spokes. */

struct machine *machineList; /* List of all machines. */
struct dlList *freeMachines;     /* List of machines ready for jobs. */
struct dlList *busyMachines;     /* List of machines running jobs. */
struct dlList *deadMachines;     /* List of machines that aren't running. */

struct dlList *runningJobs;     /* Jobs that are running. */

struct hash *userHash;		/* Hash of all users. */
struct user *userList;		/* List of all users. */
struct batch *batchList;	/* List of all batches. */
struct dlList *queuedUsers;	/* Users with jobs in queue. */
struct dlList *unqueuedUsers;   /* Users with no jobs in queue. */

struct hash *stringHash;	/* Unique strings throughout system go here
                                 * including directory names and results file
				 * names/batch names. */

struct resultQueue *resultQueues; /* Result files. */
int finishedJobCount = 0;		/* Number of finished jobs. */
int crashedJobCount = 0;		/* Number of crashed jobs. */

char *jobIdFileName = "parasol.jid";	/* File name where jobId file is. */
FILE *jobIdFile = NULL;			/* Handle to jobId file. */

char *hubHost;	/* Name of machine running this. */
struct rudp *rudpOut;	/* Our rUDP socket. */

boolean removeJobId(int id);
/* Remove job with given ID. */

void setupLists()
/* Make up machine, spoke, user and job lists - all doubly linked
 * so it is fast to remove items from one list and put them
 * on another. */
{
freeMachines = newDlList();
busyMachines = newDlList();
deadMachines = newDlList();
runningJobs = newDlList();
freeSpokes = newDlList();
busySpokes = newDlList();
deadSpokes = newDlList();
queuedUsers = newDlList();
unqueuedUsers = newDlList();
userHash = newHash(6);
}

struct batch *findBatchInList(struct dlList *list,  char *nameString)
/* Find a batch of jobs in list or return NULL. 
 * nameString must be from stringHash. */
{
struct dlNode *node;
for (node = list->head; !dlEnd(node); node = node->next)
    {
    struct batch *batch = node->val;
    if (nameString == batch->name)
        return batch;
    }
return NULL;
}

struct batch *newBatch(char *nameString, struct user *user)
/* Make new batch.  NameString must be in stringHash already */
{
struct batch *batch;
AllocVar(batch);
slAddHead(&batchList, batch);
AllocVar(batch->node);
batch->node->val = batch;
batch->name = nameString;
batch->user = user;
batch->jobQueue = newDlList();
return batch;
}

struct batch *findBatch(struct user *user, char *name)
/* Find batch of jobs.  If no such batch yet make it. */
{
struct batch *batch;
name = hashStoreName(stringHash, name);
batch = findBatchInList(user->curBatches, name);
if (batch == NULL)
     {
     batch = findBatchInList(user->oldBatches, name);
     if (batch != NULL)
	 dlRemove(batch->node);
     else
	 batch = newBatch(name, user);
     dlAddTail(user->curBatches, batch->node);
     }
return batch;
}

struct user *findUser(char *name)
/* Find user.  If it's the first time we've seen this
 * user then make up a user object and put it on the
 * idle user list. */
{
struct user *user = hashFindVal(userHash, name);
if (user == NULL)
    {
    AllocVar(user);
    slAddHead(&userList, user);
    hashAddSaveName(userHash, name, user, &user->name);
    AllocVar(user->node);
    user->node->val = user;
    dlAddTail(unqueuedUsers, user->node);
    user->curBatches = newDlList();
    user->oldBatches = newDlList();
    }
return user;
}


int userQueuedCount(struct user *user)
/* Count up jobs user has waiting */
{
struct dlNode *node;
struct batch *batch;
int count = 0;
for (node = user->curBatches->head; !dlEnd(node); node = node->next)
    {
    batch = node->val;
    count += dlCount(batch->jobQueue);
    }
return count;
}


struct user *findLuckyUser()
/* Find lucky user who gets to run a job. */
{
struct user *minUser = NULL;
int minCount = BIGNUM;
struct dlNode *node;
for (node = queuedUsers->head; !dlEnd(node); node = node->next)
    {
    struct user *user = node->val;
    if (!dlEmpty(user->curBatches) && user->runningCount < minCount)
        {
	minCount = user->runningCount;
	minUser = user;
	}
    }
return minUser;
}

struct batch *findLuckyBatch(struct user *user)
/* Find the batch that gets to run a job. */
{
struct batch *minBatch = NULL;
int minCount = BIGNUM;
struct dlNode *node;
for (node = user->curBatches->head; !dlEnd(node); node = node->next)
    {
    struct batch *batch = node->val;
    if (batch->runningCount < minCount)
        {
	minCount = batch->runningCount;
	minBatch = batch;
	}
    }
return minBatch;
}

void unactivateBatchIfEmpty(struct batch *batch)
/* If job queue on batch is empty then remove batch from
 * user's active batch list, and possibly user from active
 * user list. */
{
if (dlEmpty(batch->jobQueue))
    {
    struct user *user = batch->user;
    dlRemove(batch->node);
    dlAddTail(user->oldBatches, batch->node);
    /* Check if it's last user batch and if so take them off queue */
    if (dlEmpty(user->curBatches))
	{
	dlRemove(user->node);
	dlAddTail(unqueuedUsers, user->node);
	}
    }
}


boolean runNextJob()
/* Assign next job in pending queue if any to a machine. */
{
struct user *user;
user = findLuckyUser();
if (user != NULL && !dlEmpty(freeMachines) && !dlEmpty(freeSpokes))
    {
    struct dlNode *mNode, *jNode, *sNode;
    struct spoke *spoke;
    struct batch *batch = findLuckyBatch(user);
    struct job *job;
    struct machine *machine;

    /* Get free machine and spoke and move them to busy lists. */
    mNode = dlPopHead(freeMachines);
    dlAddTail(busyMachines, mNode);
    machine = mNode->val;
    machine->lastChecked = now;
    sNode = dlPopHead(freeSpokes);
    dlAddTail(busySpokes, sNode);
    spoke = sNode->val;

    /* Get active batch from user and take job off of it.
     * If it's the last job in the batch move batch to
     * finished list. */
    jNode = dlPopHead(batch->jobQueue);
    dlAddTail(runningJobs, jNode);
    job = jNode->val;
    ++batch->runningCount;
    ++user->runningCount;
    unactivateBatchIfEmpty(batch);

    /* Tell machine, job, and spoke about each other. */
    machine->job = job;
    job->machine = machine;
    job->startTime = job->lastClockIn = now;
    spokeSendJob(spoke, machine, job);
    return TRUE;
    }
else
    return FALSE;
}

void runner(int count)
/* Try to run a couple of jobs. */
{
while (--count >= 0)
    if (!runNextJob())
        break;
}

struct machine *machineNew(char *name, char *tempDir)
/* Create a new machine structure. */
{
struct machine *mach;
AllocVar(mach);
mach->name = cloneString(name);
mach->tempDir = cloneString(tempDir);
AllocVar(mach->node);
mach->node->val = mach;
return mach;
}

void machineFree(struct machine **pMach)
/* Delete machine structure. */
{
struct machine *mach = *pMach;
if (mach != NULL)
    {
    freeMem(mach->node);
    freeMem(mach->name);
    freeMem(mach->tempDir);
    freez(pMach);
    }
}

void doAddMachine(char *name, char *tempDir, bits32 ip)
/* Add machine to pool.  If you don't know ip yet just pass
 * in 0 for that argument. */
{
struct machine *mach;
mach = machineNew(name, tempDir);
mach->ip = ip;
dlAddTail(freeMachines, mach->node);
slAddHead(&machineList, mach);
}

void addMachine(char *line)
/* Process message to add machine to pool. */
{
char *name = nextWord(&line);
char *tempDir = nextWord(&line);
if (tempDir != NULL)
    {
    doAddMachine(name, tempDir, 0);
    runner(1);
    }
}

struct machine *findMachine(char *name)
/* Find named machine. */
{
struct machine *mach;
for (mach = machineList; mach != NULL; mach = mach->next)
     {
     if (sameString(mach->name, name))
         return mach;
     }
return NULL;
}

struct machine *findMachineWithJob(char *name, int jobId)
/* Find named machine that is running job.  If jobId is
 * 0, find it regardless of job it's running. */
{
struct machine *mach;
for (mach = machineList; mach != NULL; mach = mach->next)
     {
     if (sameString(mach->name, name))
	 {
	 struct job *job = mach->job;
	 if (jobId == 0)
	     return mach;
	 if (job != NULL && job->id == jobId)
	     return mach;
	 }
     }
return NULL;
}

struct job *jobFind(struct dlList *list, int id)
/* Find node of job with given id on list.  Return NULL if
 * not found. */
{
struct dlNode *el;
struct job *job;
for (el = list->head; !dlEnd(el); el = el->next)
    {
    job = el->val;
    if (job->id == id)
        return job;
    }
return NULL;
}

struct job *findWaitingJob(int id)
/* Find job that's waiting (as opposed to running).  Return
 * NULL if it can't be found. */
{
/* If it's not running look in user job queues. */
struct user *user;
struct job *job = NULL;
for (user = userList; user != NULL; user = user->next)
    {
    struct dlNode *node;
    for (node = user->curBatches->head; !dlEnd(node); node = node->next)
	{
	struct batch *batch = node->val;
	if ((job = jobFind(batch->jobQueue, id)) != NULL)
	    break;
	}
    if (job != NULL)
	break;
    }
return job;
}


void requeueJob(struct job *job)
/* Move job from running queue back to a user pending
 * queue.  This happens when a node is down or when
 * it missed the message about a job. */
{
struct batch *batch = job->batch;
struct user *user = batch->user;
struct machine *mach = job->machine;
if (mach != NULL)
    mach->job = NULL;
job->machine = NULL;
dlRemove(job->node);
dlAddTail(batch->jobQueue, job->node);
batch->runningCount -= 1;
batch->user->runningCount -= 1;
dlRemove(batch->node);
dlAddHead(user->curBatches, batch->node);
dlRemove(user->node);
dlAddHead(queuedUsers, user->node);
}

void removeMachine(char *name)
/* Remove machine from pool. */
{
struct machine *mach;
name = trimSpaces(name);
if ((mach = findMachine(name)) != NULL)
    {
    if (mach->job != NULL)
	requeueJob(mach->job);
    dlRemove(mach->node);
    slRemoveEl(&machineList, mach);
    machineFree(&mach);
    }
}


void machineDown(struct machine *mach)
/* Mark machine as down and move it to dead list. */
{
dlRemove(mach->node);
mach->lastChecked = time(NULL);
mach->isDead = TRUE;
dlAddTail(deadMachines, mach->node);
}

void buryMachine(struct machine *machine)
/* Reassign job that machine is processing and bury machine
 * in dead list. */
{
struct job *job;
if ((job = machine->job) != NULL)
    {
    machine->deadJobId = job->id;
    requeueJob(job);
    }
machineDown(machine);
}

void nodeDown(char *line)
/* Deal with a node going down - move it to dead list and
 * put job back on job list. */
{
struct machine *mach;
char *machName = nextWord(&line);
char *jobIdString = nextWord(&line);
int jobId;

if (jobIdString == NULL)
    return;
jobId = atoi(jobIdString);
if ((mach = findMachineWithJob(machName, jobId)) != NULL)
    buryMachine(mach);
runner(1);
}

char *exeFromCommand(char *cmd)
/* Return executable name (without path) given command line. */
{
static char exe[128];
char *s,*e,*x;
int i, size;
int lastSlash = -1;

/* Isolate first space-delimited word between s and e. */
s = skipLeadingSpaces(cmd);
e = skipToSpaces(cmd);
if (e == NULL) 
    e = s + strlen(s);
size = e - s;

/* Find last '/' in this word if any, and reposition s after it. */
for (i=0; i<size; ++i)
    {
    if (s[i] == '/')
        lastSlash = i;
    }
if (lastSlash > 0)
    s += lastSlash + 1;

/* Copy whats left to string to return . */
size = e - s;
if (size >= sizeof(exe))
    size = sizeof(exe)-1;
memcpy(exe, s, size);
exe[size] = 0;
return exe;
}

struct job *jobNew(char *cmd, char *userName, char *dir, char *in, char *out, 
	char *results)
/* Create a new job structure */
{
struct job *job;
struct user *user = findUser(userName);
struct batch *batch = findBatch(user, results);

AllocVar(job);
AllocVar(job->node);
job->node->val = job;
job->id = ++nextJobId;
job->exe = cloneString(exeFromCommand(cmd));
job->cmd = cloneString(cmd);
job->batch = batch;
job->dir = hashStoreName(stringHash, dir);
job->in = cloneString(in);
job->out = cloneString(out);
return job;
}

void jobFree(struct job **pJob)
/* Free up a job. */
{
struct job *job = *pJob;
if (job != NULL)
    {
    freeMem(job->node);
    freeMem(job->exe);
    freeMem(job->cmd);
    freeMem(job->in);
    freeMem(job->out);
    freeMem(job->err);
    freez(pJob);
    }
}

boolean sendViaSpoke(struct machine *machine, char *message)
/* Send a message to machine via spoke. */
{
struct dlNode *node = dlPopHead(freeSpokes);
struct spoke *spoke;
if (node == NULL)
    {
    logDebug("hub: out of spokes!\n");
    return FALSE;
    }
dlAddTail(busySpokes, node);
spoke = node->val;
spokeSendMessage(spoke, machine, message);
return TRUE;
}

void checkPeriodically(struct dlList *machList, int period, char *checkMessage,
	int spokesToUse)
/* Periodically send checkup messages to machines on list. */
{
struct dlNode *mNode;
struct machine *machine;
char message[512];
int i;

sprintf(message, "%s", checkMessage);
for (i=0; i<spokesToUse; ++i)
    {
    /* If we have some free spokes and some busy machines, and
     * the busy machines haven't been checked for a while, go
     * check them. */
    if (dlEmpty(freeSpokes) || dlEmpty(machList))
        break;
    machine = machList->head->val;
    if (now - machine->lastChecked < period)
        break;
    machine->lastChecked = now;
    mNode = dlPopHead(machList);
    dlAddTail(machList, mNode);
    sendViaSpoke(machine, message);
    }
}

void hangman(int spokesToUse)
/* Check that busy nodes aren't dead.  Also send message for 
 * busy nodes to check in, in case we missed one of their earlier
 * jobDone messages. */
{
int i, period = jobCheckPeriod*MINUTE;
struct dlNode *mNode;
struct machine *machine;
struct job *job;

for (i=0; i<spokesToUse; ++i)
    {
    if (dlEmpty(freeSpokes) || dlEmpty(busyMachines))
        break;
    machine = busyMachines->head->val;
    if (now - machine->lastChecked < period)
        break;
    machine->lastChecked = now;
    mNode = dlPopHead(busyMachines);
    dlAddTail(busyMachines, mNode);
    job = machine->job;
    if (job != NULL)
        {
	if (now - job->lastClockIn >= MINUTE * assumeDeadPeriod)
	    {
	    warn("hub: node %s running %d looks dead, burying\n", machine->name, job->id);
	    buryMachine(machine);
	    }
	else
	    {
	    char message[512];
	    sprintf(message, "check %d", job->id);
	    sendViaSpoke(machine, message);
	    }
	}
    }
}

void graveDigger(int spokesToUse)
/* Check out dead nodes.  Try and resurrect them periodically. */
{
checkPeriodically(deadMachines, MINUTE * machineCheckPeriod, "resurrect", 
	spokesToUse);
}


void flushResults()
/* Flush all results files. */
{
struct resultQueue *rq;
for (rq = resultQueues; rq != NULL; rq = rq->next)
    {
    if (rq->f != NULL)
       fflush(rq->f);
    }
}


void writeResults(char *fileName, char *userName, char *machineName,
	int jobId, char *exe, time_t submitTime, time_t startTime,
	char *errFile, char *cmd,
	char *status, char *uTime, char *sTime)
/* Write out job results to output queue.  This
 * will create the output queue if it doesn't yet
 * exist. */
{
struct resultQueue *rq;
for (rq = resultQueues; rq != NULL; rq = rq->next)
    if (sameString(fileName, rq->name))
        break;
if (rq == NULL)
    {
    AllocVar(rq);
    slAddHead(&resultQueues, rq);
    rq->name = fileName;
    rq->f = fopen(rq->name, "a");
    if (rq->f == NULL)
        warn("hub: couldn't open results file %s", rq->name);
    rq->lastUsed = now;
    }
if (rq->f != NULL)
    {
    fprintf(rq->f, "%s %s %d %s %s %s %lu %lu %lu %s %s '%s'\n",
        status, machineName, jobId, exe, 
	uTime, sTime, 
	submitTime, startTime, now,
	userName, errFile, cmd);
    rq->lastUsed = now;
    }
}

void writeJobResults(struct job *job, char *status,
	char *uTime, char *sTime)
/* Write out job results to output queue.  This
 * will create the output queue if it doesn't yet
 * exist. */
{
struct batch *batch = job->batch;
if (sameString(status, "0"))
    {
    ++finishedJobCount;
    ++batch->doneCount;
    ++batch->user->doneCount;
    }
else
    {
    ++crashedJobCount;
    ++batch->crashCount;
    }
writeResults(batch->name, batch->user->name, job->machine->name,
	job->id, job->exe, job->submitTime, 
	job->startTime, job->err, job->cmd,
	status, uTime, sTime);
}

void resultQueueFree(struct resultQueue **pRq)
/* Free up a results queue, closing file if open. */
{
struct resultQueue *rq = *pRq;
if (rq != NULL)
    {
    carefulCloseWarn(&rq->f);
    freez(pRq);
    }
}


void sweepResults()
/* Get rid of result queues that haven't been accessed for
 * a while. Flush all results. */
{
struct resultQueue *newList = NULL, *rq, *next;
for (rq = resultQueues; rq != NULL; rq = next)
    {
    next = rq->next;
    if (now - rq->lastUsed > 1*MINUTE)
	{
	logDebug("hub: closing results file %s\n", rq->name);
        resultQueueFree(&rq);
	}
    else
        {
	slAddHead(&newList, rq);
	}
    }
slReverse(&newList);
resultQueues = newList;
flushResults();
}

void saveJobId()
/* Save job ID. */
{
rewind(jobIdFile);
writeOne(jobIdFile, nextJobId);
fflush(jobIdFile);
if (ferror(jobIdFile))
    errnoAbort("can't write job id file %s", jobIdFileName);
}

void openJobId()
/* Open file with jobID in it and read jobId.  Bump it
 * by 100000 in case we crashed to avoid reusing job
 * id's, but do reuse every 2 billion. Let command line
 * overwrite this though . */
{
jobIdFile = fopen(jobIdFileName, "r+");
if (jobIdFile != NULL)
    {
    readOne(jobIdFile, nextJobId);
    nextJobId += 100000;
    }
else
    jobIdFile = mustOpen(jobIdFileName, "w");
if (nextJobId < 0)
    nextJobId = 0;
nextJobId = optionInt("nextJobId", nextJobId);
}

void processHeartbeat()
/* Check that system is ok.  See if we can do anything useful. */
{
int spokesToUse;
runner(30);
spokesToUse = dlCount(freeSpokes);
if (spokesToUse > 0)
    {
    spokesToUse >>= 1;
    spokesToUse -= 1;
    if (spokesToUse < 1) spokesToUse = 1;
    graveDigger(spokesToUse);
    hangman(spokesToUse);
    sweepResults();
    saveJobId();
    }
}

boolean sendKillJobMessage(struct machine *machine, int jobId)
/* Send message to compute node to kill job there. */
{
char message[64];
sprintf(message, "kill %d", jobId);
logDebug("hub: %s %s\n", machine->name, message);
if (!sendViaSpoke(machine, message))
    {
    return FALSE;
    }
return TRUE;
}


void nodeAlive(char *line)
/* Deal with message from node that says it's alive.
 * Move it from dead to free list.  The major complication
 * of this occurs if the node was running a job and it
 * didn't really go down, we just lost communication with it.
 * In this case we will have restarted the job elsewhere, and
 * that other copy could be conflicting with the copy of
 * the job the node is still running. */
{
char *name = nextWord(&line), *jobIdString;
int jobId;
struct machine *mach;
struct dlNode *node;
for (node = deadMachines->head; !dlEnd(node); node = node->next)
    {
    mach = node->val;
    if (sameString(mach->name, name) && mach->isDead)
        {
	dlRemove(node);
	dlAddTail(freeMachines, node);
	mach->isDead = FALSE;
	if (mach->deadJobId != 0)
	    {
	    warn("hub: node %s assigned %d came back.\n", 
                 name, mach->deadJobId);
	    while ((jobIdString = nextWord(&line)) != NULL)
	        {
		jobId = atoi(jobIdString);
		if (jobId == mach->deadJobId)
		    {
		    struct job *job;
		    warn("hub: Looks like %s is still keeping track of %d\n", name, jobId);
		    if ((job = findWaitingJob(jobId)) != NULL)
			{
			warn("hub: Luckily rerun of job %d has not yet happened.\n", 
                             jobId);
			dlRemove(job->node);
			dlRemove(mach->node);
			job->machine = mach;
			mach->job = job;
			mach->lastChecked = job->lastClockIn = now;
			dlAddTail(runningJobs, job->node);
			dlAddTail(busyMachines, mach->node);
			}
		    else if ((job = jobFind(runningJobs, jobId)) != NULL)
		        {
			/* Job is running on resurrected machine and another.
			 * Kill it on both since the output it created could
			 * be corrupt at this point.  Then add it back to job
			 * queue. */
			warn("hub: Job %d is running on %s as well.\n", jobId,
                             job->machine->name);
			sendKillJobMessage(mach, job->id);
			sendKillJobMessage(job->machine, job->id);
			requeueJob(job);
			}
		    else
		        {
			/* This case should be very rare.  It should happen when
			 * a node is out of touch for 2 hours, but when it comes
			 * back is running a job that we reran to completion
			 * on another node. */
			warn("hub: Job %d has finished running, there is a conflict.  Data may be corrupted, and it will take a lot of logic to fix.\n", 
                             jobId);
			}
		    }
		}
	    }
	mach->deadJobId = 0;
	runner(1);
	break;
	}
    }
}

void recycleMachine(struct machine *mach)
/* Recycle machine into free list. */
{
mach->job = NULL;
dlRemove(mach->node);
dlAddTail(freeMachines, mach->node);
}

void recycleJob(struct job *job)
/* Remove job from lists and free up memory associated with it. */
{
dlRemove(job->node);
jobFree(&job);
}

void nodeCheckIn(char *line)
/* Deal with check in message from node. */
{
char *machine = nextWord(&line);
char *jobIdString = nextWord(&line);
char *status = nextWord(&line);
int jobId = atoi(jobIdString);
if (status != NULL)
    {
    struct job *job = jobFind(runningJobs, jobId);
    if (job != NULL)
	{
        job->lastClockIn = now;
	}
    if (sameString(status, "free"))
	{
	/* Node thinks it's free, we think it has a job.  Node
	 * must have missed our job assignment... */
	struct user *user = job->batch->user;
	if (job != NULL)
	    {
	    struct machine *mach = job->machine;
	    if (mach != NULL)
	        {
	        dlRemove(mach->node);
	        dlAddTail(freeMachines, mach->node);
		}
	    requeueJob(job);
	    logDebug("hub:  requeueing job in nodeCheckIn\n");
	    runner(1);
	    }
	}
    }
}

void recycleSpoke(char *spokeName)
/* Try to find spoke and put it back on free list. */
{
struct dlNode *node;
struct spoke *spoke;
boolean foundSpoke = FALSE;
for (node = busySpokes->head; !dlEnd(node); node = node->next)
    {
    spoke = node->val;
    if (sameString(spoke->name, spokeName))
        {
	dlRemove(spoke->node);
	dlAddTail(freeSpokes, spoke->node);
	foundSpoke = TRUE;
	break;
	}
    }
if (!foundSpoke)
    warn("Couldn't free spoke %s", spokeName);
else
    runner(1);
}

int addJob(char *userName, char *dir, char *in, char *out, char *results,
	char *command)
/* Add job to queues. */
{
struct job *job;
struct user *user;
struct batch *batch;

job = jobNew(command, userName, dir, in, out, results);
batch = job->batch;
dlAddTail(batch->jobQueue, job->node);
user = batch->user;
dlRemove(user->node);
dlAddTail(queuedUsers, user->node);
job->submitTime = time(NULL);
return job->id;
}

int addJobFromMessage(char *line)
/* Parse out addJob message and add job to queues. */
{
char *userName, *dir, *in, *out, *results, *command;

if ((userName = nextWord(&line)) == NULL)
    return 0;
if ((dir = nextWord(&line)) == NULL)
    return 0;
if ((in = nextWord(&line)) == NULL)
    return 0;
if ((out = nextWord(&line)) == NULL)
    return 0;
if ((results = nextWord(&line)) == NULL)
    return 0;
if (line == NULL || line[0] == 0)
    return 0;
command = line;
return addJob(userName, dir, in, out, results, command);
}

void addJobAcknowledge(char *line, struct paraMessage *pm)
/* Add job.  Line format is <user> <dir> <stdin> <stdout> <results> <command> 
 * Returns job ID or 0 if a problem.  Send jobId back to client. */
{
int id = addJobFromMessage(line);
pmClear(pm);
pmPrintf(pm, "%d", id);
pmSend(pm, rudpOut);
runner(1);
}

void respondToPing(struct paraMessage *pm)
/* Someone want's to know we're alive. */
{
pmSendString(pm, rudpOut, "ok");
processHeartbeat();
}

void finishJob(struct job *job)
/* Recycle job memory and the machine it's running on. */
{
struct machine *mach = job->machine;
struct batch *batch = job->batch;
struct user *user = batch->user;
if (mach != NULL)
    recycleMachine(mach);
batch->runningCount -= 1;
user->runningCount -= 1;
recycleJob(job);
}

boolean removeRunningJob(struct job *job)
/* Remove job - if it's running kill it,  remove from job list. */
{
if (!sendKillJobMessage(job->machine, job->id))
    return FALSE;
finishJob(job);
return TRUE;
}

void removePendingJob(struct job *job)
/* Remove job from pending queue. */
{
struct batch *batch = job->batch;
recycleJob(job);
unactivateBatchIfEmpty(batch);
}

boolean removeJobId(int id)
/* Remove job of a given id. */
{
struct job *job = jobFind(runningJobs, id);
if (job != NULL)
    {
    logDebug("Removing %s's %s\n", job->batch->user->name, job->cmd);
    if (!removeRunningJob(job))
        return FALSE;
    }
else
    {
    job = findWaitingJob(id);
    if (job != NULL)
	{
	logDebug("Pending job %s\n", job->cmd);
	removePendingJob(job);
	}
    }
return TRUE;
}

void removeJobAcknowledge(char *names, struct paraMessage *pm)
/* Remove job of a given name(s). */
{
char *name;
char *retVal = "ok";

while ((name = nextWord(&names)) != NULL)
    {
    /* It is possible for this remove to fail if we
     * run out of spokes at the wrong time.  Currently
     * the para client will just report the problem. */
    if (!removeJobId(atoi(name)))
        {
	retVal = "err";
	break;
	}
    }
pmSendString(pm, rudpOut, retVal);
}


void chillBatch(char *line, struct paraMessage *pm)
/* Stop launching jobs from a batch, but don't disturb
 * running jobs. */
{
char *userName = nextWord(&line);
char *batchName = nextWord(&line);
char *res = "err";
if (batchName != NULL)
    {
    struct user *user = hashFindVal(userHash, userName);
    if (user != NULL)
	{
	struct batch *batch;
	batchName = hashStoreName(stringHash, batchName);
	batch = findBatchInList(user->curBatches, batchName);
	if (batch != NULL)
	    {
	    struct dlNode *el, *next;
	    for (el = batch->jobQueue->head; !dlEnd(el); el = next)
		{
		struct job *job = el->val;
		next = el->next;
		recycleJob(job);	/* This free's el too! */
		}
	    dlRemove(batch->node);
	    dlAddTail(user->oldBatches, batch->node);
	    }
	res = "ok";
	}
    }
pmSendString(pm, rudpOut, res);
}

void jobDone(char *line)
/* Handle job is done message. */
{
struct job *job;
char *id = nextWord(&line);
char *status = nextWord(&line);
char *uTime = nextWord(&line);
char *sTime = nextWord(&line);

if (sTime != NULL)
    {
    job = jobFind(runningJobs, atoi(id));
    if (job != NULL)
	{
	if (job->machine != NULL)
	    job->machine->lastChecked = now;
	writeJobResults(job, status, uTime, sTime);
	finishJob(job);
	runner(1);
	}
    }
}

void listMachines(struct paraMessage *pm)
/* Write list of machines to fd.  Format is one machine per message
 * followed by a blank message. */
{
struct machine *mach;
struct job *job;
for (mach = machineList; mach != NULL; mach = mach->next)
    {
    pmClear(pm);
    pmPrintf(pm, "%-10s ", mach->name);
    job = mach->job;
    if (job != NULL)
        pmPrintf(pm, "%-10s %s", job->batch->user->name, job->cmd);
    else
	{
	if (mach->isDead)
	    pmPrintf(pm, "dead");
	else
	    pmPrintf(pm, "idle");
	}
    pmSend(pm, rudpOut);
    }
pmSendString(pm, rudpOut, "");
}

int countUserActiveBatches(struct user *user)
/* Count active batches for user. */
{
int count = dlCount(user->curBatches);
/* Start with batches with pending jobs. */
struct dlNode *node;

/* Add in batches with running but no pending jobs. */
for (node = user->oldBatches->head; !dlEnd(node); node = node->next)
    {
    struct batch *batch = node->val;
    if (batch->runningCount > 0)
	++count;
    }
return count;
}

void listUsers(struct paraMessage *pm)
/* Write list of users to fd.  Format is one user per line
 * followed by a blank line. */
{
struct user *user;
for (user = userList; user != NULL; user = user->next)
    {
    int totalBatch = dlCount(user->curBatches) + dlCount(user->oldBatches);
    pmClear(pm);
    pmPrintf(pm, "%s ", user->name);
    pmPrintf(pm, 
    	"%d jobs running, %d waiting, %d finished, %d of %d batches active", 
	user->runningCount,  userQueuedCount(user), user->doneCount,
	countUserActiveBatches(user), totalBatch);
    pmSend(pm, rudpOut);
    }
pmSendString(pm, rudpOut, "");
}

void writeOneBatchInfo(struct paraMessage *pm, struct user *user, struct batch *batch)
/* Write out info on one batch. */
{
char shortBatchName[512];
splitPath(batch->name, shortBatchName, NULL, NULL);
pmClear(pm);
pmPrintf(pm, "%-8s %4d %6d %6d %5d %s",
	user->name, batch->runningCount, 
	dlCount(batch->jobQueue), batch->doneCount,
	batch->crashCount, shortBatchName);
pmSend(pm, rudpOut);
}

void listBatches(struct paraMessage *pm)
/* Write list of batches.  Format is one batch per
 * line followed by a blank line. */
{
struct user *user;
pmSendString(pm, rudpOut, "#user     run   wait   done crash batch");
for (user = userList; user != NULL; user = user->next)
    {
    struct dlNode *bNode;
    for (bNode = user->curBatches->head; !dlEnd(bNode); bNode = bNode->next)
        {
	writeOneBatchInfo(pm, user, bNode->val);
	}
    for (bNode = user->oldBatches->head; !dlEnd(bNode); bNode = bNode->next)
        {
	struct batch *batch = bNode->val;
	if (batch->runningCount > 0)
	    writeOneBatchInfo(pm, user, batch);
	}
    }
pmSendString(pm, rudpOut, "");
}

void appendLocalTime(struct paraMessage *pm, time_t t)
/* Append time t converted to day/time format to dy. */
{
struct tm *tm;
tm = localtime(&t);
pmPrintf(pm, "%02d/%02d/%d %02d:%02d:%02d",
   tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

char *upToFirstDot(char *s, bool dotQ)
/* Return string up to first dot. */
{
static char ret[128];
int size;
char *e = strchr(s, '.');
if (e == NULL)
    size = strlen(s);
else
    size = e - s;
if (size >= sizeof(ret)-2)	/* Leave room for .q */
    size = sizeof(ret)-3;
memcpy(ret, s, size);
ret[size] = 0;
if (dotQ)
    strcat(ret, ".q");
return ret;
}

boolean oneJobList(struct paraMessage *pm, struct dlList *list, 
	boolean sinceStart)
/* Write out one job list. Return FALSE if there is a problem. */
{
struct dlNode *el;
struct job *job;
char *machName;
for (el = list->head; !dlEnd(el); el = el->next)
    {
    job = el->val;
    if (job->machine != NULL)
        machName = upToFirstDot(job->machine->name, FALSE);
    else
	machName = "none";
    pmClear(pm);
    pmPrintf(pm, "%-4d %-10s %-10s ", job->id, machName, job->batch->user->name);
    if (sinceStart)
        appendLocalTime(pm, job->startTime);
    else
        appendLocalTime(pm, job->submitTime);
    pmPrintf(pm, " %s", job->cmd);
    if (!pmSend(pm, rudpOut))
        return FALSE;
    }
return TRUE;
}

void listJobs(struct paraMessage *pm)
/* Write list of jobs. Format is one job per message
 * followed by a blank message. */
{
struct user *user;
struct dlNode *bNode;
struct batch *batch;

if (!oneJobList(pm, runningJobs, TRUE))
    return;
for (user = userList; user != NULL; user = user->next)
    {
    for (bNode = user->curBatches->head; !dlEnd(bNode); bNode = bNode->next)
        {
	batch = bNode->val;
	if (!oneJobList(pm, batch->jobQueue, FALSE))
	    return;
	}
    }
pmSendString(pm, rudpOut, "");
}

boolean onePstatList(struct paraMessage *pm, struct dlList *list, boolean running)
/* Write out one job list in pstat format.  Return FALSE if there is
 * a problem. */
{
struct dlNode *node;
struct job *job;
time_t t;
char *machName;
char *s;
char *state = (running ? "r" : "q");

flushResults();
for (node = list->head; !dlEnd(node); node = node->next)
    {
    job = node->val;
    if (job->machine != NULL)
	machName = job->machine->name;
    else
        machName = "none";
    if (running)
        t = job->startTime;
    else
        t = job->submitTime;
    pmClear(pm);
    pmPrintf(pm, "%s %d %s %s %lu %s", 
        state, job->id, job->batch->user->name, job->exe, t, machName);
    if (!pmSend(pm, rudpOut))
        return FALSE;
    }
return TRUE;
}

void pstat(struct paraMessage *pm)
/* Write list of jobs in pstat format. */
{
struct user *user;
struct dlNode *bNode;
struct batch *batch;
if (!onePstatList(pm, runningJobs, TRUE))
    return;
for (user = userList; user != NULL; user = user->next)
    {
    for (bNode = user->curBatches->head; !dlEnd(bNode); bNode = bNode->next)
        {
	batch = bNode->val;
	if (!onePstatList(pm, batch->jobQueue, FALSE))
	    return;
	}
    }
pmSendString(pm, rudpOut, "");
}

int sumPendingJobs()
/* Return sum of all pending jobs for all users. */
{
struct user *user;
int count = 0;

for (user = userList; user != NULL; user = user->next)
    count += userQueuedCount(user);
return count;
}

int countActiveUsers()
/* Return count of users with jobs running or in queue */
{
struct user *user;
int count = 0;

for (user = userList; user != NULL; user = user->next)
    {
    if (user->runningCount > 0 || !dlEmpty(user->curBatches))
        ++count;
    }
return count;
}

int countActiveBatches()
/* Return count of active batches */
{
int count = 0;
struct user *user;

for (user = userList; user != NULL; user = user->next)
    count += countUserActiveBatches(user);
return count;
}

void status(struct paraMessage *pm)
/* Write summary status.  Format is one line per message
 * followed by a blank message. */
{
char buf[256];
sprintf(buf, "CPUs free: %d", dlCount(freeMachines));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "CPUs busy: %d", dlCount(busyMachines));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "CPUs dead: %d", dlCount(deadMachines));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Jobs running:  %d", dlCount(runningJobs));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Jobs waiting:  %d", sumPendingJobs());
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Jobs finished: %d", finishedJobCount);
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Jobs crashed:  %d", crashedJobCount);
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Spokes free: %d", dlCount(freeSpokes));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Spokes busy: %d", dlCount(busySpokes));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Spokes dead: %d", dlCount(deadSpokes));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Active batches: %d", countActiveBatches());
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Total batches: %d", slCount(batchList));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Active users: %d", countActiveUsers());
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Total users: %d", slCount(userList));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Days up: %f", (now - startupTime)/(3600.0 * 24.0));
pmSendString(pm, rudpOut, buf);
sprintf(buf, "Version: %d", version);
pmSendString(pm, rudpOut, buf);
pmSendString(pm, rudpOut, "");
}

void addSpoke()
/* Start up a new spoke and add it to free list. */
{
struct spoke *spoke;
spoke = spokeNew();
if (spoke != NULL)
    {
    slAddHead(&spokeList, spoke);
    dlAddTail(freeSpokes, spoke->node);
    }
}

void killSpokes()
/* Kill all spokes. */
{
struct spoke *spoke, *next;
for (spoke = spokeList; spoke != NULL; spoke = next)
    {
    next = spoke->next;
    dlRemove(spoke->node);
    spokeFree(&spoke);
    }
}

void startSpokes()
/* Start default number of spokes. */
{
int i;
for (i=0; i<initialSpokes; ++i)
    addSpoke();
}

void startMachines(char *fileName)
/* If they give us a beginning machine list use it here. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[7];
while (lineFileRow(lf, row))
    {
    struct machSpec ms;
    int i;
    bits32 ip;
    machSpecStaticLoad(row, &ms);
    ip = internetHostIp(ms.name);
    for (i=0; i<ms.cpus; ++i)
	doAddMachine(ms.name, ms.tempDir, ip);
    }
lineFileClose(&lf);
}

struct multiMachine 
/* A machine with multiple CPUs.   A little kludge
 * for now to cope with most of system thinking a 
 * machine is a cpu. */
    {
    struct multiMachine *next;
    char *name;			/* Name, not allocated here. */
    struct slRef *cpuList;	/* Machine valued list. */
    };

void multiMachineFree(struct multiMachine **pMm)
/* Free a multiMachine */
{
struct multiMachine *mm = *pMm;
if (mm != NULL)
    {
    slFreeList(&mm->cpuList);
    freez(&mm);
    }
}

void multiMachineFreeList(struct multiMachine **pList)
/* Free list of multiMachines */
{
struct multiMachine *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    multiMachineFree(&el);
    }
*pList = NULL;
}

void multiMachineDown(struct multiMachine *mm)
/* Note all cpu's are down for machine */
{
struct slRef *ref;
for (ref = mm->cpuList; ref != NULL; ref = ref->next)
    machineDown(ref->val);
}


struct existingResults
/* Keep track of old results we need to integrate into */
    {
    struct existingResults *next;
    char *fileName;	  /* Name of file this is in, not allocated here */
    struct hash *hash;    /* Hash keyed by ascii jobId indicated job results
                           * already recorded. */
    };

void existingResultsFree(struct existingResults **pEr)
/* Free up existing results structure */
{
struct existingResults *er = *pEr;
if (er != NULL)
    {
    freeHash(&er->hash);
    freez(pEr);
    }
}

void existingResultsFreeList(struct existingResults **pList)
/* Free list of existingResults */
{
struct existingResults *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    existingResultsFree(&el);
    }
*pList = NULL;
}


void readResults(char *fileName, struct hash *hash)
/* Read jobId's of results into hash */
{
struct lineFile *lf = lineFileMayOpen(fileName, TRUE);
char *row[3];
char *line;
int wordCount;
if (lf == NULL)
     {
     warn("Couldn't open results file %s\n", fileName);
     return;
     }
while (lineFileNext(lf, &line, NULL))
     {
     wordCount = chopLine(line, row);
     if (wordCount == 0 || row[0][0] == '#')
         continue;
     if (wordCount < 3)
	 {
         warn("Short line %d of %s", lf->lineIx, lf->fileName);
	 continue;
	 }
     if (!isdigit(row[2][0]))
         {
	 warn("Expecting number field 3 line %d of %s", lf->lineIx, lf->fileName);
	 break;
	 }
     hashAdd(hash, row[2], NULL);
     }
lineFileClose(&lf);
}

struct existingResults *getExistingResults(char *fileName, struct hash *erHash,
	struct existingResults **pErList)
/* Get results from hash if we've seen them before, otherwise
 * read them in, save in hash, and return them. */
{
struct existingResults *er = hashFindVal(erHash, fileName);
if (er == NULL)
    {
    AllocVar(er);
    slAddHead(pErList, er);
    hashAddSaveName(erHash, fileName, er, &er->fileName);
    er->hash = newHash(18);
    readResults(fileName, er->hash);
    }
return er;
}

struct machine *findFreeCpuInMulti(struct multiMachine *mm)
/* Return a free cpu or NULL. */
{
struct slRef *ref;
struct machine *mach;
for (ref = mm->cpuList; ref != NULL; ref = ref->next)
    {
    mach = ref->val;
    if (mach->job == NULL)
        return mach;
    }
return NULL;
}

void addRunningJob(struct runJobMessage *rjm, char *resultFile, 
	struct multiMachine *mm)
/* Add job that is already running to queues. */
{
struct machine *mach = findFreeCpuInMulti(mm);
if (mach == NULL)
    warn("%s seems to have more jobs running than it has cpus", mm->name);
else
    {
    struct job *job = jobNew(rjm->command, rjm->user, rjm->dir, rjm->in,
	    rjm->out, resultFile);
    struct batch *batch = job->batch;
    struct user *user = batch->user;
    job->id = atoi(rjm->jobIdString);
    ++batch->runningCount;
    ++user->runningCount;
    dlRemove(batch->node);
    dlAddTail(user->oldBatches, batch->node);
    mach->job = job;
    job->machine = mach;
    dlAddTail(runningJobs, job->node);
    dlRemove(mach->node);
    dlAddTail(busyMachines, mach->node);
    mach->lastChecked = job->submitTime = job->startTime = job->lastClockIn = now;
    }
}

void pljErr(struct multiMachine *mm, int no)
/* Print out error message in the middle of routine below. */
{
warn("%s: truncated listJobs response %d\n", mm->name, no);
}

void getExeOnly(char *command, char exe[256])
/* Extract executable file (not including path) from command line. */
{
/* Extract name of executable file with no path. */
char *dupeCommand = cloneString(command);
char *exePath = firstWordInLine(dupeCommand);
char exeFile[128], exeExt[64];
splitPath(exePath, NULL, exeFile, exeExt);
sprintf(exe, "%s%s", exeFile, exeExt);
freez(&dupeCommand);
}

void writeExistingResults(char *fileName, char *line, struct machine *mach, 
	struct runJobMessage *rjm)
{
char err[512], exe[256];
int jobId = atoi(rjm->jobIdString);
char *status = nextWord(&line);
char *uTime = nextWord(&line);
char *sTime = nextWord(&line);

if (sTime == NULL)
    {
    warn("Bad line format in writeExistingResults for %s", mach->name);
    return;
    }


getExeOnly(rjm->command, exe);
fillInErrFile(err, jobId, mach->tempDir);
fileName = hashStoreName(stringHash, fileName);

writeResults(fileName, rjm->user, mach->name, 
	jobId, exe, now, now,
	err, rjm->command, 
	status, uTime, sTime);
}

boolean processListJobs(struct multiMachine *mm, 
	struct paraMessage *pm, struct rudp *ru, 
	struct hash *erHash, struct existingResults **pErList,
	int *pRunning, int *pFinished)
/* Process response to list jobs message. Read jobs node is running and
 * has recently finished.  Add running ones to job list. Add finished
 * ones to results file if necessary.
 *
 * Format of message is
 *     running count
 *     one line for each running job.
 *     recent count
 *     two lines for each recent job.
 */
{
int running, recent, i, finCount = 0;
struct runJobMessage rjm;
char resultsFile[512], *resultsFileString;

if (!pmReceiveTimeOut(pm, ru, 2000000))
    {
    warn("%s: no listJobs response", mm->name);
    return FALSE;
    }
running = atoi(pm->data);
for (i=0; i<running; ++i)
    {
    if (!pmReceiveTimeOut(pm, ru, 2000000))
        {
	pljErr(mm, 1);
	return FALSE;
	}
    if (!parseRunJobMessage(pm->data, &rjm))
        {
	pljErr(mm, 2);
	return FALSE;
	}
    snprintf(resultsFile, sizeof(resultsFile), "%s/%s", rjm.dir, "para.results");
    addRunningJob(&rjm, resultsFile, mm);
    }
*pRunning += running;
if (!pmReceiveTimeOut(pm, ru, 2000000))
    {
    pljErr(mm, 3);
    return FALSE;
    }
recent = atoi(pm->data);
for (i=0; i<recent; ++i)
    {
    struct existingResults *er;
    char *startLine = NULL;
    if (!pmReceiveTimeOut(pm, ru, 2000000))
        {
	pljErr(mm, 4);
	return FALSE;
	}
    startLine = cloneString(pm->data);;
    if (!parseRunJobMessage(startLine, &rjm))
        {
	pljErr(mm, 5);
	freez(&startLine);
	return FALSE;
	}
    if (!pmReceiveTimeOut(pm, ru, 2000000))
        {
	pljErr(mm, 6);
	freez(&startLine);
	return FALSE;
	}
    snprintf(resultsFile, sizeof(resultsFile), "%s/%s", rjm.dir, "para.results");
    er = getExistingResults(resultsFile, erHash, pErList);
    if (!hashLookup(er->hash, rjm.jobIdString))
        {
	struct machine *mach = mm->cpuList->val;
	writeExistingResults(resultsFile, pm->data, mach, &rjm);
	++finCount;
	}
    freez(&startLine);
    }
*pFinished += finCount;
return TRUE;
}

void checkForJobsOnNodes()
/* Poll nodes and see if they have any jobs for us. */
{
struct machine *mach;
char *line;
int running = 0, finished = 0;
struct hash *erHash = newHash(8);	/* A hash of existingResults */
struct existingResults *erList = NULL, *er;
struct hash *mmHash = newHash(0);	/* Hash of machines. */
struct multiMachine *mmList = NULL, *mm;

printf("Checking for jobs already running on nodes\n");
for (mach = machineList; mach != NULL; mach = mach->next)
    {
    mm = hashFindVal(mmHash, mach->name);
    mach->lastChecked = now;
    if (mm == NULL)
        {
	AllocVar(mm);
	slAddHead(&mmList, mm);
	hashAddSaveName(mmHash, mach->name, mm, &mm->name);
	}
    refAdd(&mm->cpuList, mach);
    }

for (mm = mmList; mm != NULL; mm = mm->next)
    {
    struct paraMessage pm;
    struct rudp *ru = rudpNew(rudpOut->socket);	/* Get own resend timing */
    printf("%s\n", mm->name);
    pmInitFromName(&pm, mm->name, paraNodePort);
    if (!pmSendString(&pm, ru, "listJobs"))
        {
	multiMachineDown(mm);
	continue;
	}
    if (!processListJobs(mm, &pm, rudpOut, erHash, &erList, &running, &finished))
	multiMachineDown(mm);
    rudpFree(&ru);
    }

/* Clean up time. */
multiMachineFreeList(&mmList);
hashFree(&mmHash);
existingResultsFreeList(&erList);
hashFree(&erHash);

/* Report results. */
printf("%d running jobs, %d jobs that finished while hub was down\n",
	running, finished);
}

void startHub(char *machineList)
/* Do hub daemon - set up socket, and loop around on it until we get a quit. */
{
struct sockaddr_in sai;
char *line, *command;
struct rudp *rudpIn = NULL;

/* Note startup time. */
findNow();
startupTime = now;

/* Find name and IP address of our machine. */
hubHost = getMachine();
logOpen("paraHub", optionVal("logFacility", NULL));
logInfo("Starting paraHub on %s\n", hubHost);

/* Set up various lists. */
hubMessageQueueInit();
stringHash = newHash(0);
setupLists();
startMachines(machineList);

/* Initialize socket etc. */
ZeroVar(&sai);
sai.sin_family = AF_INET;
sai.sin_port = htons(paraHubPort);
sai.sin_addr.s_addr = INADDR_ANY;
rudpIn = rudpMustOpenBound(&sai);
rudpOut = rudpMustOpen();

/* Start up daemons. */
sockSuckStart(rudpIn);
startHeartbeat();
startSpokes();


if (!optionExists("noResume"))
    checkForJobsOnNodes();

openJobId();
printf("Starting paraHub. Next job ID is %d.\n", nextJobId);

/* Bump up our priority to just shy of real-time. */
nice(-40);

/* Main event loop. */
for (;;)
    {
    struct paraMessage *pm = hubMessageGet();
    findNow();
    line = pm->data;
    logDebug("hub: %s\n", line);
    command = nextWord(&line);
    if (sameWord(command, "jobDone"))
	 jobDone(line);
    else if (sameWord(command, "recycleSpoke"))
	 recycleSpoke(line);
    else if (sameWord(command, "heartbeat"))
	 processHeartbeat();
    else if (sameWord(command, "addJob"))
	 addJobAcknowledge(line, pm);
    else if (sameWord(command, "nodeDown"))
	 nodeDown(line);
    else if (sameWord(command, "alive"))
	 nodeAlive(line);
    else if (sameWord(command, "checkIn"))
	 nodeCheckIn(line);
    else if (sameWord(command, "removeJob"))
	 removeJobAcknowledge(line, pm);
    else if (sameWord(command, "chill"))
	 chillBatch(line, pm);
    else if (sameWord(command, "ping"))
	 respondToPing(pm);
    else if (sameWord(command, "addMachine"))
	 addMachine(line);
    else if (sameWord(command, "removeMachine"))
	 removeMachine(line);
    else if (sameWord(command, "listJobs"))
	 listJobs(pm);
    else if (sameWord(command, "listMachines"))
	 listMachines(pm);
    else if (sameWord(command, "listUsers"))
	 listUsers(pm);
    else if (sameWord(command, "listBatches"))
	 listBatches(pm);
    else if (sameWord(command, "status"))
	 status(pm);
    else if (sameWord(command, "pstat"))
	 pstat(pm);
    else if (sameWord(command, "addSpoke"))
	 addSpoke();
    if (sameWord(command, "quit"))
	 break;
    pmFree(&pm);
    }
endHeartbeat();
killSpokes();
saveJobId();
#ifdef SOON
#endif /* SOON */
}

void notGoodSubnet(char *sns)
/* Complain about subnet format. */
{
errAbort("'%s' is not a properly formatted subnet.  Subnets must consist of\n"
         "one to three dot-separated numbers between 0 and 255\n");
}

void fillInSubnet()
/* Parse subnet paramenter if any into subnet variable. */
{
char *sns = optionVal("subnet", NULL);
if (sns == NULL)
    sns = optionVal("subNet", NULL);
if (sns != NULL)
    {
    char *snsCopy = strdup(sns);
    char *words[5];
    int wordCount, i;
    wordCount = chopString(snsCopy, ".", words, ArraySize(words));
    if (wordCount > 3 || wordCount < 1)
        notGoodSubnet(sns);
    for (i=0; i<wordCount; ++i)
	{
	char *s = words[i];
	int x;
	if (!isdigit(s[0]))
	    notGoodSubnet(sns);
	x = atoi(s);
	if (x > 255)
	    notGoodSubnet(sns);
	hubSubnet[i] = x;
	}
    freez(&snsCopy);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 2)
    usage();
jobCheckPeriod = optionInt("jobCheckPeriod", jobCheckPeriod);
machineCheckPeriod = optionInt("machineCheckPeriod", machineCheckPeriod);
initialSpokes = optionInt("spokes",  initialSpokes);
fillInSubnet();
startHub(argv[1]);
return 0;
}


