# -*- coding: utf-8 -*-

"""API utilities."""

from datetime import datetime, timedelta
import logging
import sys
from ._version import __version__

# pylint: disable=no-name-in-module, import-error
try:
    from urllib.parse import quote
except ImportError:
    sys.exit('Unable to import urllib')
# pylint: enable=no-name-in-module, import-error

REC_STATUS_CACHE = {}
REC_TYPE_CACHE = {}
DUP_METHOD_CACHE = {}
UTC_OFFSET = None

LOG = logging.getLogger(__name__)
logging.getLogger(__name__).addHandler(logging.NullHandler())


def url_encode(value=None):
    """
    This is really unnecessary. It's more of a reminder about how to
    use urllib.[parse]quote(). At least as of this writing, 0.28-pre
    doesn't decode the escaped values and the endpoints just get the
    percent encoded text. E.g. don't use it. How show titles with
    & or = in them work isn't clear.

    Input:  A string. E.g. a program's title or anything that has
            special characters like ?, & and UTF characters beyond
            the ASCII set.

    Output: The URL encoded string. E.g. the DEGREE SIGN becomes: %C2%B0
            or a QUESTION MARK becomes %3F.
    """

    if value is None:
        LOG.warning('url_encode() called without any value')
        return value

    return quote(value)


def create_find_time(time=''):
    """
    Normally used to take a starttime and convert it for use in adding
    new recordings. get_utc_offset() should be called before this is, but
    that only needs to be done once.

    Input:  Full UTC timestamp, e.g. 2014-08-12T22:00:00 (with or without
            the trailing 'Z'.)

    Output: Time portion of the above in local time. Or -1 for invalid
            timestamp input.
    """

    time_format = '%Y-%m-%dT%H:%M:%S'

    if not time:
        LOG.error('create_find_time() called without any time')
        return None

    try:
        int(UTC_OFFSET)
        utc_offset = UTC_OFFSET
    except (NameError, TypeError, ValueError):
        LOG.warning('Run get_utc_offset() first. Using UTC offset of 0.')
        utc_offset = 0

    time = time.replace('Z', '')

    try:
        time_stamp = datetime.strptime(time, time_format)
    except (NameError, TypeError, ValueError):
        LOG.error('Invalid timestamp: %s, required: %s.', time, time_format)
        return -1

    return (time_stamp + timedelta(seconds=utc_offset)).strftime('%H:%M:%S')


def utc_to_local(utctime='', omityear=False, omitseconds=True):
    """
    Does exactly that conversion. get_utc_offset() should be run once before
    calling this function. A UTC offset of 0 will be used if UTC_OFFSET
    isn't available, so the function won't abort.

    Inputs:  utctime:     Full UTC timestamp, e.g. 2014-08-12T22:00:00[Z].
             omityear:    If True, then drop the 4 digit year and following -.
             omitseconds: If False, don't return the trailing :SS

    Output: Local time, also a string. Possibly without the year- and always
            without the T between the data/time and no trailing Z.
    """

    try:
        int(UTC_OFFSET)
        utc_offset = UTC_OFFSET
    except (NameError, TypeError, ValueError):
        LOG.warning('Run get_utc_offset() first. Using UTC offset of 0.')
        utc_offset = 0

    if not utctime:
        LOG.error('utc_to_local(): utctime is empty!')
        return None

    utctime = utctime.replace('Z', '').replace('T', ' ')

    try:
        time_stamp = datetime.strptime(utctime, '%Y-%m-%d %H:%M:%S')
    except ValueError:
        LOG.error('utc_to_local(): bad timestamp format!')
        return None

    if omityear:
        fromstring = '%m-%d %H:%M:%S'
    else:
        fromstring = '%Y-%m-%d %H:%M:%S'

    if omitseconds:
        fromstring = fromstring[:-3]

    return (time_stamp + timedelta(seconds=utc_offset)).strftime(fromstring)


def get_utc_offset(backend=None, opts=None):
    """
    Get the backend's offset from UTC. Once retrieved, it's saved value is
    available in UTC_OFFSET and is returned too. Additional calls to this
    function aren't necessary, but if made, won't query the backend again.

    Input:  backend object, optionally opts.

    Output: The offset (in seconds) or -1 and a message prints
    """

    global UTC_OFFSET

    if not backend:
        LOG.error('get_utc_offset(): Error: backend not set.')
        return -1

    try:
        try:
            int(UTC_OFFSET)
            return UTC_OFFSET
        except (NameError, TypeError, ValueError):
            resp_dict = backend.send(endpoint='Myth/GetTimeZone', opts=opts)
            UTC_OFFSET = int(resp_dict['TimeZoneInfo']['UTCOffset'])
            return UTC_OFFSET
    except (RuntimeError, RuntimeWarning) as error:
        LOG.error('get_utc_offset(): warning/failure: %s.', error)
        return -1


def rec_status_to_string(backend=None, rec_status=0, opts=None):
    """
    Convert a signed integer to a Recording Status String
     and cache the result.

    rec_status defaults to 0, which currently (29.0) means 'Unknown'
    """

    if not backend:
        LOG.error('rec_status_to_string(): Error: backend not set.')
        return None

    try:
        try:
            str(REC_STATUS_CACHE[rec_status])
            return REC_STATUS_CACHE[rec_status]
        except (KeyError, NameError, ValueError):
            endpoint = 'Dvr/RecStatusToString'
            rest = f'RecStatus={rec_status}'

            resp_dict = backend.send(endpoint=endpoint, rest=rest, opts=opts)

            REC_STATUS_CACHE[rec_status] = resp_dict['String']

            return REC_STATUS_CACHE[rec_status]
    except (RuntimeError, RuntimeWarning) as error:
        LOG.error('rec_status_to_string(): warning/failure: %s.', error)
        return None


def rec_type_to_string(backend=None, rec_type=0, opts=None):
    """
    Convert a signed integer to a Recording Type String and cache
    the result.

    rec_typedefaults to 0, which currently (29.0) means 'Not Recording'
    """

    if not backend:
        LOG.error('rec_type_to_string(): Error: backend not set.')
        return None

    try:
        try:
            str(REC_TYPE_CACHE[rec_type])
            return REC_TYPE_CACHE[rec_type]
        except (KeyError, NameError, ValueError):
            endpoint = 'Dvr/RecTypeToString'
            rest = f'RecType={rec_type}'

            resp_dict = backend.send(endpoint=endpoint, rest=rest, opts=opts)

            REC_TYPE_CACHE[rec_type] = resp_dict['String']

            return REC_TYPE_CACHE[rec_type]
    except (RuntimeError, RuntimeWarning) as error:
        LOG.error('rec_type_to_string(): warning/failure: %s.', error)
        return None


def dup_method_to_string(backend=None, dup_method=0, opts=None):
    """
    Convert a signed integer to a Duplicate Method String and cache
    the result.

    dup_method defaults to 0, which currently (29.0) means 'No Search'
    """

    if not backend:
        LOG.error('dup_method_to_string(): Error: backend not set.')
        return None

    try:
        try:
            str(DUP_METHOD_CACHE[dup_method])
            return DUP_METHOD_CACHE[dup_method]
        except (KeyError, NameError, ValueError):
            endpoint = 'Dvr/DupMethodToString'
            rest = f'DupMethod={dup_method}'

            resp_dict = backend.send(endpoint=endpoint, rest=rest, opts=opts)

            DUP_METHOD_CACHE[dup_method] = resp_dict['String']

            return DUP_METHOD_CACHE[dup_method]
    except (RuntimeError, RuntimeWarning) as error:
        LOG.error('dup_method_to_string(): warning/failure: %s.', error)
        return None

# vim: set expandtab tabstop=4 shiftwidth=4 smartindent noai colorcolumn=80:
