#ifndef AUDIOOUTPUTDOWNMIX
#define AUDIOOUTPUTDOWNMIX

class AudioOutputDownmix
{
public:
    static int DownmixFrames(int channels_in, int  channels_out,
                             float *dst, const float *src, int frames);
};

#endif
