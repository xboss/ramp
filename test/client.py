#!/usr/bin/python
# -*- coding: UTF-8 -*-
# 文件名：client.py

import socket               # 导入 socket 模块
import sys
import time

s = socket.socket()         # 创建 socket 对象
# host = socket.gethostname()  # 获取本地主机名
# port = 12345                # 设置端口号


def nowTime(): return int(round(time.time() * 1000))


address = ((sys.argv[1]), (int(sys.argv[2])))
s.connect(address)
while True:
    now = nowTime()
    s.send(bytes(str(now), 'ascii'))
    msg = s.recv(14)
    rtt = nowTime() - int(msg[:13])
    print(rtt)
    time.sleep(0.001)
s.close()
