#include <ev.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include "net.h"

struct ev_loop *loop;

typedef struct
{
    struct ev_io w;
    char *response;
    Request *req;
} client;

char CONNECTION_CLOSE[18] = "Connection: close";

static void write_cb(struct ev_loop *loop, struct ev_io *w, int revents);
static void read_cb(struct ev_loop *loop, struct ev_io *w, int revents);

int check_connection_close(char *buffer)
{
    for (size_t i = 0; buffer[i] != '\0' && (buffer[i] != '\n' || buffer[i + 1] != '\n'); i++)
    {
        size_t j;
        for (j = 0; j < 17 && CONNECTION_CLOSE[j] == buffer[i + j]; j++)
            ;

        if (j == 17)
            return 1;
    }

    return 0;
}

static void cleanup(client *c)
{
    if (c->req->data != NULL)
        free(c->req->data);
    if (c->req != NULL)
        free(c->req);
    if (c->response != NULL)
        free(c->response);
    if (c->req->data != NULL)
        free(c);
}

static void write_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    client *c = (client *)w;
    if (EV_ERROR & revents)
    {
        perror("got invalid event");
        return;
    }
    char body[] = "{\"ok\": \"Nice! 😎\"}";
    char content_type[] = "application/json; charset=utf-8";
    write_response(&c->response, HTTP_OK, content_type, body);
    send_bytes(c->w.fd, c->response);
    free(c->req->data);
    c->req->data = NULL;

    ev_io_stop(loop, &c->w);
    ev_io_init(&c->w, read_cb, c->w.fd, EV_READ);
    ev_io_start(loop, &c->w);
}
static void worker_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    client *c = (client *)w;
    if (0 != extract_target(c->req, c->req->data))
    {
        ev_io_stop(loop, &c->w);
        ev_io_init(&c->w, read_cb, c->w.fd, EV_READ);
        ev_io_start(loop, &c->w);
        return;
    }
    printf("@%s %s\n", c->req->method, c->req->path);

    ev_io_stop(loop, &c->w);
    ev_io_init(&c->w, write_cb, c->w.fd, EV_WRITE);
    ev_io_start(loop, &c->w);
}

static void read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    client *c = (client *)w;
    c->req = (Request *)malloc(sizeof(Request));
    c->req->data = NULL;
    ssize_t read;
    if (EV_ERROR & revents)
    {
        perror("invalid event received");
        return;
    }
    read = read_bytes(c->w.fd, &c->req->data);
    if (read < 0)
    {
        perror("read error");
        ev_io_stop(loop, &c->w);
        close(c->w.fd);
        cleanup(c);
        return;
    }
    if (read == 0 || check_connection_close(c->req->data) == 1)
    {
        ev_io_stop(loop, &c->w);
        close(c->w.fd);
        cleanup(c);
        printf("peer might be closing\n");
        return;
    }

    ev_io_stop(loop, &c->w);
    ev_io_init(&c->w, worker_cb, c->w.fd, EV_WRITE);
    ev_io_start(loop, &c->w);
}

static void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sd;
    client *c = (client *)malloc(sizeof(client));
    if (c == NULL)
    {
        perror("malloc error");
        return;
    }

    if (EV_ERROR & revents)
    {
        perror("got invalid event");
        return;
    }

    client_sd = accept(watcher->fd, (struct sockaddr *)&client_addr, &client_len);
    set_non_blocking(client_sd);
    if (client_sd < 0)
    {
        perror("accept error");
        cleanup(c);
        return;
    }

    ev_io_init(&c->w, read_cb, client_sd, EV_READ);
    ev_io_start(loop, &c->w);
}

int http_run(int port)
{
    int fd = net_init_non_blocking_server(port);

    printf("server listening on http://localhost:%d\n", port);

    struct ev_loop *loop = ev_default_loop(0);
    struct ev_io io;

    ev_io_init(&io, accept_cb, fd, EV_READ);
    ev_io_start(loop, &io);

    while (1)
        ev_loop(loop, 0);

    return 0;
}