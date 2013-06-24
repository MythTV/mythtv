# -*- coding: utf-8 -*-

"""
Provides base classes for accessing MythTV
"""

from MythTV.static import *
from MythTV.exceptions import *
from MythTV.logging import MythLog
from MythTV.connections import FEConnection, XMLConnection, BEEventConnection
from MythTV.utility import databaseSearch, datetime, check_ipv6, _donothing
from MythTV.database import DBCache, DBData
from MythTV.system import SystemEvent
from MythTV.mythproto import BECache, FileOps, Program, FreeSpace, EventLock
from MythTV.dataheap import *

from datetime import timedelta
from weakref import proxy
from urllib import urlopen
import re

class CaptureCard( DBData ):
    pass

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
        isRecording()             - determinds whether recorder is
                                    currently recording
        walkSG()                  - walks a storage group tree, similarly
                                    to os.walk(). returns a tuple of dirnames
                                    and dictionary of filenames with sizes
    """

    locked_tuners = {}

    def __del__(self):
        self.freeTuner()

    @FileOps._ProgramQuery('QUERY_GETALLPENDING', header_length=1, sorted=True)
    def getPendingRecordings(self, pg):
        """
        Returns a list of Program objects which are scheduled to be recorded.
        """
        return pg

    @FileOps._ProgramQuery('QUERY_GETALLSCHEDULED', sorted=True)
    def getScheduledRecordings(self, pg):
        """
        Returns a list of Program objects which are scheduled to be recorded.
        """
        return pg

    @FileOps._ProgramQuery('QUERY_GETALLPENDING', header_length=1, sorted=True,
                           recstatus=Program.rsWillRecord)
    def getUpcomingRecordings(self, pg):
        """
        Returns a list of Program objects which are scheduled to be recorded.

        Sorts the list by recording start time and only returns those with
        record status of WillRecord.
        """
        return pg

    @FileOps._ProgramQuery('QUERY_GETALLPENDING', header_length=1, sorted=True,
                           recstatus=Program.rsConflict)
    def getConflictedRecordings(self, pg):
        """
        Retuns a list of Program objects subject to conflicts in the schedule.
        """
        return pg

    def getRecorderList(self):
        """
        Returns a list of recorders, or an empty list if none.
        """
        recorders = []
        with self.db.cursor(self.log) as cursor:
            cursor.execute('SELECT cardid FROM capturecard')
            for row in cursor:
                recorders.append(int(row[0]))
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
        cmd = 'LOCK_TUNER'
        be = self

        if id is not None:
            card = None
            try:
                # pull information from database to confirm existance
                card = CaptureCard(id)
            except MythError:
                raise MythError("Capture card %s not found" % id)

            cmd += ' %d' % id
            if card.hostname != be.hostname:
                # connect to slave backend if needed
                be = MythBE(card.hostname, db=self.db)

        res = be.backendCommand(cmd).split(BACKEND_SEP)
        err = int(res[0])

        if err > 0:
            # success, store tuner and return device nodes
            self.locked_tuners[err] = be.hostname
            return tuple(res[1:])

        # return failure mode
        return err

    def freeTuner(self,id=None):
        """
        Frees a requested tuner ID
        If no ID given, free all tuners listed as used by this class instance
        """
        tunerlist = {}

        if id is not None:
            id = int(id)
            if id in self.locked_tuners:
                # tuner is known, pop from list
                tunerlist[id] = self.locked_tuners.pop(id)

            else:
                # tuner is not known, find hostname
                try:
                    card = CaptureCard(id)
                except MythError:
                    raise MythError("Capture card %s not found" % id)
                tunerlist[id] = card.hostname
        else:
            # use the stored list
            tunerlist = self.locked_tuners

        hosts = {self.hostname:self}

        while True:
            try:
                # get a tuner
                id, host = tunerlist.popitem()

                # get the backend connection
                be = None
                if host in hosts:
                    be = hosts[host]
                else:
                    be = MythBE(host, db=self.db)
                    hosts[host] = be

                # unlock the tuner
                be.backendCommand('FREE_TUNER %d' % id)
            except KeyError:
                # out of tuners
                break

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

    @FileOps._ProgramQuery('QUERY_RECORDINGS Ascending', sorted=True)
    def getRecordings(self, pg):
        """
        Returns a list of all Program objects which have already recorded
        """
        return pg

    @FileOps._ProgramQuery('QUERY_GETEXPIRING', sorted=True)
    def getExpiring(self, pg):
        """
        Returns a tuple of all Program objects nearing expiration
        """
        return pg

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
        l = len(FreeSpace._field_order)
        for i in xrange(len(res)/l):
            yield FreeSpace(res[i*l:i*l+l])

    def getFreeSpaceSummary(self):
        """
        Returns a tuple of total space (in KB) and used space (in KB)
        """
        res = [int(r) for r in self.backendCommand('QUERY_FREE_SPACE_SUMMARY')\
                                   .split(BACKEND_SEP)]
        return (res[0], res[1])

    def getLoad(self):
        """
        Returns a tuple of the 1, 5, and 15 minute load averages
        """
        return [float(r) for r in self.backendCommand('QUERY_LOAD')\
                                      .split(BACKEND_SEP)]

    def getUptime(self):
        """
        Returns machine uptime in seconds
        """
        return timedelta(0, int(self.backendCommand('QUERY_UPTIME')))

    def walkSG(self, host, sg, top=None):
        """
        Performs similar to os.walk(), returning a tuple of tuples containing
            (dirpath, dirnames, filenames).
        'dirnames' is a tuple of all directories at the current level.
        'filenames' is a dictionary, where the values are the file sizes.
        """
        def walk(self, host, sg, root, path):
            res = self.getSGList(host, sg, root+path+'/')
            if res < 0:
                return {}
            dlist = list(res[0])
            res = [dlist, dict(zip(res[1], res[2]))]
            if path == '':
                res = {'/':res}
            else:
                res = {path:res}
            for d in dlist:
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
                se = entry.split('::')
                if se[0] == 'file':
                    files.append(se[1])
                    sizes.append(se[2])
                elif se[0] == 'dir':
                    dirs.append(se[1])
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
        """
        try:
            return datetime.duck(self.backendCommand('QUERY_GUIDEDATATHROUGH'))
        except ValueError:
            return None

    def clearSettings(self):
        """
        Triggers an event to clear the settings cache on all active systems.
        """
        self.backendCommand('MESSAGE%sCLEAR_SETTINGS_CACHE' % BACKEND_SEP)
        self.db.settings.clear()

class BEEventMonitor( BECache ):
    def __init__(self, backend=None, blockshutdown=False,
                       systemevents=False, db=None):
        self.systemevents = systemevents
        super(BEEventMonitor, self).__init__(backend, blockshutdown, True, db)

    def _listhandlers(self):
        return [self.eventMonitor]

    def _neweventconn(self):
        return BEEventConnection(self.host, self.port, self.db.gethostname(),
                    level = (2,1)[self.systemevents])

    def eventMonitor(self, event=None):
        if event is None:
            return re.compile('BACKEND_MESSAGE')
        self.log(MythLog.ALL, MythLog.INFO, event)

class MythSystemEvent( BECache ):
    class systemeventhandler( object ):
        # decorator class for system events
        bs = BACKEND_SEP.replace('[','\[').replace(']','\]')
        re_process = re.compile(bs.join([
                'BACKEND_MESSAGE',
                'SYSTEM_EVENT (?P<event>[A-Z_]*)'
                    '( HOSTNAME (?P<hostname>[a-zA-Z0-9_\.]*))?'
                    '( SENDER (?P<sender>[a-zA-Z0-9_\.]*))?'
                    '( CARDID (?P<cardid>[0-9]*))?'
                    '( CHANID (?P<chanid>[0-9]*))?'
                    '( STARTTIME (?P<starttime>[0-9-]*T[0-9-]))?'
                    '( SECS (?P<secs>[0-9]*))?',
                'empty']))

        def __init__(self, func):
            self.func = func
            self.__doc__ = self.func.__doc__
            self.__name__ = self.func.__name__
            self.__module__ = self.func.__module__
            self.filter = None

            self.re_search = re.compile('BACKEND_MESSAGE%sSYSTEM_EVENT %s' %\
                                (self.bs, self.__name__.upper()))

            self.generic = (self.__name__ == '_generic_handler')
            if self.generic:
                self.re_search = re.compile(\
                                'BACKEND_MESSAGE%sSYSTEM_EVENT' % self.bs)

        def __get__(self, inst, own):
            self.inst = inst
            return self

        def __call__(self, event=None):
            if event is None:
                return self.re_search

            # basic string processing
            match = self.re_process.match(event)
            event = {}
            for a in ('event','hostname','sender','cardid','secs'):
                if match.group(a) is not None:
                    event[a] = match.group(a)

            # event filter for generic handler
            if self.generic:
                if self.filter is None:
                    self.filter = [f.__name__.upper() \
                                        for f in self.inst._events]
                if event['event'] in self.filter:
                    return

            # pull program information
            if (match.group('chanid') is not None) and \
               (match.group('starttime') is not None):
                    be = MythBE(self.inst.hostname)
                    event['program'] = be.getRecording(\
                          (match.group('chanid'),match.group('starttime')))

            self.func(self.inst, event)

    def _listhandlers(self):
        return []

    def __init__(self, backend=None, blockshutdown=False, db=None,
                       enablehandler=True):
        super(MythSystemEvent, self).__init__(backend, blockshutdown, True, db)

        if enablehandler:
            self._events.append(self._generic_handler)
            self.registerevent(self._generic_handler)

    def _neweventconn(self):
        return BEEventConnection(self.host, self.port, self.db.gethostname(), 3)

    @systemeventhandler
    def _generic_handler(self, event):
        SystemEvent(event['event'], inst.db).command(event)

class Frontend( FEConnection ):
    _db = None
    class _Jump( object ):
        def __str__(self):  return str(self.list())
        def __repr__(self): return str(self)

        def __init__(self, parent):
            self._parent = proxy(parent)
            self._points = {}

        def _populate(self):
            self._populate = _donothing
            self._points = dict(self._parent.send('jump'))

        def __getitem__(self, key):
            self._populate()
            if key in self._points:
                return self._parent.send('jump', key)
            else:
                return False

        def __getattr__(self, key):
            self._populate()
            if key in self.__dict__:
                return self.__dict__[key]
            return self.__getitem__(key)

        def dict(self):
            self._populate()
            return self._points

        def list(self):
            self._populate()
            return self._points.items()

    class _Key( object ):
        _keymap = { 9:'tab',        10:'enter',     27:'escape',
                    32:'space',     92:'backslash', 127:'backspace',
                    258:'down',     259:'up',       260:'left',
                    261:'right',    265:'f1',       266:'f2',
                    267:'f3',       268:'f4',       269:'f5',
                    270:'f6',       271:'f7',       272:'f8',
                    273:'f9',       274:'f10',      275:'f11',
                    276:'f12',      330:'delete',   331:'insert',
                    338:'pagedown', 339:'pageup'}
        _alnum = [chr(i) for i in range(48,58)+range(65,91)+range(97,123)]

        def __str__(self):  return str(self.list())
        def __repr__(self): return str(self)

        def __init__(self, parent):
            self._parent = proxy(parent)
            self._keys = []

        def _populate(self):
            self._populate = _donothing
            self._keys = self._parent.send('key')

        def _sendLiteral(self, key):
            if (key in self._keys) or (key in self._alnum):
                return self._parent.send('key', key)
            return False

        def _sendOrdinal(self, key):
            try:
                key = int(key)
                if key in self._keymap:
                    key = self._keymap[key]
                else:
                    key = chr(key)
                return self._sendLiteral(key)
            except ValueError:
                return False

        def __getitem__(self, key):
            self._populate()
            if self._sendOrdinal(key):
                return True
            elif self._sendLiteral(key):
                return True
            else:
                return False

        def __getattr__(self, key):
            self._populate()
            if key in self.__dict__:
                return self.__dict__[key]
            return self._sendLiteral(key)

        def list(self):
            self._populate()
            return self._keys

    def __init__(self, *args, **kwargs):
        FEConnection.__init__(self, *args, **kwargs)
        self.jump = self._Jump(self)
        self.key = self._Key(self)

    def sendQuery(self,query): return self.send('query', query)
    def getQuery(self): return self.send('query')
    def sendPlay(self,play): return self.send('play', play)
    def getPlay(self): return self.send('play')

    def play(self, media):
        """Plays selected media on frontend."""
        return media._playOnFe(self)

    def getLoad(self):
        """Returns tuple of 1/5/15 load averages."""
        return tuple([float(l) for l in self.sendQuery('load').split()])

    def getUptime(self):
        """Returns timedelta of uptime."""
        return timedelta(0, int(self.sendQuery('uptime')))

    def getTime(self):
        """Returns current time on frontend."""
        return datetime.fromIso(self.sendQuery('time'))

    def getMemory(self):
        """Returns free and total, physical and swap memory."""
        return dict(zip(('totalmem','freemem','totalswap','freeswap'),
                    [int(m) for m in self.sendQuery('memstats').split()]))

    def getScreenShot(self):
        """Returns PNG image of the frontend."""
        fd = urlopen('http://%s:6547/MythFE/GetScreenShot' % self.host)
        img = fd.read()
        fd.close()
        return img

class MythDB( DBCache ):
    __doc__ = DBCache.__doc__+"""
        obj.searchRecorded()    - return a list of matching Recorded objects
        obj.searchOldRecorded() - return a list of matching
                                  OldRecorded objects
        obj.searchJobs()        - return a list of matching Job objects
        obj.searchGuide()       - return a list of matching Guide objects
        obj.searchRecord()      - return a list of matching Record rules
        obj.getFrontends()      - return a list of available Frontends
        obj.getFrontend()       - return a single Frontend object
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
            syndicatedepisodenumber,            olderthan,  newerthan,
            inetref,    season,     episode

        Multiple keywords can be chained as such:
            obj.searchRecorded(title='Title', commflagged=False)
        """

        if init:
            # table and join descriptor
            init.table = 'recorded'
            init.handler = Recorded
            init.require = ('livetv',)
            init.joins = (init.Join(table='recordedprogram',
                                    tableto='recorded',
                                    fieldsfrom=('chanid','starttime'),
                                    fieldsto=('chanid','progstart')),
                          init.Join(table='recordedcredits',
                                    tableto='recorded',
                                    fieldsfrom=('chanid','starttime'),
                                    fieldsto=('chanid','progstart')),
                          init.Join(table='people',
                                    tableto='recordedcredits',
                                    fields=('person',)))
            return None

        # local table matches
        if key in ('title','subtitle','chanid',
                        'category','hostname','autoexpire','commflagged',
                        'stars','recgroup','playgroup','duplicate',
                        'transcoded','watched','storagegroup','basename',
                        'inetref','season','episode'):
            return ('recorded.%s=%%s' % key, value, 0)

        # time matches
        if key in ('starttime','endtime','progstart','progend'):
            return ('recorded.%s=?' % key, datetime.duck(value), 0)

        if key == 'olderthan':
            return ('recorded.starttime<?', datetime.duck(value), 0)
        if key == 'newerthan':
            return ('recorded.starttime>?', datetime.duck(value), 0)

        # recordedprogram matches
        if key in ('category_type','airdate','stereo','subtitled','hdtv',
                    'closecaptioned','partnumber','parttotal','seriesid',
                    'showtype','syndicatedepisodenumber','programid',
                    'manualid','generic'):
            return ('recordedprogram.%s=?' % key, value, 1)

        if key == 'cast':
            return ('people.name', 'recordedcredits', 4, 1)

        if key == 'livetv':
            if (value is None) or (value == False):
                return ('recorded.recgroup!=?', 'LiveTV', 0)
            return ()

        return None

    @databaseSearch
    def searchOldRecorded(self, init=False, key=None, value=None):
        """
        obj.searchOldRecorded(**kwargs) -> list of OldRecorded objects

        Supports the following keywords:
            title,      subtitle,   chanid,     starttime,  endtime,
            category,   seriesid,   programid,  station,    duplicate,
            generic,    recstatus,  inetref,    season,     episode
        """

        if init:
            init.table = 'oldrecorded'
            init.handler = OldRecorded
            return None

        if key in ('title','subtitle','chanid',
                        'category','seriesid','programid','station',
                        'duplicate','generic','recstatus','inetref',
                        'season','episode'):
            return ('oldrecorded.%s=?' % key, value, 0)
                # time matches
        if key in ('starttime','endtime'):
            return ('oldrecorded.%s=?' % key, datetime.duck(value), 0)
        return None

    @databaseSearch
    def searchArtwork(self, init=False, key=None, value=None):
        """
        obj.searchArtwork(**kwargs) -> list of RecordedArtwork objects

        Supports the following keywords:
            inetref,    season,     host,   chanid,     starttime
            title,      subtitle
        """

        if init:
            # table and join descriptor
            init.table = 'recordedartwork'
            init.handler = RecordedArtwork
            init.joins = (init.Join(table='recorded',
                                    tableto='recordedartwork',
                                    fieldsfrom=('inetref','season','hostname'),
                                    fieldsto=('inetref','season','host')),)
            return None

        if key in ('inetref', 'season', 'host'):
            return ('recordedartwork.%s=?' % key, value, 0)

        if key in ('chanid', 'title', 'subtitle'):
            return ('recorded.%s=?' % key, value, 1)

        if key == 'starttime':
            return ('recorded.%s=?' % key, datetime.duck(value), 1)

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
            init.table = 'jobqueue'
            init.handler = Job
            init.joins = (init.Join(table='recorded',
                                    tableto='jobqueue',
                                    fields=('chanid','starttime')),)
            return None

        if key in ('chanid','type','status','hostname'):
            return ('jobqueue.%s=?' % key, value, 0)
        if key in ('title','subtitle'):
            return ('recorded.%s=?' % key, value, 1)
        if key == 'flags':
            return ('jobqueue.flags&?', value, 0)

        if key == 'starttime':
            return ('jobqueue.starttime=?', datetime.duck(value), 0)
        if key == 'olderthan':
            return ('jobqueue.inserttime>?', datetime.duck(value), 0)
        if key == 'newerthan':
            return ('jobqueue.inserttime<?', datetime.duck(value), 0)
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
            showtype,   programid,  generic,    syndicatedepisodenumber,
            ondate,     cast,       startbefore,startafter
            endbefore,  endafter,   dayofweek,  weekday
            first,      last,       callsign,   commfree
            channelgroup,           videosource,
            genre,      rating,     cast,       fuzzytitle
            fuzzysubtitle,          fuzzydescription,
            fuzzyprogramid,         beforedate, afterdate,
        """
        if init:
            init.table = 'program'
            init.handler = Guide
            init.joins = (init.Join(table='credits',
                                    tableto='program',
                                    fields=('chanid','starttime')),
                          init.Join(table='people',
                                    tableto='credits',
                                    fields=('person',)),
                          init.Join(table='channel',
                                    tableto='program',
                                    fields=('chanid',)),
                          init.Join(table='channelgroup',
                                    tableto='program',
                                    fields=('chanid',)),
                          init.Join(table='channelgroupnames',
                                    tableto='channelgroup',
                                    fields=('grpid',)),
                          init.Join(table='videosource',
                                    tableto='channel',
                                    fields=('sourceid',)),
                          init.Join(table='programgenres',
                                    tableto='program',
                                    fields=('chanid','starttime')),
                          init.Join(table='programrating',
                                    tableto='program',
                                    fields=('chanid','starttime')))
            return None

        if key in ('chanid','title','subtitle',
                        'category','airdate','stars','previouslyshown','stereo',
                        'subtitled','hdtv','closecaptioned','partnumber',
                        'parttotal','seriesid','originalairdate','showtype',
                        'syndicatedepisodenumber','programid','generic',
                        'category_type'):
            return ('program.%s=?' % key, value, 0)
        if key in ('starttime','endtime'):
            return ('program.%s=?' % key, datetime.duck(value), 0)
        if key == 'dayofweek':
            return ('DAYNAME(program.starttime)=?', value, 0)
        if key == 'weekday':
            return ('WEEKDAY(program.starttime)<?', 5, 0)
        if key in ('first', 'last'):
            return ('program.%s=?' % key, 1, 0)

        if key == 'callsign':
            return ('channel.callsign=?', value, 4)
        if key == 'commfree':
            return ('channel.commmethod=?', -2, 4)
        if key == 'channelgroup':
            return ('channelgroupnames.name=?', value, 24)
        if key == 'videosource':
            try:
                value = int(value)
                return ('channel.sourceid=?', value, 4)
            except:
                return ('videosource.name=?', value, 36)
        if key == 'genre':
            return ('programgenres.genre=?', value, 64)
        if key == 'rating':
            return ('programrating.rating=?', value, 128)
        if key == 'cast':
            return ('people.name', 'credits', 2, 0)

        if key.startswith('fuzzy'):
            if key[5:] in ('title', 'subtitle', 'description', 'programid'):
                return ('program.%s LIKE ?' % key[5:], '%'+value+'%', 0)
            if key[5:] == 'callsign':
                return ('channel.callsign LIKE ?', '%'+value+'%', 4)
        if key.endswith('date'):
            prefix = {'on':'=', 'before':'<', 'after':'>'}
            if key[:-4] in prefix:
                return ('DATE(program.starttime){0}?'.format(prefix[key[:-4]]),
                        value, 0)

        if key == 'startbefore':
            return ('program.starttime<?', datetime.duck(value), 0)
        if key == 'startafter':
            return ('program.starttime>?', datetime.duck(value), 0)
        if key == 'endbefore':
            return ('program.endtime<?', datetime.duck(value), 0)
        if key == 'endafter':
            return ('program.endtime>?', datetime.duck(value), 0)
        return None

    def makePowerRule(self, ruletitle='unnamed (Power Search', 
                            type=RECTYPE.kAllRecord, **kwargs):
        where, args, join = self.searchGuide.parseInp(kwargs)
        where = ' AND '.join(where)
        join = self.searchGuide.buildJoin(join)
        return Record.fromPowerRule(title=ruletitle, type=type, where=where,
                                    args=args, join=join, db=self)

    @databaseSearch
    def searchRecord(self, init=False, key=None, value=None):
        """
        obj.searchRecord(**kwargs) -> list of Record objects

        Supports the following keywords:
            type,       chanid,     starttime,  startdate,  endtime
            enddate,    title,      subtitle,   category,   profile
            recgroup,   station,    seriesid,   programid,  playgroup,
            inetref
        """
        if init:
            init.table = 'record'
            init.handler = Record
            return None

        if key in ('type','chanid','starttime','startdate','endtime','enddate',
                        'title','subtitle','category','profile','recgroup',
                        'station','seriesid','programid','playgroup','inetref'):
            return ('%s=?' % key, value, 0)
        return None

    @databaseSearch
    def searchInternetContent(self, init=False, key=None, value=None):
        """
        obj.searchInternetContent(**kwargs) -> list of content

        Supports the following keywords:
            feedtitle,  title,      subtitle,   season,     episode,
            url,        type,       author,     rating,     player,
            width,      height,     language,   podcast,    downloadable,
            ondate,     olderthan,  newerthan,  longerthan, shorterthan,
            country,    description
        """
        if init:
            init.table = 'internetcontentarticles'
            init.handler = InternetContentArticles
            return None

        if key in ('feedtitle','title','subtitle','season','episode','url',
                        'type','author','rating','player','width','height',
                        'language','podcast','downloadable', 'description'):
            return ('%s=?' % key, value, 0)
        if key == 'ondate':
            return ('DATE(date)=?', value, 0)
        if key == 'olderthan':
            return ('date>?', value, 0)
        if key == 'newerthan':
            return ('date<?', value, 0)
        if key == 'longerthan':
            return ('time<?', value, 0)
        if key == 'shorterthan':
            return ('time>?', value, 0)
        if key == 'country':
            return ('countries LIKE ?', '%'+value+'%', 0)

    def getFrontends(self):
        """
        Returns a list of Frontend objects for accessible frontends
        """
        with self.cursor(self.log) as cursor:
            cursor.execute("""SELECT DISTINCT hostname
                              FROM settings
                              WHERE hostname IS NOT NULL
                              AND value='NetworkControlEnabled'
                              AND data=1""")
            frontends = [(fe[0], self.settings[fe[0]].NetworkControlPort) \
                            for fe in cursor]
        return Frontend.testList(frontends)

    def getFrontend(self,host):
        """
        Returns a Frontend object for the specified host
        """
        port = self.settings[host].NetworkControlPort
        return Frontend(host,port)

    def scanVideos(self):
        """
        obj.scanVideo() --> list of new, moved, and deleted Videos
        """
        startvids = dict([(vid.intid, vid) for vid in Video.getAllEntries(db=self)])

        be = MythBE(db=self)
        r = re.compile(re.escape(BACKEND_SEP).\
                join(['BACKEND_MESSAGE',
                      '(VIDEO_LIST(_NO)?_CHANGE)',
                      '(.*)']))
        lock = EventLock(r, db=self)

        be.backendCommand("SCAN_VIDEOS")
        lock.wait()

        newvids = []
        movvids = []
        oldvids = []

        match = r.match(lock.event)
        if match.group(1) == "VIDEO_LIST_CHANGE":
            for entry in match.group(3).split(BACKEND_SEP):
                type,intid = entry.split('::')
                intid = int(intid)
                if type == 'added':
                    newvids.append(Video(intid))
                elif type == 'moved':
                    v = startvids[intid]
                    v._pull()
                    movvids.append(v)
                elif type == 'deleted':
                    oldvids.append(startvids[intid])

        return newvids,movvids,oldvids

    @databaseSearch
    def searchVideos(self, init=False, key=None, value=None):
        """
        obj.searchVideos(**kwargs) -> list of Video objects

        Supports the following keywords:
            title, subtitle, season, episode, host, director, year, cast,
            genre, country, category, insertedbefore, insertedafter, custom
        """
        if init:
            init.table = 'videometadata'
            init.handler = Video
            init.joins = (init.Join(table='videometadatacast',
                                    tableto='videometadata',
                                    fieldsfrom=('idvideo',),
                                    fieldsto=('intid',)),
                          init.Join(table='videocast',
                                    tableto='videometadatacast',
                                    fieldsfrom=('intid',),
                                    fieldsto=('idcast',)),
                          init.Join(table='videometadatagenre',
                                    tableto='videometadata',
                                    fieldsfrom=('idvideo',),
                                    fieldsto=('intid',)),
                          init.Join(table='videogenre',
                                    tableto='videometadatagenre',
                                    fieldsfrom=('intid',),
                                    fieldsto=('idgenre',)),
                          init.Join(table='videometadatacountry',
                                    tableto='videometadata',
                                    fieldsfrom=('idvideo',),
                                    fieldsto=('intid',)),
                          init.Join(table='videocountry',
                                    tableto='videometadatacountry',
                                    fieldsfrom=('intid',),
                                    fieldsto=('idcountry',)),
                          init.Join(table='videocategory',
                                    tableto='videometadata',
                                    fieldsfrom=('intid',),
                                    fieldsto=('category',)))
            return None

        if key in ('title','subtitle','season','episode','host',
                        'director','year'):
            return('videometadata.%s=?' % key, value, 0)
        vidref = {'cast':(2,0), 'genre':(8,2), 'country':(32,4)}
        if key in vidref:
            return ('video%s.%s' % (key,key),
                    'videometadata%s' % key,
                    vidref[key][0],
                    vidref[key][1])
        if key == 'category':
            return ('videocategory.%s=?' % key, value, 64)
        if key == 'exactfile':
            return ('videometadata.filename=?', value, 0)
        if key == 'file':
            return ('videometadata.filename LIKE ?', '%'+value, 0)
        if key == 'insertedbefore':
            return ('videometadata.insertdate<?', value, 0)
        if key == 'insertedafter':
            return ('videometadata.insertdate>?', value, 0)
        return None

class MythVideo( MythDB ):
    """legacy - do not use"""
    def scanStorageGroups(self, delete=True):
        """legacy - do not use"""
        added, moved, deleted = scanVideos()
        return (added, deleted)

class MythXML( XMLConnection ):
    """
    Provides convenient methods to access the backend XML server.
    """
    def __init__(self, backend=None, port=None, db=None):
        if backend and port:
            XMLConnection.__init__(self, backend, port)
            self.db = db
            return

        self.db = DBCache(db)
        self.log = MythLog('Python XML Connection')
        if backend is None:
            # use master backend
            backend = self.db.settings.NULL.MasterServerIP
        if re.match('(?:\d{1,3}\.){3}\d{1,3}',backend) or \
                    check_ipv6(backend):
            # process ip address
            host = self.db._gethostfromaddr(backend)
            self.host = backend
            self.port = int(self.db.settings[host].BackendStatusPort)
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

    def getHosts(self):
        """Returns a list of unique hostnames found in the settings table."""
        return self._request('Myth/GetHosts')\
                                        .readJSON()['StringList']['Values']

    def getKeys(self):
        """Returns a list of unique keys found in the settings table."""
        return self._request('Myth/GetKeys')\
                                        .readJSON()['StringList']['Values']

    def getSetting(self, key, hostname=None, default=None):
        """Retrieves a setting from the backend."""
        args = {'Key':key}
        if hostname:
            args['HostName'] = hostname
        if default:
            args['Default'] = default
        return self._request('Myth/GetSetting', **args)\
                         .readJSON()['SettingList']['Settings'][0]['Value']

    def getProgramGuide(self, starttime, endtime, startchan, numchan=None):
        """
        Returns a list of Guide objects corresponding to the given time period.
        """
        starttime = datetime.duck(starttime)
        endtime = datetime.duck(endtime)
        args = {'StartTime':starttime.utcisoformat().rsplit('.',1)[0],
                'EndTime':endtime.utcisoformat().rsplit('.',1)[0], 
                'StartChanId':startchan, 'Details':1}
        if numchan:
            args['NumOfChannels'] = numchan
        else:
            args['NumOfChannels'] = 1

        dat = self._request('Guide/GetProgramGuide', **args).readJSON()
        for chan in dat['ProgramGuide']['Channels']:
            for prog in chan['Programs']:
                prog['ChanId'] = chan['ChanId']
                yield Guide.fromJSON(prog, self.db)

    def getProgramDetails(self, chanid, starttime):
        """
        Returns a Program object for the matching show.
        """
        starttime = datetime.duck(starttime)
        args = {'ChanId': chanid, 'StartTime': starttime.utcisoformat()}
        return Program.fromJSON(
                self._request('Guide/GetProgramDetails', **args)\
                    .readJSON()['Program'],
                db=self.db)

    def getChannelIcon(self, chanid):
        """Returns channel icon as a data string"""
        return self._request('Guide/GetChannelIcon', ChanId=chanid).read()

    def getRecorded(self, descending=True):
        """
        Returns a list of Program objects for recorded shows on the backend.
        """
        for prog in self._request('Dvr/GetRecorded', Descending=descending)\
                    .readJSON()['ProgramList']['Programs']:
            yield Program.fromJSON(prog, self.db)

    def getExpiring(self):
        """
        Returns a list of Program objects for expiring shows on the backend.
        """
        for prog in self._request('Dvr/GetExpiringList')\
                    .readJSON()['ProgramList']['Programs']:
            yield Program.fromJSON(prog, self.db)

#    def getInternetSources(self):
#        for grabber in self._queryTree('GetInternetSources').\
#                        find('InternetContent').findall('grabber'):
#            yield InternetSource.fromEtree(grabber, self)

    def getInternetContentUrl(self, grabber, videocode):
        return "mythflash://%s:%s/InternetContent/GetInternetContent?Grabber=%s&videocode=%s" \
            % (self.host, self.port, grabber, videocode)

    def getPreviewImage(self, chanid, starttime, width=None, \
                                                 height=None, secsin=None):
        starttime = datetime.duck(starttime)
        args = {'ChanId':chanid, 'StartTime':starttime.utcisoformat()}
        if width: args['Width'] = width
        if height: args['Height'] = height
        if secsin: args['SecsIn'] = secsin

        return self._result('Content/GetPreviewImage', **args).read()

class MythMusic( MusicSchema, DBCache ):
    """
    Provides convenient methods to access the MythTV MythMusic database.
    """
    @databaseSearch
    def searchMusic(self, init=False, key=None, value=None):
        """
        obj.searchMusic(**kwargs) -> iterable of Song objects

        Supports the following keywords:
            name,  track,  disc_number, artist,      album,   year
            genre, rating, format,      sample_rate, bitrate
        """
        if init:
            init.table = 'music_songs'
            init.handler = Song
            init.joins = (init.Join(table='music_artists',
                                    tableto='music_songs',
                                    fields=('artist_id',)),
                          init.Join(table='music_albums',
                                    tableto='music_songs',
                                    fields=('album_id',)),
                          init.Join(table='music_genres',
                                    tableto='music_songs',
                                    fields=('genre_id',)))

        if key in ('name','track','disc_number','rating',
                        'format','sample_rate','bitrate'):
            return ('music_songs.%s=%%s' % key, value, 0)
        if key == 'artist':
            return ('music_artists.artist_name=%s', value, 1)
        if key == 'album':
            return ('music_albums.album_name=%s', value, 2)
        if key == 'year':
            return ('music_albums.year=%s', value, 2)
        if key == 'genre':
            return ('music_genres.genre=%s', value, 4)
