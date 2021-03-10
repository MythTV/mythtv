#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

from __future__ import unicode_literals

__title__ = "TVmaze.com"
__author__ = "Roland Ernst"
__version__ = "0.1.0"


import sys
import os
import shlex
from optparse import OptionParser


def print_etree(etostr):
    """lxml.etree.tostring is a bytes object in python3, and a str in python2.
    """
    if sys.version_info[0] == 2:
        sys.stdout.write(etostr)
    else:
        sys.stdout.write(etostr.decode("utf-8"))


def check_item(m, mitem, ignore=True):
    # item is a tuple of (str, value)
    # ToDo: Add this to the 'Metadata' class of MythTV's python bindings
    try:
        k, v = mitem
        if v is None:
            return None
        m._inv_trans[m._global_type[k]](v)
        return v
    except:
        if ignore:
            return None
        else:
            raise


def _get_series_image(item):
    url = item['resolutions']['original']['url']
    if item['resolutions'].get('medium') is not None:
        thumb = item['resolutions']['medium']['url']
        pdict = {'type': 'fanart', 'url': url, 'thumb': thumb}
    else:
        pdict = {'type': 'fanart', 'url': url}
    return pdict


def buildList(tvtitle, opts):
    # option -M title
    from lxml import etree
    from MythTV import VideoMetadata, datetime
    from MythTV.utility import levenshtein
    from MythTV.tvmaze import tvmaze_api as tvmaze
    from MythTV.tvmaze import locales

    # set the session
    if opts.session:
        tvmaze.set_session(opts.session)

    if opts.debug:
        print("Function 'buildList' called with argument '%s'" % tvtitle)

    showlist = tvmaze.search_show(tvtitle)

    if opts.debug:
        print("tvmaze.search_show(%s) returned :" % tvtitle)
        for l in showlist:
            print(l, type(l))
            for k, v in l.__dict__.items():
                print(k, " : ", v)

    tree = etree.XML(u'<metadata></metadata>')

    for show_info in showlist:
        m = VideoMetadata()
        m.title = check_item(m, ("title", show_info.name), ignore=False)
        m.description = check_item(m, ("description", show_info.summary))
        m.inetref = check_item(m, ("inetref", str(show_info.id)), ignore=False)
        m.collectionref = check_item(m, ("collectionref", str(show_info.id)), ignore=False)
        m.language = check_item(m, ("language", str(locales.Language.getstored(show_info.language))))
        if show_info.premiere_date:
            m.releasedate = check_item(m, ("releasedate", show_info.premiere_date))
            m.year = check_item(m, ("year", show_info.premiere_date.year))
        if show_info.images is not None and len(show_info.images) > 0:
            m.images.append({'type': 'coverart', 'url': show_info.images['original'],
                             'thumb': show_info.images['medium']})
        tree.append(m.toXML())

    print_etree(etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                               xml_declaration=True))


def buildNumbers(args, opts):
    # either option -N inetref subtitle
    # or  option -N title subtitle
    from lxml import etree
    from MythTV import VideoMetadata, datetime
    from MythTV.utility import levenshtein
    from MythTV.tvmaze import tvmaze_api as tvmaze
    from MythTV.tvmaze import locales

    if opts.debug:
        print("Function 'buildNumbers' called with arguments: " +
              (" ".join(["'%s'" % i for i in args])))

    # set the session
    if opts.session:
        tvmaze.set_session(opts.session)

    # ToDo:
    # below check shows a deficiency of the MythTV grabber API itself:
    # TV-Shows or Movies with an integer as title are not recognized correctly.
    # see https://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
    # and https://code.mythtv.org/trac/ticket/11850
    try:
        inetref = int(args[0])
        tvsubtitle = args[1]
    except ValueError:
        tvtitle = args[0]
        tvsubtitle = args[1]
        serie = tvmaze.search_show_best_match(tvtitle)
        try:
            # inetref = str(serie.id)
            inetref = int(serie.id)
            if opts.debug:
                print("tvmaze.search_show_best_match(%s) returned inetref : %s"
                      % (tvtitle, inetref))
                if 0 and opts.debug:
                    print(serie, type(serie))
                    for k, v in serie.__dict__.items():
                        print(k, " : ", v)
        except(TypeError, ValueError):
            raise Exception("Cannot resolve argument 'inetref'")

    # get episode based on subtitle
    episodes = tvmaze.get_show_episode_list(inetref)

    best_ep_index = None
    ep_distance = 5                     ### XXX read distance from settings
    for i, ep in enumerate(episodes):
        if 0 and opts.debug:
            print("tvmaze.vmaze.get_show_episode_list(%s) returned :" % inetref)
            for k, v in ep.__dict__.items():
                print(k, " : ", v)
        distance = levenshtein(ep.name, tvsubtitle)
        if distance < ep_distance:
            best_ep_index = i
            ep_distance = distance
    if len(episodes) > 0 and best_ep_index is not None:
        season_nr  = str(episodes[best_ep_index].season)
        episode_nr = str(episodes[best_ep_index].number)
        episode_id = episodes[best_ep_index].id
        if opts.debug:
            print("tvmaze.vmaze.get_show_episode_list(%s) returned :" % inetref)
            print("with season : %s and episode %s" % (season_nr, episode_nr))
            print("Chosen episode index '%d' based on levenshtein distance '%d'."
                  % (best_ep_index, ep_distance))

        # we have now inetref, season, episode, episode_id
        buildSingle([inetref, season_nr, episode_nr], opts, tvmaze_episode_id=episode_id)
    else:
        # tvmaze.py -N 4711 "Episode 42"
        raise Exception("Cannot find episodes for inetref '%s'." % inetref)


def buildSingle(args, opts, tvmaze_episode_id=None):
    """
    The tvmaze api returns different id's for season, episode and series.
    MythTV stores only the series-id, therefore we need to fetch the correct id's
    for season and episode for that series-id.
    """
    # option -D inetref season episode

    from lxml import etree
    from MythTV import VideoMetadata, datetime
    from MythTV.utility import levenshtein
    from MythTV.tvmaze import tvmaze_api as tvmaze
    from MythTV.tvmaze import locales

    if opts.debug:
        dstr = "Function 'buildSingle' called with arguments: " + \
                (" ".join(["'%s'" % i for i in args]))
        if tvmaze_episode_id is not None:
            dstr += "tvmaze_episode_id = %d" % tvmaze_episode_id
        print(dstr)
    inetref = args[0]
    season  = args[1]
    episode = args[2]

    # set the session
    if opts.session:
        tvmaze.set_session(opts.session)

    # get the episode_id if not provided:
    if tvmaze_episode_id is None:
        episodes = tvmaze.get_show_episode_list(inetref)
        for ep in (episodes):
            if 0 and opts.debug:
                print("tvmaze.vmaze.get_show_episode_list(%s) returned :" % inetref)
                for k, v in ep.__dict__.items():
                    print(k, " : ", v)
            if ep.season == int(season) and ep.number == int(episode):
                tvmaze_episode_id = ep.id
                ### XXX check if season == int(ep.season)          ### XXX
                if opts.debug:
                    print(" Found tvmaze_episode_id : %d" % tvmaze_episode_id)
                break

    # get info for dedicated season:
    ep_info = tvmaze.get_episode_information(tvmaze_episode_id)
    if opts.debug:
        for k, v in ep_info.__dict__.items():
            print(k, " : ", v)

    # get global info for all seasons/episodes:
    show_info = tvmaze.get_show(inetref, populated=True)
    if opts.debug:
        print("tvmaze.get_show(%s, populated=True) returned :" % inetref)
        for k, v in show_info.__dict__.items():
            print(k, " : ", v)
        if 0 and opts.debug:
            for c in show_info.cast:
                for k, v in c.__dict__.items():
                    print(k, " : ", v)
            for c in show_info.crew:
                for k, v in c.__dict__.items():
                    print(k, " : ", v)

    # get info for dedicated season:
    season_info = show_info.seasons[int(season)]
    if opts.debug:
        for k, v in season_info.__dict__.items():
            print(k, " : ", v)

    # build xml:
    tree = etree.XML(u'<metadata></metadata>')
    m = VideoMetadata()
    if show_info.genres is not None and len(show_info.genres) > 0:
        for g in show_info.genres:
            try:
                if g is not None and len(g) > 0:
                    m.categories.append(g)
            except:
                pass
    m.title = check_item(m, ("title", show_info.name), ignore=False)
    m.subtitle = check_item(m, ("title", ep_info.name), ignore=False)
    m.season = check_item(m, ("season", ep_info.season), ignore=False)
    m.episode = check_item(m, ("episode", ep_info.number), ignore=False)
    m.description = check_item(m, ("description", ep_info.summary))
    if m.description is None:
        m.description = check_item(m, ("description", show_info.summary))
    try:
        sinfo = show_info.network['name']
        if sinfo is not None and len(sinfo) > 0:
            m.studios.append(sinfo)
    except:
        pass
    m.inetref = check_item(m, ("inetref", str(show_info.id)), ignore=False)
    m.collectionref = check_item(m, ("inetref", str(show_info.id)), ignore=False)
    m.language = check_item(m, ("language", str(locales.Language.getstored(show_info.language))))
    # prefer episode airdate dates:
    if ep_info.airdate:
        m.releasedate = check_item(m, ("releasedate", ep_info.airdate))
        m.year = check_item(m, ("year", ep_info.airdate.year))
    elif show_info.premiere_date:
        m.releasedate = check_item(m, ("releasedate", show_info.premiere_date))
        m.year = check_item(m, ("year", show_info.premiere_date.year))
    m.runtime = check_item(m, ("runtime", int(ep_info.duration)))

    for actor in show_info.cast:
        try:
            if len(actor.person.name) > 0 and len(actor.name) > 0:
                d = {'name': actor.person.name, 'character': actor.name, 'job': 'Actor'}
                ### , 'department': 'Actors'}
                m.people.append(d)
        except:
            pass

    for member in show_info.crew:
        try:
            if len(member.name) > 0 and len(member.job) > 0:
                d = {'name': member.name, 'job': member.job}
                m.people.append(d)
        except:
            pass

    # prefer season coverarts over series coverart:
    if season_info.images is not None and len(season_info.images) > 0:
        m.images.append({'type': 'coverart', 'url': season_info.images['original'],
                         'thumb': season_info.images['medium']})
    if show_info.images is not None and len(show_info.images) > 0:
        m.images.append({'type': 'coverart', 'url': show_info.images['original'],
                         'thumb': show_info.images['medium']})
    # screenshot is associated to episode
    if ep_info.images is not None and len(ep_info.images) > 0:
        m.images.append({'type': 'screenshot', 'url': ep_info.images['original'],
                         'thumb': ep_info.images['medium']})

    if show_info.series_images is not None and len(show_info.series_images) > 0:
        # sort for 'background' and then for 'poster'
        for item in show_info.series_images:
            if item['type'] == 'background' and not item['main']:
                try:
                    pdict = _get_series_image(item)
                    if pdict not in m.images:
                        m.images.append(pdict)
                except(KeyError, ValueError):
                    pass

        ## posters cannot be stretched to fullscreen as fanart          ### XXX
        #for item in show_info.series_images:
        #    if item['type'] == 'poster' and not item['main']:
        #        try:
        #            pdict = _get_series_image(item)
        #            if pdict not in m.images:
        #                m.images.append(pdict)
        #        except(KeyError, ValueError):
        #            pass

    tree.append(m.toXML())
    print_etree(etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                               xml_declaration=True))


def buildCollection(tvinetref, opts):
    # option -C inetref
    from lxml import etree
    from MythTV import VideoMetadata, datetime
    from MythTV.utility import levenshtein
    from MythTV.tvmaze import tvmaze_api as tvmaze
    from MythTV.tvmaze import locales

    # set the session
    if opts.session:
        tvmaze.set_session(opts.session)

    if opts.debug:
        print("Function 'buildCollection' called with argument '%s'" % tvinetref)

    show_info = tvmaze.get_show(tvinetref)
    if opts.debug:
        for k, v in show_info.__dict__.items():
            print(k, " : ", v)

    tree = etree.XML(u'<metadata></metadata>')
    m = VideoMetadata()
    m.title = check_item(m, ("title", show_info.name), ignore=False)
    m.description = check_item(m, ("description", show_info.summary))
    if show_info.genres is not None and len(show_info.genres) > 0:
        for g in show_info.genres:
            try:
                if g is not None and len(g) > 0:
                    m.categories.append(g)
            except:
                pass
    m.inetref = check_item(m, ("inetref", str(show_info.id)), ignore=False)
    m.collectionref = check_item(m, ("collectionref", str(show_info.id)), ignore=False)
    m.imdb = check_item(m, ("imdb", str(show_info.external_ids['imdb'])))
    m.language = check_item(m, ("language", str(locales.Language.getstored(show_info.language))))
    m.userrating = check_item(m, ("userrating", show_info.rating['average']))
    if show_info.premiere_date:
        m.releasedate = check_item(m, ("releasedate", show_info.premiere_date))
        m.year = check_item(m, ("year", show_info.premiere_date.year))
    try:
        sinfo = show_info.network['name']
        if sinfo is not None and len(sinfo) > 0:
            m.studios.append(sinfo)
    except:
        pass
    if show_info.images is not None and len(show_info.images) > 0:
        m.images.append({'type': 'coverart', 'url': show_info.images['original'],
                         'thumb': show_info.images['medium']})
    tree.append(m.toXML())

    print_etree(etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                               xml_declaration=True))


def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "thumbnail").text = 'tvmaze.png'
    etree.SubElement(version, "command").text = 'tvmaze.py'
    etree.SubElement(version, "type").text = 'television'
    etree.SubElement(version, "description").text = \
        'Search and metadata downloads for tvmaze.com'
    etree.SubElement(version, "version").text = __version__
    print_etree(etree.tostring(version, encoding='UTF-8', pretty_print=True,
                               xml_declaration=True))
    sys.exit(0)


def performSelfTest(opts):
    err = 0
    try:
        import lxml
    except:
        err = 1
        print("Failed to import python lxml library.")
    try:
        import requests
        import requests_cache
    except:
        err = 1
        print("Failed to import python-requests or python-request-cache library.")
    try:
        import MythTV
    except:
        err = 1
        print("Failed to import MythTV bindings. Check your `configure` output "
              "to make sure installation was not disabled due to external "
              "dependencies")
    try:
        from MythTV.tvmaze import tvmaze_api as tvmaze
        if opts.debug:
            print("File location: ", tvmaze.__file__)
            print("TVMAZE Script Version: ", __version__)
            print("TVMAZE-API version:    ", tvmaze.MYTHTV_TVMAZE_API_VERSION)
    except:
        err = 1
        print("Failed to import PyTVmaze library. This should have been included "
              "with the python MythTV bindings.")
    if not err:
        print("Everything appears in order.")
    sys.exit(err)


def main():
    """
    Main executor for MythTV's tvmaze grabber.
    """

    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Perform self-test for dependencies.")
    parser.add_option('-M', "--list", action="store_true", default=False,
                      dest="tvlist", help="Get TV Shows matching search.")
    parser.add_option('-D', "--data", action="store_true", default=False,
                      dest="tvdata", help="Get TV Show data.")
    parser.add_option('-C', "--collection", action="store_true", default=False,
                      dest="collectiondata", help="Get Collection data.")
    parser.add_option('-N', "--numbers", action="store_true", default=False,
                      dest="tvnumbers", help="Get Season and Episode numbers")
    parser.add_option('-l', "--language", metavar="LANGUAGE", default=u'en',
                      dest="language", help="Specify language for filtering.")
    parser.add_option('-a', "--area", metavar="COUNTRY", default=None,
                      dest="country", help="Specify country for custom data.")
    parser.add_option('--debug', action="store_true", default=False,
                      dest="debug", help=("Disable caching and enable raw "
                                          "data output."))
    parser.add_option('--doctest', action="store_true", default=False,
                      dest="doctest", help="Run doctests")

    opts, args = parser.parse_args()

    if opts.debug:
        print("Args: ", args)
        print("Opts: ", opts)

    if opts.doctest:
        import doctest
        opts.debug = False

        try:
            with open("tvmaze_tests.txt") as f:
                if sys.version_info[0] == 2:
                    dtests = b"".join(f.readlines()).decode('utf-8')
                else:
                    dtests = "".join(f.readlines())
            main.__doc__ += dtests
        except IOError:
            pass
        # perhaps try optionflags=doctest.ELLIPSIS | doctest.NORMALIZE_WHITESPACE
        doctest.testmod(verbose=True, optionflags=doctest.ELLIPSIS)

    if opts.version:
        buildVersion()

    if opts.test:
        performSelfTest(opts)

    if opts.debug:
        import requests
        pass
    else:
        confdir = os.environ.get('MYTHCONFDIR', '')
        if (not confdir) or (confdir == '/'):
            confdir = os.environ.get('HOME', '')
            if (not confdir) or (confdir == '/'):
                print("Unable to find MythTV directory for metadata cache.")
                sys.exit(1)
            confdir = os.path.join(confdir, '.mythtv')
        cachedir = os.path.join(confdir, 'cache')
        if not os.path.exists(cachedir):
            os.makedirs(cachedir)
        if sys.version_info[0] == 2:
            cache_name = os.path.join(cachedir, 'py2tvmaze')
        else:
            cache_name = os.path.join(cachedir, 'py3tvmaze')
        import requests
        import requests_cache
        requests_cache.install_cache(cache_name, backend='sqlite', expire_after=3600)

    with requests.Session() as s:
        s.headers.update({'Accept': 'application/json',
                          'User-Agent': 'mythtv tvmaze grabber %s' % __version__})
        opts.session = s
        try:
            if opts.tvlist:
                # option -M title
                if (len(args) != 1) or (len(args[0]) == 0):
                    sys.stdout.write('ERROR: tvmaze -M requires exactly one non-empty argument')
                    sys.exit(1)
                buildList(args[0], opts)

            if opts.tvnumbers:
                # either option -N inetref subtitle
                # or  option -N title subtitle
                if (len(args) != 2) or (len(args[0]) == 0) or (len(args[1]) == 0):
                    sys.stdout.write('ERROR: tvmaze -N requires exactly two non-empty arguments')
                    sys.exit(1)
                buildNumbers(args, opts)

            if opts.tvdata:
                # option -D inetref season episode
                if (len(args) != 3) or (len(args[0]) == 0) or (len(args[1]) == 0) or (len(args[2]) == 0):
                    sys.stdout.write('ERROR: tvmaze -D requires exactly three non-empty arguments')
                    sys.exit(1)
                buildSingle(args, opts)

            if opts.collectiondata:
                if (len(args) != 1) or (len(args[0]) == 0):
                    sys.stdout.write('ERROR: tvmaze -C requires exactly one non-empty argument')
                    sys.exit(1)
                buildCollection(args[0], opts)
        except:
            if opts.debug:
                raise
            sys.stdout.write('ERROR: ' + str(sys.exc_info()[0]) + ' : ' + str(sys.exc_info()[1]) + '\n')
            sys.exit(1)


if __name__ == "__main__":
    main()
