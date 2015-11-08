#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#define PARSE 0
#define PROCESS 1

typedef struct pool_t pool_t;

typedef struct {
    char status;
    struct request *req; 
    int connfd;
} argument_t;

pool_t *pool_create(int thread_count, int queue_size);

int pool_add_task(pool_t *pool, void *(*routine)(void *), void *arg);

int pool_destroy(pool_t *pool);

void* handler(void *arg);

#endif
