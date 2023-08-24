#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define SERVER_MODE 0
#define CLIENT_MODE 1

#define IV_LEN 32
#define KEY_LEN 32
#define _ALLOC(v_type, v_element_size) (v_type *)calloc(1, v_element_size)
#define LOG_E(fmt, args...)  \
    do {                     \
        printf("ERROR ");    \
        printf(fmt, ##args); \
        printf("\n");        \
    } while (0)

/* -------------------------------------------------------------------------- */
/*                                   config                                   */
/* -------------------------------------------------------------------------- */

#define CONF_MAX_CHAR_PER_LINE 1024

typedef struct {
    char tun_ip[128];
    char tun_mask[128];
    char tcp_addr[128];
    uint16_t tcp_port;
    int mode;
    char key[KEY_LEN + 1];
    char iv[IV_LEN + 1];
} ramp_conf_t;

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
    } else if (strcmp("tcp_port", k) == 0) {
        conf->tcp_port = (uint16_t)atoi(v);
    } else if (strcmp("password", k) == 0) {
        char_to_hex(v, KEY_LEN / 2, conf->key);
    }
}

static ramp_conf_t *load_conf(const char *conf_file) {
    ramp_conf_t *conf = _ALLOC(ramp_conf_t, sizeof(ramp_conf_t));
    memcpy(conf->iv, "1234567890abcdef1234567890abcdef", IV_LEN);
    FILE *fp;
    if ((fp = fopen(conf_file, "r")) == NULL) {
        LOG_E("can't open conf file %s", conf_file);
        return NULL;
    }

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
        k = strtok(line, d);
        if (k == NULL) {
            continue;
        }
        v = strtok(NULL, d);
        if (v == NULL) {
            continue;
        }
        k = trim(k);
        v = trim(v);
        printf("key:%s value:%s\n", k, v);
        fill_conf(conf, k, v);
    }
    fclose(fp);

    printf("------\n");
    printf("mode:%d\n", conf->mode);
    printf("tun_ip:%s\n", conf->tun_ip);
    printf("tun_mask:%s\n", conf->tun_mask);
    printf("tcp_addr:%s\n", conf->tcp_addr);
    printf("tcp_port:%u\n", conf->tcp_port);
    printf("password:%s\n", conf->key);
    return conf;
}

int main(int argc, char const *argv[]) {
    load_conf(argv[1]);
    return 0;
}
