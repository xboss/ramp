#ifndef _TAP_H
#define _TAP_H

int tap_open(char *dev_name, int name_len);
// void tap_setup(char *dev_name, char *dev_ip, char *dev_mask);
int tap_read(int fd, char *buf, int len);
int tap_write(int fd, char *buf, int len);

#endif  // TAP_H