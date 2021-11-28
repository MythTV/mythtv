# -*- coding: utf-8 -*-

import sys, os
import setuptools

run_post_build_action = False
index = 0
mythtv_prefix = None

# handle extra build option for 'mythtv-prefix'
if sys.argv[1] == 'build':
    for index,arg in enumerate(sys.argv):
        if arg.startswith("--mythtv-prefix="):
            run_post_build_action = True
            try:
                mythtv_prefix = arg.split("=")[1].strip()
            except IndexError:
                pass
            break

    # 'mythtv-prefix' argument is not meant for setup()
    if run_post_build_action:
        del sys.argv[index]

# run setup() from setuptools with expected arguments
setuptools.setup()

# adapt the path to the grabber scripts according installation
if run_post_build_action:
    # check for alternate prefix
    if mythtv_prefix is None or mythtv_prefix == '':
        sys.exit(0)

    # find file
    for path,dirs,files in os.walk('build'):
        if 'static.py' in files:
            break
    else:
        sys.exit(0)

    # read in and replace existing line
    buff = []
    path = os.path.join(path,'static.py')
    with open(path) as fi:
        for line in fi:
            if 'INSTALL_PREFIX' in line:
                line = "INSTALL_PREFIX = '%s'\n" % mythtv_prefix
            buff.append(line)

    # push back to file
    with open(path,'w') as fo:
        fo.write(''.join(buff))
