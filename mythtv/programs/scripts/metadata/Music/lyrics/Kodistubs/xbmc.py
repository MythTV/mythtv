# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**General functions on Kodi.**

Offers classes and functions that provide information about the media currently
playing and that allow manipulation of the media player (such as starting a new
song). You can also find system information using the functions available in
this library.
"""
from typing import Union, List, Dict, Tuple, Optional

__kodistubs__ = True

DRIVE_NOT_READY = 1
ENGLISH_NAME = 2
ISO_639_1 = 0
ISO_639_2 = 1
LOGDEBUG = 0
LOGERROR = 3
LOGFATAL = 4
LOGINFO = 1
LOGNONE = 5
LOGWARNING = 2
PLAYLIST_MUSIC = 0
PLAYLIST_VIDEO = 1
SERVER_AIRPLAYSERVER = 2
SERVER_EVENTSERVER = 6
SERVER_JSONRPCSERVER = 3
SERVER_UPNPRENDERER = 4
SERVER_UPNPSERVER = 5
SERVER_WEBSERVER = 1
SERVER_ZEROCONF = 7
TRAY_CLOSED_MEDIA_PRESENT = 3
TRAY_CLOSED_NO_MEDIA = 2
TRAY_OPEN = 1


class InfoTagGame:
    """
    **Kodi's game info tag class.**

    Access and / or modify the game metadata of a ListItem.

    @python_v20 New class added.

    Example::

        ...
        tag = item.getGameInfoTag()
        
        title = tag.getTitle()
        tag.setDeveloper('John Doe')
        ...
    """
    
    def __init__(self, offscreen: bool = False) -> None:
        pass
    
    def getTitle(self) -> str:
        """
        Gets the title of the game.

        :return: [string] title

        @python_v20 New function added.
        """
        return ""
    
    def getPlatform(self) -> str:
        """
        Gets the platform on which the game is run.

        :return: [string] platform

        @python_v20 New function added.
        """
        return ""
    
    def getGenres(self) -> List[str]:
        """
        Gets the genres of the game.

        :return: [list] genres

        @python_v20 New function added.
        """
        return [""]
    
    def getPublisher(self) -> str:
        """
        Gets the publisher of the game.

        :return: [string] publisher

        @python_v20 New function added.
        """
        return ""
    
    def getDeveloper(self) -> str:
        """
        Gets the developer of the game.

        :return: [string] developer

        @python_v20 New function added.
        """
        return ""
    
    def getOverview(self) -> str:
        """
        Gets the overview of the game.

        :return: [string] overview

        @python_v20 New function added.
        """
        return ""
    
    def getYear(self) -> int:
        """
        Gets the year in which the game was published.

        :return: [integer] year

        @python_v20 New function added.
        """
        return 0
    
    def getGameClient(self) -> str:
        """
        Gets the add-on ID of the game client executing the game.

        :return: [string] game client

        @python_v20 New function added.
        """
        return ""
    
    def setTitle(self, title: str) -> None:
        """
        Sets the title of the game.

        :param title: string - title.

        @python_v20 New function added.
        """
        pass
    
    def setPlatform(self, platform: str) -> None:
        """
        Sets the platform on which the game is run.

        :param platform: string - platform.

        @python_v20 New function added.
        """
        pass
    
    def setGenres(self, genres: List[str]) -> None:
        """
        Sets the genres of the game.

        :param genres: list - genres.

        @python_v20 New function added.
        """
        pass
    
    def setPublisher(self, publisher: str) -> None:
        """
        Sets the publisher of the game.

        :param publisher: string - publisher.

        @python_v20 New function added.
        """
        pass
    
    def setDeveloper(self, developer: str) -> None:
        """
        Sets the developer of the game.

        :param developer: string - title.

        @python_v20 New function added.
        """
        pass
    
    def setOverview(self, overview: str) -> None:
        """
        Sets the overview of the game.

        :param overview: string - overview.

        @python_v20 New function added.
        """
        pass
    
    def setYear(self, year: int) -> None:
        """
        Sets the year in which the game was published.

        :param year: integer - year.

        @python_v20 New function added.
        """
        pass
    
    def setGameClient(self, gameClient: str) -> None:
        """
        Sets the add-on ID of the game client executing the game.

        :param gameClient: string - game client.

        @python_v20 New function added.
        """
        pass
    

class InfoTagMusic:
    """
    **Kodi's music info tag class.**

    Access and / or modify the music metadata of a ListItem.

    Example::

        ...
        tag = xbmc.Player().getMusicInfoTag()
        
        title = tag.getTitle()
        url   = tag.getURL()
        ...
    """
    
    def __init__(self, offscreen: bool = False) -> None:
        pass
    
    def getDbId(self) -> int:
        """
        Get identification number of tag in database.

        :return: [integer] database id.

        @python_v18 New function added.
        """
        return 0
    
    def getURL(self) -> str:
        """
        Returns url of source as string from music info tag.

        :return: [string] Url of source
        """
        return ""
    
    def getTitle(self) -> str:
        """
        Returns the title from music as string on info tag.

        :return: [string] Music title
        """
        return ""
    
    def getMediaType(self) -> str:
        """
        Get the media type of the music item.

        :return: [string] media type

        Available strings about media type for music:

        ====== ============================= 
        String Description                   
        ====== ============================= 
        artist If it is defined as an artist 
        album  If it is defined as an album  
        song   If it is defined as a song    
        ====== ============================= 

        @python_v18 New function added.
        """
        return ""
    
    def getArtist(self) -> str:
        """
        Returns the artist from music as string if present.

        :return: [string] Music artist
        """
        return ""
    
    def getAlbum(self) -> str:
        """
        Returns the album from music tag as string if present.

        :return: [string] Music album name
        """
        return ""
    
    def getAlbumArtist(self) -> str:
        """
        Returns the album artist from music tag as string if present.

        :return: [string] Music album artist name
        """
        return ""
    
    def getGenre(self) -> str:
        """
        Returns the genre name from music tag as string if present.

        :return: [string] Genre name

        @python_v20 Deprecated. Use **`getGenres()`** instead.
        """
        return ""
    
    def getGenres(self) -> List[str]:
        """
        Returns the list of genres from music tag if present.

        :return: [list]`List` of genres

        @python_v20 New function added.
        """
        return [""]
    
    def getDuration(self) -> int:
        """
        Returns the duration of music as integer from info tag.

        :return: [integer] Duration
        """
        return 0
    
    def getYear(self) -> int:
        """
        Returns the year of music as integer from info tag.

        :return: [integer] Year

        @python_v20 New function added.
        """
        return 0
    
    def getRating(self) -> int:
        """
        Returns the scraped rating as integer.

        :return: [integer] Rating
        """
        return 0
    
    def getUserRating(self) -> int:
        """
        Returns the user rating as integer (-1 if not existing)

        :return: [integer] User rating
        """
        return 0
    
    def getTrack(self) -> int:
        """
        Returns the track number (if present) from music info tag as integer.

        :return: [integer] Track number
        """
        return 0
    
    def getDisc(self) -> int:
        """
        Returns the disk number (if present) from music info tag as integer.

        :return: [integer] Disc number
        """
        return 0
    
    def getReleaseDate(self) -> str:
        """
        Returns the release date as string from music info tag (if present).

        :return: [string] Release date
        """
        return ""
    
    def getListeners(self) -> int:
        """
        Returns the listeners as integer from music info tag.

        :return: [integer] Listeners
        """
        return 0
    
    def getPlayCount(self) -> int:
        """
        Returns the number of carried out playbacks.

        :return: [integer] Playback count
        """
        return 0
    
    def getLastPlayed(self) -> str:
        """
        Returns last played time as string from music info tag.

        :return: [string] Last played date / time on tag

        @python_v20 Deprecated. Use **`getLastPlayedAsW3C()`** instead.
        """
        return ""
    
    def getLastPlayedAsW3C(self) -> str:
        """
        Returns last played time as string in W3C format (YYYY-MM-DDThh:mm:ssTZD).

        :return: [string] Last played datetime (W3C)

        @python_v20 New function added.
        """
        return ""
    
    def getComment(self) -> str:
        """
        Returns comment as string from music info tag.

        :return: [string] Comment on tag
        """
        return ""
    
    def getLyrics(self) -> str:
        """
        Returns a string from lyrics.

        :return: [string] Lyrics on tag
        """
        return ""
    
    def getMusicBrainzTrackID(self) -> str:
        """
        Returns the MusicBrainz Recording ID from music info tag (if present).

        :return: [string] MusicBrainz Recording ID

        @python_v19 New function added.
        """
        return ""
    
    def getMusicBrainzArtistID(self) -> List[str]:
        """
        Returns the MusicBrainz Artist IDs from music info tag (if present).

        :return: [list] MusicBrainz Artist IDs

        @python_v19 New function added.
        """
        return [""]
    
    def getMusicBrainzAlbumID(self) -> str:
        """
        Returns the MusicBrainz Release ID from music info tag (if present).

        :return: [string] MusicBrainz Release ID

        @python_v19 New function added.
        """
        return ""
    
    def getMusicBrainzReleaseGroupID(self) -> str:
        """
        Returns the MusicBrainz Release Group ID from music info tag (if present).

        :return: [string] MusicBrainz Release Group ID

        @python_v19 New function added.
        """
        return ""
    
    def getMusicBrainzAlbumArtistID(self) -> List[str]:
        """
        Returns the MusicBrainz Release Artist IDs from music info tag (if present).

        :return: [list] MusicBrainz Release Artist IDs

        @python_v19 New function added.
        """
        return [""]
    
    def setDbId(self, dbId: int, type: str) -> None:
        """
        Set the database identifier of the music item.

        :param dbId: integer - Database identifier.
        :param type: string - Media type of the item.

        @python_v20 New function added.
        """
        pass
    
    def setURL(self, url: str) -> None:
        """
        Set the URL of the music item.

        :param url: string - URL.

        @python_v20 New function added.
        """
        pass
    
    def setMediaType(self, mediaType: str) -> None:
        """
        Set the media type of the music item.

        :param mediaType: string - Media type.

        @python_v20 New function added.
        """
        pass
    
    def setTrack(self, track: int) -> None:
        """
        Set the track number of the song.

        :param track: integer - Track number.

        @python_v20 New function added.
        """
        pass
    
    def setDisc(self, disc: int) -> None:
        """
        Set the disc number of the song.

        :param disc: integer - Disc number.

        @python_v20 New function added.
        """
        pass
    
    def setDuration(self, duration: int) -> None:
        """
        Set the duration of the song.

        :param duration: integer - Duration in seconds.

        @python_v20 New function added.
        """
        pass
    
    def setYear(self, year: int) -> None:
        """
        Set the year of the music item.

        :param year: integer - Year.

        @python_v20 New function added.
        """
        pass
    
    def setReleaseDate(self, releaseDate: str) -> None:
        """
        Set the release date of the music item.

        :param releaseDate: string - Release date in ISO8601 format (YYYY, YYYY-MM or YYYY-MM-DD).

        @python_v20 New function added.
        """
        pass
    
    def setListeners(self, listeners: int) -> None:
        """
        Set the number of listeners of the music item.

        :param listeners: integer - Number of listeners.

        @python_v20 New function added.
        """
        pass
    
    def setPlayCount(self, playcount: int) -> None:
        """
        Set the playcount of the music item.

        :param playcount: integer - Playcount.

        @python_v20 New function added.
        """
        pass
    
    def setGenres(self, genres: List[str]) -> None:
        """
        Set the genres of the music item.

        :param genres: list - Genres.

        @python_v20 New function added.
        """
        pass
    
    def setAlbum(self, album: str) -> None:
        """
        Set the album of the music item.

        :param album: string - Album.

        @python_v20 New function added.
        """
        pass
    
    def setArtist(self, artist: str) -> None:
        """
        Set the artist(s) of the music item.

        :param artist: string - Artist(s).

        @python_v20 New function added.
        """
        pass
    
    def setAlbumArtist(self, albumArtist: str) -> None:
        """
        Set the album artist(s) of the music item.

        :param albumArtist: string - Album artist(s).

        @python_v20 New function added.
        """
        pass
    
    def setTitle(self, title: str) -> None:
        """
        Set the title of the music item.

        :param title: string - Title.

        @python_v20 New function added.
        """
        pass
    
    def setRating(self, rating: float) -> None:
        """
        Set the rating of the music item.

        :param rating: float - Rating.

        @python_v20 New function added.
        """
        pass
    
    def setUserRating(self, userrating: int) -> None:
        """
        Set the user rating of the music item.

        :param userrating: integer - User rating.

        @python_v20 New function added.
        """
        pass
    
    def setLyrics(self, lyrics: str) -> None:
        """
        Set the lyrics of the song.

        :param lyrics: string - Lyrics.

        @python_v20 New function added.
        """
        pass
    
    def setLastPlayed(self, lastPlayed: str) -> None:
        """
        Set the last played date of the music item.

        :param lastPlayed: string - Last played date (YYYY-MM-DD HH:MM:SS).

        @python_v20 New function added.
        """
        pass
    
    def setMusicBrainzTrackID(self, musicBrainzTrackID: str) -> None:
        """
        Set the MusicBrainz track ID of the song.

        :param musicBrainzTrackID: string - MusicBrainz track ID.

        @python_v20 New function added.
        """
        pass
    
    def setMusicBrainzArtistID(self, musicBrainzArtistID: List[str]) -> None:
        """
        Set the MusicBrainz artist IDs of the music item.

        :param musicBrainzArtistID: list - MusicBrainz artist IDs.

        @python_v20 New function added.
        """
        pass
    
    def setMusicBrainzAlbumID(self, musicBrainzAlbumID: str) -> None:
        """
        Set the MusicBrainz album ID of the music item.

        :param musicBrainzAlbumID: string - MusicBrainz album ID.

        @python_v20 New function added.
        """
        pass
    
    def setMusicBrainzReleaseGroupID(self, musicBrainzReleaseGroupID: str) -> None:
        """
        Set the MusicBrainz release group ID of the music item.

        :param musicBrainzReleaseGroupID: string - MusicBrainz release group ID.

        @python_v20 New function added.
        """
        pass
    
    def setMusicBrainzAlbumArtistID(self, musicBrainzAlbumArtistID: List[str]) -> None:
        """
        Set the MusicBrainz album artist IDs of the music item.

        :param musicBrainzAlbumArtistID: list - MusicBrainz album artist IDs.

        @python_v20 New function added.
        """
        pass
    
    def setComment(self, comment: str) -> None:
        """
        Set the comment of the music item.

        :param comment: string - Comment.

        @python_v20 New function added.
        """
        pass
    

class InfoTagPicture:
    """
    **Kodi's picture info tag class.**

    Access and / or modify the picture metadata of a ListItem.

    @python_v20 New class added.

    Example::

        ...
        tag = item.getPictureInfoTag()
        
        datetime_taken  = tag.getDateTimeTaken()
        tag.setResolution(1920, 1080)
        ...
    """
    
    def __init__(self, offscreen: bool = False) -> None:
        pass
    
    def getResolution(self) -> str:
        """
        Get the resolution of the picture in the format "w x h".

        :return: [string] Resolution of the picture in the format "w x h".

        @python_v20 New function added.
        """
        return ""
    
    def setResolution(self, width: int, height: int) -> None:
        """
        Sets the resolution of the picture.

        :param width: int - Width of the picture in pixels.
        :param height: int - Height of the picture in pixels.
        
        @python_v20 New function added.
        """
        pass
    
    def setDateTimeTaken(self, datetimetaken: str) -> None:
        """
        Sets the date and time at which the picture was taken in W3C format. The
        following formats are supported:

        YYYY

        YYYY-MM-DD

        YYYY-MM-DDThh:mm[TZD]

        YYYY-MM-DDThh:mm:ss[TZD] where the timezone (TZD) is always optional and can be
        in one of the following formats:

        Z (for UTC)

        +hh:mm

        -hh:mm

        :param datetimetaken: string - Date and time at which the picture was taken in W3C format.

        @python_v20 New function added.
        """
        pass
    

class InfoTagRadioRDS:
    """
    **Kodi's radio RDS info tag class.**

    To get radio RDS info tag data of currently played `PVR` radio channel source.

    .. note::
        Info tag load is only be possible from present player class.  Also
        is all the data variable from radio channels and not known on
        beginning of radio receiving.

    Example::

        ...
        tag = xbmc.Player().getRadioRDSInfoTag()
        
        title  = tag.getTitle()
        artist = tag.getArtist()
        ...
    """
    
    def __init__(self) -> None:
        pass
    
    def getTitle(self) -> str:
        """
        Title of the item on the air; i.e. song title.

        :return: Title
        """
        return ""
    
    def getBand(self) -> str:
        """
        Band of the item on air.

        :return: Band
        """
        return ""
    
    def getArtist(self) -> str:
        """
        Artist of the item on air.

        :return: Artist
        """
        return ""
    
    def getComposer(self) -> str:
        """
        Get the Composer of the music.

        :return: Composer
        """
        return ""
    
    def getConductor(self) -> str:
        """
        Get the Conductor of the Band.

        :return: Conductor
        """
        return ""
    
    def getAlbum(self) -> str:
        """
        Album of item on air.

        :return: Album name
        """
        return ""
    
    def getComment(self) -> str:
        """
        Get Comment text from channel.

        :return: Comment
        """
        return ""
    
    def getAlbumTrackNumber(self) -> int:
        """
        Get the album track number of currently sended music.

        :return: Track Number
        """
        return 0
    
    def getInfoNews(self) -> str:
        """
        Get News informations.

        :return: News Information
        """
        return ""
    
    def getInfoNewsLocal(self) -> str:
        """
        Get Local news informations.

        :return: Local News Information
        """
        return ""
    
    def getInfoSport(self) -> str:
        """
        Get Sport informations.

        :return: Sport Information
        """
        return ""
    
    def getInfoStock(self) -> str:
        """
        Get Stock informations.

        :return: Stock Information
        """
        return ""
    
    def getInfoWeather(self) -> str:
        """
        Get Weather informations.

        :return: Weather Information
        """
        return ""
    
    def getInfoHoroscope(self) -> str:
        """
        Get Horoscope informations.

        :return: Horoscope Information
        """
        return ""
    
    def getInfoCinema(self) -> str:
        """
        Get Cinema informations.

        :return: Cinema Information
        """
        return ""
    
    def getInfoLottery(self) -> str:
        """
        Get Lottery informations.

        :return: Lottery Information
        """
        return ""
    
    def getInfoOther(self) -> str:
        """
        Get other informations.

        :return: Other Information
        """
        return ""
    
    def getEditorialStaff(self) -> str:
        """
        Get Editorial Staff names.

        :return: Editorial Staff
        """
        return ""
    
    def getProgStation(self) -> str:
        """
        Name describing station.

        :return: Program Station
        """
        return ""
    
    def getProgStyle(self) -> str:
        """
        The the radio channel style currently used.

        :return: Program Style
        """
        return ""
    
    def getProgHost(self) -> str:
        """
        Host of current radio show.

        :return: Program Host
        """
        return ""
    
    def getProgWebsite(self) -> str:
        """
        Link to URL (web page) for radio station homepage.

        :return: Program Website
        """
        return ""
    
    def getProgNow(self) -> str:
        """
        Current radio program show.

        :return: Program Now
        """
        return ""
    
    def getProgNext(self) -> str:
        """
        Next program show.

        :return: Program Next
        """
        return ""
    
    def getPhoneHotline(self) -> str:
        """
        Telephone number of the radio station's hotline.

        :return: Phone Hotline
        """
        return ""
    
    def getEMailHotline(self) -> str:
        """
        Email address of the radio station's studio.

        :return: EMail Hotline
        """
        return ""
    
    def getPhoneStudio(self) -> str:
        """
        Telephone number of the radio station's studio.

        :return: Phone Studio
        """
        return ""
    
    def getEMailStudio(self) -> str:
        """
        Email address of radio station studio.

        :return: EMail Studio
        """
        return ""
    
    def getSMSStudio(self) -> str:
        """
        SMS (Text Messaging) number for studio.

        :return: SMS Studio
        """
        return ""
    

class Actor:
    """
    **`Actor` class used in combination with `InfoTagVideo`.**

    Represents a single actor in the cast of a video item wrapped by `InfoTagVideo`.

    @python_v20 New class added.

    Example::

        ...
        actor = xbmc.Actor('Sean Connery', 'James Bond', order=1)
        ...
    """
    
    def __init__(self, name: str = "",
                 role: str = "",
                 order: int = -1,
                 thumbnail: str = "") -> None:
        pass
    
    def getName(self) -> str:
        """
        Get the name of the actor.

        :return: [string] Name of the actor

        @python_v20 New function added.
        """
        return ""
    
    def getRole(self) -> str:
        """
        Get the role of the actor in the specific video item.

        :return: [string] Role of the actor in the specific video item

        @python_v20 New function added.
        """
        return ""
    
    def getOrder(self) -> int:
        """
        Get the order of the actor in the cast of the specific video item.

        :return: [integer] Order of the actor in the cast of the specific video item

        @python_v20 New function added.
        """
        return 0
    
    def getThumbnail(self) -> str:
        """
        Get the path / URL to the thumbnail of the actor.

        :return: [string] Path / URL to the thumbnail of the actor

        @python_v20 New function added.
        """
        return ""
    
    def setName(self, name: str) -> None:
        """
        Set the name of the actor.

        :param name: string - Name of the actor.

        @python_v20 New function added.
        """
        pass
    
    def setRole(self, role: str) -> None:
        """
        Set the role of the actor in the specific video item.

        :param role: string - Role of the actor in the specific video item.

        @python_v20 New function added.
        """
        pass
    
    def setOrder(self, order: int) -> None:
        """
        Set the order of the actor in the cast of the specific video item.

        :param order: integer - Order of the actor in the cast of the specific video item.

        @python_v20 New function added.
        """
        pass
    
    def setThumbnail(self, thumbnail: str) -> None:
        """
        Set the path / URL to the thumbnail of the actor.

        :param thumbnail: string - Path / URL to the thumbnail of the actor.

        @python_v20 New function added.
        """
        pass
    

class VideoStreamDetail:
    """
    **Video stream details class used in combination with `InfoTagVideo`.**

    Represents a single selectable video stream for a video item wrapped
    by `InfoTagVideo`.

    @python_v20 New class added.

    Example::

        ...
        videostream = xbmc.VideoStreamDetail(1920, 1080, language='English')
        ...
    """
    
    def __init__(self, width: int = 0,
                 height: int = 0,
                 aspect: float = 0.0,
                 duration: int = 0,
                 codec: str = "",
                 stereoMode: str = "",
                 language: str = "",
                 hdrType: str = "") -> None:
        pass
    
    def getWidth(self) -> int:
        """
        Get the width of the video stream in pixel.

        :return: [integer] Width of the video stream

        @python_v20 New function added.
        """
        return 0
    
    def getHeight(self) -> int:
        """
        Get the height of the video stream in pixel.

        :return: [integer] Height of the video stream

        @python_v20 New function added.
        """
        return 0
    
    def getAspect(self) -> float:
        """
        Get the aspect ratio of the video stream.

        :return: [float] Aspect ratio of the video stream

        @python_v20 New function added.
        """
        return 0.0
    
    def getDuration(self) -> int:
        """
        Get the duration of the video stream in seconds.

        :return: [float] Duration of the video stream in seconds

        @python_v20 New function added.
        """
        return 0
    
    def getCodec(self) -> str:
        """
        Get the codec of the stream.

        :return: [string] Codec of the stream

        @python_v20 New function added.
        """
        return ""
    
    def getStereoMode(self) -> str:
        """
        Get the stereo mode of the video stream.

        :return: [string] Stereo mode of the video stream

        @python_v20 New function added.
        """
        return ""
    
    def getLanguage(self) -> str:
        """
        Get the language of the stream.

        :return: [string] Language of the stream

        @python_v20 New function added.
        """
        return ""
    
    def getHDRType(self) -> str:
        """
        Get the HDR type of the stream.

        :return: [string] HDR type of the stream

        @python_v20 New function added.
        """
        return ""
    
    def setWidth(self, width: int) -> None:
        """
        Set the width of the video stream in pixel.

        :param width: integer - Width of the video stream in pixel.

        @python_v20 New function added.
        """
        pass
    
    def setHeight(self, height: int) -> None:
        """
        Set the height of the video stream in pixel.

        :param height: integer - Height of the video stream in pixel.

        @python_v20 New function added.
        """
        pass
    
    def setAspect(self, aspect: float) -> None:
        """
        Set the aspect ratio of the video stream.

        :param aspect: float - Aspect ratio of the video stream.

        @python_v20 New function added.
        """
        pass
    
    def setDuration(self, duration: int) -> None:
        """
        Set the duration of the video stream in seconds.

        :param duration: integer - Duration of the video stream in seconds.

        @python_v20 New function added.
        """
        pass
    
    def setCodec(self, codec: str) -> None:
        """
        Set the codec of the stream.

        :param codec: string - Codec of the stream.

        @python_v20 New function added.
        """
        pass
    
    def setStereoMode(self, stereoMode: str) -> None:
        """
        Set the stereo mode of the video stream.

        :param stereoMode: string - Stereo mode of the video stream.

        @python_v20 New function added.
        """
        pass
    
    def setLanguage(self, language: str) -> None:
        """
        Set the language of the stream.

        :param language: string - Language of the stream.

        @python_v20 New function added.
        """
        pass
    
    def setHDRType(self, hdrType: str) -> None:
        """
        Set the HDR type of the stream.

        :param hdrType: string - HDR type of the stream. The following types are supported:
            dolbyvision, hdr10, hlg

        @python_v20 New function added.
        """
        pass
    

class AudioStreamDetail:
    """
    **Audio stream details class used in combination with `InfoTagVideo`.**

    Represents a single selectable audio stream for a video item wrapped
    by `InfoTagVideo`.

    @python_v20 New class added.

    Example::

        ...
        audiostream = xbmc.AudioStreamDetail(6, 'DTS', 'English')
        ...
    """
    
    def __init__(self, channels: int = -1,
                 codec: str = "",
                 language: str = "") -> None:
        pass
    
    def getChannels(self) -> int:
        """
        Get the number of channels in the stream.

        :return: [integer] Number of channels in the stream

        @python_v20 New function added.
        """
        return 0
    
    def getCodec(self) -> str:
        """
        Get the codec of the stream.

        :return: [string] Codec of the stream

        @python_v20 New function added.
        """
        return ""
    
    def getLanguage(self) -> str:
        """
        Get the language of the stream.

        :return: [string] Language of the stream

        @python_v20 New function added.
        """
        return ""
    
    def setChannels(self, channels: int) -> None:
        """
        Set the number of channels in the stream.

        :param channels: integer - Number of channels in the stream.

        @python_v20 New function added.
        """
        pass
    
    def setCodec(self, codec: str) -> None:
        """
        Set the codec of the stream.

        :param codec: string - Codec of the stream.

        @python_v20 New function added.
        """
        pass
    
    def setLanguage(self, language: str) -> None:
        """
        Set the language of the stream.

        :param language: string - Language of the stream.

        @python_v20 New function added.
        """
        pass
    

class SubtitleStreamDetail:
    """
    **Subtitle stream details class used in combination with `InfoTagVideo`.**

    Represents a single selectable subtitle stream for a video item wrapped
    by `InfoTagVideo`.

    @python_v20 New class added.

    Example::

        ...
        subtitlestream = xbmc.SubtitleStreamDetail('English')
        ...
    """
    
    def __init__(self, language: str = "") -> None:
        pass
    
    def getLanguage(self) -> str:
        """
        Get the language of the stream.

        :return: [string] Language of the stream

        @python_v20 New function added.
        """
        return ""
    
    def setLanguage(self, language: str) -> None:
        """
        Set the language of the stream.

        :param language: string - Language of the stream.

        @python_v20 New function added.
        """
        pass


class InfoTagVideo:
    """
    **Kodi's video info tag class.**

    Access and / or modify the video metadata of a ListItem.

    Example::

        ...
        tag = xbmc.Player().getVideoInfoTag()
        
        title = tag.getTitle()
        file  = tag.getFile()
        ...
    """
    
    def __init__(self, offscreen: bool = False) -> None:
        pass
    
    def getDbId(self) -> int:
        """
        Get identification number of tag in database

        :return: [integer] database id

        @python_v17 New function added.
        """
        return 0
    
    def getDirector(self) -> str:
        """
        Getfilm director who has made the film (if present).

        :return: [string] Film director name.

        @python_v20 Deprecated. Use **`getDirectors()`** instead.
        """
        return ""
    
    def getDirectors(self) -> List[str]:
        """
        Get a list offilm directors who have made the film (if present).

        :return: [list]`List` of film director names.

        @python_v20 New function added.
        """
        return [""]
    
    def getWritingCredits(self) -> str:
        """
        Get the writing credits if present from video info tag.

        :return: [string] Writing credits

        @python_v20 Deprecated. Use **`getWriters()`** instead.
        """
        return ""
    
    def getWriters(self) -> List[str]:
        """
        Get the list of writers (if present) from video info tag.

        :return: [list] `List` of writers

        @python_v20 New function added.
        """
        return [""]
    
    def getGenre(self) -> str:
        """
        To get theVideo Genre if available.

        :return: [string] Genre name

        @python_v20 Deprecated. Use **`getGenres()`** instead.
        """
        return ""
    
    def getGenres(self) -> List[str]:
        """
        Get the list ofVideo Genres if available.

        :return: [list]`List` of genres

        @python_v20 New function added.
        """
        return [""]
    
    def getTagLine(self) -> str:
        """
        Get video tag line if available.

        :return: [string] Video tag line
        """
        return ""
    
    def getPlotOutline(self) -> str:
        """
        Get the outline plot of the video if present.

        :return: [string] Outline plot
        """
        return ""
    
    def getPlot(self) -> str:
        """
        Get the plot of the video if present.

        :return: [string] Plot
        """
        return ""
    
    def getPictureURL(self) -> str:
        """
        Get a picture URL of the video to show as screenshot.

        :return: [string] Picture URL
        """
        return ""
    
    def getTitle(self) -> str:
        """
        Get the video title.

        :return: [string] Video title
        """
        return ""
    
    def getTVShowTitle(self) -> str:
        """
        Get the video TV show title.

        :return: [string] TV show title

        @python_v17 New function added.
        """
        return ""
    
    def getMediaType(self) -> str:
        """
        Get the media type of the video.

        :return: [string] media type

        Available strings about media type for video:

        ========== ====================================
        String     Description
        ========== ====================================
        video      For normal video
        set        For a selection of video
        musicvideo To define it as music video
        movie      To define it as normal movie
        tvshow     If this is it defined as tvshow
        season     The type is used as a series season
        episode    The type is used as a series episode
        ========== ====================================

        @python_v17 New function added.
        """
        return ""
    
    def getVotes(self) -> str:
        """
        Get the video votes if available from video info tag.

        :return: [string] Votes

        @python_v20 Deprecated. Use **`getVotesAsInt()`** instead.
        """
        return ""
    
    def getVotesAsInt(self, type: str = "") -> int:
        """
        Get the votes of the rating (if available) as an integer.

        :param type: [opt] string - the type of the rating.  Some rating type values (any
            string possible):

        ===== ==================
        Label Type
        ===== ==================
        imdb  string - type name
        tvdb  string - type name
        tmdb  string - type name
        anidb string - type name
        ===== ==================

        :return: [integer] Votes

        @python_v20 New function added.
        """
        return 0
    
    def getCast(self) -> str:
        """
        To get the cast of the video when available.

        :return: [string] Video casts

        @python_v20 Deprecated. Use **`getActors()`** instead.
        """
        return ""
    
    def getActors(self) -> List[Actor]:
        """
        Get the cast of the video if available.

        :return: [list]`List` of actors

        @python_v20 New function added.
        """
        return [Actor()]
    
    def getFile(self) -> str:
        """
        To get the video file name.

        :return: [string] File name
        """
        return ""
    
    def getPath(self) -> str:
        """
        To get the path where the video is stored.

        :return: [string] Path
        """
        return ""
    
    def getFilenameAndPath(self) -> str:
        """
        To get the full path with filename where the video is stored.

        :return: [string] File name and Path

        @python_v19 New function added.
        """
        return ""
    
    def getIMDBNumber(self) -> str:
        """
        To get theIMDb number of the video (if present).

        :return: [string] IMDb number
        """
        return ""
    
    def getSeason(self) -> int:
        """
        To get season number of a series

        :return: [integer] season number

        @python_v17 New function added.
        """
        return 0
    
    def getEpisode(self) -> int:
        """
        To get episode number of a series

        :return: [integer] episode number

        @python_v17 New function added.
        """
        return 0
    
    def getYear(self) -> int:
        """
        Get production year of video if present.

        :return: [integer] Production Year
        """
        return 0
    
    def getRating(self, type: str = "") -> float:
        """
        Get the video rating if present as float (double where supported).

        :param type: [opt] string - the type of the rating.  Some rating type values (any
            string possible):

        ===== ==================
        Label Type
        ===== ==================
        imdb  string - type name
        tvdb  string - type name
        tmdb  string - type name
        anidb string - type name
        ===== ==================

        :return: [float] The rating of the video

        @python_v20 Optional ``type`` parameter added.
        """
        return 0.0
    
    def getUserRating(self) -> int:
        """
        Get the user rating if present as integer.

        :return: [integer] The user rating of the video
        """
        return 0
    
    def getPlayCount(self) -> int:
        """
        To get the number of plays of the video.

        :return: [integer] Play Count
        """
        return 0
    
    def getLastPlayed(self) -> str:
        """
        Get the last played date / time as string.

        :return: [string] Last played date / time

        @python_v20 Deprecated. Use **`getLastPlayedAsW3C()`** instead.
        """
        return ""
    
    def getLastPlayedAsW3C(self) -> str:
        """
        Get last played datetime as string in W3C format (YYYY-MM-DDThh:mm:ssTZD).

        :return: [string] Last played datetime (W3C)

        @python_v20 New function added.
        """
        return ""
    
    def getOriginalTitle(self) -> str:
        """
        To get the original title of the video.

        :return: [string] Original title
        """
        return ""
    
    def getPremiered(self) -> str:
        """
        To getpremiered date of the video, if available.

        :return: [string]

        @python_v20 Deprecated. Use **`getPremieredAsW3C()`** instead.
        """
        return ""
    
    def getPremieredAsW3C(self) -> str:
        """
        Getpremiered date as string in W3C format (YYYY-MM-DD).

        :return: [string] Premiered date (W3C)

        @python_v20 New function added.
        """
        return ""
    
    def getFirstAired(self) -> str:
        """
        Returns first aired date as string from info tag.

        :return: [string] First aired date

        @python_v20 Deprecated. Use **`getFirstAiredAsW3C()`** instead.
        """
        return ""
    
    def getFirstAiredAsW3C(self) -> str:
        """
        Get first aired date as string in W3C format (YYYY-MM-DD).

        :return: [string] First aired date (W3C)

        @python_v20 New function added.
        """
        return ""
    
    def getTrailer(self) -> str:
        """
        To get the path where the trailer is stored.

        :return: [string] Trailer path

        @python_v17 New function added.
        """
        return ""
    
    def getArtist(self) -> List[str]:
        """
        To get the artist name (for musicvideos)

        :return: [List[str]] Artist name

        @python_v18 New function added.
        """
        return [""]
    
    def getAlbum(self) -> str:
        """
        To get the album name (for musicvideos)

        :return: [string] Album name

        @python_v18 New function added.
        """
        return ""
    
    def getTrack(self) -> int:
        """
        To get the track number (for musicvideos)

        :return: [int] Track number

        @python_v18 New function added.
        """
        return 0
    
    def getDuration(self) -> int:
        """
        To get the duration

        :return: [unsigned int] Duration

        @python_v18 New function added.
        """
        return 0
    
    def getResumeTime(self) -> float:
        """
        Gets the resume time of the video item.

        :return: [double] Resume time

        @python_v20 New function added.
        """
        return 0.0

    def getResumeTimeTotal(self) -> float:
        """
        Gets the total duration stored with the resume time of the video item.

        :return: [double] Total duration stored with the resume time

        @python_v20 New function added.
        """
        return 0.0

    def getUniqueID(self, key: str) -> str:
        """
        Get the unique ID of the given key. A unique ID is an identifier used by a
        (online) video database used to identify a video in its database.

        :param key: string - uniqueID name.  Some default uniqueID values (any string
            possible):

        ===== ======================
        Label Type
        ===== ======================
        imdb  string - uniqueid name
        tvdb  string - uniqueid name
        tmdb  string - uniqueid name
        anidb string - uniqueid name
        ===== ======================

        @python_v20 New function added.
        """
        return ""

    def setUniqueID(self, uniqueid: str,
                    type: str = "",
                    isdefault: bool = False) -> None:
        """
        Set the given unique ID. A unique ID is an identifier used by a (online) video
        database used to identify a video in its database.

        :param uniqueid: string - value of the unique ID.
        :param type: [opt] string - type / label of the unique ID.
        :param isdefault: [opt] bool - whether the given unique ID is the default unique ID.

        @python_v20 New function added.
        """
        pass

    def setUniqueIDs(self, uniqueIDs: Dict[str, str],
                     defaultuniqueid: str = "") -> None:
        """
        Set the given unique IDs. A unique ID is an identifier used by a (online) video
        database used to identify a video in its database.

        :param values: dictionary - pairs of{ 'label: 'value' }`.
        :param defaultuniqueid: [opt] string - the name of default uniqueID.

        Some example values (any string possible):

        ===== ======================
        Label Type
        ===== ======================
        imdb  string - uniqueid name
        tvdb  string - uniqueid name
        tmdb  string - uniqueid name
        anidb string - uniqueid name
        ===== ======================

        @python_v20 New function added.
        """
        pass

    def setDbId(self, dbid: int) -> None:
        """
        Set the database identifier of the video item.

        :param dbid: integer - Database identifier.

        @python_v20 New function added.
        """
        pass

    def setYear(self, year: int) -> None:
        """
        Set the year of the video item.

        :param year: integer - Year.

        @python_v20 New function added.
        """
        pass
    
    def setEpisode(self, episode: int) -> None:
        """
        Set the episode number of the episode.

        :param episode: integer - Episode number.

        @python_v20 New function added.
        """
        pass
    
    def setSeason(self, season: int) -> None:
        """
        Set the season number of the video item.

        :param season: integer - Season number.

        @python_v20 New function added.
        """
        pass

    def setSortEpisode(self, sortepisode: int) -> None:
        """
        Set the episode sort number of the episode.

        :param sortepisode: integer - Episode sort number.

        @python_v20 New function added.
        """
        pass

    def setSortSeason(self, sortseason: int) -> None:
        """
        Set the season sort number of the season.

        :param sortseason: integer - Season sort number.

        @python_v20 New function added.
        """
        pass

    def setEpisodeGuide(self, episodeguide: str) -> None:
        """
        Set the episode guide of the video item.

        :param episodeguide: string - Episode guide.

        @python_v20 New function added.
        """
        pass

    def setTop250(self, top250: int) -> None:
        """
        Set the top 250 number of the video item.

        :param top250: integer - Top 250 number.

        @python_v20 New function added.
        """
        pass

    def setSetId(self, setid: int) -> None:
        """
        Set the movie set identifier of the video item.

        :param setid: integer - Set identifier.

        @python_v20 New function added.
        """
        pass

    def setTrackNumber(self, tracknumber: int) -> None:
        """
        Set the track number of the music video item.

        :param tracknumber: integer - Track number.

        @python_v20 New function added.
        """
        pass

    def setRating(self, rating: float,
                  votes: int = 0,
                  type: str = "",
                  isdefault: bool = False) -> None:
        """
        Set the rating of the video item.

        :param rating: float - Rating number.
        :param votes: integer - Number of votes.
        :param type: string - Type of the rating.
        :param isdefault: bool - Whether the rating is the default or not.

        @python_v20 New function added.
        """
        pass

    def setRatings(self, ratings: Dict[str, Tuple[float, int]],
                   defaultrating: str = "") -> None:
        """
        Set the ratings of the video item.

        :param ratings: dictionary -{ 'type: (rating, votes) }`.
        :param defaultrating: string - Type / Label of the default rating.

        @python_v20 New function added.
        """
        pass

    def setUserRating(self, userrating: int) -> None:
        """
        Set the user rating of the video item.

        :param userrating: integer - User rating.

        @python_v20 New function added.
        """
        pass

    def setPlaycount(self, playcount: int) -> None:
        """
        Set the playcount of the video item.

        :param playcount: integer - Playcount.

        @python_v20 New function added.
        """
        pass

    def setMpaa(self, mpaa: str) -> None:
        """
        Set the MPAA rating of the video item.

        :param mpaa: string - MPAA rating.

        @python_v20 New function added.
        """
        pass

    def setPlot(self, plot: str) -> None:
        """
        Set the plot of the video item.

        :param plot: string - Plot.

        @python_v20 New function added.
        """
        pass

    def setPlotOutline(self, plotoutline: str) -> None:
        """
        Set the plot outline of the video item.

        :param plotoutline: string - Plot outline.

        @python_v20 New function added.
        """
        pass

    def setTitle(self, title: str) -> None:
        """
        Set the title of the video item.

        :param title: string - Title.

        @python_v20 New function added.
        """
        pass

    def setOriginalTitle(self, originaltitle: str) -> None:
        """
        Set the original title of the video item.

        :param originaltitle: string - Original title.

        @python_v20 New function added.
        """
        pass

    def setSortTitle(self, sorttitle: str) -> None:
        """
        Set the sort title of the video item.

        :param sorttitle: string - Sort title.

        @python_v20 New function added.
        """
        pass

    def setTagLine(self, tagline: str) -> None:
        """
        Set the tagline of the video item.

        :param tagline: string - Tagline.

        @python_v20 New function added.
        """
        pass

    def setTvShowTitle(self, tvshowtitle: str) -> None:
        """
        Set the TV show title of the video item.

        :param tvshowtitle: string - TV show title.

        @python_v20 New function added.
        """
        pass

    def setTvShowStatus(self, status: str) -> None:
        """
        Set the TV show status of the video item.

        :param status: string - TV show status.

        @python_v20 New function added.
        """
        pass

    def setGenres(self, genre: List[str]) -> None:
        """
        Set the genres of the video item.

        :param genre: list - Genres.

        @python_v20 New function added.
        """
        pass

    def setCountries(self, countries: List[str]) -> None:
        """
        Set the countries of the video item.

        :param countries: list - Countries.

        @python_v20 New function added.
        """
        pass

    def setDirectors(self, directors: List[str]) -> None:
        """
        Set the directors of the video item.

        :param directors: list - Directors.

        @python_v20 New function added.
        """
        pass

    def setStudios(self, studios: List[str]) -> None:
        """
        Set the studios of the video item.

        :param studios: list - Studios.

        @python_v20 New function added.
        """
        pass

    def setWriters(self, writers: List[str]) -> None:
        """
        Set the writers of the video item.

        :param writers: list - Writers.

        @python_v20 New function added.
        """
        pass

    def setDuration(self, duration: int) -> None:
        """
        Set the duration of the video item.

        :param duration: integer - Duration in seconds.

        @python_v20 New function added.
        """
        pass

    def setPremiered(self, premiered: str) -> None:
        """
        Set the premiere date of the video item.

        :param premiered: string - Premiere date.

        @python_v20 New function added.
        """
        pass

    def setSet(self, set: str) -> None:
        """
        Set the movie set (name) of the video item.

        :param set: string - Movie set (name).

        @python_v20 New function added.
        """
        pass

    def setSetOverview(self, setoverview: str) -> None:
        """
        Set the movie set overview of the video item.

        :param setoverview: string - Movie set overview.

        @python_v20 New function added.
        """
        pass

    def setTags(self, tags: List[str]) -> None:
        """
        Set the tags of the video item.

        :param tags: list - Tags.

        @python_v20 New function added.
        """
        pass

    def setProductionCode(self, productioncode: str) -> None:
        """
        Set the production code of the video item.

        :param productioncode: string - Production code.

        @python_v20 New function added.
        """
        pass

    def setFirstAired(self, firstaired: str) -> None:
        """
        Set the first aired date of the video item.

        :param firstaired: string - First aired date.

        @python_v20 New function added.
        """
        pass

    def setLastPlayed(self, lastplayed: str) -> None:
        """
        Set the last played date of the video item.

        :param lastplayed: string - Last played date (YYYY-MM-DD HH:MM:SS).

        @python_v20 New function added.
        """
        pass

    def setAlbum(self, album: str) -> None:
        """
        Set the album of the video item.

        :param album: string - Album.

        @python_v20 New function added.
        """
        pass

    def setVotes(self, votes: int) -> None:
        """
        Set the number of votes of the video item.

        :param votes: integer - Number of votes.

        @python_v20 New function added.
        """
        pass

    def setTrailer(self, trailer: str) -> None:
        """
        Set the trailer of the video item.

        :param trailer: string - Trailer.

        @python_v20 New function added.
        """
        pass

    def setPath(self, path: str) -> None:
        """
        Set the path of the video item.

        :param path: string - Path.

        @python_v20 New function added.
        """
        pass

    def setFilenameAndPath(self, filenameandpath: str) -> None:
        """
        Set the filename and path of the video item.

        :param filenameandpath: string - Filename and path.

        @python_v20 New function added.
        """
        pass

    def setIMDBNumber(self, imdbnumber: str) -> None:
        """
        Set the IMDb number of the video item.

        :param imdbnumber: string - IMDb number.

        @python_v20 New function added.
        """
        pass

    def setDateAdded(self, dateadded: str) -> None:
        """
        Set the date added of the video item.

        :param dateadded: string - Date added (YYYY-MM-DD HH:MM:SS).

        @python_v20 New function added.
        """
        pass

    def setMediaType(self, mediatype: str) -> None:
        """
        Set the media type of the video item.

        :param mediatype: string - Media type.

        @python_v20 New function added.
        """
        pass

    def setShowLinks(self, showlinks: List[str]) -> None:
        """
        Set the TV show links of the movie.

        :param showlinks: list - TV show links.

        @python_v20 New function added.
        """
        pass

    def setArtists(self, artists: List[str]) -> None:
        """
        Set the artists of the music video item.

        :param artists: list - Artists.

        @python_v20 New function added.
        """
        pass

    def setCast(self, actors: List[Actor]) -> None:
        """
        Set the cast / actors of the video item.

        :param actors: list - Cast / Actors.

        @python_v20 New function added.
        """
        pass

    def setResumePoint(self, time: float, totaltime: float = 0.0) -> None:
        """
        Set the resume point of the video item.

        :param time: float - Resume point in seconds.
        :param totaltime: float - Total duration in seconds.

        @python_v20 New function added.
        """
        pass

    def addSeason(self, number: int, name: str = "") -> None:
        """
        Add a season with name. It needs at least the season number.

        :param number: int - the number of the season.
        :param name: string - the name of the season. Default "".

        @python_v20 New function added.

        Example::

            ...
            # addSeason(number, name))
            infotagvideo.addSeason(1, "Murder House")
            ...
        """
        pass

    def addSeasons(self, namedseasons: List[Tuple[int, str]]) -> None:
        """
        Add named seasons to the TV show.

        :param namedseasons: list -``[ (season, name) ]``.

        @python_v20 New function added.
        """
        pass

    def addVideoStream(self, stream: VideoStreamDetail) -> None:
        """
        Add a video stream to the video item.

        :param stream: `VideoStreamDetail` - Video stream.

        @python_v20 New function added.
        """
        pass

    def addAudioStream(self, stream: AudioStreamDetail) -> None:
        """
        Add an audio stream to the video item.

        :param stream: `AudioStreamDetail` - Audio stream.

        @python_v20 New function added.
        """
        pass

    def addSubtitleStream(self, stream: SubtitleStreamDetail) -> None:
        """
        Add a subtitle stream to the video item.

        :param stream: `SubtitleStreamDetail` - Subtitle stream.

        @python_v20 New function added.
        """
        pass

    def addAvailableArtwork(self, url: str,
                            arttype: str = "",
                            preview: str = "",
                            referrer: str = "",
                            cache: str = "",
                            post: bool = False,
                            isgz: bool = False,
                            season: int = -1) -> None:
        """
        Add an image to available artworks (needed for video scrapers)

        :param url: string - image path url
        :param arttype: string - image type
        :param preview: [opt] string - image preview path url
        :param referrer: [opt] string - referrer url
        :param cache: [opt] string - filename in cache
        :param post: [opt] bool - use post to retrieve the image (default false)
        :param isgz: [opt] bool - use gzip to retrieve the image (default false)
        :param season: [opt] integer - number of season in case of season thumb

        @python_v20 New function added.

        Example::

            ...
            infotagvideo.addAvailableArtwork(path_to_image_1, "thumb")
            ...
        """
        pass


class Keyboard:
    """
    **Kodi's keyboard class.**

    Creates a new `Keyboard` object with default text heading and hidden input flag
    if supplied.

    :param default: : [opt] string - default text entry.
    :param heading: : [opt] string - keyboard heading.
    :param hidden: : [opt] boolean - True for hidden text entry.

    Example::

        ..
        kb = xbmc.Keyboard('default', 'heading', True)
        kb.setDefault('password') # optional
        kb.setHeading('Enter password') # optional
        kb.setHiddenInput(True) # optional
        kb.doModal()
        if (kb.isConfirmed()):
        text = kb.getText()
        ..
    """
    
    def __init__(self, line: str = "",
                 heading: str = "",
                 hidden: bool = False) -> None:
        pass
    
    def doModal(self, autoclose: int = 0) -> None:
        """
        Show keyboard and wait for user action.

        :param autoclose: [opt] integer - milliseconds to autoclose dialog. (default=do not
            autoclose)

        Example::

            ..
            kb.doModal(30000)
            ..
        """
        pass
    
    def setDefault(self, line: str = "") -> None:
        """
        Set the default text entry.

        :param line: string - default text entry.

        Example::

            ..
            kb.setDefault('password')
            ..
        """
        pass
    
    def setHiddenInput(self, hidden: bool = False) -> None:
        """
        Allows hidden text entry.

        :param hidden: boolean - True for hidden text entry.

        Example::

            ..
            kb.setHiddenInput(True)
            ..
        """
        pass
    
    def setHeading(self, heading: str) -> None:
        """
        Set the keyboard heading.

        :param heading: string - keyboard heading.

        Example::

            ..
            kb.setHeading('Enter password')
            ..
        """
        pass
    
    def getText(self) -> str:
        """
        Returns the user input as a string.

        .. note::
            This will always return the text entry even if you cancel the
            keyboard. Use the `isConfirmed()` method to check if user cancelled
            the keyboard.

        :return: get the in keyboard entered text

        Example::

            ..
            text = kb.getText()
            ..
        """
        return ""
    
    def isConfirmed(self) -> bool:
        """
        Returns False if the user cancelled the input.

        :return: true if confirmed, if cancelled false

        Example::

            ..
            if (kb.isConfirmed()):
            ..
        """
        return True
    

class Monitor:
    """
    **Kodi's monitor class.**

    Creates a new monitor to notify addon about changes.
    """
    
    def __init__(self) -> None:
        pass
    
    def onSettingsChanged(self) -> None:
        """
        onSettingsChanged method.

        Will be called when addon settings are changed
        """
        pass
    
    def onScreensaverActivated(self) -> None:
        """
        onScreensaverActivated method.

        Will be called when screensaver kicks in
        """
        pass
    
    def onScreensaverDeactivated(self) -> None:
        """
        onScreensaverDeactivated method.

        Will be called when screensaver goes off
        """
        pass
    
    def onDPMSActivated(self) -> None:
        """
        onDPMSActivated method.

        Will be called when energysaving/DPMS gets active
        """
        pass
    
    def onDPMSDeactivated(self) -> None:
        """
        onDPMSDeactivated method.

        Will be called when energysaving/DPMS is turned off
        """
        pass
    
    def onScanStarted(self, library: str) -> None:
        """
        onScanStarted method.

        :param library: Video / music as string

        .. note::
            Will be called when library clean has ended and return video or
            music to indicate which library is being scanned

        @python_v14 New function added.
        """
        pass
    
    def onScanFinished(self, library: str) -> None:
        """
        onScanFinished method.

        :param library: Video / music as string

        .. note::
            Will be called when library clean has ended and return video or
            music to indicate which library has been scanned

        @python_v14 New function added.
        """
        pass
    
    def onCleanStarted(self, library: str) -> None:
        """
        onCleanStarted method.

        :param library: Video / music as string

        .. note::
            Will be called when library clean has ended and return video or
            music to indicate which library has been cleaned

        @python_v14 New function added.
        """
        pass
    
    def onCleanFinished(self, library: str) -> None:
        """
        onCleanFinished method.

        :param library: Video / music as string

        .. note::
            Will be called when library clean has ended and return video or
            music to indicate which library has been finished

        @python_v14 New function added.
        """
        pass
    
    def onNotification(self, sender: str, method: str, data: str) -> None:
        """
        onNotification method.

        :param sender: Sender of the notification
        :param method: Name of the notification
        :param data: JSON-encoded data of the notification

        .. note::
            Will be called when Kodi receives or sends a notification

        @python_v13 New function added.
        """
        pass
    
    def waitForAbort(self, timeout: float = -1) -> bool:
        """
        Wait for Abort

        Block until abort is requested, or until timeout occurs. If an abort requested
        have already been made, return immediately.

        :param timeout: [opt] float - timeout in seconds. Default: no timeout.
        :return: True when abort have been requested, False if a timeout is given and the operation times out.

        @python_v14 New function added.

        Example::

            ..
            monitor = xbmc.Monitor()
            # do something
            monitor.waitForAbort(10) # sleeps for 10 secs or returns early if kodi aborts
            if monitor.abortRequested():
            # abort was requested to Kodi (e.g. shutdown), do your cleanup logic
            ..
        """
        return True
    
    def abortRequested(self) -> bool:
        """
        Returns True if abort has been requested.

        :return: True if requested

        @python_v14 New function added.
        """
        return True
    

class Player:
    """
    **Kodi's player.**

    To become and create the class to play something.

    Example::

        ...
        xbmc.Player().play(url, listitem, windowed)
        ...
    """
    
    def __init__(self) -> None:
        pass
    
    def play(self, item: Union[str,  'PlayList'] = "",
             listitem: Optional['xbmcgui.ListItem'] = None,
             windowed: bool = False,
             startpos: int = -1) -> None:
        """
        Play an item.

        :param item: [opt] string - filename, url or playlist
        :param listitem: [opt] listitem - used with setInfo() to set different infolabels.
        :param windowed: [opt] bool - true=play video windowed, false=play users
            preference.(default)
        :param startpos: [opt] int - starting position when playing a playlist. Default = -1

        .. note::
            If item is not given then the `Player` will try to play the current
            item in the current playlist.   You can use the above as keywords
            for arguments and skip certain optional arguments.  Once you use a
            keyword, all following arguments require the keyword.

        Example::

            ...
            listitem = xbmcgui.ListItem('Ironman')
            listitem.setInfo('video', {'Title': 'Ironman', 'Genre': 'Science Fiction'})
            xbmc.Player().play(url, listitem, windowed)
            xbmc.Player().play(playlist, listitem, windowed, startpos)
            ...
        """
        pass
    
    def stop(self) -> None:
        """
        Stop playing.
        """
        pass
    
    def pause(self) -> None:
        """
        Pause or resume playing if already paused.
        """
        pass
    
    def playnext(self) -> None:
        """
        Play next item in playlist.
        """
        pass
    
    def playprevious(self) -> None:
        """
        Play previous item in playlist.
        """
        pass
    
    def playselected(self, selected: int) -> None:
        """
        Play a certain item from the current playlist.

        :param selected: Integer - Item to select
        """
        pass
    
    def isPlaying(self) -> bool:
        """
        Check Kodi is playing something.

        :return: True if Kodi is playing a file.
        """
        return True
    
    def isPlayingAudio(self) -> bool:
        """
        Check for playing audio.

        :return: True if Kodi is playing an audio file.
        """
        return True
    
    def isPlayingVideo(self) -> bool:
        """
        Check for playing video.

        :return: True if Kodi is playing a video.
        """
        return True
    
    def isPlayingRDS(self) -> bool:
        """
        Check for playing radio data system (RDS).

        :return: True if kodi is playing a radio data system (RDS).
        """
        return True
    
    def isExternalPlayer(self) -> bool:
        """
        Check for external player.

        :return: True if kodi is playing using an external player.

        @python_v18 New function added.
        """
        return True
    
    def getPlayingFile(self) -> str:
        """
        Returns the current playing file as a string.

        .. note::
            For LiveTV, returns a **pvr://** url which is not translatable to
            an OS specific file or external url.

        :return: Playing filename
        :raises Exception: If player is not playing a file.
        """
        return ""
    
    def getPlayingItem(self) -> 'xbmcgui.ListItem':
        """
        Returns the current playing item.

        :return: Playing item
        :raises Exception: If player is not playing a file.

        @python_v20 New function added.
        """
        from xbmcgui import ListItem
        return ListItem()
    
    def getTime(self) -> float:
        """
        Get playing time.

        Returns the current time of the current playing media as fractional seconds.

        :return: Current time as fractional seconds
        :raises Exception: If player is not playing a file.
        """
        return 0.0
    
    def seekTime(self, seekTime: float) -> None:
        """
        Seek time.

        Seeks the specified amount of time as fractional seconds. The time specified is
        relative to the beginning of the currently. playing media file.

        :param seekTime: Time to seek as fractional seconds
        :raises Exception: If player is not playing a file.
        """
        pass
    
    def setSubtitles(self, subtitleFile: str) -> None:
        """
        Set subtitle file and enable subtitles.

        :param subtitleFile: File to use as source ofsubtitles
        """
        pass
    
    def showSubtitles(self, bVisible: bool) -> None:
        """
        Enable / disable subtitles.

        :param visible: [boolean] True for visible subtitles.

        Example::

            ...
            xbmc.Player().showSubtitles(True)
            ...
        """
        pass
    
    def getSubtitles(self) -> str:
        """
        Get subtitle stream name.

        :return: Stream name
        """
        return ""
    
    def getAvailableSubtitleStreams(self) -> List[str]:
        """
        Get Subtitle stream names.

        :return: `List` of subtitle streams as name
        """
        return [""]
    
    def setSubtitleStream(self, iStream: int) -> None:
        """
        Set Subtitle Stream.

        :param iStream: [int] Subtitle stream to select for play

        Example::

            ...
            xbmc.Player().setSubtitleStream(1)
            ...
        """
        pass
    
    def updateInfoTag(self, item: 'xbmcgui.ListItem') -> None:
        """
        Update info labels for currently playing item.

        :param item: ListItem with new info
        :raises Exception: If player is not playing a file

        @python_v18 New function added.

        Example::

            ...
            item = xbmcgui.ListItem()
            item.setPath(xbmc.Player().getPlayingFile())
            item.setInfo('music', {'title' : 'foo', 'artist' : 'bar'})
            xbmc.Player().updateInfoTag(item)
            ...
        """
        pass
    
    def getVideoInfoTag(self) -> InfoTagVideo:
        """
        To get video info tag.

        Returns the VideoInfoTag of the current playing Movie.

        :return: Video info tag
        :raises Exception: If player is not playing a file or current file is not a movie file.
        """
        return InfoTagVideo()
    
    def getMusicInfoTag(self) -> InfoTagMusic:
        """
        To get music info tag.

        Returns the MusicInfoTag of the current playing 'Song'.

        :return: Music info tag
        :raises Exception: If player is not playing a file or current file is not a music file.
        """
        return InfoTagMusic()
    
    def getRadioRDSInfoTag(self) -> InfoTagRadioRDS:
        """
        To get Radio RDS info tag

        Returns the RadioRDSInfoTag of the current playing 'Radio Song if. present'.

        :return: Radio RDS info tag
        :raises Exception: If player is not playing a file or current file is not a rds file.
        """
        return InfoTagRadioRDS()
    
    def getTotalTime(self) -> float:
        """
        To get total playing time.

        Returns the total time of the current playing media in seconds. This is only
        accurate to the full second.

        :return: Total time of the current playing media
        :raises Exception: If player is not playing a file.
        """
        return 0.0
    
    def getAvailableAudioStreams(self) -> List[str]:
        """
        Get Audio stream names

        :return: `List` of audio streams as name
        """
        return [""]
    
    def setAudioStream(self, iStream: int) -> None:
        """
        Set Audio Stream.

        :param iStream: [int] Audio stream to select for play

        Example::

            ...
            xbmc.Player().setAudioStream(1)
            ...
        """
        pass
    
    def getAvailableVideoStreams(self) -> List[str]:
        """
        Get Video stream names

        :return: `List` of video streams as name
        """
        return [""]
    
    def setVideoStream(self, iStream: int) -> None:
        """
        Set Video Stream.

        :param iStream: [int] Video stream to select for play

        Example::

            ...
            xbmc.Player().setVideoStream(1)
            ...
        """
        pass
    
    def onPlayBackStarted(self) -> None:
        """
        onPlayBackStarted method.

        Will be called when Kodi player starts. Video or audio might not be available at
        this point.

        @python_v18 Use `onAVStarted()` instead if you need to detect if Kodi is actually
        playing a media file (i.e, if a stream is available)
        """
        pass
    
    def onAVStarted(self) -> None:
        """
        onAVStarted method.

        Will be called when Kodi has a video or audiostream.

        @python_v18 New function added.
        """
        pass
    
    def onAVChange(self) -> None:
        """
        onAVChange method.

        Will be called when Kodi has a video, audio or subtitle stream. Also happens
        when the stream changes.

        @python_v18 New function added.
        """
        pass
    
    def onPlayBackEnded(self) -> None:
        """
        onPlayBackEnded method.

        Will be called when Kodi stops playing a file.
        """
        pass
    
    def onPlayBackStopped(self) -> None:
        """
        onPlayBackStopped method.

        Will be called when user stops Kodi playing a file.
        """
        pass
    
    def onPlayBackError(self) -> None:
        """
        onPlayBackError method.

        Will be called when playback stops due to an error.
        """
        pass
    
    def onPlayBackPaused(self) -> None:
        """
        onPlayBackPaused method.

        Will be called when user pauses a playing file.
        """
        pass
    
    def onPlayBackResumed(self) -> None:
        """
        onPlayBackResumed method.

        Will be called when user resumes a paused file.
        """
        pass
    
    def onQueueNextItem(self) -> None:
        """
        onQueueNextItem method.

        Will be called when user queues the next item.
        """
        pass
    
    def onPlayBackSpeedChanged(self, speed: int) -> None:
        """
        onPlayBackSpeedChanged method.

        Will be called when players speed changes (eg. user FF/RW).

        :param speed: [integer] Current speed of player

        .. note::
            Negative speed means player is rewinding, 1 is normal playback
            speed.
        """
        pass
    
    def onPlayBackSeek(self, time: int, seekOffset: int) -> None:
        """
        onPlayBackSeek method.

        Will be called when user seeks to a time.

        :param time: [integer] Time to seek to
        :param seekOffset: [integer] ?
        """
        pass
    
    def onPlayBackSeekChapter(self, chapter: int) -> None:
        """
        onPlayBackSeekChapter method.

        Will be called when user performs a chapter seek.

        :param chapter: [integer] Chapter to seek to
        """
        pass
    

class PlayList:
    """
    **Kodi's Play `List` class.**

    To create and edit a playlist which can be handled by the player.

    :param playList: [integer] To define the stream type

    ===== =================== =================================== 
    Value Integer String      Description                         
    ===== =================== =================================== 
    0     xbmc.PLAYLIST_MUSIC Playlist for music files or streams 
    1     xbmc.PLAYLIST_VIDEO Playlist for video files or streams 
    ===== =================== =================================== 

    Example::

        ...
        play=xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
        ...
    """
    
    def __init__(self, playList: int) -> None:
        pass
    
    def getPlayListId(self) -> int:
        """
        Get the `PlayList` Identifier

        :return: Id as an integer.
        """
        return 0
    
    def add(self, url: str,
            listitem: Optional['xbmcgui.ListItem'] = None,
            index: int = -1) -> None:
        """
        Adds a new file to the playlist.

        :param url: string or unicode - filename or url to add.
        :param listitem: [opt] listitem - used with setInfo() to set different infolabels.
        :param index: [opt] integer - position to add playlist item. (default=end)

        .. note::
            You can use the above as keywords for arguments and skip certain
            optional arguments.  Once you use a keyword, all following
            arguments require the keyword.

        Example::

            ..
            playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
            video = 'F:\\movies\\Ironman.mov'
            listitem = xbmcgui.ListItem('Ironman', thumbnailImage='F:\\movies\\Ironman.tbn')
            listitem.setInfo('video', {'Title': 'Ironman', 'Genre': 'Science Fiction'})
            playlist.add(url=video, listitem=listitem, index=7)
            ..
        """
        pass
    
    def load(self, filename: str) -> bool:
        """
        Load a playlist.

        Clear current playlist and copy items from the file to this Playlist filename
        can be like .pls or .m3u ...

        :param filename: File with list to play inside
        :return: False if unable to load playlist
        """
        return True
    
    def remove(self, filename: str) -> None:
        """
        Remove an item with this filename from the playlist.

        :param filename: The file to remove from list.
        """
        pass
    
    def clear(self) -> None:
        """
        Clear all items in the playlist.
        """
        pass
    
    def size(self) -> int:
        """
        Returns the total number of PlayListItems in this playlist.

        :return: Amount of playlist entries.
        """
        return 0
    
    def shuffle(self) -> None:
        """
        Shuffle the playlist.
        """
        pass
    
    def unshuffle(self) -> None:
        """
        Unshuffle the playlist.
        """
        pass
    
    def getposition(self) -> int:
        """
        Returns the position of the current song in this playlist.

        :return: Position of the current song
        """
        return 0
    

class RenderCapture:
    """
    **Kodi's render capture.**
    """
    
    def __init__(self) -> None:
        pass
    
    def getWidth(self) -> int:
        """
        Get width

        To get width of captured image as set during `RenderCapture.capture()`. Returns 0
        prior to calling capture.

        :return: Width or 0 prior to calling capture
        """
        return 0
    
    def getHeight(self) -> int:
        """
        Get height

        To get height of captured image as set during `RenderCapture.capture()`. Returns
        0 prior to calling capture.

        :return: height or 0 prior to calling capture
        """
        return 0
    
    def getAspectRatio(self) -> float:
        """
        Get aspect ratio of currently displayed video.

        :return: Aspect ratio

        This may be called prior to calling `RenderCapture.capture()`.
        """
        return 0.0
    
    def getImageFormat(self) -> str:
        """
        Get image format

        :return: Format of captured image: 'BGRA'

        @python_v17 Image will now always be returned in BGRA
        """
        return ""
    
    def getImage(self, msecs: int = 0) -> bytearray:
        """
        Returns captured image as a bytearray.

        :param msecs: [opt] Milliseconds to wait. Waits 1000ms if not specified
        :return: Captured image as a bytearray

        .. note::
            The size of the image is m_width * m_height * 4

        @python_v17 Added the option to specify wait time in msec.
        """
        return bytearray()
    
    def capture(self, width: int, height: int) -> None:
        """
        Issue capture request.

        :param width: Width capture image should be rendered to
        :param height: Height capture image should should be rendered to

        @python_v17 Removed the option to pass **flags**
        """
        pass


def log(msg: str, level: int = LOGDEBUG) -> None:
    """
    Write a string to Kodi's log file and the debug window.

    :param msg: string - text to output.
    :param level: [opt] integer - log level to output at.(default=LOGDEBUG)

    =============== ================================================================================
    Value:          Description:
    =============== ================================================================================
    xbmc.LOGDEBUG   In depth information about the status of Kodi. This information can pretty much
                    only be deciphered by a developer or long time Kodi power user.
    xbmc.LOGINFO    Something has happened. It's not a problem, we just thought you might want
                    to know. Fairly excessive output that most people won't care about.
    xbmc.LOGWARNING Something potentially bad has happened. If Kodi did something you didn't expect,
                    this is probably why. Watch for errors to follow.
    xbmc.LOGERROR   This event is bad. Something has failed. You likely noticed problems with
                    the application be it skin artifacts, failure of playback a crash, etc.
    xbmc.LOGFATAL   We're screwed. Kodi is about to crash.
    =============== ================================================================================

    .. note::
        Addon developers are advised to keep ``LOGDEBUG`` as the default
        logging level and to use conservative logging (log only if
        needed). Excessive logging makes it harder to debug kodi itself.

    Logging in kodi has a global configuration level that controls how
    text is written to the log. This global logging behaviour can be
    changed in the GUI (**Settings -> System -> Logging**) (debug toggle)
    or furthered configured in advancedsettings (loglevel setting).

    Text is written to the log for the following conditions:

    * loglevel == -1 (NONE, nothing at all is logged to the log)

    * loglevel == 0 (NORMAL, shows ``LOGINFO``,``LOGWARNING``,``LOGERROR``
      and ``LOGFATAL``) - Default kodi behaviour

    * loglevel == 1 (DEBUG, shows all) - Behaviour if you toggle debug log in the GUI

    @python_v17 Default level changed from ``LOGNOTICE`` to ``LOGDEBUG``

    @python_v19 Removed ``LOGNOTICE`` (use ``LOGINFO``) and ``LOGSEVERE`` (use ``LOGFATAL``)

    Example::

        ..
        xbmc.log(msg='This is a test string.', level=xbmc.LOGDEBUG);
        ..
    """
    pass


def shutdown() -> None:
    """
    Shutdown the htpc.

    Example::

        ..
        xbmc.shutdown()
        ..
    """
    pass


def restart() -> None:
    """
    Restart the htpc.

    Example::

        ..
        xbmc.restart()
        ..
    """
    pass


def executescript(script: str) -> None:
    """
    Execute a python script.

    :param script: string - script filename to execute.

    Example::

        ..
        xbmc.executescript('special://home/scripts/update.py')
        ..
    """
    pass


def executebuiltin(function: str, wait: bool = False) -> None:
    """
    Execute a built in Kodi function.

    :param function: string - builtin function to execute.
    :param wait: [opt] bool - If Kodi should wait for the builtin function execution to
        finish (default False)

    List of builtin functions: https://kodi.wiki/view/List_of_built-in_functions

    Example::

        ..
        xbmc.executebuiltin('Skin.SetString(abc,def)')
        ..
    """
    pass


def executeJSONRPC(jsonrpccommand: str) -> str:
    """
    Execute an JSONRPC command.

    :param jsonrpccommand: string - jsonrpc command to execute.
    :return: jsonrpc return string

    Example::

        ..
        response = xbmc.executeJSONRPC('{ "jsonrpc": "2.0", "method": "JSONRPC.Introspect", "id": 1 }')
        ..
    """
    return ""


def sleep(timemillis: int) -> None:
    """
    Sleeps for 'time' (msec).

    :param time: integer - number of msec to sleep.
    :raises TypeError: If time is not an integer.

    This is useful if you need to sleep for a small amount of time (milisecond
    range) somewhere in your addon logic. Please note that Kodi will attempt to stop
    any running scripts when signaled to exit and wait for a maximum of 5 seconds
    before trying to force stop your script. If your addon makes use
    of `xbmc.sleep()` incorrectly (long periods of time, e.g. that exceed the force
    stop waiting time) it may lead to Kodi hanging on shutdown. In case your addon
    needs long sleep/idle periods use `xbmc.Monitor().waitForAbort(secs)` instead.

    Example::

        ..
        xbmc.sleep(2000) # sleeps for 2 seconds
        ..
    """
    pass


def getLocalizedString(id: int) -> str:
    """
    Get a localized 'unicode string'.

    :param id: integer - id# for string you want to localize.
    :return: Localized 'unicode string'

    .. note::
        See strings.po in``\language\{yourlanguage}\`` for which id you
        need for a string.

    Example::

        ..
        locstr = xbmc.getLocalizedString(6)
        ..
    """
    return ""


def getSkinDir() -> str:
    """
    Get the active skin directory.

    :return: The active skin directory as a string

    .. note::
        This is not the full path like
        'special://home/addons/MediaCenter', but only 'MediaCenter'.

    Example::

        ..
        skindir = xbmc.getSkinDir()
        ..
    """
    return ""


def getLanguage(format: int = ENGLISH_NAME, region: bool = False) -> str:
    """
    Get the active language.

    :param format: [opt] format of the returned language string

    ================= ========================================================== 
    Value             Description                                                
    ================= ========================================================== 
    xbmc.ISO_639_1    Two letter code as defined in ISO 639-1                    
    xbmc.ISO_639_2    Three letter code as defined in ISO 639-2/T or ISO 639-2/B 
    xbmc.ENGLISH_NAME Full language name in English (default)                    
    ================= ========================================================== 

    :param region: [opt] append the region delimited by "-" of the language (setting) to
        the returned language string
    :return: The active language as a string

    @python_v13 Added new options **format** and **region**.

    Example::

        ..
        language = xbmc.getLanguage(xbmc.ENGLISH_NAME)
        ..
    """
    return ""


def getIPAddress() -> str:
    """
    Get the current ip address.

    :return: The current ip address as a string

    Example::

        ..
        ip = xbmc.getIPAddress()
        ..
    """
    return ""


def getDVDState() -> int:
    """
    Returns the dvd state as an integer.

    :return: Values for state are:

    ===== ============================== 
    Value Name                           
    ===== ============================== 
    1     xbmc.DRIVE_NOT_READY           
    16    xbmc.TRAY_OPEN                 
    64    xbmc.TRAY_CLOSED_NO_MEDIA      
    96    xbmc.TRAY_CLOSED_MEDIA_PRESENT 
    ===== ============================== 

    Example::

        ..
        dvdstate = xbmc.getDVDState()
        ..
    """
    return 0


def getFreeMem() -> int:
    """
    Get amount of free memory in MB.

    :return: The amount of free memory in MB as an integer

    Example::

        ..
        freemem = xbmc.getFreeMem()
        ..
    """
    return 0


def getInfoLabel(cLine: str) -> str:
    """
    Get a info label

    :param infotag: string - infoTag for value you want returned.
    :return: InfoLabel as a string

    List of InfoLabels: https://kodi.wiki/view/InfoLabels

    Example::

        ..
        label = xbmc.getInfoLabel('Weather.Conditions')
        ..
    """
    return ""


def getInfoImage(infotag: str) -> str:
    """
    Get filename including path to the InfoImage's thumbnail.

    :param infotag: string - infotag for value you want returned
    :return: Filename including path to the InfoImage's thumbnail as a string

    List of InfoTags: http://kodi.wiki/view/InfoLabels

    Example::

        ..
        filename = xbmc.getInfoImage('Weather.Conditions')
        ..
    """
    return ""


def playSFX(filename: str, useCached: bool = True) -> None:
    """
    Plays a wav file by filename

    :param filename: string - filename of the wav file to play
    :param useCached: [opt] bool - False = Dump any previously cached wav associated with
        filename

    @python_v14 Added new option **useCached**.

    Example::

        ..
        xbmc.playSFX('special://xbmc/scripts/dingdong.wav')
        xbmc.playSFX('special://xbmc/scripts/dingdong.wav',False)
        ..
    """
    pass


def stopSFX() -> None:
    """
    Stops wav file

    @python_v14 New function added.

    Example::

        ..
        xbmc.stopSFX()
        ..
    """
    pass


def enableNavSounds(yesNo: bool) -> None:
    """
    Enables/Disables nav sounds

    :param yesNo: bool - enable (True) or disable (False) nav sounds

    Example::

        ..
        xbmc.enableNavSounds(True)
        ..
    """
    pass


def getCondVisibility(condition: str) -> bool:
    """
    Get visibility conditions

    :param condition: string - condition to check
    :return: True (if the condition is verified) or False (otherwise)

    List of boolean conditions: https://kodi.wiki/view/List_of_boolean_conditions

    .. note::
        You can combine two (or more) of the above settings by
        using ``+`` as an AND operator, ``|`` as an OR operator, ``!``
        as a NOT operator, and ``[`` and ``]`` to bracket expressions.

    Example::

        ..
        visible = xbmc.getCondVisibility('[Control.IsVisible(41) + !Control.IsVisible(12)]')
        ..
    """
    return True


def getGlobalIdleTime() -> int:
    """
    Get the elapsed idle time in seconds.

    :return: Elapsed idle time in seconds as an integer

    Example::

        ..
        t = xbmc.getGlobalIdleTime()
        ..
    """
    return 0


def getCacheThumbName(path: str) -> str:
    """
    Get thumb cache filename.

    :param path: string - path to file
    :return: Thumb cache filename

    Example::

        ..
        thumb = xbmc.getCacheThumbName('f:\\videos\\movie.avi')
        ..
    """
    return ""


def getCleanMovieTitle(path: str,
                       usefoldername: bool = False) -> Tuple[str, str]:
    """
    Get clean movie title and year string if available.

    :param path: string - String to clean
    :param usefoldername: [opt] bool - use folder names (defaults to false)
    :return: Clean movie title and year string if available.

    Example::

        ..
        title, year = xbmc.getCleanMovieTitle('/path/to/moviefolder/test.avi', True)
        ..
    """
    return "", ""


def getRegion(id: str) -> str:
    """
    Returns your regions setting as a string for the specified id.

    :param id: string - id of setting to return
    :return: Region setting

    .. note::
        choices are (dateshort, datelong, time, meridiem, tempunit,
        speedunit)

    Example::

        ..
        date_long_format = xbmc.getRegion('datelong')
        ..
    """
    return ""


def getSupportedMedia(mediaType: str) -> str:
    """
    Get the supported file types for the specific media.

    :param media: string - media type
    :return: Supported file types for the specific media as a string

    .. note::
        Media type can be (video, music, picture). The return value is a
        pipe separated string of filetypes (eg. ``'.mov |.avi'``).

    Example::

        ..
        mTypes = xbmc.getSupportedMedia('video')
        ..
    """
    return ""


def skinHasImage(image: str) -> bool:
    """
    Check skin for presence of Image.

    :param image: string - image filename
    :return: True if the image file exists in the skin

    .. note::
        If the media resides in a subfolder include it.
        (eg. home-myfiles\home-myfiles2.png).

    Example::

        ..
        exists = xbmc.skinHasImage('ButtonFocusedTexture.png')
        ..
    """
    return True


def startServer(iTyp: int, bStart: bool) -> bool:
    """
    Start or stop a server.

    :param typ: integer - use SERVER_* constants  Used format of the returned language
        string

    ========================= ====================================================================== 
    Value                     Description                                                            
    ========================= ====================================================================== 
    xbmc.SERVER_WEBSERVER     To control Kodi's builtin webserver                                    
    xbmc.SERVER_AIRPLAYSERVER AirPlay is a proprietary protocol stack/suite developed by Apple Inc.  
    xbmc.SERVER_JSONRPCSERVER Control JSON-RPC HTTP/TCP socket-based interface                       
    xbmc.SERVER_UPNPRENDERER  UPnP client (aka UPnP renderer)                                        
    xbmc.SERVER_UPNPSERVER    Control built-in UPnP A/V media server (UPnP-server)                   
    xbmc.SERVER_EVENTSERVER   Set eventServer part that accepts remote device input on all platforms 
    xbmc.SERVER_ZEROCONF      Control Kodi's Avahi Zeroconf                                          
    ========================= ====================================================================== 

    :param bStart: bool - start (True) or stop (False) a server
    :return: bool - True or False

    @python_v20 Removed option **bWait**.

    Example::

        ..
        xbmc.startServer(xbmc.SERVER_AIRPLAYSERVER, False)
        ..
    """
    return True


def audioSuspend() -> None:
    """
    Suspend Audio engine.

    Example::

        ..
        xbmc.audioSuspend()
        ..
    """
    pass


def audioResume() -> None:
    """
    Resume Audio engine.

    Example::

        ..
        xbmc.audioResume()
        ..
    """
    pass


def getUserAgent() -> str:
    """
    Returns Kodi's HTTP UserAgent string

    :return: HTTP user agent

    Example::

        ..
        xbmc.getUserAgent()
        ..

    example output: Kodi/17.0-ALPHA1 (X11; Linux x86_64) Ubuntu/15.10 App_Bitness/64
    Version/17.0-ALPHA1-Git:2015-12-23-5770d28
    """
    return ""


def convertLanguage(language: str, format: int) -> str:
    """
    Returns the given language converted to the given format as a string.

    :param language: string either as name in English, two letter code (ISO 639-1), or
        three letter code (ISO 639-2/T(B)
    :param format: format of the returned language string

    ================= ========================================================== 
    Value             Description                                                
    ================= ========================================================== 
    xbmc.ISO_639_1    Two letter code as defined in ISO 639-1                    
    xbmc.ISO_639_2    Three letter code as defined in ISO 639-2/T or ISO 639-2/B 
    xbmc.ENGLISH_NAME Full language name in English (default)                    
    ================= ========================================================== 

    :return: Converted Language string

    @python_v13 New function added.

    Example::

        ..
        language = xbmc.convertLanguage(English, xbmc.ISO_639_2)
        ..
    """
    return ""
