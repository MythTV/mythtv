#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tmdb_ui.py  Is a simple console user interface for the tmdb_api. The interface is used selection
#		of a movie from themoviesdb.org.
# Python Script
# Author:   dbr/Ben modified by R.D. Vaughan
# Purpose:  Console interface for selecting a movie from themoviedb.org. This interface would be invoked when
#			an exact match is not found and the invoking script has specified the "interactive = True" when
#			creating an instance of MovieDb().
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="tmdb_ui - Is a simple console user interface for the tmdb_api. The interface is used selection of a movie oe person from themoviesdb.org.";
__author__="dbr/Ben modified by R.D. Vaughan"
__purpose__='''Console interface for selecting a movie from themoviedb.org. This interface would be invoked when an exact match is not found and the invoking script has specified the "interactive = True" when creating an instance of MovieDb().
'''
__version__="v0.1.4"
# 0.1.0 Initial development
# 0.1.1 Alpha Release
# 0.1.2 Release bump - no changes to this code
# 0.1.3 Release bump - no changes to this code
# 0.1.4 Release bump - no changes to this code


"""Contains included user interfaces for tmdb movie/person selection.

A UI is a callback. A class, it's __init__ function takes two arguments:

- config, which is the tmdb config dict, setup in tmdb_api.py
- log, which is tmdb's logger instance (which uses the logging module). You can
call log.info() log.warning() etc

It pass a dictionary "allElements", this is passed a list of dicts, each dict
contains at least the keys "name" (human readable movie name), and "id" (the movies/person)
ID as on themoviedb.org). for movies only if the key 'released' is included the year will be added
to the movie title like 'Avatar (2009)' all other keys will be ignored.
For example:

[{'name': u'Avatar', 'id': u'19995', 'released': '2009-12-25'},
 {'name': u'Avatar - Sequel', 'id': u'73181'}]
OR
[{'name': u'Tom Cruise', 'id': u'500'},
 {'name': u'Cruise Moylan', 'id': u'77716'}]


The "selectMovieOrPerson" method must return the appropriate dict, or it can raise
TmdbUiAbort (if the selection is aborted), TmdbMovieOrPersonNotFound (if the movie
or person cannot be found).

A simple example callback, which returns a random movie:

>>> import random
>>> from tmdb_api_ui import BaseUI
>>> class RandomUI(BaseUI):
...    def selectMovieOrPerson(self, allElements):
...            import random
...            return random.choice(allElements)

Then to use it..

>>> from tmdb_api import MovieDb
>>> t = MovieDb(custom_ui = RandomUI)
>>> random_matching_movie = t.searchTitle('Avatar')
>>> type(random_matching_movie)
[{"Avatar",'19995'}]
"""

__version__=u"v0.1.0"
 # 0.1.0 Initial development

import os, struct, sys
from tmdb_exceptions import TmdbUiAbort

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


def makeDict(movieORperson):
    '''Make a dictionary out of the chosen movie data
    return a dictionary of the movie and ancillary data
    '''
    selection = {}
    for key in movieORperson.keys():
        selection[key] = movieORperson[key]
    return selection
# end makeDict()


class BaseUI:
    """Default non-interactive UI, which auto-selects first results
    """
    def __init__(self, config, log):
        self.config = config
        self.log = log

    def selectMovieOrPerson(self, allElements):
        return makeDict([allElements[0]])


class ConsoleUI(BaseUI):
    """Interactively allows the user to select a movie or person from a console based UI
    """

    def _displayMovie(self, allElements):
        """Helper function, lists movies or people with corresponding ID
        """
        print u"themoviedb.org Search Results:"
        for i in range(len(allElements[:15])): # list first 15 search results
            i_show = i + 1 # Start at more human readable number 1 (not 0)
            self.log.debug('Showing allElements[%s] = %s)' % (i_show, allElements[i]))
            if not allElements[i].has_key('released'):
            	title = allElements[i]['name']
            elif len(allElements[i]['released']) > 3:
            	title = u"%s (%s)" % (allElements[i]['name'], allElements[i]['released'][:4])
            else:
            	title = allElements[i]['name']
            if allElements[i]['url'].find('/person/') > -1:
                format = u"%2s -> %-30s # http://www.themoviedb.org/person/%s"
            else:
                format = u"%2s -> %-50s # http://www.themoviedb.org/movie/%s"
            print format % (
                i_show,
                title,
                allElements[i]['id']
            )
        print u"Direct search of themoviedb.org # http://themoviedb.org/"
	# end _displayMovie()

    def selectMovieOrPerson(self, allElements):
        self._displayMovie(allElements)

        refsize = 5     # THe number of digits required ia a TMDB number is directly entered
        if len(allElements) == 1:
            # Single result, return it!
            print u"Automatically selecting only result"
            return [makeDict(allElements[0])]

        if self.config['select_first'] is True:
            print u"Automatically returning first search result"
            return [makeDict(allElements[0])]

        while True: # return breaks this loop
            try:
                if allElements[0]['url'].find('/movie/') > -1:
                    morp = u'movie'
                else:
                    morp = u'person'
                print u'Enter choice:\n("Enter" key equals first selection (1)) or input a zero padded 5 digit %s TMDB id number, ? for help):' % morp
                ans = raw_input()
            except KeyboardInterrupt:
                raise TmdbUiAbort("User aborted (^c keyboard interupt)")
            except EOFError:
                raise TmdbUiAbort("User aborted (EOF received)")

            self.log.debug(u'Got choice of: %s' % (ans))
            try:
                if ans == '': # Enter pressed which equates to the first selection
					selected_id = 0
                else:
                    if int(ans) == 0:
                        raise ValueError
                    selected_id = int(ans) - 1 # The human entered 1 as first result, not zero
            except ValueError: # Input was not number
                if ans == "q":
                    self.log.debug(u'Got quit command (q)')
                    raise TmdbUiAbort("User aborted ('q' quit command)")
                elif ans == "?":
                    print u"## Help"
                    print u"# Enter the number that corresponds to the correct movie."
                    print u"# Paste a TMDB %s ID number (pad with leading zeros to make 5 digits) for themoviedb.org and hit 'Enter'" % morp
                    print u"# ? - this help"
                    print u"# q - abort/skip movie selection"
                else:
                	print '! Unknown/Invalid keypress "%s"\n' % (ans)
                	self.log.debug('Unknown keypress %s' % (ans))
            else:
                self.log.debug('Trying to return ID: %d' % (selected_id))
                try:
                    return [makeDict(allElements[selected_id])]
                except IndexError:
                    if len(ans) == refsize:
                        return [{'name': u'User selected', 'id': u'%d' % int(ans)}]
            #end try
        #end while not valid_input
	# end selectMovieOrPerson()
