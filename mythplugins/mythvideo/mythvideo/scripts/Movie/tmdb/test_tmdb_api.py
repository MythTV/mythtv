#!/usr/bin/python
# -*- coding: UTF-8 -*-
# test_tmdb.py - Exercise the tmdb_api.py routines
#
#
import sys, os, time, string, glob, re, subprocess
import optparse, logging, traceback, platform
import datetime
from socket import gethostname, gethostbyname

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


import tmdb_api as tmdb_api
from tmdb_exceptions import (TmdBaseError, TmdHttpError, TmdXmlError, TmdbUiAbort, TmdbMovieOrPersonNotFound,)

# themoviedb.org api key given by Travis Bell for Mythtv
api_key = "c27cb71cff5bd76e1a7a009380562c62"
tmdb = tmdb_api.MovieDb(api_key, mythtv=True)

error_messages = {'TmdHttpError': u"! Error: A connection error to themoviedb.org was raised (%s)\n", 'TmdXmlError': u"! Error: Invalid XML was received from the.moviedb.org (%s)\n", 'TmdBaseError': u"! Error: A user interface error was raised (%s)\n", 'TmdbUiAbort': u"! Error: A user interface input error was raised (%s)\n", }

print
print "========================================================================================================"
print "TMDB search by title returns summary information for any matching movies. Includes summary data."
print "Search title is 'Avatar'"
print "--------------------------------------------------------------------------------------------------------"
try:
	for match in tmdb.searchTitle('Avatar', lang=u'en'):
		print u'%s:%s' % (u'name', match[u'name'])
		keys = sorted(match.keys())
		for key in keys:
		    if key == u'name':
		        continue
		    print u'    %s:%s' % (key, match[key])
		print
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a Title search\n")
    sys.exit(1)

print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB search by title and output IMDB#:Movie Title"
print "This emulates the current tmdb.pl -M option using the title 'Avatar'"
print "--------------------------------------------------------------------------------------------------------"
try:
    for match in tmdb.searchTitle('Avatar', lang=u'en'):
        if match.has_key(u'imdb_id'):
            print u'%s:%s' % (match[u'imdb_id'][2:], match[u'name'])
        else:
            print u'Movie (%s) does not have an IMDB#' % match[u'name']
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a Title search\n")
    sys.exit(1)

print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB search by title and output TMDB#:Movie Title"
print "This emulates the a future tmdb.pl -M option using the title 'Avatar'"
print "--------------------------------------------------------------------------------------------------------"
try:
    for match in tmdb.searchTitle('Avatar', lang=u'en'):
        print u'%s:%s' % (match[u'id'], match[u'name'])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB search\n")
    sys.exit(1)

print "========================================================================================================"
print

print
print "========================================================================================================"
print "Test that all data used by a MythTV grabber that is applicable to a movie is present"
print "This emulates the tmdb.pl -D option using TMDB '187'"
print "--------------------------------------------------------------------------------------------------------"
mythtvgrabber = [u'Title', u'Subtitle', u'Year', u'ReleaseDate', u'InetRef', u'URL', u'Director', u'Plot', u'UserRating', u'MovieRating', u'Runtime', u'Season', u'Episode', u'Coverart', u'Fanart', u'Banner', u'Screenshot', u'Cast', u'Genres', u'Countries']

data = tmdb.searchTMDB(u'187', lang=u'en')
try:
    for key in mythtvgrabber:
        if data.has_key(key.lower()):
            print u"%s:%s" % (key, data[key.lower()])
    print "--------------------------------------------------------------------------------------------------------"
    print "Data that is included but not used by a MythTV grabbers:"
    print "--------------------------------------------------------------------------------------------------------"
    for key in data.keys():
        for item in mythtvgrabber:
            if item.lower() == key.lower():
                break
        else:
            print u"%s:%s" % (key, data[key.lower()])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a Movie inforamtion display\n")
    sys.exit(1)

print "========================================================================================================"
print

print
print "========================================================================================================"
print "Movie details using an IMDB number search translated into keys and data types specific to MythTV"
print "Search IMDB# is '0401792'"
print "--------------------------------------------------------------------------------------------------------"
try:
    data = tmdb.searchIMDB(u'0401792', lang=u'en')
    print u'%s:%s' % (u'name', data[u'title'])
    keys = sorted(data.keys())
    for key in keys:
        if key == u'title':
            continue
        print u'%s:%s' % (key, data[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a IMDB Movie information display\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "Movie details using an TMDB number search translated into keys and data types specific to MythTV"
print "Search TMDB# is '19995'"
print "--------------------------------------------------------------------------------------------------------"
try:
    data = tmdb.searchTMDB(u'19995', lang=u'en')
    print u'%s:%s' % (u'name', data[u'title'])
    keys = sorted(data.keys())
    for key in keys:
        if key == u'title':
            continue
        print u'%s:%s' % (key, data[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB search\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "Raw data and keys from TMDB with no translation into MythTV keys or data massaging"
print "--------------------------------------------------------------------------------------------------------"
try:
    tmdbraw = tmdb_api.MovieDb(api_key, mythtv=False)
    print "Using IMDB number '0499549':"
    data = tmdbraw.searchIMDB(u'0499549', lang=u'en')
    print u'%s:%s' % (u'name', data[u'name'])
    keys = sorted(data.keys())
    for key in keys:
        if key == u'name':
            continue
        print u'%s:%s' % (key, data[key])
    print
    print "Using TMDB number '19995':"
    data = tmdbraw.searchTMDB(u'19995', lang=u'en')
    print u'%s:%s' % (u'name', data[u'name'])
    keys = sorted(data.keys())
    for key in keys:
        if key == u'name':
            continue
        print u'%s:%s' % (key, data[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a Movie raw data display\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "Image search api - returns all URL images available in all sizes for coverart and fanart"
print "Should return all image URLs for TMDB# '187'"
print "--------------------------------------------------------------------------------------------------------"
try:
    data = tmdb.searchImage(u'187', lang=u'en')
    if len(data):
        for key in data.keys():
            print u'%s:%s' % (key, data[key])
            print
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a Image search\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "Image search api with a filter - returns URL images specific to a coverart/fanart and a size"
print "Should return only poster thumb image URLs for TMDB# '187'"
print "--------------------------------------------------------------------------------------------------------"
try:
    print tmdb.searchImage(u'187', lang=u'en', filterout=u'poster_thumb')
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a Image search with filter\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "Try to get images from a TMDB that does not exist"
print "Should return an empty dictionary"
print "--------------------------------------------------------------------------------------------------------"
try:
    data = tmdb.searchImage(u'0', lang=u'en', filterout=False)
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB search for non-existing Movie\n")
    sys.exit(1)
print "========================================================================================================"
#print

print
print "========================================================================================================"
print "People search: Find all People in TMDB with a specific name. "
print "Search in 'Cruise'. Return all matching people and their details"
print "--------------------------------------------------------------------------------------------------------"
try:
    for people in tmdb.searchPeople(u'Cruise', lang=u'en'):
        print u'%s:%s' % (u'name', people[u'name'])
        keys = sorted(people.keys())
        for key in keys:
            if key == u'name':
                continue
            print u'    %s:%s' % (key, people[key])
        print
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a People search\n")
    sys.exit(1)
print "========================================================================================================"
print


print
print "========================================================================================================"
print "Person info: Select by a TMDB Person id and return all the person attributes"
print "Search in 'Tom Cruise' using person id '500'"
print "--------------------------------------------------------------------------------------------------------"
try:
    data = tmdb.personInfo('500', lang=u'en')
    print u'%s:%s' % (u'name', data[u'name'])
    keys = sorted(data.keys())
    for key in keys:
        if key == u'name':
            continue
        if key in ['also_known_as', 'filmography', 'images' ]:
            print u'%s:' % (key)
            for k in data[key]:
                if key != 'filmography':
                    print u'    %s' % (k)
                else:
                    kys = sorted(k.keys())
                    print u'    %s:%s' % (u'name', k[u'name'])
                    for c in kys:
                        if c == u'name':
                            continue
                        print u'        %s:%s' % (c, k[c])
        else:
            print u'%s:%s' % (key, data[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a Person information display\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "Movie Hash Search: Find and display a Movie by the Hash '00277ff46533b155' for the Movie 'Willow'."
print "--------------------------------------------------------------------------------------------------------"
try:
    data = tmdb.searchHash('00277ff46533b155')
    print u'%s:%s' % (u'name', data[u'title'])
    keys = sorted(data.keys())
    for key in keys:
        if key == u'title':
            continue
        print u'%s:%s' % (key, data[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a Hash search\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB select first search item automatically"
print "Search for 'Avatar'"
print "--------------------------------------------------------------------------------------------------------"
try:
    tmdb = tmdb_api.MovieDb(api_key, mythtv=True, interactive = True, select_first = True)
    for match in tmdb.searchTitle('Avatar'):
        print u'%s:%s' % (u'name', match[u'name'])
        keys = sorted(match.keys())
        for key in keys:
            if key == u'name':
                continue
            print u'    %s:%s' % (key, match[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB select first search item automatically\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB Automatic return of search by title when only one matching movie"
print "Search for 'Blue Thunder'"
print "--------------------------------------------------------------------------------------------------------"
try:
    tmdb = tmdb_api.MovieDb(api_key, mythtv=True, interactive = True,)
    for match in tmdb.searchTitle('Blue Thunder'):
        print u'%s:%s' % (u'name', match[u'name'])
        keys = sorted(match.keys())
        for key in keys:
            if key == u'name':
                continue
            print u'    %s:%s' % (key, match[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB Automatic return of search by title when only one matching movie\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB Interactive search by with a movie title that is not on themoviesdb.com"
print "Search for 'ayhlkajgd'"
print "--------------------------------------------------------------------------------------------------------"
tmdb = tmdb_api.MovieDb(api_key, mythtv=True, interactive = True,)
try:
    data = tmdb.searchTitle('ayhlkajgd')
    if len(data):
        for match in data:
            print u'%s:%s' % (u'name', match[u'name'])
            keys = sorted(match.keys())
            for key in keys:
                if key == u'name':
                    continue
                print u'    %s:%s' % (key, match[key])
    else:
        print "No Person matches found for name 'ayhlkajgd'"
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB Interactive search by with a movie title that is not on themoviesdb.com\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB Interactive search by title and display of Movie Titles"
print "Search for 'Avatar'"
print "--------------------------------------------------------------------------------------------------------"
tmdb = tmdb_api.MovieDb(api_key, mythtv=True, interactive = True,)
try:
    for match in tmdb.searchTitle('Avatar'):
        print u'%s:%s' % (u'name', match[u'name'])
        keys = sorted(match.keys())
        for key in keys:
            if key == u'name':
                continue
            print u'    %s:%s' % (key, match[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB Interactive search by title and display of Movie Titles\n")
    sys.exit(1)
print "========================================================================================================"
print


print
print "========================================================================================================"
print "TMDB select first search item automatically"
print "Search for the person 'Tom Cruise'"
print "--------------------------------------------------------------------------------------------------------"
tmdb = tmdb_api.MovieDb(api_key, mythtv=True, interactive = True, select_first = True)
try:
    for match in tmdb.searchPeople('Tom Cruise'):
        print u'%s:%s' % (u'name', match[u'name'])
        keys = sorted(match.keys())
        for key in keys:
            if key == u'name':
                continue
            print u'    %s:%s' % (key, match[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB select first search item automatically\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB Automatic return of search by name when only one matching person"
print "Search for 'Megan Fox'"
print "--------------------------------------------------------------------------------------------------------"
#interactive = False,
#select_first = False,
tmdb = tmdb_api.MovieDb(api_key, mythtv=True, interactive = True,)
try:
    for match in tmdb.searchPeople('Megan Fox'):
        print u'%s:%s' % (u'name', match[u'name'])
        keys = sorted(match.keys())
        for key in keys:
            if key == u'name':
                continue
            print u'    %s:%s' % (key, match[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB Automatic return of search by name when only one matching person\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB Interactive search by with a person name that is not on themoviesdb.com"
print "Search for 'ayhlkajgd'"
print "--------------------------------------------------------------------------------------------------------"
tmdb = tmdb_api.MovieDb(api_key, mythtv=True, interactive = True,)
try:
    data = tmdb.searchPeople('ayhlkajgd')
    if len(data):
        for match in data:
            print u'%s:%s' % (u'name', match[u'name'])
            keys = sorted(match.keys())
            for key in keys:
                if key == u'name':
                    continue
                print u'    %s:%s' % (key, match[key])
    else:
        print "No Movie matches found for title 'ayhlkajgd'"
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB Interactive search by with a person name that is not on themoviesdb.com\n")
    sys.exit(1)
print "========================================================================================================"
print

print
print "========================================================================================================"
print "TMDB Interactive search by name and display of People Names"
print "Search for 'Fox'"
print "--------------------------------------------------------------------------------------------------------"
#interactive = False,
#select_first = False,
tmdb = tmdb_api.MovieDb(api_key, mythtv=True, interactive = True,)
try:
    for match in tmdb.searchPeople('Fox'):
        print u'%s:%s' % (u'name', match[u'name'])
        keys = sorted(match.keys())
        for key in keys:
            if key == u'name':
                continue
            print u'    %s:%s' % (key, match[key])
except TmdbMovieOrPersonNotFound, msg:
    sys.stderr.write(u"%s\n" % msg)
    sys.exit(0)
except TmdHttpError, msg:
    sys.stderr.write(error_messages['TmdHttpError'] % msg)
    sys.exit(1)
except TmdXmlError, msg:
    sys.stderr.write(error_messages['TmdXmlError'] % msg)
    sys.exit(1)
except TmdBaseError, msg:
    sys.stderr.write(error_messages['TmdBaseError'] % msg)
    sys.exit(1)
except TmdbUiAbort, msg:
    sys.stderr.write(error_messages['TmdbUiAbort'] % msg)
    sys.exit(1)
except:
    sys.stderr.write(u"! Error: Unknown error during a TMDB Interactive search by name and display of People Names\n")
    sys.exit(1)
print "========================================================================================================"
print


sys.exit(0)
