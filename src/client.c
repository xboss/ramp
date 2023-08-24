#include "client.h"

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <string.h>
#include <unistd.h>

#include "cipher.h"
#include "common.h"
#include "tap.h"

// // TODO: debug
// static void _prx(const void *buf, int len) {
//     const char *pb = buf;
//     for (size_t i = 0; i < len; i++) {
//         // unsigned char c = *pb;
//         printf("%2X ", ((*pb) & 0xFF));
//         pb++;
//     }
//     printf("\n");
// }

struct client_s {
    int tapfd;
    int udpfd;
    struct sockaddr_in server_addr;
    char *key;
    char *iv;
    // char key[KEY_LEN + 1];
    // char iv[IV_LEN + 1];
};

client_t *init_client(char *server_ip, uint16_t server_port, char *key, char *iv) {
    if (server_ip == NULL || server_port <= 0 || key == NULL || iv == NULL) {
        return NULL;
    }

    // init tap
    char dev_name[32] = {0};
    int tapfd = tap_open(dev_name, sizeof(dev_name));

    if (tapfd == -1) {
        LOG_E("open tap error");
        return NULL;
    }

    LOG_D("open tap ok %d %s", tapfd, dev_name);

    // init udp
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) {
        LOG_E("udp socket error %s", strerror(errno));
        return NULL;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        LOG_E("server ip error %s", strerror(errno));
        close(udpfd);
        return NULL;
    }

    client_t *cli = _ALLOC(client_t, sizeof(client_t));
    _IF_NULL(cli) {
        LOG_E("%s", strerror(errno));
        return NULL;
    }

    cli->tapfd = tapfd;
    cli->udpfd = udpfd;
    cli->server_addr = server_addr;
    cli->key = key;
    cli->iv = iv;

    return cli;
}

void free_client(client_t *cli) {
    _IF_NULL(cli) { return; }

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

void *client_send_routine(void *ctx) {
    client_t *cli = (client_t *)ctx;
    char data[ETHER_MAX_LEN];
    int rlen = 0;
    int wlen = 0;
    char *cipher_txt = NULL;
    int cipher_txt_len = 0;
    while (true) {
        rlen = tap_read(cli->tapfd, data, sizeof(data));
        if (rlen <= 0) {
            LOG_E("tap read error %s", strerror(errno));
            continue;
        }
        assert(rlen >= 14);

        // // TODO: debug
        // if (rlen >= 1472) {
        //     LOG_D("raw len from tap: %d", rlen);
        // }

        // // TODO: debug
        // struct ether_header *hdr = (struct ether_header *)data;
        // printf("src_mac: ");
        // _prx(hdr->ether_shost, ETHER_ADDR_LEN);
        // printf("dst_mac: ");
        // _prx(hdr->ether_dhost, ETHER_ADDR_LEN);
        // // _prx(data, rlen);
        // LOG_D("---------");

        // encrypt
        cipher_txt = data;
        cipher_txt_len = rlen;
        _IF_STR_EMPTY(cli->key) {
            cipher_txt = aes_encrypt(cli->key, cli->iv, data, rlen, &cipher_txt_len);  // TODO: 性能优化
        }

        wlen = sendto(cli->udpfd, cipher_txt, cipher_txt_len, 0, (struct sockaddr *)&cli->server_addr,
                      sizeof(cli->server_addr));

        _IF_STR_EMPTY(cli->key) { _FREE_IF(cipher_txt); }
        if (wlen <= 0) {
            LOG_E("send to server error %s", strerror(errno));
            continue;
        }
        if (cipher_txt_len != wlen) {
            LOG_E("client_send_routine length error %d != %d", cipher_txt_len, wlen);
        }
        // LOG_D("send to server ok");
    }
}

void *client_recv_routine(void *ctx) {
    client_t *cli = (client_t *)ctx;
    char data[ETHER_MAX_LEN];
    int rlen = 0;
    int wlen = 0;
    char *plain_txt = NULL;
    int plain_txt_len = 0;
    socklen_t addr_len;
    while (true) {
        addr_len = sizeof(cli->server_addr);
        rlen = recvfrom(cli->udpfd, data, sizeof(data), 0, (struct sockaddr *)&cli->server_addr, &addr_len);
        if (rlen <= 0) {
            LOG_E("udp recv error %s", strerror(errno));
            continue;
        }

        // decrypt
        plain_txt = data;
        plain_txt_len = rlen;
        _IF_STR_EMPTY(cli->key) { plain_txt = aes_decrypt(cli->key, cli->iv, data, rlen, &plain_txt_len); }

        assert(plain_txt_len >= 14);

        // // TODO: debug
        // struct ether_header *hdr = (struct ether_header *)plain_txt;
        // printf("src_mac: ");
        // _prx(hdr->ether_shost, ETHER_ADDR_LEN);
        // printf("dst_mac: ");
        // _prx(hdr->ether_dhost, ETHER_ADDR_LEN);
        // // _prx(plain_txt, plain_txt_len);
        // LOG_D("---------");

        wlen = tap_write(cli->tapfd, plain_txt, plain_txt_len);
        _IF_STR_EMPTY(cli->key) { _FREE_IF(plain_txt); }
        if (wlen <= 0) {
            LOG_E("write to tap error %s", strerror(errno));
            continue;
        }

        if (plain_txt_len != wlen) {
            LOG_E("client_recv_routine length error %d != %d", plain_txt_len, wlen);
        }
        // LOG_D("write to tap ok");
    }
}