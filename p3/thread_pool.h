#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <pthread.h>
#include <errno.h>

#define PARSE 0
#define PROCESS 1

typedef struct pool_t pool_t;

// element to be added to the queue
typedef struct pool_task {
    int status;
    int connfd;
    struct request *req;
    struct pool_task *next;
} pool_task_t;

struct pool_t {
  pthread_mutex_t lock;
  //pthread_mutex_t parse_queue_head_lock;
  //pthread_mutex_t first_queue_head_lock;
  //pthread_mutex_t biz_queue_head_lock;
  //pthread_mutex_t coach_queue_head_lock;
  pthread_cond_t notify;
  //pthread_t *threads;
  pool_task_t *parse_queue_head;
  pool_task_t *parse_queue_tail;
  pool_task_t *first_queue_head;
  pool_task_t *first_queue_tail;
  pool_task_t *biz_queue_head;
  pool_task_t *biz_queue_tail;
  pool_task_t *coach_queue_head;
  pool_task_t *coach_queue_tail;
  int thread_count;
  int task_queue_size_limit;
};

pool_t *pool_create(int thread_count, int queue_size);

//int pool_add_task(pool_t *pool, void *(*routine)(void *), void *arg);
int pool_add_task(pool_t *pool, pool_task_t *task);

static void *thread_do_work(void *pool);

int pool_destroy(pool_t *pool);

#endif
