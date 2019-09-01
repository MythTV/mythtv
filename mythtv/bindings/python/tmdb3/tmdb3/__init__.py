#!/usr/bin/env python

import sys
IS_PY2 = sys.version_info[0] == 2

from .tmdb_api import Configuration, searchMovie, searchMovieWithYear, \
                     searchPerson, searchStudio, searchList, searchCollection, \
                     searchSeries, Person, Movie, Collection, Genre, List, \
                     Series, Studio, Network, Episode, Season, __version__
from .request import Request, set_key, set_cache
from .locales import get_locale, set_locale
from .tmdb_auth import get_session, set_session
from .cache_engine import CacheEngine
from .tmdb_exceptions import *

