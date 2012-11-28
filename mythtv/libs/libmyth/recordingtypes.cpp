#include <QObject>

#include "recordingtypes.h"

/// Converts a RecordingType to a simple integer so it's specificity can
/// be compared to another.  Lower number means more specific.
int RecTypePrecedence(RecordingType rectype)
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
        case kTemplateRecord: return 0; break;
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
        case kTemplateRecord:
            return QObject::tr("Template Recording");
        default:
            return QObject::tr("Not Recording");
    }
}

/// \brief Converts "rectype" into an untranslated description.
QString toRawString(RecordingType rectype)
{
    switch (rectype)
    {
        case kSingleRecord:
            return QString("Single Record");
        case kTimeslotRecord:
            return QString("Record Daily");
        case kWeekslotRecord:
            return QString("Record Weekly");
        case kChannelRecord:
            return QString("Channel Record");
        case kAllRecord:
            return QString("Record All");
        case kFindOneRecord:
            return QString("Find One");
        case kFindDailyRecord:
            return QString("Find Daily");
        case kFindWeeklyRecord:
            return QString("Find Weekly");
        case kOverrideRecord:
        case kDontRecord:
            return QString("Override Recording");
        default:
            return QString("Not Recording");
    }
}

RecordingType recTypeFromString(QString type)
{
    if (type.toLower() == "not recording" || type.toLower() == "not")
        return kNotRecording;
    if (type.toLower() == "single record" || type.toLower() == "single")
        return kSingleRecord;
    else if (type.toLower() == "record daily" || type.toLower() == "daily")
        return kTimeslotRecord;
    else if (type.toLower() == "record weekly" || type.toLower() == "weekly")
        return kWeekslotRecord;
    else if (type.toLower() == "channel record" || type.toLower() == "channel")
        return kChannelRecord;
    else if (type.toLower() == "record all" || type.toLower() == "all")
        return kAllRecord;
    else if (type.toLower() == "find one" || type.toLower() == "findone")
        return kFindOneRecord;
    else if (type.toLower() == "find daily" || type.toLower() == "finddaily")
        return kFindDailyRecord;
    else if (type.toLower() == "find weekly" || type.toLower() == "findweekly")
        return kFindWeeklyRecord;
    else if (type.toLower() == "template" || type.toLower() == "template")
        return kTemplateRecord;
    else if (type.toLower() == "override recording" || type.toLower() == "override")
        return kOverrideRecord;
    else
        return kDontRecord;
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
        case kTemplateRecord:
            ret = QObject::tr("t", "RecTypeChar kTemplateRecord");   break;
        case kNotRecording:
        default:
            ret = " ";
    }
    return (ret.isEmpty()) ? QChar(' ') : ret[0];
}

QString toRawString(RecordingDupInType recdupin)
{
    switch (recdupin)
    {
        case kDupsInRecorded:
            return QString("Current Recordings");
        case kDupsInOldRecorded:
            return QString("Previous Recordings");
        case kDupsInAll:
            return QString("All Recordings");
        case kDupsNewEpi:
            return QString("New Episodes Only");
        default:
            return QString("Unknown");
    }
}

RecordingDupInType dupInFromString(QString type)
{
    if (type.toLower() == "current recordings" || type.toLower() == "current")
        return kDupsInRecorded;
    else if (type.toLower() == "previous recordings" || type.toLower() == "previous")
        return kDupsInOldRecorded;
    else if (type.toLower() == "all recordings" || type.toLower() == "all")
        return kDupsInAll;
    else if (type.toLower() == "new episodes only" || type.toLower() == "new")
        return kDupsNewEpi;
    else
        return kDupsInAll;
}

QString toRawString(RecordingDupMethodType duptype)
{
    switch (duptype)
    {
        case kDupCheckNone:
            return QString("None");
        case kDupCheckSub:
            return QString("Subtitle");
        case kDupCheckDesc:
            return QString("Description");
        case kDupCheckSubDesc:
            return QString("Subtitle and Description");
        case kDupCheckSubThenDesc:
            return QString("Subtitle then Description");
        default:
            return QString("Unknown");
    }
}

RecordingDupMethodType dupMethodFromString(QString type)
{
    if (type.toLower() == "none")
        return kDupCheckNone;
    else if (type.toLower() == "subtitle")
        return kDupCheckSub;
    else if (type.toLower() == "description")
        return kDupCheckDesc;
    else if (type.toLower() == "subtitle and description" || type.toLower() == "subtitleanddescription")
        return kDupCheckSubDesc;
    else if (type.toLower() == "subtitle then description" || type.toLower() == "subtitlethendescription")
        return kDupCheckSubThenDesc;
    else
        return kDupCheckSubDesc;
}

QString toRawString(RecSearchType searchtype)
{
    switch (searchtype)
    {
        case kNoSearch:
            return QString("None");
        case kPowerSearch:
            return QString("Power Search");
        case kTitleSearch:
            return QString("Title Search");
        case kKeywordSearch:
            return QString("Keyword Search");
        case kPeopleSearch:
            return QString("People Search");
        case kManualSearch:
            return QString("Manual Search");
        default:
            return QString("Unknown");
    }
}

RecSearchType searchTypeFromString(QString type)
{
    if (type.toLower() == "none")
        return kNoSearch;
    else if (type.toLower() == "power search" || type.toLower() == "power")
        return kPowerSearch;
    else if (type.toLower() == "title search" || type.toLower() == "title")
        return kTitleSearch;
    else if (type.toLower() == "keyword search" || type.toLower() == "keyword")
        return kKeywordSearch;
    else if (type.toLower() == "people search" || type.toLower() == "people")
        return kPeopleSearch;
    else if (type.toLower() == "manual search" || type.toLower() == "manual")
        return kManualSearch;
    else
        return kNoSearch;
}
