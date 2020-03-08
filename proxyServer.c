/*Daniel Raz*/

#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>

#define usage "Usage: proxyServer <port> <pool size> <max number of request> <filter>\n"
#define SIZE_BUF 100

/*this for the filter list*/
int size = 1;
char **filter;
/*-----------------------*/

int clientHendler(void *arg);
char *errorResponse(int code);
int create_socket(int);
char *check_input(char *str, int client_sd);
int get_the_filter(char *);
char *talk_to_server(char *host, int port, char *request, int client_sd);

int main(int argc, char **argv)
{

    if (argc != 5)
    {
        printf("%s", usage);
        exit(EXIT_FAILURE);
    }

    /*check if port,pool size and number of request are numbers*/
    for (int i = 1; i <= 3; i++)
    {
        int x = atoi(argv[i]);
        if (x <= 0)
        {
            printf("%s", usage);
            exit(EXIT_FAILURE);
        }
    }
    /* this inputs are definitly numbers */
    int proxyPort = atoi(argv[1]),
        poolSize = atoi(argv[2]),
        maxReq = atoi(argv[3]);

    /*create main socket*/
    int sockfd = create_socket(proxyPort);
    if (sockfd < 0)
        exit(1);

    threadpool *thpool = create_threadpool(poolSize);
    if (thpool == NULL)
    {
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /*try load the lisst from the filter file*/
    if (get_the_filter(argv[4]) < 0)
    {
        destroy_threadpool(thpool);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int fdis[maxReq];
    for (int i = 0; i < maxReq; i++)
    {
        fdis[i] = accept(sockfd, NULL, NULL);
        if (fdis[i] < 0)
        {
            perror("error: accept\n");
            continue;
        }
        dispatch(thpool, clientHendler, (void *)&fdis[i]);
    }

    destroy_threadpool(thpool);
    for (int i = 0; i < size; i++)
    {
        free(filter[i]);
    }
    free(filter);

    return 0;
}

/*create main socket for the proxy server*/
int create_socket(int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("error: socket\n");
        return -1;
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("error: bind\n");
        close(sockfd);
        return -1;
    }
    if (listen(sockfd, 5) < 0)
    {
        perror("error: listen\n");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

//make the error response
char *errorResponse(int code)
{
    char *ht = "HTTP/1.0";
    char *server = "Server: webserver/1.0";
    char *type = "Content-Type: text/html";
    char *connection = "Connection: close";
    char *problem, *contentLength, *finalProblem;

    char *errMessege = (char *)malloc(sizeof(char));
    if (errMessege == NULL)
    {
        perror("error: malloc\n");
        return NULL;
    }

    if (code == 400)
    {
        problem = "400 Bad Request";
        contentLength = "Content-Length: 113";
        finalProblem = "Bad Request.";
        errMessege = realloc(errMessege, strlen(ht) + strlen(server) + strlen(type) + strlen(connection) + strlen(problem) + strlen(contentLength) + strlen(finalProblem) + 113);
        if (errMessege == NULL)
        {
            perror("error: realloc\n");
            return NULL;
        }
    }
    else if (code == 403)
    {
        problem = "403 Forbidden";
        contentLength = "Content-Length: 111";
        finalProblem = "Access denied.";
        errMessege = realloc(errMessege, strlen(ht) + strlen(server) + strlen(type) + strlen(connection) + strlen(problem) + strlen(contentLength) + strlen(finalProblem) + 111);
        if (errMessege == NULL)
        {
            perror("error: realloc\n");
            return NULL;
        }
    }
    else if (code == 404)
    {
        problem = "404 Not Found";
        contentLength = "Content-Length: 112";
        finalProblem = "File not found.";
        errMessege = realloc(errMessege, strlen(ht) + strlen(server) + strlen(type) + strlen(connection) + strlen(problem) + strlen(contentLength) + strlen(finalProblem) + 112);
        if (errMessege == NULL)
        {
            perror("error: realloc\n");
            return NULL;
        }
    }
    else if (code == 500)
    {
        problem = "500 Internal Server Error";
        contentLength = "Content-Length: 144";
        finalProblem = "Some server side error.";
        errMessege = realloc(errMessege, strlen(ht) + strlen(server) + strlen(type) + strlen(connection) + strlen(problem) + strlen(contentLength) + strlen(finalProblem) + 144);
        if (errMessege == NULL)
        {
            perror("error: realloc\n");
            return NULL;
        }
    }
    else if (code == 501)
    {
        problem = "501 Not supported";
        contentLength = "Content-Length: 129";
        finalProblem = "Method is not supported.";
        errMessege = realloc(errMessege, strlen(ht) + strlen(server) + strlen(type) + strlen(connection) + strlen(problem) + strlen(contentLength) + strlen(finalProblem) + 129);
        if (errMessege == NULL)
        {
            perror("error: realloc\n");
            return NULL;
        }
    }
    else
        return NULL;

    sprintf(errMessege, "%s %s\n%s\n%s\n%s\n%s\n\n<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n<BODY><H4>%s</H4>\n%s\n</BODY></HTML>", ht, problem, server, type, contentLength, connection, problem, problem, finalProblem);
    return errMessege;
}

/*this function for handle the client request*/
int clientHendler(void *arg)
{
    if (arg == NULL)
        return -1;

    int *pointer_for_cast = (int *)arg;
    int sd = *pointer_for_cast;

    char *request = (char *)malloc(sizeof(char));
    if (request == NULL)
    {
        perror("error: malloc\n");
        return -1;
    }

    char buffer[SIZE_BUF];
    int r, x = 1;
    buffer[0] = '\0';
    request[0] = '\0';
    while ((r = read(sd, buffer, SIZE_BUF)) != 0) //read the request from the client to buffer[].
    {
        if (r < 0)
        {
            close(sd);
            free(request);
            return -1;
        }
        if (r == 0)
            break;

        x += r;
        request = realloc(request, x);
        if (request == NULL)
        {
            perror("error: realloc\n");
            return -1;
        }
        buffer[r] = '\0';
        strncat(request, buffer, r);
        if (strstr(request, "\r\n\r\n") != NULL)
            break;
    }

    /*check if the request valid and return return response*/
    char *response = check_input(request, sd);
    if (strcmp(response, "ok") != 0)
    {
        int w = write(sd, response, strlen(response) + 1);
        if (w < 0)
        {
            perror("error: write\n");
            close(sd);
            free(request);
            free(response);
            return -1;
        }
        free(response);
    }
    close(sd);
    free(request);
    return 0;
}

/*check the request from the client and make the response to him or the request for the server*/
char *check_input(char *str, int client_sd)
{
    if (str == NULL)
        return NULL;

    char *real_req = (char *)malloc(sizeof(char) * (strlen(str) + 1));
    if (real_req == NULL)
    {
        perror("error: malloc\n");
        return errorResponse(400);
    }
    strncpy(real_req, str, strlen(str));
    real_req[strlen(str)] = '\0';

    char *first_line = strtok(str, "\r\n");
    if (first_line == NULL)
    {
        free(real_req);
        return errorResponse(400);
    }
    char *request = (char *)malloc(sizeof(char) * (strlen(str) + 1));
    if (request == NULL)
    {
        perror("error: malloc\n");
        free(real_req);
        return NULL;
    }
    strncpy(request, first_line, strlen(first_line));
    request[strlen(str)] = '\0';

    char *method = NULL, *path = NULL, *version = NULL;
    method = strtok(first_line, " ");
    path = strtok(NULL, " ");
    version = strtok(NULL, " ");
    int port;

    /*check if one of this tokens not exist*/
    if (method == NULL || path == NULL || version == NULL)
    {
        free(request);
        free(real_req);
        return errorResponse(400);
    }

    /*we support onle GET method*/
    if (strcmp(method, "GET") != 0)
    {
        free(request);
        free(real_req);
        return errorResponse(501);
    }

    /*we support only this version of HTTP protcole*/
    if (strcmp(version, "HTTP/1.0") != 0 && strcmp(version, "HTTP/1.1") != 0)
    {
        free(request);
        free(real_req);
        return errorResponse(501);
    }

    /*look for the host header*/
    char *host = strstr(real_req, "Host:");
    if (host == NULL)
    {
        free(request);
        free(real_req);
        return errorResponse(400);
    }
    host += 5;
    while (host[0] == ' ')
        host++;

    host = strtok(host, "\r");
    char *look_for_port = strstr(host, ":");
    if (look_for_port != NULL)
    {
        look_for_port++;
        port = atoi(look_for_port);
        if (port <= 0)
        {
            free(request);
            free(real_req);
            return errorResponse(400);
        }
        host = strtok(host, ":");
    }
    else
    {
        port = 80;
        host = strtok(host, "/");
    }

    /*check if the host is not forbidden*/
    for (int i = 0; i < size - 1; i++)
    {
        char *temp = strtok(filter[i], "\r");
        if (temp != NULL)
            if (strcmp(host, temp) == 0)
            {
                free(request);
                free(real_req);
                return errorResponse(403);
            }
    }

    /*so far the request is have header host, GET method ,path and version{HTTP/1.0 || HTTP/1.1} */
    char *response = (char *)malloc(sizeof(char) * (strlen(request) + strlen(host) + 36));
    if (response == NULL)
    {
        perror("error: malloc\n");
        free(real_req);
        free(request);
        return errorResponse(500);
    }
    sprintf(response, "%s\r\nConnection: close\r\nHost: %s\r\n\r\n", request, host);

    char *answer = talk_to_server(host, port, response, client_sd);

    free(request);
    free(response);
    free(real_req);
    return answer;
}

/*make list of the forbidden host*/
int get_the_filter(char *path)
{
    size_t len = 0;
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        perror("error: fopen\n");
        return -1;
    }
    filter = (char **)malloc(sizeof(char *));
    if (filter == NULL)
    {
        perror("error: malloc\n");
        return -1;
    }

    /*read line by line from the filter file */
    int i = 0;
    filter[i] = NULL;
    for (; getline(&filter[i], &len, fp) != -1; i++)
    {
        size++;
        filter = realloc(filter, sizeof(char *) * size);
        if (filter == NULL)
        {
            perror("error: malloc\n");
            return -1;
        }
        filter[size - 1] = NULL;
    }
    fclose(fp);
    return 0;
}

/*make communication with the server and return the response*/
char *talk_to_server(char *host, int port, char *request, int client_sd)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;

    //create the socket.
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        perror("error: socket\n");
        return errorResponse(500);
    }

    // initlaize the struct with the ip and port.
    server = gethostbyname(host);
    if (server == NULL)
    {
        herror("gethostbyname:");
        return errorResponse(404);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    //try connect to the socket.
    int rc = connect(sd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (rc < 0)
    {
        perror("error: connect\n");
        return errorResponse(500);
    }

    //write the request for the server.
    int w = write(sd, request, strlen(request) + 1);
    if (w < 0)
    {
        perror("error: write\n");
        close(sd);
        return errorResponse(500);
    }

    unsigned char buffer[SIZE_BUF];
    buffer[0] = '\0';
    int r;
    while (1) //read response from the server and return to the client. 
    {
        r = read(sd, buffer, SIZE_BUF);
        if (r < 0)
        {
            close(sd);
            perror("error: read\n");
            return errorResponse(500);
        }
        if (r == 0)
            break;

        size_t w = send(client_sd, buffer, r, MSG_NOSIGNAL);
        if (w < 0)
        {
            close(sd);
            perror("error: write\n");
            return errorResponse(500);
        }
    }
    close(sd);
    return "ok";
}
