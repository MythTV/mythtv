#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: thewb.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform The WB video lookups for the MythTV Netvision plugin
#   based on information found on the http://www.thewb.com/ website. It
#   follows the MythTV Netvision grabber standards.
#
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Read the ".../emml/feConfig.xml"
#   2) Check if the CGI Web server should be used or if the script is run locally
#   3) Initialize the correct target functions for processing (local or remote)
#   4) Process the search or treeview request and display to stdout
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="The WB";
__mashup_title__ = "thewb"
__author__="R.D. Vaughan"
__version__="0.13"
# 0.1.0 Initial development
# 0.11  Change to support xml version information display
# 0.12  Added the "command" tag to the xml version information display
# 0.13  Converted to new common_api.py library

__usage_examples__ ='''
(Option Help)
> ./thewb.py -h
Usage: ./thewb.py -hduvlST [parameters] <search text>
Version: v0.11 Author: R.D.Vaughan

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
                        back to English if nothing found (e.g. 'es' Español,
                        'de' Deutsch ... etc). Not all sites or grabbers
                        support this option.
  -p PAGE NUMBER, --pagenumber=PAGE NUMBER
                        Display specific page of the search results. Default
                        is page 1. Page number is ignored with the Tree View
                        option (-T).
  -S, --search          Search for videos
  -T, --treeview        Display a Tree View of a sites videos

> ./thewb.py -v
<grabber>
  <name>The WB</name>
  <author>R.D.Vaughan</author>
  <thumbnail>thewb.png</thumbnail>
  <type>video</type>
  <description>Watch full episodes of your favorite shows on The WB.com, like Friends, The O.C., Veronica Mars, Pushing Daisies, Smallville, Buffy The Vampire Slayer, One Tree Hill and Gilmore Girls.</description>
  <version>v0.11</version>
  <search>true</search>
  <tree>true</tree>
</grabber>

> ./thewb.py -S "Firefly"

> ./thewb.py -T
'''
__search_max_page_items__ = 20
__tree_max_page_items__ = 20

import sys, os


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
        if isinstance(obj, unicode):
            try:
                self.out.write(obj.encode(self.encoding))
            except IOError:
                pass
        else:
            try:
                self.out.write(obj)
            except IOError:
                pass

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)
sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')


# Used for debugging
#import nv_python_libs.common.common_api
try:
    '''Import the common python class
    '''
    import nv_python_libs.common.common_api as common_api
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/common" containing the modules common_api.py and
common_exceptions.py (v0.1.3 or greater),
They should have been included with the distribution of MythNetvision
Error(%s)
''' % e)
    sys.exit(1)
if common_api.__version__ < '0.1.3':
    sys.stderr.write("\n! Error: Your current installed common_api.py version is (%s)\nYou must at least have version (0.1.3) or higher.\n" % target.__version__)
    sys.exit(1)

# Used for debugging
#import nv_python_libs.thewb.thewb_api as target
try:
    '''Import the python thewb support classes
    '''
    import nv_python_libs.thewb.thewb_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/thewb" containing the modules thewb_api and
thewb_exceptions.py (v0.1.0 or greater),
They should have been included with the distribution of thewb.py.
Error(%s)
''' % e)
    sys.exit(1)
if target.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed thewb_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % target.__version__)
    sys.exit(1)

# Verify that the main process modules are installed and accessible
try:
    import nv_python_libs.mainProcess as process
except Exception, e:
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
    main.grabberInfo['title'] = __title__
    main.grabberInfo['command'] = u'thewb.py'
    main.grabberInfo['mashup_title'] = __mashup_title__
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'thewb.png'
    main.grabberInfo['type'] = ['video', ]
    main.grabberInfo['desc'] = u"Watch full episodes of your favorite shows on The WB.com, like Friends, The O.C., Veronica Mars, Pushing Daisies, Smallville, Buffy The Vampire Slayer, One Tree Hill and Gilmore Girls."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
