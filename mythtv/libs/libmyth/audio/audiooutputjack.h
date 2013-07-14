#ifndef AUDIOOUTPUTJACK
#define AUDIOOUTPUTJACK

// Qt headers
#include <QCoreApplication>

#include <jack/jack.h>
#include <jack/statistics.h>
#include "audiooutputbase.h"
#include "audiooutputsettings.h"

using namespace std;

//! maximum number of channels supported, avoids lots of mallocs
#define JACK_CHANNELS_MIN 2
#define JACK_CHANNELS_MAX 8

class AudioOutputJACK : public AudioOutputBase
{
    Q_DECLARE_TR_FUNCTIONS(AudioOutputJACK)

  public:
    AudioOutputJACK(const AudioSettings &settings);
    virtual ~AudioOutputJACK();

    // Volume control
    virtual int GetVolumeChannel(int channel) const; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume); // range 0-100

  protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual int  GetBufferedOnSoundcard(void) const;
    AudioOutputSettings* GetOutputSettings(bool digital);

    // Overriding these to do nothing.  Not needed here.
    virtual bool StartOutputThread(void);
    virtual void StopOutputThread(void);

  private:

    void VolumeInit(void);

    // Our various callback functions
    inline int JackCallback(jack_nframes_t nframes);
    static int _JackCallback(jack_nframes_t nframes, void *arg);
    inline int JackXRunCallback();
    static int _JackXRunCallback(void *arg);
    inline int JackGraphOrderCallback();
    static int _JackGraphOrderCallback(void *arg);

    jack_client_t* _jack_client_open(void);
    const char** _jack_get_ports(void);
    bool _jack_connect_ports(const char**);
    inline void _jack_client_close(jack_client_t **client);

    void DeinterleaveAudio(float *aubuf, float **bufs,
                           int nframes, int* channel_volumes);

    jack_port_t *ports[JACK_CHANNELS_MAX];
    int chan_volumes[JACK_CHANNELS_MAX];
    jack_client_t *client, *stale_client;
    int jack_latency;
    bool jack_underrun;
    int jack_xruns;
    unsigned char *aubuf;


};

#endif

