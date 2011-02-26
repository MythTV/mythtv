#ifndef AUDIOOUTPUTUTIL
#define AUDIOOUTPUTUTIL

using namespace std;
#include "mythverbose.h"
#include "audiooutputsettings.h"
extern "C" {
#include "pink.h"
}

class MPUBLIC AudioOutputUtil
{
 public:
    static bool has_hardware_fpu();
    static int  toFloat(AudioFormat format, void *out, void *in, int bytes);
    static int  fromFloat(AudioFormat format, void *out, void *in, int bytes);
    static void MonoToStereo(void *dst, void *src, int samples);
    static void AdjustVolume(void *buffer, int len, int volume,
                             bool music, bool upmix);
    static void MuteChannel(int obits, int channels, int ch,
                            void *buffer, int bytes);
    static char *GeneratePinkFrames(char *frames, int channels,
                                    int channel, int count, int bits = 16);
};

#endif
