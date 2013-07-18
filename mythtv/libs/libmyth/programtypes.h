#ifndef _PROGRAM_INFO_TYPES_H_
#define _PROGRAM_INFO_TYPES_H_

//////////////////////////////////////////////////////////////////////
//
// WARNING
//
// The enums in this header are used in libmythservicecontracts,
// and for database values: hence when removing something from
// these enums leave a gap, and when adding a new value give it
// a explicit integer value.
// 
//////////////////////////////////////////////////////////////////////

// ANSI C
#include <stdint.h> // for [u]int[32,64]_t

// C++ headers
#include <deque>
using namespace std;

// Qt headers
#include <QString>
#include <QMap>
#include <QHash>

// MythTV headers
#include "mythexp.h" // for MPUBLIC
#include "recordingtypes.h" // for RecordingType

class QDateTime;
class QMutex;

MPUBLIC extern const char *kPlayerInUseID;
MPUBLIC extern const char *kPIPPlayerInUseID;
MPUBLIC extern const char *kPBPPlayerInUseID;
MPUBLIC extern const char *kImportRecorderInUseID;
MPUBLIC extern const char *kRecorderInUseID;
MPUBLIC extern const char *kFileTransferInUseID;
MPUBLIC extern const char *kTruncatingDeleteInUseID;
MPUBLIC extern const char *kFlaggerInUseID;
MPUBLIC extern const char *kTranscoderInUseID;
MPUBLIC extern const char *kPreviewGeneratorInUseID;
MPUBLIC extern const char *kJobQueueInUseID;
MPUBLIC extern const char *kCCExtractorInUseID;

/// Frame # -> File offset map
typedef QMap<uint64_t, uint64_t> frm_pos_map_t;

typedef enum {
    MARK_ALL           = -100,
    MARK_UNSET         = -10,
    MARK_TMP_CUT_END   = -5,
    MARK_TMP_CUT_START = -4,
    MARK_UPDATED_CUT   = -3,
    MARK_PLACEHOLDER   = -2,
    MARK_CUT_END       = 0,
    MARK_CUT_START     = 1,
    MARK_BOOKMARK      = 2,
    MARK_BLANK_FRAME   = 3,
    MARK_COMM_START    = 4,
    MARK_COMM_END      = 5,
    MARK_GOP_START     = 6,
    MARK_KEYFRAME      = 7,
    MARK_SCENE_CHANGE  = 8,
    MARK_GOP_BYFRAME   = 9,
    MARK_ASPECT_1_1    = 10, //< deprecated, it is only 1:1 sample aspect ratio
    MARK_ASPECT_4_3    = 11,
    MARK_ASPECT_16_9   = 12,
    MARK_ASPECT_2_21_1 = 13,
    MARK_ASPECT_CUSTOM = 14,
    MARK_VIDEO_WIDTH   = 30,
    MARK_VIDEO_HEIGHT  = 31,
    MARK_VIDEO_RATE    = 32,
    MARK_DURATION_MS   = 33,
    MARK_TOTAL_FRAMES  = 34,
} MarkTypes;
MPUBLIC QString toString(MarkTypes type);

/// Frame # -> Mark map
typedef QMap<uint64_t, MarkTypes> frm_dir_map_t;

typedef enum CommFlagStatuses {
    COMM_FLAG_NOT_FLAGGED = 0,
    COMM_FLAG_DONE = 1,
    COMM_FLAG_PROCESSING = 2,
    COMM_FLAG_COMMFREE = 3
} CommFlagStatus;

/// This is used as a bitmask.
typedef enum SkipTypes {
    COMM_DETECT_COMMFREE    = -2,
    COMM_DETECT_UNINIT      = -1,
    COMM_DETECT_OFF         = 0x00000000,
    COMM_DETECT_BLANK       = 0x00000001,
    COMM_DETECT_BLANKS      = COMM_DETECT_BLANK,
    COMM_DETECT_SCENE       = 0x00000002,
    COMM_DETECT_LOGO        = 0x00000004,
    COMM_DETECT_BLANK_SCENE = (COMM_DETECT_BLANKS | COMM_DETECT_SCENE),
    COMM_DETECT_ALL         = (COMM_DETECT_BLANKS |
                               COMM_DETECT_SCENE |
                               COMM_DETECT_LOGO),
    COMM_DETECT_2           = 0x00000100,
    COMM_DETECT_2_LOGO      = COMM_DETECT_2 | COMM_DETECT_LOGO,
    COMM_DETECT_2_BLANK     = COMM_DETECT_2 | COMM_DETECT_BLANKS,
    COMM_DETECT_2_SCENE     = COMM_DETECT_2 | COMM_DETECT_SCENE,
    /* Scene detection doesn't seem to be too useful (in the USA); there *
     * are just too many false positives from non-commercial cut scenes. */
    COMM_DETECT_2_ALL       = (COMM_DETECT_2_LOGO | COMM_DETECT_2_BLANK),

    COMM_DETECT_PREPOSTROLL = 0x00000200,
    COMM_DETECT_PREPOSTROLL_ALL = (COMM_DETECT_PREPOSTROLL
                                   | COMM_DETECT_BLANKS
                                   | COMM_DETECT_SCENE)
} SkipType;

MPUBLIC QString SkipTypeToString(int);
MPUBLIC deque<int> GetPreferredSkipTypeCombinations(void);

typedef enum TranscodingStatuses {
    TRANSCODING_NOT_TRANSCODED = 0,
    TRANSCODING_COMPLETE       = 1,
    TRANSCODING_RUNNING        = 2
} TranscodingStatus;

/// If you change these please update:
/// mythplugins/mythweb/modules/tv/classes/Program.php
/// mythtv/bindings/perl/MythTV/Program.pm
/// (search for "Assign the program flags" in both)
typedef enum FlagMask {
    FL_NONE           = 0x00000000,
    FL_COMMFLAG       = 0x00000001,
    FL_CUTLIST        = 0x00000002,
    FL_AUTOEXP        = 0x00000004,
    FL_EDITING        = 0x00000008,
    FL_BOOKMARK       = 0x00000010,
    FL_REALLYEDITING  = 0x00000020,
    FL_COMMPROCESSING = 0x00000040,
    FL_DELETEPENDING  = 0x00000080,
    FL_TRANSCODED     = 0x00000100,
    FL_WATCHED        = 0x00000200,
    FL_PRESERVED      = 0x00000400,
    FL_CHANCOMMFREE   = 0x00000800,
    FL_REPEAT         = 0x00001000,
    FL_DUPLICATE      = 0x00002000,
    FL_REACTIVATE     = 0x00004000,
    FL_IGNOREBOOKMARK = 0x00008000,
    // if you move the type mask please edit {Set,Get}ProgramInfoType()
    FL_TYPEMASK       = 0x000F0000,
    FL_INUSERECORDING = 0x00100000,
    FL_INUSEPLAYING   = 0x00200000,
    FL_INUSEOTHER     = 0x00400000,
} ProgramFlag;

typedef enum ProgramInfoType {
    kProgramInfoTypeRecording = 0,
    kProgramInfoTypeVideoFile,
    kProgramInfoTypeVideoDVD,
    kProgramInfoTypeVideoStreamingHTML,
    kProgramInfoTypeVideoStreamingRTSP,
    kProgramInfoTypeVideoBD,
} ProgramInfoType;

/// if AudioProps changes, the audioprop column in program and
/// recordedprogram has to changed accordingly
typedef enum AudioProps {
    AUD_UNKNOWN       = 0x00, // For backwards compatibility do not change 0 or 1
    AUD_STEREO        = 0x01,
    AUD_MONO          = 0x02,
    AUD_SURROUND      = 0x04,
    AUD_DOLBY         = 0x08,
    AUD_HARDHEAR      = 0x10,
    AUD_VISUALIMPAIR  = 0x20,
} AudioProperty; // has 6 bits in ProgramInfo::properties
#define kAudioPropertyBits 6
#define kAudioPropertyOffset 0
#define kAudioPropertyMask (0x3f<<kAudioPropertyOffset)

/// if VideoProps changes, the videoprop column in program and
/// recordedprogram has to changed accordingly
typedef enum VideoProps {
    // For backwards compatibility do not change 0 or 1
    VID_UNKNOWN       = 0x00,
    VID_HDTV          = 0x01,
    VID_WIDESCREEN    = 0x02,
    VID_AVC           = 0x04,
    VID_720           = 0x08,
    VID_1080          = 0x10,
    VID_DAMAGED       = 0x20,
    VID_3DTV          = 0x40,
} VideoProperty; // has 7 bits in ProgramInfo::properties
#define kVideoPropertyBits 7
#define kVideoPropertyOffset kAudioPropertyBits
#define kVideoPropertyMask (0x3f<<kVideoPropertyOffset)

/// if SubtitleTypes changes, the subtitletypes column in program and
/// recordedprogram has to changed accordingly
typedef enum SubtitleTypes {
    // For backwards compatibility do not change 0 or 1
    SUB_UNKNOWN       = 0x00,
    SUB_HARDHEAR      = 0x01,
    SUB_NORMAL        = 0x02,
    SUB_ONSCREEN      = 0x04,
    SUB_SIGNED        = 0x08
} SubtitleType; // has 4 bits in ProgramInfo::properties
#define kSubtitlePropertyBits 4
#define kSubtitlePropertyOffset (kAudioPropertyBits+kVideoPropertyBits)
#define kSubtitlePropertyMask (0x0f<<kSubtitlePropertyOffset)

typedef enum RecStatusTypes {
    rsOtherRecording = -13,
    rsOtherTuning = -12,
    rsMissedFuture = -11,
    rsTuning = -10,
    rsFailed = -9,
    rsTunerBusy = -8,
    rsLowDiskSpace = -7,
    rsCancelled = -6,
    rsMissed = -5,
    rsAborted = -4,
    rsRecorded = -3,
    rsRecording = -2,
    rsWillRecord = -1,
    rsUnknown = 0,
    rsDontRecord = 1,
    rsPreviousRecording = 2,
    rsCurrentRecording = 3,
    rsEarlierShowing = 4,
    rsTooManyRecordings = 5,
    rsNotListed = 6,
    rsConflict = 7,
    rsLaterShowing = 8,
    rsRepeat = 9,
    rsInactive = 10,
    rsNeverRecord = 11,
    rsOffLine = 12,
    rsOtherShowing = 13
} RecStatusType; // note stored in int8_t in ProgramInfo
MPUBLIC QString toUIState(RecStatusType);
MPUBLIC QString toString(RecStatusType, uint id);
MPUBLIC QString toString(RecStatusType, RecordingType);
MPUBLIC QString toDescription(RecStatusType, RecordingType,
                              const QDateTime &recstartts);

typedef enum AvailableStatusTypes {
    asAvailable = 0,
    asNotYetAvailable,
    asPendingDelete,
    asFileNotFound,
    asZeroByte,
    asDeleted
} AvailableStatusType; // note stored in uint8_t in ProgramInfo
MPUBLIC QString toString(AvailableStatusType);

enum WatchListStatus {
    wlDeleted = -4,
    wlEarlier = -3,
    wlWatched = -2,
    wlExpireOff = -1
};

typedef enum AutoExpireTypes {
    kDisableAutoExpire = 0,
    kNormalAutoExpire  = 1,
    kDeletedAutoExpire = 9999,
    kLiveTVAutoExpire = 10000
} AutoExpireType;

#endif // _PROGRAM_INFO_TYPES_H_
