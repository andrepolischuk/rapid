# rapid

Simple HTTP server for web applications

## Install

## Usage

```c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <cjson/cJSON.h>
#include "rapid.h"

rapid_server *server;

void on_request(rapid_request *request, rapid_response *response) {
  printf("%s %s\n", request->method, request->path);
}

void on_user(rapid_request *request, rapid_response *response) {
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "id", "123");
  cJSON_AddStringToObject(json, "name", "Foo Bar");

  response->body = json;
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

  signal(SIGINT, on_destroy);
  signal(SIGTERM, on_destroy);

  if ((error = rapid_listen(server, port, on_listen))) {
    puts(rapid_get_error(error));
    rapid_destroy(server);
    return error;
  }

  return 0;
}
```

## License

MIT
