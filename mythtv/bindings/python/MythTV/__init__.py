#!/usr/bin/env python

__all__ = ['static', 'MSearch', 'MythLog',\
            \
           'MythError', 'MythDBError', 'MythBEError', \
           'MythFEError', 'MythFileError', \
            \
           'LoggedCursor', 'DBConnection','BEConnection', \
           'FEConnection','XMLConnection',\
            \
           'schemaUpdate', 'databaseSearch', 'SplitInt', 'deadlinesocket',\
            \
           'OrdDict', 'DictData', 'DictInvert', 'DictInvertCI',\
            \
           'DBConnection', 'BEConnection', 'FEConnection', 'XMLConnection', \
            \
           'DBData', 'DBDataWrite', 'DBDataRef', 'DBDataCRef', \
           'DBCache', 'StorageGroup', \
            \
           'System', 'Grabber', 'Metadata', 'VideoMetadata', 'MusicMetadata', \
           'GameMetadata', 'InternetMetadata', 'SystemEvent', \
            \
           'BECache', 'BEEvent', 'findfile', 'ftopen', 'FileTransfer', \
           'FileOps', 'FreeSpace', 'Program', \
            \
           'Record', 'Recorded', 'RecordedProgram', 'OldRecorded', 'Job', \
           'Channel', 'Guide', 'Video', 'VideoGrabber', 'InternetContent', \
           'InternetContentArticles', 'InternetSource', \
            \
           'MythBE', 'BEEventMonitor', 'MythSystemEvent', 'SystemEvent', \
           'Frontend', 'MythDB', 'MythVideo', 'MythXML']

import26 = """
import warnings
with warnings.catch_warnings():
    warnings.simplefilter('ignore')
    exec(importall)
"""

importall = """
import static
from exceptions import *
from logging import *
from msearch import *
from utility import *
from altdict import *
from connections import *
from database import *
from system import *
from mythproto import *
from dataheap import *
from methodheap import *
"""

from sys import version_info
if version_info >= (2, 6): # 2.6 or newer
    exec(import26)
else:
    exec(importall)

__version__ = OWN_VERSION
static.mysqldb = MySQLdb.__version__

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
