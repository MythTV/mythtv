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
        case kOneRecord:      return 4; break;
        case kWeeklyRecord:   return 6; break;
        case kDailyRecord:    return 8; break;
        case kAllRecord:      return 9; break;
        case kTemplateRecord: return 0; break;
        default: return 11;
     }
}

/// \brief Converts "rectype" into a human readable string.
QString toString(RecordingType rectype)
{
    switch (rectype)
    {
        case kSingleRecord:
            return QObject::tr("Single Record");
        case kAllRecord:
            return QObject::tr("Record All");
        case kOneRecord:
            return QObject::tr("Record One");
        case kDailyRecord:
            return QObject::tr("Record Daily");
        case kWeeklyRecord:
            return QObject::tr("Record Weekly");
        case kOverrideRecord:
        case kDontRecord:
            return QObject::tr("Override Recording");
        case kTemplateRecord:
            return QObject::tr("Recording Template");
        default:
            return QObject::tr("Not Recording");
    }
}

/// \brief Converts "rectype" into a human readable description.
QString toDescription(RecordingType rectype)
{
    switch (rectype)
    {
        case kSingleRecord:
            return QObject::tr("Record only this showing");
        case kAllRecord:
            return QObject::tr("Record all showings");
        case kOneRecord:
            return QObject::tr("Record only one showing");
        case kDailyRecord:
            return QObject::tr("Record one showing every day");
        case kWeeklyRecord:
            return QObject::tr("Record one showing every week");
        case kOverrideRecord:
            return QObject::tr("Record this showing with override options");
        case kDontRecord:
            return QObject::tr("Do not record this showing");
        case kTemplateRecord:
            return QObject::tr("Modify this recording rule template");
        default:
            return QObject::tr("Do not record this program");
    }
}

/// \brief Converts "rectype" into an untranslated string.
QString toRawString(RecordingType rectype)
{
    switch (rectype)
    {
        case kSingleRecord:
            return QString("Single Record");
        case kAllRecord:
            return QString("Record All");
        case kOneRecord:
            return QString("Record One");
        case kDailyRecord:
            return QString("Record Daily");
        case kWeeklyRecord:
            return QString("Record Weekly");
        case kOverrideRecord:
            return QString("Override Recording");
        case kDontRecord:
            return QString("Do not Record");
        case kTemplateRecord:
            return QString("Recording Template");
        default:
            return QString("Not Recording");
    }
}

RecordingType recTypeFromString(const QString& type)
{
    if (type.toLower() == "not recording" || type.toLower() == "not")
        return kNotRecording;
    if (type.toLower() == "single record" || type.toLower() == "single")
        return kSingleRecord;
    if (type.toLower() == "record all" || type.toLower() == "all")
        return kAllRecord;
    if (type.toLower() == "record one" || type.toLower() == "one" ||
        type.toLower() == "find one" || type.toLower() == "findone")
        return kOneRecord;
    if (type.toLower() == "record daily" || type.toLower() == "daily" ||
        type.toLower() == "find daily" || type.toLower() == "finddaily")
        return kDailyRecord;
    if (type.toLower() == "record weekly" || type.toLower() == "weekly" ||
        type.toLower() == "find weekly" || type.toLower() == "findweekly")
        return kWeeklyRecord;
    if (type.toLower() == "recording template" || type.toLower() == "template")
        return kTemplateRecord;
    if (type.toLower() == "override recording" || type.toLower() == "override")
        return kOverrideRecord;
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
        case kAllRecord:
            ret = QObject::tr("A", "RecTypeChar kAllRecord");        break;
        case kOneRecord:
            ret = QObject::tr("1", "RecTypeChar kOneRecord");    break;
        case kDailyRecord:
            ret = QObject::tr("D", "RecTypeChar kDailyRecord");  break;
        case kWeeklyRecord:
            ret = QObject::tr("W", "RecTypeChar kWeeklyRecord"); break;
        case kOverrideRecord:
        case kDontRecord:
            ret = QObject::tr("O", "RecTypeChar kOverrideRecord/kDontRecord");
            break;
        case kTemplateRecord:
            ret = QObject::tr("T", "RecTypeChar kTemplateRecord");   break;
        case kNotRecording:
        default:
            ret = " ";
    }
    return (ret.isEmpty()) ? QChar(' ') : ret[0];
}

QString toString(RecordingDupInType recdupin)
{
    switch (recdupin)
    {
        case kDupsInRecorded:
            return QObject::tr("Current Recordings");
        case kDupsInOldRecorded:
            return QObject::tr("Previous Recordings");
        case kDupsInAll:
            return QObject::tr("All Recordings");
        // TODO: This is wrong, kDupsNewEpi is returned in conjunction with
        // one of the other values.
        case kDupsNewEpi:
            return QObject::tr("New Episodes Only");
        default:
            return QString();
    }
}

QString toDescription(RecordingDupInType recdupin)
{
    switch (recdupin)
    {
        case kDupsInRecorded:
            return QObject::tr("Look for duplicates in current recordings only");
        case kDupsInOldRecorded:
            return QObject::tr("Look for duplicates in previous recordings only");
        case kDupsInAll:
            return QObject::tr("Look for duplicates in current and previous "
                               "recordings");
        // TODO: This is wrong, kDupsNewEpi is returned in conjunction with
        // one of the other values.
        case kDupsNewEpi:
            return QObject::tr("Record new episodes only");
        default:
            return QString();
    }
}

QString toRawString(RecordingDupInType recdupin)
{
    // Remove "New Episodes" flag
    recdupin = (RecordingDupInType) (recdupin & (-1 - kDupsNewEpi));
    switch (recdupin)
    {
        case kDupsInRecorded:
            return QString("Current Recordings");
        case kDupsInOldRecorded:
            return QString("Previous Recordings");
        case kDupsInAll:
            return QString("All Recordings");
        default:
            return QString("Unknown");
    }
}

// New Episodes Only is a flag added to DupIn
bool newEpifromDupIn(RecordingDupInType recdupin)
{
    return (recdupin & kDupsNewEpi);
}

RecordingDupInType dupInFromString(const QString& type)
{
    if (type.toLower() == "current recordings" || type.toLower() == "current")
        return kDupsInRecorded;
    if (type.toLower() == "previous recordings" || type.toLower() == "previous")
        return kDupsInOldRecorded;
    if (type.toLower() == "all recordings" || type.toLower() == "all")
        return kDupsInAll;
    if (type.toLower() == "new episodes only" || type.toLower() == "new")
        return static_cast<RecordingDupInType> (kDupsInAll | kDupsNewEpi);
    return kDupsInAll;
}

RecordingDupInType dupInFromStringAndBool(const QString& type, bool newEpisodesOnly) {
    RecordingDupInType result = dupInFromString(type);
    if (newEpisodesOnly)
        result = static_cast<RecordingDupInType> (result | kDupsNewEpi);
    return result;
}

QString toString(RecordingDupMethodType duptype)
{
    switch (duptype)
    {
        case kDupCheckNone:
            return QObject::tr("None");
        case kDupCheckSub:
            return QObject::tr("Subtitle");
        case kDupCheckDesc:
            return QObject::tr("Description");
        case kDupCheckSubDesc:
            return QObject::tr("Subtitle and Description");
        case kDupCheckSubThenDesc:
            return QObject::tr("Subtitle then Description");
        default:
            return QString();
    }
}

QString toDescription(RecordingDupMethodType duptype)
{
    switch (duptype)
    {
        case kDupCheckNone:
            return QObject::tr("Don't match duplicates");
        case kDupCheckSub:
            return QObject::tr("Match duplicates using subtitle");
        case kDupCheckDesc:
            return QObject::tr("Match duplicates using description");
        case kDupCheckSubDesc:
            return QObject::tr("Match duplicates using subtitle & description");
        case kDupCheckSubThenDesc:
            return QObject::tr("Match duplicates using subtitle then description");
        default:
            return QString();
    }
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

RecordingDupMethodType dupMethodFromString(const QString& type)
{
    if (type.toLower() == "none")
        return kDupCheckNone;
    if (type.toLower() == "subtitle")
        return kDupCheckSub;
    if (type.toLower() == "description")
        return kDupCheckDesc;
    if (type.toLower() == "subtitle and description" || type.toLower() == "subtitleanddescription")
        return kDupCheckSubDesc;
    if (type.toLower() == "subtitle then description" || type.toLower() == "subtitlethendescription")
        return kDupCheckSubThenDesc;
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

RecSearchType searchTypeFromString(const QString& type)
{
    if (type.toLower() == "none")
        return kNoSearch;
    if (type.toLower() == "power search" || type.toLower() == "power")
        return kPowerSearch;
    if (type.toLower() == "title search" || type.toLower() == "title")
        return kTitleSearch;
    if (type.toLower() == "keyword search" || type.toLower() == "keyword")
        return kKeywordSearch;
    if (type.toLower() == "people search" || type.toLower() == "people")
        return kPeopleSearch;
    if (type.toLower() == "manual search" || type.toLower() == "manual")
        return kManualSearch;
    return kNoSearch;
}

QString toString(AutoExtendType extType)
{
    switch (extType)
    {
        default:
            return QString();
        case AutoExtendType::None:
            return QStringLiteral(u"None");
        case AutoExtendType::ESPN:
            return QStringLiteral(u"ESPN");
        case AutoExtendType::MLB:
            return QStringLiteral(u"MLB");
    }
}

QString toDescription(AutoExtendType extType)
{
    switch (extType)
    {
        default:
            return QString();
        case AutoExtendType::None:
            return QObject::tr("Do not automatically extend recording");
        case AutoExtendType::ESPN:
            return QObject::tr("Automatically extend recording using ESPN");
        case AutoExtendType::MLB:
            return QObject::tr("Automatically extend recording using MLB");
    }
}

AutoExtendType autoExtendTypeFromInt(uint8_t type)
{
    if (type >= static_cast<uint8_t>(AutoExtendType::Last))
        return AutoExtendType::None;
    return static_cast<AutoExtendType>(type);
}

AutoExtendType autoExtendTypeFromString(const QString& type)
{
    if (type.toLower() == "espn")
        return AutoExtendType::ESPN;
    if (type.toLower() == "mlb")
        return AutoExtendType::MLB;
    return AutoExtendType::None;
}
