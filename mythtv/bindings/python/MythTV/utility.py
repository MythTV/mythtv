# -*- coding: utf-8 -*-
"""Provides decorator classes for assorted functions"""

from __future__ import with_statement

from logging import MythLog
from exceptions import MythDBError

from cStringIO import StringIO
from select import select
from time import time
import socket

class schemaUpdate( object ):
    # TODO: do locking and lock checking
    #       if interactive terminal, ask for update permission
    #       perform database backup (partial?)
    """
    Decorator class

    Acquires name of database variable by calling function with input of '-1'.
    Recursively calls decorated function with the current schema version.
        Expects (updates, newver) in return, where:
                updates -- tuple of (sql commands, values)
                newver -- new schema revision
        Stops when StopIteration is called.
    """
    def __init__(self, func):
        self.func = func
        self.__doc__ = self.func.__doc__
        self.__name__ = self.func.__name__
        self.__module__ = self.func.__module__

        self.schemavar = func(-1)

    def __call__(self, db):
        log = MythLog('Schema Update')

        with db as cursor:
          while True:
            try:
                updates, newver = self.func(db.settings.NULL[self.schemavar])
            except StopIteration:
                break

            log(log.IMPORTANT, 'Updating %s from %s to %s' % \
                        (schemaname, db.settings.NULL[self.schemavar], newver))

            try:
                for sql, values in updates:
                    cursor.execute(sql, values)
            except Exception, e:
                log(log.IMPORTANT, 'Update of %s failed' % self.schemavar)
                raise MythDBError(MythError.DB_SCHEMAUPDATE, e.args)

            db.settings.NULL[self.schemavar] = newver

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
    def __init__(self, func):
        # set function and update strings
        self.func = func
        self.__doc__ = self.func.__doc__
        self.__name__ = self.func.__name__
        self.__module__ = self.func.__module__

        # process joins
        res = list(self.func(self, True))
        self.table = res[0]
        self.dbclass = res[1]
        self.require = res[2]
        if len(res) > 3:
            self.joins = res[3:]
        else:
            self.joins = ()

    def __get__(self, inst, own):
        # set instance and return self
        self.inst = inst
        return self

    def __call__(self, **kwargs):
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
                res = list(self.func(self.inst, key=key[3:], value=val))
                if res is None:
                    raise TypeError(errstr % (self.__name__, key))
                res[0] = 'NOT '+res[0]

            if len(res) == 3:
                where.append(res[0])
                fields.append(res[1])
                joinbit = joinbit|res[2]
            elif len(res) == 4:
                lval = val.split(',')
                where.append('(%s)=%d' %\
                    (self.buildQuery(
                        (   self.buildJoinOn(res[3]),
                            '(%s)' % \
                                 ' OR '.join(['%s=%%s' % res[0] \
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

        # process query
        query = self.buildQuery(where, joinbit=joinbit)
        with self.inst.cursor(self.inst.log) as cursor:
            if len(where) > 0:
                cursor.execute(query, fields)
            else:
                cursor.execute(query)

        for row in cursor:
            yield self.dbclass.fromRaw(row, self.inst)

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

        if joinbit:
            for i in range(len(self.joins)):
                if (2**i)&joinbit:
                    sql += ' JOIN %s ON %s' % \
                            (self.joins[i][0], self.buildJoinOn(i))

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
        if ('starttime' not in other) or ('video' not in other):
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
        socket.socket.connect(self, *args, **kwargs)
        self.setblocking(0)

    def getdeadline(self): return self._deadline
    def setdeadline(self, deadline): self._deadline = deadline

    def dlrecv(self, bufsize, flags=0, deadline=None):
        t = time()
        if deadline is None:
            deadline = t + self._deadline

        buff = StringIO()
        # loop until necessary data has been received
        while bufsize > buff.tell():
            # wait for data on the socket
            if len(select([self],[],[], deadline-t)[0]) == 0:
                # deadline reached, terminate
                return u''
            # append response to buffer
            buff.write(self.recv(bufsize-buff.tell(), flags))
            if t >= deadline:
                break
            t = time()
        return buff.getvalue()

    def dlexpect(self, pattern, flags=0, deadline=None):
        """Loop recv listening for a provided regular expression."""
        t = time()
        if deadline is None:
            deadline = t + self._deadline

        buff = StringIO()
        # loop until pattern has been found
        while not pattern.search(buff.getvalue()):
            if len(select([self],[],[], deadline-t)[0]) == 0:
                return ''
            buff.write(self.recv(100, flags))
            if t >= deadline:
                break
            t = time()
        return buff.getvalue()

    def recvheader(self, flags=0, deadline=None):
        """
        Loop recv listening for an amount of data provided
            in the first 8 bytes.
        """
        try:
            size = int(self.dlrecv(8, flags, deadline))
        except:
            return ''
        data = self.dlrecv(size, flags, deadline)
        self.log(MythLog.SOCKET|MythLog.NETWORK, \
                            'read <-- %d' % size, data)
        return data

    def sendheader(self, data, flags=0):
        """Send data, prepending the length in the first 8 bytes."""
        self.log(MythLog.SOCKET|MythLog.NETWORK, \
                            'write --> %d' % len(data), data)
        data = '%-8d%s' % (len(data), data)
        self.send(data, flags)

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
