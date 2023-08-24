#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <ev.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define _LOG(fmt, args...)   \
    do {                     \
        printf(fmt, ##args); \
        printf("\n");        \
    } while (0)

static struct ev_timer *send_watcher = NULL;
static int fd = 0;
static char addr[64] = "127.0.0.1";
static uint16_t port = 8888;
static char read_buf[13] = {0};
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
        _LOG("error fcntl");
        return -1;
    }
    return 0;
}

inline static int setreuseaddr(int fd) {
    int reuse = 1;
    if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
        _LOG("error setsockopt");
        return -1;
    }
    return 0;
}

inline static void set_recv_timeout(int fd, time_t sec) {
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        _LOG("set_recv_timeout error");
    }
}

inline static void set_send_timeout(int fd, time_t sec) {
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        _LOG("set_recv_timeout error");
    }
}

// return: >0:success fd
static int client_connect(struct sockaddr_in servaddr, long recv_timeout, long send_timeout) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == fd) {
        _LOG("client_connect socket error fd: %d", fd);
        // error
        return -1;
    }

    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (int[]){1}, sizeof(int));
    // 设置非阻塞, 设置立即释放端口并可以再次使用
    setnonblock(fd);
    setreuseaddr(fd);
    // 设置超时
    set_recv_timeout(fd, recv_timeout);
    set_send_timeout(fd, send_timeout);

    int rt = connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (0 != rt) {
        if (errno != EINPROGRESS) {
            // 连接失败
            _LOG("client_connect error fd:%d %s ", fd, strerror(errno));
            // error
            return -1;
        } else {
            // TODO:  连接没有立即成功，需进行二次判断
            // _LOG("client_connect waiting fd:%d %s ", fd, strerror(errno));
            // pending
        }
    }

    // _LOG("client_connect ok fd: %d", fd);

    return fd;
}

// return: >0:success read bytes; 0:close; -1:pending; -2:error
static int tcp_read(int fd, char *buf, int len) {
    if (fd <= 0 || !buf || len <= 0) {
        return -2;
    }

    int rt = 0;
    ssize_t bytes = read(fd, buf, len);

    if (bytes == 0) {
        // tcp close
        rt = 0;
    } else if (bytes == -1) {
        if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno) {
            // pending
            rt = -1;
        }
        // error
        rt = -2;
    } else if (bytes < -1) {
        // error
        rt = -2;
    } else {
        rt = bytes;
    }

    return rt;
}

// return: >0:success write bytes; 0:close; -1:pending; -2:error
static int tcp_write(int fd, char *buf, int len) {
    if (fd <= 0 || !buf || len <= 0) {
        return -2;
    }

    int rt = 0;
    ssize_t bytes = write(fd, buf, len);
    if (bytes == 0) {
        // tcp close
        rt = 0;
    } else if (bytes == -1) {
        if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno) {
            // pending
            rt = -1;
        }
        // error
        rt = -2;
    } else if (bytes < -1) {
        // error
        rt = -2;
    } else {
        rt = bytes;
    }

    return rt;
}

/* -------------------------------- callback -------------------------------- */
static void cli_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    if (EV_ERROR & revents) {
        _LOG("conn_read_cb error event fd: %d", watcher->fd);
        return;
    }

    bzero(read_buf, sizeof(read_buf));
    int rt = tcp_read(fd, read_buf, sizeof(read_buf));
    assert(rt >= 0);

    char tm[32] = {0};
    int tm_len = strlen(read_buf) > 31 ? 31 : strlen(read_buf);
    memcpy(tm, read_buf, tm_len);
    uint64_t tmi = atoll(tm);
    uint64_t now = getmillisecond();
    uint64_t rtt = now - tmi;
    cnt++;
    sum += rtt;
    if (cnt > 0) {
        avg = sum / cnt;
    }

    _LOG("rtt:%llu avg:%llu", rtt, avg);
}

static void send_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents) {
    if (EV_ERROR & revents) {
        _LOG("send_cb got invalid event");
        return;
    }
    // if (cli_fd <= 0) {
    //     cli_fd = etcp_client_create_conn(cli, addr, port, NULL);
    //     assert(cli_fd > 0);
    //     // return;
    //     // ev_sleep(10);
    // }

    bzero(send_buf, sizeof(send_buf));
    sprintf(send_buf, "%llu", getmillisecond());
    int rt = tcp_write(fd, send_buf, strlen(send_buf));
    assert(rt >= 0);
    // _LOG("msg: %s", msg);
}

static void *send_thread_fn() {
    usleep(1 * 1000 * 1000);
    for (;;) {
        if (fd <= 0) {
            usleep(10 * 1000);
            continue;
        }

        bzero(send_buf, sizeof(send_buf));
        sprintf(send_buf, "%llu", getmillisecond());
        int rt = tcp_write(fd, send_buf, strlen(send_buf));
        if (rt <= 0) {
            perror("tcp_write");
            break;
        }

        assert(rt >= 0);
        usleep(10 * 1000);
    }
    return NULL;
}

/* -------------------------------------------------------------------------- */
/*                                    main                                    */
/* -------------------------------------------------------------------------- */
int main(int argc, char const *argv[]) {
    _LOG("test start...");

#if (defined(__linux__) || defined(__linux))
    struct ev_loop *loop = ev_loop_new(EVBACKEND_EPOLL);
#elif defined(__APPLE__)
    struct ev_loop *loop = ev_loop_new(EVBACKEND_KQUEUE);
#else
    struct ev_loop *loop = ev_default_loop(0);
#endif

    if (argc == 3) {
        if (argv[1]) {
            memcpy(addr, argv[1], strlen(argv[1]));
            addr[strlen(argv[1])] = '\0';
        }
        if (argv[2]) {
            port = atoi(argv[2]);
        }
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, addr, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    fd = client_connect(servaddr, 5, 5);
    if (fd <= 0) {
        _LOG("connect error addr: %s port: %u fd: %d", addr, port, fd);
        return -1;
    }

    // init watchers
    struct ev_io *r_watcher = (struct ev_io *)malloc(sizeof(struct ev_io));
    ev_io_init(r_watcher, cli_read_cb, fd, EV_READ);
    ev_io_start(loop, r_watcher);

    send_watcher = malloc(sizeof(ev_timer));
    ev_init(send_watcher, send_cb);
    ev_timer_set(send_watcher, 1, 0.01);
    ev_timer_start(loop, send_watcher);

    // pthread_t tid;
    // if (pthread_create(&tid, NULL, send_thread_fn, NULL)) {  // TODO:  free it
    //     _LOG("start send thread error");
    //     return -1;
    // }

    ev_run(loop, 0);

    close(fd);

    _LOG("test end...");
    return 0;
}
