#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

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
#define FIRST 1
#define BIZ 2
#define COACH 3

/*
 * Create a threadpool, initialize variables, etc
 *
 */
pool_t *pool_create(int queue_size, int num_threads)
{
    pthread_t threads[MAX_THREADS];
    pool_t *pool = (pool_t *) malloc(sizeof(pool_t));
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, thread_do_work, pool)) {
            fprintf(stderr, "Failed to create process %d: %s\n", i, strerror(errno));
        }
    }

    pthread_mutex_init(&pool->lock, NULL);
    //pthread_mutex_init(&pool->parse_queue_head_lock, NULL);
    //pthread_mutex_init(&pool->first_queue_head_lock, NULL);
    //pthread_mutex_init(&pool->biz_queue_head_lock, NULL);
    //pthread_mutex_init(&pool->coach_queue_head_lock, NULL);

    pthread_cond_init(&pool->notify, NULL);

    pool->parse_queue_head = NULL;
    pool->parse_queue_tail = NULL;
    pool->first_queue_head = NULL;
    pool->first_queue_tail = NULL;
    pool->biz_queue_head = NULL;
    pool->biz_queue_tail = NULL;
    pool->coach_queue_head = NULL;
    pool->coach_queue_tail = NULL;
    pool->thread_count = MAX_THREADS;
    pool->task_queue_size_limit = queue_size;
    return pool;
}


/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, pool_task_t *task)
{
    pthread_mutex_lock(&pool->lock);
    if (task->status == PARSE) {
        if (pool->parse_queue_head == NULL) {
            pool->parse_queue_head = task;
            pool->parse_queue_tail = task;
        } else {
            pool->parse_queue_tail->next = task;
            pool->parse_queue_tail = task;
        }
    }

    if (task->status == PROCESS) {
        struct request *req = task->req;
        if (req->customer_priority == FIRST) {
            if (pool->first_queue_head == NULL) {
                pool->first_queue_head = task;
                pool->first_queue_tail = task;
            } else {
                pool->first_queue_tail->next = task;
                pool->parse_queue_tail = task;
            }
        }
        if (req->customer_priority == BIZ) {
            if (pool->biz_queue_head == NULL) {
                pool->biz_queue_head = task;
                pool->biz_queue_tail = task;
            } else {
                pool->biz_queue_tail->next = task;
                pool->biz_queue_tail = task;
            }
        }
        if (req->customer_priority == COACH) {
            if (pool->coach_queue_head == NULL) {
                pool->coach_queue_head = task;
                pool->coach_queue_tail = task;
            } else {
                pool->coach_queue_tail->next = task;
                pool->coach_queue_tail = task;
            }
        }
    }
    pthread_mutex_unlock(&pool->lock);
    pthread_cond_broadcast(&pool->notify);
        
    return 0;
}



/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */
int pool_destroy(pool_t *pool)
{
    int err = 0;
 
    pthread_mutex_destroy(&pool->lock);
    //pthread_mutex_destroy(&(pool->parse_queue_head_lock));
    //pthread_mutex_destroy(&(pool->first_queue_head_lock));
    //pthread_mutex_destroy(&(pool->biz_queue_head_lock));
    //pthread_mutex_destroy(&(pool->coach_queue_head_lock));

    pthread_cond_destroy(&(pool->notify));
    free(pool);
    return err;
}

/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *arg)
{ 
    pool_t *pool = (pool_t *) arg;
    while (1) {
        pool_task_t *task = NULL;

        pthread_mutex_lock(&pool->lock);
        if (pool->parse_queue_head != NULL || pool->first_queue_head != NULL ||
                pool->biz_queue_head != NULL || pool->coach_queue_head != NULL) {
            if (pool->parse_queue_head != NULL) {
                task = pool->parse_queue_head;
                pool->parse_queue_head = task->next;
                break;
            }
            if (pool->first_queue_head != NULL) {
                task = pool->first_queue_head;
                pool->first_queue_head = task->next;
                break;
            }
            if (pool->biz_queue_head != NULL) {
                task = pool->biz_queue_head;
                pool->biz_queue_head = task->next;
                break;
            }
            if (pool->coach_queue_head != NULL) {
                task = pool->coach_queue_head;
                pool->coach_queue_head = task->next;
                break;
            }
        } else {
            pthread_cond_wait(&pool->notify, &pool->lock);
        }
        pthread_mutex_unlock(&pool->lock);

        if (task != NULL) {
            if (task->status == PROCESS) {
                process_request(task->connfd, task->req);
                close(task->connfd);
                free(task->req);
                free(task);
            }
            if (task->status == PARSE) {
                parse_request(task->connfd, task->req); 
                task->status = PROCESS;
                task->next = NULL;
                pool_add_task(pool, task);
            }
        }
    }
    pthread_exit(NULL);
}
