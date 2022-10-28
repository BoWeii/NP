#!/usr/bin/env python3

with open('./testcase2', 'wb+') as f:
    f.write(b'ls ')
    for _ in range(1500):
        f.write(b'| cat ')
    f.write(b'|1')
    f.write(b'cat \n')
    f.write(b'exit')
