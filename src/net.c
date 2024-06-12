#include "net.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>

const char *HTTP_OK =
    "HTTP/1.1 200 OK\r\n"
    "connection: close\r\n"
    "Connection: close\r\n"
    "Connection: Close\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %lu\r\n"
    "\r\n"
    "%s";
const char *HTTP_NOT_FOUND =
    "HTTP/1.1 404 Not Found\r\n"
    "connection: close\r\n"
    "Connection: close\r\n"
    "Connection: Close\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %lu\r\n"
    "\r\n"
    "%s";
const char *HTTP_NO_CONTENT =
    "HTTP/1.1 204 No Content\r\n"
    "connection: close\r\n"
    "Connection: close\r\n"
    "Connection: Close\r\n"
    "Content-Length: 0\r\n"
    "\r\n";
const char *HTTP_INTERNAL_ERROR =
    "HTTP/1.1 500 INTERNAL ERROR\r\n"
    "connection: close\r\n"
    "Connection: close\r\n"
    "Connection: Close\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %lu\r\n"
    "\r\n"
    "%s";

ssize_t r_size(const char *http, char *content_type, char *data)
{
    ssize_t size = strlen(http);
    if (content_type != NULL)
    {
        size += strlen(content_type);
    }
    if (data != NULL)
    {
        size += strlen(data);
    }
    size += 50;
    return size;
}
void write_response(char **res, const char *http_header, char *content_type, char *body)
{
    if (strcmp(http_header, HTTP_NO_CONTENT) == 0)
    {
        ssize_t response_size = strlen(http_header) + 1;
        *res = (char *)malloc(response_size * sizeof(char));
        snprintf(*res, response_size, "%s", http_header);
        return;
    }
    ssize_t response_size = r_size(http_header, content_type, body);
    *res = (char *)malloc(response_size * sizeof(char));
    snprintf(*res, response_size - 1, http_header, content_type, strlen(body), body);
}
int send_bytes(int socket, char *response)
{
    size_t total_sent = 0;
    size_t response_length = strlen(response);
    while (total_sent < response_length)
    {
        ssize_t sent = send(socket, response + total_sent, response_length - total_sent, 0);
        if (sent == -1)
        {
            return -1;
        }
        total_sent += sent;
    }
    return 0;
}
int send_internal_error(int socket, char *error)
{
    char *res = NULL;
    char content_type[] = "text/plain; charset=UTF-8";
    write_response(&res, HTTP_INTERNAL_ERROR, content_type, error);
    send_bytes(socket, res);
    free(res);
    return 0;
}
int parse_body(Request *req, char *data)
{
    char *stmt_key = strstr(data, "\"stmt\":");
    if (stmt_key != NULL)
    {
        stmt_key += 7;
        while (*stmt_key == ' ')
            stmt_key++;
        if (*stmt_key == '\"')
            stmt_key++;
        char *stmt_end = strstr(stmt_key, "\"");
        if (stmt_end != NULL)
        {
            char tmp = *stmt_end;
            *stmt_end = '\0';
            if (strlen(stmt_key) >= MAX_CHARACTERS)
            {
                perror("stmt overflow");
                *stmt_end = tmp;
                return 1;
            }
            strncpy(req->stmt, stmt_key, MAX_CHARACTERS - 1);
            req->stmt[MAX_CHARACTERS - 1] = '\0';
            *stmt_end = tmp;
        }
    }
    else
    {
        if (strncmp(req->path, "/sql", strlen("/sql")) == 0)
        {
            printf("request body missing key 'stmt'\n");
            return 1;
        }
        else
        {

            return 0;
        }
    }
    return 0;
}
int extract_target(Request *request, char *data)
{
    char *line_end = strstr(data, "\r\n");
    if (line_end == NULL)
    {
        return 1;
    }
    size_t line_length = line_end - data;
    char *line = (char *)malloc(line_length + 1);
    if (line == NULL)
    {
        perror("malloc");
        return 1;
    }
    strncpy(line, data, line_length);
    line[line_length] = '\0';
    char *method = strtok(line, " ");
    if (method != NULL)
    {
        strncpy(request->method, method, MAX_CHARACTERS - 1);
        request->method[MAX_CHARACTERS - 1] = '\0';
    }

    char *path = strtok(NULL, " ");
    if (path != NULL)
    {
        strncpy(request->path, path, MAX_CHARACTERS - 1);
        request->path[MAX_CHARACTERS - 1] = '\0';
    }
    free(line);
    return 0;
}
int read_bytes(int socket, char **data)
{
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    ssize_t total_bytes_received = 0;
    ssize_t content_length = -1;
    char *header_end = NULL;

    while (1)
    {
        ssize_t bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0)
            break;
        
        if (bytes_received == 0)
            break;
        
        *data = realloc(*data, total_bytes_received + bytes_received + 1);
        if (*data == NULL)
        {
            perror("malloc");
            return -1;
        }
        buffer[bytes_received] = '\0';
        memcpy(*data + total_bytes_received, buffer, bytes_received);
        total_bytes_received += bytes_received;
        (*data)[total_bytes_received] = '\0';

        if (header_end == NULL)
        {
            header_end = strstr(*data, "\r\n\r\n");
            if (header_end != NULL)
            {
                header_end += 4;
                char *content_length_str = strstr(*data, "Content-Length: ");
                if (content_length_str == NULL)
                {
                    content_length_str = strstr(*data, "Content-length: ");
                }
                if (content_length_str == NULL)
                {
                    content_length_str = strstr(*data, "content-length: ");
                }
                if (content_length_str != NULL)
                {
                    sscanf(content_length_str, "%*s %zd", &content_length);
                    content_length += header_end - *data;
                }
                else
                {
                    content_length = total_bytes_received;
                }
            }
        }

        if (header_end != NULL && total_bytes_received >= content_length)
        {
            break;
        }
    }

    return total_bytes_received;
}

int read_request(int client_socket, Request *request)
{
    // read_bytes(client_socket, &request->data);
    if (read_bytes(client_socket, &request->data) != 0 || request->data == NULL || extract_target(request, request->data) != 0 || parse_body(request, request->data) != 0)
    {
        return 1;
    }
    return 0;
}

int net_init_server(int port)
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("error initializing socket");
        return 1;
    }
    int srv_flag = 1;
    setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &srv_flag, sizeof(srv_flag));
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &srv_flag, sizeof(srv_flag));
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("error binding to addr");
        return 1;
    }
    if (listen(server_socket, BACK_LOG) == -1)
    {
        perror("error listening");
        return 1;
    }
    return server_socket;
}
int net_init_non_blocking_server(int port)
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("error initializing socket");
        return 1;
    }
    int srv_flag = 1;
    // setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &srv_flag, sizeof(srv_flag));
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &srv_flag, sizeof(srv_flag));
#ifdef SO_REUSEPORT
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &srv_flag, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEPORT) failed");
#endif
    int flags = fcntl(server_socket, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        return 1;
    }

    if (fcntl(server_socket, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL O_NONBLOCK");
        return 1;
    }
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("error binding to addr");
        return 1;
    }
    if (listen(server_socket, BACK_LOG) == -1)
    {
        perror("error listening");
        return 1;
    }
    return server_socket;
}
int net_connect(char *host_name, int port)
{
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    struct hostent *host;
    host = gethostbyname(host_name);
    if (host == NULL)
    {
        perror("gethostbyname");
        return -1;
    }
    memcpy(&target_addr.sin_addr, host->h_addr_list[0], host->h_length);
    int target_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (target_socket < 0)
    {
        printf("error creating target socket\n");
        return -1;
    }
    int flag = 1;
    setsockopt(target_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    setsockopt(target_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    if (connect(target_socket, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0)
    {
        perror("proxy connect");
        shutdown(target_socket, SHUT_WR);
        close(target_socket);
        return -1;
    }
    return target_socket;
}
int set_non_blocking(int socket)
{
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        return 1;
    }

    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL O_NONBLOCK");
        return 1;
    }
    return 0;
}
int parse_request(Request *req, char *buffer)
{
    extract_target(req, buffer);
    return 0;
}