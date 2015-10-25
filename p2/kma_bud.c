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
#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#define ALLOCATED 1
#define FREE 2

typedef struct Node
{
  bool status;
  //int page_id;
  //size_t start_bit;
  //int end_bit;
  short offset;
  int size;
  struct Node *next;
  struct Node *pre;
} Node;

/************Global Variables*********************************************/
int NUM_IN_USE = 0;
kma_page_t **BASE = NULL;
void *BITMAP_BASE = NULL;
Node **HEAD_BASE = NULL;
int NEXT_PAGE_ID = 0;

/************Function Prototypes******************************************/
void* getFromNewPage(kma_size_t);
void initFirstPage();
void addToFreeList(Node*);
void* getFromFreeList(kma_size_t);
void setOnes(void *map_ptr, int num_bits, int start);
void setZeros(void *map_ptr, int num_bits, int start);
int kma_log2(int);
int roundUp(int);
Node* rightCoalesce(Node *, Node *);	
Node* leftCoalesce(Node *, Node *);	
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void* kma_malloc(kma_size_t size)
{
  if (size > PAGESIZE) {
    return NULL;
  }

  if (size <= MIN_BLOCK_SIZE) {
    size = MIN_BLOCK_SIZE;
  } else {
    size = roundUp(size);
  }

  if (NUM_IN_USE == 0) {
    kma_page_t *book_page = get_page();
    BASE = (kma_page_t **) book_page->ptr;
    BASE[0] = book_page;

    int i;
    for (i = 1; i < NUM_BOOK_PAGES; i++) {
      BASE[i] = get_page();
    }
    
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

      if (res == NULL) {
        return NULL;
      }

      Node *node = (Node *) ((size_t) res + size);
      node->status = FREE;
      node->size = size;
      node->pre = NULL;
      node->next = NULL;
      addToFreeList(node);

      res->size = size;
      return res;
    }
  } else {
    Node *next = p->next;
    HEAD_BASE[offset] = next;
    if (next != NULL) {
      next->pre = NULL;
    }
    p->next = NULL;
    return p;
  }
}

void* getFromFreeList(kma_size_t size)
{
  int offset = kma_log2(size);
  Node *p = HEAD_BASE[offset];

  if (p == NULL) {
    if (offset == MAX_OFFSET) {
      return NULL;
    } else {
      Node *res = getFromFreeListHelper(offset + 1, size << 1);

      if (res == NULL) {
        return NULL;
      }
      
      Node *node = (Node *) ((size_t) res + size);
      node->status = FREE;
      node->size = size;
      node->pre = NULL;
      node->next = NULL;
      addToFreeList(node);
      
      ((Node *) res)->status = ALLOCATED;
      ((Node *) res)->size = -1;
      return res;
    }
  } else {
    Node *next = p->next;
    HEAD_BASE[offset] = next;
    if (next != NULL) {
      next->pre = NULL;
    }
    p->status = ALLOCATED;
    p->size = -1;
    p->next = NULL;
    return p;
  }
}

void initFirstPage()
{
  kma_page_t *first_page = get_page();
  BASE[NUM_BOOK_PAGES] = first_page;

  ((Node *) (first_page->ptr))->status = ALLOCATED;
  ((Node *) (first_page->ptr))->offset = 0;
  ((Node *) (first_page->ptr))->size = -1;
  ((Node *) (first_page->ptr))->next = NULL;
  ((Node *) (first_page->ptr))->pre = NULL;

  kma_size_t page_size = first_page->size;
  HEAD_BASE = (Node **) first_page->ptr;

  while (page_size >= NUM_HEADERS_BYTES << 1) {
    page_size = page_size >> 1;
    Node *node = (Node *) ((size_t) first_page->ptr + page_size);
    node->status = FREE;
    node->size = page_size;
    node->pre = NULL;
    node->next = NULL;
    addToFreeList(node);
  }

  NUM_IN_USE += 1;
}

void* getFromNewPage(kma_size_t size)
{
  kma_page_t *page = get_page();
  int page_id = PAGE_ID(page->ptr);
  BASE[NUM_BOOK_PAGES + page_id] = page;
  NUM_IN_USE++;
  void *res = page->ptr;
  ((Node *) res)->status = ALLOCATED;
  ((Node *) res)->size = -1;
  ((Node *) res)->next = NULL;
  ((Node *) res)->pre = NULL;
  if (size > PAGESIZE >> 1) {
    return res;
  } else {
    int page_size = page->size;

    while (page_size >= MIN_BLOCK_SIZE << 1) {
      page_size = page_size >> 1;
      Node *node = (Node *) ((size_t) page->ptr + page_size);
      node->status = FREE;
      node->size = page_size;
      node->next = NULL;
      node->pre = NULL;
      addToFreeList(node);
      if (size > page_size >> 1) {
        break;
      }
    }
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
  int offset = kma_log2(node->size);
  node->offset = offset;
  Node *head = HEAD_BASE[offset];

  if (head == NULL) {
    HEAD_BASE[offset] = node;
  } else {
    node->next = head;
    head->pre = node;
    HEAD_BASE[offset] = node;
  }
}

void freePageHelper(void *ptr)
{
  int page_id = PAGE_ID(ptr);
  kma_page_t *page = BASE[NUM_BOOK_PAGES + page_id];
  free_page(page);
  NUM_IN_USE--;

  if (NUM_IN_USE == 1) {
    Node *node = (Node *) ((size_t) HEAD_BASE + NUM_HEADERS_BYTES);

    int size = NUM_HEADERS_BYTES;

    while (BASEADDR(node) == BASEADDR(HEAD_BASE)) {
      if (node->status == FREE) {
        size += node->size;
        node = (Node *) ((size_t) node + node->size);
      } else {
        break;
      }
    }

    if (size == PAGESIZE) {
      int i;
      for (i = 0; i < NUM_BOOK_PAGES + 1; i++) {
        free_page(BASE[i]);
      }
      NUM_IN_USE = 0;
    }
  }
}

void kma_free(void* ptr, kma_size_t size)
{
  if (size <= MIN_BLOCK_SIZE) {
    size = MIN_BLOCK_SIZE;
  } else {
    size = roundUp(size);
  }

  if (size == PAGESIZE) {
    freePageHelper(ptr);
    return;
  }

  Node *node = (Node *) ptr;
  node->status = FREE;
  node->size = size;
  node->next = NULL;
  node->pre = NULL;

  Node *res = node;

  if ((size_t) BASEADDR(ptr) == (size_t) ptr) {
    Node *right = (Node *) ((size_t) res + res->size);
    res = rightCoalesce(res, right);
  } else {
    int prev_size = res->size;

    while (1) {
      Node *right = (Node *) ((size_t) res + res->size);
      res = rightCoalesce(res, right);


      Node *left = (Node *) ((size_t) res - res->size);
      res = leftCoalesce(left, res);

      if (res->size != prev_size) {
        prev_size = res->size;
      } else {
        break;
      }
    }
  }

  if (NUM_IN_USE == 1) {
    Node *node = (Node *) ((size_t) HEAD_BASE + NUM_HEADERS_BYTES);

    int size = NUM_HEADERS_BYTES;

    while (BASEADDR(node) == BASEADDR(HEAD_BASE)) {
      if (node->status == FREE) {
        size += node->size;
        node = (Node *) ((size_t) node + node->size);
      } else {
        break;
      }
    }

    if (size == PAGESIZE) {
      int i;
      for (i = 0; i < NUM_BOOK_PAGES + 1; i++) {
        free_page(BASE[i]);
      }
    } else {
      addToFreeList(res);
    }
  } else {
    if (res->size == PAGESIZE) {
      freePageHelper(res);
    } else {
      addToFreeList(res);
    }
  }
}

bool canRightCoalesce(Node *left, Node *right)
{
  if (BASEADDR(left) == BASEADDR(right)
      && right->status == FREE && right->size == left->size) {
    int size_sum = left->size + right->size; 
    if (((size_t) left) % size_sum == 0) {
      return TRUE;
    } else{
      return FALSE;
    }
  } else {
    return FALSE;
  }
}

bool canLeftCoalesce(Node *left, Node *right)
{
  if (BASEADDR(left) == BASEADDR(right)
      && left->status == FREE && right->size == left->size) {
    int size_sum = left->size + right->size; 
    if (((size_t) left) % size_sum == 0) {
      return TRUE;
    } else{
      return FALSE;
    }
  } else {
    return FALSE;
  }
}

Node* rightCoalesce(Node *left, Node *right)
{
  while (1) {
    if (canRightCoalesce(left, right)) {
      Node *pre = right->pre;
      // if right is the head
      if (pre == NULL) {
        short offset = right->offset;
        HEAD_BASE[offset] = right->next; 
        if (right->next != NULL) {
          right->next->pre = NULL;
        }
        right->next = NULL;
      } else {
        pre->next = right->next;
        if (right->next != NULL) {
          right->next->pre = pre;
        }
        right->pre = NULL;
        right->next = NULL;
      }
    
      left->size = (left->size) << 1;
      right = (Node *) ((size_t) left + left->size);
    } else {
      break;
    }
  }

  return left;
}

Node* leftCoalesce(Node *left, Node *right)
{
  while (1) {
    if (canLeftCoalesce(left, right)) {
      Node *pre = left->pre;
      // if left is the head
      if (pre == NULL) {
        short offset = left->offset;
        HEAD_BASE[offset] = left->next; 
        if (left->next != NULL) {
          left->next->pre = NULL;
        }
        left->next = NULL;
      } else {
        pre->next = left->next;
        if (left->next != NULL) {
          left->next->pre = pre;
        }
        left->pre = NULL;
        left->next = NULL;
      }
    
      left->size = (left->size) << 1;
      right = left;
      left = (Node *) ((size_t) left - left->size);
    } else {
      return right;
    }
  }

  return left;
}

int kma_log2(int size)
{
  int k = 0;
  while (size != 1) {
    size >>= 1;
    k++;
  }
  return k;
}

int roundUp(int size) {
  int low = 0;
  int high = 13;
  int middle;
  while (low < high - 1) {
    middle = (low + high) / 2;
    if ((1 << middle) < size) {
      low = middle;
    } else {
      high = middle; 
    }
  }
  return (1 << high);
}

#endif // KMA_BUD
