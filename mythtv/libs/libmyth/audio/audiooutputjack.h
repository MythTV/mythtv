#ifndef AUDIOOUTPUTJACK
#define AUDIOOUTPUTJACK

// Qt headers
#include <QCoreApplication>

#include <jack/jack.h>
#include <jack/statistics.h>
#include "audiooutputbase.h"
#include "audiooutputsettings.h"

//! maximum number of channels supported, avoids lots of mallocs
static constexpr int8_t JACK_CHANNELS_MIN { 2 };
static constexpr int8_t JACK_CHANNELS_MAX { 8 };

using jack_port_array = std::array<jack_port_t*,JACK_CHANNELS_MAX>;
using jack_vol_array  = std::array<int,JACK_CHANNELS_MAX>;

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
    static int JackCallbackHelper(jack_nframes_t nframes, void *arg);
    inline int JackXRunCallback();
    static int JackXRunCallbackHelper(void *arg);
    inline int JackGraphOrderCallback();
    static int JackGraphOrderCallbackHelper(void *arg);

    static jack_client_t* JackClientOpen(void);
    const char** JackGetPorts(void);
    bool JackConnectPorts(const char** /*matching_ports*/);
    inline void JackClientClose(jack_client_t **client);

    void DeinterleaveAudio(const float *aubuf, float **bufs,
                           int nframes, const jack_vol_array& channel_volumes);

    jack_port_array m_ports       {};
    jack_vol_array m_chanVolumes  {};
    jack_client_t *m_client       {nullptr};
    jack_client_t *m_staleClient  {nullptr};
    int            m_jackLatency  {0};
    int            m_jackXruns    {0};
    unsigned char *m_auBuf        {nullptr};


};

#endif

