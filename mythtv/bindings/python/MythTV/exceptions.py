# -*- coding: utf-8 -*-
"""Provides custom error codes"""

from MythTV.static import ERRCODES

class MythError( Exception, ERRCODES ):
    """
    MythError('Generic Error Message')
    MythError(SYSTEM, returncode, command, stderr)
    MythError(SOCKET, socketcode, socketerr)

    Error string will be available as obj.args[0].  Additional attributes
        may be available depending on the error code.
    """
    ecode = None

    def __init__(self, *args):
        if args[0] == self.SYSTEM:
            self.ename = 'SYSTEM'
            self.ecode, self.retcode, self.command, self.stderr = args
            self.args = ("External system call failed: code %d" %self.retcode,)
        elif args[0] == self.SOCKET:
            self.ename = 'SOCKET'
            self.ecode, (self.sockcode, self.sockerr) = args
            self.args = ("Socket Error: %s" %self.sockerr,)
        elif self.ecode is None:
            self.ename = 'GENERIC'
            self.ecode = self.GENERIC
            self.args = args
        self.message = str(self.args[0])

class MythDBError( MythError ):
    """
    MythDBError('Generic Error Message')
    MythDBError(DB_RAW, sqlerr)
    MythDBError(DB_CONNECTION, dbconn)
    MythDBError(DB_CREDENTIALS)
    MythDBError(DB_SETTING, setting, hostname)
    MythDBError(DB_SCHEMAMISMATCH, setting, remote, local)
    MythDBError(DB_SCHEMAUPDATE, sqlerr)

    Error string will be available as obj.args[0].  Additional attributes
        may be available depending on the error code.
    """
    def __init__(self, *args):
        if args[0] == self.DB_RAW:
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
        elif args[0] == self.DB_SCHEMAUPDATE:
            self.ename = 'DB_SCHEMAUPDATE'
            self.ecode, sqlerr = args
            if len(sqlerr) == 1:
                self.sqlcode = 0
                self.sqlerr = sqlerr[0]
                self.args = ("Schema update failure: %s" % self.sqlerr)
            else:
                self.sqlcode, self.sqlerr = sqlerr
                self.args = ("Schema update failure %d: %s" % sqlerr,)
        elif args[0] == self.DB_RESTRICT:
            self.ename = 'DB_RESTRICT'
            self.ecode, self.args = args
        MythError.__init__(self, *args)

class MythBEError( MythError ): 
    """
    MythBEError('Generic Error Message')
    MythBEError(PROTO_CONNECTION, backend, port)
    MythBEError(PROTO_ANNOUNCE, backend, port, response)
    MythBEError(PROTO_MISMATCH, remote, local)
    MythBEError(PROTO_PROGRAMINFO)

    Error string will be available as obj.args[0].  Additional attributes
        may be available depending on the error code.
    """
    def __init__(self, *args):
        if args[0] == self.PROTO_CONNECTION:
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
        elif args[0] == self.PROTO_PROGRAMINFO:
            self.ename = 'PROTO_PROGRAMINFO'
            self.ecode = args[0]
            self.args = ("Received invalid field count for program info",)
        MythError.__init__(self, *args)

class MythFEError( MythError ):
    """
    MythFEError('Generic Error Message')
    MythFEError(FE_CONNECTION, frontend, port)
    MythFEError(FE_ANNOUNCE, frontend, port)

    Error string will be available as obj.args[0].  Additional attributes
        may be available depending on the error code.
    """
    def __init__(self, *args):
        if args[0] == self.FE_CONNECTION:
            self.ename = 'FE_CONNECTION'
            self.ecode, self.frontend, self.port = args
            self.args = ('Connection to frontend %s:%d failed' \
                                            % (self.frontend, self.port),)
        elif args[0] == self.FE_ANNOUNCE:
            self.ename = 'FE_ANNOUNCE'
            self.ecode, self.frontend, self.port = args
            self.args = ('Open socket at %s:%d not recognized as mythfrontend'\
                                            % (self.frontend, self.port),)
        MythError.__init__(self, *args)

class MythFileError( MythError ):
    """
    MythFileError('Generic Error Message')
    MythFileError(FILE_ERROR)
    MythFileError(FILE_FAILED_READ, file)
    MythFileError(FILE_FAILED_WRITE, file, reason)

    Error string will be available as obj.args[0].  Additional attributes
        may be available depending on the error code.
    """
    def __init__(self, *args):
        if args[0] == self.FILE_ERROR:
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
            self.args = ("Error writing to %s, %s" % \
                    (self.file, self.reason),)
        elif args[0] == self.FILE_FAILED_SEEK:
            self.ename = 'FILE_FAILED_SEEK'
            self.ecode, self.file, self.offset, self.whence = args
            self.args = ("Error seeking %s to %d,%d" % \
                    (self.file, self.offset, self.whence),)
        MythError.__init__(self, *args)

class MythTZError( MythError ):
    """
    MythTZError('Generic Error Message')
    MythTZError(TZ_ERROR)
    MythTZError(TZ_INVALID_FILE)
    MythTZError(TZ_INVALID_TRANSITION, given, max)
    MythTZError(TZ_CONVERSION_ERROR, name, datetime)

    Error string will be available as obj.args[0].  Additional attributes
        may be available depending on the error code.
    """
    def __init__(self, *args):
        if args[0] == self.TZ_ERROR:
            self.ename = 'TZ_ERROR'
            self.ecode, self.reason = args
            self.args = ("Error processing time zone: {0}".format(self.reason))
        elif args[0] == self.TZ_INVALID_FILE:
            self.ename = 'TZ_INVALID_FILE'
            self.ecode, = args
            self.args = ("ZoneInfo file does not have proper magic string.",)
        elif args[0] == self.TZ_INVALID_TRANSITION:
            self.ename = 'TZ_INVALID_TRANSITION'
            self.ecode, self.given, self.max = args
            self.args = (("Transition has out-of-bounds definition index "
                          "(given: {0.given}, max: {0.max})").format(self),)
        elif args[0] == self.TZ_CONVERSION_ERROR:
            self.ename = 'TZ_CONVERSION_ERROR'
            self.ecode, self.tzname, self.datetime = args
            self.args = (("Timezone ({0.tzname}) does not have sufficiently "
                          "old data for search: {0.datetime}").format(self),)
        MythError.__init__(self, *args)

