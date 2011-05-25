# -*- coding: utf-8 -*-
"""Provides decorator classes for assorted functions"""

from logging import MythLog
from exceptions import MythDBError, MythError

from cStringIO import StringIO
from select import select
from time import time, mktime
from datetime import datetime as _pydatetime
from datetime import tzinfo as _pytzinfo
from datetime import timedelta
from itertools import imap
import weakref
import socket
import re

def _donothing(*args, **kwargs):
    pass

class SchemaUpdate( object ):
    # TODO: do locking and lock checking
    #       if interactive terminal, ask for update permission
    #       perform database backup (partial?)
    """
    Iteratively calls methods named 'up<schema version>' to update
        schema. Methods must return an integer of the new schema
        version.
    """
    _schema_name = None
    def __init__(self, db):
        self.db = db
        self.log = MythLog('Schema Update (%s)' % self._schema_name)

    def run(self):
        if self._schema_name is None:
            raise MythDBError('Schema update failed, variable name not set')
        if self.db.settings.NULL[self._schema_name] is None:
            self.db.settings.NULL[self._schema_name] = self.create()

        origschema = int(self.db.settings.NULL[self._schema_name])
        schema = origschema
        try:
            while True:
                
                newschema = getattr(self, 'up%d' % schema)()
                self.log(MythLog.DATABASE,
                         'successfully updated from %d to %d' %\
                                (schema, newschema))
                schema = newschema
                self.db.settings.NULL[self._schema_name] = schema

        except AttributeError, e:
            self.log(MythLog.DATABASE|MythLog.IMPORTANT,
                     'failed at %d' % schema, 'no handler method')
            raise MythDBError('Schema update failed, ' 
                    "SchemaUpdate has no function 'up%s'" % schema)

        except StopIteration:
            if schema != origschema:
                self.log(MythLog.DATABASE,
                         '%s update complete' % self._schema_name)
            pass

        except Exception, e:
            raise MythDBError(MythError.DB_SCHEMAUPDATE, e.args)

    def create(self):
        raise MythDBError('Schema creation failed, method not defined.')

class databaseSearch( object ):
    # decorator class for database searches
    """
    Decorator class

    Decorated functions must accept three keywords: init, key, value.
        'init' will be set once to True, to initialize the decorator. During
                initialization, the decorated class must return a tuple
                of the following format
                    (<table name>,   -- Primary table to pull data from.
                     <data class>,   -- Data handling class to use to process
                                        data. Ideally a subclass of DBData, 
                                        this class must provide a 'fromRaw'
                                        classmethod.
                     <required keywords>, -- Tuple of keywords that must be
                                             sent to the decorated function.
                                             If not provided by the user when
                                             called, these will be sent with
                                             a value of 'None'.
                     [join definition, [join definition, [...]]]
                            -- Optional definitions for additional tables
                               that can be JOINed in for extra search
                               capacity. Uses the following format:
                        (<table name>,  -- New table to be joined in.
                         <table name>,  -- Existing table to be joined to.
                         <joined fields>,  -- Tuple of fields to be used to
                                              match the two tables in the
                                              join.
                         [joined fields]   -- Optional field names to be used
                                              on the existing table.
                        )
                    )

        When the function is called, the decorator will pass the given
            keywords and values to the function one at a time. The function
            can return one of three responses.
                None     -- Invalid keyword, decorator throws an error.
                3-field  -- Normal response consisting of:
                    (<where statement>, -- like 'title=%s'
                     <where value>,
                     <bitwise>          -- defines which if any tables need
                                           to be JOINed in
                    )
                4-field  -- Special response consisting of:
                    (see example in methodheap.py:MythDB.searchRecorded)
    """
    class Join( object ):
        def __init__(self, table=None, tableto=None, fields=None, \
                           fieldsto=None, fieldsfrom=None):
            if (table is None) or (tableto is None) or \
                    ((fields is None) and \
                        ((fieldsto is None) or (fieldsfrom is None))):
                raise MythDBError('Invalid input to databaseSearch.Join.')
            self.table = table
            self.tableto = tableto
            if fields:
                self.fieldsfrom = fields
                self.fieldsto = fields
            else:
                self.fieldsfrom = fieldsfrom
                self.fieldsto = fieldsto

        def buildWhere(self):
            s = '%s.%%s=%s.%%s' % (self.table, self.tableto)
            return ' AND '.join([s % (f,t) \
                        for f,t in zip(self.fieldsfrom,self.fieldsto)])

        def buildJoin(self):
            return 'JOIN %s ON %s' % (self.table, self.buildWhere())

    def __init__(self, func):
        # set function and update strings
        self.func = func
        self.__doc__ = self.func.__doc__
        self.__name__ = self.func.__name__
        self.__module__ = self.func.__module__

        # set defaults
        self.table = None
        self.handler = None
        self.require = ()
        self.joins = ()

        # pull in properties
        self.func(self, self)

        # sanity check
        if (self.table is None) or (self.handler is None):
            raise MythError('Improperly configured databaseSearch class')

    def __get__(self, inst, own):
        # set instance and return self
        self.inst = inst
        return self

    def __call__(self, **kwargs):
        where,fields,joinbit = self.parseInp(kwargs)

        # process query
        query = self.buildQuery(where, joinbit=joinbit)
        with self.inst.cursor(self.inst.log) as cursor:
            if len(where) > 0:
                cursor.execute(query, fields)
            else:
                cursor.execute(query)

            for row in cursor:
                yield self.handler.fromRaw(row, db=self.inst)

    def parseInp(self, kwargs):
        where = []
        fields = []
        joinbit = 0

        # loop through inputs
        for key, val in kwargs.items():
            if val is None:
                continue

            # process custom query
            if key == 'custom':
                custwhere = {}
                custwhere.update(val)
                for k,v in custwhere.items():
                    where.append(k)
                    fields.append(v)
                continue

            # let function process remaining queries
            res = self.func(self.inst, key=key, value=val)
            errstr = "%s got an unexpected keyword argument '%s'"
            if res is None:
                if 'not' not in key:
                    raise TypeError(errstr % (self.__name__, key))
                # try inverted argument
                res = list(self.func(self.inst, key=key[3:], value=val))
                if res is None:
                    raise TypeError(errstr % (self.__name__, key))
                res[0] = 'NOT '+res[0]

            if len(res) == 0:
                continue
            elif len(res) == 3:
                # normal processing
                where.append(res[0])
                fields.append(res[1])
                joinbit = joinbit|res[2]
            elif len(res) == 4:
                # special format for crossreferenced data
                lval = [f.strip() for f in val.split(',')]
                where.append('(%s)=%d' %\
                    (self.buildQuery(
                        (   self.joins[res[3]].buildWhere(),
                            '(%s)' % \
                                 ' OR '.join(['%s=?' % res[0] \
                                                    for f in lval])),
                        'COUNT( DISTINCT %s )' % res[0],
                        res[1],
                        res[2]),
                    len(lval)))
                fields += lval
                            
        for key in self.require:
            if key not in kwargs:
                res = self.func(self.inst, key=key)
                if res is None:
                    continue
                where.append(res[0])
                fields.append(res[1])
                joinbit = joinbit|res[2]

        return where,fields,joinbit

    def buildJoinOn(self, i):
        if len(self.joins[i]) == 3:
            # shared field name
            on = ' AND '.join(['%s.%s=%s.%s' % \
                    (self.joins[i][0], field,\
                     self.joins[i][1], field)\
                             for field in self.joins[i][2]])
        else:
            # separate field name
            on = ' AND '.join(['%s.%s=%s.%s' % \
                    (self.joins[i][0], ffield,\
                     self.joins[i][1], tfield)\
                             for ffield, tfield in zip(\
                                    self.joins[i][2],\
                                    self.joins[i][3])])
        return on

    def buildJoin(self, joinbit=0):
        join = []
        for i,v in enumerate(self.joins):
            if (2**i)&joinbit:
                join.append(v.buildJoin())
        return ' '.join(join)

    def buildQuery(self, where, select=None, tfrom=None, joinbit=0):
        sql = 'SELECT '
        if select:
            sql += select
        elif tfrom:
            sql += tfrom+'.*'
        else:
            sql += self.table+'.*'

        sql += ' FROM '
        if tfrom:
            sql += tfrom
        else:
            sql += self.table

        sql += ' '+self.buildJoin(joinbit)

        if len(where):
            sql += ' WHERE '
            sql += ' AND '.join(where)

        return sql

class SplitInt( object ):
    """Utility class for handling split integers sent over myth protocol."""
    @staticmethod
    def joinInt(high,low):
        """obj.joinInt(high, low) -> 64-bit int, from two signed ints"""
        return (int(high) + (int(low)<0))*2**32 + int(low)

    @staticmethod
    def splitInt(dint):
        """obj.joinInt(64-bit int) -> (high, low)"""
        return dint/(2**32),dint%2**32 - (dint%2**32 > 2**31)*2**32

class CMPVideo( object ):
    """
    Utility class providing comparison operators between data objects
        with 'title' and 'subtitle' attributes.
    """
    def __cmp__(self, other):
        res = cmp(self.title, other.title)
        if res:
            return res
        return cmp(self.subtitle, other.subtitle)

class CMPRecord( CMPVideo ):
    """
    Utility class providing comparison operators between data objects
        with 'chanid' and 'starttime', falling back to 'title' and 'subtitle'.
    """
    def __cmp__(self, other):
        if ('starttime' not in other) or ('chanid' not in other):
            return CMPVideo.__cmd__(self, other)
        res = cmp(self.starttime, other.starttime)
        if res:
            return res
        return cmp(self.chanid, other.chanid)

class deadlinesocket( socket.socket ):
    """
    Customized socket providing logging, and several convenience functions for
        connecting with frontend and backend, guaranteeing termination after
        a set timeout.
    """
    def __init__(self, *args, **kwargs):
        socket.socket.__init__(self, *args, **kwargs)
        self.log = MythLog('Python Socket')
        self.setdeadline(10.0)

    def connect(self, *args, **kwargs):
        self.settimeout(self.getdeadline())
        socket.socket.connect(self, *args, **kwargs)
        self.setblocking(0)

    def getdeadline(self): return self._deadline
    def setdeadline(self, deadline): self._deadline = deadline

    def dlrecv(self, bufsize, flags=0, deadline=None):
        # pull default timeout
        if deadline is None:
            deadline = self._deadline
        if deadline < 1000:
            deadline += time()

        buff = StringIO()
        # loop until necessary data has been received
        while bufsize > buff.tell():
            # wait for data on the socket
            t = time()
            timeout = (deadline-t) if (deadline-t>0) else 0.0
            if len(select([self],[],[], timeout)[0]) == 0:
                # deadline reached, terminate
                return u''

            # append response to buffer
            p = buff.tell()
            try:
                buff.write(self.recv(bufsize-buff.tell(), flags))
            except socket.error, e:
                raise MythError(MythError.SOCKET, e.args)
            if buff.tell() == p:
               # no data read from a 'ready' socket, connection terminated
                raise MythError(MythError.SOCKET, (54, 'Connection reset by peer'))

            if timeout == 0:
                break
        return buff.getvalue()

    def dlexpect(self, pattern, flags=0, deadline=None):
        """Loop recv listening for a provided regular expression."""
        # pull default timeout
        if deadline is None:
            deadline = self._deadline
        if deadline < 1000:
            deadline += time()

        buff = StringIO()
        # loop until pattern has been found
        while not pattern.search(buff.getvalue()):
            # wait for data on the socket
            t = time()
            timeout = (deadline-t) if (deadline-t>0) else 0.0
            if len(select([self],[],[], timeout)[0]) == 0:
                # deadline reached, terminate
                return ''

            # append response to buffer
            p = buff.tell()
            try:
                buff.write(self.recv(100, flags))
            except socket.error, e:
                raise MythError(MythError.SOCKET, e.args)
            if buff.tell() == p:
                # no data read from a 'ready' socket, connection terminated
                raise MythError(MythError.CLOSEDSOCKET)

            if timeout == 0:
                break
        return buff.getvalue()

    def recvheader(self, flags=0, deadline=None):
        """
        Loop recv listening for an amount of data provided
            in the first 8 bytes.
        """
        size = int(self.dlrecv(8, flags, deadline))
        data = self.dlrecv(size, flags, deadline)
        self.log(MythLog.SOCKET|MythLog.NETWORK, \
                            'read <-- %d' % size, data)
        return data

    def sendheader(self, data, flags=0):
        """Send data, prepending the length in the first 8 bytes."""
        try:
            self.log(MythLog.SOCKET|MythLog.NETWORK, \
                                'write --> %d' % len(data), data)
            data = '%-8d%s' % (len(data), data)
            self.send(data, flags)
        except socket.error, e:
            raise MythError(MythError.SOCKET, e.args)

class MARKUPLIST( object ):
    """
    Utility class for building seek/cutlists from video markup data.
    """
    def _buildlist(self, ms, me):
        start = []
        stop = []
        for mark in sorted(self, key=lambda m: m.mark):
            if mark.type == ms:
                if len(start) == len(stop):
                    # start new cut
                    start.append(mark.mark)
                #else: a cut has already been started
            elif mark.type == me:
                if len(start) == 0:
                    # endpoint found without startpoint
                    start.append(0)
                if len(start) > len(stop):
                    # end cut
                    stop.append(mark.mark)
                else:
                    # replace cut endpoint
                    stop[len(stop)-1] = mark.mark
        if len(start) > len(stop):
            # endpoint missing, no known better option
            stop.append(9999999)
        return zip(start, stop)

def levenshtein(s1, s2):
    """Compute the Levenshtein distance of two strings."""
    # http://en.wikibooks.org/wiki/Algorithm_implementation/Strings/Levenshtein_distance
    if len(s1) < len(s2):
        return levenshtein(s2, s1)
    if not s1:
        return len(s2)
 
    previous_row = xrange(len(s2) + 1)
    for i, c1 in enumerate(s1):
        current_row = [i + 1]
        for j, c2 in enumerate(s2):
            insertions = previous_row[j + 1] + 1
            deletions = current_row[j] + 1
            substitutions = previous_row[j] + (c1 != c2)
            current_row.append(min(insertions, deletions, substitutions))
        previous_row = current_row
 
    return previous_row[-1]

class datetime( _pydatetime ):
    _reiso = re.compile('(?P<year>[0-9]{4})'
                       '-(?P<month>[0-9]{1,2})'
                       '-(?P<day>[0-9]{1,2})'
                        '.'
                        '(?P<hour>[0-9]{2})'
                       ':(?P<min>[0-9]{2})'
                       '(:(?P<sec>[0-9]{2}))?'
                        '(?P<tz>Z|'
                            '(?P<tzdirec>[-+])'
                            '(?P<tzhour>[0-9]{1,2}?)'
                            '(:)?'
                            '(?P<tzmin>[0-9]{2})?'
                        ')?')

    class tzinfo( _pytzinfo):
        def __init__(self, direc='+', hr=0, min=0):
            if direc == '-':
                hr = -1*int(hr)
            self._offset = timedelta(hours=int(hr), minutes=int(min))
        def utcoffset(self, dt): return self._offset
        def tzname(self, dt): return ''
        def dst(self, dt): return timedelta(0)

    @classmethod
    def fromTimestamp(cls, posix):
        return cls.fromtimestamp(float(posix))

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
        if match.group('tz'):
            if match.group('tz') == 'Z':
                tz = cls.tzinfo()
            elif match.group('tzmin'):
                tz = cls.tzinfo(*match.group('tzdirec','tzhour','tzmin'))
            else:
                tz = cls.tzinfo(*match.group('tzdirec','tzhour'))
            dt.append(0)
            dt.append(tz)
        return cls(*dt)

    @classmethod
    def fromRfc(cls, rfctime):
        return cls.strptime(rfctime.strip(), '%a, %d %b %Y %H:%M:%S %Z')

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
        return int(mktime(self.timetuple()))

    def rfcformat(self):
        return self.strftime('%a, %d %b %Y %H:%M:%S %Z')

class ParseEnum( object ):
    _static = None
    def __str__(self): 
        return str([k for k,v in self.iteritems() if v==True])
    def __repr__(self): return str(self)
    def __init__(self, parent, field_name, enum, editable=True):
        self._parent = weakref.proxy(parent)
        self._field = field_name
        self._enum = enum
        self._static = not editable

    def __getattr__(self, name):
        if name in self.__dict__:
            return self.__dict__[name]
        return bool(self._parent[self._field]&getattr(self._enum, name))

    def __setattr__(self, name, value):
        if self._static is None:
            object.__setattr__(self, name, value)
            return
        if self._static:
            raise AttributeError("'%s' cannot be edited." % name)
        self.__setitem__(name, value)

    def __getitem__(self, key):
        return bool(self._parent[self._field]&getattr(self._enum, key))

    def __setitem__(self, key, value):
        if self._static:
            raise KeyError("'%s' cannot be edited." % name)
        val = getattr(self._enum, key)
        if value:
            self._parent[self._field] |= val
        else:
            self._parent[self._field] -= self._parent[self._field]&val

    def keys(self):
        return [key for key in self._enum.__dict__ if key[0] != '_']

    def values(self):
        return list(self.itervalues())

    def items(self):
        return list(self.iteritems())

    def __iter__(self):
        return self.itervalues()

    def iterkeys(self):
        return iter(self.keys())

    def itervalues(self):
        return imap(self.__getitem__, self.keys())

    def iteritems(self):
        for key in self.keys():
            yield (key, self[key])

class ParseSet( ParseEnum ):
    def __init__(self, parent, field_name, editable=True):
        self._parent = weakref.proxy(parent)
        self._field = field_name
        field = parent._db.tablefields[parent._table][self._field].type
        if field[:4] != 'set(':
            raise MythDBError("ParseSet error. "+\
                        "Field '%s' not of type 'set()'" % self._field)
        self._enum = dict([(t,2**i) for i,t in enumerate([type.strip("'")\
                                for type in field[4:-1].split(',')])])
        self._static = not editable

    def __getattr__(self, name):
        if name in self.__dict__:
            return self.__dict__[name]
        return self.__getitem__(name)

    def __setattr__(self, name, value):
        if self._static is None:
            object.__setattr__(self, name, value)
            return
        if self._static:
            raise AttributeError("'%s' cannot be edited." % name)
        self.__setitem__(name, value)

    def __getitem__(self, key):
        return key in self._parent[self._field].split(',')

    def __setitem__(self, key, value):
        if self._static:
            raise KeyError("'%s' cannot be edited." % name)
        if self[key] == value:
            return
        tmp = self._parent[self._field].split(',')
        if value:
            tmp.append(key)
        else:
            tmp.remove(key)
        self._parent[self._field] = ','.join(tmp)

    def keys(self):
        return self._enum.keys()

def CopyData(dfrom, dto, keys, lower=False):
    def copystraight(dfrom, dto, key): dto[key] = dfrom[key]
    def copylower(dfrom, dto, key): dto[key.lower()] = dfrom[key]

    copydata = copystraight
    if lower: copydata = copylower

    for key in keys:
        if key in dfrom:
            copydata(dfrom, dto, key)

def CopyData2(dfrom, dto, keys):
    if hasattr(keys, 'items'):
        keys = keys.items()

    for key,key2 in keys:
        if key in dfrom:
            dto[key2] = dfrom[key]

def check_ipv6(n):
    try:
        socket.inet_pton(socket.AF_INET6, n)
        return True
    except socket.error:
        return False

