#!/usr/bin/env python3
# This file is part of MythTV.
# Copyright 2016, Paul Harrison.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.

"""Various utilities using the musicbrainzngs python bindings to access the MB database"""

from optparse import OptionParser
import musicbrainzngs
import sys
import pprint

__author__      = "Paul Harrison'"
__title__       = "Music Brainz utilities"
__description__ = "Various utilities using the musicbrainzngs python bindings to access the MB database"
__version__     = "0.2"

debug = False

musicbrainzngs.set_useragent(
    "MythTV",
    "32.0",
    "https://www.mythtv.org",
)

musicbrainzngs.set_hostname("musicbrainz.org")

def log(debug, txt):
    if debug:
        print(txt)

def convert_etree(etostr):
    """lxml.etree.tostring is a bytes object in python3, and a str in python2.
    """
    return(etostr.decode())

def search_releases(artist, album, limit):
    from lxml import etree

    root = etree.XML(u'<searchreleases></searchreleases>')

    result = musicbrainzngs.search_releases(artist=artist, release=album, country="GB", limit=limit)

    if not result['release-list']:
        etree.SubElement(root, "error").text = "No Releases found"
        log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
        sys.exit(1)

    for (idx, release) in enumerate(result['release-list']):
        pprint.pprint(release)
        relNode = etree.SubElement(root, "release")

        etree.SubElement(relNode, "id").text = release['id']
        etree.SubElement(relNode, "ext-score").text = release['ext:score']
        etree.SubElement(relNode, "title").text = release['title']
        etree.SubElement(relNode, "artist-credit-phrase").text = release["artist-credit-phrase"]
        etree.SubElement(relNode, "country").text = release["country"]

        if 'date' in release:
            etree.SubElement(relNode, "date").text = release['date']
            etree.SubElement(relNode, "status").text = release['status']

        if 'release-group' in release:
            etree.SubElement(relNode, "releasegroup").text = release["release-group"]["id"]

    log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))

    sys.exit(0)

def search_artists(artist, limit):
    from lxml import etree

    root = etree.XML(u'<artists></artists>')
    result = musicbrainzngs.search_artists(artist=artist, limit=limit)

    if debug:
        pprint.pprint(result)

    if result['artist-list']:
        for (idx, artist) in enumerate(result['artist-list']):
            artistNode = etree.SubElement(root, "id")
            etree.SubElement(artistNode, "id").text = artist['id']
            etree.SubElement(artistNode, "name").text = artist['name']
            etree.SubElement(artistNode, "sort-name").text = artist['sort-name']
    else:
        etree.SubElement(root, "error").text = "No Artists found"
        log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
        sys.exit(1)

    log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))

    sys.exit(0)

def get_artist(artistid):
    from lxml import etree

    root = etree.XML(u'<artist></artist>')

    # lookup the artist id
    result = musicbrainzngs.get_artist_by_id(artistid, includes=['url-rels'])

    if debug:
        pprint.pprint(result)

    if result:
        etree.SubElement(root, "id").text = result['artist']['id']
        etree.SubElement(root, "name").text = result['artist']['name']
        etree.SubElement(root, "sort-name").text = result['artist']['sort-name']

        if 'url-relation-list' in result['artist']:
            urls = result['artist']['url-relation-list']
            if urls:
                for (idx, url) in enumerate(urls):
                    node = etree.SubElement(root, "url")
                    node.text = url['target']
                    node.set("type", url['type'])
    else:
        etree.SubElement(root, "error").text = "MusicBrainz ID was not found"
        log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
        sys.exit(1)

    log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def find_coverart(release):
    from lxml import etree

    root = etree.XML(u'<coverart></coverart>')

    try:
        data = musicbrainzngs.get_image_list(release)
    except musicbrainzngs.ResponseError as err:
        if err.cause.code == 404:
            etree.SubElement(root, "error").text = "Release ID not found"
            log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
            sys.exit(1)
        else:
            etree.SubElement(root, "error").text = "Received bad response from the MB server"
            log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
            sys.exit(1)

    if debug:
        pprint.pprint(data)

    for image in data["images"]:
        imageNode =  etree.SubElement(root, "image")
        etree.SubElement(imageNode, "image").text = image["image"]
        etree.SubElement(imageNode, "approved").text = str(image["approved"])
        etree.SubElement(imageNode, "front").text = str(image["front"])
        etree.SubElement(imageNode, "back").text = str(image["back"])
        #etree.SubElement(imageNode, "types").text = image["types"]
        etree.SubElement(imageNode, "thumb-small").text = image["thumbnails"]["small"]
        etree.SubElement(imageNode, "thumb-large").text = image["thumbnails"]["large"]

    log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def find_coverart_releasegroup(releaseGroup):
    from lxml import etree

    root = etree.XML(u'<coverart></coverart>')

    try:
        data = musicbrainzngs.get_release_group_image_list(releaseGroup)
    except musicbrainzngs.ResponseError as err:
        if err.cause.code == 404:
            etree.SubElement(root, "error").text = "Release ID not found"
            log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
            sys.exit(1)
        else:
            etree.SubElement(root, "error").text = "Received bad response from the MB server"
            log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
            sys.exit(1)

    if debug:
        pprint.pprint(data)

    for image in data["images"]:
        imageNode =  etree.SubElement(root, "image")
        etree.SubElement(imageNode, "image").text = image["image"]
        etree.SubElement(imageNode, "approved").text = str(image["approved"])
        etree.SubElement(imageNode, "front").text = str(image["front"])
        etree.SubElement(imageNode, "back").text = str(image["back"])
        #etree.SubElement(imageNode, "types").text = image["types"]
        etree.SubElement(imageNode, "thumb-small").text = image["thumbnails"]["small"]
        etree.SubElement(imageNode, "thumb-large").text = image["thumbnails"]["large"]

    log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def find_disc(cddrive):
    import discid
    from lxml import etree

    root = etree.XML(u'<finddisc></finddisc>')

    try:
        disc = discid.read(cddrive, ["mcn", "isrc"])
        id = disc.id
        toc = disc.toc_string
    except discid.DiscError as err:
        etree.SubElement(root, "error").text = "Failed to get discid ({})".format(str(err))
        log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
        sys.exit(1)

    etree.SubElement(root, "discid").text = id
    etree.SubElement(root, "toc").text = toc

    try:
        # the "labels" include enables the cat#s we display
        result = musicbrainzngs.get_releases_by_discid(id, includes=["labels"], toc=toc, cdstubs=False)
    except musicbrainzngs.ResponseError as err:
        if err.cause.code == 404:
            etree.SubElement(root, "error").text = "Disc not found"
            log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
            sys.exit(1)
        else:
            etree.SubElement(root, "error").text = "Received bad response from the MB server"
            log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
            sys.exit(1)

    # The result can either be a "disc" or a "cdstub"
    if result.get('disc'):
        discnode = etree.SubElement(root, "disc")

        etree.SubElement(discnode, "sectors").text = result['disc']['sectors']

        if "offset-list" in result['disc']:
            offsets = None
            for offset in result['disc']['offset-list']:
                if offsets is None:
                    offsets = str(offset)
                else:
                    offsets += " " + str(offset)

            etree.SubElement(discnode, "offsets").text = offsets
            etree.SubElement(discnode, "tracks").text = str(result['disc']['offset-count'])

        for release in result['disc']['release-list']:
            relnode = etree.SubElement(discnode, "release")

            etree.SubElement(relnode, "title").text = release['title']
            etree.SubElement(relnode, "musicbrainzid").text = release['id']

            if release.get('barcode'):
                etree.SubElement(relnode, "barcode").text = release['barcode']
            for info in release['label-info-list']:
                if info.get('catalog-number'):
                    etree.SubElement(relnode, "catalog-number").text = info['catalog-number']
    elif result.get('cdstub'):
        stubnode = etree.SubElement(root, "cdstub")

        etree.SubElement(stubnode, "artist").text = result['cdstub']['artist']
        etree.SubElement(stubnode, "title").text = result['cdstub']['title']

        if result['cdstub'].get('barcode'):
            etree.SubElement(stubnode, "barcode").text = result['cdstub']['barcode']
    else:
        etree.SubElement(root, "error").text = "No valid results"
        log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
        sys.exit(1)

    log(True, convert_etree(etree.tostring(root, encoding='UTF-8', pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<version></version>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "command").text = 'mbutils.py'
    etree.SubElement(version, "description").text = __description__
    etree.SubElement(version, "version").text = __version__

    log(True, convert_etree(etree.tostring(version, encoding='UTF-8', pretty_print=True,
                             xml_declaration=True)))
    sys.exit(0)

def performSelfTest():
    err = 0
    try:
        import discid
    except:
        err = 1
        print ("Failed to import python discid lirary. Is libdiscid installed?")
    try:
        import lxml
    except:
        err = 1
        print("Failed to import python lxml library.")

    if not err:
        print("Everything appears in order.")
    sys.exit(err)


def main():
    global debug

    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Perform self-test for dependencies.")
    parser.add_option('-r', "--searchreleases", action="store_true", default=False,
                      dest="searchreleases", help="Search for musicbrainz release id's for a given artist & album. Requires --artist/--album. Optional --limit.")
    parser.add_option('-s', "--searchartists", action="store_true", default=False,
                      dest="searchartists", help="Search for musicbrainz artist id's for a given artist. Requires --artist. Optional --limit.")
    parser.add_option('-g', "--getartist", action="store_true", default=False,
                      dest="getartist", help="Lookup the details of a given musicbrainz id of an artist. Requires --id.")
    parser.add_option('-c', "--finddisc", action="store_true", default=None,
                      dest="finddisc", help="Find the musicbrainz id for a cd. Requires --cddevice.")
    parser.add_option('-f', "--findcoverart", action="store_true", default=None,
                      dest="findcoverart", help="Find coverart for a given musicbrainz id of a release or release group. Requires --id or --relgroupid.")
    parser.add_option('-d', '--debug', action="store_true", default=False,
                      dest="debug", help=("Show debug messages"))
    parser.add_option('-a', '--artist', metavar="ARTIST", default=None,
                      dest="artist", help=("Name of artist"))
    parser.add_option('-b', '--album', metavar="ALBUM", default=None,
                      dest="album", help=("Name of Album"))
    parser.add_option('-l', '--limit', metavar="LIMIT", default=None,
                      dest="limit", help=("Limits the maximum number of results to return for some commands (defaults to 5)"))
    parser.add_option('-i', '--id', metavar="ID", default=None,
                      dest="id", help=("Music Brainz ID to use"))
    parser.add_option('-I', '--relgroupid', metavar="RELEASEGROUPID", default=None,
                      dest="relgroupid", help=("Music Brainz ID of Release Group to use"))
    parser.add_option('-D', '--cddevice', metavar="CDDEVICE", default=None,
                      dest="cddevice", help=("CD device to use"))

    opts, args = parser.parse_args()

    if opts.debug:
        debug = True

    if opts.version:
        buildVersion()

    if opts.test:
        performSelfTest()

    if opts.searchreleases:
        if opts.artist is None:
            print("Missing --artist argument")
            sys.exit(1)

        if opts.album is None:
            print("Missing --album argument")
            sys.exit(1)

        limit = 5
        if opts.limit:
            limit = int(opts.limit)

        search_releases(opts.artist, opts.album, limit)

    if opts.searchartists:
        if opts.artist is None:
            print("Missing --artist argument")
            sys.exit(1)

        limit = 5
        if opts.limit:
            limit = int(opts.limit)

        search_artists(opts.artist, limit)

    if opts.getartist:
        if opts.id is None:
            print("Missing --id argument")
            sys.exit(1)

        get_artist(opts.id)

    if opts.finddisc:
        if opts.cddevice is None:
            print("Missing --cddevice argument")
            sys.exit(1)

        find_disc(opts.cddevice)

    if opts.findcoverart:
        if opts.id is None and opts.relgroupid is None:
            print("Missing --id or --relgroupid argument")
            sys.exit(1)

        if opts.id is not None:
            find_coverart(opts.id)
        else:
            find_coverart_releasegroup(opts.relgroupid)

    sys.exit(0)

if __name__ == '__main__':
    main()
