/******************************************************************************
 * echo_server.c                                                               *
 *                                                                             *
 * Description: This file contains the C source code for an echo server.  The  *
 *              server runs on a hard-coded port and simply write back anything*
 *              sent to it by connected clients.  It does not support          *
 *              concurrent clients.                                            *
 *                                                                             *
 * Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
 *          Wolf Richter <wolf@cs.cmu.edu>                                     *
 *                                                                             *
 *******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "parse.h"
#include <arpa/inet.h>

#define ECHO_PORT 9999
#define BUF_SIZE 8192
#define MAX_CLIENTS 1024

void get_request(int client_sock, ssize_t readret, char *recv_buf);
void handle_head(int client_sock, Request *request, char *response);
void handle_get(int client_sock, Request *request, char *response);

void get_request(int client_sock, ssize_t readret, char *recv_buf)
{
    Request *request = parse(recv_buf, readret, client_sock);
    char response[BUF_SIZE];
    memset(response, 0, BUF_SIZE);
    if (request == NULL)
    {
        // res 400
        strcat(response, "HTTP/1.1");
        strcat(response, " 400 Bad request\r\n\r\n");
        send(client_sock, response, strlen(response), 0);
        log_acc(request, 400, strlen(response));
        return;
    }
    if (strcmp(request->http_version, "HTTP/1.1") != 0)
    {
        // res 505
        strcat(response, "HTTP/1.1");
        strcat(response, " 505 HTTP Version not supported\r\n\r\n");
        send(client_sock, response, strlen(response), 0);
        log_acc(request, 505, strlen(response));
        return;
    }
    else
    {
        if (!strcmp(request->http_method, "HEAD"))
        {
            handle_head(client_sock, request, response);
        }
        else if (!strcmp(request->http_method, "POST"))
        {
            send(client_sock, recv_buf, readret, 0);
            return;
        }
        else if (!strcmp(request->http_method, "GET"))
        {
            handle_get(client_sock, request, response);
        }
        else
        {
            // res 501
            strcat(response, "HTTP/1.1");
            strcat(response, " 501 Not Implemented\r\n\r\n");
            send(client_sock, response, strlen(response), 0);
            log_acc(request, 501, strlen(response));
            return;
        }
    }
    send(client_sock, response, strlen(response), 0);
    free(request->headers);
    free(request);
}

void handle_head(int client_sock, Request *request, char *response)
{
    char path[1024];
    strcpy(path, "static_site");
    if (strcmp(request->http_uri, "/") == 0)
    {
        strcat(path, "/index.html");
    }
    else
        strcat(path, request->http_uri);
    int errno = 2;
    FILE *fd = fopen(path, "rb");
    struct stat buf;
    stat(path, &buf);
    if (fd == NULL)
    {
        strcat(response, "HTTP/1.1");
        strcat(response, " 404 Not Found\r\n\r\n");
        log_err("error", strerror(errno));
        return;
    }
    int file_size = buf.st_size;
    char file_size_header[1024];
    sprintf(file_size_header, "Content-Lenght: %d\r\n", file_size);
    strcat(response, "HTTP/1.1");
    strcat(response, " 200 OK\r\n");
    strcat(response, "Content-Type: text/html\r\n");
    strcat(response, file_size_header);
    log_acc(request, 200, strlen(response));
}

void handle_get(int client_sock, Request *request, char *response)
{
    char path[1024];
    strcpy(path, "static_site");
    if (strcmp(request->http_uri, "/") == 0)
    {
        strcat(path, "/index.html");
    }
    else
        strcat(path, request->http_uri);
    int errno = 2;
    FILE *fd = fopen(path, "rb");
    struct stat buf;
    stat(path, &buf);
    if (fd == NULL)
    {
        strcat(response, "HTTP/1.1");
        strcat(response, " 404 Not Found\r\n\r\n");
        log_err("error", strerror(errno));
        return;
    }
    int file_size = buf.st_size;
    char file_size_header[1024];
    sprintf(file_size_header, "Content-Lenght: %d\r\n", file_size);
    char file[BUF_SIZE];
    fread(file, 1, file_size, fd);
    strcat(response, "HTTP/1.1");
    strcat(response, " 200 OK\r\n");
    strcat(response, "Content-Type: text/html\r\n");
    strcat(response, file_size_header);
    strcat(response, "\r\n");
    strcat(response, file);
    log_acc(request, 200, strlen(response));
}

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}
struct sockaddr_in addr, cli_addr;
int client_sockets[MAX_CLIENTS];

int main(int argc, char *argv[])
{
    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    char buf[BUF_SIZE];
    fd_set client_set;
    int MAX_fd;
    struct timeval time_out;
    time_out.tv_sec = 30;
    time_out.tv_usec = 0;

    memset(client_sockets, 0, sizeof(client_sockets));
    logger_init();

    fprintf(stdout, "----- Echo Server -----\n");

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        FD_ZERO(&client_set);      //
        FD_SET(sock, &client_set); //
        MAX_fd = sock;             //

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int sock_fd = client_sockets[i];
            if (sock_fd > 0)
                FD_SET(sock_fd, &client_set);
            if (sock_fd > MAX_fd)
                MAX_fd = sock_fd;
        }

        int ret = select(MAX_fd + 1, &client_set, NULL, NULL, &time_out);

        if (ret < 0)
        {
            printf("select failed\n");
            break;
        }
        if (ret == 0)
        {
            printf("timeout\n");
            continue;
        }

        cli_size = sizeof(cli_addr);

        if (FD_ISSET(sock, &client_set))
        {
            if ((client_sock = accept(sock, (struct sockaddr *)&cli_addr, &cli_size)) == -1)
            {
                // close(sock);
                // fprintf(stderr, "Error accepting connection.\n");
                exit(EXIT_FAILURE);
            }
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_sockets[i] == 0)
                {
                    client_sockets[i] = client_sock;
                    break;
                }
            }
        }

        readret = 0;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int sock_fd = client_sockets[i];
            if (FD_ISSET(sock_fd, &client_set))
            {
                if ((readret = recv(sock_fd, buf, BUF_SIZE, 0)) >= 1)
                {
                    char *div = "\r\n\r\n";
                    char *haystack = buf;
                    char *pos = strstr(haystack, div);
                    char a_request[BUF_SIZE];
                    while (pos != NULL)
                    {
                        strncpy(a_request, haystack, pos - haystack + strlen(div));
                        get_request(sock_fd, strlen(a_request), a_request);
                        haystack = pos + strlen(div);
                        pos = strstr(haystack, div);
                    }
                }
                else
                {
                    int sd = client_sockets[i];
                    client_sockets[i] = 0;
                    close_socket(sd);
                }
                memset(buf, 0, BUF_SIZE);
            }
        }
    }

    close_socket(sock);

    return EXIT_SUCCESS;
}
