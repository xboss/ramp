#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199506L
#endif

#include <net/ethernet.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cipher.h"
#include "client.h"
#include "common.h"
#include "server.h"
#include "ssconf.h"
#include "sslog.h"

/* -------------------------------------------------------------------------- */
/*                                   config                                   */
/* -------------------------------------------------------------------------- */

struct config_s {
    int mode; /* server:0; client:1; */
    char key[CIPHER_KEY_LEN + 1];
    char server_ip[INET_ADDRSTRLEN + 1];
    unsigned short server_port;
    char tuntap_ip[INET_ADDRSTRLEN + 1];
    char tuntap_mask[INET_ADDRSTRLEN + 1];
    char* log_file;
    int log_level;
};
typedef struct config_s config_t;

static int load_conf(const char* conf_file, config_t* conf) {
    char* keys[] = {"mode", "server_ip", "server_port", "tuntap_ip", "tuntap_mask", "password", "log_file", "log_level"};
    int keys_cnt = sizeof(keys) / sizeof(char*);
    ssconf_t* cf = ssconf_init(keys, keys_cnt);
    assert(cf);
    int rt = ssconf_load(cf, conf_file);
    if (rt != 0) return -1;
    conf->log_level = SSLOG_LEVEL_ERROR;
    char* v = NULL;
    int i;
    for (i = 0; i < keys_cnt; i++) {
        v = ssconf_get_value(cf, keys[i]);
        if (!v) {
            printf("'%s' does not exists in config file '%s'.\n", keys[i], conf_file);
            continue;
        }
        int len = strlen(v);
        if (strcmp("mode", keys[i]) == 0) {
            if (strcmp(v, "client") == 0) {
                conf->mode = CLIENT_MODE;
            } else if (strcmp(v, "server") == 0) {
                conf->mode = SERVER_MODE;
            } else {
                conf->mode = -1;
            }
        } else if (strcmp("server_ip", keys[i]) == 0) {
            if (len <= INET_ADDRSTRLEN) {
                memcpy(conf->server_ip, v, len);
            }
        } else if (strcmp("server_port", keys[i]) == 0) {
            conf->server_port = (unsigned short)atoi(v);
        } else if (strcmp("tuntap_ip", keys[i]) == 0) {
            if (len <= INET_ADDRSTRLEN) {
                memcpy(conf->tuntap_ip, v, len);
            }
        } else if (strcmp("tuntap_mask", keys[i]) == 0) {
            if (len <= INET_ADDRSTRLEN) {
                memcpy(conf->tuntap_mask, v, len);
            }
        } else if (strcmp("password", keys[i]) == 0) {
            pwd2key(conf->key, CIPHER_KEY_LEN, v, strlen(v));
        } else if (strcmp("log_file", keys[i]) == 0) {
            _ALLOC(conf->log_file, char*, len + 1);
            memset(conf->log_file, 0, len + 1);
            memcpy(conf->log_file, v, len);
        } else if (strcmp("log_level", keys[i]) == 0) {
            if (strcmp(v, "DEBUG") == 0) {
                conf->log_level = SSLOG_LEVEL_DEBUG;
            } else if (strcmp(v, "INFO") == 0) {
                conf->log_level = SSLOG_LEVEL_INFO;
            } else if (strcmp(v, "NOTICE") == 0) {
                conf->log_level = SSLOG_LEVEL_NOTICE;
            } else if (strcmp(v, "WARN") == 0) {
                conf->log_level = SSLOG_LEVEL_WARN;
            } else if (strcmp(v, "ERROR") == 0) {
                conf->log_level = SSLOG_LEVEL_ERROR;
            } else {
                conf->log_level = SSLOG_LEVEL_FATAL;
            }
        }
        printf("%s : %s\n", keys[i], v);
    }
    ssconf_free(cf);
    printf("------------\n");
    return 0;
}

static int check_config(config_t* conf) {
    if (conf->mode != CLIENT_MODE && conf->mode != SERVER_MODE) {
        fprintf(stderr,
                "Invalid mode:%d in configfile. local mode is 'local', remote mode "
                "is 'remote'.\n",
                conf->mode);
        return -1;
    }

    if (conf->server_port > 65535) {
        fprintf(stderr, "Invalid target_port:%u in configfile.\n", conf->server_port);
        return -1;
    }

    return 0;
}

static config_t g_conf;
client_t* g_cli = NULL;
server_t* g_serv = NULL;

/* -------------------------------------------------------------------------- */
/*                                    ramp                                    */
/* -------------------------------------------------------------------------- */

/* ---------- signal ---------- */

static void handle_exit(int sig) {
    _LOG("exit by signal %d ... ", sig);
    if (g_conf.mode == CLIENT_MODE) {
        assert(g_cli);
        g_cli->is_running = 0;
    } else if (g_conf.mode == SERVER_MODE) {
        assert(g_serv);
        g_serv->is_running = 0;
    }
}

static void signal_handler(int sn) {
    _LOG("signal_handler sig:%d", sn);
    switch (sn) {
        case SIGQUIT:
        case SIGINT:
        case SIGTERM:
            handle_exit(sn);
            break;
        default:
            break;
    }
}

static void start_client() {
    g_cli = init_client(g_conf.server_ip, g_conf.server_port, g_conf.key, g_conf.tuntap_ip, g_conf.tuntap_mask);
    if (!g_cli) {
        return;
    }

    pthread_t client_send_thd;
    if (pthread_create(&client_send_thd, NULL, client_send_routine, g_cli) != 0) {
        _LOG_E("create client send thread error %s", strerror(errno));
        return;
    }

    pthread_t client_recv_thd;
    if (pthread_create(&client_recv_thd, NULL, client_recv_routine, g_cli) != 0) {
        _LOG_E("create client recv thread error %s", strerror(errno));
        return;
    }

    if (pthread_join(client_send_thd, NULL) != 0 || pthread_join(client_recv_thd, NULL) != 0) {
        _LOG_E("join thread error %s", strerror(errno));
        return;
    }
}

static void start_server() {
    g_serv = init_server(g_conf.server_ip, g_conf.server_port, g_conf.key);
    if (!g_serv) {
        return;
    }
    run_server(g_serv);
    free_server(g_serv);
}

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <config file>\n", argv[0]);
        return 1;
    }

    /* init config */
    memset(&g_conf, 0, sizeof(config_t));
    int rt = load_conf(argv[1], &g_conf);
    if (rt != 0) return 1;
    if (check_config(&g_conf) != 0) return 1;
    rt = sslog_init(g_conf.log_file, g_conf.log_level);
    assert(rt == 0);
    if (g_conf.log_file) free(g_conf.log_file);

    /* init signal */
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;
    sigaction(SIGPIPE, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    if (g_conf.mode == CLIENT_MODE) {
        start_client();
    } else if (g_conf.mode == SERVER_MODE) {
        start_server();
    } else {
        _LOG_E("mode error %d", g_conf.mode);
        return 1;
    }

    sslog_free();
    return 0;
}
