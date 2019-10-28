#ifndef AUDIOOUTPUTJACK
#define AUDIOOUTPUTJACK

// Qt headers
#include <QCoreApplication>

#include <jack/jack.h>
#include <jack/statistics.h>
#include "audiooutputbase.h"
#include "audiooutputsettings.h"

//! maximum number of channels supported, avoids lots of mallocs
#define JACK_CHANNELS_MIN 2
#define JACK_CHANNELS_MAX 8

class AudioOutputJACK : public AudioOutputBase
{
    Q_DECLARE_TR_FUNCTIONS(AudioOutputJACK);

  public:
    explicit AudioOutputJACK(const AudioSettings &settings);
    ~AudioOutputJACK() override;

    // Volume control
    int GetVolumeChannel(int channel) const override; // VolumeBase
    void SetVolumeChannel(int channel, int volume) override; // VolumeBase

  protected:

    // You need to implement the following functions
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    AudioOutputSettings* GetOutputSettings(bool digital) override; // AudioOutputBase

    // Overriding these to do nothing.  Not needed here.
    bool StartOutputThread(void) override; // AudioOutputBase
    void StopOutputThread(void) override; // AudioOutputBase

  private:

    void VolumeInit(void);

    // Our various callback functions
    inline int JackCallback(jack_nframes_t nframes);
    static int _JackCallback(jack_nframes_t nframes, void *arg);
    inline int JackXRunCallback();
    static int _JackXRunCallback(void *arg);
    inline int JackGraphOrderCallback();
    static int _JackGraphOrderCallback(void *arg);

    static jack_client_t* _jack_client_open(void);
    const char** _jack_get_ports(void);
    bool _jack_connect_ports(const char** /*matching_ports*/);
    inline void _jack_client_close(jack_client_t **client);

    void DeinterleaveAudio(const float *aubuf, float **bufs,
                           int nframes, const int* channel_volumes);

    jack_port_t   *m_ports[JACK_CHANNELS_MAX];
    int            m_chan_volumes[JACK_CHANNELS_MAX];
    jack_client_t *m_client       {nullptr};
    jack_client_t *m_stale_client {nullptr};
    int            m_jack_latency {0};
    int            m_jack_xruns   {0};
    unsigned char *m_aubuf        {nullptr};


};

#endif

