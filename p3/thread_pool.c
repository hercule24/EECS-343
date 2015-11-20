#include <stdlib.h>
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
extern pthread_mutex_t time_lock;
extern int num_requests;
extern time_t total_time;

pool_t *pool_create(int num_seats)
{
    // initialize the lock for worker queues
    pool_t *pool = (pool_t *) malloc(sizeof(pool_t));
    pthread_mutex_init(&pool->head_lock, NULL);
    // create a lock for every seat
    int i;
    pool->seat_locks = (pthread_mutex_t *) malloc(num_seats * sizeof(pthread_mutex_t));
    for (i = 0; i < num_seats; i++) {
        pthread_mutex_init(&pool->seat_locks[i], NULL);
    }

    sem_init(&pool->sem);

    pthread_cond_init(&pool->notify, NULL);

    // create a queue for every kind of task
    pool->parse_queue_head = NULL;
    pool->parse_queue_tail = NULL;
    pool->first_queue_head = NULL;
    pool->first_queue_tail = NULL;
    pool->biz_queue_head = NULL;
    pool->biz_queue_tail = NULL;
    pool->coach_queue_head = NULL;
    pool->coach_queue_tail = NULL;
    pool->standby_list_head = NULL;
    pool->standby_list_tail = NULL;

    pool->standby_list_size = 0;

    pool->num_seats = num_seats;
    pool->seats_taken = 0;
    pthread_mutex_init(&pool->seats_taken_lock, NULL);

    // create working threads
    for (i = 0; i < MAX_THREADS; i++) {
        if (pthread_create(&pool->threads[i], NULL, thread_do_work, pool) != 0) {
            fprintf(stderr, "Failed to create thread %d: %s\n", i, strerror(errno));
        }
    }

    // create clean up thread
    if (pthread_create(&pool->clean_thread, NULL, cleanUp, pool) != 0) {
        fprintf(stderr, "Failed to create thread %d: %s\n", i, strerror(errno));
    }

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
    } else if (task->status == PROCESS) {
        struct request *req = task->req;
        if (req->customer_priority == FIRST) {
            if (pool->first_queue_head == NULL) {
                pool->first_queue_head = task;
                pool->first_queue_tail = task;
            } else {
                pool->first_queue_tail->next = task;
                pool->parse_queue_tail = task;
            }
        } else if (req->customer_priority == BIZ) {
            if (pool->biz_queue_head == NULL) {
                pool->biz_queue_head = task;
                pool->biz_queue_tail = task;
            } else {
                pool->biz_queue_tail->next = task;
                pool->biz_queue_tail = task;
            }
            // somehow 0 indicates a priority, make it as COACH here
        } else if (req->customer_priority == COACH || req->customer_priority == 0) {
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
    // notify all other waiting processes.
    pthread_cond_broadcast(&pool->notify);
        
    return 0;
}


/**
 * Add a task to the standby list
 */
int addToStandbyList(pool_t *pool, pool_task_t *task)
{
    int res; 
    sem_wait(&pool->sem);
    if (pool->standby_list_head == NULL) {
        pool->standby_list_head = task;
        pool->standby_list_tail = task;
        pool->standby_list_size++;
        res = 0;
    } else if (pool->standby_list_size == STANDBY_SIZE) {
        res = -1;
    } else {
        pool->standby_list_tail->next = task;
        pool->standby_list_tail = task;
        pool->standby_list_size++;
        res = 0;
    }
    sem_post(&pool->sem);
    return res;
}


/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */
int pool_destroy(pool_t *pool)
{
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (pthread_cancel(pool->threads[i]) != 0) {
            fprintf(stderr, "Failed to cancel thread %d: %s\n", i, strerror(errno));
            return -1;
        }
        if (pthread_join(pool->threads[i], NULL) != 0) {
            fprintf(stderr, "Failed to join thread %d: %s\n", i, strerror(errno));
            return -1;
        }
    }

    if (pthread_cancel(pool->clean_thread) != 0) {
        fprintf(stderr, "Failed to cancel clean up thread %d: %s\n", i, strerror(errno));
        return -1;
    }
    if (pthread_join(pool->clean_thread, NULL) != 0) {
        fprintf(stderr, "Failed to join clean up thread %d: %s\n", i, strerror(errno));
        return -1;
    }
 
    pthread_mutex_destroy(&pool->head_lock);
    for (i = 0; i < pool->num_seats; i++) {
        pthread_mutex_destroy(&pool->seat_locks[i]);
    }
    free(pool->seat_locks);

    sem_destroy(&pool->sem);

    pthread_cond_destroy(&pool->notify);
    pthread_mutex_destroy(&pool->seats_taken_lock);
    free(pool);
    return 0;
}

/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
void *thread_do_work(void *arg)
{ 
    pool_t *pool = (pool_t *) arg;
    while (1) {
        pool_task_t *task = NULL;

        // check if there is task available, do the task
        pthread_mutex_lock(&pool->head_lock);
        if (pool->parse_queue_head != NULL || pool->first_queue_head != NULL || 
                pool->biz_queue_head != NULL || pool->coach_queue_head != NULL) {
            if (pool->parse_queue_head != NULL) {
                task = pool->parse_queue_head;
                pool->parse_queue_head = task->next;
                if (pool->parse_queue_head == NULL) {
                    pool->parse_queue_tail = NULL;
                }
            } else if (pool->first_queue_head != NULL) {
                task = pool->first_queue_head;
                pool->first_queue_head = task->next;
                if (pool->first_queue_head == NULL) {
                    pool->first_queue_tail = NULL;
                }
            } else if (pool->biz_queue_head != NULL) {
                task = pool->biz_queue_head;
                pool->biz_queue_head = task->next;
                if (pool->biz_queue_head == NULL) {
                    pool->biz_queue_tail = NULL;
                }
            } else if (pool->coach_queue_head != NULL) {
                task = pool->coach_queue_head;
                pool->coach_queue_head = task->next;
                if (pool->coach_queue_head == NULL) {
                    pool->coach_queue_tail = NULL;
                }
            }
        } else {
            // otherwise wait to be notified.
            pthread_cond_wait(&pool->notify, &pool->head_lock);
        }
        pthread_mutex_unlock(&pool->head_lock);

        if (task != NULL) {
            if (task->status == PARSE) {
                parse_request(task->connfd, task->req); 
                //printf("incoming request: user id = %d, connfd = %d, seat_id = %d\n", task->req->user_id, task->connfd, task->req->seat_id);
                task->status = PROCESS;
                task->next = NULL;
                pool_add_task(pool, task);
            } else if (task->status == PROCESS) {
                int res = process_request(task->connfd, task->req);
                // res = 1, indicating the task is added to the standby list
                if (res == 1) {
                    if (addToStandbyList(pool, task) == -1) {
                        //printf("closing request if stanbylist is full: user id = %d, connfd = %d\n", task->req->user_id, task->connfd);

                        pthread_mutex_lock(&time_lock);
                        total_time += time(0) - task->start_time;
                        num_requests += 1;
                        pthread_mutex_unlock(&time_lock);

                        close(task->connfd);
                        free(task->req);
                        free(task);
                    }
                } else {
                    //printf("closing request if finished successfully: user id = %d, connfd = %d\n", task->req->user_id, task->connfd);

                    pthread_mutex_lock(&time_lock);
                    total_time += time(0) - task->start_time;
                    num_requests += 1;
                    pthread_mutex_unlock(&time_lock);

                    close(task->connfd);
                    free(task->req);
                    free(task);
                }
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
        // sleep by 5 secs to check any pending
	sleep(5);
	seat_t *cur = seat_header;
	while (cur != NULL) {
	    if (cur->state == PENDING) {
		i = cur->id; // - 1 if start from 1
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
