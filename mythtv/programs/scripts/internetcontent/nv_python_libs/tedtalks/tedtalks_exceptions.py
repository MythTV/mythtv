#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tedtalks_exceptions - Custom exceptions used or raised by tedtalks_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by tedtalks_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="tedtalks_exceptions - Custom exceptions used or raised by tedtalks_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["TedTalksUrlError", "TedTalksHttpError", "TedTalksRssError", "TedTalksVideoNotFound", "TedTalksConfigFileError", "TedTalksUrlDownloadError"]

class TedTalksBaseError(Exception):
    pass

class TedTalksUrlError(TedTalksBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class TedTalksHttpError(TedTalksBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class TedTalksRssError(TedTalksBaseError):
    def __repr__(self):
        return None
    # end __repr__

class TedTalksVideoNotFound(TedTalksBaseError):
    def __repr__(self):
        return None
    # end __repr__

class TedTalksConfigFileError(TedTalksBaseError):
    def __repr__(self):
        return None
    # end __repr__

class TedTalksUrlDownloadError(TedTalksBaseError):
    def __repr__(self):
        return None
    # end __repr__
