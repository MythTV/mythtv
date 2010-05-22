#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: bliptv_exceptions - Custom exceptions used or raised by bliptv_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by bliptv_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="bliptv_exceptions - Custom exceptions used or raised by bliptv_api";
__author__="R.D. Vaughan"
__version__="v0.2.0"
# 0.1.0 Initial development
# 0.1.1 Documentation review
# 0.2.0 Public release

__all__ = ["BliptvUrlError", "BliptvHttpError", "BliptvRssError", "BliptvVideoNotFound", "BliptvXmlError"]

class BliptvBaseError(Exception):
    pass

class BliptvUrlError(BliptvBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class BliptvHttpError(BliptvBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class BliptvRssError(BliptvBaseError):
    def __repr__(self):
        return None
    # end __repr__

class BliptvVideoNotFound(BliptvBaseError):
    def __repr__(self):
        return None
    # end __repr__

class BliptvXmlError(BliptvBaseError):
    def __repr__(self):
        return None
    # end __repr__
