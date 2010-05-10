#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: bbciplayer_exceptions - Custom exceptions used or raised by bbciplayer_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by bbciplayer_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="bbciplayer_exceptions - Custom exceptions used or raised by bbciplayer_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["BBCUrlError", "BBCHttpError", "BBCRssError", "BBCVideoNotFound", "BBCConfigFileError", "BBCUrlDownloadError"]

class BBCBaseError(Exception):
    pass

class BBCUrlError(BBCBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class BBCHttpError(BBCBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class BBCRssError(BBCBaseError):
    def __repr__(self):
        return None
    # end __repr__

class BBCVideoNotFound(BBCBaseError):
    def __repr__(self):
        return None
    # end __repr__

class BBCConfigFileError(BBCBaseError):
    def __repr__(self):
        return None
    # end __repr__

class BBCUrlDownloadError(BBCBaseError):
    def __repr__(self):
        return None
    # end __repr__
