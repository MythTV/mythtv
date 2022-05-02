#ifndef RECORDINGTYPES_H_
#define RECORDINGTYPES_H_

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

#include <QString>

#include "mythbaseexp.h"

enum RecordingType
{
    kNotRecording = 0,
    kSingleRecord = 1,
    kDailyRecord = 2,
    //kChannelRecord = 3, (Obsolete)
    kAllRecord = 4,
    kWeeklyRecord = 5,
    kOneRecord = 6,
    kOverrideRecord = 7,
    kDontRecord = 8,
    //kFindDailyRecord = 9, (Obsolete)
    //kFindWeeklyRecord = 10, (Obsolete)
    kTemplateRecord = 11
}; // note stored in uint8_t in ProgramInfo
MBASE_PUBLIC QString toString(RecordingType rectype);
MBASE_PUBLIC QString toDescription(RecordingType rectype);
MBASE_PUBLIC QString toRawString(RecordingType rectype);
MBASE_PUBLIC QChar   toQChar( RecordingType rectype);
MBASE_PUBLIC RecordingType recTypeFromString(const QString& type);

MBASE_PUBLIC int RecTypePrecedence(RecordingType rectype);

enum RecordingDupInType
{
    kDupsUnset          = 0x00,
    kDupsInRecorded     = 0x01,
    kDupsInOldRecorded  = 0x02,
    kDupsInAll          = 0x0F,
    kDupsNewEpi         = 0x10
}; // note stored in uint8_t in ProgramInfo
MBASE_PUBLIC QString toString(RecordingDupInType rectype);
MBASE_PUBLIC QString toDescription(RecordingDupInType rectype);
MBASE_PUBLIC QString toRawString(RecordingDupInType rectype);
MBASE_PUBLIC bool newEpifromDupIn(RecordingDupInType recdupin);
MBASE_PUBLIC RecordingDupInType dupInFromString(const QString& type);
MBASE_PUBLIC RecordingDupInType dupInFromStringAndBool(const QString& type, bool newEpisodesOnly);

enum RecordingDupMethodType
{
    kDupCheckUnset    = 0x00,
    kDupCheckNone     = 0x01,
    kDupCheckSub      = 0x02,
    kDupCheckDesc     = 0x04,
    kDupCheckSubDesc  = 0x06,
    kDupCheckSubThenDesc = 0x08
}; // note stored in uint8_t in ProgramInfo
MBASE_PUBLIC QString toString(RecordingDupMethodType rectype);
MBASE_PUBLIC QString toDescription(RecordingDupMethodType rectype);
MBASE_PUBLIC QString toRawString(RecordingDupMethodType rectype);
MBASE_PUBLIC RecordingDupMethodType dupMethodFromString(const QString& type);

enum RecSearchType
{
    kNoSearch = 0,
    kPowerSearch,
    kTitleSearch,
    kKeywordSearch,
    kPeopleSearch,
    kManualSearch
};
MBASE_PUBLIC QString toString(RecSearchType rectype);
MBASE_PUBLIC QString toRawString(RecSearchType rectype);
MBASE_PUBLIC RecSearchType searchTypeFromString(const QString& type);

enum class AutoExtendType : uint8_t
{
    None = 0,
    ESPN,
    MLB,
    Last
};
MBASE_PUBLIC QString toString(AutoExtendType extType);
MBASE_PUBLIC QString toDescription(AutoExtendType extType);
MBASE_PUBLIC AutoExtendType autoExtendTypeFromString(const QString& type);
MBASE_PUBLIC AutoExtendType autoExtendTypeFromInt(uint8_t type);

#endif

