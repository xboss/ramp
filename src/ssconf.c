#include "ssconf.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stream_buf.h"

#define _OK 0
#define _ERR -1

#ifndef _ALLOC
#define _ALLOC(_p, _type, _size)   \
    (_p) = (_type)malloc((_size)); \
    if (!(_p)) {                   \
        perror("alloc error");     \
        exit(1);                   \
    }
#endif

#define DEF_LINE_SIZE 128

typedef struct {
    char *key;
    char *value;
} ssconf_item_t;

struct ssconf_s {
    ssconf_item_t **items;
    int items_cnt;
};

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

static void fill_conf(ssconf_t *conf, const char *k, const char *v) {
    /* printf("k:%s v:%s\n", k, v); */
    int i;
    int vl;
    char *tv;
    for (i = 0; i < conf->items_cnt; i++) {
        if (strcmp(k, conf->items[i]->key) == 0) {
            vl = strlen(v);
            _ALLOC(tv, char *, vl + 1);
            memset(tv, 0, vl + 1);
            memcpy(tv, v, vl);
            conf->items[i]->value = tv;
        }
    }
}

static void read_kv(ssconf_t *conf, char *line) {
    /* printf("%s\n", line); */
    char *k = NULL;
    char *v = NULL;
    char *d = "=";
    if (strlen(line) == 0) return;
    char *p = trim(line);
    if (*p == '#') return;
    k = strtok(p, d);
    if (k == NULL) return;
    v = strtok(NULL, d);
    if (v == NULL) return;
    k = trim(k);
    v = trim(v);
    fill_conf(conf, k, v);
}

int ssconf_load(ssconf_t *conf, const char *file) {
    FILE *fp;
    if ((fp = fopen(file, "r")) == NULL) {
        fprintf(stderr, "can't open config file %s", file);
        return _ERR;
    }
    stream_buf_t *sb = sb_init(NULL, 0);
    char line[DEF_LINE_SIZE];
    char *p = NULL;
    int rt;
    char *tmpbuf;
    int sb_sz;
    int line_sz;
    while (fgets(line, DEF_LINE_SIZE, fp) != NULL) {
        sb_sz = sb_get_size(sb);
        tmpbuf = line;
        line_sz = DEF_LINE_SIZE;
        if (sb_sz > 0) {
            _ALLOC(tmpbuf, char *, sb_sz + DEF_LINE_SIZE);
            rt = sb_read_all(sb, tmpbuf, sb_sz + DEF_LINE_SIZE);
            assert(rt == sb_sz);
            memcpy(tmpbuf + rt, line, DEF_LINE_SIZE);
            line_sz = sb_sz + DEF_LINE_SIZE;
        }
        p = tmpbuf;
        while (*p != '\n' && p - tmpbuf < line_sz - 1) {
            p++;
        }
        if (*p != '\n') {
            rt = sb_write(sb, tmpbuf, p - tmpbuf);
            assert(rt == 0);
        } else {
            *p = '\0';
            read_kv(conf, tmpbuf);
        }
        if (sb_sz > 0) free(tmpbuf);
    }
    sb_sz = sb_get_size(sb);
    if (sb_sz > 0) {
        _ALLOC(tmpbuf, char *, sb_sz);
        rt = sb_read_all(sb, tmpbuf, sb_sz);
        assert(rt == sb_sz);
        read_kv(conf, tmpbuf);
        free(tmpbuf);
    }
    sb_free(sb);
    fclose(fp);
    return _OK;
}

char *ssconf_get_value(ssconf_t *conf, char *key) {
    if (!conf || !key) return NULL;
    int i;
    for (i = 0; i < conf->items_cnt; i++) {
        if (strcmp(key, conf->items[i]->key) == 0) return conf->items[i]->value;
    }
    return NULL;
}

ssconf_t *ssconf_init(char *keys[], int cnt) {
    if (!keys || cnt <= 0) {
        return NULL;
    }
    ssconf_t *_ALLOC(conf, ssconf_t *, sizeof(ssconf_t));
    _ALLOC(conf->items, ssconf_item_t **, sizeof(ssconf_item_t *) * cnt);
    int i;
    ssconf_item_t *item;
    for (i = 0; i < cnt; i++) {
        if (!keys[i] || strlen(keys[i]) <= 0) continue;
        _ALLOC(item, ssconf_item_t *, sizeof(ssconf_item_t));
        memset(item, 0, sizeof(ssconf_item_t));
        item->key = keys[i];
        conf->items[i] = item;
    }
    conf->items_cnt = i;
    return conf;
}

void ssconf_free(ssconf_t *conf) {
    if (!conf) return;
    if (conf->items_cnt > 0) {
        assert(conf->items);
        int i;
        for (i = 0; i < conf->items_cnt; i++) {
            if (conf->items[i]->value) free(conf->items[i]->value);
            free(conf->items[i]);
        }
        free(conf->items);
    }
    free(conf);
}

/* "mode", "listen_ip", "listen_port", "listen_ip", "listen_port", "password", "timeout", "read_buf_size" */
/* int main(int argc, char const *argv[]) {
    char *keys[] = {"mode",        "listen_ip", "listen_port", "listen_ip",
                    "listen_port", "password",  "timeout",     "read_buf_size"};
    ssconf_t *conf = ssconf_init(keys, sizeof(keys) / sizeof(char *));
    ssconf_load(conf, "./local.conf");
    int i;
    char *v;
    for (i = 0; i < sizeof(keys) / sizeof(char *); i++) {
        v = ssconf_get_value(conf, keys[i]);
        printf("%s : %s\n", keys[i], v);
    }
    ssconf_free(conf);
    return 0;
} */
