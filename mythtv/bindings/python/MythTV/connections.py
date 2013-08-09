# -*- coding: utf-8 -*-
"""Provides basic connection classes."""

from MythTV.static import SCHEMA_VERSION, PROTO_VERSION, PROTO_TOKEN, BACKEND_SEP
from MythTV.msearch import MSearch
from MythTV.logging import MythLog
from MythTV.exceptions import *
from MythTV.altdict import OrdDict
from MythTV.utility import deadlinesocket

from time import sleep, time
from select import select
from thread import start_new_thread, allocate_lock, get_ident
import lxml.etree as etree
import weakref
import urllib2
import socket
import Queue
import json
import re

try:
    import _conn_oursql as dbmodule
    from   _conn_oursql import LoggedCursor
except:
    try:
        import _conn_mysqldb as dbmodule
        from   _conn_mysqldb import LoggedCursor
    except:
        raise MythError("No viable database module found.")

class _Connection_Pool( object ):
    """
    Provides a scaling connection pool to access a shared resource.
    """

    _defpoolsize = 2
    _logmode = MythLog.SOCKET
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
                self._pool.append(self._connect())
                diff -= 1
            else:
                try:
                    conn = self._pool.pop()
                    conn.close()
                except IndexError:
                    key,conn = self._inuse.popitem()
                    conn.close()
                diff += 1

    def __init__(self):
        self._pool = []
        self._inuse = {}
        self._refs  = {}
        self._stack = {}
        self._poolsize = self._defpoolsize

        for i in range(self._poolsize):
            self._pool.append(self._connect())

    def __del__(self):
        for conn in self._pool:
            conn.close()
        for id,conn in self._inuse.items():
            conn.close()

    def acquire(self):
        """
        Acquire one connection from the pool,
            or open a new one if none are available.
        """
        try:
            conn = self._pool.pop(0)
            self._inuse[id(conn)] = conn
            self.log(self._logmode, MythLog.DEBUG,
                        'Acquiring connection from pool')
        except IndexError:
            conn = self._connect()
        return conn

    def release(self, id):
        """
        Release a connection back to the pool to allow reuse.
        """
        conn = self._inuse.pop(id)
        self._pool.append(conn)
        self.log(self._logmode, MythLog.DEBUG,
                    'Releasing connection to pool')

class DBConnection( _Connection_Pool ):
    """
    This is the basic database connection object.
    You dont want to use this directly.
    """

    _logmode = MythLog.DATABASE

    def _connect(self):
        return dbmodule.dbconnect(self.dbconn, self.log)

    def __init__(self, dbconn):
        self.log = MythLog('Python Database Connection')
        self.tablefields = None
        self.settings = None
        self.dbconn = dbconn
        self._refs = {}

        self.log(MythLog.DATABASE, MythLog.INFO,
                        "Attempting connection: {0}".format(dbconn.ident))
        try:
            _Connection_Pool.__init__(self)
        except:
            raise MythDBError(MythError.DB_CONNECTION, dbconn)

    def cursor(self, log=None, type=LoggedCursor):
        """
        Create a cursor on which queries may be performed. The
        optional cursorclass parameter is used to create the
        Cursor. By default, self.cursorclass=connections.LoggedCursor
        is used.
        """
        if log is None:
            log = self.log
        conn = self.acquire()
        cursor = conn.cursor(type)
        cursor.log = log

        r = weakref.ref(cursor, self._callback)
        self._refs[id(r)] = (r, id(conn))

        return cursor

    def _callback(self, ref):
        self.log(MythLog.DATABASE, MythLog.DEBUG, \
                    'database callback received',\
                     str(hex(id(ref))))
        refid = id(ref)
        ref, connid = self._refs.pop(refid)
        if connid in self._inuse:
            self.release(connid)

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
        if type:
            cursor.rollback()
        else:
            cursor.commit()
        cursor.close()

class BEConnection( object ):
    """
    This is the basic backend connection object.
    You probably don't want to use this directly.
    """
    logmodule = 'Python Backend Connection'

    def __init__(self, backend, port, localname=None,
                    blockshutdown=False, timeout=10.0):
        self.connected = False
        self.log = MythLog(self.logmodule)
        self._socklock = allocate_lock()

        self.host = backend
        self.port = port
        self.hostname = None
        self.deadline = timeout
        self.blockshutdown = blockshutdown

        self.localname = localname
        if self.localname is None:
            self.localname = socket.gethostname()

        try:
            self.connect()
        except socket.error, e:
            self.log.logTB(MythLog.SOCKET)
            self.connected = False
            self.log(MythLog.GENERAL, MythLog.CRIT,
                    "Couldn't connect to backend [%s]:%d" \
                    % (self.host, self.port))
            raise MythBEError(MythError.PROTO_CONNECTION, self.host, self.port)
        except:
            raise

    def __del__(self):
        self.disconnect()

    def connect(self):
        if self.connected:
            return
        self.log(MythLog.SOCKET|MythLog.NETWORK, MythLog.INFO,
                "Connecting to backend [%s]:%d" % (self.host, self.port))
        if ':' in self.host:
            self.socket = deadlinesocket(socket.AF_INET6, socket.SOCK_STREAM)
        else:
            self.socket = deadlinesocket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.log = self.log
        self.socket.connect((self.host, self.port))
        self.socket.setdeadline(self.deadline)
        self.connected = True
        self.check_version()
        self.announce()

    def disconnect(self, hard=False):
        if not self.connected:
            return
        self.log(MythLog.SOCKET|MythLog.NETWORK, MythLog.INFO,
                "Terminating connection to [%s]:%d" % (self.host, self.port))
        if not hard:
            self.backendCommand('DONE',0)
        self.connected = False
        self.socket.shutdown(1)
        self.socket.close()

    def reconnect(self, hard=False):
        self.disconnect(hard)
        self.connect()

    def announce(self):
        # set type, 'Playback' blocks backend shutdown
        type = ('Monitor','Playback')[self.blockshutdown]

        res = self.backendCommand('ANN %s %s 0' % (type, self.localname))
        if res != 'OK':
            self.log(MythLog.GENERAL, MythLog.ERR,
                            "Unexpected answer to ANN", res)
            raise MythBEError(MythError.PROTO_ANNOUNCE,
                                                self.host, self.port, res)
        else:
            self.log(MythLog.SOCKET, MythLog.INFO,
                     "Successfully connected to backend",
                     "[%s]:%d" % (self.host, self.port))
        self.hostname = self.backendCommand('QUERY_HOSTNAME')

    def check_version(self):
        res = self.backendCommand('MYTH_PROTO_VERSION %s %s' \
                    % (PROTO_VERSION, PROTO_TOKEN)).split(BACKEND_SEP)
        if res[0] == 'REJECT':
            self.log(MythLog.GENERAL, MythLog.ERR,
                            "Backend has version %s, and we speak %s" %\
                            (res[1], PROTO_VERSION))
            raise MythBEError(MythError.PROTO_MISMATCH,
                                                int(res[1]), PROTO_VERSION)

    def backendCommand(self, data, deadline=None):
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
        if deadline is None:
            deadline = self.socket.getdeadline()
        if deadline < 1000:
            deadline += time()

        try:
            # lock socket access
            with self._socklock:
                # send command string
                self.socket.sendheader(data.encode('utf-8'))
                # wait timeout for data to be received on the socket
                t = time()
                timeout = (deadline-t) if (deadline-t>0) else 0.0
                if len(select([self.socket],[],[], timeout)[0]) == 0:
                    # no data, return
                    return u''
                res = self.socket.recvheader(deadline=deadline)

                # convert to unicode
                try:
                    res = unicode(''.join([res]), 'utf8')
                except:
                    res = u''.join([res])

                return res
        except MythError, e:
            if e.sockcode == 54:
                # remote has closed connection, attempt reconnect
                self.reconnect(True)
                return self.backendCommand(data, deadline)
            else:
                raise

    def blockShutdown(self):
        if not self.blockshutdown:
            self.backendCommand('BLOCK_SHUTDOWN')
            self.blockshutdown = True

    def unblockShutdown(self):
        if self.blockshutdown:
            self.backendCommand('ALLOW_SHUTDOWN')
            self.blockshutdown = False

class BEEventConnection( BEConnection ):
    """
    This is the basic event listener object.
    You probably don't want to use this directly.
    """
    logmodule = 'Python Event Connection'

    def __init__(self, backend, port, localname=None, deadline=10.0, level=2):
        self._regevents = weakref.WeakValueDictionary()
        self._announced = False
        self._eventlevel = level

        self.hostname = ""
        self.threadrunning = False
        self.eventqueue = Queue.Queue()

        super(BEEventConnection, self).__init__(backend, port, localname, 
                                                False, deadline)

    def connect(self):
        if self.connected:
            return
        super(BEEventConnection, self).connect()
        if len(self._regevents) and (not self.threadrunning):
            start_new_thread(self.eventloop, ())

    def announce(self):
        # set event level, 3=system only, 2=generic only, 1=both, 0=none
        res = self.backendCommand('ANN Monitor %s %d' % \
                                (self.localname, self._eventlevel))
        ### NOTE: backendCommand cannot be reliably used beyond this point
        self._announced = True
        if res != 'OK':
            self.log(MythLog.GENERAL, MythLog.ERR,
                            "Unexpected answer to ANN", res)
            raise MythBEError(MythError.PROTO_ANNOUNCE,
                                                self.host, self.port, res)
        else:
            self.log(MythLog.SOCKET, MythLog.INFO,
                     "Successfully connected to backend",
                     "%s:%d" % (self.host, self.port))

    def queueEvents(self):
        """
        Pulls any unsolicited messages from the receive buffer
        """

        # return if not connected
        if not self.connected:
            return

        try:
            with self._socklock:
                while True:
                    # continue looping as long as data exists on the line
                    if len(select([self.socket],[],[], 0.0)[0]) == 0:
                        return
                    event = self.socket.recvheader(deadline=0.0)

                    try:
                        event = unicode(''.join([event]), 'utf8')
                    except:
                        event = u''.join([event])

                    if event[:15] == 'BACKEND_MESSAGE':
                        self.eventqueue.put(event)
                    # else discard

        except MythError, e:
            if e.sockcode == 54:
                # remote has closed connection, attempt reconnect
                self.reconnect(True, True)
                return self.backendCommand(data, deadline)
            else:
                raise
 
    def registeruser(self, uuid, opts):
        self._regusers[uuid] = opts

    def registerevent(self, regex, function):
        self._regevents[regex] = function
        if (not self.threadrunning) and \
           (self._eventlevel):
                start_new_thread(self.eventloop, ())

    def eventloop(self):
        self.threadrunning = True
        while (len(self._regevents) > 0) and self.connected:
            self.queueEvents()
            while True:
                try:
                    event = self.eventqueue.get_nowait()
                    for r,f in self._regevents.items():
                        # check for matches against registered handlers
                        if r.match(event):
                            try:
                                f(event)
                            except KeyboardInterrupt:
                                raise
                            except EOFError:
                                raise
                            except:
                                pass
                except Queue.Empty:
                    break
            sleep(0.1)
        self.threadrunning = False

    def backendCommand(self, data, deadline=None):
        if self._announced:
            return ""
        return super(BEEventConnection, self).backendCommand(data, deadline)

class FEConnection( object ):
    """
    This is the basic frontend connection object.
    You probably dont want to use this directly.
    """
    _res_handler = {'jump':  lambda r: r=='OK',
                    'key':   lambda r: r=='OK',
                    'query': lambda r: r,
                    'play':  lambda r: r=='OK'}
    _res_help = {'jump':  re.compile('(\w+)[ ]+- ([\w /,]+)'),
                 'key':   lambda r: r.split('\r\n')[4].split(', '),
                 'query': re.compile('query ([\w ]*\w+)[ \r\n]+- ([\w /,]+)'),
                 'play':  re.compile('play ([\w -:]*\w+)[ \r\n]+- ([\w /:,\(\)]+)')}

    def __init__(self, host, port, deadline=10.0, test=True):
        self.isConnected = False
        self.host = host
        self.port = int(port)
        self.log = MythLog('Python Frontend Connection')
        self._deadline = deadline
        self.connect(test)

    @classmethod
    def fromUPNP(cls, timeout=5):
        reLOC = re.compile('http://(?P<ip>[0-9\.]+):(?P<port>[0-9]+)/.*')
        msearch = MSearch()
        for res in msearch.searchMythFE(timeout):
            ip, port = reLOC.match(res['location']).group(1,2)
            port = 6546
            try:
                yield cls(ip, port)
            except MythFEError:
                pass

    @classmethod
    def testList(cls, felist):
        felist = [cls(addr, port, test=False) for addr,port in felist]
        for fe in felist:
            if not fe.isConnected:
                continue
            try:
                t = time()
                fe._test(t + 2.0)
            except MythError, e:
                continue
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
            return
        self.socket = deadlinesocket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setdeadline(self._deadline)
        self.socket.log = self.log
        try:
            self.socket.connect((self.host, self.port))
        except:
            if test:
                raise MythFEError(MythError.FE_CONNECTION, self.host, self.port)
            return
        self.isConnected = True
        if test: self._test()

    def disconnect(self):
        if not self.isConnected:
            return
        self.socket.send("exit")
        self.socket.close()
        self.socket = None
        self.isConnected = False

    def _test(self, timeout=None):
        if re.search("MythFrontend Network Control.*",self.recv(timeout)) is None:
            self.socket.close()
            self.socket = None
            self.isConnected = False
            raise MythFEError(MythError.FE_ANNOUNCE, self.host, self.port)

    def send(self, mode, command=None):
        if not self.isConnected:
            self.connect()
        if command is None:
            self.socket.send("help %s\n" % mode)
            res = self.recv()
            try:
                return self._res_help[mode].findall(res)
            except:
                return self._res_help[mode](res)
        else:
            self.socket.send("%s %s\n" % (mode, command))
            return self._res_handler[mode](self.recv())

    def recv(self, deadline=None):
        prompt = re.compile('([\r\n.]*)\r\n# ')
        try:
            res = self.socket.dlexpect(prompt, deadline=deadline)
        except socket.error:
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

    class _Request( urllib2.Request ):
        def open(self): return urllib2.urlopen(self)
        def read(self): return self.open().read()

        def setJSON(self):
            if self.get_header('Accept') != 'application/json':
                self.add_header('Accept', 'application/json')

        def readEtree(self):
            return etree.fromstring(self.read())

        def readJSON(self):
            self.setJSON()
            return json.loads(self.read())

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

    def _request(self, path='', **keyvars):
        """
        obj._request(path=None, **keyvars) -> Request object

        'path' is an optional page to access.
        'keyvars' are a series of optional variables to specify on the URL.

        The request object supports open() and read(), as well as supports
            editing of HTTP headers and POST data. 
        """
        url = 'http://{0.host}:{0.port}/{1}'.format(self, path)
        if keyvars:
            url += '?' + '&'.join(
                        ['{0}={1}'.format(k,urllib2.quote(v))
                                for k,v in keyvars.items()])
        self.log(self.log.NETWORK, self.log.DEBUG, 'Generating request', url)
        return self._Request(url)

    def getConnectionInfo(self, pin=0):
        """Return dbconn dict from backend connection info."""
        dbconn = {'SecurityPin':pin}
        try:
            dat = self._request('Myth/GetConnectionInfo', \
                                        Pin='{0:0>4}'.format(pin)).readJSON()
            return dat['ConnectionInfo']['Database']
        except:
            return {}

