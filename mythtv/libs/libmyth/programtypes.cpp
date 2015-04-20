// -*- Mode: c++ -*-

#include <QDateTime>
#include <QMutex>
#include <QObject>

#include "programtypes.h"
#include "mythdate.h"

#include <deque>
using std::deque;

const char *kPlayerInUseID           = "player";
const char *kPIPPlayerInUseID        = "pipplayer";
const char *kPBPPlayerInUseID        = "pbpplayer";
const char *kImportRecorderInUseID   = "import_recorder";
const char *kRecorderInUseID         = "recorder";
const char *kFileTransferInUseID     = "filetransfer";
const char *kTruncatingDeleteInUseID = "truncatingdelete";
const char *kFlaggerInUseID          = "flagger";
const char *kTranscoderInUseID       = "transcoder";
const char *kPreviewGeneratorInUseID = "preview_generator";
const char *kJobQueueInUseID         = "jobqueue";
const char *kCCExtractorInUseID      = "ccextractor";

QString toString(MarkTypes type)
{
    switch (type)
    {
        case MARK_ALL:          return "ALL";
        case MARK_UNSET:        return "UNSET";
        case MARK_TMP_CUT_END:  return "TMP_CUT_END";
        case MARK_TMP_CUT_START:return "TMP_CUT_START";
        case MARK_UPDATED_CUT:  return "UPDATED_CUT";
        case MARK_PLACEHOLDER:  return "PLACEHOLDER";
        case MARK_CUT_END:      return "CUT_END";
        case MARK_CUT_START:    return "CUT_START";
        case MARK_BOOKMARK:     return "BOOKMARK";
        case MARK_BLANK_FRAME:  return "BLANK_FRAME";
        case MARK_COMM_START:   return "COMM_START";
        case MARK_COMM_END:     return "COMM_END";
        case MARK_GOP_START:    return "GOP_START";
        case MARK_KEYFRAME:     return "KEYFRAME";
        case MARK_SCENE_CHANGE: return "SCENE_CHANGE";
        case MARK_GOP_BYFRAME:  return "GOP_BYFRAME";
        case MARK_ASPECT_1_1:   return "ASPECT_1_1 (depreciated)";
        case MARK_ASPECT_4_3:   return "ASPECT_4_3";
        case MARK_ASPECT_16_9:  return "ASPECT_16_9";
        case MARK_ASPECT_2_21_1:return "ASPECT_2_21_1";
        case MARK_ASPECT_CUSTOM:return "ASPECT_CUSTOM";
        case MARK_VIDEO_WIDTH:  return "VIDEO_WIDTH";
        case MARK_VIDEO_HEIGHT: return "VIDEO_HEIGHT";
        case MARK_VIDEO_RATE:   return "VIDEO_RATE";
        case MARK_DURATION_MS:  return "DURATION_MS";
        case MARK_TOTAL_FRAMES: return "TOTAL_FRAMES";
        case MARK_UTIL_PROGSTART: return "UTIL_PROGSTART";
        case MARK_UTIL_LASTPLAYPOS: return "UTIL_LASTPLAYPOS";
    }

    return "unknown";
}

QString RecStatus::toUIState(RecStatus::Type recstatus)
{
    if (recstatus == RecStatus::Recorded     || recstatus == RecStatus::WillRecord)
        return "normal";

    if (recstatus == RecStatus::Recording    || recstatus == RecStatus::Tuning)
        return "running";

    if (recstatus == RecStatus::Conflict     || recstatus == RecStatus::Offline      ||
        recstatus == RecStatus::TunerBusy    || recstatus == RecStatus::Failed       ||
        recstatus == RecStatus::Aborted      || recstatus == RecStatus::Missed       ||
        recstatus == RecStatus::Failing)
    {
        return "error";
    }

    if (recstatus == RecStatus::Repeat       ||
        recstatus == RecStatus::NeverRecord  || recstatus == RecStatus::DontRecord   ||
        (recstatus != RecStatus::DontRecord && recstatus <= RecStatus::EarlierShowing))
    {
        return "disabled";
    }

    return "warning";
}

/// \brief Converts "recstatus" into a short (unreadable) string.
QString RecStatus::toString(RecStatus::Type recstatus, uint id)
{
    QString ret = "-";
    switch (recstatus)
    {
        case RecStatus::Aborted:
            ret = QObject::tr("A", "RecStatusChar RecStatus::Aborted");
            break;
        case RecStatus::Recorded:
            ret = QObject::tr("R", "RecStatusChar RecStatus::Recorded");
            break;
        case RecStatus::Recording:
            ret = QString::number(id);
            break;
        case RecStatus::Tuning:
            ret = QString::number(id);
            break;
        case RecStatus::Failing:
            ret = QString::number(id);
            break;
        case RecStatus::WillRecord:
            ret = QString::number(id);
            break;
        case RecStatus::DontRecord:
            ret = QObject::tr("X", "RecStatusChar RecStatus::DontRecord");
            break;
        case RecStatus::PreviousRecording:
            ret = QObject::tr("P", "RecStatusChar RecStatus::PreviousRecording");
            break;
        case RecStatus::CurrentRecording:
            ret = QObject::tr("R", "RecStatusChar RecStatus::CurrentRecording");
            break;
        case RecStatus::EarlierShowing:
            ret = QObject::tr("E", "RecStatusChar RecStatus::EarlierShowing");
            break;
        case RecStatus::TooManyRecordings:
            ret = QObject::tr("T", "RecStatusChar RecStatus::TooManyRecordings");
            break;
        case RecStatus::Cancelled:
            ret = QObject::tr("c", "RecStatusChar RecStatus::Cancelled");
            break;
        case RecStatus::MissedFuture:
        case RecStatus::Missed:
            ret = QObject::tr("M", "RecStatusChar RecStatus::Missed");
            break;
        case RecStatus::Conflict:
            ret = QObject::tr("C", "RecStatusChar RecStatus::Conflict");
            break;
        case RecStatus::LaterShowing:
            ret = QObject::tr("L", "RecStatusChar RecStatus::LaterShowing");
            break;
        case RecStatus::Repeat:
            ret = QObject::tr("r", "RecStatusChar RecStatus::Repeat");
            break;
        case RecStatus::Inactive:
            ret = QObject::tr("x", "RecStatusChar RecStatus::Inactive");
            break;
        case RecStatus::LowDiskSpace:
            ret = QObject::tr("K", "RecStatusChar RecStatus::LowDiskSpace");
            break;
        case RecStatus::TunerBusy:
            ret = QObject::tr("B", "RecStatusChar RecStatus::TunerBusy");
            break;
        case RecStatus::Failed:
            ret = QObject::tr("f", "RecStatusChar RecStatus::Failed");
            break;
        case RecStatus::NotListed:
            ret = QObject::tr("N", "RecStatusChar RecStatus::NotListed");
            break;
        case RecStatus::NeverRecord:
            ret = QObject::tr("V", "RecStatusChar RecStatus::NeverRecord");
            break;
        case RecStatus::Offline:
            ret = QObject::tr("F", "RecStatusChar RecStatus::Offline");
            break;
        case RecStatus::Unknown:
            break;
    }

    return (ret.isEmpty()) ? QString("-") : ret;
}

/// \brief Converts "recstatus" into a human readable string
QString RecStatus::toString(RecStatus::Type recstatus, RecordingType rectype)
{
    if (recstatus == RecStatus::Unknown && rectype == kNotRecording)
        return QObject::tr("Not Recording");

    switch (recstatus)
    {
        case RecStatus::Aborted:
            return QObject::tr("Aborted");
        case RecStatus::Recorded:
            return QObject::tr("Recorded");
        case RecStatus::Recording:
            return QObject::tr("Recording");
        case RecStatus::Tuning:
            return QObject::tr("Tuning");
        case RecStatus::Failing:
            return QObject::tr("Failing");
        case RecStatus::WillRecord:
            return QObject::tr("Will Record");
        case RecStatus::DontRecord:
            return QObject::tr("Don't Record");
        case RecStatus::PreviousRecording:
            return QObject::tr("Previously Recorded");
        case RecStatus::CurrentRecording:
            return QObject::tr("Currently Recorded");
        case RecStatus::EarlierShowing:
            return QObject::tr("Earlier Showing");
        case RecStatus::TooManyRecordings:
            return QObject::tr("Max Recordings");
        case RecStatus::Cancelled:
            return QObject::tr("Manual Cancel");
        case RecStatus::MissedFuture:
        case RecStatus::Missed:
            return QObject::tr("Missed");
        case RecStatus::Conflict:
            return QObject::tr("Conflicting");
        case RecStatus::LaterShowing:
            return QObject::tr("Later Showing");
        case RecStatus::Repeat:
            return QObject::tr("Repeat");
        case RecStatus::Inactive:
            return QObject::tr("Inactive");
        case RecStatus::LowDiskSpace:
            return QObject::tr("Low Disk Space");
        case RecStatus::TunerBusy:
            return QObject::tr("Tuner Busy");
        case RecStatus::Failed:
            return QObject::tr("Recorder Failed");
        case RecStatus::NotListed:
            return QObject::tr("Not Listed");
        case RecStatus::NeverRecord:
            return QObject::tr("Never Record");
        case RecStatus::Offline:
            return QObject::tr("Recorder Off-Line");
        case RecStatus::Unknown:
            return QObject::tr("Unknown");
    }

    return QObject::tr("Unknown");
}

/// \brief Converts "recstatus" into a long human readable description.
QString RecStatus::toDescription(RecStatus::Type recstatus, RecordingType rectype,
                      const QDateTime &recstartts)
{
    if (recstatus == RecStatus::Unknown && rectype == kNotRecording)
        return QObject::tr("This showing is not scheduled to record");

    QString message;
    QDateTime now = MythDate::current();

    if (recstatus <= RecStatus::WillRecord)
    {
        switch (recstatus)
        {
            case RecStatus::WillRecord:
                message = QObject::tr("This showing will be recorded.");
                break;
            case RecStatus::Recording:
                message = QObject::tr("This showing is being recorded.");
                break;
            case RecStatus::Tuning:
                message = QObject::tr("The showing is being tuned.");
                break;
            case RecStatus::Failing:
                message = QObject::tr("The showing is failing to record "
                                      "because of errors.");
                break;
            case RecStatus::Recorded:
                message = QObject::tr("This showing was recorded.");
                break;
            case RecStatus::Aborted:
                message = QObject::tr("This showing was recorded but was "
                                      "aborted before completion.");
                break;
            case RecStatus::Missed:
            case RecStatus::MissedFuture:
                message = QObject::tr("This showing was not recorded because "
                                      "the master backend was not running.");
                break;
            case RecStatus::Cancelled:
                message = QObject::tr("This showing was not recorded because "
                                      "it was manually cancelled.");
                break;
            case RecStatus::LowDiskSpace:
                message = QObject::tr("This showing was not recorded because "
                                      "there wasn't enough disk space.");
                break;
            case RecStatus::TunerBusy:
                message = QObject::tr("This showing was not recorded because "
                                      "the recorder was already in use.");
                break;
            case RecStatus::Failed:
                message = QObject::tr("This showing was not recorded because "
                                      "the recorder failed.");
                break;
            default:
                message = QObject::tr("The status of this showing is unknown.");
                break;
        }

        return message;
    }

    if (recstartts > now)
        message = QObject::tr("This showing will not be recorded because ");
    else
        message = QObject::tr("This showing was not recorded because ");

    switch (recstatus)
    {
        case RecStatus::DontRecord:
            message += QObject::tr("it was manually set to not record.");
            break;
        case RecStatus::PreviousRecording:
            message += QObject::tr("this episode was previously recorded "
                                   "according to the duplicate policy chosen "
                                   "for this title.");
            break;
        case RecStatus::CurrentRecording:
            message += QObject::tr("this episode was previously recorded and "
                                   "is still available in the list of "
                                   "recordings.");
            break;
        case RecStatus::EarlierShowing:
            message += QObject::tr("this episode will be recorded at an "
                                   "earlier time instead.");
            break;
        case RecStatus::TooManyRecordings:
            message += QObject::tr("too many recordings of this program have "
                                   "already been recorded.");
            break;
        case RecStatus::Conflict:
            message += QObject::tr("another program with a higher priority "
                                   "will be recorded.");
            break;
        case RecStatus::LaterShowing:
            message += QObject::tr("this episode will be recorded at a "
                                   "later time instead.");
            break;
        case RecStatus::Repeat:
            message += QObject::tr("this episode is a repeat.");
            break;
        case RecStatus::Inactive:
            message += QObject::tr("this recording rule is inactive.");
            break;
        case RecStatus::NotListed:
            message += QObject::tr("this rule does not match any showings in "
                                   "the current program listings.");
            break;
        case RecStatus::NeverRecord:
            message += QObject::tr("it was marked to never be recorded.");
            break;
        case RecStatus::Offline:
            message += QObject::tr("the required recorder is off-line.");
            break;
        default:
            if (recstartts > now)
                message = QObject::tr("This showing will not be recorded.");
            else
                message = QObject::tr("This showing was not recorded.");
            break;
    }

    return message;
}

QString toString(AvailableStatusType status)
{
    switch (status)
    {
        case asAvailable:       return "Available";
        case asNotYetAvailable: return "NotYetAvailable";
        case asPendingDelete:   return "PendingDelete";
        case asFileNotFound:    return "FileNotFound";
        case asZeroByte:        return "ZeroByte";
        case asDeleted:         return "Deleted";
    }
    return QString("Unknown(%1)").arg((int)status);
}


QString SkipTypeToString(int flags)
{
    if (COMM_DETECT_COMMFREE == flags)
        return QObject::tr("Commercial Free");
    if (COMM_DETECT_UNINIT == flags)
        return QObject::tr("Use Global Setting");

    QChar chr = '0';
    QString ret = QString("0x%1").arg(flags,3,16,chr);
    bool blank = COMM_DETECT_BLANK & flags;
    bool scene = COMM_DETECT_SCENE & flags;
    bool logo  = COMM_DETECT_LOGO  & flags;
    bool exp   = COMM_DETECT_2     & flags;
    bool prePst= COMM_DETECT_PREPOSTROLL & flags;

    if (blank && scene && logo)
        ret = QObject::tr("All Available Methods");
    else if (blank && scene && !logo)
        ret = QObject::tr("Blank Frame + Scene Change");
    else if (blank && !scene && logo)
        ret = QObject::tr("Blank Frame + Logo Detection");
    else if (!blank && scene && logo)
        ret = QObject::tr("Scene Change + Logo Detection");
    else if (blank && !scene && !logo)
        ret = QObject::tr("Blank Frame Detection");
    else if (!blank && scene && !logo)
        ret = QObject::tr("Scene Change Detection");
    else if (!blank && !scene && logo)
        ret = QObject::tr("Logo Detection");

    if (exp)
        ret = QObject::tr("Experimental") + ": " + ret;
    else if(prePst)
        ret = QObject::tr("Pre & Post Roll") + ": " + ret;

    return ret;
}

deque<int> GetPreferredSkipTypeCombinations(void)
{
    deque<int> tmp;
    tmp.push_back(COMM_DETECT_BLANK | COMM_DETECT_SCENE | COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_BLANK);
    tmp.push_back(COMM_DETECT_BLANK | COMM_DETECT_SCENE);
    tmp.push_back(COMM_DETECT_SCENE);
    tmp.push_back(COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_2 | COMM_DETECT_BLANK | COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_PREPOSTROLL | COMM_DETECT_BLANK |
                  COMM_DETECT_SCENE);
    return tmp;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
