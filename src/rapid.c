#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cjson/cJSON.h>
#include "rapid.h"

typedef struct {
  rapid_response_status code;
  const char *value;
} rapid_response_status_record;

static const rapid_response_status_record STATUSES[] = {
  {OK, "200 OK"},
  {MOVED_PERMANENTLY, "301 Moved Permanently"},
  {FOUND, "302 Found"},
  {BAD_REQUEST, "400 Bad Request"},
  {UNAUTHORIZED, "401 Unauthorized"},
  {FORBIDDEN, "403 Forbidden"},
  {NOT_FOUND, "404 Not Found"},
  {INTERNAL_SERVER_ERROR, "500 Internal Server Error"},
};

static const char *get_status(int code) {
  for (int i = 0; i < sizeof(STATUSES) / sizeof(STATUSES[0]); i++) {
    if (STATUSES[i].code == code) {
      return STATUSES[i].value;
    }
  }

  return STATUSES[0].value;
}

typedef struct {
  rapid_error code;
  const char *value;
} rapid_error_record;

static const rapid_error_record ERRORS[] = {
  {ERR_RAPID_UNKNOWN, "Unknown error"},
  {ERR_RAPID_ALLOC, "Server allocation failed"},
  {ERR_RAPID_SOCKET, "Server socket failed"},
  {ERR_RAPID_BIND, "Server binding failed"},
  {ERR_RAPID_LISTEN, "Server listening failed"},
};

const char *rapid_get_error(int code) {
  for (int i = 0; i < sizeof(ERRORS) / sizeof(ERRORS[0]); i++) {
    if (ERRORS[i].code == code) {
      return ERRORS[i].value;
    }
  }

  return ERRORS[0].value;
}

const char *rapid_get_request_query(rapid_request *request, char *name) {
  for (int i = 0; i < request->query_size; i++) {
    if (strcmp(request->query[i].name, name) == 0) {
      return request->query[i].value;
    }
  }

  return NULL;
}

static void rapid_add_request_query(rapid_request *request, char *name, char *value) {
  int i = request->query_size++;
  rapid_record *query = &request->query[i];

  query->name = name;
  query->value = value;
}

const char *rapid_get_request_header(rapid_request *request, char *name) {
  for (int i = 0; i < request->headers_size; i++) {
    if (strcmp(request->headers[i].name, name) == 0) {
      return request->headers[i].value;
    }
  }

  return NULL;
}

void rapid_add_request_header(rapid_request *request, char *name, char *value) {
  int i = request->headers_size++;
  rapid_record *header = &request->headers[i];

  header->name = name;
  header->value = value;
}

const char *rapid_get_response_header(rapid_response *response, char *name) {
  for (int i = 0; i < response->headers_size; i++) {
    if (strcmp(response->headers[i].name, name) == 0) {
      return response->headers[i].value;
    }
  }

  return NULL;
}

void rapid_add_response_header(rapid_response *response, char *name, char *value) {
  int i = response->headers_size++;
  rapid_record *header = &response->headers[i];

  header->name = name;
  header->value = value;
}

static void parse_request(const char *request_string, rapid_request *request) {
  char headers[RAPID_BUFFER_SIZE];
  char *body;

  char *headers_start = strstr(request_string, "\r\n");
  char *headers_end = strstr(headers_start, "\r\n\r\n");
  int headers_size = headers_end - headers_start;

  strncpy(headers, headers_start, headers_size);
  headers[headers_size] = '\0';

  char *method = strtok((char *) request_string, " ");
  char *url = strtok(NULL, " ");
  char *version = strtok(NULL, "\r\n");
  request->method = method;
  request->version = version;

  char *path = strtok((char *) url, "?");
  request->path = path;

  char *query = strtok(NULL, "");

  if (query != NULL) {
    char *pair = strtok(query, "&");

    while (pair != NULL) {
      char *name = strtok(pair, "=");
      char *value = strtok(NULL, "");

      if (name != NULL && value != NULL) {
        rapid_add_request_query(request, name, value);
      }

      pair = strtok(NULL, "&");
    }
  }

  char *header = strtok(headers, "\r\n");

  while (header != NULL) {
    char name[RAPID_MAX_HEADER_NAME_LENGTH];
    char value[RAPID_MAX_HEADER_VALUE_LENGTH];
    char *colon = strchr(header, ':');

    if (colon) {
      int name_size = colon - header;
      int value_size = strlen(colon + 2);

      strncpy(name, header, name_size);
      name[name_size] = '\0';

      strncpy(value, colon + 2, value_size);
      value[value_size] = '\0';

      rapid_add_request_header(request, name, value);
    }

    header = strtok(NULL, "\r\n");
  }

  if (headers_end != NULL) {
    body = headers_end + 4;
    request->body = cJSON_Parse(body);
  }
}

static void serialize_response(char *response_string, rapid_response *response) {
  char *body = response->body == NULL
    ? NULL :
    cJSON_PrintUnformatted(response->body);

  int body_size = 0;
  int response_size = 0;

  if (body != NULL) {
    body_size = strlen(body);
    body_size += sprintf(body + body_size, "\r\n");
  }

  response_size += sprintf(response_string + response_size, "HTTP/1.1 %s\r\n", get_status(response->status));
  response_size += sprintf(response_string + response_size, "Content-Length: %d\r\n", body_size);

  for (int i = 0; i < response->headers_size; i++) {
    response_size += sprintf(response_string + response_size, "%s: %s\r\n", response->headers[i].name, response->headers[i].value);
  }

  response_size += sprintf(response_string + response_size, "\r\n");

  if (body != NULL) {
    response_size += sprintf(response_string + response_size, "%s", body);
  }
}

static int match_route(char *route_method, char *route_path, char *request_method, char *request_path) {
  return (strcmp(route_method, request_method) == 0 || strcmp(route_method, "*") == 0) &&
    (strcmp(route_path, request_path) == 0 || strcmp(route_path, "*") == 0);
}

static long long get_current_time() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (ts.tv_sec * 1000000LL) + (ts.tv_nsec / 1000);
}

static void *handle_connection(void *arg) {
  rapid_connection *connection = (rapid_connection *) arg;

  char request_string[RAPID_BUFFER_SIZE];
  char response_string[RAPID_BUFFER_SIZE];

  rapid_request request = {
    .headers_size = 0,
    .query_size = 0,
    .body = NULL
  };

  rapid_response response = {
    .status = 0,
    .headers_size = 0,
    .body = NULL
  };

  request.time = get_current_time();
  request.thread_id = (unsigned long) pthread_self();

  int bytes_read = read(connection->socket_fd, request_string, RAPID_BUFFER_SIZE);

  if (bytes_read < 0) {
      close(connection->socket_fd);
      free(connection);
      return NULL;
  }

  request_string[bytes_read] = '\0';

  parse_request(request_string, &request);

  int matched_route = 0;

  for (int i = 0; i < connection->server->routes_size; i++) {
    rapid_route *route = &connection->server->routes[i];

    if (match_route(route->method, route->path, request.method, request.path)) {
      if (strcmp(route->path, "*") != 0) {
        matched_route = 1;
      }

      route->middleware(&request, &response);

      if (response.redirect) {
        rapid_add_response_header(&response, "Location", response.redirect);
        break;
      }

      if (response.body) {
        rapid_add_response_header(&response, "Content-Type", "application/json");
        break;
      }
    }
  }

  if (!response.status) {
    response.status = response.redirect
      ? FOUND
      : matched_route
      ? OK
      : NOT_FOUND;
  }

  response.time = get_current_time();

  rapid_add_response_header(&response, "X-Powered-By", "rapid");

  char server_time[20];
  sprintf(server_time, "%lld", response.time - request.time);
  rapid_add_response_header(&response, "X-Server-Time", server_time);

  char thread_id[20];
  sprintf(thread_id, "%lu", request.thread_id);
  rapid_add_response_header(&response, "X-Thread-Id", thread_id);

  serialize_response(response_string, &response);

  send(connection->socket_fd, response_string, strlen(response_string), 0);
  close(connection->socket_fd);
  free(connection);

  if (request.body != NULL) {
    cJSON_Delete(request.body);
  }

  if (response.body != NULL) {
    cJSON_Delete(response.body);
  }

  return NULL;
}

int rapid_init(rapid_server **server) {
  *server = (rapid_server *) malloc(sizeof(rapid_server));

  if (*server == NULL) {
    return ERR_RAPID_ALLOC;
  }

  (*server)->socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if ((*server)->socket_fd == 0) {
    return ERR_RAPID_SOCKET;
  }

  return 0;
}

void rapid_use_route(rapid_server *server, char *method, char *path, rapid_middleware middleware) {
  int i = server->routes_size++;
  rapid_route *route = &server->routes[i];

  route->method = method;
  route->path = path;
  route->middleware = middleware;
}

void rapid_use_middleware(rapid_server *server, rapid_middleware middleware) {
  rapid_use_route(server, "*", "*", middleware);
}

int rapid_listen(rapid_server *server, int port, rapid_callback callback) {
  server->port = port;

  int socket_fd;
  struct sockaddr_in address;
  int address_size = sizeof(address);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(server->port);

  if (bind(server->socket_fd, (struct sockaddr *) &address, (socklen_t) address_size) < 0) {
    return ERR_RAPID_BIND;
  }

  if (listen(server->socket_fd, 3) < 0) {
    return ERR_RAPID_LISTEN;
  }

  callback(server);

  while (1) {
    if ((socket_fd = accept(server->socket_fd, (struct sockaddr *) &address, (socklen_t*) &address_size)) < 0) {
      continue;
    }

    rapid_connection *connection = malloc(sizeof(rapid_connection));

    if (connection == NULL) {
      close(socket_fd);
      continue;
    }

    pthread_t thread_id;

    connection->socket_fd = socket_fd;
    connection->server = server;

    if (pthread_create(&thread_id, NULL, handle_connection, connection) != 0) {
      close(socket_fd);
      free(connection);
      continue;
    }

    pthread_detach(thread_id);
  }

  return 0;
}

void rapid_destroy(rapid_server *server) {
  close(server->socket_fd);
  free(server);
}
