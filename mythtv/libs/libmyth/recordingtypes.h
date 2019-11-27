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
MPUBLIC QString toString(RecordingType);
MPUBLIC QString toDescription(RecordingType);
MPUBLIC QString toRawString(RecordingType);
MPUBLIC QChar   toQChar( RecordingType);
MPUBLIC RecordingType recTypeFromString(const QString&);

MPUBLIC int RecTypePrecedence(RecordingType rectype);

enum RecordingDupInType
{
    kDupsUnset          = 0x00,
    kDupsInRecorded     = 0x01,
    kDupsInOldRecorded  = 0x02,
    kDupsInAll          = 0x0F,
    kDupsNewEpi         = 0x10
}; // note stored in uint8_t in ProgramInfo
MPUBLIC QString toString(RecordingDupInType);
MPUBLIC QString toDescription(RecordingDupInType);
MPUBLIC QString toRawString(RecordingDupInType);
MPUBLIC RecordingDupInType dupInFromString(const QString&);

enum RecordingDupMethodType
{
    kDupCheckUnset    = 0x00,
    kDupCheckNone     = 0x01,
    kDupCheckSub      = 0x02,
    kDupCheckDesc     = 0x04,
    kDupCheckSubDesc  = 0x06,
    kDupCheckSubThenDesc = 0x08
}; // note stored in uint8_t in ProgramInfo
MPUBLIC QString toString(RecordingDupMethodType);
MPUBLIC QString toDescription(RecordingDupMethodType);
MPUBLIC QString toRawString(RecordingDupMethodType);
MPUBLIC RecordingDupMethodType dupMethodFromString(const QString&);

enum RecSearchType
{
    kNoSearch = 0,
    kPowerSearch,
    kTitleSearch,
    kKeywordSearch,
    kPeopleSearch,
    kManualSearch
};
MPUBLIC QString toString(RecSearchType);
MPUBLIC QString toRawString(RecSearchType);
MPUBLIC RecSearchType searchTypeFromString(const QString&);

#endif

