/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2006/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:25:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))
Job *jobListHead = NULL;
Job *jobListTail = NULL;

int nextJobId = 1;

commandT *fgCmd = NULL;

/*alias list implemented by linked list*/
aliasL* head;
aliasL* tail;
/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*, int index);
/* return the index of if char * is a builtin command, otherwise -1 */
static int findBuiltIn(char*);
/* function for builtin cd */
static int tsh_cd(commandT *cmd);
/* function for builtin echo */
static int tsh_echo(commandT *cmd);
/* function for builtin alias and unalias*/
static int tsh_alias(commandT *cmd);
static int tsh_unalias(commandT *cmd);
/*function for bg, jobs, fg*/
static int tsh_bg(commandT *cmd);
static int tsh_jobs(commandT *cmd);
static int tsh_fg(commandT *cmd);
/* function for add to job list */
void addToBgList(Job *job);
/* function for removing from job list */
void removeFromJobList(pid_t pid);
/* print command */
void printCommand(commandT*);
/* wait for foregournd process */
void waitFg(pid_t);
/************External Declaration*****************************************/

/************Constants for builtin commands******************************/
static const char *BuiltInCommands[] = {
  "cd",
  "echo",
  "alias",
  "unalias",
  "bg",
  "jobs",
  "fg"
};

/* 
 * array of function pointers
 */
static int (*FUNCTION_POINTERS[]) (commandT *cmd) = {
  tsh_cd,
  tsh_echo,
  tsh_alias,
  tsh_unalias,
  tsh_bg,
  tsh_jobs,
  tsh_fg
};

/**************Implementation***********************************************/
static int tsh_cd(commandT *cmd)
{
  if (cmd->argc == 1) {
    chdir(getenv("HOME"));
  } else {
    chdir(cmd->argv[1]);
  }

  return 0;
}

// no sure if newline character will be deleted after interpretation.
static int tsh_echo(commandT *cmd)
{
  if (cmd->argc == 1) {
    printf("\n");
  } else {
    int i;
    for (i = 1; i < cmd->argc; i++) {
      printf("%s", cmd->argv[i]);
      if (i != cmd->argc - 1) {
        printf(" ");
      }
    }
    printf("\n");
  }
  return 0;
}
//alias and unalias implementation
static int tsh_alias(commandT *cmd)
{
	aliasL* alias_list;
	if(cmd->argc==1){
	//print the alias list
		alias_list = head;
		while(alias_list){
			printf("alias %s='%s'\n",alias_list->newname,
					alias_list->oldname);
			alias_list=alias_list->next;
		}
	}else if(cmd->argc==2){
		//get the newname and oldname out of expression xxx='xx'
		char* nname = NULL;
		char* oname = NULL;
		char* left = NULL;
		if(strstr(cmd->cmdline,"='")){
			strtok(cmd->cmdline," ");
			nname = strtok(NULL,"='");
			oname = strtok(NULL,"");
			oname++;
			if(strchr(oname,'\'')){
				oname = strtok(oname,"\'");
			}else{
				oname = NULL;
			}
			left = strtok(NULL,"");
		}else if(strstr(cmd->cmdline,"=\"")){
			strtok(cmd->cmdline," ");
			nname = strtok(NULL,"='");
			oname++;
			if(strchr(oname,'\"')){
			oname = strtok(oname,"\"");
			}else{
				oname = NULL;
			}
			left = strtok(NULL,"");
		}
		if(!oname||left){
			printf("invalid alias command\n");
			return 0;
		}
		//whether nname is valid or not
		if((strchr(nname, '='))||(strchr(nname,'$'))||(strchr(nname,'/')))
		{
			printf("invalid alias name\n");
		}else
		{
		bool exist = FALSE,upd=FALSE;
		aliasL * update;
		//whether oname or nname is an existing alias
			alias_list= head;	
			while(alias_list){
				if(strcmp(oname,alias_list->newname)==0){
					printf("You can't expand an existing alias.\n");
					exist = TRUE;
					break;
				}
				if(strcmp(nname,alias_list->newname)==0){
					update  = alias_list;
					upd = TRUE;
				}
				alias_list=alias_list->next;
			}
		//correct input, store the alias into list
		if(!exist){
			//update the oldvalue if redefining the newvalue
			if(upd){
				update->oldname = oname;	
			}else{
			char * sd = "";
			printf("%s",sd);
			aliasL* node = malloc(sizeof(aliasL));
			node->newname = nname;
			node->oldname = oname;
			node->next = NULL;
			if(head==NULL){
				head = node;
				tail = node;
			}else{
				tail->next = node;
				tail = node;
			}	 			
			}
		}
		}
	}else {
		printf("alias format: alias xxx='xx'\n");
	}
	return 0;
}
static int tsh_unalias(commandT *cmd)
{
	if(cmd->argc != 2){
		printf("unalias format: unalias xxx\n");
		return 0;
	}else{
		//search the list
		char * name = cmd->argv[1];
		if(head==NULL){
			return 0;
		}
		aliasL * child = head;
		aliasL * parent = head;
		if(strcmp(head->newname,name)==0){
			if(head == tail){
				head = NULL;
				tail = NULL;
			}else{
				head = head->next;
			}
			free(child);
			return 0;
		}else{
			while(parent->next){
				child = parent->next;
				if(strcmp(child->newname,name)==0){
					//remove form the list
					parent->next=child->next;
					if(child==tail){
						tail = parent;
					}
					free(child);
					return 0;
				}
				parent=child;
			}
		}
	}
	printf("unalias name not defined previously\n");
	return 0;
}

static int tsh_bg(commandT *cmd)
{
  Job *p = jobListHead;
  if (p == NULL) {
    printf("bg: no current job\n");
    return -1;
  }
  if (cmd->argc == 1) {
    while (p != NULL) {
      // if it is stopped
      if (p->state == STOPPED) {
        // make it running
        p->state = RUNNING;
        p->cmd->bg = BACKGROUND;
        kill(-(p->pgid), SIGCONT);
        return 0;
      }  
      p = p->next;
    }
  } else {
    while (p != NULL) {
      if (p->jobId == atoi(cmd->argv[1])) {
        if (p->state == STOPPED) {
          p->state = RUNNING;
          p->cmd->bg = BACKGROUND;
          kill(-(p->pgid), SIGCONT);
          return 0;
        }
      }
      p = p->next;
    }
  }

  return 0;
}

static int tsh_jobs(commandT *cmd)
{
  Job *p = jobListTail;
  while (p != NULL) {
    switch (p->state) {
      case RUNNING: printf("[%d]   Running                 ", p->jobId); printCommand(p->cmd); printf("&"); printf("\n"); break;
      case STOPPED: printf("[%d]   Stopped                 ", p->jobId); printCommand(p->cmd); printf("\n"); break;
    }
    p = p->pre;
  }
  return 0;
}

static int tsh_fg(commandT *cmd)
{
  Job *p = jobListHead;
  if (p == NULL) {
    printf("fg: current: no such job\n");
    return -1;
  }

  pid_t pgid;
  if (cmd->argc == 1) {
    // first bring stopped process to foreground
    while (p != NULL) {
      if (p->state == STOPPED) {
        pgid = p->pgid;
        kill(-pgid, SIGCONT);
        fgCmd = p->cmd;
        tcsetpgrp(STDOUT_FILENO, pgid);
        waitFg(pgid);
        tcsetpgrp(STDOUT_FILENO, getpid());
        removeFromJobList(pgid);
        return 0;
      }
      p = p->next;
    }

    // if there is none, then background running process
    p = jobListHead;
    pgid = p->pgid;
    fgCmd = p->cmd;
    tcsetpgrp(STDOUT_FILENO, pgid);
    waitFg(pgid);
    tcsetpgrp(STDOUT_FILENO, getpid());
    removeFromJobList(pgid);
    return 0;
  } else {
    while (p != NULL) {
      if (p->jobId == atoi(cmd->argv[1])) {
        pgid = p->pgid;
        if (p->state == STOPPED) {
          kill(-pgid, SIGCONT);
        }
        fgCmd = p->cmd;
        tcsetpgrp(STDOUT_FILENO, pgid);
        waitFg(pgid);
        tcsetpgrp(STDOUT_FILENO, getpid());
        removeFromJobList(pgid);
        return 0;
      }
      p = p->next;
    }
  }
  return 0;
}

int total_task;
void RunCmd(commandT** cmd, int n)
{
  int i;
  bool last;
  total_task = n;
  if(n == 1){
    RunCmdFork(cmd[0], TRUE);
  }else{
//remove the limit of # pipes
  for(i=0;i<n-1;i++){
	  //if not the last command, run it in next loop
	if(i!=n-2){
		last = FALSE;
	}else{
		last = TRUE;
	}
	RunCmdPipe(cmd[i],cmd[i+1], last);
	
  }
    for(i = 0; i < n; i++)
      ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc<=0)
    return;
  int index;
  index = findBuiltIn(cmd->argv[0]);
  if (index != -1)
  {
    RunBuiltInCmd(cmd, index);
    ReleaseCmdT(&cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);
  }
}

void RunCmdBg(commandT* cmd)
{
  // TODO
}

void RunCmdPipe(commandT* cmd1, commandT* cmd2, bool last)
{
	int pid = fork(),pid1;
	if(pid==0){
		int p[2];
		pipe(p);
		pid1 = fork();
		if(pid1 == 0){
		//replace stdin
		dup2(p[0],0);
		close(p[1]);
		if(last){
			execvp(cmd2->name, cmd2->argv);
		}
		}else{
			//replace stdout
			dup2(p[1],1);
			close(p[0]);
			execvp(cmd1->name, cmd1->argv);
                        waitFg(pid1);
		}
	}else{
          waitFg(pid);
	}
}

void RunCmdRedirOut(commandT* cmd, char* file)
{
}

void RunCmdRedirIn(commandT* cmd, char* file)
{
}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;
  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
        }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
        buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
        buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf); 
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

// since the forceFork is always true, don't understand why it's there.
static void Exec(commandT* cmd, bool forceFork)
{
  pid_t pid = fork();
  if (pid == 0) {
    setpgid(0, 0);
    // redirect out
    int out_fd;
    if (cmd->is_redirect_out) {
      close(STDOUT_FILENO);
      // 0644 is to grant the owner of the process read, and write permission.
      // When specifying O_CREAT, you need to provide this mode as chmod does.
      out_fd = open(cmd->redirect_out, O_CREAT | O_WRONLY, 0644);
      dup(out_fd);
    }
    
    // redirect in
    int in_fd;
    if (cmd->is_redirect_in) {
      close(STDIN_FILENO);
      if ((in_fd = open(cmd->redirect_in, O_RDONLY)) == -1) {
        PrintPError(NULL);
      }
      dup(in_fd);
    }

    execv(cmd->name, cmd->argv); 
  } else {
    if (cmd->bg == FOREGROUND) {
      fgCmd = cmd;
      signal(SIGTTOU, SIG_IGN);
      signal(SIGTTIN, SIG_IGN);
      tcsetpgrp(STDOUT_FILENO, pid);
      waitFg(pid);
      tcsetpgrp(STDOUT_FILENO, getpid());
    } else {
      // printf("[%d] %d\n", nextJobId, pid);
      Job *job = (Job *) malloc(sizeof(Job));
      job->jobId = nextJobId;
      job->pgid = pid;
      job->next = NULL;
      job->pre = NULL;
      job->cmd = cmd;
      job->state = RUNNING;
      addToBgList(job);
      nextJobId++;
    }
  }
}

static int findBuiltIn(char* cmd)
{
  int i;
  for (i = 0; i < NBUILTINCOMMANDS; i++) {
    if (strcmp(cmd, BuiltInCommands[i]) == 0) {
      return i;
    }
  }

  return -1;
}

void addToBgList(Job *job)
{
  if (jobListHead == NULL) {
    jobListHead = job;
    jobListTail = job;
  } else {
    job->next = jobListHead;
    jobListHead->pre = job;
    jobListHead = job;
  }
}

void removeFromJobList(pid_t pid)
{
  // no job
  if (jobListHead == NULL) {
    return;
  }

  // only one job
  if (jobListHead->next == NULL) {
    if (jobListHead->pgid == pid) {
      //ReleaseCmdT(&(jobListHead->cmd));
      free(jobListHead);
      jobListHead = NULL;
      jobListTail = NULL;
      return;
    }
  }

  Job *p = jobListHead;

  // at lease two jobs
  while (p != NULL) {
    Job *next = p->next;
    if (next != NULL && next->pgid == pid) {
      Job *next_next = next->next;
      p->next = next_next;
      if (next_next != NULL) {
        next_next->pre = p;
      }
      //ReleaseCmdT(&(next->cmd));
      free(next);
      return;
    }
    p = p->next;
  }
}


static void RunBuiltInCmd(commandT* cmd, int index)
{
  (*FUNCTION_POINTERS[index]) (cmd);
}

void CheckJobs()
{
  //printf("At the very beginning of CheckJobs\n"); 
  if (jobListHead == NULL) {
    //printf("no jobs\n");
    return;
  }

  // only one job
  if (jobListHead->next == NULL) {
    if (jobListHead->state == 2) {
      //printf("inside only one job\n");
      printf("[%d]   Done                    ", jobListHead->jobId);
      printCommand(jobListHead->cmd);
      printf("\n");
      ReleaseCmdT(&(jobListHead->cmd));
      free(jobListHead);
      jobListHead = NULL;
      jobListTail = NULL;
      return;
    }
  }

  Job *p = jobListHead;

  // at leasst two jobs
  while (p != NULL) {
    Job *next = p->next;
    if (next != NULL && next->state == 2) {
      //printf("inside two jobs\n");
      Job *next_next = next->next;
      p->next = next_next;
      if (next_next != NULL) {
        next_next->pre = p;
      }
      printf("[%d]   Done                    ", p->jobId);
      printCommand(p->cmd);
      printf("\n");
      ReleaseCmdT(&(next->cmd));
      free(next);
    }
    p = p->next;
  }
}

void printCommand(commandT *cmd)
{
  int i;
  for (i = 0; i < cmd->argc; i++) {
    printf("%s ", cmd->argv[i]);
  }
}

void waitFg(pid_t pid) {
  int status;
  waitpid(pid, &status, WUNTRACED);
  if (WIFSTOPPED(status)) {
    tcsetpgrp(STDOUT_FILENO, getpid()); 
    Job *job = (Job *) malloc(sizeof(Job));
    job->jobId = nextJobId;
    job->pgid = pid;
    job->next = NULL;
    job->pre = NULL;
    job->cmd = fgCmd;
    job->state = STOPPED;
    printf("\n");
    printf("[%d]   Stopped                 ", nextJobId);
    printCommand(fgCmd);
    printf("\n");
    addToBgList(job);
    nextJobId++;
  } else {
    ReleaseCmdT(&fgCmd);
  }
}

commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}
