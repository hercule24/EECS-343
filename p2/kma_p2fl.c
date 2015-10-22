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
//#define KMA_P2FL
#ifdef KMA_P2FL
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
//int REQUEST_COUNT = 0;
//int FREE_COUNT = 0;
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
  //printf("REQUEST_COUNT: %d\n", REQUEST_COUNT);
 // REQUEST_COUNT++;
  if (!INIT) { //first time using malloc
    initFreeList();
    INIT = TRUE;
  //printf("PAGE_COUNT+1 by initFreeList: %d\n",PAGE_COUNT);
  }
  size = roundUp(size);
  if(diff(size) > 8) return NULL; //larger than 8192
  void** entry = FREE_LIST_HEAD + diff(size); 
 // printf("entry = %d\n", *entry);
  if (*entry == NULL) { //allocate a new page and set the whole page same as size
  //printf("1.entry = %d\n", *entry);
 // printf("PAGE_COUNT will be + 1 by initNewPage: %d\n",PAGE_COUNT);
    return initNewPage(size, entry);
  } else { //has free buffers
  //printf("2.entry = %d\n", *entry);
    return getBuffer(size, entry);
  }
}

void kma_free(void* ptr, kma_size_t size)
{
 // printf("FREE_COUNT: %d\n", FREE_COUNT);
 // FREE_COUNT++;
  size = roundUp(size);
  kma_page_t *page = *((kma_page_t **) BASEADDR(ptr));
  if (diff(size) == 8) { //if 8196, free the page
    free_page(page);
    PAGE_COUNT--;
  } else {
    page->size += size;
 //   printf("free: page addr = %d, size = %d\n", page->ptr, page->size);
    if(page->size == PAGESIZE) { //8192 case
      free_page(page);
      PAGE_COUNT--;
    } else if (page->size == PAGESIZE - sizeof(kma_page_t*)) {
  //    printf("page size = %d\n", page->size);
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
 // printf("PAGE_COUNT: %d\n",PAGE_COUNT);
}

void initFreeList() {
  kma_page_t * page = get_page();
  *((kma_page_t**) page->ptr) = page;
  PAGE_COUNT++;
  FREEPAGE = page;
  FREE_LIST_HEAD = page->ptr + sizeof(void*); 
  * FREE_LIST_HEAD = NULL;
  for (int i = 1; i <= 8; i++) {
    *(FREE_LIST_HEAD + i) = NULL;
    //printf(" %d\n",FREE_LIST_HEAD + i);
  }
  /*size_t n = 1 << 12; //4096
   // printf("size = %d\n",page->size);
  void* base = page->ptr + 10 * sizeof(void*); //beginning of free space
  page->size -= sizeof(void *) * 10;//10 more  pointers
  while (page->size >= n) { //4096. 2048, 1024, 512, ....
   // printf("n = %zu, size = %d\n",n, page->size);
    page->size -= n;
    base += n / sizeof(void*);
    * (FREE_LIST_HEAD + diff(n)) = base;
    * (void **)base = NULL;
    n >>= 1;
  }*/
  //page->size = PAGESIZE - sizeof(void *) * 10;
   // printf("page = %d, %d, %d\n",page, page->ptr + 1, FREE_LIST_HEAD);
}

size_t roundUp(size_t size) {
  size_t res = size + sizeof(void *);
  size_t n = 1 << 5;
   // printf("n = %zu\n",n);
  while (res > n) {
    n <<= 1;
  }
    //printf("After RoundUp: n = %zu\n",n);
  return n;
}

int diff(size_t size) {
  size_t base = 1 << 5;
  int count = 0;
  while (size > base) {
    base <<= 1;
    count ++;
  }
 //   printf("diff :count = %d\n",count);
  return count;
}

void* initNewPage(size_t size, void** entry) {
  kma_page_t * page = get_page();
  *((kma_page_t**) page->ptr) = page;
  PAGE_COUNT++;
  //printf("initNP: page addr = %d\n", page->ptr);
  //set header points to entry
  *((void **)(page->ptr + sizeof(void *))) = entry;
  if(diff(size) < 3) {
    size_t cur = 1;
    void* next = NULL;
    while (page->size >= 2 * size + sizeof(void *)) { //can add more than 1 buffer
      cur += size/sizeof(void *);
      if (*entry == NULL) { //first time append buffer
        *entry = (void *)(page->ptr + cur * sizeof(void *));
        next = *entry;
      }
      else {
        *(void **)next = page->ptr + cur * sizeof(void *);
        next = *(void **)next;
      }
      page->size -= size;
    }
  }
  page->size = size == PAGESIZE? 0 : PAGESIZE - sizeof(void*) - size;
  //printf("initNP: page size = %d\n", page->size);
  //printf("initNP: return addr = %d\n", page->ptr + 2 * sizeof(void *));
  return (void *) (page->ptr + 2 * sizeof(void*));
}

void* getBuffer(size_t size, void** entry) {
  void* cur = *entry;
 // printf("cur = %d\n", cur);
  void* next = *(void **)cur;
 // printf("next = %d\n", next);
  //set cur points to entry
  *(void **)cur = entry;
 // printf("GB: entry = %d\n", entry);
  //set entry points to next buffer
  *entry = next;
  kma_page_t * page = *((kma_page_t **) BASEADDR(cur));
 // printf("page = %d\n", BASEADDR(cur));
  page->size -= size; 
 // printf("getBuffer: cur + 1, cur= %d, %d\n", cur + 1, cur + sizeof(void *));
  return cur + sizeof(void *);
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
  void** entry = FREE_LIST_HEAD + diff(size);
  //void** entry = *(void ***)(ptr - 1); //header points to entry
  //printf("iAH: entry = %d, ptr - 1 = %d, %d\n", entry, ptr - 1, ptr);
  void* next = *entry;
  //printf("iAH: next, *entry = %d, %d\n", next, *entry);
  *entry = ptr - sizeof(void *); //set entry points to self
 // printf("iAH: *entry = %d, ptr - 1 = %d\n", *entry, ptr - 1);
  *((void **)(ptr - sizeof(void *))) = next; //set self points to the next buffer
//  printf("iAH: next, *entry = %d, %d\n", next, ptr - 1);
}
#endif // KMA_P2FL
