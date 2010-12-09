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
#include "audiooutpututil.h"
#include "audiooutputdownmix.h"
#include "SoundTouch.h"
#include "freesurround.h"

#define LOC QString("AO: ")
#define LOC_ERR QString("AO, ERROR: ")

#define WPOS audiobuffer + org_waud
#define RPOS audiobuffer + raud
#define ABUF audiobuffer
#define STST soundtouch::SAMPLETYPE
#define AOALIGN(x) (((long)&x + 15) & ~0xf);

#define QUALITY_DISABLED   -1
#define QUALITY_LOW         0
#define QUALITY_MEDIUM      1
#define QUALITY_HIGH        2

static const char *quality_string(int q)
{
    switch(q) {
        case QUALITY_DISABLED: return "disabled";
        case QUALITY_LOW:      return "low";
        case QUALITY_MEDIUM:   return "medium";
        case QUALITY_HIGH:     return "high";
        default:               return "unknown";
    }
}

AudioOutputBase::AudioOutputBase(const AudioSettings &settings) :
    // protected
    channels(-1),               codec(CODEC_ID_NONE),
    bytes_per_frame(0),         output_bytes_per_frame(0),
    format(FORMAT_NONE),        output_format(FORMAT_NONE),
    samplerate(-1),             effdsp(0),
    fragment_size(0),           soundcard_buffer_size(0),

    main_device(settings.GetMainDevice()),
    passthru_device(settings.GetPassthruDevice()),
    passthru(false),
    enc(false),                 reenc(false),
    stretchfactor(1.0f),
    eff_stretchfactor(100000),

    source(settings.source),    killaudio(false),

    pauseaudio(false),          actually_paused(false),
    was_paused(false),          unpause_when_ready(false),

    set_initial_vol(settings.set_initial_vol),
    buffer_output_data_for_use(false),

    // private
    output_settingsraw(NULL),   output_settings(NULL),
    need_resampler(false),      src_ctx(NULL),

    pSoundStretch(NULL),
    encoder(NULL),              upmixer(NULL),
    source_channels(-1),        source_samplerate(0),
    source_bytes_per_frame(0),
    needs_upmix(false),         needs_downmix(false),
    surround_mode(QUALITY_LOW), old_stretchfactor(1.0f),
    volume(80),                 volumeControl(QString()),

    processing(false),

    frames_buffered(0),

    audio_thread_exists(false),

    audiotime(0),
    raud(0),                    waud(0),
    audbuf_timecode(0),

    killAudioLock(QMutex::NonRecursive),
    current_seconds(-1),        source_bitrate(-1),

    memory_corruption_test0(0xdeadbeef),
    memory_corruption_test1(0xdeadbeef),
    src_out(NULL),              kAudioSRCOutputSize(0),
    memory_corruption_test2(0xdeadbeef),
    memory_corruption_test3(0xdeadbeef)
{
    src_in = (float *)AOALIGN(src_in_buf);
    // The following are not bzero() because MS Windows doesn't like it.
    memset(&src_data,          0, sizeof(SRC_DATA));
    memset(src_in,             0, sizeof(float) * kAudioSRCInputSize);
    memset(audiobuffer,        0, sizeof(char)  * kAudioRingBufferSize);

    // Default SRC quality - QUALITY_HIGH is quite expensive
    src_quality  = QUALITY_MEDIUM;

    // Handle override of SRC quality settings
    if (gCoreContext->GetNumSetting("AdvancedAudioSettings", false) &&
        gCoreContext->GetNumSetting("SRCQualityOverride",    false))
    {
        src_quality = gCoreContext->GetNumSetting("SRCQuality", QUALITY_MEDIUM);
        // Extra test to keep backward compatibility with earlier SRC setting
        if (src_quality > QUALITY_HIGH)
            src_quality = QUALITY_HIGH;

        VBAUDIO(QString("SRC quality = %1").arg(quality_string(src_quality)));
    }
}

/**
 * Destructor
 *
 * You must kill the output thread via KillAudio() prior to destruction
 */
AudioOutputBase::~AudioOutputBase()
{
    if (!killaudio)
        VBERROR("Programmer Error: "
                "~AudioOutputBase called, but KillAudio has not been called!");

    // We got this from a subclass, delete it
    delete output_settings;
    delete output_settingsraw;

    if (kAudioSRCOutputSize > 0)
        delete[] src_out;

    assert(memory_corruption_test0 == 0xdeadbeef);
    assert(memory_corruption_test1 == 0xdeadbeef);
    assert(memory_corruption_test2 == 0xdeadbeef);
    assert(memory_corruption_test3 == 0xdeadbeef);
}

void AudioOutputBase::InitSettings(const AudioSettings &settings)
{
    // Ask the subclass what we can send to the device
    output_settings = GetOutputSettingsUsers();

    max_channels = output_settings->BestSupportedChannels();
    configured_channels = max_channels;

    upmix_default = max_channels > 2 ?
        gCoreContext->GetNumSetting("AudioDefaultUpmix", false) :
        false;
    if (settings.upmixer == 1) // music, upmixer off
        upmix_default = false;
    else if (settings.upmixer == 2) // music, upmixer on
        upmix_default = true;
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3 and DTS)
 */
AudioOutputSettings* AudioOutputBase::GetOutputSettingsCleaned(void)
{
        // If we've already checked the port, use the cache
        // version instead
    if (output_settingsraw)
        return output_settingsraw;

    AudioOutputSettings* aosettings = GetOutputSettings();

    if (aosettings)
        aosettings->GetCleaned();
    else
        aosettings = new AudioOutputSettings(true);

    return (output_settingsraw = aosettings);
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3 and DTS) as well as the user settings
 */
AudioOutputSettings* AudioOutputBase::GetOutputSettingsUsers(void)
{
    if (output_settings)
        return output_settings;

    AudioOutputSettings* aosettings = new AudioOutputSettings;

    *aosettings = *GetOutputSettingsCleaned();
    aosettings->GetUsers();

    return (output_settings = aosettings);
}

/**
 * Test if we can output digital audio and if sample rate is supported
 */
bool AudioOutputBase::CanPassthrough(int samplerate, int channels) const
{
    bool ret = false;
    ret = output_settings->IsSupportedFormat(FORMAT_S16);
    ret &= output_settings->IsSupportedRate(samplerate);
    // Don't know any cards that support spdif clocked at < 44100
    // Some US cable transmissions have 2ch 32k AC-3 streams
    ret &= samplerate >= 44100;
        // Will downmix if we can't support the amount of channels
    ret &= channels <= max_channels;
        // Stereo content will always be decoded so it can later be upmixed
    ret &= channels != 2;

    return ret;
}

/**
 * Set the bitrate of the source material, reported in periodic OutputEvents
 */
void AudioOutputBase::SetSourceBitrate(int rate)
{
    if (rate > 0)
        source_bitrate = rate;
}

/*
 * Set the timestretch factor
 *
 * You must hold the audio_buflock to call this safely
 */
void AudioOutputBase::SetStretchFactorLocked(float lstretchfactor)
{
    if (stretchfactor == lstretchfactor && pSoundStretch)
        return;

    stretchfactor = lstretchfactor;
    eff_stretchfactor = (int)(100000.0f * lstretchfactor + 0.5);
    if (pSoundStretch)
    {
        VBGENERAL(QString("Changing time stretch to %1").arg(stretchfactor));
        pSoundStretch->setTempo(stretchfactor);
    }
    else if (stretchfactor != 1.0f)
    {
        VBGENERAL(QString("Using time stretch %1").arg(stretchfactor));
        pSoundStretch = new soundtouch::SoundTouch();
        pSoundStretch->setSampleRate(samplerate);
        pSoundStretch->setChannels(needs_upmix || needs_downmix ?
            configured_channels : source_channels);
        pSoundStretch->setTempo(stretchfactor);
        pSoundStretch->setSetting(SETTING_SEQUENCE_MS, 35);
        /* If we weren't already processing we need to turn on float conversion
           adjust sample and frame sizes accordingly and dump the contents of
           the audiobuffer */
        if (!processing)
        {
            processing = true;
            bytes_per_frame = source_channels *
                              AudioOutputSettings::SampleSize(FORMAT_FLT);
            waud = raud = 0;
            reset_active.Ref();
        }
    }
}

/**
 * Set the timestretch factor
 */
void AudioOutputBase::SetStretchFactor(float lstretchfactor)
{
    QMutexLocker lock(&audio_buflock);
    SetStretchFactorLocked(lstretchfactor);
}

/**
 * Get the timetretch factor
 */
float AudioOutputBase::GetStretchFactor(void) const
{
    return stretchfactor;
}

/**
 * Toggle between stereo and upmixed 5.1 if the source material is stereo
 */
bool AudioOutputBase::ToggleUpmix(void)
{
    // Can only upmix from stereo to 6 ch
    if (max_channels == 2 || source_channels != 2 || passthru)
        return false;

    upmix_default = !upmix_default;

    const AudioSettings settings(format, source_channels, codec,
                                 source_samplerate, passthru);
    Reconfigure(settings);
    return configured_channels == max_channels;
}

/**
 * (Re)Configure AudioOutputBase
 *
 * Must be called from concrete subclasses
 */
void AudioOutputBase::Reconfigure(const AudioSettings &orig_settings)
{
    AudioSettings settings  = orig_settings;
    int  lsource_channels   = settings.channels;
    bool lneeds_upmix       = false;
    bool lneeds_downmix     = false;
    bool lreenc             = false;

    if (!settings.use_passthru)
    {
        // update channels configuration if source_channels has changed
        if (lsource_channels > configured_channels)
        {
            if (lsource_channels <= 6)
                configured_channels = min(max_channels, 6);
            else if (lsource_channels > 6)
                 configured_channels = max_channels;
        }
        else
        {
            // if source was mono, and hardware doesn't support it
            // we will upmix it to stereo (can safely assume hardware supports
            // stereo)
            if (lsource_channels == 1 &&
                !output_settings->IsSupportedChannels(1))
            {
                configured_channels = 2;
            }
            else
                configured_channels = (upmix_default && lsource_channels == 2) ?
                    max_channels : lsource_channels;
        }

        /* Might we reencode a bitstream that's been decoded for timestretch?
           If the device doesn't support the number of channels - see below */
        if (output_settings->canAC3() &&
            (settings.codec == CODEC_ID_AC3 || settings.codec == CODEC_ID_DTS))
        {
            lreenc = true;
        }

        // Enough channels? Upmix if not, but only from mono/stereo to 5.1
        if (settings.channels <= 2 && settings.channels < configured_channels)
        {
            int conf_channels = (configured_channels > 6) ?
                                                    6 : configured_channels;
            VBAUDIO(QString("Needs upmix from %1 -> %2 channels")
                    .arg(settings.channels).arg(conf_channels));
            settings.channels = conf_channels;
            lneeds_upmix = true;
        }
        else if (settings.channels > max_channels)
        {
            VBAUDIO(QString("Needs downmix from %1 -> %2 channels")
                    .arg(settings.channels).arg(max_channels));
            settings.channels = max_channels;
            lneeds_downmix = true;
        }
    }

    ClearError();

    // Check if anything has changed
    bool general_deps = settings.format == format &&
                        settings.samplerate  == source_samplerate &&
                        settings.use_passthru == passthru &&
                        lneeds_upmix == needs_upmix && lreenc == reenc &&
                        lsource_channels == source_channels &&
                        lneeds_downmix == needs_downmix;

    if (general_deps)
    {
        VBAUDIO("Reconfigure(): No change -> exiting");
        return;
    }

    KillAudio();

    QMutexLocker lock(&audio_buflock);
    QMutexLocker lockav(&avsync_lock);

    waud = raud = 0;
    reset_active.Clear();
    actually_paused = processing = false;

    channels               = settings.channels;
    source_channels        = lsource_channels;
    reenc                  = lreenc;
    codec                  = settings.codec;
    passthru               = settings.use_passthru;
    needs_upmix            = lneeds_upmix;
    needs_downmix          = lneeds_downmix;
    format                 = output_format   = settings.format;
    source_samplerate      = samplerate      = settings.samplerate;

    killaudio = pauseaudio = false;
    was_paused = true;
    internal_vol = gCoreContext->GetNumSetting("MythControlsVolume", 0);

    // Don't try to do anything if audio hasn't been
    // initialized yet (e.g. rubbish was provided)
    if (source_channels <= 0 || format <= 0 || samplerate <= 0)
    {
        SilentError(QString("Aborting Audio Reconfigure. ") +
                    QString("Invalid audio parameters ch %1 fmt %2 @ %3Hz")
                    .arg(source_channels).arg(format).arg(samplerate));
        return;
    }

    VBAUDIO(QString("Original codec was %1, %2, %3 kHz, %4 channels")
            .arg(ff_codec_id_string((CodecID)codec))
            .arg(output_settings->FormatToString(format))
            .arg(samplerate/1000).arg(source_channels));

    /* Encode to AC-3 if we're allowed to passthru but aren't currently
       and we have more than 2 channels but multichannel PCM is not supported
       or if the device just doesn't support the number of channels or
       (hack) if we are upmixing stereo to 5.1 */
    enc = (!passthru &&
           output_settings->IsSupportedFormat(FORMAT_S16) &&
           output_settings->canAC3() &&
           ((needs_upmix && source_channels == 2) ||
            (!output_settings->canLPCM() && configured_channels > 2) ||
            !output_settings->IsSupportedChannels(channels)));
    VBAUDIO(QString("enc(%1), passthru(%2), canAC3(%3), canDTS(%4), canLPCM(%5)"
                    ", configured_channels(%6), %7 channels supported(%8)")
            .arg(enc)
            .arg(passthru)
            .arg(output_settings->canAC3())
            .arg(output_settings->canDTS())
            .arg(output_settings->canLPCM())
            .arg(configured_channels)
            .arg(channels).arg(output_settings->IsSupportedChannels(channels)));

    int dest_rate = 0;

    // Force resampling if we are encoding to AC3 and sr > 48k
    // or if 48k override was checked in settings
    if ((samplerate != 48000 &&
         gCoreContext->GetNumSetting("AdvancedAudioSettings", false) &&
         gCoreContext->GetNumSetting("Audio48kOverride", false)) ||
        (enc && (samplerate > 48000 || (need_resampler && dest_rate > 48000))))
    {
        VBAUDIO("Forcing resample to 48 kHz");
        if (src_quality < 0)
            src_quality = QUALITY_MEDIUM;
        need_resampler = true;
        dest_rate = 48000;
    }
    else if ((need_resampler = !output_settings->IsSupportedRate(samplerate)))
        dest_rate = output_settings->NearestSupportedRate(samplerate);

    if (need_resampler && src_quality > QUALITY_DISABLED)
    {
        int error;
        samplerate = dest_rate;

        VBGENERAL(QString("Resampling from %1 kHz to %2 kHz with quality %3")
                .arg(settings.samplerate/1000).arg(samplerate/1000)
                .arg(quality_string(src_quality)));

        src_ctx = src_new(2-src_quality, source_channels, &error);
        if (error)
        {
            Error(QString("Error creating resampler: %1")
                  .arg(src_strerror(error)));
            src_ctx = NULL;
            return;
        }

        src_data.src_ratio      = (double)samplerate / settings.samplerate;
        src_data.data_in        = src_in;
        int newsize             = ((long)((float)kAudioSRCInputSize *
                                          samplerate / settings.samplerate) +
                                   15) & ~0xf;
        if (kAudioSRCOutputSize < newsize)
        {
            kAudioSRCOutputSize = newsize;
            VBAUDIO(QString("Resampler allocating %1").arg(newsize));
            if (src_out)
                delete[] src_out;
            src_out = new float[kAudioSRCOutputSize];
        }
        src_data.data_out       = src_out;
        src_data.output_frames  = kAudioSRCOutputSize;
        src_data.end_of_input = 0;
    }

    if (enc)
    {
        if (reenc)
            VBAUDIO("Reencoding decoded AC-3/DTS to AC-3");

        VBAUDIO(QString("Creating AC-3 Encoder with sr = %1, ch = %2")
                .arg(samplerate).arg(channels));

        encoder = new AudioOutputDigitalEncoder();
        if (!encoder->Init(CODEC_ID_AC3, 448000, samplerate, channels))
        {
            Error("AC-3 encoder initialization failed");
            delete encoder;
            encoder = NULL;
            enc = false;
        }
    }

    source_bytes_per_frame = source_channels *
                             output_settings->SampleSize(format);

    // Turn on float conversion?
    if (need_resampler || needs_upmix || needs_downmix ||
        stretchfactor != 1.0f || (internal_vol && SWVolume()) ||
        (enc && output_format != FORMAT_S16) ||
        !output_settings->IsSupportedFormat(output_format))
    {
        VBAUDIO("Audio processing enabled");
        processing  = true;
        if (enc)
            output_format = FORMAT_S16;  // Output s16le for AC-3 encoder
        else
            output_format = format; //output_settings->BestSupportedFormat();
    }

    if (passthru)
        channels = 2; // IEC958 bitstream - 2 ch

    bytes_per_frame =  processing ? 4 : output_settings->SampleSize(format);
    bytes_per_frame *= channels;

    if (enc)
        channels = 2; // But only post-encoder

    output_bytes_per_frame = channels *
                             output_settings->SampleSize(output_format);

    VBGENERAL(
        QString("Opening audio device '%1' ch %2(%3) sr %4 sf %5 reenc %6")
        .arg(main_device).arg(channels).arg(source_channels).arg(samplerate)
        .arg(output_settings->FormatToString(output_format)).arg(reenc));

    // Actually do the device specific open call
    if (!OpenDevice())
    {
        if (GetError().isEmpty())
            Error("Aborting reconfigure");
        else
            VBGENERAL("Aborting reconfigure");
        return;
    }

    // Only used for software volume
    if (set_initial_vol && internal_vol && SWVolume())
    {
        VBAUDIO("Software volume enabled");
        volumeControl  = gCoreContext->GetSetting("MixerControl", "PCM");
        volumeControl += "MixerVolume";
        volume = gCoreContext->GetNumSetting(volumeControl, 80);
    }

    VolumeBase::SetChannels(channels);
    VolumeBase::SyncVolume();
    VolumeBase::UpdateVolume();

    VBAUDIO(QString("Audio fragment size: %1").arg(fragment_size));

    audbuf_timecode = audiotime = frames_buffered = 0;
    current_seconds = source_bitrate = -1;
    effdsp = samplerate * 100;

    // Upmix Stereo to 5.1
    if (needs_upmix && source_channels == 2 && configured_channels > 2)
    {
        surround_mode = gCoreContext->GetNumSetting("AudioUpmixType", QUALITY_HIGH);
        if ((upmixer = new FreeSurround(samplerate, source == AUDIOOUTPUT_VIDEO,
                                    (FreeSurround::SurroundMode)surround_mode)))
            VBAUDIO(QString("Create %1 quality upmixer done")
                    .arg(quality_string(surround_mode)));
        else
        {
            VBERROR("Failed to create upmixer");
            needs_upmix = false;
        }
    }

    VBAUDIO(QString("Audio Stretch Factor: %1").arg(stretchfactor));
    SetStretchFactorLocked(old_stretchfactor);

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();

    if (unpause_when_ready)
        pauseaudio = actually_paused = true;

    StartOutputThread();

    VBAUDIO("Ending Reconfigure()");
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

/**
 * Kill the output thread and cleanup
 */
void AudioOutputBase::KillAudio()
{
    killAudioLock.lock();

    VBAUDIO("Killing AudioOutputDSP");
    killaudio = true;
    StopOutputThread();
    QMutexLocker lock(&audio_buflock);

    if (pSoundStretch)
    {
        delete pSoundStretch;
        pSoundStretch = NULL;
        old_stretchfactor = stretchfactor;
        stretchfactor = 1.0f;
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

    if (src_ctx)
    {
        src_delete(src_ctx);
        src_ctx = NULL;
    }

    needs_upmix = need_resampler = enc = false;

    CloseDevice();

    killAudioLock.unlock();
}

void AudioOutputBase::Pause(bool paused)
{
    if (unpause_when_ready)
        return;
    VBAUDIO(QString("Pause %0").arg(paused));
    if (pauseaudio != paused)
        was_paused = pauseaudio;
    pauseaudio = paused;
    actually_paused = false;
}

void AudioOutputBase::PauseUntilBuffered()
{
    Reset();
    Pause(true);
    unpause_when_ready = true;
}

/**
 * Reset the audiobuffer, timecode and mythmusic visualisation
 */
void AudioOutputBase::Reset()
{
    QMutexLocker lock(&audio_buflock);
    QMutexLocker lockav(&avsync_lock);

    audbuf_timecode = audiotime = frames_buffered = 0;
    waud = raud;    // empty ring buffer
    reset_active.Ref();
    current_seconds = -1;
    was_paused = !pauseaudio;

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();
}

/**
 * Set the timecode of the samples most recently added to the audiobuffer
 *
 * Used by mythmusic for seeking since it doesn't provide timecodes to
 * AddFrames()
 */
void AudioOutputBase::SetTimecode(int64_t timecode)
{
    audbuf_timecode = audiotime = timecode;
    frames_buffered = (int64_t)((timecode * source_samplerate) / 1000);
}

/**
 * Set the effective DSP rate
 *
 * Equal to 100 * samples per second
 * NuppelVideo sets this every sync frame to achieve av sync
 */
void AudioOutputBase::SetEffDsp(int dsprate)
{
    VBAUDIO(QString("SetEffDsp: %1").arg(dsprate));
    effdsp = dsprate;
}

/**
 * Get the number of bytes in the audiobuffer
 */
inline int AudioOutputBase::audiolen()
{
    if (waud >= raud)
        return waud - raud;
    else
        return kAudioRingBufferSize - (raud - waud);
}

/**
 * Get the free space in the audiobuffer in bytes
 */
int AudioOutputBase::audiofree()
{
    return kAudioRingBufferSize - audiolen() - 1;
    /* There is one wasted byte in the buffer. The case where waud = raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is kAudioRingBufferSize - 1. */
}

/**
 * Get the scaled number of bytes in the audiobuffer, i.e. the number of
 * samples * the output bytes per sample
 *
 * This value can differ from that returned by audiolen if samples are
 * being converted to floats and the output sample format is not 32 bits
 */
int AudioOutputBase::audioready()
{
    if (passthru || enc || bytes_per_frame == output_bytes_per_frame)
        return audiolen();
    else
        return audiolen() * output_bytes_per_frame / bytes_per_frame;
}

/**
 * Calculate the timecode of the samples that are about to become audible
 */
int64_t AudioOutputBase::GetAudiotime(void)
{
    if (audbuf_timecode == 0)
        return 0;

    int obpf = output_bytes_per_frame;
    int64_t oldaudiotime;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       Which is leaving the sound card at this instant.

       We use these variables:

       'effdsp' is frames/sec

       'audbuf_timecode' is the timecode of the audio that has just been
       written into the buffer.

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer. */


    QMutexLocker lockav(&avsync_lock);

    int64_t soundcard_buffer = GetBufferedOnSoundcard(); // bytes
    int64_t main_buffer = audioready();

    /* audioready tells us how many bytes are in audiobuffer
       scaled appropriately if output format != internal format */

    oldaudiotime = audiotime;

    /* timecode is the stretch adjusted version
       of major post-stretched buffer contents
       processing latencies are catered for in AddFrames/SetAudiotime
       to eliminate race */
    audiotime = audbuf_timecode - (
        ((main_buffer + soundcard_buffer) * eff_stretchfactor ) /
        (effdsp * obpf));

    /* audiotime should never go backwards, but we might get a negative
       value if GetBufferedOnSoundcard() isn't updated by the driver very
       quickly (e.g. ALSA) */
    if (audiotime < oldaudiotime)
        audiotime = oldaudiotime;

    VBAUDIOTS(QString("GetAudiotime audt=%1 atc=%2 mb=%3 sb=%4 tb=%5 "
                      "sr=%6 obpf=%7 bpf=%8 sf=%9 %10 %11")
              .arg(audiotime).arg(audbuf_timecode)
              .arg(main_buffer)
              .arg(soundcard_buffer)
              .arg(main_buffer+soundcard_buffer)
              .arg(samplerate).arg(obpf).arg(bytes_per_frame).arg(stretchfactor)
              .arg((main_buffer + soundcard_buffer) * eff_stretchfactor)
              .arg(((main_buffer + soundcard_buffer) * eff_stretchfactor ) /
                   (effdsp * obpf))
              );

    return audiotime;
}

/**
 * Set the timecode of the top of the ringbuffer
 * Exclude all other processing elements as they dont vary
 * between AddFrames calls
 */
void AudioOutputBase::SetAudiotime(int frames, int64_t timecode)
{
    int64_t processframes_stretched = 0;
    int64_t processframes_unstretched = 0;

    if (needs_upmix && upmixer)
        processframes_unstretched -= upmixer->frameLatency();

    if (pSoundStretch)
    {
        processframes_unstretched -= pSoundStretch->numUnprocessedSamples();
        processframes_stretched -= pSoundStretch->numSamples();
    }

    if (encoder)
        // the input buffered data is still in audio_bytes_per_sample format
        processframes_stretched -= encoder->Buffered() / output_bytes_per_frame;

    int64_t old_audbuf_timecode = audbuf_timecode;

    audbuf_timecode = timecode +
                (((frames + processframes_unstretched) * 100000) +
                  (processframes_stretched * eff_stretchfactor )) / effdsp;

    // check for timecode wrap and reset audiotime if detected
    // timecode will always be monotonic asc if not seeked and reset
    // happens if seek or pause happens
    if (audbuf_timecode < old_audbuf_timecode)
        audiotime = 0;

    VBAUDIOTS(QString("SetAudiotime atc=%1 tc=%2 f=%3 pfu=%4 pfs=%5")
              .arg(audbuf_timecode)
              .arg(timecode)
              .arg(frames)
              .arg(processframes_unstretched)
              .arg(processframes_stretched));
#ifdef AUDIOTSTESTING
    GetAudiotime();
#endif
}

/**
 * Get the difference in timecode between the samples that are about to become
 * audible and the samples most recently added to the audiobuffer, i.e. the
 * time in ms representing the sum total of buffered samples
 */
int64_t AudioOutputBase::GetAudioBufferedTime(void)
{
    int ret = audbuf_timecode - GetAudiotime();
    // Pulse can give us values that make this -ve
    if (ret < 0)
        return 0;
    return ret;
}

/**
 * Set the volume for software volume control
 */
void AudioOutputBase::SetSWVolume(int new_volume, bool save)
{
    volume = new_volume;
    if (save && volumeControl != NULL)
        gCoreContext->SaveSetting(volumeControl, volume);
}

/**
 * Get the volume for software volume control
 */
int AudioOutputBase::GetSWVolume()
{
    return volume;
}

/**
 * Check that there's enough space in the audiobuffer to write the provided
 * number of frames
 *
 * If there is not enough space, set 'frames' to the number that will fit
 *
 * Returns the number of bytes that the frames will take up
 */
int AudioOutputBase::CheckFreeSpace(int &frames)
{
    int bpf   = bytes_per_frame;
    int len   = frames * bpf;
    int afree = audiofree();

    if (len <= afree)
        return len;

    VBERROR(QString("Audio buffer overflow, %1 frames lost!")
            .arg(frames - (afree / bpf)));

    frames = afree / bpf;
    len = frames * bpf;

    if (!src_ctx)
        return len;

    int error = src_reset(src_ctx);
    if (error)
    {
        VBERROR(QString("Error occurred while resetting resampler: %1")
                .arg(src_strerror(error)));
        src_ctx = NULL;
    }

    return len;
}

/*
 * Copy frames into the audiobuffer, upmixing en route if necessary
 *
 * Returns the number of frames written, which may be less than requested
 * if the upmixer buffered some (or all) of them
 */
int AudioOutputBase::CopyWithUpmix(char *buffer, int frames, int &org_waud)
{
    int len   = CheckFreeSpace(frames);
    int bdiff = kAudioRingBufferSize - org_waud;
    int bpf   = bytes_per_frame;
    int off   = 0;

    if (!needs_upmix)
    {
        int num  = len;
        if (bdiff <= num)
        {
            memcpy(WPOS, buffer, bdiff);
            num -= bdiff;
            off = bdiff;
            org_waud = 0;
        }
        if (num > 0)
            memcpy(WPOS, buffer + off, num);
        org_waud += num;
        return len;
    }

    // Convert mono to stereo as most devices can't accept mono
    if (channels == 2 && source_channels == 1)
    {
        int bdFrames = bdiff / bpf;
        if (bdFrames <= frames)
        {
            AudioOutputUtil::MonoToStereo(WPOS, buffer, bdFrames);
            frames -= bdFrames;
            off = bdFrames * bpf;
            org_waud = 0;
        }
        if (frames > 0)
            AudioOutputUtil::MonoToStereo(WPOS, buffer + off, frames);

        org_waud += frames * bpf;
        return len;
    }

    // Upmix to 6ch via FreeSurround
    // Calculate frame size of input
    off =  processing ? 4 : output_settings->SampleSize(format);
    off *= source_channels;

    len = 0;
    int i = 0;
    while (i < frames)
    {
        int nFrames;

        i += upmixer->putFrames(buffer + i * off,
                                frames - i, source_channels);

        nFrames = upmixer->numFrames();

        if (!nFrames)
            continue;

        len += CheckFreeSpace(nFrames);

        int bdFrames = (kAudioRingBufferSize - org_waud) / bpf;
        if (bdFrames < nFrames)
        {
            upmixer->receiveFrames((float *)(WPOS), bdFrames);
            nFrames -= bdFrames;
            org_waud = 0;
        }
        if (nFrames > 0)
            upmixer->receiveFrames((float *)(WPOS), nFrames);

        org_waud += nFrames * bpf;
    }
    return len;
}

/**
 * Add frames to the audiobuffer and perform any required processing
 *
 * Returns false if there's not enough space right now
 */
bool AudioOutputBase::AddFrames(void *in_buffer, int in_frames,
                                int64_t timecode)
{
    int org_waud = waud,               afree = audiofree();
    int frames   = in_frames;
    void *buffer = in_buffer;
    int bpf      = bytes_per_frame,    len = frames * source_bytes_per_frame;
    int used     = kAudioRingBufferSize - afree;
    bool music   = false;
    int bdiff;

    VBAUDIOTS(QString("AddFrames frames=%1, bytes=%2, used=%3, free=%4, "
                      "timecode=%5 needsupmix=%6")
              .arg(frames).arg(len).arg(used).arg(afree).arg(timecode)
              .arg(needs_upmix));

    /* See if we're waiting for new samples to be buffered before we unpause
       post channel change, seek, etc. Wait for 4 fragments to be buffered */
    if (unpause_when_ready && pauseaudio && audioready() > fragment_size << 2)
    {
        unpause_when_ready = false;
        Pause(false);
    }

    // Don't write new samples if we're resetting the buffer or reconfiguring
    QMutexLocker lock(&audio_buflock);

    // Mythmusic doesn't give us timestamps
    if (timecode < 0)
    {
        // Send original samples to mythmusic visualisation
        timecode = (int64_t)(frames_buffered) * 1000 / source_samplerate;
        frames_buffered += frames;
        dispatchVisual((uchar *)in_buffer, len, timecode, source_channels,
                       output_settings->FormatToBits(format));
        music = true;
    }

    // Calculate amount of free space required in ringbuffer
    if (processing)
    {
        // Final float conversion space requirement
        len = sizeof(*src_in_buf) /
            AudioOutputSettings::SampleSize(format) * len;

        // Account for changes in number of channels
        if (needs_upmix || needs_downmix)
            len = (len / source_channels) * channels;

        // Check we have enough space to write the data
        if (need_resampler && src_ctx)
            len = (int)ceilf(float(len) * src_data.src_ratio);

        // Include samples in upmix buffer that may be flushed
        if (needs_upmix && upmixer)
            len += upmixer->numUnprocessedFrames() * bpf;

        // Include samples in soundstretch buffers
        if (pSoundStretch)
            len += (pSoundStretch->numUnprocessedSamples() +
                    (int)(pSoundStretch->numSamples() / stretchfactor)) * bpf;
    }

    if (len > afree)
    {
        VBAUDIOTS("Buffer is full, AddFrames returning false");
        return false; // would overflow
    }

    int frames_remaining = in_frames;
    int frames_offset = 0;
    int frames_final = 0;
    int maxframes = (kAudioSRCInputSize / channels) & ~0xf;

    while(frames_remaining > 0)
    {
        buffer = (char *)in_buffer + frames_offset;
        frames = frames_remaining;

        len = frames * source_bytes_per_frame;

        if (processing)
        {
            if (frames > maxframes)
            {
                frames = maxframes;
                len = frames * source_bytes_per_frame;
                frames_offset += len;
            }
            // Convert to floats
            len = AudioOutputUtil::toFloat(format, src_in, buffer, len);
        }
        frames_remaining -= frames;

        // Perform downmix if necessary
        if (needs_downmix)
            if(AudioOutputDownmix::DownmixFrames(source_channels, channels,
                                                 src_in, src_in, frames) < 0)
                VBERROR("Error occurred while downmixing");

        // Resample if necessary
        if (need_resampler && src_ctx)
        {
            src_data.input_frames = frames;
            int error = src_process(src_ctx, &src_data);

            if (error)
                VBERROR(QString("Error occurred while resampling audio: %1")
                        .arg(src_strerror(error)));

            buffer = src_out;
            frames = src_data.output_frames_gen;
        }
        else if (processing)
            buffer = src_in;

        /* we want the timecode of the last sample added but we are given the
           timecode of the first - add the time in ms that the frames added
           represent */

        // Copy samples into audiobuffer, with upmix if necessary
        if ((len = CopyWithUpmix((char *)buffer, frames, org_waud)) <= 0)
        {
            continue;
        }

        frames = len / bpf;
        frames_final += frames;

        bdiff = kAudioRingBufferSize - waud;

        if (pSoundStretch)
        {
            // does not change the timecode, only the number of samples
            org_waud     = waud;
            int bdFrames = bdiff / bpf;

            if (bdiff < len)
            {
                pSoundStretch->putSamples((STST *)(WPOS), bdFrames);
                pSoundStretch->putSamples((STST *)ABUF, (len - bdiff) / bpf);
            }
            else
                pSoundStretch->putSamples((STST *)(WPOS), frames);

            int nFrames = pSoundStretch->numSamples();
            if (nFrames > frames)
                CheckFreeSpace(nFrames);

            len = nFrames * bpf;

            if (nFrames > bdFrames)
            {
                nFrames -= pSoundStretch->receiveSamples((STST *)(WPOS),
                                                         bdFrames);
                org_waud = 0;
            }
            if (nFrames > 0)
                nFrames = pSoundStretch->receiveSamples((STST *)(WPOS),
                                                        nFrames);

            org_waud += nFrames * bpf;
        }

        if (internal_vol && SWVolume())
        {
            org_waud    = waud;
            int num     = len;

            if (bdiff <= num)
            {
                AudioOutputUtil::AdjustVolume(WPOS, bdiff, volume,
                                              music, needs_upmix && upmixer);
                num -= bdiff;
                org_waud = 0;
            }
            if (num > 0)
                AudioOutputUtil::AdjustVolume(WPOS, num, volume,
                                              music, needs_upmix && upmixer);
            org_waud += num;
        }

        if (encoder)
        {
            org_waud            = waud;
            int org_waud2       = waud;
            int remaining       = len;
            int to_get          = 0;
            // The AC3 encoder can only work on 128kB of data at a time
            int maxframes       = ((INBUFSIZE / encoder->FrameSize() - 1) *
                                   encoder->FrameSize()) & ~0xf;

            do
            {
                len = remaining;
                if (len > maxframes)
                {
                    len = maxframes;
                }
                remaining -= len;

                bdiff = kAudioRingBufferSize - org_waud;
                if (bdiff < len)
                {
                    encoder->Encode(WPOS, bdiff, processing);
                    to_get = encoder->Encode(ABUF, len - bdiff, processing);
                    org_waud = len - bdiff;
                }
                else
                {
                    to_get = encoder->Encode(WPOS, len, processing);
                    org_waud += len;
                }

                bdiff = kAudioRingBufferSize - org_waud2;
                if (bdiff <= to_get)
                {
                    encoder->GetFrames(audiobuffer + org_waud2, bdiff);
                    to_get -= bdiff ;
                    org_waud2 = 0;
                }
                if (to_get > 0)
                    encoder->GetFrames(audiobuffer + org_waud2, to_get);

                org_waud2 += to_get;
            }
            while (remaining > 0);
            org_waud = org_waud2;
        }

        waud = org_waud;
    }

    SetAudiotime(frames_final, timecode);

    return true;
}

/**
 * Report status via an OutputEvent
 */
void AudioOutputBase::Status()
{
    long ct = GetAudiotime();

    if (ct < 0)
        ct = 0;

    if (source_bitrate == -1)
        source_bitrate = source_samplerate * source_channels *
                         output_settings->FormatToBits(format);

    if (ct / 1000 != current_seconds)
    {
        current_seconds = ct / 1000;
        OutputEvent e(current_seconds, ct, source_bitrate, source_samplerate,
                      output_settings->FormatToBits(format), source_channels);
        dispatch(e);
    }
}

/**
 * Fill in the number of bytes in the audiobuffer and the total
 * size of the audiobuffer
 */
void AudioOutputBase::GetBufferStatus(uint &fill, uint &total)
{
    fill  = kAudioRingBufferSize - audiofree();
    total = kAudioRingBufferSize;
}

/**
 * Run in the output thread, write frames to the output device
 * as they become available and there's space in the device
 * buffer to write them
 */
void AudioOutputBase::OutputAudioLoop(void)
{
    uchar *zeros        = new uchar[fragment_size];
    uchar *fragment_buf = new uchar[fragment_size + 16];
    uchar *fragment     = (uchar *)AOALIGN(fragment_buf[0]);

    // to reduce startup latency, write silence in 8ms chunks
    int zero_fragment_size = (int)(0.008*samplerate/channels);
    // make sure its a multiple of output_bytes_per_frame
    zero_fragment_size *= output_bytes_per_frame;
    if (zero_fragment_size > fragment_size)
        zero_fragment_size = fragment_size;

    bzero(zeros, fragment_size);

    while (!killaudio)
    {
        if (pauseaudio)
        {
            if (!actually_paused)
            {
                VBAUDIO("OutputAudioLoop: audio paused");
                OutputEvent e(OutputEvent::Paused);
                dispatch(e);
                was_paused = true;
            }

            actually_paused = true;
            audiotime = 0; // mark 'audiotime' as invalid.

            // only send zeros if card doesn't already have at least one
            // fragment of zeros -dag
            WriteAudio(zeros, zero_fragment_size);
            continue;
        }
        else
        {
            if (was_paused)
            {
                VBAUDIO("OutputAudioLoop: Play Event");
                OutputEvent e(OutputEvent::Playing);
                dispatch(e);
                was_paused = false;
            }
        }

        /* do audio output */
        int ready = audioready();

        // wait for the buffer to fill with enough to play
        if (fragment_size > ready)
        {
            if (ready > 0)  // only log if we're sending some audio
                VBAUDIOTS(QString("audio waiting for buffer to fill: "
                                  "have %1 want %2")
                          .arg(ready).arg(fragment_size));

            usleep(10000);
            continue;
        }

#ifdef AUDIOTSTESTING
        VBAUDIOTS("WriteAudio Start");
#endif
        Status();

        // delay setting raud until after phys buffer is filled
        // so GetAudiotime will be accurate without locking
        reset_active.TestAndDeref();
        int next_raud = raud;
        if (GetAudioData(fragment, fragment_size, true, &next_raud))
        {
            if (!reset_active.TestAndDeref())
            {
                WriteAudio(fragment, fragment_size);
                if (!reset_active.TestAndDeref())
                    raud = next_raud;
            }
        }
#ifdef AUDIOTSTESTING
        GetAudiotime();
        VBAUDIOTS("WriteAudio Done");
#endif

    }

    delete[] zeros;
    delete[] fragment_buf;
    VBAUDIO("OutputAudioLoop: Stop Event");
    OutputEvent e(OutputEvent::Stopped);
    dispatch(e);
}

/**
 * Copy frames from the audiobuffer into the buffer provided
 *
 * If 'full_buffer' is true we copy either 'size' bytes (if available) or
 * nothing. Otherwise, we'll copy less than 'size' bytes if that's all that's
 * available. Returns the number of bytes copied.
 */
int AudioOutputBase::GetAudioData(uchar *buffer, int size, bool full_buffer,
                                  int *local_raud)
{

#define LRPOS audiobuffer + *local_raud
    // re-check audioready() in case things changed.
    // for example, ClearAfterSeek() might have run
    int avail_size   = audioready();
    int frag_size    = size;
    int written_size = size;

    if (local_raud == NULL)
        local_raud = &raud;

    if (!full_buffer && (size > avail_size))
    {
        // when full_buffer is false, return any available data
        frag_size = avail_size;
        written_size = frag_size;
    }

    if (!avail_size || (frag_size > avail_size))
        return 0;

    int bdiff = kAudioRingBufferSize - raud;

    int obytes = output_settings->SampleSize(output_format);
    bool fromFloats = processing && !enc && output_format != FORMAT_FLT;

    // Scale if necessary
    if (fromFloats && obytes != 4)
        frag_size *= 4 / obytes;

    int off = 0;

    if (bdiff <= frag_size)
    {
        if (fromFloats)
            off = AudioOutputUtil::fromFloat(output_format, buffer,
                                             LRPOS, bdiff);
        else
        {
            memcpy(buffer, LRPOS, bdiff);
            off = bdiff;
        }

        frag_size -= bdiff;
        *local_raud = 0;
    }
    if (frag_size > 0)
    {
        if (fromFloats)
            AudioOutputUtil::fromFloat(output_format, buffer + off,
                                       LRPOS, frag_size);
        else
            memcpy(buffer + off, LRPOS, frag_size);
    }

    *local_raud += frag_size;

    // Mute individual channels through mono->stereo duplication
    MuteState mute_state = GetMuteState();
    if (written_size && channels > 1 &&
        (mute_state == kMuteLeft || mute_state == kMuteRight))
    {
        AudioOutputUtil::MuteChannel(obytes << 3, channels,
                                     mute_state == kMuteLeft ? 0 : 1,
                                     buffer, written_size);
    }

    return written_size;
}

/**
 * Block until all available frames have been written to the device
 */
void AudioOutputBase::Drain()
{
    while (audioready() > fragment_size)
        usleep(1000);
}

/**
 * Main routine for the output thread
 */
void AudioOutputBase::run(void)
{
    VBAUDIO(QString("kickoffOutputAudioLoop: pid = %1").arg(getpid()));
    OutputAudioLoop();
    VBAUDIO("kickoffOutputAudioLoop exiting");
}

int AudioOutputBase::readOutputData(unsigned char*, int)
{
    VBERROR("AudioOutputBase should not be getting asked to readOutputData()");
    return 0;
}


