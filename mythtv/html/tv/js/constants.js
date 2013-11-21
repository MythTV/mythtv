
// This should be frozen with Object.freeze(), but it seems QtScript is
// based on the 3rd or 4th edition of ECMA-262 and doesn't support it

// AudioProp
var AudioProperty = {
STEREO          :  1,
MONO            :  2,
SURROUND        :  4,
DOLBY           :  8,
HARDHEAR        : 16,
VISUALIMPAIR    : 32
};

// VideoProp
var VideoProperty = {
HDTV        :  1,
WIDESCREEN  :  2,
AVC         :  4,
HD720         :  8,
HD1080        : 16,
DAMAGED     : 32,
_3DTV        : 64 // Object names can't start with numbers
};

// Subtitles
var SubtitleType = {
HARDHEAR    :  1,
NORMAL      :  2,
ONSCREEN    :  4,
SIGNED      :  8
};

// Program Flags
var ProgramFlag = {
COMMFLAG       :  1,
CUTLIST        :  2,
AUTOEXP        :  4,
EDITING        :  8,
BOOKMARK       : 16,
REALLYEDITING  : 32,
COMMPROCESSING : 64,
DELETEPENDING  : 128,
TRANSCODED     : 256,
WATCHED        : 512,
PRESERVED      : 1024,
CHANCOMMFREE   : 2048,
REPEAT         : 4096,
DUPLICATE      : 8192,
REACTIVATE     : 16384,
IGNOREBOOKMARK : 32768,
TYPEMASK       : 983040,
INUSERECORDING : 1048576,
INUSEPLAYING   : 2097152,
INUSEOTHER     : 4194304
};

// Recording Status
var RecordingStatus = {
OTHERRECORDING    : -13,
OTHERTUNING       : -12,
MISSEDFUTURE      : -11,
TUNING            : -10,
FAILED            : -9,
TUNERBUSY         : -8,
LOWDISKSPACE      : -7,
CANCELLED         : -6,
MISSED            : -5,
ABORTED           : -4,
RECORDED          : -3,
RECORDING         : -2,
WILLRECORD        : -1,
UNKNOWN           : 0,
DONTRECORD        : 1,
PREVIOUSRECORDING : 2,
CURRENTRECORDING  : 3,
EARLIERSHOWING    : 4,
TOOMANYRECORDINGS : 5,
NOTLISTED         : 6,
CONFLICT          : 7,
LATERSHOWING      : 8,
REPEAT            : 9,
INACTIVE          : 10,
NEVERRECORD       : 11,
OFFLINE           : 12,
OTHERSHOWING      : 13
};

// Recording Schedule Types
// var RecordingType = {
// NOTRECORDING     : 0,
// SINGLERECORD     : 1,
// DAILYRECORD      : 2,
// ALLRECORD        : 4,
// WEEKLYRECORD     : 5,
// ONERECORD        : 6,
// OVERRIDERECORD   : 7,
// DONTRECORD       : 8,
// TEMPLATERECORD   : 11
// };

//F**king 'ell
var RecordingType = {
NOTRECORDING     : "Not Recording",
SINGLERECORD     : "Single Record",
ONERECORD        : "Record One",
ALLRECORD        : "Record All",
DAILYRECORD      : "Record Daily",
WEEKLYRECORD     : "Record Weekly",
OVERRIDERECORD   : "Override Recording",
DONTRECORD       : "Do not Record",
TEMPLATERECORD   : "Recording Template",
};

// Schedule Duplicate Methods
// var RecordingDupMethodType = {
// CHECKNONE        : 1,
// CHECKSUB         : 2,
// CHECKDESC        : 4,
// CHECKSUBDESC     : 6,
// CHECKSUBTHENDESC : 8
// };

var RecordingDupMethodType = {
CHECKNONE        : "None",
CHECKSUB         : "Subtitle",
CHECKDESC        : "Description",
CHECKSUBDESC     : "Subtitle and Description",
CHECKSUBTHENDESC : "Subtitle then Description"
};

// Schedule Duplicate-In Type
// var RecordingDupInType = {
// INRECORDED     : 1,
// INOLDRECORDED  : 2,
// INALL          : 15,
// NEWEPISODES    : 16
// };

var RecordingDupInType = {
INRECORDED     : "Current Recordings",
INOLDRECORDED  : "Previous Recordings",
INALL          : "All Recordings",
NEWEPISODES    : "New Episodes Only"
};

// Category Types
var CategoryType = {
NONE : "",
MOVIE : "movie",
SERIES : "series",
TVSHOW : "tvshow",
SPORTS : "sports"
};
