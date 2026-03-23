#!/usr/bin/pypy3

import sys

with open(sys.argv[1], mode = 'rb') as fd:
    data = fd.read()

assert len(data) & 1 == 0

with open(sys.argv[1] + '.luma', mode = 'wb') as fd:
    fd.write(data[::2])

