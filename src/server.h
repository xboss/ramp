#ifndef _SERVER_H
#define _SERVER_H

#include <stdint.h>

typedef struct server_s server_t;

server_t *init_server(char *bind_ip, uint16_t port, char *key, char *iv);
void run_server(server_t *server);
void free_server(server_t *server);

#endif  /* SERVER_H */