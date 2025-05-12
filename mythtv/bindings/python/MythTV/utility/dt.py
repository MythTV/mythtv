#------------------------------
# MythTV/utility/datetime.py
# Author: Raymond Wagner
# Description: Provides convenient use of several canned
#              timestamp formats
#------------------------------

from MythTV.exceptions import MythError, MythTZError

from datetime import datetime as _pydatetime, \
                     tzinfo as _pytzinfo, \
                     timedelta
from collections import namedtuple
import os
import re
import time
import sys

IS_PY312plus = (sys.version_info[:3] >= (3, 12, 0))

HAVEZONEINFO = False
try:
    # python 3.9+
    from zoneinfo import ZoneInfo
    HAVEZONEINFO = True
except ImportError:
    HAVEZONEINFO = False

from .singleton import InputSingleton
time.tzset()

class basetzinfo( _pytzinfo ):
    """
    Base timezone class that provides the methods required for interaction
    with Python datetime utilities.  This class depends on subclasses to
    populate the proper data fields with transition information.
    """
    _Transition = namedtuple('Transition', \
                             'time utc local offset abbrev isdst')

    def _get_transition(self, dt=None):
        if len(self._transitions) == 0:
            self._get_transition = self._get_transition_empty
        elif len(self._transitions) == 1:
            self._get_transition = self._get_transition_single
        else:
            self.__last = 0
            self._compute_ranges()
            self._get_transition = self._get_transition_search
        return self._get_transition(dt)

    def _get_transition_search(self, dt=None):
        if dt is None:
            dt = _pydatetime.now()
        dt = (dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second)

        index = self.__last
        direction = 0
        oob_range = False
        while True:
            if dt < self._ranges[index][0]:
                if direction == 1:
                    # gap found, use later transition
                    break
                else:
                    index -= 1
                    direction = -1
            elif dt >= self._ranges[index][1]:
                if direction == -1:
                    # gap found, use later transition
                    index += 1
                    break
                else:
                    index += 1
                    direction = 1
            else:
                break

            if index >= len(self._ranges):
                # out of bounds future, use final transition
                index = len(self._ranges) - 1
                oob_range = True
                break
            elif index < 0:
                # out of bounds past, undefined time frame
                raise MythTZError(MythTZError.TZ_CONVERSION_ERROR,
                                  self.tzname(), dt)

        self.__last = index
        if oob_range:
            # out of bounds future, use final transition
            return self._transitions[self._ranges[index][2] + 1]
        return self._transitions[self._ranges[index][2]]

    def _get_transition_empty(self, dt=None):
        return self._Transition(0, None, None, 0, 'UTC', False)
    def _get_transition_single(self, dt=None):
        return self._transitions[0]

    def _compute_ranges(self):
        self._ranges = []
        for i in range(0, len(self._transitions)-1):
            p,n = self._transitions[i:i+2]
            rstart = p.local[0:5]

            # ToDo: re-evaluate this for 'all year in dst' zones
            offset = p.offset - n.offset
            rend = list(n.local[0:5])
            rend[4] += offset//60
            if (0 > rend[4]) or (rend[4] > 59):
                rend[3] += rend[4]//60
                rend[4] = rend[4]%60
            if (0 > rend[3]) or (rend[3] > 23):
                rend[2] += rend[3]//24
                rend[3] = rend[3]%24

            self._ranges.append((rstart, tuple(rend), i))

    def utcoffset(self, dt=None):
        return timedelta(0, self._get_transition(dt).offset)

    def dst(self, dt=None):
        transition = self._get_transition(dt)
        if transition.isdst:
            return timedelta(0,3600) # may need to find a more accurate value
        return timedelta(0)

    def tzname(self, dt=None):
        return self._get_transition(dt).abbrev

#class sqltzinfo( basetzinfo ):
#    """
#    Customized timezone class that can import timezone data from the MySQL
#    database.
#    """

class posixtzinfo(basetzinfo, metaclass=InputSingleton):
    """
    Customized timezone class that can import timezone data from the local
    POSIX zoneinfo files.
    """
    _Count = namedtuple('Count', \
                        'gmt_indicators std_indicators leap_seconds '+\
                        'transitions types abbrevs')

    @staticmethod
    def _get_version(fd):
        """Confirm zoneinfo file magic string, and return version number."""
        if fd.read(4) != b'TZif':
            raise MythTZError(MythTZError.TZ_INVALID_FILE)
        version = fd.read(1) # read version number
        fd.seek(15, 1) # skip reserved bytes
        if version == '\0':
            return 1
        else:
            return int(version) # should be 2 or 3

    def _process(self, fd, version=1, skip=False):
        from struct import unpack, calcsize
        if version == 1:
            ttmfmt = '!l'
            lfmt = '!ll'
        elif version == 2 or version == 3:
            ttmfmt = '!q'
            lfmt = '!ql'
        else:
            raise MythTZError(MythTZError.TZ_VERSION_ERROR)

        counts = self._Count(*unpack('!llllll', fd.read(24)))
        if skip:
            fd.seek(counts.transitions * (calcsize(ttmfmt)+1) +\
                    counts.types * 6 +\
                    counts.abbrevs +\
                    counts.leap_seconds * calcsize(lfmt) +\
                    counts.std_indicators +\
                    counts.gmt_indicators,\
                    1)
            return

        transitions = [None] * counts.transitions
        # First transition which is in range for gmtime. Some zoneinfo
        # files have massively negative leading entries for e.g. the
        # big bang which gmtime() cannot cope with.
        first_modern_transition = None
        i = 0   # assign i, in case the for-loop is not executed:
        for i in range(counts.transitions):  # read in epoch time data
            t = unpack(ttmfmt, fd.read(calcsize(ttmfmt)))[0]

            try:
                tt = time.gmtime(t)
                transitions[i] = ([t, tt, None, None, None, None])
                if first_modern_transition is None:
                    first_modern_transition = i
            except (ValueError, OSError) as e:
                # ValueError is only accepted until we have seen a modern
                # transition.
                if first_modern_transition is not None:
                    raise e

        # Special case if there are no modern transitions, like e.g. UTC timezone:
        if ( (i == 0) and first_modern_transition is None ):
            first_modern_transition = counts.transitions

        # read in transition type indexes
        types = [None]*counts.transitions
        for i in range(counts.transitions):
            types[i] = t = unpack('!b', fd.read(1))[0]
            if t >= counts.types:
                raise MythTZError(MythTZError.TZ_INVALID_TRANSITION,
                                  t, counts.types-1)

        # read in type definitions
        typedefs = []
        for i in range(counts.types):
            offset, isdst, _ = unpack('!lbB', fd.read(6))
            typedefs.append([offset, isdst])
        for i in range(first_modern_transition, counts.transitions):
            offset, isdst = typedefs[types[i]]
            transitions[i][2] = time.gmtime(transitions[i][0] + offset)
            transitions[i][3] = offset
            transitions[i][5] = isdst

        # read in type names
        for i, name in enumerate(fd.read(counts.abbrevs)[:-1].split(b'\0')):
            for j in range(first_modern_transition, counts.transitions):
                if types[j] == i:
                    transitions[j][4] = name

        # skip leap second definitions
        fd.seek(counts.leap_seconds + calcsize(lfmt), 1)
        # skip std/wall indicators
        fd.seek(counts.std_indicators, 1)
        # skip utc/local indicators
        fd.seek(counts.gmt_indicators, 1)

        transitions = [self._Transition(*x)
                       for x
                       in transitions[first_modern_transition:]]
        self._transitions = tuple(transitions)


    def __init__(self, name=None):
        if name:
            fd = open('/usr/share/zoneinfo/' + name, 'rb')
        elif os.getenv('TZ'):
            fd = open('/usr/share/zoneinfo/' + os.getenv('TZ'), 'rb')
        else:
            fd = open('/etc/localtime', 'rb')

        version = self._get_version(fd)
        if version == 2 or version == 3:
            self._process(fd, skip=True)
            self._get_version(fd)
        elif version > 3:
            raise MythTZError(MythTZError.TZ_VERSION_ERROR)
        self._process(fd, version)
        fd.close()

class offsettzinfo( _pytzinfo ):
    """Customized timezone class that provides a simple static offset."""
    @classmethod
    def local(cls):
        return cls(sec=-time.timezone)
    def __init__(self, direc='+', hr=0, min=0, sec=0):
        sec = int(sec) + 60 * (int(min) + 60 * int(hr))
        if direc == '-':
            sec = -1*sec
        self._offset = timedelta(seconds=sec)
    def utcoffset(self, dt): return self._offset
    def tzname(self, dt): return ''
    def dst(self, dt): return timedelta(0)

class datetime( _pydatetime ):
    """
    Customized datetime class offering canned import and export of several
    common time formats, and 'duck' importing between them.
    """
    global HAVEZONEINFO
    _reiso = re.compile('(?P<year>[0-9]{4})'
                       '-(?P<month>[0-9]{1,2})'
                       '-(?P<day>[0-9]{1,2})'
                        '.'
                        '(?P<hour>[0-9]{2})'
                       ':(?P<min>[0-9]{2})'
                       '(:(?P<sec>[0-9]{2}))?'
                        '( )?(?P<tz>Z|'
                            '(?P<tzdirec>[-+])'
                            '(?P<tzhour>[0-9]{1,2})'
                            '(:)?'
                            '(?P<tzmin>[0-9]{2})?'
                        ')?')
    _rerfc = re.compile('(?P<dayname>[a-zA-Z]{3}), '
                        '(?P<day>[0-9]{1,2}) '
                        '(?P<month>[a-zA-Z]{3}) '
                        '(?P<year>[0-9]{2,4}) '
                        '(?P<hour>[0-9]{2})'
                       ':(?P<min>[0-9]{2})'
                      '(:(?P<sec>[0-9]{2}))?'
                        '( )?(?P<tz>[A-Z]{2,3}|'
                            '(?P<tzdirec>[-+])'
                            '(?P<tzhour>[0-9]{2})'
                            '(?P<tzmin>[0-9]{2})'
                        ')?')
    _localtz = None
    _utctz = None

    @classmethod
    def localTZ(cls):
        global HAVEZONEINFO
        if cls._localtz is None:
            if HAVEZONEINFO:
                try:
                    if os.getenv('TZ'):
                        cls._localtz = ZoneInfo(os.getenv('TZ'))
                    elif os.path.exists('/usr/share/zoneinfo/localtime'):
                        cls._localtz = ZoneInfo('localtime')
                    else:
                        with open('/etc/localtime', 'rb') as zfile:
                            cls._localtz = ZoneInfo.from_file(zfile, 'localtime')
                except:
                    HAVEZONEINFO = False
            if not HAVEZONEINFO:
                try:
                    cls._localtz = posixtzinfo()
                except:
                    cls._localtz = offsettzinfo.local()
        return cls._localtz

    @classmethod
    def UTCTZ(cls):
        global HAVEZONEINFO
        if cls._utctz is None:
            if HAVEZONEINFO:
                try:
                    cls._utctz = ZoneInfo('Etc/UTC')
                except:
                    HAVEZONEINFO = False
            if not HAVEZONEINFO:
                try:
                    cls._utctz = posixtzinfo('Etc/UTC')
                except:
                    cls._utctz = offsettzinfo()
        return cls._utctz

    @classmethod
    def fromDatetime(cls, dt, tzinfo=None, fold=None):
        if tzinfo is None:
            tzinfo = dt.tzinfo
            fold = dt.fold
        return cls(dt.year, dt.month, dt.day, dt.hour, dt.minute,
                   dt.second, dt.microsecond, tzinfo, fold)

# override existing classmethods to enforce use of timezone
    @classmethod
    def now(cls, tz=None):
        if tz is None:
            tz = cls.localTZ()
        obj = super(datetime, cls).now(tz)
        return cls.fromDatetime(obj)

    @classmethod
    def utcnow(cls):
        if IS_PY312plus:
            return cls.now(tz=cls.UTCTZ())
        else:
            obj = super(datetime, cls).utcnow()
            return obj.replace(tzinfo=cls.UTCTZ())

    @classmethod
    def fromtimestamp(cls, timestamp, tz=None):
        if tz is None:
            tz = cls.localTZ()
        obj = super(datetime, cls).fromtimestamp(float(timestamp), tz)
        return cls.fromDatetime(obj)

    @classmethod
    def utcfromtimestamp(cls, timestamp):
        if IS_PY312plus:
            return cls.fromtimestamp(timestamp, tz=cls.UTCTZ())
        else:
            obj = super(datetime, cls).utcfromtimestamp(float(timestamp))
            return obj.replace(tzinfo=cls.UTCTZ())

    @classmethod
    def strptime(cls, datestring, format, tzinfo=None):
        obj = super(datetime, cls).strptime(datestring, format)
        return cls.fromDatetime(obj, tzinfo)

# new class methods for interfacing with MythTV
    @classmethod
    def fromnaiveutc(cls, dt):
        return cls.fromDatetime(dt).replace(tzinfo=cls.UTCTZ())\
                                   .astimezone(cls.localTZ())

    @classmethod
    def frommythtime(cls, mtime, tz=None):
        if tz in ('UTC', 'Etc/UTC'):
            tz = cls.UTCTZ()
        elif tz is None:
            tz = cls.localTZ()
        return cls.strptime(str(mtime), '%Y%m%d%H%M%S', tz)

    @classmethod
    def fromIso(cls, isotime, sep='T', tz=None):
        match = cls._reiso.match(isotime)
        if match is None:
            raise TypeError("time data '{0}' does not match ISO 8601 format" \
                                .format(isotime))

        dt = [int(a) for a in match.groups()[:5]]
        if match.group('sec') is not None:
            dt.append(int(match.group('sec')))
        else:
            dt.append(0)

        # microseconds
        dt.append(0)

        if match.group('tz'):
            if match.group('tz') == 'Z':
                tz = cls.UTCTZ()
            elif match.group('tzmin'):
                tz = offsettzinfo(*match.group('tzdirec','tzhour','tzmin'))
            else:
                tz = offsettzinfo(*match.group('tzdirec','tzhour'))
        elif tz in ('UTC', 'Etc/UTC'):
            tz = cls.UTCTZ()
        elif tz is None:
            tz = cls.localTZ()
        dt.append(tz)

        return cls(*dt)

    @classmethod
    def fromRfc(cls, rfctime, tz=None):
        match = cls._rerfc.match(rfctime)
        if match is None:
            raise TypeError("time data '{0}' does not match RFC 822 format"\
                                .format(rfctime))

        year = int(match.group('year'))
        if year > 100:
            dt = [year]
        elif year > 30:
            dt = [1900+year]
        else:
            dt = [2000+year]
        dt.append(['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul',
                   'Aug', 'Sep', 'Oct', 'Nov', 'Dec']\
                        .index(match.group('month'))+1)
        dt.append(int(match.group('day')))

        dt.append(int(match.group('hour')))
        dt.append(int(match.group('min')))
        if match.group('sec') is not None:
            dt.append(int(match.group('sec')))
        else:
            dt.append(0)

        # microseconds
        dt.append(0)

        if match.group('tz'):
            if match.group('tz') in ('UT', 'GMT'):
                tz = cls.UTCTZ()
            elif match.group('tz') == 'EDT':
                tz = offsettzinfo(hr=-4)
            elif match.group('tz') in ('EST', 'CDT'):
                tz = offsettzinfo(hr=-5)
            elif match.group('tz') in ('CST', 'MDT'):
                tz = offsettzinfo(hr=-6)
            elif match.group('tz') in ('MST', 'PDT'):
                tz = offsettzinfo(hr=-7)
            elif match.group('tz') == 'PST':
                tz = offsettzinfo(hr=-8)
            elif match.group('tzmin'):
                tz = offsettzinfo(*match.group('tzdirec','tzhour','tzmin'))
            else:
                tz = offsettzinfo(*match.group('tzdirec','tzhour'))
        elif tz in ('UTC', 'Etc/UTC'):
            tz = cls.UTCTZ()
        elif tz is None:
            tz = cls.localTZ()
        dt.append(tz)

        return cls(*dt)

    @classmethod
    def duck(cls, t):
        try:
            # existing modified datetime
            t.mythformat
            return t
        except: pass
        try:
            # existing built-in datetime
            return cls.fromDatetime(t)
        except: pass
        for func in [cls.fromtimestamp, #epoch time
                     cls.frommythtime, #iso time with integer characters only
                     cls.fromIso, #iso 8601 time
                     cls.fromRfc]: #rfc 822 time
            try:
                return func(t)
            except:
                pass
        raise TypeError("time data '%s' does not match supported formats"%t)

    def __new__(cls, year, month, day, hour=None, minute=None, second=None,
                      microsecond=None, tzinfo=None, fold=None):

        if tzinfo is None:
            kwargs = {'tzinfo':cls.localTZ()}
        else:
            kwargs = {'tzinfo':tzinfo}
        if hour is not None:
            kwargs['hour'] = hour
        if minute is not None:
            kwargs['minute'] = minute
        if second is not None:
            kwargs['second'] = second
        if microsecond is not None:
            kwargs['microsecond'] = microsecond
        if fold is not None:
            kwargs['fold'] = fold
        return _pydatetime.__new__(cls, year, month, day, **kwargs)

    def mythformat(self):
        return self.astimezone(self.UTCTZ()).strftime('%Y%m%d%H%M%S')

    def rfcformat(self):
        return self.strftime('%a, %d %b %Y %H:%M:%S %z')

    def utcrfcformat(self):
        return self.astimezone(self.UTCTZ()).strftime('%a, %d %b %Y %H:%M:%S')

    def utcisoformat(self):
        return self.astimezone(self.UTCTZ()).isoformat().split('+')[0]

    def astimezone(self, tz):
        return self.fromDatetime(super(datetime, self).astimezone(tz))

    def asnaiveutc(self):
        return self.astimezone(self.UTCTZ()).replace(tzinfo=None)
