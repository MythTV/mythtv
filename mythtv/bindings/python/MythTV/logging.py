# -*- coding: utf-8 -*-
"""Provides managed logging."""

from MythTV.static import LOGLEVEL, LOGMASK, LOGFACILITY
from MythTV.exceptions import MythError

import os
import syslog
import codecs

from sys import version_info, stdout, argv
from datetime import datetime
from thread import allocate_lock
from StringIO import StringIO
from traceback import format_exc

def _donothing(*args, **kwargs):
    pass

class DummyLogger( LOGLEVEL, LOGMASK, LOGFACILITY ):
    def __init__(self, module=None, db=None): pass
    def logTB(self, mask): pass
    def log(self, mask, level, message, detail=None): pass
    def __call__(self, mask, level, message, detail=None): pass

class MythLog( LOGLEVEL, LOGMASK, LOGFACILITY ):
    """
    MythLog(module='pythonbindings', lstr=None, lbit=None, \
                    db=None) -> logging object

    'module' defines the source of the message in the logs
    'lstr' and 'lbit' define the message filter
        'lbit' takes a bitwise value
        'lstr' takes a string in the normal '-v level' form
        default is set to 'important,general'

    The filter level is global values, shared between all logging instances.
    The logging object is callable, and implements the MythLog.log() method.
    """

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
 "  commflag     "  -  Commercial detection related messages
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

    @classmethod
    def _initlogger(cls):
        cls._initlogger = classmethod(_donothing)
        cls._MASK = LOGMASK.GENERAL
        cls._LEVEL = LOGLEVEL.INFO
        cls._LOGFILE = stdout
        cls._logwrite = cls._logfile
        cls._QUIET = 0
        cls._DBLOG = True
        cls._SYSLOG = None
        cls._lock = allocate_lock()
        cls._parseinput()

    @classmethod
    def _parseinput(cls):
        args = iter(argv)
        args.next()
        try:
            while True:
                arg = args.next()
                if arg == '--quiet':
                    cls._QUIET += 1
                elif arg == '--nodblog':
                    cls._DBLOG = False
                elif arg == '--loglevel':
                    cls._setlevel(args.next())
                elif arg == '--verbose':
                    cls._setmask(args.next())
                elif arg == '--logfile':
                    cls._setfile(args.next())
                elif arg == '--logpath':
                    cls._setpath(args.next())
                elif arg == '--syslog':
                    cls._setsyslog(args.next())
                elif arg == '--':
                    break

        except StopIteration:
            pass

    @classmethod
    def _optparseinput(cls):
        opts, args = cls._parser.parse_args()
        if opts.quiet:
            cls._QUIET = opts.quiet
        if opts.dblog:
            cls._DBLOG = False
        if opts.loglevel:
            cls._setlevel(opts.loglevel)
        if opts.verbose:
            cls._setmask(opts.verbose)
        if opts.logfile:
            cls._setfile(opts.logfile)
        if opts.logpath:
            cls._setpath(opts.logpath)
        if opts.syslog:
            cls._setsyslog(opts.syslog)

    @classmethod
    def _argparseinput(cls):
        opts = cls._parser.parse_args()
        if opts.quiet:
            cls._QUIET = opts.quiet
        if opts.dblog:
            cls._DBLOG = False
        if opts.loglevel:
            cls._setlevel(opts.loglevel)
        if opts.verbose:
            cls._setmask(opts.verbose)
        if opts.logfile:
            cls._setfile(opts.logfile)
        if opts.logpath:
            cls._setpath(opts.logpath)
        if opts.syslog:
            cls._setsyslog(opts.syslog)

    @classmethod
    def loadOptParse(cls, parser):
        cls._parser = parser
        cls._parseinput = cls._optparseinput
        parser.add_option('--quiet', action="count", dest="quiet",
            help="Run quiet. One use squelches terminal, two stops all logging.")
        parser.add_option('--nodblog', action="store_true", dest="dblog",
            help="Prevent logging to the database.")
        parser.add_option('--loglevel', type="string", action="store", dest="loglevel",
            help="Specify log verbosity, using standard syslog levels.")
        parser.add_option('--verbose', type="string", action="store", dest="verbose",
            help="Specify log mask, deciding what areas are allowed to log.")
        parser.add_option('--logfile', type="string", action="store", dest="logfile",
            help="Specify file to log all output to.")
        parser.add_option('--logpath', type="string", action="store", dest="logpath",
            help="Specify directory to log to, filename will be automatically decided.")
        parser.add_option('--syslog', type="string", action="store", dest="syslog",
            help="Specify syslog facility to log to.")

    @classmethod
    def loadArgParse(cls, parser):
        from argparse import Action
        class Count( Action ):
            def __call__(self, parser, namespace, values, option_string=None):
                values = getattr(namespace, self.dest)
                if values is None:
                    values = 1
                else:
                    values += 1
                setattr(namespace, self.dest, values)
        cls._parser = parser
        cls._parseinput = cls._argparseinput
        parser.add_argument('--quiet', action=Count, nargs=0, dest="quiet",
            help="Run quiet. One use squelches terminal, two stops all logging.")
        parser.add_argument('--nodblog', action="store_true", dest="dblog",
            help="Prevent logging to the database.")
        parser.add_argument('--loglevel', action="store", dest="loglevel",
            help="Specify log verbosity, using standard syslog levels.")
        parser.add_argument('--verbose', action="store", dest="verbose",
            help="Specify log mask, deciding what areas are allowed to log.")
        parser.add_argument('--logfile', action="store", dest="logfile",
            help="Specify file to log all output to.")
        parser.add_argument('--logpath', action="store", dest="logpath",
            help="Specify directory to log to, filename will be automatically decided.")
        parser.add_argument('--syslog', action="store", dest="syslog",
            help="Specify syslog facility to log to.")

    def __repr__(self):
        return "<%s '%s','%s' at %s>" % \
                (str(self.__class__).split("'")[1].split(".")[-1], 
                 self.module, bin(self._MASK), hex(id(self)))

    def __new__(cls, *args, **kwargs):
        # abuse the __new__ constructor to set some immutable class attributes
        # before the class is instantiated
        cls._initlogger()
        return super(MythLog, cls).__new__(cls)

    def __init__(self, module='pythonbindings', db=None):
        self.module = module
        self.db = db

    @classmethod
    def _setlevel(cls, level):
        cls._initlogger()
        try:
            level = int(level)
            cls._LEVEL = level
        except:
            if level not in ('any', 'emerg', 'alert', 'crit', 'err',
                             'warning', 'info', 'notice', 'debug', 'unknown'):
                return
            cls._LEVEL = getattr(cls, level.upper())

    @classmethod
    def _setmask(cls, mask):
        """Manually set loglevel."""
        cls._initlogger()
        try:
            cls._MASK = int(mask)
        except:
            cls._MASK = cls._parsemask(mask)

    @classmethod
    def _setfile(cls, filename):
        """Redirect log output to a specific file."""
        cls._initlogger()
        cls._setfileobject(codecs.open(filename, 'w', encoding='utf-8'))

    @classmethod
    def _setpath(cls, filepath):
        cls._initlogger()
        cls._setfile(os.path.join(filepath, "{0}.{1}.{2}.log"\
                            .format(argv[0].split('/')[-1],
                                    datetime.now().strftime('%Y%m%d%H%M%S'),
                                    os.getpid())))

    @classmethod
    def _setfileobject(cls, fileobject, close=True):
        """Redirect log output to an opened file pointer."""
        cls._initlogger()
        if (cls._LOGFILE.fileno() != 1) and close:
            cls._LOGFILE.close()
        cls._LOGFILE = fileobject
        cls._logwrite = cls._logfile
        if cls._SYSLOG:
            cls._SYSLOG = None
            syslog.closelog()

    @classmethod
    def _setsyslog(cls, facility=LOGFACILITY.USER):
        cls._initlogger()
        try:
            facility = int(facility)
            for fac in dir(LOGFACILITY):
                if '_' in fac:
                    continue
                if getattr(LOGFACILITY, fac) == facility:
                    facility = 'LOG_'+fac
                    break
            else:
                raise MythError("Invalid syslog facility")

        except ValueError:
            if not facility.startswith('LOG_'):
                facility = 'LOG_'+facility.upper()
            if not hasattr(LOGFACILITY, facility[4:]):
                raise MythError("Invalid syslog facility")

        cls._SYSLOG = facility
        syslog.openlog(argv[0].rsplit('/', 1)[1],
                       syslog.LOG_NDELAY|syslog.LOG_PID,
                       getattr(syslog, facility))
        cls._logwrite = cls._logsyslog
        if cls._LOGFILE:
            if cls._LOGFILE.fileno() != 1:
                cls._LOGFILE.close()
            cls._LOGFILE = None

    @classmethod
    def _parsemask(cls, mstr=None):
        bwlist = (  'important','general','record','playback','channel','osd',
                    'file','schedule','network','commflag','audio','libav',
                    'jobqueue','siparser','eit','vbi','database','dsmcc',
                    'mheg','upnp','socket','xmltv','dvbcam','media','idle',
                    'channelscan','extra','timestamp')
        if mstr:
            mask = cls.NONE
            for m in mstr.split(','):
                try:
                    if m in ('all','most','none'):
                        # set initial bitfield
                        mask = getattr(cls, m.upper())
                    elif m in bwlist:
                        # update bitfield OR
                        mask |= getattr(cls, m.upper())
                    elif len(m) > 2:
                        if m[0:2] == 'no':
                            if m[2:] in bwlist:
                                # update bitfield NOT
                                mask &= mask^getattr(cls, m[2:].upper())
                except AttributeError:
                    pass
            return mask
        else:
            cls._initlogger()
            mask = []
            for m,v in enumerate(bwlist):
                if cls._MASK&2**l:
                    mask.append(v)
            return ','.join(mask)

    def time(self): return datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')

    def logTB(self, mask):
        """
        MythLog.logTB(mask) -> None

        'mask' sets the bitwise log mask, to be matched against the log
                    filter. If any bits match true, the message will be logged.
            This will log the latest traceback.
        """
        self.log(mask, self.CRIT, format_exc())

    def log(self, mask, level, message, detail=None):
        """
        MythLog.log(mask, message, detail=None) -> None

        'mask' sets the bitwise log mask, to be matched against the log
                    filter. If any bits match true, the message will be logged.
        'message' and 'detail' set the log message content using the format:
                <timestamp> <module>: <message>
                        ---- or ----
                <timestamp> <module>: <message> -- <detail>
        """
        if level > self._LEVEL:
            return
        if not mask&self._MASK:
            return
        if self._QUIET > 1:
            return

        with self._lock:
            self._logwrite(mask, level, message, detail)
        self._logdatabase(mask, level, message, detail)

    def _logfile(self, mask, level, message, detail):
        if self._QUIET and (self._LOGFILE == stdout):
            return

        buff = StringIO()
        buff.write("{0} {3} [{1}] {2} "\
            .format(self.time(), os.getpid(), self.module,
                    ['!','A','C','E','W','N','I','D'][level]))

        multiline = False
        if '\n' in message:
            multiline = True
        elif detail:
            if '\n' in detail:
                multiline = True

        if multiline:
            for line in message.split('\n'):
                buff.write('\n    %s' % line)
            if detail:
                for line in detail.split('\n'):
                    buff.write('\n        %s' % line)
        else:
            buff.write(message)
            if detail:
                buff.write(' -- %s' % detail)

        buff.write('\n')

        self._LOGFILE.write(buff.getvalue())
        self._LOGFILE.flush()

    def _logsyslog(self, mask, level, message, detail):
        syslog.syslog(level,
                      message + (' -- {0}'.format(detail) if detail else ''))

    def _logdatabase(self, mask, level, message, detail):
        if self.db and self._DBLOG:
            with self.db.cursor(DummyLogger()) as cursor:
                application = argv[0]
                if '/' in application:
                    application = application.rsplit('/', 1)[1]

                cursor.execute("""INSERT INTO logging
                                    (host, application, pid, thread,
                                     msgtime, level, message)
                                  VALUES (?, ?, ?, ?, ?, ?, ?)""",
                    (self.db.gethostname(), application,
                     os.getpid(), self.module, self.time(), level,
                     message + (' -- {0}'.format(detail) if detail else '')))

    def __call__(self, mask, level, message, detail=None):
        self.log(mask, level, message, detail)
