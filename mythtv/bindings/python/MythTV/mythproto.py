# -*- coding: utf-8 -*-

"""
Provides connection cache and data handlers for accessing the backend.
"""

from MythTV.static import PROTO_VERSION, BACKEND_SEP, RECSTATUS, AUDIO_PROPS, \
                          VIDEO_PROPS, SUBTITLE_TYPES
from MythTV.exceptions import MythError, MythDBError, MythBEError, MythFileError
from MythTV.logging import MythLog
from MythTV.altdict import DictData
from MythTV.connections import BEConnection, BEEventConnection
from MythTV.database import DBCache
from MythTV.utility import CMPRecord, datetime, \
                           ParseEnum, CopyData, CopyData2, check_ipv6

from datetime import date
from time import sleep
from thread import allocate_lock
from random import randint
import socket
import weakref
import re
import os

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
        backendCommand()    - Sends a formatted command to the backend
                              and returns the response.
    """

    class _ConnHolder( object ):
        blockshutdown = 0
        command = None
        event = None

    logmodule = 'Python Backend Connection'
    _shared = weakref.WeakValueDictionary()
    _reip = re.compile('(?:\d{1,3}\.){3}\d{1,3}')

    def __repr__(self):
        return "<%s 'myth://%s:%d/' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1],
                 self.hostname, self.port, hex(id(self)))

    def __init__(self, backend=None, blockshutdown=False, events=False, db=None):
        self.db = DBCache(db)
        self.log = MythLog(self.logmodule, db=self.db)
        self.hostname = None

        self.sendcommands = True
        self.blockshutdown = blockshutdown
        self.receiveevents = events

        if backend is None:
            # no backend given, use master
            self.host = self.db.settings.NULL.MasterServerIP
            self.hostname = self.db._gethostfromaddr(self.host)

        else:
            backend = backend.strip('[]')
            query = "SELECT hostname FROM settings WHERE value=? AND data=?"
            if self._reip.match(backend):
                # given backend is IP address
                self.host = backend
                self.hostname = self.db._gethostfromaddr(
                                            backend, 'BackendServerIP')
            elif check_ipv6(backend):
                # given backend is IPv6 address
                self.host = backend
                self.hostname = self.db._gethostfromaddr(
                                            backend, 'BackendServerIP6')
            else:
                # given backend is hostname, pull address from database
                self.hostname = backend
                self.host = self.db._getpreferredaddr(backend)

        # lookup port from database
        self.port = int(self.db.settings[self.hostname].BackendServerPort)
        if not self.port:
            raise MythDBError(MythError.DB_SETTING, 'BackendServerPort',
                                            self.port)

        self._ident = '%s:%d' % (self.host, self.port)
        if self._ident in self._shared:
            # existing connection found
            self._conn = self._shared[self._ident]
            if self.sendcommands:
                if self._conn.command is None:
                    self._conn.command = self._newcmdconn()
                elif self.blockshutdown:
                    # upref block of shutdown
                    # issue command to backend if needed
                    self._conn.blockshutdown += 1
                    if self._conn.blockshutdown == 1:
                        self._conn.command.blockShutdown()
            if self.receiveevents:
                if self._conn.event is None:
                    self._conn.event = self._neweventconn()
        else:
            # no existing connection, create new
            self._conn = self._ConnHolder()

            if self.sendcommands:
                self._conn.command = self._newcmdconn()
                if self.blockshutdown:
                    self._conn.blockshutdown = 1
            if self.receiveevents:
                self._conn.event = self._neweventconn()

            self._shared[self._ident] = self._conn

        self._events = self._listhandlers()
        for func in self._events:
            self.registerevent(func)

    def __del__(self):
        # downref block of shutdown
        # issue command to backend if needed
        #print 'destructing BECache'
        if self.blockshutdown:
            self._conn.blockshutdown -= 1
            if not self._conn.blockshutdown:
                self._conn.command.unblockShutdown()

    def _newcmdconn(self):
        return BEConnection(self.host, self.port, self.db.gethostname(),
                            self.blockshutdown)

    def _neweventconn(self):
        return BEEventConnection(self.host, self.port, self.db.gethostname())

    def backendCommand(self, data):
        """
        obj.backendCommand(data) -> response string

        Sends a formatted command via a socket to the mythbackend.
        """
        if self._conn.command is None:
            return ""
        return self._conn.command.backendCommand(data)

    def _listhandlers(self):
        return []

    def registerevent(self, func, regex=None):
        if self._conn.event is None:
            return
        if regex is None:
            regex = func()
        self._conn.event.registerevent(regex, func)

    def clearevents(self):
        self._events = []

class BEEvent( BECache ):
    #this class will be removed at a later date
    pass

def findfile(filename, sgroup, db=None):
    """
    findfile(filename, sgroup, db=None) -> StorageGroup object

    Will search through all matching storage groups, searching for file.
    Returns matching storage group upon success.
    """
    db = DBCache(db)
    for sg in db.getStorageGroup(groupname=sgroup):
        # search given group
        if sg.local:
            if os.access(sg.dirname+filename, os.F_OK):
                return sg
    for sg in db.getStorageGroup():
        # not found, search all other groups
        if sg.local:
            if os.access(sg.dirname+filename, os.F_OK):
                return sg
    return None

def ftopen(file, mode, forceremote=False, nooverwrite=False, db=None, \
                       chanid=None, starttime=None, download=False):
    """
    ftopen(file, mode, forceremote=False, nooverwrite=False, db=None)
                                        -> FileTransfer object
                                        -> file object
    Method will attempt to open file locally, falling back to remote access
            over mythprotocol if necessary.
    'forceremote' will force a FileTransfer object if possible.
    'file' takes a standard MythURI:
                myth://<group>@<host>:<port>/<path>
    'mode' takes a 'r' or 'w'
    'nooverwrite' will refuse to open a file writable,
                if a local file is found.
    """
    db = DBCache(db)
    log = MythLog('Python File Transfer', db=db)
    reuri = re.compile(\
        'myth://((?P<group>.*)@)?(?P<host>[\[\]a-zA-Z0-9_\-\.]*)(:[0-9]*)?/(?P<file>.*)')
    reip = re.compile('(?:\d{1,3}\.){3}\d{1,3}')

    if mode not in ('r','w'):
        raise TypeError("File I/O must be of type 'r' or 'w'")

    if chanid and starttime:
        protoopen = lambda host, file, storagegroup: \
                      RecordFileTransfer(host, file, storagegroup,\
                                         mode, chanid, starttime, db)
    elif download:
        protoopen = lambda host, lfile, storagegroup: \
                      DownloadFileTransfer(host, lfile, storagegroup, \
                                           mode, file, db)
    else:
        protoopen = lambda host, file, storagegroup: \
                      FileTransfer(host, file, storagegroup, mode, db)

    # process URI (myth://<group>@<host>[:<port>]/<path/to/file>)
    match = reuri.match(file)
    if match is None:
        raise MythError('Invalid FileTransfer input string: '+file)
    host = match.group('host')
    filename = match.group('file')
    sgroup = match.group('group')
    if sgroup is None:
        sgroup = 'Default'

    # get full system name
    host = host.strip('[]')
    if reip.match(host) or check_ipv6(host):
        host = db._gethostfromaddr(host)

    # user forced to remote access
    if forceremote:
        if (mode == 'w') and (filename.find('/') != -1):
            raise MythFileError(MythError.FILE_FAILED_WRITE, file,
                                'attempting remote write outside base path')
        if nooverwrite and FileOps(host, db=db).fileExists(filename, sgroup):
            raise MythFileError(MythError.FILE_FAILED_WRITE, file,
                                'refusing to overwrite existing file')
        return protoopen(host, filename, sgroup)

    if mode == 'w':
        # check for pre-existing file
        path = FileOps(host, db=db).fileExists(filename, sgroup)
        sgs = list(db.getStorageGroup(groupname=sgroup))
        if path is not None:
            if nooverwrite:
                raise MythFileError(MythError.FILE_FAILED_WRITE, file,
                                'refusing to overwrite existing file')
            for sg in sgs:
                if sg.dirname in path:
                    if sg.local:
                        return open(sg.dirname+filename, mode)
                    else:
                        return protoopen(host, filename, sgroup)

        # prefer local storage for new files
        for i,v in reversed(list(enumerate(sgs))):
            if not v.local:
                sgs.pop(i)
            else:
                st = os.statvfs(v.dirname)
                v.free = st[0]*st[3]
        if len(sgs) > 0:
            # choose path with most free space
            sg = sorted(sgs, key=lambda sg: sg.free, reverse=True)[0]
            # create folder if it does not exist
            if filename.find('/') != -1:
                path = sg.dirname+filename.rsplit('/',1)[0]
                if not os.access(path, os.F_OK):
                    os.makedirs(path)
            log(log.FILE, log.INFO, 'Opening local file (w)',
                sg.dirname+filename)
            return open(sg.dirname+filename, mode)

        # fallback to remote write
        else:
            if filename.find('/') != -1:
                raise MythFileError(MythError.FILE_FAILED_WRITE, file,
                                'attempting remote write outside base path')
            return protoopen(host, filename, sgroup)
    else:
        # search for file in local directories
        sg = findfile(filename, sgroup, db)
        if sg is not None:
            # file found, open local
            log(log.FILE, log.INFO, 'Opening local file (r)',
                sg.dirname+filename)
            return open(sg.dirname+filename, mode)
        else:
        # file not found, open remote
            return protoopen(host, filename, sgroup)

class FileTransfer( BEEvent ):
    """
    A connection to mythbackend intended for file transfers.
    Emulates the primary functionality of the local 'file' object.
    """
    logmodule = 'Python FileTransfer'

    class BETransConn( BEConnection ):
        def __init__(self, host, port, localname, filename, sgroup, mode):
            self.filename = filename
            self.sgroup = sgroup
            self.mode = mode
            BEConnection.__init__(self, host, port, localname)

        def announce(self):
            if self.mode == 'r':
                cmd = 'ANN FileTransfer %s 0 0 2000'
            elif self.mode == 'w':
                cmd = 'ANN FileTransfer %s 1'

            res = self.backendCommand(
                    BACKEND_SEP.join([cmd % self.localname,
                                      self.filename,
                                      self.sgroup]))
            if res.split(BACKEND_SEP)[0] != 'OK':
                raise MythBEError(MythError.PROTO_ANNOUNCE,
                                  self.host, self.port, res)
            else:
                sp = res.split(BACKEND_SEP)
                self._sockno = int(sp[1])
                self._size = int(sp[2])

        def __del__(self):
            self.socket.shutdown(1)
            self.socket.close()

        def send(self, buffer):
            return self.socket.send(buffer)

        def recv(self, count):
            return self.socket.dlrecv(count)

    def __str__(self):
        return 'myth://%s@%s/%s' % (self.sgroup, self.host, self.filename)

    def __repr__(self):
        return "<open file '%s', mode '%s' at %s>" % \
                          (str(self), self.mode, hex(id(self)))

    def __init__(self, host, filename, sgroup, mode, db=None):
        self.filename = filename
        self.sgroup = sgroup
        self.mode = mode

        # open control socket
        BEEvent.__init__(self, host, True, db=db)
        # open transfer socket
        self.ftsock = self.BETransConn(self.host, self.port,
                    self._conn.command.localname, self.filename,
                    self.sgroup, self.mode)
        self.open = True

        self._sockno = self.ftsock._sockno
        self._size = self.ftsock._size
        self._pos = 0
        self._tsize = 2**15
        self._tmax = 2**17
        self._count = 0
        self._step = 2**12

    def __del__(self):
        self.backendCommand('QUERY_FILETRANSFER '+BACKEND_SEP.join(
                                        [str(self._sockno), 'DONE']))
        del self.ftsock
        self.open = False

    def tell(self):
        """FileTransfer.tell() -> current offset in file"""
        return self._pos

    def close(self):
        """FileTransfer.close() -> None"""
        self.__del__()

    def rewind(self):
        """FileTransfer.rewind() -> None"""
        self.seek(0)

    def read(self, size):
        """
        FileTransfer.read(size) -> string of <size> characters
            Requests over 128KB will be buffered internally.
        """

        # some sanity checking
        if self.mode != 'r':
            raise MythFileError('attempting to read from a write-only socket')
        if size == 0:
            return ''
        if self._pos + size > self._size:
            size = self._size - self._pos

        buff = ''
        while len(buff) < size:
            ct = size - len(buff)
            if ct > self._tsize:
                # drop size and bump counter if over limit
                self._count += 1
                ct = self._tsize

            # request transfer
            res = self.backendCommand('QUERY_FILETRANSFER '\
                        +BACKEND_SEP.join(
                                [str(self._sockno),
                                 'REQUEST_BLOCK',
                                 str(ct)]))

            if res == '':
                # complete failure, hard reset position and retry
                self._count = 0
                self.seek(self._pos)
                continue

            if int(res) == ct:
                if (self._count >= 5) and (self._tsize < self._tmax):
                    # multiple successful transfers, bump transfer limit
                    self._count = 0
                    self._tsize += self._step

            else:
                if int(res) == -1:
                    # complete failure, hard reset position and retry
                    self._count = 0
                    self.seek(self._pos)
                    continue

                # partial failure, reset counter and drop transfer limit
                ct = int(res)
                self._count = 0
                self._tsize -= 2*self._step
                if self._tsize < self._step:
                    self._tsize = self._step

            # append data and move position
            buff += self.ftsock.recv(ct)
            self._pos += ct
        return buff

    def write(self, data):
        """
        FileTransfer.write(data) -> None
            Requests over 128KB will be buffered internally
        """
        if self.mode != 'w':
            raise MythFileError('attempting to write to a read-only socket')
        while len(data) > 0:
            size = len(data)
            # check size for buffering
            if size > self._tsize:
                size = self._tsize
                buff = data[:size]
                data = data[size:]
            else:
                buff = data
                data = ''
            # push data to server
            self._pos += int(self.ftsock.send(buff))
            if self._pos > self._size:
                self._size = self._pos
            # inform server of new data
            self.backendCommand('QUERY_FILETRANSFER '\
                    +BACKEND_SEP.join(\
                            [str(self._sockno),
                             'WRITE_BLOCK',
                             str(size)]))
        return

    def seek(self, offset, whence=0):
        """
        FileTransfer.seek(offset, whence=0) -> None
            Seek 'offset' number of bytes
            whence == 0 - from start of file
                      1 - from current position
                      2 - from end of file
        """
        if whence == 0:
            if offset < 0:
                offset = 0
            elif offset > self._size:
                offset = self._size
        elif whence == 1:
            if offset + self._pos < 0:
                offset = -self._pos
            elif offset + self._pos > self._size:
                offset = self._size - self._pos
        elif whence == 2:
            if offset > 0:
                offset = 0
            elif offset < -self._size:
                offset = -self._size
            whence = 0
            offset = self._size+offset

        res = self.backendCommand('QUERY_FILETRANSFER '\
                +BACKEND_SEP.join(
                        [str(self._sockno),'SEEK',
                         str(offset),
                         str(whence),
                         str(self._pos)])\
                 ).split(BACKEND_SEP)
        if res[0] == '-1':
            raise MythFileError(MythError.FILE_FAILED_SEEK, \
                                    str(self), offset, whence)
        self._pos = int(res[0])

    def flush(self):
        pass

class RecordFileTransfer( FileTransfer ):
    """
    A connection to mythbackend intended for file transfers.
    Emulates the primary functionality of the local 'file' object.
    Listens for UPDATE_FILE_SIZE events from the backend.
    """
    logmodule = 'Python RecordFileTransfer'

    def _listhandlers(self):
        return [self.updatesize]

    def updatesize(self, event=None):
        if event is None:
            self.re_update = re.compile(\
              re.escape(BACKEND_SEP).\
                join(['BACKEND_MESSAGE',
                      'UPDATE_FILE_SIZE %s %s (?P<size>[0-9]*)' %\
                         (self.chanid, \
                          self.starttime.isoformat()),
                      'empty']))
            return self.re_update
        match = self.re_update.match(event)
        self._size = int(match.group('size'))

    def __init__(self, host, filename, sgroup, mode,
                       chanid, starttime, db=None):
        self.chanid = chanid
        self.starttime = starttime
        FileTransfer.__init__(self, host, filename, sgroup, mode, db)

class DownloadFileTransfer( FileTransfer ):
    """
    A connection to mythbackend intended for file transfers.
    Emulates the primary functionality of the local 'file' object.
    Listens for DOWNLOAD_FILE events from the backend.
    """
    logmodule = 'Python DownloadFileTransfer'

    def _listhandlers(self):
        return [self.updatesize, self.downloadcomplete]

    def updatesize(self, event=None):
        if event is None:
            self.re_update = re.compile(\
                    re.escape(BACKEND_SEP).\
                            join(['BACKEND_MESSAGE',
                                  'DOWNLOAD_FILE UPDATE',
                                  '.*',
                                  re.escape(self.uri),
                                  '(?P<size>[0-9]*)',
                                  '(?P<total>[0-9]*)']))
            return self.re_update
        match = self.re_update.match(event)
        self._size = int(match.group('size'))

    def downloadcomplete(self, event=None):
        if event is None:
            self.re_complete = re.compile(\
                    re.escape(BACKEND_SEP).\
                            join(['BACKEND_MESSAGE',
                                  'DOWNLOAD_FILE FINISHED',
                                  '.*',
                                  re.escape(self.uri),
                                  '(?P<total>[0-9]*)',
                                  '(?P<errstr>.*)',
                                  '(?P<errcode>.*)']))
            return self.re_complete
        match = self.re_complete.match(event)
        self._size = int(match.group('total'))
        self.clearevents()

    def __init__(self, host, filename, sgroup, mode, uri, db=None):
        self.uri = uri
        FileTransfer.__init__(self, host, filename, sgroup, mode, db)

    def __del__(self):
        FileTransfer.__del__(self)
        if self.sgroup == 'Temp':
            FileOps(self.host, db=self.db).\
                        deleteFile(self.filename, self.sgroup)

class FileOps( BECache ):
    __doc__ = BECache.__doc__+"""
        getRecording()      - return a Program object for a recording
        deleteRecording()   - notify the backend to delete a recording
        forgetRecording()   - allow a recording to re-record
        stopRecording()     - stop an in-progress recording without deleting
                              the file
        deleteFile()        - notify the backend to delete a file
                              in a storage group
        getHash()           - return the hash of a file in a storage group
        reschedule()        - trigger a run of the scheduler
        fileExists()        - check whether a file can be found on a backend
        download()          - issue a download by the backend
        downloadTo()        - issue a download by the backend to a defined 
                              location
        allocateEventLock() - create an EventLock object that will be locked
                              until a requested regular expression is
                              encountered
    """
    logmodule = 'Python Backend FileOps'

    def getRecording(self, chanid, starttime):
        """FileOps.getRecording(chanid, starttime) -> Program object"""
        starttime = datetime.duck(starttime)
        res = self.backendCommand('QUERY_RECORDING TIMESLOT %s %s' \
                        % (chanid, starttime.mythformat()))\
                    .split(BACKEND_SEP)
        if res[0] == 'ERROR':
            return None
        else:
            return Program(res[1:], db=self.db)

    def deleteRecording(self, program, force=False):
        """
        FileOps.deleteRecording(program, force=False) -> retcode
            'force' will force a delete even if the file cannot be found
            retcode will be -1 on success, -2 on failure
        """
        command = 'DELETE_RECORDING'
        if force:
            command = 'FORCE_DELETE_RECORDING'
        return self.backendCommand(BACKEND_SEP.join(\
                    [command,program.toString()]))

    def forgetRecording(self, program):
        """FileOps.forgetRecording(program) -> None"""
        self.backendCommand(BACKEND_SEP.join(['FORGET_RECORDING',
                    program.toString()]))

    def stopRecording(self, program):
        """FileOps.stopRecording(program) -> None"""
        self.backendCommand(BACKEND_SEP.join(['STOP_RECORDING',
                    program.toString()]))

    def deleteFile(self, file, sgroup):
        """FileOps.deleteFile(file, storagegroup) -> retcode"""
        return self.backendCommand(BACKEND_SEP.join(\
                    ['DELETE_FILE',file,sgroup]))

    def getHash(self, file, sgroup, host=None):
        """FileOps.getHash(file, storagegroup, host) -> hash string"""
        m = ['QUERY_FILE_HASH', file, sgroup]
        if host:
            m.append(host)
        return self.backendCommand(BACKEND_SEP.join(m))

    def reschedule(self, recordid=-1, wait=False):
        """FileOps.reschedule() -> None"""
        if wait:
            eventlock = self.allocateEventLock(\
                            re.escape(BACKEND_SEP).join(\
                                    ['BACKEND_MESSAGE',
                                     'SCHEDULE_CHANGE',
                                     'empty']))
        if recordid == 0:
            self.backendCommand(BACKEND_SEP.join(\
                ['RESCHEDULE_RECORDINGS',
                 'CHECK 0 0 0 Python',
                 '', '', '', '**any**']))
        else:
            if recordid == -1:
                recordid = 0
            self.backendCommand(BACKEND_SEP.join(\
                ['RESCHEDULE_RECORDINGS',
                 'MATCH ' + str(recordid) + ' 0 0 - Python']))
        if wait:
            eventlock.wait()

    def fileExists(self, file, sgroup='Default'):
        """FileOps.fileExists() -> file path"""
        res = self.backendCommand(BACKEND_SEP.join((\
                    'QUERY_FILE_EXISTS',file,sgroup))).split(BACKEND_SEP)
        if int(res[0]) == 0:
            return None
        else:
            return res[1]

    def download(self, url):
        filename = '/download_%s.tmp' % hex(randint(0,2**32)).split('x')[-1]
        return self.downloadTo(url, 'Temp', filename, True, True)

    def downloadTo(self, url, storagegroup, filename, \
                         forceremote=False, openfile=False):
        if openfile:
            eventlock = self.allocateEventLock(\
                    re.escape(BACKEND_SEP).\
                            join(['BACKEND_MESSAGE',
                                  'DOWNLOAD_FILE UPDATE',
                                  re.escape(url)]))
        if filename[0] != '/':
            filename = '/'+filename
        res = self.backendCommand(BACKEND_SEP.join((\
                    'DOWNLOAD_FILE', url, storagegroup, filename))).\
                    split(BACKEND_SEP)
        if res[0] != 'OK':
            raise MythBEError('Download failed')
        if openfile:
            eventlock.wait()
            return ftopen(res[1], 'r', forceremote, db=self.db, download=True)

    def allocateEventLock(self, regex):
        try:
            regex.match('')
        except AttributeError:
            regex = re.compile(regex)
        return EventLock(regex, self.hostname, self.db)

    class _ProgramQuery( object ):
        def __init__(self, query, header_length=0, sorted=False,
                     recstatus=None, handler=None):
            self.query = query
            self.header_length = header_length
            self.recstatus = recstatus
            self.handler = handler if handler else Program
            self.sorted = sorted
            self.inst = None
            self.func = None

        def __call__(self, *args, **kwargs):
            if self.func is None:
                if len(args) == 1:
                    self.func = args[0]
                elif 'func' in kwargs:
                    self.func = kwargs['func']
                if not callable(self.func):
                    raise MythError('_ProgramQuery must receive a callable '+\
                                    'before it is functional')
                self.__doc__ = self.func.__doc__
                self.__name__ = self.func.__name__
                self.__module__ = self.func.__module__
                return self
            elif self.inst is None:
                raise MythError('Call to uninitialized _ProgramQuery instance')
            if self.sorted:
                return self.sortedrun(*args, **kwargs)
            return self.run(*args, **kwargs)

        def __get__(self, obj, type):
            if obj is None:
                return self
            cls = self.__class__(self.query, self.header_length, self.sorted,
                                 self.recstatus, self.handler)
            cls.inst = obj
            cls.func = self.func.__get__(obj, type)
            return cls

        def run(self, *args, **kwargs):
            pgfieldcount = len(Program._field_order)
            pgrecstatus = Program._field_order.index('recstatus')
            pgrecordid = Program._field_order.index('recordid')

            res = self.inst.backendCommand(self.query).split(BACKEND_SEP)
            for i in xrange(self.header_length):
                res.pop(0)
            num_progs = int(res.pop(0))
            if num_progs*pgfieldcount != len(res):
                raise MythBEError(MythBEError.PROTO_PROGRAMINFO)

            for i in range(num_progs):
                offs = i * pgfieldcount
                if self.recstatus is not None:
                    if int(res[offs+pgrecstatus]) != self.recstatus:
                        continue
                pg = self.handler(res[offs:offs+pgfieldcount],
                                  db=self.inst.db)
                pg = self.func(pg, *args, **kwargs)
                if pg is not None:
                    yield pg

        def sortedrun(self, *args, **kwargs):
            return iter(sorted(self.run(*args, **kwargs),
                               key=lambda p: p.starttime))

class FreeSpace( DictData ):
    """Represents a FreeSpace entry."""
    _field_order = [ 'host',         'path',     'islocal',
                    'disknumber',   'sgroupid', 'blocksize',
                    'totalspace', 'usedspace']
    _field_type = [3, 3, 2, 0, 0, 0, 0, 0]
    def __str__(self):
        return "<FreeSpace '%s@%s' at %s>"\
                    % (self.path, self.host, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, raw):
        DictData.__init__(self, raw)
        self.freespace = self.totalspace - self.usedspace

class Program( CMPRecord, DictData, RECSTATUS, AUDIO_PROPS, \
                         VIDEO_PROPS, SUBTITLE_TYPES ):
    """Represents a program with all detail returned by the backend."""

    _field_order = [ 'title',        'subtitle',     'description',
                     'season',       'episode',      'category',
                     'chanid',       'channum',      'callsign',
                     'channame',     'filename',     'filesize',
                     'starttime',    'endtime',      'findid',
                     'hostname',     'sourceid',     'cardid',
                     'inputid',      'recpriority',  'recstatus',
                     'recordid',     'rectype',      'dupin',
                     'dupmethod',    'recstartts',   'recendts',
                     'programflags', 'recgroup',     'outputfilters',
                     'seriesid',     'programid',    'inetref',
                     'lastmodified', 'stars',        'airdate',
                     'playgroup',    'recpriority2', 'parentid',
                     'storagegroup', 'audio_props',  'video_props',
                     'subtitle_type','year']
    _field_type = [  3,      3,      3,
                     0,      0,      3,
                     0,      3,      3,
                     3,      3,      0,
                     4,      4,      0,
                     3,      0,      0,
                     0,      0,      0,
                     0,      3,      0,
                     0,      4,      4,
                     3,      3,      3,
                     3,      3,      3,
                     4,      1,      5,
                     3,      0,      3,
                     3,      0,      0,
                     0,      0]
    def __str__(self):
        return u"<Program '%s','%s' at %s>" % (self.title,
                 self.starttime.isoformat(' '), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, raw, db=None):
        DictData.__init__(self, raw)
        self._db = db
        self.AudioProps = ParseEnum(self, 'audio_props', AUDIO_PROPS, False)
        self.VideoProps = ParseEnum(self, 'video_props', VIDEO_PROPS, False)
        self.SubtitleType = ParseEnum(self, 'subtitle_type', SUBTITLE_TYPES, False)

    @classmethod
    def fromEtree(cls, etree, db=None):
        xmldat = dict(etree.attrib)
        xmldat.update(etree.find('Channel').attrib)
        if etree.find('Recording') is not None:
            xmldat.update(etree.find('Recording').attrib)

        dat = {}
        if etree.text:
            dat['description'] = etree.text.strip()
        for key in ('title','subTitle','seriesId','programId','airdate',
                'category','hostname','chanNum','callSign','playGroup',
                'recGroup','rectype','programFlags','chanId','recStatus',
                'commFree','stars','filesize'):
            if key in xmldat:
                dat[key.lower()] = xmldat[key]
        for key in ('startTime','endTime','lastModified',
                                'recStartTs','recEndTs'):
            if key in xmldat:
                dat[key.lower()] = str(datetime.fromIso(xmldat[key])\
                                            .timestamp())

        raw = []
        defs = (0,0,0,'',0,'')
        for i,v in enumerate(cls._field_order):
            if v in dat:
                raw.append(dat[v])
            else:
                raw.append(defs[cls._field_type[i]])
        return cls(raw, db)

    @classmethod
    def fromJSON(cls, prog, db=None):
        dat = {}
        CopyData(prog, dat, ('Title', 'SubTitle', 'SeriesId', 'ProgramId',
                             'Airdate', 'Category', 'Hostname', 'ProgramFlags',
                             'Stars', 'FileSize', 'Description'), True)
        CopyData(prog['Channel'], dat,
                ('ChanId', 'CallSign', 'ChanNum', 'InputId', 'SourceId'), True)
        CopyData2(prog['Channel'], dat, (('ChannelName', 'channame'),))
        CopyData(prog['Recording'], dat,
                ('DupMethod', 'PlayGroup', 'RecType', 'RecordId'), True)
        CopyData2(prog['Recording'], dat, (('DupInType', 'dupin'),
                                           ('Status', 'recstatus')))

        for k,v in (('starttime', prog['StartTime']),
                    ('endtime', prog['EndTime']),
                    ('recstartts', prog['Recording']['StartTs']),
                    ('recendts', prog['Recording']['EndTs']),
                    ('lastmodified', prog['LastModified'])):
            if v:
                dat[k] = str(datetime.fromIso(v).timestamp())

        raw = []
        defs = (0,0,0,'',0,'')
        for i,v in enumerate(cls._field_order):
            if v in dat:
                raw.append(dat[v])
            else:
                raw.append(defs[cls._field_type[i]])
        return cls(raw, db)

    @classmethod
    def fromRecorded(cls, rec):
        be = FileOps(db=rec._db)
        return be.getRecording(rec.chanid, rec.starttime)

    def toString(self):
        """
        Program.toString() -> string representation
                    for use with backend protocol commands
        """
        return BACKEND_SEP.join(self._deprocess())

    def delete(self, force=False, rerecord=False):
        """
        Program.delete(force=False, rerecord=False) -> retcode
                Informs backend to delete recording and all relevent data.
                'force' forces a delete if the file cannot be found.
                'rerecord' sets the file as recordable in oldrecorded
        """
        be = FileOps(db=self._db)
        res = int(be.deleteRecording(self, force=force))
        if res < -1:
            raise MythBEError('Failed to delete file')
        if rerecord:
            be.forgetRecording(self)
        return res

    def _openProto(self):
        if not self.filename.startswith('myth://'):
            self.filename = 'myth://%s@%s/%s' % (self.storagegroup, \
                                                 self.hostname, \
                                                 self.filename)
        return ftopen(self.filename, 'r', db=self._db, chanid=self.chanid, \
                      starttime=self.recstartts)

    def _openXML(self):
        xml = XMLConnection(self.hostname, 6544)
        return xml._request('Content/GetRecording', ChanId=self.chanid, \
                            StartTime=self.starttime.isoformat()).open()

    def open(self, type='r'):
        """Program.open(type='r') -> file or FileTransfer object"""
        if type != 'r':
            raise MythFileError(MythError.FILE_FAILED_WRITE, self.filename,
                            'Program () objects cannot be opened for writing.')
        if self.recstatus not in (self.rsRecording, self.rsRecorded):
            raise MythFileEror(MythError.FILE_FAILED_READ, '',
                            'Program () does not exist for access.')

        if self.filename is None:
            return self._openXML()
        else:
            return self._openProto()

    def formatPath(self, path, replace=None):
        """
        Program.formatPath(path, replace=None) -> formatted path string
                'path' string is formatted as per mythrename.pl syntax
        """
        for (tag, data) in (('T','title'), ('S','subtitle'),
                            ('R','description'), ('C','category'),
                            ('U','recgroup'), ('hn','hostname'),
                            ('c','chanid') ):
            tmp = unicode(self[data]).replace('/','-')
            path = path.replace('%'+tag, tmp)

        for (data, pre) in (   ('recstartts','%'), ('recendts','%e'),
                               ('starttime','%p'),('endtime','%pe') ):
            for (tag, format) in (('y','%y'),('Y','%Y'),('n','%m'),('m','%m'),
                                  ('j','%d'),('d','%d'),('g','%I'),('G','%H'),
                                  ('h','%I'),('H','%H'),('i','%M'),('s','%S'),
                                  ('a','%p'),('A','%p') ):
                path = path.replace(pre+tag, self[data].strftime(format))

        airdate = self.airdate
        if airdate is None:
            airdate = date(1900,1,1)
        for (tag, format) in (('y','%y'),('Y','%Y'),('n','%m'),('m','%m'),
                              ('j','%d'),('d','%d')):
            path = path.replace('%o'+tag, airdate.strftime(format))

        path = path.replace('%-','-')
        path = path.replace('%%','%')
        path += '.'+self.filename.split('.')[-1]

        # clean up for windows
        if replace is not None:
            for char in ('\\',':','*','?','"','<','>','|'):
                path = path.replace(char, replace)
        return path

    def formatJob(self, cmd):
        """
        Program.formatPath(cmd) -> formatted command string
                'cmd' string is formatted as per MythJobQueue syntax
        """
        for tag in ('chanid','title','subtitle','description','hostname',
                    'category','recgroup','playgroup','parentid','findid',
                    'recstatus','rectype'):
            cmd = cmd.replace('%%%s%%' % tag.upper(), str(self[tag]))
        cmd = cmd.replace('%ORIGINALAIRDATE%', self.airdate.isoformat())
        for (tag, data) in (('STARTTIME','recstartts'),('ENDTIME','recendts'),
                            ('PROGSTART','starttime'),('PROGEND','endtime')):
            t = self[data]
            cmd = cmd.replace('%%%s%%' % tag, t.mythformat())
            cmd = cmd.replace('%%%sISO%%' % tag, t.isoformat())
            cmd = cmd.replace('%%%sISOUTC%%' % tag, \
                        (t+timedelta(0,altzone)).isoformat())
        cmd = cmd.replace('%VERBOSELEVEL%', MythLog._parsemask())
        cmd = cmd.replace('%RECID%', str(self.recordid))

        path = FileOps(self.hostname, db=self._db).fileExists(\
                        self.filename.rsplit('/',1)[1], self.storagegroup)
        cmd = cmd.replace('%DIR%', path.rsplit('/',1)[0])
        cmd = cmd.replace('%FILE%',path.rsplit('/',1)[1])
        cmd = cmd.replace('%REACTIVATE%', str(OldRecorded(\
                    (self.chanid, self.recstartts),db=self._db).reactivate))
        return cmd

    def _playOnFe(self, fe):
        return fe.send('play','program %d %s' % \
                    (self.chanid, self.recstartts.isoformat()))


class EventLock( BECache ):
    event = None
    def __init__(self, regex, backend=None, db=None):
        self.regex = regex
        self._lock = allocate_lock()
        self._lock.acquire()
        super(EventLock, self).__init__(backend, False, True, db)

    def _listhandlers(self):
        return [self._unlock]

    def _unlock(self, event=None):
        if event is None:
            return self.regex
        self._lock.release()
        self._events = []
        self.event = event

    def wait(self, blocking=True):
        res = self._lock.acquire(blocking)
        if res:
            self._lock.release()
        return res
