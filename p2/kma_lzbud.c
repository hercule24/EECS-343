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
#ifdef KMA_LZBUD
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
#define ALLOCATED 0
#define FREE 1
#define GFREE 2

typedef struct Node
{
  bool status;
  short offset;
  int size;
  struct Node *next;
  struct Node *pre;
} Node;

/************Global Variables*********************************************/
//int id = -1;
//int freeid = 0;
int NUM_IN_USE = 0;
kma_page_t **BASE = NULL;
Node **HEAD_BASE = NULL;
int *SLACK_BASE = NULL;

/************Function Prototypes******************************************/
void* getFromNewPage(kma_size_t);
void initFirstPage();
void addToFreeList(Node*);
void* getFromFreeList(kma_size_t);
void setOnes(void *map_ptr, int num_bits, int start);
void setZeros(void *map_ptr, int num_bits, int start);
int kma_log2(int);
int roundUp(int);
void printSlack();
void freePageHelper(void*);
Node* updateFreeList(int);
void deleteFromList(Node*);
bool checkCoalesce(Node *, Node *);
void coalesce(Node **, bool*);
int countFreeList(int);
void countFreeListHelper(int);
Node* rightCoalesce(Node *, Node *);  
Node* leftCoalesce(Node *, Node *); 
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void* kma_malloc(kma_size_t size)
{ 
  //id++;
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
      node->status = GFREE;
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
    if (p->status == FREE){
      SLACK_BASE[offset] +=1;
    }
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
    //printf("size is %d, offset is %d, p is null\n", size, offset);
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
    if (p->status == FREE){
      SLACK_BASE[offset] +=2;
    } else if (p->status == GFREE){
      SLACK_BASE[offset] +=1;
    }
    if (next != NULL) {
      next->pre = NULL;
    }
    p->status = ALLOCATED;
    p->size = -1;
    p->next = NULL;
    return p;
  }
}

void printSlack(){
  int i;
  for(i =5; i<14; i++){
    printf("O:%d,S:%d   ",i, SLACK_BASE[i]);
  }
  printf("\n");
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

  SLACK_BASE = (int *)((size_t) first_page->ptr + NUM_HEADERS_BYTES);
  
  int i;
  for(i =0; i<13; i++){
    SLACK_BASE[i] = 0;
  }

  while (page_size >= NUM_HEADERS_BYTES << 2) {
    page_size = page_size >> 1;
    Node *node = (Node *) ((size_t) first_page->ptr + page_size);
    node->status = GFREE;
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
  int offset;
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
      offset = kma_log2(page_size);
      Node *node = (Node *) ((size_t) page->ptr + page_size);
      node->status = GFREE;
      node->size = page_size;
      node->next = NULL;
      node->pre = NULL;
      if (size > page_size >> 1) {
        node->status = FREE;
        //add to free list head
      }
      //add to free list tail
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
  //printf("****  address of node is : %lx id is %d size is %d \n", (size_t)node,id,node->size) ;
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

int printFreeListHelper(int offset){
  int n = 0;
  Node *head = HEAD_BASE[offset];
  if (head == NULL){
    return n;
  }
  while (head != NULL){
    n++;
    head = head->next;
  }
  return n;
}

void printFreeList(){
  int i;
  int j;
  for (i = 5; i < 13; i++){
    j = printFreeListHelper(i);
    printf("%d : %d; ",i, j);
  }
}

// bool isLoop(int offset){
//   int n = 0;
//   Node *slow = HEAD_BASE[offset];
//   Node *fast = HEAD_BASE[offset];
//   if (slow == NULL){
//     return FALSE;
//   }
//   while (fast->next != NULL && fast->next->next != NULL){
//     slow = slow->next;
//     fast = fast->next;
//     fast = fast->next;
//     n++;
//     if (slow == fast){
//       return TRUE;
//     }
//   }
//   return FALSE;
// }

void kma_free(void* ptr, kma_size_t size)
{
  bool isCoal = FALSE;
  bool* isCoalPointer = &isCoal;
  if (size <= MIN_BLOCK_SIZE) {
    size = MIN_BLOCK_SIZE;
  } else {
    size = roundUp(size);
  }
  int offset = kma_log2(size);
  //printSlack();
  // if(isLoop(5)){
  //   printf("5 is a loop\n");
  // }
  // if(isLoop(6))
  // {
  //   printf("6 is a loop\n");
  // }
  // if(isLoop(7))
  // {
  //   printf("7 is a loop\n");
  // }
  if (size == PAGESIZE) {
    freePageHelper(ptr);
    return;
  }
  Node *node_selected = NULL;
  Node *node = (Node *) ptr;
  node->size = size;
  node->next = NULL;
  node->pre = NULL;
  Node *res = NULL;
  //mark it locally free and free it locally
  if (SLACK_BASE[offset] >= 2){
    SLACK_BASE[offset] -= 2;
    node->status = FREE;
    res = node;
  } else if (SLACK_BASE[offset] == 1){
    //mark it globally free and free it globally, coalesce if possible
    SLACK_BASE[offset] -= 1;
    node->status = GFREE;
    res = node;
    Node ** nodePointer = &res;
    coalesce(nodePointer, isCoalPointer);
  } else{
    //mark it globally free and free it globally, coalesce if possible
    node->status = GFREE;
    res = node;
    Node ** nodePointer = &res;
    node_selected = updateFreeList(offset);
    countFreeListHelper(5);
    if (node_selected == NULL){
      printf("\n");
      printf("to be freed size is %d\n", size);
      printf("node selected is NULL\n");
      printf("break point 1\n");
      printf("\n");
      printf("break point 1");
    }
    //printf("break point 1");
    if(checkCoalesce(res, node_selected)){
      //printf("we don't need coalesce\n");
      coalesce(nodePointer, isCoalPointer);
    }
    else{
      nodePointer = &node_selected;
      coalesce(nodePointer, isCoalPointer);
      nodePointer = &res;
      coalesce(nodePointer, isCoalPointer);
    }
  }

  if (NUM_IN_USE == 1) {
    Node *node = (Node *) ((size_t) HEAD_BASE + NUM_HEADERS_BYTES + 128);

    int size = NUM_HEADERS_BYTES + 128;
    //printf("enter if \n");
    while (BASEADDR(node) == BASEADDR(HEAD_BASE)) {
      if (node->status == GFREE) {
        size += node->size;
        //printf(" node size is %d\n",node->size);
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
      if (isCoal == FALSE){
        addToFreeList(res);
      }
    }
  } else {
    if (res->size == PAGESIZE) {
      freePageHelper(res);
    } else {
      if (isCoal == FALSE){
        addToFreeList(res);
      }
    }
    if (node_selected != NULL){
      if (node_selected->size == PAGESIZE) {
        freePageHelper(node_selected);
      } 
    }
  }
}

void freePageHelper(void *ptr)
{
  int page_id = PAGE_ID(ptr);
  kma_page_t *page = BASE[NUM_BOOK_PAGES + page_id];
  free_page(page);
  NUM_IN_USE--;
  //printf("num in use is %d \n",NUM_IN_USE);
  if (NUM_IN_USE == 1) {
    Node *node = (Node *) ((size_t) HEAD_BASE + NUM_HEADERS_BYTES + 128);

    int size = NUM_HEADERS_BYTES + 128;

    while (BASEADDR(node) == BASEADDR(HEAD_BASE)) {
      if (node->status == FREE || node->status == GFREE) {
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

void coalesce(Node ** headPointer, bool *isCoalPointer){
  if (headPointer == NULL){
    printf("bad access !!!! ALERT!!!\n");
  }
  Node* head = *headPointer;
  if ((size_t) BASEADDR(head) == (size_t) head) {
    int prev_size = head->size;
    while(1){
      int original_size = head->size;
      Node *right = (Node *) ((size_t) head + head->size);
      head = rightCoalesce(head, right);
      if (head->size != original_size && head->size != PAGESIZE){
        original_size = head->size;
        addToFreeList(head);
      }
      if (head->size != prev_size) {
        prev_size = head->size;
        *headPointer = head;
        *isCoalPointer = TRUE;
      } else {
        break;
      }
    }
  }
  else if (((size_t) BASEADDR(head) + PAGESIZE) == ((size_t) head + head->size)){
    int prev_size = head->size;
    while(1){
      int original_size = head->size;
      Node *left = (Node *) ((size_t) head - head->size);
      head = leftCoalesce(left, head);
      if (head->size != original_size && head->size != PAGESIZE){
        original_size = head->size;
        addToFreeList(head);
      }
      if (head->size != prev_size) {
        prev_size = head->size;
        *headPointer = head;
        *isCoalPointer = TRUE;
      } else {
        break;
      }
    }
  } 
  else {
    //printf("break point 3 !!!\n");
    int prev_size = head->size;

    while (1) {
      //printf("enter the loop in coalesce\n");
      int original_size = head->size;
      Node *right = (Node *) ((size_t) head + head->size);
      head = rightCoalesce(head, right);

      if (head->size == PAGESIZE) {
        *headPointer = head;
        *isCoalPointer = TRUE;
        break;
      }

      if (head->size != original_size && head->size != PAGESIZE){
        original_size = head->size;
        addToFreeList(head);
      }

      Node *left = (Node *) ((size_t) head - head->size);
      head = leftCoalesce(left, head);
      //printf("after left coal address head:  %lx size : %d\n", (size_t)head, head->size);

      if (head->size == PAGESIZE) {
        *headPointer = head;
        *isCoalPointer = TRUE;
        break;
      }

      if (head->size != original_size && head->size != PAGESIZE){
        addToFreeList(head);
      }

      if (head->size != prev_size) {
        prev_size = head->size;
        *headPointer = head;
        *isCoalPointer = TRUE;
      } else {
        break;
      }
    }
  }
}

bool checkCoalesce(Node *left, Node * right){
  if (BASEADDR(left) == BASEADDR(right) && right->size == left->size) {
    int size_sum = left->size + right->size;
    if ((size_t)right == (size_t)left + left->size){
      if (((size_t) left) % size_sum == 0) {
        return TRUE;
      } else{
        return FALSE;
      }
    }
    else if ((size_t)left == (size_t)right + right->size){
      if (((size_t) right) % size_sum == 0) {
        return TRUE;
      } else{
        return FALSE;
      }
    }
    else{
      return FALSE;
    }
  } else {
    return FALSE;
  }
}

Node* updateFreeList(int offset){
  Node *head = HEAD_BASE[offset];
  while (head != NULL && head->status != FREE){
    head = head->next;
  }
  if (head == NULL){
    return head;
  }
  //printf("******  address of updated node is : %lx id is %d size is %d \n", (size_t)head,id,head->size) ;
  head->status = GFREE;
  return head;
}

int countFreeList(int offset){
  int n = 0;
  Node *head = HEAD_BASE[offset];
  if (head == NULL){
    return 0;
  }
  while (head != NULL){
    n++;
    head = head->next;
  }
  return n;
}

void countFreeListHelper(int offset){
  int n = 0;
  Node *head = HEAD_BASE[offset];
  if (head == NULL){
    return;
  }
  while (head != NULL){
    n++;
    //printf("address is %lx status is %d\n", (size_t)head, head->status);
    head = head->next;
  }
  return;
}

bool canRightCoalesce(Node *left, Node *right)
{
  if (BASEADDR(left) == BASEADDR(right)
      && right->size == left->size && right->status == GFREE) {
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
      && left->status == GFREE && right->size == left->size) {
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

void deleteFromList(Node* head){
  Node *pre = head->pre;
  // if right is the head
  if (pre == NULL) {
    short offset = head->offset;
    HEAD_BASE[offset] = head->next; 
    if (head->next != NULL) {
      head->next->pre = NULL;
    }
    head->next = NULL;
  } else {
    pre->next = head->next;
    if (head->next != NULL) {
      head->next->pre = pre;
    }
    head->pre = NULL;
    head->next = NULL;
  }
}

Node* rightCoalesce(Node *left, Node *right)
{
  if (canRightCoalesce(left, right)) {
    deleteFromList(right);
    deleteFromList(left);
  
    left->size = (left->size) << 1;
    right = (Node *) ((size_t) left + left->size);
  }
  return left;
}

Node* leftCoalesce(Node *left, Node *right)
{
  if (canLeftCoalesce(left, right)) {
    deleteFromList(left);
    deleteFromList(right);
  
    left->size = (left->size) << 1;
    right = left;
    left = (Node *) ((size_t) left - left->size);
  }
  return right;
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

