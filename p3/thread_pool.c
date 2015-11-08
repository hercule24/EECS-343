#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "thread_pool.h"
#include "util.h"

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  Feel free to make any modifications you want to the function prototypes and structs
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

#define MAX_THREADS 4
#define STANDBY_SIZE 10

// element to be added to the queue
typedef struct pool_task {
    //void (*function)(void *);
    void *argument;
    pool_task *next;
} pool_task_t;

struct pool_t {
  pthread_mutex_t lock;
  pthread_cond_t notify;
  pthread_t *threads;
  pool_task_t *queue;
  int thread_count;
  int task_queue_size_limit;
};

static void *thread_do_work(void *pool);

//pool_task_t *TASK_HEAD = NULL;

/*
 * Create a threadpool, initialize variables, etc
 *
 */
pool_t *pool_create(int queue_size, int num_threads)
{
    pthread_t threads[MAX_THREADS];
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, handler, NULL)) {
            fprintf(stderr, "Failed to create process %d: %s\n", i, strerror(errno));
        }
    }
    pool_t *pool = (pool_t *) malloc(sizeof(pool_t));
    pool->queue = NULL;
    pool->thread_count = MAX_THREADS;
    pool->task_queue_size_limit = queue_size;
    return pool;
}


/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, void* (*function)(void *), void *argument)
{
    int err = 0;
        
    return err;
}



/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */
int pool_destroy(pool_t *pool)
{
    int err = 0;
 
    free(pool);
    return err;
}



/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *pool)
{ 

    while(1) {
        
    }

    pthread_exit(NULL);
    return(NULL);
}

void* handler(void *arg)
{
    argument_t *arguments = (argument_t *) arg;
    if (arguments->status == PARSE) {
        parse_request(arguments->connfd, arguments->req);
    } 
    if (arguments->status == PROCESS) {
        process_request(arguments->connfd, arguments->req); 
    }
    pthread_exit(NULL);
}
