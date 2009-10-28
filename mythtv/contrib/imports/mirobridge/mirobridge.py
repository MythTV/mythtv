#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mirobridge.py   Maintains MythTV database with Miro's downloaded video files.
# Python Script
# Author: 	R.D. Vaughan
# Purpose: 	This python script is intended to perform synchronise Miro's video files with MythTV's
#           "Watch Recordings" and MythVideo.
#
#           The source of all video files is from those downloaded my Miro.
#			The source of all cover art and screen shoots are from those downloaded and maintained by
#			Miro.
#			Miro v2.03 or later must be already installed and configured and already capable of
#			downloading videos.
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

__version__=u"v0.5.0"
# 0.1.0 Initial development
# 0.2.0 Initial Alpha release for internal testing only
# 0.2.1 Fixes from initial alpha test
#		Renamed imported micro python modules
# 0.2.2 Fixed the duplicate video situation when Miro and a Channel feed has an issue which causes
#		the same video to be downloaded multiple time. This situation is now detected and protects
#		the duplicates from being added to either the MythTV or MythVideo.
#		Fixes a few problems with stripping HTML and converting HTML characters in Miro descriptions.
#		Changed the identification of HD and 720 videos to conform to MythTV standards
#		Removed the file "mirobridge_util.py" from the mirobridge distribution as the main Miro
#		distribution file "util.py" is used instead.
#		Changed ALL symlink Miro icons (coverart) to a copied Miro icon file, replace any coverart that
#		is currently a symlink to a copied Miro icon file and check that each MythVideo subdirectory
#		has an actual file that has the proper naming convention that supports storage groups.
#		Use using ImageMagick's utility 'mogrify' to convert Miro's screenshots to true png's (they are
#		really jpg files with a png extension) and reduce their size by 50% for the Watch Recordings
#		screen. Imagemagick is now a requirement to have installed.
#		Fixed a bug when either the Miro video's Channel title or title exceeded MythTV database 128
#		characters limit for their equivalent title and subtitle fields.
#		When Miro has no screen shot for a Video substitute the item icon if it is high enough quality.
# 0.2.3 All subdirectory coverfiles names are changed to "folder".(jpg/png) and gif files are converted.
#		Imagemagick is now mandatory as Miro has cover art which are gif types and must be converted
#		to either jpg or png so they will be recognised as folder coverart for Storage Groups.
#		Added if coverart from Miro is a gif type convert it to a jpg.
#		Removed resizing screenshots by 50% when adding a screen shot to the Watch Recordings screen. As
#		some graphics were already very small.
#		Fixed a bug were a Watch Recordings video was deleted by the user but the graphics files were not
#       also deleted.
#		Converted all mirobridge console messages to proper logger format (critical, error, warning, info)
#		Add changes as Anduin did to ttvdb.py to force 'utf8' output
#		Tested foreign language video's with their foreign language metadata - No problem
# 0.2.4 Added a percentage downloaded message for each item that is downloading - updated every 30 secs.
#       Made Miro update and auto-download the default (no option -d). Added option (-n) to suppress
#       update and download processing.
#		If there is no screenshot then create one with ffmpeg. The size for Watch Recordings is 320 wide
#       and the mythtvideo screenshots are the same size as the video. From this point on this feature
#		will known as the "iamlindoro effect".
#		Fixed a bug where fold covers were being created even if a "folder.png" was already available.
#		As Anduin had done with ttvdb's support *.py files mirobridge's support *.py and example conf
#		files have been moved to a mirobridge subdirectory.
#		Added a check that makes sure that the Miro items are only video files. Audio files are skipped.
#		This is the final version that will support Miro v2.0.3 all higher versions will support
#		Miro v2.5.2 and higher. Except for small bugs this version will no longer be enhanced.
#       Added a small statistics report.
# 0.2.5 Changes required for mirobridge to support Miro v2.5.2 have been made now mirobridge dynamically
#		supports both versions v2.0.3 and v2.5.2
#		Fixed a statistics report bug for the copied to MythVideo totals and add new totals.
#		Fixed a bug where the Miro screen shot was not being resized for the Watch Recordings screen.
#		Fixed a bug when the Miro download time has not been set. In that case use the current date and
#		time. If this is not done the video cannot be deleted from Watch Recordings.
#		Fixed a bug where a video copied to MythVideo gets stranded in the Watch Recordings screen
#		even though the video file has been deleted in Miro.
#		Fixed a bug where a video that has been watched does not get removed from the Watch Recordings
#		screen. This stranded in the Watch Recordings screen even though the video file has been
#		deleted in Miro. This only happened when videos had been watched BUT there were no new videos
#		to add to the Watch Recordings screen.
#		Fixed a typo in the statistics report.
#		Fixed a Miro version check bug
#		Fixed a test environment option bug
#		Changed the "except" statement on imports to "except Exception:" as Anduin did to ttvdb.py
# 0.3.0 Beta release
# 0.3.1 Changed the check for Imagmagick convert utility and ffmpeg as the positive return value
#       is different on some distributions (0 or 1). The fail value is consistent (127).
#		Fixed the environment test option (-t) at times could give incorrect success message
# 0.3.2 Fixed a bug when an empty item description would abort the script on an "extras" request
#		Fixed a bug when the Watched item processing failed message would abort the script
#		Fixed a bug when a user accidental deletes a video file symlink then the video symlink is NOT
#		recreated as it should have been.
#		Fixed a bug where double quotes in the title or subtitle caused issues with file names of graphics
#		Added to the statistics report the total number of Miro-MythVideo videos that will eventually be
#       expired and removed by Miro.
#		Added a info log message with the Miro Bridge version number, which may help in problem analysis.
#		Added trapping and diagnostic messages when the HTML tags could not be removed from a description.
#		Added a check that the installed "pyparsing" python library is at least v1.5.0
#		Added detection of a Miro video deletion though the MythVideo UI. When this occurs
#		Miro is told to also deletes the video, graphics and meta data.
# 0.3.3 Status change from Beta to production. No code changes
# 0.3.4 Fixed when checking for orphaned videometadata records and there were no Miro videometadata
#		records.
#		Added additional detection and restrictions to the supported versions of Miro (minimum v2.0.3)
#		preferrably v2.5.2 or higher.
# 0.3.5 Use the MythVideo.py binding rmMetadata() routine to delete old videometatdata records.
#       Added access checks for all directories that Miro Bridge needs to write
# 0.3.6 Modifications as per requested changes.
# 0.3.7 Fixed a bug with previous modifications that impacted Miro v2.0.3 only
# 0.3.8 Fixed unicode errors with file names
#       Change to assist SG image hunting added the suffix "_coverart, _fanart, _banner,
#        _screenshot" respectively for any copied/created graphics.
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
#		using download date as was done previously. Download date is still the fall back of there is no
#		release date.
# 0.4.7 Changed all occurances of "strftime(u'" to "strftime('" as the unicode causes issues with python versions
#		less than 2.6
# 0.4.8 Some Miro "release" date values are not valid. Override with the current date.
# 0.4.9 The ffmpeg SVN (e.g. SVN-r20151) is now outputting additional metadata, skip metadata that cannot be
#		processed.
# 0.5.0 Correct the addition of adding hostnames to videometadata records and the use of relative paths when
#       there is no Videos Storage Group.
#       Added more informative error messages when trying to connect to the MythTV data base


examples_txt=u'''
For examples, please see the Mirobridge's wiki page at http://www.mythtv.org/wiki/MiroBridge
'''

# Common function imports
import sys, os, re, locale, subprocess, locale, ConfigParser, codecs, shutil
import datetime, fnmatch, string, time, logging, traceback, platform, fnmatch, ConfigParser
from optparse import OptionParser
from socket import gethostname, gethostbyname
import formatter
import MySQLdb
import htmlentitydefs

# Global variables
localhostname = gethostname() # Initalize the local hostname
local_only = True
dir_dict={u'posterdir': u"VideoArtworkDir", u'bannerdir': u'mythvideo.bannerDir', u'fanartdir': 'mythvideo.fanartDir', u'episodeimagedir': u'mythvideo.screenshotDir', u'mythvideo': u'VideoStartupDir'}
vid_graphics_dirs={u'default': u'', u'mythvideo': u'', u'posterdir': u'', u'bannerdir': u'', u'fanartdir': u'', u'episodeimagedir': u'',}
key_trans = {u'filename': u'mythvideo', u'coverfile': u'posterdir', u'banner': u'bannerdir', u'fanart': u'fanartdir', u'screenshot': u'episodeimagedir'}

graphic_suffix={u'default': u'', u'mythvideo': u'', u'posterdir': u'_coverart', u'bannerdir': u'_banner', u'fanartdir': u'_fanart', u'episodeimagedir': u'_screenshot',}
graphic_path_suffix = u"%s%s%s.%s"
graphic_name_suffix = u"%s%s.%s"

storagegroupnames = {u'Default': u'default', u'Videos': u'mythvideo', u'Coverart': u'posterdir', u'Banners': u'bannerdir', u'Fanart': u'fanartdir', u'Screenshots': u'episodeimagedir'}
storagegroups={} # The gobal dictionary is only populated with the current hosts storage group entries
image_extensions = [u"png", u"jpg", u"bmp"]
simulation = False
verbose = False
ffmpeg = True
channel_id = 9999
channel_num = 999
symlink_filename_format = u"%s - %s"
flat = False 					# The MythVideo Miro directory structure flat or channel subdirectories
download_sleeptime = float(60)	# The default value for the time to wait for auto downloading to start
channel_icon_override = []
channel_watch_only = []
channel_mythvideo_only = {}
channel_new_watch_copy = {}
tv_channels = {}
movie_trailers = []
test_environment = False
requirements_are_met = True		# Used with the (-t) test environment option
imagemagick = True
mythcommflag_recordings = u'%s -c %%s -s "%%s" --rebuild' # or u'mythcommflag -f "%s" --rebuild'
mythcommflag_videos = u'%s --rebuild --video "%%s"'


# Initalize Report Statistics:
statistics = {u'WR_watched': 0, u'Miro_marked_watch_seen': 0, u'Miro_videos_deleted': 0, u'Miros_videos_downloaded': 0, u'Miros_MythVideos_video_removed': 0, u'Miros_MythVideos_added': 0, u'Miros_MythVideos_copied': 0, u'Total_unwatched': 0, u'Total_Miro_expiring': 0, u'Total_Miro_MythVideos': 0,  }

# MythTV data base initialization dictionary
videometadata_initialize_record = {u'subtitle': u'', u'director': u'Unknown', u'rating': u'NR', u'inetref': u'99999999', u'year': 1895, u'userrating': 0.0, u'length': 0, u'showlevel': 1, u'coverfile': u'No Cover', u'host': '',}

recorded_initialize_record = {u'chanid': 9999, u'starttime': u'', u'endtime': u'', u'title': u'', u'subtitle': u'', u'description': u'', u'category': u'', u'hostname': u'', u'bookmark': 0, u'editing': 0, u'cutlist': 0, u'autoexpire': 1, u'commflagged': 0, u'recgroup': u'Default', u'recordid': 0, u'seriesid': u'', u'programid': u'', u'lastmodified': u'', u'filesize': 0, u'stars': 0, u'previouslyshown': 0, u'originalairdate': u'', u'preserve': 0, u'findid': 0, u'deletepending': 0, u'transcoder': 56, u'timestretch': 1, u'recpriority': 0, u'basename': u'', u'progstart': u'', u'progend': u'', u'playgroup': u'Default', u'profile': u'Default', u'duplicate': 1, u'transcoded': 0, u'watched': 0, u'storagegroup': u'Default',}

recordedprogram_initialize_record = {u'chanid': 9999, u'starttime': u'', u'endtime': u'', u'title': u'', u'subtitle': u'', u'description': u'', u'category': u'', u'category_type': u'', u'airdate': u'', u'stars': 0, u'previouslyshown': 0, u'title_pronounce':	'', u'stereo': 0, u'subtitled': 0, u'hdtv': 0, u'closecaptioned': 0, u'partnumber': 0, u'parttotal': 0, u'seriesid': '', u'originalairdate': u'', u'showtype': u'Series', u'colorcode': u'', u'syndicatedepisodenumber': u'', u'programid': u'', u'manualid': 0, u'generic': 0, u'listingsource': 0, u'first': 0, u'last': 0, u'audioprop': u'', u'subtitletypes': u'', u'videoprop': u'',}

channel_initialize_record = {u'chanid': 9999, u'channum': '999', u'freqid':'999', u'sourceid': 0, u'callsign': 'Miro', u'name': 'Miro', u'icon': u'', u'finetune': 0, u'videofilters': '', u'xmltvid': '', u'recpriority': 0, u'contrast': 32768, u'brightness': 32768, u'colour': 32768, u'hue': 32768, u'tvformat': 'Default', u'visible': 1, u'outputfilters': '', u'useonairguide': 0, u'mplexid': 32767, u'serviceid': 0, u'tmoffset': 0, u'atsc_major_chan': 999, u'atsc_minor_chan': 0, u'last_record': datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), u'default_authority': '', u'commmethod': -1, }

oldrecorded_initialize_record = {u'chanid': 0L, u'starttime': datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), u'endtime': datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), u'title': u'', u'subtitle': u'', u'description': u'', u'category': u'Miro', u'seriesid': u'', u'programid': u'', u'findid': 0, u'recordid': 0, u'station': u'MIRO', u'rectype': 1, u'duplicate': 1, u'recstatus': -3, u'reactivate': 0, u'generic': 0, }



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
		logger.critical(u"The python library 'pyparsing' must be at version 1.5.0 or higher. Your version is v%s" % pyparsing.__version__)
		sys.exit(False)
except Exception:
	logger.critical(u"The python library 'pyparsing' must be installed and be version 1.5.0 or higher")
	sys.exit(False)
logger.info(u"Using python library 'pyparsing' version %s" % pyparsing.__version__)


# Find out if the MythTV python bindings can be accessed and instances can created
try:
	'''If the MythTV python interface is found, we can insert data directly to MythDB or
	get the directories to store poster, fanart, banner and episode graphics.
	'''
	from MythTV import MythDB, MythVideo, MythTV
	mythdb = None
	mythvideo = None
	mythtv = None
	try:
		'''Create an instance of each: MythTV, MythVideo and MythDB
		'''
		mythdb = MythDB()
		mythvideo = MythVideo()
		mythtv = MythTV()
	except MythError, e:
		logger.critical(e.message)
		filename = os.path.expanduser("~")+'/.mythtv/config.xml'
		if not os.path.isfile(filename):
			logger.critical(u'A correctly configured (%s) file must exist\n' % filename)
		else:
			logger.critical(u'Check that (%s) is correctly configured\n' % filename)
		sys.exit(False)
	except Exception:
		logger.critical(u'''Creating an instance caused an error for one of: MythTV, MythVideo or MythDB
''')
		sys.exit(False)
except Exception:
	logger.critical(u"MythTV python bindings could not be imported")
	sys.exit(False)

# Find out if the Miro python bindings can be accessed and instances can be created
try:
	# Set up gettext before everything else
	from miro import gtcache
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
	from miro import config
	from miro import app
	from miro.frontends.cli.events import EventHandler
except Exception:
	logger.critical(u"Importing Miro functions has an issue. Miro must be installed and functional.")
	sys.exit(False)

logger.info(u"Miro Bridge version %s with Miro version %s" % (__version__, config.get(prefs.APP_VERSION)))
if config.get(prefs.APP_VERSION) < u"2.0.3":
	logger.critical(u"Your version of Miro (v%s) is not recent enough. Miro v2.0.3 is the minimum and it is preferred that you upgrade to Miro v2.5.2 or later.")
	sys.exit(False)

try:
	if config.get(prefs.APP_VERSION) < u"2.5.2":
		logger.info("Using mirobridge_interpreter_2_0_3")
		from mirobridge.mirobridge_interpreter_2_0_3 import MiroInterpreter
	else:
		logger.info("Using mirobridge_interpreter_2_5_2")
		from mirobridge.mirobridge_interpreter_2_5_2 import MiroInterpreter
except Exception:
	logger.critical(u"Importing mirobridge functions has failed. The following mirobridge files must be in the subdirectory 'mirobridge'.\n'mirobridge_interpreter_2_0_3.py' and 'mirobridge_interpreter_2_5_2.py'")
	sys.exit(False)


def _can_int(x):
    """Takes a string, checks if it is numeric.
    >>> _can_int("2")
    True
    >>> _can_int("A test")
    False
    """
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


def useImageMagick(cmd):
	""" Process graphics files using ImageMagick's utility 'mogrify'.
	>>> useImageMagick('convert screenshot.jpg -resize 50% screenshot.png')
	>>> 0
	>>> -1
	"""
	return subprocess.call(u'%s > /dev/null' % cmd, shell=True)
# end useImageMagick()

def getlocationMythcommflag():
	'''Find the exact path of the mythcommflag executable
	return the absolute full path to mythcommflag if found
	return None is not found
	'''
	try:
		p = subprocess.Popen(u'whereis mythcommflag', shell=True, bufsize=4096, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
	except:
		return None
	data = p.stdout.readline()
	try:
		data = unicode(data, 'utf8')
	except (UnicodeEncodeError, TypeError):
		pass
	if data == u'mythcommflag:':
		return None
	else:
		return data.replace(u'mythcommflag: ', u'').rstrip()
 #end getlocationMythcommflag()


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
		p = subprocess.Popen(u'ps aux | grep "miro.real"', shell=True, bufsize=4096, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
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


def rtnAbsolutePath(relpath, filetype=u'mythvideo'):
	'''Check if there is a Storage Group for the file type (mythvideo, coverfile, banner, fanart,
	screenshot)	and form an appropriate absolute path and file name.
	return an absolute path and file name
	return the relpath sting if the file does not actually exist in the absolute path location
	'''
	if relpath == None or relpath == u'':
		return relpath

	# There is a chance that this is already an absolute path
	if relpath[0] == u'/':
		return relpath

	if not storagegroups.has_key(filetype):
		return relpath	# The Videos storage group path does not exist at all the metadata entry is useless

	for directory in storagegroups[filetype]:
		abpath = u"%s/%s" % (directory, relpath)
		if os.path.isfile(abpath): # The file must actually exist locally
			return abpath
	else:
		return relpath	# The relative path does not exist at all the metadata entry is useless
# end rtnAbsolutePath


def rtnStreamingPath(filename, filetype=u'mythvideo'):
	'''Take a file path and return a "myth://..." streaming path if video storage groups exist.
	return a streaming path and file name
	return None if a streaming URL cannot be created for the filename
	'''
	global localhostname, storagegroups

	if filename == None or filename == u'':
		return None

	# There is a chance that this is already a stream path
	if filename[0].startswith(u'myth://'):
		return filename

	# If an absolute path strip the storage group
	if filename[0] == u'/':
		if not storagegroups.has_key(filetype): # If there are no storage groups then no Streaming
			return None
		for directory in storagegroups[filetype]:
			if filename.startswith(directory):
				filename = filename,replace(directory, u'')
				break
		else:
			return None

	# Example: "myth://Videos@192.168.0.102:6543/..."
	return u"myth://Videos@%s:6543/%s" % (gethostbyname(localhostname), filename)
# end rtnStreamingPath


def getStorageGroups():
	'''Populate the storage group dictionary with the host's storage groups.
	return False if there is an error
	'''
	# Get storagegroup table field names:
	table_names = mythvideo.getTableFieldNames(u'storagegroup')

	# Get Recorded videos recordedprogram / airdate
	cur = mythdb.cursor()
	try:
		cur.execute(u'select * from storagegroup')
	except MySQLdb.Error, e:
		logger.error(u"Reading storage group MythTV table: %d: %s" % (e.args[0], e.args[1]))
		return False

	while True:
		data_id = cur.fetchone()
		if not data_id:
			break
		record = {}
		i = 0
		for elem in data_id:
			if table_names[i] == 'groupname' or table_names[i] == 'hostname' or table_names[i] == 'dirname':
				record[table_names[i]] = elem
			i+=1
		if record['hostname'].lower() == localhostname.lower() and record['groupname'] in storagegroupnames.keys():
			try:
				dirname = unicode(record['dirname'], 'utf8')
			except (UnicodeDecodeError):
				logger.error(u"The local Storage group (%s) directory contained\ncharacters that caused a UnicodeDecodeError. This storage group has been rejected." % (record['groupname']))
				continue	# Skip any line that has non-utf8 characters in it
			except (UnicodeEncodeError, TypeError):
				pass

			# Add a slash if missing to any storage group dirname
			if dirname[-1:] == u'/':
				storagegroups[storagegroupnames[record['groupname']]] = dirname
			else:
				storagegroups[storagegroupnames[record['groupname']]] = dirname+u'/'
		continue
	cur.close()

	if len(storagegroups):
		# Verify that each storage group is an existing local directory
		storagegroup_ok = True
		for key in storagegroups.keys():
			if not os.path.isdir(storagegroups[key]):
				logger.critical(u"The Storage group (%s) directory (%s) does not exist" % (key, storagegroups[key]))
				storagegroup_ok = False
		if not storagegroup_ok:
			sys.exit(False)
# end getStorageGroups

def getMythtvDirectories():
	"""Get all video and graphics directories found in the MythTV DB and add them to the dictionary.
	Ignore any MythTV Frontend setting when there is already a storage group configured.
	"""
	# Stop processing if this local host has any storage groups
	global localhostname, vid_graphics_dirs, dir_dict, storagegroups, local_only

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
				vid_graphics_dirs[key] = storagegroups[u'mythvideo'] # Set fall back graphics directory to Videos
				storagegroups[key] = storagegroups[u'mythvideo'] # Set fall back SG graphics directory to Videos
	else:
		local_only = True
		if storagegroups.has_key(u'default'):
			vid_graphics_dirs[u'default'] = storagegroups[u'default']

	if local_only:
		logger.warning(u'There is no "Videos" Storage Group set so ONLY MythTV Frontend local paths for videos and graphics that are accessable from this MythTV Backend can be used.')

	for key in dir_dict.keys():
		if vid_graphics_dirs[key]:
			continue
		graphics_dir = mythdb.getSetting(dir_dict[key], hostname = localhostname)
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
						 		logger.error(u"MythVideo video directory (%s) does not exist(%s)" % (key, tmp_directories[i]))
			else:
		 		logger.error(u"MythVideo video directory (%s) is not set" % (key, ))

		if key != u'mythvideo':
			if graphics_dir and os.path.exists(graphics_dir):
				if graphics_dir[-1] != u'/':
					graphics_dir+=u'/'
				vid_graphics_dirs[key] = graphics_dir
			else:	# There is the chance that MythTv DB does not have a dir
		 		logger.error(u"(%s) directory is not set or does not exist(%s)" % (key, dir_dict[key]))

	# Make sure there is a directory set for Videos and other graphics directories on this host
	for key in vid_graphics_dirs.keys():
		if not vid_graphics_dirs[key]:
 			logger.critical(u"There must be a directory for Videos and each graphics type. At least the (%s) directory is missing." % (key))
			sys.exit(False)

	# Make sure that there is read/write access to all the directories Miro Bridge uses
	access_issue = False
	for key in vid_graphics_dirs.keys():
		if not os.access(vid_graphics_dirs[key], os.F_OK | os.R_OK | os.W_OK):
 			logger.critical(u"\nEvery Video and graphics directory must be read/writable for Miro Bridge to function. There is a permissions issue with (%s)." % (vid_graphics_dirs[key], ))
			access_issue = True
	if access_issue:
		sys.exit(False)
	# end getMythtvDirectories()

def setUseroptions():
	"""	Change variables through a user supplied configuration file
	abort the script if there are issues with the configuration file values
	"""
	global simulation, verbose, channel_icon_override, channel_watch_only, channel_mythvideo_only
	global vid_graphics_dirs, tv_channels, movie_trailers

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
					channel_mythvideo_only[filter(is_not_punct_char, option.lower())] = cfg.get(section, option)
			for key in channel_mythvideo_only.keys():
				if not channel_mythvideo_only[key].startswith(vid_graphics_dirs[u'mythvideo']):
		 			logger.critical(u"All Mythvideo only configuration (%s) directories (%s) must be a subrirectory of the MythVideo base directory (%s)." % (key, channel_mythvideo_only[key], vid_graphics_dirs[u'mythvideo']))
					sys.exit(False)
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
					channel_new_watch_copy[filter(is_not_punct_char, option.lower())] = cfg.get(section, option)
			for key in channel_new_watch_copy.keys():
				if not channel_new_watch_copy[key].startswith(vid_graphics_dirs[u'mythvideo']):
		 			logger.critical(u"All 'new->watch->copy' channel (%s) directory (%s) must be a subrirectory of the MythVideo base directory (%s)." % (key, channel_new_watch_copy[key], vid_graphics_dirs[u'mythvideo']))
					sys.exit(False)
				if channel_new_watch_copy[key][-1] != u'/':
					channel_new_watch_copy[key]+=u'/'
			continue
# end setUserconfig


def getVideoDetails(videofilename, screenshot=False):
	'''Using ffmpeg (if it can be found) get a video file's details
	return False if ffmpeg is cannot be found
	return empty dictionary if the file is not a video
	return dictionary of the Video's details
	'''
	ffmpeg_details = {u'video': u'', u'audio': u'', u'duration': 0, u'stereo': 0, u'hdtv': 0}

	global ffmpeg
	if not ffmpeg and not screenshot:
		return ffmpeg_details
	if not ffmpeg and screenshot:
		return False

	video = re.compile(u' Video: ')
	video_HDTV_small = re.compile(u' 1280x', re.UNICODE)
	video_HDTV_large = re.compile(u' 1920x', re.UNICODE)
	width_height = re.compile(u'''^(.+?)[ ]\[?([0-9]+)x([0-9]+)[^\\/]*$''', re.UNICODE)
	audio = re.compile(u' Audio: ', re.UNICODE)
	audio_stereo = re.compile(u' stereo,', re.UNICODE)
	audio_mono = re.compile(u' mono,', re.UNICODE)
	audio_ac3 = re.compile(u' ac3,', re.UNICODE)
	audio_51 = re.compile(u' 5.1,', re.UNICODE)
	audio_2C = re.compile(u' 2 channels,', re.UNICODE)
	audio_1C = re.compile(u' 1 channels,', re.UNICODE)
	audio_6C = re.compile(u' 6 channels,', re.UNICODE)

	try:
		p = subprocess.Popen(u'ffmpeg -i "%s"' % (videofilename), shell=True, bufsize=4096, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
	except:
		if not screenshot:
			return ffmpeg_details
		else:
			return False

	ffmpeg_found = True
	alldata = 3
	datacount = 0
	while 1:
		if datacount == alldata:	# Stop if all the required data has been extracted from ffmpeg's output
			break
		data = p.stderr.readline()
		if data == '':
			break
		try:
			data = unicode(data, 'utf8')
		except (UnicodeDecodeError):
			continue	# Skip any line that has non-utf8 characters in it
		except (UnicodeEncodeError, TypeError):
			pass
		if data.endswith(u'command not found\n'):
			ffmpeg_found = False
			break
		if data.startswith(u'  Duration:'):
			time = (data[data.index(':')+1: data.index('.')]).strip()
			ffmpeg_details[u'duration'] = (360*(int(time[:2]))+(int(time[3:5])*60))+int(time[6:8])
			datacount+=1
		elif len(video.findall(data)):
			match = width_height.match(data)
			datacount+=1
			if match:
				dummy, width, height = match.groups()
				width, height = float(width), float(height)
				aspect = height/width
				if screenshot:
					ffmpeg_details[u'width'] = width
					ffmpeg_details[u'height'] = height
					ffmpeg_details[u'aspect'] = aspect
				if width > 1300.0 or width > 800.0 or width == 720.0 or width == 1080.0 :
					ffmpeg_details[u'video']+=u'HDTV'
					ffmpeg_details[u'hdtv'] = 1
				elif aspect <= 0.5625:
					ffmpeg_details[u'video']+=u'WIDESCREEN'
				if len(ffmpeg_details[u'video']):
					comma = u','
				else:
					comma = u''
				if width > 1300.0:
					ffmpeg_details[u'video']+=comma+u'1080'
				elif width > 800.0:
					ffmpeg_details[u'video']+=comma+u'720'
		elif len(audio.findall(data)):
			datacount+=1
			if len(audio_stereo.findall(data)) or len(audio_2C.findall(data)):
				ffmpeg_details[u'audio']+=u'STEREO'
				ffmpeg_details[u'stereo'] = 1
			elif len(audio_mono.findall(data)) or len(audio_1C.findall(data)):
				ffmpeg_details[u'audio']+=u'MONO'
			elif (len(audio_51.findall(data)) and len(audio_ac3.findall(data))) or len(audio_6C.findall(data)):
				ffmpeg_details[u'audio']+=u'DOLBY'
				continue
			elif len(audio_51.findall(data)):
				ffmpeg_details[u'audio']+=u'SURROUND'

	if ffmpeg_found == False:
		ffmpeg = False
		return False
	else:
		return ffmpeg_details
# end getVideoDetails()


def takeScreenShot(videofile, screenshot_filename, size_limit=False, just_demensions=False):
	'''Take a screen shot 1/8th of the way into a video file
	return the fully qualified screen shot name
	>>> True - If the screenshot was created
	>>> False - If ffmpeg could not find details for the video file
	'''
	try:
		ffmpeg_details = getVideoDetails(videofile, screenshot=True)
	except:
		return None
	if not ffmpeg_details:
		return None
	elif not u'width' in ffmpeg_details.keys():
		return None

	max_length = 2*60	# Maximum length of a video check if set at 2 minutes
	if ffmpeg_details[u'duration']:
		if ffmpeg_details[u'duration'] < max_length:
			delay = (ffmpeg_details[u'duration']/2) # Maximum time to take screenshot into a video is 1 minute
		else:
			delay = 60 # For a large videos take screenshot at the 1 minute mark

	cmd = u'ffmpeg -i "%s" -y -f image2 -ss %d -sameq -t 0.001 -s %d*%d "%s"'

	width = int(ffmpeg_details[u'width'])
	height = int(ffmpeg_details[u'height'])

	if size_limit:
		width = 320
		height = (int(width * ffmpeg_details[u'aspect'])/2)*2 # ((xxxxx)/2) *2

	if just_demensions:
		return u"%dx%d" % (width, height)

	cmd2 = cmd % (videofile, delay, width, height, screenshot_filename)

	return subprocess.call(u'%s > /dev/null' % cmd2, shell=True)
# end takeScreenShot()


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


def setRecord(table, data, channel, delete=False, id=None):
	"""Adds or updates or deletes a record in the database.
	return True if requested data base action was successful
	return False if requested data base action was unsuccessful
	"""
	global simulation, verbose

	cur = mythdb.db.cursor()

	if delete and not channel and not len(data):
		logger.critical(u"Cannot delete a (%s) record without details" % table)
		sys.exit(False)

	if delete:
		if simulation:
			logger.info(u'Simulation: DELETE FROM %s WHERE chanid=%d AND starttime="%s" AND endtime="%s"' % (table, channel, data[u'starttime'], data[u'endtime']))
			return True
		try:
			cur.execute(u'DELETE FROM %s WHERE chanid=%d AND starttime="%s" AND endtime="%s"' % (table, channel, data[u'starttime'], data[u'endtime']))
		except MySQLdb.Error, e:
			logger.error(u"Cannot delete from table (%s) record: (%s) (%s) record\nMySql error: %d: %s" % (table, channel, data['subtitle'], e.args[0], e.args[1]))
			cur.close()
			return False
		cur.close()
		return True

	if id is None:
		fields = u', '.join(data.keys())
		format_string = u', '.join([u'%s' for d in data.values()])
		sql = u"INSERT INTO %s(%s) VALUES(%s)" % (table, fields, format_string)
		if simulation:
			logger.info(u"Simulation: sql(%s) data.values(%s)" % (sql, data.values()))
			return True
		try:
			cur.execute(sql, data.values())
		except MySQLdb.Error, e:
			logger.error(u"Cannot Insert in table (%s) record:  (%s) (%s) record\nMySql error: %d: %s" % (table, data['title'], data['subtitle'], e.args[0], e.args[1]))
		cur.close()
		return True
	else:
		format_string = u', '.join([u'%s = %%s' % d for d in data])
		sql = u"UPDATE %s SET %s" % (table, format_string)
		sql_values = data.values()
		if simulation:
			logger.info(u"Simulation: %s" % (sql, sql_values))
			return True
		try:
			cur.execute(sql, sql_values)
		except MySQLdb.Error, e:
			logger.error(u"Cannot Update table (%s) record:  (%s) (%s) record\nMySql error: %d: %s" % (table, data['title'], data['subtitle'], e.args[0], e.args[1]))
		cur.close()
		return True
	# end setRecord()

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

	def createDict(record, fieldnames):
		record_dict = {}
		index = 0
		for field in fieldnames:
			record_dict[field] = record[index]
			index+=1
		return record_dict
		# end createDict()

	def getRecordDictionary(tablename):
		fieldnames = mythvideo.getTableFieldNames(tablename)
		mysqlcommand = u"SELECT * FROM %s WHERE chanid=%d" % (tablename, channel_id)
		c = mythdb.db.cursor()
		try:
			c.execute(mysqlcommand)
		except MySQLdb.Error, e:
			logger.error(u"SELECT * FROM %s WHERE chanid=%d failed: %d: %s" % (tablename, channel_id, e.args[0], e.args[1]))
			c.close()
			return None
		recs=[]
		while True:
			row = c.fetchone()
			if not row:
				break
			recs.append(row)
		c.close()

		records=[]
		if len(recs):
			for record in recs:
				rec = createDict(record, fieldnames)
				if tablename == u'recorded':
					if rec[u'hostname'] == localhostname:
						records.append(rec)
				else:
						records.append(rec)
		return records

	recorded_array = getRecordDictionary(u'recorded')
	if not recorded_array:
		recorded_array = []
	oldrecorded_array = getRecordDictionary(u'oldrecorded')
	if not oldrecorded_array:
		oldrecorded_array = []
	videometadata = getMiroVideometadataRecords()
	if not videometadata:
		videometadata = []

	orphans = []
	for record in oldrecorded_array:
		for recorded in recorded_array:
			if recorded[u'starttime'] == record[u'starttime'] and recorded[u'endtime'] == record[u'endtime']:
				break
		else:
			for video in videometadata:
				if video[u'title'] == record[u'title'] and video[u'subtitle'] == record[u'subtitle']:
					break
			else:
				orphans.append(record)

	for data in orphans:
		if simulation:
			logger.info(u"Simulation: Remove orphaned oldrecorded record (%s - %s)" % (data[u'title'], data[u'subtitle']))
		else:
			setRecord(u'oldrecorded', data, channel_id, delete=True, id=None)
			setRecord(u'recordedprogram', data, channel_id, delete=True, id=None)
			# Attempt a clean up for orphaned recorded video files and/or graphics
			(dirName, fileName) = os.path.split(u'%s%s - %s.%s' % (vid_graphics_dirs[u'default'], data[u'title'], data[u'subtitle'], u'png'))
			(fileBaseName, fileExtension)=os.path.splitext(fileName)
			try:
				dirName = unicode(dirName, u'utf8')
			except (UnicodeEncodeError, TypeError):
				pass
			try:
				fileBaseName = unicode(fileBaseName, u'utf8')
			except (UnicodeEncodeError, TypeError):
				pass
			for name in os.listdir(dirName): # Clean up
				if name.startswith(fileBaseName):
					try:
						if simulation:
							logger.info(u"Simulation: Remove file/symbolic link(%s)" % (u"%s/%s" % (dirName, name)))
						else:
							os.remove(u"%s/%s" % (dirName, name))
					except OSError:
						pass
			# Attempt a clean up for orphaned MythVideo screenshot and UNIQUE coverart
			(dirName, fileName) = os.path.split(u'%s%s - %s%s.%s' % (vid_graphics_dirs[u'episodeimagedir'], data[u'title'], data[u'subtitle'], graphic_suffix[u'episodeimagedir'], u'png'))
			(fileBaseName, fileExtension)=os.path.splitext(fileName)
			try:
				dirName = unicode(dirName, u'utf8')
			except (UnicodeEncodeError, TypeError):
				pass
			try:
				fileBaseName = unicode(fileBaseName, u'utf8')
			except (UnicodeEncodeError, TypeError):
				pass
			for name in os.listdir(dirName): # Clean up
				if name.startswith(fileBaseName):
					try:
						if simulation:
							logger.info(u"Simulation: Remove screenshot file (%s)" % (u"%s/%s" % (dirName, name)))
						else:
							os.remove(u"%s/%s" % (dirName, name))
					except OSError:
						pass
					break
			# Remove any unique cover art graphic files
			if data[u'title'].lower() in channel_icon_override:
				(dirName, fileName) = os.path.split(u'%s%s - %s%s.%s' % (vid_graphics_dirs[u'posterdir'], data[u'title'], data[u'subtitle'], graphic_suffix[u'posterdir'], u'png'))
				(fileBaseName, fileExtension)=os.path.splitext(fileName)
				try:
					dirName = unicode(dirName, u'utf8')
				except (UnicodeEncodeError, TypeError):
					pass
				try:
					fileBaseName = unicode(fileBaseName, u'utf8')
				except (UnicodeEncodeError, TypeError):
					pass
				for name in os.listdir(dirName): # Clean up
					if name.startswith(fileBaseName):
						try:
							if simulation:
								logger.info(u"Simulation: Remove unique cover art file (%s)" % (u"%s/%s" % (dirName, name)))
							else:
								os.remove(u"%s/%s" % (dirName, name))
						except OSError:
							pass
						break
			displayMessage(u"Removed orphaned Miro video and graphics files (%s - %s)" % (data[u'title'], data[u'subtitle']))

	return orphans
	# end getOldrecordedOrphans()


def deleteOldrecordedForMythVideo(title, subtitle):
	'''Delete an oldrecorded record when it matches a specific MythVideo title and subtitle
	return nothing
	'''
	global simulation, verbose, channel_id, localhostname, vid_graphics_dirs, statistics

	def createDict(record, fieldnames):
		record_dict = {}
		index = 0
		for field in fieldnames:
			record_dict[field] = record[index]
			index+=1
		return record_dict
		# end createDict()

	def getRecordDictionary(tablename):
		fieldnames = mythvideo.getTableFieldNames(tablename)
		mysqlcommand = u"SELECT * FROM %s WHERE chanid=%d" % (tablename, channel_id)
		c = mythdb.db.cursor()
		try:
			c.execute(mysqlcommand)
		except MySQLdb.Error, e:
			logger.error(u"SELECT * FROM %s WHERE chanid=%d failed: %d: %s" % (tablename, channel_id, e.args[0], e.args[1]))
			c.close()
			return None
		recs=[]
		while True:
			row = c.fetchone()
			if not row:
				break
			recs.append(row)
		c.close()

		records=[]
		if len(recs):
			for record in recs:
				rec = createDict(record, fieldnames)
				if tablename == u'recorded':
					if rec[u'hostname'] == localhostname:
						records.append(rec)
				else:
						records.append(rec)
		return records

	oldrecorded_array = getRecordDictionary(u'oldrecorded')

	for oldrecorded in oldrecorded_array:
		if oldrecorded[u'title'] == title and oldrecorded[u'subtitle'] == subtitle:
			setRecord(u'oldrecorded', oldrecorded, channel_id, delete=True, id=None)
			break
	# end deleteOldrecordedForMythVideo()


def getRecorded():
	"""Retrieves the Miro recorded records for localhostname.
	return array of recorded dictionary records
	return None if there are no matching recorded records
	"""
	global simulation, verbose, channel_id, localhostname

	def createDict(record, fieldnames):
		record_dict = {}
		index = 0
		for field in fieldnames:
			record_dict[field] = record[index]
			index+=1
		return record_dict
		# end createDict()

	fieldnames = mythvideo.getTableFieldNames(u'recorded')

	mysqlcommand = u"SELECT * FROM recorded WHERE chanid=%d" % (channel_id)
	c = mythdb.db.cursor()
	try:
		c.execute(mysqlcommand)
	except MySQLdb.Error, e:
		logger.error(u"SELECT * FROM recorded WHERE chanid=%d failed: %d: %s" % (channel_id, e.args[0], e.args[1]))
		c.close()
		return None
	recorded=[]
	while True:
		row = c.fetchone()
		if not row:
			break
		recorded.append(row)
	c.close()

	recordedrecords=[]
	if len(recorded):
		for record in recorded:
			rec = createDict(record, fieldnames)
			if rec[u'hostname'] == localhostname:
				recordedrecords.append(rec)
		return recordedrecords
	else:
		return None
	# end getRecorded()

def getMiroVideometadataRecords():
	"""Fetches all videometadata records with an inetref of '99999999' and a category of 'Miro'. If the
	videometadata record has a host them it must match the lower-case of the locahostname.
	aborts if processing failed
	return and array of matching videometadata dictionary records
	"""
	global localhostname, simulation
	intids = []

	category_id = mythvideo.getGenreId(u'Miro')

	mysqlcommand = u"SELECT intid FROM videometadata WHERE inetref = 99999999 and category = %d" % category_id

	c = mythdb.db.cursor()
	try:
		c.execute(mysqlcommand)
	except MySQLdb.Error, e:
		logger.error(u"SELECT intid FROM videometadata WHERE inetref = 99999999 and category = %d failed: %d: %s" % (category_id, e.args[0], e.args[1]))
		c.close()
		sys.exit(False)
	intids=[]
	while True:
		row = c.fetchone()
		if not row:
			break
		intids.append(row[0])
	c.close()

	videometadatarecords=[]
	if len(intids):
		for intid in intids:
			vidrec = mythvideo.getMetadataDictionary(intid)
			if vidrec[u'host']:
				if vidrec[u'host'].lower() != localhostname.lower():
					continue
			videometadatarecords.append(vidrec)

		return videometadatarecords
	else:
		return None
	# end getMiroVideometadataRecords()

def getStartEndTimes(duration, downloadedTime):
	'''Calculate a videos start and end times and isotime for the recorded file name.
	return an array of initialised values if either duration or downloadedTime is invalid
	return an array of the video's start, end times and isotime
	'''
	start_end = [datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), datetime.datetime.now().strftime('%Y%m%d%H%M%S')]

	if downloadedTime != None:
		try:
			dummy = downloadedTime.strftime('%Y-%m-%d')
		except ValueError:
			downloadedTime = datetime.datetime.now()
		end = downloadedTime+datetime.timedelta(seconds=duration)
		start_end[0] = downloadedTime.strftime('%Y-%m-%d %H:%M:%S')
		start_end[1] = end.strftime('%Y-%m-%d %H:%M:%S')
		start_end[2] = downloadedTime.strftime('%Y%m%d%H%M%S')

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
		sym_filepath = graphic_path_suffix % (storagegroups[storagegroupkey], symbolic_name, graphic_suffix[storagegroupkey], ext)
		sym_filename = graphic_name_suffix % (symbolic_name, graphic_suffix[storagegroupkey], ext)
	elif storagegroupkey in storagegroups.keys() and not local_only:
		sym_filepath = graphic_path_suffix % (storagegroups[storagegroupkey], symbolic_name, graphic_suffix[storagegroupkey], ext)
		sym_filename = graphic_name_suffix % (symbolic_name, graphic_suffix[storagegroupkey], ext)
	else:
		sym_filepath = graphic_path_suffix % (vid_graphics_dirs[storagegroupkey], symbolic_name, graphic_suffix[storagegroupkey], ext)
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
			logger.info(u"Simulation: Used file (%s) to create symlink as (%s)" % (filename, sym_filepath))
		else:
			logger.info(u"Simulation: Used file (%s) copy as (%s)" % (filename, sym_filepath))
		return sym_filename

	try:
		if allow_symlink:
			os.symlink(filename, sym_filepath)
			displayMessage(u"Used file (%s) to created symlink (%s)" % (filename, sym_filepath))
		else:
			try:	# Every file but video files copied to support Storage groups
				if convert:
					useImageMagick(u'convert "%s" "%s"' % (filename, sym_filepath))
					displayMessage(u"Convert and copy Miro file (%s) to file (%s)" % (filename, sym_filepath))
				else:
					shutil.copy2(filename, sym_filepath)
					displayMessage(u"Copied Miro file (%s) to file (%s)" % (filename, sym_filepath))
			except OSError:
				logger.critical(u"Trying to copy the Miro file (%s) to the file (%s).\n         This maybe a permissions error (mirobridge.py does not have permission to write to the directory)." % (filename ,sym_filepath))
				sys.exit(False)
	except OSError:
		logger.critical(u"Trying to create the Miro file (%s) symlink (%s).\n         This maybe a permissions error (mirobridge.py does not have permission to write to the directory)." % sym_filepath)
		sys.exit(False)

	return sym_filename
	# end setSymbolic()


def createOldRecordedRecord(item, start, end):
	'''Using the details from a Miro item record create a MythTV oldrecorded record
	return an array of MythTV of a full initialised oldrecorded record dictionaries
	'''
	global localhostname, simulation, verbose, storagegroups, ffmpeg, channel_id
	global oldrecorded_initialize_record

	tmp_oldrecorded={}
	for key in oldrecorded_initialize_record.keys():
		tmp_oldrecorded[key] = oldrecorded_initialize_record[key]

	# Create the oldrecorded dictionary
	tmp_oldrecorded[u'chanid'] = channel_id
	tmp_oldrecorded[u'starttime'] = start
	tmp_oldrecorded[u'endtime'] = end
	tmp_oldrecorded[u'title'] = item[u'channelTitle']
	tmp_oldrecorded[u'subtitle'] = item[u'title']
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
	global recorded_initialize_record, recordedprogram_initialize_record

	tmp_recorded={}
	for key in recorded_initialize_record.keys():
		tmp_recorded[key] = recorded_initialize_record[key]
	tmp_recordedprogram={}
	for key in recordedprogram_initialize_record.keys():
		tmp_recordedprogram[key] = recordedprogram_initialize_record[key]

	ffmpeg_details = getVideoDetails(item[u'videoFilename'])
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

	basename = setSymbolic(item[u'videoFilename'], u'default', u"%s_%s" % (channel_id, start_end[2]), allow_symlink=True)
	if basename != None:
		tmp_recorded[u'basename'] = basename
	else:
		logger.critical(u"The file (%s) must exist to create a recorded record" % item[u'videoFilename'])
		sys.exit(False)

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

	return [tmp_recorded, tmp_recordedprogram, createOldRecordedRecord(item, start_end[0], start_end[1])]
	# end  createRecordedRecord()


def createVideometadataRecord(item):
	'''Using the details from a Miro item create a MythTV videometadata record
	return an dictionary of MythTV an initialised videometadata record
	'''
	global localhostname, simulation, verbose, storagegroups, ffmpeg, channel_id, flat, image_extensions
	global videometadata_initialize_record, vid_graphics_dirs, channel_icon_override
	global local_only

	ffmpeg_details = getVideoDetails(item[u'videoFilename'])
	start_end = getStartEndTimes(ffmpeg_details[u'duration'], item[u'downloadedTime'])

	fieldnames = mythvideo.getTableFieldNames(u'videometadata')

	sympath = u'Miro'
	if not flat:
		sympath+=u"/%s" % item[u'channelTitle']
	banners = u'mirobridge_banner.jpg'
	for ext in image_extensions:
		filename = u"%s_banner.%s" % (item[u'channelTitle'], ext)
		if os.path.isfile(vid_graphics_dirs[u'bannerdir']+filename):
			banners = setSymbolic(vid_graphics_dirs[u'bannerdir']+filename, u'bannerdir', item[u'channelTitle'])
			break
	else:
		if not os.path.isfile(vid_graphics_dirs[u'bannerdir']+banners):
			banners = ''
	fanart = u'mirobridge_fanart.jpg'
	for ext in image_extensions:
		filename = u"%s_fanart.%s" % (item[u'channelTitle'], ext)
		if os.path.isfile(vid_graphics_dirs[u'fanartdir']+filename):
			fanart = setSymbolic(vid_graphics_dirs[u'fanartdir']+filename, u'fanartdir', item[u'channelTitle'])
			break
	else:
		if not os.path.isfile(vid_graphics_dirs[u'fanartdir']+fanart):
			fanart = ''

	ffmpeg_details = getVideoDetails(item[u'videoFilename'])
	start_end = getStartEndTimes(ffmpeg_details[u'duration'], item[u'downloadedTime'])
	videometadata = {}
	for key in videometadata_initialize_record: # Initialize the videometadata record
		videometadata[key] = videometadata_initialize_record[key]

	videometadata[u'title'] = item[u'channelTitle']
	videometadata[u'subtitle'] = item[u'title']

	try:
		details = massageDescription(item[u'description'], extras=True)
	except TypeError:
		print
		print u"MythVideo-Channel title(%s) subtitle(%s)" % (item[u'channelTitle'], item[u'title'])
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
	if u'episode' in fieldnames:
		videometadata[u'season'] = 0
		videometadata[u'episode'] = 0
	videometadata[u'length'] = ffmpeg_details[u'duration']/60
	videometadata[u'category'] = mythvideo.getGenreId(u'Miro')

	if not u'copied' in item.keys():
		ext = (item[u'videoFilename'])
		videofile = setSymbolic(item[u'videoFilename'], u'mythvideo', "%s/%s - %s" % (sympath, item[u'channelTitle'], item[u'title']), allow_symlink=True)
		if videofile != None:
			videometadata[u'filename'] = videofile
			if not local_only and videometadata[u'filename'][0] != u'/':
				videometadata[u'host'] = localhostname.lower()
		else:
			logger.critical(u"The file (%s) must exist to create a videometadata record" % item[u'videoFilename'])
			sys.exit(False)
	else:
		videometadata[u'filename'] = item[u'videoFilename']
		if not local_only and videometadata[u'filename'][0] != u'/':
			videometadata[u'host'] = localhostname.lower()

	if not u'copied' in item.keys():
		if item[u'channel_icon'] and not item[u'channelTitle'].lower() in channel_icon_override:
			ext = (item[u'channel_icon'])
			filename = setSymbolic(item[u'channel_icon'], u'posterdir', u"%s" % (item[u'channelTitle']))
			if filename != None:
				videometadata[u'coverfile'] = filename
		else:
			if item[u'item_icon']:
				ext = (item[u'item_icon'])
				filename = setSymbolic(item[u'item_icon'], u'posterdir', u"%s - %s" % (item[u'channelTitle'], item[u'title']))
				if filename != None:
					videometadata[u'coverfile'] = filename
	else:
		videometadata[u'coverfile'] = item[u'channel_icon']

	if not item.has_key(u'copied'):
		if item[u'screenshot']:
			ext = (item[u'screenshot'])
			filename = setSymbolic(item[u'screenshot'], u'episodeimagedir', u"%s - %s" % (item[u'channelTitle'], item[u'title']))
			if filename != None:
				videometadata[u'screenshot'] = filename
	else:
		if item[u'screenshot']:
			videometadata[u'screenshot'] = item[u'screenshot']

	if banners != u'' and videometadata[u'subtitle'] != u'':
		if storagegroups.has_key(u'bannerdir'):
			videometadata[u'banner'] = banners
		else:
			videometadata[u'banner'] = vid_graphics_dirs[u'bannerdir']+banners

	if fanart != u'':
		if storagegroups.has_key(u'fanartdir'):
			videometadata[u'fanart'] = fanart
		else:
			videometadata[u'fanart'] = vid_graphics_dirs[u'fanartdir']+fanart

	return [videometadata, createOldRecordedRecord(item, start_end[0], start_end[1])]
	# end createVideometadataRecord()


def createChannelRecord(icon, channel_id, channel_num):
	'''Create the optional Miro "channel" record as it makes the Watch Recordings screen look better.
	return True if the record was created and written successfully
	abort if the processing failed
	'''
	global localhostname, simulation, verbose, vid_graphics_dirs
	global channel_initialize_record

	if icon != u"":
		if not os.path.isfile(icon):
			logger.critical(u'The Miro channel icon file (%s) does not exist.\nThe variable needs to be a fully qualified file name and path.\ne.g. ./mirobridge.py -C "/path to the channel icon/miro_channel_icon.jpg"' % (icon))
			sys.exit(False)

	channel_initialize_record[u'chanid'] = channel_id
	channel_initialize_record[u'channum'] = str(channel_num)
	channel_initialize_record[u'freqid'] = str(channel_num)
	if icon != u"":
		channel_initialize_record[u'icon'] = icon
	channel_initialize_record[u'atsc_major_chan'] = channel_num

	if simulation:
		logger.info(u"Simulation: Create Miro channel record channel_id(%d) and channel_num(%d)" % (channel_id, channel_num))
		logger.info(u"Simulation: Channel icon file(%s)" % (icon))
	else:
		try:
			setRecord(u'channel', channel_initialize_record, channel_id, delete=False, id=None)
		except:
			logger.critical(u"Failed writing the Miro channel record. Most likely the Channel Id and number already exists.\nUse MythTV set up program (mythtv-setup) to alter or remove the offending channel.\nYou specified Channel ID (%d) and Channel Number (%d)" % (channel_id, channel_num))
			sys.exit(False)
	return True
	# end createChannelRecord(icon)


def checkVideometadataFails(record, flat):
	'''Verify that the real path exists for both video and graphics for this MythVideo Miro record.
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


def deleteVideometadataRecord(intid):
	'''Remove a Miro MythTV videometadata
	return nothing
	'''
	global simulation

	if simulation:
		logger.info(u"Simulation: DELETE videometadata for intid = %s" % (intid,))
	else:
		mythvideo.rmMetadata(intid)
	# end deleteVideometadataRecord()


def createMiroMythVideoDirectory():
	'''If the "Miro" directory does not exist in MythVideo then create it.
	abort if there is an issue creating the directory
	'''
	global localhostname, vid_graphics_dirs, storagegroups, channel_id, flat, simulation, verbose

	# Check that a MIRO video directory exists
	# if not then create the dir and add symbolic link to cover/file or icon
	miro = u'Miro'
	miro_icon = u'folder.jpg'
	miro_path = vid_graphics_dirs[u'mythvideo']+miro
	miro_icon_filename = (u"%s/%s" % (miro_path, miro_icon))
	if not os.path.isdir(miro_path):
		miro_cover = vid_graphics_dirs[u'posterdir']+miro_icon
		try:
			if simulation:
				logger.info(u"Simulation: Create Miro Mythvideo directory (%s)" % (miro_path,))
			else:
				try:
					os.mkdir(miro_path)
				except OSError:
					logger.critical(u"Create Miro Mythvideo directory (%s).\nThis may be due to a permissions error." % (miro_path))
					sys.exit(False)
			if os.path.isfile(miro_cover):
				if simulation:
					logger.info(u"Simulation: Copy Miro directory cover file link from (%s) to (%s)" % (miro_cover, miro_icon_filename))
				else:
					try:
						if not os.path.isfile(os.path.realpath(miro_cover)):
							useImageMagick(u'convert "%s" "%s"' % (miro_cover, miro_icon_filename))
					except OSError:
						logger.critical(u"File (%s) copy to (%s) failed.\nThis may be due to a permissions error." % (miro_cover, miro_icon_filename))
						sys.exit(False)
		except OSError:
			logger.critical(u"Creation of MythVideo 'Miro' directory (%s) failed.\nThis may be due to a permissions error." % (miro_path))
			sys.exit(False)
	# end createMiroMythVideoDirectory()

def createMiroChannelSubdirectory(item):
	'''Create the Miro Channel subdirectory in MythVideo
	abort if the subdirectory cannot be made
	'''
	global localhostname, vid_graphics_dirs, storagegroups, channel_id, flat, simulation, verbose

	miro = u'Miro'
	path = u"%s%s/%s" % (vid_graphics_dirs[u'mythvideo'], miro, item[u'channelTitle'])
	if item[u'channel_icon']:
		ext = getExtention(item[u'channel_icon'])
		cover_filename = u"folder.jpg"
		cover_filename_path = u"%s/%s" % (path, cover_filename)
		cover_filename_path2 = cover_filename_path[:-3]+u'png'
	if os.path.isdir(path):
		if item[u'channel_icon']:
			if os.path.isfile(os.path.realpath(cover_filename_path)) or os.path.isfile(os.path.realpath(cover_filename_path2)):
				return
			else:
				try: # Somthing is wrong with the subdirectory coverart - fix it
					os.remove(cover_filename_path)
				except OSError:
					pass
		else:
			return

	if simulation:
		logger.info(u"Simulation: Make subdirectory(%s)" % (path))
	else:
		if not os.path.isdir(path):
			try:
				os.mkdir(path)
			except OSError:
				logger.critical(u"Creation of MythVideo 'Miro' subdirectory path (%s) failed.\nThis may be due to a permissions error." % (path))
				sys.exit(False)
	if item[u'channel_icon']:
		if simulation:
			logger.info(u"Simulation: Copying subdirectory cache icon(%s) cover file(%s)" % (item[u'channel_icon'], cover_filename_path))
		else:
			try:
				if not os.path.isfile(os.path.realpath(cover_filename_path)):
					useImageMagick(u'convert "%s" "%s"' % (item[u'channel_icon'], cover_filename_path))
			except OSError:
				logger.critical(u"Copying subdirectory cache icon(%s) cover file(%s) failed.\nThis may be due to a permissions error." % (item[u'channel_icon'], cover_filename_path))
				sys.exit(False)

	# end createMiroChannelSubdirectory()


def getPlayedMiroVideos():
	'''From the MythTV database recorded records identify all "played" Miro Video files.
	return None if there were either no Miro recorded records or none that were in "watched" status
	return an array of subtitles of those Miro video files that were "watched"
	'''
	global localhostname, vid_graphics_dirs, storagegroups, verbose, channel_id, statistics

	filenames=[]
	recorded = getRecorded()
	if not recorded:
		return None

	for record in recorded:
		if record[u'watched'] == 0:	# Skip if the video has NOT been watched
			continue
		try:
			filenames.append(os.path.realpath(storagegroups[u'default']+record[u'basename']))
			statistics[u'WR_watched']+=1
		except OSError:
			logger.info(u"Miro video file has been removed (%s) outside of mirobridge" % (storagegroups[u'default']+record[u'basename'],))
			continue
		displayMessage(u"Miro video (%s) (%s) has been marked as watched in MythTV." % (record[u'title'], record[u'subtitle']))

	if len(filenames):
		return filenames
	else:
		return None
	# end getPlayedMiroVideos()

def removeRecordedSeekTable(data, channel):
	'''Remove seektable entries for a recording from the "recordedseek" table. Entries may of may not
	exist.
	return nothing
	'''
	global simulation, verbose
	table = u'recordedseek'

	if simulation:
		logger.info(u'Simulation: DELETE FROM %s WHERE chanid=%d AND starttime="%s"' % (table, channel, data[u'starttime']))
	else:
		cur = mythdb.db.cursor()
		try:
			cur.execute(u'DELETE FROM %s WHERE chanid=%d AND starttime="%s"' % (table, channel, data[u'starttime']))
		except MySQLdb.Error, e:
			logger.error(u"Cannot delete from table (%s) records: (%s) (%s) record\nMySql error: %d: %s" % (table, channel, data['subtitle'], e.args[0], e.args[1]))
		cur.close()
	# end removeRecordedSeekTable()


def removeMythvideoSeekTable(filename):
	'''Remove seektable entries for a Mythvideo from the "filemarkup" table. Entries may of may not
	exist. The deletes need to remove entries for both an absolute file path and a Storage Groups
	"myth://..." file name definition.
	return nothing
	'''
	global simulation, verbose, storagegroups
	table = u'filemarkup'

	## "myth://Videos@192.168.0.102:6543/0Review/Boxing.......mpg"
	## Plus turn any relative path into an absolute path (create a routine as also needed in the create)
	filename = rtnAbsolutePath(filename, filetype=u'mythvideo')
	if storagegroups.has_key(u'mythvideo'):
		if filename.startswith(storagegroups[u'mythvideo']):
			filename2 = rtnStreamingPath(filename, filetype=u'mythvideo')
			filename3 = filename.replace(storagegroups[u'mythvideo'], u'')
		else:
			filename2 = False
			filename3 = False
	else:
		filename2 = False
		filename3 = False

	if simulation:
		logger.info(u'Simulation: DELETE FROM %s WHERE filename="%s"' % (table, filename))
		if filename2:
			logger.info(u'Simulation: DELETE FROM %s WHERE filename="%s"' % (table, filename2))
		if filename3:
			logger.info(u'Simulation: DELETE FROM %s WHERE filename="%s"' % (table, filename3))
	else:
		cur = mythdb.db.cursor()
		try:
			cur.execute(u'DELETE FROM %s WHERE  filename="%s"' % (table, filename))
		except MySQLdb.Error, e:
			logger.error(u"Cannot delete from table (%s) records: (%s)\nMySql error: %d: %s" % (table, filename, e.args[0], e.args[1]))
		if filename2:
			try:
				cur.execute(u'DELETE FROM %s WHERE  filename="%s"' % (table, filename2))
			except MySQLdb.Error, e:
				logger.error(u"Cannot delete from table (%s) records: (%s)\nMySql error: %d: %s" % (table, filename2, e.args[0], e.args[1]))
		if filename3:
			try:
				cur.execute(u'DELETE FROM %s WHERE  filename="%s"' % (table, filename3))
			except MySQLdb.Error, e:
				logger.error(u"Cannot delete from table (%s) records: (%s)\nMySql error: %d: %s" % (table, filename3, e.args[0], e.args[1]))
		cur.close()
	# end removeMythvideoSeekTable()


def updateMythRecorded(items):
	'''Add and delete MythTV (Watch Recordings) Miro recorded records. Add and delete symbolic links
	to coverart/Miro icons.
	Abort if processing failed
	return True if processing was successful
	'''
	global localhostname, vid_graphics_dirs, storagegroups, channel_id, simulation, imagemagick
	global graphic_suffix, graphic_path_suffix, graphic_name_suffix

	if not items: # There may not be any new items but a clean up of existing recorded may be required
		items = []
	items_copy = []
	for item in items:
		items_copy.append(item)

	# Deal with existing Miro videos already in the MythTV data base
	recorded = getRecorded()
	if recorded:
		for record in recorded:
			if storagegroups.has_key(u'default'):
				sym_filepath = u"%s%s" % (storagegroups[u'default'], record[u'basename'])
			else:
				sym_filepath = u"%s%s" % (vid_graphics_dirs[u'default'], record[u'basename'])
			# Remove any Miro related "watched" recordings (symlink, recorded and recordedprogram records)
			# Remove any Miro related with broken symbolic video links
			if record[u'watched'] == 1 or not os.path.isfile(os.path.realpath(sym_filepath)):
				displayMessage(u"Removing watched Miro recording (%s) (%s)" % (record[u'title'], record[u'subtitle']))
				(dirName, fileName) = os.path.split(sym_filepath)
				(fileBaseName, fileExtension)=os.path.splitext(fileName)
				try:
					dirName = unicode(dirName, u'utf8')
				except (UnicodeEncodeError, TypeError):
					pass
				try:
					fileBaseName = unicode(fileBaseName, u'utf8')
				except (UnicodeEncodeError, TypeError):
					pass
				for name in os.listdir(dirName):
					if name.startswith(fileBaseName):
						try:
							if simulation:
								logger.info(u"Simulation: Remove file/symbolic link(%s)" % (u"%s/%s" % (dirName, name,)))
							else:
								os.remove(u"%s/%s" % (dirName, name,))
						except OSError:
							pass
				# Remove the database recorded and recordedprogram records
				setRecord(u'recorded', record, channel_id, delete=True, id=None)
				setRecord(u'recordedprogram', record, channel_id, delete=True, id=None)
				setRecord(u'oldrecorded', record, channel_id, delete=True, id=None)
				removeRecordedSeekTable(record, channel_id)

	recorded = getRecorded()
	if recorded:
		for record in recorded:	# Skip any item already in MythTV data base
			for item in items:
				if item[u'channelTitle'] == record[u'title'] and item[u'title'] == record[u'subtitle']:
					items_copy.remove(item)
					break

	# Add new Miro unwatched videos to MythTV'd data base
	for item in items_copy:
		if not os.path.isfile(os.path.realpath(item[u'videoFilename'])):
			continue # Do not create records for Miro video files that do not exist
		records = createRecordedRecords(item)
		if records:
			if simulation:
				logger.info(u"Simulation: Added recorded and recordedprogram records for (%s - %s)" % (item[u'channelTitle'], item[u'title'],))
			else:
				setRecord(u'recorded', records[0], channel_id, delete=False, id=None)
				setRecord(u'recordedprogram', records[1], channel_id, delete=False, id=None)
				setRecord(u'oldrecorded', records[2], channel_id, delete=False, id=None)
				# Add a seek table
				cmd = mythcommflag_recordings % (records[0][u'chanid'], records[0][u'starttime'])
				#subprocess.call(u'%s' % cmd, shell=True) # Seek table creation has been disabled
		else:
			logger.critical(u"Creation of recorded and recordedprogram records failed for (%s - %s)" % (item[u'channelTitle'], item[u'title'],))
			sys.exit(False)

		if item[u'channel_icon']: # Add Cover art link to channel icon
			ext = getExtention(item[u'channel_icon'])
			coverart_filename = u"%s%s%s.%s" % (vid_graphics_dirs[u'posterdir'], item[u'channelTitle'], graphic_suffix[u'posterdir'], ext)
			if not os.path.isfile(os.path.realpath(coverart_filename)): # Make sure it does not exist
				if simulation:
					logger.info(u"Simulation: Remove symbolic link(%s)" % (coverart_filename,))
				else:
					try:	# Clean up broken symbolic link
						os.remove(coverart_filename)
					except OSError:
						pass
				if simulation:
					logger.info(u"Simulation: Create icon file(%s) cover art file(%s)" % (item[u'channel_icon'], coverart_filename))
				else:
					try:
						shutil.copy2(item[u'channel_icon'], coverart_filename)
						displayMessage(u"Copied a Miro Channel Icon file (%s) to MythTV as file (%s)." % (item[u'channel_icon'], coverart_filename))
					except OSError:
						logger.critical(u"Copying an icon file(%s) to coverart file(%s) failed.\nThis may be due to a permissions error." (item[u'channel_icon'], coverart_filename))
						sys.exit(False)

		if item[u'screenshot'] and imagemagick: # Add Miro screen shot to 'Default' recordings directory
			screenshot_recorded = u"%s%s.png" % (vid_graphics_dirs[u'default'], records[0][u'basename'])
			if not os.path.isfile(screenshot_recorded): # Make sure it does not exist
				if simulation:
					logger.info(u"Simulation: Create screenshot file(%s) as(%s)" % (item[u'screenshot'], screenshot_recorded))
				else:
					try:
						demensions = u''
						try:
							demensions = takeScreenShot(item[u'videoFilename'], screenshot_recorded, size_limit=True, just_demensions=False)
						except:
							pass
						if demensions:
							demensions = u"-size %s" % demensions
						useImageMagick(u'convert "%s" %s "%s"' % (item[u'screenshot'], demensions, screenshot_recorded))
						displayMessage(u"Used a Miro Channel screenshot file (%s) to\ncreate using ImageMagick the MythTV Watch Recordings screen shot file\n(%s)." % (item[u'screenshot'], screenshot_recorded))
					except OSError:
						logger.critical(u"Creating screenshot file(%s) as(%s) failed.\nThis may be due to a permissions error." (item[u'screenshot'], screenshot_recorded))
						sys.exit(False)
		else:
			screenshot_recorded = u"%s%s.png" % (vid_graphics_dirs[u'default'], records[0][u'basename'])
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

	if not items: # There may not be any new items but a clean up of existing records may be required
		items = []
	# Check that a MIRO video directory exists
	# if not then create the dir and add symbolic link to cover/file or icon
	createMiroMythVideoDirectory()

	# Remove any Miro Mythvideo records which the video or graphics paths are broken
	records = getMiroVideometadataRecords()
	if records:
		statistics[u'Total_Miro_MythVideos'] = len(records)
		for record in records: # Count the Miro-MythVideos that Miro is expiring or has saved
			if record[u'filename'][0] == u'/':
				if os.path.islink(record[u'filename']) and os.path.isfile(record[u'filename']):
					statistics[u'Total_Miro_expiring']+=1
			elif record[u'host'] and storagegroups.has_key(u'mythvideo'):
				if os.path.islink(storagegroups[u'mythvideo']+record[u'filename']) and os.path.isfile(storagegroups[u'mythvideo']+record[u'filename']):
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
					deleteVideometadataRecord(record[u'intid'])
					deleteOldrecordedForMythVideo(record[u'title'], record[u'subtitle'])
					removeMythvideoSeekTable(record[u'filename'])
					statistics[u'Total_Miro_MythVideos']-=1
					if record[u'filename'][0] == '/':
						try:
							if simulation:
								logger.info(u"Simulation: Remove video file symlink (%s)" % (record[u'filename']))
							else:
								os.remove(record[u'filename'])
								statistics[u'Miros_MythVideos_video_removed']+=1
						except OSError:
							pass
					elif record[u'host'] and storagegroups.has_key(u'mythvideo'):
						try:
							if simulation:
								logger.info(u"Simulation: Remove video file (%s)" % (storagegroups[u'mythvideo']+record[u'filename']))
							else:
								os.remove(storagegroups[u'mythvideo']+record[u'filename'])
						except OSError:
							pass
				if record[u'screenshot']: # Remove any associated Screenshot
					if record[u'screenshot'][0] == '/':
						try:
							if simulation:
								logger.info(u"Simulation: Remove screenshot symlink (%s)" % (record[u'screenshot']))
							else:
								os.remove(record[u'screenshot'])
						except OSError:
							pass
					elif record[u'host'] and storagegroups.has_key(u'episodeimagedir'):
						try:
							if simulation:
								logger.info(u"Simulation: Remove file (%s)" % (storagegroups[u'episodeimagedir']+record[u'screenshot']))
							else:
								os.remove(storagegroups[u'episodeimagedir']+record[u'screenshot'])
						except OSError:
							pass
				# Remove any unique cover art graphic files
				if record[u'title'].lower() in channel_icon_override:
					if record[u'coverfile'][0] == u'/':
						try:
							if simulation:
								logger.info(u"Simulation: Remove item cover art file (%s)" % (record[u'coverfile']))
							else:
								os.remove(record[u'coverfile'])
						except OSError:
							pass
					elif record[u'host'] and storagegroups.has_key(u'posterdir'):
						try:
							if simulation:
								logger.info(u"Simulation: Remove item cover art file (%s)" % (storagegroups[u'posterdir']+record[u'coverfile']))
							else:
								os.remove(storagegroups[u'posterdir']+record[u'coverfile'])
						except OSError:
							pass

	if not items: # There may not be any new items to add to MythVideo
		return True
	# Reread Miro Mythvideo videometadata records
	# Remove the matching videometadata record from array of items
	items_copy = []
	for item in items:
		items_copy.append(item)
	records = getMiroVideometadataRecords()
	if records:
		for record in records:
			for item in items:
				if item[u'channelTitle'] == record[u'title'] and item[u'title'] == record[u'subtitle']:
					try:
						items_copy.remove(item)
					except ValueError:
						logger.info(u"Video (%s - %s) was found multiple times in list of (watched and/or saved) items from Miro - skipping" % (item[u'channelTitle'], item[u'title']))
						pass
					break

	for item in items: # Remove any items that are for a Channel that does not get MythVideo records
		if filter(is_not_punct_char, item[u'channelTitle'].lower()) in channel_watch_only:
			try:	# Some items may have already been removed, let those passed
				items_copy.remove(item)
			except ValueError:
				pass
	# Add Miro videos that remain in the item list
	# If not a flat directory check if title directory exists and add icon symbolic link as coverfile
	for item in items_copy:
		if not flat and not item.has_key(u'copied'):
			createMiroChannelSubdirectory(item)
		if not item[u'screenshot']: # If there is no screen shot then create one
			screenshot_mythvideo = u"%s%s - %s%s.jpg" % (vid_graphics_dirs[u'episodeimagedir'], item[u'channelTitle'], item[u'title'], graphic_suffix[u'episodeimagedir'])
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
			logger.info(u"Simulation: Create videometadata record for (%s - %s)" % (item[u'channelTitle'], item[u'title']))
		else:
			intid = mythvideo.getMetadataId(videometadata[u'filename']) # Check for duplicates
			if intid == None:
				intid = mythvideo.setMetadata(videometadata, id=None)
				if not intid:
					logger.critical(u"Adding Miro video to MythVideo (%s - %s) failed." % (item[u'channelTitle'], item[u'title']))
					sys.exit(False)
				if not item.has_key(u'copied'):
					setRecord(u'oldrecorded', oldrecorded, channel_id, delete=False, id=None)
				if videometadata[u'filename'][0] == u'/':
					cmd = mythcommflag_videos % videometadata[u'filename']
				elif videometadata[u'host'] and storagegroups[u'mythvideo']:
					cmd = mythcommflag_videos % ((storagegroups[u'mythvideo']+videometadata[u'filename']))
				#subprocess.call(u'%s' % cmd, shell=True) # Seek table creation disabled
				statistics[u'Miros_MythVideos_added']+=1
				statistics[u'Total_Miro_expiring']+=1
				statistics[u'Total_Miro_MythVideos']+=1
				displayMessage(u"Added Miro video to MythVideo (%s - %s)" % (videometadata[u'title'], videometadata[u'subtitle']))
			else:
				sys.stdout.write(u'')
				displayMessage(u"Skipped adding a duplicate Miro video to MythVideo:\n(%s - %s)\nSometimes a Miro channel has the same video downloaded multiple times.\nThis is a Miro/Channel web site issue and often rectifies itself overtime.\n" % (videometadata[u'title'], videometadata[u'subtitle']))

	return True
	# end updateMythVideo()

def printStatistics():
	global statistics

	# Print statistics
	sys.stdout.write(u"\n\n-------------------Statistics--------------------\nNumber of Watch Recording's watched...... (% 5d)\nNumber of Miro videos marked as seen..... (% 5d)\nNumber of Miro videos deleted............ (% 5d)\nNumber of New Miro videos downloaded..... (% 5d)\nNumber of Miro/MythVideo's removed....... (% 5d)\nNumber of Miro/MythVideo's added......... (% 5d)\nNumber of Miro videos copies to MythVideo (% 5d)\n-------------------------------------------------\nTotal Unwatched Miro/Watch Recordings.... (% 5d)\nTotal Miro/MythVideo videos to expire.... (% 5d)\nTotal Miro/MythVideo videos.............. (% 5d)\n-------------------------------------------------\n\n" % (statistics[u'WR_watched'],  statistics[u'Miro_marked_watch_seen'], statistics[u'Miro_videos_deleted'], statistics[u'Miros_videos_downloaded'], statistics[u'Miros_MythVideos_video_removed'], statistics[u'Miros_MythVideos_added'], statistics[u'Miros_MythVideos_copied'], statistics[u'Total_unwatched'], statistics[u'Total_Miro_expiring'], statistics[u'Total_Miro_MythVideos'], ))
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
	global local_only

 	parser = OptionParser(usage=u"%prog usage: mirobridge -huevstdociVHSCWM [parameters]\n")

	parser.add_option(  "-e", "--examples", action="store_true", default=False, dest="examples",
                        help=u"Display examples for executing the jamu script")
	parser.add_option(  "-v", "--version", action="store_true", default=False, dest="version",
                        help=u"Display version and author information")
 	parser.add_option(  "-s", "--simulation", action="store_true", default=False, dest="simulation",
                        help=u"Simulation (dry run), no files are copied, symlinks created or MythTV data bases altered. If option (-n) is NOT specified Miro auto downloads WILL take place. See option (-n) help for details.")
  	parser.add_option(  "-t", "--testenv", action="store_true", default=False, dest="testenv",
                        help=u"Test that the local environment can run all mirobridge functionality")
  	parser.add_option(  "-n", "--no_autodownload", action="store_true", default=False, dest="no_autodownload",
                        help=u"Do not perform Miro Channel updates, video expiry and auto-downloadings. Default is to perform all perform all Channel maintenance features.")
  	parser.add_option(  "-o", "--nosubdirs", action="store_true", default=False, dest="nosubdirs",
                        help=u"Organise MythVideo's Miro directory WITHOUT Miro channel subdirectories. The default is to have Channel subdirectories.")
  	parser.add_option(  "-c", "--channel", metavar="CHANNEL_ID:CHANNEL_NUM", default="", dest="channel",
                        help=u'Specifies the channel id that is used for Miros unplayed recordings. Enter as "xxxx:yyy". Default is 9999:999. Be warned that once you change the default channel_id "9999" you must always use this option!')
  	#parser.add_option(  "-i", "--import", metavar="CONFIGFILE", default="", dest="import",
    #                    help=u'Import Miro exported configuration file and or channel changes.')
  	parser.add_option(  "-V", "--verbose", action="store_true", default=False, dest="verbose",
                        help=u"Display verbose messages when processing")
  	parser.add_option(  "-H", "--hostname", metavar="HOSTNAME", default="", dest="hostname",
                        help=u"MythTV Backend hostname mirobridge is to up date")
  	parser.add_option(  "-S", "--sleeptime", metavar="SLEEP_DELAY_SECONDS", default="", dest="sleeptime",
                        help=u"The amount of seconds to wait for an auto download to start.\nThe default is 60 seconds, but this may need to be adjusted for slower Internet connections.")
  	parser.add_option(  "-C", "--addchannel", metavar="ICONFILE_PATH", default="OFF", dest="addchannel",
                        help=u'Add a Miro Channel record to MythTV. This gets rid of the "#9999 #9999" on the Watch Recordings screen and replaces it with the usual\nthe channel number and channel name.\nThe default if not overridden by the (-c) option is channel number 999.\nIf a filename and path is supplied it will be set as the channels icon. Make sure your override channel number is NOT one of your current MythTV channel numbers.\nThis option is typically only used once as there can only be one Miro channel record at a time.')
  	parser.add_option(  "-N", "--new_watch_copy", action="store_true", default=False, dest="new_watch_copy",
                        help=u'For ALL Miro Channels: Use the "Watch Recording" screen to watch new Miro downloads then once watched copy the videos, icons, screen shot and metadata to MythVideo. Once coping is complete delete the video from Miro.\nThis option overrides any "mirobridge.conf" settings.')
  	parser.add_option(  "-W", "--watch_only", action="store_true", default=False, dest="watch_only",
                        help=u'For ALL Miro Channels: Only use "Watch Recording" never move any Miro videos to MythVideo.\nThis option overrides any "mirobridge.conf" settings.')
  	parser.add_option(  "-M", "--mythvideo_only", action="store_true", default=False, dest="mythvideo_only",
                        help=u'For ALL Miro Channel videos: Copy newly downloaded Miro videos to MythVideo and removed from Miro. These Miro videos never appear in the MythTV "Watch Recording" screen.\nThis option overrides any "mirobridge.conf" settings.')


	opts, args = parser.parse_args()

	if opts.examples:					# Display example information
		sys.stdout.write(examples_txt+'\n')
		sys.exit(True)

	if opts.version:		# Display program information
		sys.stdout.write(u"\nTitle: (%s); Version: description(%s); Author: (%s)\n%s\n" % (
		__title__, __version__, __author__, __purpose__ ))
		sys.exit(True)

	if opts.testenv:
		test_environment = True
	else:
		test_environment = False

	# Verify that Miro is not currently running
	if isMiroRunning():
		sys.exit(False)

	# Verify that only None or one of the mutually exclusive (-W), (-M) and (-N) options is being used
	x = 0
	if opts.new_watch_copy: x+=1
	if opts.watch_only: x+=1
	if opts.mythvideo_only: x+=1
	if x > 1:
		logger.critical(u"The (-W), (-M) and (-N) options are mutually exclusive, so only one can be specified at a time.")
		sys.exit(False)

	# Set option related global variables
	simulation = opts.simulation
	verbose = opts.verbose
	if opts.hostname:	# Override localhostname if the user specified an hostname
		localhostname = opts.hostname

	# Validate settings
	# Make sure mirobridge is to update a real MythTV backend
	if not mythdb.getSetting(u'BackendServerIP', hostname = localhostname):
		logger.critical(u"The MythTV backend (%s) is not a MythTV backend." % localhostname)
		if test_environment:
			requirements_are_met = False
		else:
			sys.exit(False)

	## Video base directory and current version and revision numbers
	base_video_dir = config.get(prefs.MOVIES_DIRECTORY)
	miro_version_rev = u"%s r%s" % (config.get(prefs.APP_VERSION), config.get(prefs.APP_REVISION_NUM))

	displayMessage(u"Miro Version (%s)" % (miro_version_rev))
	displayMessage(u"Base Miro Video Directory (%s)" % (base_video_dir,))
	logger.info(u'')

	# Verify Miro version sufficent and Video file configuration correct.
	if not os.path.isdir(base_video_dir):
		logger.critical(u"The Miro Videos directory (%s) does not exist." % str(base_video_dir))
		if test_environment:
			requirements_are_met = False
		else:
			sys.exit(False)

	if config.get(prefs.APP_VERSION) < u"2.0.3":
		logger.critical(u"The installed version of Miro (%s) is too old. It must be at least v2.0.3 or higher." % config.get(prefs.APP_VERSION))
		if test_environment:
			requirements_are_met = False
		else:
			sys.exit(False)

	# Get storage groups
	if getStorageGroups() == False:
		logger.critical(u"Retrieving storage groups from the MythTV data base failed")
		if test_environment:
			requirements_are_met = False
		else:
			sys.exit(False)
	elif not u'default' in storagegroups.keys():
		logger.critical(u"There must be a 'Default' storage group")
		if test_environment:
			requirements_are_met = False
		else:
			sys.exit(False)

	if opts.channel:
		channel = opts.channel.split(u':')
		if len(channel) != 2:
			logger.critical(u"The Channel (%s) must be in the format xxx:yyy with x an y all numeric." % str(opts.channel))
			if test_environment:
				requirements_are_met = False
			else:
				sys.exit(False)
		elif not _can_int(channel[0]) or  not _can_int(channel[1]):
			logger.critical(u"The Channel_id (%s) and Channel_num (%s) must be numeric." % (channel[0], channel[1]))
			if test_environment:
				requirements_are_met = False
			else:
				sys.exit(False)
		else:
			channel_id = int(channel[0])
			channel_num = int(channel[1])

	if opts.sleeptime:
		if not _can_int(opts.sleeptime):
			logger.critical(u"Auto-dewnload sleep time (%s) must be numeric." % str(opts.sleeptime))
			if test_environment:
				requirements_are_met = False
			else:
				sys.exit(False)
		else:
			download_sleeptime = float(opts.sleeptime)

	getMythtvDirectories()	# Initialize all the Video and graphics directory dictionary

	if opts.nosubdirs: # Did the user want a flat MythVideo "Miro" directory structure?
		flat = True

	# Get the values in the mirobridge.conf configuration file
	setUseroptions()

	if opts.watch_only:		# ALL Miro videos will only be viewed in the MythTV "Watch Recordings" screen
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
				logger.critical(u'The Miro Channel (%s) cannot be used as both a "Mythvideo Only" and "New-Watch-Copy" channel.' % key)
				if test_environment:
					requirements_are_met = False
				else:
					sys.exit(False)

	# Verify that ImageMagick is installed
	ret = useImageMagick(u"convert -version")
	if ret < 0 or ret > 1:
		logger.critical(u"ImageMagick must be installed, graphics cannot be resized or converted to the required graphics format (e.g. jpg and or png)")
		if test_environment:
			requirements_are_met = False
		else:
			sys.exit(False)

	# Verify that mythcommflag is installed
	mythcommflagpath = getlocationMythcommflag()
	if mythcommflagpath:
		mythcommflag_recordings = mythcommflag_recordings % mythcommflagpath
		mythcommflag_videos = mythcommflag_videos % mythcommflagpath
	else:
		logger.critical(u"mythcommflag must be installed so that Miro video seek tables can be built.")
		if test_environment:
			requirements_are_met = False
		else:
			sys.exit(False)

	if opts.testenv:		# All tests passed
		getVideoDetails(u"") # Test that ffmpeg is available
		if ffmpeg and requirements_are_met:
			logger.info(u"The environment test passed !\n\n")
			sys.exit(True)
		else:
			logger.critical(u"The environment test FAILED. See previously displayed error messages!")
			sys.exit(False)

	if opts.addchannel != u'OFF':	# Add a Miro Channel record - Should only be done once
		createChannelRecord(opts.addchannel, channel_id, channel_num)
		logger.info(u"The Miro Channel record has been successfully created !\n\n")
		sys.exit(True)


	###########################################
	# Mainlogic for all Miro and MythTV bridge
	###########################################

	#
	# Start the Miro Front and Backend - This allows mirobridge to execute actions on the Miro backend
	#
	displayMessage(u"Starting Miro Frontend and Backend")
	startup.initialize(config.get(prefs.THEME_NAME))
	app.cli_events = EventHandler()
	app.cli_events.connect_to_signals()
	startup.startup()
	app.cli_events.startup_event.wait()
	if app.cli_events.startup_failure:
		logger.critical(u"Starting Miro Frontend and Backend failed: (%s)" % app.cli_events.startup_failure[0])
		print_text(app.cli_events.startup_failure[1])
		app.controller.shutdown()
		time.sleep(5) # Let the shutdown processing complete
		sys.exit(False)
	app.cli_interpreter = MiroInterpreter()
	if opts.verbose:
		app.cli_interpreter.verbose = True
	else:
		app.cli_interpreter.verbose = False
	app.cli_interpreter.simulation = opts.simulation
	app.cli_interpreter.videofiles = []
	app.cli_interpreter.downloading = False
	app.cli_interpreter.icon_cache_dir = config.get(prefs.ICON_CACHE_DIRECTORY)
	app.cli_interpreter.imagemagick = imagemagick
	app.cli_interpreter.statistics = statistics
	if config.get(prefs.APP_VERSION) < u"2.5.0":
		app.renderer = app.cli_interpreter
	else:
		app.movie_data_program_info = app.cli_interpreter.movie_data_program_info

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
		displayMessage(u"Starting Miro delete of videos deleted in the MythTV Watched Recordings screen.")
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
					displayMessage(u"Skipped adding a duplicate Miro video to the MythTV Watch Recordings screen (%s - %s) which is already in MythVideo.\nSometimes a Miro channel has the same video downloaded multiple times.\nThis is a Miro/Channel web site issue and often rectifies itself overtime." % (item[u'channelTitle'], item[u'title']))
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
			displayMessage(u"Skipped adding a Miro video to the MythTV Watch Recordings screen (%s - %s) as there are duplicate 'new' video items.\nSometimes a Miro channel has the same video downloaded multiple times.\nThis is a Miro/Channel web site issue and often rectifies itself overtime." % (duplicate[u'channelTitle'], duplicate[u'title']))
		except ValueError:
			pass

	#
	# Deal with any Channel videos that are to be copied and removed from Miro
	#
	copy_items = []
	# Copy unwatched and watched Miro videos (all or only selected Channels)
	if u'all' in channel_mythvideo_only:
		for array in [watched, unwatched]:
			for item in array:
				copy_items.append(item)
	elif len(channel_mythvideo_only):
		for array in [watched, unwatched]:
			for video in array:
				if filter(is_not_punct_char, video[u'channelTitle'].lower()) in channel_mythvideo_only.keys():
					copy_items.append(video)
	# Copy ONLY watched Miro videos (all or only selected Channels)
	if u'all' in channel_new_watch_copy:
		for video in watched:
			copy_items.append(video)
	elif len(channel_new_watch_copy):
		for video in watched:
			if filter(is_not_punct_char, video[u'channelTitle'].lower()) in channel_new_watch_copy.keys():
				copy_items.append(video)

	channels_to_copy = {}
	for key in channel_mythvideo_only.keys():
		channels_to_copy[key] = channel_mythvideo_only[key]
	for key in channel_new_watch_copy.keys():
		channels_to_copy[key] = channel_new_watch_copy[key]

	for video in copy_items:
		dir_key = filter(is_not_punct_char, video[u'channelTitle'].lower())
		# Create the subdirectories to copy the video into
		directory_coverart = False
		if not os.path.isdir(channels_to_copy[dir_key]):
			if simulation:
				logger.info(u"Simulation: Creating the MythVideo directory (%s)." % (channels_to_copy[dir_key]))
			else:
				os.makedirs(channels_to_copy[dir_key])
			directory_coverart = True	# If the directory was just created it needs coverart
		else:
			if video[u'channel_icon']:
				ext = getExtention(video[u'channel_icon'])
				if not os.path.isfile(u"%s%s.%s" % (channels_to_copy[dir_key], video[u'channelTitle'].lower(), ext)):
					directory_coverart = True	# If the directory was just created it needs coverart
			elif video[u'item_icon']:
				ext = getExtention(video[u'item_icon'])
				if not os.path.isfile(u"%s%s - %s.%s" % (channels_to_copy[dir_key], video[u'channelTitle'].lower(), video[u'title'].lower(), ext)):
					directory_coverart = True	# If the directory was just created it needs coverart

		# Copy the Channel icon located in the posters/coverart directory
		if directory_coverart and video[u'channel_icon']:
			ext = getExtention(video[u'channel_icon'])
			tmp_path = channels_to_copy[dir_key][:-1]
			foldername = tmp_path[tmp_path.rindex(u'/')+1:]
			dirpath = u"%s%s" % (channels_to_copy[dir_key], u'folder.jpg')
			dirpath2 = u"%s%s" % (channels_to_copy[dir_key], u'folder.png')
			if os.path.isfile(dirpath) or os.path.isfile(dirpath2): # See if a folder cover already exists
				pass
			else:
				if simulation:
					logger.info(u"Simulation: Copy a Channel Icon (%s) for directory (%s)." % (video[u'channel_icon'], dirpath))
				else:
					try:	# Miro Channel icon copy for the new subdirectory
						useImageMagick(u'convert "%s" "%s"' % (video[u'channel_icon'], dirpath))
					except:
						logger.critical(u"Copy a Channel Icon (%s) for directory (%s) failed." % (video[u'channel_icon'], dirpath))
						# Gracefully close the Miro database and shutdown the Miro Front and Back ends
						app.controller.shutdown()
						time.sleep(5) # Let the shutdown processing complete
						sys.exit(False)

		# Copy the Miro video file
		save_video_filename = video[u'videoFilename'] # This filename is needed later for deleting in Miro
		ext = getExtention(video[u'videoFilename'])
		if ext.lower() == u'm4v':
			ext = u'mpg'
		filepath = u"%s%s - %s.%s" % (channels_to_copy[dir_key], video[u'channelTitle'], video[u'title'], ext)
		if simulation:
			logger.info(u"Simulation: Copying the Miro video (%s) to the MythVideo directory (%s)." % (video[u'videoFilename'], filepath))
		else:
			try:	# Miro video copied into a MythVideo directory
				shutil.copy2(video[u'videoFilename'], filepath)
				statistics[u'Miros_MythVideos_copied']+=1
				if u'mythvideo' in storagegroups.keys() and not local_only:
					video[u'videoFilename'] = filepath.replace(storagegroups[u'mythvideo'], u'')
				else:
					video[u'videoFilename'] = filepath
			except:
				logger.critical(u"Copying the Miro video (%s) to the MythVideo directory (%s).\n         This maybe a permissions error (mirobridge.py does not have permission to write to the directory)." % (video[u'videoFilename'], filepath))
				# Gracefully close the Miro database and shutdown the Miro Front and Back ends
				app.controller.shutdown()
				time.sleep(5) # Let the shutdown processing complete
				sys.exit(False)

		# Copy the Channel or item's icon
		if video[u'channel_icon'] and not video[u'channelTitle'].lower() in channel_icon_override:
			pass
		else:
			if video[u'item_icon']:
				video[u'channel_icon'] = video[u'item_icon']
		if video[u'channel_icon']:
			ext = getExtention(video[u'channel_icon'])
			if video[u'channelTitle'].lower() in channel_icon_override:
				filepath = u"%s%s - %s%s.%s" % (vid_graphics_dirs[u'posterdir'], video[u'channelTitle'], video[u'title'], graphic_suffix[u'posterdir'], ext)
			else:
				filepath = u"%s%s%s.%s" % (vid_graphics_dirs[u'posterdir'], video[u'channelTitle'], graphic_suffix[u'posterdir'], ext)
		# There may already be a Channel icon available or it is a symlink which needs to be replaced
		if not os.path.isfile(filepath) or os.path.islink(filepath):
			if simulation:
				logger.info(u"Simulation: Copying the Channel Icon (%s) to the poster directory (%s)." % (video[u'channel_icon'], filepath))
			else:
				try:	# Miro Channel icon copied into a MythVideo directory
					try: # Remove any old symlink file
						os.remove(filepath)
					except OSError:
						pass
					shutil.copy2(video[u'channel_icon'], filepath)
					if u'posterdir' in storagegroups.keys() and not local_only:
						video[u'channel_icon'] = filepath.replace(storagegroups[u'posterdir'], u'')
					else:
						video[u'channel_icon'] = filepath
				except:
					logger.critical(u"Copying the Channel Icon (%s) to the poster directory (%s).\n         This maybe a permissions error (mirobridge.py does not have permission to write to the directory)." % (video[u'channel_icon'], filepath))
					# Gracefully close the Miro database and shutdown the Miro Front and Back ends
					app.controller.shutdown()
					time.sleep(5) # Let the shutdown processing complete
					sys.exit(False)
		else:
			if u'posterdir' in storagegroups.keys() and not local_only:
				video[u'channel_icon'] = filepath.replace(storagegroups[u'posterdir'], u'')
			else:
				video[u'channel_icon'] = filepath

		# There may already be a Screenshot available or it is a symlink which needs to be replaced
		if video[u'screenshot']:
			ext = getExtention(video[u'screenshot'])
			filepath = u"%s%s - %s%s.%s" % (vid_graphics_dirs[u'episodeimagedir'], video[u'channelTitle'], video[u'title'], graphic_suffix[u'episodeimagedir'], ext)
		else:
			filepath = u''

		if not os.path.isfile(filepath) or os.path.islink(filepath):
			if video[u'screenshot']:
				if simulation:
					logger.info(u"Simulation: Copying the Screenshot (%s) to the Screenshot directory (%s)." % (video[u'screenshot'], filepath))
				else:
					try:	# Miro Channel icon copied into a MythVideo directory
						try: # Remove any old symlink file
							os.remove(filepath)
						except OSError:
							pass
						shutil.copy2(video[u'screenshot'], filepath)
						displayMessage(u"Copied Miro screenshot file (%s) to MythVideo (%s)" % (video[u'screenshot'], filepath))
						if u'episodeimagedir' in storagegroups.keys() and not local_only:
							video[u'screenshot'] = filepath.replace(storagegroups[u'episodeimagedir'], u'')
						else:
							video[u'screenshot'] = filepath
					except:
						logger.critical(u"Copying the Screenshot (%s) to the Screenshot directory (%s).\n         This maybe a permissions error (mirobridge.py does not have permission to write to the directory)." % (video[u'screenshot'], filepath))
						# Gracefully close the Miro database and shutdown the Miro Front and Back ends
						app.controller.shutdown()
						time.sleep(5) # Let the shutdown processing complete
						sys.exit(False)
		elif video[u'screenshot']:
			if u'episodeimagedir' in storagegroups.keys() and not local_only:
				video[u'screenshot'] = filepath.replace(storagegroups[u'episodeimagedir'], u'')
			else:
				video[u'screenshot'] = filepath
		video[u'copied'] = True 	# Mark this video item as being copied

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
				if not filter(is_not_punct_char, video[u'channelTitle'].lower()) in channel_mythvideo_only.keys():
					unwatched_copy.append(video)
				else:
					watched.append(video)
			unwatched = unwatched_copy

	statistics[u'Total_unwatched'] = len(unwatched)
	if not len(unwatched):
		displayMessage(u"There are no Miro unwatched video items to add as MythTV Recorded videos.")
	if not updateMythRecorded(unwatched):
		logger.critical(u"Updating MythTV Recording with Miro video files failed." % str(base_video_dir))
		sys.exit(False)

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
		sys.exit(False)

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
		sys.exit(False)

	main()
	displayMessage(u"Miro Bridge Processing completed")
