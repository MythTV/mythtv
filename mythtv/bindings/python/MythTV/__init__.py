#!/usr/bin/env python

__all_exceptions__  = ['MythError', 'MythDBError', 'MythBEError', \
                       'MythFEError', 'MythFileError']

__all_utility__     = ['schemaUpdate', 'databaseSearch', 'SplitInt',
                       'deadlinesocket']

__all_altdict__     = ['OrdDict', 'DictData', 'DictInvert', 'DictInvertCI']

__all_connections__ = ['LoggedCursor', 'DBConnection','BEConnection', \
                       'FEConnection','XMLConnection']

__all_database__    = ['DBData', 'DBDataWrite', 'DBDataWriteAI', \
                       'DBDataRef', 'DBDataCRef', 'DBCache', 'StorageGroup']

__all_system__      = ['System', 'Grabber', 'Metadata', 'VideoMetadata', \
                       'MusicMetadata', 'GameMetadata', 'InternetMetadata', \
                       'SystemEvent']

__all_proto__       = ['BECache', 'BEEvent', 'findfile', 'ftopen', \
                       'FileTransfer', 'FileOps', 'FreeSpace', 'Program']

__all_data__        = ['Record', 'Recorded', 'RecordedProgram', 'OldRecorded', \
                       'Job', 'Channel', 'Guide', 'Video', 'VideoGrabber', \
                       'InternetContent', 'InternetContentArticles', \
                       'InternetSource', 'Song', 'Album', 'Artist', \
                       'MusicPlaylist', 'MusicDirectory']

__all_method__      = ['MythBE', 'BEEventMonitor', 'MythSystemEvent', \
                       'SystemEvent', 'Frontend', 'MythDB', 'MythVideo', \
                       'MythXML', 'MythMusic']

__all__             = ['static', 'MSearch', 'MythLog']\
                        +__all_exceptions__\
                        +__all_utility__\
                        +__all_altdict__\
                        +__all_connections__\
                        +__all_database__\
                        +__all_system__\
                        +__all_proto__\
                        +__all_data__\
                        +__all_method__

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
static.mysqldb = MySQLdb.version_info

