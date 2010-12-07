#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: youtube_exceptions - Custom exceptions used or raised by youtube_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by youtube_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="youtube_exceptions - Custom exceptions used or raised by youtube_api";
__author__="R.D. Vaughan"
__version__="v0.2.0"
# 0.1.0 Initial development
# 0.1.1 Added exceptions for Tree View processing
# 0.1.2 Documentation review
# 0.2.0 Public release

__all__ = ["YouTubeUrlError", "YouTubeHttpError", "YouTubeRssError", "YouTubeVideoNotFound", "YouTubeInvalidSearchType", "YouTubeXmlError", "YouTubeVideoDetailError", "YouTubeCategoryNotFound", ]

class YouTubeBaseError(Exception):
    pass

class YouTubeUrlError(YouTubeBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class YouTubeHttpError(YouTubeBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class YouTubeRssError(YouTubeBaseError):
    def __repr__(self):
        return None
    # end __repr__

class YouTubeVideoNotFound(YouTubeBaseError):
    def __repr__(self):
        return None
    # end __repr__

class YouTubeInvalidSearchType(YouTubeBaseError):
    def __repr__(self):
        return None
    # end __repr__

class YouTubeXmlError(YouTubeBaseError):
    def __repr__(self):
        return None
    # end __repr__

class YouTubeVideoDetailError(YouTubeBaseError):
    def __repr__(self):
        return None
    # end __repr__

class YouTubeCategoryNotFound(YouTubeBaseError):
    def __repr__(self):
        return None
    # end __repr__
