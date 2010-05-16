# -*- coding: utf-8 -*-

"""
Provides base classes for accessing MythTV
"""

from __future__ import with_statement

from MythStatic import *

import os
import re
import socket
import sys
import locale
import weakref
import signal
import xml.etree.cElementTree as etree
from datetime import datetime
from time import sleep, time, mktime
from urllib import urlopen
from subprocess import Popen
from sys import version_info
from thread import start_new_thread, allocate_lock
from select import select
from uuid import uuid4

import MySQLdb, MySQLdb.cursors
MySQLdb.__version__ = tuple([v for v in MySQLdb.__version__.split('.')])

class DictData( object ):
    """
    DictData.__init__(raw) -> DictData object

    Basic class for providing management of data resembling dictionaries.
    Must be subclassed for use.

    Subclasses must provide:
        _field_order
            string list of field names
        _field_type
            integer list of field types by:
                0: integer  - int(value)
                1: float    - float(value)
                2: bool     - bool(value)
                3: string   - value
                4: date     - datetime.fromtimestamp(int(value))
            can be the single string 'Pass', to pass all values untouched

    Data can be accessed as:
        obj.field_name
            -- or --
        obj['field_name']
    """

    def _DictIterator(self, mode):
        for key in self._field_order:
            if mode == 1:
                yield key
            elif mode == 2:
                yield self[key]
            elif mode == 3:
                yield (key, self[key])

    _logmodule = 'Python DictData'
    # class emulation methods
    def __getattr__(self,name):
        # method for class attribute access of data fields
        # accesses real attributes, falls back to data fields, and errors
        if name in self.__dict__:
            return self.__dict__[name]
        else:
            try:
                return self[name]
            except KeyError:
                raise AttributeError(str(name))

    def __setattr__(self,name,value):
        # method for class attribute access of data fields
        # sets value in data fields, falling back to real attributes
        if name in self._field_order:
            self._data[name] = value
        else:
            self.__dict__[name] = value

    # container emulation methods
    def __getitem__(self,key):
        # method for and dict-like access to content
        # as a side effect of dual operation, dict data cannot be indexed
        #       keyed off integer values
        if key in self._data:
            return self._data[key]
        else:
            raise KeyError("'DictData' object has no key '%s'" %key)

    def __setitem__(self,key,value):
        # method for dist-like access to content
        # does not allow setting of data not already in field_order
        if key in self._field_order:
            self._data[key] = value
        else:
            raise KeyError("'DictData' does not allow new data")

    def __contains__(self,item):
        return bool(item in self._data.keys())

    def __iter__(self):
        return self._DictIterator(2)
        
    def iterkeys(self):
        """
        obj.iterkeys() -> an iterator over the keys of obj
                                                ordered by self.field_order
        """
        return self._DictIterator(1)
        
    def itervalues(self):
        """
        obj.itervalues() -> an iterator over the values of obj
                                                ordered by self.field_order
        """
        return self._DictIterator(2)
    
    def iteritems(self):
        """
        obj.iteritems() -> an iterator of over the (key,value) pairs of obj
                                                ordered by self.field_order
        """
        return self._DictIterator(3)

    def keys(self):
        """obj.keys() -> list of self.field_order"""
        return list(self.iterkeys())
        
    def has_key(self,item):
        return self.__contains__(item)
        
    def values(self):
        """obj.values() -> list of values, ordered by self.field_order"""
        return list(self.itervalues())
        
    def get(self, key, default=None):
        return self._data.get(key, default)
        
    def items(self):
        """
        obj.items() -> list of (key,value) pairs, ordered by self.field_order
        """
        return list(self.iteritems())
        
    def pop(self,key): raise NotImplementedError
    def popitem(self): raise NotImplementedError

    def __len__(self):
        return len(self._field_order)

    def __length_hint__(self):
        return len(self._field_order)
        
    def update(self, *args, **keywords):
        self._data.update(*args, **keywords)

    def __setstate__(self, dat):
        for k,v in dat.items():
            self.__dict__[k] = v

    ### additional management functions
    def _setDefs(self):
        self._data = {}
        self._log = MythLog(self._logmodule)
        
    def __init__(self, raw):
        self._setDefs()
        self._data.update(self._process(raw))

    def _process(self, data):
        data = list(data)
        for i in xrange(len(data)):
            if data[i] == '':
                data[i] = None
            elif self._field_type == 'Pass':
                data[i] = data[i]
            elif self._field_type[i] == 0:
                data[i] = int(data[i])
            elif self._field_type[i] == 1:
                data[i] = locale.atof(data[i])
            elif self._field_type[i] == 2:
                data[i] = bool(data[i])
            elif self._field_type[i] == 3:
                data[i] = data[i]
            elif self._field_type[i] == 4:
                data[i] = datetime.fromtimestamp(int(data[i]))
        return dict(zip(self._field_order,data))

    def _deprocess(self):
        data = self.values()
        for i in xrange(len(data)):
            if data[i] is None:
                data[i] = ''
            elif self._field_type == 'Pass':
                pass
            elif self._field_type[i] == 0:
                data[i] = str(data[i])
            elif self._field_type[i] == 1:
                data[i] = locale.format("%0.6f", data[i])
            elif self._field_type[i] == 2:
                data[i] = str(int(data[i]))
            elif self._field_type[i] == 3:
                pass
            elif self._field_type[i] == 4:
                data[i] = str(int(mktime(data[i].timetuple())))
        return data

    @staticmethod
    def joinInt(high,low):
        """obj.joinInt(high, low) -> 64-bit int, from two signed ints"""
        return (high + (low<0))*2**32 + low

    @staticmethod
    def splitInt(integer):
        """obj.joinInt(64-bit int) -> (high, low)"""
        return integer/(2**32),integer%2**32 - (integer%2**32 > 2**31)*2**32

class QuickDictData( DictData ):
    """
    QuickDictData.__init__(raw) -> QuickDictData object

    A simple DictData implementation that accepts raw information as
    a tuple of two-tuples. Accepts new information stored as attributes
    or keys.
    """
    _localvars = ['_field_order','_data','_log']
    def __init__(self, raw):
        self._setDefs()
        self._field_order = [k for k,v in raw]
        self._data.update(raw)

    def _setDefs(self):
        self.__dict__['_field_order'] = []
        DictData._setDefs(self)

    def __setattr__(self, name, value):
        if name in self._localvars:
            self.__dict__[name] = value
        else:
            self.__setitem__(name, value)

    def __setitem__(self, key, value):
        if key not in self._field_order:
            self._field_order.append(key)
        self._data[key] = value

    def __delattr__(self, name):
        if name in self._localvars:
            del self.__dict__[name]
        else:
            self.__delitem__(name)

    def __delitem__(self, key):
        self._field_order.remove(key)
        del self._data[key]

    def clear(self):
        self._field_order = []
        self._data.clear()

    def copy(self):
        return self.__class__(self.items())

class DBData( DictData ):
    """
    DBData.__init__(data=None, db=None, raw=None) --> DBData object

    Altered DictData adding several functions for dealing with individual rows.
    Must be subclassed for use.

    Subclasses must provide:
        _table
            Name of database table to be accessed
        _where
            String defining WHERE clause for database lookup
        _setwheredat
            String of comma separated variables to be evaluated to set
            'wheredat'. 'eval(setwheredat)' must return a tuple, so string must 
            contain at least one comma.

    Subclasses may provide:
        _allow_empty
            Controls whether DBData() will allow an empty instance.
        _schema_value, _schema_local, _schema_name
            Allows checking of alternate plugin schemas

    Can be populated in two manners:
        data
            Tuple used to perform a SQL query, using the subclass provided 
            'where' clause
        raw
            Raw list as returned by 'select * from mytable'
    """
    _field_type = 'Pass'
    _allow_empty = False
    _logmodule = 'Python DBData'
    _schema_value = 'DBSchemaVer'
    _schema_local = SCHEMA_VERSION
    _schema_name = 'Database'

    def getAllEntries(self):
        """obj.getAllEntries() -> tuple of DBData objects"""
        c = self._db.cursor(self._log)
        query = """SELECT * FROM %s""" % self._table
        self._log(MythLog.DATABASE, query)
        if c.execute(query) == 0:
            return ()
        objs = []
        for row in c.fetchall():
            objs.append(self.__class__(db=self._db, raw=row))
        c.close()
        return objs

    def _setDefs(self):
        self.__dict__['_field_order'] = self._db.tablefields[self._table]
        DictData._setDefs(self)
        self._fillNone()
        self._wheredat = None
        self._log = MythLog(self._logmodule, db=self._db)

    def __init__(self, data=None, db=None, raw=None):
        self.__dict__['_db'] = DBCache(db)
        self._db._check_schema(self._schema_value, 
                                self._schema_local, self._schema_name)
        self._setDefs()

        if raw is not None:
            if len(raw) == len(self._field_order):
                self._data.update(self._process(raw))
                self._wheredat = eval(self._setwheredat)
            else:
                raise MythError('Incorrect raw input length to DBData()')
        elif data is not None:
            if None not in data:
                self._wheredat = tuple(data)
                self._pull()
        else:
            if self._allow_empty:
                self._fillNone()
            else:
                raise MythError('DBData() not given sufficient information')

    def _pull(self):
        """Updates table with data pulled from database."""
        c = self._db.cursor(self._log)
        count = c.execute("""SELECT * FROM %s WHERE %s""" \
                           % (self._table, self._where), self._wheredat)
        if count == 0:
            raise MythError('DBData() could not read from database')
        elif count > 1:
            raise MythError('DBData() could not find unique entry')
        data = c.fetchone()
        c.close()
        self._data.update(self._process(data))

    def _fillNone(self):
        """Fills out dictionary fields with empty data"""
        field_order = self._db.tablefields[self._table]
        self._field_order = field_order
        self._data = dict(zip(field_order, [None]*len(field_order)))

class DBDataWrite( DBData ):
    """
    DBDataWrite.__init__(data=None, db=None, raw=None) --> DBDataWrite object

    Altered DBData, with support for writing back to the database.
    Must be subclassed for use.

    Subclasses must provide:
        _table
            Name of database table to be accessed
        _where
            String defining WHERE clause for database lookup
        _setwheredat
            String of comma separated variables to be evaluated to set
            'wheredat'. 'eval(setwheredat)' must return a tuple, so string must 
            contain at least one comma.

    Subclasses may provide:
        _defaults
            Dictionary of default values to be used when creating new 
            database entries. Additionally, values of 'None' will be stripped
            and not used to alter the database.

    Can be populated in two manners:
        data
            Tuple used to perform a SQL query, using the subclass provided 
            'where' clause
        raw
            Raw list as returned by 'select * from mytable'
    Additionally, can be left uninitialized to allow creation of a new entry
    """
    _defaults = None
    _allow_empty = True
    _logmodule = 'Python DBDataWrite'

    def _sanitize(self, data, fill=True):
        """Remove fields from dictionary that are not in database table."""
        data = data.copy()
        for key in data.keys():
            if key not in self._field_order:
                del data[key]
        if self._defaults is not None:
            for key in self._defaults:
                if key in data:
                    if self._defaults[key] is None:
                        del data[key]
                    elif data[key] is None:
                        if fill:
                            data[key] = self._defaults[key]
        return data

    def _setDefs(self):
        DBData._setDefs(self)
        self._fillDefs()

    def _fillDefs(self):
        self._fillNone()
        self._data.update(self._defaults)

    def __init__(self, data=None, db=None, raw=None):
        DBData.__init__(self, data, db, raw)
        if raw is not None:
            self._origdata = self._data.copy()

    def create(self,data=None):
        """
        obj.create(data=None) -> new database row

        Creates a new database entry using given information.
        Will add any information in 'data' dictionary to local information
        before pushing the entire set onto the database.
        Will only function with an uninitialized object.
        """
        if self._wheredat is not None:
            raise MythError('DBDataWrite object already bound to '+\
                                                    'existing instance')

        if data is not None:
            data = self._sanitize(data, False)
            self._data.update(data)
        data = self._sanitize(self._data)
        for key in data.keys():
            if data[key] is None:
                del data[key]
        c = self._db.cursor(self._log)
        fields = ', '.join(data.keys())
        format_string = ', '.join(['%s' for d in data.values()])
        c.execute("""INSERT INTO %s (%s) VALUES(%s)""" \
                        % (self._table, fields, format_string), data.values())
        intid = c.lastrowid
        c.close()
        return intid

    def _pull(self):
        DBData._pull(self)
        self._origdata = self._data.copy()

    def _push(self):
        if (self._where is None) or (self._wheredat is None):
            return
        c = self._db.cursor(self._log)
        data = self._sanitize(self._data)
        for key, value in data.items():
            if value == self._origdata[key]:
            # filter unchanged data
                del data[key]
        if len(data) == 0:
            # no updates
            return
        format_string = ', '.join(['%s = %%s' % d for d in data])
        sql_values = data.values()
        sql_values.extend(self._wheredat)

        c.execute("""UPDATE %s SET %s WHERE %s""" \
                    % (self._table, format_string, self._where), sql_values)
        c.close()
        self._pull()
        
    def update(self, *args, **keywords):
        """
        obj.update(*args, **keywords) -> None

        Follows dict.update() syntax. Updates local information, and pushes
        changes onto the database.
        Will only function on an initialized object.
        """

        data = {}
        data.update(*args, **keywords)
        self._data.update(self._sanitize(data))
        self._push()

    def delete(self):
        """
        obj.delete() -> None

        Delete video entry from database.
        """
        if (self._where is None) or \
                (self._wheredat is None) or \
                (self._data is None):
            return
        c = self._db.cursor(self._log)
        c.execute("""DELETE FROM %s WHERE %s""" \
                        % (self._table, self._where), self._wheredat)
        c.close()

class DBDataRef( object ):
    """
    DBDataRef.__init__(where, db=None) --> DBDataRef object

    Class for managing lists of referenced data, such as recordedmarkup
    Subclasses must provide:
        _table
            Name of database table to be accessed
        _ref
            list of fields for WHERE argument in lookup
    """
    class Data( QuickDictData ):
        # this is the full list of matching entries
        def _rehash(self, key=None):
            if key is not None:
                k = self._field_order[key]
                v = self._data[k]
                h = hash(v)
                if k != h:
                    del self._data[k]
                    self._data[h] = v
                    self._field_order[key] = h
            else:
                for key in range(len(self)):
                    self._rehash(key)
                    
        def __xor__(self, other):
            res = self.__class__(())
            for k,v in self.items():
                if k not in other:
                    res[k] = v
            for k,v in other.items():
                if k not in self:
                    res[k] = v
            return res

        def __and__(self, other):
            res = self.__class__(())
            for k,v in self.items():
                if k in other:
                    res[k] = v
            return res

    class SubData( QuickDictData ):
        # this is one matching entry
        def __str__(self):
            return str(self.items())
        def __repr__(self):
            return str(self).encode('utf-8')
        def __hash__(self):
            return hash(str(self.values()))

    def __str__(self):
        return str(list(self))
    def __repr__(self):
        return str(self).encode('utf-8')

    def __getitem__(self, key):
        self._populate()
        return self._data[self._data._field_order[key]]

    def __setitem__(self, key, value):
        self._populate()
        if key not in range(len(self._data)):
            raise KeyError
        self._data[self._data._field_order[key]] = \
                    self.SubData(zip(self._datfields, list(value)))
        self._data._rehash(key)

    def __delitem__(self, key):
        self._populate()
        del self._data[self._data._field_order[key]]

    def __iter__(self):
        self._populate()
        return iter(self._data)

    def __init__(self, where, db=None):
        self._db = DBCache(db)
        self._refdat = where

        fields = list(self._db.tablefields[self._table])
        for f in self._ref:
            if f in fields:
                fields.remove(f)
        self._datfields = fields
        self._data = None

    def _populate(self, force=False):
        if not ((self._data is None) or force):
            return
        self._data = self.Data(())
        c = self._db.cursor()
        c.execute("""SELECT %s FROM %s WHERE %s""" % \
                         (','.join(self._datfields),
                          self._table,
                          ' AND '.join(['%s=%%s' % f for f in self._ref])),
                    self._refdat)
        for row in c.fetchall():
            sd = self.SubData(zip(self._datfields, row))
            self._data[hash(sd)] = sd
        c.close()
        self._origdata = self._data.copy()

    def revert(self):
        """Delete all local changes to database."""
        self._populate()
        self._data = self._origdata.copy()

    def commit(self):
        """Push all local changes to database."""
        if self._data is None:
            return
        # push changes to database
        self._data._rehash()
        diff = self._data^self._origdata
        if len(diff) == 0:
            return
        c = self._db.cursor()
        fields = list(self._ref)+list(self._datfields)

        # remove old entries
        for v in (self._origdata&diff):
            data = list(self._refdat)+v.values()
            wf = []
            for i in range(len(data)):
                if data[i] is None:
                    wf.append('%s IS %%s' % fields[i])
                else:
                    wf.append('%s=%%s' % fields[i])
            c.execute("""DELETE FROM %s WHERE %s""" % \
                        (self._table, ' AND '.join(wf)), data)

        # add new entries
        data = []
        for v in (self._data&diff):
            # add new entries
            data.append(list(self._refdat)+v.values())
        if len(data) > 0:
            c.executemany("""INSERT INTO %s (%s) VALUES(%s)""" % \
                            (self._table,
                             ','.join(fields),
                             ','.join(['%s' for a in fields])), data)
        c.close()
        self._origdata = self._data.copy()

    def add(self, *data):
        """Adds a list of data matching those specified in '_datfields'."""
        # add new entry to local copy
        self._populate()
        sd = self.SubData(zip(self._datfields, data))
        self._data[hash(sd)] = sd

    def delete(self, *data):
        """Deletes an entry matching the provided list of data."""
        # delete entry from local copy
        self._populate()
        sd = self.SubData(zip(self._datfields, data))
        del self._data[hash(sd)]

    def clean(self):
        """Remove all entries and commit."""
        # clear all and commit
        self._populate()
        self._data.clear()
        self.commit()

class DBDataCRef( DBDataRef ):
    """
    DBDataRef.__init__(where, db=None) --> DBDataRef object

    Class for managing lists of referenced data, such as recordedmarkup
    Subclasses must provide:
        _table
            List of database tables to be accessed
        _ref
            List of fields for WHERE argument in lookup
        _cref
            2-list of fields used for cross reference
    """

    class SubData( DBDataRef.SubData ):
        _localvars = DBDataRef.SubData._localvars+['_cref']

    def __init__(self, where, db=None):
        self._db = DBCache(db)
        self._refdat = list(where)

        rfields = list(self._db.tablefields[self._table[0]])
        crfields = list(self._db.tablefields[self._table[1]])
        for f in self._ref+self._cref[:1]:
            if f in rfields:
                rfields.remove(f)
        if self._cref[-1] in crfields:
            crfields.remove(self._cref[-1])

        self._rdatfields = rfields
        self._crdatfields = crfields
        self._datfields = crfields+rfields
        self._data = None

    def _populate(self, force=False):
        if not ((self._data is None) or force):
            return
        self._data = self.Data(())
        self._crdata = {}
        datfields = self._datfields
        reffield = '%s.%s' % (self._table[1],self._cref[-1])

        c = self._db.cursor()
        c.execute("""SELECT %s FROM %s JOIN %s ON %s WHERE %s""" % \
                        (','.join(datfields+[reffield]),
                         self._table[0],
                         self._table[1],
                         '%s.%s=%s.%s' % \
                                (self._table[0], self._cref[0],
                                 self._table[1], self._cref[-1]),
                         ' AND '.join(['%s=%%s' % f for f in self._ref])),
                    self._refdat)

        for row in c.fetchall():
            sd = self.SubData(zip(datfields, row[:-1]))
            sd._cref = row[-1]
            self._data[hash(sd)] = sd

        c.close()
        self._origdata = self._data.copy()

    def commit(self):
        if self._data is None:
            return
        # push changes to database
        self._data._rehash()
        diff = self._data^self._origdata
        if len(diff) == 0:
            return
        c = self._db.cursor()

        # add new cross-references
        newdata = self._data&diff
        for d in newdata:
            data = [d[a] for a in self._crdatfields]
            fields = self._crdatfields
            count = c.execute("""SELECT %s FROM %s WHERE %s""" % \
                        (self._cref[-1], self._table[1],
                         ' AND '.join(['%s=%%s' % f for f in fields])),
                    data)
            if count == 1:
                # match found
                d._cref = c.fetchone()[0]
            else:
                c.execute("""INSERT INTO %s (%s) VALUES(%s)""" % \
                        (self._table[1],
                         ','.join(self._crdatfields),
                         ','.join(['%s' for a in data])),
                    data)
                d._cref = c.lastrowid
        # add new references
        for d in newdata:
            data = [d[a] for a in self._rdatfields]+[d._cref]+self._refdat
            fields = self._rdatfields+self._cref[:1]+self._ref
            c.execute("""INSERT INTO %s (%s) VALUES(%s)""" % \
                        (self._table[0], 
                         ','.join(fields),
                         ','.join(['%s' for a in data])),
                    data)

        # remove old references
        olddata = self._origdata&diff
        crefs = [d._cref for d in olddata]
        for d in olddata:
            data = [d[a] for a in self._rdatfields]+[d._cref]+self._refdat
            fields = self._rdatfields+self._cref[:1]+self._ref
            c.execute("""DELETE FROM %s WHERE %s""" % \
                        (self._table[0],
                         ' AND '.join(['%s=%%s' % f for f in fields])),
                    data)
        # remove unused cross-references
        for cr in crefs:
            c.execute("""SELECT COUNT(1) FROM %s WHERE %s=%%s""" % \
                        (self._table[0], self._cref[0]), cr)
            if c.fetchone()[0] == 0:
                c.execute("""DELETE FROM %s WHERE %s=%%s""" % \
                        (self._table[1], self._cref[-1]), cr)

        self._origdata = self._data.copy()

class LoggedCursor( MySQLdb.cursors.Cursor ):
    """
    Custom cursor, offering logging and error handling
    """
    def __init__(self, connection):
        self.log = None
        MySQLdb.cursors.Cursor.__init__(self, connection)
        if MySQLdb.__version__ >= ('1','2','2'):
            self.ping = self._ping
        else:
            self.ping = self._ping_pre122
        self.ping()
        
    def _ping(self): self.connection.ping(True)
    def _ping_pre122(self): self.connection.ping()
    def log_query(self, query, args):
        self.log(self.log.DATABASE, ' '.join(query.split()), str(args))

    def execute(self, query, args=()):
        self.ping()
        self.log_query(query, args)
        try:
            return MySQLdb.cursors.Cursor.execute(self, query, args)
        except Exception, e:
            raise MythDBError(MythDBError.DB_RAW, e.args)

    def executemany(self, query, args):
        self.ping()
        self.log_query(query, args)
        try:
            return MySQLdb.cursors.Cursor.executemany(self, query, args)
        except Exception, e:
            raise MythDBError(MythDBError.DB_RAW, e.args)

class DBConnection( object ):
    """
    This is the basic database connection object.
    You dont want to use this directly.
    """

    def __init__(self, dbconn):
        self.log = MythLog('Python Database Connection')
        self.tablefields = None
        self.settings = None
        try:
            self.log(MythLog.DATABASE, "Attempting connection", 
                                                str(dbconn))
            self.db = MySQLdb.connect(  user=   dbconn['DBUserName'],
                                        host=   dbconn['DBHostName'],
                                        passwd= dbconn['DBPassword'],
                                        db=     dbconn['DBName'],
                                        port=   dbconn['DBPort'],
                                        use_unicode=True,
                                        charset='utf8')
        except:
            raise MythDBError(MythError.DB_CONNECTION, dbconn)

    def cursor(self, log=None, type=LoggedCursor):
        c = self.db.cursor(type)
        if log:
            c.log = log
        else:
            c.log = self.log
        return c

class DBCache( object ):
    """
    DBCache(db=None, args=None, **kwargs) -> database connection object

    Basic connection to the mythtv database
    Offers connection and result caching reduce server load

    'db' will accept an existing DBCache object, or any subclass there of
    'args' will accept a tuple of 2-tuples for connection settings
    'kwargs' will accept a series of keyword arguments for connection settings
        'args' and 'kwargs' accept the following values:
            DBHostName
            DBName
            DBUserName
            DBPassword
            SecurityPin

    The class will first use an existing connection if provided, use the 'args'
        and 'kwargs' values if sufficient information is available, falling
        back to '~/.mythtv/config.xml' and finally UPnP detection. If no 
        'SecurityPin' is given, UPnP detection will default to 0000, and if 
        successful, will populate '~/.mythtv/config.xml' with the necessary
        information.

    Available methods:
        obj.cursor()            - open a cursor for direct database
                                  manipulation
        obj.getStorageGroup()   - return a tuple of StorageGroup objects
    """
    logmodule = 'Python Database Connection'
    cursorclass = LoggedCursor
    shared = weakref.WeakValueDictionary()

    def __repr__(self):
        return "<%s '%s' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1], 
                 self.ident, hex(id(self)))

    class _TableFields( QuickDictData ):
        """Provides a dictionary-like list of table fieldnames"""
        class _FieldData( QuickDictData ):
            def __str__(self): return str(list(self))
            def __repr__(self): return str(self).encode('utf-8')
            def __iter__(self): return self._DictIterator(1)
            def __init__(self, result):
                data = [(row[0],
                         QuickDictData(zip( \
                                ('type','null','key','default','extra'),
                                row[1:])) \
                         )for row in result]
                QuickDictData.__init__(self, data)
            def __getitem__(self,key):
                if key in self._data:
                    return self._data[key]
                else:
                    try:
                        return self._field_order[key]
                    except:
                        raise KeyError(str(key))
        _localvars = QuickDictData._localvars+['_db']
        def __str__(self): return str(list(self))
        def __repr__(self): return str(self).encode('utf-8')
        def __iter__(self): return self._DictIterator(1)
        def __init__(self, db, log):
            QuickDictData.__init__(self, ())
            self._db = db
            self._log = log
            self._field_order = self._data.keys()

        def __getitem__(self,key):
            if key not in self._data:
                # pull field list from database
                c = self._db.cursor(self._log)
                try:
                    c.execute("DESC %s" % (key,))
                except:
                    return None

                self._field_order.append(key)
                self._data[key] = self._FieldData(c.fetchall())
                c.close()

            # return cached information
            return self._data[key]

    class _Settings( QuickDictData ):
        """Provides dictionary-like list of hosts"""
        class _HostSettings( QuickDictData ):
            _localvars = QuickDictData._localvars+\
                    ['_db','_host','_insert','_where']
            def __str__(self): return str(list(self))
            def __repr__(self): return str(self).encode('utf-8')
            def __iter__(self): return self._DictIterator(1)
            def __init__(self, db, log, host):
                QuickDictData.__init__(self, ())
                self._db = db
                self._host = host
                self._log = log
                if host is 'NULL':
                    self._insert = """INSERT INTO settings
                                             (value, data, hostname)
                                      VALUES (%s, %s, NULL)"""
                    self._where = """hostname IS NULL"""
                else:
                    self._insert = """INSERT INTO settings
                                             (value, data, hostname)
                                      VALUES (%%s, %%s, '%s')""" % host
                    self._where = """hostname='%s'""" % host

            def __getitem__(self, key):
                if key not in self._data:
                    c = self._db.cursor(self._log)
                    if c.execute("""SELECT data FROM settings
                                    WHERE value=%%s
                                    AND %s LIMIT 1""" \
                                % self._where, key):
                        self._data[key] = c.fetchone()[0]
                    else:
                        self._data[key] = None
                    self._field_order.append(key)
                    c.close()
                return self._data[key]

            def __setitem__(self, key, value):
                c = self._db.cursor(self._log)
                if self[key] is None:
                    if value is None:
                        return
                    c.execute(self._insert, (key, value))
                else:
                    if value is None:
                        del self[key]
                    c.execute("""UPDATE settings
                                 SET data=%%s
                                 WHERE value=%%s
                                 AND %s""" \
                            % self._where, (value, key))
                self._data[key] = value

            def __delitem__(self, key):
                if self[key] is None:
                    return
                c = self._db.cursor(self._log)
                c.execute("""DELETE FROM settings
                             WHERE value=%%s
                             AND %s""" \
                        % self._where, (key,))
                QuickDictData.__delitem__(self, key)

            def getall(self):
                c = self._db.cursor(self._log)
                c.execute("""SELECT value,data FROM settings
                             WHERE %s""" % self._where)
                for k,v in c.fetchall():
                    self._data[k] = v
                    if k not in self._field_order:
                        self._field_order.append(k)
                return self.items()

        _localvars = QuickDictData._localvars+['_db']
        def __str__(self): return str(list(self))
        def __repr__(self): return str(self).encode('utf-8')
        def __iter__(self): return self._DictIterator(1)
        def __init__(self, db, log):
            QuickDictData.__init__(self, ())
            self._db = db
            self._log = log
            self._field_order = self._data.keys()

        def __getitem__(self, key):
            if key not in self._data:
                self._data[key] = self._HostSettings(self._db, self._log, key)
                self._field_order.append(key)
            return self._data[key]

    def __init__(self, db=None, args=None, **dbconn):
        self.db = None
        self.log = MythLog(self.logmodule)
        self.settings = None
        if db is not None:
            # load existing database connection
            self.log(MythLog.DATABASE, "Loading existing connection",
                                    str(db.dbconn))
            dbconn.update(db.dbconn)
        if args is not None:
            # load user defined arguments (takes dict, or key/value tuple)
            self.log(MythLog.DATABASE, "Loading user settings", str(args))
            dbconn.update(args)
        if 'SecurityPin' not in dbconn:
            dbconn['SecurityPin'] = 0
        if not self._check_dbconn(dbconn):
            # insufficient information for connection given
            # try to read from config.xml

            # build list of possible config paths
            home = os.environ.get('HOME','')
            mythconfdir = os.environ.get('MYTHCONFDIR','')
            config_dirs = []
            if not (((home == '') | (home == '/')) &
                    ((mythconfdir == '') | ('$HOME' in mythconfdir))):
                if mythconfdir:
                    config_dirs.append(os.path.expandvars(mythconfdir))
                if home:
                    config_dirs.append(os.path.expanduser('~/.mythtv/'))

            for confdir in config_dirs:
                conffile = os.path.join(confdir,'config.xml')
                dbconn.update({ 'DBHostName':None,  'DBName':None,
                                'DBUserName':None,  'DBPassword':None,
                                'DBPort':0})
                try:
                    config = etree.parse(conffile).getroot()
                    for child in config.find('UPnP').find('MythFrontend').\
                                        find('DefaultBackend').getchildren():
                        if child.tag in dbconn:
                            dbconn[child.tag] = child.text
                except:
                    continue

                if self._check_dbconn(dbconn):
                    self.log(MythLog.IMPORTANT|MythLog.DATABASE,
                           "Using connection settings from %s" % conffile)
                    break
            else:
                # fall back to UPnP
                dbconn = self._listenUPNP(dbconn['SecurityPin'], 5.0)

                # push data to new settings file
                settings = [dbconn[key] for key in \
                                ('SecurityPin','DBHostName','DBUserName',
                                 'DBPassword','DBName','DBPort')]
                config = """<Configuration>
  <UPnP>
    <UDN>
      <MediaRenderer/>
    </UDN>
    <MythFrontend>
      <DefaultBackend>
        <USN/>
        <SecurityPin>%s</SecurityPin>
        <DBHostName>%s</DBHostName>
        <DBUserName>%s</DBUserName>
        <DBPassword>%s</DBPassword>
        <DBName>%s</DBName>
        <DBPort>%s</DBPort>
      </DefaultBackend>
    </MythFrontend>
  </UPnP>
</Configuration>
""" % tuple(settings)
                confdir = os.path.expanduser('~/.mythtv')
                if mythconfdir:
                    confdir = os.path.expandvars(mythconfdir)
                conffile = os.path.join(confdir,'config.xml')
                if not os.access(confdir, os.F_OK):
                    os.mkdir(confdir,0755)
                fp = open(conffile, 'w')
                fp.write(config)
                fp.close()

        if 'DBPort' not in dbconn:
            dbconn['DBPort'] = 3306
        else:
            dbconn['DBPort'] = int(dbconn['DBPort'])
        if dbconn['DBPort'] == 0:
            dbconn['DBPort'] = 3306

        self.dbconn = dbconn
        self.ident = "sql://%s@%s:%d/" % \
                (dbconn['DBName'],dbconn['DBHostName'],dbconn['DBPort'])
        if self.ident in self.shared:
            # reuse existing database connection
            self.db = self.shared[self.ident]
        else:
            # attempt new connection
            self.db = DBConnection(dbconn)

            # check schema version
            self._check_schema('DBSchemaVer',SCHEMA_VERSION)

            # add connection to cache
            self.shared[self.ident] = self.db
            self.db.tablefields = self._TableFields(self.db, self.db.log)
            self.db.settings = self._Settings(self.db, self.db.log)

        # connect to table name cache
        self.tablefields = self.db.tablefields
        self.settings = self.db.settings

    def _listenUPNP(self, pin, timeout):
        # open outbound socket

        upnpport = 1900
        upnptup  = ('239.255.255.250', upnpport)        
        sreq = '\r\n'.join(['M-SEARCH * HTTP/1.1',
                            'HOST: %s:%s' % upnptup,
                            'MAN: "ssdp:discover"',
                            'MX: 5',
                            'ST: ssdp:all',''])

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                             socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            sock.bind(('', upnpport))
        except:
            raise MythDBError(MythError.DB_CREDENTIALS)
        sock.setblocking(0)
        
        # spam the request a couple times
        sock.sendto(sreq, upnptup)
        sock.sendto(sreq, upnptup)
        sock.sendto(sreq, upnptup)

        reLOC = re.compile('http://(?P<ip>[0-9\.]+):(?P<port>[0-9]+)/.*')

        # listen for <timeout>
        atime = time()+timeout
        while time() < atime:
            sleep(0.1)
            try:
                sdata, saddr = sock.recvfrom(2048)
            except socket.error:
                continue # on fault from nonblocking recv

            lines = sdata.split('\n')
            sdict = {'request':lines[0].strip()}
            for line in lines[1:]:
                fields = line.split(':',1)
                if len(fields) == 2:
                    sdict[fields[0].strip().lower()] = fields[1].strip()

            if 'st' not in sdict:
                continue
            if sdict['st'] not in \
                       ('urn:schemas-mythtv-org:device:MasterMediaServer:1',
                        'urn:schemas-mythtv-org:device:SlaveMediaServer:1',
                        'urn:schemas-upnp-org:device:MediaServer:1'):
                continue
            ip, port = reLOC.match(sdict['location']).group(1,2)
            xml = XMLConnection(backend=ip, port=port)
            dbconn = xml.getConnectionInfo(pin)
            if self._check_dbconn(dbconn):
                break

        else:
            sock.close()
            raise MythDBError(MythError.DB_CREDENTIALS)

        sock.close()
        return dbconn

    def _check_dbconn(self,dbconn):
        reqs = ['DBHostName','DBName','DBUserName','DBPassword']
        for req in reqs:
            if req not in dbconn:
                return False
            if dbconn[req] is None:
                return False
        return True

    def _check_schema(self, value, local, name='Database'):
        if self.settings is None:
            c = self.cursor(self.log)
            lines = c.execute("""SELECT data FROM settings 
                                            WHERE value LIKE(%s)""",(value,))
            if lines == 0:
                c.close()
                raise MythDBError(MythError.DB_SETTING, value)
            
            sver = int(c.fetchone()[0])
            c.close()
        else:
            sver = int(self.settings['NULL'][value])

        if local != sver:
            self.log(MythLog.DATABASE|MythLog.IMPORTANT,
                    "%s schema mismatch: we speak %d but database speaks %s" \
                    % (name, local, sver))
            self.db = None
            raise MythDBError(MythError.DB_SCHEMAMISMATCH, value, sver, local)

    def getStorageGroup(self, groupname=None, hostname=None):
        """
        obj.getStorageGroup(groupname=None, hostname=None)
                                            -> tuple of StorageGroup objects
        groupname and hostname can be used as optional filters
        """
        c = self.cursor(self.log)
        where = []
        wheredat = []
        if groupname:
            where.append("groupname=%s")
            wheredat.append(groupname)
        if hostname:
            where.append("hostname=%s")
            wheredat.append(hostname)
        if len(where):
            where = 'WHERE '+' AND '.join(where)
            c.execute("""SELECT * FROM storagegroup %s ORDER BY id""" \
                                                    % where, wheredat)
        else:
            c.execute("""SELECT * FROM storagegroup ORDER BY id""")

        ret = []
        for row in c.fetchall():
            ret.append(StorageGroup(raw=row,db=self))
        return ret

    def cursor(self, log=None):
        if not log:
            log = self.log
        return self.db.cursor(log, self.cursorclass)

class BEConnection( object ):
    """
    This is the basic backend connection object.
    You probably dont want to use this directly.
    """
    logmodule = 'Python Backend Connection'

    class BEConnOpts( QuickDictData ):
        def __init__(self, noshutdown=False, systemevents=False,
                           generalevents=False):
            QuickDictData.__init__(self, (('noshutdown',noshutdown),
                                          ('systemevents',systemevents),
                                          ('generalevents',generalevents)))
        def __and__(self, other):
            res = self.__class__()
            for key in self._field_order:
                if self[key] & other[key]:
                    res[key] = True
            return res
        def __or__(self, other):
            res = self.__class__()
            for key in self._field_order:
                if self[key] | other[key]:
                    res[key] = True
            return res
        def __xor__(self, other):
            res = self.__class__()
            for key in self._field_order:
                if self[key] ^ other[key]:
                    res[key] = True
            return res

    def __init__(self, backend, port, opts=None, timeout=10.0):
        """
        BEConnection(backend, type, db=None) -> backend socket connection

        'backend' can be either a hostname or IP address, or will default
            to the master backend if None.
        """
        self._regusers = weakref.WeakValueDictionary()
        self._regevents = weakref.WeakValueDictionary()
        self._socklock = allocate_lock()

        self.connected = False
        self.threadrunning = False
        self.log = MythLog(self.logmodule)
        self.timeout = timeout

        self.host = backend
        self.port = port
        self.hostname = None
        self.localname = socket.gethostname()

        self.opts = opts
        if self.opts is None:
            self.opts = self.BEConnOpts()

        try:
            self.connect()
        except socket.error, e:
            self.connected = False
            self.log(MythLog.IMPORTANT|MythLog.SOCKET,
                    "Couldn't connect to backend %s:%d" \
                    % (self.host, self.port))
            raise MythBEError(MythError.PROTO_CONNECTION, self.host, self.port)
        except:
            raise

    def __del__(self):
        self.disconnect()

    def connect(self):
        if self.connected:
            return
        self.log(MythLog.SOCKET|MythLog.NETWORK,
                "Connecting to backend %s:%d" % (self.host, self.port))
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.host, self.port))
        self.socket.setblocking(0)
        self.connected = True
        self.check_version()
        self.announce()

    def disconnect(self):
        if not self.connected:
            return
        self.log(MythLog.SOCKET|MythLog.NETWORK,
                "Terminating connection to %s:%d" % (self.host, self.port))
        self.backendCommand('DONE',0)
        self.connected = False
        self.socket.shutdown(1)
        self.socket.close()

    def reconnect(self, force=False):
        # compute new connection options
        opts = self.BEConnOpts()
        for o in self._regusers.values():
            opts |= o
        if (True in (opts^self.opts).values()) or force:
            # options have changed, reconnect
            self.opts = opts
            self.disconnect()
            self.connect()

    def announce(self):
        # set type, 'Playback' blocks backend shutdown
        type = ('Monitor','Playback')[self.opts.noshutdown]

        # set event level, 3=system only, 2=generic only, 1=both, 0=none
        a = self.opts.generalevents
        b = self.opts.systemevents
        eventlevel = (a|b) + (a^b)*(a + 2*b)

        res = self.backendCommand('ANN %s %s %d' % \
                                (type, self.localname, eventlevel))
        if res != 'OK':
            self.log(MythLog.IMPORTANT|MythLog.NETWORK,
                            "Unexpected answer to ANN", res)
            raise MythBEError(MythError.PROTO_ANNOUNCE,
                                                self.host, self.port, res)
        else:
            self.log(MythLog.SOCKET,"Successfully connected to backend",
                                        "%s:%d" % (self.host, self.port))
        self.hostname = self.backendCommand('QUERY_HOSTNAME')

    def check_version(self):
        res = self.backendCommand('MYTH_PROTO_VERSION %s' \
                    % PROTO_VERSION).split(BACKEND_SEP)
        if res[0] == 'REJECT':
            self.log(MythLog.IMPORTANT|MythLog.NETWORK, 
                            "Backend has version %s, and we speak %d" %\
                            (res[1], PROTO_VERSION))
            raise MythBEError(MythError.PROTO_MISMATCH,
                                                int(res[1]), PROTO_VERSION)

    def _recv(self, size=None):
        data = ''
        if size is None:
            # pull 8-byte header, signalling packet size
            size = int(self._recv(8))

        # loop until necessary data has been received
        while size - len(data) > 0:
            # wait for data on the socket
            select([self.socket],[],[])
            # append response to buffer
            data += self.socket.recv(size-len(data))
        return data

    def _send(self, data, header=True):
        if header:
            # prepend 8-byte header, signalling packet size
            data = data.encode('utf-8')
            data = '%-8d%s' % (len(data), data)
            self.log(MythLog.NETWORK, "Sending command", data)

        self.socket.send(data)

    def backendCommand(self, data=None, timeout=None):
        """
        obj.backendCommand(data=None, timeout=None) -> response string

        Sends a formatted command via a socket to the mythbackend. 'timeout'
            will override the default timeout given when the object was 
            created. If 'data' is None, the method will return any events
            in the receive buffer.
        """

        def alrm():
            raise MythBEError('Socket Timeout Reached')

        # return if not connected
        if not self.connected:
            return u''

        # pull default timeout
        if timeout is None:
            timerem = self.timeout
        else:
            timerem = timeout

        # send command string
        if data is not None:
            self._send(data)

        # lock socket access
        with self._socklock:
            signal.signal(signal.SIGALRM, alrm)
            # loop waiting for proper response
            while timerem >= 0:
                # wait timeout for data to be received on the socket
                tic = time()
                if len(select([self.socket],[],[], timeout)[0]) == 0:
                    # no data, return
                    return u''
                timerem -= time()-tic

                try:
                    # pull available socket data
                    signal.alarm(int(timerem)+1)
                    event = self._recv()
                    signal.alarm(0)
                except ValueError:
                    # header read failed, terminate alarm
                    signal.alarm(0)
                    return u''
                except MythBEError:
                    # timeout reached, alarm triggered
                    return u''

                # convert to unicode
                try:
                    event = unicode(''.join([event]), 'utf8')
                except:
                    event = u''.join([event])

                if event[:15] == 'BACKEND_MESSAGE':
                    for r,f in self._regevents.items():
                        # check for matches against registered handlers
                        if r.match(event):
                            try:
                                f(event)
                            except:
                                pass
                    if data is None:
                        # no sent data, no further response expected
                        return u''
                else:
                    return event

    def registeruser(self, uuid, opts):
        self._regusers[uuid] = opts

    def registerevent(self, regex, function):
        self._regevents[regex] = function
        if (not self.threadrunning) and \
           (self.opts.generalevents or self.opts.systemevents):
                start_new_thread(self.eventloop, ())

    def eventloop(self):
        self.threadrunning = True
        while (len(self._regevents) > 0) and self.connected:
            self.backendCommand(timeout=0.001)
            sleep(0.1)
        self.threadrunning = False


class BECache( object ):
    """
    BECache(backend=None, noshutdown=False, db=None)
                                            -> MythBackend connection object

    Basic class for mythbackend socket connections.
    Offers connection caching to prevent multiple connections.

    'backend' allows a hostname or IP address to connect to. If not provided,
                connect will be made to the master backend. Port is always
                taken from the database.
    'db' allows an existing database object to be supplied.
    'noshutdown' specified whether the backend will be allowed to go into
                automatic shutdown while the connection is alive.

    Available methods:
        joinInt()           - convert two signed ints to one 64-bit
                              signed int
        splitInt()          - convert one 64-bit signed int to two
                              signed ints
        backendCommand()    - Sends a formatted command to the backend
                              and returns the response.
    """
    logmodule = 'Python Backend Connection'
    _shared = weakref.WeakValueDictionary()
    _reip = re.compile('(?:\d{1,3}\.){3}\d{1,3}')

    def __repr__(self):
        return "<%s 'myth://%s:%d/' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1], 
                 self.hostname, self.port, hex(id(self)))

    def __init__(self, backend=None, noshutdown=False, db=None, opts=None):
        self.db = DBCache(db)
        self.log = MythLog(self.logmodule, db=self.db)
        self.hostname = None

        if opts is None:
            self.opts = BEConnection.BEConnOpts(noshutdown)
        else:
            self.opts = opts

        if backend is None:
            # no backend given, use master
            self.host = self.db.settings.NULL.MasterServerIP

        else:
            if self._reip.match(backend):
                # given backend is IP address
                self.host = backend
            else:
                # given backend is hostname, pull address from database
                self.hostname = backend
                self.host = self.db.settings[backend].BackendServerIP
                if not self.host:
                    raise MythDBError(MythError.DB_SETTING,
                                            'BackendServerIP', backend)

        if self.hostname is None:
            # reverse lookup hostname from address
            c = self.db.cursor(self.log)
            if c.execute("""SELECT hostname FROM settings
                            WHERE value='BackendServerIP'
                            AND data=%s""", self.host) == 0:
                # no match found
                c.close()
                raise MythDBError(MythError.DB_SETTING, 'BackendServerIP',
                                            self.host)
            self.hostname = c.fetchone()[0]
            c.close()

        # lookup port from database
        self.port = int(self.db.settings[self.hostname].BackendServerPort)
        if not self.port:
            raise MythDBError(MythError.DB_SETTING, 'BackendServerPort',
                                            self.port)

        self._uuid = uuid4()
        self._ident = '%s:%d' % (self.host, self.port)
        if self._ident in self._shared:
            # existing connection found
            # register and reconnect if necessary
            self.be = self._shared[self._ident]
            self.be.registeruser(self._uuid, self.opts)
            self.be.reconnect()
        else:
            # no existing connection, create new
            self.be = BEConnection(self.host, self.port, self.opts)
            self.be.registeruser(self._uuid, self.opts)
            self._shared[self._ident] = self.be

    def backendCommand(self, data):
        """
        obj.backendCommand(data) -> response string

        Sends a formatted command via a socket to the mythbackend.
        """
        return self.be.backendCommand(data)

    def joinInt(self,high,low):
        """obj.joinInt(high, low) -> 64-bit int, from two signed ints"""
        return (int(high) + (int(low)<0))*2**32 + int(low)

    def splitInt(self,integer):
        """obj.joinInt(64-bit int) -> (high, low)"""
        return integer/(2**32),integer%2**32 - (integer%2**32 > 2**31)*2**32

class BEEvent( BECache ):
    __doc__ = BECache.__doc__
    def _listhandlers(self):
        return []

    def __init__(self, backend=None, noshutdown=False, systemevents=False,\
                       generalevents=True, db=None, opts=None):
        if opts is None:
            opts = BEConnection.BEConnOpts(noshutdown,\
                             systemevents, generalevents)
        BECache.__init__(self, backend, db=db, opts=opts)

        # must be done to retain hard reference
        self._events = self._listhandlers()
        for func in self._events:
            self.registerevent(func)

    def registerevent(self, func, regex=None):
        if regex is None:
            regex = func()
        self.be.registerevent(regex, func)

class XMLConnection( object ):
    """
    XMLConnection(backend=None, db=None, port=None) -> Backend status object

    Basic access to MythBackend status page and XML data server

    'backend' allows a hostname or IP, defaulting to the master backend.
    'port' defines the port used to access the backend, retrieved from the
        database if not given.
    'db' allows an existing database connection. Will only be used if
        either 'backend' or 'port' is not defined.
    """
    def __repr__(self):
        return "<%s 'http://%s:%d/' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1], 
                 self.host, self.port, hex(id(self)))

    def __init__(self, backend=None, db=None, port=None):
        if backend and port:
            self.db = None
            self.host, self.port = backend, int(port)
            return
        self.db = DBCache(db)
        self.log = MythLog('Python XML Connection', db=self.db)
        if backend is None:
            # use master backend
            backend = self.db.settings.NULL.MasterServerIP
        if re.match('(?:\d{1,3}\.){3}\d{1,3}',backend):
            # process ip address
            c = self.db.cursor(self.log)
            if c.execute("""SELECT hostname FROM settings
                            WHERE value='BackendServerIP'
                            AND data=%s""", backend) == 0:
                raise MythDBError(MythError.DB_SETTING, 
                                        backend+': BackendServerIP')
            self.host = c.fetchone()[0]
            self.port = int(self.db.settings[self.host].BackendStatusPort)
            c.close()
        else:
            # assume given a hostname
            self.host = backend
            self.port = int(self.db.settings[self.host].BackendStatusPort)
            if not self.port:
                # try a truncated hostname
                self.host = backend.split('.')[0]
                self.port = int(self.db.setting[self.host].BackendStatusPort)
                if not self.port:
                    raise MythDBError(MythError.DB_SETTING, 
                                        backend+': BackendStatusPort')

    def _query(self, path=None, **keyvars):
        """
        obj._query(path=None, **keyvars) -> xml string

        'path' is an optional page to access.
        'keyvars' are a series of optional variables to specify on the URL.
        """
        url = 'http://%s:%d/' % (self.host, self.port)
        if path == 'xml':
            url += 'xml'
        elif path is not None:
            url += 'Myth/%s' % path
        if len(keyvars) > 0:
            fields = []
            for key in keyvars:
                fields.append('%s=%s' % (key, keyvars[key]))
            url += '?'+'&'.join(fields)
        ufd = urlopen(url)
        res = ufd.read()
        ufd.close()
        return res

    def _queryTree(self, path=None, **keyvars):
        """obj._queryTree(path=None, **keyvars) -> xml element tree"""
        xmlstr = self._query(path, **keyvars)
        if xmlstr.find('xmlns') >= 0:
            ind1 = xmlstr.find('xmlns')
            ind2 = xmlstr.find('"', ind1+7) + 1
            xmlstr = xmlstr[:ind1] + xmlstr[ind2:]
        return etree.fromstring(xmlstr)

    def getConnectionInfo(self, pin=0):
        """Return dbconn dict from backend connection info."""
        dbconn = {'SecurityPin':pin}
        conv = {'Host':'DBHostName',    'Port':'DBPort',
                'UserName':'DBUserName','Password':'DBPassword',
                'Name':'DBName'}
        tree = self._queryTree('GetConnectionInfo', Pin=pin)
        if tree.tag == 'GetConnectionInfoResponse':
            for child in tree.find('Info').find('Database'):
                if child.tag in conv:
                    dbconn[conv[child.tag]] = child.text
            if 'DBPort' in dbconn:
                dbconn['DBPort'] = int(dbconn['DBPort'])
        return dbconn
    

class MythLog( LOGLEVEL ):
    """
    MythLog(module='pythonbindings', lstr=None, lbit=None, \
                    db=None, logfile=None) -> logging object

    'module' defines the source of the message in the logs
    'lstr' and 'lbit' define the message filter
        'lbit' takes a bitwise value
        'lstr' takes a string in the normal '-v level' form
        default is set to 'important,general'
    'logfile' sets a file to open and subsequently log to

    The filter level and logfile are global values, shared between all logging
        instances. Furthermode, the logfile can only be set once, and cannot
        be changed.

    The logging object is callable, and implements the MythLog.log() method.
    """

    LEVEL = LOGLEVEL.IMPORTANT|LOGLEVEL.GENERAL
    LOGFILE = None

    helptext = """Verbose debug levels.
 Accepts any combination (separated by comma) of:

 "  all          "  -  ALL available debug output 
 "  most         "  -  Most debug (nodatabase,notimestamp,noextra) 
 "  important    "  -  Errors or other very important messages 
 "  general      "  -  General info 
 "  record       "  -  Recording related messages 
 "  playback     "  -  Playback related messages 
 "  channel      "  -  Channel related messages 
 "  osd          "  -  On-Screen Display related messages 
 "  file         "  -  File and AutoExpire related messages 
 "  schedule     "  -  Scheduling related messages 
 "  network      "  -  Network protocol related messages 
 "  commflag     "  -  Commercial Flagging related messages 
 "  audio        "  -  Audio related messages 
 "  libav        "  -  Enables libav debugging 
 "  jobqueue     "  -  JobQueue related messages 
 "  siparser     "  -  Siparser related messages 
 "  eit          "  -  EIT related messages 
 "  vbi          "  -  VBI related messages 
 "  database     "  -  Display all SQL commands executed 
 "  dsmcc        "  -  DSMCC carousel related messages 
 "  mheg         "  -  MHEG debugging messages 
 "  upnp         "  -  upnp debugging messages 
 "  socket       "  -  socket debugging messages 
 "  xmltv        "  -  xmltv output and related messages 
 "  dvbcam       "  -  DVB CAM debugging messages 
 "  media        "  -  Media Manager debugging messages 
 "  idle         "  -  System idle messages 
 "  channelscan  "  -  Channel Scanning messages 
 "  extra        "  -  More detailed messages in selected levels 
 "  timestamp    "  -  Conditional data driven messages 
 "  none         "  -  NO debug output 
 
 The default for this program appears to be: '-v  "important,general" '

 Most options are additive except for none, all, and important.
 These three are semi-exclusive and take precedence over any
 prior options given.  You can however use something like
 '-v none,jobqueue' to get only JobQueue related messages
 and override the default verbosity level.
 
 The additive options may also be subtracted from 'all' by 
 prefixing them with 'no', so you may use '-v all,nodatabase'
 to view all but database debug messages.
 
 Some debug levels may not apply to this program.
"""

    def __repr__(self):
        return "<%s '%s','%s' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1], 
                 self.module, bin(self.LEVEL), hex(id(self)))

    def __init__(self, module='pythonbindings', lstr=None, lbit=None, \
                    db=None, logfile=None):
        if lstr or lbit:
            self._setlevel(lstr, lbit)
        self.module = module
        self.db = db
        if db:
            self.db = DBCache(db)
        if (logfile is not None) and (self.LOGFILE is None):
            self.LOGFILE = open(logfile,'w')

    @staticmethod
    def _setlevel(lstr=None, lbit=None):
        if lstr:
            MythLog.LEVEL = MythLog._parselevel(lstr)
        elif lbit:
            MythLog.LEVEL = lbit

    @staticmethod
    def _parselevel(lstr=None):
        bwlist = (  'important','general','record','playback','channel','osd',
                    'file','schedule','network','commflag','audio','libav',
                    'jobqueue','siparser','eit','vbi','database','dsmcc',
                    'mheg','upnp','socket','xmltv','dvbcam','media','idle',
                    'channelscan','extra','timestamp')
        if lstr:
            level = MythLog.NONE
            for l in lstr.split(','):
                if l in ('all','most','none'):
                    # set initial bitfield
                    level = eval('MythLog.'+l.upper())
                elif l in bwlist:
                    # update bitfield OR
                    level |= eval('MythLog.'+l.upper())
                elif len(l) > 2:
                    if l[0:2] == 'no':
                        if l[2:] in bwlist:
                            # update bitfield NOT
                            level &= level^eval('MythLog.'+l[2:].upper())
            return level
        else:
            level = []
            for l in xrange(len(bwlist)):
                if MythLog.LEVEL&2**l:
                    level.append(bwlist[l])
            return ','.join(level)

    def log(self, level, message, detail=None, dblevel=None):
        """
        MythLog.log(level, message, detail=None, dblevel=None) -> None

        'level' sets the bitwise log level, to be matched against the log
                    filter. If any bits match true, the message will be logged.
        'message' and 'detail' set the log message content using the format:
                <timestamp> <module>: <message>
                        ---- or ----
                <timestamp> <module>: <message> -- <detail>
        """
        if level&self.LEVEL:
            if (version_info[0]>2) | (version_info[1]>5): # 2.6 or newer
                nowstr = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
            else:
                nowstr = datetime.now().strftime('%Y-%m-%d %H:%M:%S.000')
            if detail is not None:
                lstr = "%s %s: %s -- %s"%(nowstr, self.module, message, detail)
            else:
                lstr = "%s %s: %s" % (nowstr, self.module, message)
            if self.LOGFILE:
                self.LOGFILE.write(lstr+'\n')
                self.LOGFILE.flush()
                os.fsync(self.LOGFILE.fileno())
            else:
                print lstr
        return

        if dblevel is not None:
            if self.db is None:
                self.db = DBCache()
            c = self.db.cursor(self.log)
            c.execute("""INSERT INTO mythlog (module,   priority,   logdate,
                                              host,     message,    details)
                                    VALUES   (%s, %s, %s, %s, %s, %s)""",
                                    (self.module,   dblevel,    now(),
                                     self.host,     message,    detail))
            c.close()

    def __call__(self, level, message, detail=None, dblevel=None):
        self.log(level, message, detail, dblevel)

class MythError( Exception, ERRCODES ):
    """
    MythError('Generic Error Code') -> Exception
    MythError(error_code, additional_arguments) -> Exception

    Objects will have an error string available at obj.args as well as an 
        error code at obj.ecode.  Additional attributes may be available
        depending on the error code.
    """

    def __init__(self, *args):
        if args[0] == self.SYSTEM:
            self.ename = 'SYSTEM'
            self.ecode, self.retcode, self.command, self.stderr = args
            self.args = ("External system call failed: code %d" %self.retcode,)
        elif args[0] == self.DB_RAW:
            self.ename = 'DB_RAW'
            self.ecode, sqlerr = args
            if len(sqlerr) == 1:
                self.sqlcode = 0
                self.sqlerr = sqlerr[0]
                self.args = ("MySQL error: %s" % self.sqlerr,)
            else:
                self.sqlcode, self.sqlerr = sqlerr
                self.args = ("MySQL error %d: %s" % sqlerr,)
        elif args[0] == self.DB_CONNECTION:
            self.ename = 'DB_CONNECTION'
            self.ecode, self.dbconn = args
            self.args = ("Failed to connect to database at '%s'@'%s'" \
                   % (self.dbconn['DBName'], self.dbconn['DBHostName']) \
                            +"for user '%s' with password '%s'." \
                   % (self.dbconn['DBUserName'], self.dbconn['DBPassword']),)
        elif args[0] == self.DB_CREDENTIALS:
            self.ename = 'DB_CREDENTIALS'
            self.ecode = args
            self.args = ("Could not find database login credentials",)
        elif args[0] == self.DB_SETTING:
            self.ename = 'DB_SETTING'
            self.ecode, self.setting, self.hostname = args
            self.args = ("Could not find setting '%s' on host '%s'" \
                            % (self.setting, self.hostname),)
        elif args[0] == self.DB_SCHEMAMISMATCH:
            self.ename = 'DB_SCHEMAMISMATCH'
            self.ecode, self.setting, self.remote, self.local = args
            self.args = ("Mismatched schema version for '%s': " % self.setting \
                            + "database speaks version %d, we speak version %d"\
                                    % (self.remote, self.local),)
        elif args[0] == self.DB_SCHEMAUPDATE:
            self.ename = 'DB_SCHEMAUPDATE'
            self.ecode, sqlerr = args
            if len(sqlerr) == 1:
                self.sqlcode = 0
                self.sqlerr = sqlerr[0]
                self.args = ("Schema update failure: %s" % self.sqlerr)
            else:
                self.sqlcode, self.sqlerr = sqlerr
                self.args = ("Schema update failure %d: %s" % sqlerr,)
        elif args[0] == self.PROTO_CONNECTION:
            self.ename = 'PROTO_CONNECTION'
            self.ecode, self.backend, self.port = args
            self.args = ("Failed to connect to backend at %s:%d" % \
                        (self.backend, self.port),)
        elif args[0] == self.PROTO_ANNOUNCE:
            self.ename = 'PROTO_ANNOUNCE'
            self.ecode, self.backend, self.port, self.response = args
            self.args = ("Unexpected response to ANN on %s:%d - %s" \
                            % (self.backend, self.port, self.response),)
        elif args[0] == self.PROTO_MISMATCH:
            self.ename = 'PROTO_MISMATCH'
            self.ecode, self.remote, self.local = args
            self.args = ("Backend speaks version %s, we speak version %s" % \
                        (self.remote, self.local),)
        elif args[0] == self.FE_CONNECTION:
            self.ename = 'FE_CONNECTION'
            self.ecode, self.frontend, self.port = args
            self.args = ('Connection to frontend %s:%d failed' \
                                            % (self.frontend, self.port),)
        elif args[0] == self.FE_ANNOUNCE:
            self.ename = 'FE_ANNOUNCE'
            self.ecode, self.frontend, self.port = args
            self.args = ('Open socket at %s:%d not recognized as mythfrontend'\
                                            % (self.frontend, self.port),)
        elif args[0] == self.FILE_ERROR:
            self.ename = 'FILE_ERROR'
            self.ecode, self.reason = args
            self.args = ("Error accessing file: " % self.reason,)
        elif args[0] == self.FILE_FAILED_READ:
            self.ename = 'FILE_FAILED_READ'
            self.ecode, self.file = args
            self.args = ("Error accessing %s" %(self.file,self.mode),)
        elif args[0] == self.FILE_FAILED_WRITE:
            self.ename = 'FILE_FAILED_WRITE'
            self.ecode, self.file, self.reason = args
            self.args = ("Error writing to %s, %s" % (self.file, self.reason),)
        else:
            self.ename = 'GENERIC'
            self.ecode = self.GENERIC
            self.args = args
        self.message = str(self.args[0])

class MythDBError( MythError ): pass
class MythBEError( MythError ): pass
class MythFEError( MythError ): pass
class MythFileError( MythError ): pass

class StorageGroup( DBData ):
    """
    StorageGroup(id=None, db=None, raw=None) -> StorageGroup object
    Represents a single storage group entry
    """
    _table = 'storagegroup'
    _where = 'id=%s'
    _setwheredat = 'self.id,'
    _logmodule = 'Python StorageGroup'
    
    def __str__(self):
        return u"<StorageGroup 'myth://%s@%s%s' at %s" % \
                    (self.groupname, self.hostname,
                        self.dirname, hex(id(self)))

    def __repr__(self): return str(self).encode('utf-8')

    def __init__(self, id=None, db=None, raw=None):
        DBData.__init__(self, (id,), db, raw)
        if self._wheredat is None:
            return
        if (self.hostname == socket.gethostname()) or \
              os.access(self.dirname.encode('utf-8'), os.F_OK):
            self.local = True
        else:
            self.local = False

class System( DBCache ):
    """
    System(path=None, setting=None, db=None) -> System object

    'path' sets the object to use a path to an external command
    'setting' pulls the external command from a setting in the database
    """
    logmodule = 'Python system call handler'

    def __init__(self, path=None, setting=None, db=None):
        DBCache.__init__(self, db=db)
        self.log = MythLog(self.logmodule, db=self)
        self.path = ''
        if path is not None:
            self.path = path
        elif setting is not None:
            host = socket.gethostname()
            self.path = self.settings[host][setting]
            if self.path is None:
                raise MythDBError(MythError.DB_SETTING, setting, host)
        else:
            raise MythError('Invalid input to System()')
        self.returncode = 0
        self.stderr = ''

    def __call__(self, *args): return self.command(*args)

    def __str__(self):
        return "<%s '%s' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1], 
                    self.path, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def append(self, *args):
        """
        obj.append(*args) -> None

        Permenantly appends one or more strings to the command
            path, separated by spaces.
        """
        self.path += ' '+' '.join(['%s' % a for a in args])

    def command(self, *args):
        """
        obj.command(*args) -> output string

        Executes external command, adding one or more strings to the
            command during the call. If call exits with a code not 
            equal to 0, a MythError will be raised. The error code and
            stderr will be available in the exception and this object
            as attributes 'returncode' and 'stderr'.
        """
        if self.path is '':
            return ''
        arg = ' '+' '.join(['%s' % a for a in args])
        return self._runcmd('%s %s' % (self.path, arg))

    def _runcmd(self, cmd):
        fd = Popen(cmd, stdout=-1, stderr=-1, shell=True)
        self.returncode = fd.wait()
        stdout,self.stderr = fd.communicate()

        if self.returncode:
            raise MythError(MythError.SYSTEM,self.returncode,cmd,self.stderr)
        return stdout

class Grabber( System ): pass    
