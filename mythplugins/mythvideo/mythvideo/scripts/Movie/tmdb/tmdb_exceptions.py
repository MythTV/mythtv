#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tmdb_exceptions - Custom exceptions used or raised by tmdb_api
# Python Script
# Author:   dbr/Ben modified by R.D. Vaughan
# Purpose:  Custom exceptions used or raised by tmdb_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="tmdb_exceptions - Custom exceptions used or raised by tmdb_api";
__author__="dbr/Ben modified by R.D. Vaughan"
__version__="v0.1.4"
# 0.1.0 Initial development
# 0.1.1 Alpha Release
# 0.1.2 Release bump - no changes to this code
# 0.1.3 Release bump - no changes to this code
# 0.1.4 Release bump - no changes to this code

__all__ = ["TmdBaseError", "TmdHttpError", "TmdXmlError", "TmdbUiAbort", "TmdbMovieOrPersonNotFound", ]

# Start of code used to access themoviedb.org api
class TmdBaseError(Exception):
    pass

class TmdHttpError(TmdBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class TmdXmlError(TmdBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class TmdbMovieOrPersonNotFound(TmdBaseError):
    def __repr__(self):
        return None
    # end __repr__

class TmdbUiAbort(TmdBaseError):
    def __repr__(self):
        return None
    # end __repr__
