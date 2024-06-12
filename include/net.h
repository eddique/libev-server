#ifndef libev_server_net_h

#define MAX_CHARACTERS 32
#define MAX_BUFFER_SIZE 1024
#define BACK_LOG 40

extern const char *HTTP_OK;
extern const char *HTTP_NO_CONTENT;
extern const char *HTTP_NOT_FOUND;
extern const char *HTTP_INTERNAL_ERROR;


typedef struct
{
    char method[MAX_CHARACTERS];
    char path[MAX_CHARACTERS];
    char stmt[MAX_CHARACTERS];
    char *data;
} Request;

int send_bytes(int socket, char *response);
int send_internal_error(int socket, char *error);
int read_bytes(int socket, char **data);
int read_request(int client_socket, Request *request);
int net_init_server(int port);
int net_init_non_blocking_server(int port);
int net_connect(char *host_name, int port);
void write_response(char **res, const char *http_header, char *content_type, char *body);
int set_non_blocking(int socket);
int parse_request(Request *req, char *buffer);
int extract_target(Request *request, char *data);
int parse_body(Request *req, char *data);
#endif