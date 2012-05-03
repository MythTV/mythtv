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
__version__ = "0.3.0"
# 0.1.0 Initial version
# 0.2.0 Add language support, move cache to home directory
# 0.3.0 Enable version detection to allow use in MythTV

from MythTV.tmdb3 import searchMovie, Movie, Collection, set_key, set_cache, set_locale
from MythTV import VideoMetadata

from optparse import OptionParser
from lxml import etree
import sys

def buildSingle(inetref):
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
    m.inetref = str(movie.id)
    if movie.releasedate:
        m.year = movie.releasedate.year
    if movie.collection:
        m.collectionref = str(movie.collection.id)
    for country, release in movie.releases.items():
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
             'url':'http://www.themoviedb.org/people/{0}'.format(cast.id)}
        if crew.profile: d['thumb'] = crew.profile.geturl()
        m.people.append(d)
    for backdrop in movie.backdrops:
        m.images.append({'type':'fanart', 'url':backdrop.geturl(),
                        'thumb':backdrop.geturl(backdrop.sizes()[0])})
    for poster in movie.posters:
        m.images.append({'type':'coverart', 'url':poster.geturl(),
                        'thumb':poster.geturl(poster.sizes()[0])})
    tree.append(m.toXML())
    sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit()

def buildList(query):
    # TEMPORARY FIX:
    # remove all dashes from queries to work around search behavior
    # as negative to all text that comes afterwards
    query = query.replace('-','')
    results = searchMovie(query)
    tree = etree.XML(u'<metadata></metadata>')
    mapping = [['runtime',      'runtime'],     ['title',       'originaltitle'],
               ['releasedate',  'releasedate'], ['tagline',     'tagline'],
               ['description',  'overview'],    ['homepage',    'homepage'],
               ['userrating',   'userrating'],  ['popularity',  'popularity']]

    count = 0
    for res in results:
        m = VideoMetadata()
        for i,j in mapping:
            if getattr(res, j):
                setattr(m, i, getattr(res, j))
        m.inetref = str(res.id)
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

def buildCollection(inetref):
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

def main():
    set_key('c27cb71cff5bd76e1a7a009380562c62')
    set_cache(engine='file', filename='~/.mythtv/pytmdb3.cache')

    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-M', "--movielist", action="store_true", default=False,
                      dest="movielist", help="Get Movies matching search.")
    parser.add_option('-D', "--moviedata", action="store_true", default=False,
                      dest="moviedata", help="Get Movie data.")
    parser.add_option('-C', "--collection", action="store_true", default=False,
                      dest="collectiondata", help="Get Collection data.")
    parser.add_option( "-l", "--language", metavar="LANGUAGE", default=u'en',
                      dest="language", help="Specify language for filtering.")

    opts, args = parser.parse_args()

    if opts.version:
        buildVersion()

    if opts.language:
        set_locale(language=opts.language, fallthrough=True)

    if (len(args) != 1) or (args[0] == ''):
        sys.stdout.write('ERROR: tmdb3.py requires exactly one non-empty argument')
        sys.exit(1)

    if opts.movielist:
        buildList(args[0])

    if opts.moviedata:
        buildSingle(args[0])

    if opts.collectiondata:
        buildCollection(args[0])

if __name__ == '__main__':
    main()
