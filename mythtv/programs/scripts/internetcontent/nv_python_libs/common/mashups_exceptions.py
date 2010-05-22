#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mashups_exceptions - Custom exceptions used or raised by Mashups_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by Mashups_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mashups_exceptions - Custom exceptions used or raised by mashups";
__author__="R.D. Vaughan"
__version__="v0.1.1"
# 0.1.0 Initial development
# 0.1.1 Changes due to abanding the EMML Engine/Tomcat server and the addition of the common function class

__all__ = ["MashupsUrlError", "MashupsHttpError", "MashupsRssError", "MashupsVideoNotFound", "MashupsInvalidSearchType", "MashupsXmlError", "MashupsVideoDetailError", "MashupsCategoryNotFound", ]

class MashupsBaseError(Exception):
    pass

class MashupsUrlError(MashupsBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class MashupsHttpError(MashupsBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class MashupsRssError(MashupsBaseError):
    def __repr__(self):
        return None
    # end __repr__

class MashupsVideoNotFound(MashupsBaseError):
    def __repr__(self):
        return None
    # end __repr__

class MashupsInvalidSearchType(MashupsBaseError):
    def __repr__(self):
        return None
    # end __repr__

class MashupsXmlError(MashupsBaseError):
    def __repr__(self):
        return None
    # end __repr__
