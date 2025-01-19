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
  long long time;
  unsigned long thread_id;
  Record query[SERVER_MAX_HEADERS_SIZE];
  int query_size;
  Record headers[SERVER_MAX_HEADERS_SIZE];
  int headers_size;
  cJSON *body;
} Request;

typedef enum {
  OK = 200,
  MOVED_PERMANENTLY = 301,
  FOUND = 302,
  BAD_REQUEST = 400,
  UNAUTHORIZED = 401,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
} ResponseStatus;

typedef struct {
  ResponseStatus status;
  long long time;
  Record headers[SERVER_MAX_HEADERS_SIZE];
  int headers_size;
  char *redirect;
  cJSON *body;
} Response;

typedef void (*ServerMiddleware)(Request*, Response*);

typedef struct {
  char *method;
  char *path;
  ServerMiddleware middleware;
} ServerRoute;

typedef struct {
  int socket_fd;
  int port;
  int routes_size;
  ServerRoute routes[SERVER_MAX_ROUTES_SIZE];
} Server;

typedef struct {
  int socket_fd;
  Server *server;
} ClientContext;

typedef enum {
  ERR_SERVER_UNKNOWN = -10,
  ERR_SERVER_ALLOC = -11,
  ERR_SERVER_SOCKET = -12,
  ERR_SERVER_BIND = -13,
  ERR_SERVER_LISTEN = -14,
} ServerError;

typedef void (*Callback)(Server*);

const char *request_get_query(Request *request, char *name);

const char *request_get_header(Request *request, char *name);

void request_add_header(Request *request, char *name, char *value);

const char *response_get_header(Response *response, char *name);

void response_add_header(Response *response, char *name, char *value);

const char *server_error(int code);

int server_init(Server **server);

int server_listen(Server *server, int port, Callback callback);

void server_route(Server *server, char *method, char *path, ServerMiddleware middleware);

void server_middleware(Server *server, ServerMiddleware middleware);

void server_response_hook(Server *server, ServerMiddleware middleware);

void server_destroy(Server *server);
