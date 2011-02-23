#!/usr/bin/env python

__all__ = ['MythStatic', \
            \
           'DictData', 'DBData', 'DBDataWrite', 'DBDataCRef', 'MythDBConn', \
           'MythBEConn', 'MythXMLConn', 'MythLog', 'MythError', \
           'StorageGroup', 'Grabber', \
            \
           'ftopen', 'FileTransfer', 'FreeSpace', 'Program', 'Record', \
           'Recorded', 'RecordedProgram', 'OldRecorded', 'Job', 'Channel', \
           'Guide', 'Video', 'VideoGrabber', 'NetVisionRSSItem', \
           'NetVisionTreeItem', 'NetVisionSite', 'NetVisionGrabber', \
            \
           'MythBE', 'Frontend', 'MythDB', 'MythVideo', 'MythXML']

import26 = """
import warnings
with warnings.catch_warnings():
    warnings.simplefilter('ignore')
    from MythStatic import *
    from MythBase import *
    from MythData import *
    from MythFunc import *
"""

import25 = """
from MythStatic import *
from MythBase import *
from MythData import *
from MythFunc import *
"""

from sys import version_info
if version_info >= (2, 6): # 2.6 or newer
    exec(import26)
elif version_info >= (2, 5):
    exec(import25)
else:
    raise Exception("The MythTV Python bindings will only operate against Python 2.5 or later.")

__version__ = OWN_VERSION
MythStatic.mysqldb = MySQLdb.__version__

if __name__ == '__main__':
    banner = 'MythTV Python interactive shell.'
    import code
    try:
        import readline, rlcompleter
    except:
        pass
    else:
        readline.parse_and_bind("tab: complete")
        banner += ' TAB completion available.'
    namespace = globals().copy()
    namespace.update(locals())
    code.InteractiveConsole(namespace).interact(banner)
