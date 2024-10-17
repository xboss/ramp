#include "cipher.h"

#include <assert.h>
#include <openssl/aes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int align_size = AES_BLOCK_SIZE;
static char *pkcs7_padding(const char *in, int in_len, int *out_len) {
    int remainder = in_len % align_size;
    int padding_size = remainder == 0 ? align_size : align_size - remainder;
    *out_len = in_len + padding_size;
    char *out = (char *)malloc(*out_len);
    if (!out) {
        perror("alloc error");
        return NULL;
    }
    memcpy(out, in, in_len);
    memset(out + in_len, padding_size, padding_size);
    return out;
}

static int pkcs7_unpadding(const char *in, int in_len) {
    char padding_size = in[in_len - 1];
    return (int)padding_size;
}

void pwd2key(char *key, int ken_len, const char *pwd, int pwd_len) {
    int i;
    int sum = 0;
    for (i = 0; i < pwd_len; i++) {
        sum += pwd[i];
    }
    int avg = sum / pwd_len;
    for (i = 0; i < ken_len; i++) {
        key[i] = pwd[i % pwd_len] ^ avg;
    }
}

char *aes_encrypt(const char *key, const char *in, int in_len, int *out_len) {
    if (!key || !in || in_len <= 0) {
        return NULL;
    }
    AES_KEY aes_key;
    if (AES_set_encrypt_key((const unsigned char *)key, 128, &aes_key) < 0) {
        return NULL;
    }
    char *out = pkcs7_padding(in, in_len, out_len);
    char *pi = out;
    char *po = out;
    int en_len = 0;
    while (en_len < *out_len) {
        AES_encrypt((unsigned char *)pi, (unsigned char *)po, &aes_key);
        pi += AES_BLOCK_SIZE;
        po += AES_BLOCK_SIZE;
        en_len += AES_BLOCK_SIZE;
    }
    return out;
}

char *aes_decrypt(const char *key, const char *in, int in_len, int *out_len) {
    if (!key || !in || in_len <= 0) {
        return NULL;
    }
    AES_KEY aes_key;
    if (AES_set_decrypt_key((const unsigned char *)key, 128, &aes_key) < 0) {
        return NULL;
    }
    char *out = malloc(in_len);
    if (!out) {
        perror("alloc error");
        return NULL;
    }
    memset(out, 0, in_len);
    char *po = out;
    int en_len = 0;
    while (en_len < in_len) {
        AES_decrypt((unsigned char *)in, (unsigned char *)po, &aes_key);
        in += AES_BLOCK_SIZE;
        po += AES_BLOCK_SIZE;
        en_len += AES_BLOCK_SIZE;
    }
    *out_len = in_len - pkcs7_unpadding(out, en_len);
    return out;
}