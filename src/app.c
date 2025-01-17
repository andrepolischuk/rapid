#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "server.h"

Server *server;

void on_request(Request *request, Response *response) {
  // aggregate some data before routes
}

void on_response(Request *request, Response *response) {
  int server_time = response->time - request->time;

  printf("%s %s %d %dms\n", request->method, request->path, response->status, server_time);
}

void on_user(Request *request, Response *response) {
  const char *id = request_get_query(request, "id");

  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "id", id);
  cJSON_AddStringToObject(json, "name", "Foo Bar");

  response->body = json;
}

void on_redirect(Request *request, Response *response) {
  response->redirect = "/user";
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
  server_route(server, "GET", "/user", on_user);
  server_route(server, "GET", "/redirect", on_redirect);
  server_response_hook(server, on_response);

  signal(SIGINT, on_destroy);
  signal(SIGTERM, on_destroy);

  if ((error = server_listen(server, port, on_listen))) {
    puts(server_error(error));
    return error;
  }

  return 0;
}
