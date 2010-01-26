# -*- coding: utf-8 -*-

"""
Provides base classes for accessing MythTV
"""

from MythStatic import *
from MythBase import *
from MythData import *

import os, re, socket


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
                continue
            where.append(res[0])
            fields.append(res[1])
            joinbit = joinbit|res[2]
        for key in self.require:
            if key not in kwargs:
                res = self.func(self.inst, key=key)
                if res is None:
                    continue
                where.append(res[0])
                fields.append(res[1])
                joinbit = joinbit|res[2]

        # build joins
        joins = []
        for i in range(len(self.joins)):
            if (2**i)&joinbit:
                if len(self.joins[i]) == 3:
                    on = ' AND '.join(['%s.%s=%s.%s' % \
                            (self.joins[i][0], field, self.joins[i][1], field)\
                            for field in self.joins[i][2]])
                else:
                    on = ' AND '.join(['%s.%s=%s.%s' % \
                        (self.joins[i][0], ffield, self.joins[i][1], tfield)\
                        for ffield, tfield in \
                                    zip(self.joins[i][2],self.joins[i][3])])
                joins.append('JOIN %s ON %s' % (self.joins[i][0], on))
        joins = ' '.join(joins)

        # build where
        where = ' AND '.join(where)

        # build query
        if len(where) > 0:
            query = """SELECT %s.* FROM %s %s WHERE %s""" \
                                    % (self.table, self.table, joins, where)
        else:
            query = """SELECT %s.* FROM %s""" % (self.table, self.table)

        # process query
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
            return tuple(records)
        else:
            return None


class MythBE( FileOps ):
    __doc__ = FileOps.__doc__+"""
    Includes several canned file backend functions.
    """

    locked_tuners = []

    def __del__(self):
        self.freeTuner()
        MythBEConn.__del__(self)

    def getPendingRecordings(self):
        """
        Returns a list of Program objects which are scheduled to be recorded.
        """
        programs = []
        res = self.backendCommand('QUERY_GETALLPENDING').split(BACKEND_SEP)
        has_conflict = int(res.pop(0))
        num_progs = int(res.pop(0))
        for i in range(num_progs):
            programs.append(Program(res[i * PROGRAM_FIELDS:(i * PROGRAM_FIELDS)
                + PROGRAM_FIELDS], db=self.db))
        return tuple(programs)

    def getScheduledRecordings(self):
        """
        Returns a list of Program objects which are scheduled to be recorded.
        """
        programs = []
        res = self.backendCommand('QUERY_GETALLSCHEDULED').split(BACKEND_SEP)
        num_progs = int(res.pop(0))
        for i in range(num_progs):
            programs.append(Program(res[i * PROGRAM_FIELDS:(i * PROGRAM_FIELDS)
                + PROGRAM_FIELDS], db=self.db))
        return tuple(programs)

    def getUpcomingRecordings(self):
        """
        Returns a list of Program objects which are scheduled to be recorded.

        Sorts the list by recording start time and only returns those with
        record status of WillRecord.
        """
        def sort_programs_by_starttime(x, y):
            if x.starttime > y.starttime:
                return 1
            elif x.starttime == y.starttime:
                return 0
            else:
                return -1
        programs = []
        res = self.getPendingRecordings()
        for p in res:
            if p.recstatus == p.WILLRECORD:
                programs.append(p)
        programs.sort(sort_programs_by_starttime)
        return tuple(programs)

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
        return tuple(res)


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
        programs = []
        res = self.backendCommand('QUERY_RECORDINGS Play').split(BACKEND_SEP)
        num_progs = int(res.pop(0))
        for i in range(num_progs):
            programs.append(Program(res[i * PROGRAM_FIELDS:(i*PROGRAM_FIELDS)
                + PROGRAM_FIELDS], db=self.db))
        return tuple(programs)

    def getExpiring(self):
        """
        Returns a tuple of all Program objects nearing expiration
        """
        programs = []
        res = self.backendCommand('QUERY_GETEXPIRING').split(BACKEND_SEP)
        num_progs = int(res.pop(0))
        for i in range(num_progs):
            programs.append(Program(res[i * PROGRAM_FIELDS:(i*PROGRAM_FIELDS)
                + PROGRAM_FIELDS], db=self.db))
        return tuple(programs)

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
            dirs.append(res[i*10:i*10+18])
        return tuple(dirs)

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

    def walkSG(self, host, sg, top):
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
        if top == '/': top = ''
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
            return tuple(files)
        elif path == '':
            return tuple(dirs)
        else:
            return (tuple(dirs),tuple(files),tuple(sizes))

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

class MythTV( MythBE ):
    # renamed to MythBE()
    pass


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
        return tuple(re.findall('(\w+)[ ]+- ([\w /,]+)',self.recv()))

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
        return tuple(self.recv().split('\r\n')[4].split(', '))

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
        return tuple(re.findall('query ([\w ]*\w+)[ \r\n]+- ([\w /,]+)',
                        self.recv()))

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
        return tuple(re.findall('play ([\w -:]*\w+)[ \r\n]+- ([\w /:,\(\)]+)',
                        self.recv()))

class MythDB( MythDBConn ):
    """
    Several canned queries for general purpose access to the database
    """

    @databaseSearch
    def searchRecorded(self, init=False, key=None, value=None):
        """
        Tries to find recordings matching the given information.
        Returns a tuple of Recorded objects
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
                        'transcoded','watched','storagegroup'):
            return ('recorded.%s=%%s' % key, value, 0)

        # recordedprogram matches
        if key in ('airdate','stereo','subtitled','hdtv','closecaptioned',
                    'partnumber','parttotal','seriesid','showtype',
                    'syndicatedepisodenumber','programid','manualid','generic'):
            return ('recordedprogram.%s=%%s' % key, value, 1)

        if key == 'cast':
            return ('people.name=%s', value, 2|4)

        if key == 'livetv':
            if value is None:
                return ('recorded.recgroup!=%s', 'LiveTV', 0)

        return None

    @databaseSearch
    def searchOldRecorded(self, init=False, key=None, value=None):
        """
        Tries to find old recordings matching the given information.
        Returns a tuple of OldRecorded objects.
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
        Tries to find jobs matching the given information.
        Returns a tuple of Job objects.
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
        Tries to find guide matching the given information.
        Returns a tuple of Guide objects.
        """
        if init:
            return ('program', Guide, ())
        if key in ('chanid','starttime','endtime','title','subtitle',
                        'category','airdate','stars','previouslyshown','stereo',
                        'subtitled','hdtv','closecaptioned','partnumber',
                        'parttotal','seriesid','originalairdate','showtype',
                        'syndicatedepisodenumber','programid','generic'):
            return ('%s=%%s' % key, value, 0)
        if key == 'startbefore':
            return ('starttime<%s', value, 0)
        if key == 'startafter':
            return ('starttime>%s', value, 0)
        if key == 'endbefore':
            return ('endtime<%s', value, 0)
        if key == 'endafter':
            return ('endtime>%s', value, 0)
        return None

    def getFrontends(self):
        """
        Returns a list of Frontend objects for accessible frontends
        """
        cursor = self.db.cursor(self.log)
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
        c = self.db.cursor(self.log)
        c.execute("""SELECT * FROM channel""")
        for row in c.fetchall():
            channels.append(Channel(db=self, raw=row))
        c.close()
        return tuple(channels)

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

class MythVideo( MythDBConn ):
    """
    Provides convenient methods to access the MythTV MythVideo database.
    """
    def __init__(self,db=None):
        """
        Initialise the MythDB connection.
        """
        MythDBConn.__init__(self,db)

        # check schema version
        self._check_schema('mythvideo.DBSchemaVer', 
                                MVSCHEMA_VERSION, 'MythVideo')
        
    def scanStorageGroups(self, returnnew=True, deleteold=True):
        """
        Removes metadata from the database for files that no longer exist.
        Respects 'Videos' storage groups and relative paths.
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
                    newvidlist.append((hostname, tpath, thash))

        curvids = curvids.values()
        if returnnew:
            res = tuple(newvidlist)
        else:
            res = None
        if deleteold:
            for vid in curvids:
                #print 'deleting '+str(vid)
                vid.delete()
        else:
            if res:
                res = (res, tuple(curvids))
            else:
                res = curvids
        return res

    @databaseSearch
    def searchVideos(self, init=False, key=None, value=None):
        """
        Tries to find videos matching the given information.
        Returns a tuple of Video objects.
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
        vidref = {'cast':1|2, 'genre':4|8, 'country':16|32, 'category':64}
        if key in vidref:
            return ('video%s.%s=%%s' % (key, key), value, vidref[key])
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
                return tuple(res)
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
      
