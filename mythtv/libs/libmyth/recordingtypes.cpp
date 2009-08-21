#include "recordingtypes.h"

// Convert a RecordingType to a simple integer so it's specificity can
// be compared to another.  Lower number means more specific.
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

