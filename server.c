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

void doit(int fd);
int read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int ver, char* uri);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int ver);
void clienterror(int fd, char *cause, char *errnum, 
         char *shortmsg, char *longmsg, int ver);

/* ------------------------ 2.1 System Status Web Service --------------------------------- */
static void write_response(int fd, int version, char* output);
static void loadavg_parser(int fd, int version, char* input);
static void meminfo_parser(int fd, int version, char* input);
/* ---------------------------------------------------------------------------------------- */

/* ------------------------ 2.3 Synthetic Load Requests ----------------------------------- */
static void runloop(int fd, int version);
static void allocanon(int fd, int version);
static void freeanon(int fd, int version);
/* ---------------------------------------------------------------------------------------- */

#define MAX_AMOUNT 1024

// Global variables
int numMap = 0;
void* block[MAX_AMOUNT];    
int totalBlocks = 0;
bool not_complete;
char* path;

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
static void* handle(struct callable_data * callable)
{
    int fd = callable->fd;

    doit(fd);                                              
    Close(fd);                                           
    free(callable);

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
 *  Gets the socket address
 */
void *get_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) 
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*  
 * Given a socket and a string, send the string's length to the socket
 */
int send_group_pid(int socket, char *str) 
{
    return send(socket, str, strlen(str), 0);
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
    path = NULL;
    int port = 10100;   // default port
    bool rel_act = 0;
    char* relay = NULL;

    /* Process command-line arguments. */
    while ((opt = getopt(argc, argv, "hp:r:R:")) > 0) {
        switch (opt) {
        case 'h':
            fprintf(stderr, "usage: %s -p <port> -r <relayhost:port> -R <path> \n", argv[0]);
            fprintf(stderr, "Options\n");
            fprintf(stderr, "\t-p <port> \n");
            fprintf(stderr, "\t-r <relayhost:port> \n");
            fprintf(stderr, "\t-R <path> \n");
            
            exit(1);
            break;
        case 'p':
            port = atoi(optarg);
            break;

        case 'r':
            rel_act = 1;
            relay = optarg;
            break;
        case 'R':
            path = optarg;  
            break;
        }
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
    
    //Relay mode
    if (rel_act) {
        char *relay_port = NULL;
        char *relay_server = strtok_r(relay, ":", &relay_port);

        info.ai_family = AF_INET;
        info.ai_socktype = SOCK_STREAM;

        while (1) 
        {
            if ((rv = getaddrinfo(relay_server, relay_port, &info, &server)) != 0) 
            {
                    return 1;
            }

            //Connect to available
            for (connection = server; connection != NULL; connection = connection->ai_next) 
            {
                    server_fd = socket(connection->ai_family, connection->ai_socktype, connection->ai_protocol);

                    if (server_fd < 0) {
                            continue;
                    }
                    //Retry connection if fails
                    if (connect(server_fd, connection->ai_addr, connection->ai_addrlen) != 0) {
                            close(server_fd);
                            continue;
                    }
                    break;
            }

            // No connection available            
            if (connection == NULL) 
            {
                    return 1;
            }

            freeaddrinfo(server);

            inet_ntop(connection->ai_family, get_addr((struct sockaddr *)connection->ai_addr), address, sizeof address);

            //Send member PID
            send_group_pid(server_fd, "group400");
            send_group_pid(server_fd, "\r\n");

            struct callable_data * callable_data = malloc(sizeof *callable_data);
            callable_data->fd = server_fd;

            handle(callable_data);
        }
    } 
    else 
    {
        //Server Mode
        //Begin listenning to the port
        listenfd = Open_listenfd6(port); 

        while(1) 
        {
            clientlen = sizeof(clientaddr);

            fd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 

            struct callable_data * callable_data = malloc(sizeof *callable_data);
            callable_data->fd = fd;

            thread_pool_submit(pool, (thread_pool_callable_func_t) handle, callable_data);
        }
    }

    thread_pool_shutdown(pool);
}

/*
 * doit - handle one HTTP request/response transaction
 * in a possible loop
 */
/* $begin doit */
void doit(int fd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    int loop = 1;
    int size = 1;

    //Looping for persistant connection
    while (loop)
    { 
        /* Read request line and headers */
        Rio_readinitb(&rio, fd);
        size = Rio_readlineb(&rio, buf, MAXLINE);
        if (!size)
        {
            break;  
        }

        sscanf(buf, "%s %s %s", method, uri, version);    

        if (strcasecmp(method, "GET")) {                     
           clienterror(fd, method, "501", "Not Implemented",
                    "Server does not implement this method", loop);
            return;
        }                              
                    
        size = read_requesthdrs(&rio);
        if (!size)
        {
            break;  
        }

        // Check version of the request
        if (strcasecmp(version, "HTTP/1.1"))
        {
            loop = 0;
        }

    /* ------------------------ 2.1 System Status Web Service --------------------------------- */
        //loadavg
        if ((strlen(uri) == 8 && strncmp(uri, "/loadavg", 8) == 0) || 
            (strlen(uri) > 8 && strncmp(uri, "/loadavg?", 9) == 0 && (strstr(uri, "callback=") != NULL || strstr(uri, "=") != NULL)))
        {
            loadavg_parser(fd, loop, uri);
            continue;
        }
        //meminfo
        if ((strlen(uri) == 8 && strncmp(uri, "/meminfo", 8) == 0) || 
            (strlen(uri) > 8 && strncmp(uri, "/meminfo?", 9) == 0 && (strstr(uri, "callback=") != NULL || strstr(uri, "=") != NULL)))
        {
            meminfo_parser(fd, loop, uri);
            continue;
        }

    /* ------------------------- 2.3 Synthetic Load Requests  --------------------------------- */
        // runloop
        if (strcmp(uri, "/runloop") == 0)
        {
            runloop(fd, loop);
            continue;
        }

        // allocanon
        if (strcmp(uri, "/allocanon") == 0)
        {
            allocanon(fd, loop);
            continue;
        }

        // freeanon
        if (strcmp(uri, "/freeanon") == 0)
        {
            freeanon(fd, loop);
            continue;
        }

    /*  ------------------------------------ File serving -----------------------------------------------*/ 
        // Parse URI from GET request 
        is_static = parse_uri(uri, filename, cgiargs);       
        if (stat(filename, &sbuf) < 0) {                     
            clienterror(fd, filename, "404", "Not found",
                    "Server couldn't find this file", loop);
            return;
        }                                                    

        /* Serve static content */          
        if (is_static) 
        { 
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) 
            {
                clienterror(fd, filename, "403", "Forbidden",
                    "Server couldn't read the file", loop);
                return;
            }
            
            serve_static(fd, filename, sbuf.st_size, loop, uri);
        }
        else  /* Serve dynamic content */
        { 
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) 
            {
                clienterror(fd, filename, "403", "Forbidden",
                    "Server couldn't run the CGI program", loop);
                return;
            }
            
            serve_dynamic(fd, filename, cgiargs, loop);        
        }
    }
}
/* $end doit */

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
int read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];
    int size;

    size = Rio_readlineb(rp, buf, MAXLINE);
    if (!size)
    {
        return 0;
    }
    //For incomplete Requests
    while (strcmp(buf, "\r\n") && strcmp(buf, "")) {    
        size = Rio_readlineb(rp, buf, MAXLINE);
        if (!size)
        {
            return 0;
        }
        printf("%s", buf);
    }

    if (!strcmp(buf, ""))
    {
        size = 0;
    }

    return size;    
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
    strcpy(cgiargs, "");                 
    strcpy(filename, ".");               
    strcat(filename, uri);               
    if (uri[strlen(uri)-1] == '/')       
        strcat(filename, "home.html");   
    return 1;
    }
    else {  /* Dynamic content */        
    ptr = index(uri, '?');               
    if (ptr) {
        strcpy(cgiargs, ptr+1);
        *ptr = '\0';
    }
    else 
        strcpy(cgiargs, "");                       
    strcpy(filename, ".");                         
    strcat(filename, uri);                         
    return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize, int ver, char *uri) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
 
    char tmp[MAX_AMOUNT];

    if (path != NULL)
    {
        strcat(tmp, path);
        strcat(tmp, filename + 1);
        strcpy(filename, tmp);
    }

    /* Send response headers to client */
    get_filetype(filename, filetype);       

    // Check if it is a valid directory
    char* firstCheck = strstr(uri, "..");
    char* secondCheck = strstr(uri, "/files/");
    if (firstCheck != NULL || secondCheck == NULL)
    {
        clienterror(fd, filename, "403", "Forbidden",
                    "Access Denied", ver);
        
        return;
    }

    sprintf(buf, "HTTP/1.%d 200 OK\r\n", ver);
    sprintf(buf, "%sServer: Server Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));       

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);    
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);                           
    Rio_writen(fd, srcp, filesize);         
    Munmap(srcp, filesize);                 
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
       strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
       strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
       strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".js"))
        strcpy(filetype, "text/javascript");
    else
       strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs, int ver) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.%d 200 OK\r\n", ver); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* child */ 
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); 
    Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ 
    Execve(filename, emptylist, environ); /* Run CGI program */ 
    }
    Wait(NULL); /* Parent waits for and reaps child */ 
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
         char *shortmsg, char *longmsg, int ver) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Server Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.%d %s %s\r\n", ver, errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

/*
 * This writes out an HTML text response to the fd to send out
 */
static void write_response(int fd, int version, char* output)
{
    // Header
    char header[MAX_AMOUNT];
    sprintf(header, "HTTP/1.%d 200 OK\r\n", version);   
    sprintf(header, "%sServer: Web Server\r\n", header);
    sprintf(header, "%sContent-length: %d\r\n", header, (int)strlen(output)); 
    sprintf(header, "%sContent-type: %s\r\n\r\n", header, "text/html");
    Rio_writen(fd, header, strlen(header));       

    // Content
    Rio_writen(fd, output, strlen(output));
}

/*
 * This parses out the loadAVG function from the system loadavg file.
 */
static void loadavg_parser(int fd, int version, char* input)
{
    int callback_loc = 0;
    char* output = malloc(MAX_AMOUNT * sizeof(char));
    output[0] = '\0';

    char* callback = strstr(input, "?callback=");  

    if (callback == NULL)
    {
        callback = strstr(input, "&callback=");          
    }
    
    if (callback != NULL)
    {
        callback += 10;
        while (*callback != '&' && *callback != '\0')    
        {
            output[callback_loc] = *callback;
            callback_loc++;
            callback++;
        }
        output[callback_loc] = '\0';
        
        strcat(output, "(");
    }
    
    // Open the /proc/loadavg file to read
    FILE* file = fopen("/proc/loadavg", "r");
    if (file == NULL)
    {
        return;
    }

    // Start reading
    float loadavg[3];
    int runningThreads;
    int totalThreads;

    fscanf(file, "%f %f %f", &loadavg[0], &loadavg[1], &loadavg[2]);
    fscanf(file, "%d/%d", &runningThreads, &totalThreads);

    char str[MAX_AMOUNT];
    // total threads
    strcat(output, "{\"total_threads\": \"");
    sprintf(str, "%d\", \"loadavg\": [\"", totalThreads);
    strcat(output, str);    

    // loadavg
    // 1
    sprintf(str, "%f\", \"", loadavg[0]);
    strcat(output, str);    

    // 2
    sprintf(str, "%f\", \"", loadavg[1]);
    strcat(output, str);    

    // 3
    sprintf(str, "%f\"], \"running_threads\": \"", loadavg[2]);
    strcat(output, str);    

    // running threads
    sprintf(str, "%d\"}", runningThreads);
    strcat(output, str);    

    if (callback != NULL)
    {
        strcat(output, ")");
    }

    // Print out response
    write_response(fd, version, output);

    free(output);
    output = NULL;

    fclose(file);
}

/*
 * This parses out the meminfo function from the meminfo file
 */
static void meminfo_parser(int fd, int version, char* input)
{
    int callback_loc = 0;
    char* output = malloc(MAX_AMOUNT * sizeof(char));
    output[0] = '\0';
    
    char* callback = strstr(input, "?callback=");  

    if (callback == NULL)
    {
        callback = strstr(input, "&callback=");          
    }

    if (callback != NULL)
    {
        callback += 10;
        while (*callback != '&' && *callback != '\0')    
        {
            output[callback_loc] = *callback;
            callback_loc++;
            callback++;
        }
        output[callback_loc] = '\0';
        
        strcat(output, "(");
    }
    
    // Open the /proc/meminfo file to read
    FILE* file = fopen("/proc/meminfo", "r");
    if (file == NULL)
    {
        return;
    }

    // Start reading
    char str[MAX_AMOUNT];
    int i = 1;

    strcat(output, "{");
    while (fscanf(file, "%s", str) == 1)
    {
        if (i == 3)
        {
            i = 1;
            continue;
        }

        strcat(output, "\"");    
        if (i == 1)
        {
            str[strlen(str) - 1] = '\0';
            strcat(output, str);
            strcat(output, "\": ");        

            if (strstr(str, "HugePages_") != NULL)
            {
                fscanf(file, "%s", str);
                strcat(output, "\"");            
                strcat(output, str);
                strcat(output, "\", ");           
                i = 1;
                continue;
            }
        }

        if (i == 2)
        {
            strcat(output, str);
            strcat(output, "\", ");           
        }

        ++i;
    }
    output[strlen(output) - 2] = '\0';
    strcat(output, "}");

    if (callback != NULL)
    {
        strcat(output, ")");
    }

    // Print out the response
    write_response(fd, version, output);
    free(output);
    output = NULL;

    fclose(file);
}

/*
 * This runs a loop via busy-waiting to test the
 * use of resources.  
 */
static void runloop(int fd, int version)
{
    char* output = malloc(MAX_AMOUNT * sizeof(char));
    output[0] = '\0';

    time_t start;
    start = time(NULL);
    while ((time(NULL) - start) < 15) 
    {
        continue;
    }

    sprintf(output, "<html>\n<body>\n<p>It was a 15-second loop</p>\n</body>\n</html>");

    // Print out the response
    write_response(fd, version, output);
    free(output);
    output = NULL;
}

/*
 * This allocates virtual memory in 64mb incerements using mmap
 */
static void allocanon(int fd, int version)
{
    char *ptr = malloc(67108864);
    char* output = malloc(MAX_AMOUNT * sizeof(char));
    output[0] = '\0';

    char temp[MAX_AMOUNT];
            
    if (ptr == NULL)
    {
        ++numMap;
        ptr = Mmap(NULL, 67108864, PROT_NONE, MAP_SHARED |MAP_ANONYMOUS, -1, 0);
    }
    else
    {
        ptr = memset(ptr, '0', 67108864);
    }

    // Allocate memory for an unallocated block
    int i;
    for (i = 0; i < MAX_AMOUNT; i++)
    {
        if (block[i] == NULL)
        {
            break;
        }
    }
    block[i] = ptr;
    ++totalBlocks;

    strcat(output, "<html>\n<body>\n<p>");
    strcat(output, "Number of blocks: ");
    sprintf(temp, "%d", totalBlocks);
    strcat(output, temp);
    strcat(output, "</p>\n</body>\n</html>");

    // Print out the response
    write_response(fd, version, output);
    free(output);
    output = NULL;
}

/*
 * This deallocates virtual memory in 64mb increments using munmap
 */
static void freeanon(int fd, int version)
{
    char* output = malloc(MAX_AMOUNT * sizeof(char));
    output[0] = '\0';
    
    char temp[MAX_AMOUNT];

    if (totalBlocks == 0)
    {
        write_response(fd, version, "<html>\n<body>\n<p>Number of blocks: 0</p>\n</body>\n</html>");

        return;
    }

    if (numMap == 0)
    {
        free(block[totalBlocks - 1]);
    }
    else
    {
        --numMap;
        Munmap(block[totalBlocks - 1], 67108864);
    }
    --totalBlocks;

    strcat(output, "<html>\n<body>\n<p>");
    strcat(output, "Number of blocks: ");
    sprintf(temp, "%d", totalBlocks);
    strcat(output, temp);
    strcat(output, "</p>\n</body>\n</html>");

    // Print out the response
    write_response(fd, version, output);
    free(output);
    output = NULL;
}