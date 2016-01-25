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
"""Disc class
"""

import re
from ctypes import c_int, c_void_p, c_char_p, c_uint

from discid.libdiscid import _LIB, FEATURES
from discid.util import _encode, _decode, _sectors_to_seconds
from discid.track import Track


# our implemented of libdiscid's enum discid_feature
_FEATURE_MAPPING = {"read": 1 << 0, "mcn": 1 << 1, "isrc": 1 << 2}


FEATURES_IMPLEMENTED = list(_FEATURE_MAPPING.keys())

def read(device=None, features=[]):
    """Reads the TOC from the device given as string
    and returns a :class:`Disc` object.

    That string can be either of:
    :obj:`str <python:str>`, :obj:`unicode` or :obj:`bytes`.
    However, it should in no case contain non-ASCII characters.
    If no device is given, a default, also given by :func:`get_default_device`
    is used.

    You can optionally add a subset of the features in
    :data:`FEATURES` or the whole list to read more than just the TOC.
    In contrast to libdiscid, :func:`read` won't read any
    of the additional features by default.

    A :exc:`DiscError` exception is raised when the reading fails,
    and :exc:`NotImplementedError` when libdiscid doesn't support
    reading discs on the current platform.
    """
    disc = Disc()
    disc.read(device, features)
    return disc

def put(first, last, disc_sectors, track_offsets):
    """Creates a TOC based on the information given
    and returns a :class:`Disc` object.

    Takes the `first` track and `last` **audio** track as :obj:`int`.
    `disc_sectors` is the end of the last audio track,
    normally the total sector count of the disc.
    `track_offsets` is a list of all audio track offsets.

    Depending on how you get the total sector count,
    you might have to substract 11400 (2:32 min.) for discs with data tracks.

    A :exc:`TOCError` exception is raised when illegal parameters
    are provided.

    .. seealso:: :musicbrainz:`Disc ID Calculation`
    """
    disc = Disc()
    disc.put(first, last, disc_sectors, track_offsets)
    return disc


class DiscError(IOError):
    """:func:`read` will raise this exception when an error occured.
    """
    pass

class TOCError(Exception):
    """:func:`put` will raise this exception when illegal paramaters
    are provided.
    """
    pass


class Disc(object):
    """The class of the object returned by :func:`read` or :func:`put`.
    """

    _LIB.discid_new.argtypes = ()
    _LIB.discid_new.restype = c_void_p
    def __init__(self):
        """The initialization will reserve some memory
        for internal data structures.
        """
        self._handle = c_void_p(_LIB.discid_new())
        self._success = False
        self._requested_features = []
        assert self._handle.value is not None

    def __str__(self):
        assert self._success
        return self.id

    _LIB.discid_get_error_msg.argtypes = (c_void_p, )
    _LIB.discid_get_error_msg.restype = c_char_p
    def _get_error_msg(self):
        """Get the error message for the last error with the object.
        """
        error = _LIB.discid_get_error_msg(self._handle)
        return _decode(error)


    _LIB.discid_read.argtypes = (c_void_p, c_char_p)
    _LIB.discid_read.restype = c_int
    try:
        _LIB.discid_read_sparse.argtypes = (c_void_p, c_char_p, c_uint)
        _LIB.discid_read_sparse.restype = c_int
    except AttributeError:
        pass
    def read(self, device=None, features=[]):
        """Reads the TOC from the device given as string

        The user is supposed to use :func:`discid.read`.
        """
        if "read" not in FEATURES:
            raise NotImplementedError("discid_read not implemented on platform")

        # only use features implemented on this platform and in this module
        self._requested_features = list(set(features) & set(FEATURES)
                                        & set(FEATURES_IMPLEMENTED))

        # create the bitmask for libdiscid
        c_features = 0
        for feature in features:
            c_features += _FEATURE_MAPPING.get(feature, 0)

        # device = None will use the default device (internally)
        try:
            result = _LIB.discid_read_sparse(self._handle, _encode(device),
                                             c_features) == 1
        except AttributeError:
            result = _LIB.discid_read(self._handle, _encode(device)) == 1
        self._success = result
        if not self._success:
            raise DiscError(self._get_error_msg())
        return self._success

    _LIB.discid_put.argtypes = (c_void_p, c_int, c_int, c_void_p)
    _LIB.discid_put.restype = c_int
    def put(self, first, last, disc_sectors, track_offsets):
        """Creates a TOC based on the input given.

        The user is supposed to use :func:`discid.put`.
        """
        # check for common usage errors
        if len(track_offsets) != last - first + 1:
            raise TOCError("Invalid number of track offsets")
        elif False in [disc_sectors >= off for off in track_offsets]:
            raise TOCError("Disc sector count too low")

        # only the "read" (= TOC) feature is supported by put
        self._requested_features = ["read"]

        offsets = [disc_sectors] + track_offsets
        c_offsets = (c_int * len(offsets))(*tuple(offsets))
        result = _LIB.discid_put(self._handle, first, last, c_offsets) == 1
        self._success = result
        if not self._success:
            raise TOCError(self._get_error_msg())
        return self._success


    _LIB.discid_get_id.argtypes = (c_void_p, )
    _LIB.discid_get_id.restype = c_char_p
    def _get_id(self):
        """Gets the current MusicBrainz disc ID
        """
        assert self._success
        result = _LIB.discid_get_id(self._handle)
        return _decode(result)

    _LIB.discid_get_freedb_id.argtypes = (c_void_p, )
    _LIB.discid_get_freedb_id.restype = c_char_p
    def _get_freedb_id(self):
        """Gets the current FreeDB disc ID
        """
        assert self._success
        result = _LIB.discid_get_freedb_id(self._handle)
        return _decode(result)

    _LIB.discid_get_submission_url.argtypes = (c_void_p, )
    _LIB.discid_get_submission_url.restype = c_char_p
    def _get_submission_url(self):
        """Give an URL to submit the current TOC
        as a new Disc ID to MusicBrainz.
        """
        assert self._success
        result = _LIB.discid_get_submission_url(self._handle)
        return _decode(result)

    try:
        _LIB.discid_get_toc_string.argtypes = (c_void_p, )
        _LIB.discid_get_toc_string.restype = c_char_p
    except AttributeError:
        pass
    def _get_toc_string(self):
        """The TOC suitable as value of the `toc parameter`
        when accessing the MusicBrainz Web Service.
        """
        assert self._success
        try:
            result = _LIB.discid_get_toc_string(self._handle)
        except AttributeError:
            return None
        else:
            return _decode(result)

    _LIB.discid_get_first_track_num.argtypes = (c_void_p, )
    _LIB.discid_get_first_track_num.restype = c_int
    def _get_first_track_num(self):
        """Gets the first track number
        """
        assert self._success
        return _LIB.discid_get_first_track_num(self._handle)

    _LIB.discid_get_last_track_num.argtypes = (c_void_p, )
    _LIB.discid_get_last_track_num.restype = c_int
    def _get_last_track_num(self):
        """Gets the last track number
        """
        assert self._success
        return _LIB.discid_get_last_track_num(self._handle)

    _LIB.discid_get_sectors.argtypes = (c_void_p, )
    _LIB.discid_get_sectors.restype = c_int
    def _get_sectors(self):
        """Gets the total number of sectors on the disc
        """
        assert self._success
        return _LIB.discid_get_sectors(self._handle)

    try:
        _LIB.discid_get_mcn.argtypes = (c_void_p, )
        _LIB.discid_get_mcn.restype = c_char_p
    except AttributeError:
        pass
    def _get_mcn(self):
        """Gets the current Media Catalogue Number (MCN/UPC/EAN)
        """
        assert self._success
        if "mcn" in self._requested_features:
            try:
                result = _LIB.discid_get_mcn(self._handle)
            except AttributeError:
                return None
            else:
                return _decode(result)
        else:
            return None


    @property
    def id(self):
        """This is the MusicBrainz :musicbrainz:`Disc ID`,
        a :obj:`unicode` or :obj:`str <python:str>` object.
        """
        return self._get_id()

    @property
    def freedb_id(self):
        """This is the :musicbrainz:`FreeDB` Disc ID (without category),
        a :obj:`unicode` or :obj:`str <python:str>` object.
        """
        return self._get_freedb_id()

    @property
    def submission_url(self):
        """Disc ID / TOC Submission URL for MusicBrainz

        With this url you can submit the current TOC
        as a new MusicBrainz :musicbrainz:`Disc ID`.
        This is a :obj:`unicode` or :obj:`str <python:str>` object.
        """
        url = self._get_submission_url()
        if url is None:
            return None
        else:
            # update submission url, which saves a couple of redirects
            url = url.replace("//mm.", "//")
            url = url.replace("/bare/cdlookup.html", "/cdtoc/attach")
            return url

    @property
    def toc_string(self):
        """The TOC suitable as value of the `toc parameter`
        when accessing the MusicBrainz Web Service.

        This is a :obj:`unicode` or :obj:`str <python:str>` object
        and enables fuzzy searching when the actual Disc ID is not found.

        Note that this is the unencoded value, which still contains spaces.

        .. seealso:: `MusicBrainz Web Service <http://musicbrainz.org/doc/Development/XML_Web_Service/Version_2#discid>`_
        """
        toc_string = self._get_toc_string()
        if toc_string is None and self.submission_url:
            # probably an old version of libdiscid (< 0.6.0)
            # gather toc string from submission_url
            match = re.search("toc=([0-9+]+)", self.submission_url)
            if match is None:
                raise ValueError("can't get toc string from submission url")
            else:
                return match.group(1).replace("+", " ")
        else:
            return toc_string

    @property
    def first_track_num(self):
        """Number of the first track"""
        return self._get_first_track_num()

    @property
    def last_track_num(self):
        """Number of the last **audio** track"""
        return self._get_last_track_num()

    @property
    def sectors(self):
        """Total length in sectors"""
        return self._get_sectors()

    length = sectors
    """This is an alias for :attr:`sectors`"""

    @property
    def seconds(self):
        """Total length in seconds"""
        if self.sectors is None:
            return None
        else:
            return _sectors_to_seconds(self.sectors)

    @property
    def mcn(self):
        """This is the Media Catalogue Number (MCN/UPC/EAN)

        It is set after the `"mcn"` feature was requested on a read
        and supported by the platform or :obj:`None`.
        If set, this is a :obj:`unicode` or :obj:`str <python:str>` object.
        """
        return self._get_mcn()

    @property
    def tracks(self):
        """A list of :class:`Track` objects for this Disc.
        """
        tracks = []
        assert self._success
        for number in range(self.first_track_num, self.last_track_num + 1):
            tracks.append(Track(self, number))
        return tracks


    _LIB.discid_free.argtypes = (c_void_p, )
    _LIB.discid_free.restype = None
    def _free(self):
        """This will free the internal allocated memory for the object.
        """
        _LIB.discid_free(self._handle)
        self._handle = None

    def __enter__(self):
        """deprecated :keyword:`with` usage"""
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        """deprecated :keyword:`with` usage"""
        pass

    def __del__(self):
        self._free()


# vim:set shiftwidth=4 smarttab expandtab:
