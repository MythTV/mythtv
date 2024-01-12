# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**Plugin functions on Kodi.**

Offers classes and functions that allow a developer to present information
through Kodi's standard menu structure. While plugins don't have the same
flexibility as scripts, they boast significantly quicker development time and a
more consistent user experience.
"""
from typing import List, Tuple, Optional

__kodistubs__ = True

SORT_METHOD_ALBUM = 14
SORT_METHOD_ALBUM_IGNORE_THE = 15
SORT_METHOD_ARTIST = 11
SORT_METHOD_ARTIST_IGNORE_THE = 13
SORT_METHOD_BITRATE = 43
SORT_METHOD_CHANNEL = 41
SORT_METHOD_COUNTRY = 17
SORT_METHOD_DATE = 3
SORT_METHOD_DATEADDED = 21
SORT_METHOD_DATE_TAKEN = 44
SORT_METHOD_DRIVE_TYPE = 6
SORT_METHOD_DURATION = 8
SORT_METHOD_EPISODE = 24
SORT_METHOD_FILE = 5
SORT_METHOD_FULLPATH = 35
SORT_METHOD_GENRE = 16
SORT_METHOD_LABEL = 1
SORT_METHOD_LABEL_IGNORE_FOLDERS = 36
SORT_METHOD_LABEL_IGNORE_THE = 2
SORT_METHOD_LASTPLAYED = 37
SORT_METHOD_LISTENERS = 39
SORT_METHOD_MPAA_RATING = 31
SORT_METHOD_NONE = 0
SORT_METHOD_PLAYCOUNT = 38
SORT_METHOD_PLAYLIST_ORDER = 23
SORT_METHOD_PRODUCTIONCODE = 28
SORT_METHOD_PROGRAM_COUNT = 22
SORT_METHOD_SIZE = 4
SORT_METHOD_SONG_RATING = 29
SORT_METHOD_SONG_USER_RATING = 30
SORT_METHOD_STUDIO = 33
SORT_METHOD_STUDIO_IGNORE_THE = 34
SORT_METHOD_TITLE = 9
SORT_METHOD_TITLE_IGNORE_THE = 10
SORT_METHOD_TRACKNUM = 7
SORT_METHOD_UNSORTED = 40
SORT_METHOD_VIDEO_ORIGINAL_TITLE = 49
SORT_METHOD_VIDEO_ORIGINAL_TITLE_IGNORE_THE = 50
SORT_METHOD_VIDEO_RATING = 19
SORT_METHOD_VIDEO_RUNTIME = 32
SORT_METHOD_VIDEO_SORT_TITLE = 26
SORT_METHOD_VIDEO_SORT_TITLE_IGNORE_THE = 27
SORT_METHOD_VIDEO_TITLE = 25
SORT_METHOD_VIDEO_USER_RATING = 20
SORT_METHOD_VIDEO_YEAR = 18


def addDirectoryItem(handle: int,
                     url: str,
                     listitem: 'xbmcgui.ListItem',
                     isFolder: bool = False,
                     totalItems: int = 0) -> bool:
    """
    Callback function to pass directory contents back to Kodi.

    :param handle: integer - handle the plugin was started with.
    :param url: string - url of the entry. would be ``plugin://`` for another virtual
        directory
    :param listitem: ListItem - item to add.
    :param isFolder: [opt] bool - True=folder / False=not a folder(default).
    :param totalItems: [opt] integer - total number of items that will be passed.(used for
        progressbar)
    :return: Returns a bool for successful completion.

    .. note::
        You can use the above as keywords for arguments and skip certain
        optional arguments. Once you use a keyword, all following
        arguments require the keyword.

    Example::

        ..
        if not xbmcplugin.addDirectoryItem(int(sys.argv[1]), 'F:\\Trailers\\300.mov', listitem, totalItems=50): break
        ..
    """
    return True


def addDirectoryItems(handle: int,
                      items: List[Tuple[str,  'xbmcgui.ListItem',  bool]],
                      totalItems: int = 0) -> bool:
    """
    Callback function to pass directory contents back to Kodi as a list.

    :param handle: integer - handle the plugin was started with.
    :param items: List - list of (url, listitem[, isFolder]) as a tuple to add.
    :param totalItems: [opt] integer - total number of items that will be passed.(used for
        progressbar)
    :return: Returns a bool for successful completion.

    Large lists benefit over using the standard `addDirectoryItem()`. You may call
    this more than once to add items in chunks.

    Example::

        ..
        if not xbmcplugin.addDirectoryItems(int(sys.argv[1]), [(url, listitem, False,)]: raise
        ..
    """
    return True


def endOfDirectory(handle: int,
                   succeeded: bool = True,
                   updateListing: bool = False,
                   cacheToDisc: bool = True) -> None:
    """
    Callback function to tell Kodi that the end of the directory listing in a
    virtualPythonFolder module is reached.

    :param handle: integer - handle the plugin was started with.
    :param succeeded: [opt] bool - True=script completed successfully(Default)/False=Script
        did not.
    :param updateListing: [opt] bool - True=this folder should update the current
        listing/False=Folder is a subfolder(Default).
    :param cacheToDisc: [opt] bool - True=Folder will cache if extended
        time(default)/False=this folder will never cache to disc.

    Example::

        ..
        xbmcplugin.endOfDirectory(int(sys.argv[1]), cacheToDisc=False)
        ..
    """
    pass


def setResolvedUrl(handle: int,
                   succeeded: bool,
                   listitem: 'xbmcgui.ListItem') -> None:
    """
    Callback function to tell Kodi that the file plugin has been resolved to a url

    :param handle: integer - handle the plugin was started with.
    :param succeeded: bool - True=script completed successfully/False=Script did not.
    :param listitem: ListItem - item the file plugin resolved to for playback.

    Example::

        ..
        xbmcplugin.setResolvedUrl(int(sys.argv[1]), True, listitem)
        ..
    """
    pass


def addSortMethod(handle: int,
                  sortMethod: int,
                  labelMask: str = "",
                  label2Mask: str = "") -> None:
    """
    @brief \python_func{ xbmcplugin.addSortMethod(handle, sortMethod
    [,labelMask, label2Mask]) }Adds a sorting method for the media list.

    :param handle: integer - handle the plugin was started with.
    :param sortMethod: integer - see available sort methods at the bottom (or see SortUtils).

    =================================================================== ================================================
    Value                                                               Description                                             
    =================================================================== ================================================
    xbmcplugin.SORT_METHOD_NONE                                         Do not sort                                             
    xbmcplugin.SORT_METHOD_LABEL                                        Sort by label                                           
    xbmcplugin.SORT_METHOD_LABEL_IGNORE_THE                             Sort by the label and ignore "The" before               
    xbmcplugin.SORT_METHOD_DATE                                         Sort by the date                                        
    xbmcplugin.SORT_METHOD_SIZE                                         Sort by the size                                        
    xbmcplugin.SORT_METHOD_FILE                                         Sort by the file                                        
    xbmcplugin.SORT_METHOD_DRIVE_TYPE                                   Sort by the drive type                                  
    xbmcplugin.SORT_METHOD_TRACKNUM                                     Sort by the track number                                
    xbmcplugin.SORT_METHOD_DURATION                                     Sort by the duration                                    
    xbmcplugin.SORT_METHOD_TITLE                                        Sort by the title                                       
    xbmcplugin.SORT_METHOD_TITLE_IGNORE_THE                             Sort by the title and ignore "The" before               
    xbmcplugin.SORT_METHOD_ARTIST                                       Sort by the artist                                      
    xbmcplugin.SORT_METHOD_ARTIST_IGNORE_THE                            Sort by the artist and ignore "The" before              
    xbmcplugin.SORT_METHOD_ALBUM                                        Sort by the album                                       
    xbmcplugin.SORT_METHOD_ALBUM_IGNORE_THE                             Sort by the album and ignore "The" before               
    xbmcplugin.SORT_METHOD_GENRE                                        Sort by the genre                                       
    xbmcplugin.SORT_SORT_METHOD_VIDEO_YEAR, xbmcplugin.SORT_METHOD_YEAR Sort by the year                                        
    xbmcplugin.SORT_METHOD_VIDEO_RATING                                 Sort by the video rating                                
    xbmcplugin.SORT_METHOD_PROGRAM_COUNT                                Sort by the program count                               
    xbmcplugin.SORT_METHOD_PLAYLIST_ORDER                               Sort by the playlist order                              
    xbmcplugin.SORT_METHOD_EPISODE                                      Sort by the episode                                     
    xbmcplugin.SORT_METHOD_VIDEO_TITLE                                  Sort by the video title                                 
    xbmcplugin.SORT_METHOD_VIDEO_SORT_TITLE                             Sort by the video sort title                            
    xbmcplugin.SORT_METHOD_VIDEO_SORT_TITLE_IGNORE_THE                  Sort by the video sort title and ignore
                                                                        "The" before
    xbmcplugin.SORT_METHOD_PRODUCTIONCODE                               Sort by the production code                             
    xbmcplugin.SORT_METHOD_SONG_RATING                                  Sort by the song rating                                 
    xbmcplugin.SORT_METHOD_MPAA_RATING                                  Sort by the mpaa rating                                 
    xbmcplugin.SORT_METHOD_VIDEO_RUNTIME                                Sort by video runtime                                   
    xbmcplugin.SORT_METHOD_STUDIO                                       Sort by the studio                                      
    xbmcplugin.SORT_METHOD_STUDIO_IGNORE_THE                            Sort by the studio and ignore "The" before              
    xbmcplugin.SORT_METHOD_UNSORTED                                     Use list not sorted                                     
    xbmcplugin.SORT_METHOD_BITRATE                                      Sort by the bitrate                                     
    xbmcplugin.SORT_METHOD_LISTENERS                                    Sort by the listeners                                   
    xbmcplugin.SORT_METHOD_COUNTRY                                      Sort by the country                                     
    xbmcplugin.SORT_METHOD_DATEADDED                                    Sort by the added date                                  
    xbmcplugin.SORT_METHOD_FULLPATH                                     Sort by the full path name                              
    xbmcplugin.SORT_METHOD_LABEL_IGNORE_FOLDERS                         Sort by the label names and ignore related
                                                                        folder names
    xbmcplugin.SORT_METHOD_LASTPLAYED                                   Sort by last played date                                
    xbmcplugin.SORT_METHOD_PLAYCOUNT                                    Sort by the play count                                  
    xbmcplugin.SORT_METHOD_CHANNEL                                      Sort by the channel                                     
    xbmcplugin.SORT_METHOD_DATE_TAKEN                                   Sort by the taken date                                  
    xbmcplugin.SORT_METHOD_VIDEO_USER_RATING                            Sort by the rating of the user of video                 
    xbmcplugin.SORT_METHOD_SONG_USER_RATING                             Sort by the rating of the user of song                  
    =================================================================== ================================================

    :param labelMask: [opt] string - the label mask to use for the first label.  applies to:

    ========================== ======================= 
    sortMethod                 labelMask               
    ========================== ======================= 
    SORT_METHOD_TRACKNUM       Defaults to``[%N. ]%T`` 
    SORT_METHOD_EPISODE        Defaults to``%H. %T``   
    SORT_METHOD_PRODUCTIONCODE Defaults to``%H. %T``   
    All other sort methods     Defaults to``%T``       
    ========================== ======================= 

    :param label2Mask: [opt] string - the label mask to use for the second label. Defaults
        to``%D``  applies to:

    ================================ ==================== =======================================
    SORT_METHOD_NONE                 SORT_METHOD_UNSORTED SORT_METHOD_VIDEO_TITLE                 
    SORT_METHOD_TRACKNUM             SORT_METHOD_FILE     SORT_METHOD_TITLE                       
    SORT_METHOD_TITLE_IGNORE_THE     SORT_METHOD_LABEL    SORT_METHOD_LABEL_IGNORE_THE            
    SORT_METHOD_VIDEO_SORT_TITLE     SORT_METHOD_FULLPATH SORT_METHOD_VIDEO_SORT_TITLE_IGNORE_THE 
    SORT_METHOD_LABEL_IGNORE_FOLDERS SORT_METHOD_CHANNEL                                          
    ================================ ==================== ======================================= 

    .. note::
        to add multiple sort methods just call the method multiple times.

    @python_v13 Added new sort **SORT_METHOD_DATE_TAKEN**, **SORT_METHOD_COU
    NTRY**, **SORT_METHOD_DATEADDED**, **SORT_METHOD_FULLPATH**, **SORT_METHO
    D_LABEL_IGNORE_FOLDERS**, **SORT_METHOD_LASTPLAYED**, **SORT_METHOD_PLAY
    COUNT**, **SORT_METHOD_CHANNEL**.

    @python_v17 Added new sort **SORT_METHOD_VIDEO_USER_RATING**.

    @python_v19 Added new option **labelMask**.

    Example::

        ..
        xbmcplugin.addSortMethod(int(sys.argv[1]), xbmcplugin.SORTMETHOD_DATEADDED)
        ..
    """
    pass


def getSetting(handle: int, id: str) -> str:
    """
    Returns the value of a setting as a string.

    :param handle: integer - handle the plugin was started with.
    :param id: string - id of the setting that the module needs to access.
    :return: Setting value as string

    .. note::
        You can use the above as a keyword.

    Example::

        ..
        apikey = xbmcplugin.getSetting(int(sys.argv[1]), 'apikey')
        ..
    """
    return ""


def setSetting(handle: int, id: str, value: str) -> None:
    """
    Sets a plugin setting for the current running plugin.

    :param handle: integer - handle the plugin was started with.
    :param id: string - id of the setting that the module needs to access.
    :param value: string or unicode - value of the setting.

    Example::

        ..
        xbmcplugin.setSetting(int(sys.argv[1]), id='username', value='teamxbmc')
        ..
    """
    pass


def setContent(handle: int, content: str) -> None:
    """
    Sets the plugins content.

    :param handle: integer - handle the plugin was started with.
    :param content: string - content type (eg. movies)

    Available content strings

    ====== ======= ======== ===========
    files  songs   artists  albums      
    movies tvshows episodes musicvideos 
    videos images  games                
    ====== ======= ======== =========== 

    Use **videos** for all videos which do not apply to the more specific mentioned
    ones like "movies", "episodes" etc. A good example is youtube.

    @python_v18 Added new **games** contentExample::

        ..
        xbmcplugin.setContent(int(sys.argv[1]), 'movies')
        ..
    """
    pass


def setPluginCategory(handle: int, category: str) -> None:
    """
    Sets the plugins name for skins to display.

    :param handle: integer - handle the plugin was started with.
    :param category: string or unicode - plugins sub category.

    Example::

        ..
        xbmcplugin.setPluginCategory(int(sys.argv[1]), 'Comedy')
        ..
    """
    pass


def setPluginFanart(handle: int,
                    image: Optional[str] = None,
                    color1: Optional[str] = None,
                    color2: Optional[str] = None,
                    color3: Optional[str] = None) -> None:
    """
    Sets the plugins fanart and color for skins to display.

    :param handle: integer - handle the plugin was started with.
    :param image: [opt] string - path to fanart image.
    :param color1: [opt] hexstring - color1. (e.g. '0xFFFFFFFF')
    :param color2: [opt] hexstring - color2. (e.g. '0xFFFF3300')
    :param color3: [opt] hexstring - color3. (e.g. '0xFF000000')

    Example::

        ..
        xbmcplugin.setPluginFanart(int(sys.argv[1]), 'special://home/addons/plugins/video/Apple movie trailers II/fanart.png',
                                   color2='0xFFFF3300')
        ..
    """
    pass


def setProperty(handle: int, key: str, value: str) -> None:
    """
    Sets a container property for this plugin.

    :param handle: integer - handle the plugin was started with.
    :param key: string - property name.
    :param value: string or unicode - value of property.

    .. note::
        Key is NOT case sensitive.

    Example::

        ..
        xbmcplugin.setProperty(int(sys.argv[1]), 'Emulator', 'M.A.M.E.')
        ..
    """
    pass
