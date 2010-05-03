#!/usr/bin/env python

__all__ = ['MythStatic', \
            \
           'DictData', 'QuickDictData', 'DBData', 'DBDataWrite', \
           'DBDataRef', 'DBDataCRef', 'DBConnection', 'DBCache', \
           'BEConnection', 'BECache', 'BEEvent', 'XMLConnection', \
           'MythLog', 'MythError', 'StorageGroup', 'System', \
            \
           'ftopen', 'FileTransfer', 'FreeSpace', 'Program', 'Record', \
           'Recorded', 'RecordedProgram', 'OldRecorded', 'Job', 'Channel', \
           'Guide', 'Video', 'VideoGrabber', 'NetVisionRSSItem', \
           'NetVisionTreeItem', 'NetVisionSite', 'NetVisionGrabber', \
            \
           'MythBE', 'BEEventMonitor', 'MythSystemEvent', 'SystemEvent', \
           'Frontend', 'MythDB', 'MythVideo', 'MythXML']

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
else:
    exec(import25)

__version__ = OWN_VERSION

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
