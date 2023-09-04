#ifndef _CLIENT_H
#define _CLIENT_H

#include <stdint.h>

typedef struct client_s client_t;

client_t *init_client(char *server_ip, uint16_t server_port, char *key, char *iv, char *tuntap_ip, char *tuntap_mask);
void free_client(client_t *cli);
void *client_send_routine(void *ctx);
void *client_recv_routine(void *ctx);

#endif  // CLIENT_H