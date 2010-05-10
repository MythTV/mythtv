#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: thewb_exceptions - Custom exceptions used or raised by thewb_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by thewb_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="thewb_exceptions - Custom exceptions used or raised by thewb_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["TheWBUrlError", "TheWBHttpError", "TheWBRssError", "TheWBVideoNotFound", "TheWBConfigFileError", "TheWBUrlDownloadError"]

class TheWBBaseError(Exception):
    pass

class TheWBUrlError(TheWBBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class TheWBHttpError(TheWBBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class TheWBRssError(TheWBBaseError):
    def __repr__(self):
        return None
    # end __repr__

class TheWBVideoNotFound(TheWBBaseError):
    def __repr__(self):
        return None
    # end __repr__

class TheWBConfigFileError(TheWBBaseError):
    def __repr__(self):
        return None
    # end __repr__

class TheWBUrlDownloadError(TheWBBaseError):
    def __repr__(self):
        return None
    # end __repr__
