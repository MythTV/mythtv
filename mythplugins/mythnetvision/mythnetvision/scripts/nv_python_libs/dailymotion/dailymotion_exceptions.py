#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: dailymotion_exceptions - Custom exceptions used or raised by dailymotion_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by dailymotion_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="dailymotion_exceptions - Custom exceptions used or raised by dailymotion_api";
__author__="R.D. Vaughan"
__version__="v0.2.0"
# 0.1.0 Initial development
# 0.1.2 Documentation update
# 0.2.0 Public release

__all__ = ["DailymotionUrlError", "DailymotionHttpError", "DailymotionRssError", "DailymotionVideoNotFound", "DailymotionInvalidSearchType", "DailymotionXmlError", "DailymotionVideoDetailError", "DailymotionCategoryNotFound", ]

class DailymotionBaseError(Exception):
    pass

class DailymotionUrlError(DailymotionBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class DailymotionHttpError(DailymotionBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class DailymotionRssError(DailymotionBaseError):
    def __repr__(self):
        return None
    # end __repr__

class DailymotionVideoNotFound(DailymotionBaseError):
    def __repr__(self):
        return None
    # end __repr__

class DailymotionInvalidSearchType(DailymotionBaseError):
    def __repr__(self):
        return None
    # end __repr__

class DailymotionXmlError(DailymotionBaseError):
    def __repr__(self):
        return None
    # end __repr__

class DailymotionVideoDetailError(DailymotionBaseError):
    def __repr__(self):
        return None
    # end __repr__

class DailymotionCategoryNotFound(DailymotionBaseError):
    def __repr__(self):
        return None
    # end __repr__
