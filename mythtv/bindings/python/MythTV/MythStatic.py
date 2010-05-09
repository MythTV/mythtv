# -*- coding: utf-8 -*-

"""
Contains any static and global variables for MythTV Python Bindings
"""

OWN_VERSION = (0,23,0,8)
SCHEMA_VERSION = 1254
MVSCHEMA_VERSION = 1035
NVSCHEMA_VERSION = 1006
PROTO_VERSION = 56
PROGRAM_FIELDS = 47
BACKEND_SEP = '[]:[]'

class MARKUP( object ):
    MARK_UNSET          = -10
    MARK_UPDATED_CUT    = -3
    MARK_EDIT_MODE      = -2
    MARK_CUT_END        = 0
    MARK_CUT_START      = 1
    MARK_BOOKMARK       = 2
    MARK_BLANK_FRAME    = 3
    MARK_COMM_START     = 4
    MARK_COMM_END       = 5
    MARK_GOP_START      = 6
    MARK_KEYFRAME       = 7
    MARK_SCENE_CHANGE   = 8
    MARK_GOP_BYFRAME    = 9

class RECTYPE( object ):
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

class RECSTATUS( object ):
    rsFailed            = -9
    rsTunerBusy         = -8
    rsLowDiskSpace      = -7
    rsCancelled         = -6
    rsMissed            = -5
    rsAborted           = -4
    rsRecorded          = -3
    rsRecording         = -2
    rsWillRecord        = -1
    rsUnknown           = 0
    rsDontRecord        = 1
    rsPreviousRecording = 2
    rsCurrentRecording  = 3
    rsEarlierShowing    = 4
    rsTooManyRecordings = 5
    rsNotListed         = 6
    rsConflict          = 7
    rsLaterShowing      = 8
    rsRepeat            = 9
    rsInactive          = 10
    rsNeverRecord       = 11
    rsOffline           = 12
    rsOtherShowing      = 13

class JOBTYPE( object ):
    NONE         = 0x0000
    SYSTEMJOB    = 0x00ff
    TRANSCODE    = 0x0001
    COMMFLAG     = 0x0002
    USERJOB      = 0xff00
    USERJOB1     = 0x0100
    USERJOB2     = 0x0200
    USERJOB3     = 0x0300
    USERJOB4     = 0x0400

class JOBCMD( object ):
    RUN          = 0x0000
    PAUSE        = 0x0001
    RESUME       = 0x0002
    STOP         = 0x0004
    RESTART      = 0x0008

class JOBFLAG( object ):
    NO_FLAGS     = 0x0000
    USE_CUTLIST  = 0x0001
    LIVE_REC     = 0x0002
    EXTERNAL     = 0x0004

class JOBSTATUS( object ):
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

