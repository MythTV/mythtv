# -*- coding: UTF-8 -*-

# ----------------------------------------------------
# Purpose:   MythTV Python Bindings for TheTVDB v4 API
# Copyright: (c) 2021 Roland Ernst
# License:   GPL v2 or later, see LICENSE for details
# ----------------------------------------------------


import sys
from requests import codes as requestcodes
if 0:
    from urllib.parse import urlencode, quote_plus, quote
from pprint import pprint

from .definitions import *


MYTHTV_TTVDBV4_API_VERSION = "4.5.4.0"

# set this to true for showing raw json data
#JSONDEBUG = True
JSONDEBUG = False

# Set an open requests session here
ReqSession = None

def set_jsondebug(b):
    global JSONDEBUG
    JSONDEBUG = bool(b)

def set_session(s):
    global ReqSession
    #print("set_session called with %s" % type(s))
    ReqSession = s


def _query_api(url, params=None):
    global ReqSession
    if JSONDEBUG:
        print("Params: ", params)
    if 0:
        # python requests encodes everything with 'quote_plus"
        # thetvdb api may need '%20' instead of '+' for a space character
        if params:
            url = '{0}?{1}'.format(url, urlencode(params, safe='', quote_via=quote))
        res = ReqSession.get(url)
    else:
        res = ReqSession.get(url, params=params)

    if res is None:
        return {}
    if JSONDEBUG:
        print(res.url)
        print(res.request.headers)
    if res.status_code != requestcodes.OK:
        if JSONDEBUG:
            print('http request was unsuccessful: ' + str(res.status_code), res.reason, res.url)
            #sys.exit(1)
        return {}
    # res is a dictionary with 'data', 'status' and 'links' dicts
    resjson = res.json()
    if JSONDEBUG:
        print("Successful http request '%s':" % res.url)
        pprint(resjson)
    return resjson


def _query_yielded(record, path, params, listname=None):
    # do not trust the 'links' section in the response
    # simply loop until no data are provided
    curr_page = int(params['page'])
    while True:
        res = _query_api(path, params)
        # check for 'success'
        if res is not None and res.get('data') is not None and \
            res.get('status') is not None and res['status'] == 'success':
            if listname:
                datalist = res['data'][listname]
            else:
                datalist = res['data']
            if datalist:
                for item in datalist:
                    yield record(item)
            else:
                raise StopIteration
        else:
            #break
            raise StopIteration
        curr_page += 1
        params['page'] = curr_page


"""Generated API for thetvdb.com TVDB API V4 v 4.5.4"""
# modifications marked with '### XXX'


TTVDBV4_path = "https://api4.thetvdb.com/v4"
getArtworkBase_path = TTVDBV4_path + "/artwork/{id}"
getArtworkExtended_path = TTVDBV4_path + "/artwork/{id}/extended"
getAllArtworkStatuses_path = TTVDBV4_path + "/artwork/statuses"
getAllArtworkTypes_path = TTVDBV4_path + "/artwork/types"
getAllAwards_path = TTVDBV4_path + "/awards"
getAward_path = TTVDBV4_path + "/awards/{id}"
getAwardExtended_path = TTVDBV4_path + "/awards/{id}/extended"
getAwardCategory_path = TTVDBV4_path + "/awards/categories/{id}"
getAwardCategoryExtended_path = TTVDBV4_path + "/awards/categories/{id}/extended"
getCharacterBase_path = TTVDBV4_path + "/characters/{id}"
getAllCompanies_path = TTVDBV4_path + "/companies"
getCompanyTypes_path = TTVDBV4_path + "/companies/types"
getCompany_path = TTVDBV4_path + "/companies/{id}"
getAllContentRatings_path = TTVDBV4_path + "/content/ratings"
getAllCountries_path = TTVDBV4_path + "/countries"
getEntityTypes_path = TTVDBV4_path + "/entities"
getEpisodeBase_path = TTVDBV4_path + "/episodes/{id}"
getEpisodeExtended_path = TTVDBV4_path + "/episodes/{id}/extended"
getEpisodeTranslation_path = TTVDBV4_path + "/episodes/{id}/translations/{language}"
getAllGenders_path = TTVDBV4_path + "/genders"
getAllGenres_path = TTVDBV4_path + "/genres"
getGenreBase_path = TTVDBV4_path + "/genres/{id}"
getAllInspirationTypes_path = TTVDBV4_path + "/inspiration/types"
getAllLanguages_path = TTVDBV4_path + "/languages"
getAllLists_path = TTVDBV4_path + "/lists"
getList_path = TTVDBV4_path + "/lists/{id}"
getListExtended_path = TTVDBV4_path + "/lists/{id}/extended"
getListTranslation_path = TTVDBV4_path + "/lists/{id}/translations/{language}"
getAllMovie_path = TTVDBV4_path + "/movies"
getMovieBase_path = TTVDBV4_path + "/movies/{id}"
getMovieExtended_path = TTVDBV4_path + "/movies/{id}/extended"
getMovieTranslation_path = TTVDBV4_path + "/movies/{id}/translations/{language}"
getAllMovieStatuses_path = TTVDBV4_path + "/movies/statuses"
getPeopleBase_path = TTVDBV4_path + "/people/{id}"
getPeopleExtended_path = TTVDBV4_path + "/people/{id}/extended"
getPeopleTranslation_path = TTVDBV4_path + "/people/{id}/translations/{language}"
getAllPeopleTypes_path = TTVDBV4_path + "/people/types"
getSearchResults_path = TTVDBV4_path + "/search"
getAllSeasons_path = TTVDBV4_path + "/seasons"
getSeasonBase_path = TTVDBV4_path + "/seasons/{id}"
getSeasonExtended_path = TTVDBV4_path + "/seasons/{id}/extended"
getSeasonTypes_path = TTVDBV4_path + "/seasons/types"
getSeasonTranslation_path = TTVDBV4_path + "/seasons/{id}/translations/{language}"
getAllSeries_path = TTVDBV4_path + "/series"
getSeriesBase_path = TTVDBV4_path + "/series/{id}"
getSeriesArtworks_path = TTVDBV4_path + "/series/{id}/artworks"
getSeriesExtended_path = TTVDBV4_path + "/series/{id}/extended"
getSeriesEpisodes_path = TTVDBV4_path + "/series/{id}/episodes/{season_type}"
getSeriesSeasonEpisodesTranslated_path = TTVDBV4_path + "/series/{id}/episodes/{season_type}/{lang}"
getSeriesTranslation_path = TTVDBV4_path + "/series/{id}/translations/{language}"
getAllSeriesStatuses_path = TTVDBV4_path + "/series/statuses"
getAllSourceTypes_path = TTVDBV4_path + "/sources/types"
updates_path = TTVDBV4_path + "/updates"


def getArtworkBase(id):
    path = getArtworkBase_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( ArtworkBaseRecord(data) if data is not None else None )


def getArtworkExtended(id):
    path = getArtworkExtended_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( ArtworkExtendedRecord(data) if data is not None else None )


def getAllArtworkStatuses():
    path = getAllArtworkStatuses_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [ArtworkStatus(x) for x in data] if data is not None else [] )


def getAllArtworkTypes():
    path = getAllArtworkTypes_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [ArtworkType(x) for x in data] if data is not None else [] )


def getAllAwards():
    path = getAllAwards_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [AwardBaseRecord(x) for x in data] if data is not None else [] )


def getAward(id):
    path = getAward_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( AwardBaseRecord(data) if data is not None else None )


def getAwardExtended(id):
    path = getAwardExtended_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( AwardExtendedRecord(data) if data is not None else None )


def getAwardCategory(id):
    path = getAwardCategory_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( AwardCategoryBaseRecord(data) if data is not None else None )


def getAwardCategoryExtended(id):
    path = getAwardCategoryExtended_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( AwardCategoryExtendedRecord(data) if data is not None else None )


def getCharacterBase(id):
    path = getCharacterBase_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( Character(data) if data is not None else None )


def getAllCompanies(page=0, yielded=False):
    params = {}
    if page is not None:
        params['page'] = page
    path = getAllCompanies_path.format()
    if yielded:
        return _query_yielded(Company, path, params, listname=None)
    else:
        res = _query_api(path, params)
        data = res['data'] if res.get('data') is not None else None
        return( [Company(x) for x in data] if data is not None else [] )


def getCompanyTypes():
    path = getCompanyTypes_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [CompanyType(x) for x in data] if data is not None else [] )


def getCompany(id):
    path = getCompany_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( Company(data) if data is not None else None )


def getAllContentRatings():
    path = getAllContentRatings_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [ContentRating(x) for x in data] if data is not None else [] )


def getAllCountries():
    path = getAllCountries_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [Country(x) for x in data] if data is not None else [] )


def getEntityTypes():
    path = getEntityTypes_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [EntityType(x) for x in data] if data is not None else [] )


def getEpisodeBase(id):
    path = getEpisodeBase_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( EpisodeBaseRecord(data) if data is not None else None )


def getEpisodeExtended(id, meta=None):
    params = {}
    if meta is not None:
        params['meta'] = meta
    path = getEpisodeExtended_path.format(id=str(id))
    res = _query_api(path, params)
    data = res['data'] if res.get('data') is not None else None
    return( EpisodeExtendedRecord(data) if data is not None else None )


def getEpisodeTranslation(id, language):
    path = getEpisodeTranslation_path.format(id=str(id), language=language)
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( Translation(data) if data is not None else None )


def getAllGenders():
    path = getAllGenders_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [Gender(x) for x in data] if data is not None else [] )


def getAllGenres():
    path = getAllGenres_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [GenreBaseRecord(x) for x in data] if data is not None else [] )


def getGenreBase(id):
    path = getGenreBase_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( GenreBaseRecord(data) if data is not None else None )


def getAllInspirationTypes():
    path = getAllInspirationTypes_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [InspirationType(x) for x in data] if data is not None else [] )


def getAllLanguages():
    path = getAllLanguages_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [Language(x) for x in data] if data is not None else [] )


def getAllLists(page=0, yielded=False):
    params = {}
    if page is not None:
        params['page'] = page
    path = getAllLists_path.format()
    if yielded:
        return _query_yielded(ListBaseRecord, path, params, listname=None)
    else:
        res = _query_api(path, params)
        data = res['data'] if res.get('data') is not None else None
        return( [ListBaseRecord(x) for x in data] if data is not None else [] )


def getList(id):
    path = getList_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( ListBaseRecord(data) if data is not None else None )


def getListExtended(id):
    path = getListExtended_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( ListExtendedRecord(data) if data is not None else None )


def getListTranslation(id, language):
    path = getListTranslation_path.format(id=str(id), language=language)
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [Translation(x) for x in data] if data is not None else [] )


def getAllMovie(page=0, yielded=False):
    params = {}
    if page is not None:
        params['page'] = page
    path = getAllMovie_path.format()
    if yielded:
        return _query_yielded(MovieBaseRecord, path, params, listname=None)
    else:
        res = _query_api(path, params)
        data = res['data'] if res.get('data') is not None else None
        return( [MovieBaseRecord(x) for x in data] if data is not None else [] )


def getMovieBase(id):
    path = getMovieBase_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( MovieBaseRecord(data) if data is not None else None )


def getMovieExtended(id, meta=None):
    params = {}
    if meta is not None:
        params['meta'] = meta
    path = getMovieExtended_path.format(id=str(id))
    res = _query_api(path, params)
    data = res['data'] if res.get('data') is not None else None
    return( MovieExtendedRecord(data) if data is not None else None )


def getMovieTranslation(id, language):
    path = getMovieTranslation_path.format(id=str(id), language=language)
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( Translation(data) if data is not None else None )


def getAllMovieStatuses():
    path = getAllMovieStatuses_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [Status(x) for x in data] if data is not None else [] )


def getPeopleBase(id):
    path = getPeopleBase_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( PeopleBaseRecord(data) if data is not None else None )


def getPeopleExtended(id, meta=None):
    params = {}
    if meta is not None:
        params['meta'] = meta
    path = getPeopleExtended_path.format(id=str(id))
    res = _query_api(path, params)
    data = res['data'] if res.get('data') is not None else None
    return( PeopleExtendedRecord(data) if data is not None else None )


def getPeopleTranslation(id, language):
    path = getPeopleTranslation_path.format(id=str(id), language=language)
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( Translation(data) if data is not None else None )


def getAllPeopleTypes():
    path = getAllPeopleTypes_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [PeopleType(x) for x in data] if data is not None else [] )


def getSearchResults(query=None, q=None, type=None, year=None, company=None, country=None,
                     director=None, language=None, primaryType=None, network=None,
                     remote_id=None, offset=None, limit=None):
    params = {}
    if query is not None:
        # The primary search string, which can include the main title for a record including
        # all translations and aliases.
        params['query'] = query
    if q is not None:
        # Alias of the "query" parameter.  Recommend using query instead as this field will
        # eventually be deprecated.
        params['q'] = q
    if type is not None:
        # Restrict results to a specific entity type.  Can be movie, series, person, or company.
        params['type'] = type
    if year is not None:
        #  Restrict results to a specific year. Currently, only used for series and movies.
        params['year'] = year
    if company is not None:
        # Restrict results to a specific company (original network, production company, studio, etc).
        # As an example, "The Walking Dead" would have companies of "AMC", "AMC+", and "Disney+".
        params['company'] = company
    if country is not None:
        # Restrict results to a specific country of origin. Should contain a 3 character country code.
        # Currently only used for series and movies.
        params['country'] = country
    if director is not None:
        # Restrict results to a specific director.  Generally only used for movies.  Should include
        # the full name of the director, such as "Steven Spielberg".
        params['director'] = director
    if language is not None:
        # Restrict results to a specific primary language.  Should include the 3 character language
        # code.  Currently only used for series and movies.
        params['language'] = language
    if primaryType is not None:
        # Restrict results to a specific type of company.  Should include the full name of the
        # type of company, such as "Production Company".  Only used for companies.
        params['primaryType'] = primaryType
    if network is not None:
        # Restrict results to a specific network.  Used for TV and TV movies, and functions
        # the same as the company parameter with more specificity.
        params['network'] = network
    if remote_id is not None:
        # Search for a specific remote id.  Allows searching for an IMDB or EIDR id, for example.
        params['remote_id'] = remote_id
    if offset is not None:
        # Offset results.
        params['offset'] = offset
    if limit is not None:
        # Limit results.
        params['limit'] = limit
    path = getSearchResults_path.format()
    res = _query_api(path, params)
    data = res['data'] if res.get('data') is not None else None
    return( [SearchResult(x) for x in data] if data is not None else [] )


def getAllSeasons(page=0, yielded=False):
    params = {}
    if page is not None:
        params['page'] = page
    path = getAllSeasons_path.format()
    if yielded:
        return _query_yielded(SeasonBaseRecord, path, params, listname=None)
    else:
        res = _query_api(path, params)
        data = res['data'] if res.get('data') is not None else None
        return( [SeasonBaseRecord(x) for x in data] if data is not None else [] )


def getSeasonBase(id):
    path = getSeasonBase_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( SeasonBaseRecord(data) if data is not None else None )


def getSeasonExtended(id):
    path = getSeasonExtended_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( SeasonExtendedRecord(data) if data is not None else None )


def getSeasonTypes():
    path = getSeasonTypes_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [SeasonType(x) for x in data] if data is not None else [] )


def getSeasonTranslation(id, language):
    path = getSeasonTranslation_path.format(id=str(id), language=language)
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( Translation(data) if data is not None else None )


def getAllSeries(page=0, yielded=False):
    params = {}
    if page is not None:
        params['page'] = page
    path = getAllSeries_path.format()
    if yielded:
        return _query_yielded(SeriesBaseRecord, path, params, listname=None)
    else:
        res = _query_api(path, params)
        data = res['data'] if res.get('data') is not None else None
        return( [SeriesBaseRecord(x) for x in data] if data is not None else [] )


def getSeriesBase(id):
    path = getSeriesBase_path.format(id=str(id))
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( SeriesBaseRecord(data) if data is not None else None )


def getSeriesArtworks(id, lang=None, type=None):
    params = {}
    if lang is not None:
        params['lang'] = lang
    if type is not None:
        params['type'] = type
    path = getSeriesArtworks_path.format(id=str(id))
    res = _query_api(path, params)
    data = res['data'] if res.get('data') is not None else None
    return( SeriesExtendedRecord(data) if data is not None else None )


def getSeriesExtended(id, meta=None):
    params = {}
    if meta is not None:
        params['meta'] = meta
    path = getSeriesExtended_path.format(id=str(id))
    res = _query_api(path, params)
    data = res['data'] if res.get('data') is not None else None
    return( SeriesExtendedRecord(data) if data is not None else None )


def getSeriesEpisodes(id, season_type, season=None, episodeNumber=None, airDate=None, page=0, yielded=False):
    params = {}
    if season is not None:
        params['season'] = season
    if episodeNumber is not None:
        params['episodeNumber'] = episodeNumber
    if airDate is not None:
        params['airDate'] = airDate
    if page is not None:
        params['page'] = page
    path = getSeriesEpisodes_path.format(id=str(id), season_type=season_type)
    if yielded:
        return _query_yielded(EpisodeBaseRecord, path, params, listname='episodes')
    else:
        res = _query_api(path, params)
        data = res['data'] if res.get('data') is not None else None
        return( SeriesBaseRecord(data['series']) if data is not None else None,
                [EpisodeBaseRecord(x) for x in data['episodes']] if data is not None else [] )


def getSeriesSeasonEpisodesTranslated(id, season_type, lang, page=0, yielded=False):
    params = {}
    if page is not None:
        params['page'] = page
    path = getSeriesSeasonEpisodesTranslated_path.format(id=str(id), season_type=season_type, lang=lang)
    if yielded:
        return _query_yielded(SeriesBaseRecord, path, params, listname=None)
    else:
        res = _query_api(path, params)
        data = res['data'] if res.get('data') is not None else None
        return( SeriesBaseRecord(data['series']) if data is not None else None )


def getSeriesTranslation(id, language):
    path = getSeriesTranslation_path.format(id=str(id), language=language)
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( Translation(data) if data is not None else None )


def getAllSeriesStatuses():
    path = getAllSeriesStatuses_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [Status(x) for x in data] if data is not None else [] )


def getAllSourceTypes():
    path = getAllSourceTypes_path.format()
    res = _query_api(path)
    data = res['data'] if res.get('data') is not None else None
    return( [SourceType(x) for x in data] if data is not None else [] )


def updates(since, type=None, action=None, page=0, yielded=False):
    params = {}
    if type is not None:
        params['type'] = type
    if action is not None:
        params['action'] = action
    if page is not None:
        params['page'] = page
    path = updates_path.format(since=since)
    if yielded:
        return _query_yielded(EntityUpdate, path, params, listname=None)
    else:
        res = _query_api(path, params)
        data = res['data'] if res.get('data') is not None else None
        return( [EntityUpdate(x) for x in data] if data is not None else [] )

