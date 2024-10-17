#ifndef _COMMON_H
#define _COMMON_H

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

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

#define SERVER_MODE 0
#define CLIENT_MODE 1

#endif  // COMMON_H