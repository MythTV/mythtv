#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mnvsearch_exceptions - Custom exceptions used or raised by mnvsearch_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by mnvsearch_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mnvsearch_exceptions - Custom exceptions used or raised by mnvsearch_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["MNVSQLError", "MNVVideoNotFound", ]

class MNVBaseError(Exception):
    pass

class MNVSQLError(MNVBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class MNVVideoNotFound(MNVBaseError):
    def __repr__(self):
        return None
    # end __repr__
