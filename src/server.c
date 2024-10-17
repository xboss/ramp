#include "server.h"

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <string.h>
#include <unistd.h>

#include "cipher.h"
#include "common.h"
#include "uthash.h"

struct mac_talbe_s {
    unsigned char mac_addr[ETHER_ADDR_LEN];
    struct sockaddr_in target_addr;
    UT_hash_handle hh;
};
typedef struct mac_talbe_s mac_talbe_t;

struct server_s {
    int udpfd;
    char* key;
    mac_talbe_t* mac_talbe;
};

server_t* init_server(char* bind_ip, uint16_t port, char* key, char* iv) {
    if (port <= 0 || key == NULL || iv == NULL) {
        return NULL;
    }

    // 设置服务端
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == udpfd) {
        return NULL;
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    if (NULL == bind_ip) {
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        servaddr.sin_addr.s_addr = inet_addr(bind_ip);
    }
    servaddr.sin_port = htons(port);

    if (-1 == bind(udpfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
        close(udpfd);
        return NULL;
    }

    server_t* server = _ALLOC(server_t, sizeof(server_t));
    _IF_NULL(server) {
        LOG_E("%s", strerror(errno));
        return NULL;
    }

    server->udpfd = udpfd;
    server->key = key;
    server->iv = iv;
    server->mac_talbe = NULL;

    return server;
}

void free_server(server_t* server) {
    if (!server) {
        return;
    }

    mac_talbe_t *mac, *tmp;
    HASH_ITER(hh, server->mac_talbe, mac, tmp) {
        HASH_DEL(server->mac_talbe, mac);
        _FREE_IF(mac);
    }
    server->mac_talbe = NULL;

    _FREE_IF(server);
}

void run_server(server_t* server) {
    char data[ETHER_MAX_LEN];
    int rlen = 0;
    int wlen = 0;
    char* plain_txt = NULL;
    int plain_txt_len = 0;
    // char *cipher_txt = NULL;
    // int cipher_txt_len = 0;
    struct sockaddr_in cli_addr;
    socklen_t addr_len;
    struct ether_header* hdr = NULL;
    mac_talbe_t* item = NULL;
    mac_talbe_t* new_item = NULL;
    mac_talbe_t* tmp = NULL;
    mac_talbe_t* itr_item = NULL;
    // mac_talbe_t key_tmp;
    // memset(&key_tmp, 0, sizeof(mac_talbe_t));
    u_char broadcast_addr[ETHER_ADDR_LEN];
    memset(broadcast_addr, 0xff, ETHER_ADDR_LEN);
    u_char src_mac[ETHER_ADDR_LEN];
    memset(src_mac, 0, sizeof(ETHER_ADDR_LEN));
    u_char dst_mac[ETHER_ADDR_LEN];
    memset(dst_mac, 0, sizeof(ETHER_ADDR_LEN));
    while (true) {
        addr_len = sizeof(cli_addr);
        rlen = recvfrom(server->udpfd, data, sizeof(data), 0, (struct sockaddr*)&cli_addr, &addr_len);
        if (rlen <= 0) {
            LOG_E("udp recv error %s", strerror(errno));
            continue;
        }

        // decrypt
        plain_txt = data;
        plain_txt_len = rlen;
        _IF_STR_EMPTY(server->key) {
            plain_txt = aes_decrypt(server->key, server->iv, data, rlen, &plain_txt_len);
        }
        assert(plain_txt_len >= 14);

        hdr = (struct ether_header*)plain_txt;
        memcpy(src_mac, hdr->ether_shost, ETHER_ADDR_LEN);
        memcpy(dst_mac, hdr->ether_dhost, ETHER_ADDR_LEN);

        // // TODO: debug
        // printf("src_mac: ");
        // _prx(hdr->ether_shost, ETHER_ADDR_LEN);
        // printf("dst_mac: ");
        // _prx(hdr->ether_dhost, ETHER_ADDR_LEN);
        // // _prx(plain_txt, plain_txt_len);
        // LOG_D("---------");

        _IF_STR_EMPTY(server->key) {
            _FREE_IF(plain_txt);
        }

        // if register self to mac table
        HASH_FIND(hh, server->mac_talbe, src_mac, sizeof(src_mac), item);
        if (!item) {
            new_item = _ALLOC(mac_talbe_t, sizeof(mac_talbe_t));
            _IF_NULL(new_item) {
                LOG_E("%s", strerror(errno));
                exit(1);
            }
            memcpy(new_item->mac_addr, src_mac, sizeof(new_item->mac_addr));
            new_item->target_addr = cli_addr;
            HASH_ADD(hh, server->mac_talbe, mac_addr, sizeof(new_item->mac_addr), new_item);
            // LOG_D("hash add ok");
        } else if (memcmp(&item->target_addr, &cli_addr, sizeof(cli_addr)) != 0) {
            // LOG_D("hash replace %u %u", item->target_addr.sin_port, cli_addr.sin_port);
            item->target_addr = cli_addr;

            // new_item = _ALLOC(mac_talbe_t, sizeof(mac_talbe_t));
            // _IF_NULL(new_item) {
            //     LOG_E("%s", strerror(errno));
            //     exit(1);
            // }
            // memcpy(new_item->mac_addr, src_mac, sizeof(new_item->mac_addr));
            // new_item->target_addr = cli_addr;
            // HASH_REPLACE(hh, server->mac_talbe, mac_addr, sizeof(new_item->mac_addr), new_item, item);
        }

        // printf("route dst_mac: ");
        // _prx(dst_mac, ETHER_ADDR_LEN);
        // printf("route broadcast_addr: ");
        // _prx(broadcast_addr, ETHER_ADDR_LEN);

        // route
        HASH_FIND(hh, server->mac_talbe, dst_mac, sizeof(dst_mac), item);
        if (item) {
            // LOG_D("find dst_mac");
            wlen = sendto(server->udpfd, data, rlen, 0, (struct sockaddr*)&item->target_addr, sizeof(item->target_addr));
            if (wlen <= 0) {
                LOG_E("send to client error %s", strerror(errno));
                continue;
            } else {
                // LOG_D("send to client ok ethernet");
            }
        } else if (memcmp(dst_mac, broadcast_addr, sizeof(broadcast_addr)) == 0) {
            // broadcast
            // LOG_D("broadcast");
            HASH_ITER(hh, server->mac_talbe, itr_item, tmp) {
                wlen = sendto(server->udpfd, data, rlen, 0, (struct sockaddr*)&itr_item->target_addr, sizeof(itr_item->target_addr));
                if (wlen <= 0) {
                    LOG_E("send to client error %s", strerror(errno));
                    // TODO:  continue;
                } else {
                    // LOG_D("send to client ok broadcast");
                }
            }
        } else {
            // discard
            // LOG_D("discard packet");
            continue;
        }
    }
}
