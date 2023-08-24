#!/usr/bin/python3
# usage python3 echoTcpServer.py [bind IP]  [bind PORT]

import socket
import sys
import string
import random
import time
import datetime

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# Bind the socket to the port
server_address = ((sys.argv[1]), (int(sys.argv[2])))
sock.bind(server_address)
# Listen for incoming connections
sock.listen(1)


def nowTime(): return int(round(time.time() * 1000))


while True:
    # Wait for a connection
    print('waiting for a connection')
    connection, client_address = sock.accept()
    try:
        print('connection from', client_address)

        # Receive the data in small chunks and retransmit it
        while True:
            msg = connection.recv(14)
            connection.send(msg)
            # msg = connection.recv(14)
            # now = nowTime()
            # rtt = now - int(msg[:13])
            # print(int(msg[:13]), now, rtt)
            # connection.send(bytes(str(rtt), 'ascii'))

    finally:
        # Clean up the connection
        connection.close()
