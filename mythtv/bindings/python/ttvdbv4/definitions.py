# -*- coding: UTF-8 -*-

# ----------------------------------------------------
# Purpose:   MythTV Python Bindings for TheTVDB v4 API
# Copyright: (c) 2021 Roland Ernst
# License:   GPL v2 or later, see LICENSE for details
# ----------------------------------------------------


def _get_list(item, name):
    l = []
    if item is not None:
        try:
            l = [x for x in item.get('%s' % name)]
        except (AttributeError, ValueError, IndexError, TypeError):
            pass
    return l


def _handle_single(handle, data):
    try:
        return handle(data)
    except (AttributeError, ValueError):
        return None


def _handle_list(handle, data):
    l = []
    if data is not None:
        try:
            for d in data:
                el = _handle_single(handle, d)
                if el is not None:
                    l.append(el)
        except IndexError:
            pass
        return l
    else:
        return l


"""Generated API for thetvdb.com TVDB API V4 v 4.5.4"""
# modifications marked with '### XXX'


class Alias(object):
    """An alias model, which can be associated with a series, season, movie, person, or list."""
    def __init__(self, data):
        self.language = data.get('language', '')                             # string
        self.name = data.get('name', '')                                     # string


class ArtworkBaseRecord(object):
    """base artwork record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.language = data.get('language', '')                             # string
        self.score = data.get('score', 0.0)                                  # number
        self.thumbnail = data.get('thumbnail', '')                           # string
        self.type = data.get('type', 0)                                      # integer


class ArtworkExtendedRecord(object):
    """extended artwork record"""
    def __init__(self, data):
        self.episodeId = data.get('episodeId', 0)                            # integer
        self.height = data.get('height', 0)                                  # integer
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.language = data.get('language', '')                             # string
        self.movieId = data.get('movieId', 0)                                # integer
        self.networkId = data.get('networkId', 0)                            # integer
        self.peopleId = data.get('peopleId', 0)                              # integer
        self.score = data.get('score', 0.0)                                  # number
        self.seasonId = data.get('seasonId', 0)                              # integer
        self.seriesId = data.get('seriesId', 0)                              # integer
        self.seriesPeopleId = data.get('seriesPeopleId', 0)                  # integer
        self.status = _handle_single(ArtworkStatus, data.get('status'))
        self.tagOptions = _handle_list(TagOption, data.get('tagOptions'))
        self.thumbnail = data.get('thumbnail', '')                           # string
        self.thumbnailHeight = data.get('thumbnailHeight', 0)                # integer
        self.thumbnailWidth = data.get('thumbnailWidth', 0)                  # integer
        self.type = data.get('type', 0)                                      # integer
        self.updatedAt = data.get('updatedAt', 0)                            # integer
        self.width = data.get('width', 0)                                    # integer


class ArtworkStatus(object):
    """artwork status record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string


class ArtworkType(object):
    """artwork type record"""
    def __init__(self, data):
        self.height = data.get('height', 0)                                  # integer
        self.id = data.get('id', 0)                                          # integer
        self.imageFormat = data.get('imageFormat', '')                       # string
        self.name = data.get('name', '')                                     # string
        self.recordType = data.get('recordType', '')                         # string
        self.slug = data.get('slug', '')                                     # string
        self.thumbHeight = data.get('thumbHeight', 0)                        # integer
        self.thumbWidth = data.get('thumbWidth', 0)                          # integer
        self.width = data.get('width', 0)                                    # integer


class AwardBaseRecord(object):
    """base award record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string


class AwardCategoryBaseRecord(object):
    """base award category record"""
    def __init__(self, data):
        self.allowCoNominees = data.get('allowCoNominees', False)            # boolean
        self.award = _handle_single(AwardBaseRecord, data.get('award'))
        self.forMovies = data.get('forMovies', False)                        # boolean
        self.forSeries = data.get('forSeries', False)                        # boolean
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string


class AwardCategoryExtendedRecord(object):
    """extended award category record"""
    def __init__(self, data):
        self.allowCoNominees = data.get('allowCoNominees', False)            # boolean
        self.award = _handle_single(AwardBaseRecord, data.get('award'))
        self.forMovies = data.get('forMovies', False)                        # boolean
        self.forSeries = data.get('forSeries', False)                        # boolean
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.nominees = _handle_list(AwardNomineeBaseRecord, data.get('nominees'))


class AwardExtendedRecord(object):
    """extended award record"""
    def __init__(self, data):
        self.categories = _handle_list(AwardCategoryBaseRecord, data.get('categories'))
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.score = data.get('score', 0)                                    # integer


class AwardNomineeBaseRecord(object):
    """base award nominee record"""
    def __init__(self, data):
        self.character = _handle_single(Character, data.get('character'))
        self.details = data.get('details', '')                               # string
        self.episode = _handle_single(EpisodeBaseRecord, data.get('episode'))
        self.id = data.get('id', 0)                                          # integer
        self.isWinner = data.get('isWinner', False)                          # boolean
        self.movie = _handle_single(MovieBaseRecord, data.get('movie'))
        self.series = _handle_single(SeriesBaseRecord, data.get('series'))
        self.year = data.get('year', '')                                     # string
        self.category = data.get('category', '')                             # string
        self.name = data.get('name', '')                                     # string


class Biography(object):
    """biography record"""
    def __init__(self, data):
        self.biography = data.get('biography', '')                           # string
        self.language = data.get('language', '')                             # string


class Character(object):
    """character record"""
    def __init__(self, data):
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.episodeId = data.get('episodeId', 0)                            # integer
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.isFeatured = data.get('isFeatured', False)                      # boolean
        self.movieId = data.get('movieId', 0)                                # integer
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.peopleId = data.get('peopleId', 0)                              # integer
        self.personImgURL = data.get('personImgURL', '')                     # string
        self.peopleType = data.get('peopleType', '')                         # string
        self.seriesId = data.get('seriesId', 0)                              # integer
        self.sort = data.get('sort', 0)                                      # integer
        self.tagOptions = _handle_list(TagOption, data.get('tagOptions'))
        self.type = data.get('type', 0)                                      # integer
        self.url = data.get('url', '')                                       # string
        self.personName = data.get('personName', '')                         # string
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class Company(object):
    """A company record"""
    def __init__(self, data):
        self.activeDate = data.get('activeDate', '')                         # string
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.country = data.get('country', '')                               # string
        self.id = data.get('id', 0)                                          # integer
        self.inactiveDate = data.get('inactiveDate', '')                     # string
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.primaryCompanyType = data.get('primaryCompanyType', 0)          # integer
        self.slug = data.get('slug', '')                                     # string
        self.parentCompany = _handle_single(ParentCompany, data.get('parentCompany'))
        self.tagOptions = _handle_list(TagOption, data.get('tagOptions'))
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class ParentCompany(object):
    """A parent company record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.relation = _handle_single(CompanyRelationShip, data.get('relation'))


class CompanyRelationShip(object):
    """A company relationship"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.typeName = data.get('typeName', '')                             # string


class CompanyType(object):
    """A company type record"""
    def __init__(self, data):
        self.companyTypeId = data.get('companyTypeId', 0)                    # integer
        self.companyTypeName = data.get('companyTypeName', '')               # string


class ContentRating(object):
    """content rating record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.description = data.get('description', '')                       # string
        self.country = data.get('country', '')                               # string
        self.contentType = data.get('contentType', '')                       # string
        self.order = data.get('order', 0)                                    # integer
        self.fullName = data.get('fullName', '')                             # string


class Country(object):
    """country record"""
    def __init__(self, data):
        self.id = data.get('id', '')                                         # string
        self.name = data.get('name', '')                                     # string
        self.shortCode = data.get('shortCode', '')                           # string


class Entity(object):
    """Entity record"""
    def __init__(self, data):
        self.movieId = data.get('movieId', 0)                                # integer
        self.order = data.get('order', 0)                                    # integer
        self.seriesId = data.get('seriesId', 0)                              # integer


class EntityType(object):
    """Entity Type record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.hasSpecials = data.get('hasSpecials', False)                    # boolean


class EntityUpdate(object):
    """entity update record"""
    def __init__(self, data):
        self.entityType = data.get('entityType', '')                         # string
        self.method = data.get('method', '')                                 # string
        self.recordId = data.get('recordId', 0)                              # integer
        self.timeStamp = data.get('timeStamp', 0)                            # integer
        self.seriesId = data.get('seriesId', 0)                              # integer


class EpisodeBaseRecord(object):
    """base episode record"""
    def __init__(self, data):
        self.aired = data.get('aired', '')                                   # string
        self.airsAfterSeason = data.get('airsAfterSeason', 0)                # integer
        self.airsBeforeEpisode = data.get('airsBeforeEpisode', 0)            # integer
        self.airsBeforeSeason = data.get('airsBeforeSeason', 0)              # integer
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.imageType = data.get('imageType', 0)                            # integer
        self.isMovie = data.get('isMovie', 0)                                # integer
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.number = data.get('number', 0)                                  # integer
        self.overview = data.get('overview', '')                             # string
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.runtime = data.get('runtime', 0)                                # integer
        self.seasonNumber = data.get('seasonNumber', 0)                      # integer
        self.seasons = _handle_list(SeasonBaseRecord, data.get('seasons'))
        self.seriesId = data.get('seriesId', 0)                              # integer
        self.seasonName = data.get('seasonName', '')                         # string
        self.lastUpdated = data.get('lastUpdated', '')                       # string
        self.finaleType = data.get('finaleType', '')                         # string
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class EpisodeExtendedRecord(object):
    """extended episode record"""
    def __init__(self, data):
        self.aired = data.get('aired', '')                                   # string
        self.airsAfterSeason = data.get('airsAfterSeason', 0)                # integer
        self.airsBeforeEpisode = data.get('airsBeforeEpisode', 0)            # integer
        self.airsBeforeSeason = data.get('airsBeforeSeason', 0)              # integer
        self.awards = _handle_list(AwardBaseRecord, data.get('awards'))
        self.characters = _handle_list(Character, data.get('characters'))
        self.companies = _handle_list(Company, data.get('companies'))
        self.contentRatings = _handle_list(ContentRating, data.get('contentRatings'))
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.imageType = data.get('imageType', 0)                            # integer
        self.isMovie = data.get('isMovie', 0)                                # integer
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.network = _handle_list(Company, data.get('network'))
        self.nominations = _handle_list(AwardNomineeBaseRecord, data.get('nominations'))
        self.number = data.get('number', 0)                                  # integer
        self.overview = data.get('overview', '')                             # string
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.productionCode = data.get('productionCode', '')                 # string
        self.remoteIds = _handle_list(RemoteID, data.get('remoteIds'))
        self.runtime = data.get('runtime', 0)                                # integer
        self.seasonNumber = data.get('seasonNumber', 0)                      # integer
        self.seasons = _handle_list(SeasonBaseRecord, data.get('seasons'))
        self.seriesId = data.get('seriesId', 0)                              # integer
        self.studios = _handle_list(Company, data.get('studios'))
        self.tagOptions = _handle_list(TagOption, data.get('tagOptions'))
        self.trailers = _handle_list(Trailer, data.get('trailers'))
        self.lastUpdated = data.get('lastUpdated', '')                       # string
        self.finaleType = data.get('finaleType', '')                         # string
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class Gender(object):
    """gender record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string


class GenreBaseRecord(object):
    """base genre record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.slug = data.get('slug', '')                                     # string


class Language(object):
    """language record"""
    def __init__(self, data):
        self.id = data.get('id', '')                                         # string
        self.name = data.get('name', '')                                     # string
        self.nativeName = data.get('nativeName', '')                         # string
        self.shortCode = data.get('shortCode', '')                           # string


class ListBaseRecord(object):
    """base list record"""
    def __init__(self, data):
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.imageIsFallback = data.get('imageIsFallback', False)            # boolean
        self.isOfficial = data.get('isOfficial', False)                      # boolean
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.overview = data.get('overview', '')                             # string
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.url = data.get('url', '')                                       # string
        self.score = data.get('score', 0)                                    # integer
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class ListExtendedRecord(object):
    """extended list record"""
    def __init__(self, data):
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.entities = _handle_list(Entity, data.get('entities'))
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.imageIsFallback = data.get('imageIsFallback', False)            # boolean
        self.isOfficial = data.get('isOfficial', False)                      # boolean
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.overview = data.get('overview', '')                             # string
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.score = data.get('score', 0)                                    # integer
        self.url = data.get('url', '')                                       # string
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class MovieBaseRecord(object):
    """base movie record"""
    def __init__(self, data):
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.score = data.get('score', 0.0)                                  # number
        self.slug = data.get('slug', '')                                     # string
        self.status = _handle_single(Status, data.get('status'))
        self.runtime = data.get('runtime', 0)                                # integer
        self.lastUpdated = data.get('lastUpdated', '')                       # string
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class MovieExtendedRecord(object):
    """extended movie record"""
    def __init__(self, data):
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.artworks = _handle_list(ArtworkBaseRecord, data.get('artworks'))
        self.audioLanguages = _get_list(data, 'audioLanguages')
        self.awards = _handle_list(AwardBaseRecord, data.get('awards'))
        self.boxOffice = data.get('boxOffice', '')                           # string
        self.budget = data.get('budget', '')                                 # string
        self.characters = _handle_list(Character, data.get('characters'))
        self.companies = _handle_single(Companies, data.get('companies'))
        self.contentRatings = _handle_list(ContentRating, data.get('contentRatings'))
        self.first_release = _handle_single(Release, data.get('first_release'))
        self.genres = _handle_list(GenreBaseRecord, data.get('genres'))
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.inspirations = _handle_list(Inspiration, data.get('inspirations'))
        self.lastUpdated = data.get('lastUpdated', '')                       # string
        self.lists = _handle_list(ListBaseRecord, data.get('lists'))
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.originalCountry = data.get('originalCountry', '')               # string
        self.originalLanguage = data.get('originalLanguage', '')             # string
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.production_countries = _handle_list(ProductionCountry, data.get('production_countries'))
        self.releases = _handle_list(Release, data.get('releases'))
        self.remoteIds = _handle_list(RemoteID, data.get('remoteIds'))
        self.runtime = data.get('runtime', 0)                                # integer
        self.score = data.get('score', 0.0)                                  # number
        self.slug = data.get('slug', '')                                     # string
        self.spoken_languages = _get_list(data, 'spoken_languages')
        self.status = _handle_single(Status, data.get('status'))
        self.studios = _handle_list(StudioBaseRecord, data.get('studios'))
        self.subtitleLanguages = _get_list(data, 'subtitleLanguages')
        self.tagOptions = _handle_list(TagOption, data.get('tagOptions'))
        self.trailers = _handle_list(Trailer, data.get('trailers'))
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class PeopleBaseRecord(object):
    """base people record"""
    def __init__(self, data):
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.score = data.get('score', 0)                                    # integer
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class PeopleExtendedRecord(object):
    """extended people record"""
    def __init__(self, data):
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.awards = _handle_list(AwardBaseRecord, data.get('awards'))
        self.biographies = _handle_list(Biography, data.get('biographies'))
        self.birth = data.get('birth', '')                                   # string
        self.birthPlace = data.get('birthPlace', '')                         # string
        self.characters = _handle_list(Character, data.get('characters'))
        self.death = data.get('death', '')                                   # string
        self.gender = data.get('gender', 0)                                  # integer
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.races = _handle_list(Race, data.get('races'))
        self.remoteIds = _handle_list(RemoteID, data.get('remoteIds'))
        self.score = data.get('score', 0)                                    # integer
        self.slug = data.get('slug', '')                                     # string
        self.tagOptions = _handle_list(TagOption, data.get('tagOptions'))
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class PeopleType(object):
    """people type record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string


class Race(object):
    """race record"""
    def __init__(self, data):
        pass


class Release(object):
    """release record"""
    def __init__(self, data):
        self.country = data.get('country', '')                               # string
        self.date = data.get('date', '')                                     # string
        self.detail = data.get('detail', '')                                 # string


class RemoteID(object):
    """remote id record"""
    def __init__(self, data):
        self.id = data.get('id', '')                                         # string
        self.type = data.get('type', 0)                                      # integer
        self.sourceName = data.get('sourceName', '')                         # string


class SearchResult(object):
    """search result"""
    def __init__(self, data):
        self.aliases = _get_list(data, 'aliases')
        self.companies = _get_list(data, 'companies')
        self.companyType = data.get('companyType', '')                       # string
        self.country = data.get('country', '')                               # string
        self.director = data.get('director', '')                             # string
        self.first_air_time = data.get('first_air_time', '')                 # string
        self.genres = _get_list(data, 'genres')
        self.id = data.get('id', '')                                         # string
        self.image_url = data.get('image_url', '')                           # string
        self.name = data.get('name', '')                                     # string
        self.is_official = data.get('is_official', False)                    # boolean
        self.name_translated = data.get('name_translated', '')               # string
        self.network = data.get('network', '')                               # string
        self.objectID = data.get('objectID', '')                             # string
        self.officialList = data.get('officialList', '')                     # string
        self.overview = data.get('overview', '')                             # string
### XXX self.overviews = _handle_list(TranslationSimple, data.get('overviews'))
        self.overviews = data.get('overviews', {})                           ### XXX
        self.overview_translated = _get_list(data, 'overview_translated')
        self.poster = data.get('poster', '')                                 # string
        self.posters = _get_list(data, 'posters')
        self.primary_language = data.get('primary_language', '')             # string
        self.remote_ids = _handle_list(RemoteID, data.get('remote_ids'))
        self.status = data.get('status', '')                                 # string
        self.slug = data.get('slug', '')                                     # string
        self.studios = _get_list(data, 'studios')
        self.title = data.get('title', '')                                   # string
        self.thumbnail = data.get('thumbnail', '')                           # string
### XXX  self.translations = _handle_list(TranslationSimple, data.get('translations'))
        self.translations = data.get('translations', {})                     ### XXX
        self.translationsWithLang = _get_list(data, 'translationsWithLang')
        self.tvdb_id = data.get('tvdb_id', '')                               # string
        self.type = data.get('type', '')                                     # string
        self.year = data.get('year', '')                                     # string
        # additional attributes needed by the mythtv grabber script:
        self.name_similarity = 0.0                                           ### XXX


class SeasonBaseRecord(object):
    """season genre record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.imageType = data.get('imageType', 0)                            # integer
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.number = data.get('number', 0)                                  # integer
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.companies = _handle_single(Companies, data.get('companies'))
        self.seriesId = data.get('seriesId', 0)                              # integer
        self.type = _handle_single(SeasonType, data.get('type'))
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class SeasonExtendedRecord(object):
    """extended season record"""
    def __init__(self, data):
        self.artwork = _handle_list(ArtworkBaseRecord, data.get('artwork'))
        self.companies = _handle_single(Companies, data.get('companies'))
        self.episodes = _handle_list(EpisodeBaseRecord, data.get('episodes'))
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.imageType = data.get('imageType', 0)                            # integer
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.number = data.get('number', 0)                                  # integer
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.seriesId = data.get('seriesId', 0)                              # integer
        self.trailers = _handle_list(Trailer, data.get('trailers'))
        self.type = _handle_single(SeasonType, data.get('type'))
        self.tagOptions = _handle_list(TagOption, data.get('tagOptions'))
        self.translations = _handle_list(Translation, data.get('translations'))
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class SeasonType(object):
    """season type record"""
    def __init__(self, data):
        self.alternateName = data.get('alternateName', '')                   # string
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.type = data.get('type', '')                                     # string


class SeriesAirsDays(object):
    """A series airs day record"""
    def __init__(self, data):
        self.friday = data.get('friday', False)                              # boolean
        self.monday = data.get('monday', False)                              # boolean
        self.saturday = data.get('saturday', False)                          # boolean
        self.sunday = data.get('sunday', False)                              # boolean
        self.thursday = data.get('thursday', False)                          # boolean
        self.tuesday = data.get('tuesday', False)                            # boolean
        self.wednesday = data.get('wednesday', False)                        # boolean


class SeriesBaseRecord(object):
    """
    The base record for a series. All series airs time like firstAired, lastAired, nextAired, etc.
    are in US EST for US series, and for all non-US series, the time of the show’s
    country capital or most populous city. For streaming services, is the official release time.
    See https://support.thetvdb.com/kb/faq.php?id=29.
    """
    def __init__(self, data):
        self.abbreviation = data.get('abbreviation', '')                     # string
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.averageRuntime = data.get('averageRuntime', 0)                  # integer
        self.country = data.get('country', '')                               # string
        self.defaultSeasonType = data.get('defaultSeasonType', 0)            # integer
        self.episodes = _handle_list(EpisodeBaseRecord, data.get('episodes'))
        self.firstAired = data.get('firstAired', '')                         # string
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.isOrderRandomized = data.get('isOrderRandomized', False)        # boolean
        self.lastAired = data.get('lastAired', '')                           # string
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.nextAired = data.get('nextAired', '')                           # string
        self.originalCountry = data.get('originalCountry', '')               # string
        self.originalLanguage = data.get('originalLanguage', '')             # string
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.score = data.get('score', 0.0)                                  # number
        self.slug = data.get('slug', '')                                     # string
        self.status = _handle_single(Status, data.get('status'))
        self.lastUpdated = data.get('lastUpdated', '')                       # string
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class SeriesExtendedRecord(object):
    """
    The extended record for a series. All series airs time like firstAired, lastAired, nextAired, etc.
    are in US EST for US series, and for all non-US series, the time of the show’s country capital
    or most populous city. For streaming services, is the official release time.
    See https://support.thetvdb.com/kb/faq.php?id=29.
    """
    def __init__(self, data):
        self.abbreviation = data.get('abbreviation', '')                     # string
        self.airsDays = _handle_single(SeriesAirsDays, data.get('airsDays'))
        self.airsTime = data.get('airsTime', '')                             # string
        self.airsTimeUTC = data.get('airsTimeUTC', '')                       # string
        self.aliases = _handle_list(Alias, data.get('aliases'))
        self.artworks = _handle_list(ArtworkExtendedRecord, data.get('artworks'))
        self.averageRuntime = data.get('averageRuntime', 0)                  # integer
        self.characters = _handle_list(Character, data.get('characters'))
        self.contentRatings = _handle_list(ContentRating, data.get('contentRatings'))
        self.country = data.get('country', '')                               # string
        self.defaultSeasonType = data.get('defaultSeasonType', 0)            # integer
        self.firstAired = data.get('firstAired', '')                         # string
        self.genres = _handle_list(GenreBaseRecord, data.get('genres'))
        self.id = data.get('id', 0)                                          # integer
        self.image = data.get('image', '')                                   # string
        self.isOrderRandomized = data.get('isOrderRandomized', False)        # boolean
        self.lastAired = data.get('lastAired', '')                           # string
        self.lastUpdated = data.get('lastUpdated', '')                       # string
        self.name = data.get('name', '')                                     # string
        self.nameTranslations = _get_list(data, 'nameTranslations')
        self.companies = _handle_list(Company, data.get('companies'))
        self.nextAired = data.get('nextAired', '')                           # string
        self.originalCountry = data.get('originalCountry', '')               # string
        self.originalLanguage = data.get('originalLanguage', '')             # string
        self.originalNetwork = _handle_single(Company, data.get('originalNetwork'))
        self.overview = data.get('overview', '')                             # string
        self.latestNetwork = _handle_single(Company, data.get('latestNetwork'))
        self.overviewTranslations = _get_list(data, 'overviewTranslations')
        self.remoteIds = _handle_list(RemoteID, data.get('remoteIds'))
        self.score = data.get('score', 0.0)                                  # number
        self.seasons = _handle_list(SeasonBaseRecord, data.get('seasons'))
        self.seasonTypes = _handle_list(SeasonType, data.get('seasonTypes'))
        self.slug = data.get('slug', '')                                     # string
        self.status = _handle_single(Status, data.get('status'))
        self.tags = _handle_list(TagOption, data.get('tags'))
        self.trailers = _handle_list(Trailer, data.get('trailers'))
        self.translations = _handle_list(TranslationExtended, data.get('translations'))
        # additional attributes needed by the mythtv grabber script:
        self.fetched_translations = []
        self.name_similarity = 0.0


class SourceType(object):
    """source type record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.postfix = data.get('postfix', '')                               # string
        self.prefix = data.get('prefix', '')                                 # string
        self.slug = data.get('slug', '')                                     # string
        self.sort = data.get('sort', 0)                                      # integer


class Status(object):
    """status record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.keepUpdated = data.get('keepUpdated', False)                    # boolean
        self.name = data.get('name', '')                                     # string
        self.recordType = data.get('recordType', '')                         # string


class StudioBaseRecord(object):
    """studio record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.parentStudio = data.get('parentStudio', 0)                      # integer


class Tag(object):
    """tag record"""
    def __init__(self, data):
        self.allowsMultiple = data.get('allowsMultiple', False)              # boolean
        self.helpText = data.get('helpText', '')                             # string
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.options = _handle_list(TagOption, data.get('options'))


class TagOption(object):
    """tag option record"""
    def __init__(self, data):
        self.helpText = data.get('helpText', '')                             # string
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.tag = data.get('tag', 0)                                        # integer
        self.tagName = data.get('tagName', '')                               # string


class Trailer(object):
    """trailer record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.language = data.get('language', '')                             # string
        self.name = data.get('name', '')                                     # string
        self.url = data.get('url', '')                                       # string


class Translation(object):
    """translation record"""
    def __init__(self, data):
        self.aliases = _get_list(data, 'aliases')
        self.isAlias = data.get('isAlias', False)                            # boolean
        self.isPrimary = data.get('isPrimary', False)                        # boolean
        self.language = data.get('language', '')                             # string
        self.name = data.get('name', '')                                     # string
        self.overview = data.get('overview', '')                             # string
        self.tagline = data.get('tagline', '')                               # string


class TranslationSimple(object):
    """translation simple record"""
    def __init__(self, data):
        self.language = data.get('language', '')                             # string


class TranslationExtended(object):
    """translation extended record"""
    def __init__(self, data):
        self.nameTranslations = _handle_list(Translation, data.get('nameTranslations'))
        self.overviewTranslations = _handle_list(Translation, data.get('overviewTranslations'))
        self.alias = _get_list(data, 'alias')


class TagOptionEntity(object):
    """a entity with selected tag option"""
    def __init__(self, data):
        self.name = data.get('name', '')                                     # string
        self.tagName = data.get('tagName', '')                               # string
        self.tagId = data.get('tagId', 0)                                    # integer


class Inspiration(object):
    """Movie inspiration record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.type = data.get('type', '')                                     # string
        self.type_name = data.get('type_name', '')                           # string
        self.url = data.get('url', '')                                       # string


class InspirationType(object):
    """Movie inspiration type record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.name = data.get('name', '')                                     # string
        self.description = data.get('description', '')                       # string
        self.reference_name = data.get('reference_name', '')                 # string
        self.url = data.get('url', '')                                       # string


class ProductionCountry(object):
    """Production country record"""
    def __init__(self, data):
        self.id = data.get('id', 0)                                          # integer
        self.country = data.get('country', '')                               # string
        self.name = data.get('name', '')                                     # string


class Companies(object):
    """Companies by type record"""
    def __init__(self, data):
        self.studio = _handle_single(Company, data.get('studio'))
        self.network = _handle_single(Company, data.get('network'))
        self.production = _handle_single(Company, data.get('production'))
        self.distributor = _handle_single(Company, data.get('distributor'))
        self.special_effects = _handle_single(Company, data.get('special_effects'))


class Links(object):
    """Links for next, previous and current record"""
    def __init__(self, data):
        self.prev = data.get('prev', '')                                     # string
        self.self = data.get('self', '')                                     # string
        self.next = data.get('next', '')                                     # string
        self.total_items = data.get('total_items', 0)                        # integer
        self.page_size = data.get('page_size', 0)                            # integer
