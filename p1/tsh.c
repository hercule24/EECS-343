// Net IDs: 
//
//
/***************************************************************************
 *  Title: MySimpleShell 
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2006/10/13 05:25:59 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: tsh.c,v $
 *    Revision 1.1  2005/10/13 05:25:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.4  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.3  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __MYSS_IMPL__

/************System include***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80
#define DIRECTORY_LENGTH 256

/************Global Variables*********************************************/

/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */	
static void sig_handler(int);
static void sigchld_handler(int);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

int main (int argc, char *argv[])
{
  /* Initialize command buffer */
  chdir(getenv("HOME"));
  char* cmdLine = malloc(sizeof(char*)*BUFSIZE);

  /* shell initialization */
  if (signal(SIGINT, sig_handler) == SIG_ERR) PrintPError("SIGINT");
  if (signal(SIGTSTP, sig_handler) == SIG_ERR) PrintPError("SIGTSTP");
  if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) PrintPError("SIGCHLD");

  while (!forceExit) /* repeat forever */
  {
    /* This block should be commented out when handin. */
    char buf[DIRECTORY_LENGTH];
    getcwd(buf, DIRECTORY_LENGTH);
    if (strcmp(buf, getenv("HOME")) == 0) {
      printf("tsh:~ $ ");
    } else {
      printf("tsh:%s $ ", getcwd(buf, DIRECTORY_LENGTH));
    }
    /********************************************/

    /* read command line */
    getCommandLine(&cmdLine, BUFSIZE);

    if(strcmp(cmdLine, "exit") == 0)
    {
      forceExit=TRUE;
      continue;
    }

    /* checks the status of background jobs */
    CheckJobs();

    /* interpret command and line
     * includes executing of commands */
    Interpret(cmdLine);

  }

  /* shell termination */
  free(cmdLine);
  return 0;
} /* end main */

static void sig_handler(int signo)
{
}

// reap zombie childe process
// "-1" indicates it will wait for any child process,
// Setting "WNOHANG" prevents this handler from blocking
static void sigchld_handler(int signo)
{
  int status;
  while (waitpid(-1, &status, WNOHANG) > 0) {}
}

