#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "thread_pool.h"
#include "util.h"
#include "seats.h"

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  Feel free to make any modifications you want to the function prototypes and structs
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

/*
 * Create a threadpool, initialize variables, etc
 *
 */
extern seat_t *seat_header;

pool_t *pool_create(int num_seats)
{
    pool_t *pool = (pool_t *) malloc(sizeof(pool_t));
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (pthread_create(&pool->threads[i], NULL, thread_do_work, pool) != 0) {
            fprintf(stderr, "Failed to create thread %d: %s\n", i, strerror(errno));
        }
    }

    if (pthread_create(&pool->clean_thread, NULL, cleanUp, NULL) != 0) {
        fprintf(stderr, "Failed to create thread %d: %s\n", i, strerror(errno));
    }

    pthread_mutex_init(&pool->head_lock, NULL);

    // create a lock for every seat
    i = 0;
    pool->seat_locks = (pthread_mutex_t *) malloc(num_seats * sizeof(pthread_mutex_t));
    for ( ; i < num_seats; i++) {
        pthread_mutex_init(&pool->seat_locks[i], NULL);
    }
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

    pool->num_seats = num_seats;
    //pool->thread_count = MAX_THREADS;

    return pool;
}


/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, pool_task_t *task)
{
    pthread_mutex_lock(&pool->head_lock);
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
    pthread_mutex_unlock(&pool->head_lock);
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
 
    pthread_mutex_destroy(&pool->head_lock);
    int i;
    for (i = 0; i < pool->num_seats; i++) {
        pthread_mutex_destroy(&pool->seat_locks[i]);
    }
    free(pool->seat_locks);
    //pthread_mutex_destroy(&(pool->parse_queue_head_lock));
    //pthread_mutex_destroy(&(pool->first_queue_head_lock));
    //pthread_mutex_destroy(&(pool->biz_queue_head_lock));
    //pthread_mutex_destroy(&(pool->coach_queue_head_lock));

    pthread_cond_destroy(&pool->notify);
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

        pthread_mutex_lock(&pool->head_lock);
        if (pool->parse_queue_head != NULL || pool->first_queue_head != NULL ||
                pool->biz_queue_head != NULL || pool->coach_queue_head != NULL) {
            if (pool->parse_queue_head != NULL) {
                task = pool->parse_queue_head;
                pool->parse_queue_head = task->next;
                if (pool->parse_queue_head == NULL) {
                    pool->parse_queue_tail = NULL;
                }
                break;
            }
            if (pool->first_queue_head != NULL) {
                task = pool->first_queue_head;
                pool->first_queue_head = task->next;
                if (pool->first_queue_head == NULL) {
                    pool->first_queue_tail = NULL;
                }
                break;
            }
            if (pool->biz_queue_head != NULL) {
                task = pool->biz_queue_head;
                pool->biz_queue_head = task->next;
                if (pool->biz_queue_head == NULL) {
                    pool->biz_queue_tail = NULL;
                }
                break;
            }
            if (pool->coach_queue_head != NULL) {
                task = pool->coach_queue_head;
                pool->coach_queue_head = task->next;
                if (pool->coach_queue_head == NULL) {
                    pool->coach_queue_tail = NULL;
                }
                break;
            }
        } else {
            pthread_cond_wait(&pool->notify, &pool->head_lock);
        }
        pthread_mutex_unlock(&pool->head_lock);

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

void *cleanUp(void *args) {
    pool_t *pool = (pool_t *) args;
    int n = pool->num_seats;
    int status[n];
    int cid[n]; 
    int i = 0;
    for (; i < n; ++i) {
	status[i] = 0;
	cid[i] = -1;
    }
    while (1) {
	sleep(5);
	seat_t *cur = seat_header;
	while (cur != NULL) {
	    if (cur->state == PENDING) {
		i = cur->id - 1;
		if (status[i] == 1 && cur->customer_id == cid[i]) {
		    pthread_mutex_lock(&pool->seat_locks[i]);
		    cur->state = AVAILABLE;
		    pthread_mutex_unlock(&pool->seat_locks[i]);
		    status[i] = 0;
		} else if (status[i] == 0) {
		    status[i] = 1;
		    cid[i] = cur->customer_id;
		} else {
		    cid[i] = cur->customer_id;
		}
	    }
	    cur = cur->next;
	}
    }
    pthread_exit(NULL);
}
