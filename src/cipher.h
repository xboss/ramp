#ifndef _CIPHER_H
#define _CIPHER_H

char *aes_encrypt(const char *key, const char *iv, const char *in, int in_len, int *out_len);
char *aes_decrypt(const char *key, const char *iv, const char *in, int in_len, int *out_len);

#endif  // CIPHER_H