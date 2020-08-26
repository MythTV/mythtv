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

#include "mythexp.h"

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
MPUBLIC QString toString(RecordingType rectype);
MPUBLIC QString toDescription(RecordingType rectype);
MPUBLIC QString toRawString(RecordingType rectype);
MPUBLIC QChar   toQChar( RecordingType rectype);
MPUBLIC RecordingType recTypeFromString(const QString& type);

MPUBLIC int RecTypePrecedence(RecordingType rectype);

enum RecordingDupInType
{
    kDupsUnset          = 0x00,
    kDupsInRecorded     = 0x01,
    kDupsInOldRecorded  = 0x02,
    kDupsInAll          = 0x0F,
    kDupsNewEpi         = 0x10
}; // note stored in uint8_t in ProgramInfo
MPUBLIC QString toString(RecordingDupInType rectype);
MPUBLIC QString toDescription(RecordingDupInType rectype);
MPUBLIC QString toRawString(RecordingDupInType rectype);
MPUBLIC bool newEpifromDupIn(RecordingDupInType recdupin);
MPUBLIC RecordingDupInType dupInFromString(const QString& type);
MPUBLIC RecordingDupInType dupInFromStringAndBool(const QString& type, bool newEpisodesOnly);

enum RecordingDupMethodType
{
    kDupCheckUnset    = 0x00,
    kDupCheckNone     = 0x01,
    kDupCheckSub      = 0x02,
    kDupCheckDesc     = 0x04,
    kDupCheckSubDesc  = 0x06,
    kDupCheckSubThenDesc = 0x08
}; // note stored in uint8_t in ProgramInfo
MPUBLIC QString toString(RecordingDupMethodType rectype);
MPUBLIC QString toDescription(RecordingDupMethodType rectype);
MPUBLIC QString toRawString(RecordingDupMethodType rectype);
MPUBLIC RecordingDupMethodType dupMethodFromString(const QString& type);

enum RecSearchType
{
    kNoSearch = 0,
    kPowerSearch,
    kTitleSearch,
    kKeywordSearch,
    kPeopleSearch,
    kManualSearch
};
MPUBLIC QString toString(RecSearchType rectype);
MPUBLIC QString toRawString(RecSearchType rectype);
MPUBLIC RecSearchType searchTypeFromString(const QString& type);

#endif

