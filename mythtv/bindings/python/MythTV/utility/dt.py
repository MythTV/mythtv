#------------------------------
# MythTV/utility/datetime.py
# Author: Raymond Wagner
# Description: Provides convenient use of several canned
#              timestamp formats
#------------------------------

from datetime import datetime as _pydatetime, \
                     tzinfo as _pytzinfo, \
                     timedelta
import re
import time
time.tzset()

class datetime( _pydatetime ):
    """
    Customized datetime class offering canned import and export of several
    common time formats, and 'duck' importing between them.
    """
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

    class _tzinfo( _pytzinfo):
        def __init__(self, direc='+', hr=0, min=0):
            if direc == '-':
                hr = -1*int(hr)
            self._offset = timedelta(hours=int(hr), minutes=int(min))
        def utcoffset(self, dt): return self._offset
        def tzname(self, dt): return ''
        def dst(self, dt): return timedelta(0)

    @classmethod
    def localTZ(cls):
        offset = -(time.timezone, time.altzone)[time.daylight]
        tz = cls._tzinfo(hr=offset/3600, min=(offset%3600)/60)
        return tz

    @classmethod
    def UTCTZ(cls):
        return cls._tzinfo()

    @classmethod
    def fromTimestamp(cls, posix, tz=None):
        return cls.fromtimestamp(float(posix), tz)

    @classmethod
    def frommythtime(cls, mtime):
        return cls.strptime(str(mtime), '%Y%m%d%H%M%S')

    @classmethod
    def fromIso(cls, isotime, sep='T'):
        match = cls._reiso.match(isotime)
        if match is None:
            raise TypeError("time data '%s' does not match ISO 8601 format" \
                                % isotime)

        dt = [int(a) for a in match.groups()[:5]]
        if match.group('sec') is not None:
            dt.append(int(match.group('sec')))
        else:
            dt.append(0)

        # microseconds
        dt.append(0)

        if match.group('tz'):
            if match.group('tz') == 'Z':
                tz = cls._tzinfo()
            elif match.group('tzmin'):
                tz = cls._tzinfo(*match.group('tzdirec','tzhour','tzmin'))
            else:
                tz = cls._tzinfo(*match.group('tzdirec','tzhour'))
            dt.append(tz)
        else:
            dt.append(cls.localTZ())

        return cls(*dt)

    @classmethod
    def fromRfc(cls, rfctime):
        return cls.strptime(rfctime.strip(), '%a, %d %b %Y %H:%M:%S %z')

    @classmethod
    def fromDatetime(cls, dt):
        return cls(dt.year, dt.month, dt.day, dt.hour, dt.minute,
                   dt.second, dt.microsecond, dt.tzinfo)

    @classmethod
    def duck(cls, t):
        try:
            # existing modified datetime
            t.mythformat
            return t
        except: pass
        try:
            # existing built-in datetime
            return cls.fromIso(t.isoformat())
        except: pass
        for func in [cls.fromTimestamp, #epoch time
                     cls.frommythtime, #iso time with integer characters only
                     cls.fromIso, #iso 8601 time
                     cls.fromRfc]: #rfc 822 time
            try:
                return func(t)
            except:
                pass
        raise TypeError("time data '%s' does not match supported formats"%t)

    def mythformat(self):
        return self.strftime('%Y%m%d%H%M%S')

    def timestamp(self):
        tt = self.astimezone(self.UTCTZ()).timetuple()
        return int(time.mktime(self.timetuple()))

    def rfcformat(self):
        return self.strftime('%a, %d %b %Y %H:%M:%S %z')

    @classmethod
    def now(cls, tz=None):
        if tz is None:
            tz = cls.localTZ()
        obj = super(datetime, cls).now(tz)
        return cls.fromDatetime(obj)

    @classmethod
    def fromtimestamp(cls, timestamp, tz=None):
        if tz is None:
            tz = cls.localTZ()
        obj = super(datetime, cls).fromtimestamp(tz)
        return cls.fromDatetime(obj)

    def astimezone(self, tz):
        return self.fromDatetime(super(datetime, self).astimezone(tz))
