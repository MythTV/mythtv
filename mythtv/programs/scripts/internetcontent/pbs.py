#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------
# Name: pbs.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform a mashup of various PBS Internet media sources
#   for the MythTV Netvision plugin. It follows the MythTV Netvision grabber standards.
#
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Read "~/.mythtv/MythNetvision/userGrabberPrefs/pbs.xml" configuration file
#   2) Input the sources that are marked as enabled
#   3) Process the results and display to stdout
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="PBS";
__mashup_title__ = "pbs"
__author__="R.D. Vaughan"
__version__="v0.10"
# 0.10  Initial development

__usage_examples__ ='''
(Option Help)
> ./pbs.py -h
Usage: ./pbs.py -hduvlST [parameters] <search text>
Version: v0.1.0 Author: R.D.Vaughan

For details on the MythTV Netvision plugin see the wiki page at:
http://www.mythtv.org/wiki/MythNetvision

Options:
  -h, --help            show this help message and exit
  -d, --debug           Show debugging info (URLs, raw XML ... etc, info
                        varies per grabber)
  -u, --usage           Display examples for executing the script
  -v, --version         Display grabber name and supported options
  -l LANGUAGE, --language=LANGUAGE
                        Select data that matches the specified language fall
                        back to English if nothing found (e.g. 'es' Espa√±ol,
                        'de' Deutsch ... etc). Not all sites or grabbers
                        support this option.
  -p PAGE NUMBER, --pagenumber=PAGE NUMBER
                        Display specific page of the search results. Default
                        is page 1. Page number is ignored with the Tree View
                        option (-T).
  -S, --search          Search for videos
  -T, --treeview        Display a Tree View of a sites videos

> ./pbs.py -v
<grabber>
  <name>PBS</name>
  <author>R.D. Vaughan</author>
  <thumbnail>pbs.png</thumbnail>
  <command>pbs.py</command>
  <type>video</type>
  <description>Discover award-winning programming right at your fingertips on PBS Video. Catch the episodes you may have missed and watch your favorite shows whenever you want.</description>
  <version>v0.10</version>
  <search>true</search>
  <tree>true</tree>
</grabber>

> ./pbs.py -S "Dinosaurs"

> ./pbs.py -T
'''
__search_max_page_items__ = 20
__tree_max_page_items__ = 20

import sys, os
import io

class OutStreamEncoder(object):
    """Wraps a stream with an encoder"""
    def __init__(self, outstream, encoding=None):
        self.out = outstream
        if not encoding:
            self.encoding = sys.getfilesystemencoding()
        else:
            self.encoding = encoding

    def write(self, obj):
        """Wraps the output stream, encoding Unicode strings with the specified encoding"""
        if isinstance(obj, str):
            obj = obj.encode(self.encoding)
        self.out.buffer.write(obj)

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)

if isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
    sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')


# Used for debugging
#import nv_python_libs.common.common_api
try:
    '''Import the common python class
    '''
    import nv_python_libs.common.common_api as common_api
except Exception as e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/common" containing the modules mashups_api.py and
mashups_exceptions.py (v0.1.3 or greater),
They should have been included with the distribution of MythNetvision
Error(%s)
''' % e)
    sys.exit(1)

if common_api.__version__ < '0.1.3':
    sys.stderr.write("\n! Error: Your current installed common_api.py version is (%s)\nYou must at least have version (0.1.3) or higher.\n" % target.__version__)
    sys.exit(1)

# Used for debugging
#import nv_python_libs.pbs.pbs_api as target
try:
    '''Import the python mashups support classes
    '''
    import nv_python_libs.pbs.pbs_api as target
except Exception as e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/pbs" containing the modules pbs_api and
pbs_exceptions.py (v0.1.0 or greater),
They should have been included with the distribution of pbs.py.
Error(%s)
''' % e)
    sys.exit(1)
if target.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed pbs_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % target.__version__)
    sys.exit(1)

# Verify that the main process modules are installed and accessible
try:
    import nv_python_libs.mainProcess as process
except Exception as e:
    sys.stderr.write('''
The python script "nv_python_libs/mainProcess.py" must be present.
Error(%s)
''' % e)
    sys.exit(1)

if process.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed mainProcess.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % process.__version__)
    sys.exit(1)

if __name__ == '__main__':
    # No api key is required
    apikey = ""
    # Set the base processing directory that the grabber is installed
    target.baseProcessingDir = os.path.dirname( os.path.realpath( __file__ ))
    # Make sure the target functions have an instance of the common routines
    target.common = common_api.Common()
    main = process.mainProcess(target, apikey, )
    main.grabberInfo = {}
    main.grabberInfo['enabled'] = True
    main.grabberInfo['title'] = __title__
    main.grabberInfo['command'] = 'pbs.py'
    main.grabberInfo['mashup_title'] = __mashup_title__
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'pbs.png'
    main.grabberInfo['type'] = ['video', ]
    main.grabberInfo['desc'] = "Discover award-winning programming right at your fingertips on PBS Video. Catch the episodes you may have missed and watch your favorite shows whenever you want."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = target.common.checkIfDBItem('dummy', {'feedtitle': __title__, })
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
