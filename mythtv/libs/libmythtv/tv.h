#ifndef TV_H
#define TV_H

#include <qstring.h>
#include <pthread.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "RingBuffer.h"

#include "channel.h"

#include "libmyth/programinfo.h"

typedef enum 
{
    kState_Error = -1,
    kState_None = 0,
    kState_WatchingLiveTV,
    kState_WatchingPreRecorded,
    kState_WatchingRecording,       // watching _what_ you're recording
    kState_WatchingOtherRecording,  // watching something else
    kState_RecordingOnly
} TVState;

#include "tv_play.h"
#include "tv_rec.h"

#endif
