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
#include <errno.h>

#include "util.h"
#include "seats.h"


#define BUFSIZE 1024

int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
int get_line(int, char*,int);

int parse_int_arg(char* filename, char* arg);

void parse_request(int connfd, struct request* req)
{
    char buf[BUFSIZE+1];
    char instr[20];
    char file[100];
    char type[20];

    int i=0;
    int j=0;


    char *bad_request = "HTTP/1.0 400 BAD REQUEST\r\n"\
                              "Content-type: text/html\r\n\r\n"\
                              "<html><body><h2>BAD REQUEST</h2>"\
                              "</body></html>\n";
    
    get_line(connfd, buf, BUFSIZE);
    
    //parse out instruction
    while( !isspace(buf[j]) && (i < sizeof(instr) - 1))
    {
        instr[i] = buf[i];
        i++;
        j++;
    }
    j+=2;
    instr[i] = '\0';

    //Only accept GET requests
    if (strncmp(instr, "GET", 3) != 0) {
        writenbytes(connfd, bad_request, strlen(bad_request));
        close(connfd);
        return;
    }

    //parse out filename
    i=0;
    while (!isspace(buf[j]) && (i < sizeof(file) - 1))
    {
        file[i] = buf[j];
        i++;
        j++;
    }
    j++;
    file[i] = '\0';

    //parse out type
    i=0;
    while (!isspace(buf[j]) && (buf[j] != '\0') && (i < sizeof(type) - 1))
    {
        type[i] = buf[j];
        i++;
        j++;
    }
    type[i] = '\0';

    while (get_line(connfd, buf, BUFSIZE) > 0)
    {
        //ignore headers -> (for now)
    }

    int length;
    for(i = 0; i < strlen(file); i++)
    {
        if(file[i] == '?')
            break;
    }
    length = i;
    
    req->resource = malloc(length+1);

    if (length > strlen(file)) {
      length = strlen(file);
    }

    strncpy(req->resource, file, length);
    req->resource[length] = 0;
    
    req->seat_id = parse_int_arg(file, "seat=");
    req->user_id = parse_int_arg(file, "user=");
    req->customer_priority = parse_int_arg(file, "priority=");
}

void process_request(int connfd, struct request* req)
{
    char *ok_response = "HTTP/1.0 200 OK\r\n"\
                           "Content-type: text/html\r\n\r\n";   
    
    char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                            "Content-type: text/html\r\n\r\n"\
                            "<html><body bgColor=white text=black>\n"\
                            "<h2>404 FILE NOT FOUND</h2>\n"\
                            "</body></html>\n";
    int fd;
    char buf[BUFSIZE+1];
    int length = strlen(req->resource);
    // Check if the request is for one of our operations
    if (strncmp(req->resource, "list_seats", length) == 0)
    {  
        list_seats(buf, BUFSIZE);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    } 
    else if(strncmp(req->resource, "view_seat", length) == 0)
    {
        view_seat(buf, BUFSIZE, req->seat_id, req->user_id, req->customer_priority);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    } 
    else if(strncmp(req->resource, "confirm", length) == 0)
    {
        confirm_seat(buf, BUFSIZE, req->seat_id, req->user_id, req->customer_priority);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    }
    else if(strncmp(req->resource, "cancel", length) == 0)
    {
        cancel(buf, BUFSIZE, req->seat_id, req->user_id, req->customer_priority);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    }
    else
    {
        // try to open the file
        if ((fd = open(req->resource, O_RDONLY)) == -1)
        {
            writenbytes(connfd, notok_response, strlen(notok_response));
        } 
        else
        {
            // send headers
            writenbytes(connfd, ok_response, strlen(ok_response));
            // send file
            int ret;
            while ( (ret = read(fd, buf, BUFSIZE)) > 0) {
                writenbytes(connfd, buf, ret);
            }  
            // close file and free space
            close(fd);
        } 
    }
    free(req->resource);
}

int get_line(int fd, char *buf, int size)
{
    int i=0;
    char c = '\0';
    int n;

    while((i < size-1) && (c != '\n'))
    {
        n = readnbytes(fd, &c, 1);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = readnbytes(fd, &c, 1);

                if ((n > 0) && (c == '\n'))
                {
                    //this is an \r\n endline for request
                    //we want to then return the line
                    //readnbytes(fd, &c, 1);
                    continue;
                } 
                else 
                {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } 
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

int readnbytes(int fd,char *buf,int size)
{
    int rc = 0;
    int totalread = 0;
    while ((rc = read(fd,buf+totalread,size-totalread)) > 0)
        totalread += rc;

    if (rc < 0)
    {
        return -1;
    }
    else
        return totalread;
}

int writenbytes(int fd,char *str,int size)
{
    int rc = 0;
    int totalwritten =0;
    while ((rc = write(fd,str+totalwritten,size-totalwritten)) > 0)
        totalwritten += rc;

    if (rc < 0)
        return -1;
    else
        return totalwritten;
}

int parse_int_arg(char* filename, char* arg)
{
    int i;
    bool found_value_start = false;
    bool found_arg_list_start = false;
    int intarg = 0;
    for(i=0; i < strlen(filename); i++)
    {
        if (!found_arg_list_start)
        {
            if (filename[i] == '?')
            {
                found_arg_list_start = true;
            }
            continue;
        }
        if (!found_value_start && strncmp(&filename[i], arg, strlen(arg)) == 0)
        {
            found_value_start = true;
            i += strlen(arg);
        }
        if (found_value_start)
        {
            if(isdigit(filename[i]))
            {
                intarg = intarg * 10 + (int) filename[i] - (int) '0';
                continue;
            } 
            else
            {
                break;
            }
        }
    }
    return intarg;
}
