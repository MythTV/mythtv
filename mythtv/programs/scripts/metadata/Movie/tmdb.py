#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tmdb.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform Movie data lookups
#   based on information found on the http://themoviedb.org/ website. It
#   follows the MythTV standards set for grabbers
#   http://www.mythtv.org/wiki/MythVideo_Grabber_Script_Format
#   This script uses the python module tmdb_api.py which should be included
#   with this script.
#   The tmdb_api.py module uses the full access v2.1 XML api published by
#   themoviedb.org see: http://api.themoviedb.org/2.1/
#   Users of this script are encouraged to populate themoviedb.org with Movie
#   information, posters and fan art. The richer the source the more
#   valuable the script.
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Verify the command line options (display help or version and exit)
#   2) Verify that themoviedb.org has the Movie being requested exit if does not exit
#   3) Find the requested information and send to stdout if any found
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="TheMovieDB.org";
__author__="R.D.Vaughan"
__version__="0.21"
# 0.1.0 Initial development
# 0.1.1 Alpha Release
# 0.1.2 New movie data fields now have proper key names
#       Dynamic CamelCoding of keys if they are not already in the translation list
#       Fixed and re-arranged some code for minor issues.
# 0.1.3 Fixed an abort when there is no data found for Movie and People information display
#       Added CamelCase to all People information keys
# 0.1.4 Added handling of all of the tmdb_api exceptions and information process exceptions with proper exit
#       codes (0-Script Normal exit; 1-Script Exception exit)
# 0.1.5 Corrected processing of Person images
#       Added a specific check and error when there is an empty argument passed
#       Changed the formatting of person information for AlsoKnownAs and Filmography
# 0.1.6 Stopped stderr output when any TMDB meta data search or access does not find anything.
#       This was causing issues for MythVideo.
# 0.1.7 Change over to the installed TMDB api library
# 0.1.8 Improved displayed messages on an exception abort
# 0.1.9 Added support for XML output
# 0.2.0 Make XML output the default
# Version 1.12  Convert version information to XML
#         1.13  Add test mode


__usage_examples__='''
Request tmdb.py verison number:
> ./tmdb.py -v
<grabber>
  <name>TheMovieDB.org</name>
  <author>R.D.Vaughan</author>
  <thumbnail>tmdb.png</thumbnail>
  <command>tmdb.py</command>
  <type>movie</type>
  <description>Search and metadata downloads for themoviedb.org</description>
  <version>0.XX</version>
</grabber>

Request a list of matching movie titles:
> ./tmdb.py -M "Avatar"
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <language>en</language>
    <title>Avatar</title>
    <subtitle>Creating the World of Pandora</subtitle>
    <inetref>31631</inetref>
    <imdb/>
    <userrating>8.5</userrating>
    <certifications>
      <certification locale="us" name="PG"/>
    </certifications>
    <description>The cast and crew take you on an in-depth look at the making of James Cameron's epic.</description>
    <releasedate>2009-12-01</releasedate>
...
  <item>
    <language>en</language>
    <title>Avatar 2</title>
    <inetref>37739</inetref>
    <imdb>1630029</imdb>
    <userrating>0.0</userrating>
    <description>soon :)</description>
    <releasedate>2014-04-10</releasedate>
    <lastupdated>Fri, 14 May 2010 05:41:13 GMT</lastupdated>
  </item>
</metadata>

Request movie details using a TMDB#:
> ./tmdb.py -D 19995
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Avatar</title>
    <tagline>Enter the World</tagline>
    <language>en</language>
    <description>A paraplegic former Marine finds a new life on the distant planet of Pandora. Only to find himself battling humankind alongside the planet's indigenous Na'vi race in this ambitious digital 3D sci-fi epic from Academy Award-winning Titanic director James Cameron.</description>
    <certifications>
      <certification locale="us" name="PG-13"/>
    </certifications>
    <categories>
      <category type="genre" name="Action"/>
      <category type="genre" name="Adventure"/>
      <category type="genre" name="Fantasy"/>
      <category type="genre" name="Science Fiction"/>
      <category type="genre" name="Thriller"/>
    </categories>
    <studios>
      <studio name="20th Century Fox"/>
    </studios>
...
      <image type="fanart" thumb="http://i1.themoviedb.org/backdrops/9fa/4bc95845017a3c57fe0279fa/avatar-thumb.jpg" url="http://i2.themoviedb.org/backdrops/9fa/4bc95845017a3c57fe0279fa/avatar-original.jpg" width="1920" height="1080"/>
      <image type="fanart" thumb="http://i2.themoviedb.org/backdrops/a1e/4bc9584d017a3c57fe027a1e/avatar-thumb.jpg" url="http://i2.themoviedb.org/backdrops/a1e/4bc9584d017a3c57fe027a1e/avatar-original.jpg" width="1920" height="1080"/>
      <image type="fanart" thumb="http://i1.themoviedb.org/backdrops/9fe/4bc95846017a3c57fe0279fe/avatar-thumb.jpg" url="http://i3.themoviedb.org/backdrops/9fe/4bc95846017a3c57fe0279fe/avatar-original.jpg" width="1920" height="1080"/>
    </images>
  </item>
</metadata>

Request movie details using a IMDB#:
> ./tmdb.py -D 0499549
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Avatar</title>
    <language>en</language>
    <description>A paraplegic former Marine finds a new life on the distant planet of Pandora. Only to find himself battling humankind alongside the planet's indigenous Na'vi race in this ambitious digital 3D sci-fi epic from Academy Award-winning Titanic director James Cameron.</description>
    <certifications>
      <certification locale="us" name="PG-13"/>
    </certifications>
...
width="1920" height="1080"/>
      <image type="fanart" thumb="http://i1.themoviedb.org/backdrops/9fe/4bc95846017a3c57fe0279fe/avatar-thumb.jpg" url="http://i2.themoviedb.org/backdrops/9fe/4bc95846017a3c57fe0279fe/avatar-original.jpg" width="1920" height="1080"/>
    </images>
  </item>
</metadata>

Request a list of People matching a name:
> ./tmdb.py -P "Cruise"
500:Tom Cruise
77716:Cruise Moylan

Request a Person's information using their TMDB id number:
> ./tmdb.py -I 2638
Name:Cary Grant
AlsoKnownAs:Alexander Archibald Leach|Archibald Alec Leach
Birthday:1904-01-18
Birthplace:Bristol, England
Filmography:North by Northwest|Character:Roger O. Thornhill|Id:213|Job:Actor|URL:http://www.themoviedb.org/movie/213
Filmography:Arsenic and Old Lace|Character:Mortimer Brewster|Id:212|Job:Actor|URL:http://www.themoviedb.org/movie/212
...
Filmography:The Grass Is Greener|Character:Victor Rhyall, Earl|Id:25767|Job:Actor|URL:http://www.themoviedb.org/movie/25767
Id:2638
Profile:http://images.themoviedb.org/profiles/11078/CG_profile.jpg,http://images.themoviedb.org/profiles/11075/CG_profile.jpg,http://images.themoviedb.org/profiles/10994/CG_profile.jpg,http://images.themoviedb.org/profiles/10991/CG_profile.jpg,http://images.themoviedb.org/profiles/10988/CG_profile.jpg,http://images.themoviedb.org/profiles/10985/CG_profile.jpg,http://images.themoviedb.org/profiles/4414/Cary_Grant_profile.jpg
Original:http://images.themoviedb.org/profiles/11078/CG.jpg,http://images.themoviedb.org/profiles/11075/CG.jpg,http://images.themoviedb.org/profiles/10994/CG.jpg,http://images.themoviedb.org/profiles/10991/CG.jpg,http://images.themoviedb.org/profiles/10988/CG.jpg,http://images.themoviedb.org/profiles/10985/CG.jpg,http://images.themoviedb.org/profiles/4414/Cary_Grant.jpg
Thumb:http://images.themoviedb.org/profiles/11078/CG_thumb.jpg,http://images.themoviedb.org/profiles/11075/CG_thumb.jpg,http://images.themoviedb.org/profiles/10994/CG_thumb.jpg,http://images.themoviedb.org/profiles/10991/CG_thumb.jpg,http://images.themoviedb.org/profiles/10988/CG_thumb.jpg,http://images.themoviedb.org/profiles/10985/CG_thumb.jpg,http://images.themoviedb.org/profiles/4414/Cary_Grant_thumb.jpg
KnownMovies:25
Popularity:2
URL:http://www.themoviedb.org/person/2638

Request Movie details using a Hash value:
> ./tmdb.py -H "00277ff46533b155"
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <title>Underworld</title>
    <language>en</language>
    <description>Selene, a beautiful vampire warrior, is entrenched in a war between the vampire and Lycan races. Although she is aligned with the vampires, she falls in love with Michael, a recently turned Lycan whose blood may lead to the end of the war, raging for centuries.</description>
    <categories>
      <category type="genre" name="Action"/>
      <category type="genre" name="Fantasy"/>
      <category type="genre" name="Horror"/>
      <category type="genre" name="Science Fiction"/>
    </categories>
...
      <image type="fanart" thumb="http://i1.themoviedb.org/backdrops/db0/4bc90605017a3c57fe001db0/underworld-thumb.jpg" url="http://i1.themoviedb.org/backdrops/db0/4bc90605017a3c57fe001db0/underworld-original.jpg" width="1920" height="1080"/>
    </images>
  </item>
</metadata>
'''

import sys, os
from optparse import OptionParser
import re
from string import capitalize


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
sys.stdout = OutStreamEncoder(sys.stdout)
sys.stderr = OutStreamEncoder(sys.stderr)

# Verify that the tmdb_api modules are installed and accessable


try:
    import MythTV.tmdb.tmdb_api as tmdb_api
    from MythTV.tmdb.tmdb_exceptions import (TmdBaseError, TmdHttpError, TmdXmlError, TmdbUiAbort, TmdbMovieOrPersonNotFound,)
except Exception, e:
    sys.stderr.write('''
The subdirectory "tmdb" containing the modules tmdb_api.py (v0.1.3 or greater), tmdb_ui.py,
tmdb_exceptions.py must have been installed with the MythTV python bindings.
Error:(%s)
''' %  e)
    sys.exit(1)

if tmdb_api.__version__ < '0.1.3':
    sys.stderr.write("\n! Error: Your current installed tmdb_api.py version is (%s)\nYou must at least have version (0.1.3) or higher.\n" % tmdb_api.__version__)
    sys.exit(1)


try:
    from StringIO import StringIO
    from lxml import etree as etree
except Exception, e:
    sys.stderr.write(u'\n! Error - Importing the "lxml" and "StringIO" python libraries failed on error(%s)\n' % e)
    sys.exit(1)

# Check that the lxml library is current enough
# From the lxml documents it states: (http://codespeak.net/lxml/installation.html)
# "If you want to use XPath, do not use libxml2 2.6.27. We recommend libxml2 2.7.2 or later"
# Testing was performed with the Ubuntu 9.10 "python-lxml" version "2.1.5-1ubuntu2" repository package
version = ''
for digit in etree.LIBXML_VERSION:
    version+=str(digit)+'.'
version = version[:-1]
if version < '2.7.2':
    sys.stderr.write(u'''
! Error - The installed version of the "lxml" python library "libxml" version is too old.
          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
''' % version)
    sys.exit(1)


class moviedbQueries():
    '''Methods that query themoviedb.org for metadata and outputs the results to stdout any errors are output
    to stderr.
    '''
    def __init__(self,
                apikey,
                mythtv = False,
                interactive = False,
                select_first = False,
                debug = False,
                custom_ui = None,
                language = None,
                search_all_languages = False, ###CHANGE - Needs to be added
                ):
        """apikey (str/unicode):
            Specify the themoviedb.org API key. Applications need their own key.
            See http://api.themoviedb.org/2.1/ to get your own API key

        mythtv (True/False):
            When True, the movie metadata is being returned has the key and values massaged to match MythTV
            When False, the movie metadata is being returned matches what TMDB returned

        interactive (True/False):
            When True, uses built-in console UI is used to select the correct show.
            When False, the first search result is used.

        select_first (True/False):
            Automatically selects the first series search result (rather
            than showing the user a list of more than one series).
            Is overridden by interactive = False, or specifying a custom_ui

        debug (True/False):
             shows verbose debugging information

        custom_ui (tvdb_ui.BaseUI subclass):
            A callable subclass of tvdb_ui.BaseUI (overrides interactive option)

        language (2 character language abbreviation):
            The language of the returned data. Is also the language search
            uses. Default is "en" (English). For full list, run..

            >>> MovieDb().config['valid_languages'] #doctest: +ELLIPSIS
            ['da', 'fi', 'nl', ...]

        search_all_languages (True/False):
            By default, TMDB will only search in the language specified using
            the language option. When this is True, it will search for the
            show in any language

        """
        self.config = {}

        self.config['apikey'] = apikey
        self.config['moviedb'] = tmdb_api.MovieDb(apikey, mythtv = mythtv,
                interactive = interactive,
                select_first = select_first,
                debug = debug,
                custom_ui = custom_ui,
                language = language,
                search_all_languages = search_all_languages,)
        self.mythtvgrabber = [u'Title', u'Subtitle', u'Year', u'ReleaseDate', u'InetRef', u'URL', u'Director', u'Plot', u'UserRating', u'MovieRating', u'Runtime', u'Season', u'Episode', u'Coverart', u'Fanart', u'Banner', u'Screenshot', u'Cast', u'Genres', u'Countries', u'ScreenPlay', u'Studios', u'Producer', u'ProductionDesign', u'DirectorOfPhotography', u'OriginalMusicComposer', u'Story', u'CostumeDesign', u'Editor', u'Type', u'Casting', u'AssociateProducer', u'Popularity', u'Budget', u'Imdb', u'ArtDirection', u'MovieRating']
        self.error_messages = {'TmdHttpError': u"! Error: A connection error to themoviedb.org was raised (%s)\n", 'TmdXmlError': u"! Error: Invalid XML was received from themoviedb.org (%s)\n", 'TmdBaseError': u"! Error: A user interface error was raised (%s)\n", 'TmdbUiAbort': u"! Error: A user interface input error was raised (%s)\n", }
    # end __init__()

    def movieSearch(self, title):
        '''Search for movies that match the title and output their "tmdb#:Title" to stdout
        '''
        title = title.replace("-"," ")
        try:
            data = self.config['moviedb'].searchTitle(title)
        except TmdbMovieOrPersonNotFound, msg:
            #sys.stderr.write(u"%s\n" % msg)
            return
        except TmdHttpError, msg:
            sys.stderr.write(self.error_messages['TmdHttpError'] % msg)
            sys.exit(1)
        except TmdXmlError, msg:
            sys.stderr.write(self.error_messages['TmdXmlError'] % msg)
            sys.exit(1)
        except TmdBaseError, msg:
            sys.stderr.write(self.error_messages['TmdBaseError'] % msg)
            sys.exit(1)
        except TmdbUiAbort, msg:
            sys.stderr.write(self.error_messages['TmdbUiAbort'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Title search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        if data != None:
            for match in data:
                if not match.has_key('released'):
                    name = match['name']
                elif len(match['released']) > 3:
                    name = u"%s (%s)" % (match['name'], match['released'][:4])
                else:
                    name = match['name']
                sys.stdout.write( u'%s:%s\n' % (match[u'id'], name))
    # end movieSearch()

    def peopleSearch(self, persons_name):
        '''Search for People that match the name and output their "tmdb#:Name" to stdout
        '''
        try:
            for match in self.config['moviedb'].searchPeople(persons_name):
                sys.stdout.write( u'%s:%s\n' % (match[u'id'], match['name']))
        except TmdbMovieOrPersonNotFound, msg:
            #sys.stderr.write(u"%s\n" % msg)
            return
        except TmdHttpError, msg:
            sys.stderr.write(self.error_messages['TmdHttpError'] % msg)
            sys.exit(1)
        except TmdXmlError, msg:
            sys.stderr.write(self.error_messages['TmdXmlError'] % msg)
            sys.exit(1)
        except TmdBaseError, msg:
            sys.stderr.write(self.error_messages['TmdBaseError'] % msg)
            sys.exit(1)
        except TmdbUiAbort, msg:
            sys.stderr.write(self.error_messages['TmdbUiAbort'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a People search for (%s)\nError(%s)\n" % (persons_name, e))
            sys.exit(1)
    # end moviePeople()

    def camelcase(self, value):
        '''Make a string CamelCase
        '''
        if not value.strip(u'ABCEDFGHIJKLMNOPQRSTUVWXYZ'):
            return value
        if value == u'url':
            return u'URL'
        return u"".join([capitalize(w) for w in re.split(re.compile(u"[\W_]*"), value)])
    # end camelcase()

    def displayMovieData(self, data):
        '''Display movie data to stdout
        '''
        if data == None:
            return
        data_keys = data.keys()
        data_keys_org = data.keys()
        for index in range(len(data_keys)):
            data_keys[index] = data_keys[index].replace(u' ',u'').lower()

        for key in self.mythtvgrabber:
            if  key.lower() in data_keys:
                sys.stdout.write(u"%s:%s\n" % (key, data[data_keys_org[data_keys.index(key.lower())]]))

        mythtvgrabber = []
        for item in self.mythtvgrabber:
            mythtvgrabber.append(item.lower())
        for key in data_keys:
            if not key in mythtvgrabber:
                sys.stdout.write(u"%s:%s\n" % (self.camelcase(data_keys_org[data_keys.index(key)]), data[data_keys_org[data_keys.index(key)]]))
    # end displayMovieData(()


    def movieData(self, tmdb_id):
        '''Get Movie data by IMDB or TMDB number and display "key:value" pairs to stdout
        '''
        try:
            if len(tmdb_id) == 7:
                self.displayMovieData(self.config['moviedb'].searchIMDB(tmdb_id))
            else:
                self.displayMovieData(self.config['moviedb'].searchTMDB(tmdb_id))
        except TmdbMovieOrPersonNotFound, msg:
            #sys.stderr.write(u"%s\n" % msg)
            return
        except TmdHttpError, msg:
            sys.stderr.write(self.error_messages['TmdHttpError'] % msg)
            sys.exit(1)
        except TmdXmlError, msg:
            sys.stderr.write(self.error_messages['TmdXmlError'] % msg)
            sys.exit(1)
        except TmdBaseError, msg:
            sys.stderr.write(self.error_messages['TmdBaseError'] % msg)
            sys.exit(1)
        except TmdbUiAbort, msg:
            sys.stderr.write(self.error_messages['TmdbUiAbort'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Movie (%s) information display\nError(%s)\n" % (tmdb_id, e))
            sys.exit(1)
    # end movieData()

    def peopleData(self, person_id):
        '''Get People data by TMDB people id number and display "key:value" pairs to stdout
        '''
        try:
            data = self.config['moviedb'].personInfo(person_id)
        except TmdbMovieOrPersonNotFound, msg:
            #sys.stderr.write(u"%s\n" % msg)
            return
        except TmdHttpError, msg:
            sys.stderr.write(self.error_messages['TmdHttpError'] % msg)
            sys.exit(1)
        except TmdXmlError, msg:
            sys.stderr.write(self.error_messages['TmdXmlError'] % msg)
            sys.exit(1)
        except TmdBaseError, msg:
            sys.stderr.write(self.error_messages['TmdBaseError'] % msg)
            sys.exit(1)
        except TmdbUiAbort, msg:
            sys.stderr.write(self.error_messages['TmdbUiAbort'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Person (%s) information display\n" % (person_id, e))
            sys.exit(1)

        if data == None:
            return
        sys.stdout.write(u'%s:%s\n' % (u'Name', data[u'name']))
        keys = sorted(data.keys())
        images = {}
        for key in keys:
            if key == u'name':
                continue
            if key in ['also_known_as', 'filmography', 'images' ]:
                if key == 'images':
                    images = {}
                    for k in data[key].keys():
                        images[k] = u''
                if key == 'also_known_as':
                    alias = u''
                    for k in data[key]:
                        alias+=u'%s|' % k.replace(u'|', u' ')
                    if alias:
                       sys.stdout.write(u'%s:%s\n' % (self.camelcase(key), alias[:-1]))
                    continue
                for k in data[key]:
                    if key == 'filmography':
                        kys = sorted(k.keys())
                        filmography = u''
                        for c in kys:
                            if c == u'name':
                                continue
                            filmography+=u'%s:%s|' % (self.camelcase(c.replace(u'|', u' ')), k[c].replace(u'|', u' '))
                        if filmography:
                            sys.stdout.write(u'%s:%s|%s\n' % (self.camelcase(key), k[u'name'].replace(u'|', u' '), filmography[:-1]))
                    elif key == 'images':
                        sys.stdout.write(u'%s:%s\n' % (self.camelcase(k), data[key][k]))
            else:
                sys.stdout.write(u'%s:%s\n' % (self.camelcase(key), data[key]))
    # end peopleData()

    def hashData(self, hash_value):
        '''Get Movie data by Hash value and display "key:value" pairs to stdout
        '''
        try:
            self.displayMovieData(self.config['moviedb'].searchHash(hash_value))
        except TmdbMovieOrPersonNotFound, msg:
            #sys.stderr.write(u"%s\n" % msg)
            return
        except TmdHttpError, msg:
            sys.stderr.write(self.error_messages['TmdHttpError'] % msg)
            sys.exit(1)
        except TmdXmlError, msg:
            sys.stderr.write(self.error_messages['TmdXmlError'] % msg)
            sys.exit(1)
        except TmdBaseError, msg:
            sys.stderr.write(self.error_messages['TmdBaseError'] % msg)
            sys.exit(1)
        except TmdbUiAbort, msg:
            sys.stderr.write(self.error_messages['TmdbUiAbort'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Hash value Movie information display for (%s)\nError(%s)\n" % (hash_value, e))
            sys.exit(1)
    # end hashData()

# end Class moviedbQueries()


def main():
    """Gets movie details using an IMDB# and a TMDB# OR get People information using a name
    """
    # themoviedb.org api key given by Travis Bell for Mythtv
    apikey = "c27cb71cff5bd76e1a7a009380562c62"

    parser = OptionParser(usage=u"%prog usage: tmdb -hdruviomMPFBDS [parameters]\n <series name or 'series and season number' or 'series and season number and episode number'>\n\nFor details on using tmdb with Mythvideo see the tmdb wiki page at:\nhttp://www.mythtv.org/wiki/tmdb.py")

    parser.add_option(  "-d", "--debug", action="store_true", default=False, dest="debug",
                        help=u"Show debugging info")
    parser.add_option(  "-r", "--raw", action="store_true",default=False, dest="raw",
                        help=u"Dump raw data only")
    parser.add_option(  "-u", "--usage", action="store_true", default=False, dest="usage",
                        help=u"Display examples for executing the tmdb script")
    parser.add_option(  "-v", "--version", action="store_true", default=False, dest="version",
                        help=u"Display version and author")
    parser.add_option(  "-i", "--interactive", action="store_true", default=False, dest="interactive",
                        help=u"Interaction mode (allows selection of a specific Movie or Person)")
    parser.add_option(  "-l", "--language", metavar="LANGUAGE", default=u'en', dest="language",
                        help=u"Select data that matches the specified language fall back to english if nothing found (e.g. 'es' Español, 'de' Deutsch ... etc)")
    parser.add_option(  "-a", "--area", metavar="COUNTRY", default=False, dest="country", 
			help=u"Select certificate data that matches the specified country (e.g. 'de' Germany, 'gb' UK ... etc)")
    parser.add_option(  "-M", "--movielist", action="store_true", default=False, dest="movielist",
                        help=u"Get matching Movie list")
    parser.add_option(  "-D", "--moviedata", action="store_true", default=False, dest="moviedata",
                        help=u"Get Movie metadata including graphic URLs")
    parser.add_option(  "-H", "--moviehash", action="store_true", default=False, dest="moviehash",
                        help=u"Get Movie metadata including graphic URLs using a Hash value.\nSee: http://api.themoviedb.org/2.1/methods/Hash.getInfo")
    parser.add_option(  "-P", "--peoplelist", action="store_true", default=False, dest="peoplelist",
                        help=u"Get matching People list")
    parser.add_option(  "-I", "--peopleinfo", action="store_true", default=False, dest="peopleinfo",
                        help=u"Get A Person's metadata including graphic URLs")
    parser.add_option(  "-t", action="store_true", default=False, dest="test",
                        help=u"Test for the availability of runtime dependencies")

    opts, args = parser.parse_args()

    # Test mode, if we've made it here, everything is ok
    if opts.test:
        print "Everything appears to be in order"
        sys.exit(0)

    # Make all command line arguments unicode utf8
    for index in range(len(args)):
        args[index] = unicode(args[index], 'utf8')

    if opts.debug:
        sys.stdout.write("\nopts: %s\n" % opts)
        sys.stdout.write("\nargs: %s\n\n" % args)

    # Process version command line requests
    if opts.version:
        version = etree.XML(u'<grabber></grabber>')
        etree.SubElement(version, "name").text = __title__
        etree.SubElement(version, "author").text = __author__
        etree.SubElement(version, "thumbnail").text = 'tmdb.png'
        etree.SubElement(version, "command").text = 'tmdb.py'
        etree.SubElement(version, "type").text = 'movie'
        etree.SubElement(version, "description").text = 'Search and metadata downloads for themoviedb.org'
        etree.SubElement(version, "version").text = __version__
        sys.stdout.write(etree.tostring(version, encoding='UTF-8', pretty_print=True))
        sys.exit(0)

    # Process usage command line requests
    if opts.usage:
        sys.stdout.write(__usage_examples__)
        sys.exit(0)

    if not len(args) == 1:
        sys.stderr.write("! Error: There must be one value for any option. Your options are (%s)\n" % (args))
        sys.exit(1)

    if args[0] == u'':
        sys.stderr.write("! Error: There must be a non-empty argument, yours is empty.\n")
        sys.exit(1)


    passedLanguage = opts.language
    if (opts.language and opts.country):
        passedLanguage = opts.language + "-" + opts.country
    
    Queries = moviedbQueries(apikey,
                mythtv = True,
                interactive = opts.interactive,
                select_first = False,
                debug = opts.debug,
                custom_ui = None,
                language = passedLanguage,
                search_all_languages = False,)

    # Display in XML format
    # See: http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
    Queries.config['moviedb'].xml = True

    # Process requested option
    if opts.movielist:                  # Movie Search -M
       Queries.movieSearch(args[0])
    elif opts.moviedata:                # Movie metadata -D
       Queries.movieData(args[0])
    elif opts.peoplelist:               # People Search -P
       Queries.peopleSearch(args[0])
    elif opts.peopleinfo:               # Person metadata -I
       Queries.peopleData(args[0])
    elif opts.moviehash:                # Movie metadata using a hash value -H
       Queries.hashData(args[0])

    sys.exit(0)
# end main()

if __name__ == '__main__':
    main()
