#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "seats.h"
#include "util.h"
#include "thread_pool.h"

seat_t* seat_header = NULL;
extern pool_t *pool;

char seat_state_to_char(seat_state_t);

void list_seats(char* buf, int bufsize)
{
    seat_t* curr = seat_header;
    int index = 0;
    while(curr != NULL && index < bufsize+ strlen("%d %c,"))
    {
        int length = snprintf(buf+index, bufsize-index, 
                "%d %c,", curr->id, seat_state_to_char(curr->state));
        if (length > 0)
            index = index + length;
        curr = curr->next;
    }
    if (index > 0) {
        snprintf(buf+index-1, bufsize-index-1, "\n");
    } else {
        snprintf(buf, bufsize, "No seats not found\n\n");
    }
}

// return 1 if this request gets into the standby list
int view_seat(char* buf, int bufsize,  int seat_id, int customer_id, int customer_priority)
{
    seat_t* curr = seat_header;
    int res = 0;
    while(curr != NULL)
    {
        if(curr->id == seat_id)
        {
            pthread_mutex_lock(&pool->seat_locks[curr->id]);
            if(curr->state == AVAILABLE || (curr->state == PENDING && curr->customer_id == customer_id))
            {
                snprintf(buf, bufsize, "Confirm seat: %d %c ?\n\n",
                        curr->id, seat_state_to_char(curr->state));
                curr->state = PENDING;
                curr->customer_id = customer_id;
                pthread_mutex_lock(&pool->seats_taken_lock);
                pool->seats_taken++;
                pthread_mutex_unlock(&pool->seats_taken_lock);
            }
            else
            {
                snprintf(buf, bufsize, "Seat unavailable\n\n");
                pthread_mutex_lock(&pool->seats_taken_lock);
                if (pool->seats_taken == pool->num_seats) {
                    res = 1;
                }
                pthread_mutex_unlock(&pool->seats_taken_lock);
            }
            pthread_mutex_unlock(&pool->seat_locks[curr->id]);
            return res;
        }
        curr = curr->next;
    }
    snprintf(buf, bufsize, "Requested seat not found\n\n");
    return res;
}

void confirm_seat(char* buf, int bufsize, int seat_id, int customer_id, int customer_priority)
{
    seat_t* curr = seat_header;
    while(curr != NULL)
    {
        if(curr->id == seat_id)
        {
            pthread_mutex_lock(&pool->seat_locks[curr->id]);
            if(curr->state == PENDING && curr->customer_id == customer_id )
            {
                snprintf(buf, bufsize, "Seat confirmed: %d %c\n\n",
                        curr->id, seat_state_to_char(curr->state));
                curr->state = OCCUPIED;
            }
            else if(curr->customer_id != customer_id )
            {
                snprintf(buf, bufsize, "Permission denied - seat held by another user\n\n");
            }
            else if(curr->state != PENDING)
            {
                snprintf(buf, bufsize, "No pending request\n\n");
            }
            pthread_mutex_unlock(&pool->seat_locks[curr->id]);

            return;
        }
        curr = curr->next;
    }
    snprintf(buf, bufsize, "Requested seat not found\n\n");
    
    return;
}

// return the seat_id if it's a successful cancel request
void cancel(char* buf, int bufsize, int seat_id, int customer_id, int customer_priority)
{
    seat_t* curr = seat_header;
    while(curr != NULL)
    {
        if(curr->id == seat_id)
        {
            pthread_mutex_lock(&pool->seat_locks[curr->id]);
            if(curr->state == PENDING && curr->customer_id == customer_id )
            {
                snprintf(buf, bufsize, "Seat request cancelled: %d %c\n\n",
                        curr->id, seat_state_to_char(curr->state));
                curr->state = AVAILABLE;
                curr->customer_id = -1;

                sem_wait(&pool->sem);
                if (pool->standby_list_head != NULL) {
                    pool_task_t *task = pool->standby_list_head;
                    pool->standby_list_head = task->next;
                    pool->standby_list_size--;
                    if (pool->standby_list_head == NULL) {
                        pool->standby_list_tail = NULL;
                    }
                    curr->state = OCCUPIED;
                    customer_id = task->req->user_id;
                    printf("closing request if a stanbylist task is finished: user id = %d, connfd = %d\n", task->req->user_id, task->connfd);
                    close(task->connfd);
                    free(task->req);
                    free(task);
                } else {
                    pthread_mutex_lock(&pool->seats_taken_lock);
                    pool->seats_taken--;
                    pthread_mutex_unlock(&pool->seats_taken_lock);
                }
                sem_post(&pool->sem);
            }
            else if(curr->customer_id != customer_id )
            {
                snprintf(buf, bufsize, "Permission denied - seat held by another user\n\n");
            }
            else if(curr->state != PENDING)
            {
                snprintf(buf, bufsize, "No pending request\n\n");
            }
            pthread_mutex_unlock(&pool->seat_locks[curr->id]);

            return;
        }
        curr = curr->next;
    }
    snprintf(buf, bufsize, "Seat not found\n\n");
    return;
}

void load_seats(int number_of_seats)
{
    seat_t* curr = NULL;
    int i;
    for(i = 0; i < number_of_seats; i++)
    {   
        seat_t* temp = (seat_t*) malloc(sizeof(seat_t));
        temp->id = i;
        temp->customer_id = -1;
        temp->state = AVAILABLE;
        temp->next = NULL;
        
        if (seat_header == NULL)
        {
            seat_header = temp;
        }
        else
        {
            curr-> next = temp;
        }
        curr = temp;
    }
}

void unload_seats()
{
    seat_t* curr = seat_header;
    while(curr != NULL)
    {
        seat_t* temp = curr;
        curr = curr->next;
        free(temp);
    }
}

char seat_state_to_char(seat_state_t state)
{
    switch(state)
    {
        case AVAILABLE:
            return 'A';
        case PENDING:
            return 'P';
        case OCCUPIED:
            return 'O';
    }
    return '?';
}
