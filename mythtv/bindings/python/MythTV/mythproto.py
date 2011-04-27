# -*- coding: utf-8 -*-

"""
Provides connection cache and data handlers for accessing the backend.
"""

from __future__ import with_statement

from static import PROTO_VERSION, BACKEND_SEP, RECSTATUS, AUDIO_PROPS, \
                   VIDEO_PROPS, SUBTITLE_TYPES
from exceptions import MythError, MythDBError, MythBEError, MythFileError
from logging import MythLog
from altdict import DictData
from connections import BEConnection
from database import DBCache
from utility import SplitInt, CMPRecord, datetime

from datetime import date
from time import sleep
from thread import allocate_lock
from random import randint
from uuid import uuid4
import socket
import weakref
import re
import os

class BECache( SplitInt ):
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
            with self.db.cursor(self.log) as cursor:
                if cursor.execute("""SELECT hostname FROM settings
                                     WHERE value='BackendServerIP'
                                     AND data=%s""", self.host) == 0:
                    # no match found
                    raise MythDBError(MythError.DB_SETTING, 'BackendServerIP',
                                            self.host)
                self.hostname = cursor.fetchone()[0]

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
            self.be = BEConnection(self.host, self.port, \
                                    self.db.gethostname(), self.opts)
            self.be.registeruser(self._uuid, self.opts)
            self._shared[self._ident] = self.be

    def backendCommand(self, data):
        """
        obj.backendCommand(data) -> response string

        Sends a formatted command via a socket to the mythbackend.
        """
        return self.be.backendCommand(data)

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

    def clearevents(self):
        self._events = []

def findfile(filename, sgroup, db=None):
    """
    findfile(filename, sgroup, db=None) -> StorageGroup object

    Will search through all matching storage groups, searching for file.
    Returns matching storage group upon success.
    """
    db = DBCache(db)
    for sg in db.getStorageGroup(groupname=sgroup):
        # search given group
        if not sg.local:
            continue
        if os.access(sg.dirname+filename, os.F_OK):
            return sg
    for sg in db.getStorageGroup():
        # not found, search all other groups
        if not sg.local:
            continue
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
        'myth://((?P<group>.*)@)?(?P<host>[a-zA-Z0-9_\-\.]*)(:[0-9]*)?/(?P<file>.*)')
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
    if reip.match(host):
        with db.cursor(log) as cursor:
            if cursor.execute("""SELECT hostname FROM settings
                                 WHERE value='BackendServerIP'
                                 AND data=%s""", host) == 0:
                raise MythDBError(MythError.DB_SETTING, \
                                  'BackendServerIP', backend)
            host = cursor.fetchone()[0]

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
            log(log.FILE, 'Opening local file (w)', sg.dirname+filename)
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
            log(log.FILE, 'Opening local file (r)',
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
                write = False
            elif self.mode == 'w':
                write = True

            res = self.backendCommand('ANN FileTransfer %s %d %d %s' \
                      % (self.localname, write, False,
                         BACKEND_SEP.join(
                                ['2000', self.filename, self.sgroup])))
            if res.split(BACKEND_SEP)[0] != 'OK':
                raise MythBEError(MythError.PROTO_ANNOUNCE,
                                  self.host, self.port, res)
            else:
                sp = res.split(BACKEND_SEP)
                self._sockno = int(sp[1])
                self._size = [int(i) for i in sp[2:]]

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
        self.ftsock = self.BETransConn(self.host, self.port, \
                    self.be.localname, self.filename, self.sgroup, self.mode)
        self.open = True

        self._sockno = self.ftsock._sockno
        self._size = self.joinInt(*self.ftsock._size)
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
            self.pos += int(self.ftsock.send(buff))
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

        curhigh,curlow = self.splitInt(self._pos)
        offhigh,offlow = self.splitInt(offset)

        res = self.backendCommand('QUERY_FILETRANSFER '\
                +BACKEND_SEP.join(
                        [str(self._sockno),'SEEK',
                         str(offhigh),str(offlow),
                         str(whence),
                         str(curhigh),str(curlow)])\
                 ).split(BACKEND_SEP)
        if res[0] == '-1':
            raise MythFileError(MythError.FILE_FAILED_SEEK, \
                                    str(self), offset, whence)
        self._pos = self.joinInt(*res)

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
        match = self.re_update(event)
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

class FileOps( BEEvent ):
    __doc__ = BEEvent.__doc__+"""
        getRecording()      - return a Program object for a recording
        deleteRecording()   - notify the backend to delete a recording
        forgetRecording()   - allow a recording to re-record
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

    def deleteFile(self, file, sgroup):
        """FileOps.deleteFile(file, storagegroup) -> retcode"""
        return self.backendCommand(BACKEND_SEP.join(\
                    ['DELETE_FILE',file,sgroup]))

    def getHash(self, file, sgroup):
        """FileOps.getHash(file, storagegroup) -> hash string"""
        return self.backendCommand(BACKEND_SEP.join((\
                    'QUERY_FILE_HASH',file, sgroup)))

    def reschedule(self, recordid=-1, wait=False):
        """FileOps.reschedule() -> None"""
        if wait:
            eventlock = self.allocateEventLock(\
                            re.escape(BACKEND_SEP).join(\
                                    ['BACKEND_MESSAGE',
                                     'SCHEDULE_CHANGE',
                                     'empty']))
        self.backendCommand('RESCHEDULE_RECORDINGS '+str(recordid))
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

    def _getPrograms(self, query, recstatus=None, recordid=None, header=0):
        pgfieldcount = len(Program._field_order)
        pgrecstatus = Program._field_order.index('recstatus')
        pgrecordid = Program._field_order.index('recordid')

        res = self.backendCommand(query).split(BACKEND_SEP)
        for i in xrange(header):
            res.pop(0)
        num_progs = int(res.pop(0))
        if num_progs*pgfieldcount != len(res):
            raise MythBEError(MythBEError.PROTO_PROGRAMINFO)

        for i in range(num_progs):
            offs = i * pgfieldcount
            if recstatus is not None:
                if int(res[offs+pgrecstatus]) != recstatus:
                    continue
            if recordid is not None:
                if int(res[offs+pgrecordid]) != recordid:
                    continue
            yield Program(res[offs:offs+pgfieldcount], db=self.db)

    def _getSortedPrograms(self, query, recstatus=None, \
                           recordid=None, header=0):
        return sorted(self._getPrograms(query, recstatus, recordid, header),\
                      key=lambda p: p.starttime)

class FreeSpace( DictData, SplitInt ):
    """Represents a FreeSpace entry."""
    _field_order = [ 'host',         'path',     'islocal',
                    'disknumber',   'sgroupid', 'blocksize',
                    'ts_high',      'ts_low',   'us_high',
                    'us_low']
    _field_type = [3, 3, 2, 0, 0, 0, 0, 0, 0, 0]
    def __str__(self):
        return "<FreeSpace '%s@%s' at %s>"\
                    % (self.path, self.host, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, raw):
        DictData.__init__(self, raw)
        self.totalspace = self.joinInt(self.ts_high, self.ts_low)
        self.usedspace = self.joinInt(self.us_high, self.us_low)
        self.freespace = self.totalspace - self.usedspace

class Program( DictData, RECSTATUS, AUDIO_PROPS, VIDEO_PROPS, \
                         SUBTITLE_TYPES, CMPRecord ):
    """Represents a program with all detail returned by the backend."""

    _field_order = [ 'title',        'subtitle',     'description',
                     'category',     'chanid',       'channum',
                     'callsign',     'channame',     'filename',
                     'filesize',     'starttime',    'endtime',
                     'findid',       'hostname',     'sourceid',
                     'cardid',       'inputid',      'recpriority',
                     'recstatus',    'recordid',     'rectype',
                     'dupin',        'dupmethod',    'recstartts',
                     'recendts',     'programflags', 'recgroup',
                     'outputfilters','seriesid',     'programid',
                     'lastmodified', 'stars',        'airdate',
                     'playgroup',    'recpriority2', 'parentid',
                     'storagegroup', 'audio_props',  'video_props',
                     'subtitle_type','year']
    _field_type = [  3,      3,      3,
                     3,      0,      3,
                     3,      3,      3,
                     0,      4,      4,
                     0,      3,      0,
                     0,      0,      0,
                     0,      0,      3,
                     0,      0,      4,
                     4,      3,      3,
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
        return xml._queryObject('GetRecording', ChanId=self.chanid, \
                                    StartTime=self.starttime.isoformat())

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
        for (tag, data) in (('STARTTIME','recstartts'),('ENDTIME','recendts'),
                            ('PROGSTART','starttime'),('PROGEND','endtime')):
            t = self[data]
            cmd = cmd.replace('%%%s%%' % tag, t.mythformat())
            cmd = cmd.replace('%%%sISO%%' % tag, t.isoformat())
            cmd = cmd.replace('%%%sISOUTC%%' % tag, \
                        (t+timedelta(0,altzone)).isoformat())
        cmd = cmd.replace('%VERBOSELEVEL%', MythLog._parselevel())
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


class EventLock( BEEvent ):
    def __init__(self, regex, backend=None, db=None):
        self.regex = regex
        self._lock = allocate_lock()
        self._lock.acquire()
        BEEvent.__init__(self, backend, db=db)

    def _listhandlers(self):
        return [self._unlock]

    def _unlock(self, event=None):
        if event is None:
            return self.regex
        self._lock.release()
        self._events = []

    def wait(self, blocking=True):
        res = self._lock.acquire(blocking)
        if res:
            self._lock.release()
        return res
