#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tmdb3.py
# Python Script
# Author: Peter Bennett
# Purpose:
#   Frontend for the tmdb3 lookup.py script that now supports both Movies and
#   TV shows. This frontend supports Movies
# Command example:
# See help (-h) options
#
# Code that was originally here is now in tmdb3/lookup.py
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
# -------------------------------------
#
__title__ = "TheMovieDB.org for TV V3"
__author__ = "Peter Bennett"

from optparse import OptionParser
import sys
import signal

def performSelfTest():
    err = 0
    try:
        import MythTV
    except:
        err = 1
        print ("Failed to import MythTV bindings. Check your `configure` output "
               "to make sure installation was not disabled due to external "
               "dependencies")
    try:
        import MythTV.tmdb3
    except:
        err = 1
        print ("Failed to import PyTMDB3 library. This should have been included "
               "with the python MythTV bindings.")
    try:
        import lxml
    except:
        err = 1
        print ("Failed to import python lxml library.")

    if not err:
        print ("Everything appears in order.")
    return err

def main(showType, command):

    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Perform self-test for dependencies.")
    parser.add_option('-M', "--movielist", "--list", action="store_true", default=False,
                      dest="movielist",
                      help="Get Movies. Needs search key.")
    parser.add_option('-D', "--moviedata", "--data", action="store_true", default=False,
                      dest="moviedata", help="Get Movie data. " \
                      "Needs inetref. ")
    parser.add_option('-C', "--collection", action="store_true", default=False,
                      dest="collectiondata", help="Get Collection data.")
    parser.add_option('-l', "--language", metavar="LANGUAGE", default=u'en',
                      dest="language", help="Specify language for filtering.")
    parser.add_option('-a', "--area", metavar="COUNTRY", default=None,
                      dest="country", help="Specify country for custom data.")
    parser.add_option('--debug', action="store_true", default=False,
                      dest="debug", help=("Disable caching and enable raw "
                                          "data output."))

    opts, args = parser.parse_args()

    from MythTV.tmdb3.lookup import timeouthandler
    signal.signal(signal.SIGALRM, timeouthandler)
    signal.alarm(180)

    if opts.test:
        return performSelfTest()

    from MythTV.tmdb3 import set_key, set_cache, set_locale
    set_key('c27cb71cff5bd76e1a7a009380562c62')

    if opts.debug:
        import MythTV.tmdb3
        MythTV.tmdb3.request.DEBUG = True
        set_cache(engine='null')
    else:
        import os
        confdir = os.environ.get('MYTHCONFDIR', '')
        if (not confdir) or (confdir == '/'):
            confdir = os.environ.get('HOME', '')
            if (not confdir) or (confdir == '/'):
                print ("Unable to find MythTV directory for metadata cache.")
                return 1
            confdir = os.path.join(confdir, '.mythtv')
        cachedir = os.path.join(confdir, 'cache')
        if not os.path.exists(cachedir):
            os.makedirs(cachedir)
        cachepath = os.path.join(cachedir, 'pytmdb3.cache')
        set_cache(engine='file', filename=cachepath)

    if opts.language:
        set_locale(language=opts.language, fallthrough=True)
    if opts.country:
        set_locale(country=opts.country, fallthrough=True)

    if (not opts.version):
        if (len(args) < 1) or (args[0] == ''):
            sys.stdout.write('ERROR: tmdb3.py requires at least one non-empty argument.\n')
            return 1

    from MythTV.tmdb3.lookup import buildVersion, buildMovieList, \
        buildSingle, buildCollection, print_etree
    try:
        xml = None
        if opts.version:
            xml = buildVersion(showType, command)

        elif opts.movielist:
            xml = buildMovieList(args[0], opts)

        elif opts.moviedata:
            xml = buildSingle(args[0], opts)

        elif opts.collectiondata:
            xml = buildCollection(args[0], opts)

        # if a number is returned, it is an error code return
        if isinstance(xml,int):
            return xml

        if xml:
            print_etree(xml)
        else:
            return 1

    except RuntimeError as exc:
        sys.stdout.write('ERROR: ' + str(exc) + ' exception')
        import traceback
        traceback.print_exc()
        return 1

    return 0

if __name__ == '__main__':
    sys.exit(main("movie",'tmdb3.py'))
