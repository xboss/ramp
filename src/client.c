#include "client.h"

#include <net/ethernet.h>
#include <string.h>
#include <unistd.h>

#include "cipher.h"
#include "common.h"
#include "tap.h"

client_t* init_client(char* server_ip, unsigned short server_port, char* key, char* tuntap_ip, char* tuntap_mask) {
    if (server_ip == NULL || server_port <= 0 || key == NULL || tuntap_ip == NULL || tuntap_mask == NULL) {
        return NULL;
    }

    /* init tap */
    char dev_name[32] = {0};
    int tapfd = tap_open(dev_name, sizeof(dev_name));
    if (tapfd == -1) {
        _LOG_E("open tap error");
        return NULL;
    }
    tap_setup(dev_name, tuntap_ip, tuntap_mask);
    _LOG("open tap ok %d %s", tapfd, dev_name);

    /* init udp */
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) {
        close(tapfd);
        _LOG_E("udp socket error %s", strerror(errno));
        return NULL;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        _LOG_E("server ip error %s", strerror(errno));
        close(tapfd);
        close(udpfd);
        return NULL;
    }
    client_t* _ALLOC(cli, client_t*, sizeof(client_t));
    memset(cli, 0, sizeof(client_t));
    cli->tapfd = tapfd;
    cli->udpfd = udpfd;
    cli->server_addr = server_addr;
    cli->key = key;
    return cli;
}

void free_client(client_t* cli) {
    if (!cli) return;
    cli->is_running = 0;
    if (cli->tapfd) {
        close(cli->tapfd);
        cli->tapfd = 0;
    }
    if (cli->udpfd) {
        close(cli->udpfd);
        cli->udpfd = 0;
    }
    _FREE_IF(cli);
}

void* client_send_routine(void* ctx) {
    client_t* cli = (client_t*)ctx;
    char data[ETHER_MAX_LEN];
    int rlen = 0;
    int wlen = 0;
    char* cipher_txt = NULL;
    int cipher_txt_len = 0;
    while (cli->is_running) {
        rlen = tap_read(cli->tapfd, data, sizeof(data));
        if (rlen <= 0) {
            _LOG_E("tap read error %s", strerror(errno));
            continue;
        }
        assert(rlen >= 14);
        /* encrypt */
        cipher_txt = data;
        cipher_txt_len = rlen;
        if (cli->key && *cli->key != '\0') cipher_txt = aes_encrypt(cli->key, data, rlen, &cipher_txt_len);
        wlen = sendto(cli->udpfd, cipher_txt, cipher_txt_len, 0, (struct sockaddr*)&cli->server_addr, sizeof(cli->server_addr));
        if (cli->key && *cli->key != '\0') free(cipher_txt);
        if (wlen <= 0) {
            _LOG_E("send to server error %s", strerror(errno));
            continue;
        }
        if (cipher_txt_len != wlen) {
            _LOG_E("client_send_routine length error %d != %d", cipher_txt_len, wlen);
        }
    }
}

void* client_recv_routine(void* ctx) {
    client_t* cli = (client_t*)ctx;
    char data[ETHER_MAX_LEN];
    int rlen = 0;
    int wlen = 0;
    char* plain_txt = NULL;
    int plain_txt_len = 0;
    socklen_t addr_len;
    while (cli->is_running) {
        addr_len = sizeof(cli->server_addr);
        rlen = recvfrom(cli->udpfd, data, sizeof(data), 0, (struct sockaddr*)&cli->server_addr, &addr_len);
        if (rlen <= 0) {
            _LOG_E("udp recv error %s", strerror(errno));
            continue;
        }
        /* decrypt */
        plain_txt = data;
        plain_txt_len = rlen;
        if (cli->key && *cli->key != '\0') plain_txt = aes_decrypt(cli->key, data, rlen, &plain_txt_len);
        assert(plain_txt_len >= 14);
        wlen = tap_write(cli->tapfd, plain_txt, plain_txt_len);
        if (cli->key && *cli->key != '\0') free(plain_txt);
        if (wlen <= 0) {
            _LOG_E("write to tap error %s", strerror(errno));
            continue;
        }
        if (plain_txt_len != wlen) {
            _LOG_E("client_recv_routine length error %d != %d", plain_txt_len, wlen);
        }
    }
}