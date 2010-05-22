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
__title__ ="The WB|ST";
__mashup_title__ = "thewb"
__author__="R.D.Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__usage_examples__ ='''
(Option Help)
> ./thewb.py -h

> ./thewb.py -v
The WB|ST

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
#import nv_python_libs.common.mashups_api

try:
    '''Import the common python class
    '''
    import nv_python_libs.common.mashups_api as mashups_api
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/common" containing the modules mashups_api.py and
mashups_exceptions.py (v0.1.3 or greater),
They should have been included with the distribution of MythNetvision
Error(%s)
''' % e)
    sys.exit(1)

if mashups_api.__version__ < '0.1.3':
    sys.stderr.write("\n! Error: Your current installed mashups_api.py version is (%s)\nYou must at least have version (0.1.3) or higher.\n" % target.__version__)
    sys.exit(1)

# Check if this grabber is processed remotely on a CGI Web server or on a local Frontend
common = mashups_api.Common()
try:
  common.getFEConfig()
except Exception, e:
  sys.stderr.write(u'%s\n' % e)
  pass
if common.local:
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
else:
  target = mashups_api    # This grabber is to be run on a CGI enabled Web server

# Verify that the main process modules are installed and accessable
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
    target.common = common
    main = process.mainProcess(target, apikey, )
    main.grabber_title = __title__
    main.mashup_title = __mashup_title__
    main.grabber_author = __author__
    main.grabber_version = __version__
    main.grabber_usage_examples = __usage_examples__
    main.search_max_page_items = __search_max_page_items__
    main.tree_max_page_items = __tree_max_page_items__
    main.main()
