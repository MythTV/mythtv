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
__title__ = "TheMovieDB.org"
__author__ = "Raymond Wagner"
__version__ = "0.1.0"
# 0.1.0 Initial version

from tmdb_api import searchMovie, Movie, Collection, set_key
from MythTV import VideoMetadata

from optparse import OptionParser
from lxml import etree
import sys

def buildSingle(inetref, language):
    movie = Movie(inetref)
    tree = etree.XML(u'<metadata></metadata>')
    mapping = [['runtime',      'runtime'],     ['title',       'originaltitle'],
               ['releasedate',  'releasedate'], ['tagline',     'tagline'],
               ['description',  'overview'],    ['homepage',    'homepage'],
               ['userrating',   'userrating'],  ['popularity',  'popularity'],
               ['budget',       'budget'],      ['revenue',     'revenue']]
    m = VideoMetadata()
    for i,j in mapping:
        setattr(m, i, getattr(movie, j))
    m.inetref = str(movie.id)
    m.year = movie.releasedate.year
    if movie.collection:
        m.collectionref = movie.collection.id
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
        if cast.profile is not None: d['thumb'] = cast.profile.geturl()
        m.people.append(d)
    for crew in movie.crew:
        d = {'name':crew.name, 'job':crew.job, 'department':crew.department,
             'url':'http://www.themoviedb.org/people/{0}'.format(cast.id)}
        if crew.profile is not None: d['thumb'] = crew.profile.geturl()
        m.people.append(d)
    for backdrop in movie.backdrops:
        m.images.append({'type':'fanart', 'url':backdrop.geturl(),
                        'thumb':backdrop.geturl(backdrop.sizes()[0])})
    for poster in movie.posters:
        m.images.append({'type':'coverart', 'url':poster.geturl(),
                        'thumb':poster.geturl(poster.sizes()[0])})
    tree.append(m.toXML())
    sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True))
    sys.exit()

def buildList(query, language):
    results = searchMovie(query, language)
    tree = etree.XML(u'<metadata></metadata>')
    mapping = [['runtime',      'runtime'],     ['title',       'originaltitle'],
               ['releasedate',  'releasedate'], ['tagline',     'tagline'],
               ['description',  'overview'],    ['homepage',    'homepage'],
               ['userrating',   'userrating'],  ['popularity',  'popularity']]
    for res in results:
        m = VideoMetadata()
        for i,j in mapping:
            setattr(m, i, getattr(res, j))
        m.inetref = str(res.id)
        m.year = res.releasedate.year
        m.images.append({'type':'fanart', 'url':res.backdrop.geturl()})
        m.images.append({'type':'coverart', 'url':res.poster.geturl()})
        tree.append(m.toXML())
    sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True))
    sys.exit(0)

def buildCollection(inetref, language):
    collection = Collection(inetref)
    tree = etree.XML(u'<metadata></metadata>')
    m = VideoMetadata()
    m.collectionref = str(collection.id)
    m.title = collection.name
    if collection.backdrop:
        m.images.append({'type':'fanart', 'url':collection.backdrop.geturl(),
                  'thumb':collection.backdrop.geturl(collection.backdrop.sizes()[0])})
    if collection.poster:
        m.images.append({'type':'coverart', 'url':collection.poster.geturl(),
                  'thumb':collection.poster.geturl(collection.poster.sizes()[0])})
    tree.append(m.toXML())
    sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True))
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
    sys.stdout.write(etree.tostring(version, encoding='UTF-8', pretty_print=True))
    sys.exit(0)

def main():
    set_key('c27cb71cff5bd76e1a7a009380562c62')

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

    if (len(args) != 1) or (args[0] == ''):
        sys.stdout.write('ERROR: tmdb3.py requires exactly one non-empty argument')
        sys.exit(1)

    if opts.movielist:
        buildList(args[0], opts.language)

    if opts.moviedata:
        buildSingle(args[0], opts.language)

    if opts.collectiondata:
        buildCollection(args[0], opts.language)

if __name__ == '__main__':
    main()
