#include <stdlib.h>
#include <unistd.h>
#include "thread_pool.h"

int sem_wait(m_sem_t *s)
{
    pthread_mutex_lock(&s->lock);
    // if the critical part is occupied, then wait
    if (s->occupied) {
        pthread_cond_wait(&s->cond, &s->lock); 
    }
    // set the critical part as occupied
    s->occupied = true;
    pthread_mutex_unlock(&s->lock);
    return 0;
}

int sem_post(m_sem_t *s)
{
    pthread_mutex_lock(&s->lock);
    // releasing the critical part.
    s->occupied = false;
    pthread_mutex_unlock(&s->lock);
    pthread_cond_broadcast(&s->cond);
    return 0;
}

void sem_init(m_sem_t *s)
{
    s->occupied = false;
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->cond, NULL);
}

void sem_destroy(m_sem_t *s) {
    pthread_mutex_destroy(&s->lock);
    pthread_cond_destroy(&s->cond);
}
