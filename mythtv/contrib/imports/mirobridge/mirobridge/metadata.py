#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: metadata.py   Performs all metadata/graphics functions for MiroBridge
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script performs metadata/graphics functions for mirobridge.py
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="metadata - Maintains Miro's Video files with MythTV";
__author__="R.D.Vaughan"
__purpose__='''
This python script performs metadata/graphics functions for mirobridge.py'''

__version__="v0.2.0"
# 0.1.0 Initial development
# 0.2.0 Fixed unicode error
#       Added a filter on punctuation when finding TVDB title matches

# Global imports
import os, sys, subprocess, re, glob, string

# Verify that tvdb_api.py, tvdb_ui.py and tvdb_exceptions.py are available
try:
    # thetvdb.com specific modules
    import MythTV.ttvdb.tvdb_ui as tvdb_ui
    # from tvdb_api import Tvdb
    import MythTV.ttvdb.tvdb_api as tvdb_api
    from MythTV.ttvdb.tvdb_exceptions import (tvdb_error, tvdb_shownotfound, tvdb_seasonnotfound, tvdb_episodenotfound, tvdb_episodenotfound, tvdb_attributenotfound, tvdb_userabort)

    # verify version of tvdbapi to make sure it is at least 1.0
    if tvdb_api.__version__ < '1.0':
        print "\nYour current installed tvdb_api.py version is (%s)\n" % tvdb_api.__version__
        raise
except Exception, e:
    print '''
The modules tvdb_api.py (v1.0.0 or greater), tvdb_ui.py, tvdb_exceptions.py and cache.py.
They should have been installed along with the MythTV python bindings.
Error:(%s)
''' %  e
    sys.exit(1)

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

# Used for TVDB title search and matching
def is_punct_char(char):
    '''check if char is punctuation char
    return True if char is punctuation
    return False if char is not punctuation
    '''
    return char in string.punctuation

def is_not_punct_char(char):
    '''check if char is not punctuation char
    return True if char is not punctuation
    return False if chaar is punctuation
    '''
    return not is_punct_char(char)


class MetaData(object):
    """MetaData functions for MiroBridge

    Supports function to acquire, create, delete graphics for Miro videos. Also perform Miro channel look ups in ttvdb.com and create
    a Recoding rule when a Miro Channel is found on ttvdb.com. Use the the local graphics for any Miro Channel that has a Recording rule
    which includes an inetref.
    """
    def __init__(self,
                mythdb,
                Video,
                Record,
                storagegroups,
                vid_graphics_dirs,
                channel_id,
                ffmpeg,
                logger,
                simulation,
                verbose,
                ):
        """ mythdb:
            Access to MythTV data base methods

            Video:
            Access videometatdata record methods

            Record:
            Access Record rule methods

            storagegroups:
            Dictionary of any storage groups for this MythTV backend

            vid_graphics_dirs:
            Dictionary of this MythTV backend's graphics directories

            channel_id:
            The Channel id number used in the the creation of Record Rules

            ffmpeg:
            Flag indicating that ffmpeg is installed and functioning

            logger:
            Console logging method

            simulation:
            All file and database (write and delete) actions are simulated only

            verbose:
            Level of console messaging
        """
        self.mythdb = mythdb
        self.Video = Video
        self.Record = Record
        self.storagegroups = storagegroups
        self.vid_graphics_dirs = vid_graphics_dirs
        self.ffmpeg = ffmpeg
        self.logger = logger
        self.simulation = simulation
        self.verbose = verbose
        confdir = os.environ.get('MYTHCONFDIR', '')
        if (not confdir) or (confdir == '/'):
            confdir = os.environ.get('HOME', '')
            if (not confdir) or (confdir == '/'):
                print "Unable to find MythTV directory for metadata cache."
                sys.exit(1)
            confdir = os.path.join(confdir, '.mythtv')
        self.cache_dir=os.path.join(confdir, "cache/tvdb_api/")
        self.ttvdb = tvdb_api.Tvdb(banners=False, debug = False, cache = self.cache_dir, custom_ui=None, language = u'en', apikey="0BB856A59C51D607")
        self.makeRecordRule = {'chanid': channel_id, 'description': u'Mirobridge Record rule used to acquire ttvdb.com artwork',
                               'autometadata': 1, 'season': 1, 'episode': 1, 'category': u'Miro', 'station': u'Miro'  }
    # end __init__()

    def getMetadata(self, title):
        """Find existing graphics for a Miro video.
        return a dictionary of metadata
        """
        ttvdbGraphics = {'inetref': u'', 'coverart': u'', 'banner': u'', 'fanart': u'', } # Initalize dictionary of graphics

        # Check for an existing Record Rule for this Miro Channel title
        recordedRules_array = list(self.mythdb.searchRecord(title=title))

        # If there is no Record rule then check ttvdb.com
        if not len(recordedRules_array):
            inetref = self.searchTvdb(title)
            if inetref != None:     # Create a new rule for this Miro Channel title
                ttvdbGraphics['inetref'] = inetref
                self.makeRecordRule['title'] = title
                self.makeRecordRule['inetref'] = inetref
                if self.simulation:
                    self.logger.info(u"Simulation: Create Recoding rule for Title(%s) inetref(%s)" % (self.makeRecordRule['title'], self.makeRecordRule['inetref'], ))
                else:
                    record = self.Record().create(self.makeRecordRule)
                    if self.verbose:
                        self.logger.info(u"Created Recoding rule for Title(%s) inetref(%s)" % (self.makeRecordRule['title'], self.makeRecordRule['inetref'], ))
            return ttvdbGraphics

        # Find a matching recordedartwork record to acquire the graphic images
        ttvdbGraphics['inetref'] = recordedRules_array[0]['inetref']
        artworkArray = list(self.mythdb.searchArtwork(inetref=recordedRules_array[0]['inetref']))
        if len(artworkArray):
            for key in ttvdbGraphics.keys():
                ttvdbGraphics[key] = artworkArray[0][key]

        # Check if the coverart exists even though a recordartwork record did not have any.
        # This makes sure that low quality coverart is not automatically created when an image already exists.
        if not ttvdbGraphics['coverart']:
            result = glob.glob(u'%s%s Season*_coverart.*' % (self.vid_graphics_dirs['posterdir'], title, ))
            if len(result):
                ttvdbGraphics['coverart'] = result[0]

        return ttvdbGraphics
    # end getMetadata()

    def convertOldMiroVideos(self):
        """Convert any old Miro videometadata records from category from "Miro"
        to "Video Cast" and inetref from '99999999' to ''
        This method is required to prevent the accidental removal of copied Miro
        videos due to the inetref of '99999999' no longer being used. This allows
        to the "watch_then_copy" and "mythvideo_only" config options to still work.
        return count of converted videometadata records
        """
        videometadata = list(self.mythdb.searchVideos(category=u'Miro', custom=(('inetref=%s',u'99999999'),)))
        if not len(videometadata):
            return
        for record in videometadata:
            # Make sure the record is an absolute path
            if not record[u'host'] or record[u'filename'][0] == '/':
                filename = record[u'filename']
            else:
                filename = self.vid_graphics_dirs['mythvideo']+record[u'filename']

            # Skip any videos that are still expiring in Miro
            if os.path.islink(filename):
                continue

            # Convert the old Miro copied video to be independant of MiroBridge processing
            graphics = self.getMetadata(record['title'])
            record['inetref'] = graphics['inetref']
            record['category'] = u'Video Cast'
            if self.simulation:
                self.logger.info(u"Simulation: Converting old copied Miro video Title(%s) file(%s)" % (record[u'title'], record[u'filename'], ))
            else:
                self.Video(record['intid'], db=self.mythdb).update(record)
                if self.verbose:
                    self.logger.info(u"Converting old copied Miro video Title(%s) file(%s)" % (record[u'title'], record[u'filename'], ))
        return
    # end convertOldMiroVideos()

    def cleanupVideoAndGraphics(self, fileName):
        """ Attempt a clean up video files and/or graphics from a directory
        return nothing
        """
        (dirName, fileName) = os.path.split(fileName)
        (fileBaseName, fileExtension)=os.path.splitext(fileName)
        try:
            results = glob.glob(u'%s/%s*' % (dirName, fileBaseName, ))
        except (UnicodeEncodeError, UnicodeDecodeError, TypeError):
            return
        if len(results):
            for result in results:
                try:
                    if self.simulation:
                        self.logger.info(u"Simulation: Remove file/symbolic link(%s)" % result)
                    else:
                        os.remove(result)
                except (UnicodeEncodeError, UnicodeDecodeError, TypeError, OSError):
                    pass
        return
        # end cleanupVideoAndGraphics()

    def deleteFile(self, filename, host, groupKey):
        """Delete a file that may or may not exist
        return nothing
        """
        if filename[0] == u'/':
            try:
                if self.simulation:
                    self.logger.info(u"Simulation: Remove of a file (%s)" % (filename))
                else:
                    os.remove(filename)
            except OSError:
                pass
        elif host and self.storagegroups.has_key(groupKey):
            try:
                if self.simulation:
                    self.logger.info(u"Simulation: Remove of a file (%s)" % (self.storagegroups[groupKey]+filename))
                else:
                    os.remove(self.storagegroups[groupKey]+filename)
            except OSError:
                pass
        return
    # end deleteFile()

    def takeScreenShot(self, videofile, screenshot_filename, size_limit=False, just_demensions=False):
        '''Take a screen shot 1/8th of the way into a video file
        return the fully qualified screen shot name
        >>> True - If the screenshot was created
        >>> False - If ffmpeg could not find details for the video file
        '''
        try:
            ffmpeg_details = self.getVideoDetails(videofile, screenshot=True)
        except:
            return None
        if not ffmpeg_details:
            return None
        elif not u'width' in ffmpeg_details.keys():
            return None

        max_length = 2*60    # Maximum length of a video check if set at 2 minutes
        if ffmpeg_details[u'duration']:
            if ffmpeg_details[u'duration'] < max_length:
                delay = (ffmpeg_details[u'duration']/2) # Maximum time to take screenshot into a video is 1 minute
            else:
                delay = 60 # For a large videos take screenshot at the 1 minute mark

        cmd = u'ffmpeg -i "%s" -y -f image2 -ss %d -sameq -vframes 1 -s %d*%d "%s"'

        width = int(ffmpeg_details[u'width'])
        height = int(ffmpeg_details[u'height'])

        if size_limit:
            width = 320
            height = (int(width * ffmpeg_details[u'aspect'])/2)*2 # ((xxxxx)/2) *2

        if just_demensions:
            return u"%dx%d" % (width, height)

        cmd2 = cmd % (videofile, delay, width, height, screenshot_filename)
        return subprocess.call(u'%s > /dev/null 2>/dev/null' % cmd2, shell=True)
    # end takeScreenShot()

    def getVideoDetails(self, videofilename, screenshot=False):
        '''Using ffmpeg (if it can be found) get a video file's details
        return False if ffmpeg is cannot be found
        return empty dictionary if the file is not a video
        return dictionary of the Video's details
        '''
        ffmpeg_details = {u'video': u'', u'audio': u'', u'duration': 0, u'stereo': 0, u'hdtv': 0}

        if not self.ffmpeg and not screenshot:
            return ffmpeg_details
        if not self.ffmpeg and screenshot:
            return False

        video = re.compile(u' Video: ')
        video_HDTV_small = re.compile(u' 1280x', re.UNICODE)
        video_HDTV_large = re.compile(u' 1920x', re.UNICODE)
        width_height = re.compile(u'''^(.+?)\[?([0-9]+)x([0-9]+)\\,[^\\/]''', re.UNICODE)

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
            if datacount == alldata:    # Stop if all the required data has been extracted from ffmpeg's output
                break
            data = p.stderr.readline()
            if data == '':
                break
            try:
                data = unicode(data, 'utf8')
            except (UnicodeDecodeError):
                continue    # Skip any line that has non-utf8 characters in it
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
            self.ffmpeg = False
            return False
        else:
            return ffmpeg_details
    # end getVideoDetails()

    # Verify that a Miro Channel title exists on thetvdb.com
    def searchTvdb(self, series_name):
        """ Search for a Miro Channel in ttvdb.com. There must be an exact match.
        return If an exact match was found then return the ttvdb sid (inetref) number
        return If no match then return nothing
        """
        try:
            # Search for the series
            allSeries = self.ttvdb._getSeries(series_name)
        except tvdb_shownotfound:
            # No such show found.
            return None
        except Exception, e:
            # Error communicating with thetvdb.com
            return None

        if filter(is_not_punct_char, allSeries['name'].lower()) == \
            filter(is_not_punct_char, series_name.lower()):
            return allSeries['sid']
        return None
    # end searchTvdb

# end class MetaData()
