#include "tv.h"

/** \fn StateToString(TVState state)
 *  \brief Returns a human readable QString representing a TVState.
 *  \param state State to print.
 */
QString StateToString(TVState state)
{
    QString statestr = QString("Unknown(%1)").arg((int)state);
    switch (state) {
        case kState_Error: statestr = "Error"; break;
        case kState_None: statestr = "None"; break;
        case kState_WatchingLiveTV: statestr = "WatchingLiveTV"; break;
        case kState_WatchingPreRecorded:
            statestr = "WatchingPreRecorded"; break;
        case kState_WatchingRecording: statestr = "WatchingRecording"; break;
        case kState_RecordingOnly: statestr = "RecordingOnly"; break;
        case kState_ChangingState: statestr = "ChangingState"; break;
    }
    return statestr;
}
