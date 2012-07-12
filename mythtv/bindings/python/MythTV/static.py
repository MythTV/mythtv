# -*- coding: utf-8 -*-

"""
Contains any static and global variables for MythTV Python Bindings
"""

OWN_VERSION = (0,26,-1,1)
SCHEMA_VERSION = 1308
NVSCHEMA_VERSION = 1007
MUSICSCHEMA_VERSION = 1018
PROTO_VERSION = '75'
PROTO_TOKEN = 'SweetRock'
BACKEND_SEP = '[]:[]'
INSTALL_PREFIX = '/usr/local'

class MARKUP( object ):
    MARK_UNSET          = -10
    MARK_UPDATED_CUT    = -3
    MARK_PLACEHOLDER    = -2
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
    MARK_ASPECT_1_1     = 10
    MARK_ASPECT_4_3     = 11
    MARK_ASPECT_16_9    = 12
    MARK_ASPECT_2_21_1  = 13
    MARK_ASPECT_CUSTOM  = 14
    MARK_VIDEO_WIDTH    = 30
    MARK_VIDEO_HEIGHT   = 31

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

class RECSEARCHTYPE( object ):
    kNoSearch           = 0
    kPowerSearch        = 1
    kTitleSearch        = 2
    kKeywordSearch      = 3
    kPeopleSearch       = 4
    kManualSearch       = 5

class RECSTATUS( object ):
    rsTuning            = -10
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

class AUDIO_PROPS( object ):
    AUD_UNKNOWN         = 0x00
    AUD_STEREO          = 0x01
    AUD_MONO            = 0x02
    AUD_SURROUND        = 0x04
    AUD_DOLBY           = 0x08
    AUD_HARDHEAR        = 0x10
    AUD_VISUALIMPAIR    = 0x20

class VIDEO_PROPS( object ):
    VID_UNKNOWN         = 0x00
    VID_HDTV            = 0x01
    VID_WIDESCREEN      = 0x02
    VID_AVC             = 0x04
    VID_720             = 0x08
    VID_1080            = 0x10

class SUBTITLE_TYPES( object ):
    SUB_UNKNOWN         = 0x00
    SUB_HARDHEAR        = 0x01
    SUB_NORMAL          = 0x02
    SUB_ONSCREEN        = 0x04
    SUB_SIGNED          = 0x08

class JOBTYPE( object ):
    NONE         = 0x0000
    SYSTEMJOB    = 0x00ff
    TRANSCODE    = 0x0001
    COMMFLAG     = 0x0002
    USERJOB      = 0xff00
    USERJOB1     = 0x0100
    USERJOB2     = 0x0200
    USERJOB3     = 0x0400
    USERJOB4     = 0x0800

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
    REBUILD      = 0x0008

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
    ABORTING     = 0x0009
    DONE         = 0x0100
    FINISHED     = 0x0110
    ABORTED      = 0x0120
    ERRORED      = 0x0130
    CANCELLED    = 0x0140

class LOGMASK( object ):
    ALL         = 0b111111111111111111111111111
    MOST        = 0b011111111110111111111111111
    NONE        = 0b000000000000000000000000000

    GENERAL     = 0b000000000000000000000000001
    RECORD      = 0b000000000000000000000000010
    PLAYBACK    = 0b000000000000000000000000100
    CHANNEL     = 0b000000000000000000000001000
    OSD         = 0b000000000000000000000010000
    FILE        = 0b000000000000000000000100000
    SCHEDULE    = 0b000000000000000000001000000
    NETWORK     = 0b000000000000000000010000000
    COMMFLAG    = 0b000000000000000000100000000
    AUDIO       = 0b000000000000000001000000000
    LIBAV       = 0b000000000000000010000000000
    JOBQUEUE    = 0b000000000000000100000000000
    SIPARSER    = 0b000000000000001000000000000
    EIT         = 0b000000000000010000000000000
    VBI         = 0b000000000000100000000000000
    DATABASE    = 0b000000000001000000000000000
    DSMCC       = 0b000000000010000000000000000
    MHEG        = 0b000000000100000000000000000
    UPNP        = 0b000000001000000000000000000
    SOCKET      = 0b000000010000000000000000000
    XMLTV       = 0b000000100000000000000000000
    DVBCAM      = 0b000001000000000000000000000
    MEDIA       = 0b000010000000000000000000000
    IDLE        = 0b000100000000000000000000000
    CHANNELSCAN = 0b001000000000000000000000000
    SYSTEM      = 0b010000000000000000000000000
    TIMESTAMP   = 0b100000000000000000000000000

class LOGLEVEL( object ):
    ANY         = -1
    EMERG       = 0
    ALERT       = 1
    CRIT        = 2
    ERR         = 3
    WARNING     = 4
    NOTICE      = 5
    INFO        = 6
    DEBUG       = 7
    UNKNOWN     = 8

class LOGFACILITY( object ):
    KERN        = 1
    USER        = 2
    MAIL        = 3
    DAEMON      = 4
    AUTH        = 5
    LPR         = 6
    NEWS        = 7
    UUCP        = 8
    CRON        = 9
    LOCAL0      = 10
    LOCAL1      = 11
    LOCAL2      = 12
    LOCAL3      = 13
    LOCAL4      = 14
    LOCAL5      = 15
    LOCAL6      = 16
    LOCAL7      = 17

class ERRCODES( object ):
    GENERIC             = 0
    SYSTEM              = 1
    SOCKET              = 2
    DB_RAW              = 50
    DB_CONNECTION       = 51
    DB_CREDENTIALS      = 52
    DB_SETTING          = 53
    DB_SCHEMAMISMATCH   = 54
    DB_SCHEMAUPDATE     = 55
    DB_RESTRICT         = 56
    PROTO_CONNECTION    = 100
    PROTO_ANNOUNCE      = 101
    PROTO_MISMATCH      = 102
    PROTO_PROGRAMINFO   = 103
    FE_CONNECTION       = 150
    FE_ANNOUNCE         = 151
    FILE_ERROR          = 200
    FILE_FAILED_READ    = 201
    FILE_FAILED_WRITE   = 202
    FILE_FAILED_SEEK    = 203

class MythSchema( object ):
    _schema_value = 'DBSchemaVer'
    _schema_local = SCHEMA_VERSION
    _schema_name = 'Database'
    _schema_update = None

class VideoSchema( MythSchema ):
    pass

class MusicSchema( object ):
    _schema_value = 'MusicDBSchemaVer'
    _schema_local = MUSICSCHEMA_VERSION
    _schema_name = 'MythMusic'
    _schema_update = None

