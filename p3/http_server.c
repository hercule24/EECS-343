#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include "thread_pool.h"
#include "seats.h"
#include "util.h"

#define BUFSIZE 1024
#define FILENAMESIZE 100

int listenfd;
// TODO: Declare your threadpool!
pool_t *pool = NULL;

int main(int argc,char *argv[])
{
    int flag, num_seats = 20;
    int connfd = 0;
    struct sockaddr_in serv_addr;

    char send_buffer[BUFSIZE];
    
    listenfd = 0; 

    int server_port = 8080;

    if (argc > 1)
    {
        num_seats = atoi(argv[1]);
    } 

    if (server_port < 1500)
    {
        fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
        exit(-1);
    }
    
    if (signal(SIGINT, shutdown_server) == SIG_ERR) 
        printf("Issue registering SIGINT handler");

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( listenfd < 0 ){
        perror("Socket");
        exit(errno);
    }
    printf("Established Socket: %d\n", listenfd);
    flag = 1;
    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag) );

    // Load the seats;
    load_seats(num_seats);

    // set server address 
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(send_buffer, '0', sizeof(send_buffer));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(server_port);

    // bind to socket
    if ( bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) != 0)
    {
        perror("socket--bind");
        exit(errno);
    }

    // listen for incoming requests
    listen(listenfd, 10);

    // TODO: Initialize your threadpool!
    pool = pool_create(num_seats);

    // This while loop "forever", handling incoming connections
    while(1)
    {
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        /*********************************************************************
            You should not need to modify any of the code above this comment.
            However, you will need to add lines to declare and initialize your 
            threadpool!

            The lines below will need to be modified! Some may need to be moved
            to other locations when you make your server multithreaded.
        *********************************************************************/
        
        struct request *req = (struct request *) malloc(sizeof(struct request));
        pool_task_t *task = (pool_task_t *) malloc(sizeof(pool_task_t));
        task->status = PARSE;
        task->connfd = connfd;
        task->req = req;
        task->next = NULL;
        pool_add_task(pool, task);
    }
}

void shutdown_server(int signo)
{
    printf("Shutting down the server...\n");
    
    // TODO: Teardown your threadpool
    if (pool != NULL) {
        int i;
        for (i = 0; i < MAX_THREADS; i++) {
            if (pthread_cancel(pool->threads[i]) != 0) {
                fprintf(stderr, "Failed to cancel thread %d: %s\n", i, strerror(errno));
            }
            if (pthread_join(pool->threads[i], NULL) != 0) {
                fprintf(stderr, "Failed to join thread %d: %s\n", i, strerror(errno));
            }
        }
        pool_destroy(pool);
    }

    // TODO: Print stats about your ability to handle requests.
    unload_seats();
    close(listenfd);
    exit(0);
}
