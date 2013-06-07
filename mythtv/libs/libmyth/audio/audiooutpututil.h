#ifndef AUDIOOUTPUTUTIL_H_
#define AUDIOOUTPUTUTIL_H_

#include "audiooutputsettings.h"

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
    static int DecodeAudio(AVCodecContext *ctx,
                           uint8_t *buffer, int &data_size,
                           const AVPacket *pkt);
    static void DeinterleaveSamples(AudioFormat format, int channels,
                                    uint8_t *output, const uint8_t *input,
                                    int data_size);
    static void InterleaveSamples(AudioFormat format, int channels,
                                  uint8_t *output, const uint8_t * const *input,
                                  int data_size);
    static void InterleaveSamples(AudioFormat format, int channels,
                                  uint8_t *output, const uint8_t *input,
                                  int data_size);
};

#endif
