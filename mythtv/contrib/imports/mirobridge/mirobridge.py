#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mirobridge.py   Maintains MythTV database with Miro's downloaded video files.
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform synchronise Miro's video files with MythTV's
#           "Watch Recordings" and MythVideo.
#
#           The source of all video files is from those downloaded my Miro.
#           The source of all cover art and screen shoots are from those downloaded and maintained by
#           Miro.
#           Miro v2.03 or later must be already installed and configured and already capable of
#           downloading videos.
#
# Command line examples:
# See help (-u and -h) options
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mirobridge - Maintains Miro's Video files with MythTV";
__author__="R.D.Vaughan"
__purpose__='''
This python script is intended to synchronise Miro's video files with MythTV's "Watch Recordings" and MythVideo.

The source of all video files are from those downloaded my Miro.
The source of all meta data for the video files is from the Miro data base.
The source of all cover art and screen shots are from those downloaded and maintained by Miro.
Miro v2.0.3 or later must already be installed and configured and capable of downloading videos.
'''

__version__=u"v0.7.0"
# 0.1.0 Initial development
# 0.2.0 Initial Alpha release for internal testing only
# 0.2.1 Fixes from initial alpha test
#       Renamed imported micro python modules
# 0.2.2 Fixed the duplicate video situation when Miro and a Channel feed has an issue which causes
#       the same video to be downloaded multiple time. This situation is now detected and protects
#       the duplicates from being added to either the MythTV or MythVideo.
#       Fixes a few problems with stripping HTML and converting HTML characters in Miro descriptions.
#       Changed the identification of HD and 720 videos to conform to MythTV standards
#       Removed the file "mirobridge_util.py" from the mirobridge distribution as the main Miro
#       distribution file "util.py" is used instead.
#       Changed ALL symlink Miro icons (coverart) to a copied Miro icon file, replace any coverart that
#       is currently a symlink to a copied Miro icon file and check that each MythVideo subdirectory
#       has an actual file that has the proper naming convention that supports storage groups.
#       Use using ImageMagick's utility 'mogrify' to convert Miro's screenshots to true png's (they are
#       really jpg files with a png extension) and reduce their size by 50% for the Watch Recordings
#       screen. Imagemagick is now a requirement to have installed.
#       Fixed a bug when either the Miro video's Channel title or title exceeded MythTV database 128
#       characters limit for their equivalent title and subtitle fields.
#       When Miro has no screen shot for a Video substitute the item icon if it is high enough quality.
# 0.2.3 All subdirectory coverfiles names are changed to "folder".(jpg/png) and gif files are converted.
#       Imagemagick is now mandatory as Miro has cover art which are gif types and must be converted
#       to either jpg or png so they will be recognised as folder coverart for Storage Groups.
#       Added if coverart from Miro is a gif type convert it to a jpg.
#       Removed resizing screenshots by 50% when adding a screen shot to the Watch Recordings screen. As
#       some graphics were already very small.
#       Fixed a bug were a Watch Recordings video was deleted by the user but the graphics files were not
#       also deleted.
#       Converted all mirobridge console messages to proper logger format (critical, error, warning, info)
#       Add changes as Anduin did to ttvdb.py to force 'utf8' output
#       Tested foreign language video's with their foreign language metadata - No problem
# 0.2.4 Added a percentage downloaded message for each item that is downloading - updated every 30 secs.
#       Made Miro update and auto-download the default (no option -d). Added option (-n) to suppress
#       update and download processing.
#       If there is no screenshot then create one with ffmpeg. The size for Watch Recordings is 320 wide
#       and the mythtvideo screenshots are the same size as the video. From this point on this feature
#       will known as the "iamlindoro effect".
#       Fixed a bug where fold covers were being created even if a "folder.png" was already available.
#       As Anduin had done with ttvdb's support *.py files mirobridge's support *.py and example conf
#       files have been moved to a mirobridge subdirectory.
#       Added a check that makes sure that the Miro items are only video files. Audio files are skipped.
#       This is the final version that will support Miro v2.0.3 all higher versions will support
#       Miro v2.5.2 and higher. Except for small bugs this version will no longer be enhanced.
#       Added a small statistics report.
# 0.2.5 Changes required for mirobridge to support Miro v2.5.2 have been made now mirobridge dynamically
#       supports both versions v2.0.3 and v2.5.2
#       Fixed a statistics report bug for the copied to MythVideo totals and add new totals.
#       Fixed a bug where the Miro screen shot was not being resized for the Watch Recordings screen.
#       Fixed a bug when the Miro download time has not been set. In that case use the current date and
#       time. If this is not done the video cannot be deleted from Watch Recordings.
#       Fixed a bug where a video copied to MythVideo gets stranded in the Watch Recordings screen
#       even though the video file has been deleted in Miro.
#       Fixed a bug where a video that has been watched does not get removed from the Watch Recordings
#       screen. This stranded in the Watch Recordings screen even though the video file has been
#       deleted in Miro. This only happened when videos had been watched BUT there were no new videos
#       to add to the Watch Recordings screen.
#       Fixed a typo in the statistics report.
#       Fixed a Miro version check bug
#       Fixed a test environment option bug
#       Changed the "except" statement on imports to "except Exception:" as Anduin did to ttvdb.py
# 0.3.0 Beta release
# 0.3.1 Changed the check for Imagmagick convert utility and ffmpeg as the positive return value
#       is different on some distributions (0 or 1). The fail value is consistent (127).
#       Fixed the environment test option (-t) at times could give incorrect success message
# 0.3.2 Fixed a bug when an empty item description would abort the script on an "extras" request
#       Fixed a bug when the Watched item processing failed message would abort the script
#       Fixed a bug when a user accidental deletes a video file symlink then the video symlink is NOT
#       recreated as it should have been.
#       Fixed a bug where double quotes in the title or subtitle caused issues with file names of graphics
#       Added to the statistics report the total number of Miro-MythVideo videos that will eventually be
#       expired and removed by Miro.
#       Added a info log message with the Miro Bridge version number, which may help in problem analysis.
#       Added trapping and diagnostic messages when the HTML tags could not be removed from a description.
#       Added a check that the installed "pyparsing" python library is at least v1.5.0
#       Added detection of a Miro video deletion though the MythVideo UI. When this occurs
#       Miro is told to also deletes the video, graphics and meta data.
# 0.3.3 Status change from Beta to production. No code changes
# 0.3.4 Fixed when checking for orphaned videometadata records and there were no Miro videometadata
#       records.
#       Added additional detection and restrictions to the supported versions of Miro (minimum v2.0.3)
#       preferrably v2.5.2 or higher.
# 0.3.5 Use the MythVideo.py binding rmMetadata() routine to delete old videometatdata records.
#       Added access checks for all directories that Miro Bridge needs to write
# 0.3.6 Modifications as per requested changes.
# 0.3.7 Fixed a bug with previous modifications that impacted Miro v2.0.3 only
# 0.3.8 Fixed unicode errors with file names
#       Change to assist SG image hunting added the suffix "_coverart, _fanart, _banner,
#       _screenshot" respectively for any copied/created graphics.
# 0.3.9 Fixed an issue when deleting a Miro video and the title/subtitle was not found due to special
#       characters. The search and matching is now more robust.
#       Fixed a bug where file name unicode errors caused an abort when creating screen shots
#       Removed from the mirobrodge.conf file the sections "[tv] and [movies]". This functionality
#       will be added to the Jamu v0.5.0 -MW option.
#       Added a check for locally available banners and fanart files when creating a MythVideo record.
#       This is added as Jamu v0.5.0 option -MW downloads graphics from TVDB and TMDB for Miro videos
#       when available.
#       Modified the check for mirobridge.conf to accomodate the needs of Mythbuntu.
#       Add mythcommflag seektable building for both recordings and mythvideo Miro videos.
# 0.4.0 Fixed an abort where a variable had not been named properly due to a cut and paste error.
# 0.4.1 Added a check that no other instance of Miro Bridge or Miro is already running. If there is
#       then post a critical error message and exit.
#       Do not add the Miro Bridge default banner when a Miro video has no subtitle as it overlaps
#       the title display on MythVideo information pop-ups.
#       Fixed a bug where a folder icon was being recreated even when it already existed.
# 0.4.2 Added a missing import of "htmlentitydefs" which is rarely used in the description/plot XTML parsing
# 0.4.3 Added support for audio type detection (2-channel, 6-channel, ...) that matches changes in ffmpeg.
#       ffmpeg is used in the detection of a video file's audio properties.
#       New Miro Videos added to the Default recordings directory have their names conform to MythTV standards
#       of "CHANID_ISODATETIME.ext". This resolves an obsure bug that caused orphaned screen shot graphics and
#       issues with Miro video deletions from the Watch Recordings screen if MiroBridge was run when the user
#       had MythTV started and in the Watch Recordings screen.
# 0.4.4 Fixed a unicode issue with data read from a subprocess call.
#       Fixed an issue with the check for other instances of mirobridge.py running.
# 0.4.5 Fixed a deletion issue when a Miro video subtitle contained more than 128 characters.
#       Disabled seek table creation as a number of the Miro video types (e.g. mov) do not work in MythTV with
#       seek tables.
# 0.4.6 Changed "original air date" and "air date" to be Miro's item release date. This is more appropriate then
#       using download date as was done previously. Download date is still the fall back of there is no
#       release date.
# 0.4.7 Changed all occurances of "strftime(u'" to "strftime('" as the unicode causes issues with python versions
#       less than 2.6
# 0.4.8 Some Miro "release" date values are not valid. Override with the current date.
# 0.4.9 The ffmpeg SVN (e.g. SVN-r20151) is now outputting additional metadata, skip metadata that cannot be
#       processed.
# 0.5.0 Correct the addition of adding hostnames to videometadata records and the use of relative paths when
#       there is no Videos Storage Group.
#       Added more informative error messages when trying to connect to the MythTV data base
# 0.5.1 Fixed the config "all" options for command line -N and -M and config file sections [watch_then_copy]
#       and [mythvideo_only].
#       Changed return codes from True to 0 and False to 1.
#       Added display of the directories that will be used by MiroBridge and whether they are storage groups.
#       Added file name sanitising logic plus a config file variable to add characters to be replaced by '_'.
#       The config file variable 'file name_char_filter' is required by users who save MiroBridge files on a
#       file system which only supports MS-Windows file naming conventions.
# 0.5.2 Convert to new python bindings and replace all direct mysql data base calls. See ticket #7264
#       Remove the requirement for the MySQLdb python library.
#       Remove all seek table processing as it is not used due to issues with some video file types. This code had been
#       previously disable but the code and related mythcommflag related code has been removed entirely.
#       Initialized new videometadata fields 'releasedate' and 'hash'.
# 0.5.3 Fixed Exception messages
# 0.5.4 Add the command line option (-i) to import an OPML file that was exported from Miro on a different PC.
#       This new option allows configuring Miro channels on a separate PC then importing those channels on
#       a Myth backend without a keyboard. No Miro GUI needs to run on the MythTV backend.
# 0.5.5 Fixed bug #8051 - creating hash value for non-SG Miro video files had an incorrect variable and missing
#       library import.
# 0.5.6 Fixed an abort and subsequent process hang when displaying a critical Miro start-up error
# 0.5.7 The "DeletesFollowLinks" setting is incompatible with MiroBridge processings. A check
#       and termination of MiroBridge with an appropriate error message has been added.
#       Added better system error messages when an IOError exception occurs
# 0.5.8 Add support for Miro version 3.0
# 0.5.9 Update for changes in Python Bindings
# 0.6.0 Fixed a issue when a Miro video's metadata does not have a title. It was being re-added and the
#       database title fields in several records was being left empty.
# 0.6.1 Modifications to support MythTV python bindings changes
# 0.6.2 Trapped possible unicode errors which would hang the MiroBridge process
# 0.6.3 Pull hostname from python bindings instead of socket libraries
# 0.6.4 MythTV python bindings changes
# 0.6.5 Added support for Miro v3.5.x
#       Small internal document changes
# 0.6.6 Fixed screenshot code due to changes in ffmpeg. First
#       noticed in Ubuntu 10.10 (ffmepg v 0.6-4:0.6-2ubuntu6)
# 0.6.7 Added support for Miro v4.0.2 or higher
#       Integrate with the new metadata functionality in recordings. Now users can specify graphics for
#           Miro Channels or set the Miro Channel name to match an entry in ttvdb.com and MythTV will
#           download the artwork automatically.
#       Automatically convert any copied Miro videos with an inetref of '99999999' which was only required
#           for the defunct Jamu script. The category is changed to 'Video Cast" and  the inetref is removed
#           unless there is a matching Recording rule.
#       Silenced verbose output from ffmpeg when creating a screenshot
#       Fixed delOldRecording abort when a Channel sends two videos with identical published date and time.
#           All starttimes are now unique.
#       Fixed aborts caused by bad metadata in Miro (videoFilename)
#       Fixed a minor bug when a video is marked as watched within Miro but was not being removed from
#           "Watched Recordings" until the video expired
#       Removed creation of "folder.png" graphics when creating directories as that is no longer used
#           by MythVideo
#       Fixed the options "-h, --help" command line display
# 0.6.8 Sometimes Miro metadata has no video filename. Skip these invalid videos.
# 0.6.9 Adjust to datetime issues with MythTV v0.26's move to UTC datatimes in DB
# 0.7.0 Fix bug introduced with v0.6.9, ticket reported as #11219 and #11220

examples_txt=u'''
For examples, please see the Mirobridge's wiki page at http://www.mythtv.org/wiki/MiroBridge
'''

# Common function imports
import sys, os, re, locale, subprocess, locale, ConfigParser, codecs, shutil, struct
import fnmatch, string, time, logging, traceback, platform, fnmatch, ConfigParser
from datetime import timedelta
from optparse import OptionParser
from socket import gethostbyname
import formatter
import htmlentitydefs

# Set command line options and arguments
parser = OptionParser(usage=u"%prog usage: mirobridge -huevstdociVHSCWM [parameters]\n")

parser.add_option(  "-e", "--examples", action="store_true", default=False, dest="examples",
                    help=u"Display examples for executing the mirobridge script")
parser.add_option(  "-v", "--version", action="store_true", default=False, dest="version",
                    help=u"Display version and author information")
parser.add_option(  "-s", "--simulation", action="store_true", default=False, dest="simulation",
                    help=u"Simulation (dry run), no files are copied, symlinks created or MythTV data "\
                         u"bases altered. If option (-n) is NOT specified Miro auto downloads WILL take "\
                         u"place. See option (-n) help for details.")
parser.add_option(  "-t", "--testenv", action="store_true", default=False, dest="testenv",
                    help=u"Test that the local environment can run all mirobridge functionality")
parser.add_option(  "-n", "--no_autodownload", action="store_true", default=False, dest="no_autodownload",
                    help=u"Do not perform Miro Channel updates, video expiry and auto-downloadings. "\
                         u"Default is to perform all perform all Channel maintenance features.")
parser.add_option(  "-o", "--nosubdirs", action="store_true", default=False, dest="nosubdirs",
                    help=u"Organise MythVideo's Miro directory WITHOUT Miro channel subdirectories. "\
                         u"The default is to have Channel subdirectories.")
parser.add_option(  "-c", "--channel", metavar="CHANNEL_ID:CHANNEL_NUM", default="", dest="channel",
                    help=u'Specifies the channel id that is used for Miros unplayed recordings. Enter '\
                         u'as "xxxx:yyy". Default is 9999:999. Be warned that once you change the '\
                         u'default channel_id "9999" you must always use this option!')
parser.add_option(  "-i", "--import_opml", metavar="OPMLFILEPATH", default="", dest="import_opml",
                    help=u'Import Miro exported OPML file containing Channel configurations. File '\
                         u'name must be a fully qualified path. This option is exclusive to Miro '\
                         u'v2.5.x and higher.')
parser.add_option(  "-V", "--verbose", action="store_true", default=False, dest="verbose",
                    help=u"Display verbose messages when processing")
parser.add_option(  "-H", "--hostname", metavar="HOSTNAME", default="", dest="hostname",
                    help=u"MythTV Backend hostname mirobridge is to up date")
parser.add_option(  "-S", "--sleeptime", metavar="SLEEP_DELAY_SECONDS", default="", dest="sleeptime",
                    help=u"The amount of seconds to wait for an auto download to start.\nThe default "\
                         u"is 60 seconds, but this may need to be adjusted for slower Internet connections.")
parser.add_option(  "-C", "--addchannel", metavar="ICONFILE_PATH", default="OFF", dest="addchannel",
                    help=u'Add a Miro Channel record to MythTV. This gets rid of the "#9999 #9999" '\
                         u'on the Watch Recordings screen and replaces it with the usual\nthe channel '\
                         u'number and channel name.\nThe default if not overridden by the (-c) option '\
                         u'is channel number 999.\nIf a filename and path is supplied it will be set '\
                         u'as the channels icon. Make sure your override channel number is NOT one of '\
                         u'your current MythTV channel numbers.\nThis option is typically only used '\
                         u'once as there can only be one Miro channel record at a time.')
parser.add_option(  "-N", "--new_watch_copy", action="store_true", default=False, dest="new_watch_copy",
                    help=u'For ALL Miro Channels: Use the "Watch Recording" screen to watch new Miro '\
                         u'downloads then once watched copy the videos, icons, screen shot and metadata '\
                         u'to MythVideo. Once coping is complete delete the video from Miro.\nThis option '\
                         u'overrides any "mirobridge.conf" settings.')
parser.add_option(  "-W", "--watch_only", action="store_true", default=False, dest="watch_only",
                    help=u'For ALL Miro Channels: Only use "Watch Recording" never move any Miro videos '\
                         u'to MythVideo.\nThis option overrides any "mirobridge.conf" settings.')
parser.add_option(  "-M", "--mythvideo_only", action="store_true", default=False, dest="mythvideo_only",
                    help=u'For ALL Miro Channel videos: Copy newly downloaded Miro videos to MythVideo '\
                         u'and removed from Miro. These Miro videos never appear in the MythTV "Watch '\
                         u'Recording" screen.\nThis option overrides any "mirobridge.conf" settings.')


# Global variables
opts, args = parser.parse_args() # Command line arguments and options
local_only = True
dir_dict          = {u'posterdir': u"VideoArtworkDir",     u'bannerdir': u'mythvideo.bannerDir',
                     u'fanartdir': u'mythvideo.fanartDir', u'episodeimagedir': u'mythvideo.screenshotDir',
                     u'mythvideo': u'VideoStartupDir'}
vid_graphics_dirs = {u'default'  : u'', u'mythvideo': u'', u'posterdir'      : u'',
                     u'bannerdir': u'', u'fanartdir': u'', u'episodeimagedir': u'',}
key_trans         = {u'filename'  : u'mythvideo', u'coverfile': u'posterdir',
                     u'banner'    : u'bannerdir', u'fanart'   : u'fanartdir',
                     u'screenshot': u'episodeimagedir'}

graphic_suffix    = {u'default'  : u'',        u'mythvideo': u'',        u'posterdir': u'_coverart',
                     u'bannerdir': u'_banner', u'fanartdir': u'_fanart', u'episodeimagedir': u'_screenshot',}
graphic_path_suffix = u"%s%s%s.%s"
graphic_name_suffix = u"%s%s.%s"

storagegroupnames = {u'Default' :  u'default',  u'Videos'     : u'mythvideo',
                     u'Coverart': u'posterdir', u'Banners'    : u'bannerdir',
                     u'Fanart'  : u'fanartdir', u'Screenshots': u'episodeimagedir'}
storagegroups={} # The gobal dictionary is only populated with the current hosts storage group entries
image_extensions = [u"png", u"jpg", u"bmp"]
simulation = False
verbose = False
ffmpeg = True
channel_id = 9999
channel_num = 999
symlink_filename_format = u"%s - %s"
flat = False                     # The MythVideo Miro directory structure flat or channel subdirectories
download_sleeptime = float(60)    # The default value for the time to wait for auto downloading to start
channel_icon_override = []
channel_watch_only = []
channel_mythvideo_only = {}
channel_new_watch_copy = {}
tv_channels = {}
movie_trailers = []
test_environment = False
requirements_are_met = True        # Used with the (-t) test environment option
imagemagick = True
mythcommflag_recordings = u'%s -c %%s -s "%%s" --rebuild' # or u'mythcommflag -f "%s" --rebuild'
mythcommflag_videos = u'%s --rebuild --video "%%s"'
filename_char_filter = u"/%\000"
emptyTitle = u'_NO_TITLE_From_Miro'
emptySubTitle = u'_NO_SUBTITLE_From_Miro'


# Initalize Report Statistics:
statistics = {u'WR_watched': 0,                     u'Miro_marked_watch_seen': 0,
              u'Miro_videos_deleted': 0,            u'Miros_videos_downloaded': 0,
              u'Miros_MythVideos_video_removed': 0, u'Miros_MythVideos_added': 0,
              u'Miros_MythVideos_copied': 0,        u'Total_unwatched': 0,
              u'Total_Miro_expiring': 0,            u'Total_Miro_MythVideos': 0}


class OutStreamEncoder(object):
    """Wraps a stream with an encoder
    """
    def __init__(self, outstream, encoding=None):
        self.out = outstream
        if not encoding:
            self.encoding = sys.getfilesystemencoding()
        else:
            self.encoding = encoding

    def write(self, obj):
        """Wraps the output stream, encoding Unicode strings with the specified encoding"""
        if isinstance(obj, unicode):
            self.out.write(obj.encode(self.encoding))
        else:
            self.out.write(obj)

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)
# Sub class sys.stdout and sys.stderr as a utf8 stream. Deals with print and stdout unicode issues
sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')

# Create logger
logger = logging.getLogger(u"mirobridge")
logger.setLevel(logging.DEBUG)
# Create console handler and set level to debug
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
# Create formatter
formatter = logging.Formatter(u"%(asctime)s - %(name)s - %(levelname)s - %(message)s")
# Add formatter to ch
ch.setFormatter(formatter)
# Add ch to logger
logger.addHandler(ch)

# The pyparsing python library must be installed version 1.5.0 or higher
try:
    from pyparsing import *
    import pyparsing
    if pyparsing.__version__ < "1.5.0":
        logger.critical(u"The python library 'pyparsing' must be at version 1.5.0 or higher. "\
                        u"Your version is v%s" % pyparsing.__version__)
        sys.exit(1)
except Exception, e:
    logger.critical(u"The python library 'pyparsing' must be installed and be version 1.5.0 or "\
                    u"higher, error(%s)" % e)
    sys.exit(1)
logger.info(u"Using python library 'pyparsing' version %s" % pyparsing.__version__)


# Find out if the MythTV python bindings can be accessed and instances can be created
try:
    #If the MythTV python interface is found, we can insert data directly to MythDB or
    #get the directories to store poster, fanart, banner and episode graphics.
    from MythTV import OldRecorded, Recorded, RecordedProgram, Record, Channel, \
                        MythDB, Video, MythVideo, MythBE, MythError, MythLog
    from MythTV.database import DBDataWrite
    from MythTV.utility import datetime
    mythdb = None
    mythbeconn = None
    try:
        #Create an instance of each: MythDB, MythVideo
        mythdb = MythDB()
    except MythError, e:
        print u'\n! Warning - %s' % e.args[0]
        filename = os.path.expanduser("~")+'/.mythtv/config.xml'
        if not os.path.isfile(filename):
            logger.critical(u'A correctly configured (%s) file must exist\n' % filename)
        else:
            logger.critical(u'Check that (%s) is correctly configured\n' % filename)
        sys.exit(1)
    except Exception, e:
        logger.critical(u"Creating an instance caused an error for one of: MythDB or "\
                        u"MythVideo, error(%s)\n" % e)
        sys.exit(1)
    localhostname = mythdb.gethostname()
    try:
        mythbeconn = MythBE(backend=localhostname, db=mythdb)
    except MythError, e:
        logger.critical(u'MiroBridge must be run on a MythTV backend, error(%s)' % e.args[0])
        sys.exit(1)
except Exception, e:
    logger.critical(u"MythTV python bindings could not be imported, error(%s)" % e)
    sys.exit(1)

# Find out if the Miro python bindings can be accessed and instances can be created
try:
    # Initialize locale as required for Miro v3.x
    try:
        # Setup gconf_name early on so we can load config values
        from miro.plat import utils
        utils.initialize_locale()
    except:
        pass

    # Set up gettext before everything else
    from miro import config             # New for Miro4 (Changed import location)
    from miro import eventloop          # New for Miro4
    from miro import gtcache
    config.load()                       # New for Miro4
    gtcache.init()

    # This fixes some/all problems with Python 2.6 support but should be
    # re-addressed when we have a system with Python 2.6 to test with.
    # (bug 11262)
    if sys.version_info[0] == 2 and sys.version_info[1] > 5:
        import miro.feedparser
        import miro.storedatabase
        sys.modules['feedparser'] = miro.feedparser
        sys.modules['storedatabase'] = miro.storedatabase

    from miro import prefs
    from miro import startup
    from miro import app
    from miro.frontends.cli.events import EventHandler

    # Required for Miro 4 as the configuration calls changed location
    # and additional Miro 4 specific imports are required
    try:
        dummy = app.config.get(prefs.APP_VERSION)
        # A test to see if this is Miro v4 before the version can be read.
        # If there is no exception this is Miro v4
        eventloop.setup_config_watcher()
        from miro import signals
        from miro import messages
        from miro import eventloop
        from miro import feed
        from miro import workerprocess
        from miro.frontends.cli.application import InfoUpdaterCallbackList
        from miro.frontends.cli.application import InfoUpdater
        from miro.plat.renderers.gstreamerrenderer import movie_data_program_info
        miroConfiguration = app.config.get
        from miro import controller
        app.controller = controller.Controller()
    except:
        miroConfiguration = config.get
        pass

except Exception, e:
    logger.critical(u"Importing Miro functions has an issue. Miro must be installed "\
                    u"and functional, error(%s)", e)
    sys.exit(1)

logger.info(u"Miro Bridge version %s with Miro version %s" % \
                    (__version__, miroConfiguration(prefs.APP_VERSION)))
if miroConfiguration(prefs.APP_VERSION) < u"2.0.3":
    logger.critical((u"Your version of Miro (v%s) is not recent enough. Miro v2.0.3 is "\
                     u"the minimum and it is preferred that you upgrade to Miro v2.5.2 or "\
                     u"later.") % miroConfiguration(prefs.APP_VERSION))
    sys.exit(1)

try:
    if miroConfiguration(prefs.APP_VERSION) < u"2.5.2":
        logger.info("Using mirobridge_interpreter_2_0_3")
        from mirobridge.mirobridge_interpreter_2_0_3 import MiroInterpreter
    elif miroConfiguration(prefs.APP_VERSION) < u"3.0":
        logger.info("Using mirobridge_interpreter_2_5_2")
        from mirobridge.mirobridge_interpreter_2_5_2 import MiroInterpreter
    elif miroConfiguration(prefs.APP_VERSION) < u"3.5":
        logger.info("Using mirobridge_interpreter_3_0_0")
        from mirobridge.mirobridge_interpreter_3_0_0 import MiroInterpreter
    elif miroConfiguration(prefs.APP_VERSION) < u"4.0":
        logger.info("Using mirobridge_interpreter_3_5_0")
        from mirobridge.mirobridge_interpreter_3_5_0 import MiroInterpreter
    else:
        logger.info("Using mirobridge_interpreter_4_0_2")
        from mirobridge.mirobridge_interpreter_4_0_2 import MiroInterpreter
    from mirobridge.metadata import MetaData
except Exception, e:
    logger.critical(u"Importing mirobridge functions has failed. At least a 'mirobridge_interpreter' "\
                    u"file that matches your Miro version must be in the subdirectory 'mirobridge'.\n'"\
                    u"e.g. mirobridge_interpreter_2_0_3.py', 'mirobridge_interpreter_2_5_2.py' ... etc, "\
                    u"error(%s)" % e)
    sys.exit(1)

def _can_int(x):
    """Takes a string, checks if it is numeric.
    >>> _can_int("2")
    True
    >>> _can_int("A test")
    False
    """
    if x == None:
        return False
    try:
        int(x)
    except ValueError:
        return False
    else:
        return True
# end _can_int

def displayMessage(message):
    """Displays messages through stdout. Usually used with option (-V) verbose mode.
    returns nothing
    """
    global verbose
    if verbose:
        logger.info(message)
# end _displayMessage

def getExtention(filename):
    """Get the graphic file extension from a filename
    return the file extension from the filename
    """
    (dirName, fileName) = os.path.split(filename)
    (fileBaseName, fileExtension)=os.path.splitext(fileName)
    return fileExtension[1:]
# end getExtention


def sanitiseFileName(name):
    '''Take a file name and change it so that invalid or problematic characters are substituted with a "_"
    return a sanitised valid file name
    '''
    global filename_char_filter
    if name == None or name == u'':
        return u'_'
    for char in filename_char_filter:
        name = name.replace(char, u'_')
    if name[0] == u'.':
        name = u'_'+name[1:]
    return name
# end sanitiseFileName()


def useImageMagick(cmd):
    """ Process graphics files using ImageMagick's utility 'mogrify'.
    >>> useImageMagick('convert screenshot.jpg -resize 50% screenshot.png')
    >>> 0
    >>> -1
    """
    return subprocess.call(u'%s > /dev/null' % cmd, shell=True)
# end useImageMagick()

class singleinstance(object):
    '''
    singleinstance - based on Windows version by Dragan Jovelic this is a Linux
                     version that accomplishes the same task: make sure that
                     only a single instance of an application is running.
    '''

    def __init__(self, pidPath):
        '''
        pidPath - full path/filename where pid for running application is to be
                  stored.  Often this is ./var/<pgmname>.pid
        '''
        from os import kill
        self.pidPath=pidPath
        #
        # See if pidFile exists
        #
        if os.path.exists(pidPath):
            #
            # Make sure it is not a "stale" pidFile
            #
            try:
                pid=int(open(pidPath, 'r').read().strip())
                #
                # Check list of running pids, if not running it is stale so
                # overwrite
                #
                try:
                    kill(pid, 0)
                    pidRunning = 1
                except OSError:
                    pidRunning = 0
                if pidRunning:
                    self.lasterror=True
                else:
                    self.lasterror=False
            except:
                self.lasterror=False
        else:
            self.lasterror=False

        if not self.lasterror:
            #
            # Write my pid into pidFile to keep multiple copies of program from
            # running.
            #
            fp=open(pidPath, 'w')
            fp.write(str(os.getpid()))
            fp.close()

    def alreadyrunning(self):
        return self.lasterror

    def __del__(self):
        if not self.lasterror:
            import os
            os.unlink(self.pidPath)
    # end singleinstance()

def isMiroRunning():
    '''Check if Miro is running. Only one can be running at the same time.
    return True if Miro us already running
    return False if Miro is NOT running
    '''
    try:
        p = subprocess.Popen(u'ps aux | grep "miro.real"', shell=True, bufsize=4096,
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             close_fds=True)
    except:
        return False

    while True:
        data = p.stdout.readline()
        if not data:
            return False
        try:
            data = unicode(data, 'utf8')
        except (UnicodeEncodeError, TypeError):
            pass
        if data.find(u'grep') != -1:
            continue

        if data.find(u'miro.real') != -1:
            logger.critical(u"Miro is already running and therefore Miro Bridge should not be run:")
            logger.critical(u"(%s)" % data)
            break

    return True
 #end isMiroRunning()


# Two routines used for movie title search and matching
def is_punct_char(char):
    '''check if char is punctuation char
    return True if char is punctuation
    return False if char is not punctuation
    '''
    return char in string.punctuation

def is_not_punct_char(char):
    '''check if char is not punctuation char
    return True if char is not punctuation
    return False if char is punctuation
    '''
    return not is_punct_char(char)


def delOldRecorded(chanid, starttime, endtime, title, \
                   subtitle, description):
    '''
    This routine is not supported in the native python bindings as MiroBridge
    uses the oldrecorded table outside of its original intent.
    return nothing
    '''
    sql_cmd = u"""DELETE FROM `mythconverg`.`oldrecorded`
                        WHERE `oldrecorded`.`chanid` = '%s'
                          AND `oldrecorded`.`starttime` = '%s'
                          AND `oldrecorded`.`endtime` = '%s';"""
    sql_del_a_record(sql_cmd % (chanid, set_del_datatime(starttime),
                     set_del_datatime(endtime)))
# end delOldRecorded()


def delRecorded(chanid, starttime):
    '''Just delete a recorded record. Never abort as sometimes a record
    may not exist.
    return nothing
    '''
    sql_cmd = u"""DELETE FROM `mythconverg`.`recorded`
                        WHERE `recorded`.`chanid` = %s
                          AND `recorded`.`starttime` = '%s';"""
    sql_del_a_record(sql_cmd % (chanid, set_del_datatime(starttime)))
# end delRecorded()


def set_del_datatime(rec_time):
    ''' Set the SQL datetime so that the delRecorded and delOldrecorded
    methods use UTC datetime values.
    return rec_time datetime string
    '''
    #
    return rec_time.astimezone(
                        rec_time.UTCTZ()).strftime("%Y-%m-%d %H:%M:%S")
# end set_del_datetime()

def sql_del_a_record(sql_cmd):
    ## Get a MythTV data base cursor
    cursor = mythdb.cursor()
    cursor.execute(sql_cmd)
    cursor.close()
    #
    return


def hashFile(filename):
    '''Create metadata hash values for mythvideo files
    return a hash value
    return u'' if the was an error with the video file or the video file length was zero bytes
    '''
    # Use the MythVideo hashing protocol when the video is in a storage groups
    if filename[0] != u'/':
        hash_value = mythbeconn.getHash(filename, u'Videos')
        if hash_value == u'NULL':
            return u''
        else:
            return hash_value

    # Use a local hashing routine when video is not in a Videos storage group
    # For original code: http://trac.opensubtitles.org/projects/opensubtitles/wiki/HashSourceCodes#Python
    try:
        longlongformat = 'q'  # long long
        bytesize = struct.calcsize(longlongformat)
        f = open(filename, "rb")
        filesize = os.path.getsize(filename)
        hash = filesize
        if filesize < 65536 * 2:    # Video file is too small
               return u''
        for x in range(65536/bytesize):
                buffer = f.read(bytesize)
                (l_value,)= struct.unpack(longlongformat, buffer)
                hash += l_value
                hash = hash & 0xFFFFFFFFFFFFFFFF #to remain as 64bit number
        f.seek(max(0,filesize-65536),0)
        for x in range(65536/bytesize):
                buffer = f.read(bytesize)
                (l_value,)= struct.unpack(longlongformat, buffer)
                hash += l_value
                hash = hash & 0xFFFFFFFFFFFFFFFF
        f.close()
        returnedhash =  "%016x" % hash
        return returnedhash

    except(IOError):    # Accessing to this video file caused and error
        return u''
# end hashFile()


def rtnAbsolutePath(relpath, filetype=u'mythvideo'):
    '''Check if there is a Storage Group for the file type (mythvideo, coverfile, banner, fanart,
    screenshot)    and form an appropriate absolute path and file name.
    return an absolute path and file name
    return the relpath sting if the file does not actually exist in the absolute path location
    '''
    if relpath == None or relpath == u'':
        return relpath

    # There is a chance that this is already an absolute path
    if relpath[0] == u'/':
        return relpath

    if not storagegroups.has_key(filetype):
        return relpath    # The Videos storage group path does not exist at all the metadata entry is useless

    for directory in storagegroups[filetype]:
        abpath = u"%s/%s" % (directory, relpath)
        if os.path.isfile(abpath): # The file must actually exist locally
            return abpath
    else:
        return relpath    # The relative path does not exist at all the metadata entry is useless
# end rtnAbsolutePath


def getStorageGroups():
    '''Populate the storage group dictionary with the host's storage groups.
    return False if there is an error
    '''
    records = mythdb.getStorageGroup(hostname=localhostname)
    if records:
        for record in records:
            if record.groupname in storagegroupnames.keys():
                try:
                    dirname = unicode(record.dirname, 'utf8')
                except (UnicodeDecodeError):
                    logger.error((u"The local Storage group (%s) directory contained\ncharacters "\
                                  u"that caused a UnicodeDecodeError. This storage group has been "\
                                  u"rejected.") % (record.groupname))
                    continue    # Skip any line that has non-utf8 characters in it
                except (UnicodeEncodeError, TypeError):
                    dirname = record.dirname  # assume already unicode and pass unchanged

                # Add a slash if missing to any storage group dirname
                if dirname[-1:] == u'/':
                    storagegroups[storagegroupnames[record.groupname]] = dirname
                else:
                    storagegroups[storagegroupnames[record.groupname]] = dirname+u'/'
            continue

    if len(storagegroups):
        # Verify that each storage group is an existing local directory
        storagegroup_ok = True
        for key in storagegroups.keys():
            if not os.path.isdir(storagegroups[key]):
                logger.critical(u"The Storage group (%s) directory (%s) does not exist" % \
                                        (key, storagegroups[key]))
                storagegroup_ok = False
        if not storagegroup_ok:
            sys.exit(1)
# end getStorageGroups

def getMythtvDirectories():
    """
    Get all video and graphics directories found in the MythTV DB and add them to the dictionary.
    Ignore any MythTV Frontend setting when there is already a storage group configured.
    """
    # Stop processing if this local host has any storage groups
    global localhostname, vid_graphics_dirs, dir_dict, storagegroups, local_only, verbose

    # When there is NO SG for Videos then ALL graphics paths MUST be local paths set in the FE and accessable
    # from the backend
    if storagegroups.has_key(u'mythvideo'):
        local_only = False
        # Pick up storage groups first
        for key in storagegroups.keys():
            vid_graphics_dirs[key] = storagegroups[key]
        for key in dir_dict.keys():
            if key == u'default' or key == u'mythvideo':
                continue
            if not storagegroups.has_key(key):
                # Set fall back graphics directory to Videos
                vid_graphics_dirs[key] = storagegroups[u'mythvideo']
                # Set fall back SG graphics directory to Videos
                storagegroups[key] = storagegroups[u'mythvideo']
    else:
        local_only = True
        if storagegroups.has_key(u'default'):
            vid_graphics_dirs[u'default'] = storagegroups[u'default']

    if local_only:
        logger.warning(u'There is no "Videos" Storage Group set so ONLY MythTV Frontend local '\
                       u'paths for videos and graphics that are accessable from this MythTV '\
                       u'Backend can be used.')

    for key in dir_dict.keys():
        if vid_graphics_dirs[key]:
            continue
        graphics_dir = mythdb.settings[localhostname][dir_dict[key]]
        # Only use path from MythTV if one was found
        if key == u'mythvideo':
            if graphics_dir:
                tmp_directories = graphics_dir.split(u':')
                if len(tmp_directories):
                    for i in range(len(tmp_directories)):
                        tmp_directories[i] = tmp_directories[i].strip()
                        if tmp_directories[i] != u'':
                            if os.path.exists(tmp_directories[i]):
                                if tmp_directories[i][-1] != u'/':
                                    tmp_directories[i]+=u'/'
                                vid_graphics_dirs[key] = tmp_directories[i]
                                break
                            else:
                                 logger.error(u"MythVideo video directory (%s) does not exist(%s)" % \
                                                    (key, tmp_directories[i]))
            else:
                 logger.error(u"MythVideo video directory (%s) is not set" % (key, ))

        if key != u'mythvideo':
            if graphics_dir and os.path.exists(graphics_dir):
                if graphics_dir[-1] != u'/':
                    graphics_dir+=u'/'
                vid_graphics_dirs[key] = graphics_dir
            else:    # There is the chance that MythTv DB does not have a dir
                 logger.error(u"(%s) directory is not set or does not exist(%s)" % (key, dir_dict[key]))

    # Make sure there is a directory set for Videos and other graphics directories on this host
    dir_for_all = True
    for key in vid_graphics_dirs.keys():
        if not vid_graphics_dirs[key]:
            logger.critical(u"There must be a directory for Videos and each graphics type "\
                            u"the (%s) directory is missing." % (key))
            dir_for_all = False
    if not dir_for_all:
        sys.exit(1)

    # Make sure that there is read/write access to all the directories Miro Bridge uses
    access_issue = False
    for key in vid_graphics_dirs.keys():
        if not os.access(vid_graphics_dirs[key], os.F_OK | os.R_OK | os.W_OK):
            logger.critical(u"\nEvery Video and graphics directory must be read/writable for "\
                            u"Miro Bridge to function. There is a permissions issue with (%s)." % \
                                    (vid_graphics_dirs[key], ))
            access_issue = True
    if access_issue:
        sys.exit(1)

    # Print out the video and image directories that will be used for processing
    if verbose:
        dir_types={'posterdir' : "Cover art  ", 'bannerdir': 'Banners    ',
                   'fanartdir' : 'Fan art    ', 'episodeimagedir': 'Screenshots',
                    'mythvideo': 'Video      ', 'default': 'Default    '}
        sys.stdout.write(u"""
==========================================================================================
Listed below are the types and base directories that will be use for processing.
The list reflects your current configuration for the '%s' back end
and whether a directory is a 'SG' (storage group) or not.
Note: All directories are from settings in the MythDB specific to hostname (%s).
------------------------------------------------------------------------------------------
""" % (localhostname, localhostname))
        for key in vid_graphics_dirs.keys():
            sg_flag = 'NO '
            if storagegroups.has_key(key):
                if vid_graphics_dirs[key] == storagegroups[key]:
                    sg_flag = 'YES'
            sys.stdout.write(u"Type: %s - SG-%s - Directory: (%s)\n" % \
                                    (dir_types[key], sg_flag, vid_graphics_dirs[key]))
        sys.stdout.write(\
u"""------------------------------------------------------------------------------------------
If a directory you set from a separate Front end is not displayed it means
that the directory is not accessible from this backend OR
you must add the missing directories using the Front end on this Back end.
Front end settings are host machine specific.
==========================================================================================

""")
    # end getMythtvDirectories()

def setUseroptions():
    """    Change variables through a user supplied configuration file
    abort the script if there are issues with the configuration file values
    """
    global simulation, verbose, channel_icon_override, channel_watch_only, channel_mythvideo_only
    global vid_graphics_dirs, tv_channels, movie_trailers, filename_char_filter

    useroptions=u"~/.mythtv/mirobridge.conf"

    if useroptions[0]==u'~':
        useroptions=os.path.expanduser(u"~")+useroptions[1:]
    if os.path.isfile(useroptions) == False:
        logger.info(
            u"There was no mirobridge configuration file found (%s)" % useroptions)
        return

    cfg = ConfigParser.SafeConfigParser()
    cfg.read(useroptions)
    for section in cfg.sections():
        if section[:5] == u'File ':
            continue
        if section == u'variables':
            for option in cfg.options(section):
                if option == u'filename_char_filter':
                    for char in cfg.get(section, option):
                        filename_char_filter+=char
                    continue
        if section == u'icon_override':
            # Add the Channel names to the array of Channels that are to use their item's icon
            for option in cfg.options(section):
                channel_icon_override.append(option)
            continue
        if section == u'watch_only':
            # Add the Channel names to the array of Channels that will only not be moved to MythVideo
            for option in cfg.options(section):
                if option == u'all miro channels':
                    channel_watch_only = [u'all']
                    break
                else:
                    channel_watch_only.append(filter(is_not_punct_char, option.lower()))
            continue
        if section == u'mythvideo_only':
            # Add the Channel names to the array of Channels that will be moved to MythVideo only
            for option in cfg.options(section):
                if option == u'all miro channels':
                    channel_mythvideo_only[u'all'] = cfg.get(section, option)
                    break
                else:
                    channel_mythvideo_only[filter(is_not_punct_char, option.lower())] = \
                                cfg.get(section, option)
            for key in channel_mythvideo_only.keys():
                if not channel_mythvideo_only[key].startswith(vid_graphics_dirs[u'mythvideo']):
                    logger.critical((u"All Mythvideo only configuration (%s) directories (%s) must "\
                                     u"be a subdirectory of the MythVideo base directory (%s).") % \
                                            (key, channel_mythvideo_only[key],
                                             vid_graphics_dirs[u'mythvideo']))
                    sys.exit(1)
                if channel_mythvideo_only[key][-1] != u'/':
                    channel_mythvideo_only[key]+=u'/'
            continue
        if section == u'watch_then_copy':
            # Add the Channel names to the array of Channels once watched will be copied to MythVideo
            for option in cfg.options(section):
                if option == u'all miro channels':
                    channel_new_watch_copy[u'all'] = cfg.get(section, option)
                    break
                else:
                    channel_new_watch_copy[filter(is_not_punct_char, option.lower())] =\
                                 cfg.get(section, option)
            for key in channel_new_watch_copy.keys():
                if not channel_new_watch_copy[key].startswith(vid_graphics_dirs[u'mythvideo']):
                    logger.critical((u"All 'new->watch->copy' channel (%s) directory (%s) must "\
                                     u"be a subdirectory of the MythVideo base directory (%s).") % \
                                                (key, channel_new_watch_copy[key],
                                                 vid_graphics_dirs[u'mythvideo']))
                    sys.exit(1)
                if channel_new_watch_copy[key][-1] != u'/':
                    channel_new_watch_copy[key]+=u'/'
            continue
# end setUserconfig

def massageDescription(description, extras=False):
    '''Massage the Miro description removing all HTML.
    return the massaged description
    '''

    def unescape(text):
       """Removes HTML or XML character references
          and entities from a text string.
       @param text The HTML (or XML) source text.
       @return The plain text, as a Unicode string, if necessary.
       from Fredrik Lundh
       2008-01-03: input only unicode characters string.
       http://effbot.org/zone/re-sub.htm#unescape-html
       """
       def fixup(m):
          text = m.group(0)
          if text[:2] == u"&#":
             # character reference
             try:
                if text[:3] == u"&#x":
                   return unichr(int(text[3:-1], 16))
                else:
                   return unichr(int(text[2:-1]))
             except ValueError:
                logger.warn(u"Remove HTML or XML character references: Value Error")
                pass
          else:
             # named entity
             # reescape the reserved characters.
             try:
                if text[1:-1] == u"amp":
                   text = u"&amp;amp;"
                elif text[1:-1] == u"gt":
                   text = u"&amp;gt;"
                elif text[1:-1] == u"lt":
                   text = u"&amp;lt;"
                else:
                   logger.info(u"%s" % text[1:-1])
                   text = unichr(htmlentitydefs.name2codepoint[text[1:-1]])
             except KeyError:
                logger.warn(u"Remove HTML or XML character references: keyerror")
                pass
          return text # leave as is
       return re.sub(u"&#?\w+;", fixup, text)

    details = {}
    if not description: # Is there anything to massage
        if extras:
            details[u'plot'] = description
            return details
        else:
            return description

    director_text = u'Director: '
    director_re = re.compile(director_text, re.UNICODE)
    ratings_text = u'Rating: '
    ratings_re = re.compile(ratings_text, re.UNICODE)

    removeText = replaceWith("")
    scriptOpen,scriptClose = makeHTMLTags(u"script")
    scriptBody = scriptOpen + SkipTo(scriptClose) + scriptClose
    scriptBody.setParseAction(removeText)

    anyTag,anyClose = makeHTMLTags(Word(alphas,alphanums+u":_"))
    anyTag.setParseAction(removeText)
    anyClose.setParseAction(removeText)
    htmlComment.setParseAction(removeText)

    commonHTMLEntity.setParseAction(replaceHTMLEntity)

    # first pass, strip out tags and translate entities
    firstPass = (htmlComment | scriptBody | commonHTMLEntity |
                 anyTag | anyClose ).transformString(description)

    # first pass leaves many blank lines, collapse these down
    repeatedNewlines = LineEnd() + OneOrMore(LineEnd())
    repeatedNewlines.setParseAction(replaceWith(u"\n\n"))
    secondPass = repeatedNewlines.transformString(firstPass)
    text = secondPass.replace(u"Link to Catalog\n ",u'')
    text = unescape(text)

    if extras:
        text_lines = text.split(u'\n')
        for index in range(len(text_lines)):
            text_lines[index] = text_lines[index].rstrip()
            index+=1
    else:
        text_lines = [text.replace(u'\n', u' ')]

    if extras:
        description = u''
        for text in text_lines:
            if len(director_re.findall(text)):
                details[u'director'] = text.replace(director_text, u'')
                continue
            # probe the nature [...]Rating: 3.0/5 (1 vote cast)
            if len(ratings_re.findall(text)):
                data = text[text.index(ratings_text):].replace(ratings_text, u'')
                try:
                    number = data[:data.index(u'/')]
                    # HD trailers ratings are our of 5 not 10 like MythTV so must be multiplied by two
                    try:
                        details[u'userrating'] = float(number) * 2
                    except ValueError:
                        details[u'userrating'] = 0.0
                except:
                    details[u'userrating'] = 0.0
                text = text[:text.index(ratings_text)]
            if text.rstrip():
                description+=text+u' '
    else:
        description = text_lines[0].replace(u"[...]Rating:", u"[...] Rating:")

    if extras:
        details[u'plot'] = description.rstrip()
        return details
    else:
        return description
    # end massageDescription()


def getOldrecordedOrphans():
    """Retrieves the Miro oldrecorded records for localhostname. Match them against the Miro recorded
    records and identify any orphaned oldrecorded records. Those records mean a MythTV user deleted the
    Miro video from the Watched Recordings screen or from MythVideo. Delete the orphaned records from
    MythTV plus any left over graphic files.
    return array of oldrecorded dictionary records which are orphaned
    return None if there are no orphaned oldrecorded records
    """
    global simulation, verbose, channel_id, localhostname, vid_graphics_dirs, statistics
    global channel_icon_override
    global graphic_suffix, graphic_path_suffix, graphic_name_suffix

    # Convert any old videmetadata copied videos changing their inetref and category values.
    # Prevents accidental deletions.
    metadata.convertOldMiroVideos()

    recorded_array = list(mythdb.searchRecorded(chanid=channel_id, hostname=localhostname))
    oldrecorded_array = list(mythdb.searchOldRecorded(chanid=channel_id, ))
    videometadata = list(mythdb.searchVideos(category=u'Miro'))

    orphans = []
    for record in oldrecorded_array:
        for recorded in recorded_array:
            if recorded[u'starttime'] == record[u'starttime'] and recorded[u'endtime'] == \
                                         record[u'endtime']:
                break
        else:
            for video in videometadata:
                if video[u'title'] == record[u'title'] and video[u'subtitle'] == record[u'subtitle']:
                    break
            else:
                orphans.append(record)

    for data in orphans:
        if simulation:
            logger.info(u"Simulation: Remove orphaned oldrecorded record (%s - %s)" % \
                            (data[u'title'], data[u'subtitle']))
        else:
            try:
            # Sometimes a channel issues videos with identical publishing (starttime) dates.
            # Try to using additiional details to identify the correct oldrecord.
                delOldRecorded(channel_id, data['starttime'],
                                data['endtime'], data['title'],
                                data['subtitle'], data['description'])
            except:
                pass

            # Attempt a clean up for orphaned recorded video files and/or graphics
            metadata.cleanupVideoAndGraphics(u'%s%s_%s.%s' % \
                                (vid_graphics_dirs[u'default'], channel_id,
                                 data[u'starttime'].strftime('%Y%m%d%H%M%S'), u'png'))

            # Attempt a clean up for orphaned MythVideo files and/or graphics from the Default directory
            metadata.cleanupVideoAndGraphics(u'%s%s - %s.%s' % \
                                (vid_graphics_dirs[u'default'], data[u'title'],
                                 data[u'subtitle'], u'png'))

            # Attempt a clean up for orphaned MythVideo screenshot
            metadata.cleanupVideoAndGraphics(u'%s%s - %s%s.%s' % \
                                (vid_graphics_dirs[u'episodeimagedir'], data[u'title'],
                                 data[u'subtitle'], graphic_suffix[u'episodeimagedir'], u'png'))

            # Remove any unique cover art graphic files
            if data[u'title'].lower() in channel_icon_override:
                metadata.cleanupVideoAndGraphics(u'%s%s - %s%s.%s' % \
                                (vid_graphics_dirs[u'posterdir'], data[u'title'],
                                 data[u'subtitle'], graphic_suffix[u'posterdir'], u'png'))

            displayMessage(u"Removed orphaned Miro video and graphics files (%s - %s)" % \
                                (data[u'title'], data[u'subtitle']))

    return orphans
    # end getOldrecordedOrphans()


def getStartEndTimes(duration, downloadedTime):
    '''Calculate a videos start and end times and isotime for the recorded file name.
    return an array of initialised values if either duration or downloadedTime is invalid
    return an array of the video's start, end times and isotime
    '''
    starttime = datetime.now()
    end = starttime
    start_end = [starttime.strftime('%Y-%m-%d %H:%M:%S'),
                 starttime.strftime('%Y-%m-%d %H:%M:%S'),
                 starttime.strftime('%Y%m%d%H%M%S')]

    if downloadedTime != None:
        try:
            dummy = downloadedTime.strftime('%Y-%m-%d')
        except ValueError:
            downloadedTime = datetime.now()
        end = downloadedTime+timedelta(seconds=duration)
        start_end[0] = downloadedTime.strftime('%Y-%m-%d %H:%M:%S')
        start_end[1] = end.strftime('%Y-%m-%d %H:%M:%S')
        start_end[2] = downloadedTime.strftime('%Y%m%d%H%M%S')

    # Check if there is a Miro video with an identical start time.
    # If there is incremement that start and end times by one until unique.
    while True:
        if not len(list(mythdb.searchOldRecorded(chanid=channel_id, starttime=start_end[0]))):
            break
        starttime = starttime + timedelta(0,1)
        end = end + timedelta(0,1)
        start_end[0] = starttime.strftime('%Y-%m-%d %H:%M:%S')
        start_end[1] = end.strftime('%Y-%m-%d %H:%M:%S')
        start_end[2] = starttime.strftime('%Y%m%d%H%M%S')
        continue

    return start_end
    # end getStartEndTimes()

def setSymbolic(filename, storagegroupkey, symbolic_name, allow_symlink=False):
    '''Convert the file into a symbolic name according to it's storage group (there may be
    no storage group for the key). Check if a symbolic link exists and replace the link with a copy of
    the file. except for video files. Abort if the file does not exist.
    return the symbolic link to the file
    '''
    global simulation, verbose, storagegroups, vid_graphics_dirs
    global graphic_suffix, graphic_path_suffix, graphic_name_suffix
    global local_only

    if not os.path.isfile(filename):
        logger.error(u"The file (%s) must exist to create a symbolic link" % filename)
        return None

    ext = getExtention(filename)
    if ext.lower() == u'm4v':
        ext = u'mpg'

    convert = False
    if ext.lower() in [u'gif', u'jpeg', u'JPG', ]: # convert graphics to standard jpg and png types
        ext = u'jpg'
        convert = True

    if storagegroupkey in storagegroups.keys() and storagegroupkey == u'default':
        sym_filepath = graphic_path_suffix % (storagegroups[storagegroupkey], symbolic_name,
                                              graphic_suffix[storagegroupkey], ext)
        sym_filename = graphic_name_suffix % (symbolic_name, graphic_suffix[storagegroupkey], ext)
    elif storagegroupkey in storagegroups.keys() and not local_only:
        sym_filepath = graphic_path_suffix % (storagegroups[storagegroupkey], symbolic_name,
                                              graphic_suffix[storagegroupkey], ext)
        sym_filename = graphic_name_suffix % (symbolic_name, graphic_suffix[storagegroupkey], ext)
    else:
        sym_filepath = graphic_path_suffix % (vid_graphics_dirs[storagegroupkey], symbolic_name,
                                              graphic_suffix[storagegroupkey], ext)
        sym_filename = sym_filepath

    if allow_symlink:
        if os.path.isfile(os.path.realpath(sym_filepath)):
            return sym_filename
    else:
        if os.path.isfile(os.path.realpath(sym_filepath)) and not os.path.islink(sym_filepath):
            return sym_filename # file already exists
    try: # Try to remove a broken symbolic link
        os.remove(sym_filepath)
    except OSError:
        pass

    if simulation:
        if allow_symlink:
            logger.info(u"Simulation: Used file (%s) to create symlink as (%s)" % \
                                (filename, sym_filepath))
        else:
            logger.info(u"Simulation: Used file (%s) copy as (%s)" % (filename, sym_filepath))
        return sym_filename

    try:
        if allow_symlink:
            os.symlink(filename, sym_filepath)
            displayMessage(u"Used file (%s) to created symlink (%s)" % (filename, sym_filepath))
        else:
            try:    # Every file but video files copied to support Storage groups
                if convert:
                    useImageMagick(u'convert "%s" "%s"' % (filename, sym_filepath))
                    displayMessage(u"Convert and copy Miro file (%s) to file (%s)" % \
                                        (filename, sym_filepath))
                else:
                    shutil.copy2(filename, sym_filepath)
                    displayMessage(u"Copied Miro file (%s) to file (%s)" % (filename, sym_filepath))
            except OSError, e:
                logger.critical((u"Trying to copy the Miro file (%s) to the file (%s).\nError(%s)"\
                                 u"\nThis maybe a permissions error (mirobridge.py does not have "\
                                 u"permission to write to the directory).") % \
                                        (filename ,sym_filepath, e))
                sys.exit(1)
    except OSError, e:
        logger.critical((u"Trying to create the Miro file (%s) symlink (%s).\nError(%s)\nThis "\
                         u"maybe a permissions error (mirobridge.py does not have permission to "\
                         u"write to the directory).") % (sym_filepath, e))
        sys.exit(1)

    return sym_filename
    # end setSymbolic()


def createOldRecordedRecord(item, start, end, inetref):
    '''Using the details from a Miro item record create a MythTV oldrecorded record
    return an array of MythTV of a full initialised oldrecorded record dictionaries
    '''
    global localhostname, simulation, verbose, storagegroups, ffmpeg, channel_id

    tmp_oldrecorded={}

    # Create the oldrecorded dictionary
    tmp_oldrecorded[u'chanid'] = channel_id
    tmp_oldrecorded[u'starttime'] = start
    tmp_oldrecorded[u'endtime'] = end
    tmp_oldrecorded[u'title'] = item[u'channelTitle']
    tmp_oldrecorded[u'subtitle'] = item[u'title']
    tmp_oldrecorded[u'category'] = u'Miro'
    tmp_oldrecorded[u'station'] = u'MIRO'
    tmp_oldrecorded[u'inetref'] = inetref
    tmp_oldrecorded[u'season'] = item[u'season']
    tmp_oldrecorded[u'episode'] = item[u'episode']

    try:
        tmp_oldrecorded[u'description'] = massageDescription(item[u'description'])
    except TypeError:
        tmp_oldrecorded[u'description'] = item[u'description']
    return tmp_oldrecorded
    # end createOldRecordedRecord()


def createRecordedRecords(item):
    '''Using the details from a Miro item record create a MythTV recorded record
    return an array of MythTV full initialised recorded and recordedprogram record dictionaries
    '''
    global localhostname, simulation, verbose, storagegroups, ffmpeg, channel_id

    tmp_recorded={}
    tmp_recordedprogram={}

    # Get any graphics that already exist
    graphics = metadata.getMetadata(sanitiseFileName(item[u'channelTitle']))

    ffmpeg_details = metadata.getVideoDetails(item[u'videoFilename'])
    start_end = getStartEndTimes(ffmpeg_details[u'duration'], item[u'downloadedTime'])

    if item[u'releasedate'] == None:
        item[u'releasedate'] = item[u'downloadedTime']
    try:
        dummy = item[u'releasedate'].strftime('%Y-%m-%d')
    except ValueError:
        item[u'releasedate'] = item[u'downloadedTime']

    # Create the recorded dictionary
    tmp_recorded[u'chanid'] = channel_id
    tmp_recorded[u'starttime'] = start_end[0]
    tmp_recorded[u'endtime'] = start_end[1]
    tmp_recorded[u'title'] = item[u'channelTitle']
    tmp_recorded[u'subtitle'] = item[u'title']
    tmp_recorded[u'season'] = item[u'season']
    tmp_recorded[u'episode'] = item[u'episode']
    tmp_recorded[u'inetref'] = graphics[u'inetref']
    try:
        tmp_recorded[u'description'] = massageDescription(item[u'description'])
    except TypeError:
        print
        print u"Channel title(%s) subtitle(%s)" % (item[u'channelTitle'], item[u'title'])
        print u"The 'massageDescription()' function could not remove HTML and XML tags from:"
        print u"Description (%s)\n\n" % item[u'description']
        tmp_recorded[u'description'] = item[u'description']
    tmp_recorded[u'category'] = u'Miro'
    tmp_recorded[u'hostname'] = localhostname
    tmp_recorded[u'lastmodified'] = tmp_recorded[u'endtime']
    tmp_recorded[u'filesize'] = item[u'size']
    if item[u'releasedate'] != None:
        tmp_recorded[u'originalairdate'] = item[u'releasedate'].strftime('%Y-%m-%d')

    basename = setSymbolic(item[u'videoFilename'], u'default', u"%s_%s" % \
                                    (channel_id, start_end[2]), allow_symlink=True)
    if basename != None:
        tmp_recorded[u'basename'] = basename
    else:
        logger.critical(u"The file (%s) must exist to create a recorded record" % \
                                    item[u'videoFilename'])
        sys.exit(1)

    tmp_recorded[u'progstart'] = start_end[0]
    tmp_recorded[u'progend'] = start_end[1]

    # Create the recordedpropgram dictionary
    tmp_recordedprogram[u'chanid'] = channel_id
    tmp_recordedprogram[u'starttime'] = start_end[0]
    tmp_recordedprogram[u'endtime'] = start_end[1]
    tmp_recordedprogram[u'title'] = item[u'channelTitle']
    tmp_recordedprogram[u'subtitle'] = item[u'title']
    try:
        tmp_recordedprogram[u'description'] = massageDescription(item[u'description'])
    except TypeError:
        tmp_recordedprogram[u'description'] = item[u'description']

    tmp_recordedprogram[u'category'] = u"Miro"
    tmp_recordedprogram[u'category_type'] = u"series"
    if item[u'releasedate'] != None:
        tmp_recordedprogram[u'airdate'] = item[u'releasedate'].strftime('%Y')
        tmp_recordedprogram[u'originalairdate'] = item[u'releasedate'].strftime('%Y-%m-%d')
    tmp_recordedprogram[u'stereo'] = ffmpeg_details[u'stereo']
    tmp_recordedprogram[u'hdtv'] = ffmpeg_details[u'hdtv']
    tmp_recordedprogram[u'audioprop'] = ffmpeg_details[u'audio']
    tmp_recordedprogram[u'videoprop'] = ffmpeg_details[u'video']

    return [tmp_recorded, tmp_recordedprogram,
            createOldRecordedRecord(item, start_end[0], start_end[1], graphics[u'inetref'])]
    # end  createRecordedRecord()


def createVideometadataRecord(item):
    '''Using the details from a Miro item create a MythTV videometadata record
    return an dictionary of MythTV an initialised videometadata record
    '''
    global localhostname, simulation, verbose, storagegroups, ffmpeg, channel_id
    global vid_graphics_dirs, channel_icon_override, flat, image_extensions
    global local_only

    ffmpeg_details = metadata.getVideoDetails(item[u'videoFilename'])
    start_end = getStartEndTimes(ffmpeg_details[u'duration'], item[u'downloadedTime'])

    sympath = u'Miro'
    if not flat:
        sympath+=u"/%s" % item[u'channelTitle']

    # Get any graphics that already exist
    graphics = metadata.getMetadata(sanitiseFileName(item[u'channelTitle']))

    videometadata = {}

    videometadata[u'title'] = item[u'channelTitle']
    videometadata[u'subtitle'] = item[u'title']
    videometadata[u'inetref'] = graphics[u'inetref']
    videometadata[u'season'] = item[u'season']
    videometadata[u'episode'] = item[u'episode']

    try:
        details = massageDescription(item[u'description'], extras=True)
    except TypeError:
        print
        print u"MythVideo-Channel title(%s) subtitle(%s)" % \
                    (item[u'channelTitle'], item[u'title'])
        print u"The 'massageDescription()' function could not remove HTML and XML tags from:"
        print u"Description (%s)\n\n" % item[u'description']
        details = {u'plot': item[u'description']}

    for key in details.keys():
        videometadata[key] = details[key]

    if item[u'releasedate'] == None:
        item[u'releasedate'] = item[u'downloadedTime']
    try:
        dummy = item[u'releasedate'].strftime('%Y-%m-%d')
    except ValueError:
        item[u'releasedate'] = item[u'downloadedTime']

    if item[u'releasedate'] != None:
        videometadata[u'year'] = item[u'releasedate'].strftime('%Y')
        videometadata[u'releasedate'] = item[u'releasedate'].strftime('%Y-%m-%d')
    videometadata[u'length'] = ffmpeg_details[u'duration']/60
    if item.has_key(u'copied'):
        videometadata[u'category'] = u'Video Cast'
    else:
        videometadata[u'category'] = u'Miro'

    if not item.has_key(u'copied'):
        videofile = setSymbolic(item[u'videoFilename'], u'mythvideo', "%s/%s - %s" % \
                            (sympath, sanitiseFileName(item[u'channelTitle']),
                             sanitiseFileName(item[u'title'])), allow_symlink=True)
        if videofile != None:
            videometadata[u'filename'] = videofile
            if not local_only and videometadata[u'filename'][0] != u'/':
                videometadata[u'host'] = localhostname.lower()
        else:
            logger.critical(u"The file (%s) must exist to create a videometadata record" % \
                                    item[u'videoFilename'])
            sys.exit(1)
    else:
        videometadata[u'filename'] = item[u'videoFilename']
        if not local_only and videometadata[u'filename'][0] != u'/':
            videometadata[u'host'] = localhostname.lower()

    videometadata[u'hash'] = hashFile(videometadata[u'filename'])

    if not item.has_key(u'copied'):
        if graphics['coverart']:
            videometadata[u'coverfile'] = graphics['coverart']
        elif item[u'channel_icon'] and not item[u'channelTitle'].lower() in channel_icon_override:
            filename = setSymbolic(item[u'channel_icon'], u'posterdir', u"%s" % \
                                (sanitiseFileName(item[u'channelTitle'])))
            if filename != None:
                videometadata[u'coverfile'] = filename
        else:
            if item[u'item_icon']:
                filename = setSymbolic(item[u'item_icon'], u'posterdir', u"%s - %s" % \
                                        (sanitiseFileName(item[u'channelTitle']),
                                         sanitiseFileName(item[u'title'])))
                if filename != None:
                    videometadata[u'coverfile'] = filename
    else:
        videometadata[u'coverfile'] = item[u'channel_icon']

    if not item.has_key(u'copied'):
        if item[u'screenshot']:
            filename = setSymbolic(item[u'screenshot'], u'episodeimagedir', u"%s - %s" % \
                                        (sanitiseFileName(item[u'channelTitle']),
                                         sanitiseFileName(item[u'title'])))
            if filename != None:
                videometadata[u'screenshot'] = filename
    else:
        if item[u'screenshot']:
            videometadata[u'screenshot'] = item[u'screenshot']

    if graphics['banner']:
        videometadata[u'banner'] = graphics['banner']
    if graphics['fanart']:
        videometadata[u'fanart'] = graphics['fanart']

    return [videometadata, createOldRecordedRecord(item, start_end[0], start_end[1],
            graphics[u'inetref'])]
    # end createVideometadataRecord()


def createChannelRecord(icon, channel_id, channel_num):
    '''
    Create the optional Miro "channel" record as it makes the Watch Recordings screen look better.
    return True if the record was created and written successfully
    abort if the processing failed
    '''
    global localhostname, simulation, verbose, vid_graphics_dirs

    if icon != u"":
        if not os.path.isfile(icon):
            logger.critical((u'The Miro channel icon file (%s) does not exist.\nThe variable '\
                             u'needs to be a fully qualified file name and path.\ne.g. '\
                             u'./mirobridge.py -C "/path to the channel icon/miro_channel_icon.'\
                             u'jpg"') % (icon))
            sys.exit(1)

    data={}
    data['chanid'] = channel_id
    data['channum'] = str(channel_num)
    data['freqid'] = str(channel_num)
    data['atsc_major_chan'] = int(channel_num)
    data['icon'] = u''
    if icon != u'':
        data['icon'] = icon
    data['callsign'] = u'Miro'
    data['name'] = u'Miro'
    data['last_record'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    if simulation:
        logger.info(u"Simulation: Create Miro channel record channel_id(%d) and channel_num(%d)" % \
                            (channel_id, channel_num))
        logger.info(u"Simulation: Channel icon file(%s)" % (icon))
    else:
        try:
            Channel().create(data)
        except MythError, e:
            logger.critical((u"Failed writing the Miro channel record. Most likely the Channel "\
                             u"Id and number already exists.\nUse MythTV set up program "\
                             u"(mythtv-setup) to alter or remove the offending channel.\nYou "\
                             u"specified Channel ID (%d) and Channel Number (%d), error(%s)") % \
                                        (channel_id, channel_num, e.args[0]))
            sys.exit(1)
    return True
    # end createChannelRecord(icon)


def checkVideometadataFails(record, flat):
    '''
    Verify that the real path exists for both video and graphics for this MythVideo Miro record.
    return False if there were no failures
    return True if a failure was found
    '''
    global localhostname, verbose, vid_graphics_dirs, key_trans

    for field in key_trans.keys():
        if not record[field]:
            continue
        if record[field] == u'No Cover':
            continue
        if not record[u'host'] or record[field][0] == u'/':
            if not os.path.isfile(record[field]):
                return True
        else:
            if not os.path.isfile(vid_graphics_dirs[key_trans[field]]+record[field]):
                return True
    return False
     # end checkVideometadataFails()


def createMiroMythVideoDirectory():
    '''If the "Miro" directory does not exist in MythVideo then create it.
    abort if there is an issue creating the directory
    '''
    global localhostname, vid_graphics_dirs, storagegroups, channel_id, flat, simulation, verbose

    # Check that a MIRO video directory exists
    # if not then create the dir and add symbolic link to cover/file or icon
    miro = u'Miro'
    miro_path = vid_graphics_dirs[u'mythvideo']+miro
    if not os.path.isdir(miro_path):
        try:
            if simulation:
                logger.info(u"Simulation: Create Miro Mythvideo directory (%s)" % (miro_path,))
            else:
                try:
                    os.mkdir(miro_path)
                except OSError, e:
                    logger.critical((u"Create Miro Mythvideo directory (%s).\nError(%s)\n" \
                                     u"This may be due to a permissions error.") % \
                                            (miro_path, e))
                    sys.exit(1)
        except OSError, e:
            logger.critical((u"Creation of MythVideo 'Miro' directory (%s) failed.\nError(%s)"\
                             u"\nThis may be due to a permissions error.") % (miro_path, e))
            sys.exit(1)
    # end createMiroMythVideoDirectory()

def createMiroChannelSubdirectory(item):
    '''Create the Miro Channel subdirectory in MythVideo
    abort if the subdirectory cannot be made
    '''
    global localhostname, vid_graphics_dirs, storagegroups, channel_id, flat, simulation, verbose

    miro = u'Miro'
    path = u"%s%s/%s" % (vid_graphics_dirs[u'mythvideo'], miro,
                         sanitiseFileName(item[u'channelTitle']))

    if simulation:
        logger.info(u"Simulation: Make subdirectory(%s)" % (path))
    else:
        if not os.path.isdir(path):
            try:
                os.mkdir(path)
            except OSError, e:
                logger.critical((u"Creation of MythVideo 'Miro' subdirectory path (%s) failed."\
                                 u"\nError(%s)\nThis may be due to a permissions error.") % \
                                        (path, e))
                sys.exit(1)
    # end createMiroChannelSubdirectory()


def getPlayedMiroVideos():
    '''From the MythTV database recorded records identify all "played" Miro Video files.
    return None if there were either no Miro recorded records or none that were in "watched" status
    return an array of subtitles of those Miro video files that were "watched"
    '''
    global localhostname, vid_graphics_dirs, storagegroups, verbose, channel_id, statistics

    filenames=[]
    recorded = list(mythdb.searchRecorded(chanid=channel_id, hostname=localhostname))
    for record in recorded:
        if record[u'watched'] == 0:    # Skip if the video has NOT been watched
            continue
        try:
            filenames.append(os.path.realpath(storagegroups[u'default']+record[u'basename']))
            statistics[u'WR_watched']+=1
        except OSError, e:
            logger.info(u"Miro video file has been removed (%s) outside of mirobridge\nError(%s)" % \
                                (storagegroups[u'default']+record[u'basename'], e))
            continue
        displayMessage(u"Miro video (%s) (%s) has been marked as watched in MythTV." % \
                                (record[u'title'], record[u'subtitle']))
    if len(filenames):
        return filenames
    else:
        return None
    # end getPlayedMiroVideos()

def updateMythRecorded(items):
    '''
    Add and delete MythTV (Watch Recordings) Miro recorded records. Add and delete symbolic links
    to coverart/Miro icons.
    Abort if processing failed
    return True if processing was successful
    '''
    global localhostname, vid_graphics_dirs, storagegroups, channel_id, simulation, imagemagick
    global graphic_suffix, graphic_path_suffix, graphic_name_suffix

    if not items: # There may not be any new items but a clean up of existing recorded may be required
        items = []
    items_copy = list(items)

    # Deal with existing Miro videos already in the MythTV data base
    recorded = list(mythdb.searchRecorded(chanid=channel_id, hostname=localhostname))
    for record in recorded:
        if storagegroups.has_key(u'default'):
            sym_filepath = u"%s%s" % (storagegroups[u'default'], record[u'basename'])
        else:
            sym_filepath = u"%s%s" % (vid_graphics_dirs[u'default'], record[u'basename'])
        # Remove any videos that were marked as viewed within Miro but NOT MythTV
        remove = False
        for item in items:
            if item[u'channelTitle'] == record[u'title'] and item[u'title'] == record[u'subtitle']:
                break
        else:
            remove = True
        # Remove any Miro related "watched" recordings (symlink, recorded records)
        # Or remove any Miro related with broken symbolic video links
        if record[u'watched'] == 1 or not os.path.isfile(os.path.realpath(sym_filepath)):
            remove = True
        if remove:
            displayMessage(u"Removing watched Miro recording (%s) (%s)" % \
                                    (record[u'title'], record[u'subtitle']))
            # Remove the database recorded and oldrecorded records
            if simulation:
                logger.info(u"Simulation: Remove recorded/recordedprogram/oldrecorded records and "\
                            u"associated Miro Video file for chanid(%s), starttime(%s)" % \
                                    (record['chanid'], record['starttime']))
            else:
                try:
                    # Attempting to clean up an recorded record and
                    # its associated video file (miro symlink)
                    rtn = delRecorded(record['chanid'], record['starttime'])
                except MythError, e:
                    pass

                # Clean up for recorded video files and/or graphics
                metadata.cleanupVideoAndGraphics(u'%s%s_%s.%s' % \
                            (vid_graphics_dirs[u'default'], record['chanid'],
                             record[u'starttime'].strftime('%Y%m%d%H%M%S'), u'png'))

                try: # Attempting to clean up an orphaned oldrecorded record which may or may not exist
                    rtn = delOldRecorded(record['chanid'], record['starttime'],
                                          record['endtime'], record['title'],
                                          record['subtitle'], record['description'])
                except Exception, e:
                    pass

    recorded = list(mythdb.searchRecorded(chanid=channel_id, hostname=localhostname))
    for record in recorded:    # Skip any item already in MythTV data base
        for item in items:
            if item[u'channelTitle'] == record[u'title'] and item[u'title'] == record[u'subtitle']:
                items_copy.remove(item)
                break

    # Add new Miro unwatched videos to MythTV'd data base
    for item in items_copy:
        # Do not create records for Miro video files when Miro has a corrupt or missing file name
        if item[u'videoFilename'] == None:
            continue
        # Do not create records for Miro video files that do not exist
        if not os.path.isfile(os.path.realpath(item[u'videoFilename'])):
            continue
        if not os.path.isfile(os.path.realpath(item[u'videoFilename'])):
            continue # Do not create records for Miro video files that do not exist
        records = createRecordedRecords(item)
        if records:
            if simulation:
                logger.info(u"Simulation: Added recorded and recordedprogram records for "\
                            u"(%s - %s)" % (item[u'channelTitle'], item[u'title'],))
            else:
                try:
                    Recorded().create(records[0])
                    RecordedProgram().create(records[1])
                    OldRecorded().create(records[2])
                except MythError, e:
                    logger.warning(u"Inserting recorded/recordedprogram/oldrecorded records "\
                                   u"non-critical error (%s) for (%s - %s)" % \
                                            (e.args[0], item[u'channelTitle'], item[u'title']))
        else:
            logger.critical(u"Creation of recorded/recordedprogram/oldrecorded record data for "\
                            u"(%s - %s)" % (item[u'channelTitle'], item[u'title'],))
            sys.exit(1)

        if item[u'channel_icon']: # Add Cover art link to channel icon
            ext = getExtention(item[u'channel_icon'])
            coverart_filename = u"%s%s%s.%s" % (vid_graphics_dirs[u'posterdir'],
                                                sanitiseFileName(item[u'channelTitle']),
                                                graphic_suffix[u'posterdir'], ext)
            if not os.path.isfile(os.path.realpath(coverart_filename)): # Make sure it does not exist
                if simulation:
                    logger.info(u"Simulation: Remove symbolic link(%s)" % (coverart_filename,))
                else:
                    try:    # Clean up broken symbolic link
                        os.remove(coverart_filename)
                    except OSError:
                        pass
                if simulation:
                    logger.info(u"Simulation: Create icon file(%s) cover art file(%s)" % \
                                        (item[u'channel_icon'], coverart_filename))
                else:
                    try:
                        shutil.copy2(item[u'channel_icon'], coverart_filename)
                        displayMessage((u"Copied a Miro Channel Icon file (%s) to MythTV as "\
                                        u"file (%s).") % (item[u'channel_icon'], coverart_filename))
                    except OSError, e:
                        logger.critical((u"Copying an icon file(%s) to coverart file(%s) failed."\
                                         u"\nError(%s)\nThis may be due to a permissions error.") % \
                                                 (item[u'channel_icon'], coverart_filename, e))
                        sys.exit(1)

        if item[u'screenshot'] and imagemagick: # Add Miro screen shot to 'Default' recordings directory
            screenshot_recorded = u"%s%s.png" % (vid_graphics_dirs[u'default'], records[0][u'basename'])
            if not os.path.isfile(screenshot_recorded): # Make sure it does not exist
                if simulation:
                    logger.info(u"Simulation: Create screenshot file(%s) as(%s)" % \
                                        (item[u'screenshot'], screenshot_recorded))
                else:
                    try:
                        demensions = u''
                        try:
                            demensions = takeScreenShot(item[u'videoFilename'],
                                                        screenshot_recorded,
                                                        size_limit=True,
                                                        just_demensions=False)
                        except:
                            pass
                        if demensions:
                            demensions = u"-size %s" % demensions
                        useImageMagick(u'convert "%s" %s "%s"' % \
                                            (item[u'screenshot'], demensions,
                                             screenshot_recorded))
                        displayMessage((u"Used a Miro Channel screenshot file (%s) to\ncreate "\
                                        u"using ImageMagick the MythTV Watch Recordings screen "\
                                        u"shot file\n(%s).") % \
                                                (item[u'screenshot'], screenshot_recorded))
                    except OSError, e:
                        logger.critical((u"Creating screenshot file(%s) as(%s) failed.\nError"\
                                         u"(%s)\nThis may be due to a permissions error.") % \
                                                (item[u'screenshot'], screenshot_recorded, e))
                        sys.exit(1)
        else:
            screenshot_recorded = u"%s%s.png" % \
                                        (vid_graphics_dirs[u'default'], records[0][u'basename'])
            try:
                takeScreenShot(item[u'videoFilename'], screenshot_recorded, size_limit=True)
            except:
                pass

    return True
    # updateMythRecorded()

def updateMythVideo(items):
    '''Add and delete MythVideo records for played Miro Videos. Add and delete symbolic links
    to Miro Videos, to coverart/Miro icons, banners and Miro screenshots and fanart.
    NOTE: banner and fanart graphics were provided with the script and are used only if present.
    Abort if processing failed
    return True if processing was successful
    '''
    global localhostname, vid_graphics_dirs, storagegroups, channel_id, flat, simulation, verbose
    global channel_watch_only, statistics
    global graphic_suffix, graphic_path_suffix, graphic_name_suffix
    global local_only

    if not items: # There may not be any new items but a clean up of existing records may be required
        items = []
    # Check that a MIRO video directory exists
    # if not then create the dir and add symbolic link to cover/file or icon
    createMiroMythVideoDirectory()

    # Remove any Miro Mythvideo records which the video or graphics paths are broken
    records = list(mythdb.searchVideos(category=u'Miro'))
    statistics[u'Total_Miro_MythVideos'] = len(records)
    for record in records: # Count the Miro-MythVideos that Miro is expiring or has saved
        if record[u'filename'][0] == u'/':
            if os.path.islink(record[u'filename']) and os.path.isfile(record[u'filename']):
                statistics[u'Total_Miro_expiring']+=1
        elif record[u'host'] and storagegroups.has_key(u'mythvideo'):
            if os.path.islink(storagegroups[u'mythvideo']+record[u'filename']) and \
                        os.path.isfile(storagegroups[u'mythvideo']+record[u'filename']):
                statistics[u'Total_Miro_expiring']+=1
    for record in records:
        if checkVideometadataFails(record, flat):
            delete = False
            if os.path.islink(record[u'filename']): # Only delete video files if they are symlinks
                if not record[u'host'] or record[u'filename'][0] == '/':
                    if not os.path.isfile(record[u'filename']):
                        delete = True
                else:
                    if not os.path.isfile(vid_graphics_dirs[key_trans[field]]+record[u'filename']):
                        delete = True
            else:
                if not os.path.isfile(record[u'filename']):
                    delete = True
            if delete: # Only delete video files if they are symlinks
                if simulation:
                    logger.info(u"Simulation: DELETE videometadata for intid = %s" % \
                                        (record[u'intid'],))
                    logger.info(u"Simulation: DELETE oldrecorded for title(%s), subtitle(%s)" % \
                                        (record[u'title'], record[u'subtitle']))
                else:
                    rtn = Video(record[u'intid'], db=mythdb).delete()
                    try: # An orphaned oldrecorded record may not exist
                        for oldrecorded in mythdb.searchOldRecorded(title=record[u'title'],
                                                                    subtitle=record[u'subtitle'] ):
                            rtn = delOldRecorded(channel_id,
                                                oldrecorded['starttime'],
                                                oldrecorded['endtime'],
                                                oldrecorded['title'],
                                                oldrecorded['subtitle'],
                                                oldrecorded['description'])
                    except Exception, e:
                        pass
                statistics[u'Total_Miro_MythVideos']-=1
                # Remove video file
                metadata.deleteFile(record[u'filename'], record[u'host'], u'mythvideo')
            if record[u'screenshot']: # Remove any associated Screenshot
                metadata.deleteFile(record[u'screenshot'], record[u'host'], u'episodeimagedir')
            # Remove any unique cover art graphic files
            if record[u'title'].lower() in channel_icon_override:
                metadata.deleteFile(record[u'coverfile'], record[u'host'], u'posterdir')

    if not items: # There may not be any new items to add to MythVideo
        return True
    # Reread Miro Mythvideo videometadata records
    # Remove the matching videometadata record from array of items
    items_copy = list(items)
    records = list(mythdb.searchVideos(category=u'Miro'))
    for record in records:
        for item in items:
            if item[u'channelTitle'] == record[u'title'] and item[u'title'] == record[u'subtitle']:
                try:
                    items_copy.remove(item)
                except ValueError:
                    logger.info((u"Video (%s - %s) was found multiple times in list of (watched "\
                                 u"and/or saved) items from Miro - skipping") % \
                                        (item[u'channelTitle'], item[u'title']))
                    pass
                break

    for item in items: # Remove any items that are for a Channel that does not get MythVideo records
        if filter(is_not_punct_char, item[u'channelTitle'].lower()) in channel_watch_only:
            try:    # Some items may have already been removed, let those passed
                items_copy.remove(item)
            except ValueError:
                pass
    # Add Miro videos that remain in the item list
    # If not a flat directory check if title directory exists and add icon symbolic link as coverfile
    for item in items_copy:
        if not flat and not item.has_key(u'copied'):
            createMiroChannelSubdirectory(item)
        if not item[u'screenshot']: # If there is no screen shot then create one
            screenshot_mythvideo = u"%s%s - %s%s.jpg" % \
                            (vid_graphics_dirs[u'episodeimagedir'],
                             sanitiseFileName(item[u'channelTitle']),
                             sanitiseFileName(item[u'title']),
                             graphic_suffix[u'episodeimagedir'])
            try:
                result = takeScreenShot(item[u'videoFilename'], screenshot_mythvideo, size_limit=False)
            except:
                result = None
            if result != None:
                item[u'screenshot'] = screenshot_mythvideo
        tmp_array = createVideometadataRecord(item)
        videometadata = tmp_array[0]
        oldrecorded = tmp_array[1]
        if simulation:
            logger.info(u"Simulation: Create videometadata record for (%s - %s)" % \
                                            (item[u'channelTitle'], item[u'title']))
        else:  # Check for duplicates
            if not local_only and videometadata[u'filename'][0] != u'/':
                intid = list(mythdb.searchVideos(exactfile=videometadata[u'filename'],
                                                 host=localhostname.lower()))
            else:
                intid = list(mythdb.searchVideos(exactfile=videometadata[u'filename']))

            if intid == []: # Check for an empty array
                try:
                    intid = Video(db=mythdb).create(videometadata).intid
                except MythError, e:
                    logger.critical(u"Adding Miro video to MythVideo (%s - %s) failed for (%s)." % \
                                            (item[u'channelTitle'], item[u'title'], e.args[0]))
                    sys.exit(1)
                if not item.has_key(u'copied'):
                    try:
                        OldRecorded().create(oldrecorded)
                    except MythError, e:
                        logger.critical(u"Failed writing the oldrecorded record for(%s)" % (e.args[0]))
                        sys.exit(1)
                if videometadata[u'filename'][0] == u'/':
                    cmd = mythcommflag_videos % videometadata[u'filename']
                elif videometadata[u'host'] and storagegroups[u'mythvideo']:
                    cmd = mythcommflag_videos % \
                                ((storagegroups[u'mythvideo']+videometadata[u'filename']))
                statistics[u'Miros_MythVideos_added']+=1
                statistics[u'Total_Miro_expiring']+=1
                statistics[u'Total_Miro_MythVideos']+=1
                displayMessage(u"Added Miro video to MythVideo (%s - %s)" % \
                                        (videometadata[u'title'], videometadata[u'subtitle']))
            else:
                sys.stdout.write(u'')
                displayMessage(\
u"""Skipped adding a duplicate Miro video to MythVideo:
(%s - %s)
Sometimes a Miro channel has the same video downloaded multiple times.
This is a Miro/Channel web site issue and often rectifies itself overtime.
""" % (videometadata[u'title'], videometadata[u'subtitle']))

    return True
    # end updateMythVideo()

def printStatistics():
    global statistics

    # Print statistics
    sys.stdout.write(u"""

-------------------Statistics--------------------
Number of Watch Recording's watched...... (% 5d)
Number of Miro videos marked as seen..... (% 5d)
Number of Miro videos deleted............ (% 5d)
Number of New Miro videos downloaded..... (% 5d)
Number of Miro/MythVideo's removed....... (% 5d)
Number of Miro/MythVideo's added......... (% 5d)
Number of Miro videos copies to MythVideo (% 5d)
-------------------------------------------------
Total Unwatched Miro/Watch Recordings.... (% 5d)
Total Miro/MythVideo videos to expire.... (% 5d)
Total Miro/MythVideo videos.............. (% 5d)
-------------------------------------------------

""" % (statistics[u'WR_watched'],  statistics[u'Miro_marked_watch_seen'],
       statistics[u'Miro_videos_deleted'], statistics[u'Miros_videos_downloaded'],
       statistics[u'Miros_MythVideos_video_removed'], statistics[u'Miros_MythVideos_added'],
       statistics[u'Miros_MythVideos_copied'], statistics[u'Total_unwatched'],
       statistics[u'Total_Miro_expiring'], statistics[u'Total_Miro_MythVideos'], ))
    # end printStatistics()


# Main script processing starts here
def main():
    """Support mirobridge from the command line
    returns True
    """
    global localhostname, simulation, verbose, storagegroups, ffmpeg, channel_id, channel_num
    global flat, download_sleeptime, channel_watch_only, channel_mythvideo_only, channel_new_watch_copy
    global vid_graphics_dirs, imagemagick, statistics, requirements_are_met
    global graphic_suffix, graphic_path_suffix, graphic_name_suffix
    global mythcommflag_recordings, mythcommflag_videos
    global local_only, metadata
    global parser, opts, args

    if opts.examples:                    # Display example information
        sys.stdout.write(examples_txt+'\n')
        sys.exit(0)

    if opts.version:        # Display program information
        sys.stdout.write(u"\nTitle: (%s); Version: description(%s); Author: (%s)\n%s\n" % (
        __title__, __version__, __author__, __purpose__ ))
        sys.exit(0)

    if opts.testenv:
        test_environment = True
    else:
        test_environment = False

    # Verify that Miro is not currently running
    if isMiroRunning():
        sys.exit(1)

    # Verify that only None or one of the mutually exclusive (-W), (-M) and (-N) options is being used
    x = 0
    if opts.new_watch_copy: x+=1
    if opts.watch_only: x+=1
    if opts.mythvideo_only: x+=1
    if opts.import_opml: x+=1
    if x > 1:
        logger.critical(u"The (-W), (-M), (-N) and (-i) options are mutually exclusive, "\
                        u"so only one can be specified at a time.")
        sys.exit(1)

    # Set option related global variables
    simulation = opts.simulation
    verbose = opts.verbose
    if opts.hostname:    # Override localhostname if the user specified an hostname
        localhostname = opts.hostname

    # Validate settings

    ## Video base directory and current version and revision numbers
    base_video_dir = miroConfiguration(prefs.MOVIES_DIRECTORY)
    miro_version_rev = u"%s r%s" % (miroConfiguration(prefs.APP_VERSION),
                                    miroConfiguration(prefs.APP_REVISION_NUM))

    displayMessage(u"Miro Version (%s)" % (miro_version_rev))
    displayMessage(u"Base Miro Video Directory (%s)" % (base_video_dir,))
    logger.info(u'')

    # Verify Miro version sufficent and Video file configuration correct.
    if not os.path.isdir(base_video_dir):
        logger.critical(u"The Miro Videos directory (%s) does not exist." % str(base_video_dir))
        if test_environment:
            requirements_are_met = False
        else:
            sys.exit(1)

    if miroConfiguration(prefs.APP_VERSION) < u"2.0.3":
        logger.critical((u"The installed version of Miro (%s) is too old. It must be at least "\
                         u"v2.0.3 or higher.") % miroConfiguration(prefs.APP_VERSION))
        if test_environment:
            requirements_are_met = False
        else:
            sys.exit(1)

    # Miro 4.0.1 has a critical bug that effects MiroBridge. Miro must be upgraded.
    if miroConfiguration(prefs.APP_VERSION) == u"4.0.1":
        logger.critical((u"The installed version of Miro (%s) must be upgraded to Miro version "\
                         u"4.0.2 or higher.") % miroConfiguration(prefs.APP_VERSION))
        if test_environment:
            requirements_are_met = False
        else:
            sys.exit(1)

    # Verify that the import opml option can be used
    if opts.import_opml:
        if miroConfiguration(prefs.APP_VERSION) < u"2.5.2":
            logger.critical(u"The OPML import option requires Miro v2.5.2 or higher your Miro "\
                            u"(%s) is too old." % miroConfiguration(prefs.APP_VERSION))
            if test_environment:
                requirements_are_met = False
            else:
                sys.exit(1)
        if not os.path.isfile(opts.import_opml):
            logger.critical(u"The OPML import file (%s) does not exist" % opts.import_opml)
            if test_environment:
                requirements_are_met = False
            else:
                sys.exit(1)
        if len(opts.import_opml) > 5:
            if not opts.import_opml[:-4] != '.opml':
                logger.critical(u"The OPML import file (%s) must had a file extention of '.opml'" % \
                                        opts.import_opml)
                if test_environment:
                    requirements_are_met = False
                else:
                    sys.exit(1)
        else:
            logger.critical(u"The OPML import file (%s) must had a file extention of '.opml'" % \
                                        opts.import_opml)
            if test_environment:
                requirements_are_met = False
            else:
                sys.exit(1)

    # Get storage groups
    if getStorageGroups() == False:
        logger.critical(u"Retrieving storage groups from the MythTV data base failed")
        if test_environment:
            requirements_are_met = False
        else:
            sys.exit(1)
    elif not u'default' in storagegroups.keys():
        logger.critical(u"There must be a 'Default' storage group")
        if test_environment:
            requirements_are_met = False
        else:
            sys.exit(1)

    if opts.channel:
        channel = opts.channel.split(u':')
        if len(channel) != 2:
            logger.critical((u"The Channel (%s) must be in the format xxx:yyy with x and "\
                             u"y all numeric.") % str(opts.channel))
            if test_environment:
                requirements_are_met = False
            else:
                sys.exit(1)
        elif not _can_int(channel[0]) or  not _can_int(channel[1]):
            logger.critical(u"The Channel_id (%s) and Channel_num (%s) must be numeric." % \
                                    (channel[0], channel[1]))
            if test_environment:
                requirements_are_met = False
            else:
                sys.exit(1)
        else:
            channel_id = int(channel[0])
            channel_num = int(channel[1])

    if opts.sleeptime:
        if not _can_int(opts.sleeptime):
            logger.critical(u"Auto-download sleep time (%s) must be numeric." % str(opts.sleeptime))
            if test_environment:
                requirements_are_met = False
            else:
                sys.exit(1)
        else:
            download_sleeptime = float(opts.sleeptime)

    getMythtvDirectories()    # Initialize all the Video and graphics directory dictionary

    if opts.nosubdirs: # Did the user want a flat MythVideo "Miro" directory structure?
        flat = True

    # Get the values in the mirobridge.conf configuration file
    setUseroptions()

    if opts.watch_only:
        # ALL Miro videos will only be viewed in the MythTV "Watch Recordings" screen
        channel_watch_only = [u'all']

    if opts.mythvideo_only: # ALL Miro videos will be copied to MythVideo and removed from Miro
        channel_mythvideo_only = {u'all': vid_graphics_dirs[u'mythvideo']+u'Miro/'}

    # Once watched ALL Miro videos will be copied to MythVideo and removed from Miro
    if opts.new_watch_copy:
        channel_new_watch_copy = {u'all': vid_graphics_dirs[u'mythvideo']+u'Miro/'}

    # Verify that "Mythvideo Only" and "New-Watch-Copy" channels do not clash
    if len(channel_mythvideo_only) and len(channel_new_watch_copy):
        for key in channel_mythvideo_only.keys():
            if key in channel_new_watch_copy.keys():
                logger.critical((u'The Miro Channel (%s) cannot be used as both a "Mythvideo '\
                                 u'Only" and "New-Watch-Copy" channel.') % key)
                if test_environment:
                    requirements_are_met = False
                else:
                    sys.exit(1)

    # Verify that ImageMagick is installed
    ret = useImageMagick(u"convert -version")
    if ret < 0 or ret > 1:
        logger.critical(u"ImageMagick must be installed, graphics cannot be resized or "\
                        u"converted to the required graphics format (e.g. jpg and or png)")
        if test_environment:
            requirements_are_met = False
        else:
            sys.exit(1)

    # Verify that the "DeletesFollowLinks" setting is not set to the character '1'
    if mythdb.settings.NULL.DeletesFollowLinks == '1':
        logger.critical(u'The MythTV back end setting "Follow symbolic links when deleting '\
                        u'files" is checked and it is incompatible with MiroBridge processing. '\
                        u'It must be unchecked it to use MiroBridge.\nTo uncheck this setting '\
                        u'start "mythtv-setup" or with Mythbuntu start "MythTV Backend Setup" '\
                        u'and then General->Miscellaneous Settings and uncheck the "Follow '\
                        u'symbolic links when deleting files" setting')
        if test_environment:
            requirements_are_met = False
        else:
            sys.exit(1)

    # Initialize class with the metadata methods
    metadata = MetaData(mythdb,
                Video,
                Record,
                storagegroups,
                vid_graphics_dirs,
                channel_id,
                ffmpeg,
                logger,
                simulation,
                verbose,
                )

    if opts.testenv:        # All tests passed
        metadata.getVideoDetails(u"") # Test that ffmpeg is available
        if ffmpeg and requirements_are_met:
            logger.info(u"The environment test passed !\n\n")
            sys.exit(0)
        else:
            logger.critical(u"The environment test FAILED. See previously displayed error messages!")
            sys.exit(1)

    if opts.addchannel != u'OFF':    # Add a Miro Channel record - Should only be done once
        createChannelRecord(opts.addchannel, channel_id, channel_num)
        logger.info(u"The Miro Channel record has been successfully created !\n\n")
        sys.exit(0)


    ###########################################
    # Mainlogic for all Miro bridge and MythTV
    ###########################################

    #
    # Start the Miro Front and Backend - This allows mirobridge to execute actions on the Miro backend
    #
    displayMessage(u"Starting Miro Frontend and Backend")
    startup.initialize(miroConfiguration(prefs.THEME_NAME))
    if miroConfiguration(prefs.APP_VERSION) > u"4.0": # Only required for Miro 4
        app.info_updater = InfoUpdater()
    app.cli_events = EventHandler()
    app.cli_events.connect_to_signals()

    if miroConfiguration(prefs.APP_VERSION) > u"4.0": # Only required for Miro 4
        startup.install_first_time_handler(app.cli_events.handle_first_time)

    startup.startup()
    app.cli_events.startup_event.wait()
    if app.cli_events.startup_failure:
        logger.critical(u"Starting Miro Frontend and Backend failed: (%s)\n(%s)" % \
                            (app.cli_events.startup_failure[0], app.cli_events.startup_failure[1]))
        app.controller.shutdown()
        time.sleep(5) # Let the shutdown processing complete
        sys.exit(1)

    if miroConfiguration(prefs.APP_VERSION) > u"4.0": # Only required for Miro 4
        app.movie_data_program_info = movie_data_program_info
        messages.FrontendStarted().send_to_backend()

    app.cli_interpreter = MiroInterpreter()
    if opts.verbose:
        app.cli_interpreter.verbose = True
    else:
        app.cli_interpreter.verbose = False
    app.cli_interpreter.simulation = opts.simulation
    app.cli_interpreter.videofiles = []
    app.cli_interpreter.downloading = False
    app.cli_interpreter.icon_cache_dir = miroConfiguration(prefs.ICON_CACHE_DIRECTORY)
    app.cli_interpreter.imagemagick = imagemagick
    app.cli_interpreter.statistics = statistics
    if miroConfiguration(prefs.APP_VERSION) < u"2.5.0":
        app.renderer = app.cli_interpreter
    elif miroConfiguration(prefs.APP_VERSION) > u"4.0": # Only required for Miro 4
        pass
    else:
        app.movie_data_program_info = app.cli_interpreter.movie_data_program_info

    #
    # Attempt to import an opml file
    #
    if opts.import_opml:
        results = 0
        try:
            app.cli_interpreter.do_mythtv_import_opml(opts.import_opml)
            time.sleep(30) # Let the Miro backend process the OPML file before shutting down
        except Exception, e:
            logger.critical(u"Import of OPML file (%s) failed, error(%s)." % (opts.import_opml, e))
            results = 1
        # Gracefully close the Miro database and shutdown the Miro Front and Back ends
        app.controller.shutdown()
        time.sleep(5) # Let the shutdown processing complete
        sys.exit(results)

    #
    # Optionally Update Miro feeds and
    # download any "autodownloadable" videos which are pending
    #
    if not opts.no_autodownload:
        if opts.verbose:
            app.cli_interpreter.verbose = False
        app.cli_interpreter.do_mythtv_getunwatched(u'')
        before_download = len(app.cli_interpreter.videofiles)
        if opts.verbose:
            app.cli_interpreter.verbose = True
        if miroConfiguration(prefs.APP_VERSION) < u"4.0":
            # Miro 4 automatically refreshes feeds and downloads
            app.cli_interpreter.do_mythtv_update_autodownload(u'')
        time.sleep(download_sleeptime)
        firsttime = True
        while True:
            app.cli_interpreter.do_mythtv_check_downloading(u'')
            if app.cli_interpreter.downloading:
                time.sleep(30)
                firsttime = False
                continue
            elif firsttime:
                time.sleep(download_sleeptime)
                firsttime = False
                continue
            else:
                break
        if opts.verbose:
            app.cli_interpreter.verbose = False
        app.cli_interpreter.do_mythtv_getunwatched(u'')
        after_download = len(app.cli_interpreter.videofiles)
        statistics[u'Miros_videos_downloaded'] = after_download - before_download
        if opts.verbose:
            app.cli_interpreter.verbose = True

    # Deal with orphaned oldrecorded records.
    # These records indicate that the MythTV user deleted the video from the Watched Recordings screen
    # or from MythVideo
    # These video items must also be deleted from Miro
    videostodelete = getOldrecordedOrphans()
    if len(videostodelete):
        displayMessage(u"Starting Miro delete of videos deleted in the MythTV "\
                       u"Watched Recordings screen.")
        for video in videostodelete:
            # Completely remove the video and item information from Miro
            app.cli_interpreter.do_mythtv_item_remove([video[u'title'], video[u'subtitle']])

    #
    # Collect the set of played Miro video files
    #
    app.cli_interpreter.videofiles = getPlayedMiroVideos()

    #
    # Updated the played status of items
    #
    if app.cli_interpreter.videofiles:
        displayMessage(u"Starting Miro update of watched MythTV videos")
        app.cli_interpreter.do_mythtv_updatewatched(u'')

    #
    # Get the unwatched videos details from Miro
    #
    app.cli_interpreter.do_mythtv_getunwatched(u'')
    unwatched = app.cli_interpreter.videofiles

    #
    # Get the watched videos details from Miro
    #
    app.cli_interpreter.do_mythtv_getwatched(u'')
    watched = app.cli_interpreter.videofiles

    #
    # Massage empty titles and subtitles from Miro
    #
    for item in unwatched:
        # Deal with empty titles and subtitles from Miro
        if not item[u'channelTitle']:
            item[u'channelTitle'] = emptyTitle
        if not item[u'title']:
            item[u'title'] = emptySubTitle
    for item in watched:
        # Deal with empty titles and subtitles from Miro
        if not item[u'channelTitle']:
            item[u'channelTitle'] = emptyTitle
        if not item[u'title']:
            item[u'title'] = emptySubTitle

    #
    # Remove any duplicate Miro videoes from the unwatched or watched list of Miro videos
    # This means that Miro has duplicates due to a Miro/Channel website issue
    # These videos should not be added to the MythTV Watch Recordings screen
    #
    unwatched_copy = []
    for item in unwatched:
        unwatched_copy.append(item)
    for item in unwatched_copy: # Check for a duplicate against already watched Miro videos
        for x in watched:
            if item[u'channelTitle'] == x[u'channelTitle'] and item[u'title'] == x[u'title']:
                try:
                    unwatched.remove(item)
                    # Completely remove this duplicate video and item information from Miro
                    app.cli_interpreter.do_mythtv_item_remove(item[u'videoFilename'])
                    displayMessage((u"Skipped adding a duplicate Miro video to the MythTV "\
                                    u"Watch Recordings screen (%s - %s) which is already in "\
                                    u"MythVideo.\nSometimes a Miro channel has the same video "\
                                    u"downloaded multiple times.\nThis is a Miro/Channel web "\
                                    u"site issue and often rectifies itself overtime.") % \
                                            (item[u'channelTitle'], item[u'title']))
                except ValueError:
                    pass
    duplicates = []
    for item in unwatched_copy:
        dup_flag = 0
        for x in unwatched: # Check for a duplicate against un-watched Miro videos
            if item[u'channelTitle'] == x[u'channelTitle'] and item[u'title'] == x[u'title']:
                dup_flag+=1
        if dup_flag > 1:
            for x in duplicates:
                if item[u'channelTitle'] == x[u'channelTitle'] and item[u'title'] == x[u'title']:
                    break
            else:
                duplicates.append(item)

    for duplicate in duplicates:
        try:
            unwatched.remove(duplicate)
            # Completely remove this duplicate video and item information from Miro
            app.cli_interpreter.do_mythtv_item_remove(duplicate[u'videoFilename'])
            displayMessage((u"Skipped adding a Miro video to the MythTV Watch Recordings "\
                            u"screen (%s - %s) as there are duplicate 'new' video items."\
                            u"\nSometimes a Miro channel has the same video downloaded "\
                            u"multiple times.\nThis is a Miro/Channel web site issue and "\
                            u"often rectifies itself overtime.") % \
                                    (duplicate[u'channelTitle'], duplicate[u'title']))
        except ValueError:
            pass

    #
    # Deal with any Channel videos that are to be copied and removed from Miro
    #
    copy_items = []
    # Copy unwatched and watched Miro videos (all or only selected Channels)
    if u'all' in channel_mythvideo_only.keys():
        for array in [watched, unwatched]:
            for item in array:
                copy_items.append(item)
    elif len(channel_mythvideo_only):
        for array in [watched, unwatched]:
            for video in array:
                if filter(is_not_punct_char, video[u'channelTitle'].lower()) in \
                            channel_mythvideo_only.keys():
                    copy_items.append(video)
    # Copy ONLY watched Miro videos (all or only selected Channels)
    if u'all' in channel_new_watch_copy.keys():
        for video in watched:
            copy_items.append(video)
    elif len(channel_new_watch_copy):
        for video in watched:
            if filter(is_not_punct_char, video[u'channelTitle'].lower()) in \
                            channel_new_watch_copy.keys():
                copy_items.append(video)

    channels_to_copy = {}
    for key in channel_mythvideo_only.keys():
        channels_to_copy[key] = channel_mythvideo_only[key]
    for key in channel_new_watch_copy.keys():
        channels_to_copy[key] = channel_new_watch_copy[key]

    for video in copy_items:
        if channels_to_copy.has_key('all'):
            copy_dir = u"%s%s/" % (channels_to_copy['all'], sanitiseFileName(video[u'channelTitle']))
        else:
            copy_dir = channels_to_copy[filter(is_not_punct_char, video[u'channelTitle'].lower())]

        # Create the subdirectories to copy the video into
        if not os.path.isdir(copy_dir):
            if simulation:
                logger.info(u"Simulation: Creating the MythVideo directory (%s)." % (copy_dir))
            else:
                os.makedirs(copy_dir)

        # Copy the Miro video file
        # This filename is needed later for deleting in Miro
        save_video_filename = video[u'videoFilename']
        ext = getExtention(video[u'videoFilename'])
        if ext.lower() == u'm4v':
            ext = u'mpg'
        filepath = u"%s%s - %s.%s" % (copy_dir, sanitiseFileName(video[u'channelTitle']),
                                      sanitiseFileName(video[u'title']), ext)
        if simulation:
            logger.info(u"Simulation: Copying the Miro video (%s) to the MythVideo directory (%s)." % \
                                (video[u'videoFilename'], filepath))
        else:
            try:    # Miro video copied into a MythVideo directory
                shutil.copy2(video[u'videoFilename'], filepath)
                statistics[u'Miros_MythVideos_copied']+=1
                if u'mythvideo' in storagegroups.keys() and not local_only:
                    video[u'videoFilename'] = filepath.replace(storagegroups[u'mythvideo'], u'')
                else:
                    video[u'videoFilename'] = filepath
            except Exception, e:
                logger.critical((u"Copying the Miro video (%s) to the MythVideo directory (%s)."\
                                 u"\n         This maybe a permissions error (mirobridge.py does "\
                                 u"not have permission to write to the directory), error(%s)") % \
                                            (video[u'videoFilename'], filepath, e))
                # Gracefully close the Miro database and shutdown the Miro Front and Back ends
                app.controller.shutdown()
                time.sleep(5) # Let the shutdown processing complete
                sys.exit(1)

        # Copy the Channel or item's icon
        if video[u'channel_icon'] and not video[u'channelTitle'].lower() in channel_icon_override:
            pass
        else:
            if video[u'item_icon']:
                video[u'channel_icon'] = video[u'item_icon']
        # Get any graphics that already exist
        graphics = metadata.getMetadata(sanitiseFileName(video[u'channelTitle']))
        if video[u'channel_icon'] and graphics['coverart'] == u'':
            ext = getExtention(video[u'channel_icon'])
            if video[u'channelTitle'].lower() in channel_icon_override:
                filepath = u"%s%s - %s%s.%s" % (vid_graphics_dirs[u'posterdir'],
                                                sanitiseFileName(video[u'channelTitle']),
                                                sanitiseFileName(video[u'title']),
                                                graphic_suffix[u'posterdir'], ext)
            else:
                filepath = u"%s%s%s.%s" % (vid_graphics_dirs[u'posterdir'],
                                           sanitiseFileName(video[u'channelTitle']),
                                           graphic_suffix[u'posterdir'], ext)
            # There may already be a Channel icon available
            # or it is a symlink which needs to be replaced
            if not os.path.isfile(filepath) or os.path.islink(filepath):
                if simulation:
                    logger.info((u"Simulation: Copying the Channel Icon (%s) to the poster "\
                                 u"directory (%s).") % (video[u'channel_icon'], filepath))
                else:
                    try:    # Miro Channel icon copied into a MythVideo directory
                        try: # Remove any old symlink file
                            os.remove(filepath)
                        except OSError:
                            pass
                        shutil.copy2(video[u'channel_icon'], filepath)
                        if u'posterdir' in storagegroups.keys() and not local_only:
                            video[u'channel_icon'] = filepath.replace(storagegroups[u'posterdir'], u'')
                        else:
                            video[u'channel_icon'] = filepath
                    except Exception, e:
                        logger.critical((u"Copying the Channel Icon (%s) to the poster directory "\
                                         u"(%s).\n         This maybe a permissions error "\
                                         u"(mirobridge.py does not have permission to write to the "\
                                         u"directory), error(%s)") % \
                                    (video[u'channel_icon'], filepath, e))
                        # Gracefully close the Miro database and shutdown the Miro Front and Back ends
                        app.controller.shutdown()
                        time.sleep(5) # Let the shutdown processing complete
                        sys.exit(1)
            else:
                if u'posterdir' in storagegroups.keys() and not local_only:
                    video[u'channel_icon'] = filepath.replace(storagegroups[u'posterdir'], u'')
                else:
                    video[u'channel_icon'] = filepath
        else:
            video[u'channel_icon'] = graphics['coverart']

        # There may already be a Screenshot available or it is a symlink which needs to be replaced
        if video[u'screenshot']:
            ext = getExtention(video[u'screenshot'])
            filepath = u"%s%s - %s%s.%s" % (vid_graphics_dirs[u'episodeimagedir'],
                                            sanitiseFileName(video[u'channelTitle']),
                                            sanitiseFileName(video[u'title']),
                                            graphic_suffix[u'episodeimagedir'], ext)
        else:
            filepath = u''

        if not os.path.isfile(filepath) or os.path.islink(filepath):
            if video[u'screenshot']:
                if simulation:
                    logger.info((u"Simulation: Copying the Screenshot (%s) to the Screenshot "\
                                 u"directory (%s).") % (video[u'screenshot'], filepath))
                else:
                    try:    # Miro Channel icon copied into a MythVideo directory
                        try: # Remove any old symlink file
                            os.remove(filepath)
                        except OSError:
                            pass
                        shutil.copy2(video[u'screenshot'], filepath)
                        displayMessage(u"Copied Miro screenshot file (%s) to MythVideo (%s)" % \
                                                (video[u'screenshot'], filepath))
                        if u'episodeimagedir' in storagegroups.keys() and not local_only:
                            video[u'screenshot'] = filepath.replace(storagegroups[u'episodeimagedir'],
                                                                    u'')
                        else:
                            video[u'screenshot'] = filepath
                    except Exception, e:
                        logger.critical((u"Copying the Screenshot (%s) to the Screenshot directory "\
                                         u"(%s).\n         This maybe a permissions error "\
                                         u"(mirobridge.py does not have permission to write to the "\
                                         u"directory), error(%s)") % \
                                (video[u'screenshot'], filepath, e))
                        # Gracefully close the Miro database and shutdown the Miro Front and Back ends
                        app.controller.shutdown()
                        time.sleep(5) # Let the shutdown processing complete
                        sys.exit(1)
        elif video[u'screenshot']:
            if u'episodeimagedir' in storagegroups.keys() and not local_only:
                video[u'screenshot'] = filepath.replace(storagegroups[u'episodeimagedir'], u'')
            else:
                video[u'screenshot'] = filepath
        video[u'copied'] = True     # Mark this video item as being copied

        # Completely remove the video and item information from Miro
        app.cli_interpreter.do_mythtv_item_remove(save_video_filename)


    # Gracefully close the Miro database and shutdown the Miro Front and Back ends
    app.controller.shutdown()
    time.sleep(5) # Let the shutdown processing complete

    #
    # Add and delete MythTV (Watch Recordings) Miro recorded records
    # Add and remove symlinks for Miro video files
    #

    # Check if the user does not want any channels Added to the "Watch Recordings" screen
    if channel_mythvideo_only.has_key(u'all'):
        for video in unwatched:
            watched.append(video)
        unwatched = []
    else:
        if len(channel_mythvideo_only):
            unwatched_copy = []
            for video in unwatched:
                if not filter(is_not_punct_char, video[u'channelTitle'].lower()) in \
                            channel_mythvideo_only.keys():
                    unwatched_copy.append(video)
                else:
                    watched.append(video)
            unwatched = unwatched_copy

    statistics[u'Total_unwatched'] = len(unwatched)
    if not len(unwatched):
        displayMessage(u"There are no Miro unwatched video items to add as MythTV Recorded videos.")
    if not updateMythRecorded(unwatched):
        logger.critical(u"Updating MythTV Recording with Miro video files failed." % \
                                    str(base_video_dir))
        sys.exit(1)

    #
    # Add and delete MythVideo records for played Miro Videos
    # Add and delete symbolic links to Miro Videos and subdirectories
    # Add and delete symbolic links to coverart/Miro icons and Miro screenshots/fanart
    #
    if len(channel_watch_only): # If the user does not want any channels moved to MythVideo exit
        if channel_watch_only[0].lower() == u'all':
            printStatistics()
            return True

    if not len(watched):
        displayMessage(u"There are no Miro watched items to add to MythVideo")
    if not updateMythVideo(watched):
        logger.critical(u"Updating MythVideo with Miro video files failed.")
        sys.exit(1)

    printStatistics()
    return True
# end main

if __name__ == "__main__":
    myapp = singleinstance(u'/tmp/mirobridge.pid')
    #
    # check is another instance of Miro Bridge running
    #
    if myapp.alreadyrunning():
        print u'\nMiro Bridge is already running only one instance can run at a time\n\n'
        sys.exit(0)

    main()
    displayMessage(u"Miro Bridge Processing completed")

