# -*- coding: utf-8 -*-

"""
Provides base classes for accessing MythTV
"""

from MythStatic import *
from MythBase import *
from MythData import *

import os
import re
import socket
from datetime import datetime


class databaseSearch( object ):
    # decorator class for database searches
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
            if res is None:
                raise TypeError("%s got an unexpected keyword arguemnt '%s'" \
                                    % (self.__name__, key))

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
        c = self.inst.cursor(self.inst.log)
        if len(where) > 0:
            c.execute(query, fields)
        else:
            c.execute(query)
        rows = c.fetchall()
        c.close()

        if len(rows) == 0:
            return None
        records = []
        for row in rows:
            records.append(self.dbclass(db=self.inst, raw=row))
        if len(records):
            return records
        else:
            return None

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


class MythBE( FileOps ):
    __doc__ = FileOps.__doc__+"""
        getPendingRecordings()    - returns a list of scheduled recordings
        getScheduledRecordings()  - returns a list of scheduled recordings
        getUpcomingRecordings()   - returns a list of scheduled recordings
        getConflictedRecordings() - returns a list of conflicting recordings
        getRecorderList()         - returns a list of all recorder ids
        getFreeRecorderList()     - returns a list of free recorder ids
        lockTuner()               - requests a lock of a recorder
        freeTuner()               - requests an unlock of a recorder
        getCheckfile()            - returns the location of a recording
        getExpiring()             - returns a list of expiring recordings
        getFreeSpace()            - returns a list of FreeSpace objects
        getFreeSpaceSummary()     - returns a tuple of total and used space
        getLastGuideData()        - returns the last date of guide data
        getLoad()                 - returns a tuple of load averages
        getRecordings()           - returns a list of all recordings
        getSGFile()               - returns information on a single file
        getSGList()               - returns lists of directories, 
                                    files, and sizes
        getUptime()               - returns system uptime in seconds
        isActiveBackend()         - determines whether backend is
                                    currently active
        isRecording               - determinds whether recorder is
                                    currently recording
        walkSG()                  - walks a storage group tree, similarly
                                    to os.walk(). returns a tuple of dirnames
                                    and dictionary of filenames with sizes
    """

    locked_tuners = []

    def __del__(self):
        self.freeTuner()

    def _getPrograms(self, query, recstatus=None, header=0):
        programs = []
        res = self.backendCommand(query).split(BACKEND_SEP)
        for i in range(header):
            res.pop(0)

        num_progs = int(res.pop(0))
        for i in range(num_progs):
            offs = i * PROGRAM_FIELDS
            programs.append(Program(res[offs:offs+PROGRAM_FIELDS], db=self.db))

        if recstatus:
            for i in reversed(range(num_progs)):
                if programs[i].recstatus != recstatus:
                    programs.pop(i)

        return sorted(programs, key=lambda p: p.starttime)
        

    def getPendingRecordings(self):
        """
        Returns a list of Program objects which are scheduled to be recorded.
        """
        return self._getPrograms('QUERY_GETALLPENDING', header=1)

    def getScheduledRecordings(self):
        """
        Returns a list of Program objects which are scheduled to be recorded.
        """
        return self._getPrograms('QUERY_GETALLSCHEDULED')

    def getUpcomingRecordings(self):
        """
        Returns a list of Program objects which are scheduled to be recorded.

        Sorts the list by recording start time and only returns those with
        record status of WillRecord.
        """
        return self._getPrograms('QUERY_GETALLPENDING', \
                                recstatus=Program.WILLRECORD, header=1)

    def getConflictedRecordings(self):
        """
        Retuns a list of Program objects subject to conflicts in the schedule.
        """
        return self._getPrograms('QUERY_GETALLPENDING', \
                                recstatus=Program.CONFLICT, header=1)

    def getRecorderList(self):
        """
        Returns a list of recorders, or an empty list if none.
        """
        recorders = []
        c = self.db.cursor(self.log)
        c.execute('SELECT cardid FROM capturecard')
        row = c.fetchone()
        while row is not None:
            recorders.append(int(row[0]))
            row = c.fetchone()
        c.close()
        return recorders

    def getFreeRecorderList(self):
        """
        Returns a list of free recorders, or an empty list if none.
        """
        res = self.backendCommand('GET_FREE_RECORDER_LIST').split(BACKEND_SEP)
        recorders = [int(d) for d in res]
        return recorders

    def lockTuner(self,id=None):
        """
        Request a tuner be locked from use, optionally specifying which tuner
        Returns a tuple of ID, video dev node, audio dev node, vbi dev node
        Returns an ID of -2 if tuner is locked
                         -1 if no tuner could be found
        """
        local = True
        cmd = 'LOCK_TUNER'
        if id is not None:
            cmd += ' %d' % id
            res = self.getRecorderDetails(id).hostname
            if res != socket.gethostname():
                local = False

        res = ''
        if local:
            res = self.backendCommand(cmd).split(BACKEND_SEP)
        else:
            myth = MythTV(res)
            res = myth.backendCommand(cmd).split(BACKEND_SEP)
            myth.close()
        res[0] = int(res[0])
        if res[0] > 0:
            self.locked_tuners.append(res[0])
            return tuple(res[1:])
        return res[0]


    def freeTuner(self,id=None):
        """
        Frees a requested tuner ID
        If no ID given, free all tuners listed as used by this class instance
        """
        def free(self,id):
            res = self.getRecorderDetails(id).hostname
            if res == socket.gethostname():
                self.backendCommand('FREE_TUNER %d' % id)
            else:
                myth = MythTV(res)
                myth.backendCommand('FREE_TUNER %d' % id)
                myth.close()

        if id is None:
            for i in range(len(self.locked_tuners)):
                free(self,self.locked_tuners.pop())
        else:
            try:
                self.locked_tuners.remove(id)
            except:
                pass
            free(self,id)

    def getCurrentRecording(self, recorder):
        """
        Returns a Program object for the current recorders recording.
        """
        res = self.backendCommand('QUERY_RECORDER '+
                BACKEND_SEP.join([str(recorder),'GET_CURRENT_RECORDING']))
        return Program(res.split(BACKEND_SEP), db=self.db)

    def isRecording(self, recorder):
        """
        Returns a boolean as to whether the given recorder is recording.
        """
        res = self.backendCommand('QUERY_RECORDER '+
                BACKEND_SEP.join([str(recorder),'IS_RECORDING']))
        if res == '1':
            return True
        else:
            return False

    def isActiveBackend(self, hostname):
        """
        Returns a boolean as to whether the given host is an active backend
        """
        res = self.backendCommand(BACKEND_SEP.join(\
                    ['QUERY_IS_ACTIVE_BACKEND',hostname]))
        if res == 'TRUE':
            return True
        else:
            return False

    def getRecording(self, chanid, starttime):
        """
        Returns a Program object matching the channel id and start time
        """
        res = self.backendCommand('QUERY_RECORDING TIMESLOT %d %d' \
                        % (chanid, starttime)).split(BACKEND_SEP)
        if res[0] == 'ERROR':
            return None
        else:
            return Program(res[1:], db=self.db)

    def getRecordings(self):
        """
        Returns a list of all Program objects which have already recorded
        """
        return self._getPrograms('QUERY_RECORDINGS Play')

    def getExpiring(self):
        """
        Returns a tuple of all Program objects nearing expiration
        """
        return self._getPrograms('QUERY_GETEXPIRING')

    def getCheckfile(self,program):
        """
        Returns location of recording in file system
        """
        res = self.backendCommand(BACKEND_SEP.join(\
                        ['QUERY_CHECKFILE','1',program.toString()]\
                    )).split(BACKEND_SEP)
        if res[0] == 0:
            return None
        else:
            return res[1]

    def getFreeSpace(self,all=False):
        """
        Returns a tuple of FreeSpace objects
        """
        command = 'QUERY_FREE_SPACE'
        if all:
            command = 'QUERY_FREE_SPACE_LIST'
        res = self.backendCommand(command).split(BACKEND_SEP)
        dirs = []
        for i in range(0,len(res)/10):
            dirs.append(FreeSpace(res[i*10:i*10+10]))
        return dirs

    def getFreeSpaceSummary(self):
        """
        Returns a tuple of total space (in KB) and used space (in KB)
        """
        res = self.backendCommand('QUERY_FREE_SPACE_SUMMARY')\
                    .split(BACKEND_SEP)
        return (self.joinInt(int(res[0]),int(res[1])),
                self.joinInt(int(res[2]),int(res[3])))

    def getLoad(self):
        """
        Returns a tuple of the 1, 5, and 15 minute load averages
        """
        res = self.backendCommand('QUERY_LOAD').split(BACKEND_SEP)
        return (float(res[0]),float(res[1]),float(res[2]))

    def getUptime(self):
        """
        Returns machine uptime in seconds
        """
        return self.backendCommand('QUERY_UPTIME')

    def walkSG(self, host, sg, top=None):
        """
        Performs similar to os.walk(), returning a tuple of tuples containing
            (dirpath, dirnames, filenames).
        'dirnames' is a tuple of all directories at the current level.
        'filenames' is a dictionary, where the values are the file sizes.
        """
        def walk(self, host, sg, root, path):
            dn, fn, fs = self.getSGList(host, sg, root+path+'/')
            res = [list(dn), dict(zip(fn, fs))]
            if path == '':
                res = {'/':res}
            else:
                res = {path:res}
            for d in dn:
                res.update(walk(self, host, sg, root, path+'/'+d))
            return res

        bases = self.getSGList(host, sg, '')
        if (top == '/') or (top is None): top = ''
        walked = {}
        for base in bases:
            res = walk(self, host, sg, base, top)
            for d in res:
                if d in walked:
                    walked[d][0] += res[d][0]
                    walked[d][1].update(res[d][1])
                else:
                    walked[d] = res[d]

        res = []
        for key, val in walked.iteritems():
            res.append((key, tuple(val[0]), val[1]))
        res.sort()
        return res

    def getSGList(self,host,sg,path,filenamesonly=False):
        """
        Returns a tuple of directories, files, and filesizes.
        Two special modes
            'filenamesonly' returns only filenames, no directories or size
            empty path base directories returns base storage group paths
        """
        if filenamesonly: path = '' # force empty path
        res = self.backendCommand(BACKEND_SEP.join(\
                    ['QUERY_SG_GETFILELIST',host,sg,path,
                    str(int(filenamesonly))]\
                )).split(BACKEND_SEP)
        if res[0] == 'EMPTY LIST':
            return -1
        if res[0] == 'SLAVE UNREACHABLE: ':
            return -2
        dirs = []
        files = []
        sizes = []
        for entry in res:
            if filenamesonly:
                files.append(entry)
            elif path == '':
                type,name = entry.split('::')
                dirs.append(name)
            else:
                type,name,size = entry.split('::')
                if type == 'file':
                    files.append(name)
                    sizes.append(size)
                if type == 'dir':
                    dirs.append(name)
        if filenamesonly:
            return files
        elif path == '':
            return dirs
        else:
            return (dirs, files, sizes)

    def getSGFile(self,host,sg,path):
        """
        Returns a tuple of last modification time and file size
        """
        res = self.backendCommand(BACKEND_SEP.join(\
                ['QUERY_SG_FILEQUERY',host,sg,path])).split(BACKEND_SEP)
        if res[0] == 'EMPTY LIST':
            return -1
        if res[0] == 'SLAVE UNREACHABLE: ':
            return -2
        return tuple(res[1:3])

    def getLastGuideData(self):
        """
        Returns the last dat for which guide data is available
        On error, 0000-00-00 00:00 is returned
        """
        return self.backendCommand('QUERY_GUIDEDATATHROUGH')

class BEEventMonitor( BEEvent ):
    def _listhandlers(self):
        return [self.eventMonitor]

    def eventMonitor(self, event=None):
        if event is None:
            return re.compile('BACKEND_MESSAGE')
        self.log(MythLog.ALL, event)

class MythSystemEvent( BEEvent ):
    class systemeventhandler( object ):
        # decorator class for system events
        def __init__(self, func):
            self.func = func
            self.__doc__ = self.func.__doc__
            self.__name__ = self.func.__name__
            self.__module__ = self.func.__module__

            bs = BACKEND_SEP.replace('[','\[').replace(']','\]')

            self.re_search = re.compile('BACKEND_MESSAGE%sSYSTEM_EVENT %s' %\
                                (bs, self.__name__.upper()))
            self.re_process = re.compile(bs.join([
                'BACKEND_MESSAGE',
                'SYSTEM_EVENT (?P<event>[A-Z_]*)'
                    '( HOSTNAME (?P<hostname>[a-zA-Z0-9_\.]*))?'
                    '( SENDER (?P<sender>[a-zA-Z0-9_\.]*))?'
                    '( CARDID (?P<cardid>[0-9]*))?'
                    '( CHANID (?P<chanid>[0-9]*))?'
                    '( STARTTIME (?P<starttime>[0-9-]*T[0-9-]))?'
                    '( SECS (?P<secs>[0-9]*))?',
                'empty']))
        def __get__(self, inst, own):
            self.inst = inst
            return self
        def __call__(self, event=None):
            if event is None:
                return self.re_search
            match = self.re_process.match(event)
            event = {}
            for a in ('event','hostname','sender','cardid','secs'):
                if match.group(a) is not None:
                    event[a] = match.group(a)
            if (match.group('chanid') is not None) and \
               (match.group('starttime') is not None):
                    be = MythBE(self.inst.hostname)
                    event['program'] = be.getRecording(\
                          (match.group('chanid'),match.group('starttime')))
            self.func(self.inst, event)

    def _listhandlers(self):
        return [self.client_connected, self.client_disconnected,
                self.rec_pending, self.rec_started, self.rec_finished,
                self.rec_deleted, self.rec_expired, self.livetv_started,
                self.play_started, self.play_paused, self.play_stopped,
                self.play_unpaused, self.play_changed, self.master_started,
                self.master_shutdown, self.slave_connected,
                self.slave_disconnected, self.net_ctrl_connected,
                self.net_ctrl_disconnected, self.mythfilldatabase_ran,
                self.scheduler_ran, self.settings_cache_cleared, self.user_1,
                self.user_2, self.user_3, self.user_4, self.user_5,
                self.user_6, self.user_7, self.user_8, self.user_9]

    def __init__(self, backend=None, noshutdown=False, generalevents=False, \
                       db=None, opts=None):
        if opts is None:
            opts = BEConnection.BEConnOpts(noshutdown,\
                             True, generalevents)
        BEEvent.__init__(self, backend, db=db, opts=opts)

    @systemeventhandler
    def client_connected(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def client_disconnected(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def rec_pending(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def rec_started(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def rec_finished(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def rec_deleted(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def rec_expired(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def livetv_started(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def play_started(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def play_stopped(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def play_paused(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def play_unpaused(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def play_changed(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def master_started(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def master_shutdown(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def slave_connected(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def slave_disconnected(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def net_ctrl_connected(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def net_ctrl_disconnected(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def mythfilldatabase_ran(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def scheduler_ran(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def settings_cache_cleared(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_1(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_2(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_3(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_4(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_5(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_6(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_7(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_8(self, event):
        SystemEvent(event['event'], self.db).command(event)
    @systemeventhandler
    def user_9(self, event):
        SystemEvent(event['event'], self.db).command(event)

class SystemEvent( System ):
    """
    SystemEvent(eventname, db=None) -> SystemEvent object

    External function handler for system event messages.
        'eventname' is the event name sent in the BACKEND_MESSAGE message.
    """
    def __init__(self, event, db=None):
        setting = 'EventCmd'+''.join(\
                            [e.capitalize() for e in event.split('_')])
        try:
            System.__init__(self, setting=setting, db=db)
        except MythError:
            # no event handler registered
            self.path = ''
        except:
            raise

    def command(self, eventdata):
        """
        obj.command(eventdata) -> output string

        Executes external command, substituting event information into the
            command string. If call exits with a code not 
            equal to 0, a MythError will be raised. The error code and
            stderr will be available in the exception and this object
            as attributes 'returncode' and 'stderr'.
        """
        if self.path is '':
            return
        cmd = self.path
        if 'program' in eventdata:
            cmd = eventdata['program'].formatJob(cmd)
        for a in ('sender','cardid','secs'):
            if a in eventdata:
                cmd = cmd.replace('%%%s%%' % a, eventdata[a])
        return self._runcmd(cmd)


class Frontend(object):
    isConnected = False
    socket = None
    host = None
    port = None

    def __init__(self, host, port):
        self.host = host
        self.port = int(port)
        self.connect()
        self.disconnect()

    def __del__(self):
        if self.isConnected:
            self.disconnect()

    def __repr__(self):
        return "<Frontend '%s@%d' at %s>" % (self.host,self.port,hex(id(self)))

    def __str__(self):
        return "%s@%d" % (self.host, self.port)

    def close(self): self.__del__()

    def connect(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.settimeout(10)
        try:
            self.socket.connect((self.host, self.port))
        except:
            raise MythFEError(MythError.FE_CONNECTION, self.host, self.port)
        if re.search("MythFrontend Network Control.*",self.recv()) is None:
            self.socket.close()
            self.socket = None
            raise MythFEError(MythError.FE_ANNOUNCE, self.host, self.port)
        self.isConnected = True

    def disconnect(self):
        self.send("exit")
        self.socket.close()
        self.socket = None
        self.isConnected = False

    def send(self,command):
        if not self.isConnected:
            self.connect()
        self.socket.send("%s\n" % command)

    def recv(self):
        curstr = ''
        prompt = re.compile('([\r\n.]*)\r\n# ')
        while not prompt.search(curstr):
            try:
                curstr += self.socket.recv(100)
            except socket.error:
                raise MythFEError(MythError.FE_CONNECTION, self.host, self.port)
            except KeyboardInterrupt:
                raise
        return prompt.split(curstr)[0]

    def sendJump(self,jumppoint):
        """
        Sends jumppoint to frontend
        """
        self.send("jump %s" % jumppoint)
        if self.recv() == 'OK':
            return 0
        else:
            return 1

    def getJump(self):
        """
        Returns a tuple containing available jumppoints
        """
        self.send("help jump")
        return re.findall('(\w+)[ ]+- ([\w /,]+)',self.recv())

    def sendKey(self,key):
        """
        Sends keycode to connected frontend
        """
        self.send("key %s" % key)
        if self.recv() == 'OK':
            return 0
        else:
            return 1

    def getKey(self):
        """
        Returns a tuple containing available special keys
        """
        self.send("help key")
        return self.recv().split('\r\n')[4].split(', ')

    def sendQuery(self,query):
        """
        Returns query from connected frontend
        """
        self.send("query %s" % query)
        return self.recv()

    def getQuery(self):
        """
        Returns a tuple containing available queries
        """
        self.send("help query")
        return re.findall('query ([\w ]*\w+)[ \r\n]+- ([\w /,]+)', self.recv())

    def sendPlay(self,play):
        """
        Send playback command to connected frontend
        """
        self.send("play %s" % play)
        if self.recv() == 'OK':
            return 0
        else:
            return 1

    def getPlay(self):
        """
        Returns a tuple containing available playback commands
        """
        self.send("help play")
        return re.findall('play ([\w -:]*\w+)[ \r\n]+- ([\w /:,\(\)]+)',
                        self.recv())

class MythDB( DBCache ):
    __doc__ = DBCache.__doc__+"""
        obj.searchRecorded()    - return a list of matching Recorded objects
        obj.getRecorded()       - return a single matching Recorded object
        obj.searchOldRecorded() - return a list of matching
                                  OldRecorded objects
        obj.searchJobs()        - return a list of matching Job objects
        obj.searchGuide()       - return a list of matching Guide objects
        obj.searchRecord()      - return a list of matching Record rules
        obj.getFrontends()      - return a list of available Frontends
        obj.getFrontend()       - return a single Frontend object
        obj.getChannels()       - return a list of all channels
    """

    @databaseSearch
    def searchRecorded(self, init=False, key=None, value=None):
        """
        obj.searchRecorded(**kwargs) -> list of Recorded objects

        Supports the following keywords:
            title,      subtitle,   chanid,     starttime,  progstart,
            category,   hostname,   autoexpire, commflagged,
            stars,      recgroup,   playgroup,  duplicate,  transcoded,
            watched,    storagegroup,           category_type,
            airdate,    stereo,     subtitled,  hdtv,       closecaptioned,
            partnumber, parttotal,  seriesid,   showtype,   programid,
            manualid,   generic,    cast,       livetv,     basename,
            syndicatedepisodenumber
        """

        if init:
            # table and join descriptor
            return ('recorded', Recorded, ('livetv',),
                    ('recordedprogram','recorded',('chanid','starttime')),
                    ('recordedcredits','recorded',('chanid','starttime')),
                    ('people','recordedcredits',('person',)))

        # local table matches
        if key in ('title','subtitle','chanid','starttime','progstart',
                        'category','hostname','autoexpire','commflagged',
                        'stars','recgroup','playgroup','duplicate',
                        'transcoded','watched','storagegroup','basename'):
            return ('recorded.%s=%%s' % key, value, 0)

        # recordedprogram matches
        if key in ('category_type','airdate','stereo','subtitled','hdtv',
                    'closecaptioned','partnumber','parttotal','seriesid',
                    'showtype','syndicatedepisodenumber','programid',
                    'manualid','generic'):
            return ('recordedprogram.%s=%%s' % key, value, 1)

        if key == 'cast':
            return ('people.name', 'recordedcredits', 4, 1)

        if key == 'livetv':
            if value is None:
                return ('recorded.recgroup!=%s', 'LiveTV', 0)

        return None

    @databaseSearch
    def searchOldRecorded(self, init=False, key=None, value=None):
        """
        obj.searchOldRecorded(**kwargs) -> list of OldRecorded objects

        Supports the following keywords:
            title,      subtitle,   chanid,     starttime,  endtime,
            category,   seriesid,   programid,  station,    duplicate,
            generic
        """

        if init:
            return ('oldrecorded', OldRecorded, ())
        if key in ('title','subtitle','chanid','starttime','endtime',
                        'category','seriesid','programid','station',
                        'duplicate','generic'):
            return ('oldrecorded.%s=%%s' % key, value, 0)
        return None

    @databaseSearch
    def searchJobs(self, init=False, key=None, value=None):
        """
        obj.searchJobs(**kwars) -> list of Job objects

        Supports the following keywords:
            chanid,     starttime,  type,       status,     hostname,
            title,      subtitle,   flags,      olderthan,  newerthan
        """
        if init:
            return ('jobqueue', Job, (),
                    ('recorded','jobqueue',('chanid','starttime')))
        if key in ('chanid','starttime','type','status','hostname'):
            return ('jobqueue.%s=%%s' % key, value, 0)
        if key in ('title','subtitle'):
            return ('recorded.%s=%%s' % key, value, 1)
        if key == 'flags':
            return ('jobqueue.flags&%s', value, 0)
        if key == 'olderthan':
            return ('jobqueue.inserttime>%s', value, 0)
        if key == 'newerthan':
            return ('jobqueue.inserttime<%s', value, 0)
        return None

    @databaseSearch
    def searchGuide(self, init=False, key=None, value=None):
        """
        obj.searchGuide(**args) -> list of Guide objects

        Supports the following keywords:
            chanid,     starttime,  endtime,    title,      subtitle,
            category,   airdate,    stars,      previouslyshown,
            stereo,     subtitled,  hdtv,       closecaptioned,
            partnumber, parttotal,  seriesid,   originalairdate,
            showtype,   programid,  generic,    syndicatedepisodenumber
        """
        if init:
            return ('program', Guide, (),
                    ('credits','program',('chanid','starttime')),
                    ('people','credits',('person',)))
        if key in ('chanid','starttime','endtime','title','subtitle',
                        'category','airdate','stars','previouslyshown','stereo',
                        'subtitled','hdtv','closecaptioned','partnumber',
                        'parttotal','seriesid','originalairdate','showtype',
                        'syndicatedepisodenumber','programid','generic'):
            return ('%s=%%s' % key, value, 0)
        if key == 'cast':
            return ('people.name', 'credits', 2, 0)
        if key == 'startbefore':
            return ('starttime<%s', value, 0)
        if key == 'startafter':
            return ('starttime>%s', value, 0)
        if key == 'endbefore':
            return ('endtime<%s', value, 0)
        if key == 'endafter':
            return ('endtime>%s', value, 0)
        return None

    @databaseSearch
    def searchRecord(self, init=False, key=None, value=None):
        """
        obj.searchRecord(**kwargs) -> list of Record objects

        Supports the following keywords:
            type,       chanid,     starttime,  startdate,  endtime
            enddate,    title,      subtitle,   category,   profile
            recgroup,   station,    seriesid,   programid,  playgroup
        """
        if init:
            return ('record', Record, ())
        if key in ('type','chanid','starttime','startdate','endtime','enddate',
                        'title','subtitle','category','profile','recgroup',
                        'station','seriesid','programid','playgroup'):
            return ('%s=%%s' % key, value, 0)
        return None

    def getFrontends(self):
        """
        Returns a list of Frontend objects for accessible frontends
        """
        cursor = self.cursor(self.log)
        cursor.execute("""SELECT DISTINCT hostname
                          FROM settings
                          WHERE hostname IS NOT NULL
                          AND value='NetworkControlEnabled'
                          AND data=1""")
        frontends = []
        for fehost in cursor.fetchall():
            try:
                frontend = self.getFrontend(fehost[0])
                frontends.append(frontend)
            except:
                print "%s is not a valid frontend" % fehost[0]
        cursor.close()
        return frontends

    def getFrontend(self,host):
        """
        Returns a Frontend object for the specified host
        """
        port = self.settings[host].NetworkControlPort
        return Frontend(host,port)

    def getRecorded(self, title=None, subtitle=None, chanid=None,
                        starttime=None, progstart=None):
        """
        Tries to find a recording matching the given information.
        Returns a Recorded object.
        """
        records = self.searchRecorded(title=title, subtitle=subtitle,\
                            chanid=chanid, starttime=starttime,\
                            progstart=progstart)
        if records:
            return records[0]
        else:
            return None

    def getChannels(self):
        """
        Returns a tuple of channel object defined in the database
        """
        channels = []
        c = self.cursor(self.log)
        c.execute("""SELECT * FROM channel""")
        for row in c.fetchall():
            channels.append(Channel(db=self, raw=row))
        c.close()
        return channels

# wrappers for backwards compatibility of old functions

    def getChannel(self,chanid):
        return Channel(chanid, db=self)

    def getGuideData(self, chanid, date):
        return self.searchGuide(chanid=chanid, 
                                custom=('DATE(starttime)=%s',date))

    def getSetting(self, value, hostname=None):
        if not hostname:
            hostname = 'NULL'
        return self.settings[hostname][value]

    def setSetting(self, value, data, hostname=None):
        if not hostname:
            hostname = 'NULL'
        self.settings[hostname][value] = data

class MythXML( XMLConnection ):
    """
    Provides convenient methods to access the backend XML server.
    """
    def getServDesc(self):
        """
        Returns a dictionary of valid pages, as well as input and output
        arguments.
        """
        tree = self._queryTree('GetServDesc')
        acts = {}
        for act in tree.find('actionList').getchildren():
            actname = act.find('name').text
            acts[actname] = {'in':[], 'out':[]}
            for arg in act.find('argumentList').getchildren():
                argname = arg.find('name').text
                argdirec = arg.find('direction').text
                acts[actname][argdirec].append(argname)
        return acts

    def getHosts(self):
        """Returns a list of unique hostnames found in the settings table."""
        tree = self._queryTree('GetHosts')
        return [child.text for child in tree.find('Hosts').getchildren()]

    def getKeys(self):
        """Returns a list of unique keys found in the settings table."""
        tree = self._queryTree('GetKeys')
        return [child.text for child in tree.find('Keys').getchildren()]

    def getSetting(self, key, hostname=None, default=None):
        """Retrieves a setting from the backend."""
        args = {'Key':key}
        if hostname:
            args['HostName'] = hostname
        if default:
            args['Default'] = default
        tree = self._queryTree('GetSetting', **args)
        return tree.find('Values').find('Value').text

    def getProgramGuide(self, starttime, endtime, startchan=None, numchan=None):
        """
        Returns a list of Guide objects corresponding to the given time period.
        """
        args = {'StartTime':starttime, 'EndTime':endtime, 'Details':1}
        if startchan:
            args['StartChanId'] = startchan
            if numchan:
                args['NumOfChannels'] = numchan
            else:
                args['NumOfChannels'] = 1

        progs = []
        tree = self._queryTree('GetProgramGuide', **args)
        for chan in tree.find('ProgramGuide').find('Channels').getchildren():
            chanid = int(chan.attrib['chanId'])
            for guide in chan.getchildren():
                progs.append(Guide(db=self.db, etree=(chanid, guide)))
        return progs

    def getProgramDetails(self, chanid, starttime):
        """
        Returns a Program object for the matching show.
        """
        args = {'ChanId': chanid, 'StartTime':starttime}
        tree = self._queryTree('GetProgramDetails', **args)
        prog = tree.find('ProgramDetails').find('Program')
        return Program(db=self.db, etree=prog)

    def getChannelIcon(self, chanid):
        """Returns channel icon as a data string"""
        return self._query('GetChannelIcon', ChanId=chanid)

    def getRecorded(self, descending=True):
        """
        Returns a list of Program objects for recorded shows on the backend.
        """
        tree = self._queryTree('GetRecorded', Descending=descending)
        progs = []
        for prog in tree.find('Recorded').find('Programs').getchildren():
            progs.append(Program(db=self.db, etree=prog))
        return progs

    def getExpiring(self):
        """
        Returns a list of Program objects for expiring shows on the backend.
        """
        tree = self._queryTree('GetExpiring')
        progs = []
        for prog in tree.find('Expiring').find('Programs').getchildren():
            progs.append(Program(db=self.db, etree=prog))
        return progs

class MythVideo( DBCache ):
    """
    Provides convenient methods to access the MythTV MythVideo database.
    """
    def __init__(self,db=None):
        """
        Initialise the MythDB connection.
        """
        DBCache.__init__(self,db)

        # check schema version
        self._check_schema('mythvideo.DBSchemaVer', 
                                MVSCHEMA_VERSION, 'MythVideo')
        
    def scanStorageGroups(self, deleteold=True):
        """
        obj.scanStorageGroups(deleteold=True) ->
                            tuple of (new videos, missing videos)

        If deleteold is true, missing videos will be automatically
            deleted from videometadata, as well as any matching 
            country, cast, genre or markup data.
        """
        # pull available hosts and videos
        groups = self.getStorageGroup(groupname='Videos')
        curvids = self.searchVideos()
        newvids = {}

        # pull available types
        c = self.cursor(self.log)
        c.execute("""SELECT extension FROM videotypes WHERE f_ignore=False""")
        extensions = [a[0] for a in c.fetchall()]
        c.close()
        for i in range(len(extensions)):
            extensions[i] = extensions[i].lower()

        # filter local videos, only work on SG content
        for i in range(len(curvids)-1, -1, -1):
            if curvids[i].host in ('', None):
                del curvids[i]

        # index by file path
        curvids = dict([(curvids[i].filename, curvids[i]) \
                            for i in range(len(curvids))])

        # loop through all storage groups on all hosts
        for sg in groups:
            if sg.hostname not in newvids:
                newvids[sg.hostname] = []
            try:
                be = MythBE(backend=sg.hostname, db=self)
            except:
                # skip any offline backends
                curvids = curvids.values()
                for i in range(len(curvids)-1, -1, -1):
                    if curvids[i].host == sg.hostname:
                        del curvids[i]
                curvids = dict([(curvids[i].filename, curvids[i]) \
                                    for i in range(len(curvids))])
                continue
            folders = be.walkSG(sg.hostname, 'Videos', '/')
            # loop through each folder
            for sgfold in folders:
                # loop through each file
                for sgfile in sgfold[2]:
                    if sgfold[0] == '/':
                        tpath = sgfile
                    else:
                        tpath = sgfold[0][1:]+'/'+sgfile

                    # filter by extension
                    if tpath.rsplit('.',1)[1].lower() not in extensions:
                        #print 'skipping: '+tpath
                        continue

                    # existing file in existing location
                    if tpath in curvids:
                        #print 'matching file: '+tpath
                        del curvids[tpath]
                    else:
                        newvids[sg.hostname].append(tpath)

        # re-index by hash value
        curvids = curvids.values()
        curvids = dict([(curvids[i].hash, curvids[i]) \
                            for i in range(len(curvids))])
        # loop through unmatched videos for missing files
        newvidlist = []
        for hostname in newvids:
            be = MythBE(backend=hostname, db=self)
            for tpath in newvids[hostname]:
                thash = be.getHash(tpath, 'Videos')
                if thash in curvids:
                    #print 'matching hash: '+tpath
                    curvids[thash].update({'filename':tpath})
                    del curvids[thash]
                else:
                    vid = Video(db=self).fromFilename(tpath)
                    vid.host = hostname
                    vid.hash = thash
                    newvidlist.append(vid)

        if deleteold:
            for vid in curvids.values():
                vid.delete()

        return (newvidlist, curvids.values())

    @databaseSearch
    def searchVideos(self, init=False, key=None, value=None):
        """
        obj.searchVideos(**kwargs) -> list of Video objects

        Supports the following keywords:
            title, subtitle, season, episode, host, directory, year, cast,
            genre, country, category, insertedbefore, insertedafter, custom
        """

        if init:
            return ('videometadata', Video, (),
                    ('videometadatacast','videometadata',
                                            ('idvideo',),('intid',)),   #1
                    ('videocast','videometadatacast',
                                            ('intid',),('idcast',)),    #2
                    ('videometadatagenre','videometadata',
                                            ('idvideo',),('intid',)),   #4
                    ('videogenre','videometadatagenre',
                                            ('intid',),('idgenre',)),   #8
                    ('videometadatacountry','videometadata',
                                            ('idvideo',),('intid',)),   #16
                    ('videocountry','videometadatacountry',
                                            ('intid',),('idcountry',)), #32
                    ('videocategory','videometadata',
                                            ('intid',),('category',)))  #64
        if key in ('title','subtitle','season','episode','host',
                        'director','year'):
            return('videometadata.%s=%%s' % key, value, 0)
        vidref = {'cast':(2,0), 'genre':(8,2), 'country':(32,4)}
        if key in vidref:
            return ('video%s.%s' % (key,key),
                    'videometadata%s' % key,
                    vidref[key][0],
                    vidref[key][1])
        if key == 'category':
            return ('videocategory.%s=%%s' % key, value, 64)
        if key == 'exactfile':
            return ('videometadata.filename=%s', value, 0)
        if key == 'file':
            return ('videometadata.filename LIKE %s', '%'+value, 0)
        if key == 'insertedbefore':
            return ('videometadata.insertdate<%s', value, 0)
        if key == 'insertedafter':
            return ('videometadata.insertdate>%s', value, 0)
        return None

    def getVideo(self, **kwargs):
        """
        Tries to find a video matching the given information.
        Returns a Video object.
        """
        videos = self.searchVideos(**kwargs)
        if videos:
            return videos[0]
        else:
            return None

# wrappers for backwards compatibility of old functions
    def pruneMetadata(self):
        self.scanStorageGroups(returnnew=False)

    def getTitleId(self, title, subtitle=None, episode=None,
                            array=False, host=None):
        videos = self.searchVideos(title, subtitle, season, episode, host)
        if videos:
            if array:
                res = []
                for vid in videos:
                    res.append(vid.intid)
                return res
            else:
                return videos[0].intid
        else:
            return None

    def getMetadata(self, id):
        return list(Video(id=id, db=self))

    def getMetadataDictionary(self, id):
        return Video(id=id, db=self).data

    def setMetadata(self, data, id=None):
        if id is None:
            return Video(db=self).create(data)
        else:
            Video(id=id, db=self).update(data)

    def getMetadataId(self, videopath, host=None):
        video = self.getVideo(file=videopath, host=host)
        if video is None:
            return None
        else:
            return video.intid

    def hasMetadata(self, videopath, host=None):
        vid = self.getVideo(file=videopath, host=host, exactfile=True)
        if vid is None:
            return False
        elif vid.inetref == 0:
            return False
        else:
            return True

    def rmMetadata(self, file=None, id=None, host=None):
        if file:
            self.getVideo(file=file, host=host, exactfile=True).delete()
        elif id:
            Video(id=id, db=self).delete()

    def rtnVideoStorageGroup(self, host=None):
        return self.getStorageGroup(groupname='Videos',hostname=host)

    def getTableFieldNames(self, tablename):
        return self.tablefields[tablename]

    def setCast(self, cast_name, idvideo):
        Video(id=idvideo, db=self).cast.add(cast_name)
        # WARNING: no return value

    def setGenres(self, genre_name, idvideo):
        Video(id=idvideo, db=self).genre.add(genre_name)
        # WARNING: no return value

    def setCountry(self, country_name, idvideo):
        Video(id=idvideo, db=self).country.add(country_name)
        # WARNING: no return value

    def cleanGenres(self, idvideo):
        Video(id=idvideo, db=self).genre.clean()

    def cleanCountry(self, idvideo):
        Video(id=idvideo, db=self).country.clean()

    def cleanCast(self, idvideo):
        Video(id=idvideo, db=self).cast.clean()
      
