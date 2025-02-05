#include <cjson/cJSON.h>

#define RAPID_BUFFER_SIZE 1024
#define RAPID_MAX_HEADER_NAME_LENGTH 1024
#define RAPID_MAX_HEADER_VALUE_LENGTH 1024
#define RAPID_MAX_HEADERS_SIZE 100
#define RAPID_MAX_ROUTES_SIZE 100

typedef struct {
  char *name;
  char *value;
} rapid_record;

typedef struct {
  char *method;
  char *path;
  char *version;
  long long time;
  unsigned long thread_id;
  rapid_record query[RAPID_MAX_HEADERS_SIZE];
  int query_size;
  rapid_record headers[RAPID_MAX_HEADERS_SIZE];
  int headers_size;
  cJSON *body;
} rapid_request;

typedef enum {
  OK = 200,
  MOVED_PERMANENTLY = 301,
  FOUND = 302,
  BAD_REQUEST = 400,
  UNAUTHORIZED = 401,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  INTERNAL_SERVER_ERROR = 500,
} rapid_response_status;

typedef struct {
  rapid_response_status status;
  long long time;
  rapid_record headers[RAPID_MAX_HEADERS_SIZE];
  int headers_size;
  int error_code;
  char *redirect;
  cJSON *body;
} rapid_response;

typedef void (*rapid_middleware)(rapid_request*, rapid_response*);

typedef struct {
  char *method;
  char *path;
  rapid_middleware middleware;
} rapid_route;

typedef struct {
  int socket_fd;
  int port;
  int routes_size;
  rapid_route routes[RAPID_MAX_ROUTES_SIZE];
} rapid_server;

typedef struct {
  int socket_fd;
  rapid_server *server;
} rapid_connection;

typedef enum {
  ERR_RAPID_UNKNOWN = -10,
  ERR_RAPID_ALLOC = -11,
  ERR_RAPID_SOCKET = -12,
  ERR_RAPID_BIND = -13,
  ERR_RAPID_LISTEN = -14,
} rapid_error;

typedef void (*rapid_callback)(rapid_server*);

const char *rapid_get_request_query(rapid_request *request, char *name);

const char *rapid_get_request_header(rapid_request *request, char *name);

void rapid_add_request_header(rapid_request *request, char *name, char *value);

const char *rapid_get_response_header(rapid_response *response, char *name);

void rapid_add_response_header(rapid_response *response, char *name, char *value);

const char *rapid_get_error(int code);

int rapid_init(rapid_server **server);

int rapid_listen(rapid_server *server, int port, rapid_callback callback);

void rapid_use_route(rapid_server *server, char *method, char *path, rapid_middleware middleware);

void rapid_use_middleware(rapid_server *server, rapid_middleware middleware);

void rapid_destroy(rapid_server *server);
