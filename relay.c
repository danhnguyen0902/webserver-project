/* $begin Server main */
/*
 * server.c - A simple, connectionative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"
#include "threadpool.h"
#include "list.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_AMOUNT 1024

// Global variables

char** ID[MAX_AMOUNT];
int CONNECT[MAX_AMOUNT];
int count = 0;

/*  
 * Contains the data for each execution
 */
/*Data to be passed to handle() function. */
struct callable_data 
{
    int fd;
};

/*  
 * This is the thread handle that calls the server 
 * functions and closes the socket when it is done.
 */
static void storeid(int fd)
{
        /* Read request line and headers */
        Rio_readinitb(&rio, fd);
        size = Rio_readlineb(&rio, buf, MAXLINE);
        sscanf(buf, "%s %s %s", method, uri, version);    

        CONNECT[count] = fd
        count++;
}

/*  
 * This is the thread handle that calls the server 
 * functions and closes the socket when it is done.
 */
static void* handle(struct callable_data * callable)
{
    int fd = callable->fd;

    doit(fd);                                              
    Close(fd);                                           

    return NULL;
}
/*  
 * This is the thread handle that calls the server 
 * functions and closes the socket when it is done.
 */
static void* handleclient(struct callable_data * callable)
{
    int listenfd = callable->fd;
    while(1) 
    {
        clientlen = sizeof(clientaddr);

        fd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
        struct callable_data * callable_data = malloc(sizeof *callable_data);
        callable_data->fd = fd;

        thread_pool_submit(pool, (thread_pool_callable_func_t) handle, callable_data);
    }                                     

    thread_pool_shutdown(pool);

    return NULL;
}
/*  
 * This is the thread handle that calls the server 
 * functions and closes the socket when it is done.
 */
static void* handleserver(struct callable_data * callable)
{
    int fd = callable->fd;

    while(1) 
    {
        clientlen = sizeof(clientaddr);

        fd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 

        storeid(fd);
    }
    return NULL;
}

/*  
 * open_listenfd - open and return a listening socket on port
 *     Returns -1 and sets errno on Unix error.
 * This is done for IPV6
 */
/* $begin open_listenfd */
int open_listenfd6(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in6 serveraddr;
  
    //Create a socket file descriptor
    if ((listenfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
    return -1;
 
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
           (const void *)&optval , sizeof(int)) < 0)
    return -1;

    // Listenfd will be an endpoint for given requests
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin6_family = AF_INET6; 
    serveraddr.sin6_addr = in6addr_any; 
    serveraddr.sin6_port = htons((unsigned short)port); 
    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
    return -1;

    // Make it a listening socket ready to accept connection requests
    if (listen(listenfd, LISTENQ) < 0)
    return -1;
    return listenfd;
}
/* $end open_listenfd */

/*  
 * This calls the listening function with error handling
 */
int Open_listenfd6(int port) 
{
    int rc;

    if ((rc = open_listenfd6(port)) < 0)
        unix_error("Open_listenfd error");
    
    return rc;
}

/*  
 * This is the main function for the server
 */
int main(int argc, char **argv) 
{
    Signal(SIGPIPE, SIG_IGN);

    int listenfd, fd;
    socklen_t clientlen;
    struct sockaddr_in6 clientaddr;
    int opt;    
    int port = 10100;   // default port
    int port2 = 11090;
    }

    // sets up the server thread pools and mode
    int nThreads = 10;
    struct thread_pool * pool = thread_pool_new(nThreads);

    struct addrinfo info;
    struct addrinfo *server, *connection;
    int rv;
    int server_fd;

    char address[INET_ADDRSTRLEN];             

    memset(&info, 0, sizeof info);

    //Begin listenning to the port
    listenfd = Open_listenfd6(port); 
    listenfdclient = Open_listenfd6(port2); 

    //server thread
    struct callable_data * callable_data = malloc(sizeof *callable_data);
    callable_data->fd = listenfd;
    thread_pool_submit(pool, (thread_pool_callable_func_t) handle, callable_data);
    //client thread
    struct callable_data * callable_data2 = malloc(sizeof *callable_data);
    callable_data->fd = listenfd2;
    thread_pool_submit(pool, (thread_pool_callable_func_t) handle, callable_data2);
}

//Exectutes a relaying service for the client fd
void doit(int fd) 
{

}