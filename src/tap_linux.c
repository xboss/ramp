#if (defined(__linux__) || defined(__linux))

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>

#include "tap.h"

int tap_open(char *dev_name, int name_len) {
    struct ifreq ifr;
    int fd;
    char *clonedev = "/dev/net/tun";

    /* open the clone device */
    if ((fd = open(clonedev, O_RDWR)) < 0) {
        return -1;
    }

    /* preparation of the struct ifr, of type "struct ifreq" */
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_TAP | IFF_NO_PI; /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

    /* try to create the device */
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        close(fd);
        return -1;
    }

    snprintf(dev_name, name_len, "%s", ifr.ifr_name);
    printf("dev_name: %s\n", dev_name);

    return fd;
}

void tap_setup(char *dev_name, char *dev_ip, char *dev_mask) {
    char buf[256] = {0};
    // snprintf(buf, sizeof(buf), "ip addr add %s/24 dev %s", dev_ip, dev_name);
    // printf("run: %s\n", buf);
    // system(buf);

    // memset(buf, 0, 256);
    snprintf(buf, sizeof(buf), "ifconfig %s %s netmask %s", dev_name, dev_ip, dev_mask);
    printf("run: %s\n", buf);
    system(buf);

    // ifconfig mtu 1400
    memset(buf, 0, 256);
    snprintf(buf, sizeof(buf), "ifconfig %s mtu 1400", dev_name);
    printf("run: %s\n", buf);
    system(buf);

    memset(buf, 0, 256);
    snprintf(buf, sizeof(buf), "ifconfig %s up", dev_name);
    printf("run: %s\n", buf);
    system(buf);
}

int tap_read(int fd, char *buf, int len) { return read(fd, buf, len); }

int tap_write(int fd, char *buf, int len) { return write(fd, buf, len); }

#endif
