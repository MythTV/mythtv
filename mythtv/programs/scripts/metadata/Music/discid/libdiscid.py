# Copyright (C) 2013  Johannes Dewender
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Please submit bug reports to GitHub:
# https://github.com/JonnyJD/python-discid/issues
"""libdiscid dynamic loading code and constants

The code that works with Disc objects is in disc.py
"""

import os
import sys
import ctypes
from ctypes import c_void_p, c_char_p
from ctypes.util import find_library

from discid.util import _encode, _decode

_LIB_BASE_NAME = "discid"
_LIB_MAJOR_VERSION = 0


def _find_library(name, version=0):
    """Find a library by base-name and major version
    """
    windows_names = ["%s.dll" % name, "lib%s.dll" % name,
                     "lib%s-%d.dll" % (name, version)]

    lib_file = None

    # force prefer current folder
    # for linux/UNIX-like and Windows
    if sys.platform in ["darwin", "cygwin"]:
        # these will already work fine with find_library
        pass
    elif sys.platform == "win32":
        for lib_name in windows_names:
            if os.path.isfile(lib_name):
                lib_file = lib_name
                break
    else:
        # that would be linux/UNIX-like
        # these need to prepend ./
        lib_name = "./lib%s.so.%d" % (name, version)
        if os.path.isfile(lib_name):
            lib_file = lib_name

    # doesn't work on Windows
    # Darwin gives a full path when found system-wide
    # Linux and Cygwin give base filename when found
    # Darwin and Cygwin will find and prefer in current folder
    if lib_file is None:
        lib_file = find_library(name)

    # Windows needs complete filenames
    # and gives a full path when found system-wide
    # also searches in current folder, but system-wide preferred
    if lib_file is None and sys.platform == "win32":
        for lib_name in windows_names:
            if lib_file is None:
                lib_file = find_library(lib_name)

    if lib_file is None:
        # this won't help anymore,
        # but gives a nice platform dependent file in the error message
        if sys.platform == "win32":
            lib_file = "%s.dll" % name
        elif sys.platform == "darwin":
            lib_file = "lib%s.%d.dylib" % (name, version)
        elif sys.platform == "cygwin":
            lib_file = "cyg%s-%d.dll" % (name, version)
        else:
            lib_file = "lib%s.so.%d" % (name, version)

    return lib_file

def _open_library(lib_name):
    """Open a library by name or location
    """
    try:
        return ctypes.cdll.LoadLibrary(lib_name)
    except OSError as exc:
        if lib_name not in str(exc):
            # replace uninformative Error on Windows
            raise OSError("could not find libdiscid: %s" % lib_name)
        else:
            raise

_LIB_NAME = _find_library(_LIB_BASE_NAME, _LIB_MAJOR_VERSION)
_LIB = _open_library(_LIB_NAME)



try:
    _LIB.discid_get_version_string.argtypes = ()
    _LIB.discid_get_version_string.restype = c_char_p
except AttributeError:
    pass
def _get_version_string():
    """Get the version string of libdiscid
    """
    try:
        version_string = _LIB.discid_get_version_string()
    except AttributeError:
        return "libdiscid < 0.4.0"
    else:
        return _decode(version_string)

_LIB.discid_get_default_device.argtypes = ()
_LIB.discid_get_default_device.restype = c_char_p
def get_default_device():
    """The default device to use for :func:`read` on this platform
    given as a :obj:`unicode` or :obj:`str <python:str>` object.
    """
    device = _LIB.discid_get_default_device()
    return _decode(device)

try:
    _LIB.discid_get_feature_list.argtypes = (c_void_p, )
    _LIB.discid_get_feature_list.restype = None
except AttributeError:
    _features_available = False
else:
    _features_available = True
def _get_features():
    """Get the supported features for the platform.
    """
    features = []
    if _features_available:
        c_features = (c_char_p * 32)()
        _LIB.discid_get_feature_list(c_features)
        for feature in c_features:
            if feature:
                features.append(_decode(feature))
    else:
        # libdiscid <= 0.4.0
        features = ["read"]     # no generic platform yet
        try:
            # test for ISRC/MCN API (introduced 0.3.0)
            _LIB.discid_get_mcn
        except AttributeError:
            pass
        else:
            # ISRC/MCN API found -> libdiscid = 0.3.x
            if (sys.platform.startswith("linux") and
                    not os.path.isfile("/usr/lib/libdiscid.so.0.3.0")
                    and not os.path.isfile("/usr/lib64/libdiscid.so.0.3.0")):
                features += ["mcn", "isrc"]
            elif sys.platform in ["darwin", "win32"]:
                features += ["mcn", "isrc"]

    return features

LIBDISCID_VERSION_STRING = _get_version_string()

FEATURES = _get_features()


# vim:set shiftwidth=4 smarttab expandtab:
