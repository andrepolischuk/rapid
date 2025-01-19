#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "rapid.h"

rapid_server *server;

void on_request(rapid_request *request, rapid_response *response) {
  // aggregate some data before routes
}

void on_response(rapid_request *request, rapid_response *response) {
  int server_time = response->time - request->time;

  printf("%s %s %d %dms\n", request->method, request->path, response->status, server_time);
}

void on_user(rapid_request *request, rapid_response *response) {
  const char *id = rapid_get_request_query(request, "id");

  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "id", id);
  cJSON_AddStringToObject(json, "name", "Foo Bar");

  response->body = json;
}

void on_redirect(rapid_request *request, rapid_response *response) {
  response->redirect = "/user";
}

void on_listen(rapid_server *server) {
  printf("Server started on %d\n", server->port);
}

void on_destroy(int code) {
  rapid_destroy(server);
  printf("Server is shutting down...\n");
  exit(code);
}

int main (int arc, char **argv) {
  int port = atoi(argv[1]);
  int error;

  if ((error = rapid_init(&server))) {
    puts(rapid_get_error(error));
    return error;
  }

  rapid_use_middleware(server, on_request);
  rapid_use_route(server, "GET", "/user", on_user);
  rapid_use_route(server, "GET", "/redirect", on_redirect);
  rapid_use_response_hook(server, on_response);

  signal(SIGINT, on_destroy);
  signal(SIGTERM, on_destroy);

  if ((error = rapid_listen(server, port, on_listen))) {
    puts(rapid_get_error(error));
    return error;
  }

  return 0;
}
