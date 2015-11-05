#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


typedef struct m_sem_t {
    int value;
} m_sem_t;

int sem_wait(m_sem_t *s);
int sem_post(m_sem_t *s);

int sem_wait(m_sem_t *s)
{
    //TODO
    return 0;
}

int sem_post(m_sem_t *s)
{
    //TODO
    return 0;
}
