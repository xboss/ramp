#include "cipher.h"

#include <assert.h>
#include <openssl/aes.h>
#include <string.h>

#include "common.h"

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
    _FREE_IF(skey);
    _FREE_IF(siv);
}
inline static void aes_cbc_decrypt(const char *raw_buf, char **encrpy_buf, int len, const char *key, const char *iv) {
    AES_KEY aes_key;
    unsigned char *skey = str2hex(key);
    unsigned char *siv = str2hex(iv);
    AES_set_decrypt_key(skey, 128, &aes_key);
    AES_cbc_encrypt((unsigned char *)raw_buf, (unsigned char *)*encrpy_buf, len, &aes_key, siv, AES_DECRYPT);
    _FREE_IF(skey);
    _FREE_IF(siv);
}

char *aes_encrypt(const char *key, const char *iv, const char *in, int in_len, int *out_len) {
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
        _FREE_IF(after_padding_buf);
    }
    return out_buf;
}

char *aes_decrypt(const char *key, const char *iv, const char *in, int in_len, int *out_len) {
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
        _FREE_IF(after_padding_buf);
    }
    return out_buf;
}