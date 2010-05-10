#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: rev3_exceptions - Custom exceptions used or raised by rev3_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by rev3_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="rev3_exceptions - Custom exceptions used or raised by rev3_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["Rev3UrlError", "Rev3HttpError", "Rev3RssError", "Rev3VideoNotFound", "Rev3ConfigFileError", "Rev3UrlDownloadError"]

class Rev3BaseError(Exception):
    pass

class Rev3UrlError(Rev3BaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class Rev3HttpError(Rev3BaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class Rev3RssError(Rev3BaseError):
    def __repr__(self):
        return None
    # end __repr__

class Rev3VideoNotFound(Rev3BaseError):
    def __repr__(self):
        return None
    # end __repr__

class Rev3ConfigFileError(Rev3BaseError):
    def __repr__(self):
        return None
    # end __repr__

class Rev3UrlDownloadError(Rev3BaseError):
    def __repr__(self):
        return None
    # end __repr__
