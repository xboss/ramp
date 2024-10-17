#ifndef _CLIENT_H
#define _CLIENT_H

#include <arpa/inet.h>

struct client_s {
    int tapfd;
    int udpfd;
    struct sockaddr_in server_addr;
    char* key;
    int is_running;
};
typedef struct client_s client_t;

client_t* init_client(char* server_ip, unsigned short server_port, char* key, char* tuntap_ip, char* tuntap_mask);
void free_client(client_t *cli);
void *client_send_routine(void *ctx);
void *client_recv_routine(void *ctx);

#endif  /* CLIENT_H */