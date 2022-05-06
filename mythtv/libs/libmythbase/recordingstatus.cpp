#include "recordingstatus.h"

#include "mythdate.h"

QString RecStatus::toUIState(RecStatus::Type recstatus)
{
    if (recstatus == RecStatus::Recorded ||
        recstatus == RecStatus::WillRecord ||
        recstatus == RecStatus::Pending)
        return "normal";

    if (recstatus == RecStatus::Recording ||
        recstatus == RecStatus::Tuning)
        return "running";

    if (recstatus == RecStatus::Conflict ||
        recstatus == RecStatus::Offline ||
        recstatus == RecStatus::TunerBusy ||
        recstatus == RecStatus::Failed ||
        recstatus == RecStatus::Aborted ||
        recstatus == RecStatus::Missed ||
        recstatus == RecStatus::Failing)
    {
        return "error";
    }

    if (recstatus == RecStatus::Repeat ||
        recstatus == RecStatus::NeverRecord ||
        recstatus == RecStatus::DontRecord   ||
        (recstatus != RecStatus::DontRecord &&
         recstatus <= RecStatus::EarlierShowing))
    {
        return "disabled";
    }

    return "warning";
}

/// \brief Converts "recstatus" into a short (unreadable) string.
QString RecStatus::toString(RecStatus::Type recstatus, uint id)
{
    return toString(recstatus, QString::number(id));
}

/// \brief Converts "recstatus" into a short (unreadable) string.
QString RecStatus::toString(RecStatus::Type recstatus, const QString &name)
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
        case RecStatus::Tuning:
        case RecStatus::Failing:
        case RecStatus::WillRecord:
        case RecStatus::Pending:
            ret = name;
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
        case RecStatus::Pending:
            return QObject::tr("Pending");
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
            case RecStatus::Pending:
                message = QObject::tr("This showing is about to record.");
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
