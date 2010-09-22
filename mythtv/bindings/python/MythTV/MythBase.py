# -*- coding: utf-8 -*-

"""
Provides base classes for accessing MythTV
"""

from MythStatic import *

import os, re, socket, sys, locale, weakref
import xml.etree.cElementTree as etree
from datetime import datetime
from time import sleep, time
from urllib import urlopen
from subprocess import Popen
from sys import version_info

import MySQLdb, MySQLdb.cursors
MySQLdb.__version__ = tuple([v for v in MySQLdb.__version__.split('.')])

class DictData( object ):
    """
    DictData.__init__(raw) -> DictData object

    Basic class for providing management of data resembling dictionaries.
    Must be subclassed for use.

    Subclasses must provide:
        field_order
            string list of field names
        field_type
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

    class DictIterator( object ):
        def __init__(self, mode, parent):
            # modes = 1:keys, 2:values, 3:items
            self.index = 0
            self.mode = mode
            self.data = parent
        def __iter__(self): return self
        def next(self):
            self.index += 1
            if self.index > len(self.data.field_order):
                raise StopIteration
            key = self.data.field_order[self.index-1]
            if self.mode == 1:
                return key
            elif self.mode == 2:
                return self.data[key]
            elif self.mode == 3:
                return (key, self.data[key])

    logmodule = 'Python DictData'
    # class emulation functions
    def __getattr__(self,name):
        # function for class attribute access of data fields
        # accesses real attributes, falls back to data fields, and errors
        if name in self.__dict__:
            return self.__dict__[name]
        elif name in self.field_order:
            return self.data[name]
        else:
            raise AttributeError("'DictData' object has no attribute '%s'"%name)

    def __setattr__(self,name,value):
        # function for class attribute access of data fields
        # sets value in data fields, falling back to real attributes
        if name in self.field_order:
            self.data[name] = value
        else:
            self.__dict__[name] = value

    # container emulation functions
    def __getitem__(self,key):
        # function for and dist-like access to content
        # as a side effect of dual operation, dict data cannot be indexed
        #       keyed off integer values
        if key in self.data:
            return self.data[key]
        else:
            raise KeyError("'DictData' object has no key '%s'" %key)

    def __setitem__(self,key,value):
        # function for dist-like access to content
        # does not allow setting of data not already in field_order
        if key in self.field_order:
            self.data[key] = value
        else:
            raise KeyError("'DictData' does not allow new data")

    def __contains__(self,item):
        return bool(item in self.data.keys())

    def __iter__(self):
        return self.DictIterator(2,self)
        
    def iterkeys(self):
        """
        obj.iterkeys() -> an iterator over the keys of obj
                                                ordered by self.field_order
        """
        return self.DictIterator(1,self)
        
    def itervalues(self):
        """
        obj.itervalues() -> an iterator over the values of obj
                                                ordered by self.field_order
        """
        return self.DictIterator(2,self)
    
    def iteritems(self):
        """
        obj.iteritems() -> an iterator of over the (key,value) pairs of obj
                                                ordered by self.field_order
        """
        return self.DictIterator(3,self)

    def keys(self):
        """obj.keys() -> list of self.field_order"""
        return list(self.iterkeys())
        
    def has_key(self,item):
        return self.__contains__(item)
        
    def values(self):
        """obj.values() -> list of values, ordered by self.field_order"""
        return list(self.itervalues())
        
    def get(self,key):
        return self.data[key]
        
    def items(self):
        """
        obj.items() -> list of (key,value) pairs, ordered by self.field_order
        """
        return list(self.iteritems())
        
    def pop(self,key):
        raise NotImplementedError
    
    def popitem(self):
        raise NotImplementedError
        
    def update(self, *args, **keywords):
        self.data.update(*args, **keywords)

    ### additional management functions
    def _setDefs(self):
        self.data = {}
        self.field = 0
        self.log = MythLog(self.logmodule)
        
    def __init__(self, raw):
        self._setDefs()
        self.data.update(self._process(raw))

    def _process(self, data):
        data = list(data)
        for i in range(0, len(data)):
            if data[i] == '':
                data[i] = None
            elif self.field_type == 'Pass':
                data[i] = data[i]
            elif self.field_type[i] == 0:
                data[i] = int(data[i])
            elif self.field_type[i] == 1:
                data[i] = locale.atof(data[i])
            elif self.field_type[i] == 2:
                data[i] = bool(data[i])
            elif self.field_type[i] == 3:
                data[i] = data[i]
            elif self.field_type[i] == 4:
                data[i] = datetime.fromtimestamp(int(data[i]))
        return dict(zip(self.field_order,data))

    @staticmethod
    def joinInt(high,low):
        """obj.joinInt(high, low) -> 64-bit int, from two signed ints"""
        return (high + (low<0))*2**32 + low

    @staticmethod
    def splitInt(integer):
        """obj.joinInt(64-bit int) -> (high, low)"""
        return integer/(2**32),integer%2**32 - (integer%2**32 > 2**31)*2**32

class DBData( DictData ):
    """
    DBData.__init__(data=None, db=None, raw=None) --> DBData object

    Altered DictData adding several functions for dealing with individual rows.
    Must be subclassed for use.

    Subclasses must provide:
        table
            Name of database table to be accessed
        where
            String defining WHERE clause for database lookup
        setwheredat
            String of comma separated variables to be evaluated to set
            'wheredat'. 'eval(setwheredat)' must return a tuple, so string must 
            contain at least one comma.

    Subclasses may provide:
        allow_empty
            Controls whether DBData() will allow an empty instance.
        schema_value, schema_local, schema_name
            Allows checking of alternate plugin schemas

    Can be populated in two manners:
        data
            Tuple used to perform a SQL query, using the subclass provided 
            'where' clause
        raw
            Raw list as returned by 'select * from mytable'
    """
    field_type = 'Pass'
    allow_empty = False
    logmodule = 'Python DBData'
    schema_value = 'DBSchemaVer'
    schema_local = SCHEMA_VERSION
    schema_name = 'Database'

    def getAllEntries(self):
        """obj.getAllEntries() -> tuple of DBData objects"""
        c = self.db.cursor(self.log)
        query = """SELECT * FROM %s""" % self.table
        self.log(MythLog.DATABASE, query)
        if c.execute(query) == 0:
            return ()
        objs = []
        for row in c.fetchall():
            objs.append(self.__class__(db=self.db, raw=row))
        c.close()
        return objs

    def _setDefs(self):
        self.__dict__['field_order'] = self.db.tablefields[self.table]
        DictData._setDefs(self)
        self._fillNone()
        self.wheredat = None
        self.log = MythLog(self.logmodule, db=self.db)

    def __init__(self, data=None, db=None, raw=None):
        self.__dict__['db'] = MythDBBase(db)
        self.db._check_schema(self.schema_value, 
                                self.schema_local, self.schema_name)
        self._setDefs()

        if raw is not None:
            if len(raw) == len(self.field_order):
                self.data.update(self._process(raw))
                self.wheredat = eval(self.setwheredat)
            else:
                raise MythError('Incorrect raw input length to DBData()')
        elif data is not None:
            if None not in data:
                self.wheredat = tuple(data)
                self._pull()
        else:
            if self.allow_empty:
                self._fillNone()
            else:
                raise MythError('DBData() not given sufficient information')

    def _pull(self):
        """Updates table with data pulled from database."""
        c = self.db.cursor(self.log)
        c.execute("""SELECT * FROM %s WHERE %s""" \
                           % (self.table, self.where), self.wheredat)
        data = c.fetchone()
        c.close()
        if data is None:
            return
        self.data.update(self._process(data))

    def _fillNone(self):
        """Fills out dictionary fields with empty data"""
        self.field_order = self.db.tablefields[self.table]
        for field in self.field_order:
            self.data[field] = None

class DBDataWrite( DBData ):
    """
    DBDataWrite.__init__(data=None, db=None, raw=None) --> DBDataWrite object

    Altered DBData, with support for writing back to the database.
    Must be subclassed for use.

    Subclasses must provide:
        table
            Name of database table to be accessed
        where
            String defining WHERE clause for database lookup
        setwheredat
            String of comma separated variables to be evaluated to set
            'wheredat'. 'eval(setwheredat)' must return a tuple, so string must 
            contain at least one comma.

    Subclasses may provide:
        defaults
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
    defaults = None
    allow_empty = True
    logmodule = 'Python DBDataWrite'

    def _sanitize(self, data, fill=True):
        """Remove fields from dictionary that are not in database table."""
        data = data.copy()
        for key in data.keys():
            if key not in self.field_order:
                del data[key]
        if self.defaults is not None:
            for key in self.defaults:
                if key in data:
                    if self.defaults[key] is None:
                        del data[key]
                    elif data[key] is None:
                        if fill:
                            data[key] = self.defaults[key]
        return data

    def _setDefs(self):
        DBData._setDefs(self)
        self._fillDefs()

    def _fillDefs(self):
        self._fillNone()
        self.data.update(self.defaults)

    def __init__(self, data=None, db=None, raw=None):
        DBData.__init__(self, data, db, raw)
        if raw is not None:
            self.origdata = self.data.copy()

    def create(self,data=None):
        """
        obj.create(data=None) -> new database row

        Creates a new database entry using given information.
        Will add any information in 'data' dictionary to local information
        before pushing the entire set onto the database.
        Will only function with an uninitialized object.
        """
        if self.wheredat is not None:
            raise MythError('DBDataWrite object already bound to '+\
                                                    'existing instance')

        if data is not None:
            data = self._sanitize(data, False)
            self.data.update(data)
        data = self._sanitize(self.data)
        for key in data.keys():
            if data[key] is None:
                del data[key]
        c = self.db.cursor(self.log)
        fields = ', '.join(data.keys())
        format_string = ', '.join(['%s' for d in data.values()])
        c.execute("""INSERT INTO %s (%s) VALUES(%s)""" \
                        % (self.table, fields, format_string), data.values())
        intid = c.lastrowid
        c.close()
        return intid

    def _pull(self):
        DBData._pull(self)
        self.origdata = self.data.copy()

    def _push(self):
        if (self.where is None) or (self.wheredat is None):
            return
        c = self.db.cursor(self.log)
        data = self._sanitize(self.data)
        for key, value in data.items():
            if value == self.origdata[key]:
            # filter unchanged data
                del data[key]
        if len(data) == 0:
            # no updates
            return
        format_string = ', '.join(['%s = %%s' % d for d in data])
        sql_values = data.values()
        sql_values.extend(self.wheredat)

        c.execute("""UPDATE %s SET %s WHERE %s""" \
                    % (self.table, format_string, self.where), sql_values)
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
        self.data.update(self._sanitize(data))
        self._push()

    def delete(self):
        """
        obj.delete() -> None

        Delete video entry from database.
        """
        if (self.where is None) or \
                (self.wheredat is None) or \
                (self.data is None):
            return
        c = self.db.cursor(self.log)
        c.execute("""DELETE FROM %s WHERE %s""" \
                        % (self.table, self.where), self.wheredat)
        c.close()

class DBDataRef( object ):
    """
    DBDataRef.__init__(where, db=None) --> DBDataRef object

    Class for managing lists of referenced data, such as recordedmarkup
    Subclasses must provide:
        table
            Name of database table to be accessed
        wfield
            list of fields for WHERE argument in lookup

    Can be populated by supplying a tuple, matching the fields specified in wfield
    """
    logmodule = 'Python DBDataRef'

    class SubData( DictData ):
        def _setDefs(self):
            self.__dict__['field_order'] = []
            DictData._setDefs(self)
        def __repr__(self):
            return str(tuple(self))
        def __init__(self,data,fields):
            self._setDefs()
            self.field_order = fields
            self.field_type = 'Pass'
            self.data.update(self._process(data))
        def __hash__(self):
            dat = self.data.copy()
            keys = dat.keys()
            keys.sort()
            return hash(str([hash(dat[k]) for k in keys]))
        def __cmp__(self, x):
            return cmp(hash(self),hash(x))

    def _setDefs(self):
        self.data = None
        self.hash = None
        self.log = MythLog(self.logmodule, db=self.db)

    def __repr__(self):
        if self.data is None:
            self._fill()
        return "<DBDataRef '%s' at %s>" % \
                        (str(tuple(self.data)), hex(id(self)))

    def __getitem__(self,index):
        if self.data is None:
            self._fill()
        if index in range(-len(self.data),0)+range(len(self.data)):
            return self.data[index]
        else:
            raise IndexError

    def __iter__(self):
        self.field = 0
        if self.data is None:
            self._fill()
        return self

    def next(self):
        if self.field == len(self.data):
            del self.field
            raise StopIteration
        self.field += 1
        return self[self.field-1]

    def __init__(self,where,db=None):
        self.db = MythDBBase(db)
        self._setDefs()
        self.where = tuple(where)
        self.dfield = list(self.db.tablefields[self.table])
        for field in self.wfield:
            del self.dfield[self.dfield.index(field)]

    def _fill(self):
        self.data = []
        self.hash = []
        fields = ','.join(self.dfield)
        where = ' AND '.join('%s=%%s' % d for d in self.wfield)
        c = self.db.cursor(self.log)
        c.execute("""SELECT %s FROM %s WHERE %s""" \
                            % (fields, self.table, where), self.where)
        for entry in c.fetchall():
            self.data.append(self.SubData(entry, self.dfield))
            self.hash.append(hash(self.data[-1]))

    def add(self, data):
        """obj.add(data) -> None"""
        if self.data is None:
            self._fill()
        if len(data) != len(self.wfield):
            return

        dat = self.SubData(data, self.dfield)
        if hash(dat) in self.hash:
            return

        c = self.db.cursor(self.log)
        fields = ', '.join(self.wfield+self.dfield)
        format = ', '.join(['%s' for d in fields])
        c.execute("""INSERT INTO %s (%s) VALUES(%s)""" \
                                % (self.table, fields, format),
                                self.where+list(data))
        c.close()

        self.data.append(dat)
        self.hash.append(hash(dat))

    def delete(self, index):
        """obj.delete(data) -> None"""
        if self.data is None:
            self._fill()
        if index not in range(0, len(self.data)):
            return

        where = ' AND '.join(['%s=%%s' % d for d in self.wfield+self.dfield])
        values = self.where+list(self.data[index])
        c = self.db.cursor(self.log)
        c.execute("""DELETE FROM %s WHERE %s""" \
                            % (self.table, where), values)
        c.close()

        del self.data[index]
        del self.hash[index]


class DBDataCRef( object ):
    """
    DBDataRef.__init__(where, db=None) --> DBDataRef object

    Class for managing lists of crossreferenced data, such as recordedcredits
    Subclasses must provide:
        table   - primary data table
        rtable  - cross reference table
        t_ref   - primary table field to align with cross reference table
        r_ref   - cross reference table field to align with primary table
        t_add   - list of fields for data stored in primary table
        r_add   - list of fields for data stored in cross reference table
        w_field - list of fields for WHERE argument in lookup
    """
    logmodule = 'Python DBDataCRef'

    class SubData( DictData ):
        def _setDefs(self):
            self.__dict__['field_order'] = []
            DictData._setDefs(self)
        def __repr__(self):
            return str(tuple(self.data.values()))
        def __init__(self,data,fields):
            self._setDefs()
            self.field_order = ['cross']+fields
            self.field_type = 'Pass'
            self.data.update(self._process(data))
        def __hash__(self):
            dat = self.data.copy()
            del dat['cross']
            keys = dat.keys()
            keys.sort()
            return hash(str([hash(dat[k]) for k in keys]))
        def __cmp__(self, x):
            return cmp(hash(self),hash(x))

    def _setDefs(self):
        self.data = None
        self.hash = None
        self.log = MythLog(self.logmodule, db=self.db)

    def __repr__(self):
        if self.data is None:
            self._fill()
        return "<DBDataCRef '%s' at %s>" % \
                        (str(tuple(self.data)), hex(id(self)))

    def __getitem__(self,index):
        if self.data is None:
            self._fill()
        if index in range(-len(self.data),0)+range(len(self.data)):
            return self.data[index]
        else:
            raise IndexError

    def __iter__(self):
        self.field = 0
        if self.data is None:
            self._fill()
        return self

    def next(self):
        if self.field == len(self.data):
            del self.field
            raise StopIteration
        self.field += 1
        return self[self.field-1]

    def __init__(self,where,db=None):
        self.db = MythDBBase(db)
        self._setDefs()
        self.w_dat = tuple(where)

    def _fill(self):
        self.data = []
        self.hash = []
        fields = ','.join([self.table+'.'+self.t_ref]+self.t_add+self.r_add)
        where = ' AND '.join('%s=%%s' % d for d in self.w_field)
        join = '%s.%s=%s.%s' % (self.table,self.t_ref,self.rtable,self.r_ref)
        c = self.db.cursor(self.log)
        c.execute("""SELECT %s FROM %s JOIN %s ON %s WHERE %s"""\
                            % (fields, self.table, self.rtable, join, where),
                            self.w_dat)
        for entry in c.fetchall():
            self.data.append(self.SubData(entry, self.t_add+self.r_add))
            self.hash.append(hash(self.data[-1]))

    def _tdata(self, sub):
        return tuple([sub[key] for key in self.t_add])

    def _rdata(self, sub):
        return tuple([sub[key] for key in self.r_add+['cross']]+list(self.w_dat))

    def _search(self, sub):
        for i in range(len(self.data)):
            if self.data[i] == sub:
                return i

    def add(self,data):
        """obj.add(data) -> None"""
        if self.data is None:
            self._fill()
        if len(data) != len(self.t_add+self.r_add):
            return

        # check for existing
        dat = self.SubData([0]+list(data),self.t_add+self.r_add)
        if dat in self.data:
            return

        tdata = self._tdata(dat)

        c = self.db.cursor(self.log)
        # search for existing table data, add if needed
        where = ' AND '.join('%s=%%s' % d for d in self.t_add)
        if c.execute("""SELECT %s FROM %s WHERE %s""" \
                                    % (self.t_ref, self.table, where),
                                    self._tdata(dat)) > 0:
            id = c.fetchone()[0]
        else:
            fields = ', '.join(self.t_add)
            format = ', '.join(['%s' for d in self.t_add])
            c.execute("""INSERT INTO %s (%s) VALUES(%s)""" \
                        % (self.table, fields, format), self._tdata(dat))
            id = c.lastrowid

        dat = self.SubData([id]+list(data),self.t_add+self.r_add)

        # double check for existing
        fields = self.r_add+[self.r_ref]+self.w_field
        where = ' AND '.join('%s=%%s' % d for d in fields)
        if c.execute("""SELECT * FROM %s WHERE %s""" % (self.rtable, where),
                                                    self._rdata(dat)) > 0:
            return

        # add cross reference
        fields = ', '.join(self.r_add+[self.r_ref]+self.w_field)
        format = ', '.join(['%s' for d in fields.split(', ')])
        c.execute("""INSERT INTO %s (%s) VALUES(%s)"""\
                            % (self.rtable, fields, format), self._rdata(dat))
        c.close()

        # add local entry
        self.data.append(dat)

    def delete(self,data):
        """obj.delete(data) -> None"""
        if self.data is None:
            self._fill()
        # check for local entry
        dat = self.SubData([0]+list(data),self.t_add+self.r_add)
        if dat not in self.data:
            return
        # remove local entry
        index = self._search(dat)
        dat = self.data.pop(index)
        
        # remove database cross reference
        fields = self.r_add+[self.r_ref]+self.w_field
        where = ' AND '.join('%s=%%s' % d for d in fields)
        c = self.db.cursor(self.log)
        c.execute("""DELETE FROM %s WHERE %s""" % (self.rtable, where),
                                                    self._rdata(dat))

        # remove primary entry if unused
        if c.execute("""SELECT %s FROM %s WHERE %s=%%s""" \
                                % (self.r_ref, self.rtable, self.r_ref),
                                dat.cross) == 0:
            c.execute("""DELETE FROM %s WHERE %s=%%s""" \
                                % (self.table, self.t_ref), dat.cross)
        c.close()

        index = self.hash.index(dat)
        del self.hash[index]
        del self.data[index]

class MythDBCursor( MySQLdb.cursors.Cursor ):
    """
    Custom cursor, offering logging and error handling
    """
    def __init__(self, connection):
        self.log = None
        MySQLdb.cursors.Cursor.__init__(self, connection)

    def execute(self, query, args=None):
        if MySQLdb.__version__ >= ('1','2','2'):
            self.connection.ping(True)
        else:
            self.connection.ping()
        if self.log == None:
            self.log = MythLog('Python Database Connection')
        if args:
            self.log(self.log.DATABASE, ' '.join(query.split()), str(args))
        else:
            self.log(self.log.DATABASE, ' '.join(query.split()))
        try:
            return MySQLdb.cursors.Cursor.execute(self, query, args)
        except Exception, e:
            raise MythDBError(MythDBError.DB_RAW, e.args)

    def executemany(self, query, args):
        if MySQLdb.__version__ >= ('1','2','2'):
            self.connection.ping(True)
        else:
            self.connection.ping()
        if self.log == None:
            self.log = MythLog('Python Database Connection')
        for arg in args:
            self.log(self.log.DATABASE, ' '.join(query.split()), str(arg))
        try:
            return MySQLdb.cursors.Cursor.executemany(self, query, args)
        except Exception, e:
            raise MythDBError(MythDBError.DB_RAW, e.args)

class MythDBConn( object ):
    """
    This is the basic database connection object.
    You dont want to use this directly.
    """

    def __init__(self, dbconn):
        self.log = MythLog('Python Database Connection')
        self.tablefields = {}
        self.settings = {}
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

    def cursor(self, log=None, type=MythDBCursor):
        if MySQLdb.__version__ >= ('1','2','2'):
            self.db.ping(True)
        else:
            self.db.ping()
        c = self.db.cursor(type)
        if log:
            c.log = log
        else:
            c.log = self.log
        return c

class MythDBBase( object ):
    """
    MythDBBase(db=None, args=None, **kwargs) -> database connection object

    Basic connection to the mythtv database
    Offers connection caching to prevent multiple connections

    'db' will accept an existing MythDBBase object, or any subclass there of
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
    cursorclass = MythDBCursor
    shared = weakref.WeakValueDictionary()

    def __repr__(self):
        return "<%s '%s' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1], 
                 self.ident, hex(id(self)))

    class _TableFields(object):
        """Provides a dictionary-like list of table fieldnames"""
        def __init__(self, db, log):
            self._db = db
            self._log = log
        def __getattr__(self,key):
            if key in self.__dict__:
                return self.__dict__[key]
            elif key in self._db.tablefields:
                return self._db.tablefields[key]
            else:
                return self.__getitem__(key)
        def __getitem__(self,key):
            if key in self._db.tablefields:
                return self._db.tablefields[key]
            table_names = []
            c = self._db.cursor(self._log)
            try:
                c.execute("DESC %s" % (key,))
            except:
                return None
            
            for name in c.fetchall():
                table_names.append(name[0])
            c.close()
            self._db.tablefields[key] = tuple(table_names)
            return table_names

    class _Settings(object):
        """Provides dictionary-like list of hosts"""
        class __Settings(object):
            """Provides dictionary-like list of settings"""
            def __repr__(self):
                return str(self._shared.keys())
            def __init__(self, db, log, host):
                self._db = db
                self._log = log
                self._host = host
            def __getattr__(self,name):
                if name in self.__dict__:
                    return self.__dict__[name]
                else:
                    return self.__getitem__(name)
            def __setattr__(self,name,value):
                if name in ('_db','_host','_log'):
                    self.__dict__[name] = value
                else:
                    self.__setitem__(name,value)
            def __getitem__(self,key):
                if key in self._db.settings[self._host]:
                    return self._db.settings[self._host][key]
                if self._host == 'NULL':
                    where = 'IS NULL'
                    wheredat = (key,)
                else:
                    where = 'LIKE(%s)'
                    wheredat = (key, self._host)
                c = self._db.cursor(self._log)
                c.execute("""SELECT data FROM settings
                             WHERE value=%%s AND hostname %s
                             LIMIT 1""" % where, wheredat)
                row = c.fetchone()
                c.close()
                if row:
                    self._db.settings[self._host][key] = row[0]
                    return row[0]
                else:
                    return None
            def __setitem__(self, key, value):
                if key not in self._db.settings[self._host]:
                    self.__getitem__(key)
                c = self._db.cursor(self._log)
                if key not in self._db.settings[self._host]:
                    host = self._host
                    if host is 'NULL':
                        host = None
                    c.execute("""INSERT INTO settings (value,data,hostname)
                             VALUES (%s,%s,%s)""", (key, value, host))
                else:
                    query = """UPDATE settings SET data=%s
                                WHERE value=%s AND"""
                    dat = [value, key]
                    if self._host == 'NULL':
                        query += ' hostname IS NULL'
                    else:
                        query += ' hostname=%s'
                        dat.append(self._host)
                    c.execute(query, dat)
                self._db.settings[self._host][key] = value
        def __repr__(self):
            return str(self._db.settings.keys())
        def __init__(self, db, log):
            self._db = db
            self._log = log
        def __getattr__(self,key):
            if key in self.__dict__:
                return self.__dict__[key]
            else:
                return self.__getitem__(key)
        def __getitem__(self,key):
            if key not in self._db.settings:
                self._db.settings[key] = {}
            return self.__Settings(self._db, self._log, key)

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
            config_files = [ os.path.expanduser('~/.mythtv/config.xml') ]
            if 'MYTHCONFDIR' in os.environ:
                config_files.append('%s/config.xml' %os.environ['MYTHCONFDIR'])

            for config_file in config_files:
                dbconn.update({ 'DBHostName':None,  'DBName':None,
                                'DBUserName':None,  'DBPassword':None,
                                'DBPort':0,         'LocalHostName':None})
                try:
                    config = etree.parse(config_file).getroot()
                    for child in config.find('UPnP').find('MythFrontend').\
                                        find('DefaultBackend').getchildren():
                        if child.tag in dbconn:
                            dbconn[child.tag] = child.text
                except:
                    continue

                if self._check_dbconn(dbconn):
                    self.log(MythLog.IMPORTANT|MythLog.DATABASE,
                           "Using connection settings from %s" % config_file)
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
                mythdir = os.path.expanduser('~/.mythtv')
                if not os.access(mythdir, os.F_OK):
                    os.mkdir(mythdir,0755)
                fp = open(mythdir+'/config.xml', 'w')
                fp.write(config)
                fp.close()
                
        if 'DBPort' in dbconn:
            if dbconn['DBPort'] == '0':
                dbconn['DBPort'] = 3306
            else:
                dbconn['DBPort'] = int(dbconn['DBPort'])
        else:
            dbconn['DBPort'] = 3306

        if 'LocalHostName' in dbconn:
            if dbconn['LocalHostName'] is None:
                dbconn['LocalHostName'] = socket.gethostname()
        else:
            dbconn['LocalHostName'] = socket.gethostname()

        self.dbconn = dbconn
        self.ident = "sql://%s@%s:%d/" % \
                (dbconn['DBName'],dbconn['DBHostName'],dbconn['DBPort'])
        if self.ident in self.shared:
            # reuse existing database connection and update count
            self.db = self.shared[self.ident]
        else:
            # attempt new connection
            self.db = MythDBConn(dbconn)

            # check schema version
            self._check_schema('DBSchemaVer',SCHEMA_VERSION)

            # add connection to cache
            self.shared[self.ident] = self.db

        # connect to table name cache
        self.tablefields = self._TableFields(self.shared[self.ident], self.log)
        self.settings = self._Settings(self.shared[self.ident], self.log)

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
            xml = MythXMLConn(backend=ip, port=port)
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

    def gethostname(self):
        return self.dbconn['LocalHostName']

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

class MythBEConn( object ):
    """
    This is the basic backend connection object.
    You probably dont want to use this directly.
    """
    logmodule = 'Python Backend Connection'

    def __init__(self, backend, type, db=None):
        """
        MythBEConn(backend, type, db=None) -> backend socket connection

        'backend' can be either a hostname or IP address, or will default
            to the master backend if None.
        'type' is any value, as required by obj.announce(type). The stock
            method passes 'Monitor' or 'Playback' during connection to 
            backend. There is no checking on this input.
        'db' will accept an existing MythDBBase object,
            or any subclass there of.
        """
        self.connected = False
        self.db = MythDBBase(db)
        self.log = MythLog(self.logmodule, db=self.db)
        if backend is None:
            # use master backend, no sanity checks, these should always be set
            self.host = self.db.settings.NULL.MasterServerIP
            self.port = self.db.settings.NULL.MasterServerPort
        else:
            if re.match('(?:\d{1,3}\.){3}\d{1,3}',backend):
                # process ip address
                c = self.db.cursor(self.log)
                if c.execute("""SELECT hostname FROM settings
                                        WHERE value='BackendServerIP'
                                        AND data=%s""", backend) == 0:
                    c.close()
                    raise MythError(MythError.DB_SETTING,
                                        'BackendServerIP', backend)
                backend = c.fetchone()[0]
                c.close()
            self.host = self.db.settings[backend].BackendServerIP
            if not self.host:
                raise MythDBError(MythError.DB_SETTING,
                                        'BackendServerIP', backend)
            self.port = self.db.settings[backend].BackendServerPort
            if not self.port:
                raise MythDBError(MythError.DB_SETTING,
                                        'BackendServerPort', backend)
        self.port = int(self.port)

        try:
            self.log(MythLog.SOCKET|MythLog.NETWORK,
                    "Connecting to backend %s:%d" % (self.host, self.port))
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            self.socket.connect((self.host, self.port))
            self.check_version()
            self.announce(type)
            self.connected = True
        except socket.error, e:
            self.log(MythLog.IMPORTANT|MythLog.SOCKET,
                    "Couldn't connect to backend %s:%d" \
                    % (self.host, self.port))
            raise MythBEError(MythError.PROTO_CONNECTION, self.host, self.port)
        except:
            raise

    def announce(self,type):
        res = self.backendCommand('ANN %s %s 0' % (type, self.db.gethostname()))
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

    def backendCommand(self, data):
        """
        obj.backendCommand(data) -> response string

        Sends a formatted command via a socket to the mythbackend.
        """
        def recv():
            """
            Reads the data returned from the backend.
            """
            # The first 8 bytes of the response gives us the length
            data = self.socket.recv(8)
            try:
                length = int(data)
            except:
                return u''
            data = []
            while length > 0:
                chunk = self.socket.recv(length)
                length = length - len(chunk)
                data.append(chunk)
            try:
                return unicode(''.join(data),'utf8')
            except:
                return u''.join(data)

        data = data.encode('utf-8')
        command = '%-8d%s' % (len(data), data)
        self.log(MythLog.NETWORK, "Sending command", command)
        self.socket.send(command)
        return recv()

    def __del__(self):
        if self.connected:
            self.log(MythLog.SOCKET|MythLog.NETWORK,
                    "Terminating connection to %s:%d" % (self.host, self.port))
            self.backendCommand('DONE')
            self.socket.shutdown(1)
            self.socket.close()
            self.connected = False

class MythBEBase( object ):
    """
    MythBEBase(backend=None, type='Monitor', db=None)
                                            -> MythBackend connection object

    Basic class for mythbackend socket connections.
    Offers connection caching to prevent multiple connections.

    'backend' allows a hostname or IP address to connect to. If not provided,
                connect will be made to the master backend. Port is always
                taken from the database.
    'db' allows an existing database object to be supplied.
    'type' specifies the type of connection to declare to the backend. Accepts
                'Monitor' or 'Playback'.

    Available methods:
        joinInt()           - convert two signed ints to one 64-bit
                              signed int
        splitInt()          - convert one 64-bit signed int to two
                              signed ints
        backendCommand()    - Sends a formatted command to the backend
                              and returns the response.
    """
    logmodule = 'Python Backend Connection'
    shared = weakref.WeakValueDictionary()

    def __repr__(self):
        return "<%s 'myth://%s:%d/' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1], 
                 self.hostname, self.port, hex(id(self)))

    def __init__(self, backend=None, type='Monitor', db=None):
        self.db = MythDBBase(db)
        self.log = MythLog(self.logmodule, db=self.db)
        self._ident = '%s@%s' % (type, backend)
        if self._ident in self.shared:
            self.be = self.shared[self._ident]
        else:
            self.be = MythBEConn(backend, type, db)
            self.shared[self._ident] = self.be
        self.hostname = self.be.hostname
        self.port = self.be.port

    def backendCommand(self, data):
        """
        obj.backendCommand(data) -> response string

        Sends a formatted command via a socket to the mythbackend.
        """
        return self.be.backendCommand(data)

    def joinInt(self,high,low):
        """obj.joinInt(high, low) -> 64-bit int, from two signed ints"""
        return (high + (low<0))*2**32 + low

    def splitInt(self,integer):
        """obj.joinInt(64-bit int) -> (high, low)"""
        return integer/(2**32),integer%2**32 - (integer%2**32 > 2**31)*2**32


class MythXMLConn( object ):
    """
    MythXMLConn(backend=None, db=None, port=None) -> Backend status object

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
        self.db = MythDBBase(db)
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
    

class MythLog( object ):
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

    ALL         = int('1111111111111111111111111111', 2)
    MOST        = int('0011111111101111111111111111', 2)
    NONE        = int('0000000000000000000000000000', 2)

    IMPORTANT   = int('0000000000000000000000000001', 2)
    GENERAL     = int('0000000000000000000000000010', 2)
    RECORD      = int('0000000000000000000000000100', 2)
    PLAYBACK    = int('0000000000000000000000001000', 2)
    CHANNEL     = int('0000000000000000000000010000', 2)
    OSD         = int('0000000000000000000000100000', 2)
    FILE        = int('0000000000000000000001000000', 2)
    SCHEDULE    = int('0000000000000000000010000000', 2)
    NETWORK     = int('0000000000000000000100000000', 2)
    COMMFLAG    = int('0000000000000000001000000000', 2)
    AUDIO       = int('0000000000000000010000000000', 2)
    LIBAV       = int('0000000000000000100000000000', 2)
    JOBQUEUE    = int('0000000000000001000000000000', 2)
    SIPARSER    = int('0000000000000010000000000000', 2)
    EIT         = int('0000000000000100000000000000', 2)
    VBI         = int('0000000000001000000000000000', 2)
    DATABASE    = int('0000000000010000000000000000', 2)
    DSMCC       = int('0000000000100000000000000000', 2)
    MHEG        = int('0000000001000000000000000000', 2)
    UPNP        = int('0000000010000000000000000000', 2)
    SOCKET      = int('0000000100000000000000000000', 2)
    XMLTV       = int('0000001000000000000000000000', 2)
    DVBCAM      = int('0000010000000000000000000000', 2)
    MEDIA       = int('0000100000000000000000000000', 2)
    IDLE        = int('0001000000000000000000000000', 2)
    CHANNELSCAN = int('0010000000000000000000000000', 2)
    EXTRA       = int('0100000000000000000000000000', 2)
    TIMESTAMP   = int('1000000000000000000000000000', 2)

    DBALL       = 8
    DBDEBUG     = 7
    DBINFO      = 6
    DBNOTICE    = 5
    DBWARNING   = 4
    DBERROR     = 3
    DBCRITICAL  = 2
    DBALERT     = 1
    DBEMERGENCY = 0

    LOGLEVEL = IMPORTANT|GENERAL
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
                 self.module, bin(self.LOGLEVEL), hex(id(self)))

    def __init__(self, module='pythonbindings', lstr=None, lbit=None, \
                    db=None, logfile=None):
        self._setlevel(lstr, lbit)
        self.module = module
        self.db = db
        if self.db:
            self.db = MythDBBase(self.db)
        if (logfile is not None) and (self.LOGFILE is None):
            self.LOGFILE = open(logfile,'w')

    def _setlevel(lstr=None, lbit=None):
        if lstr:
            MythLog.LOGLEVEL = MythLog._parselevel(lstr)
        elif lbit:
            MythLog.LOGLEVEL = lbit
    _setlevel = staticmethod(_setlevel)

    def _parselevel(lstr):
        level = MythLog.NONE
        bwlist = (  'important','general','record','playback','channel','osd',
                    'file','schedule','network','commflag','audio','libav',
                    'jobqueue','siparser','eit','vbi','database','dsmcc',
                    'mheg','upnp','socket','xmltv','dvbcam','media','idle',
                    'channelscan','extra','timestamp')
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
    _parselevel = staticmethod(_parselevel)

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
        if level&self.LOGLEVEL:
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
                self.db = MythDBBase()
            c = self.db.cursor(self.log)
            c.execute("""INSERT INTO mythlog (module,   priority,   logdate,
                                              host,     message,    details)
                                    VALUES   (%s, %s, %s, %s, %s, %s)""",
                                    (self.module,   dblevel,    now(),
                                     self.host,     message,    detail))
            c.close()

    def __call__(self, level, message, detail=None, dblevel=None):
        self.log(level, message, detail, dblevel)

class MythError( Exception ):
    """
    MythError('Generic Error Code') -> Exception
    MythError(error_code, additional_arguments) -> Exception

    Objects will have an error string available at obj.args as well as an 
        error code at obj.ecode.  Additional attributes may be available
        depending on the error code.
    """
    GENERIC             = 0
    SYSTEM              = 1
    DB_RAW              = 50
    DB_CONNECTION       = 51
    DB_CREDENTIALS      = 52
    DB_SETTING          = 53
    DB_SCHEMAMISMATCH   = 54
    PROTO_CONNECTION    = 100
    PROTO_ANNOUNCE      = 101
    PROTO_MISMATCH      = 102
    FE_CONNECTION       = 150
    FE_ANNOUNCE         = 151
    FILE_ERROR          = 200
    FILE_FAILED_READ    = 201
    FILE_FAILED_WRITE   = 202
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
    table = 'storagegroup'
    where = 'id=%s'
    setwheredat = 'self.id,'
    logmodule = 'Python StorageGroup'
    
    def __str__(self):
        return u"<StorageGroup 'myth://%s@%s%s' at %s" % \
                    (self.groupname, self.hostname,
                        self.dirname, hex(id(self)))

    def __repr__(self): return str(self).encode('utf-8')

    def __init__(self, id=None, db=None, raw=None):
        DBData.__init__(self, (id,), db, raw)
        if self.wheredat is None:
            return
        if (self.hostname == self.db.gethostname()) or \
              os.access(self.dirname.encode('utf-8'), os.F_OK):
            self.local = True
        else:
            self.local = False

class Grabber( MythDBBase ):
    """
    Grabber(path=None, setting=None, db=None) -> Grabber object

    'path' sets the object to use a path to an external command
    'setting' pulls the external command from a setting in the database
    """
    logmodule = 'Python Metadata Grabber'

    def __init__(self, path=None, setting=None, db=None):
        MythDBBase.__init__(self, db=db)
        self.log = MythLog(self.logmodule, db=self)
        self.path = ''
        if path is not None:
            self.path = path
        elif setting is not None:
            host = self.gethostname()
            self.path = self.settings[host][setting]
            if self.path is None:
                raise MythDBError(MythError.DB_SETTING, setting, host)
        else:
            raise MythError('Invalid input to Grabber()')
        self.returncode = 0
        self.stderr = ''

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
        fd = Popen('%s %s' % (self.path, arg), stdout=-1, stderr=-1,
                        shell=True)
        self.returncode = fd.wait()
        stdout,self.stderr = fd.communicate()

        if self.returncode:
            raise MythError(MythError.SYSTEM,self.returncode,arg,self.stderr)
        return stdout

    
