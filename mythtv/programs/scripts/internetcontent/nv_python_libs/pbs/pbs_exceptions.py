#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: pbs_exceptions - Custom exceptions used or raised by pbs_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by pbs_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="pbs_exceptions - Custom exceptions used or raised by pbs_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["PBSUrlError", "PBSHttpError", "PBSRssError", "PBSVideoNotFound", "PBSConfigFileError", "PBSUrlDownloadError"]

class PBSBaseError(Exception):
    pass

class PBSUrlError(PBSBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class PBSHttpError(PBSBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class PBSRssError(PBSBaseError):
    def __repr__(self):
        return None
    # end __repr__

class PBSVideoNotFound(PBSBaseError):
    def __repr__(self):
        return None
    # end __repr__

class PBSConfigFileError(PBSBaseError):
    def __repr__(self):
        return None
    # end __repr__

class PBSUrlDownloadError(PBSBaseError):
    def __repr__(self):
        return None
    # end __repr__
