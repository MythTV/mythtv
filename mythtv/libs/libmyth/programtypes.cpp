// -*- Mode: c++ -*-

#include <QDateTime>
#include <QMutex>

#include "programtypes.h"
#include "mythdate.h"

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
    }

    return "unknown";
}

QString toUIState(RecStatusType recstatus)
{
    if (recstatus == rsRecorded     || recstatus == rsWillRecord   ||
        recstatus == rsOtherShowing)
        return "normal";

    if (recstatus == rsRecording    || recstatus == rsTuning       ||
        recstatus == rsOtherRecording || recstatus == rsOtherTuning)
        return "running";

    if (recstatus == rsConflict     || recstatus == rsOffLine      ||
        recstatus == rsTunerBusy    || recstatus == rsFailed       ||
        recstatus == rsAborted      || recstatus == rsMissed)
    {
        return "error";
    }

    if (recstatus == rsRepeat       ||
        recstatus == rsNeverRecord  || recstatus == rsDontRecord   ||
        (recstatus != rsDontRecord && recstatus <= rsEarlierShowing))
    {
        return "disabled";
    }

    return "warning";
}

/// \brief Converts "recstatus" into a short (unreadable) string.
QString toString(RecStatusType recstatus, uint id)
{
    QString ret = "-";
    switch (recstatus)
    {
        case rsAborted:
            ret = QObject::tr("A", "RecStatusChar rsAborted");
            break;
        case rsRecorded:
            ret = QObject::tr("R", "RecStatusChar rsRecorded");
            break;
        case rsRecording:
            ret = QString::number(id);
            break;
        case rsTuning:
            ret = QString::number(id);
            break;
        case rsWillRecord:
            ret = QString::number(id);
            break;
        case rsDontRecord:
            ret = QObject::tr("X", "RecStatusChar rsDontRecord");
            break;
        case rsPreviousRecording:
            ret = QObject::tr("P", "RecStatusChar rsPreviousRecording");
            break;
        case rsCurrentRecording:
            ret = QObject::tr("R", "RecStatusChar rsCurrentRecording");
            break;
        case rsEarlierShowing:
            ret = QObject::tr("E", "RecStatusChar rsEarlierShowing");
            break;
        case rsTooManyRecordings:
            ret = QObject::tr("T", "RecStatusChar rsTooManyRecordings");
            break;
        case rsCancelled:
            ret = QObject::tr("c", "RecStatusChar rsCancelled");
            break;
        case rsMissed:
            ret = QObject::tr("M", "RecStatusChar rsMissed");
            break;
        case rsMissedFuture:
            ret = "M";
            break;
        case rsConflict:
            ret = QObject::tr("C", "RecStatusChar rsConflict");
            break;
        case rsLaterShowing:
            ret = QObject::tr("L", "RecStatusChar rsLaterShowing");
            break;
        case rsRepeat:
            ret = QObject::tr("r", "RecStatusChar rsRepeat");
            break;
        case rsInactive:
            ret = QObject::tr("x", "RecStatusChar rsInactive");
            break;
        case rsLowDiskSpace:
            ret = QObject::tr("K", "RecStatusChar rsLowDiskSpace");
            break;
        case rsTunerBusy:
            ret = QObject::tr("B", "RecStatusChar rsTunerBusy");
            break;
        case rsFailed:
            ret = QObject::tr("f", "RecStatusChar rsFailed");
            break;
        case rsNotListed:
            ret = QObject::tr("N", "RecStatusChar rsNotListed");
            break;
        case rsNeverRecord:
            ret = QObject::tr("V", "RecStatusChar rsNeverRecord");
            break;
        case rsOffLine:
            ret = QObject::tr("F", "RecStatusChar rsOffLine");
            break;
        case rsOtherShowing:
            ret = QString::number(id);
            break;
        case rsOtherRecording:
            ret = QString::number(id);
            break;
        case rsOtherTuning:
            ret = QString::number(id);
            break;
        case rsUnknown:
            break;
    }

    return (ret.isEmpty()) ? QString("-") : ret;
}

/// \brief Converts "recstatus" into a human readable string
QString toString(RecStatusType recstatus, RecordingType rectype)
{
    if (recstatus == rsUnknown && rectype == kNotRecording)
        return QObject::tr("Not Recording");

    switch (recstatus)
    {
        case rsAborted:
            return QObject::tr("Aborted");
        case rsRecorded:
            return QObject::tr("Recorded");
        case rsRecording:
            return QObject::tr("Recording");
        case rsTuning:
            return QObject::tr("Tuning");
        case rsWillRecord:
            return QObject::tr("Will Record");
        case rsDontRecord:
            return QObject::tr("Don't Record");
        case rsPreviousRecording:
            return QObject::tr("Previously Recorded");
        case rsCurrentRecording:
            return QObject::tr("Currently Recorded");
        case rsEarlierShowing:
            return QObject::tr("Earlier Showing");
        case rsTooManyRecordings:
            return QObject::tr("Max Recordings");
        case rsCancelled:
            return QObject::tr("Manual Cancel");
        case rsMissed:
            return QObject::tr("Missed");
        case rsMissedFuture:
            return "Missed Future";
        case rsConflict:
            return QObject::tr("Conflicting");
        case rsLaterShowing:
            return QObject::tr("Later Showing");
        case rsRepeat:
            return QObject::tr("Repeat");
        case rsInactive:
            return QObject::tr("Inactive");
        case rsLowDiskSpace:
            return QObject::tr("Low Disk Space");
        case rsTunerBusy:
            return QObject::tr("Tuner Busy");
        case rsFailed:
            return QObject::tr("Recorder Failed");
        case rsNotListed:
            return QObject::tr("Not Listed");
        case rsNeverRecord:
            return QObject::tr("Never Record");
        case rsOffLine:
            return QObject::tr("Recorder Off-Line");
        case rsOtherShowing:
            return QObject::tr("Other Showing");
        case rsOtherRecording:
            return QObject::tr("Other Recording");
        case rsOtherTuning:
            return QObject::tr("Other Tuning");
        case rsUnknown:
            return QObject::tr("Unknown");
    }

    return QObject::tr("Unknown");
}

/// \brief Converts "recstatus" into a long human readable description.
QString toDescription(RecStatusType recstatus, RecordingType rectype,
                      const QDateTime &recstartts)
{
    if (recstatus == rsUnknown && rectype == kNotRecording)
        return QObject::tr("This showing is not scheduled to record");

    QString message;
    QDateTime now = MythDate::current();

    if (recstatus <= rsWillRecord ||
        recstatus == rsOtherShowing)
    {
        switch (recstatus)
        {
            case rsWillRecord:
                message = QObject::tr("This showing will be recorded.");
                break;
            case rsRecording:
                message = QObject::tr("This showing is being recorded.");
                break;
            case rsTuning:
                message = QObject::tr("The showing is being tuned.");
                break;
            case rsOtherRecording:
                message = QObject::tr("This showing is being recorded on "
                                      "a different channel.");
                break;
            case rsOtherTuning:
                message = QObject::tr("The showing is being tuned on a "
                                      "different channel.");
                break;
            case rsRecorded:
                message = QObject::tr("This showing was recorded.");
                break;
            case rsAborted:
                message = QObject::tr("This showing was recorded but was "
                                      "aborted before completion.");
                break;
            case rsMissed:
            case rsMissedFuture:
                message = QObject::tr("This showing was not recorded because "
                                      "the master backend was not running.");
                break;
            case rsCancelled:
                message = QObject::tr("This showing was not recorded because "
                                      "it was manually cancelled.");
                break;
            case rsLowDiskSpace:
                message = QObject::tr("This showing was not recorded because "
                                      "there wasn't enough disk space.");
                break;
            case rsTunerBusy:
                message = QObject::tr("This showing was not recorded because "
                                      "the recorder was already in use.");
                break;
            case rsFailed:
                message = QObject::tr("This showing was not recorded because "
                                      "the recorder failed.");
                break;
            case rsOtherShowing:
                message += QObject::tr("This showing will be recorded on a "
                                       "different channel.");
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
        case rsDontRecord:
            message += QObject::tr("it was manually set to not record.");
            break;
        case rsPreviousRecording:
            message += QObject::tr("this episode was previously recorded "
                                   "according to the duplicate policy chosen "
                                   "for this title.");
            break;
        case rsCurrentRecording:
            message += QObject::tr("this episode was previously recorded and "
                                   "is still available in the list of "
                                   "recordings.");
            break;
        case rsEarlierShowing:
            message += QObject::tr("this episode will be recorded at an "
                                   "earlier time instead.");
            break;
        case rsTooManyRecordings:
            message += QObject::tr("too many recordings of this program have "
                                   "already been recorded.");
            break;
        case rsConflict:
            message += QObject::tr("another program with a higher priority "
                                   "will be recorded.");
            break;
        case rsLaterShowing:
            message += QObject::tr("this episode will be recorded at a "
                                   "later time instead.");
            break;
        case rsRepeat:
            message += QObject::tr("this episode is a repeat.");
            break;
        case rsInactive:
            message += QObject::tr("this recording rule is inactive.");
            break;
        case rsNotListed:
            message += QObject::tr("this rule does not match any showings in "
                                   "the current program listings.");
            break;
        case rsNeverRecord:
            message += QObject::tr("it was marked to never be recorded.");
            break;
        case rsOffLine:
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

/* vim: set expandtab tabstop=4 shiftwidth=4: */
