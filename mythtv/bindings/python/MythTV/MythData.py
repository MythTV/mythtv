# -*- coding: utf-8 -*-

"""
Provides data access classes for accessing and managing MythTV data
"""

from MythStatic import *
from MythBase import *

import re, sys, socket, os
import xml.etree.cElementTree as etree
from time import mktime, strftime, strptime
from datetime import date, time, datetime
from socket import gethostbyaddr, gethostname

#### FILE ACCESS ####

def ftopen(file, type, forceremote=False, nooverwrite=False, db=None):
    """
    ftopen(file, type, forceremote=False, nooverwrite=False, db=None)
                                        -> FileTransfer object
                                        -> file object
    Method will attempt to open file locally, falling back to remote access
            over mythprotocol if necessary.
    'forceremote' will force a FileTransfer object if possible.
    'file' takes a standard MythURI:
                myth://<group>@<host>:<port>/<path>
    'type' takes a 'r' or 'w'
    'nooverwrite' will refuse to open a file writable, if a local file is found.
    """
    db = MythDBBase(db)
    log = MythLog('Python File Transfer', db=db)
    reuri = re.compile(\
        'myth://((?P<group>.*)@)?(?P<host>[a-zA-Z0-9\.]*)(:[0-9]*)?/(?P<file>.*)')
    reip = re.compile('(?:\d{1,3}\.){3}\d{1,3}')

    if type not in ('r','w'):
        raise TypeError("File I/O must be of type 'r' or 'w'")

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
        c = db.cursor(log)
        if c.execute("""SELECT hostname FROM settings
                        WHERE value='BackendServerIP'
                        AND data=%s""", host) == 0:
            c.close()
            raise MythDBError(MythError.DB_SETTING, 'BackendServerIP', backend)
        host = c.fetchone()[0]
        c.close()

    # user forced to remote access
    if forceremote:
        if (type == 'w') and (filename.find('/') != -1):
            raise MythFileError(MythError.FILE_FAILED_WRITE, file, 
                                'attempting remote write outside base path')
        return FileTransfer(host, filename, sgroup, type, db)

    sgs = db.getStorageGroup(groupname=sgroup)

    if type == 'w':
        # prefer local storage always
        for i in range(len(sgs)-1,-1,-1):
            if not sgs[i].local:
                sgs.pop(i)
            else:
                st = os.statvfs(sgs[i].dirname)
                sgs[i].free = st[0]*st[3]
        if len(sgs) > 1:
            # choose path with most free space
            sg = sgs.pop()
            while len(sgs):
                if sgs[0].free > sg.free: sg = sgs.pop()
                else: sgs.pop()
            # check that folder exists
            if filename.find('/') != -1:
                path = sg.dirname+filename.rsplit('/',1)[0]
                if not os.access(path, os.F_OK):
                    os.makedirs(path)
            if nooverwrite:
                if os.access(sg.dirname+filename, os.F_OK):
                    raise MythDBError(MythError.FILE_FAILED_WRITE, file,
                                        'refusing to overwrite existing file')
            log(log.FILE, 'Opening local file (w)', sg.dirname+filename)
            return open(sg.dirname+filename, 'w')
        elif len(sgs) == 1:
            if filename.find('/') != -1:
                path = sgs[0].dirname+filename.rsplit('/',1)[0]
                if not os.access(path, os.F_OK):
                    os.makedirs(path)
            if nooverwrite:
                if os.access(sgs[0].dirname+filename, os.F_OK):
                    raise MythFileError(MythError.FILE_FAILED_WRITE, file,
                                        'refusing to overwrite existing file')
            log(log.FILE, 'Opening local file (w)', sgs[0].dirname+filename)
            return open(sgs[0].dirname+filename, 'w')
        else:
            if filename.find('/') != -1:
                raise MythFileError(MythError.FILE_FAILED_WRITE, file, 
                                'attempting remote write outside base path')
            return FileTransfer(host, filename, sgroup, 'w', db)
    else:
        # search for file in local directories
        for sg in sgs:
            if sg.local:
                if os.access(sg.dirname+filename, os.F_OK):
                    # file found, open local
                    log(log.FILE, 'Opening local file (r)',
                                                sg.dirname+filename)
                    return open(sg.dirname+filename, type)
        # file not found, open remote
        return FileTransfer(host, filename, sgroup, type, db)

class FileTransfer( MythBEConn ):
    """
    A connection to mythbackend intended for file transfers.
    Emulates the primary functionality of the local 'file' object.
    """
    logmodule = 'Python FileTransfer'
    def __repr__(self):
        return "<open file 'myth://%s:%s/%s', mode '%s' at %s>" % \
                          (self.sgroup, self.host, self.filename, \
                                self.type, hex(id(self)))

    def __init__(self, host, filename, sgroup, type, db=None):
        if type not in ('r','w'):
            raise MythError("FileTransfer type must be read ('r') "+
                                "or write ('w')")

        self.host = host
        self.filename = filename
        self.sgroup = sgroup
        self.type = type

        self.tsize = 2**15
        self.tmax = 2**17
        self.count = 0
        self.step = 2**12

        self.open = False
        # create control socket
        self.control = MythBEBase(host, 'Playback', db=db)
        self.control.log.module = 'Python FileTransfer Control'
        # continue normal Backend initialization
        MythBEConn.__init__(self, host, type, db=db)
        self.open = True

    def announce(self, type):
        # replacement announce for Backend object
        if type == 'w':
            self.w = True
        elif type == 'r':
            self.w = False
        res = self.backendCommand('ANN FileTransfer %s %d %d %s' \
                    % (socket.gethostname(), self.w, False,
                        BACKEND_SEP.join(['-1',self.filename,self.sgroup])))
        if res.split(BACKEND_SEP)[0] != 'OK':
            raise MythError(MythError.PROTO_ANNOUNCE, self.host, self.port, res)
        else:
            sp = res.split(BACKEND_SEP)
            self.sockno = int(sp[1])
            self.pos = 0
            self.size = (int(sp[2]) + (int(sp[3])<0))*2**32 + int(sp[3])

    def __del__(self):
        if not self.open:
            return
        self.control.backendCommand('QUERY_FILETRANSFER '\
                    +BACKEND_SEP.join([str(self.sockno), 'DONE']))
        self.socket.shutdown(1)
        self.socket.close()
        self.open = False

    def tell(self):
        """FileTransfer.tell() -> current offset in file"""
        return self.pos

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

        def recv(self, size):
            buff = ''
            while len(buff) < size:
                buff += self.socket.recv(size-len(buff))
            return buff

        if self.w:
            raise MythFileError('attempting to read from a write-only socket')
        if size == 0:
            return ''

        final = self.pos+size
        if final > self.size:
            final = self.size

        buff = ''
        while self.pos < final:
            self.count += 1
            size = final - self.pos
            if size > self.tsize:
                size = self.tsize
            res = self.control.backendCommand('QUERY_FILETRANSFER '+\
                        BACKEND_SEP.join([  str(self.sockno),
                                            'REQUEST_BLOCK',
                                            str(size)]))
            #print '%s - %s' % (int(res), size)
            if int(res) == size:
                if (self.count == 10) and (self.tsize < self.tmax) :
                    self.count = 0
                    self.tsize += self.step
            else:
                if int(res) == -1:
                    self.seek(self.pos)
                    continue
                size = int(res)
                self.count = 0
                self.tsize -= self.step
                if self.tsize < self.step:
                    self.tsize = self.step

            buff += recv(self, size)
            self.pos += size
        return buff

    def write(self, data):
        """
        FileTransfer.write(data) -> None
            Requests over 128KB will be buffered internally
        """
        if not self.w:
            raise MythFileError('attempting to write to a read-only socket')
        while len(data) > 0:
            size = len(data)
            if size > self.tsize:
                buff = data[self.tsize:]
                data = data[:self.tsize]
            else:
                buff = data
                data = ''
            self.pos += int(self.socket.send(data))
            self.control.backendCommand('QUERY_FILETRANSFER '+BACKEND_SEP\
                    .join([str(self.sockno),'WRITE_BLOCK',str(size)]))
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
            elif offset > self.size:
                offset = self.size
        elif whence == 1:
            if offset + self.pos < 0:
                offset = -self.pos
            elif offset + self.pos > self.size:
                offset = self.size - self.pos
        elif whence == 2:
            if offset > 0:
                offset = 0
            elif offset < -self.size:
                offset = -self.size
            whence = 0
            offset = self.size+offset

        curhigh,curlow = self.control.splitInt(self.pos)
        offhigh,offlow = self.control.splitInt(offset)

        res = self.control.backendCommand('QUERY_FILETRANSFER '+BACKEND_SEP\
                        .join([str(self.sockno),'SEEK',str(offhigh),
                                str(offlow),str(whence),str(curhigh),
                                str(curlow)])\
                    ).split(BACKEND_SEP)
        self.pos = self.control.joinInt(int(res[0]),int(res[1]))

class FileOps( MythBEBase ):
    __doc__ = MythBEBase.__doc__+"""
        getRecording()      - return a Program object for a recording
        deleteRecording()   - notify the backend to delete a recording
        forgetRecording()   - allow a recording to re-record
        deleteFile()        - notify the backend to delete a file
                              in a storage group
        getHash()           - return the hash of a file in a storage group
        reschedule()        - trigger a run of the scheduler
    """
    logmodule = 'Python Backend FileOps'

    def getRecording(self, chanid, starttime):
        """FileOps.getRecording(chanid, starttime) -> Program object"""
        res = self.backendCommand('QUERY_RECORDING TIMESLOT %d %d' \
                        % (chanid, starttime)).split(BACKEND_SEP)
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

    def reschedule(self, recordid=-1):
        """FileOps.reschedule() -> None"""
        self.backendCommand('RESCHEDULE_RECORDINGS '+str(recordid))


class FreeSpace( DictData ):
    """Represents a FreeSpace entry."""
    field_order = [ 'host',         'path',     'islocal',
                    'disknumber',   'sgroupid', 'blocksize',
                    'ts_high',      'ts_low',   'us_high',
                    'us_low']
    field_type = [3, 3, 2, 0, 0, 0, 0, 0, 0, 0]
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


#### RECORDING ACCESS ####

class Program( DictData ):
    """Represents a program with all detail returned by the backend."""

    # recstatus
    TUNERBUSY           = -8
    LOWDISKSPACE        = -7
    CANCELLED           = -6
    DELETED             = -5
    ABORTED             = -4
    RECORDED            = -3
    RECORDING           = -2
    WILLRECORD          = -1
    UNKNOWN             = 0
    DONTRECORD          = 1
    PREVIOUSRECORDING   = 2
    CURRENTRECORDING    = 3
    EARLIERSHOWING      = 4
    TOOMANYRECORDINGS   = 5
    NOTLISTED           = 6
    CONFLICT            = 7
    LATERSHOWING        = 8
    REPEAT              = 9
    INACTIVE            = 10
    NEVERRECORD         = 11

    field_order = [ 'title',        'subtitle',     'description',
                    'category',     'chanid',       'channum',
                    'callsign',     'channame',     'filename',
                    'fs_high',      'fs_low',       'starttime',
                    'endtime',      'duplicate',    'shareable',
                    'findid',       'hostname',     'sourceid',
                    'cardid',       'inputid',      'recpriority',
                    'recstatus',    'recordid',     'rectype',
                    'dupin',        'dupmethod',    'recstartts',
                    'recendts',     'repeat',       'programflags',
                    'recgroup',     'commfree',     'outputfilters',
                    'seriesid',     'programid',    'lastmodified',
                    'stars',        'airdate',      'hasairdate',
                    'playgroup',    'recpriority2', 'parentid',
                    'storagegroup', 'audio_props',  'video_props',
                    'subtitle_type','year']
    field_type = [  3,      3,      3,
                    3,      0,      3,
                    3,      3,      3,
                    0,      0,      4,
                    4,      0,      0,
                    0,      3,      0,
                    0,      0,      0,
                    0,      0,      3,
                    0,      0,      4,
                    4,      0,      3,
                    3,      0,      3,
                    3,      3,      3,
                    1,      3,      0,
                    3,      0,      3,
                    3,      0,      0,
                    0,      0]
    def __str__(self):
        return u"<Program '%s','%s' at %s>" % (self.title,
                 self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, raw=None, db=None, etree=None):
        if raw:
            DictData.__init__(self, raw)
        elif etree:
            xmldat = etree.attrib
            xmldat.update(etree.find('Channel').attrib)
            if etree.find('Recording'):
                xmldat.update(etree.find('Recording').attrib)

            dat = {}
            if etree.text:
                dat['description'] = etree.text.strip()
            for key in ('title','subTitle','seriesId','programId','airdate',
                    'category','hostname','chanNum','callSign','playGroup',
                    'recGroup','rectype','programFlags','chanId','recStatus',
                    'commFree','stars'):
                if key in xmldat:
                    dat[key.lower()] = xmldat[key]
            for key in ('startTime','endTime','lastModified',
                                    'recStartTs','recEndTs'):
                if key in xmldat:
                    dat[key.lower()] = str(int(mktime(strptime(
                                        xmldat[key], '%Y-%m-%dT%H:%M:%S'))))
            if 'fileSize' in xmldat:
                dat['fs_high'],dat['fs_low'] = \
                                    self.splitInt(int(xmldat['fileSize']))

            raw = []
            defs = (0,0,0,'',0)
            for i in range(len(self.field_order)):
                if self.field_order[i] in dat:
                    raw.append(dat[self.field_order[i]])
                else:
                    raw.append(defs[self.field_type[i]])
            DictData.__init__(self, raw)
        else:
            raise InputError("Either 'raw' or 'etree' must be provided")
        self.db = MythDBBase(db)
        self.filesize = self.joinInt(self.fs_high,self.fs_low)

    def toString(self):
        """
        Program.toString() -> string representation
                    for use with backend protocol commands
        """
        data = []
        for i in range(0,PROGRAM_FIELDS):
            if self.data[self.field_order[i]] == None:
                datum = ''
            elif self.field_type[i] == 0:
                datum = str(self.data[self.field_order[i]])
            elif self.field_type[i] == 1:
                datum = locale.format("%0.6f", self.data[self.field_order[i]])
            elif self.field_type[i] == 2:
                datum = str(int(self.data[self.field_order[i]]))
            elif self.field_type[i] == 3:
                datum = self.data[self.field_order[i]]
            elif self.field_type[i] == 4:
                datum = str(int(mktime(self.data[self.field_order[i]].\
                                                            timetuple())))
            data.append(datum)
        return BACKEND_SEP.join(data)

    def delete(self, force=False, rerecord=False):
        """
        Program.delete(force=False, rerecord=False) -> retcode
                Informs backend to delete recording and all relevent data.
                'force' forces a delete if the file cannot be found.
                'rerecord' sets the file as recordable in oldrecorded
        """
        be = FileOps(db=self.db)
        res = int(be.deleteRecording(self, force=force))
        if res < -1:
            raise MythBEError('Failed to delete file')
        if rerecord:
            be.forgetRecording(self)
        return res

    def getRecorded(self):
        """Program.getRecorded() -> Recorded object"""
        return Recorded((self.chanid,self.recstartts), db=self.db)

    def open(self, type='r'):
        """Program.open(type='r') -> file or FileTransfer object"""
        if type != 'r':
            raise MythFileError(MythError.FILE_FAILED_WRITE, self.filename, 
                            'Program () objects cannot be opened for writing')
        if not self.filename.startswith('myth://'):
            self.filename = 'myth://%s/%s' % (self.hostname, self.filename)
        return ftopen(self.filename, 'r')

class Record( DBDataWrite ):
    """
    Record(id=None, db=None, raw=None) -> Record object
    """

    kNotRecording       = 0
    kSingleRecord       = 1
    kTimeslotRecord     = 2
    kChannelRecord      = 3
    kAllRecord          = 4
    kWeekslotRecord     = 5
    kFindOneRecord      = 6
    kOverrideRecord     = 7
    kDontRecord         = 8
    kFindDailyRecord    = 9
    kFindWeeklyRecord   = 10

    table = 'record'
    where = 'recordid=%s'
    setwheredat = 'self.recordid,'
    defaults = {'recordid':None,    'type':kAllRecord,      'title':u'Unknown',
                'subtitle':'',      'description':'',       'category':'',
                'station':'',       'seriesid':'',          'search':0,
                'last_record':datetime(1900,1,1),
                'next_record':datetime(1900,1,1),
                'last_delete':datetime(1900,1,1)}
    logmodule = 'Python Record'

    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized Record Rule at %s>" % hex(id(self))
        return u"<Record Rule '%s', Type %d at %s>" \
                                    % (self.title, self.type, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, id=None, db=None, raw=None):
        DBDataWrite.__init__(self, (id,), db, raw)

    def create(self, data=None):
        """Record.create(data=None) -> Record object"""
        self.wheredat = (DBDataWrite.create(self, data),)
        self._pull()
        FileOps(db=self.db).reschedule(self.recordid)
        return self

    def update(self, *args, **keywords):
        DBDataWrite.update(*args, **keywords)
        FileOps(db=self.db).reschedule(self.recordid)

class Recorded( DBDataWrite ):
    """
    Recorded(data=None, db=None, raw=None) -> Recorded object
            'data' is a tuple containing (chanid, storagegroup)
    """
    table = 'recorded'
    where = 'chanid=%s AND starttime=%s'
    setwheredat = 'self.chanid,self.starttime'
    defaults = {'title':u'Unknown', 'subtitle':'',          'description':'',
                'category':'',      'hostname':'',          'bookmark':0,
                'editing':0,        'cutlist':0,            'autoexpire':0,
                'commflagged':0,    'recgroup':'Default',   'seriesid':'',
                'programid':'',     'lastmodified':'CURRENT_TIMESTAMP',
                'filesize':0,       'stars':0,              'previouslyshown':0,
                'preserve':0,       'bookmarkupdate':0,
                'findid':0,         'deletepending':0,      'transcoder':0,
                'timestretch':1,    'recpriority':0,        'playgroup':'Default',
                'profile':'No',     'duplicate':1,          'transcoded':0,
                'watched':0,        'storagegroup':'Default'}
    logmodule = 'Python Recorded'

    class _Cast( DBDataCRef ):
        table = 'people'
        rtable = 'recordedcredits'
        t_ref = 'person'
        t_add = ['name']
        r_ref = 'person'
        r_add = ['role']
        w_field = ['chanid','starttime']
        def __repr__(self):
            if self.data is None:
                self._fill()
            if len(self.data) == 0:
                return 'No cast'
            cast = []
            for member in self.data:
                cast.append(member.name)
            return ', '.join(cast).encode('utf-8')

    class _Seek( DBDataRef ):
        table = 'recordedseek'
        wfield = ['chanid','starttime']

    class _Markup( DBDataRef ):
        table = 'recordedmarkup'
        wfield = ['chanid','starttime']
        
    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized Recorded at %s>" % hex(id(self))
        return u"<Recorded '%s','%s' at %s>" % (self.title,
                self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, data=None, db=None, raw=None):
        DBDataWrite.__init__(self, data, db, raw)
        if (data is not None) or (raw is not None):
            self.cast = self._Cast(self.wheredat, self.db)
            self.seek = self._Seek(self.wheredat, self.db)
            self.markup = self._Markup(self.wheredat, self.db)

    def create(self, data=None):
        """Recorded.create(data=None) -> Recorded object"""
        DBDataWrite.create(self, data)
        self.wheredat = (self.chanid,self.starttime)
        self._pull()
        self.cast = self._Cast(self.wheredat, self.db)
        self.seek = self._Seek(self.wheredat, self.db)
        self.markup = self._Markup(self.wheredat, self.db)
        return self

    def delete(self, force=False, rerecord=False):
        """
        Recorded.delete(force=False, rerecord=False) -> retcode
                Informs backend to delete recording and all relevent data.
                'force' forces a delete if the file cannot be found.
                'rerecord' sets the file as recordable in oldrecorded
        """
        return self.getProgram().delete(force, rerecord)

    def open(self, type='r'):
        """Recorded.open(type='r') -> file or FileTransfer object"""
        return ftopen("myth://%s@%s/%s" % ( self.storagegroup, \
                                            self.hostname,\
                                            self.basename), type, db=self.db)

    def getProgram(self):
        """Recorded.getProgram() -> Program object"""
        be = FileOps(db=self.db)
        return be.getRecording(self.chanid, 
                    int(self.starttime.strftime('%Y%m%d%H%M%S')))

    def getRecordedProgram(self):
        """Recorded.getRecordedProgram() -> RecordedProgram object"""
        return RecordedProgram((self.chanid,self.progstart), db=self.db)

    def formatPath(self, path, replace=None):
        """
        Recorded.formatPath(path, replace=None) -> formatted path string
                'path' string is formatted as per mythrename.pl
        """
        for (tag, data) in (('T','title'), ('S','subtitle'),
                            ('R','description'), ('C','category'),
                            ('U','recgroup'), ('hn','hostname'),
                            ('c','chanid') ):
            tmp = unicode(self[data]).replace('/','-')
            path = path.replace('%'+tag, tmp)
        for (data, pre) in (   ('starttime','%'), ('endtime','%e'),
                               ('progstart','%p'),('progend','%pe') ):
            for (tag, format) in (('y','%y'),('Y','%Y'),('n','%m'),('m','%m'),
                                  ('j','%d'),('d','%d'),('g','%I'),('G','%H'),
                                  ('h','%I'),('H','%H'),('i','%M'),('s','%S'),
                                  ('a','%p'),('A','%p') ):
                path = path.replace(pre+tag, self[data].strftime(format))
        for (tag, format) in (('y','%y'),('Y','%Y'),('n','%m'),('m','%m'),
                              ('j','%d'),('d','%d')):
            path = path.replace('%o'+tag,
                    self['originalairdate'].strftime(format))
        path = path.replace('%-','-')
        path = path.replace('%%','%')
        path += '.'+self['basename'].split('.')[-1]

        # clean up for windows
        if replace is not None:
            for char in ('\\',':','*','?','"','<','>','|'):
                path = path.replace(char, replace)
        return path

class RecordedProgram( DBDataWrite ):
    """
    RecordedProgram(data=None, db=None, raw=None) -> RecordedProgram object
            'data' is a tuple containing (chanid, storagegroup)
    """
    table = 'recordedprogram'
    where = 'chanid=%s AND starttime=%s'
    setwheredat = 'self.chanid,self.starttime'
    defaults = {'title':'',     'subtitle':'',
                'category':'',  'category_type':'',     'airdate':0,
                'stars':0,      'previouslyshown':0,    'title_pronounce':'',
                'stereo':0,     'subtitled':0,          'hdtv':0,
                'partnumber':0, 'closecaptioned':0,     'parttotal':0,
                'seriesid':'',  'originalairdate':'',   'showtype':u'',
                'colorcode':'', 'syndicatedepisodenumber':'',
                'programid':'', 'manualid':0,           'generic':0,
                'first':0,      'listingsource':0,      'last':0,
                'audioprop':u'','videoprop':u'',        
                'subtitletypes':u''}
    logmodule = 'Python RecordedProgram'

    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized RecordedProgram at %s>" % hex(id(self))
        return u"<RecordedProgram '%s','%s' at %s>" % (self.title,
                self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def create(self, data=None):
        """RecordedProgram.create(data=None) -> RecordedProgram object"""
        DBDataWrite.create(self, data)
        self.wheredat = (self.chanid, self.starttime)
        self._pull()
        return self

class OldRecorded( DBDataWrite ):
    """
    OldRecorded(data=None, db=None, raw=None) -> OldRecorded object
            'data' is a tuple containing (chanid, storagegroup)
    """
    table = 'oldrecorded'
    where = 'chanid=%s AND starttime=%s'
    setwheredat = 'self.chanid,self.starttime'
    defaults = {'title':'',     'subtitle':'',      
                'category':'',  'seriesid':'',      'programid':'',
                'findid':0,     'recordid':0,       'station':'',
                'rectype':0,    'duplicate':0,      'recstatus':-3,
                'reactivate':0, 'generic':0}
    logmodule = 'Python OldRecorded'

    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized OldRecorded at %s>" % hex(id(self))
        return u"<OldRecorded '%s','%s' at %s>" % (self.title,
                self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def create(self, data=None):
        """OldRecorded.create(data=None) -> OldRecorded object"""
        DBDataWrite.create(self, data)
        self.wheredat = (self.chanid, self.starttime)
        self._pull()
        return self

    def setDuplicate(self, record=False):
        """
        OldRecorded.setDuplicate(record=False) -> None
                Toggles re-recordability
        """
        c = self.db.cursor(self.log)
        c.execute("""UPDATE oldrecorded SET duplicate=%%s
                     WHERE %s""" % self.where, \
                tuple([record]+list(self.wheredat)))
        FileOps(db=self.db).reschedule(0)

    def update(self, *args, **keywords):
        """OldRecorded entries can not be altered"""
        return
    def delete(self):
        """OldRecorded entries cannot be deleted"""
        return

class Job( DBDataWrite ):
    """
    Job(id=None, chanid=None, starttime=None, db=None, raw=None) -> Job object
            Can be initialized with a Job id, or chanid and starttime.
    """
    # types
    NONE         = 0x0000
    SYSTEMJOB    = 0x00ff
    TRANSCODE    = 0x0001
    COMMFLAG     = 0x0002
    USERJOB      = 0xff00
    USERJOB1     = 0x0100
    USERJOB2     = 0x0200
    USERJOB3     = 0x0300
    USERJOB4     = 0x0400
    # cmds
    RUN          = 0x0000
    PAUSE        = 0x0001
    RESUME       = 0x0002
    STOP         = 0x0004
    RESTART      = 0x0008
    # flags
    NO_FLAGS     = 0x0000
    USE_CUTLIST  = 0x0001
    LIVE_REC     = 0x0002
    EXTERNAL     = 0x0004
    # statuses
    UNKNOWN      = 0x0000
    QUEUED       = 0x0001
    PENDING      = 0x0002
    STARTING     = 0x0003
    RUNNING      = 0x0004
    STOPPING     = 0x0005
    PAUSED       = 0x0006
    RETRY        = 0x0007
    ERRORING     = 0x0008
    ABORTING     = 0x0008
    DONE         = 0x0100
    FINISHED     = 0x0110
    ABORTED      = 0x0120
    ERRORED      = 0x0130
    CANCELLED    = 0x0140

    table = 'jobqueue'
    logmodule = 'Python Jobqueue'
    defaults = {'id': None}

    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized Job at %s>" % hex(id(self))
        return u"<Job '%s' at %s>" % (self.id, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, id=None, chanid=None, starttime=None, \
                        db=None, raw=None):
        self.__dict__['where'] = 'id=%s'
        self.__dict__['setwheredat'] = 'self.id,'

        if raw is not None:
            DBDataWrite.__init__(self, None, db, raw)
        elif id is not None:
            DBDataWrite.__init__(self, (id,), db, None)
        elif (chanid is not None) and (starttime is not None):
            self.__dict__['where'] = 'chanid=%s AND starttime=%s'
            DBDataWrite.__init__(self, (chanid,starttime), db, None)
        else:
            DBDataWrite.__init__(self, None, db, None)

    def create(self, data=None):
        """Job.create(data=None) -> Job object"""
        id = DBDataWrite.create(self, data)
        self.where = 'id=%s'
        self.wheredat = (id,)
        return self

    def setComment(self,comment):
        """Job.setComment(comment) -> None, updates comment"""
        self.comment = comment
        self.update()

    def setStatus(self,status):
        """Job.setStatus(Status) -> None, updates status"""
        self.status = status
        self.update()

class Channel( DBDataWrite ):
    """Channel(chanid=None, data=None, raw=None) -> Channel object"""
    table = 'channel'
    where = 'chanid=%s'
    setwheredat = 'self.chanid,'
    defaults = {'icon':'none',          'videofilters':'',  'callsign':u'',
                'xmltvid':'',           'recpriority':0,    'contrast':32768,
                'brightness':32768,     'colour':32768,     'hue':32768,
                'tvformat':u'Default',  'visible':1,        'outputfilters':'',
                'useonairguide':0,      'atsc_major_chan':0,
                'tmoffset':0,           'default_authority':'',
                'commmethod':-1,        'atsc_minor_chan':0,
                'last_record':datetime(1900,1,1)}
    logmodule = 'Python Channel'
    
    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized Channel at %s>" % hex(id(self))
        return u"<Channel '%s','%s' at %s>" % \
                        (self.chanid, self.name, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, chanid=None, db=None, raw=None):
        DBDataWrite.__init__(self, (chanid,), db, raw)

    def create(self, data=None):
        """Channel.create(data=None) -> Channel object"""
        DBDataWrite.create(self, data)
        self.wheredat = (self.chanid,)
        self._pull()
        return self

class Guide( DBData ):
    """
    Guide(data=None, db=None, raw=None) -> Guide object
            Data is a tuple of (chanid, starttime).
    """
    table = 'program'
    where = 'chanid=%s AND starttime=%s'
    setwheredat = 'self.chanid,self.starttime'
    logmodule = 'Python Guide'
    
    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized Guide at %s>" % hex(id(self))
        return u"<Guide '%s','%s' at %s>" % (self.title,
                self.starttime.strftime('%Y-%m-%d %H:%M:%S'), hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

    def __init__(self, data=None, db=None, raw=None, etree=None):
        if etree:
            db = MythDBBase(db)
            dat = {'chanid':etree[0]}
            attrib = etree[1].attrib
            for key in ('title','subTitle','category','seriesId',
                        'hostname','programId','airdate'):
                if key in attrib:
                    dat[key.lower()] = attrib[key]
            if 'stars' in attrib:
                dat['stars'] = locale.atof(attrib['stars'])
            if etree[1].text:
                dat['description'] = etree[1].text.strip()
            for key in ('startTime','endTime','lastModified'):
                if key in attrib:
                    dat[key.lower()] = datetime.strptime(
                                    attrib[key],'%Y-%m-%dT%H:%M:%S')

            raw = []
            for key in db.tablefields.program:
                if key in dat:
                    raw.append(dat[key])
                else:
                    raw.append(None)
            DBData.__init__(self, db=db, raw=raw)
        else:
            DBData.__init__(self, data=data, db=db, raw=raw)

    def record(self, type=Record.kAllRecord):
        rec = Record(db=self.db)
        for key in ('chanid','title','subtitle','description', 'category',
                    'seriesid','programid'):
            rec[key] = self[key]

        rec.startdate = self.starttime.date()
        rec.starttime = self.starttime-datetime.combine(rec.startdate, time())
        rec.enddate = self.endtime.date()
        rec.endtime = self.endtime-datetime.combine(rec.enddate, time())

        rec.station = Channel(self.chanid, db=self.db).callsign
        rec.type = type
        return rec.create()


#### MYTHVIDEO ####

class Video( DBDataWrite ):
    """Video(id=None, db=None, raw=None) -> Video object"""
    table = 'videometadata'
    where = 'intid=%s'
    setwheredat = 'self.intid,'
    defaults = {'subtitle':u'',             'director':u'Unknown',
                'rating':u'NR',             'inetref':u'00000000',
                'year':1895,                'userrating':0.0,
                'length':0,                 'showlevel':1,
                'coverfile':u'No Cover',    'host':u'',
                'intid':None,               'homepage':u'',
                'watched':False,            'category':'none',
                'browse':True,              'hash':u'',
                'season':0,                 'episode':0,
                'releasedate':date(1,1,1),  'childid':-1,
                'insertdate': datetime.now()}
    logmodule = 'Python Video'
    schema_value = 'mythvideo.DBSchemaVer'
    schema_local = MVSCHEMA_VERSION
    schema_name = 'MythVideo'
    category_map = [{'None':0},{0:'None'}]

    def _fill_cm(self, name=None, id=None):
        if name:
            if name not in self.category_map[0]:
                c = self.db.cursor(self.log)
                q1 = """SELECT intid FROM videocategory WHERE category=%s"""
                q2 = """INSERT INTO videocategory SET category=%s"""
                if c.execute(q1, name) == 0:
                    c.execute(q2, name)
                    c.execute(q1, name)
                id = c.fetchone()[0]
                self.category_map[0][name] = id
                self.category_map[1][id] = name

        elif id:
            if id not in self.category_map[1]:
                c = self.db.cursor(self.log)
                if c.execute("""SELECT category FROM videocategory
                                               WHERE intid=%s""", id) == 0:
                    raise MythDBError('Invalid ID found in videometadata.category')
                else:
                    name = c.fetchone()[0]
                self.category_map[0][name] = id
                self.category_map[1][id] = name

    def _pull(self):
        DBDataWrite._pull(self)
        self._fill_cm(id=self.category)
        self.category = self.category_map[1][self.category]

    def _push(self):
        name = self.category
        self._fill_cm(name=name)
        self.category = self.category_map[0][name]
        DBDataWrite._push(self)
        self.category = name

    def __repr__(self):
        return str(self).encode('utf-8')

    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized Video at %s>" % hex(id(self))
        res = self.title
        if self.season and self.episode:
            res += u' - %dx%02d' % (self.season, self.episode)
        if self.subtitle:
            res += u' - '+self.subtitle
        return u"<Video '%s' at %s>" % (res, hex(id(self)))

    def __init__(self, id=None, db=None, raw=None):
        DBDataWrite.__init__(self, (id,), db, raw)
        if raw is not None:
            self._fill_cm(id=self.category)
            self.category = self.category_map[1][self.category]
        if (id is not None) or (raw is not None):
            self.cast = self._Cast((self.intid,), self.db)
            self.genre = self._Genre((self.intid,), self.db)
            self.country = self._Country((self.intid,), self.db)

    def create(self, data=None):
        """Video.create(data=None) -> Video object"""
        c = self.db.cursor(self.log)
        fields = ' AND '.join(['%s=%%s' % f for f in \
                        ('title','subtitle','season','episode')])
        count = c.execute("""SELECT intid FROM videometadata WHERE %s""" %
                fields, (self.title, self.subtitle, self.season, self.episode))
        if count:
            id = c.fetchone()[0]
        else:
            if data:
                if 'category' in data:
                    self._fill_cm(name=data['category'])
                    data['category'] = self.category_map[0][data['category']]
            self._fill_cm(name=self.category)
            self.category = self.category_map[0][self.category]
            id = DBDataWrite.create(self, data)
        c.close()
        self.wheredat = (id,)
        self._pull()
        self.cast = self._Cast((self.intid,), self.db)
        self.genre = self._Genre((self.intid,), self.db)
        self.country = self._Country((self.intid,), self.db)
        return self

    class _Cast( DBDataCRef ):
        table = 'videocast'
        rtable = 'videometadatacast'
        t_ref = 'intid'
        t_add = ['cast']
        r_ref = 'idcast'
        r_add = []
        w_field = ['idvideo']
        def __repr__(self):
            if self.data is None:
                self._fill()
            if len(self.data) == 0:
                return ''
            cast = []
            for member in self.data:
                cast.append(member.cast)
            return u', '.join(cast).encode('utf-8')
        def add(self, member): DBDataCRef.add(self,(member,))
        def delete(self, member): DBDataCRef.delete(self,(member,))
        def clean(self):
            for member in self:
                self.delete(member.cast)

    class _Genre( DBDataCRef ):
        table = 'videogenre'
        rtable = 'videometadatagenre'
        t_ref = 'intid'
        t_add = ['genre']
        r_ref = 'idgenre'
        r_add = []
        w_field = ['idvideo']
        def __repr__(self):
            if self.data is None:
                self._fill()
            if len(self.data) == 0:
                return ''
            genre = []
            for member in self.data:
                genre.append(member.genre)
            return ', '.join(genre).encode('utf-8')
        def add(self, genre): DBDataCRef.add(self,(genre,))
        def delete(self, genre): DBDataCRef.delete(self,(genre,))
        def clean(self):
            for member in self:
                self.delete(member.genre)

    class _Country( DBDataCRef ):
        table = 'videocountry'
        rtable = 'videometadatacountry'
        t_ref = 'intid'
        t_add = ['country']
        r_ref = 'idcountry'
        r_add = []
        w_field = ['idvideo']
        def __repr__(self):
            if self.data is None:
                self._fill()
            if len(self.data) == 0:
                return ''
            country = []
            for member in self.data:
                country.append(member.country)
            return ', '.join(country).encode('utf-8')
        def add(self, country): DBDataCRef.add(self,(country,))
        def delete(self, country): DBDataCRef.delete(self,(country,))
        def clean(self):
            for member in self:
                self.delete(member.country)

    class _Markup( DBDataRef ):
        table = 'filemarkup'
        wfield = ['filename',]

    def _open(self, type, mode='r',nooverwrite=False):
        """
        Open file pointer
        """
        sgroup = {  'filename':'Videos',        'banner':'Banners',
                    'coverfile':'Coverart',     'fanart':'Fanart',
                    'screenshot':'Screenshots', 'trailer':'Trailers'}
        if self.data is None:
            return None
        if type not in sgroup:
            raise MythFileError(MythError.FILE_ERROR,
                            'Invalid type passed to Video._open(): '+str(type))
        SG = self.db.getStorageGroup(sgroup[type], self.host)
        if len(SG) == 0:
            SG = self.db.getStorageGroup('Videos', self.host)
            if len(SG) == 0:
                raise MythFileError(MythError.FILE_ERROR,
                                    'Could not find MythVideo Storage Groups')
        return ftopen('myth://%s@%s/%s' % ( SG[0].groupname,
                                            self.host,
                                            self.data[type]),
                            mode, False, nooverwrite, self.db)

    def delete(self):
        """Video.delete() -> None"""
        if self.data is None:
            return
        self.cast.clean()
        self.genre.clean()
        self.country.clean()
        DBDataWrite.delete(self)

    def open(self,mode='r',nooverwrite=False):
        """Video.open(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('filename',mode,nooverwrite)

    def openBanner(self,mode='r',nooverwrite=False):
        """Video.openBanner(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('banner',mode,nooverwrite)

    def openCoverart(self,mode='r',nooverwrite=False):
        """Video.openCoverart(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('coverfile',mode,nooverwrite)

    def openFanart(self,mode='r',nooverwrite=False):
        """Video.openFanart(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('fanart',mode,nooverwrite)

    def openScreenshot(self,mode='r',nooverwrite=False):
        """Video.openScreenshot(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('screenshot',mode,nooverwrite)

    def openTrailer(self,mode='r',nooverwrite=False):
        """Video.openTrailer(mode='r', nooverwrite=False)
                                -> file or FileTransfer object"""
        return self._open('trailer',mode,nooverwrite)

    def getHash(self):
        """Video.getHash() -> file hash"""
        if self.host is None:
            return None
        be = FileOps(self.host)
        hash = be.getHash(self.filename, 'Videos')
        return hash

    def fromFilename(self, filename):
        if self.wheredat is not None:
            return self
        self.filename = filename
        filename = filename[:filename.rindex('.')]
        for old in ('%20','_','.'):
            filename = filename.replace(old, ' ')

        sep = '(?:\s?(?:-|/)?\s?)?'
        regex1 = re.compile('^(.*[^s0-9])'+sep \
                           +'(?:s|(?:Season))?'+sep \
                           +'(\d{1,4})'+sep \
                           +'(?:[ex/]|Episode)'+sep \
                           +'(\d{1,3})'+sep \
                           +'(.*)$', re.I)

        regex2 = re.compile('(%s(?:Season%s\d*%s)*%s)$' \
                            % (sep, sep, sep, sep), re.I)

        match1 = regex1.search(filename)
        if match1:
            self.season = int(match1.group(2))
            self.episode = int(match1.group(3))
            self.subtitle = match1.group(4)

            title = match1.group(1)
            match2 = regex2.search(title)
            if match2:
                title = title[:match2.start()]
            self.title = title[title.rindex('/')+1:]
        else:
            title = filename[filename.rindex('/')+1:]
            for left,right in (('(',')'), ('[',']'), ('{','}')):
                while left in title:
                    lin = title.index(left)
                    rin = title.index(right,lin)
                    title = title[:lin]+title[rin+1:]
            self.title = title

        return self
        

class VideoGrabber( Grabber ):
    """
    VideoGrabber(mode, lang='en', db=None) -> VideoGrabber object
            'mode' can be of either 'TV' or 'Movie'
    """
    logmodule = 'Python MythVideo Grabber'

    def __init__(self, mode, lang='en', db=None):
        if mode == 'TV':
            Grabber.__init__(self, setting='mythvideo.TVGrabber', db=db)
        elif mode == 'Movie':
            Grabber.__init__(self, setting='mythvideo.MovieGrabber', db=db)
        else:
            raise MythError('Invalid MythVideo grabber')
        self._check_schema('mythvideo.DBSchemaVer',
                                MVSCHEMA_VERSION, 'MythVideo')
        self.append('-l',lang)
        self.override = {}

    def setOverride(self, data):
        # specify overrides for seachTitle function
        self.override.update(data)

    def searchTitle(self, title, year=None):
        """
        VideoGrabber.searchTitle(title, year=None)
                            -> tuple of tuples of (inetref, title, year)
        """
        if title.lower() in self.override:
            return ((self.override[title.lower()], title, None),)
        regex = re.compile('([0-9]+):(.+?)( \(([0-9]{4})\)\n|\n)')
        res = self.command('-M', '"%s"' %title)
        ret = []
        for m in regex.finditer(res):
            m = list(m.group(1,2,4))
            m[0] = int(m[0])
            if m[2]:
                m[2] = int(m[2])
            if year and m[2]:
                if m[2] != int(year):
                    continue
            ret.append(tuple(m))
        return ret

    def searchEpisode(self, title, subtitle):
        """
        VideoGrabber.searchEpisode(title, subtitle) -> (season, episode)
        """
        regex = re.compile("S(?P<season>[0-9]*)E(?P<episode>[0-9]*)")
        res = self.command('-N', '"%s"' % title, '"%s"' % subtitle)
        match = regex.match(res)
        if match:
            season = int(match.group('season'))
            episode = int(match.group('episode'))
            return (season, episode)
        else:
            return (None, None)

    def getData(self, inetref, season=None, episode=None, additional=False):
        """
        VideoGrabber.getData(inetref, season=None, episode=None, additional=False)
                    -> (data, cast, genre, country)
                    -> (data, cast, genre, country, additional)
                'season' and 'episode' are used only for TV grabbers
                'data' is a dictionary containing data matching the fields in a
                            Video object
                'cast', 'genre', and 'country' are tuples containing such
                'additional' is an optional response with any extra data
        """
        if season and episode:
            res = self.command('-D', inetref, season, episode)
        else:
            res = self.command('-D', str(inetref) )
        trans = {   'Title':'title',        'Subtitle':'subtitle',
                    'Year':'year',          'ReleaseDate':'releasedate',
                    'InetRef':'inetref',    'URL':'homepage',
                    'Director':'director',  'Plot':'plot',
                    'UserRating':'stars',   'MovieRating':'rating',
                    'Runtime':'length',     'Season':'season',
                    'Episode':'episode',    'Seriesid':'inetref',
                    'Coverart':'coverfile', 'Fanart':'fanart',
                    'Banner':'banner',      'Screenshot':'screenshot'}
        dat = {}
        cast = ()
        genre = ()
        country = ()
        adddict = {}
        for point in res.split('\n')[:-1]:
            if point.find(':') == -1:
                continue
            key,val = point.split(':',1)
            if key in trans:
                dat[trans[key]] = val
            elif key == 'Cast':
                cast = val.split(', ')
            elif key == 'Genres':
                genre = val.split(', ')
            elif key == 'Countries':
                country = val.split(', ')
            else:
                adddict[key] = val
        if 'releasedate' in dat:
            dat['releasedate'] = datetime.strptime(dat['releasedate'],\
                                                '%Y-%m-%d').date()
        if additional:
            return (dat, cast, genre, country, adddict)
        return (dat, cast, genre, country)

#### MYTHNETVISION ####

class NetVisionRSSItem( DBData ):
    """
    Represents a single program from the netvisionrssitems table
    """
    table = 'netvisionrssitems'
    where = 'feedtitle=%s AND title=%s'
    setwheredat = 'self.feedtitle,self.title'
    schema_value = 'NetvisionDBSchemaVer'
    schema_local = NVSCHEMA_VERSION
    schema_name = 'NetVision'
    
    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized NetVisionRSSItem at %s>" % hex(id(self))
        return u"<NetVisionRSSItem '%s@%s' at %s>" % \
                    (self.title, self.feedtitle, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

class NetVisionTreeItem( DBData ):
    """
    Represents a single program from the netvisiontreeitems table
    """
    table = 'netvisiontreeitems'
    where = 'feedtitle=%s AND path=%s'
    setwheredat = 'self.feedtitle,self.path'
    schema_value = 'NetvisionDBSchemaVer'
    schema_local = NVSCHEMA_VERSION
    schema_name = 'NetVision'

    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized NetVisionTreeItem at %s>" % hex(id(self))
        return u"<NetVisionTreeItem '%s@%s' at %s>" % \
                    (self.path, self.feedtitle, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

class NetVisionSite( DBData ):
    """
    Represents a single site from the netvisionsites table
    """
    table = 'netvisionsites'
    where = 'name=%'
    setwheredat = 'name,'
    schema_value = 'NetvisionDBSchemaVer'
    schema_local = NVSCHEMA_VERSION
    schema_name = 'NetVision'

    def __str__(self):
        if self.wheredat is None:
            return u"<Uninitialized NetVisionSite at %s>" % hex(id(self))
        return u"<NetVisionSite '%s','%s' at %s>" % \
                    (self.name, self.url, hex(id(self)))

    def __repr__(self):
        return str(self).encode('utf-8')

class NetVisionGrabber( Grabber ):
    logmodule = 'Python MythNetVision Grabber'

    @staticmethod
    def grabberList(types='search,tree', db=None):
        db = MythDBBase(db)
        db._check_schema('NetvisionDBSchemaVer',
                                NVSCHEMA_VERSION, 'NetVision')
        c = db.cursor(self.log)
        log = MythLog('Python MythNetVision Grabber', db=db)
        host = gethostname()
        glist = []
        for t in types.split(','):
            c.execute("""SELECT name,commandline
                        FROM netvision%sgrabbers
                        WHERE host=%%s""" % t, (host,))
            for name,commandline in c.fetchall():
                glist.append(NetVisionGrabber(name, t, db, commandline))
        c.close()
        return glist

    def __init__(self, name, type, db=None, commandline=None):
        if type not in ('search','tree'):
            raise MythError('Invalid NetVisionGrabber() type')
        self.type = type
        self.name = name
        if commandline:
            Grabber.__init__(path=commandline, db=db)
        else:
            db = MythDBBase(db)
            self.log = MythLog(self.logmodule, db=self)
            if c.execute("""SELECT commandline
                            FROM netvision%sgrabbers
                            WHERE name=%%s AND host=%%s""" % type,
                                        (name, gethostname())) == 1:
                Grabber.__init__(path=c.fetchone()[0], db=db)
                c.close()
            else:
                c.close()
                raise MythError('NetVisionGrabber not found in database')

    def searchXML(self, title, page=1):
        return etree.fromstring(\
                        self.command('-p',page,'-S','"%s"' %title)).getroot()

    def treeXML(self):
        return etree.fromstring(self.command('-T'))

    def setUpdated(self):
        if self.type is not 'tree':
            raise MythError('Can only update tree-type grabbers')
        c = db.cursor(self.log)
        c.execute("""UPDATE netvision%sgrabbers SET update=NOW()
                     WHERE name=%%s AND host=%%s""" % type,
                     (self.name, gethostname()))
        c.close()


