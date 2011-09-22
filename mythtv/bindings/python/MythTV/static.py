# -*- coding: utf-8 -*-

"""
Contains any static and global variables for MythTV Python Bindings
"""

OWN_VERSION = (0,25,-1,2)
SCHEMA_VERSION = 1282
NVSCHEMA_VERSION = 1007
MUSICSCHEMA_VERSION = 1018
PROTO_VERSION = '69'
PROTO_TOKEN = '63835135'
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

class LOGLEVEL( object ):
    ALL         = int('11111111111111111111111111111', 2)
    MOST        = int('00111111111101111111111111111', 2)
    NONE        = int('00000000000000000000000000000', 2)

    IMPORTANT   = int('00000000000000000000000000001', 2)
    GENERAL     = int('00000000000000000000000000010', 2)
    RECORD      = int('00000000000000000000000000100', 2)
    PLAYBACK    = int('00000000000000000000000001000', 2)
    CHANNEL     = int('00000000000000000000000010000', 2)
    OSD         = int('00000000000000000000000100000', 2)
    FILE        = int('00000000000000000000001000000', 2)
    SCHEDULE    = int('00000000000000000000010000000', 2)
    NETWORK     = int('00000000000000000000100000000', 2)
    COMMFLAG    = int('00000000000000000001000000000', 2)
    AUDIO       = int('00000000000000000010000000000', 2)
    LIBAV       = int('00000000000000000100000000000', 2)
    JOBQUEUE    = int('00000000000000001000000000000', 2)
    SIPARSER    = int('00000000000000010000000000000', 2)
    EIT         = int('00000000000000100000000000000', 2)
    VBI         = int('00000000000001000000000000000', 2)
    DATABASE    = int('00000000000010000000000000000', 2)
    DSMCC       = int('00000000000100000000000000000', 2)
    MHEG        = int('00000000001000000000000000000', 2)
    UPNP        = int('00000000010000000000000000000', 2)
    SOCKET      = int('00000000100000000000000000000', 2)
    XMLTV       = int('00000001000000000000000000000', 2)
    DVBCAM      = int('00000010000000000000000000000', 2)
    MEDIA       = int('00000100000000000000000000000', 2)
    IDLE        = int('00001000000000000000000000000', 2)
    CHANNELSCAN = int('00010000000000000000000000000', 2)
    SYSTEM      = int('00100000000000000000000000000', 2)
    EXTRA       = int('01000000000000000000000000000', 2)
    TIMESTAMP   = int('10000000000000000000000000000', 2)

    DBALL       = 8
    DBDEBUG     = 7
    DBINFO      = 6
    DBNOTICE    = 5
    DBWARNING   = 4
    DBERROR     = 3
    DBCRITICAL  = 2
    DBALERT     = 1
    DBEMERGENCY = 0

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

