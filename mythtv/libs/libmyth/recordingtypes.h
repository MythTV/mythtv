#ifndef RECORDINGTYPES_H_
#define RECORDINGTYPES_H_

#include <QString>

#include "mythexp.h"

typedef enum RecordingTypes
{
    kNotRecording = 0,
    kSingleRecord = 1,
    kTimeslotRecord,
    kChannelRecord,
    kAllRecord,
    kWeekslotRecord,
    kFindOneRecord,
    kOverrideRecord,
    kDontRecord,
    kFindDailyRecord,
    kFindWeeklyRecord
} RecordingType; // note stored in uin8_t in ProgramInfo
MPUBLIC QString toString(RecordingType);
MPUBLIC QChar   toQChar( RecordingType);

MPUBLIC int RecTypePriority(RecordingType rectype);

typedef enum RecordingDupInTypes
{
    kDupsInRecorded     = 0x01,
    kDupsInOldRecorded  = 0x02,
    kDupsInAll          = 0x0F,
    kDupsNewEpi         = 0x10,
    kDupsExRepeats      = 0x20,
    kDupsExGeneric      = 0x40,
    kDupsFirstNew       = 0x80
} RecordingDupInType; // note stored in uin8_t in ProgramInfo


typedef enum RecordingDupMethodType
{
    kDupCheckNone     = 0x01,
    kDupCheckSub      = 0x02,
    kDupCheckDesc     = 0x04,
    kDupCheckSubDesc  = 0x06,
    kDupCheckSubThenDesc = 0x08
} RecordingDupMethodType; // note stored in uin8_t in ProgramInfo

typedef enum RecSearchTypes
{
    kNoSearch = 0,
    kPowerSearch,
    kTitleSearch,
    kKeywordSearch,
    kPeopleSearch,
    kManualSearch
} RecSearchType;

#endif

