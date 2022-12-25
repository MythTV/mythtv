
__all_exceptions__  = ['MythError', 'MythDBError', 'MythBEError', \
                       'MythFEError', 'MythFileError', 'MythTZError']

__all_utility__     = ['SchemaUpdate', 'databaseSearch']

__all_system__      = ['System', 'Grabber', 'Metadata', 'VideoMetadata', \
                       'MusicMetadata', 'GameMetadata', 'InternetMetadata', \
                       'SystemEvent']

__all_proto__       = ['findfile', 'ftopen', 'FreeSpace', 'Program']

__all_data__        = ['Record', 'Recorded', 'RecordedProgram', 'OldRecorded', \
                       'Job', 'Channel', 'Guide', 'Video', 'VideoGrabber', \
                       'InternetContent', 'InternetContentArticles', \
                       'InternetSource', 'Song', 'Album', 'Artist', \
                       'MusicPlaylist', 'MusicDirectory', 'RecordedArtwork', \
                       'RecordedFile']

__all_method__      = ['MythBE', 'BEEventMonitor', 'MythSystemEvent', \
                       'Frontend', 'MythDB', 'MythXML', 'MythMusic', \
                       'MythVideo']

__all_service__     = ['MythTVService']

__all__             = ['static', 'MSearch', 'MythLog', 'StorageGroup']\
                        +__all_exceptions__\
                        +__all_utility__\
                        +__all_system__\
                        +__all_proto__\
                        +__all_data__\
                        +__all_method__\
                        +__all_service__

import sys
from . import static
from .exceptions import *
from .logging import *
from .msearch import *
from .utility import *
from .connections import dbmodule
from .database import *
from .system import *
from .mythproto import *
from .dataheap import *
from .methodheap import *
if sys.version_info[0] > 2:
    from .mythservices import *
else:
   import warnings      
   warnings.warn("MythTV deprecates the usage of Python2 "
                 "and will remove support in future releases.",
                 DeprecationWarning)

__version__ = OWN_VERSION
static.dbmodule = dbmodule.__version__

