#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: ttvdb.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform TV series data lookups
#   based on information found on the http://thetvdb.com/ website. It
#   follows the MythTV standards set for the movie data
#   lookups. e.g. the perl script "tmdb.pl" used to access themoviedb.com
#   This script uses the python module tvdb_api.py (v1.0 or higher) found at
#   http://pypi.python.org/pypi?%3Aaction=search&term=tvnamer&submit=search
#   thanks to the authors of this excellant module.
#   The tvdb_api.py module uses the full access XML api published by
#   thetvdb.com see:
#     http://thetvdb.com/wiki/index.php?title=Programmers_API
#   Users of this script are encouraged to populate thetvdb.com with TV show
#   information, posters, fan art and banners. The richer the source the more
#   valuable the script.
#   This python script was modified based on the "tvnamer.py" created by
#   "dbr/Ben" who is also
#   the author of the "tvdb_api.py" module. "tvnamer.py" is used to rename avi
#   files with series/episode information found at thetvdb.com
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Verify the command line options (display help or version and exit)
#   2) Verify that thetvdb.com has the series or series_season_ep being
#      requested exit if does not exit
#   3) Find the requested information and send to stdout if any found
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="thetvdb.com Query Engine";
__author__="R.D.Vaughan"
__version__="v1.0.2"        # Version .1 Initial development
							# Version .2 Add an option to get season and episode numbers from ep name
							# Version .3 Cleaned up the documentation and added a usage display option
							# Version .4 Added override formating of the number option (-N)
							# Version .5 Added a check that tvdb_api.py dependancies are installed
							# Version .6 Added -M Series List functionality
							# Version .7 Added -o Series name override file functionality
							# Version .8 Removed a dependancy, fixed caching for multiple users and
							#            used better method of supporting the -M option with tvdb_api
							# Version .8.1 Cleaned up documentation errors.
							# Version .8.2 Added the series name to output of meta data -D
							# Version .8.3 Patched tv_api so even episode image is fully qualified URL
							# Version .8.4 Fixed bug in -N when multiple episodes are returned from
							#              a search on episode name.
							# Version .8.5 Made option -N more flexible in finding a matching episode
							# Version .8.6 Add option -t for top rated graphics (-P,B,F) for a series
							#              Add option -l to filter graphics on the local language
							#              Add season level granularity to Poster and Banner URLs
							#              Changed the override file location to be specified on the
                            #              command line along with the option -o.
							#              Increased the amount of massaging of episode data to improve
							#              compatiblilty with data bases.
							#              Changed the default season episode number format to SxxExx
							#              Add an option (-n) to return the season numbers for a series
 							#              Added passing either a thetvdb.com SID (series identifcation
							#              number) or series name for all functions except -M list.
							#              Now ALL available episode meta data is returned. This
                            #              includes meta data that MythTv DB does not currently store.
                            #              Meta data 'Year' now derived from year in release date.
							# Version .8.7 Fixed a bug with unicode meta data handling
							# Version .8.8 Replaced the old configuration file with a true conf file
							# Version .8.9 Add option -m to better conform to mythvideo standards
							# Version .9.0 Now when a season level Banner is not found then the
							#              top rated series level graphics is returned instead. This
							#              feature had previously been available for posters only.
							#              Add runtime to episode meta data (-D). It is always the
							#              same for each episode as the information is only available
							#              from the series itself.
							#              Added the TV Series cast members as part of episode
							#              meta data. Option (-D).
							#              Added TV Series series genres as part of episode
							#              meta data. Option (-D).
							#              Resync with tvdb_api getting bug fixes and new features.
							#              Add episode data download to a specific language see
							#              -l 'es' option. If there is no data for a languages episode
							#              then any engish episode data is returned. English is default.
							#              The -M is still only in English. Waiting on tvdb_api fix.
							# Version .9.1 Bug fix stdio abort when no genre exists for TV series
							# Version .9.2 Bug fix stdio abort when no cast exists for TV series
							# Version .9.3 Changed option -N when episodes partially match each
                            #              combination of season/episode numbers are returned. This was
                            #              added to deal with episodes which have a "(1)" trailing
                            #              the episode name. An episode in more than one part.
							# Version .9.4 Option -M can now list multi-language TV Series
							# Version .9.5 "Director" metadata field returned as "None" is changed to
                            #              "Unknown".
                            #              File name parsing was changed to use multi-language capable
                            #              regex patterns
							# Version .9.6 Synced up to the 1.0 release of tvdb_api
							#              Added a tvdb_api version check and abort if not at least v1.0
							#              Changed to new tvdb_api's method of assigning the tvdb api key
							# Version .9.7 Account for TVDB increasing the number of digits in their
							#              SID number (now greater then 5 
							#              e,g, "Defying Gravity" is SID 104581)
							# Version .9.8 Added a (-S) option for requesting a thetvdb  
							#              episode screen shot
							# Version .9.9 Fixed the -S option when NO episode image exists
							# Version 1.0. Removed LF and replace with a space for all TVDB metatdata 
                            #              fields
							# Version 1.0.1 Return all graphics (series and season) in the order 
							#               highest to lowest as rated by users 
							# Version 1.0.2 Added better error messages to config file checking. Updated to 
							#				v1.0.2 tvdb_api which contained fixes for concurrent instances
							#               of ttvdb.py generated by MythVideo.

usage_txt='''
This script fetches TV series information from theTVdb.com web site. The script conforms to MythTV's
query engines for Movie Data standards.
NOTE: In the case of multiple queries the display order will always be:
    Numbers            - Used as a single option not valid as a multi-option parameter
    List               - Used as a single option not valid as a multi-option parameter
    Data               - The initial line of an episode's data starts with 'Series:'
    Poster URL(s)      - The initial line is 'poster:' then the URL(s) follow on the next line(s)
    Fan art URL(s)     - The initial line is 'fanart:' then the URL(s) follow on the next line(s)
    Banner URL(s)      - The initial line is 'banner:' then the URL(s) follow on the next line(s)
    Screenshot URL(s)  - The initial line is 'screenshot:' then the URL(s) follow on the next line(s)
NOTE: Queries with no results have no output but have a return code of "True".
NOTE: All errors starts with a '! ' and a return code of "False".

Options:
  -h, --help            show this help message and exit
  -d, --debug           Show debugging info
  -r, --raw             Dump raw data only
  -u, --usage           Display examples for executing the ttvdb script
  -v, --version         Display version and author
  -i, --interactive     Interaction mode (allows selection of a specific
                        Series)
  -c FILE, --configure=FILE
                        Use configuration settings
  -l LANGUAGE, --language=LANGUAGE
                        Select data that matches the specified language fall
                        back to english if nothing found (e.g. 'es' Español,
                        'de' Deutsch ... etc)
  -n, --num_seasons     Return the season numbers for a series
  -t, --toprated        Only return the top rated graphics for a TV Series
  -m, --mythvideo       Conform to mythvideo standards when processing -M, -P,
                        -F and -D
  -M, --list            Get matching TV Series list
  -P, --poster          Get Series Poster URL(s)
  -F, --fanart          Get Series fan art URL(s)
  -B, --backdrop        Get Series banner/backdrop URL(s)
  -S, --screenshot      Get a Season's Episode screen shot
  -D, --data            Get Series episode data
  -N, --numbers         Get Season and Episode numbers
    NOTE: There are no identifiers for URLs output if ONLY one type has been selected (e.g -B versus -BF)

Command examples:
(Return the banner graphics for a series)
> ttvdb -B "Sanctuary"
http://www.thetvdb.com/banners/seasons/80159-1.jpg
http://www.thetvdb.com/banners/seasons/80159-1-2.jpg
http://www.thetvdb.com/banners/seasons/80159-1-4.jpg
http://www.thetvdb.com/banners/seasons/80159-1-3.jpg

(Return the banner graphics for a Series specific to a season)
> ttvdb -B "SG-1" 1
http://www.thetvdb.com/banners/seasonswide/72449-1.jpg

(Return the screen shot graphic for a Series Episode)
> ttvdb -S "SG-1" 1 10
http://www.thetvdb.com/banners/episodes/72449/85759.jpg

(Return the banner graphics for a SID (series ID) specific to a season)
(SID "72449" is specific for the series "SG-1")
> ttvdb -B 72449 1
http://www.thetvdb.com/banners/seasonswide/72449-1.jpg

(Return the banner graphics for a file name)
> ttvdb -B "Stargate SG-1 - S08E03 - Lockdown"
http://images.thetvdb.com.nyud.net:8080/banners/seasonswide/72449-8.jpg

(Return the posters, banners and fan art for a series)
> ttvdb -PFB "Sanctuary"
poster:
http://www.thetvdb.com/banners/posters/80159-1.jpg
fanart:
http://www.thetvdb.com/banners/fanart/original/80159-5.jpg
http://www.thetvdb.com/banners/fanart/original/80159-1.jpg
banner:
http://www.thetvdb.com/banners/seasons/80159-1.jpg

(Return thetvdb.com's top rated poster, banner and fan art for a TV Series)
(NOTE: If there is no graphic for a type or any graphics at all then those types are not returned)
> ttvdb -tPFB "Stargate SG-1"
Poster:http://www.thetvdb.com/banners/posters/72449-1.jpg
Fanart:http://www.thetvdb.com/banners/fanart/original/72449-1.jpg
Banner:http://www.thetvdb.com/banners/graphical/185-g3.jpg
> ttvdb -tB "Night Gallery"
http://www.thetvdb.com/banners/blank/70382.jpg

(Return graphics only matching the local language for a TV series)
(In this case banner 73739-g9.jpg is not included because it does not match the language 'en')
> ttvdb -Bl en "Lost"
http://www.thetvdb.com/banners/graphical/24313-g.jpg
http://www.thetvdb.com/banners/graphical/73739-g6.jpg
...
http://www.thetvdb.com/banners/graphical/73739-g2.jpg
http://www.thetvdb.com/banners/blank/73739.jpg

(Return a season and episode numbers) Note: Any other options are ignored e.g. (-BFP ... etc))
> ttvdb -N "Eleventh Hour (US)" "H2O"
S01E10

(Return a season and episode numbers with an override of the default output format)
(WARNING: There is no editing of an override output format, two '%d' must be included)
> ttvdb -N "Eleventh Hour (US)" "H2O" "Season%dx%02d"
Season1x10

(Return a season and episode numbers using the override file to identify the series as the US version)
> ttvdb -N --configure="/home/user/.tvdb/tvdb.conf" "Eleventh Hour" "H2O"
S01E10

(Return the season numbers for a series)
> ttvdb -n --configure="/home/user/.tvdb/tvdb.conf" -n "SG-1"
0,1,2,3,4,5,6,7,8,9,10

(Return the meta data for a specific series/season/episode)
> ttvdb -D "Sanctuary" 0 1
Title:Sanctuary
Season:0
Episode:1
Subtitle:Webisode 1
Year:2007
ReleaseDate:2007-05-14
Director:Martin Wood
Plot:While tracking a young boy with dangerous powers, Dr. Helen Magnus encounters Will Zimmerman, a psychiatric resident who may have what it takes to become her new protégé.
UserRating:8.0
Writers:Damian Kindler, Martin Wood
Episode Image:http://www.thetvdb.com/banners/episodes/80159/328953.jpg
Language:en
Combined_Episodenumber:1
Id:328953
Seasonid:27498
Lastupdated:1223234120
Combined_Season:0
Cast:Amanda Tapping, Robin Dunne, Emilie Ullerup, Ryan Robbins, Christopher Heyerdahl, Dan Payne, Sarah Sands, Lorena Gale, Sheri Noel, Diana Pavlovska, Malik McCall, Cainan Wiebe, Kavan Smith, Michael Adamthwaite
Seriesid:80159
Productioncode:101
Runtime:60

(Return a list of "thetv.com series id and series name" that contain specific search word(s) )
(!! Be careful with this option as poorly defined search words can result in large lists being returned !!)
> ttvdb.py -M "night a"
74382:Love on a Saturday Night
71476:Star For A Night

'''
# Episode keys that can be used in a episode data/information search.
# All keys are currently being used.
'''
'episodenumber'
'rating'
'overview'
'dvd_episodenumber'
'dvd_discid'
'combined_episodenumber'
'epimgflag'
'id'
'seasonid'
'seasonnumber'
'writer'
'lastupdated'
'filename'
'absolute_number'
'combined_season'
'imdb_id'
'director'
'dvd_chapter'
'dvd_season'
'gueststars'
'seriesid'
'language'
'productioncode'
'firstaired'
'episodename'
'''


# System modules
import sys, os, re, locale, ConfigParser
from optparse import OptionParser

# Verify that tvdb_api.py, tvdb_ui.py and tvdb_exceptions.py are available
try:
	import ttvdb.tvdb_ui as tvdb_ui
	# thetvdb.com specific modules
	from ttvdb.tvdb_api import (tvdb_error, tvdb_shownotfound, tvdb_seasonnotfound, tvdb_episodenotfound, tvdb_episodenotfound, tvdb_attributenotfound, tvdb_userabort)
# from tvdb_api import Tvdb
	import ttvdb.tvdb_api as tvdb_api

	# verify version of tvdbapi to make sure it is at least 1.0
	if tvdb_api.__version__ < '1.0':
		print "\nYour current installed tvdb_api.py version is (%s)\n" % tvdb_api.__version__
		raise
except Exception:
	print '''
The modules tvdb_api.py (v1.0.0 or greater), tvdb_ui.py, tvdb_exceptions.py and cache.py must be
in the same directory as ttvdb.py. They should have been included with the distribution of ttvdb.py.
'''
	sys.exit(False)

# Global variables
http_find="http://www.thetvdb.com"
http_replace="http://www.thetvdb.com" #Keep replace code "just in case"

logfile="/home/doug/joblog/tvdb.log"

name_parse=[
			# foo_[s01]_[e01]
			re.compile('''^(.+?)[ \._\-]\[[Ss]([0-9]+?)\]_\[[Ee]([0-9]+?)\]?[^\\/]*$'''),
			# foo.1x09*
			re.compile('''^(.+?)[ \._\-]\[?([0-9]+)x([0-9]+)[^\\/]*$'''),
			# foo.s01.e01, foo.s01_e01
			re.compile('''^(.+?)[ \._\-][Ss]([0-9]+)[\.\- ]?[Ee]([0-9]+)[^\\/]*$'''),
			# foo.103*
			re.compile('''^(.+)[ \._\-]([0-9]{1})([0-9]{2})[\._ -][^\\/]*$'''),
			# foo.0103*
			re.compile('''^(.+)[ \._\-]([0-9]{2})([0-9]{2,3})[\._ -][^\\/]*$'''),
] # contains regex parsing filename parsing strings used to extract info from video filenames

# Episode meta data that is massaged
massage={'writer':'|','director':'|', 'overview':'&', 'gueststars':'|' }
# Keys and titles used for episode data (option '-D')
data_keys =['seasonnumber','episodenumber','episodename','firstaired','director','overview','rating','writer','filename','language' ]
data_titles=['Season:','Episode:','Subtitle:','ReleaseDate:','Director:','Plot:','UserRating:','Writers:','Episode Image:','Language:' ]
# High level dictionay keys for select graphics URL(s)
fanart_key='fanart'
banner_key='series'
poster_key='poster'
season_key='season'
# Lower level dictionay keys for select graphics URL(s)
poster_series_key='680x1000'
poster_season_key='season'
fanart_hires_key='1920x1080'
fanart_lowres_key='1280x720'
banner_series_key='graphical'
banner_season_key='seasonwide'
# Type of graphics being requested
poster_type='Poster'
fanart_type='Fanart'
banner_type='Banner'
screenshot_request = False

# Cache directory name specific to the user. This avoids permission denied error with a common cache dirs
cache_dir="/tmp/tvdb_api_%s/" % os.geteuid()

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

def debuglog(message):
	message+='\n'
	target_socket = open(logfile, "a")
	target_socket.write(message)
	target_socket.close()
	return
# end debuglog

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
			self.out.write(obj.encode(self.encoding))
		else:
			self.out.write(obj)

	def __getattr__(self, attr):
		"""Delegate everything but write to the stream"""
		return getattr(self.out, attr)
sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')

# Alternate tvdb_api's method for retrieving graphics URLs but returned as a list that preserves
# the user rating order highest rated to lowest rated
def _ttvdb_parseBanners(self, sid):
	"""Parses banners XML, from
	http://www.thetvdb.com/api/[APIKEY]/series/[SERIES ID]/banners.xml

	Banners are retrieved using t['show name]['_banners'], for example:

	>>> t = Tvdb(banners = True)
	>>> t['scrubs']['_banners'].keys()
	['fanart', 'poster', 'series', 'season']
	>>> t['scrubs']['_banners']['poster']['680x1000']['35308']['_bannerpath']
	'http://www.thetvdb.com/banners/posters/76156-2.jpg'
	>>>

	Any key starting with an underscore has been processed (not the raw
	data from the XML)

	This interface will be improved in future versions.
	Changed in this interface is that a list or URLs is created to preserve the user rating order from
	top rated to lowest rated. 
	"""

	self.log.debug('Getting season banners for %s' % (sid))
	bannersEt = self._getetsrc( self.config['url_seriesBanner'] % (sid) )
	banners = {}
	bid_order = {'fanart': [], 'poster': [], 'series': [], 'season': []}
	for cur_banner in bannersEt.findall('Banner'):
		bid = cur_banner.find('id').text
		btype = cur_banner.find('BannerType')
		btype2 = cur_banner.find('BannerType2')
		if btype is None or btype2 is None:
			continue
		btype, btype2 = btype.text, btype2.text
		if not btype in banners:
			banners[btype] = {}
		if not btype2 in banners[btype]:
			banners[btype][btype2] = {}
		if not bid in banners[btype][btype2]:
			banners[btype][btype2][bid] = {}
			if btype in bid_order.keys():
				if btype2 != u'blank':
					bid_order[btype].append([bid, btype2])

		self.log.debug("Banner: %s", bid)
		for cur_element in cur_banner.getchildren():
			tag = cur_element.tag.lower()
			value = cur_element.text
			if tag is None or value is None:
				continue
			tag, value = tag.lower(), value.lower()
			self.log.debug("Banner info: %s = %s" % (tag, value))
			banners[btype][btype2][bid][tag] = value

		for k, v in banners[btype][btype2][bid].items():
			if k.endswith("path"):
				new_key = "_%s" % (k)
				self.log.debug("Transforming %s to %s" % (k, new_key))
				new_url = self.config['url_artworkPrefix'] % (v)
				self.log.debug("New banner URL: %s" % (new_url))
				banners[btype][btype2][bid][new_key] = new_url

	graphics_in_order = {'fanart': [], 'poster': [], 'series': [], 'season': []}
	for key in bid_order.keys():
		for bid in bid_order[key]:
			graphics_in_order[key].append(banners[key][bid[1]][bid[0]])
	return graphics_in_order
tvdb_api.Tvdb.ttvdb_parseBanners = _ttvdb_parseBanners
# end _ttvdb_parseBanners()

# new Tvdb method to search for a series and return it's sid
def _series_by_sid(self, sid):
	"Lookup a series via it's sid"
	seriesid = 'sid:' + sid
	if not self.corrections.has_key(seriesid):
		self._getShowData(sid)
		self.corrections[seriesid] = sid
	return self.shows[sid]
tvdb_api.Tvdb.series_by_sid = _series_by_sid

# Search for a series by SID or Series name
def search_for_series(tvdb, sid_or_name):
	"Get series data by sid or series name of the Tv show"
	if SID == True:
		return tvdb.series_by_sid(sid_or_name)
	else:
		return tvdb[sid_or_name]
# end search_for_series

# Verify that a Series or Series and Season exists on thetvdb.com
def searchseries(t, opts, series_season_ep):
	global SID
	series_name=''
	if opts.configure != "" and override.has_key(series_season_ep[0].lower()):
		series_name=override[series_season_ep[0].lower()][0] # Override series name
	else:
		series_name=series_season_ep[0] # Leave the series name alone
	try:
		# Search for the series or series & season or series & season & episode
		if len(series_season_ep)>1:
			if len(series_season_ep)>2: # series & season & episode
				seriesfound=search_for_series(t, series_name)[ int(series_season_ep[1]) ][ int(series_season_ep[2]) ]
			else:
				seriesfound=search_for_series(t, series_name)[ int(series_season_ep[1]) ] # series & season
		else:
			seriesfound=search_for_series(t, series_name) # Series only
	except tvdb_shownotfound:
		# No such show found.
		# Use the show-name from the files name, and None as the ep name
		sys.stderr.write("! Warning: Series (%s) not found\n" % (
			series_name )
		)
		sys.exit(False)
	except (tvdb_seasonnotfound, tvdb_episodenotfound, tvdb_attributenotfound):
		# The season, episode or name wasn't found, but the show was.
		# Use the corrected show-name, but no episode name.
		if len(series_season_ep)>2:
			sys.stderr.write("! Warning: For Series (%s), season (%s) or Episode (%s) not found \n" % (
				series_name, series_season_ep[1], series_season_ep[2] )
			)
		else:
			sys.stderr.write("! Warning: For Series (%s), season (%s) not found \n" % (
				series_name, series_season_ep[1] )
			)
		sys.exit(False)
	except tvdb_error, errormsg:
		# Error communicating with thetvdb.com
		if SID == True: # Maybe the digits were a series name (e.g. 90210)
			SID = False
			return searchseries(t, opts, series_season_ep)
		sys.stderr.write(
			"! Warning: Error contacting www.thetvdb.com:\n%s\n" % (errormsg)
		)
		sys.exit(False)
	except tvdb_userabort, errormsg:
		# User aborted selection (q or ^c)
		print "\n", errormsg
		sys.exit(False)
	else:
		if opts.raw==True:
			print "="*20
			print "Raw Series Data:\n"
			if len(series_season_ep)>1:
				print t[ series_name ][ int(series_season_ep[1]) ]
			else:
				print t[ series_name ]
			print "="*20
		return(seriesfound)
# end searchseries

# Retrieve Poster or Fan Art or Banner graphics URL(s)
def get_graphics(t, opts, series_season_ep, graphics_type, single_option, language=False):
	banners='_banners'
	series_name=''
	graphics=[]
	if opts.configure != "" and override.has_key(series_season_ep[0].lower()):
		series_name=override[series_season_ep[0].lower()][0] # Override series name
	else:
		series_name=series_season_ep[0] # Leave the series name alone

	if SID == True:
		URLs = t.ttvdb_parseBanners(series_name)
	else:
		URLs = t.ttvdb_parseBanners(t._nameToSid(series_name))

	if graphics_type == fanart_type: # Series fanart graphics
		if not len(URLs[u'fanart']):
			return []
		for url in URLs[u'fanart']:
			graphics.append(url)
	elif len(series_season_ep) == 1:
		if not len(URLs[u'series']):
			return []
		if graphics_type == banner_type: # Series Banners
			for url in URLs[u'series']:
				graphics.append(url)
		else: # Series Posters
			for url in URLs[u'poster']:
				graphics.append(url)
	else:
		if not len(URLs[u'season']):
			return []
		if graphics_type == banner_type: # Season Banners
			season_banners=[]
			for url in URLs[u'season']:
				if url[u'bannertype2'] == u'seasonwide' and url[u'season'] == series_season_ep[1]:
					season_banners.append(url)
			if not len(season_banners):
				return []
			graphics = season_banners
		else: # Season Posters
			season_posters=[]
			for url in URLs[u'season']:
				if url[u'bannertype2'] == u'season' and url[u'season'] == series_season_ep[1]:
					season_posters.append(url)
			if not len(season_posters):
				return []
			graphics = season_posters

	graphicsURLs=[]
	if single_option==False:
		graphicsURLs.append(graphics_type+':')

	for URL in graphics:
		if language: # Is there a language to filter URLs on?
			if language == URL['language']:
				graphicsURLs.append((URL['_bannerpath']).replace(http_find, http_replace))
		else:
			graphicsURLs.append((URL['_bannerpath']).replace(http_find, http_replace))
	if opts.debug == True:
		print u"\nGraphics:\n", graphicsURLs

	if len(graphicsURLs) == 1 and graphicsURLs[0] == graphics_type+':':
		return [] # Due to the language filter there may not be any URLs
	return(graphicsURLs)
# end get_graphics

# Massage episode name to match those in thetvdb.com for this series
def massageEpisode_name(ep_name, series_season_ep):
	for edit in override[series_season_ep[0].lower()][1]:
		ep_name=ep_name.replace(edit[0],edit[1]) # Edit episode name for each set of strings
	return ep_name
# end massageEpisode_name

# Remove '|' and replace with commas
def change_to_commas(meta_data):
	if not meta_data: return meta_data
	meta_data = (u'|'.join([d for d in meta_data.split('| ') if d]))
	return (u', '.join([d for d in meta_data.split('|') if d]))
# end change_to_commas

# Change &amp; values to ascii equivalents
def change_amp(text):
	if not text: return text
	text = text.replace("&quot;", "'").replace("\r\n", " ")
	text = text.replace(r"\'", "'")
	return text
# end change_amp

# Prepare for includion into a DB
def make_db_ready(text):
	if not text: return text
	text = text.replace(u'\u2013', "-")
	text = text.replace(u'\u2014', "-")
	text = text.replace(u'\u2018', "'")
	text = text.replace(u'\u2019', "'")
	text = text.replace(u'\u2026', "...")
	text = text.replace(u'\u201c', '"')
	text = text.replace(u'\u201d', '"')
	text = text.encode('latin-1', 'backslashreplace')
	return text
# end make_db_ready

# Get Series Episode data by season
def Getseries_episode_data(t, opts, series_season_ep, language = None):
	global screenshot_request, http_find, http_replace

	args = len(series_season_ep)
	series_name=''
	if opts.configure != "" and override.has_key(series_season_ep[0].lower()):
		series_name=override[series_season_ep[0].lower()][0] # Override series name
	else:
		series_name=series_season_ep[0] # Leave the series name alone

	# Get Cast members
	cast_members=''
	try:
		tmp_cast = search_for_series(t, series_name)['_actors']
	except:
		cast_members=''
	if len(tmp_cast):
		cast_members=''
		for cast in tmp_cast:
			cast_members+=(cast['name']+u', ').encode('utf8')
		if cast_members != '':
			try:
				cast_members = cast_members[:-2].encode('utf8')
			except UnicodeDecodeError:
				cast_members = unicode(cast_members[:-2],'utf8')
			cast_members = change_amp(cast_members)
			cast_members = change_to_commas(cast_members)
			cast_members=cast_members.replace('\n',' ')

	# Get genre(s)
	genres=''
	try:
		genres_string = search_for_series(t, series_name)[u'genre'].encode('utf-8')
	except:
		genres_string=''
	if genres_string != None and genres_string != '':
		genres = change_amp(genres_string)
		genres = change_to_commas(genres)

	seasons=search_for_series(t, series_name).keys() # Get the seasons for this series
	for season in seasons:
		if args > 1: # If a season was specified skip other seasons
			if season != int(series_season_ep[1]):
				continue
		episodes=search_for_series(t, series_name)[season].keys() # Get the episodes for this season
		for episode in episodes: # If an episode was specified skip other episodes
			if args > 2:
				if episode != int(series_season_ep[2]):
					continue
			extra_ep_data=[]
			available_keys=search_for_series(t, series_name)[season][episode].keys()
			if screenshot_request:
				if u'filename' in available_keys:
					screenshot = search_for_series(t, series_name)[season][episode][u'filename']
					if screenshot:
						print screenshot.replace(http_find, http_replace)
					return
				else:
					return
			key_values=[]
			for values in data_keys: # Initialize an array for each possible data element for
				key_values.append('') # each episode within a season
			for key in available_keys:
				try:
					i = data_keys.index(key) # Include only specific episode data
				except ValueError:
					if search_for_series(t, series_name)[season][episode][key] != None:
						text = search_for_series(t, series_name)[season][episode][key]
						text = change_amp(text)
						text = change_to_commas(text)
						if text == 'None' and key.title() == 'Director':
							text = u"Unknown"
						try:
							extra_ep_data.append(u"%s:%s" % (key.title(), text))
						except UnicodeDecodeError:
							extra_ep_data.append(u"%s:%s" % (key.title(), unicode(text, "utf8")))
					continue
				text = search_for_series(t, series_name)[season][episode][key]

				if text == None and key.title() == 'Director':
					text = u"Unknown"
				if text == None or text == 'None':
					continue
				else:
					text = change_amp(text)
					value = change_to_commas(text)
					value = value.replace(u'\n', u' ')
				key_values[i]=value
			index = 0
			if SID == False:
				print u"Title:%s" % series_name # Ouput the full series name
			else:
				print u"Title:%s" % search_for_series(t, series_name)[u'seriesname']

			for key in data_titles:
				if key_values[index] != None:
					if data_titles[index] == u'ReleaseDate:' and len(key_values[index]) > 4:
						print u'%s%s'% (u'Year:', key_values[index][:4])
					if key_values[index] != 'None':
						print u'%s%s' % (data_titles[index], key_values[index])
				index+=1
			cast_print=False
			for extra_data in extra_ep_data:
				if extra_data[:extra_data.index(':')] == u'Gueststars':
					extra_cast = extra_data[extra_data.index(':')+1:]
					if (len(extra_cast)>128) and not extra_cast.count(','):
						continue
					if cast_members:
						extra_data=(u"Cast:%s" % cast_members)+', '+extra_cast
					else:
						extra_data=u"Cast:%s" % extra_cast
					cast_print=True
				print extra_data
			if cast_print == False:
				print u"Cast:%s" % cast_members
			if genres != '':
				print u"Genres:%s" % genres
			print u"Runtime:%s" % search_for_series(t, series_name)[u'runtime']
# end Getseries_episode_data

# Get Series Season and Episode numbers
def Getseries_episode_numbers(t, opts, series_season_ep):
	series_name=''
	ep_name=''
	if opts.configure != "" and override.has_key(series_season_ep[0].lower()):
		series_name=override[series_season_ep[0].lower()][0] # Override series name
		ep_name=series_season_ep[1]
		if len(override[series_season_ep[0].lower()][1]) != 0: # Are there search-replace strings?
			ep_name=massageEpisode_name(ep_name, series_season_ep)
	else:
		series_name=series_season_ep[0] # Leave the series name alone
		ep_name=series_season_ep[1] # Leave the episode name alone

	season_ep_num=search_for_series(t, series_name).search(ep_name)
	if len(season_ep_num) != 0:
		for episode in season_ep_num:
			if (episode['episodename'].lower()).startswith(ep_name.lower()):
				if len(episode['episodename']) > (len(ep_name)+1):
					if episode['episodename'][len(ep_name):len(ep_name)+2] != ' (':
						continue # Skip episodes the are not part of a set of (1), (2) ... etc
					print season_and_episode_num.replace('\\n', '\n') % (int(episode['seasonnumber']), int(episode['episodenumber']))
				else: # Exact match
					print season_and_episode_num.replace('\\n', '\n') % (int(episode['seasonnumber']), int(episode['episodenumber']))
# end Getseries_episode_numbers

# Set up a custom interface to get all series matching a partial series name
class returnAllSeriesUI(tvdb_ui.BaseUI):
	def __init__(self, config, log):
		self.config = config
		self.log = log

	def selectSeries(self, allSeries):
		return allSeries
# ends returnAllSeriesUI

def initialize_override_dictionary(useroptions):
	""" Change variables through a user supplied configuration file
	return False and exit the script if there are issues with the configuration file values
	"""
	if useroptions[0]=='~':
		useroptions=os.path.expanduser("~")+useroptions[1:]
	if os.path.isfile(useroptions) == False:
		sys.stderr.write(
			"! The specified user configuration file (%s) is not a file\n" % useroptions
		)
		sys.exit(False)
	massage = {}
	overrides = {}
	cfg = ConfigParser.SafeConfigParser()
	cfg.read(useroptions)

	for section in cfg.sections():
		if section == 'regex':
			# Change variables per user config file
			for option in cfg.options(section):
				name_parse.append(re.compile(cfg.get(section, option)))
			continue
		if section =='ep_name_massage':
			for option in cfg.options(section):
				tmp =cfg.get(section, option).split(',')
				if len(tmp)%2 and len(cfg.get(section, option)) != 0:
					sys.stderr.write("! For (%s) 'ep_name_massage' values must be in pairs\n" % option)
					sys.exit(False)
				tmp_array=[]
				i=0
				while i != len(tmp):
					tmp_array.append([tmp[i].replace('"',''), tmp[i+1].replace('"','')])
					i+=2
				massage[option]=tmp_array
			continue
		if section =='series_name_override':
			for option in cfg.options(section):
				overrides[option] = cfg.get(section, option)
			tvdb = tvdb_api.Tvdb(banners=False, debug = False, interactive = False, cache = cache_dir, custom_ui=returnAllSeriesUI, apikey="0BB856A59C51D607")  # thetvdb.com API key requested by MythTV
			for key in overrides.keys():
				sid = overrides[key]
				if len(sid) == 0:
					continue
				try: # Check that the SID (Series id) is numeric
					dummy = int(sid)
				except:
					sys.stdout.write("! Series (%s) Invalid SID (not numeric) [%s] in config file\n" % (key, sid))
					sys.exit(False)
				# Make sure that the series name is not empty or all blanks
				if len(key.replace(' ','')) == 0:
					sys.stdout.write("! Invalid Series name (must have some non-blank characters) [%s] in config file\n" % key)
					print parts
					sys.exit(False)

				try:
					series_name_sid=tvdb.series_by_sid(sid)
				except:
					sys.stdout.write("! Invalid Series (no matches found in thetvdb,com) (%s) sid (%s) in config file\n" % (key, sid))
					sys.exit(False)
				overrides[key]=series_name_sid[u'seriesname'].encode('utf-8')
			continue

	for key in overrides.keys():
		override[key] = [overrides[key],[]]

	for key in massage.keys():
		if override.has_key(key):
			override[key][1]=massage[key]
		else:
			override[key]=[key, massage[key]]
	return
# END initialize_override_dictionary

def main():
	parser = OptionParser(usage=u"%prog usage: ttvdb -hdruviomMPFBDS [parameters]\n <series name or 'series and season number' or 'series and season number and episode number'>\n\nFor details on using ttvdb with Mythvideo see the ttvdb wiki page at:\nhttp://www.mythtv.org/wiki/Ttvdb.py")

	parser.add_option(  "-d", "--debug", action="store_true", default=False, dest="debug",
                        help=u"Show debugging info")
	parser.add_option(  "-r", "--raw", action="store_true",default=False, dest="raw",
                        help=u"Dump raw data only")
	parser.add_option(  "-u", "--usage", action="store_true", default=False, dest="usage",
                        help=u"Display examples for executing the ttvdb script")
	parser.add_option(  "-v", "--version", action="store_true", default=False, dest="version",
                        help=u"Display version and author")
	parser.add_option(  "-i", "--interactive", action="store_true", default=False, dest="interactive",
                        help=u"Interaction mode (allows selection of a specific Series)")
	parser.add_option(  "-c", "--configure", metavar="FILE", default="", dest="configure",
                        help=u"Use configuration settings")
	parser.add_option(  "-l", "--language", metavar="LANGUAGE", default=u'en', dest="language",
                        help=u"Select data that matches the specified language fall back to english if nothing found (e.g. 'es' Español, 'de' Deutsch ... etc)")
	parser.add_option(  "-n", "--num_seasons", action="store_true", default=False, dest="num_seasons",
                        help=u"Return the season numbers for a series")
	parser.add_option(  "-t", "--toprated", action="store_true", default=False, dest="toprated",
                        help=u"Only return the top rated graphics for a TV Series")
	parser.add_option(  "-m", "--mythvideo", action="store_true", default=False, dest="mythvideo",
                        help=u"Conform to mythvideo standards when processing -M, -P, -F and -D")
	parser.add_option(  "-M", "--list", action="store_true", default=False, dest="list",
                        help=u"Get matching TV Series list")
	parser.add_option(  "-P", "--poster", action="store_true", default=False, dest="poster",
                        help=u"Get Series Poster URL(s)")
	parser.add_option(  "-F", "--fanart", action="store_true", default=False, dest="fanart",
                        help=u"Get Series fan art URL(s)")
	parser.add_option(  "-B", "--backdrop", action="store_true", default=False, dest="banner",
                        help=u"Get Series banner/backdrop URL(s)")
	parser.add_option(  "-S", "--screenshot", action="store_true", default=False, dest="screenshot",
                        help=u"Get Series episode screenshot URL")
	parser.add_option(  "-D", "--data", action="store_true", default=False, dest="data",
                        help=u"Get Series episode data")
	parser.add_option(  "-N", "--numbers", action="store_true", default=False, dest="numbers",
                        help=u"Get Season and Episode numbers")

	opts, series_season_ep = parser.parse_args()


	# Make everything unicode utf8
	for index in range(len(series_season_ep)):
		series_season_ep[index] = unicode(series_season_ep[index], 'utf8')

	if opts.debug == True:
		print "opts", opts
		print "\nargs", series_season_ep

	# Process version command line requests
	if opts.version == True:
		sys.stdout.write("\nTitle: (%s); Version: (%s); Author: (%s)\n" % (
		__title__, __version__, __author__ ))
		sys.stdout.write("This python script uses the included python module tvdb_api.py (v1.0 or higher). Also found at\n\
'http://pypi.python.org/pypi?%3Aaction=search&term=tvnamer&submit=search' thanks to the authors 'dbr/Ben' for this excellant module.\n\n\
The tvdb_api.py module uses the full access XML api published by thetvdb.com see:\n\
'http://thetvdb.com/wiki/index.php?title=Programmers_API'\n\n\
Users of this script are encouraged to populate thetvdb.com with TV show information, posters,\n\
fan art and banners. The richer the source the more valuable the script.\n\n"
		)
		sys.exit(True)

	# Process usage command line requests
	if opts.usage == True:
		sys.stdout.write(usage_txt)
		sys.exit(True)

	if len(series_season_ep) == 0:
		parser.error("! No series or series season episode supplied")
		sys.exit(False)

	# Default output format of season and episode numbers
	global season_and_episode_num, screenshot_request
	season_and_episode_num='S%02dE%02d' # Format output example "S04E12"

	if opts.numbers == False:
		if len(series_season_ep) > 1:
			if not _can_int(series_season_ep[1]):
				parser.error("! Season is not numeric")
				sys.exit(False)
		if len(series_season_ep) > 2:
			if not _can_int(series_season_ep[2]):
				parser.error("! Episode is not numeric")
				sys.exit(False)
	else:
		if len(series_season_ep) < 2:
			parser.error("! An Episode name must be included")
			sys.exit(False)
		if len(series_season_ep) == 3:
			season_and_episode_num = series_season_ep[2] # Override default output format

	if opts.screenshot:
		if len(series_season_ep) > 1:
			if not _can_int(series_season_ep[1]):
				parser.error("! Season is not numeric")
				sys.exit(False)
		if len(series_season_ep) > 2:
			if not _can_int(series_season_ep[2]):
				parser.error("! Episode is not numeric")
				sys.exit(False)
		if not len(series_season_ep) > 2:
			parser.error("! Option (-S), episode screenshot search requires Season and Episode numbers")
			sys.exit(False)
		screenshot_request = True

	if opts.debug == True:
		print series_season_ep

	if opts.debug == True:
		print "#"*20
		print "# series_season_ep array(",series_season_ep,")"

	if opts.debug == True:
		print "#"*20
		print "# Starting tvtvb"
		print "# Processing (%s) Series" % ( series_season_ep[0] )

	# List of language from http://www.thetvdb.com/api/0629B785CE550C8D/languages.xml
	# Hard-coded here as it is realtively static, and saves another HTTP request, as
	# recommended on http://thetvdb.com/wiki/index.php/API:languages.xml
	valid_languages = ["da", "fi", "nl", "de", "it", "es", "fr","pl", "hu","el","tr", "ru","he","ja","pt","zh","cs","sl", "hr","ko","en","sv","no"]

	# Validate language as specified by user
	if opts.language:
		for lang in valid_languages:
			if opts.language == lang: break
		else:
			valid_langs = ''
			for lang in valid_languages: valid_langs+= lang+', '
			valid_langs=valid_langs[:-2]
			sys.stdout.write("! Specified language(%s) must match one of the following languages supported by thetvdb.com wiki:\n (%s)\n" % (opts.language, valid_langs))
			sys.exit(False)

	# Access thetvdb.com API with banners (Posters, Fanart, banners, screenshots) data retrieval enabled
	if opts.list ==True:
		t = tvdb_api.Tvdb(banners=False, debug = opts.debug, cache = cache_dir, custom_ui=returnAllSeriesUI, language = opts.language, apikey="0BB856A59C51D607")  # thetvdb.com API key requested by MythTV)
	elif opts.interactive == True:
		t = tvdb_api.Tvdb(banners=True, debug=opts.debug, interactive=True,  select_first=False, cache=cache_dir, actors = True, language = opts.language, apikey="0BB856A59C51D607")  # thetvdb.com API key requested by MythTV)
	else:
		t = tvdb_api.Tvdb(banners=True, debug = opts.debug, cache = cache_dir, actors = True, language = opts.language, apikey="0BB856A59C51D607")  # thetvdb.com API key requested by MythTV)

	# Determine if there is a SID or a series name to search with
	global SID
	SID = False
	if _can_int(series_season_ep[0]): # if it is numeric then assume it is a series ID number
			SID = True
	else:
			SID = False

	if opts.debug == True:
		print "# ..got tvdb mirrors"
		print "# Start to process series or series_season_ep"
		print "#"*20

	global override
	override={} # Initialize series name override dictionary
	# If the user wants Series name overrides and a override file exists then create an overide dictionary
	if opts.configure != '':
		if opts.configure[0]=='~':
			opts.configure=os.path.expanduser("~")+opts.configure[1:]
		if os.path.exists(opts.configure) == 1: # Do overrides exist?
			initialize_override_dictionary(opts.configure)
		else:
			debuglog("! The specified override file (%s) does not exist" % opts.configure)
			sys.exit(False)
	if len(override) == 0:
		opts.configure = False # Turn off the override option as there is nothing to override

	# Check if a video name was passed and if so parse it for series name, season and episode numbers
	if len(series_season_ep) == 1:
		for r in name_parse:
			match = r.match(series_season_ep[0])
			if match:
				seriesname, seasno, epno = match.groups()
				#remove ._- characters from name (- removed only if next to end of line)
				seriesname = re.sub("[\._]|\-(?=$)", " ", seriesname).strip()
				series_season_ep = [seriesname, seasno, epno]
				break # Matched - to the next file!

	# Fetch a list of matching series names
	if (opts.list ==True ):
		try:
			allSeries=t._getSeries(series_season_ep[0])
		except tvdb_shownotfound:
			sys.exit(True) # No matching series
		for series_name_sid in allSeries: # list search results
			print u"%s:%s" % (series_name_sid['sid'], series_name_sid['name'])
		sys.exit(True) # The Series list option (-M) is the only option honoured when used

	# Verify that thetvdb.com has the desired series_season_ep.
	# Exit this module if series_season_ep is not found
	if opts.numbers == False and opts.num_seasons == False:
		seriesfound=searchseries(t, opts, series_season_ep)
		x=1
	else:
		x=[]
		x.append(series_season_ep[0]) # Only use series name in check
		seriesfound=searchseries(t, opts, x)

	# Return the season numbers for a series
	if opts.num_seasons == True:
		season_numbers=''
		for x in seriesfound.keys():
			season_numbers+='%d,' % x
		print season_numbers[:-1]
		sys.exit(True) # Option (-n) is the only option honoured when used

	# Dump information accessable for a Series and ONLY first season of episoded data
	if opts.debug == True:
		print "#"*20
		print "# Starting Raw keys call"
		print "Lvl #1:" # Seasons for series
		x = t[series_season_ep[0]].keys()
		print t[series_season_ep[0]].keys()
		print "#"*20
		print "Lvl #2:" # Episodes for each season
		for y in x:
			print t[series_season_ep[0]][y].keys()
		print "#"*20
		print "Lvl #3:" # Keys for each episode within the 1st season
		z = t[series_season_ep[0]][1].keys()
		for aa in z:
			print t[series_season_ep[0]][1][aa].keys()
		print "#"*20
		print "Lvl #4:" # Available data for each episode in 1st season
		for aa in z:
			codes = t[series_season_ep[0]][1][aa].keys()
			print "\n\nStart:"
			for c in codes:
				print "="*50
				print 'Key Name=('+c+'):'
				print t[series_season_ep[0]][1][aa][c]
				print "="*50
		print "#"*20
		sys.exit (True)

	if opts.numbers == True: # Fetch and output season and episode numbers
		Getseries_episode_numbers(t, opts, series_season_ep)
		sys.exit(True) # The Numbers option (-N) is the only option honoured when used

	if opts.data or screenshot_request: # Fetch and output episode data
		if opts.mythvideo:
			if len(series_season_ep) != 3:
				print u"Season and Episode numbers required."
			else:
				Getseries_episode_data(t, opts, series_season_ep, language=opts.language)
		else:
			Getseries_episode_data(t, opts, series_season_ep, language=opts.language)

	# Fetch the requested graphics URL(s)
	if opts.debug == True:
		print "#"*20
		print "# Checking if Posters, Fanart or Banners are available"
		print "#"*20

	if opts.configure != "" and override.has_key(series_season_ep[0].lower()):
		banners_keys = search_for_series(t, override[series_season_ep[0].lower()][0])['_banners'].keys()
	else:
		banners_keys = search_for_series(t, series_season_ep[0])['_banners'].keys()
	banner= False
	poster= False
	fanart= False

	for x in banners_keys: # Determine what type of graphics is available
		if x == fanart_key:
			fanart=True
		elif x== poster_key:
			poster=True
		elif x==season_key or x==banner_key:
			banner=True

	# Make sure that some graphics URL(s) (Posters, FanArt or Banners) are available
	if ( fanart!=True and poster!=True and banner!=True ):
		sys.exit(True)

	if opts.debug == True:
		print "#"*20
		print "# One or more of Posters, Fanart or Banners are available"
		print "#"*20

	# Determine if graphic URL identification output is required
	y=0
	single_option=True
	if opts.poster==True:
		y+=1
	if opts.fanart==True:
		y+=1
	if opts.banner==True:
		y+=1
	if y > 1:
		single_option=False

	# Determine if only top rated by thetvdb.com graphics has been requested:
	if opts.toprated == True:
		series_name=''
		if opts.configure != "" and override.has_key(series_season_ep[0].lower()):
			series_name=override[series_season_ep[0].lower()][0] # Override series name
		else:
			series_name=series_season_ep[0] # Leave the series name alone
		if opts.poster==True:
			if search_for_series(t, series_name)['poster'] != None:
				if single_option==True:
					print (search_for_series(t, series_name)['poster']).replace(http_find, http_replace)
				else:
					print u"Poster:%s" % (search_for_series(t, series_name)['poster']).replace(http_find, http_replace)
		if opts.fanart==True:
			if search_for_series(t, series_name)['fanart'] != None:
				if single_option==True:
					print (search_for_series(t, series_name)['fanart']).replace(http_find, http_replace)
				else:
					print u"Fanart:%s" % (search_for_series(t, series_name)['fanart']).replace(http_find, http_replace)
		if opts.banner==True:
			if search_for_series(t, series_name)['banner'] != None:
				if single_option==True:
					print (search_for_series(t, series_name)['banner']).replace(http_find, http_replace)
				else:
					print u"Banner:%s" % (search_for_series(t, series_name)['banner']).replace(http_find, http_replace)
		sys.exit(True) # Only the top rated for a TV Series is returned

	if (poster==True and opts.poster==True and opts.raw!=True): # Get posters and send to stdout
		season_poster_found = False
		if opts.mythvideo:
			if len(series_season_ep) < 2:
				print u"Season and Episode numbers required."
				sys.exit(True)
		for p in get_graphics(t, opts, series_season_ep, poster_type, single_option, opts.language):
			print p
			season_poster_found = True
		if season_poster_found == False: # If there were no season posters get the series top poster
			series_name=''
			if opts.configure != "" and override.has_key(series_season_ep[0].lower()):
				series_name=override[series_season_ep[0].lower()][0] # Override series name
			else:
				series_name=series_season_ep[0] # Leave the series name alone
			print (search_for_series(t, series_name)['poster']).replace(http_find, http_replace)

	if (fanart==True and opts.fanart==True and opts.raw!=True): # Get Fan Art and send to stdout
		for f in get_graphics(t, opts, series_season_ep, fanart_type, single_option, opts.language):
			print f

	if (banner==True and opts.banner==True and opts.raw!=True): # Get Banners and send to stdout
		season_banner_found = False
		if opts.mythvideo:
			if len(series_season_ep) < 2:
				print u"Season and Episode numbers required."
				sys.exit(True)
		for b in get_graphics(t, opts, series_season_ep, banner_type, single_option, opts.language):
			print b
			season_banner_found = True
		if not season_banner_found: # If there were no season banner get the series top banner
			series_name=''
			if opts.configure != "" and override.has_key(series_season_ep[0].lower()):
				series_name=override[series_season_ep[0].lower()][0] # Override series name
			else:
				series_name=series_season_ep[0] # Leave the series name alone
			if search_for_series(t, series_name)['banner'] != None:
				print (search_for_series(t, series_name)['banner']).replace(http_find, http_replace)

	if opts.debug == True:
		print "#"*20
		print "# Processing complete"
		print "#"*20
	sys.exit(True)
#end main

if __name__ == "__main__":
	main()
