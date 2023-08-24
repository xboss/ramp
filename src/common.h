#ifndef _COMMON_H
#define _COMMON_H

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define _INFO_LOG
#define _WARN_LOG

// #define RAMP_OK 0
// #define RAMP_ERROR -1

#if !defined(_IF_NULL)
#define _IF_NULL(v) if (NULL == (v))
#endif  // _IF_NULL

#if !defined(_ALLOC)
#define _ALLOC(v_type, v_element_size) (v_type*)calloc(1, (v_element_size))
#endif  // _ALLOC

#if !defined(_FREE_IF)
#define _FREE_IF(p)     \
    do {                \
        if ((p)) {      \
            free((p));  \
            (p) = NULL; \
        }               \
    } while (0)
#endif  // _FREE_IF

#define LOG_E(fmt, args...)  \
    do {                     \
        printf("ERROR ");    \
        printf(fmt, ##args); \
        printf("\n");        \
    } while (0)

#ifdef _WARN_LOG
#define LOG_W(fmt, args...)  \
    do {                     \
        printf("WARN ");     \
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

#if !defined(_IF_STR_EMPTY)
#define _IF_STR_EMPTY(v) if ((v) != NULL && strlen((v)) > 0)
#endif  // _IF_STR_EMPTY

#define IV_LEN 32
#define KEY_LEN 32
#define TICKET_LEN 32

inline uint64_t getmillisecond() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t millisecond = (tv.tv_sec * 1000000l + tv.tv_usec) / 1000l;
    return millisecond;
}

// inline void print_now() {
//     time_t now;
//     struct tm *tm_now;
//     time(&now);
//     tm_now = localtime(&now);
//     printf("%d-%d-%d %d:%d:%d:%llu ", tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday, tm_now->tm_hour,
//            tm_now->tm_min, tm_now->tm_sec, (getmillisecond() % 1000ll));
// }

// static void setnonblock(int fd) {
//     if (-1 == fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)) {
//         LOG_E("error fcntl");
//     }
// }

// static void _PR(const void *buf, int len) {
//     const char *pb = buf;
//     for (size_t i = 0; i < len; i++) {
//         // unsigned char c = *pb;
//         printf("%2X ", ((*pb) & 0xFF));
//         pb++;
//     }
//     printf("\n");
// }

#endif  // COMMON_H