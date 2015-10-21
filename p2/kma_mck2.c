/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the McKusick-Karels
 *              algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 *    Revision 1.3  2004/12/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 
 ************************************************************************/

/************************************************************************
 Project Group: NetID1, NetID2, NetID3
 
 ***************************************************************************/

#define KMA_MCK2
#ifdef KMA_MCK2
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <math.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
#define FREE_LIST_SIZE 14;

typedef struct node
{
  struct node *next;
} Node;

typedef char Byte;
/************Global Variables*********************************************/
Byte *KMEMSIZES = NULL;
Node *FREE_LIST_ARR = NULL;
/************Function Prototypes******************************************/
int roundUp(size_t size);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void* kma_malloc(kma_size_t size)
{
  if (size < sizeof(Node)) {
    size = sizeof(Node);
  }

  if (KMEMSIZES == NULL) {
    kma_page_t *page = get_page();
    *((kma_page_t**) page->ptr) = page;
    KMEMSIZES = (Byte *) ((size_t) page->ptr + sizeof(kma_page_t *));
    FREE_LIST_ARR = (Node *) ((size_t) KMEMSIZES + MAXPAGES);
    size_t remaining_size = PAGESIZE - sizeof(kma_page_t *) - MAXPAGES * sizeof(Byte) - sizeof(size_t) * FREE_LIST_SIZE;
  }
}

int roundUp(size_t size)
{
  int low = 0;
  int high = FREE_LIST_SIZE - 1;
  int middle;
  int p;
  while (low < high - 1) {
    middle = (high + low) / 2;
    p = (int) pow(2, middle);
    if (size > p) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return high;
}

void kma_free(void* ptr, kma_size_t size)
{
  ;
}

#endif // KMA_MCK2
