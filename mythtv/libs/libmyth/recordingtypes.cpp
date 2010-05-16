#include <QObject>

#include "recordingtypes.h"

/// Converts a RecordingType to a simple integer so it's specificity can
/// be compared to another.  Lower number means more specific.
int RecTypePriority(RecordingType rectype)
{
    switch (rectype)
    {
        case kNotRecording:   return 0; break;
        case kDontRecord:     return 1; break;
        case kOverrideRecord: return 2; break;
        case kSingleRecord:   return 3; break;
        case kFindOneRecord:  return 4; break;
        case kWeekslotRecord: return 5; break;
        case kFindWeeklyRecord: return 6; break;
        case kTimeslotRecord: return 7; break;
        case kFindDailyRecord: return 8; break;
        case kChannelRecord:  return 9; break;
        case kAllRecord:      return 10; break;
        default: return 11;
     }
}

/// \brief Converts "rectype" into a human readable description.
QString toString(RecordingType rectype)
{
    switch (rectype)
    {
        case kSingleRecord:
            return QObject::tr("Single Record");
        case kTimeslotRecord:
            return QObject::tr("Record Daily");
        case kWeekslotRecord:
            return QObject::tr("Record Weekly");
        case kChannelRecord:
            return QObject::tr("Channel Record");
        case kAllRecord:
            return QObject::tr("Record All");
        case kFindOneRecord:
            return QObject::tr("Find One");
        case kFindDailyRecord:
            return QObject::tr("Find Daily");
        case kFindWeeklyRecord:
            return QObject::tr("Find Weekly");
        case kOverrideRecord:
        case kDontRecord:
            return QObject::tr("Override Recording");
        default:
            return QObject::tr("Not Recording");
    }
}

/// \brief Converts "rectype" into a human readable character.
QChar toQChar(RecordingType rectype)
{
    QString ret;
    switch (rectype)
    {
        case kSingleRecord:
            ret = QObject::tr("S", "RecTypeChar kSingleRecord");     break;
        case kTimeslotRecord:
            ret = QObject::tr("T", "RecTypeChar kTimeslotRecord");   break;
        case kWeekslotRecord:
            ret = QObject::tr("W", "RecTypeChar kWeekslotRecord");   break;
        case kChannelRecord:
            ret = QObject::tr("C", "RecTypeChar kChannelRecord");    break;
        case kAllRecord:
            ret = QObject::tr("A", "RecTypeChar kAllRecord");        break;
        case kFindOneRecord:
            ret = QObject::tr("F", "RecTypeChar kFindOneRecord");    break;
        case kFindDailyRecord:
            ret = QObject::tr("d", "RecTypeChar kFindDailyRecord");  break;
        case kFindWeeklyRecord:
            ret = QObject::tr("w", "RecTypeChar kFindWeeklyRecord"); break;
        case kOverrideRecord:
        case kDontRecord:
            ret = QObject::tr("O", "RecTypeChar kOverrideRecord/kDontRecord");
            break;
        case kNotRecording:
        default:
            ret = " ";
    }
    return (ret.isEmpty()) ? QChar(' ') : ret[0];
}
