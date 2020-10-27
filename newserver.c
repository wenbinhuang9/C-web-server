

#include <stdio.h>

#include <stdlib.h>
#include <stdint.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>

#include <pthread.h>
#include <sys/wait.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: ben server/0.0.1\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

void acceptRequest(void *);

void cat(int, FILE *);

void error_die(const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void sendStaticFiles(int, const char *);


// accept request and forward request to get response 
void parseMethodAndURL(int client, char* buf, char* method, char* url, int methodSize, int urlSize) {
    size_t numchars = get_line(client, buf, 1024);
    printf("buf is %s\n", buf);
    size_t i = 0;
    size_t j = 0;
    while (!ISspace(buf[i]) && (i < methodSize - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j=i;
    method[i] = '\0';

    i = 0;
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    while (!ISspace(buf[j]) && (i < urlSize - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';
}
void acceptRequest(void *arg)
{
    int client = (intptr_t)arg;
    char buf[1024];
    size_t numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;

    parseMethodAndURL(client, buf, method, url, 255, 255);

    sprintf(path, ".%s", url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

    printf("method:%s, path %s\n",method,  path);

    if (strcasecmp(method, "POST") == 0 || strcasecmp(method, "DELETE") == 0 ) {
        // method not supported
        char errorCode[] = "501";
        char errorMsg[] = "Method not implemented";
        char msg[] = "501 method not implemented";
        responseError(client, errorCode, errorMsg, msg);
        return;       
    }

    sendStaticFiles(client, path);

    close(client);
}


void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}


void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}


int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

void headers(int client, const char *filename)
{
    char* response = "HTTP/1.0 200 OK\r\nServer: ben server/0.0.1\r\nContent-Type: text/html\r\n\r\n";

    send(client, response, strlen(response), 0);
}


void responseError(int client, char *errorCode, char *errorMsg,   char *msg)
{
    char* responseTemplate = "HTTP/1.0 %s %s\r\n"
                    "Server: ben server/0.0.1\r\n"
                    "Content-Type: text/html\r\n"
                    "<HTML><TITLE>%s</TITLE>\r\n"
                    "<BODY> %s </BODY></HTML>\r\n";

    char response[1024]; 
    sprintf(response, responseTemplate, errorCode, errorMsg, errorMsg, msg);
    
    send(client, response, strlen(response), 0);
}
//get static files 
void sendStaicFiles(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL) 
        responseError(client, "404", "Not Found", "404 Not Found"); 
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}


int startServer(u_short servPort) {
    
    int on = 1;

    int servSocket = 0;
    servSocket = socket(PF_INET, SOCK_STREAM, 0);

    if (servSocket < 0) {
        error_die("open socket fails");
    }

    struct sockaddr_in servAddr;

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(servPort);
    if ((setsockopt(servSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)  
    {  
        error_die("setsockopt failed");
    } 
    if (bind(servSocket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        error_die("bind socket fails");
    }

    if (listen(servSocket, 30) < 0) {
        error_die("listen socket fails");
    }

    return servSocket;
}


void acceptRequestAndResponse(int servSocket) {
    while(1) {
        struct sockaddr_in clientAddr; // Client address
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(servSocket, (struct sockaddr *) &clientAddr, &clientAddrLen);

        if (clientSocket == -1) {
            error_die("accept fails");
        } 
        pthread_t thread;
        int createReuslt = pthread_create(&thread, NULL, (void *)acceptRequest, (void *)(intptr_t)clientSocket);
        if (createReuslt != 0) {
            error_die("pthread creation fails");
        }
    }

    close(servSocket);
}
int main(void)
{
    int serverSock = -1;
    u_short port = 4000;

    //create server socket and start listening. 
    serverSock = startServer(port);

    printf("web server running on port %d\n", port);

    // accept request and deal with each request. 
    acceptRequestAndResponse(serverSock);

    return 0;
}