#include <arpa/inet.h>  // inet_addr()
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // bzero()
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>  // read(), write(), close()

// #define MAX 13
// #define PORT 8080
#define SA struct sockaddr

static uint64_t getmillisecond() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t millisecond = (tv.tv_sec * 1000000l + tv.tv_usec) / 1000l;
    return millisecond;
}

void func(int sockfd) {
    // char buff[MAX];
    // int n;
    char msg[1024] = {0};
    char buff[13] = {0};
    int rt = 0;
    for (;;) {
        bzero(msg, 1024);
        sprintf(msg, "%llu", getmillisecond());
        rt = write(sockfd, msg, strlen(msg));
        // printf("msg:%s len:%d\n", msg, rt);
        assert(rt >= 0);
        bzero(buff, sizeof(buff));
        // usleep(1000 * 1000);
        rt = read(sockfd, buff, sizeof(buff));
        assert(rt >= 0);
        // printf("buff:%s len:%d\n", buff, rt);

        char tm[32] = {0};
        int tm_len = strlen(buff) > 31 ? 31 : strlen(buff);
        memcpy(tm, buff, tm_len);
        uint64_t tmi = atoll(tm);
        uint64_t now = getmillisecond();
        printf("rtt:%llu\n", now - tmi);
        // if ((strncmp(buff, "exit", 4)) == 0) {
        //     printf("Client Exit...\n");
        //     break;
        // }
        usleep(1 * 1000);
    }
}

int main(int argc, char const *argv[]) {
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    } else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    servaddr.sin_port = htons(atoi(argv[2]));

    // connect the client socket to server socket
    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    } else
        printf("connected to the server..\n");

    // function for chat
    func(sockfd);

    // close the socket
    close(sockfd);
}
