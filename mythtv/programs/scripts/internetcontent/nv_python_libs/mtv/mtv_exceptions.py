#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mtv_exceptions - Custom exceptions used or raised by mtv_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by mtv_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mtv_exceptions - Custom exceptions used or raised by mtv_api";
__author__="R.D. Vaughan"
__version__="v0.2.0"
# 0.1.0 Initial development
# 0.2.0 Public release

__all__ = ["MtvUrlError", "MtvHttpError", "MtvRssError", "MtvVideoNotFound", "MtvInvalidSearchType", "MtvXmlError", "MtvVideoDetailError", ]

class mtvBaseError(Exception):
    pass

class MtvUrlError(mtvBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class MtvHttpError(mtvBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class MtvRssError(mtvBaseError):
    def __repr__(self):
        return None
    # end __repr__

class MtvVideoNotFound(mtvBaseError):
    def __repr__(self):
        return None
    # end __repr__

class MtvInvalidSearchType(mtvBaseError):
    def __repr__(self):
        return None
    # end __repr__

class MtvXmlError(mtvBaseError):
    def __repr__(self):
        return None
    # end __repr__

class MtvVideoDetailError(mtvBaseError):
    def __repr__(self):
        return None
    # end __repr__
