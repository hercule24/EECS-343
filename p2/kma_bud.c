/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
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
 *    Revision 1.3  2004/12/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *

 ************************************************************************/
 
 /************************************************************************
 Project Group: NetID1, NetID2, NetID3
 
 ***************************************************************************/
#define KMA_BUD
#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#define MIN_BLOCK_SIZE 32
#define MAX_OFFSET 13
#define NUM_HEADERS_BYTES (sizeof(size_t) * 16)
//#define SINGLE_BITMAP_SIZE (PAGESIZE / MIN_BLOCK_SIZE / 8)
#define SINGLE_BITMAP_SIZE 32
#define NUM_BOOK_PAGES (sizeof(kma_page_t *) * MAXPAGES / PAGESIZE)
//#define NUM_BITMAP_PAGES (MAXPAGES / (MIN_BLOCK_SIZE * 8))
#define NUM_BITMAP_PAGES 16
#define NUM_META_PAGES ((NUM_BOOK_PAGES) + (NUM_BITMAP_PAGES))
#define NUM_OF_BITS(size) ((size) >> 5)
#define PAGE_ID(addr) ((((size_t) (addr)) - ((size_t) (HEAD_BASE))) / (PAGESIZE))
#define START_BIT(addr) ((((size_t) (addr)) - ((size_t) (BASEADDR(addr)))) / MIN_BLOCK_SIZE)

typedef struct Node
{
  size_t  page_id;
  size_t start_bit;
  //int end_bit;
  size_t size;
  struct Node *next;
} Node;

/************Global Variables*********************************************/
int NUM_IN_USE = 0;
kma_page_t **BASE = NULL;
void *BITMAP_BASE = NULL;
Node **HEAD_BASE = NULL;

/************Function Prototypes******************************************/
void* getFromNewPage(kma_size_t);
void initFirstPage();
void addToFreeList(Node*);
void* getFromFreeList(kma_size_t);
int roundUp(kma_size_t);
void setOnes(void *map_ptr, int num_bits, int start = 0);
void setZeros(void *map_ptr, int num_bits, int start = 0);
	
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void* kma_malloc(kma_size_t size)
{
  if (size > PAGESIZE) {
    return NULL;
  }

  if (size < MIN_BLOCK_SIZE) {
    size = MIN_BLOCK_SIZE;
  }

  if (NUM_IN_USE == 0) {
    // set up the kma_page_t book keeping
    kma_page_t *book_page = get_page();
    BASE = (kma_page_t **) book_page->ptr;
    memset((void *) BASE, 0, NUM_META_PAGES * PAGESIZE);
    BASE[0] = book_page;
    
    kma_page_t *bitmap_page = get_page();
    BASE[NUM_BOOK_PAGES] = bitmap_page;
    BITMAP_BASE = (void *) ((size_t) BASE + NUM_BOOK_PAGES * PAGESIZE);
    // set 1 for all book keeping entries
    memset(BITMAP_BASE, 0xff, NUM_META_PAGES * SINGLE_BITMAP_SIZE) ;
    BITMAP_BASE = (void *) ((size_t) BITMAP_BASE + NUM_META_PAGES * SINGLE_BITMAP_SIZE);
  
    initFirstPage();

    return getFromNewPage(size);
  } else {
    void *res = getFromFreeList(size);
    if (res != NULL) {
      return res;
    } else {
      return getFromNewPage(size);
    }
  }
}

Node* getFromFreeListHelper(int offset, kma_size_t size)
{
  Node *p = HEAD_BASE[offset];
  if (p == NULL) {
    if (offset == MAX_OFFSET) {
      return NULL;
    } else {
      Node *res = getFromFreeListHelper(offset + 1, size << 1);

      Node *node = (Node *) ((size_t) res + size);
      node->start_bit = res->start_bit + NUM_OF_BITS(size);
      node->page_id = res->page_id;
      node->next = NULL;
      node->size = size;
      addToFreeList(node);

      res->size = size;
      return res;
    }
  } else {
    HEAD_BASE[offset] = p->next;
    p->next = NULL;
    return p;
  }
}

void* getFromFreeList(kma_size_t size)
{
  int offset = log2(size); 
  Node *p = HEAD_BASE[offset];

  int num_bits = NUM_OF_BITS(size);
  void *map_ptr = (void *) ((size_t) BITMAP_BASE + p->page_id * SINGLE_BITMAP_SIZE);

  if (p == NULL) {
    if (offset == MAX_OFFSET) {
      return NULL;
    } else {
      Node *res = getFromFreeListHelper(offset + 1, size << 1);
      
      Node *node = (Node *) ((size_t) res + size);
      node->start_bit = res->start_bit + NUM_OF_BITS(size);
      node->page_id = res->page_id;
      node->next = NULL;
      node->size = size;
      addToFreeList(node);
      
      setOnes(map_ptr, num_bits, res->start_bit);
      return res;
    }
  } else {
    // TODO:
    if ((size_t) p == (size_t) BASEADDR(p)) {
      setOnes(map_ptr, num_bits);
    } else {
      setOnes(map_ptr, num_bits, p->start_bit);
    }

    HEAD_BASE[offset] = p->next;
    p->next = NULL;
    return p;
  }
}

int roundUp(kma_size_t size)
{
}

void initFirstPage()
{
  kma_page_t *first_page = get_page();
  BASE[NUM_META_PAGES] = first_page;

  kma_size_t page_size = first_page->size;
  // first mark all of them as not allocated
  memset(BITMAP_BASE, 0, SINGLE_BITMAP_SIZE);
  // set the head base
  HEAD_BASE = (Node **) first_page->ptr;
  // make all headers NULL
  memset(first_page->ptr, 0, NUM_HEADERS_BYTES);
  // mark the first 4 bit as allocated
  memset(BITMAP_BASE, 0x0f, 1);

  while (page_size >= NUM_HEADERS_BYTES << 1) {
    page_size = page_size >> 1;
    int num_bits = page_size / MIN_BLOCK_SIZE;
    Node *node = (Node *) ((size_t) first_page->ptr + page_size);
    node->page_id = NUM_IN_USE;
    node->start_bit = num_bits;
    //node->end_bit = num_bits + num_bits;
    node->size = page_size;
    node->next = NULL;
    addToFreeList(node);
  }

  NUM_IN_USE += 1;
}

void* getFromNewPage(kma_size_t size)
{
  kma_page_t *page = get_page();
  BASE[NUM_META_PAGES + NUM_IN_USE] = page;
  void *map_ptr = (void *) ((size_t) BITMAP_BASE + NUM_IN_USE * MIN_BLOCK_SIZE);
  if (size > PAGESIZE >> 1) {
    // mark as allocated
    memset(map_ptr, 0xff, SINGLE_BITMAP_SIZE);
    return page->ptr;
  } else {
    // mark as free page
    memset(map_ptr, 0, SINGLE_BITMAP_SIZE);

    void *res = NULL;
    int page_size = page->size;

    while (page_size >= MIN_BLOCK_SIZE << 1) {
      page_size = page_size >> 1;
      if (size > page_size >> 1) {
        int num_bits = NUM_OF_BITS(page_size);
        // mark num_bits as allocated
        setOnes(map_ptr, num_bits);
        Node *node = (Node *) ((size_t) page->ptr + page_size);
        node->page_id = NUM_IN_USE;
        node->start_bit = num_bits;
        //node->end_bit = num_bits + num_bits;
        node->size = page_size;
        node->next = NULL;
        addToFreeList(node);
        res = page->ptr;
        break;
      }
    }

    NUM_IN_USE++;
    return res;
  }
}

void setOnes(void *ptr, int num_bits, int start)
{
  int i;
  unsigned int setter = 1;
  int *p = (int *) ptr;

  for (i = 0; i < start; i++) {
    setter <<= 1;
    if (setter == 0) {
      p++;
      setter = 0;
    }
  }

  for (i = 0; i < num_bits; i++) {
    *p |= setter;
    setter <<= 1;
    if (setter == 0) {
      p++;
      setter = 0;
    }
  }
}

void setZeros(void *ptr, int num_bits, int start)
{
  int i;
  unsigned int setter = 0xfffffffe;
  int *p = (int *) ptr;

  for (i = 0; i < start; i++) {
    setter <<= 1;
    setter += 1;
    if (setter == 0xffffffff) {
      p++;
      setter = 0xfffffffe;
    }
  }

  for (i = 0; i < num_bits; i++) {
    *p &= setter;
    setter <<= 1;
    setter += 1;
    if (setter == 0xffffffff) {
      p++;
      setter = 0xfffffffe;
    }
  }
}

void addToFreeList(Node *node)
{
  int offset = log2(node->size);
  Node *head = HEAD_BASE[offset];

  if (head == NULL) {
    HEAD_BASE[offset] = node;
  } else {
    node->next = head;
    HEAD_BASE[offset] = node;
  }
}

void kma_free(void* ptr, kma_size_t size)
{
  Node *node = (Node *) ptr;
  node->page_id = PAGE_ID(ptr);
  node->size = size;
  node->next = NULL;

  void *map_ptr = (void *) ((size_t) BITMAP_BASE + node->page_id * SINGLE_BITMAP_SIZE);

  if ((size_t) BASEADDR(ptr) == (size_t) ptr) {
    node->start_bit = 0;
    addToFreeList(node);
    setZeros(map_ptr, NUM_OF_BITS(size), 0);
  } else {
    node->start_bit = START_BIT(ptr);
    addToFreeList(node);
    setZeros(map_ptr, NUM_OF_BITS(size), node->start_bit);
  }
}

#endif // KMA_BUD
