# -*- coding: UTF-8 -*-

# ----------------------
# Name: mashups_exceptions - Custom exceptions used or raised by mashups_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by mashups_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mashups_exceptions - Custom exceptions used or raised by mashups_api";
__author__="R.D. Vaughan"
__version__="v0.1.0"
# 0.1.0 Initial development

__all__ = ["MashupsUrlError", "MashupsHttpError", "MashupsRssError", "MashupsVideoNotFound", "MashupsConfigFileError", "MashupsUrlDownloadError"]

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

class MashupsConfigFileError(MashupsBaseError):
    def __repr__(self):
        return None
    # end __repr__

class MashupsUrlDownloadError(MashupsBaseError):
    def __repr__(self):
        return None
    # end __repr__
