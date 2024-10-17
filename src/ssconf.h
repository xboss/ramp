#ifndef _SSCONF_H
#define _SSCONF_H

typedef struct ssconf_s ssconf_t;

ssconf_t *ssconf_init(char *keys[], int cnt);
void ssconf_free(ssconf_t *conf);
int ssconf_load(ssconf_t *conf, const char *file);
char *ssconf_get_value(ssconf_t *conf, char *key);

#endif /* SSCONF_H */