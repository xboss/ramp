#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// #define MAX 13
// #define PORT 8080
#define SA struct sockaddr

static int sockfd = 0;
static char send_buf[128] = {0};

static uint64_t sum = 0;
static uint64_t cnt = 0;
static uint64_t avg = 0;

static uint64_t getmillisecond() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t millisecond = (tv.tv_sec * 1000000l + tv.tv_usec) / 1000l;
    return millisecond;
}

inline static int setnonblock(int fd) {
    if (-1 == fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)) {
        printf("error fcntl\n");
        return -1;
    }
    return 0;
}

inline static int setreuseaddr(int fd) {
    int reuse = 1;
    if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
        printf("error setsockopt\n");
        return -1;
    }
    return 0;
}

static void *send_thread_fn() {
    usleep(1 * 1000 * 1000);
    int rt = 0;
    for (;;) {
        if (sockfd <= 0) {
            usleep(10 * 1000);
            continue;
        }

        bzero(send_buf, sizeof(send_buf));
        sprintf(send_buf, "%llu", getmillisecond());
        rt = write(sockfd, send_buf, strlen(send_buf));
        if (rt <= 0) {
            if (errno == EAGAIN) {
                usleep(10 * 1000);
                continue;
            }

            // printf("tcp_write:%d\n", errno);
            perror("tcp_write");
            break;
            // continue;
        }

        assert(rt >= 0);
        usleep(10 * 1000);
    }
    return NULL;
}

void recv_fn(int sockfd) {
    // char buff[MAX];
    // int n;
    char msg[1024] = {0};
    char buff[13] = {0};
    char tm[32] = {0};
    int rt = 0;
    int tm_len = 0;
    uint64_t tmi = 0;
    uint64_t now = 0;
    for (;;) {
        // bzero(msg, 1024);
        // sprintf(msg, "%llu", getmillisecond());
        // rt = write(sockfd, msg, strlen(msg));
        // // printf("msg:%s len:%d\n", msg, rt);
        // assert(rt >= 0);

        bzero(buff, sizeof(buff));
        // usleep(1000 * 1000);
        rt = read(sockfd, buff, sizeof(buff));
        if (rt <= 0) {
            if (errno == EAGAIN) {
                usleep(10 * 1000);
                continue;
            }

            // printf("tcp_write:%d\n", errno);
            perror("read");
            break;
            // continue;
        }
        // assert(rt >= 0);
        // printf("buff:%s len:%d\n", buff, rt);
        bzero(tm, sizeof(tm));
        int tm_len = strlen(buff) > 31 ? 31 : strlen(buff);
        memcpy(tm, buff, tm_len);
        uint64_t tmi = atoll(tm);
        uint64_t now = getmillisecond();
        uint64_t rtt = now - tmi;
        cnt++;
        sum += rtt;
        if (cnt > 0) {
            avg = sum / cnt;
        }

        printf("rtt:%llu avg:%llu\n", rtt, avg);

        // usleep(1 * 1000);
    }
}

int main(int argc, char const *argv[]) {
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    } else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    setnonblock(sockfd);
    setreuseaddr(sockfd);

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    servaddr.sin_port = htons(atoi(argv[2]));

    // connect the client socket to server socket
    int rt = connect(sockfd, (SA *)&servaddr, sizeof(servaddr));
    if (0 != rt) {
        if (errno != EINPROGRESS) {
            // 连接失败
            printf("client_connect error fd:%d %s \n", sockfd, strerror(errno));
            // error
            return -1;
        } else {
            // TODO:  连接没有立即成功，需进行二次判断
            // _LOG("client_connect waiting fd:%d %s ", fd, strerror(errno));
            // pending
        }
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, send_thread_fn, NULL)) {  // TODO:  free it
        printf("start send thread error\n");
        return -1;
    }

    recv_fn(sockfd);

    // close the socket
    close(sockfd);
}
