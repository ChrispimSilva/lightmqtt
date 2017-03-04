#! /usr/bin/env python

import sys
import socket

ADDRESS = '127.0.0.1'
PORT = 4000

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((ADDRESS, PORT))
s.listen(1)

while True:
    conn = None
    try:
        conn, addr = s.accept()
        print('Client: %s' % (conn.getpeername(),))

        data = ""
        while True:
            b = conn.recv(1)
            if not b:
                conn.close()
                break

            data += b
            if len(data) >= 20:
                print('  CONNECT')
                data = ""
                conn.send("\x20\x02\x00\x00")
    except KeyboardInterrupt:
        if conn:
            conn.close()
        s.close()
        sys.exit(0)