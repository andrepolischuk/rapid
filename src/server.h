#include <cjson/cJSON.h>

#define SERVER_BUFFER_SIZE 1024
#define SERVER_MAX_HEADER_NAME_LENGTH 1024
#define SERVER_MAX_HEADER_VALUE_LENGTH 1024
#define SERVER_MAX_HEADERS_SIZE 100
#define SERVER_MAX_ROUTES_SIZE 100

typedef struct {
  char *name;
  char *value;
} Record;

typedef struct {
  char *method;
  char *path;
  char *version;
  Record query[SERVER_MAX_HEADERS_SIZE];
  int query_size;
  Record headers[SERVER_MAX_HEADERS_SIZE];
  int headers_size;
  cJSON *body;
} Request;

typedef enum {
  OK = 200,
  NOT_FOUND = 404
} ResponseStatus;

typedef struct {
  ResponseStatus status;
  Record headers[SERVER_MAX_HEADERS_SIZE];
  int headers_size;
  cJSON *body;
} Response;

typedef void (*Middleware)(Request*, Response*);

typedef struct {
  char *method;
  char *path;
  Middleware middleware;
} ServerRoute;

typedef struct {
  int fd;
  int port;
  int routes_size;
  ServerRoute routes[SERVER_MAX_ROUTES_SIZE];
} Server;

typedef enum {
  ERR_SERVER_UNKNOWN = -10,
  ERR_SERVER_ALLOC = -11,
  ERR_SERVER_SOCKET = -12,
  ERR_SERVER_BIND = -13,
  ERR_SERVER_LISTEN = -14,
} ServerError;

typedef void (*Callback)(Server*);

char *request_get_query(Request *request, char *name);

char *request_get_header(Request *request, char *name);

void request_add_header(Request *request, char *name, char *value);

char *response_get_header(Response *response, char *name);

void response_add_header(Response *response, char *name, char *value);

const char *server_error(int code);

int server_init(Server **server);

int server_listen(Server *server, int port, Callback callback);

void server_route(Server *server, char *method, char *path, Middleware middleware);

void server_middleware(Server *server, Middleware middleware);

void server_destroy(Server *server);
