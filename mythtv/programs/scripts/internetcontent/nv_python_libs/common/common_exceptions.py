#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: common_exceptions - Custom exceptions used or raised by WebCgi_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by common_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="common_exceptions - Custom exceptions used or raised by common_api.py";
__author__="R.D. Vaughan"
__version__="v0.1.2"
# 0.1.0 Initial development
# 0.1.1 Changes due to abanding the EMML Engine/Tomcat server and the addition of the common function class
# 0.1.2 Changed naming convention to WebCgi

__all__ = ["WebCgiUrlError", "WebCgiHttpError", "WebCgiRssError", "WebCgiVideoNotFound", "WebCgiInvalidSearchType", "WebCgiXmlError", "WebCgiVideoDetailError", "WebCgiCategoryNotFound", ]

class WebCgiBaseError(Exception):
    pass

class WebCgiUrlError(WebCgiBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class WebCgiHttpError(WebCgiBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class WebCgiRssError(WebCgiBaseError):
    def __repr__(self):
        return None
    # end __repr__

class WebCgiVideoNotFound(WebCgiBaseError):
    def __repr__(self):
        return None
    # end __repr__

class WebCgiInvalidSearchType(WebCgiBaseError):
    def __repr__(self):
        return None
    # end __repr__

class WebCgiXmlError(WebCgiBaseError):
    def __repr__(self):
        return None
    # end __repr__
