#!python

import sys

if len(sys.argv) < 2:
    raise Exception('function must be called with prefix argument')
prefix = sys.argv[1]

buff = []
with open('MythTV/static.py') as fi:
    for line in fi:
        if 'INSTALL_PREFIX' in line:
            line = "INSTALL_PREFIX = '%s'\n" % prefix
        buff.append(line)

#print ''.join(buff)
#sys.exit(0)

with open('MythTV/static.py','w') as fo:
    fo.write(''.join(buff))
