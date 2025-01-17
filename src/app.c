#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "server.h"

Server *server;

void on_users_me(Request *request, Response *response) {
  char *id = request_get_query(request, "id");

  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "id", id);
  cJSON_AddStringToObject(json, "name", "Foo Bar");

  response->body = json;
}

void on_request(Request *request, Response *response) {
  printf("%s %s\n", request->method, request->path);
}

void on_listen(Server *server) {
  printf("Server started on %d\n", server->port);
}

void on_destroy(int code) {
  server_destroy(server);
  printf("Server is shutting down...\n");
  exit(code);
}

int main (int arc, char **argv) {
  int port = atoi(argv[1]);
  int error;

  if ((error = server_init(&server))) {
    puts(server_error(error));
    return error;
  }

  server_middleware(server, on_request);
  server_route(server, "GET", "/users/me", on_users_me);

  signal(SIGINT, on_destroy);
  signal(SIGTERM, on_destroy);

  if ((error = server_listen(server, port, on_listen))) {
    puts(server_error(error));
    return error;
  }

  return 0;
}
