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
int nextJobId = 0;

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
    for (int i = 1; i < cmd->argc; i++) {
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
  }
  if (cmd->argc == 1) {
    while (p != NULL) {
      // if it is stopped
      if (p->state == 1) {
        // make it running
        p->state = 0;
        p->cmd->bg = 1;
        kill(-(p->pgid), SIGCONT);
      }  
      p = p->next;
    }
  } else {

  }

  return 0;
}

static int tsh_jobs(commandT *cmd)
{
	return 0;
}

static int tsh_fg(commandT *cmd)
{
	return 0;
}

int total_task;
void RunCmd(commandT** cmd, int n)
{
  int i;
  bool last;
  total_task = n;
  if(n == 1)
    RunCmdFork(cmd[0], TRUE);
  else{
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
	int status;
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
			execvp(cmd2->argv[0], cmd2->argv);
		}
		}else{
			//replace stdout
			dup2(p[1],1);
			close(p[0]);
			execvp(cmd1->argv[0], cmd1->argv);
			waitpid(pid1,&status,0);
		}
	}else{
		waitpid(pid, &status,0);
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
  int pid = fork();
  int status;
  if (pid == 0) {
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

    execvp(cmd->argv[0], cmd->argv); 
  } else {
    if (!cmd->bg) {
      waitpid(pid, &status, 0);
    } else {
      printf("[%d] %d\n", nextJobId, pid);
      Job *job = (Job *) malloc(sizeof(Job));
      job->jobId = nextJobId;
      job->pgid = pid;
      job->next = NULL;
      job->cmd = cmd;
      job->state = 0;
      addToBgList(job);
      nextJobId++;
    }
  }
}

static int findBuiltIn(char* cmd)
{
  for (int i = 0; i < NBUILTINCOMMANDS; i++) {
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
  } else {
    Job *temp = jobListHead->next;
    jobListHead = job;
    job->next = temp;
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
      free(jobListHead);
      jobListHead = NULL;
      return;
    }
  }

  Job *p = jobListHead;

  // at lease two jobs
  while (p->next != NULL) {
    Job *next = p->next;
    if (next->pgid == pid) {
      Job *next_next = next->next;
      p->next = next_next;
      free(next);
      return;
    } else {
      p = p->next;
    }
  }
}


static void RunBuiltInCmd(commandT* cmd, int index)
{
  (*FUNCTION_POINTERS[index]) (cmd);
}

void CheckJobs()
{
  // printf("At the very beginning of CheckJobs\n"); 
  if (jobListHead == NULL) {
    return;
  }

  if (jobListHead->next == NULL) {
    if (jobListHead->state == 2) {
      // printf("inside CheckJobs \n");
      printf("[%d]\tDone\t\t\t\t\t%s\n", jobListHead->jobId, jobListHead->cmd->name);
      free(jobListHead);
      jobListHead = NULL;
      return;
    } else {
      //printf("state = %d\n", jobListHead->state);
      //printf("inside else CheckJobs \n");
    }
  }

  Job *p = jobListHead;

  // at leasst two jobs
  while (p != NULL) {
    Job *next = p->next;
    if (next != NULL && next->state == 2) {
      Job *next_next = next->next;
      p->next = next_next;
      printf("[%d]\tDone\t\t\t\t\t%s\n", next->jobId, next->cmd->name);
      free(next);
    }
    p = p->next;
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
