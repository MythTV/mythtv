#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: giantbomb_exceptions - Custom exceptions used or raised by giantbomb_api
# Python Script
# Author:   R.D. Vaughan
# Purpose:  Custom exceptions used or raised by giantbomb_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="giantbomb_exceptions - Custom exceptions used or raised by giantbomb_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["GiantBombBaseError", "GiantBombHttpError", "GiantBombXmlError",  "GiantBombGameNotFound", ]

# Start of code used to access api.giantbomb.com api
class GiantBombBaseError(Exception):
    pass

class GiantBombHttpError(GiantBombBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class GiantBombXmlError(GiantBombBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class GiantBombGameNotFound(GiantBombBaseError):
    def __repr__(self):
        return None
    # end __repr__
