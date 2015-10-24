/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
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
//#define KMA_MCK2
#ifdef KMA_MCK2
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/
void ** FREE_LIST_HEAD = NULL;
bool INIT = FALSE;
int PAGE_COUNT = 0;
kma_page_t * FREEPAGE = NULL;
/************Function Prototypes******************************************/
void initFreeList();
size_t roundUp(size_t size);
int diff(size_t size);
void* initNewPage(size_t size, void** entry);
void* getBuffer(size_t size, void** entry);
void derefPage(void* page, size_t size);
void insertAtHead(void* ptr, size_t size);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void* kma_malloc(kma_size_t size)
{
  if (!INIT) { //first time using malloc
    initFreeList();
    INIT = TRUE;
  }
  size = roundUp(size);
  if(diff(size) > 8) return NULL; //larger than 8192
  void** entry = FREE_LIST_HEAD + diff(size); 
  if (*entry == NULL) { //allocate a new page and set the whole page same as size
    return initNewPage(size, entry);
  } else { //has free buffers
    return getBuffer(size, entry);
  }
}

void kma_free(void* ptr, kma_size_t size)
{
  size = roundUp(size);
  kma_page_t * page = *((kma_page_t **) BASEADDR(ptr));
  if (diff(size) == 8) { //if 8192, free the page
    free_page(page);
    PAGE_COUNT--;
  } else {
    page->size += size;
    if (page->size == PAGESIZE - sizeof(kma_page_t*)) {
      derefPage(page->ptr, size); //if page is made of free buffers, derefence the buffer in freelist
      free_page(page);
      PAGE_COUNT--;
    } else {  //not all free, give the buffer back to freelist
      insertAtHead(ptr, size);
    }
  }
  //free everything
  if(PAGE_COUNT == 1) {
    free_page(FREEPAGE);
    INIT = FALSE;
    PAGE_COUNT = 0;
    FREE_LIST_HEAD = NULL;
  }
}

void initFreeList() {
  kma_page_t * page = get_page();
  *((kma_page_t**) page->ptr) = page;
  PAGE_COUNT++;
  FREEPAGE = page;
  FREE_LIST_HEAD = page->ptr + sizeof(void*); 
  * FREE_LIST_HEAD = NULL;
  for (int i = 1; i <= 8; i++) { //32 - 4096
    *(FREE_LIST_HEAD + i) = NULL;
  }
}

size_t roundUp(size_t size) {
  size_t n = 1 << 5;
  while (size > n) {
    n <<= 1;
  }
  return n;
}

int diff(size_t size) {
  size_t base = 1 << 5;
  int count = 0;
  while (size > base) {
    base <<= 1;
    count ++;
  }
  return count;
}

void* initNewPage(size_t size, void** entry) {
  kma_page_t * page = get_page();
  *((kma_page_t**) page->ptr) = page;
  PAGE_COUNT++;
  if(diff(size) < 8) {
    size_t cur = 0;
    void* next = NULL;
    int count = (page->size - sizeof(void *)) / size - 1;
    while (count > 0) { //can add more than 1 buffer
      cur += size;
      if (*entry == NULL) { //first time append buffer
        *entry = page->ptr + sizeof(void *) + cur;
        next = *entry;
      }
      else {
        *(void **)next = page->ptr + sizeof(void *) + cur;
        next = *(void **)next;
      }
      if(count == 1) { //last one put an NULL sign
	*(void **)next = NULL;
      }
      count--;
    }
  }
  page->size = PAGESIZE - sizeof(void*) - size;
  return (void *) (page->ptr + sizeof(void*));
}

void* getBuffer(size_t size, void** entry) {
  void* cur = *entry;
  void* next = *(void **)cur;
  *entry = next;
  kma_page_t * page = *((kma_page_t **) BASEADDR(cur));
  page->size -= size; 
  return cur;
}

void derefPage(void* page, size_t size) {
  void** pre = FREE_LIST_HEAD + diff(size);
  void* cur = *pre;
  while (cur != NULL) {
    if (BASEADDR(cur) == page) {
      *pre = *(void **)cur; //remove from freelist
    } else {
      pre = cur; //move on
    }
    cur = *(void **)cur;
  }
}

void insertAtHead(void* ptr, size_t size) {
  void** entry = FREE_LIST_HEAD + diff(size); //utilize size as the mck2 array
  void* next = *entry;
  *entry = ptr; //set entry points to self
  *(void **)ptr = next; //set self points to the next buffer
}
#endif // KMA_P2FL
