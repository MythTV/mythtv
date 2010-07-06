# -*- coding: utf-8 -*-
"""Provides basic connection classes."""

from __future__ import with_statement

from static import SCHEMA_VERSION, PROTO_VERSION, BACKEND_SEP
from msearch import MSearch
from logging import MythLog
from exceptions import *
from altdict import OrdDict
from utility import deadlinesocket

from time import sleep, time
from select import select
from urllib import urlopen
from thread import start_new_thread, allocate_lock, get_ident
import MySQLdb, MySQLdb.cursors
import lxml.etree as etree
import weakref
import socket
import re

class LoggedCursor( MySQLdb.cursors.Cursor ):
    """
    Custom cursor, offering logging and error handling
    """
    def __init__(self, connection):
        self.log = None
        self._releaseCallback = None
        self.id = id(connection)
        MySQLdb.cursors.Cursor.__init__(self, connection)
        self.ping = self._ping121
        if MySQLdb.version_info >= ('1','2','2'):
            self.ping = self._ping122
        self.ping()

    def __del__(self):
        self._release()
        MySQLdb.cursors.Cursor.__del__(self)

    def close(self):
        self._release()
        MySQLdb.cursors.Cursor.close(self)

    def _release(self):
        if self._releaseCallback is not None:
            self._releaseCallback(self.id, self.connection)

    def _ping121(self): self.connection.ping(True)
    def _ping122(self): self.connection.ping()

    def log_query(self, query, args):
        self.log(self.log.DATABASE, ' '.join(query.split()), str(args))

    def execute(self, query, args=None):
        """
        Execute a query.

        query -- string, query to execute on server
        args -- optional sequence or mapping, parameters to use with query.

        Note: If args is a sequence, then %s must be used as the
        parameter placeholder in the query. If a mapping is used,
        %(key)s must be used as the placeholder.

        Returns long integer rows affected, if any
        """
        self.ping()
        self.log_query(query, args)
        try:
            if args is None:
                return MySQLdb.cursors.Cursor.execute(self, query)
            return MySQLdb.cursors.Cursor.execute(self, query, args)
        except Exception, e:
            raise MythDBError(MythDBError.DB_RAW, e.args)

    def executemany(self, query, args):
        """
        Execute a multi-row query.

        query -- string, query to execute on server

        args

            Sequence of sequences or mappings, parameters to use with
            query.

        Returns long integer rows affected, if any.

        This method improves performance on multiple-row INSERT and
        REPLACE. Otherwise it is equivalent to looping over args with
        execute().
        """
        self.ping()
        self.log_query(query, args)
        try:
            return MySQLdb.cursors.Cursor.executemany(self, query, args)
        except Exception, e:
            raise MythDBError(MythDBError.DB_RAW, e.args)

    def __enter__(self): return self
    def __exit__(self, type, value, traceback): self.close()

class DBConnection( object ):
    """
    This is the basic database connection object.
    You dont want to use this directly.
    """

    _defpoolsize = 2
    @classmethod
    def setDefaultSize(cls, size):
        """
        Set the default connection pool size for new database connections.
        """
        cls._defpoolsize = size

    def resizePool(self, size):
        """Resize the connection pool."""
        if size < 1:
            size = 1
        diff = size - self._poolsize
        self._poolsize = size

        while diff:
            if diff > 0:
                self._pool.append(self.connect())
                diff -= 1
            else:
                try:
                    conn = self._pool.pop()
                    conn.close()
                except IndexError:
                    key,conn = self._inuse.popitem()
                    conn.close()
                diff += 1

    def __init__(self, dbconn):
        self.log = MythLog('Python Database Connection')
        self.tablefields = None
        self.settings = None
        self._pool = []
        self._inuse = {}
        self._stack = {}
        self._poolsize = self._defpoolsize
        self.dbconn = dbconn

        try:
            self.log(MythLog.DATABASE, "Attempting connection", 
                                                str(dbconn))
            for i in range(self._poolsize):
                self._pool.append(self.connect())
        except:
            raise MythDBError(MythError.DB_CONNECTION, dbconn)

    def __del__(self):
        for conn in self._pool:
            conn.close()
        for id,conn in self._inuse.items():
            conn.close()

    def connect(self):
        self.log(MythLog.DATABASE, 'Spawning new database connection')
        dbconn = self.dbconn
        db = MySQLdb.connect(  user=   dbconn['DBUserName'],
                               host=   dbconn['DBHostName'],
                               passwd= dbconn['DBPassword'],
                               db=     dbconn['DBName'],
                               port=   dbconn['DBPort'],
                               use_unicode=True,
                               charset='utf8')
        db.autocommit(True)
        return db

    def acquire(self):
        """
        Acquire one connection from the pool,
            or open a new one if none are available.
        """
        try:
            conn = self._pool.pop(0)
            self._inuse[id(conn)] = conn
            self.log(MythLog.DATABASE, 'Acquiring database connection from pool')
        except IndexError:
            conn = self.connect()
        return conn

    def release(self, id, conn):
        """
        Release a connection back to the pool to allow reuse.
        """
        try:
            conn = self._inuse.pop(id)
            self._pool.append(conn)
            self.log(MythLog.DATABASE, 'Releasing database connection to pool')
        except KeyError:
            conn.close()
            self.log(MythLog.DATABASE, 'Closing spare database connection')

    def cursor(self, log=None, type=LoggedCursor):
        """
        Create a cursor on which queries may be performed. The
        optional cursorclass parameter is used to create the
        Cursor. By default, self.cursorclass=connections.LoggedCursor
        is used.
        """
        if log is None:
            log = self.log
        cursor = self.acquire().cursor(type)
        cursor._releaseCallback = self.release
        cursor.log = log
        return cursor

    def __enter__(self):
        cursor = self.cursor()
        ident = get_ident()
        if ident not in self._stack:
            self._stack[ident] = []
        self._stack[ident].append(cursor)
        return cursor

    def __exit__(self, type, value, traceback):
        ident = get_ident()
        if ident not in self._stack:
            raise MythError('Missing context stack in DBConnection')
        cursor = self._stack[ident].pop()
        cursor.close()

class BEConnection( object ):
    """
    This is the basic backend connection object.
    You probably dont want to use this directly.
    """
    logmodule = 'Python Backend Connection'

    class BEConnOpts( OrdDict ):
        def __init__(self, noshutdown=False, systemevents=False,
                           generalevents=False):
            OrdDict.__init__(self, (('noshutdown',noshutdown),
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

    def __init__(self, backend, port, opts=None, deadline=10.0):
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

        self.host = backend
        self.port = port
        self.hostname = None
        self.localname = socket.gethostname()
        self.deadline = deadline

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
            raise
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
        self.socket = deadlinesocket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.log = self.log
        self.socket.connect((self.host, self.port))
        self.socket.setdeadline(self.deadline)
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

    def backendCommand(self, data=None, deadline=None):
        """
        obj.backendCommand(data=None, timeout=None) -> response string

        Sends a formatted command via a socket to the mythbackend. 'timeout'
            will override the default timeout given when the object was 
            created. If 'data' is None, the method will return any events
            in the receive buffer.
        """

        # return if not connected
        if not self.connected:
            return u''

        # pull default timeout
        t = time()
        if deadline is None:
            deadline = self.socket.getdeadline()
        deadline = t+deadline

        # send command string
        if data is not None:
            self.socket.sendheader(data.encode('utf-8'))

        # lock socket access
        with self._socklock:
            # loop waiting for proper response
            while True:
                # wait timeout for data to be received on the socket
                if len(select([self.socket],[],[], deadline-t)[0]) == 0:
                    # no data, return
                    return u''
                event = self.socket.recvheader(deadline=deadline)

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

                if t >= deadline:
                    break
                t = time()

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
            self.backendCommand(deadline=0.001)
            sleep(0.1)
        self.threadrunning = False

class FEConnection( object ):
    """
    This is the basic frontend connection object.
    You probably dont want to use this directly.
    """
    def __init__(self, host, port, deadline=10.0, test=True):
        self.isConnected = False
        self.host = host
        self.port = int(port)
        self.log = MythLog('Python Frontend Connection')
        self.deadline = deadline
        self.connect(test)

    @classmethod
    def fromUPNP(cls, timeout=5):
        reLOC = re.compile('http://(?P<ip>[0-9\.]+):(?P<port>[0-9]+)/.*')
        msearch = MSearch()
        for res in msearch.searchMythFE(timeout):
            ip, port = reLOC.match(res['location']).group(1,2)
            port = 6546
            yield cls(ip, port)

    @classmethod
    def testList(cls, felist):
        felist = [cls(addr, port, False) for addr,port in felist]
        for fe in felist:
            try:
                fe._test(2.0)
            except MythError, e:
                pass
            yield fe

    def __del__(self):
        if self.isConnected:
            self.disconnect()

    def __repr__(self):
        return "<Frontend '%s@%d' at %s>" % (self.host,self.port,hex(id(self)))

    def __str__(self):
        return "%s@%d" % (self.host, self.port)

    def close(self): self.__del__()

    def connect(self, test=True):
        if self.isConnected:
            return True
        self.socket = deadlinesocket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setdeadline(self.deadline)
        self.socket.log = self.log
        try:
            self.socket.connect((self.host, self.port))
        except:
            raise MythFEError(MythError.FE_CONNECTION, self.host, self.port)
        self.isConnected = True
        if test: self._test()

    def disconnect(self):
        if not self.isConnected:
            return
        self.send("exit")
        self.socket.close()
        self.socket = None
        self.isConnected = False

    def _test(self, timeout=None):
        if re.search("MythFrontend Network Control.*",self.recv(timeout)) is None:
            self.socket.close()
            self.socket = None
            self.isConnected = False
            raise MythFEError(MythError.FE_ANNOUNCE, self.host, self.port)

    def send(self,command):
        if not self.isConnected:
            self.connect()
        self.socket.send("%s\n" % command)

    def recv(self, deadline=None):
        prompt = re.compile('([\r\n.]*)\r\n# ')
        try:
            res = self.socket.dlexpect(prompt, deadline=deadline)
        except socket.error:
            raise
            raise MythFEError(MythError.FE_CONNECTION, self.host, self.port)
        except KeyboardInterrupt:
            raise
        return prompt.split(res)[0]

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

    def __init__(self, host, port):
        self.host, self.port = host, int(port)
        self.log = MythLog('Python XML Connection')

    @classmethod
    def fromUPNP(cls, timeout=5.0):
        reLOC = re.compile('http://(?P<ip>[0-9\.]+):(?P<port>[0-9]+)/.*')
        msearch = MSearch()
        for res in msearch.searchMythBE(timeout):
            ip, port = reLOC.match(res['location']).group(1,2)
            yield cls(ip, port)

    def _queryObject(self, path=None, **keyvars):
        """
        obj._query(path=None, **keyvars) -> file object

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
        self.log(self.log.NETWORK, 'Executing query', url)
        ufd = urlopen(url)
        return ufd

    def _query(self, path=None, **keyvars):
        """
        obj._query(path=None, **keyvars) -> xml string

        'path' is an optional page to access.
        'keyvars' are a series of optional variables to specify on the URL.
        """
        ufd = self._queryObject(path, **keyvars)
        res = ufd.read()
        ufd.close()
        return res

    def _queryTree(self, path=None, **keyvars):
        """
        obj._queryTree(path=None, **keyvars) -> xml element tree

        'path' is an optional page to access.
        'keyvars' are a series of optional variables to specify on the URL.
        """
        return etree.fromstring(self._query(path, **keyvars))

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
