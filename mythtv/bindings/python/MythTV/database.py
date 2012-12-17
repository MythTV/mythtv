# -*- coding: utf-8 -*-

"""
Provides connection cache and data handlers for accessing the database.
"""

from MythTV.static import MythSchema
from MythTV.altdict import OrdDict, DictData
from MythTV.logging import MythLog
from MythTV.msearch import MSearch
from MythTV.utility import datetime, _donothing, QuickProperty
from MythTV.exceptions import MythError, MythDBError, MythTZError
from MythTV.connections import DBConnection, LoggedCursor, XMLConnection

from socket import gethostname
from uuid import UUID, uuid1, uuid4
from lxml import etree
import datetime as _pydt
import time as _pyt
import weakref
import os


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
    _field_order = []
    _logmodule = None

    _table = None
    _where = None
    _key   = None
    _wheredat    = None
    _setwheredat = None

    @classmethod
    def _setClassDefs(cls, db=None):
        db = DBCache(db)
        log = MythLog('DBData Setup (%s)' % cls.__name__)
        if cls._table is None:
            # pull table name from class name
            cls._table = cls.__name__.lower()
            log(log.DATABASE, log.DEBUG,
                'set _table to %s' % cls._table)

        if cls._logmodule is None:
            # pull log module from class name
            cls._logmodule = 'Python %s' % cls.__name__
            log(log.DATABASE, log.DEBUG,
                'set _logmodule to %s' % cls._logmodule)

        if cls._field_order == []:
            # pull field order from database
            cls._field_order = db.tablefields[cls._table]

        if (cls._setwheredat is None) or (cls._where is None):
            if cls._key is None:
                # pull primary key fields from database
                with db.cursor(log) as cursor:
                    cursor.execute("""SHOW KEYS FROM %s
                                      WHERE Key_name='PRIMARY'""" \
                                            % cls._table)
                    cls._key = [k[4] for k in sorted(cursor.fetchall(),
                                                     key=lambda x: x[3])]
                log(log.DATABASE, log.DEBUG,
                    'set _key to %s' % str(cls._key))
            gen = lambda j,s,l: j.join([s % k for k in l])
            cls._where = gen(' AND ', '%s=?', cls._key)
            cls._setwheredat = gen('', 'self.%s,', cls._key)
            log(log.DATABASE, log.DEBUG,
                'set _where to %s' % cls._where)
            log(log.DATABASE, log.DEBUG,
                'set _setwheredat to %s' % cls._setwheredat)

        # class has been processed, turn method into no-op
        cls._setClassDefs = classmethod(_donothing)

    @classmethod
    def getAllEntries(cls, db=None):
        """cls.getAllEntries() -> tuple of DBData objects"""
        return cls._fromQuery("", ())

    @classmethod
    def _fromQuery(cls, where, args, db=None):
        db = DBCache(db)
        cls._setClassDefs(db)
        with db as cursor:
            cursor.execute("""SELECT * FROM %s %s""" \
                        % (cls._table, where), args)
            for row in cursor:
                try:
                    yield cls.fromRaw(row, db)
                except MythDBError, e:
                    if e.ecode == MythError.DB_RESTRICT:
                        pass

    @classmethod
    def fromRaw(cls, raw, db=None):
        db = DBCache(db)
        cls._setClassDefs(db)
        dbdata = cls(None, db=db)
        DictData.__init__(dbdata, raw)
        dbdata._evalwheredat()
        dbdata._postinit()
        return dbdata

    def __setitem__(self, key, value):
        for k,v in self._field_order.items():
            if k == key:
                if v.type in ('datetime','timestamp'):
                    value = datetime.duck(value)
                break
        DictData.__setitem__(self, key, value)

    def _evalwheredat(self, wheredat=None):
        if wheredat is None:
            self._wheredat = eval(self._setwheredat)
        else:
            self._wheredat = tuple(wheredat)

    def _getwheredat(self):
        data = list(self._wheredat)
        for i,val in enumerate(data):
            if isinstance(val, datetime):
                data[i] = val.asnaiveutc()
        return data

    def _postinit(self):
        pass

    def _setDefs(self):
        self.__class__._setClassDefs(self._db)
        self._log = MythLog(self._logmodule)
        self._fillNone()

    def __init__(self, data, db=None):
        dict.__init__(self)
        self._db = DBCache(db)
        self._db._check_schema(self._schema_value, self._schema_local,
                                self._schema_name, self._schema_update)
        self._setDefs()

        if self._key is not None:
            if len(self._key) == 1:
                data = (data,)

        if data is None: pass
        elif None in data: pass
        else:
            self._evalwheredat(tuple(data))
            self._pull()
            self._postinit()

    def _process(self, data):
        data = DictData._process(self, data)
        for key, val in self._field_order.items():
            if (val.type in ('datetime','timestamp')) \
                    and (data[key] is not None):
                try:
                    # try converting to local time
                    data[key] = datetime.fromnaiveutc(data[key])
                except MythTZError:
                    # just store as UTC
                    data[key] = datetime.fromDatetime(dt,
                                                tzinfo=datetime.UTCTZ())
        return data

    def _pull(self):
        """Updates table with data pulled from database."""
        with self._db.cursor(self._log) as cursor:
            cursor.execute("""SELECT * FROM %s WHERE %s""" \
                       % (self._table, self._where), self._getwheredat())
            res = cursor.fetchall()
            if len(res) == 0:
                raise MythError('DBData() could not read from database')
            elif len(res) > 1:
                raise MythError('DBData() could not find unique entry')
            data = res[0]
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
        self._db._check_schema(self._schema_value, self._schema_local,
                               self._schema_name, self._schema_update)
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

    @classmethod
    def _setClassDefs(cls, db=None):
        db = DBCache(db)
        super(DBDataWrite, cls)._setClassDefs(db)
        if cls._defaults is None:
            cls._defaults = {}
        create = cls._create_normal
        if cls._key is not None:
            if len(cls._key) == 1:
                if 'auto_increment' in \
                        db.tablefields[cls._table][cls._key[0]].extra:
                    create = cls._create_autoincrement
                    cls._defaults[cls._key[0]] = None
        if not hasattr(cls, 'create'):
            cls.create = create

    def _sanitize(self, data, fill=True):
        """Remove fields from dictionary that are not in database table."""
        data = data.copy()
        for key in data.keys():
            if key not in self._field_order:
                del data[key]
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

    def _create(self, data=None, cursor=None):
        """
        obj._create(data=None) -> new database row

        Creates a new database entry using given information.
        Will only function with an uninitialized object.
        """
        if self._wheredat is not None:
            raise MythError('DBDataWrite object already bound to '+\
                                                    'existing instance')

        if cursor is None:
            with self._db.cursor(self._log) as cursor:
                self._create(data, cursor)
            return

        if data is not None:
            for k,v in self._field_order.items():
                if (k in data) and (data[k] is not None) and \
                        (v.type in ('datetime','timestamp')):
                    data[k] = datetime.duck(data[k])

        self._import(data)
        data = self._sanitize(dict(self))
        for key,val in data.items():
            if val is None:
                del data[key]
            elif isinstance(val, datetime):
                data[key] = val.asnaiveutc()
        fields = ', '.join(data.keys())
        format_string = ', '.join(['?' for d in data.values()])
        cursor.execute("""INSERT INTO %s (%s) VALUES(%s)""" \
                    % (self._table, fields, format_string), data.values())

    def _create_normal(self, data=None):
        """
        obj.create(data=None) -> self

        Creates a new database entry using the given information.
        Will only function on an uninitialized object.
        """
        self._create(data)
        self._evalwheredat()
        self._pull()
        self._postinit()
        return self

    def _create_autoincrement(self, data=None):
        """
        obj.create(data=None) -> self

        Creates a new database entry using the given information.
        Will only function on an uninitialized object.
        """
        with self._db.cursor(self._log) as cursor:
            self._create(data, cursor)
            intid = cursor.lastrowid
        self._evalwheredat([intid])
        self._pull()
        self._postinit()
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
            elif isinstance(value, datetime):
                data[key] = value.asnaiveutc()
        if len(data) == 0:
            # no updates
            return
        format_string = ', '.join(['%s = ?' % d for d in data])
        sql_values = data.values()
        sql_values.extend(self._getwheredat())
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
        data = self._sanitize(data)

        for k in data:
            if self._field_order[k].type in ('datetime', 'timestamp'):
                data[k] = datetime.duck(data[k])

        dict.update(self, data)
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
    _readonly = False

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
                self._hash = hash(sum(map(hash,self.values())))
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

    @classmethod
    def _setClassDefs(cls, db=None):
        db = DBCache(db)
        fields = list(db.tablefields[cls._table])
        for f in cls._ref:
            if f in fields:
                fields.remove(f)
        cls._datfields = fields

        if cls._readonly:
            cls.commit = _donothing

        cls._setClassDefs = classmethod(_donothing)

    def __init__(self, where, db=None, bypass=False):
        list.__init__(self)
        self._db = DBCache(db)
        self._setClassDefs(self._db)
        if bypass: return

        self._populated = False

        where = list(where)
        for i,v in enumerate(where):
            if isinstance(v, datetime):
                where[i] = v.asnaiveutc()
        self._refdat = tuple(where)

    def _populate(self, force=False, data=None):
        if self._populated and (not force):
            return
        if data is None:
            with self._db as cursor:
                cursor.execute("""SELECT %s FROM %s WHERE %s""" % \
                            (','.join(self._datfields),
                             self._table,
                             ' AND '.join(['%s=?' % f for f in self._ref])),
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
                for i,v in enumerate(data):
                    if v is None:
                        wf.append('%s IS ?' % fields[i])
                    else:
                        wf.append('%s=?' % fields[i])
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
                                     ','.join(['?' for a in fields])), data)
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
    DBDataCRef.__init__(where, db=None) --> DBDataRef object

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

    @classmethod
    def _setClassDefs(cls, db=None):
        db = DBCache(db)
        rfields = list(db.tablefields[cls._table[0]])
        crfields = list(db.tablefields[cls._table[1]])

        for f in cls._ref+cls._cref[:1]:
            if f in rfields:
                rfields.remove(f)
        if cls._cref[-1] in crfields:
            crfields.remove(cls._cref[-1])

        cls._rdatfields = rfields
        cls._crdatfields = crfields
        cls._datfields = crfields+rfields

        cls._setClassDefs = classmethod(_donothing)

    def __init__(self, where, db=None, bypass=False):
        list.__init__(self)
        self._db = DBCache(db)
        self._setClassDefs(self._db)
        if bypass: return

        self._populated = False

        where = list(where)
        for i,v in enumerate(where):
            if isinstance(v, datetime):
                where[i] = v.asnaiveutc()
        self._refdat = where

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
                             ' AND '.join(['%s=?' % f for f in self._ref])),
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

        with self._db as cursor:
            # add new cross-references
            newdata = self&diff
            for d in newdata:
                data = [d[a] for a in self._crdatfields]
                fields = self._crdatfields
                cursor.execute("""SELECT %s FROM %s WHERE %s""" % \
                            (self._cref[-1], self._table[1],
                             ' AND '.join(['%s=?' % f for f in fields])),
                        data)
                res = cursor.fetchone()
                if res is not None:
                    # match found
                    d._cref = res[0]
                    cursor.nextset()
                else:
                    cursor.execute("""INSERT INTO %s (%s) VALUES(%s)""" % \
                            (self._table[1],
                             ','.join(self._crdatfields),
                             ','.join(['?' for a in data])),
                        data)
                    d._cref = cursor.lastrowid
            # add new references
            for d in newdata:
                data = [d[a] for a in self._rdatfields]+[d._cref]+self._refdat
                fields = self._rdatfields+self._cref[:1]+self._ref
                cursor.execute("""INSERT INTO %s (%s) VALUES(%s)""" % \
                        (self._table[0],
                         ','.join(fields),
                         ','.join(['?' for a in data])),
                    data)

            # remove old references
            olddata = self._origdata&diff
            crefs = [d._cref for d in olddata]
            for d in olddata:
                data = [d[a] for a in self._rdatfields]+[d._cref]+self._refdat
                fields = self._rdatfields+self._cref[:1]+self._ref
                cursor.execute("""DELETE FROM %s WHERE %s""" % \
                        (self._table[0],
                         ' AND '.join(['%s=?' % f for f in fields])),
                    data)
            # remove unused cross-references
            for cr in crefs:
                cursor.execute("""SELECT COUNT(1) FROM %s WHERE %s=?""" % \
                        (self._table[0], self._cref[0]), [cr])
                if cursor.fetchone()[0] == 0:
                    cursor.execute("""DELETE FROM %s WHERE %s=?""" % \
                            (self._table[1], self._cref[-1]), [cr])
                cursor.nextset()

        self._origdata = self.deepcopy()

    def _picklelist(self):
        self._populate()
        return [a.values()+[a._cref] for a in self.copy()]

class DatabaseConfig( object ):
    class _WakeOnLanConfig( object ):
        def __init__(self):
            self.enabled = False
            self.waittime = 0
            self.retrycount = 5
            self.command = "echo 'WOLsqlServerCommand not set'"

        def readXML(self, element):
            self.enabled = bool(element.xpath('Enabled')[0].text)
            self.waittime = int(element.xpath('SQLReconnectWaitTime')[0].text)
            self.retrycount = int(element.xpath('SQLConnectRetry')[0].text)
            self.command = element.xpath('Command')[0].text

        def writeXML(self, element):
            enabled = element.makeelement('Enabled')
            enabled.text = '1' if self.enabled else '0'
            element.append(enabled)

            waittime = element.makeelement('SQLReconnectWaitTime')
            waittime.text = str(self.waittime)
            element.append(waittime)

            retrycount = element.makeelement('SQLConnectRetry')
            retrycount.text = str(self.retrycount)
            element.append(retrycount)

            command = element.makeelement('Command')
            command.text = self.command
            element.append(command)

    _conf_trans = {'PingHost':'pinghost', 'Host':'hostname',
                   'UserName':'username', 'Password':'password',
                   'DatabaseName':'database', 'Port':'port'}

    uuid =      QuickProperty('_uuid', uuid1(), UUID)
    pinghost =  QuickProperty('_pinghost', False, bool)
    port =      QuickProperty('_port', 3306, int)
    pin =       QuickProperty('_pin', 0000, int)
    hostname =  QuickProperty('_hostname', '127.0.0.1')
    username =  QuickProperty('_username', 'mythtv')
    password =  QuickProperty('_password', 'mythtv')
    database =  QuickProperty('_database', 'mythconverg')

    @QuickProperty('_profile', gethostname())
    def profile(self, value):
        if value == 'my-unique-identifier-goes-here':
            raise Exception
        return value

    @property
    def ident(self):
        return "sql://{0.database}@{0.hostname}:{0.port}/".format(self)

    def __init__(self, args, **kwargs):
        self._default = True
        self._allowwrite = False
        self.wol = self._WakeOnLanConfig()

        kwargs.update(args)
        if len(kwargs):
            self._default = False

        for key,attr in (('LocalHostName', 'profile'),
                         ('DBHostName', 'hostname'),
                         ('DBUserName', 'username'),
                         ('DBPassword', 'password'),
                         ('DBName', 'database'),
                         ('DBPort', 'port'),
                         ('SecurityPin', 'pin')):
            if key in kwargs:
                setattr(self, attr, kwargs[key])

    def __hash__(self):
        return hash(self.ident)

    def copy(self):
        cls = self.__class__
        obj = cls(())
        for attr in ('uuid', 'pinghost', 'port', 'pin', 'hostname',
                     'username', 'password', 'database', 'profile'):
            if not getattr(cls, attr).isDefault(self):
                setattr(obj, attr, getattr(self, attr))
        return obj

    def test(self, log):
        # try using user-specified values
        if not self._default:
            log(log.GENERAL, log.DEBUG,
                            "Trying user-specified database credentials.")
            yield self

        # try reading config files
        confdir = os.environ.get('MYTHCONFDIR', '')
        if confdir and (confdir != '/'):
            obj = self.copy()
            obj.confdir = confdir
            if obj.readXML(confdir):
                log(log.GENERAL, log.DEBUG,
                          "Trying database credentials from: {0}"\
                                .format(os.path.join(confdir,'config.xml')))
                yield obj
            elif obj.readOldXML(confdir):
                log(log.GENERAL, log.DEBUG,
                          "Trying old format database credentials from: {0}"\
                                .format(os.path.join(confdir,'config.xml')))
                yield obj
            else:
                log(log.GENERAL, log.DEBUG,
                          "Failed to read database credentials from: {0}"\
                                .format(os.path.join(confdir,'config.xml')))

        homedir = os.environ.get('HOME', '')
        if homedir and (homedir != '/'):
            obj = self.copy()
            obj.confdir = os.path.join(homedir, '.mythtv')
            if obj.readXML(os.path.join(homedir,'.mythtv')):
                log(log.GENERAL, log.DEBUG,
                          "Trying database credentials from: {0}"\
                                .format(os.path.join(homedir,'config.xml')))
                yield obj
            elif obj.readOldXML(os.path.join(homedir, '.mythtv')):
                log(log.GENERAL, log.DEBUG,
                          "Trying old format database credentials from: {0}"\
                                .format(os.path.join(homedir,'config.xml')))
                yield obj
            else:
                log(log.GENERAL, log.DEBUG,
                          "Failed to read database credentials from: {0}"\
                                .format(os.path.join(homedir,'config.xml')))

        # try UPnP
        for conn in XMLConnection.fromUPNP(5.0):
            obj = self.copy()
            if obj.readUPNP(conn):
                log(log.GENERAL, log.DEBUG,
                          "Trying database credentials from: {0}"\
                                .format(conn.host))
                yield obj
            else:
                log(log.GENERAL, log.DEBUG,
                          "Failed to read database credentials from: {0}"\
                                .format(conn.host))

        # try defaults if user did not specify anything
        if self._default:
            log(log.GENERAL, log.DEBUG, "Trying default database credentials")
            yield self

    def readUPNP(self, conn):
        self._allowwrite = True
        dat = conn.getConnectionInfo(self.pin)
        if len(dat) == 0:
            return False

        trans = {'Host':'hostname', 'Port':'port', 'UserName':'username',
                 'Password':'password', 'Name':'database'}
        for k,v in dat.items():
            if k in trans:
                setattr(self, trans[k], v)
        return True

    def readXML(self, confdir):
        filename = os.path.join(confdir, 'config.xml')
        if not os.access(filename, os.R_OK):
            return False

        try:
            config = etree.parse(filename)

            UDN = config.xpath('/Configuration/UPnP/UDN/MediaRenderer/text()')
            if len(UDN):
                self.uuid = UDN[0]

            name = config.xpath('/Configuration/LocalHostName/text()')
            if len(name):
                self.profile = name[0]

            for child in config.xpath('/Configuration/Database')[0]\
                                                            .getchildren():
                if child.tag in self._conf_trans:
                    setattr(self, self._conf_trans[child.tag], child.text)

            wol = config.xpath('/Configuration/WakeOnLan')
            if len(wol):
                self.wol.readXML(wol[0])
        except:
            return False
        return True

    def readOldXML(self, confdir):
        self._allowwrite = True
        filename = os.path.join(confdir, 'config.xml')
        if not os.access(filename, os.R_OK):
            return False

        try:
            config = etree.parse(filename)

            UDN = config.xpath('/Configuration/UPnP/UDN/MediaRenderer/text()')
            if len(UDN):
                self.uuid = UDN[0]

            trans = {'DBHostName':'hostname', 'DBUserName':'username',
                     'DBPassword':'password', 'DBName':'database',
                     'DBPort':'port'}
            for child in config.xpath('/Configuration/UPnP/MythFrontend/'+\
                                            'DefaultBackend')[0].getchildren():
                if child.tag in trans:
                    setattr(self, trans[child.tag], child.text)
        except:
            return False
        return True

    def writeXML(self, log):
        # only write a new file if imported from UPNP
        if not self._allowwrite:
            return
        self._allowwrite = False

        doc = etree.Element('Configuration')

        upnp = doc.makeelement('UPnP')
        udn = doc.makeelement('UDN')
        mr = doc.makeelement('MediaRenderer')
        mr.text = str(self.uuid)
        doc.append(upnp)
        upnp.append(udn)
        udn.append(mr)

        lhost = doc.makeelement('LocalHostName')
        if self.__class__.profile.isDefault(self):
            lhost.text = 'my-unique-identifier-goes-here'
        else:
            lhost.text = self.profile
        doc.append(lhost)

        db = doc.makeelement('Database')
        for tag,attr in self._conf_trans.items():
            value = getattr(self, attr)
            if attr == 'pinghost': value = 1 if value else 0

            ele = doc.makeelement(tag)
            ele.text = str(value)
            db.append(ele)
        doc.append(db)

        wol = doc.makeelement('WakeOnLan')
        self.wol.writeXML(wol)
        doc.append(wol)

        confdir = os.environ.get('MYTHCONFDIR', '')
        if confdir and (confdir != '/'):
            log(log.GENERAL, log.DEBUG, "Writing new database credentials: {0}"\
                                .format(os.path.join(confdir,'config.xml')))
            fp = open(os.path.join(confdir,'config.xml'), 'w')
            self.confdir = confdir
        else:
            homedir = os.environ.get('HOME', '')
            if homedir and (homedir != '/'):
                log(log.GENERAL, log.DEBUG,
                        "Writing new database credentials: {0}"\
                                .format(os.path.join(homedir,'config.xml')))
                fp = open(os.path.join(homedir, '.mythtv', 'config.xml'), 'w')
                self.confdir = os.path.join(homedir, '.mythtv')
            else:
                raise MythError("Cannot find home directory to write to")

        fp.write(etree.tostring(doc, pretty_print=True))
        fp.close()

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
                 self.dbconfig.ident, hex(id(self)))

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
            self._db = weakref.proxy(db)
            self._log = log

        def __getitem__(self,key):
            if key not in self:
                with self._db.cursor(self._log) as cursor:
                    # pull field list from database
                    try:
                        cursor.execute("DESC %s" % (key,))
                    except Exception, e:
                        raise MythDBError(MythDBError.DB_RAW, e.args)
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
                                      VALUES (?, ?, NULL)"""
                    self._where = """hostname IS NULL"""
                else:
                    self._insert = """INSERT INTO settings
                                             (value, data, hostname)
                                      VALUES (?, ?, '%s')""" % host
                    self._where = """hostname='%s'""" % host

            def __getitem__(self, key):
                if key not in self:
                    with self._db.cursor(self._log) as cursor:
                        cursor.execute("""SELECT data FROM settings
                                          WHERE value=? AND %s
                                          LIMIT 1""" % self._where, (key,))
                        res = cursor.fetchone()
                        if res is not None:
                            OrdDict.__setitem__(self, key, res[0])
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
                                              SET data=?
                                              WHERE value=?
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
                                      WHERE value=?
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
            self._db = weakref.proxy(db)
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
            self.log(MythLog.DATABASE, MythLog.DEBUG,
                            "Loading existing connection", db.dbconfig.ident)
            self.dbconfig = db.dbconfig

        else:
            # import settings from arguments, files, and UPnP
            if args is None:
                args = ()
            dbconfig = DatabaseConfig(args, **dbconn)
            for tmpconfig in dbconfig.test(self.log):
                # loop over possible sets of settings
                if tmpconfig in self.shared:
                    # settings already in use, grab and break
                    self.log(MythLog.DATABASE, MythLog.DEBUG,
                            "Loading existing connection", tmpconfig.ident)
                    self.dbconfig = tmpconfig
                    break
                elif self._testconfig(tmpconfig):
                    # settings resulted in a valid connection, break
                    break
            else:
                raise MythDBError(MythError.DB_CREDENTIALS)

        # write configuration back into file, ignored if not old format
        # or UPNP derived
        self.dbconfig.writeXML(self.log)

        # apply the rest of object init if not already done
        self._testconfig(self.dbconfig)
                    
    def _testconfig(self, dbconfig):
        self.dbconfig = dbconfig
        if dbconfig in self.shared:
            self.db = self.shared[dbconfig]
        else:
            # no exising connection, test a new one
            try:
                # attempt new connection
                self.db = DBConnection(dbconfig)
            except:
                # connection failed, report such so new settings can be tested
                return False

            # check schema version
            # do this outside the try so we can abort if told to connect
            # to an invalid database
            self._check_schema(self._schema_value, self._schema_local,
                               self._schema_name)

            # add connection to cache
            self.shared[dbconfig] = self.db

            # create special attributes
            self.db.tablefields = self._TableFields(self.db, self.db.log)
            self.db.settings = self._Settings(self.db, self.db.log)

        # connect special attributes to database
        self.tablefields = self.db.tablefields
        self.settings = self.db.settings
        return True

    def _check_schema(self, value, local, name='Database', update=None):
        if self.settings is None:
            with self.cursor(self.log) as cursor:
                if cursor.execute("""SELECT data
                                     FROM settings
                                     WHERE value LIKE(?)""",
                                (value,)) == 0:
                    raise MythDBError(MythError.DB_SETTING, value)

                sver = int(cursor.fetchone()[0])
        else:
            sver = int(self.settings['NULL'][value])

        if local != sver:
            if update is None:
                self.log(MythLog.GENERAL, MythLog.ERR,
                    "%s schema mismatch: we speak %d but database speaks %s" \
                    % (name, local, sver))
                self.db = None
                raise MythDBError(MythError.DB_SCHEMAMISMATCH, value, sver, local)
            else:
                update(self).run()

    def _gethostfromaddr(self, addr, value=None):
        if value is None:
            for value in ['BackendServerIP','BackendServerIP6']:
                try:
                    return self._gethostfromaddr(addr, value)
                except MythDBError:
                    pass
            else:
                raise MythDBError(MythError.DB_SETTING,
                                    'BackendServerIP[6]', addr)

        with self as cursor:
            if cursor.execute("""SELECT hostname FROM settings
                                  WHERE value=? AND data=?""", [value, addr]) == 0:
                raise MythDBError(MythError.DB_SETTING, value, addr)
            return cursor.fetchone()[0]

    def _getpreferredaddr(self, host):
        ip6 = self.settings[host].BackendServerIP6
        ip4 = self.settings[host].BackendServerIP
        if ip6 is None:
            if ip4 is None:
                raise MythDBError(MythError.DB_SETTING,
                                    'BackendServerIP[6]', host)
            return ip4
        elif (ip6 in ['::1', '0:0:0;0:0:0:0:1']) and (ip4 != '127.0.0.1'):
            return ip4
        return ip6

    def gethostname(self):
        return self.dbconfig.profile

    def getMasterBackend(self):
        return self._gethostfromaddr(self.settings.NULL.MasterServerIP)

    def getStorageGroup(self, groupname=None, hostname=None):
        """
        obj.getStorageGroup(groupname=None, hostname=None)
                                            -> tuple of StorageGroup objects
        groupname and hostname can be used as optional filters
        """
        where = []
        wheredat = []
        if groupname:
            where.append("groupname=?")
            wheredat.append(groupname)
        if hostname:
            where.append("hostname=?")
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

    def literal(self, query, args=None):
        """
        obj.literal(query, args=None) -> processed query
        """
        conv = {int: str,
                str: lambda x: '"%s"'%x,
                long: str,
                float: str,
                unicode: lambda x: '"%s"'%x,
                bool: str,
                type(None): lambda x: 'NULL',
                _pydt.datetime: lambda x: x.strftime('"%Y-%m-%d %H:%M:%S"'),
                _pydt.date: lambda x: x.strftime('"%Y-%m-%d"'),
                _pydt.time: lambda x: x.strftime('"%H:%M:%S"'),
                _pydt.timedelta: lambda x: '"%d:%02d:%02d"' % \
                                                (x.seconds/3600+x.days*24,
                                                 x.seconds%3600/60,
                                                 x.seconds%60),
                _pyt.struct_time: lambda x: _pyt.\
                                        strftime('"%Y-%m-%d %H:%M:%S"',x)}
        
        if args is None:
            return query

        args = list(args)
        for i, arg in enumerate(args):
            for k,v in conv.items():
                if isinstance(arg, k):
                    args[i] = v(arg)
                    break
            else:
                args[i] = str(arg)

        query = query.replace('?', '%s')
        return query % tuple(args)

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
    def __str__(self):
        return u"<StorageGroup 'myth://%s@%s%s' at %s" % \
                    (self.groupname, self.hostname,
                        self.dirname, hex(id(self)))

    def __repr__(self): return str(self).encode('utf-8')

    def _evalwheredat(self, wheredat=None):
        DBData._evalwheredat(self, wheredat)
        if (self.hostname == self._db.gethostname()) or \
              os.access(self.dirname.encode('utf-8'), os.F_OK):
            self.local = True
        else:
            self.local = False
