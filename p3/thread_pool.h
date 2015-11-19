#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>

#define PARSE 0
#define PROCESS 1

#define MAX_THREADS 4
#define STANDBY_SIZE 10
#define FIRST 1
#define BIZ 2
#define COACH 3

typedef struct pool_t pool_t;

// element to be added to the queue
typedef struct pool_task {
    int status;
    int connfd;
    struct request *req;
    struct pool_task *next;
} pool_task_t;

typedef struct m_sem_t {
    int occupied;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} m_sem_t;

struct pool_t {
  pthread_mutex_t head_lock;
  pthread_mutex_t *seat_locks;
  m_sem_t sem;
  //pthread_mutex_t parse_queue_head_lock;
  //pthread_mutex_t first_queue_head_lock;
  //pthread_mutex_t biz_queue_head_lock;
  //pthread_mutex_t coach_queue_head_lock;
  pthread_cond_t notify;
  pthread_t threads[MAX_THREADS];
  pthread_t clean_thread;
  pool_task_t *parse_queue_head;
  pool_task_t *parse_queue_tail;
  pool_task_t *first_queue_head;
  pool_task_t *first_queue_tail;
  pool_task_t *biz_queue_head;
  pool_task_t *biz_queue_tail;
  pool_task_t *coach_queue_head;
  pool_task_t *coach_queue_tail;
  pool_task_t *standby_list_head;
  pool_task_t *standby_list_tail;
  int standby_list_size;
  //int thread_count;
  int num_seats;
  int seats_taken;
  pthread_mutex_t seats_taken_lock;
  //int task_queue_size_limit;
};

pool_t *pool_create(int num_seats);

//int pool_add_task(pool_t *pool, void *(*routine)(void *), void *arg);
int pool_add_task(pool_t *pool, pool_task_t *task);

void *thread_do_work(void *pool);

int pool_destroy(pool_t *pool);

void shutdown_server(int);

void *cleanUp(void *args);

void sem_init(m_sem_t *s);

void sem_destroy(m_sem_t *s);

int sem_wait(m_sem_t *s);

int sem_post(m_sem_t *s);

void addToStandbyList(pool_t *pool, pool_task_t *task);

#endif
