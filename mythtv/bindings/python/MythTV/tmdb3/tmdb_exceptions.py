#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: tmdb_exceptions.py    Common exceptions used in tmdbv3 API library
# Python Library
# Author: Raymond Wagner
#-----------------------

class TMDBError( Exception ):
    pass

class TMDBKeyError( TMDBError ):
    pass

class TMDBKeyMissing( TMDBKeyError ):
    pass

class TMDBKeyInvalid( TMDBKeyError ):
    pass

class TMDBRequestError( TMDBError ):
    pass

class TMDBImageSizeError( TMDBError ):
    pass

class TMDBHTTPError( TMDBError ):
    pass
