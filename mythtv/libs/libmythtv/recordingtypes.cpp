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
        case kTimeslotRecord: return 6; break;
        case kChannelRecord:  return 7; break;
        case kAllRecord:      return 9; break;
        default: return 9;
     }
}

