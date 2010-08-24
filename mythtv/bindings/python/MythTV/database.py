# -*- coding: utf-8 -*-

"""
Provides connection cache and data handlers for accessing the database.
"""

from __future__ import with_statement

from static import MythSchema
from altdict import OrdDict, DictData
from logging import MythLog
from msearch import MSearch
from utility import datetime
from exceptions import MythError, MythDBError
from connections import DBConnection, LoggedCursor, XMLConnection

from socket import gethostname
from uuid import uuid4
from lxml import etree
import os
import weakref


class DBData( DictData, MythSchema ):
    """
    DBData.__init__(data=None, db=None) --> DBData object

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
    _logmodule = 'Python DBData'
    _localvars = ['_field_order']

    _table = None
    _where = None
    _key   = None
    _wheredat    = None
    _setwheredat = None

    @classmethod
    def getAllEntries(cls, db=None):
        """cls.getAllEntries() -> tuple of DBData objects"""
        return cls._fromQuery("", ())

    @classmethod
    def _fromQuery(cls, where, args, db=None):
        db = DBCache(db)
        with db as cursor:
            cursor.execute("""SELECT * FROM %s %s""" \
                        % (cls._table, where), args)
            for row in cursor:
                yield cls.fromRaw(row, db)

    @classmethod
    def fromRaw(cls, raw, db=None):
        dbdata = cls(None, db=db)
        DictData.__init__(dbdata, raw)
        dbdata._evalwheredat()
        return dbdata

    def _evalwheredat(self, wheredat=None):
        nokey = 'Invalid DBData subclass: _key or %s required'
        gen = lambda j,s,l: j.join([s % k for k in l])
        if self._where is None:
            # build it from _key
            if self._key is None:
                raise MythError(nokey % '_where')
            self._where = gen(' AND ', '%s=%%s', self._key)

        if self._setwheredat is None:
            if self._key is None:
                raise MythError(nokey % '_setwheredat')
            self._setwheredat = gen('', 'self.%s,', self._key)

        if wheredat is None:
            self._wheredat = eval(self._setwheredat)
        else:
            self._wheredat = tuple(wheredat)

    def _setDefs(self):
        if self._table is None:
            raise MythError('Invalid DBData subclass: _table required')
        self._field_order = self._db.tablefields[self._table]
        self._log = MythLog(self._logmodule)
        self._fillNone()

    def __init__(self, data, db=None):
        dict.__init__(self)
        self._field_order = []
        self._db = DBCache(db)
        self._db._check_schema(self._schema_value,
                                self._schema_local, self._schema_name)
        self._setDefs()

        if data is None: pass
        elif None in data: pass
        else:
            self._evalwheredat(tuple(data))
            self._pull()

    def _process(self, data):
        data = DictData._process(self, data)
        for key, val in self._db.tablefields[self._table].items():
            if (val.type == 'datetime') and (data[key] is not None):
                data[key] = datetime.duck(data[key])
        return data

    def _pull(self):
        """Updates table with data pulled from database."""
        with self._db.cursor(self._log) as cursor:
            count = cursor.execute("""SELECT * FROM %s WHERE %s""" \
                               % (self._table, self._where), self._wheredat)
            if count == 0:
                raise MythError('DBData() could not read from database')
            elif count > 1:
                raise MythError('DBData() could not find unique entry')
            data = cursor.fetchone()
        DictData.update(self, self._process(data))

    def copy(self):
        return self.fromRaw(self.values(), self._db)

    def __str__(self):
        if self._wheredat is None:
            return u"<Uninitialized %s at %s>" % \
                        (self.__class__.__name__, hex(id(self)))
        return u"<%s %s at %s>" % \
                (self.__class__.__name__, \
                 ','.join(["'%s'" % str(v) for v in self._wheredat]), \
                 hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __getstate__(self):
        data = {'data':DictData.__getstate__(self)}
        data['db'] = self._db.dbconn
        data['wheredat'] = self._wheredat
        return data

    def __setstate__(self, state):
        self._field_order = []
        self._db = DBCache(**state['db'])
        self._db._check_schema(self._schema_value,
                                self._schema_local, self._schema_name)
        self._setDefs()
        DictData.__setstate__(self, state['data'])
        if state['wheredat'] is not None:
            self._evalwheredat(state['wheredat'])

class DBDataWrite( DBData ):
    """
    DBDataWrite.__init__(data=None, db=None) --> DBDataWrite object

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
        dict.update(self, self._defaults)

    def _evalwheredat(self, wheredat=None):
        DBData._evalwheredat(self, wheredat)
        self._origdata = dict.copy(self)

    def __init__(self, data=None, db=None):
        DBData.__init__(self, data, db)

    def _import(self, data=None):
        if data is not None:
            data = self._sanitize(data, False)
            dict.update(self, data)

    def _create(self, data=None):
        """
        obj._create(data=None) -> new database row

        Creates a new database entry using given information.
        Will only function with an uninitialized object.
        """
        if self._wheredat is not None:
            raise MythError('DBDataWrite object already bound to '+\
                                                    'existing instance')

        self._import(data)
        data = self._sanitize(dict(self))
        for key in data.keys():
            if data[key] is None:
                del data[key]
        fields = ', '.join(data.keys())
        format_string = ', '.join(['%s' for d in data.values()])
        with self._db.cursor(self._log) as cursor:
            cursor.execute("""INSERT INTO %s (%s) VALUES(%s)""" \
                        % (self._table, fields, format_string), data.values())
            intid = cursor.lastrowid
        return intid

    def create(self, data=None):
        """
        obj.create(data=None) -> self

        Creates a new database entry using the given information.
        Will only function on an uninitialized object.
        """
        self._create(data)
        self._evalwheredat()
        self._pull()
        return self

    def _pull(self):
        DBData._pull(self)
        self._origdata = dict.copy(self)

    def _push(self):
        if (self._where is None) or (self._wheredat is None):
            return
        data = self._sanitize(dict(self))
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
        with self._db.cursor(self._log) as cursor:
            cursor.execute("""UPDATE %s SET %s WHERE %s""" \
                    % (self._table, format_string, self._where), sql_values)
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
        dict.update(self, self._sanitize(data))
        self._push()

    def delete(self):
        """
        obj.delete() -> None

        Delete video entry from database.
        """
        if (self._where is None) or \
                (self._wheredat is None):
            return
        with self._db.cursor(self._log) as cursor:
            cursor.execute("""DELETE FROM %s WHERE %s""" \
                        % (self._table, self._where), self._wheredat)

class DBDataWriteAI( DBDataWrite ):
    """
    DBDataWriteAI.__init__(data=None, db=None) --> DBDataWriteAI object

    Altered DBDataWrite, for use with tables whose unique identifier is
        a single auto-increment field.
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
    def __init__(self, id=None, db=None):
        DBDataWrite.__init__(self, (id,), db)

    def create(self, data=None):
        intid = self._create(data)
        self._evalwheredat([intid])
        self._pull()
        return self

class DBDataRef( list ):
    """
    DBDataRef.__init__(where, db=None) --> DBDataRef object

    Class for managing lists of referenced data, such as recordedmarkup
    Subclasses must provide:
        _table
            Name of database table to be accessed
        _ref
            list of fields for WHERE argument in lookup
    """
    class SubData( OrdDict ):
        # this is one matching entry
        _localvars = ['_field_order', '_changed', '_hash']
        def __init__(self, data):
            OrdDict.__init__(self, data)
            self._changed = True
            self.__hash__()
        def __str__(self): return str(self.items())
        def __repr__(self): return str(self).encode('utf-8')
        def __hash__(self):
            if self._changed:
                self._hash = hash(str(self.values()))
                self._changed = False
            return self._hash
        def __setitem__(self, key, value):
            OrdDict.__setitem__(self, key, value)
            self._changed = True

    def __str__(self):
        self._populate()
        return list.__str__(self)
    def __repr__(self):
        self._populate()
        return list.__repr__(self)

    def __getitem__(self, key):
        self._populate()
        return list.__getitem__(self, key)

    def __setitem__(self, key, value):
        list.__setitem__(self, key, \
                    self.SubData(zip(self._datfields, list(value))))

    def __iter__(self):
        self._populate()
        return list.__iter__(self)

    def __contains__(self, other):
        h = other.__hash__()
        for sd in self:
            if sd.__hash__() == h:
                return True
        return False

    def __xor__(self, other):
        data = []
        for dat in self:
            if dat not in other:
                data.append(dat)
        for dat in other:
            if dat not in self:
                data.append(dat)
        return self.fromCopy(data)
        
    def __and__(self, other):
        data = []
        for dat in self:
            if dat in other:
                data.append(dat)
        return self.fromCopy(data)

    def __init__(self, where, db=None, bypass=False):
        list.__init__(self)
        if bypass: return
        self._db = DBCache(db)
        self._refdat = where

        fields = list(self._db.tablefields[self._table])
        for f in self._ref:
            if f in fields:
                fields.remove(f)
        self._datfields = fields
        self._data = None
        self._populated = False

    def _populate(self, force=False, data=None):
        if self._populated and (not force):
            return
        if data is None:
            with self._db as cursor:
                cursor.execute("""SELECT %s FROM %s WHERE %s""" % \
                            (','.join(self._datfields),
                             self._table,
                             ' AND '.join(['%s=%%s' % f for f in self._ref])),
                         self._refdat)
                for row in cursor:
                    list.append(self, self.SubData(zip(self._datfields, row)))
        else:
            for row in data:
                list.append(self, self.SubData(zip(self._datfields, row)))
        self._populated = True
        self._origdata = self.deepcopy()

    @classmethod
    def fromRaw(cls, data):
        c = cls('', bypass=True)
        c._populated = True
        for dat in data:
            list.append(c, c.SubData(zip(self._datfields, row)))
        return c

    @classmethod
    def fromCopy(cls, data):
        c = cls('', bypass=True)
        c._populated = True
        for dat in data: list.append(c, dat)
        return c

    def copy(self):
        if not self._populated: return []
        return self.fromCopy(self)
    def deepcopy(self):
        if not self._populated: return []
        return self.fromCopy([dat.copy() for dat in self])

    def revert(self):
        """Delete all local changes to database."""
        self._populate()
        for i in reversed(range(len(self))): del self[i]
        for i in self._origdata: list.append(self, i)

    def commit(self):
        """Push all local changes to database."""
        if not self._populated:
            return
        # push changes to database
        diff = self^self._origdata
        if len(diff) == 0:
            return
        fields = list(self._ref)+list(self._datfields)

        with self._db as cursor:
            # remove old entries
            for v in (self._origdata&diff):
                data = list(self._refdat)+v.values()
                wf = []
                for i in range(len(data)):
                    if data[i] is None:
                        wf.append('%s IS %%s' % fields[i])
                    else:
                        wf.append('%s=%%s' % fields[i])
                cursor.execute("""DELETE FROM %s WHERE %s""" % \
                                   (self._table, ' AND '.join(wf)), data)

            # add new entries
            data = []
            for v in (self&diff):
                # add new entries
                data.append(list(self._refdat)+v.values())
            if len(data) > 0:
                cursor.executemany("""INSERT INTO %s (%s) VALUES(%s)""" % \
                                    (self._table,
                                     ','.join(fields),
                                     ','.join(['%s' for a in fields])), data)
        self._origdata = self.deepcopy()

    def append(self, *data):
        """Adds a list of data matching those specified in '_datfields'."""
        self._populate()
        sd = self.SubData(zip(self._datfields, data))
        if sd in self:
            return
        list.append(self, sd)
    def add(self, *data): self.append(*data)

    def delete(self, *data):
        """Deletes an entry matching the provided list of data."""
        # delete entry from local copy
        self._populate()
        sd = self.SubData(zip(self._datfields, data))
        del self[self.index(sd)]

    def clean(self):
        """Remove all entries and commit."""
        # clear all and commit
        self._populate()
        for i in reversed(range(len(self))): del self[i]
        self.commit()

    def _picklelist(self):
        self._populate()
        return [a.values() for a in self.copy()]

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

    def __init__(self, where, db=None, bypass=False):
        list.__init__(self)
        if bypass: return
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
        self._populated = False

    def _populate(self, force=False, data=None):
        if self._populated and (not force):
            return
        datfields = self._datfields
        reffield = '%s.%s' % (self._table[1],self._cref[-1])

        if data is None:
            with self._db as cursor:
                cursor.execute("""SELECT %s FROM %s JOIN %s ON %s WHERE %s""" % \
                          (','.join(datfields+[reffield]),
                             self._table[0],
                             self._table[1],
                             '%s.%s=%s.%s' % \
                                    (self._table[0], self._cref[0],
                                     self._table[1], self._cref[-1]),
                             ' AND '.join(['%s=%%s' % f for f in self._ref])),
                        self._refdat)

                for row in cursor:
                    sd = self.SubData(zip(datfields, row[:-1]))
                    sd._cref = row[-1]
                    list.append(self, sd)
        else:
            for row in data:
                sd = self.SubData(zip(datfields, row[:-1]))
                sd._cref = row[-1]
                list.append(self, sd)

        self._populated = True
        self._origdata = self.deepcopy()

    def commit(self):
        if not self._populated:
            return
        # push changes to database
        diff = self^self._origdata
        if len(diff) == 0:
            return
        c = self._db.cursor()

        with self._db as cursor:
            # add new cross-references
            newdata = self&diff
            for d in newdata:
                data = [d[a] for a in self._crdatfields]
                fields = self._crdatfields
                if cursor.execute("""SELECT %s FROM %s WHERE %s""" % \
                            (self._cref[-1], self._table[1],
                             ' AND '.join(['%s=%%s' % f for f in fields])),
                        data):
                    # match found
                    d._cref = cursor.fetchone()[0]
                else:
                    cursor.execute("""INSERT INTO %s (%s) VALUES(%s)""" % \
                            (self._table[1],
                             ','.join(self._crdatfields),
                             ','.join(['%s' for a in data])),
                        data)
                    d._cref = cursor.lastrowid
            # add new references
            for d in newdata:
                data = [d[a] for a in self._rdatfields]+[d._cref]+self._refdat
                fields = self._rdatfields+self._cref[:1]+self._ref
                cursor.execute("""INSERT INTO %s (%s) VALUES(%s)""" % \
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
                cursor.execute("""DELETE FROM %s WHERE %s""" % \
                        (self._table[0],
                         ' AND '.join(['%s=%%s' % f for f in fields])),
                    data)
            # remove unused cross-references
            for cr in crefs:
                cursor.execute("""SELECT COUNT(1) FROM %s WHERE %s=%%s""" % \
                        (self._table[0], self._cref[0]), cr)
                if cursor.fetchone()[0] == 0:
                    cursor.execute("""DELETE FROM %s WHERE %s=%%s""" % \
                            (self._table[1], self._cref[-1]), cr)

        self._origdata = self.deepcopy()

    def _picklelist(self):
        self._populate()
        return [a.values()+[a._cref] for a in self.copy()]

class DBCache( MythSchema ):
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

    class _TableFields( OrdDict ):
        """Provides a dictionary-like list of table fieldnames"""
        class _FieldData( OrdDict ):
            def __str__(self): return str(list(self))
            def __repr__(self): return str(self).encode('utf-8')
            def __iter__(self): return self.iterkeys()
            def __init__(self, result):
                data = [(row[0],
                         OrdDict(zip( \
                                ('type','null','key','default','extra'),
                                row[1:])) \
                         )for row in result]
                OrdDict.__init__(self, data)
            def __getitem__(self,key):
                if key in self:
                    return OrdDict.__getitem__(self,key)
                else:
                    try:
                        return self._field_order[key]
                    except:
                        raise KeyError(str(key))
        _localvars = ['_field_order','_db','_log']
        def __str__(self): return str(list(self))
        def __repr__(self): return str(self).encode('utf-8')
        def __iter__(self): return self.iterkeys()
        def __init__(self, db, log):
            OrdDict.__init__(self)
            self._db = db
            self._log = log

        def __getitem__(self,key):
            if key not in self:
                with self._db.cursor(self._log) as cursor:
                    # pull field list from database
                    try:
                        cursor.execute("DESC %s" % (key,))
                    except Exception, e:
                        MythDBError(MythDBError.DB_RAW, e.args)
                    self[key] = self._FieldData(cursor.fetchall())

            # return cached information
            return OrdDict.__getitem__(self,key)

    class _Settings( OrdDict ):
        """Provides dictionary-like list of hosts"""
        class _HostSettings( OrdDict ):
            _localvars = ['_field_order','_log','_db','_host','_insert','_where']
            def __str__(self): return str(list(self))
            def __repr__(self): return str(self).encode('utf-8')
            def __iter__(self): return self.iterkeys()
            def __init__(self, db, log, host):
                OrdDict.__init__(self)
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
                if key not in self:
                    with self._db.cursor(self._log) as cursor:
                        if cursor.execute("""SELECT data FROM settings
                                             WHERE value=%%s
                                             AND %s LIMIT 1""" \
                                         % self._where, key):
                            OrdDict.__setitem__(self, key, \
                                                cursor.fetchone()[0])
                        else:
                            return None
                return OrdDict.__getitem__(self, key)

            def __setitem__(self, key, value):
                if value is None:
                    if self[key] is None:
                        return
                    del self[key]
                else:
                    with self._db.cursor(self._log) as cursor:
                        if self[key] is not None:
                            cursor.execute("""UPDATE settings
                                              SET data=%%s
                                              WHERE value=%%s
                                              AND %s""" \
                                    % self._where, (value, key))
                        else:
                            cursor.execute(self._insert, (key, value))
                OrdDict.__setitem__(self,key,value)

            def __delitem__(self, key):
                if self[key] is None:
                    return
                with self._db.cursor(self._log) as cursor:
                    cursor.execute("""DELETE FROM settings
                                      WHERE value=%%s
                                      AND %s""" \
                            % self._where, (key,))
                OrdDict.__delitem__(self, key)

            def get(self, value, default=None):
                res = self[value]
                if res is None: res = default
                return res

            def getall(self):
                with self._db.cursor(self._log) as cursor:
                    cursor.execute("""SELECT value,data
                                      FROM settings
                                      WHERE %s""" % self._where)
                    for k,v in cursor:
                        OrdDict.__setitem__(self, k, v)
                return self.iteritems()

        _localvars = ['_field_order','_log','_db']
        def __str__(self): return str(list(self))
        def __repr__(self): return str(self).encode('utf-8')
        def __iter__(self): return self.iterkeys()
        def __init__(self, db, log):
            OrdDict.__init__(self)
            self._db = db
            self._log = log

        def __getitem__(self, key):
            if key not in self:
                self[key] = self._HostSettings(self._db, self._log, key)
            return OrdDict.__getitem__(self,key)

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
                dbconn.update(self._readXML(conffile))

                if self._check_dbconn(dbconn):
                    self.log(MythLog.IMPORTANT|MythLog.DATABASE,
                           "Using connection settings from %s" % conffile)
                    break
            else:
                # fall back to UPnP
                pin = dbconn['SecurityPin']
                for xml in XMLConnection.fromUPNP(5.0):
                    dbconn = xml.getConnectionInfo(pin)
                    if self._check_dbconn(dbconn):
                        break
                else:
                    raise MythDBError(MythError.DB_CREDENTIALS)

                # push data to new settings file
                self._writeXML(dbconn)

        if 'DBPort' in dbconn:
            if dbconn['DBPort'] == '0':
                dbconn['DBPort'] = 3306
            else:
                dbconn['DBPort'] = int(dbconn['DBPort'])
        else:
            dbconn['DBPort'] = 3306

        if 'LocalHostName' in dbconn:
            if dbconn['LocalHostName'] is None:
                dbconn['LocalHostName'] = gethostname()
        else:
            dbconn['LocalHostName'] = gethostname()

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
            self._check_schema(self._schema_value, self._schema_local,
                               self._schema_name)

            # add connection to cache
            self.shared[self.ident] = self.db
            self.db.tablefields = self._TableFields(self.db, self.db.log)
            self.db.settings = self._Settings(self.db, self.db.log)

        # connect to table name cache
        self.tablefields = self.db.tablefields
        self.settings = self.db.settings

    def _readXML(self, path):
        dbconn = { 'DBHostName':None,  'DBName':None, 'DBUserName':None,
                   'DBPassword':None,  'DBPort':0,    'LocalHostName':None}
        try:
            config = etree.parse(path).getroot()
            for child in config.find('UPnP').find('MythFrontend').\
                                find('DefaultBackend').getchildren():
                if child.tag in dbconn:
                    dbconn[child.tag] = child.text
        except: pass
        return dbconn

    def _writeXML(self, dbconn):
        doc = etree.Element('Configuration')
        upnpnode = doc.makeelement('UPnP')
        doc.append(upnpnode)

        udnnode = doc.makeelement('UDN')
        node = doc.makeelement('MediaRenderer')
        node.text = str(uuid4())
        udnnode.append(node)
        upnpnode.append(udnnode)

        fenode = doc.makeelement('MythFrontend')
        benode = doc.makeelement('DefaultBackend')
        benode.append(doc.makeelement('USN'))
        for name in ('SecurityPin','DBHostName','DBUserName',
                     'DBPassword','DBName','DBPort'):
            node = doc.makeelement(name)
            node.text = str(dbconn[name])
            benode.append(node)
        fenode.append(benode)
        upnpnode.append(fenode)

        confdir = os.path.expanduser('~/.mythtv')
        mythconfdir = os.environ.get('MYTHCONFDIR','')
        if mythconfdir:
            confdir = os.path.expandvars(mythconfdir)
        conffile = os.path.join(confdir,'config.xml')
        if not os.access(confdir, os.F_OK):
            os.mkdir(confdir,0755)
        fp = open(conffile, 'w')
        fp.write(etree.tostring(doc))
        fp.close()

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
            with self.cursor(self.log) as cursor:
                if cursor.execute("""SELECT data
                                     FROM settings
                                     WHERE value LIKE(%s)""",
                                (value,)) == 0:
                    raise MythDBError(MythError.DB_SETTING, value)

                sver = int(cursor.fetchone()[0])
        else:
            sver = int(self.settings['NULL'][value])

        if local != sver:
            self.log(MythLog.DATABASE|MythLog.IMPORTANT,
                    "%s schema mismatch: we speak %d but database speaks %s" \
                    % (name, local, sver))
            self.db = None
            raise MythDBError(MythError.DB_SCHEMAMISMATCH, value, sver, local)

    def gethostname(self):
        return self.dbconn['LocalHostName']

    def getStorageGroup(self, groupname=None, hostname=None):
        """
        obj.getStorageGroup(groupname=None, hostname=None)
                                            -> tuple of StorageGroup objects
        groupname and hostname can be used as optional filters
        """
        where = []
        wheredat = []
        if groupname:
            where.append("groupname=%s")
            wheredat.append(groupname)
        if hostname:
            where.append("hostname=%s")
            wheredat.append(hostname)
        with self.cursor(self.log) as cursor:
            if len(where):
                where = 'WHERE '+' AND '.join(where)
                cursor.execute("""SELECT * FROM storagegroup %s
                                  ORDER BY id""" % where, wheredat)
            else:
                cursor.execute("""SELECT * FROM storagegroup
                                  ORDER BY id""")

            for row in cursor:
                yield StorageGroup.fromRaw(row, self)

    def cursor(self, log=None):
        if not log:
            log = self.log
        return self.db.cursor(log, self.cursorclass)

    def __enter__(self): return self.db.__enter__()
    def __exit__(self, ty, va, tr): return self.db.__exit__(ty, va, tr)

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

    def __init__(self, id=None, db=None):
        DBData.__init__(self, (id,), db)
        if self._wheredat is None:
            return

    def _evalwheredat(self, wheredat=None):
        DBData._evalwheredat(self, wheredat)
        if (self.hostname == self._db.gethostname()) or \
              os.access(self.dirname.encode('utf-8'), os.F_OK):
            self.local = True
        else:
            self.local = False

