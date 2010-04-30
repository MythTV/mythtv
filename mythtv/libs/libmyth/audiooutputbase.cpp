// Std C headers
#include <cmath>
#include <limits>

// POSIX headers
#include <unistd.h>
#include <sys/time.h>

// Qt headers
#include <QMutexLocker>

// MythTV headers
#include "compat.h"
#include "audiooutputbase.h"
#include "audiooutputdigitalencoder.h"
#include "SoundTouch.h"
#include "freesurround.h"

#define LOC QString("AO: ")
#define LOC_ERR QString("AO, ERROR: ")

AudioOutputBase::AudioOutputBase(const AudioSettings &settings) :
    // protected
    effdsp(0),                  effdspstretched(0),
    audio_channels(-1),         audio_codec(CODEC_ID_NONE),
    audio_bytes_per_sample(0),  audio_bits(-1),
    audio_samplerate(-1),       audio_buffer_unused(0),
    fragment_size(0),           soundcard_buffer_size(0),

    audio_main_device(settings.GetMainDevice()),
    audio_passthru_device(settings.GetPassthruDevice()),
    audio_passthru(false),      audio_enc(false),
    audio_reenc(false),         audio_stretchfactor(1.0f),

    source(settings.source),    killaudio(false),

    pauseaudio(false),          audio_actually_paused(false),
    was_paused(false),

    set_initial_vol(settings.set_initial_vol),
    buffer_output_data_for_use(false),

    // private
    need_resampler(false),

    src_ctx(NULL),

    pSoundStretch(NULL),
    encoder(NULL),
    upmixer(NULL),
    source_audio_channels(-1),
    source_audio_samplerate(0),
    source_audio_bytes_per_sample(0),
    needs_upmix(false),
    surround_mode(FreeSurround::SurroundModePassive),
    old_audio_stretchfactor(1.0),
    volume(80),

    blocking(false),

    lastaudiolen(0),            samples_buffered(0),

    audio_thread_exists(false),

    audiotime(0),
    raud(0),                    waud(0),
    audbuf_timecode(0),

    numlowbuffer(0),            killAudioLock(QMutex::NonRecursive),
    current_seconds(-1),        source_bitrate(-1),

    memory_corruption_test0(0xdeadbeef),
    memory_corruption_test1(0xdeadbeef),
    memory_corruption_test2(0xdeadbeef),
    memory_corruption_test3(0xdeadbeef),
    memory_corruption_test4(0xdeadbeef)
{
    // The following are not bzero() because MS Windows doesn't like it.
    memset(&src_data,          0, sizeof(SRC_DATA));
    memset(src_in,             0, sizeof(float) * kAudioSourceInputSize);
    memset(src_out,            0, sizeof(float) * kAudioSourceOutputSize);
    memset(tmp_buff,           0, sizeof(short) * kAudioTempBufSize);
    memset(&audiotime_updated, 0, sizeof(audiotime_updated));
    memset(audiobuffer,        0, sizeof(char)  * kAudioRingBufferSize);
    orig_config_channels = gContext->GetNumSetting("MaxChannels", 2);
    src_quality = gContext->GetNumSetting("AudioUpmixType", 2);
        //Set default upsampling quality to medium if using stereo
    if (orig_config_channels == 2)
        src_quality = 1;

        // Handle override of SRC quality settings
    if (gContext->GetNumSetting("AdvancedAudioSettings", false) &&
        gContext->GetNumSetting("SRCQualityOverride", false))
    {
        src_quality = gContext->GetNumSetting("SRCQuality", 1);
            // Extra test to keep backward compatibility with earlier SRC code setting
        if (src_quality > 2)
            src_quality = 2;
        VERBOSE(VB_AUDIO, LOC + QString("Force SRC quality (%1)").arg(src_quality));
    }

    if (!settings.upmixer)
        configured_audio_channels = gContext->GetNumSetting("AudioDefaultUpmix", false) ? orig_config_channels : 2;
    else
        if (settings.upmixer == 1)
            configured_audio_channels = 2;
        else
            configured_audio_channels = 6;

    allow_ac3_passthru = (orig_config_channels > 2) ? gContext->GetNumSetting("AC3PassThru", false) : false;

    // You need to call Reconfigure from your concrete class.
    // Reconfigure(laudio_bits,       laudio_channels,
    //             laudio_samplerate, laudio_passthru);
}

AudioOutputBase::~AudioOutputBase()
{
    if (!killaudio)
    {
        VERBOSE(VB_IMPORTANT,
                "Programmer Error: "
                "~AudioOutputBase called, but KillAudio has not been called!");
    }

    assert(memory_corruption_test0 == 0xdeadbeef);
    assert(memory_corruption_test1 == 0xdeadbeef);
    assert(memory_corruption_test2 == 0xdeadbeef);
    assert(memory_corruption_test3 == 0xdeadbeef);
    assert(memory_corruption_test4 == 0xdeadbeef);
}

void AudioOutputBase::SetSourceBitrate(int rate)
{
    if (rate > 0)
        source_bitrate = rate;
}

void AudioOutputBase::SetStretchFactorLocked(float laudio_stretchfactor)
{
    effdspstretched = (int)((float)effdsp / laudio_stretchfactor);
    if ((audio_stretchfactor != laudio_stretchfactor) ||  !pSoundStretch)
    {
        audio_stretchfactor = laudio_stretchfactor;
        if (pSoundStretch)
        {
            VERBOSE(VB_GENERAL, LOC + QString("Changing time stretch to %1")
                                        .arg(audio_stretchfactor));
            pSoundStretch->setTempo(audio_stretchfactor);
        }
        else if (audio_stretchfactor != 1.0)
        {
            VERBOSE(VB_GENERAL, LOC + QString("Using time stretch %1")
                                        .arg(audio_stretchfactor));
            pSoundStretch = new soundtouch::SoundTouch();
            pSoundStretch->setSampleRate(audio_samplerate);
            pSoundStretch->setChannels(upmixer ? 
                configured_audio_channels : source_audio_channels);

            pSoundStretch->setTempo(audio_stretchfactor);
            pSoundStretch->setSetting(SETTING_SEQUENCE_MS, 35);

            // don't need these with only tempo change
            //pSoundStretch->setPitch(1.0);
            //pSoundStretch->setRate(1.0);
            //pSoundStretch->setSetting(SETTING_USE_QUICKSEEK, true);
            //pSoundStretch->setSetting(SETTING_USE_AA_FILTER, false);
        }
    }
}

void AudioOutputBase::SetStretchFactor(float laudio_stretchfactor)
{
    QMutexLocker lock(&audio_buflock);
    SetStretchFactorLocked(laudio_stretchfactor);
}

float AudioOutputBase::GetStretchFactor(void) const
{
    return audio_stretchfactor;
}

bool AudioOutputBase::ToggleUpmix(void)
{
    if (orig_config_channels == 2 || source_audio_channels > 2 ||
        audio_passthru)
        return false;
    if (configured_audio_channels == 6)
        configured_audio_channels = 2;
    else
        configured_audio_channels = 6;

    const AudioSettings settings(audio_bits, source_audio_channels,
                                 audio_codec, source_audio_samplerate,
                                 audio_passthru);
    Reconfigure(settings);
    return (configured_audio_channels == 6);
}


void AudioOutputBase::Reconfigure(const AudioSettings &orig_settings)
{
    AudioSettings settings = orig_settings;

    int lsource_audio_channels = settings.channels;
    bool lneeds_upmix = false;
    bool laudio_reenc = false;

    // Are we reencoding a (previously) timestretched bitstream?
    if ((settings.codec == CODEC_ID_AC3 || settings.codec == CODEC_ID_DTS) &&
        !settings.use_passthru && allow_ac3_passthru)
    {
        laudio_reenc = true;
        VERBOSE(VB_AUDIO, LOC + "Reencoding decoded AC3/DTS to AC3");
    }

    // Enough channels? Upmix if not
    if (settings.channels < configured_audio_channels &&
         !settings.use_passthru)
    {
        settings.channels = configured_audio_channels;
        lneeds_upmix = true;
        VERBOSE(VB_AUDIO,LOC + "Needs upmix");
    }

    ClearError();
    bool general_deps = (settings.bits == audio_bits &&
        settings.channels == audio_channels &&
        settings.samplerate == audio_samplerate && !need_resampler &&
        settings.use_passthru == audio_passthru &&
        lneeds_upmix == needs_upmix &&
        laudio_reenc == audio_reenc);
    bool upmix_deps =
        (lsource_audio_channels == source_audio_channels);
    if (general_deps && upmix_deps)
    {
        VERBOSE(VB_AUDIO,LOC + "no change exiting");
        return;
    }

    if (general_deps && !upmix_deps && lneeds_upmix && upmixer)
    {
        upmixer->flush();
        source_audio_channels = lsource_audio_channels;
        VERBOSE(VB_AUDIO,LOC + QString("source channels changed to %1")
                .arg(source_audio_channels));
        return;
    }

    KillAudio();

    QMutexLocker lock1(&audio_buflock);
    QMutexLocker lock2(&avsync_lock);

    lastaudiolen = 0;
    waud = raud = 0;
    audio_actually_paused = false;

    audio_channels = settings.channels;
    source_audio_channels = lsource_audio_channels;
    audio_bits = settings.bits;
    source_audio_samplerate = audio_samplerate = settings.samplerate;
    audio_reenc = laudio_reenc;
    audio_codec = settings.codec;
    audio_passthru = settings.use_passthru;
    needs_upmix = lneeds_upmix;

    if (audio_bits != 8 && audio_bits != 16)
    {
        Error("AudioOutput only supports 8 or 16bit audio.");
        return;
    }

    VERBOSE(VB_AUDIO, LOC + QString("Original audio codec was %1")
                            .arg(codec_id_string((CodecID)audio_codec)));

    need_resampler = false;
    killaudio = false;
    pauseaudio = false;
    was_paused = true;
    internal_vol = gContext->GetNumSetting("MythControlsVolume", 0);

    numlowbuffer = 0;

    // Encode to AC-3 if not passing thru , there's > 2 channels
    // and a passthru device is defined
    if (!audio_passthru && allow_ac3_passthru &&
        (audio_channels > 2 || audio_reenc))
        audio_enc = true;

    // Find out what sample rates we can output (if output layer supports it)
    vector<int> rates = GetSupportedRates();
    vector<int>::iterator it;
    bool resample = true;

    // Assume 48k if we can't get supported rates
    if (rates.empty())
        rates.push_back(48000);

    for (it = rates.begin(); it < rates.end(); it++)
    {
        VERBOSE(VB_AUDIO, LOC + QString("Sample rate %1 is supported")
                                .arg(*it));
        if (*it == audio_samplerate)
            resample = false;
    }

    // Force resampling if we are encoding to AC3 and sr > 48k
    if (audio_enc && audio_samplerate > 48000)
    {
        VERBOSE(VB_AUDIO, LOC + "Forcing resample to 48k for AC3 encode");
        if (src_quality < 0)
            src_quality = 1;
        resample = true;
    }

    if (resample && src_quality >= 0)
    {
        int error;
        audio_samplerate = rates.back();
        // Limit sr to 48k if we are encoding to AC3
        if (audio_enc && audio_samplerate > 48000)
        {
            VERBOSE(VB_AUDIO, LOC + "Limiting samplerate to 48k for AC3 encode");
            audio_samplerate = 48000;
        }
        VERBOSE(VB_GENERAL, LOC + QString("Using resampler. From: %1 to %2")
            .arg(settings.samplerate).arg(audio_samplerate));
        src_ctx = src_new(2-src_quality, source_audio_channels, &error);
        if (error)
        {
            Error(QString("Error creating resampler, the error was: %1")
                  .arg(src_strerror(error)) );
            src_ctx = NULL;
            return;
        }
        src_data.src_ratio = (double) audio_samplerate / settings.samplerate;
        src_data.data_in = src_in;
        src_data.data_out = src_out;
        src_data.output_frames = 16384*6;
        need_resampler = true;
    }

    if (audio_enc)
    {
        VERBOSE(VB_AUDIO, LOC + "Creating AC-3 Encoder");
        encoder = new AudioOutputDigitalEncoder();
        if (!encoder->Init(CODEC_ID_AC3, 448000, audio_samplerate, audio_channels))
        {
            VERBOSE(VB_IMPORTANT, LOC + "Can't create AC-3 encoder");
            delete encoder;
            encoder = NULL;
            audio_enc = false;
        }
    }

    if(audio_passthru || audio_enc)
        // AC-3 output - soundcard expects a 2ch 48k stream
        audio_channels = 2;

    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    source_audio_bytes_per_sample = source_audio_channels * audio_bits / 8;

    VERBOSE(VB_GENERAL, QString("Opening audio device '%1'. ch %2(%3) sr %4 (reenc %5)")
            .arg(audio_main_device).arg(audio_channels)
            .arg(source_audio_channels).arg(audio_samplerate).arg(audio_reenc));

    // Actually do the device specific open call
    if (!OpenDevice())
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "Aborting reconfigure");
        if (GetError().isEmpty())
            Error("Aborting reconfigure");
        VERBOSE(VB_AUDIO, "Aborting reconfigure");
        return;
    }

    // Only used for software volume
    if (set_initial_vol && internal_vol) 
    {
        QString controlLabel = gContext->GetSetting("MixerControl", "PCM");
        controlLabel += "MixerVolume";
        volume = gContext->GetNumSetting(controlLabel, 80);
    }

    SyncVolume();
    VolumeBase::UpdateVolume();

    VERBOSE(VB_AUDIO, LOC + QString("Audio fragment size: %1")
            .arg(fragment_size));

    if (audio_buffer_unused < 0)
        audio_buffer_unused = 0;

    if (!gContext->GetNumSetting("AdvancedAudioSettings", false))
        audio_buffer_unused = 0;
    else if (!gContext->GetNumSetting("AggressiveSoundcardBuffer", false))
        audio_buffer_unused = 0;

    audbuf_timecode = 0;
    audiotime = 0;
    samples_buffered = 0;
    effdsp = audio_samplerate * 100;
    gettimeofday(&audiotime_updated, NULL);
    current_seconds = -1;
    source_bitrate = -1;

    if (needs_upmix)
    {
        VERBOSE(VB_AUDIO, LOC + QString("create upmixer"));
        if (configured_audio_channels == 6)
        {
            surround_mode = gContext->GetNumSetting("AudioUpmixType", 2);
        }

        upmixer = new FreeSurround(
            audio_samplerate,
            source == AUDIOOUTPUT_VIDEO,
            (FreeSurround::SurroundMode)surround_mode);

        VERBOSE(VB_AUDIO, LOC +
                QString("Create upmixer done with surround mode %1")
                .arg(surround_mode));
    }

    VERBOSE(VB_AUDIO, LOC + QString("Audio Stretch Factor: %1")
            .arg(audio_stretchfactor));

    SetStretchFactorLocked(old_audio_stretchfactor);
    
    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();

    StartOutputThread();
    VERBOSE(VB_AUDIO, LOC + "Ending reconfigure");
}

bool AudioOutputBase::StartOutputThread(void)
{
    if (audio_thread_exists)
        return true;

    start();
    audio_thread_exists = true;

    return true;
}


void AudioOutputBase::StopOutputThread(void)
{
    if (audio_thread_exists)
    {
        wait();
        audio_thread_exists = false;
    }
}

void AudioOutputBase::KillAudio()
{
    killAudioLock.lock();

    VERBOSE(VB_AUDIO, LOC + "Killing AudioOutputDSP");
    killaudio = true;
    StopOutputThread();
    QMutexLocker lock1(&audio_buflock);

    // Close resampler?
    if (src_ctx)
    {
        src_delete(src_ctx);
        src_ctx = NULL;
    }

    need_resampler = false;

    // close sound stretcher
    if (pSoundStretch)
    {
        delete pSoundStretch;
        pSoundStretch = NULL;
        old_audio_stretchfactor = audio_stretchfactor;
        audio_stretchfactor = 1.0;
    }

    if (encoder)
    {
        delete encoder;
        encoder = NULL;
    }

    if (upmixer)
    {
        delete upmixer;
        upmixer = NULL;
    }
    needs_upmix = false;
    audio_enc = false;

    CloseDevice();

    killAudioLock.unlock();
}

void AudioOutputBase::Pause(bool paused)
{
    VERBOSE(VB_AUDIO, LOC + QString("Pause %0").arg(paused));
    pauseaudio = paused;
    audio_actually_paused = false;
}

void AudioOutputBase::Reset()
{
    QMutexLocker lock1(&audio_buflock);
    QMutexLocker lock2(&avsync_lock);

    raud = waud = 0;
    audbuf_timecode = 0;
    audiotime = 0;
    samples_buffered = 0;
    current_seconds = -1;
    was_paused = !pauseaudio;

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();

    gettimeofday(&audiotime_updated, NULL);
}

void AudioOutputBase::SetTimecode(long long timecode)
{
    QMutexLocker locker(&audio_buflock);
    audbuf_timecode = timecode;
    samples_buffered = (long long)((timecode * effdsp) / 100000.0);
}

void AudioOutputBase::SetEffDsp(int dsprate)
{
    VERBOSE(VB_AUDIO, LOC + QString("SetEffDsp: %1").arg(dsprate));
    effdsp = dsprate;
    effdspstretched = (int)((float)effdsp / audio_stretchfactor);
}

void AudioOutputBase::SetBlocking(bool blocking)
{
    this->blocking = blocking;
}

int AudioOutputBase::audiolen(bool use_lock)
{
    /* Thread safe, returns the number of valid bytes in the audio buffer */
    int ret;

    if (use_lock)
        audio_buflock.lock();

    if (waud >= raud)
        ret = waud - raud;
    else
        ret = kAudioRingBufferSize - (raud - waud);

    if (use_lock)
        audio_buflock.unlock();

    return ret;
}

int AudioOutputBase::audiofree(bool use_lock)
{
    return kAudioRingBufferSize - audiolen(use_lock) - 1;
    /* There is one wasted byte in the buffer. The case where waud = raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is kAudioRingBufferSize - 1. */
}

int AudioOutputBase::GetAudiotime(void)
{
    /* Returns the current timecode of audio leaving the soundcard, based
       on the 'audiotime' computed earlier, and the delay since it was computed.

       This is a little roundabout...

       The reason is that computing 'audiotime' requires acquiring the audio
       lock, which the video thread should not do. So, we call 'SetAudioTime()'
       from the audio thread, and then call this from the video thread. */
    long long ret;
    struct timeval now;

    if (audiotime == 0)
        return 0;

    QMutexLocker lock(&avsync_lock);

    gettimeofday(&now, NULL);

    ret = (now.tv_sec - audiotime_updated.tv_sec) * 1000;
    ret += (now.tv_usec - audiotime_updated.tv_usec) / 1000;
    ret = (long long)(ret * audio_stretchfactor);

#if 1
    VERBOSE(VB_AUDIO+VB_TIMESTAMP,
            QString("GetAudiotime now=%1.%2, set=%3.%4, ret=%5, audt=%6 sf=%7")
            .arg(now.tv_sec).arg(now.tv_usec)
            .arg(audiotime_updated.tv_sec).arg(audiotime_updated.tv_usec)
            .arg(ret)
            .arg(audiotime)
            .arg(audio_stretchfactor)
           );
#endif

    ret += audiotime;

    return (int)ret;
}

void AudioOutputBase::SetAudiotime(void)
{
    if (audbuf_timecode == 0)
        return;

    int soundcard_buffer = 0;
    int totalbuffer;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       which is leaving the sound card at this instant.

       We use these variables:

       'effdsp' is samples/sec, multiplied by 100.
       Bytes per sample is assumed to be 4.

       'audiotimecode' is the timecode of the audio that has just been
       written into the buffer.

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer.

       'ms/byte' is given by '25000/effdsp'...
     */

    QMutexLocker lock1(&audio_buflock);
    QMutexLocker lock2(&avsync_lock);

    soundcard_buffer = GetBufferedOnSoundcard(); // bytes
    totalbuffer = audiolen(false) + soundcard_buffer;

    // include algorithmic latencies
    if (pSoundStretch)
        totalbuffer += (int)((pSoundStretch->numUnprocessedSamples() *
                              audio_bytes_per_sample) / audio_stretchfactor);

    if (upmixer && needs_upmix)
        totalbuffer += upmixer->sampleLatency() * audio_bytes_per_sample;

    if (encoder) 
         totalbuffer += encoder->Buffered();

    audiotime = audbuf_timecode - (int)(totalbuffer * 100000.0 /
                                   (audio_bytes_per_sample * effdspstretched));

    gettimeofday(&audiotime_updated, NULL);
#if 1
    VERBOSE(VB_AUDIO+VB_TIMESTAMP,
            QString("SetAudiotime set=%1.%2, audt=%3 atc=%4 "
                    "tb=%5 sb=%6 eds=%7 abps=%8 sf=%9")
            .arg(audiotime_updated.tv_sec).arg(audiotime_updated.tv_usec)
            .arg(audiotime)
            .arg(audbuf_timecode)
            .arg(totalbuffer)
            .arg(soundcard_buffer)
            .arg(effdspstretched)
            .arg(audio_bytes_per_sample)
            .arg(audio_stretchfactor));
#endif
}

int AudioOutputBase::GetAudioBufferedTime(void)
{
     return audbuf_timecode - GetAudiotime();
}

void AudioOutputBase::SetSWVolume(int new_volume, bool save)
{
    volume = new_volume;
    if (save)
    {
        QString controlLabel = gContext->GetSetting("MixerControl", "PCM");
        controlLabel += "MixerVolume";
        gContext->SaveSetting(controlLabel, volume);
    }
}

int AudioOutputBase::GetSWVolume()
{
    return volume;
}

void AudioOutputBase::AdjustVolume(void *buffer, int len, bool music)
{
    if (audio_bits == 8)
        _AdjustVolume<char>((char *)buffer, len, music);
    else if (audio_bits == 16)
        _AdjustVolume<short>((short *)buffer, len, music);
}

template <class AudioDataType>
void AudioOutputBase::_AdjustVolume(AudioDataType *buffer, int len, bool music)
{
    float g = volume / 100.0;

    // Should probably be exponential - this'll do
    g *= g;
    
    // Add gain to AC-3 - try to ~ match PCM volume
    if (audio_enc && audio_reenc)
        g *= 1.8;

    // Music is relatively loud - ditto
    else if (music)
        g *= 0.4;

    if (g == 1.0)
        return;

    for (int i = 0; i < (int)(len / sizeof(AudioDataType)); i++)
    {
        float s = static_cast<float>(buffer[i]) * g /
                  static_cast<float>(numeric_limits<AudioDataType>::max());
        if (s >= 1.0)
            buffer[i] = numeric_limits<AudioDataType>::max();
        else if (s <= -1.0)
            buffer[i] = numeric_limits<AudioDataType>::min();
        else
            buffer[i] = static_cast<AudioDataType>
                        (s * numeric_limits<AudioDataType>::max());
    }
}

bool AudioOutputBase::AddSamples(char *buffers[], int samples,
                                 long long timecode)
{
    // NOTE: This function is not threadsafe
    int afree = audiofree(true);
    int abps = (encoder) ?
        encoder->audio_bytes_per_sample : audio_bytes_per_sample;
    int len = samples * abps;

    // Check we have enough space to write the data
    if (need_resampler && src_ctx)
        len = (int)ceilf(float(len) * src_data.src_ratio);

    // include samples in upmix buffer that may be flushed
    if (needs_upmix && upmixer)
        len += upmixer->numUnprocessedSamples() * abps;

    if (pSoundStretch)
        len += (pSoundStretch->numUnprocessedSamples() +
                (int)(pSoundStretch->numSamples()/audio_stretchfactor))*abps;

    if ((len > afree) && !blocking)
    {
        VERBOSE(VB_AUDIO+VB_TIMESTAMP, LOC + QString(
                "AddSamples FAILED bytes=%1, used=%2, free=%3, timecode=%4")
                .arg(len).arg(kAudioRingBufferSize-afree).arg(afree)
                .arg(timecode));

        return false; // would overflow
    }

    QMutexLocker lock1(&audio_buflock);

    // resample input if necessary
    if (need_resampler && src_ctx)
    {
        // Convert to floats
        // TODO: Implicit assumption dealing with 16 bit input only.
        short **buf_ptr = (short**)buffers;
        for (int sample = 0; sample < samples; sample++)
        {
            for (int channel = 0; channel < audio_channels; channel++)
            {
                src_in[sample] = buf_ptr[channel][sample] / (1.0 * 0x8000);
            }
        }

        src_data.input_frames = samples;
        src_data.end_of_input = 0;
        int error = src_process(src_ctx, &src_data);
        if (error)
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Error occurred while resampling audio: %1")
                    .arg(src_strerror(error)));

        src_float_to_short_array(src_data.data_out, (short int*)tmp_buff,
                                 src_data.output_frames_gen*audio_channels);

        _AddSamples(tmp_buff, true, src_data.output_frames_gen, timecode);
    }
    else
    {
        // Call our function to do the work
        _AddSamples(buffers, false, samples, timecode);
    }

    return true;
}

bool AudioOutputBase::AddSamples(char *buffer, int samples, long long timecode)
{
    // NOTE: This function is not threadsafe

    int afree = audiofree(true);
    int abps = (encoder) ?
        encoder->audio_bytes_per_sample : audio_bytes_per_sample;
    int len = samples * abps;
    
    // Give original samples to mythmusic visualisation
    dispatchVisual((unsigned char *)buffer, len, timecode,
                   source_audio_channels, audio_bits);

    // Check we have enough space to write the data
    if (need_resampler && src_ctx)
        len = (int)ceilf(float(len) * src_data.src_ratio);

    // include samples in upmix buffer that may be flushed
    if (needs_upmix && upmixer)
        len += upmixer->numUnprocessedSamples() * abps;

    if (pSoundStretch)
    {
        len += (pSoundStretch->numUnprocessedSamples() +
                (int)(pSoundStretch->numSamples()/audio_stretchfactor))*abps;
    }

    if ((len > afree) && !blocking)
    {
        VERBOSE(VB_AUDIO+VB_TIMESTAMP, LOC + QString(
                "AddSamples FAILED bytes=%1, used=%2, free=%3, timecode=%4")
                .arg(len).arg(kAudioRingBufferSize-afree).arg(afree)
                .arg(timecode));
        return false; // would overflow
    }

    QMutexLocker lock1(&audio_buflock);

    // resample input if necessary
    if (need_resampler && src_ctx)
    {
        // Convert to floats
        short *buf_ptr = (short*)buffer;
        for (int sample = 0; sample < samples * audio_channels; sample++)
        {
            src_in[sample] = (float)buf_ptr[sample] / (1.0 * 0x8000);
        }
 
        src_data.input_frames = samples;
        src_data.end_of_input = 0;
        int error = src_process(src_ctx, &src_data);
        if (error)
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Error occurred while resampling audio: %1")
                    .arg(src_strerror(error)));
        src_float_to_short_array(src_data.data_out, (short int*)tmp_buff,
                                 src_data.output_frames_gen*audio_channels);

        _AddSamples(tmp_buff, true, src_data.output_frames_gen, timecode);
    }
    else
    {
        // Call our function to do the work
        _AddSamples(buffer, true, samples, timecode);
    }

    return true;
}

int AudioOutputBase::WaitForFreeSpace(int samples)
{
    int abps = (encoder) ?
        encoder->audio_bytes_per_sample : audio_bytes_per_sample;
    int len = samples * abps;
    int afree = audiofree(false);

    while (len > afree)
    {
        if (blocking)
        {
            VERBOSE(VB_AUDIO+VB_TIMESTAMP, LOC + "Waiting for free space " +
                    QString("(need %1, available %2)").arg(len).arg(afree));

            // wait for more space
            audio_bufsig.wait(&audio_buflock);
            afree = audiofree(false);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Audio buffer overflow, %1 audio samples lost!")
                    .arg(samples - (afree / abps)));
            samples = afree / abps;
            len = samples * abps;
            if (src_ctx)
            {
                int error = src_reset(src_ctx);
                if (error)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                            "Error occurred while resetting resampler: %1")
                            .arg(src_strerror(error)));
                    src_ctx = NULL;
                }
            }
        }
    }
    return len;
}

void *AudioOutputBase::MonoToStereo(void *s1, void *s2, int samples)
{
    if (audio_bits == 8)
        return _MonoToStereo((unsigned char *)s1, (unsigned char *)s2, samples);
    else if (audio_bits == 16)
        return _MonoToStereo((short *)s1, (short *)s2, samples);
    else
        return NULL; // 0
}

template <class AudioDataType>
void *AudioOutputBase::_MonoToStereo(AudioDataType *s1, AudioDataType *s2, int samples)
{
    for (int i = 0; i < samples; i++)
    {
        *s1++ = *s2;
        *s1++ = *s2++;
    }
    return s2;
}

void AudioOutputBase::_AddSamples(void *buffer, bool interleaved, int samples,
                                  long long timecode)
{
    int len; // = samples * audio_bytes_per_sample;
    int audio_bytes = audio_bits / 8;
    int org_waud = waud;

    int afree = audiofree(false);

    int abps = (encoder) ?
        encoder->audio_bytes_per_sample : audio_bytes_per_sample;

    VERBOSE(VB_AUDIO+VB_TIMESTAMP,
            LOC + QString("_AddSamples samples=%1 bytes=%2, used=%3, "
                          "free=%4, timecode=%5 needsupmix %6")
            .arg(samples)
            .arg(samples * abps)
            .arg(kAudioRingBufferSize-afree).arg(afree).arg(timecode)
            .arg(needs_upmix));
    
    len = WaitForFreeSpace(samples);
        
    if (needs_upmix &&
        configured_audio_channels == 2 &&
        source_audio_channels == 1)
    {
            // We're converting mono to stereo
        int bdiff = kAudioRingBufferSize - org_waud;
        if (bdiff < len)
        {
            int bdiff_samples = bdiff / abps;
            void *buffer2 = MonoToStereo(audiobuffer + org_waud, buffer, bdiff_samples);
            MonoToStereo(audiobuffer, buffer2, samples - bdiff_samples);
        }
        else
            MonoToStereo(audiobuffer + org_waud, buffer, samples);

        org_waud = (org_waud + len) % kAudioRingBufferSize;
    }
    else if (upmixer && needs_upmix)
    {
        int out_samples = 0;
        org_waud = waud;
        int step = (interleaved)?source_audio_channels:1;
	
        for (int itemp = 0; itemp < samples; )
        {
            if (audio_bytes == 2)
            {
                itemp += upmixer->putSamples(
                    (short*)buffer + itemp * step,
                    samples - itemp,
                    source_audio_channels,
                    (interleaved) ? 0 : samples);
            }
            else
            {
                itemp += upmixer->putSamples(
                    (char*)buffer + itemp * step,
                    samples - itemp,
                    source_audio_channels,
                    (interleaved) ? 0 : samples);
            }

            int copy_samples = upmixer->numSamples();
            if (copy_samples)
            {
                int copy_len = copy_samples * abps;
                out_samples += copy_samples;
                if (out_samples > samples)
                    len = WaitForFreeSpace(out_samples);
                int bdiff = kAudioRingBufferSize - org_waud;
                if (bdiff < copy_len)
                {
                    int bdiff_samples = bdiff/abps;
                    upmixer->receiveSamples(
                        (short*)(audiobuffer + org_waud), bdiff_samples);
                    upmixer->receiveSamples(
                        (short*)(audiobuffer), (copy_samples - bdiff_samples));
                }
                else
                {
                    upmixer->receiveSamples(
                        (short*)(audiobuffer + org_waud), copy_samples);
                }
                org_waud = (org_waud + copy_len) % kAudioRingBufferSize;
            }
        }
        if (samples > 0)
            len = WaitForFreeSpace(out_samples);

        samples = out_samples;
    }
    else
    {
        if (interleaved)
        {
            char *mybuf = (char*)buffer;
            int bdiff = kAudioRingBufferSize - org_waud;
            if (bdiff < len)
            {
                memcpy(audiobuffer + org_waud, mybuf, bdiff);
                memcpy(audiobuffer, mybuf + bdiff, len - bdiff);
            }
            else
            {
                memcpy(audiobuffer + org_waud, mybuf, len);
            }

            org_waud = (org_waud + len) % kAudioRingBufferSize;
        }
        else
        {
            char **mybuf = (char**)buffer;
            for (int itemp = 0; itemp < samples * audio_bytes;
                 itemp += audio_bytes)
            {
                for (int chan = 0; chan < audio_channels; chan++)
                {
                    audiobuffer[org_waud++] = mybuf[chan][itemp];
                    if (audio_bits == 16)
                        audiobuffer[org_waud++] = mybuf[chan][itemp+1];

                    org_waud %= kAudioRingBufferSize;
                }
            }
        }
    }

    if (samples <= 0)
        return;
        
    if (pSoundStretch)
    {
        // does not change the timecode, only the number of samples
        // back to orig pos
        org_waud = waud;
        int bdiff = kAudioRingBufferSize - org_waud;
        int nSamplesToEnd = bdiff/abps;
        if (bdiff < len)
        {
            pSoundStretch->putSamples((soundtouch::SAMPLETYPE*)
                                      (audiobuffer +
                                       org_waud), nSamplesToEnd);
            pSoundStretch->putSamples((soundtouch::SAMPLETYPE*)audiobuffer,
                                      (len - bdiff) / abps);
        }
        else
        {
            pSoundStretch->putSamples((soundtouch::SAMPLETYPE*)
                                      (audiobuffer + org_waud),
                                      len / abps);
        }

        int nSamples = pSoundStretch->numSamples();
        len = WaitForFreeSpace(nSamples); 
        
        while ((nSamples = pSoundStretch->numSamples())) 
        {
            if (nSamples > nSamplesToEnd) 
                nSamples = nSamplesToEnd;
            
            nSamples = pSoundStretch->receiveSamples(
                (soundtouch::SAMPLETYPE*)
                (audiobuffer + org_waud), nSamples
            );
            
            if (nSamples == nSamplesToEnd) {
                org_waud = 0;
                nSamplesToEnd = kAudioRingBufferSize/abps;
            }
            else {
                org_waud += nSamples * abps;
                nSamplesToEnd -= nSamples;
            }
        }
    }

    if (internal_vol && SWVolume())
    {
        int bdiff = kAudioRingBufferSize - waud;
        bool music = (timecode < 1);

        if (bdiff < len)
        {
            AdjustVolume(audiobuffer + waud, bdiff, music);
            AdjustVolume(audiobuffer, len - bdiff, music);
        }
        else
            AdjustVolume(audiobuffer + waud, len, music);
    }

    // Encode to AC-3? 
    if (encoder) 
    {
        org_waud = waud;
        int bdiff = kAudioRingBufferSize - org_waud;
        int to_get = 0;

        if (bdiff < len) 
        {
            encoder->Encode(audiobuffer + org_waud, bdiff);
            to_get = encoder->Encode(audiobuffer, len - bdiff);
        }
        else 
            to_get = encoder->Encode(audiobuffer + org_waud, len);

        if (to_get > 0) 
        {
            if (to_get >= bdiff)
            {
                encoder->GetFrames(audiobuffer + org_waud, bdiff);
                to_get -= bdiff;
                org_waud = 0;
            }
            if (to_get > 0)
                encoder->GetFrames(audiobuffer + org_waud, to_get);

            org_waud += to_get;
        }
    }

    waud = org_waud;
    lastaudiolen = audiolen(false);

    if (timecode < 0)
        // mythmusic doesn't give timestamps..
        timecode = (int)((samples_buffered * 100000.0) / effdsp);

    samples_buffered += samples;

    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    // even with timestretch, timecode is still calculated from original
    // sample count
    audbuf_timecode = timecode + (int)((samples * 100000.0) / effdsp);
}

void AudioOutputBase::Status()
{
    long ct = GetAudiotime();

    if (ct < 0)
        ct = 0;

    if (source_bitrate == -1)
    {
        source_bitrate = source_audio_samplerate * source_audio_channels * audio_bits;
    }

    if (ct / 1000 != current_seconds)
    {
        current_seconds = ct / 1000;
        OutputEvent e(current_seconds, ct,
                      source_bitrate, source_audio_samplerate, audio_bits,
                      source_audio_channels);
        dispatch(e);
    }
}

void AudioOutputBase::GetBufferStatus(uint &fill, uint &total)
{
    fill = kAudioRingBufferSize - audiofree(true);
    total = kAudioRingBufferSize;
}

void AudioOutputBase::OutputAudioLoop(void)
{
    int space_on_soundcard, last_space_on_soundcard;
    unsigned char *zeros    = new unsigned char[fragment_size];
    unsigned char *fragment = new unsigned char[fragment_size];

    bzero(zeros, fragment_size);
    last_space_on_soundcard = 0;

    while (!killaudio)
    {
        if (pauseaudio)
        {
            if (!audio_actually_paused)
            {
                VERBOSE(VB_AUDIO, LOC + "OutputAudioLoop: audio paused");
                OutputEvent e(OutputEvent::Paused);
                dispatch(e);
                was_paused = true;
            }

            audio_actually_paused = true;
     
            audiotime = 0; // mark 'audiotime' as invalid.

            space_on_soundcard = GetSpaceOnSoundcard();

            if (space_on_soundcard != last_space_on_soundcard)
            {
                VERBOSE(VB_AUDIO+VB_TIMESTAMP,
                        LOC + QString("%1 bytes free on soundcard")
                        .arg(space_on_soundcard));

                last_space_on_soundcard = space_on_soundcard;
            }

            // only send zeros if card doesn't already have at least one
            // fragment of zeros -dag
            if (fragment_size >= soundcard_buffer_size - space_on_soundcard)
            {
                if (fragment_size <= space_on_soundcard) 
                {
                    WriteAudio(zeros, fragment_size);
                }
                else 
                {
                    // this should never happen now -dag
                    VERBOSE(VB_AUDIO+VB_TIMESTAMP, LOC +
                            QString("waiting for space on soundcard "
                                    "to write zeros: have %1 need %2")
                            .arg(space_on_soundcard).arg(fragment_size));
                    usleep(5000);
                }
            }

            usleep(2000);
            continue;
        }
        else
        {
            if (was_paused)
            {
                VERBOSE(VB_AUDIO, LOC + "OutputAudioLoop: Play Event");
                OutputEvent e(OutputEvent::Playing);
                dispatch(e);
                was_paused = false;
            }
        }

        space_on_soundcard = GetSpaceOnSoundcard();
 
        // if nothing has gone out the soundcard yet no sense calling
        // this (think very fast loops here when soundcard doesn't have
        // space to take another fragment) -dag
        if (space_on_soundcard != last_space_on_soundcard)
            SetAudiotime(); // once per loop, calculate stuff for a/v sync

        /* do audio output */
 
        // wait for the buffer to fill with enough to play
        if (fragment_size > audiolen(true))
        {
            if (audiolen(true) > 0)  // only log if we're sending some audio
                VERBOSE(VB_AUDIO+VB_TIMESTAMP, LOC +
                        QString("audio waiting for buffer to fill: "
                                "have %1 want %2")
                        .arg(audiolen(true)).arg(fragment_size));

            //VERBOSE(VB_AUDIO+VB_TIMESTAMP,
            //LOC + "Broadcasting free space avail");
            audio_buflock.lock();
            audio_bufsig.wakeAll();
            audio_buflock.unlock();

            usleep(2000);
            continue;
        }
 
        // wait for there to be free space on the sound card so we can write
        // without blocking.  We don't want to block while holding audio_buflock
        if (fragment_size > space_on_soundcard)
        {
            if (space_on_soundcard != last_space_on_soundcard) {
                VERBOSE(VB_AUDIO+VB_TIMESTAMP, LOC +
                        QString("audio waiting for space on soundcard: "
                                "have %1 need %2")
                        .arg(space_on_soundcard).arg(fragment_size));
                last_space_on_soundcard = space_on_soundcard;
            }

            numlowbuffer++;
            if (numlowbuffer > 5 && audio_buffer_unused)
            {
                VERBOSE(VB_IMPORTANT, LOC + "dropping back audio_buffer_unused");
                audio_buffer_unused /= 2;
            }

            usleep(5000);
            continue;
        }
        else
            numlowbuffer = 0;

        Status();

        if (GetAudioData(fragment, fragment_size, true))
            WriteAudio(fragment, fragment_size);
    }

    delete[] zeros;
    delete[] fragment;

    VERBOSE(VB_AUDIO, LOC + "OutputAudioLoop: Stop Event");
    OutputEvent e(OutputEvent::Stopped);
    dispatch(e);
}

int AudioOutputBase::GetAudioData(unsigned char *buffer, int buf_size, bool full_buffer)
{
    audio_buflock.lock(); // begin critical section

    // re-check audiolen() in case things changed.
    // for example, ClearAfterSeek() might have run
    int avail_size = audiolen(false);
    int fragment_size = buf_size;
    int written_size = 0;
    if (!full_buffer && (buf_size > avail_size))
    {
        // when full_buffer is false, return any available data
        fragment_size = avail_size;
    }

    if (avail_size && (fragment_size <= avail_size))
    {
        int bdiff = kAudioRingBufferSize - raud;
        if (fragment_size > bdiff)
        {
            // always want to write whole fragments
            memcpy(buffer, audiobuffer + raud, bdiff);
            memcpy(buffer + bdiff, audiobuffer, fragment_size - bdiff);
        }
        else
        {
            memcpy(buffer, audiobuffer + raud, fragment_size);
        }

        /* update raud */
        raud = (raud + fragment_size) % kAudioRingBufferSize;
        VERBOSE(VB_AUDIO+VB_TIMESTAMP, LOC + "Broadcasting free space avail");
        audio_bufsig.wakeAll();

        written_size = fragment_size;
    }
    audio_buflock.unlock();

    // Mute individual channels through mono->stereo duplication
    MuteState mute_state = GetMuteState();
    if (written_size &&
        audio_channels > 1 &&
        (mute_state == kMuteLeft || mute_state == kMuteRight))
    {
        int offset_src = 0;
        int offset_dst = 0;
 
        if (mute_state == kMuteLeft)
            offset_src = audio_bits / 8;    // copy channel 1 to channel 0
        else if (mute_state == kMuteRight)
            offset_dst = audio_bits / 8;    // copy channel 0 to channel 1
     
        for (int i = 0; i < written_size; i += audio_bytes_per_sample)
        {
            buffer[i + offset_dst] = buffer[i + offset_src];
            if (audio_bits == 16)
                buffer[i + offset_dst + 1] = buffer[i + offset_src + 1];
        }
    }

    return written_size;
}

// Wait for all data to finish playing
void AudioOutputBase::Drain()
{
    while (audiolen(true) > fragment_size)
    {
        usleep(1000);
    }
}

void AudioOutputBase::run(void)
{
    VERBOSE(VB_AUDIO, LOC + QString("kickoffOutputAudioLoop: pid = %1")
                                    .arg(getpid()));
    OutputAudioLoop();
    VERBOSE(VB_AUDIO, LOC + "kickoffOutputAudioLoop exiting");
}

int AudioOutputBase::readOutputData(unsigned char*, int)
{
    VERBOSE(VB_IMPORTANT, LOC_ERR + "base AudioOutputBase should not be "
                                    "getting asked to readOutputData()");
    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

