#!python

import sys
import os

if len(sys.argv) < 2:
    raise Exception('function must be called with prefix argument')
prefix = sys.argv[1]

buff = []
path = 'build/lib/MythTV/static.py'
if not os.access(path, os.F_OK):
    raise Exception("Cannot access '%s' to update." % path)
with open(path) as fi:
    for line in fi:
        if 'INSTALL_PREFIX' in line:
            line = "INSTALL_PREFIX = '%s'\n" % prefix
        buff.append(line)

#print ''.join(buff)
#sys.exit(0)

with open(path,'w') as fo:
    fo.write(''.join(buff))
