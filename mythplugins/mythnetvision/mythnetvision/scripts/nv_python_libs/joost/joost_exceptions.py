#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: joost_exceptions - Custom exceptions used or raised by joost_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by joost_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="joost_exceptions - Custom exceptions used or raised by joost_api";
__author__="R.D. Vaughan"
__version__="v0.2.0"
# 0.1.0 Initial development
# 0.1.1 Documentation updates
# 0.2.0 Public release

__all__ = ["JoostUrlError", "JoostHttpError", "JoostRssError", "JoostVideoNotFound", "JoostInvalidSearchType", "JoostXmlError", "JoostVideoDetailError", ]

class joostBaseError(Exception):
    pass

class JoostUrlError(joostBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class JoostHttpError(joostBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class JoostRssError(joostBaseError):
    def __repr__(self):
        return None
    # end __repr__

class JoostVideoNotFound(joostBaseError):
    def __repr__(self):
        return None
    # end __repr__

class JoostInvalidSearchType(joostBaseError):
    def __repr__(self):
        return None
    # end __repr__

class JoostXmlError(joostBaseError):
    def __repr__(self):
        return None
    # end __repr__

class JoostVideoDetailError(joostBaseError):
    def __repr__(self):
        return None
    # end __repr__
