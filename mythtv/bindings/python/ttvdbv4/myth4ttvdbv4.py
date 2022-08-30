# -*- coding: UTF-8 -*-

# ----------------------------------------------------
# Purpose:   MythTV Python Bindings for TheTVDB v4 API
# Copyright: (c) 2021 Roland Ernst
# License:   GPL v2 or later, see LICENSE for details
# ----------------------------------------------------


import sys
import os
import json
import re
import requests
import operator
import time
import pickle
from lxml import etree
from collections import OrderedDict
from enum import IntEnum
from datetime import timedelta, datetime
from MythTV.ttvdbv4 import ttvdbv4_api as ttvdb
from MythTV.ttvdbv4.locales import Language, Country
from MythTV.ttvdbv4.utils import convert_date, strip_tags
from MythTV import VideoMetadata
from MythTV.utility import levenshtein


def _print_class_content(obj):
    for k, v in obj.__dict__.items():
        if k.startswith('aliases'):
            print("    ", k, " : ", "[ %s ]" % '; '.join(x.name for x in obj.aliases))
        elif k.startswith('nameTranslations'):
            print("    ", k, " : ", v)
        elif k.startswith('fetched_translations'):
            print("    ", k, " : ", "[ %s ]" % '; '.join(x.name for x in obj.fetched_translations))
        elif k.startswith('name_similarity'):
            # name similarity is not calculated by now.
            pass
        elif isinstance(v, list):
            print("    ", k, " : ", "[  %d items  ]" % len(v))
        else:
            print("    ", k, " : ", v)


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


def _sanitize_me(obj, attr, default):
    try:
        if getattr(obj, attr) is None:
            setattr(obj, attr, default)
        return obj
    except (KeyError, AttributeError):
        raise


def sort_list_by_key(inlist, key, default, reverse=True):
    """ Returns a sorted list by the rating attribute.
    """
    inlist = [_sanitize_me(i, key, default) for i in inlist]
    opkey = operator.attrgetter(key)
    return sorted(inlist, key=opkey, reverse=reverse)


def sort_list_by_lang(inlist, languages, other_key=None):
    """ Returns a sorted list by given languages.
        The items of the inlist must have an attribute 'language'.
        Equal items are sorted by the 'other_key' attribute.
    """
    outlist = []
    for lang in languages:
        # sorting "==" puts the desired lang at the end of the sorted list!
        inlist.sort(key=lambda x: x.language == lang, reverse=False)
        if other_key:
            for i, tmp in enumerate(inlist):
                if tmp.language == lang:
                    ranking_list = sort_list_by_key(inlist[i:], other_key, 0)
                    outlist.extend(ranking_list)
                    del inlist[i:]
                    break
    outlist.extend(inlist)
    return outlist


def _name_match_quality(name, tvtitle):
    distance = levenshtein(name.lower(), tvtitle.lower())
    if len(tvtitle) > len(name):
        match_quality = float(len(tvtitle) - distance) / len(tvtitle)
    else:
        match_quality = float(len(name) - distance) / len(name)
    return match_quality


class People(IntEnum):
    """
    Static definitions of 'people' type according API function
    'getAllPeopleTypes' to speed up queries.
    """
    Director           =  1
    Writer             =  2
    Actor              =  3
    Guest_Star         =  4
    Crew               =  5
    Creator            =  6
    Producer           =  7
    Showrunner         =  8
    Musical_Guest      =  9
    Host               = 10
    Executive_Producer = 11


class Myth4TTVDBv4(object):
    """
    MythTV Bindings for TheTVDB v4.
    This class implements the following grabber options by means of :
        -N inetref subtitle :      'def buildNumbers()'
        -N title subtitle :        'def buildNumbers()'
        -D inetref season episode: 'def buildSingle()'
        -C collectionref:          'def buildCollection()'
        -M title:                  'def buildList()'
    It handles all the translations and name matching functionality.
    """

    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)
            # print("Init : ", k,v)

        self.starttime = time.time()

        # Authorization:
        TTVDBv4Key = '708401c2-b73e-4ce0-97ec-8ef3a5038c3a'
        TTVDBv4Pin = None
        # get authorization data from local config
        try:
            TTVDBv4Key = self.config['Authorization']['TTVDBv4Key']
            TTVDBv4Pin = self.config['Authorization']['TTVDBv4Pin']
        except KeyError:
            pass
        auth_payload = {"apikey": TTVDBv4Key,
                        "pin": TTVDBv4Pin,
                        }

        # get the lifetime of the access token from local config
        try:
            self.token_lifetime = int(self.config['AccessToken']['Lifetime'])
        except:
            self.token_lifetime = 1
        if self.debug:
            print("%04d: Init: Using this lifetime for the access token: '%d' day(s)."
                  % (self._get_ellapsed_time(), self.token_lifetime))

        # prepare list of preferred languages
        input_language = Language.getstored(self.language).ISO639_2T
        self.languagelist = [input_language]
        try:
            for k in sorted(self.config['Languages'].keys()):
                v = Language.getstored(self.config['Languages'][k]).ISO639_2T
                if v != input_language:
                    self.languagelist.append(v)
        except:
            # considered as nice to have
            pass
        if self.debug:
            print("%04d: Init: Using these languages to search: '%s'"
                  % (self._get_ellapsed_time(), " ".join(self.languagelist)))

        # get preferences:
        for pref in self.config['Preferences'].keys():
            try:
                if self.config['Preferences'][pref].lower() in ('1', 'yes'):
                    setattr(self, pref, True)
                else:
                    setattr(self, pref, False)
            except:
                # considered a nice to have
                setattr(self, pref, False)
        if self.debug:
            print("%04d: Init: Using these preferences to search:"
                  % self._get_ellapsed_time())
            for pref in self.config['Preferences'].keys():
                print("  '%s' : '%s'" % (pref, getattr(self, pref)))

        # get thresholds:
        for t in ('NameThreshold', 'ThresholdStart', 'ThresholdDecrease'):
            try:
                setattr(self, t, float(self.config['Thresholds'][t]))
            except:
                self.NameThreshold = 0.99
                self.ThresholdStart = 0.60
                self.ThresholdDecrease = 0.1
        if self.debug:
            print("%04d: Init: Using these thresholds for name searching:"
                  % self._get_ellapsed_time())
            for t in ('NameThreshold', 'ThresholdStart', 'ThresholdDecrease'):
                print("  '%s' : '%0.2f'" % (t, getattr(self, t)))

        # get title conversions and inetref conversions
        if self.debug:
            print("%04d: Init: Title and Inetref Conversions:"
                  % self._get_ellapsed_time())
            for k in self.config['TitleConversions'].keys():
                print("  %s: %s" % (k, self.config['TitleConversions'][k]))
            for k in self.config['InetrefConversions'].keys():
                print("  %s: %s" % (k, self.config['InetrefConversions'][k]))
            print("  Done")

        # read cached authorization token
        if self.debug:
            print("%04d: Init: Bearer Authentication:"
                  % self._get_ellapsed_time())
        auth_tuple = tuple()
        auth_pfile = self.config.get('auth_file')
        if auth_pfile and os.path.isfile(auth_pfile):
            try:
                with open(self.config['auth_file'], 'rb') as f:
                    auth_tuple = pickle.load(f)
            except:
                if self.debug:
                    print("  Reading cache-authentication file failed.")
        # validate authorization token (valid one day)
        token = None
        if auth_tuple:
            try:
                auth_time = auth_tuple[0]
                ptoken = auth_tuple[1]
                if ptoken and \
                        auth_time + timedelta(days=self.token_lifetime) > datetime.now():
                    if self.debug:
                        print("  Cached authentication token is valid.")
                    token = ptoken
                else:
                    if self.debug:
                        print("  Cached authentication token is invalid.")
                # print(token)
            except:
                if self.debug:
                    print("  Reading cached authentication failed.")

        # start html session, re-use token if cached
        self.session = requests.Session()
        if not token or self.debug:
            r = self.session.post('https://api4.thetvdb.com/v4/login', json=auth_payload)
            r_json = r.json()
            # if self.debug:
            #    print(r_json)
            error = r_json.get('message')
            if error:
                if error == 'Unauthorized':
                    print("tvdb_notauthorized: Error occured")
                    sys.exit(1)
            status = r_json.get('status')
            if status:
                if status != 'success':
                    print("tvdb_notauthorized: Wrong status")
                    sys.exit(1)
            try:
                token = "Bearer %s" % (r_json['data'].get('token'))
                # print(token)
            except:
                print("tvdb_notauthorized: No token")
                sys.exit(1)
            if self.debug:
                print("  Bearer Authentication passed with '%s'." % status)

            if auth_pfile:
                now = datetime.now()
                auth_tuple = (now, token)
                try:
                    with open(auth_pfile, 'wb') as f:
                        pickle.dump(auth_tuple, f, -1)
                except:
                    if self.debug:
                        print("  Writing cache-authentication file failed.")

        self.session.headers.update({'Accept': 'application/json',
                                     'Accept-Language': self.language,
                                     'User-Agent': 'mythtv.org ttvdb v4 grabber',
                                     'Authorization': "%s" % (token)
                                     })
        # set the session for the tvbd4 api, enable json debug logs
        ttvdb.set_jsondebug(self.jsondebug)
        ttvdb.set_session(self.session)

        # prepare the resulting xml tree
        self.tree = etree.XML(u'<metadata></metadata>')

    def _get_ellapsed_time(self):
        return (int(time.time() - self.starttime))

    def _select_preferred_langs(self, languages):
        pref_langs = []
        for lang in self.languagelist:
            if lang in languages:
                pref_langs.append(lang)
        return pref_langs

    def _get_title_conversion(self, title):
        try:
            if title in self.config['TitleConversions'].keys():
                return self.config['TitleConversions'][title]
            else:
                return title
        except:
            return title

    def _get_inetref_conversion(self, title):
        try:
            if str(title) in self.config['InetrefConversions'].keys():
                return int(self.config['InetrefConversions'][str(title)])
            else:
                return title
        except:
            return title

    def _split_title(self, title):
        """ separate trailing ' (year)' from title """
        rs = r'(?P<rtitle>.*) \((?P<ryear>(?:19|20)[0-9][0-9])\)$'
        match = re.search(rs, title)
        try:
            title_wo_year = match.group('rtitle')
            year = match.group('ryear')
        except:
            year = None
            title_wo_year = title
        return (title_wo_year, year)

    def _get_names_translated(self, translations):
        name_dict = OrderedDict()
        for lang in self.languagelist:
            if translations.get(lang) is not None:
                name_dict[lang] = translations.get(lang)
        return name_dict

    def _get_info_from_translations(self, translations):
        desc = ""
        name = ""
        name_found = False
        for lang in self.languagelist:
            try:
                for t in translations:
                    try:
                        if lang == t.language:
                            if not name_found:
                                name = t.name
                                name_found = True
                            desc = t.overview
                            if name_found and desc:
                                raise StopIteration
                    except (KeyError, ValueError):
                        continue
            except StopIteration:
                break
        return (name, desc)

    def _get_crew_for_xml(self, crew_list, crew_type, list_character=False):
        crew_dict_list = []
        for c in crew_list:
            try:
                if c.personName:
                    d = {'name': c.personName, 'job': '%s' % crew_type,
                             'url': 'https://thetvdb.com/people/%s' % c.peopleId}
                    if list_character and c.name:
                        d['character'] = c.name
                    crew_dict_list.append(d)
            except:
                # considered as nice to have
                pass
        return crew_dict_list

    def _format_xml(self, ser_x, sea_x=None, epi_x=None):
        m = VideoMetadata()
        m.inetref = check_item(m, ("inetref", str(ser_x.id)), ignore=False)
        # try to get title and description for the preferred language list:
        # note: there could be a title but no description:
        #       $ ttvdb4.py -l ja -C 360893 --debug

        # get series name and overview:
        ser_title, desc = self._get_info_from_translations(ser_x.fetched_translations)
        if not ser_title:
            ser_title = ser_x.name
        m.title = check_item(m, ("title", ser_title), ignore=False)

        # get subtitle and overview:
        if epi_x:
            sub_title, sub_desc = self._get_info_from_translations(epi_x.fetched_translations)
            if not sub_title:
                sub_title = epi_x.name
            m.subtitle = check_item(m, ("subtitle", sub_title), ignore=False)
            if sub_desc:
                desc = sub_desc
            m.season = check_item(m, ("season", epi_x.seasonNumber), ignore=False)
            m.episode = check_item(m, ("episode", epi_x.number), ignore=False)
            m.runtime = check_item(m, ("runtime", epi_x.runtime), ignore=True)
            if epi_x.aired:
                # convert string to 'date' object, assuming ISO naming 
                m.releasedate = convert_date(epi_x.aired[:10])

        desc = strip_tags(desc).replace("\r\n", "").replace("\n", "")
        m.description = check_item(m, ("description", desc))

        try:
            if ser_x.originalLanguage:
                lang = ser_x.originalLanguage
            else:
                lang = self.language
            if ser_x.originalCountry:
                country = ser_x.originalCountry
            else:
                country = ser_x.country
            m.language = check_item(m, ("language", Language.getstored(lang).ISO639_1))
            if country:
                m.countries.append(country.upper())  # could be 'None'
            m.year = check_item(m, ('year', int(ser_x.firstAired.split('-')[0])))
            m.userrating = check_item(m, ('userrating', ser_x.score))
            for h in [x.id for x in ser_x.remoteIds if x.type == 4]:
                # type 4 is 'Official Website'
                m.homepage = check_item(m, ('homepage', h))
                # MythTV supports only one entry
                break
        except:
            # considered as nice to have
            pass

        # add categories:
        try:
            for genre in ser_x.genres:
                if genre.name:
                    m.categories.append(genre.name)
        except:
            pass

        # add optional fields:
        if epi_x:
            try:
                # add studios
                for c in epi_x.companies:
                    m.studios.append(c.name)
            except:
                pass
            try:
                # add IMDB reference
                for r in epi_x.remoteIds:
                    if r.sourceName == 'IMDB':
                        m.imdb = check_item(m, ('imdb', r.id))
                        break
            except:
                raise
            if self.country:
                try:
                    # add certificates:
                    area = Country.getstored(self.country).alpha3
                    for c in epi_x.contentRatings:
                        if c.country.lower() == area.lower():
                            m.certifications[self.country] = c.name
                            break
                except:
                    pass

        if self.ShowPeople:
            # add characters: see class 'People'
            # characters of type 'Actor' are sorted in ascending order
            actors = [c for c in ser_x.characters if c.type == People.Actor]
            actors_sorted = sort_list_by_key(actors, "sort", 99, reverse=False)
            # prefer actors that are sorted, i.e.: 'sort' > 0
            actors_s_1 = [x for x in actors_sorted if x.sort > 0]
            # append the rest, i.e.: actors with sort == 0
            actors_s_1.extend([x for x in actors_sorted if x.sort == 0])
            m.people.extend(self._get_crew_for_xml(actors_s_1, 'Actor', list_character=True))

            # on episodes, characters of type 'Guest Star' are sorted in ascending order
            if epi_x:
                guests = [c for c in epi_x.characters if c.type == People.Guest_Star]
                guests_sorted = sort_list_by_key(guests, "sort", 99, reverse=False)
                m.people.extend(self._get_crew_for_xml(guests_sorted, 'Guest Star'))

                directors = [c for c in epi_x.characters if c.type == People.Showrunner]
                directors_sorted = sort_list_by_key(directors, "sort", 99, reverse=False)
                m.people.extend(self._get_crew_for_xml(directors_sorted, 'Director'))

        # no we have all extended records for series, season, episode, create xml for them
        series_banners = []; season_banners = []
        series_posters = []; season_posters = []
        series_fanarts = []; season_fanarts = []

        # add the artworks, season preferred
        #        art_name     what        art_type(s)   from_r / from_a
        arts = [('coverart', season_posters, (7,),   sea_x, sea_x.artwork if sea_x else []),
                ('coverart', series_posters, (2,),   ser_x, ser_x.artworks),
                ('fanart',   season_fanarts, (8, 9), sea_x, sea_x.artwork if sea_x else []),
                ('fanart',   series_fanarts, (3, 4), ser_x, ser_x.artworks),
                ('banner',   season_banners, (6,),   sea_x, sea_x.artwork if sea_x else []),
                ('banner',   series_banners, (1,),   ser_x, ser_x.artworks),
                ]
        # avoid duplicates
        used_urls = []
        for (art_name, what, art_types, from_r, from_a) in arts:
            artlist = [art for art in from_a if art.type in art_types]
            what.extend(sort_list_by_lang(artlist, self.languagelist, other_key='score'))
            for entry in what:
                try:
                    if entry.image not in used_urls:
                        used_urls.append(entry.image)
                        m.images.append({'type': art_name, 'url': entry.image,
                                         'thumb': entry.thumbnail})
                except:
                    pass
        if epi_x and epi_x.imageType in (11, 12):
            m.images.append({'type': 'screenshot', 'url': epi_x.image})

        self.tree.append(m.toXML())

    def buildSingle(self):
        """
        The ttvdb api returns different id's for series, season, and episode.
        MythTV stores only the series-id, therefore we need to fetch the correct id's
        for season and episode for that series-id.
        """
        # option -D inetref season episode
        # $ ttvdb4.py -D 76568 2 8 --debug
        # $ ttvdb4.py -l de -D 76568 2 8 --debug

        if self.debug:
            print("\n%04d: buildSingle: Query 'buildSingle' called with '%s'"
                  % (self._get_ellapsed_time(), " ".join([str(x) for x in self.tvdata])))
        inetref = self.tvdata[0]
        season = self.tvdata[1]
        episode = self.tvdata[2]

        # get series data:
        ser_x = ttvdb.getSeriesExtended(inetref)

        ser_x.fetched_translations = []
        for lang in self._select_preferred_langs(ser_x.nameTranslations):
            translation = ttvdb.getSeriesTranslation(inetref, lang)
            ser_x.fetched_translations.append(translation)

        if self.debug:
            print("%04d: buildSingle: Series information for %s:"
                  % (self._get_ellapsed_time(), inetref))
            _print_class_content(ser_x)

        gen_episodes = ttvdb.getSeriesEpisodes(inetref, season_type='default', season=season,
                                               episodeNumber=episode, yielded=True)
        ep = next(gen_episodes)
        epi_x = ttvdb.getEpisodeExtended(ep.id)
        for lang in self._select_preferred_langs(epi_x.nameTranslations):
            translation = ttvdb.getEpisodeTranslation(epi_x.id, lang)
            epi_x.fetched_translations.append(translation)
        if self.debug:
            print("%04d: buildSingle: Episode Information for %s : %s : %s"
                  % (self._get_ellapsed_time(), inetref, season, episode))
            _print_class_content(epi_x)

        # get season information:
        sea_x = None
        for s in epi_x.seasons:
            if s.type.id == ser_x.defaultSeasonType:
                sea_x = ttvdb.getSeasonExtended(s.id)
                if self.debug:
                    print("%04d: buildSingle: Season Information for %s : %s"
                          % (self._get_ellapsed_time(), inetref, season))
                    _print_class_content(sea_x)
                break

        # no we have all extended records for series, season, episode, create xml for them
        self._format_xml(ser_x, sea_x, epi_x)

    def buildCollection(self, other_inetref=None, xml_output=True):
        """
        Creates a single extendedSeriesRecord matching the 'inetref' provided by the
        command line.
        If xml_output requested, update the common xml data, otherwise return this record.
        The 'other_inetref' option overrides the default 'inetref' from the command line.
        """
        # option -C inetref
        # $ ttvdb4.py -l en -C 360893 --debug
        # $ ttvdb4.py -l de -C 360893 --debug
        # $ ttvdb4.py -l ja -C 360893 --debug  (missing Japanese overview)
        # $ ttvdb4.py -l fr -C 76568 --debug

        if other_inetref:
            tvinetref = other_inetref
        else:
            tvinetref = self.collectionref[0]

        if self.debug:
            print("\n%04d: buildCollection: Query 'buildCollection' called with "
                  "'%s', xml_output = %s"
                  % (self._get_ellapsed_time(), tvinetref, xml_output))

        # get data for passed inetref and preferred translations
        ser_x = ttvdb.getSeriesExtended(tvinetref)

        ser_x.fetched_translations = []
        for lang in self._select_preferred_langs(ser_x.nameTranslations):
            translation = ttvdb.getSeriesTranslation(tvinetref, lang)
            ser_x.fetched_translations.append(translation)

        # define exact name found:
        ser_x.name_similarity = 1.0

        if self.debug:
            print("%04d: buildCollection: Series information for %s:"
                  % (self._get_ellapsed_time(), tvinetref))
            _print_class_content(ser_x)

        if xml_output:
            self._format_xml(ser_x)

        return ser_x

    def buildList(self, other_title=None, xml_output=True, get_all=False):
        """
        Creates a list of extendedSeriesRecords matching the 'title' provided by the
        command line.
        If xml_output requested, update the common xml data, and return this list.
        The 'other_title' option overrides the default 'title' provided by the command line.
        Calls 'buildCollection(other_inetref=<inetref_matching_title>, xml_output=xml_output)
        for each record of the list.
        Stops on searching if 'get_all' is 'False' and an exact match was found.
        Returns a sorted list according preferred languages and 'name_similarity'.
        """
        # option -M title
        # this works on:
        # $ ttvdb4.py -l de -M "Chernobyl" --debug  --> exact match
        # $ ttvdb4.py -l en -M "Chernobyl" --debug  --> exact match  (inetref 360893)
        # $ ttvdb4.py -l de -M "Chernobyl:" --debug  --> multiple matches
        # $ ttvdb4.py -l de -M "Die Munsters"  --> single match
        # $ ttvdb4.py -l en -M "The Munsters"  --> single match
        # $ ttvdb4.py -l de -M "Hawaii Five-0" --> single match    ---> inetref  164541
        # $ ttvdb4.py -l de -M "Hawaii Five-O" --> multiple matches (71223 and 164541)
        # $ ttvdb4.py -l en -M "Hawaii Five-0 (2010)" --debug  ---> inetref 164541
        # $ ttvdb4.py -l el -M "Star Trek Discovery" ok (see pull request #180)
        # $ ttvdb4.py -l en -M "Marvel's Agents of S H I E L D" returns nothing, see #247
        #                       see override in ttvdbv4.ini

        if other_title:
            tvtitle = other_title
        else:
            # take care on conversions defined in the ttvdb4.ini file
            tvtitle = self._get_title_conversion(self.tvtitle[0])

        if self.debug:
            print("\n%04d: buildList: Query 'buildList' called with '%s', xml_output = %s"
                  % (self._get_ellapsed_time(), tvtitle, xml_output))

        # separate trailing ' (year)' from title
        title_wo_year, year = self._split_title(tvtitle)

        # get series overview
        so = ttvdb.getSearchResults(query=title_wo_year, type='series', year=year)
        if not so:
            return
        exact = False
        series = []
        for item in so:
            # check if we can find exact match for 'tvtitle'
            translated_names = self._get_names_translated(item.translations)
            # ttvdb returns sometimes '<tvtitle> (<year>)' like "The Forsyte Saga (2002)"
            translated_names_wo_year = \
                            [self._split_title(x)[0] for x in translated_names.values()]
            if (item.name == tvtitle or item.name == title_wo_year or
                    tvtitle in item.aliases or
                    tvtitle in translated_names.values() or
                    tvtitle in translated_names_wo_year or
                    title_wo_year in item.aliases or
                    title_wo_year in translated_names.values()):
                item.name_similarity = 1.0
                series.append(item)
                exact = True
        if exact:
            if self.debug:
                te = self._get_ellapsed_time()
                if len(series) == 1:
                    print("%04d: buildList: Found unique match for '%s': %s"
                          % (te, tvtitle, series[0].tvdb_id))
                else:
                    print("%04d: buildList: Found multiple exact matches for '%s':"
                          % (te, tvtitle))
                    for i in series:
                        print("    %s" % i.tvdb_id)
        if get_all or not exact:
            # if no exact match was found, append all series for given languages:
            if self.debug and not exact:
                print("%04d: buildList: Found no exact match for '%s', return a sorted list."
                      % (self._get_ellapsed_time(), tvtitle))
            for item in so:
                if item in series:
                    continue
                # select only items with preferred languages and calculate name similarity
                all_names = [item.name]
                all_names.extend(item.aliases)
                all_names.extend(self._get_names_translated(item.translations).values())
                item.name_similarity = \
                            max([_name_match_quality(x, title_wo_year) for x in all_names])
                series.append(item)

        # sort the list by name similarity
        series = sort_list_by_key(series, 'name_similarity', 0.0)
        ser_x_list = []
        for s in series:
            if self.debug:
                print("\n%04d: buildList: Chosen series record according "
                      "'overall name similarity': '%0.2f':"
                      % (self._get_ellapsed_time(), s.name_similarity))
            try:
                if s.tvdb_id:     # sometimes, this is empty
                    sid = s.tvdb_id
                elif s.id:
                    sid = s.id.split('-')[1]
                else:
                    sid = None
                ser_x = self.buildCollection(other_inetref=int(sid), xml_output=False)
                # re-use name similarity from SearchResult
                ser_x.name_similarity = s.name_similarity
                ser_x_list.append(ser_x)
                if self.debug:
                    print("    ", "name_similarity", " : ", "%0.2f" % s.name_similarity)
                if xml_output:
                    self._format_xml(ser_x)
            except:
                # ser_x could be 'None'
                pass

        return ser_x_list

    def buildNumbers(self):
        """
        If option -N inetref subtitle:
        calls 'buildCollection(other_inetref=inetref, xml_output=False)'
                                                       --> single extendedSeriesRecord
        If option -N title subtitle:
        calls 'buildList(other_title=title, xml_output=False)'
                                                       --> list of extendedSeriesRecords
        Then checks the most matching records for valid subtitle.
        """
        # either option -N inetref subtitle
        # or  option -N title subtitle     # note: title may include a trailing ' (year)'
        # or  option -N title timestamp    ### XXX ToDo implement me
        # this may take several minutes
        # $ ttvdb4.py -l de -N 76568 "Emily in Nöten" --debug
        # $ ttvdb4.py -l en -a us -N 76568 "The Road Trip to Harvard" --debug
        # $ ttvdb4.py -l de -N "Die Munsters" "Der Liebestrank" --debug
        # $ ttvdb4.py -l de -N "Hawaii Five-0" "Geflügelsalat" --debug
        # $ ttvdb4.py -l en -N "The Munsters" 'My Fair Munster' --debug
        # $ ttvdb4.py -l en -N "The Forsyte Saga (2002)" "Episode 1" --debug --> multiple episodes
        # $ ttvdb4.py -l en -N "The Forsyte Saga" "A Silent Wooing" --debug
        # $ ttvdb4.py -l en -N "The Forsyte Saga" "Episode 1" --debug --> multiple episodes
        # $ ttvdb4.py -l en -N "The Forsyte Saga (2002)" "A Silent Wooing" --debug  --> no match
        # $ ttvdb4.py -l de -N "Es war einmal... Das Leben" "Ein ganz besonderer Saft / Das Blut" --> single match
        # $ ttvdb4.py -l de -N "Es war einmal Das Leben" "Ein ganz besonderer Saft / Das Blut" --> single match
        # $ ttvdb4.py -l de -N "Es war einmal Das Leben" "Das Blut" --> no matches
        # $ ttvdb4.py -l en -N "Once Upon a Time Life" "The Blood" --> single match
        # $ ttvdb4.py -l en -N "Marvel's Agents of S H I E L D" "Shadows"   only with override in ttvdb4.ini
        # $ ttvdb4.py -l en -N "Eleventh Hour" "Frozen" works with and without override in ttvdb4.ini

        if self.debug:
            print("\n%04d: buildNumbers: Query 'buildNumbers' called with '%s' '%s'"
                  % (self._get_ellapsed_time(), self.tvnumbers[0], self.tvnumbers[1]))
        # ToDo:
        # below check shows a deficiency of the MythTV grabber API itself:
        # TV-Shows or Movies with an integer as title are not recognized correctly.
        # see https://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
        # and https://code.mythtv.org/trac/ticket/11850

        # take care on conversions through the ttvdb.ini file
        converted_inetref = self._get_inetref_conversion(self.tvnumbers[0])
        try:
            inetref = int(converted_inetref)
            tvsubtitle = self.tvnumbers[1]
            ser_x_list = [self.buildCollection(other_inetref=inetref, xml_output=False)]
        except ValueError:
            ### XXX ToDo: implement datetime as option for subtitle
            tvtitle = self._get_title_conversion(self.tvnumbers[0])
            tvsubtitle = self.tvnumbers[1]
            inetref = None
            ser_b_list = self.buildList(other_title=tvtitle, xml_output=False, get_all=True)
            # reduce list according name similarity
            if ser_b_list:
                ser_x_list = [x for x in ser_b_list if x.name_similarity > self.ThresholdStart]
            else:
                ser_x_list = []
        # loop over the list and calculate name_similarity of series and episode
        found_items = []
        for ser_x in ser_x_list:
            if self.debug and ser_x.fetched_translations:
                print("%04d: buildNumbers: Checking series named '%s' with inetref '%s':"
                      % (self._get_ellapsed_time(), ser_x.fetched_translations[0].name, ser_x.id))
            # get all episodes for every season, and calculate the name similarity
            episodes = []
            # get the default season type and search only for that:
            def_season_type = ser_x.defaultSeasonType
            for sea_id in [x.id for x in ser_x.seasons if x.type.id == def_season_type]:
                sea_x = ttvdb.getSeasonExtended(sea_id)
                epi_id_list = []
                if sea_x is None:
                    continue
                for epi in sea_x.episodes:
                    # an episode may be listed multiple times, include it only once:
                    # ### XXX Note: this is a bug in TVDBv4 API
                    if epi.id not in epi_id_list:
                        all_names = []
                        for lang in self._select_preferred_langs(epi.nameTranslations):
                            translation = ttvdb.getEpisodeTranslation(epi.id, lang)
                            if translation is None:
                                continue
                            all_names.append(translation.name)
                            all_names.extend(translation.aliases)
                            epi.name_similarity = \
                                max([_name_match_quality(x, tvsubtitle) for x in all_names])
                            epi.fetched_translations.append(translation)
                        episodes.append(epi)
                        epi_id_list.append(epi.id)
                        if self.debug:
                            print("%04d: buildNumbers: Found episode names: '%s'"
                                  % (self._get_ellapsed_time(),
                                     '; '.join([n.name for n in epi.fetched_translations])))
                            print("    with name similarity : %0.2f" % epi.name_similarity)
                            print("    with inetref %s : season# %d : episode# %d "
                                  % (ser_x.id, epi.seasonNumber, epi.number))

            # sort the list by name_similarity, generate xml output for max. 10 items
            sorted_episodes = sort_list_by_key(episodes, 'name_similarity', 0.0)
            for epi in sorted_episodes[:10]:
                # apply minimum threshold for name similarity
                if epi.name_similarity < self.ThresholdStart:
                    break
                # get episode and season data for that episode
                epi_x = ttvdb.getEpisodeExtended(epi.id)
                # append the translations already collected:
                epi_x.fetched_translations = epi.fetched_translations
                epi_x.name_similarity = epi.name_similarity

                sea_x = None
                for s in epi_x.seasons:
                    if s.type.id == ser_x.defaultSeasonType:
                        sea_x = ttvdb.getSeasonExtended(s.id)
                        break
                if self.debug:
                    print("%04d: buildNumbers: Selected episode names: '%s'"
                          % (self._get_ellapsed_time(),
                             '; '.join([n.name for n in epi_x.fetched_translations])))
                    print("    with name similarity : %0.2f" % epi.name_similarity)

                found_items.append((ser_x, sea_x, epi_x))

        # now sort the found_items list and create xml:
        found_items.sort(reverse=True,
                         key=lambda x: x[0].name_similarity + x[2].name_similarity)

        exact_match_count = 0
        last_similarity = 2 * self.ThresholdStart
        for (ser_x, sea_x, epi_x) in found_items:
            overall_similarity = epi_x.name_similarity + ser_x.name_similarity
            if self.debug and ser_x.fetched_translations:
                print("%04d: buildNumbers: Found item series: '%s', season: '%s', "
                      "episode: '%s' with overall similarity: %0.2f"
                      % (self._get_ellapsed_time(), ser_x.fetched_translations[0].name,
                         sea_x.number, epi_x.fetched_translations[0].name, overall_similarity))
            # stop if match similarity decreases
            if overall_similarity > 2 * self.NameThreshold:      # default 1.98
                exact_match_count += 1
            if overall_similarity > last_similarity:
                last_similarity = overall_similarity
            elif exact_match_count and \
                 abs(last_similarity - overall_similarity) > self.ThresholdDecrease:
                if self.debug:
                    print("        canceling output due to decrease of 'name similarity'.")
                break

            self._format_xml(ser_x, sea_x, epi_x)
