#ifndef TV_H
#define TV_H

#define CHANNEL_DIRECTION_UP 0
#define CHANNEL_DIRECTION_DOWN 1
#define CHANNEL_DIRECTION_FAVORITE 2

typedef enum 
{
    kState_Error = -1,
    kState_None = 0,
    kState_WatchingLiveTV,
    kState_WatchingPreRecorded,
    kState_WatchingRecording,       // watching what you're recording
    kState_RecordingOnly
} TVState;

#include "tv_play.h"
#include "tv_rec.h"

#endif
