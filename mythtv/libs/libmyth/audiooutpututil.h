#ifndef AUDIOOUTPUTUTIL
#define AUDIOOUTPUTUTIL

using namespace std;
#include "mythverbose.h"
#include "audiooutputsettings.h"

class AudioOutputUtil
{
 public:
    static int  toFloat(AudioFormat format, void *out, void *in, int bytes);
    static int  fromFloat(AudioFormat format, void *out, void *in, int bytes);
    static void MonoToStereo(void *dst, void *src, int samples);
    static void AdjustVolume(void *buffer, int len, int volume,
                             bool music, bool upmix);
    static void MuteChannel(int obits, int channels, int ch,
                            void *buffer, int bytes);
};

#endif
