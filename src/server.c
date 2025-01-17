#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cjson/cJSON.h>
#include "server.h"

typedef struct {
  ResponseStatus code;
  const char *value;
} ResponseStatusEntry;

static ResponseStatusEntry STATUSES[] = {
  {OK, "200 OK"},
  {NOT_FOUND, "404 Not Found"},
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
  ServerError code;
  const char *value;
} ServerErrorEntry;

static ServerErrorEntry ERRORS[] = {
  {ERR_SERVER_UNKNOWN, "Unknown error"},
  {ERR_SERVER_ALLOC, "Server allocation failed"},
  {ERR_SERVER_SOCKET, "Server socket failed"},
  {ERR_SERVER_BIND, "Server binding failed"},
  {ERR_SERVER_LISTEN, "Server listening failed"},
};

const char *server_error(int code) {
  for (int i = 0; i < sizeof(ERRORS) / sizeof(ERRORS[0]); i++) {
    if (ERRORS[i].code == code) {
      return ERRORS[i].value;
    }
  }

  return ERRORS[0].value;
}

char *request_get_query(Request *request, char *name) {
  for (int i = 0; i < request->query_size; i++) {
    if (strcmp(request->query[i].name, name) == 0) {
      return request->query[i].value;
    }
  }

  return NULL;
}

static void request_add_query(Request *request, char *name, char *value) {
  int n = request->query_size++;
  Record *query = &request->query[n];

  query->name = name;
  query->value = value;
}

char *request_get_header(Request *request, char *name) {
  for (int i = 0; i < request->headers_size; i++) {
    if (strcmp(request->headers[i].name, name) == 0) {
      return request->headers[i].value;
    }
  }

  return NULL;
}

void request_add_header(Request *request, char *name, char *value) {
  int n = request->headers_size++;
  Record *header = &request->headers[n];

  header->name = name;
  header->value = value;
}

char *response_get_header(Response *response, char *name) {
  for (int i = 0; i < response->headers_size; i++) {
    if (strcmp(response->headers[i].name, name) == 0) {
      return response->headers[i].value;
    }
  }

  return NULL;
}

void response_add_header(Response *response, char *name, char *value) {
  int n = response->headers_size++;
  Record *header = &response->headers[n];

  header->name = name;
  header->value = value;
}

static void parse_request(const char *request_string, Request *request) {
  char headers[SERVER_BUFFER_SIZE];
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
        request_add_query(request, name, value);
      }

      pair = strtok(NULL, "&");
    }
  }

  char *header = strtok(headers, "\r\n");

  while (header != NULL) {
    char name[SERVER_MAX_HEADER_NAME_LENGTH];
    char value[SERVER_MAX_HEADER_VALUE_LENGTH];
    char *colon = strchr(header, ':');

    if (colon) {
      int name_size = colon - header;
      int value_size = strlen(colon + 2);

      strncpy(name, header, name_size);
      name[name_size] = '\0';

      strncpy(value, colon + 2, value_size);
      value[value_size] = '\0';

      request_add_header(request, name, value);
    }

    header = strtok(NULL, "\r\n");
  }

  if (headers_end != NULL) {
    body = headers_end + 4;
    request->body = cJSON_Parse(body);
  }
}

static void serialize_response(char *response_string, Response *response) {
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

  for (int i = 0; i < response->headers_size; i++) {
    response_size += sprintf(response_string + response_size, "%s: %s\r\n", response->headers[i].name, response->headers[i].value);
  }

  response_size += sprintf(response_string + response_size, "Content-Length: %d\r\n", body_size);
  response_size += sprintf(response_string + response_size, "\r\n");

  if (body != NULL) {
    response_size += sprintf(response_string + response_size, "%s", body);
  }
}

static int match_route(char *route_method, char *route_path, char *request_method, char *request_path) {
  return (strcmp(route_method, request_method) == 0 || strcmp(route_method, "*") == 0) &&
    (strcmp(route_path, request_path) == 0 || strcmp(route_path, "*") == 0);
}

static void handle_client(Server *server, int socket) {
  char request_string[SERVER_BUFFER_SIZE];
  char response_string[SERVER_BUFFER_SIZE];

  int bytes_read = read(socket, request_string, SERVER_BUFFER_SIZE);

  if (bytes_read < 0) {
      close(socket);
      return;
  }

  request_string[bytes_read] = '\0';

  Request request = {
    .headers_size = 0,
    .query_size = 0,
    .body = NULL
  };

  Response response = {
    .status = 0,
    .headers_size = 0,
    .body = NULL
  };

  parse_request(request_string, &request);

  int matched_route = 0;

  for (int i = 0; i < server->routes_size; i++) {
    ServerRoute *route = &server->routes[i];

    if (match_route(route->method, route->path, request.method, request.path)) {
      if (strcmp(route->path, "*") != 0) {
        matched_route = 1;

        if (!response.status) {
          response.status = OK;
          response_add_header(&response, "Content-Type", "application/json");
        }
      }

      route->middleware(&request, &response);
    }
  }

  if (!matched_route && !response.status) {
    response.status = NOT_FOUND;
  }

  serialize_response(response_string, &response);

  send(socket, response_string, strlen(response_string), 0);
  close(socket);

  if (request.body != NULL) {
    cJSON_Delete(request.body);
  }

  if (response.body != NULL) {
    cJSON_Delete(response.body);
  }
}

int server_init(Server **server) {
  *server = (Server *) malloc(sizeof(Server));

  if (*server == NULL) {
    return ERR_SERVER_ALLOC;
  }

  (*server)->fd = socket(AF_INET, SOCK_STREAM, 0);

  if ((*server)->fd == 0) {
    return ERR_SERVER_SOCKET;
  }

  return 0;
}

void server_route(Server *server, char *method, char *path, Middleware middleware) {
  int n = server->routes_size++;
  ServerRoute *route = &server->routes[n];

  route->method = method;
  route->path = path;
  route->middleware = middleware;
}

void server_middleware(Server *server, Middleware middleware) {
  server_route(server, "*", "*", middleware);
}

int server_listen(Server *server, int port, Callback callback) {
  server->port = port;

  int socket;
  struct sockaddr_in address;
  int address_size = sizeof(address);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(server->port);

  if (bind(server->fd, (struct sockaddr *) &address, (socklen_t) address_size) < 0) {
    return ERR_SERVER_BIND;
  }

  if (listen(server->fd, 3) < 0) {
    return ERR_SERVER_LISTEN;
  }

  callback(server);

  while (1) {
    if ((socket = accept(server->fd, (struct sockaddr *) &address, (socklen_t*) &address_size)) < 0) {
      continue;
    }

    handle_client(server, socket);
  }

  return 0;
}

void server_destroy(Server *server) {
  close(server->fd);
  free(server);
}
