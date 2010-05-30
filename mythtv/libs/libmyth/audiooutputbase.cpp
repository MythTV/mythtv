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
    passthru(false),            enc(false),
    reenc(false),
    stretchfactor(1.0f),

    source(settings.source),    killaudio(false),

    pauseaudio(false),          actually_paused(false),
    was_paused(false),          unpause_when_ready(false),

    set_initial_vol(settings.set_initial_vol),
    buffer_output_data_for_use(false),

    // private
    output_settings(NULL),

    need_resampler(false),      src_ctx(NULL),

    pSoundStretch(NULL),
    encoder(NULL),              upmixer(NULL),
    source_channels(-1),        source_samplerate(0),
    source_bytes_per_frame(0),
    needs_upmix(false),         needs_downmix(false),
    surround_mode(QUALITY_LOW), old_stretchfactor(1.0f),
    volume(80),                 volumeControl(NULL),

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
    memory_corruption_test2(0xdeadbeef),
    memory_corruption_test3(0xdeadbeef)
{
    src_in = (float *)AOALIGN(src_in_buf);
    // The following are not bzero() because MS Windows doesn't like it.
    memset(&src_data,          0, sizeof(SRC_DATA));
    memset(src_in,             0, sizeof(float) * kAudioSRCInputSize);
    memset(src_out,            0, sizeof(float) * kAudioSRCOutputSize);
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

    bool ignore, upmix_default;
    AudioOutput::AudioSetup(max_channels, allow_ac3_passthru,
                            ignore, allow_multipcm, upmix_default);

    if (settings.upmixer == 0)      // Use general settings (video)
        configured_channels = upmix_default ? max_channels : 2;
    else if (settings.upmixer == 1) // music, upmixer off
        configured_channels = 2;
    else                            // music, upmixer on
        configured_channels = max_channels;
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
    if (output_settings)
        delete output_settings;

    assert(memory_corruption_test0 == 0xdeadbeef);
    assert(memory_corruption_test1 == 0xdeadbeef);
    assert(memory_corruption_test2 == 0xdeadbeef);
    assert(memory_corruption_test3 == 0xdeadbeef);
}

bool AudioOutputBase::CanPassthrough(void) const
{
    if (!output_settings)
        return false;
    return (output_settings->IsSupportedFormat(FORMAT_S16) &&
            allow_ac3_passthru);
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
    if (max_channels == 2 || source_channels > 2 || passthru)
        return false;

    // Reset audiobuffer now to prevent click
    audio_buflock.lock();
    avsync_lock.lock();
    waud = raud = 0;

    configured_channels =
        configured_channels == max_channels ? 2 : max_channels;

    const AudioSettings settings(format, source_channels, codec,
                                 source_samplerate, passthru);
    audio_buflock.unlock();
    avsync_lock.unlock();
    Reconfigure(settings);
    return (configured_channels == max_channels);
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
        /* Might we reencode a bitstream that's been decoded for timestretch?
           If the device doesn't support the number of channels - see below */
        if (allow_ac3_passthru &&
            (settings.codec == CODEC_ID_AC3 || settings.codec == CODEC_ID_DTS))
        {
            lreenc = true;
        }

        // Enough channels? Upmix if not, but only from mono/stereo to 5.1
        if (settings.channels <= 2 && settings.channels < configured_channels)
        {
            int conf_channels = (configured_channels > 6) ? 6 : configured_channels;
            VBAUDIO(QString("Needs upmix from %1 -> %2 channels")
                    .arg(settings.channels).arg(conf_channels));
            settings.channels = conf_channels;
            lneeds_upmix = true;
        }

        else if (settings.channels > configured_channels)
        {
            VBAUDIO(QString("Needs downmix from %1 -> %2 channels")
                    .arg(settings.channels).arg(configured_channels));
            settings.channels = configured_channels;
            lneeds_downmix = true;
        }
    }

    ClearError();

    // Check if anything has changed
    bool general_deps = settings.format == format &&
                        settings.samplerate  == source_samplerate &&
                        settings.use_passthru == passthru &&
                        lneeds_upmix == needs_upmix && lreenc == reenc &&
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

    // Ask the subclass what we can send to the device
    if (!output_settings)
        output_settings = GetOutputSettings();

    VBAUDIO(QString("Original codec was %1, %2, %3 kHz, %4 channels")
            .arg(codec_id_string((CodecID)codec))
            .arg(output_settings->FormatToString(format))
            .arg(samplerate/1000).arg(source_channels));

    /* Encode to AC-3 if we're allowed to passthru but aren't currently
       and we have more than 2 channels but multichannel PCM is not supported
       or if the device just doesn't support the number of channels */
    enc = (CanPassthrough() && !passthru &&
           ((!allow_multipcm && configured_channels > 2) ||
            !output_settings->IsSupportedChannels(channels)));

    int dest_rate = 0;
    if ((need_resampler = !output_settings->IsSupportedRate(samplerate)))
        dest_rate = output_settings->NearestSupportedRate(samplerate);

    // Force resampling if we are encoding to AC3 and sr > 48k
    if (enc && (samplerate > 48000 || (need_resampler && dest_rate > 48000)))
    {
        VBAUDIO("Forcing resample to 48 kHz for AC3 encode");
        if (src_quality < 0)
            src_quality = QUALITY_MEDIUM;
        need_resampler = true;
        dest_rate = 48000;
    }

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
            output_format = output_settings->BestSupportedFormat();
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

    // Upmix to 5.1
    if (needs_upmix && source_channels <= 2 && configured_channels > 2)
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

    raud = waud = audbuf_timecode = audiotime = frames_buffered = 0;
    current_seconds = -1;
    was_paused = !pauseaudio;

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();
}

/**
 * Set the timecode of the samples most recently added to the audiobuffer
 *
 * Used by mythmusic for seeking since it doesn't provide timecodes to
 * AddSamples()
 */
void AudioOutputBase::SetTimecode(long long timecode)
{
    audbuf_timecode = audiotime = timecode;
    frames_buffered = (long long)((timecode * source_samplerate) / 1000);
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
int AudioOutputBase::GetAudiotime(void)
{
    if (audbuf_timecode == 0)
        return 0;

    int soundcard_buffer = 0;
    int obpf = output_bytes_per_frame;
    int totalbuffer;
    long long oldaudiotime;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       which is leaving the sound card at this instant.

       We use these variables:

       'effdsp' is frames/sec

       'audbuf_timecode' is the timecode of the audio that has just been
       written into the buffer.

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer. */

    soundcard_buffer = GetBufferedOnSoundcard(); // bytes

    QMutexLocker lockav(&avsync_lock);

    /* audioready tells us how many bytes are in audiobuffer
       scaled appropriately if output format != internal format */
    totalbuffer = audioready() + soundcard_buffer;

    if (needs_upmix && upmixer)
        totalbuffer += upmixer->frameLatency() * obpf;

    if (pSoundStretch)
    {
        totalbuffer += pSoundStretch->numUnprocessedSamples() * obpf /
                       stretchfactor;
        totalbuffer += pSoundStretch->numSamples() * obpf;
    }

    if (encoder)
        totalbuffer += encoder->Buffered();

    oldaudiotime = audiotime;

    audiotime = audbuf_timecode - (long long)(totalbuffer) * 100000 *
                                        stretchfactor / (obpf * effdsp);

    /* audiotime should never go backwards, but we might get a negative
       value if GetBufferedOnSoundcard() isn't updated by the driver very
       quickly (e.g. ALSA) */
    if (audiotime < oldaudiotime)
        audiotime = oldaudiotime;

    VBAUDIOTS(QString("GetAudiotime audt=%3 atc=%4 tb=%5 sb=%6 "
                      "sr=%7 obpf=%8 sf=%9")
              .arg(audiotime).arg(audbuf_timecode)
              .arg(totalbuffer).arg(soundcard_buffer)
              .arg(samplerate).arg(obpf).arg(stretchfactor));

    return (int)audiotime;
}

/**
 * Get the difference in timecode between the samples that are about to become
 * audible and the samples most recently added to the audiobuffer, i.e. the
 * time in ms representing the sum total of buffered samples
 */
int AudioOutputBase::GetAudioBufferedTime(void)
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

    int i = 0;
    while (i < frames)
        i += upmixer->putFrames(buffer + i * off, frames - i, source_channels);

    int nFrames = upmixer->numFrames();
    if (!nFrames)
        return 0;

    len = CheckFreeSpace(nFrames);

    int bdFrames = bdiff / bpf;
    if (bdFrames < nFrames)
    {
        upmixer->receiveFrames((float *)(WPOS), bdFrames);
        nFrames -= bdFrames;
        org_waud = 0;
    }
    if (nFrames > 0)
        upmixer->receiveFrames((float *)(WPOS), nFrames);

    org_waud += nFrames * bpf;
    return len;
}

/**
 * Add frames to the audiobuffer and perform any required processing
 *
 * Returns false if there's not enough space right now
 */
bool AudioOutputBase::AddSamples(void *buffer, int frames, long long timecode)
{
    int org_waud = waud,               afree = audiofree();
    int bpf      = bytes_per_frame,    len   = frames * source_bytes_per_frame;
    int used     = kAudioRingBufferSize - afree;
    bool music   = false;

    VBAUDIOTS(QString("AddSamples frames=%1, bytes=%2, used=%3, free=%4, "
                      "timecode=%5 needsupmix=%6")
              .arg(frames).arg(len).arg(used).arg(afree).arg(timecode)
              .arg(needs_upmix));

    /* See if we're waiting for new samples to be buffered before we unpause
       post channel change, seek, etc. Wait for 2 fragments to be buffered */
    if (unpause_when_ready && pauseaudio && audioready() > fragment_size << 1)
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
        timecode = (long long)(frames_buffered) * 1000 / source_samplerate;
        frames_buffered += frames;
        dispatchVisual((uchar *)buffer, len, timecode, source_channels,
                       output_settings->FormatToBits(format));
        music = true;
    }

    if (processing)
    {
        // Convert to floats
        len = AudioOutputUtil::toFloat(format, src_in, buffer, len);

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
            VBAUDIOTS("Buffer is full, AddSamples returning false");
            return false; // would overflow
    }

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
    audbuf_timecode = timecode + ((long long)(frames) * 100000 / effdsp);

    // Copy samples into audiobuffer, with upmix if necessary
    if ((len = CopyWithUpmix((char *)buffer, frames, org_waud)) <= 0)
        return true;

    frames = len / bpf;

    int bdiff = kAudioRingBufferSize - waud;

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
            nFrames -= pSoundStretch->receiveSamples((STST *)(WPOS), bdFrames);
            org_waud = 0;
        }
        if (nFrames > 0)
            nFrames = pSoundStretch->receiveSamples((STST *)(WPOS), nFrames);

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
        org_waud   = waud;
        int to_get = 0;

        if (bdiff < len)
        {
            encoder->Encode(WPOS, bdiff, processing);
            to_get = encoder->Encode(ABUF, len - bdiff, processing);
        }
        else
            to_get = encoder->Encode(WPOS, len, processing);

        if (bdiff <= to_get)
        {
            encoder->GetFrames(WPOS, bdiff);
            to_get -= bdiff;
            org_waud = 0;
        }
        if (to_get > 0)
            encoder->GetFrames(WPOS, to_get);

        org_waud += to_get;
    }

    waud = org_waud;

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
            WriteAudio(zeros, fragment_size);
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

        Status();

        if (GetAudioData(fragment, fragment_size, true))
            WriteAudio(fragment, fragment_size);
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
int AudioOutputBase::GetAudioData(uchar *buffer, int size, bool full_buffer)
{

    // re-check audioready() in case things changed.
    // for example, ClearAfterSeek() might have run
    int avail_size   = audioready();
    int frag_size    = size;
    int written_size = size;

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
                                             RPOS, bdiff);
        else
        {
            memcpy(buffer, RPOS, bdiff);
            off = bdiff;
        }

        frag_size -= bdiff;
        raud = 0;
    }
    if (frag_size > 0)
    {
        if (fromFloats)
            AudioOutputUtil::fromFloat(output_format, buffer + off,
                                       RPOS, frag_size);
        else
            memcpy(buffer + off, RPOS, frag_size);
    }

    raud += frag_size;

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

/* vim: set expandtab tabstop=4 shiftwidth=4: */

