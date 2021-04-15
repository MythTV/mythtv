#ifndef PROGRAM_TYPES_H
#define PROGRAM_TYPES_H

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

// C++ headers
#include <cstdint> // for [u]int[32,64]_t
#include <deque>

// Qt headers
#include <QString>
#include <QMap>
#include <QHash>
#include <QtCore>

// MythTV headers
#include "mythexp.h" // for MPUBLIC
#include "recordingtypes.h" // for RecordingType

class QDateTime;
class QMutex;

MPUBLIC extern const QString kPlayerInUseID;
MPUBLIC extern const QString kPIPPlayerInUseID;
MPUBLIC extern const QString kPBPPlayerInUseID;
MPUBLIC extern const QString kImportRecorderInUseID;
MPUBLIC extern const QString kRecorderInUseID;
MPUBLIC extern const QString kFileTransferInUseID;
MPUBLIC extern const QString kTruncatingDeleteInUseID;
MPUBLIC extern const QString kFlaggerInUseID;
MPUBLIC extern const QString kTranscoderInUseID;
MPUBLIC extern const QString kPreviewGeneratorInUseID;
MPUBLIC extern const QString kJobQueueInUseID;
MPUBLIC extern const QString kCCExtractorInUseID;

/// Frame # -> File offset map
using frm_pos_map_t = QMap<long long, long long>;

enum MarkTypes {
    MARK_INVALID       = -9999,
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
    MARK_ASPECT_1_1    = 10, ///< deprecated, it is only 1:1 sample aspect ratio
    MARK_ASPECT_4_3    = 11,
    MARK_ASPECT_16_9   = 12,
    MARK_ASPECT_2_21_1 = 13,
    MARK_ASPECT_CUSTOM = 14,
    MARK_SCAN_PROGRESSIVE = 15,
    MARK_VIDEO_WIDTH   = 30,
    MARK_VIDEO_HEIGHT  = 31,
    MARK_VIDEO_RATE    = 32,
    MARK_DURATION_MS   = 33,
    MARK_TOTAL_FRAMES  = 34,
    MARK_UTIL_PROGSTART = 40,
    MARK_UTIL_LASTPLAYPOS = 41,
};

MPUBLIC QString toString(MarkTypes type);

using stringMarkMap = std::map<QString, MarkTypes>;
static stringMarkMap MarkTypeStrings =
{
    { "ALL",              MARK_ALL },
    { "UNSET",            MARK_UNSET },
    { "TMP_CUT_END",      MARK_TMP_CUT_END },
    { "TMP_CUT_START",    MARK_TMP_CUT_START },
    { "UPDATED_CUT",      MARK_UPDATED_CUT },
    { "PLACEHOLDER",      MARK_PLACEHOLDER },
    { "CUT_END",          MARK_CUT_END },
    { "CUT_START",        MARK_CUT_START },
    { "BOOKMARK",         MARK_BOOKMARK },
    { "BLANK_FRAME",      MARK_BLANK_FRAME },
    { "COMM_START",       MARK_COMM_START },
    { "COMM_END",         MARK_COMM_END },
    { "GOP_START",        MARK_GOP_START },
    { "KEYFRAME",         MARK_KEYFRAME },
    { "SCENE_CHANGE",     MARK_SCENE_CHANGE },
    { "GOP_BYFRAME",      MARK_GOP_BYFRAME },
    { "ASPECT_4_3",       MARK_ASPECT_4_3 },
    { "ASPECT_16_9",      MARK_ASPECT_16_9 },
    { "ASPECT_2_21_1",    MARK_ASPECT_2_21_1 },
    { "ASPECT_CUSTOM",    MARK_ASPECT_CUSTOM },
    { "PROGRESSIVE",      MARK_SCAN_PROGRESSIVE },
    { "VIDEO_WIDTH",      MARK_VIDEO_WIDTH },
    { "VIDEO_HEIGHT",     MARK_VIDEO_HEIGHT },
    { "VIDEO_RATE",       MARK_VIDEO_RATE },
    { "DURATION_MS",      MARK_DURATION_MS },
    { "TOTAL_FRAMES",     MARK_TOTAL_FRAMES },
    { "UTIL_PROGSTART",   MARK_UTIL_PROGSTART },
    { "UTIL_LASTPLAYPOS", MARK_UTIL_LASTPLAYPOS }
};

MPUBLIC MarkTypes markTypeFromString(const QString & str);

/// Frame # -> Mark map
using frm_dir_map_t = QMap<uint64_t, MarkTypes>;

enum CommFlagStatus {
    COMM_FLAG_NOT_FLAGGED = 0,
    COMM_FLAG_DONE = 1,
    COMM_FLAG_PROCESSING = 2,
    COMM_FLAG_COMMFREE = 3
};

/// This is used as a bitmask.
enum SkipType {
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
};

MPUBLIC QString SkipTypeToString(int flags);
MPUBLIC std::deque<int> GetPreferredSkipTypeCombinations(void);

enum TranscodingStatus {
    TRANSCODING_NOT_TRANSCODED = 0,
    TRANSCODING_COMPLETE       = 1,
    TRANSCODING_RUNNING        = 2
};

#define DEFINE_FLAGS_ENUM
#include "programtypeflags.h"
#undef DEFINE_FLAGS_ENUM

enum ProgramInfoType {
    kProgramInfoTypeRecording = 0,
    kProgramInfoTypeVideoFile,
    kProgramInfoTypeVideoDVD,
    kProgramInfoTypeVideoStreamingHTML,
    kProgramInfoTypeVideoStreamingRTSP,
    kProgramInfoTypeVideoBD,
};

enum AvailableStatusType {
    asAvailable = 0,
    asNotYetAvailable,
    asPendingDelete,
    asFileNotFound,
    asZeroByte,
    asDeleted
}; // note stored in uint8_t in ProgramInfo
MPUBLIC QString toString(AvailableStatusType status);

enum WatchListStatus {
    wlDeleted = -4,
    wlEarlier = -3,
    wlWatched = -2,
    wlExpireOff = -1
};

enum AutoExpireType {
    kDisableAutoExpire = 0,
    kNormalAutoExpire  = 1,
    kDeletedAutoExpire = 9999,
    kLiveTVAutoExpire = 10000
};

#endif // PROGRAM_TYPES_H
