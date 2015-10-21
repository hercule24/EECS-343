/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
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

#ifdef KMA_RM
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
typedef struct rm
{
  size_t size;
  struct rm *next;
} ResourceMap;

/************Global Variables*********************************************/
ResourceMap *FREE_LIST_HEAD = NULL;

/************Function Prototypes******************************************/
void* getFromList(size_t size);
void* getFromNewPage(kma_size_t);
void addToFreeList(ResourceMap *);
bool isWholePage(ResourceMap *);
ResourceMap *coalesce(ResourceMap *, ResourceMap *);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
  // allocating at least sizeof(ResourceMap)
  if (size < sizeof(ResourceMap)) {
    size = sizeof(ResourceMap);
  }
  
  // get one more page
  if (FREE_LIST_HEAD == NULL) {
    return getFromNewPage(size);
  } else {
    void *res = getFromList(size);
    if (res == NULL) {
      return getFromNewPage(size); 
    } else {
      return res;
    }
  }
}

void* getFromNewPage(kma_size_t size)
{
  kma_page_t *page = get_page();
  // add a pointer to the page structure at the beginning of the page
  *((kma_page_t**) page->ptr) = page;

  size_t page_size = page->size - sizeof(kma_page_t *);
  // what if size == page_size
  if (size <= page_size) {
    // no need to add into the free list, 
    // if the remaining size smaller than ResourceMap
    // return more than wanted
    if (page_size - size < sizeof(ResourceMap)) {
      return (void *) ((size_t) page->ptr + sizeof(kma_page_t *));
    } else {
      if (FREE_LIST_HEAD == NULL) {
        FREE_LIST_HEAD = (ResourceMap *) ((size_t) page->ptr + sizeof(kma_page_t *) + size);
        FREE_LIST_HEAD->size = page_size - size;
        FREE_LIST_HEAD->next = NULL;
      } else {
        ResourceMap *newNode = (ResourceMap *) ((size_t) page->ptr + sizeof(kma_page_t *) + size);
        newNode->size = page_size - size;
        newNode->next = NULL;
        addToFreeList(newNode);
      }
      return (void *) ((size_t) page->ptr + sizeof(kma_page_t *));
    }
  } else {
    //printf("return NULL because of larger than single page\n");
    return NULL;  // return NULL if larger than a single page size
  }
}

void
kma_free(void* ptr, kma_size_t size)
{
  if (size < sizeof(ResourceMap)) {
    size = sizeof(ResourceMap);
  }
  ResourceMap *map = (ResourceMap *) ptr;
  map->size = size;
  map->next = NULL;
  if (isWholePage(map)) {
    kma_page_t *page = *((kma_page_t **) BASEADDR(map)); 
    free_page(page);
  } else {
    addToFreeList(map);
  }
}

bool isWholePage(ResourceMap *map) {
  //if (map->size == PAGESIZE - sizeof(kma_page_t *)) {
  if (PAGESIZE - sizeof(kma_size_t*) - map->size < sizeof(ResourceMap)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

// return NULL, if cannot coalesce
ResourceMap *coalesce(ResourceMap *low, ResourceMap *high)
{
  size_t low_addr = (size_t) low + low->size;
  size_t high_addr = (size_t) high;
  size_t low_base = (size_t) BASEADDR(low);
  size_t high_base = (size_t) BASEADDR(high);
  if (low_addr == high_addr && low_base == high_base) {
    low->size = low->size + high->size;
    low->next = high->next;
    return low;
  } else {
    return NULL;
  }
}

void addToFreeList(ResourceMap *map) {
  if (FREE_LIST_HEAD == NULL) {
    FREE_LIST_HEAD = map;
    return;
  }

  ResourceMap dummy_head;
  dummy_head.size = 0;
  dummy_head.next = FREE_LIST_HEAD;

  ResourceMap pre_dummy_head;
  pre_dummy_head.size = 0;
  pre_dummy_head.next = &dummy_head;

  ResourceMap *pre = &pre_dummy_head;
  ResourceMap *cur = &dummy_head;
  ResourceMap *next = FREE_LIST_HEAD;
  
  while (next != NULL) {
    if ((size_t) next < (size_t) map) {
      // if the last element
      if (next->next == NULL) {
        if (coalesce(next, map) == NULL) {
          next->next = map;
        } else {
          if (isWholePage(next)) {
            if (next == FREE_LIST_HEAD) {
              FREE_LIST_HEAD = NULL;
            } else {
              cur->next = NULL;
            }
            kma_page_t *page = *((kma_page_t **) BASEADDR(next)); 
            free_page(page);
          }
        }
        return;
      } else {
        pre = pre->next;
        cur = cur->next;
        next = next->next;
      }
    } else {
      // if next is the head
      if (next == FREE_LIST_HEAD) {
        if (coalesce(map, next) == NULL) {
          FREE_LIST_HEAD = map;
          FREE_LIST_HEAD->next = next;
        } else {
          if (isWholePage(map)) {
            // only element
            if (next->next == NULL) {
              FREE_LIST_HEAD = NULL;
            } else {
              FREE_LIST_HEAD = map->next;
            }
            kma_page_t *page = *((kma_page_t **) BASEADDR(map)); 
            free_page(page);
          } else {
            FREE_LIST_HEAD = map;
          }
        }
      } else {
        // if we can not combine the first two
        if (coalesce(cur, map) == NULL) {
          // if we can not combine the last two
          if (coalesce(map, next) == NULL) {
            cur->next = map;
            map->next = next;
          } else { // if we can combine the last two
            if (isWholePage(map)) {
              cur->next = map->next;
              kma_page_t *page = *((kma_page_t **) BASEADDR(map)); 
              free_page(page);
            } else {
              cur->next = map;
            }
          }
        } else {
          // if we can combine the first two, but not all three
          if (coalesce(cur, next) == NULL) {
            if (isWholePage(cur)) {
              if (cur == FREE_LIST_HEAD) {
                FREE_LIST_HEAD = next;
              } else {
                pre->next = next;
              }
              kma_page_t *page = *((kma_page_t **) BASEADDR(cur)); 
              free_page(page);
            } else {
              cur->next = next;
            }
          } else { // if we can combine all of them
            if (isWholePage(cur)) {
              if (cur == FREE_LIST_HEAD) {
                FREE_LIST_HEAD = next->next;
              } else {
                pre->next = next->next;
              }
              kma_page_t *page = *((kma_page_t **) BASEADDR(cur)); 
              free_page(page);
            }
          }
        }
      }
      break;
    }
  }
}

void* getFromList(size_t size)
{
  ResourceMap dummy_head;
  dummy_head.next = FREE_LIST_HEAD;

  ResourceMap *cur = &dummy_head;
  ResourceMap *next = cur->next;

  while (next != NULL) {
    int left_size = (next->size) - size;
    int right_size = sizeof(ResourceMap);
    if (left_size >= right_size) {
      // remove it from the list
      void *res = (void *) next;
      if (next == FREE_LIST_HEAD) {
        ResourceMap *next_next = next->next;
        FREE_LIST_HEAD = (ResourceMap *) ((size_t) next + size);
        FREE_LIST_HEAD->size = next->size - size;
        FREE_LIST_HEAD->next = next_next;
      } else {
        ResourceMap *next_next = next->next;
        cur->next = (ResourceMap *) ((size_t) next + size);
        cur->next->size = next->size - size;
        cur->next->next = next_next;
      }
      return res;
    } else {
      cur = cur->next;
      next = next->next;
    }
  }

  // though free list available, but not large enough, more pages are needed
  return NULL;
}

#endif // KMA_RM
