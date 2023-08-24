#ifndef _TUNTAP_H
#define _TUNTAP_H

int tuntap_open(char *dev_name, int name_len);
void tuntap_setup(char *dev_name, char *dev_ip, char *dev_mask);
int tuntap_read(int fd, char *buf, int len);
int tuntap_write(int fd, char *buf, int len);

#endif  // TUNTAP_H