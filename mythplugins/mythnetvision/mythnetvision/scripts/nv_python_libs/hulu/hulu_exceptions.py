#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: hulu_exceptions - Custom exceptions used or raised by hulu_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by hulu_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="hulu_exceptions - Custom exceptions used or raised by hulu_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["HuluUrlError", "HuluHttpError", "HuluRssError", "HuluVideoNotFound", "HuluConfigFileError", "HuluUrlDownloadError"]

class HuluBaseError(Exception):
    pass

class HuluUrlError(HuluBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class HuluHttpError(HuluBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class HuluRssError(HuluBaseError):
    def __repr__(self):
        return None
    # end __repr__

class HuluVideoNotFound(HuluBaseError):
    def __repr__(self):
        return None
    # end __repr__

class HuluConfigFileError(HuluBaseError):
    def __repr__(self):
        return None
    # end __repr__

class HuluUrlDownloadError(HuluBaseError):
    def __repr__(self):
        return None
    # end __repr__
