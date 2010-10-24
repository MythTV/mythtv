#!python

from __future__ import with_statement

import sys
import os

if len(sys.argv) < 2:
    raise Exception('function must be called with prefix argument')
prefix = sys.argv[1]

buff = []
for path,dirs,files in os.walk('build'):
    if 'static.py' in files:
        break
else:
    raise Exception("Cannot find temporary build file for 'static.py'.")

path = os.path.join(path,'static.py')
with open(path) as fi:
    for line in fi:
        if 'INSTALL_PREFIX' in line:
            line = "INSTALL_PREFIX = '%s'\n" % prefix
        buff.append(line)

with open(path,'w') as fo:
    fo.write(''.join(buff))
