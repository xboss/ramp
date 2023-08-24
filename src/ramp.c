#include <ctype.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <pthread.h>

#include "client.h"
#include "common.h"
#include "server.h"
#include "string.h"

typedef struct {
    int mode;  // server:0; client:1;
    char key[KEY_LEN + 1];
    char iv[IV_LEN + 1];

    char server_ip[INET_ADDRSTRLEN + 1];
    uint16_t server_port;

} ramp_t;

static ramp_t *ctx = NULL;

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

static void fill_conf(const char *k, const char *v) {
    // "mode", "server_ip", "server_port", "password"
    int len = strlen(v);
    if (strcmp("mode", k) == 0) {
        if (strcmp(v, "client") == 0) {
            ctx->mode = CLIENT_MODE;
        } else {
            ctx->mode = SERVER_MODE;
        }
    } else if (strcmp("server_ip", k) == 0) {
        if (len <= INET_ADDRSTRLEN) {
            memcpy(ctx->server_ip, v, len);
        }
    } else if (strcmp("server_port", k) == 0) {
        ctx->server_port = (uint16_t)atoi(v);
    } else if (strcmp("password", k) == 0) {
        char_to_hex(v, KEY_LEN / 2, ctx->key);
    }
}

static bool load_conf(const char *conf_file) {
    FILE *fp;
    if ((fp = fopen(conf_file, "r")) == NULL) {
        LOG_E("can't open conf file %s", conf_file);
        return false;
    }

    char line[CONF_MAX_CHAR_PER_LINE] = {0};
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
        fill_conf(k, v);
    }
    fclose(fp);

    printf("---config---\n");
    printf("mode:%d\n", ctx->mode);
    printf("server_ip:%s\n", ctx->server_ip);
    printf("server_port:%u\n", ctx->server_port);
    printf("key:%s\n", ctx->key);

    return true;
}

/* -------------------------------------------------------------------------- */
/*                                    ramp                                    */
/* -------------------------------------------------------------------------- */

bool start_client() {
    // init
    client_t *cli = init_client(ctx->server_ip, ctx->server_port, ctx->key, ctx->iv);
    if (!cli) {
        return false;
    }

    pthread_t client_send_thd;
    if (pthread_create(&client_send_thd, NULL, client_send_routine, cli) != 0) {
        LOG_E("create client send thread error %s", strerror(errno));
        return false;
    }

    pthread_t client_recv_thd;
    if (pthread_create(&client_recv_thd, NULL, client_recv_routine, cli) != 0) {
        LOG_E("create client recv thread error %s", strerror(errno));
        return false;
    }

    if (pthread_join(client_send_thd, NULL) != 0 || pthread_join(client_recv_thd, NULL) != 0) {
        LOG_E("join thread error %s", strerror(errno));
        return false;
    }

    return true;
}

bool start_server() {
    server_t *server = init_server(ctx->server_ip, ctx->server_port, ctx->key, ctx->iv);
    if (!server) {
        return false;
    }

    run_server(server);

    return true;
}

#define _USAGE fprintf(stderr, "Usage: %s {config_file}\n", argv[0])

int main(int argc, char const *argv[]) {
    ctx = _ALLOC(ramp_t, sizeof(ramp_t));
    memcpy(ctx->iv, "912789a8907bcf123de4590abc678def", IV_LEN);  // TODO: dynamic iv

    if (argc != 2) {
        _USAGE;
        return 1;
    }

    if (!load_conf(argv[1])) {
        return 1;
    }

    if (ctx->mode == CLIENT_MODE) {
        start_client();
    } else if (ctx->mode == SERVER_MODE) {
        start_server();
    }

    return 0;
}
