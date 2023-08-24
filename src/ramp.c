#include <assert.h>
#include <ctype.h>
#include <ev.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <openssl/aes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "easy_tcp.h"
#include "tuntap.h"
#include "utlist.h"

#define _INFO_LOG
#define _WARN_LOG

#define RAMP_OK 0
#define RAMP_ERROR -1

#if !defined(_ALLOC)
#define _ALLOC(v_type, v_element_size) (v_type *)calloc(1, v_element_size)
#endif  // _ALLOC

#if !defined(FREE_IF)
#define FREE_IF(p)    \
    do {              \
        if (p) {      \
            free(p);  \
            p = NULL; \
        }             \
    } while (0)
#endif  // FREE_IF

#define LOG_E(fmt, args...)  \
    do {                     \
        printf("ERROR ");    \
        print_now();         \
        printf(fmt, ##args); \
        printf("\n");        \
    } while (0)

#ifdef _WARN_LOG
#define LOG_W(fmt, args...)  \
    do {                     \
        printf("WARN ");     \
        print_now();         \
        printf(fmt, ##args); \
        printf("\n");        \
    } while (0)
#else
#define LOG_W(fmt, args...) \
    do {                    \
        ;                   \
    } while (0)
#endif

#ifdef _INFO_LOG
#define LOG_I(fmt, args...)  \
    do {                     \
        printf("INFO ");     \
        print_now();         \
        printf(fmt, ##args); \
        printf("\n");        \
    } while (0)
#else
#define LOG_I(fmt, args...) \
    do {                    \
        ;                   \
    } while (0)
#endif

#ifdef DEBUG
#define LOG_D(fmt, args...)  \
    do {                     \
        printf("DEBUG ");    \
        print_now();         \
        printf(fmt, ##args); \
        printf("\n");        \
    } while (0)
#else
#define LOG_D(fmt, args...) \
    do {                    \
        ;                   \
    } while (0)
#endif

#define SERVER_MODE 0
#define CLIENT_MODE 1

#define IV_LEN 32
#define KEY_LEN 32
#define TICKET_LEN 32

#define TUN_R_BUF_SZ 1500
#define TCP_R_BUF_SZ 2048

typedef struct {
    char tun_ip[20];
    char tun_mask[20];
    char tcp_addr[128];
    char ticket[TICKET_LEN + 1];
    uint16_t tcp_port;
    int tcp_r_buf_size;
    int mode;
    char key[KEY_LEN + 1];
    char iv[IV_LEN + 1];
} ramp_conf_t;

typedef struct {
    uint ip;
    int fd;
    char buf[4096];
    int buf_len;
    UT_hash_handle hh;
} session_t;

typedef struct {
    ramp_conf_t *conf;
    int tun_fd;
    int tcp_fd;
    session_t *sess_map;
    etcp_serv_t *tcp_serv;
    etcp_cli_t *tcp_cli;
    struct ev_loop *loop;
} ctx_t;

static ctx_t *g_ctx = NULL;

/* -------------------------------------------------------------------------- */
/*                                   common                                   */
/* -------------------------------------------------------------------------- */

static uint64_t getmillisecond() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t millisecond = (tv.tv_sec * 1000000l + tv.tv_usec) / 1000l;
    return millisecond;
}

static void print_now() {
    time_t now;
    struct tm *tm_now;
    time(&now);
    tm_now = localtime(&now);
    printf("%d-%d-%d %d:%d:%d:%llu ", tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday, tm_now->tm_hour,
           tm_now->tm_min, tm_now->tm_sec, (getmillisecond() % 1000ll));
}

static void setnonblock(int fd) {
    if (-1 == fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)) {
        LOG_E("error fcntl");
    }
}

// static void _PR(const void *buf, int len) {
//     const char *pb = buf;
//     for (size_t i = 0; i < len; i++) {
//         // unsigned char c = *pb;
//         printf("%2X ", ((*pb) & 0xFF));
//         pb++;
//     }
//     printf("\n");
// }

/* -------------------------------------------------------------------------- */
/*                                   config                                   */
/* -------------------------------------------------------------------------- */

#define CONF_MAX_CHAR_PER_LINE 1024

static char *trim(char *str) {
    char *p = str;
    char *p1;
    if (p) {
        p1 = p + strlen(str) - 1;
        while (*p && isspace(*p)) p++;
        while (p1 > p && isspace(*p1)) *p1-- = '\0';
    }
    return p;
}

static void char_to_hex(const char *src, int len, char *des) {
    char hex_table[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    while (len--) {
        *(des++) = hex_table[(*src) >> 4];
        *(des++) = hex_table[*(src++) & 0x0f];
    }
}

static void fill_conf(ramp_conf_t *conf, const char *k, const char *v) {
    // "mode", "tun_ip", "tun_mask", "tcp_addr", "tcp_port", "password"
    int len = strlen(v);
    if (strcmp("mode", k) == 0) {
        if (strcmp(v, "client") == 0) {
            conf->mode = CLIENT_MODE;
        } else {
            conf->mode = SERVER_MODE;
        }
    } else if (strcmp("tun_ip", k) == 0) {
        if (len < sizeof(conf->tun_ip)) {
            memcpy(conf->tun_ip, v, len);
        }
    } else if (strcmp("tun_mask", k) == 0) {
        if (len < sizeof(conf->tun_mask)) {
            memcpy(conf->tun_mask, v, len);
        }
    } else if (strcmp("tcp_addr", k) == 0) {
        if (len < sizeof(conf->tcp_addr)) {
            memcpy(conf->tcp_addr, v, len);
        }
    } else if (strcmp("ticket", k) == 0) {
        if (len < sizeof(conf->ticket)) {
            memcpy(conf->ticket, v, len);
        }
    } else if (strcmp("tcp_port", k) == 0) {
        conf->tcp_port = (uint16_t)atoi(v);
    } else if (strcmp("tcp_read_buffer_size", k) == 0) {
        conf->tcp_r_buf_size = atoi(v);
    } else if (strcmp("password", k) == 0) {
        char_to_hex(v, KEY_LEN / 2, conf->key);
    }
}

static ramp_conf_t *load_conf(const char *conf_file) {
    FILE *fp;
    if ((fp = fopen(conf_file, "r")) == NULL) {
        LOG_E("can't open conf file %s", conf_file);
        return NULL;
    }

    ramp_conf_t *conf = _ALLOC(ramp_conf_t, sizeof(ramp_conf_t));

    char line[CONF_MAX_CHAR_PER_LINE] = {0};
    // char *token = NULL;
    char *k = NULL;
    char *v = NULL;
    char *d = "=";
    while (fgets(line, CONF_MAX_CHAR_PER_LINE, fp) != NULL) {
        line[CONF_MAX_CHAR_PER_LINE - 1] = '\0';
        if (strlen(line) == 0) {
            continue;
        }
        char *p = trim(line);
        if (*p == '#') {
            continue;
        }
        k = strtok(p, d);
        if (k == NULL) {
            continue;
        }
        v = strtok(NULL, d);
        if (v == NULL) {
            continue;
        }
        k = trim(k);
        v = trim(v);
        fill_conf(conf, k, v);
    }
    fclose(fp);

    printf("---config---\n");
    printf("mode:%d\n", conf->mode);
    printf("tun_ip:%s\n", conf->tun_ip);
    printf("tun_mask:%s\n", conf->tun_mask);
    printf("tcp_addr:%s\n", conf->tcp_addr);
    printf("tcp_port:%u\n", conf->tcp_port);
    printf("key:%s\n", conf->key);
    return conf;
}

/* -------------------------------------------------------------------------- */
/*                                   cipher                                   */
/* -------------------------------------------------------------------------- */

inline static unsigned char *str2hex(const char *str) {
    unsigned char *ret = NULL;
    int str_len = strlen(str);
    int i = 0;
    assert((str_len % 2) == 0);
    ret = malloc(str_len / 2);
    for (i = 0; i < str_len; i = i + 2) {
        sscanf(str + i, "%2hhx", &ret[i / 2]);
    }
    return ret;
}

inline static char *cipher_padding(const char *buf, int size, int *final_size) {
    char *ret = NULL;
    int padding_size = AES_BLOCK_SIZE - (size % AES_BLOCK_SIZE);
    *final_size = size + padding_size;
    ret = _ALLOC(char, *final_size);
    memcpy(ret, buf, size);
    assert(size < *final_size);
    return ret;
}

inline static void aes_cbc_encrpyt(const char *raw_buf, char **encrpy_buf, int len, const char *key, const char *iv) {
    AES_KEY aes_key;
    unsigned char *skey = str2hex(key);
    unsigned char *siv = str2hex(iv);
    AES_set_encrypt_key(skey, 128, &aes_key);
    AES_cbc_encrypt((unsigned char *)raw_buf, (unsigned char *)*encrpy_buf, len, &aes_key, siv, AES_ENCRYPT);
    FREE_IF(skey);
    FREE_IF(siv);
}
inline static void aes_cbc_decrypt(const char *raw_buf, char **encrpy_buf, int len, const char *key, const char *iv) {
    AES_KEY aes_key;
    unsigned char *skey = str2hex(key);
    unsigned char *siv = str2hex(iv);
    AES_set_decrypt_key(skey, 128, &aes_key);
    AES_cbc_encrypt((unsigned char *)raw_buf, (unsigned char *)*encrpy_buf, len, &aes_key, siv, AES_DECRYPT);
    FREE_IF(skey);
    FREE_IF(siv);
}
inline static char *aes_encrypt(const char *key, const char *iv, const char *in, int in_len, int *out_len) {
    int padding_size = in_len;
    char *after_padding_buf = (char *)in;
    if (in_len % 16 != 0) {
        after_padding_buf = cipher_padding(in, in_len, &padding_size);
    }
    *out_len = padding_size;

    char *out_buf = malloc(padding_size);
    memset(out_buf, 0, padding_size);
    aes_cbc_encrpyt(after_padding_buf, &out_buf, padding_size, key, iv);
    if (in_len % 16 != 0) {
        FREE_IF(after_padding_buf);
    }
    return out_buf;
}

static char *aes_decrypt(const char *key, const char *iv, const char *in, int in_len, int *out_len) {
    int padding_size = in_len;
    char *after_padding_buf = (char *)in;
    if (in_len % 16 != 0) {
        after_padding_buf = cipher_padding(in, in_len, &padding_size);
    }
    *out_len = padding_size;

    char *out_buf = malloc(padding_size);
    memset(out_buf, 0, padding_size);
    aes_cbc_decrypt(after_padding_buf, &out_buf, padding_size, key, iv);
    if (in_len % 16 != 0) {
        FREE_IF(after_padding_buf);
    }
    return out_buf;
}

/* -------------------------------------------------------------------------- */
/*                                     tcp                                    */
/* -------------------------------------------------------------------------- */
// protocol format: len(uint32_t)+cmd(char)+payload(char*)

#define PKT_CMD_DATA 0x01
#define PKT_CMD_PING 0x02
#define PKT_CMD_PONG 0x03
#define PKT_CMD_AUTH 0x04
#define PKT_CMD_AUTH_ACK 0x05

#define PKT_HEAD_LEN 5
#define AUTH_MSG_MAX_LEN 64
#define AUTH_MSG_ACK_MAX_LEN 32

#define AUTH_OK "ok"
#define AUTH_ERROR "error"
#define AUTH_MSG_SEP "\n"

static int tcp_send(int fd, char cmd, char *buf, int len) {
    int w_len = 0;

    // encrypt
    char *cipher_buf = buf;
    int cipher_buf_len = len;
    if (strlen(g_ctx->conf->key) > 0) {
        cipher_buf = aes_encrypt(g_ctx->conf->key, g_ctx->conf->iv, buf, len, &cipher_buf_len);
    }

    uint32_t raw_len = PKT_HEAD_LEN + cipher_buf_len;
    char *raw = _ALLOC(char, raw_len);
    uint32_t en_remain_len = htonl(cipher_buf_len + 1);
    memcpy(raw, &en_remain_len, sizeof(en_remain_len));
    *(raw + sizeof(en_remain_len)) = cmd;
    memcpy(raw + PKT_HEAD_LEN, cipher_buf, cipher_buf_len);
    if (strlen(g_ctx->conf->key) > 0) {
        FREE_IF(cipher_buf);
    }

    if (g_ctx->conf->mode == CLIENT_MODE) {
        // client
        w_len = etcp_client_send(g_ctx->tcp_cli, fd, raw, raw_len);
    } else {
        // server
        w_len = etcp_server_send(g_ctx->tcp_serv, fd, raw, raw_len);
    }
    FREE_IF(raw);
    if (w_len <= 0) {
        return 0;
    }
    return w_len;
}

typedef struct pkt_s {
    char cmd;
    char *data;
    uint32_t data_len;
    struct pkt_s *next, *prev;
} pkt_t;

static pkt_t *pack_data(char *buf, int len, session_t *sess) {
    pkt_t *pkt_list = NULL;
    char *n_buf = NULL;
    int n_len = len;

    // check the remianing data
    if (sess->buf_len > 0) {
        n_len = len + sess->buf_len;
        n_buf = _ALLOC(char, n_len);
        if (n_buf == NULL) {
            LOG_E("alloc error");
            return pkt_list;
        }
        memcpy(n_buf, sess->buf, sess->buf_len);
        memcpy(n_buf + sess->buf_len, buf, len);

        memset(sess->buf, 0, sizeof(sess->buf));
        sess->buf_len = 0;
    } else {
        n_buf = buf;
    }

    char *p = n_buf;
    int left_len = 0;
    while (1) {
        left_len = n_len - (p - n_buf);
        if (left_len > 0 && left_len < PKT_HEAD_LEN) {
            // save the remaining data
            memcpy(sess->buf, p, left_len);
            sess->buf_len = left_len;
            break;
        }

        uint32_t remain_len = ntohl(*(uint32_t *)p);
        if (remain_len < 1) {
            LOG_E("pack_data remain_len error remain_len:%u", remain_len);
            // goto pack_data_end;
            break;
        }
        p += sizeof(remain_len);
        left_len = n_len - (p - n_buf);
        if (left_len > 0 && left_len < remain_len) {
            // save the remaining data
            memcpy(sess->buf, p - sizeof(remain_len), left_len + sizeof(remain_len));
            sess->buf_len = left_len + sizeof(remain_len);
            break;
        }
        pkt_t *pkt = _ALLOC(pkt_t, sizeof(pkt_t));
        pkt->cmd = *p;
        p++;
        // decrypt
        char *plain_buf = NULL;
        int plain_len = remain_len - 1;
        if (strlen(g_ctx->conf->key) > 0) {
            plain_buf = aes_decrypt(g_ctx->conf->key, g_ctx->conf->iv, p, remain_len - 1, &plain_len);
            pkt->data = plain_buf;
        } else {
            plain_buf = p;
            pkt->data = _ALLOC(char, plain_len);
            memcpy(pkt->data, plain_buf, plain_len);
        }

        pkt->data_len = plain_len;
        DL_APPEND(pkt_list, pkt);
        p += remain_len - 1;
        if ((p - n_buf) == n_len) {
            break;
        }
        assert(n_len > (p - n_buf));
    }

    // pack_data_end:
    if (n_buf != buf) {
        FREE_IF(n_buf);
    }

    return pkt_list;
}

static void on_tcp_cli_recv(int fd, char *buf, int len) {
    pkt_t *pkt_list = pack_data(buf, len, g_ctx->sess_map);
    if (pkt_list == NULL) {
        return;
    }

    pkt_t *pkt, *tmp;
    char tm[32] = {0};
    int tm_len = 0;
    uint64_t tmi = 0;
    uint64_t now = 0;
    int rt = 0;
    DL_FOREACH_SAFE(pkt_list, pkt, tmp) {
        if (pkt->cmd == PKT_CMD_PONG) {
            bzero(tm, sizeof(tm));
            tm_len = pkt->data_len > 31 ? 31 : pkt->data_len;
            sprintf(tm, pkt->data, tm_len);
            tmi = atoll(tm);
            now = getmillisecond();
            LOG_I("rtt:%lu", (unsigned long)(now - tmi));
        } else if (pkt->cmd == PKT_CMD_DATA) {
            // LOG_D("on_tcp_cli_recv data data_len: %u", pkt->data_len);
            rt = tuntap_write(g_ctx->tun_fd, pkt->data, pkt->data_len);
            if (rt <= 0) {
                LOG_E("on_tcp_cli_recv tuntap_write error");
            }
        } else if (pkt->cmd == PKT_CMD_AUTH_ACK) {
            if (pkt->data_len < 2 || pkt->data_len >= AUTH_MSG_ACK_MAX_LEN) {
                LOG_E("on_tcp_cli_recv auth ack error len:%d", pkt->data_len);
                return;  // TODO: exit?
            }
            char auth_ack_msg[AUTH_MSG_ACK_MAX_LEN] = {0};
            memcpy(auth_ack_msg, pkt->data, pkt->data_len);
            if (strcmp(auth_ack_msg, AUTH_OK) != 0) {
                LOG_E("on_tcp_cli_recv auth ack error:%s", auth_ack_msg);
                return;  // TODO: exit?
            }
            inet_pton(AF_INET, g_ctx->conf->tun_ip, &(g_ctx->sess_map->ip));
        } else {
            LOG_E("on_tcp_cli_recv error cmd:%x", pkt->cmd);
        }

        DL_DELETE(pkt_list, pkt);
        FREE_IF(pkt->data);
        FREE_IF(pkt);
    }
}

static void on_tcp_cli_close(int fd) {
    LOG_I("on_tcp_cli_close %d", fd);
    g_ctx->tcp_fd = 0;
}

static int on_tcp_serv_accept(int fd) {
    LOG_I("on_tcp_serv_accept %d", fd);
    etcp_serv_conn_t *conn = etcp_server_get_conn(g_ctx->tcp_serv, fd);
    if (conn) {
        session_t *sess = _ALLOC(session_t, sizeof(session_t));
        sess->fd = fd;
        conn->user_data = sess;
    }

    return 0;
}

#define SEND_AUTH_ERROR                                             \
    tcp_send(fd, PKT_CMD_AUTH_ACK, AUTH_ERROR, strlen(AUTH_ERROR)); \
    etcp_server_close_conn(g_ctx->tcp_serv, fd, 0);                 \
    continue

static void on_tcp_serv_recv(int fd, char *buf, int len) {
    etcp_serv_conn_t *conn = etcp_server_get_conn(g_ctx->tcp_serv, fd);
    if (!conn) {
        LOG_E("connection is not exists. fd:%d", fd);
        return;
    }
    if (!conn->user_data) {
        LOG_E("session is not exists. fd:%d", fd);
        etcp_server_close_conn(g_ctx->tcp_serv, fd, 0);
        return;
    }

    session_t *sess = (session_t *)conn->user_data;

    pkt_t *pkt_list = pack_data(buf, len, sess);
    if (pkt_list == NULL) {
        return;
    }

    int rt = 0;
    char auth_msg[AUTH_MSG_MAX_LEN] = {0};
    char *ip_str = NULL;
    char *ticket = NULL;
    pkt_t *pkt, *tmp;
    DL_FOREACH_SAFE(pkt_list, pkt, tmp) {
        if (pkt->cmd == PKT_CMD_PING) {
            // LOG_D("on_tcp_serv_recv ping");
            rt = tcp_send(fd, PKT_CMD_PONG, pkt->data, pkt->data_len);
            if (rt <= 0) {
                LOG_E("on_tcp_serv_recv tcp_send pong error");
            }
        } else if (pkt->cmd == PKT_CMD_DATA) {
            // LOG_D("on_tcp_serv_recv data data_len: %u", pkt->data_len);
            rt = tuntap_write(g_ctx->tun_fd, pkt->data, pkt->data_len);
            if (rt <= 0) {
                LOG_E("on_tcp_serv_recv tuntap_write error");
            }
        } else if (pkt->cmd == PKT_CMD_AUTH) {
            if (pkt->data_len >= AUTH_MSG_MAX_LEN || pkt->data_len < 3) {
                SEND_AUTH_ERROR;
            }
            memcpy(auth_msg, pkt->data, pkt->data_len);
            ip_str = strtok(auth_msg, AUTH_MSG_SEP);
            if (!ip_str) {
                SEND_AUTH_ERROR;
            }
            // TODO: check ip
            ticket = strtok(NULL, AUTH_MSG_SEP);
            if (!ticket) {
                SEND_AUTH_ERROR;
            }
            // TODO: check ticket
            if (inet_pton(AF_INET, ip_str, &sess->ip) != 1) {
                SEND_AUTH_ERROR;
            }
            HASH_ADD_INT(g_ctx->sess_map, ip, sess);
            rt = tcp_send(fd, PKT_CMD_AUTH_ACK, AUTH_OK, strlen(AUTH_OK));
            if (rt <= 0) {
                LOG_E("on_tcp_serv_recv send auth_ack_ok error");
            }
        } else {
            LOG_E("on_tcp_serv_recv error cmd:%x", pkt->cmd);
        }

        DL_DELETE(pkt_list, pkt);
        FREE_IF(pkt->data);
        FREE_IF(pkt);
    }
}

static void on_tcp_serv_close(int fd) {
    LOG_I("on_tcp_serv_close %d", fd);
    etcp_serv_conn_t *conn = etcp_server_get_conn(g_ctx->tcp_serv, fd);
    if (conn && conn->user_data) {
        session_t *sess = (session_t *)conn->user_data;
        sess->fd = 0;  // TODO: for test
        if (g_ctx->sess_map) {
            HASH_DEL(g_ctx->sess_map, sess);
        }
        FREE_IF(conn->user_data);
    }
}

static int init_tcp() {
    if (g_ctx->conf->mode == CLIENT_MODE) {
        // client
        etcp_cli_conf_t *tcp_conf = _ALLOC(etcp_cli_conf_t, sizeof(etcp_cli_conf_t));
        ETCP_CLI_DEF_CONF(tcp_conf);
        if (g_ctx->conf->tcp_r_buf_size > 0) {
            tcp_conf->r_buf_size = g_ctx->conf->tcp_r_buf_size;
        } else {
            tcp_conf->r_buf_size = TCP_R_BUF_SZ;
        }
        tcp_conf->nodelay = 1;
        tcp_conf->on_close = on_tcp_cli_close;
        tcp_conf->on_recv = on_tcp_cli_recv;
        g_ctx->tcp_cli = etcp_init_client(tcp_conf, g_ctx->loop, NULL);
        if (!g_ctx->tcp_cli) {
            return RAMP_ERROR;
        }
        g_ctx->tcp_fd = etcp_client_create_conn(g_ctx->tcp_cli, g_ctx->conf->tcp_addr, g_ctx->conf->tcp_port, NULL);
        if (g_ctx->tcp_fd <= 0) {
            return RAMP_ERROR;
        }
        g_ctx->sess_map = _ALLOC(session_t, sizeof(session_t));
        LOG_I("tcp client ok. %s %d %d", g_ctx->conf->tcp_addr, g_ctx->conf->tcp_port, g_ctx->tcp_fd);
    } else {
        // server
        etcp_serv_conf_t *tcp_conf = _ALLOC(etcp_serv_conf_t, sizeof(etcp_serv_conf_t));
        ETCP_SERV_DEF_CONF(tcp_conf);

        if (g_ctx->conf->tcp_r_buf_size > 0) {
            tcp_conf->r_buf_size = g_ctx->conf->tcp_r_buf_size;
        } else {
            tcp_conf->r_buf_size = TCP_R_BUF_SZ;
        }

        tcp_conf->nodelay = 1;
        tcp_conf->serv_addr = g_ctx->conf->tcp_addr;
        tcp_conf->serv_port = g_ctx->conf->tcp_port;
        tcp_conf->on_accept = on_tcp_serv_accept;
        tcp_conf->on_recv = on_tcp_serv_recv;
        tcp_conf->on_close = on_tcp_serv_close;
        g_ctx->tcp_serv = etcp_init_server(tcp_conf, g_ctx->loop, NULL);
        if (!g_ctx->tcp_serv) {
            return RAMP_ERROR;
        }
        g_ctx->tcp_fd = g_ctx->tcp_serv->serv_fd;
        LOG_I("tcp server start ok. %s %d %d", g_ctx->conf->tcp_addr, g_ctx->conf->tcp_port, g_ctx->tcp_fd);
    }
    return RAMP_OK;
}

/* -------------------------------------------------------------------------- */
/*                                    ramp                                    */
/* -------------------------------------------------------------------------- */

static void finish() {
    if (!g_ctx) {
        return;
    }

    if (g_ctx->tun_fd > 0) {
        close(g_ctx->tun_fd);
        g_ctx->tun_fd = 0;
    }

    if (g_ctx->loop) {
        ev_break(g_ctx->loop, EVBREAK_ALL);
        g_ctx->loop = NULL;
    }

    if (g_ctx->conf->mode == CLIENT_MODE) {
        // client mode
        etcp_free_client(g_ctx->tcp_cli);
        g_ctx->tcp_cli = NULL;
        FREE_IF(g_ctx->sess_map);
    } else {
        // server mode
        etcp_free_server(g_ctx->tcp_serv);
        g_ctx->tcp_serv = NULL;
        if (g_ctx->sess_map) {
            session_t *sess, *tmp;
            HASH_ITER(hh, g_ctx->sess_map, sess, tmp) {
                HASH_DEL(g_ctx->sess_map, sess);
                FREE_IF(sess);
            }
            g_ctx->sess_map = NULL;
        }
    }

    if (g_ctx->conf) {
        FREE_IF(g_ctx->conf);
    }

    FREE_IF(g_ctx);
}

static int init_tuntap() {
    char dev_name[32] = {0};
    int tunfd = tuntap_open(dev_name, sizeof(dev_name));

    if (tunfd == -1) {
        LOG_E("open tuntap error");
        return RAMP_ERROR;
    }

    // 设置为非阻塞
    setnonblock(tunfd);

    tuntap_setup(dev_name, g_ctx->conf->tun_ip, g_ctx->conf->tun_mask);

    return tunfd;
}

static void sig_cb(struct ev_loop *loop, ev_signal *w, int revents) {
    LOG_I("sig_cb signal:%d", w->signum);
    if (w->signum == SIGPIPE) {
        return;
    }

    ev_break(loop, EVBREAK_ALL);
    LOG_I("sig_cb loop break all event ok");
}

static void on_tun_read(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    if (EV_ERROR & revents) {
        LOG_E("on_tun_read got invalid event");
        return;
    }

    if (g_ctx->tcp_fd <= 0) {
        return;
    }

    char buf[TUN_R_BUF_SZ];
    int len = tuntap_read(g_ctx->tun_fd, buf, sizeof(buf));
    if (len <= 0) {
        LOG_E("tuntap_read error tun_fd: %d", g_ctx->tun_fd);
        return;
    }

    struct ip *ip = (struct ip *)buf;
#ifdef DEBUG
    char src_ip[INET_ADDRSTRLEN] = {0};
    char dest_ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &(ip->ip_src.s_addr), src_ip, sizeof(src_ip));
    inet_ntop(AF_INET, &(ip->ip_dst.s_addr), dest_ip, sizeof(dest_ip));
    // LOG_D("tun_read_cb src_ip: %s dest_ip: %s len: %d", src_ip, dest_ip, len);
    // LOG_D("tun_read_cb len: %d", len);
    if (strcmp("0.0.0.0", src_ip) == 0) {
        // LOG_D("tun_read_cb exclude %s %s %d", src_ip, dest_ip, len);
        return;
    }
    // else {
    //     LOG_D("tun_read_cb src_ip: %s dest_ip: %s len: %d", src_ip, dest_ip, len);
    // }
#endif

    int rt = 0;
    if (g_ctx->conf->mode == CLIENT_MODE) {
        // client
        // LOG_D("tun_read_cb len:%d", len);
        rt = tcp_send(g_ctx->tcp_fd, PKT_CMD_DATA, buf, len);
        if (rt <= 0) {
            LOG_E("client tcp_send error %d", g_ctx->tcp_fd);
        }

    } else {
        // server
        session_t *sess = NULL;
        HASH_FIND_INT(g_ctx->sess_map, &(ip->ip_dst.s_addr), sess);
        if (!sess) {
            // TODO: white ip list
            char dest_ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &(ip->ip_dst.s_addr), dest_ip, sizeof(dest_ip));
            LOG_W("no connections client ip:%s", dest_ip);
            return;
        }
        rt = tcp_send(sess->fd, PKT_CMD_DATA, buf, len);
        if (rt <= 0) {
            LOG_E("server tcp_send error %d", sess->fd);
            return;
        }
    }
}

static void on_beat(struct ev_loop *loop, struct ev_timer *watcher, int revents) {
    if (EV_ERROR & revents) {
        LOG_E("on_beat got invalid event");
        return;
    }

    if (g_ctx->conf->mode == CLIENT_MODE) {
        // client
        if (g_ctx->tcp_fd > 0) {
            if (g_ctx->sess_map->ip == 0) {
                // send auth
                int tun_ip_len = strlen(g_ctx->conf->tun_ip);
                int ticket_len = strlen(g_ctx->conf->ticket);
                if (tun_ip_len + ticket_len + 2 > AUTH_MSG_MAX_LEN) {
                    LOG_E("on_beat send auth error len:%d", tun_ip_len + ticket_len + 2);
                    return;
                }

                char auth_msg[AUTH_MSG_MAX_LEN] = {0};
                sprintf(auth_msg, "%s\n%s", g_ctx->conf->tun_ip, g_ctx->conf->ticket);
                int rt = tcp_send(g_ctx->tcp_fd, PKT_CMD_AUTH, auth_msg, strlen(auth_msg));
                if (rt <= 0) {
                    LOG_E("on_beat client tcp_send error %d", g_ctx->tcp_fd);
                }
            } else {
                // ping
                LOG_D("send ping %d", g_ctx->tcp_fd);
                uint64_t now = getmillisecond();
                // now = htonll(now);
                char tm[32] = {0};
                sprintf(tm, "%lu", (unsigned long)now);
                int rt = tcp_send(g_ctx->tcp_fd, PKT_CMD_PING, tm, strlen(tm));
                if (rt <= 0) {
                    LOG_E("on_beat client tcp_send error %d", g_ctx->tcp_fd);
                }
            }

        } else {
            // reconnect
            LOG_W("client reconnect...");
            g_ctx->tcp_fd = etcp_client_create_conn(g_ctx->tcp_cli, g_ctx->conf->tcp_addr, g_ctx->conf->tcp_port, NULL);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                                    main                                    */
/* -------------------------------------------------------------------------- */

#define _USAGE fprintf(stderr, "Usage: %s <config file>\n", argv[0])

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        _USAGE;
        return -1;
    }

    ramp_conf_t *conf = load_conf(argv[1]);
    if (!conf) {
        finish();
        return -1;
    }
    memcpy(conf->iv, "612789a8907bcf123de4590abc678def", IV_LEN);  // TODO: dynamic iv

    g_ctx = _ALLOC(ctx_t, sizeof(ctx_t));
    g_ctx->conf = conf;
    g_ctx->sess_map = NULL;

// init libev
#if (defined(__linux__) || defined(__linux))
    g_ctx->loop = ev_loop_new(EVBACKEND_EPOLL);
#elif defined(__APPLE__)
    g_ctx->loop = ev_loop_new(EVBACKEND_KQUEUE);
#else
    g_ctx->loop = ev_default_loop(0);
#endif

    // init tuntap
    g_ctx->tun_fd = init_tuntap();
    if (g_ctx->tun_fd == RAMP_ERROR) {
        finish();
        return -1;
    }

    // init tcp
    if (init_tcp() == RAMP_ERROR) {
        finish();
        return -1;
    }

    // 设置tun读事件循环
    struct ev_io r_watcher;
    ev_io_init(&r_watcher, on_tun_read, g_ctx->tun_fd, EV_READ);
    ev_io_start(g_ctx->loop, &r_watcher);

    // 定时
    struct ev_timer bt_watcher;
    ev_init(&bt_watcher, on_beat);
    ev_timer_set(&bt_watcher, 1, 1);
    ev_timer_start(g_ctx->loop, &bt_watcher);

    ev_signal sig_pipe_watcher;
    ev_signal_init(&sig_pipe_watcher, sig_cb, SIGPIPE);
    ev_signal_start(g_ctx->loop, &sig_pipe_watcher);

    ev_signal sig_int_watcher;
    ev_signal_init(&sig_int_watcher, sig_cb, SIGINT);
    ev_signal_start(g_ctx->loop, &sig_int_watcher);

    ev_signal sig_stop_watcher;
    ev_signal_init(&sig_stop_watcher, sig_cb, SIGSTOP);
    ev_signal_start(g_ctx->loop, &sig_stop_watcher);

    LOG_D("ok");
    ev_run(g_ctx->loop, 0);
    LOG_D("bye");

    return 0;
}
