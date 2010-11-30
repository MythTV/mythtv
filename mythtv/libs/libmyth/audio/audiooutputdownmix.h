#ifndef AUDIOOUTPUTDOWNMIX
#define AUDIOOUTPUTDOWNMIX

class AudioOutputDownmix
{
public:
    static int DownmixFrames(int channels_in, int  channels_out,
                             float *dst, float *src, int frames);
};

#endif
