#ifndef RECORDINGTYPES_H_
#define RECORDINGTYPES_H_

#include "mythexp.h"

enum RecordingType
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
};

MPUBLIC int RecTypePriority(RecordingType rectype);

enum RecordingDupInType
{
    kDupsInRecorded     = 0x01,
    kDupsInOldRecorded  = 0x02,
    kDupsInAll          = 0x0F,
    kDupsNewEpi         = 0x10,
    kDupsExRepeats      = 0x20,
    kDupsExGeneric      = 0x40
};


enum RecordingDupMethodType
{
    kDupCheckNone     = 0x01,
    kDupCheckSub      = 0x02,
    kDupCheckDesc     = 0x04,
    kDupCheckSubDesc  = 0x06,
    kDupCheckSubThenDesc = 0x08
};

enum RecSearchType
{
    kNoSearch = 0,
    kPowerSearch,
    kTitleSearch,
    kKeywordSearch,
    kPeopleSearch,
    kManualSearch
};

#endif

