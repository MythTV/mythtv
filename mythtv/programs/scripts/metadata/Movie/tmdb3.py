#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tmdb3.py
# Python Script
# Author: Raymond Wagner
# Purpose: This python script is intended to translate lookups between the
#          TheMovieDB.org V3 API and MythTV's internal metadata format.
#               http://www.mythtv.org/wiki/MythVideo_Grabber_Script_Format
#               http://help.themoviedb.org/kb/api/about-3
#-----------------------
__title__ = "TheMovieDB.org V3"
__author__ = "Raymond Wagner"
__version__ = "0.3.7"
# 0.1.0 Initial version
# 0.2.0 Add language support, move cache to home directory
# 0.3.0 Enable version detection to allow use in MythTV
# 0.3.1 Add --test parameter for proper compatibility with mythmetadatalookup
# 0.3.2 Add --area parameter to allow country selection for release date and
#       parental ratings
# 0.3.3 Use translated title if available
# 0.3.4 Add support for finding by IMDB under -D (simulate previous version)
# 0.3.5 Add debugging mode
# 0.3.6 Add handling for TMDB site and library returning null results in
#       search. This should only need to be a temporary fix, and should be
#       resolved upstream.
# 0.3.7 Add handling for TMDB site returning insufficient results from a
#       query

from optparse import OptionParser
import sys

def buildSingle(inetref, opts):
    from MythTV.tmdb3 import Movie
    from MythTV import VideoMetadata
    from lxml import etree

    import re
    if re.match('^0[0-9]{6}$', inetref):
        movie = Movie.fromIMDB(inetref)
    else:
        movie = Movie(inetref)

    tree = etree.XML(u'<metadata></metadata>')
    mapping = [['runtime',      'runtime'],     ['title',       'originaltitle'],
               ['releasedate',  'releasedate'], ['tagline',     'tagline'],
               ['description',  'overview'],    ['homepage',    'homepage'],
               ['userrating',   'userrating'],  ['popularity',  'popularity'],
               ['budget',       'budget'],      ['revenue',     'revenue']]
    m = VideoMetadata()
    for i,j in mapping:
        if getattr(movie, j):
            setattr(m, i, getattr(movie, j))

    if movie.title:
        m.title = movie.title

    releases = movie.releases.items()

    if opts.country:
        try:
            # resort releases with selected country at top to ensure it
            # is selected by the metadata libraries
            index = zip(*releases)[0].index(opts.country)
            releases.insert(0, releases.pop(index))
        except ValueError:
            pass
        else:
            m.releasedate = releases[0][1].releasedate

    m.inetref = str(movie.id)
    if movie.collection:
        m.collectionref = str(movie.collection.id)
    if movie.releasedate:
        m.year = movie.releasedate.year
    for country, release in releases:
        if release.certification:
            m.certifications[country] = release.certification
    for genre in movie.genres:
        m.categories.append(genre.name)
    for studio in movie.studios:
        m.studios.append(studio.name)
    for country in movie.countries:
        m.countries.append(country.name)
    for cast in movie.cast:
        d = {'name':cast.name, 'character':cast.character, 'department':'Actors',
             'job':'Actor', 'url':'http://www.themoviedb.org/people/{0}'.format(cast.id)}
        if cast.profile: d['thumb'] = cast.profile.geturl()
        m.people.append(d)
    for crew in movie.crew:
        d = {'name':crew.name, 'job':crew.job, 'department':crew.department,
             'url':'http://www.themoviedb.org/people/{0}'.format(crew.id)}
        if crew.profile: d['thumb'] = crew.profile.geturl()
        m.people.append(d)
    for backdrop in movie.backdrops:
        m.images.append({'type':'fanart', 'url':backdrop.geturl(),
                        'thumb':backdrop.geturl(backdrop.sizes()[0]),
                        'height':str(backdrop.height),
                        'width':str(backdrop.width)})
    for poster in movie.posters:
        m.images.append({'type':'coverart', 'url':poster.geturl(),
                        'thumb':poster.geturl(poster.sizes()[0]),
                        'height':str(poster.height),
                        'width':str(poster.width)})
    tree.append(m.toXML())
    sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit()

def buildList(query, opts):
    # TEMPORARY FIX:
    # replace all dashes from queries to work around search behavior
    # as negative to all text that comes afterwards
    query = query.replace('-',' ')

    from MythTV.tmdb3 import searchMovie
    from MythTV import VideoMetadata
    from lxml import etree
    results = iter(searchMovie(query))
    tree = etree.XML(u'<metadata></metadata>')
    mapping = [['runtime',      'runtime'],     ['title',       'originaltitle'],
               ['releasedate',  'releasedate'], ['tagline',     'tagline'],
               ['description',  'overview'],    ['homepage',    'homepage'],
               ['userrating',   'userrating'],  ['popularity',  'popularity']]

    count = 0
    while True:
        try:
            res = results.next()
        except StopIteration:
            # end of results
            break
        except IndexError:
            # unexpected end of results
            # we still want to return whatever we have so far
            break

        if res is None:
            # faulty data, skip it and continue
            continue

        m = VideoMetadata()
        for i,j in mapping:
            if getattr(res, j):
                setattr(m, i, getattr(res, j))
        m.inetref = str(res.id)

        if res.title:
            m.title = res.title

        #TODO:
        # should releasedate and year be pulled from the country-specific data
        # or should it be left to the default information to cut down on
        # traffic from searches
        if res.releasedate:
            m.year = res.releasedate.year
        if res.backdrop:
            b = res.backdrop
            m.images.append({'type':'fanart', 'url':b.geturl(),
                             'thumb':b.geturl(b.sizes()[0])})
        if res.poster:
            p = res.poster
            m.images.append({'type':'coverart', 'url':p.geturl(),
                             'thumb':p.geturl(p.sizes()[0])})
        tree.append(m.toXML())
        count += 1
        if count >= 60:
            # page limiter, dont want to overload the server
            break

    sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit(0)

def buildCollection(inetref, opts):
    from MythTV.tmdb3 import Collection
    from MythTV import VideoMetadata
    from lxml import etree
    collection = Collection(inetref)
    tree = etree.XML(u'<metadata></metadata>')
    m = VideoMetadata()
    m.collectionref = str(collection.id)
    m.title = collection.name
    if collection.backdrop:
        b = collection.backdrop
        m.images.append({'type':'fanart', 'url':b.geturl(),
                  'thumb':b.geturl(b.sizes()[0])})
    if collection.poster:
        p = collection.poster
        m.images.append({'type':'coverart', 'url':p.geturl(),
                  'thumb':p.geturl(p.sizes()[0])})
    tree.append(m.toXML())
    sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit()

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "thumbnail").text = 'tmdb.png'
    etree.SubElement(version, "command").text = 'tmdb3.py'
    etree.SubElement(version, "type").text = 'movie'
    etree.SubElement(version, "description").text = \
                                'Search and metadata downloads for themoviedb.org'
    etree.SubElement(version, "version").text = __version__
    sys.stdout.write(etree.tostring(version, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit(0)

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
        print "Failed to import python lxml library."

    if not err:
        print "Everything appears in order."
    sys.exit(err)

def main():
    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Perform self-test for dependencies.")
    parser.add_option('-M', "--movielist", action="store_true", default=False,
                      dest="movielist", help="Get Movies matching search.")
    parser.add_option('-D', "--moviedata", action="store_true", default=False,
                      dest="moviedata", help="Get Movie data.")
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

    if opts.version:
        buildVersion()

    if opts.test:
        performSelfTest()

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
                print "Unable to find MythTV directory for metadata cache."
                sys.exit(1)
            confdir = os.path.join(confdir, '.mythtv')
        confpath = os.path.join(confdir, 'cache/pytmdb3.cache')
        if not os.access(confpath, os.F_OK|os.W_OK|os.R_OK):
            print "Unable to access cache file: "+confpath
            sys.exit(1)
        set_cache(engine='file', filename=confpath)

    if opts.language:
        set_locale(language=opts.language, fallthrough=True)
    if opts.country:
        set_locale(country=opts.country, fallthrough=True)

    if (len(args) != 1) or (args[0] == ''):
        sys.stdout.write('ERROR: tmdb3.py requires exactly one non-empty argument')
        sys.exit(1)

    if opts.movielist:
        buildList(args[0], opts)

    if opts.moviedata:
        buildSingle(args[0], opts)

    if opts.collectiondata:
        buildCollection(args[0], opts)

if __name__ == '__main__':
    main()
