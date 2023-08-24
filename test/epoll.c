#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9998
#define SERVER "127.0.0.1"
#define MAXBUF 13
#define MAX_EPOLL_EVENTS 64

static uint64_t getmillisecond() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t millisecond = (tv.tv_sec * 1000000l + tv.tv_usec) / 1000l;
    return millisecond;
}

int main() {
    int sockfd;
    struct sockaddr_in dest;
    char buffer[MAXBUF];
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int i, num_ready;

    /*---Open socket for streaming---*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        perror("Socket");
        exit(errno);
    }

    /*---Add socket to epoll---*/
    int epfd = epoll_create(1);
    struct epoll_event event;
    event.events = EPOLLIN;  // Can append "|EPOLLOUT" for write events as well
    event.data.fd = sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event);

    /*---Initialize server address/port struct---*/
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER, &dest.sin_addr.s_addr) == 0) {
        perror(SERVER);
        exit(errno);
    }

    /*---Connect to server---*/
    if (connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) != 0) {
        if (errno != EINPROGRESS) {
            perror("Connect ");
            exit(errno);
        }
    }

    /*---Wait for socket connect to complete---*/
    num_ready = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 1000 /*timeout*/);
    for (i = 0; i < num_ready; i++) {
        if (events[i].events & EPOLLIN) {
            printf("Socket %d connected\n", events[i].data.fd);
        }
    }

    /*---Wait for data---*/
    int rn = 0;
    int wn = 0;
    char msg[1024] = {0};
    for (;;) {
        num_ready = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 30 /*timeout*/);
        for (i = 0; i < num_ready; i++) {
            if (events[i].events & EPOLLIN) {
                // printf("Socket %d got some data\n", events[i].data.fd);
                bzero(buffer, MAXBUF);
                rn = recv(sockfd, buffer, sizeof(buffer), 0);
                if (rn <= 0) {
                    perror("recv");
                    break;
                }

                // printf("Received: %s, len:%d\n", buffer, rn);
                char tm[32] = {0};
                int tm_len = strlen(buffer) > 31 ? 31 : strlen(buffer);
                memcpy(tm, buffer, tm_len);
                uint64_t tmi = atoll(tm);
                uint64_t now = getmillisecond();
                printf("rtt:%llu\n", now - tmi);
            }
        }

        bzero(msg, sizeof(msg));
        sprintf(msg, "%llu", getmillisecond());
        wn = write(sockfd, msg, strlen(msg));
        assert(wn >= 0);
    }

    close(sockfd);
    return 0;
}

// /*
//  * Attention:
//  * To keep things simple, do not handle socket/bind/listen/.../epoll_create/epoll_wait API error
//  */
// #include <arpa/inet.h>
// #include <assert.h>
// #include <errno.h>
// #include <fcntl.h>
// #include <netdb.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/epoll.h>
// #include <sys/socket.h>
// #include <sys/time.h>
// #include <sys/types.h>
// #include <time.h>
// #include <unistd.h>

// #define DEFAULT_PORT 9998
// #define MAX_CONN 16
// #define MAX_EVENTS 32
// #define BUF_SIZE 16
// #define MAX_LINE 256

// static uint64_t getmillisecond() {
//     struct timeval tv;
//     gettimeofday(&tv, NULL);
//     uint64_t millisecond = (tv.tv_sec * 1000000l + tv.tv_usec) / 1000l;
//     return millisecond;
// }

// void server_run();
// void client_run();

// int main(int argc, char *argv[]) {
//     // int opt;
//     // char role = 's';
//     // while ((opt = getopt(argc, argv, "cs")) != -1) {
//     //     switch (opt) {
//     //         case 'c':
//     //             role = 'c';
//     //             break;
//     //         case 's':
//     //             break;
//     //         default:
//     //             printf("usage: %s [-cs]\n", argv[0]);
//     //             exit(1);
//     //     }
//     // }
//     // if (role == 's') {
//     //     server_run();
//     // } else {
//     //     client_run();
//     // }
//     client_run();
//     return 0;
// }

// /*
//  * register events of fd to epfd
//  */
// static void epoll_ctl_add(int epfd, int fd, uint32_t events) {
//     struct epoll_event ev;
//     ev.events = events;
//     ev.data.fd = fd;
//     if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
//         perror("epoll_ctl()\n");
//         exit(1);
//     }
// }

// static void set_sockaddr(struct sockaddr_in *addr) {
//     bzero((char *)addr, sizeof(struct sockaddr_in));
//     addr->sin_family = AF_INET;
//     // addr->sin_addr.s_addr = INADDR_ANY;
//     addr->sin_addr.s_addr = inet_addr("127.0.0.1");
//     addr->sin_port = htons(DEFAULT_PORT);
// }

// static int setnonblocking(int sockfd) {
//     if (fcntl(sockfd, F_SETFD, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1) {
//         return -1;
//     }
//     return 0;
// }

// /*
//  * epoll echo server
//  */
// void server_run() {
//     int i;
//     int n;
//     int epfd;
//     int nfds;
//     int listen_sock;
//     int conn_sock;
//     int socklen;
//     char buf[BUF_SIZE];
//     struct sockaddr_in srv_addr;
//     struct sockaddr_in cli_addr;
//     struct epoll_event events[MAX_EVENTS];

//     listen_sock = socket(AF_INET, SOCK_STREAM, 0);

//     set_sockaddr(&srv_addr);
//     bind(listen_sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr));

//     setnonblocking(listen_sock);
//     listen(listen_sock, MAX_CONN);

//     epfd = epoll_create(1);
//     epoll_ctl_add(epfd, listen_sock, EPOLLIN | EPOLLOUT | EPOLLET);

//     socklen = sizeof(cli_addr);
//     for (;;) {
//         nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
//         for (i = 0; i < nfds; i++) {
//             if (events[i].data.fd == listen_sock) {
//                 /* handle new connection */
//                 conn_sock = accept(listen_sock, (struct sockaddr *)&cli_addr, &socklen);

//                 inet_ntop(AF_INET, (char *)&(cli_addr.sin_addr), buf, sizeof(cli_addr));
//                 printf("[+] connected with %s:%d\n", buf, ntohs(cli_addr.sin_port));

//                 setnonblocking(conn_sock);
//                 epoll_ctl_add(epfd, conn_sock, EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
//             } else if (events[i].events & EPOLLIN) {
//                 /* handle EPOLLIN event */
//                 for (;;) {
//                     bzero(buf, sizeof(buf));
//                     n = read(events[i].data.fd, buf, sizeof(buf));
//                     if (n <= 0 /* || errno == EAGAIN */) {
//                         break;
//                     } else {
//                         printf("[+] data: %s\n", buf);
//                         write(events[i].data.fd, buf, strlen(buf));
//                     }
//                 }
//             } else {
//                 printf("[+] unexpected\n");
//             }
//             /* check if the connection is closing */
//             if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
//                 printf("[+] connection closed\n");
//                 epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
//                 close(events[i].data.fd);
//                 continue;
//             }
//         }
//     }
// }

// /*
//  * test clinet
//  */
// void client_run() {
//     int n;
//     int c;
//     int sockfd;
//     char buf[13];
//     struct sockaddr_in srv_addr;

//     sockfd = socket(AF_INET, SOCK_STREAM, 0);

//     set_sockaddr(&srv_addr);

//     if (connect(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
//         perror("connect()");
//         exit(1);
//     }

//     int rt = 0;
//     for (;;) {
//         char msg[1024] = {0};
//         sprintf(msg, "%llu", getmillisecond());
//         rt = write(sockfd, msg, strlen(msg));
//         assert(rt >= 0);

//         while (errno != EAGAIN && (n = read(sockfd, buf, sizeof(buf))) > 0) {
//             char tm[32] = {0};
//             int tm_len = strlen(buf) > 31 ? 31 : strlen(buf);
//             memcpy(tm, buf, tm_len);
//             uint64_t tmi = atoll(tm);
//             uint64_t now = getmillisecond();
//             printf("rtt:%llu\n", now - tmi);
//         }
//         usleep(30 * 1000);

//         // printf("input: ");
//         // fgets(buf, sizeof(buf), stdin);
//         // c = strlen(buf) - 1;
//         // buf[c] = '\0';
//         // write(sockfd, buf, c + 1);

//         // bzero(buf, sizeof(buf));
//         // while (errno != EAGAIN && (n = read(sockfd, buf, sizeof(buf))) > 0) {
//         //     printf("echo: %s\n", buf);
//         //     bzero(buf, sizeof(buf));

//         //     c -= n;
//         //     if (c <= 0) {
//         //         break;
//         //     }
//         // }
//     }
//     close(sockfd);
// }