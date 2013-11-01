
// AudioProp
AUD_STEREO          =  1;
AUD_MONO            =  2;
AUD_SURROUND        =  4;
AUD_DOLBY           =  8;
AUD_HARDHEAR        = 16;
AUD_VISUALIMPAIR    = 32;

// VideoProp
var VID_HDTV        =  1;
var VID_WIDESCREEN  =  2;
var VID_AVC         =  4;
var VID_720         =  8;
var VID_1080        = 16;
var VID_DAMAGED     = 32;
var VID_3DTV        = 64;

// Subtitles
var SUB_HARDHEAR    =  1;
var SUB_NORMAL      =  2;
var SUB_ONSCREEN    =  4;
var SUB_SIGNED      =  8;

// Program Flags
var FL_COMMFLAG       =  1;
var FL_CUTLIST        =  2;
var FL_AUTOEXP        =  4;
var FL_EDITING        =  8;
var FL_BOOKMARK       = 16;
var FL_REALLYEDITING  = 32;
var FL_COMMPROCESSING = 64;
var FL_DELETEPENDING  = 128;
var FL_TRANSCODED     = 256;
var FL_WATCHED        = 512;
var FL_PRESERVED      = 1024;
var FL_CHANCOMMFREE   = 2048;
var FL_REPEAT         = 4096;
var FL_DUPLICATE      = 8192;
var FL_REACTIVATE     = 16384;
var FL_IGNOREBOOKMARK = 32768;
var FL_TYPEMASK       = 983040;
var FL_INUSERECORDING = 1048576;
var FL_INUSEPLAYING   = 2097152;
var FL_INUSEOTHER     = 4194304;

// Recording Status
var RS_OTHERRECORDING = -13;
var RS_OTHERTUNING = -12;
var RS_MISSEDFUTURE = -11;
var RS_TUNING = -10;
var RS_FAILED = -9;
var RS_TUNERBUSY = -8;
var RS_LOWDISKSPACE = -7;
var RS_CANCELLED = -6;
var RS_MISSED = -5;
var RS_ABORTED = -4;
var RS_RECORDED = -3;
var RS_RECORDING = -2;
var RS_WILLRECORD = -1;
var RS_UNKNOWN = 0;
var RS_DONTRECORD = 1;
var RS_PREVIOUSRECORDING = 2;
var RS_CURRENTRECORDING = 3;
var RS_EARLIERSHOWING = 4;
var RS_TOOMANYRECORDINGS = 5;
var RS_NOTLISTED = 6;
var RS_CONFLICT = 7;
var RS_LATERSHOWING = 8;
var RS_REPEAT = 9;
var RS_INACTIVE = 10;
var RS_NEVERRECORD = 11;
var RS_OFFLINE = 12;
var RS_OTHERSHOWING = 13;
