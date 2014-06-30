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
#include "spdifencoder.h"
#include "mythlogging.h"

#define LOC QString("AOBase: ")

#define WPOS audiobuffer + org_waud
#define RPOS audiobuffer + raud
#define ABUF audiobuffer
#define STST soundtouch::SAMPLETYPE
#define AOALIGN(x) (((long)&x + 15) & ~0xf);

// 1,2,5 and 7 channels are currently valid for upmixing if required
#define UPMIX_CHANNEL_MASK ((1<<1)|(1<<2)|(1<<5)|1<<7)
#define IS_VALID_UPMIX_CHANNEL(ch) ((1 << (ch)) & UPMIX_CHANNEL_MASK)

const char *AudioOutputBase::quality_string(int q)
{
    switch(q)
    {
        case QUALITY_DISABLED: return "disabled";
        case QUALITY_LOW:      return "low";
        case QUALITY_MEDIUM:   return "medium";
        case QUALITY_HIGH:     return "high";
        default:               return "unknown";
    }
}

AudioOutputBase::AudioOutputBase(const AudioSettings &settings) :
    MThread("AudioOutputBase"),
    // protected
    channels(-1),               codec(AV_CODEC_ID_NONE),
    bytes_per_frame(0),         output_bytes_per_frame(0),
    format(FORMAT_NONE),        output_format(FORMAT_NONE),
    samplerate(-1),             effdsp(0),
    fragment_size(0),           soundcard_buffer_size(0),

    main_device(settings.GetMainDevice()),
    passthru_device(settings.GetPassthruDevice()),
    m_discretedigital(false),   passthru(false),
    enc(false),                 reenc(false),
    stretchfactor(1.0f),
    eff_stretchfactor(100000),

    source(settings.source),    killaudio(false),

    pauseaudio(false),          actually_paused(false),
    was_paused(false),          unpause_when_ready(false),

    set_initial_vol(settings.set_initial_vol),
    buffer_output_data_for_use(false),

    configured_channels(0),
    max_channels(0),
    src_quality(QUALITY_MEDIUM),

    // private
    output_settingsraw(NULL),   output_settings(NULL),
    output_settingsdigitalraw(NULL),   output_settingsdigital(NULL),
    need_resampler(false),      src_ctx(NULL),

    pSoundStretch(NULL),
    encoder(NULL),              upmixer(NULL),
    source_channels(-1),        source_samplerate(0),
    source_bytes_per_frame(0),  upmix_default(false),
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
    memory_corruption_test3(0xdeadbeef),
    m_configure_succeeded(false),m_length_last_data(0),
    m_spdifenc(NULL),           m_forcedprocessing(false),
    m_previousbpf(0)
{
    src_in = (float *)AOALIGN(src_in_buf);
    memset(&src_data,          0, sizeof(SRC_DATA));
    memset(src_in_buf,         0, sizeof(src_in_buf));
    memset(audiobuffer,        0, sizeof(audiobuffer));

    // Handle override of SRC quality settings
    if (gCoreContext->GetNumSetting("SRCQualityOverride", false))
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
    if (output_settings != output_settingsdigital)
    {
        delete output_settingsdigital;
        delete output_settingsdigitalraw;
    }

    if (kAudioSRCOutputSize > 0)
        delete[] src_out;

    assert(memory_corruption_test0 == 0xdeadbeef);
    assert(memory_corruption_test1 == 0xdeadbeef);
    assert(memory_corruption_test2 == 0xdeadbeef);
    assert(memory_corruption_test3 == 0xdeadbeef);
}

void AudioOutputBase::InitSettings(const AudioSettings &settings)
{
    if (settings.custom)
    {
            // got a custom audio report already, use it
            // this was likely provided by the AudioTest utility
        output_settings = new AudioOutputSettings;
        *output_settings = *settings.custom;
        output_settingsdigital = output_settings;
        max_channels = output_settings->BestSupportedChannels();
        configured_channels = max_channels;
        return;
    }

    // Ask the subclass what we can send to the device
    output_settings = GetOutputSettingsUsers(false);
    output_settingsdigital = GetOutputSettingsUsers(true);

    max_channels = max(output_settings->BestSupportedChannels(),
                       output_settingsdigital->BestSupportedChannels());
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
 * options (AC3, DTS, E-AC3 and TrueHD)
 */
AudioOutputSettings* AudioOutputBase::GetOutputSettingsCleaned(bool digital)
{
        // If we've already checked the port, use the cache
        // version instead
    if (!m_discretedigital || !digital)
    {
        digital = false;
        if (output_settingsraw)
            return output_settingsraw;
    }
    else if (output_settingsdigitalraw)
        return output_settingsdigitalraw;

    AudioOutputSettings* aosettings = GetOutputSettings(digital);
    if (aosettings)
        aosettings->GetCleaned();
    else
        aosettings = new AudioOutputSettings(true);

    if (digital)
        return (output_settingsdigitalraw = aosettings);
    else
        return (output_settingsraw = aosettings);
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3, DTS, E-AC3 and TrueHD) as well as the user settings
 */
AudioOutputSettings* AudioOutputBase::GetOutputSettingsUsers(bool digital)
{
    if (!m_discretedigital || !digital)
    {
        digital = false;
        if (output_settings)
            return output_settings;
    }
    else if (output_settingsdigital)
        return output_settingsdigital;

    AudioOutputSettings* aosettings = new AudioOutputSettings;

    *aosettings = *GetOutputSettingsCleaned(digital);
    aosettings->GetUsers();

    if (digital)
        return (output_settingsdigital = aosettings);
    else
        return (output_settings = aosettings);
}

/**
 * Test if we can output digital audio and if sample rate is supported
 */
bool AudioOutputBase::CanPassthrough(int samplerate, int channels,
                                     int codec, int profile) const
{
    DigitalFeature arg = FEATURE_NONE;
    bool           ret = !(internal_vol && SWVolume());

    switch(codec)
    {
        case AV_CODEC_ID_AC3:
            arg = FEATURE_AC3;
            break;
        case AV_CODEC_ID_DTS:
            switch(profile)
            {
                case FF_PROFILE_DTS:
                case FF_PROFILE_DTS_ES:
                case FF_PROFILE_DTS_96_24:
                    arg = FEATURE_DTS;
                    break;
                case FF_PROFILE_DTS_HD_HRA:
                case FF_PROFILE_DTS_HD_MA:
                    arg = FEATURE_DTSHD;
                    break;
                default:
                    break;
            }
            break;
        case AV_CODEC_ID_EAC3:
            arg = FEATURE_EAC3;
            break;
        case AV_CODEC_ID_TRUEHD:
            arg = FEATURE_TRUEHD;
            break;
    }
    // we can't passthrough any other codecs than those defined above
    ret &= output_settingsdigital->canFeature(arg);
    ret &= output_settingsdigital->IsSupportedFormat(FORMAT_S16);
    ret &= output_settingsdigital->IsSupportedRate(samplerate);
    // if we must resample to 48kHz ; we can't passthrough
    ret &= !((samplerate != 48000) &&
             gCoreContext->GetNumSetting("Audio48kOverride", false));
    // Don't know any cards that support spdif clocked at < 44100
    // Some US cable transmissions have 2ch 32k AC-3 streams
    ret &= samplerate >= 44100;
    if (!ret)
        return false;
    // Will passthrough if surround audio was defined. Amplifier will
    // do the downmix if required
    ret &= max_channels >= 6 && channels > 2;
    // Stereo content will always be decoded so it can later be upmixed
    // unless audio is configured for stereo. We can passthrough otherwise
    ret |= max_channels == 2;

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

/**
 * Set the timestretch factor
 *
 * You must hold the audio_buflock to call this safely
 */
void AudioOutputBase::SetStretchFactorLocked(float lstretchfactor)
{
    if (stretchfactor == lstretchfactor && pSoundStretch)
        return;

    stretchfactor = lstretchfactor;

    int channels = needs_upmix || needs_downmix ?
        configured_channels : source_channels;
    if (channels < 1 || channels > 8 || !m_configure_succeeded)
        return;

    bool willstretch = stretchfactor < 0.99f || stretchfactor > 1.01f;
    eff_stretchfactor = (int)(100000.0f * lstretchfactor + 0.5);

    if (pSoundStretch)
    {
        if (!willstretch && m_forcedprocessing)
        {
            m_forcedprocessing = false;
            processing = false;
            delete pSoundStretch;
            pSoundStretch = NULL;
            VBGENERAL(QString("Cancelling time stretch"));
            bytes_per_frame = m_previousbpf;
            waud = raud = 0;
            reset_active.Ref();
        }
        else
        {
            VBGENERAL(QString("Changing time stretch to %1")
                      .arg(stretchfactor));
            pSoundStretch->setTempo(stretchfactor);
        }
    }
    else if (willstretch)
    {
        VBGENERAL(QString("Using time stretch %1").arg(stretchfactor));
        pSoundStretch = new soundtouch::SoundTouch();
        pSoundStretch->setSampleRate(samplerate);
        pSoundStretch->setChannels(channels);
        pSoundStretch->setTempo(stretchfactor);
        pSoundStretch->setSetting(SETTING_SEQUENCE_MS, 35);
        /* If we weren't already processing we need to turn on float conversion
           adjust sample and frame sizes accordingly and dump the contents of
           the audiobuffer */
        if (!processing)
        {
            processing = true;
            m_forcedprocessing = true;
            m_previousbpf = bytes_per_frame;
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
 * Source is currently being upmixed
 */
bool AudioOutputBase::IsUpmixing(void)
{
    return needs_upmix && upmixer;
}

/**
 * Toggle between stereo and upmixed 5.1 if the source material is stereo
 */
bool AudioOutputBase::ToggleUpmix(void)
{
    // Can only upmix from mono/stereo to 6 ch
    if (max_channels == 2 || source_channels > 2 || passthru)
        return false;

    upmix_default = !upmix_default;

    const AudioSettings settings(format, source_channels, codec,
                                 source_samplerate, passthru);
    Reconfigure(settings);
    return IsUpmixing();
}

/**
 * Upmixing of the current source is available if requested
 */
bool AudioOutputBase::CanUpmix(void)
{
    return !passthru && source_channels <= 2 && max_channels > 2;
}

/*
 * Setup samplerate and number of channels for passthrough
 * Create SPDIF encoder and true if successful
 */
bool AudioOutputBase::SetupPassthrough(int codec, int codec_profile,
                                       int &samplerate_tmp, int &channels_tmp)
{
    if (codec == AV_CODEC_ID_DTS &&
        !output_settingsdigital->canFeature(FEATURE_DTSHD))
    {
        // We do not support DTS-HD bitstream so force extraction of the
        // DTS core track instead
        codec_profile = FF_PROFILE_DTS;
    }
    QString log = AudioOutputSettings::GetPassthroughParams(
        codec, codec_profile,
        samplerate_tmp, channels_tmp,
        output_settingsdigital->GetMaxHDRate() == 768000);
    VBAUDIO("Setting " + log + " passthrough");

    if (m_spdifenc)
    {
        delete m_spdifenc;
    }

    m_spdifenc = new SPDIFEncoder("spdif", codec);
    if (m_spdifenc->Succeeded() && codec == AV_CODEC_ID_DTS)
    {
        switch(codec_profile)
        {
            case FF_PROFILE_DTS:
            case FF_PROFILE_DTS_ES:
            case FF_PROFILE_DTS_96_24:
                m_spdifenc->SetMaxHDRate(0);
                break;
            case FF_PROFILE_DTS_HD_HRA:
            case FF_PROFILE_DTS_HD_MA:
                m_spdifenc->SetMaxHDRate(samplerate_tmp * channels_tmp / 2);
                break;
        }
    }

    if (!m_spdifenc->Succeeded())
    {
        delete m_spdifenc;
        m_spdifenc = NULL;
        return false;
    }
    return true;
}

AudioOutputSettings *AudioOutputBase::OutputSettings(bool digital)
{
    if (digital)
        return output_settingsdigital;
    return output_settings;
}

/**
 * (Re)Configure AudioOutputBase
 *
 * Must be called from concrete subclasses
 */
void AudioOutputBase::Reconfigure(const AudioSettings &orig_settings)
{
    AudioSettings settings    = orig_settings;
    int  lsource_channels     = settings.channels;
    int  lconfigured_channels = configured_channels;
    bool lneeds_upmix         = false;
    bool lneeds_downmix       = false;
    bool lreenc               = false;
    bool lenc                 = false;

    if (!settings.use_passthru)
    {
        // Do we upmix stereo or mono?
        lconfigured_channels =
            (upmix_default && lsource_channels <= 2) ? 6 : lsource_channels;
        bool cando_channels =
            output_settings->IsSupportedChannels(lconfigured_channels);

        // check if the number of channels could be transmitted via AC3 encoding
        lenc = output_settingsdigital->canFeature(FEATURE_AC3) &&
            (!output_settings->canFeature(FEATURE_LPCM) &&
             lconfigured_channels > 2 && lconfigured_channels <= 6);

        if (!lenc && !cando_channels)
        {
            // if hardware doesn't support source audio configuration
            // we will upmix/downmix to what we can
            // (can safely assume hardware supports stereo)
            switch (lconfigured_channels)
            {
                case 7:
                    lconfigured_channels = 8;
                    break;
                case 8:
                case 5:
                    lconfigured_channels = 6;
                        break;
                case 6:
                case 4:
                case 3:
                case 2: //Will never happen
                    lconfigured_channels = 2;
                    break;
                case 1:
                    lconfigured_channels = upmix_default ? 6 : 2;
                    break;
                default:
                    lconfigured_channels = 2;
                    break;
            }
        }
        // Make sure we never attempt to output more than what we can
        // the upmixer can only upmix to 6 channels when source < 6
        if (lsource_channels <= 6)
            lconfigured_channels = min(lconfigured_channels, 6);
        lconfigured_channels = min(lconfigured_channels, max_channels);
        /* Encode to AC-3 if we're allowed to passthru but aren't currently
           and we have more than 2 channels but multichannel PCM is not
           supported or if the device just doesn't support the number of
           channels */
        lenc = output_settingsdigital->canFeature(FEATURE_AC3) &&
            ((!output_settings->canFeature(FEATURE_LPCM) &&
              lconfigured_channels > 2) ||
             !output_settings->IsSupportedChannels(lconfigured_channels));

        /* Might we reencode a bitstream that's been decoded for timestretch?
           If the device doesn't support the number of channels - see below */
        if (output_settingsdigital->canFeature(FEATURE_AC3) &&
            (settings.codec == AV_CODEC_ID_AC3 || settings.codec == AV_CODEC_ID_DTS))
        {
            lreenc = true;
        }

        // Enough channels? Upmix if not, but only from mono/stereo/5.0 to 5.1
        if (IS_VALID_UPMIX_CHANNEL(settings.channels) &&
            settings.channels < lconfigured_channels)
        {
            VBAUDIO(QString("Needs upmix from %1 -> %2 channels")
                    .arg(settings.channels).arg(lconfigured_channels));
            settings.channels = lconfigured_channels;
            lneeds_upmix = true;
        }
        else if (settings.channels > lconfigured_channels)
        {
            VBAUDIO(QString("Needs downmix from %1 -> %2 channels")
                    .arg(settings.channels).arg(lconfigured_channels));
            settings.channels = lconfigured_channels;
            lneeds_downmix = true;
        }
    }

    ClearError();

    bool general_deps = true;

    /* Set samplerate_tmp and channels_tmp to appropriate values
       if passing through */
    int samplerate_tmp, channels_tmp;
    if (settings.use_passthru)
    {
        samplerate_tmp = settings.samplerate;
        SetupPassthrough(settings.codec, settings.codec_profile,
                         samplerate_tmp, channels_tmp);
        general_deps = samplerate == samplerate_tmp && channels == channels_tmp;
    }

    // Check if anything has changed
    general_deps &=
        settings.format == format &&
        settings.samplerate  == source_samplerate &&
        settings.use_passthru == passthru &&
        lconfigured_channels == configured_channels &&
        lneeds_upmix == needs_upmix && lreenc == reenc &&
        lsource_channels == source_channels &&
        lneeds_downmix == needs_downmix;

    if (general_deps && m_configure_succeeded)
    {
        VBAUDIO("Reconfigure(): No change -> exiting");
        return;
    }

    KillAudio();

    QMutexLocker lock(&audio_buflock);
    QMutexLocker lockav(&avsync_lock);

    waud = raud = 0;
    reset_active.Clear();
    actually_paused = processing = m_forcedprocessing = false;

    channels               = settings.channels;
    source_channels        = lsource_channels;
    reenc                  = lreenc;
    codec                  = settings.codec;
    passthru               = settings.use_passthru;
    configured_channels    = lconfigured_channels;
    needs_upmix            = lneeds_upmix;
    needs_downmix          = lneeds_downmix;
    format                 = output_format   = settings.format;
    source_samplerate      = samplerate      = settings.samplerate;
    enc                    = lenc;

    killaudio = pauseaudio = false;
    was_paused = true;

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
            .arg(ff_codec_id_string((AVCodecID)codec))
            .arg(output_settings->FormatToString(format))
            .arg(samplerate/1000)
            .arg(source_channels));

    if (needs_downmix && source_channels > 8)
    {
        Error(QObject::tr("Aborting Audio Reconfigure. "
              "Can't handle audio with more than 8 channels."));
        return;
    }

    VBAUDIO(QString("enc(%1), passthru(%2), features (%3) "
                    "configured_channels(%4), %5 channels supported(%6) "
                    "max_channels(%7)")
            .arg(enc)
            .arg(passthru)
            .arg(output_settingsdigital->FeaturesToString())
            .arg(configured_channels)
            .arg(channels)
            .arg(OutputSettings(enc || passthru)->IsSupportedChannels(channels))
            .arg(max_channels));

    int dest_rate = 0;

    // Force resampling if we are encoding to AC3 and sr > 48k
    // or if 48k override was checked in settings
    if ((samplerate != 48000 &&
         gCoreContext->GetNumSetting("Audio48kOverride", false)) ||
         (enc && (samplerate > 48000)))
    {
        VBAUDIO("Forcing resample to 48 kHz");
        if (src_quality < 0)
            src_quality = QUALITY_MEDIUM;
        need_resampler = true;
        dest_rate = 48000;
    }
        // this will always be false for passthrough audio as
        // CanPassthrough() already tested these conditions
    else if ((need_resampler =
              !OutputSettings(enc || passthru)->IsSupportedRate(samplerate)))
    {
        dest_rate = OutputSettings(enc)->NearestSupportedRate(samplerate);
    }

    if (need_resampler && src_quality > QUALITY_DISABLED)
    {
        int error;
        samplerate = dest_rate;

        VBGENERAL(QString("Resampling from %1 kHz to %2 kHz with quality %3")
                .arg(settings.samplerate/1000).arg(samplerate/1000)
                .arg(quality_string(src_quality)));

        int chans = needs_downmix ? configured_channels : source_channels;

        src_ctx = src_new(2-src_quality, chans, &error);
        if (error)
        {
            Error(QObject::tr("Error creating resampler: %1")
                  .arg(src_strerror(error)));
            src_ctx = NULL;
            return;
        }

        src_data.src_ratio = (double)samplerate / settings.samplerate;
        src_data.data_in   = src_in;
        int newsize        = (int)(kAudioSRCInputSize * src_data.src_ratio + 15)
                             & ~0xf;

        if (kAudioSRCOutputSize < newsize)
        {
            kAudioSRCOutputSize = newsize;
            VBAUDIO(QString("Resampler allocating %1").arg(newsize));
            if (src_out)
                delete[] src_out;
            src_out = new float[kAudioSRCOutputSize];
        }
        src_data.data_out       = src_out;
        src_data.output_frames  = kAudioSRCOutputSize / chans;
        src_data.end_of_input = 0;
    }

    if (enc)
    {
        if (reenc)
            VBAUDIO("Reencoding decoded AC-3/DTS to AC-3");

        VBAUDIO(QString("Creating AC-3 Encoder with sr = %1, ch = %2")
                .arg(samplerate).arg(configured_channels));

        encoder = new AudioOutputDigitalEncoder();
        if (!encoder->Init(AV_CODEC_ID_AC3, 448000, samplerate,
                           configured_channels))
        {
            Error(QObject::tr("AC-3 encoder initialization failed"));
            delete encoder;
            encoder = NULL;
            enc = false;
            // upmixing will fail if we needed the encoder
            needs_upmix = false;
        }
    }

    if (passthru)
    {
        //AC3, DTS, DTS-HD MA and TrueHD all use 16 bits samples
        channels = channels_tmp;
        samplerate = samplerate_tmp;
        format = output_format = FORMAT_S16;
        source_bytes_per_frame = channels *
            output_settings->SampleSize(format);
    }
    else
    {
        source_bytes_per_frame = source_channels *
            output_settings->SampleSize(format);
    }

    // Turn on float conversion?
    if (need_resampler || needs_upmix || needs_downmix ||
        stretchfactor != 1.0f || (internal_vol && SWVolume()) ||
        (enc && output_format != FORMAT_S16) ||
        !OutputSettings(enc || passthru)->IsSupportedFormat(output_format))
    {
        VBAUDIO("Audio processing enabled");
        processing  = true;
        if (enc)
            output_format = FORMAT_S16;  // Output s16le for AC-3 encoder
        else
            output_format = output_settings->BestSupportedFormat();
    }

    bytes_per_frame =  processing ?
        sizeof(float) : output_settings->SampleSize(format);
    bytes_per_frame *= channels;

    if (enc)
        channels = 2; // But only post-encoder

    output_bytes_per_frame = channels *
                             output_settings->SampleSize(output_format);

    VBGENERAL(
        QString("Opening audio device '%1' ch %2(%3) sr %4 sf %5 reenc %6")
        .arg(main_device).arg(channels).arg(source_channels).arg(samplerate)
        .arg(output_settings->FormatToString(output_format)).arg(reenc));

    audbuf_timecode = audiotime = frames_buffered = 0;
    current_seconds = source_bitrate = -1;
    effdsp = samplerate * 100;

    // Actually do the device specific open call
    if (!OpenDevice())
    {
        if (GetError().isEmpty())
            Error(QObject::tr("Aborting reconfigure"));
        else
            VBGENERAL("Aborting reconfigure");
        m_configure_succeeded = false;
        return;
    }

    VBAUDIO(QString("Audio fragment size: %1").arg(fragment_size));

    // Only used for software volume
    if (set_initial_vol && internal_vol && SWVolume())
    {
        VBAUDIO("Software volume enabled");
        volumeControl  = gCoreContext->GetSetting("MixerControl", "PCM");
        volumeControl += "MixerVolume";
        volume = gCoreContext->GetNumSetting(volumeControl, 80);
    }

    VolumeBase::SetChannels(configured_channels);
    VolumeBase::SyncVolume();
    VolumeBase::UpdateVolume();

    if (needs_upmix && configured_channels > 2)
    {
        surround_mode = gCoreContext->GetNumSetting("AudioUpmixType", QUALITY_HIGH);
        upmixer = new FreeSurround(samplerate, source == AUDIOOUTPUT_VIDEO,
                                   (FreeSurround::SurroundMode)surround_mode);
        VBAUDIO(QString("Create %1 quality upmixer done")
                .arg(quality_string(surround_mode)));
    }

    VBAUDIO(QString("Audio Stretch Factor: %1").arg(stretchfactor));
    SetStretchFactorLocked(old_stretchfactor);

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();

    if (unpause_when_ready)
        pauseaudio = actually_paused = true;

    m_configure_succeeded = true;

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
    VBAUDIO(QString("Pause %1").arg(paused));
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
    if (encoder)
    {
        waud = raud = 0;    // empty ring buffer
        memset(audiobuffer, 0, kAudioRingBufferSize);
    }
    else
    {
        waud = raud;        // empty ring buffer
    }
    reset_active.Ref();
    current_seconds = -1;
    was_paused = !pauseaudio;
    // clear any state that could remember previous audio in any active filters
    if (needs_upmix && upmixer)
        upmixer->flush();
    if (pSoundStretch)
        pSoundStretch->clear();
    if (encoder)
        encoder->clear();

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();
}

/**
 * Set the timecode of the samples most recently added to the audiobuffer
 *
 * Used by mythmusic for seeking since it doesn't provide timecodes to
 * AddData()
 */
void AudioOutputBase::SetTimecode(int64_t timecode)
{
    audbuf_timecode = audiotime = timecode;
    frames_buffered = (timecode * source_samplerate) / 1000;
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
    if (audbuf_timecode == 0 || !m_configure_succeeded)
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

    int soundcard_buffer = GetBufferedOnSoundcard(); // bytes

    /* audioready tells us how many bytes are in audiobuffer
       scaled appropriately if output format != internal format */
    int main_buffer = audioready();

    oldaudiotime = audiotime;

    /* timecode is the stretch adjusted version
       of major post-stretched buffer contents
       processing latencies are catered for in AddData/SetAudiotime
       to eliminate race */
    audiotime = audbuf_timecode - (effdsp && obpf ? (
        ((int64_t)(main_buffer + soundcard_buffer) * eff_stretchfactor) /
        (effdsp * obpf)) : 0);

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
 * between AddData calls
 */
void AudioOutputBase::SetAudiotime(int frames, int64_t timecode)
{
    int64_t processframes_stretched   = 0;
    int64_t processframes_unstretched = 0;
    int64_t old_audbuf_timecode       = audbuf_timecode;

    if (!m_configure_succeeded)
        return;

    if (needs_upmix && upmixer)
        processframes_unstretched -= upmixer->frameLatency();

    if (pSoundStretch)
    {
        processframes_unstretched -= pSoundStretch->numUnprocessedSamples();
        processframes_stretched   -= pSoundStretch->numSamples();
    }

    if (encoder)
    {
        processframes_stretched -= encoder->Buffered();
    }

    audbuf_timecode =
        timecode + (effdsp ? ((frames + processframes_unstretched) * 100000 +
                    (processframes_stretched * eff_stretchfactor)
                   ) / effdsp : 0);

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
    int64_t ret = audbuf_timecode - GetAudiotime();
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

/**
 * Copy frames into the audiobuffer, upmixing en route if necessary
 *
 * Returns the number of frames written, which may be less than requested
 * if the upmixer buffered some (or all) of them
 */
int AudioOutputBase::CopyWithUpmix(char *buffer, int frames, uint &org_waud)
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
        org_waud = (org_waud + num) % kAudioRingBufferSize;
        return len;
    }

    // Convert mono to stereo as most devices can't accept mono
    if (!upmixer)
    {
        // we're always in the case
        // configured_channels == 2 && source_channels == 1
        int bdFrames = bdiff / bpf;
        if (bdFrames <= frames)
        {
            AudioOutputUtil::MonoToStereo(WPOS, buffer, bdFrames);
            frames -= bdFrames;
            off = bdFrames * sizeof(float); // 1 channel of floats
            org_waud = 0;
        }
        if (frames > 0)
            AudioOutputUtil::MonoToStereo(WPOS, buffer + off, frames);

        org_waud = (org_waud + frames * bpf) % kAudioRingBufferSize;
        return len;
    }

    // Upmix to 6ch via FreeSurround
    // Calculate frame size of input
    off =  processing ? sizeof(float) : output_settings->SampleSize(format);
    off *= source_channels;

    int i = 0;
    len = 0;
    int nFrames, bdFrames;
    while (i < frames)
    {
        i += upmixer->putFrames(buffer + i * off, frames - i, source_channels);
        nFrames = upmixer->numFrames();
        if (!nFrames)
            continue;

        len += CheckFreeSpace(nFrames);

        bdFrames = (kAudioRingBufferSize - org_waud) / bpf;
        if (bdFrames < nFrames)
        {
            if ((org_waud % bpf) != 0)
            {
                VBERROR(QString("Upmixing: org_waud = %1 (bpf = %2)")
                        .arg(org_waud)
                        .arg(bpf));
            }
            upmixer->receiveFrames((float *)(WPOS), bdFrames);
            nFrames -= bdFrames;
            org_waud = 0;
        }
        if (nFrames > 0)
            upmixer->receiveFrames((float *)(WPOS), nFrames);

        org_waud = (org_waud + nFrames * bpf) % kAudioRingBufferSize;
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
    return AddData(in_buffer, in_frames * source_bytes_per_frame, timecode,
                   in_frames);
}

/**
 * Add data to the audiobuffer and perform any required processing
 *
 * Returns false if there's not enough space right now
 */
bool AudioOutputBase::AddData(void *in_buffer, int in_len,
                              int64_t timecode, int /*in_frames*/)
{
    int frames   = in_len / source_bytes_per_frame;
    void *buffer = in_buffer;
    int bpf      = bytes_per_frame;
    int len      = in_len;
    bool music   = false;
    int bdiff;

    if (!m_configure_succeeded)
    {
        LOG(VB_GENERAL, LOG_ERR, "AddData called with audio framework not "
                                 "initialised");
        m_length_last_data = 0;
        return false;
    }

    /* See if we're waiting for new samples to be buffered before we unpause
       post channel change, seek, etc. Wait for 4 fragments to be buffered */
    if (unpause_when_ready && pauseaudio && audioready() > fragment_size << 2)
    {
        unpause_when_ready = false;
        Pause(false);
    }

    // Don't write new samples if we're resetting the buffer or reconfiguring
    QMutexLocker lock(&audio_buflock);

    uint org_waud = waud;
    int  afree    = audiofree();
    int  used     = kAudioRingBufferSize - afree;

    if (passthru && m_spdifenc)
    {
        if (processing)
        {
            /*
             * We shouldn't encounter this case, but it can occur when
             * timestretch just got activated. So we will just drop the
             * data
             */
            LOG(VB_AUDIO, LOG_INFO,
                "Passthrough activated with audio processing. Dropping audio");
            return false;
        }
        // mux into an IEC958 packet
        m_spdifenc->WriteFrame((unsigned char *)in_buffer, len);
        len = m_spdifenc->GetProcessedSize();
        if (len > 0)
        {
            buffer = in_buffer = m_spdifenc->GetProcessedBuffer();
            m_spdifenc->Reset();
            frames = len / source_bytes_per_frame;
        }
        else
            frames = 0;
    }
    m_length_last_data = (int64_t)
        ((double)(len * 1000) / (source_samplerate * source_bytes_per_frame));

    VBAUDIOTS(QString("AddData frames=%1, bytes=%2, used=%3, free=%4, "
                      "timecode=%5 needsupmix=%6")
              .arg(frames).arg(len).arg(used).arg(afree).arg(timecode)
              .arg(needs_upmix));

    // Mythmusic doesn't give us timestamps
    if (timecode < 0)
    {
        timecode = (frames_buffered * 1000) / source_samplerate;
        frames_buffered += frames;
        music = true;
    }

    if (hasVisual())
    {
        // Send original samples to any attached visualisations
        dispatchVisual((uchar *)in_buffer, len, timecode, source_channels,
                       output_settings->FormatToBits(format));
    }

    // Calculate amount of free space required in ringbuffer
    if (processing)
    {
        int sampleSize = AudioOutputSettings::SampleSize(format);
        if (sampleSize <= 0)
        {
            // Would lead to division by zero (or unexpected results if negative)
            VBERROR("Sample size is <= 0, AddData returning false");
            return false;
        }

        // Final float conversion space requirement
        len = sizeof(*src_in_buf) / sampleSize * len;

        // Account for changes in number of channels
        if (needs_downmix)
            len = (len * configured_channels ) / source_channels;

        // Check we have enough space to write the data
        if (need_resampler && src_ctx)
            len = (int)ceilf(float(len) * src_data.src_ratio);

        if (needs_upmix)
            len = (len * configured_channels ) / source_channels;

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
        VBAUDIOTS("Buffer is full, AddData returning false");
        return false; // would overflow
    }

    int frames_remaining = frames;
    int frames_final = 0;
    int maxframes = (kAudioSRCInputSize / source_channels) & ~0xf;
    int offset = 0;

    while(frames_remaining > 0)
    {
        buffer = (char *)in_buffer + offset;
        frames = frames_remaining;
        len = frames * source_bytes_per_frame;

        if (processing)
        {
            if (frames > maxframes)
            {
                frames = maxframes;
                len = frames * source_bytes_per_frame;
                offset += len;
            }
            // Convert to floats
            len = AudioOutputUtil::toFloat(format, src_in, buffer, len);
        }

        frames_remaining -= frames;

        // Perform downmix if necessary
        if (needs_downmix)
            if(AudioOutputDownmix::DownmixFrames(source_channels,
                                                 configured_channels,
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
        if ((len % bpf) != 0 && bdiff < len)
        {
            VBERROR(QString("AddData: Corruption likely: len = %1 (bpf = %2)")
                    .arg(len)
                    .arg(bpf));
        }
        if ((bdiff % bpf) != 0 && bdiff < len)
        {
            VBERROR(QString("AddData: Corruption likely: bdiff = %1 (bpf = %2)")
                    .arg(bdiff)
                    .arg(bpf));
        }

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

            org_waud = (org_waud + nFrames * bpf) % kAudioRingBufferSize;
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
            org_waud = (org_waud + num) % kAudioRingBufferSize;
        }

        if (encoder)
        {
            org_waud            = waud;
            int to_get          = 0;

            if (bdiff < len)
            {
                encoder->Encode(WPOS, bdiff, processing ? FORMAT_FLT : format);
                to_get = encoder->Encode(ABUF, len - bdiff,
                                         processing ? FORMAT_FLT : format);
            }
            else
            {
                to_get = encoder->Encode(WPOS, len,
                                         processing ? FORMAT_FLT : format);
            }

            if (bdiff <= to_get)
            {
                encoder->GetFrames(WPOS, bdiff);
                to_get -= bdiff ;
                org_waud = 0;
            }
            if (to_get > 0)
                encoder->GetFrames(WPOS, to_get);

            org_waud = (org_waud + to_get) % kAudioRingBufferSize;
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
    memset(zeros, 0, fragment_size);

    // to reduce startup latency, write silence in 8ms chunks
    int zero_fragment_size = 8 * samplerate * output_bytes_per_frame / 1000;
    if (zero_fragment_size > fragment_size)
        zero_fragment_size = fragment_size;

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
        volatile uint next_raud = raud;
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
                                  volatile uint *local_raud)
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

    if (obytes <= 0)
        return 0;

    bool fromFloats = processing && !enc && output_format != FORMAT_FLT;

    // Scale if necessary
    if (fromFloats && obytes != sizeof(float))
        frag_size *= sizeof(float) / obytes;

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
    if (!enc && !passthru &&
        written_size && configured_channels > 1 &&
        (mute_state == kMuteLeft || mute_state == kMuteRight))
    {
        AudioOutputUtil::MuteChannel(obytes << 3, configured_channels,
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
    while (!pauseaudio && audioready() > fragment_size)
        usleep(1000);
    if (pauseaudio)
    {
        // Audio is paused and can't be drained, clear ringbuffer
        QMutexLocker lock(&audio_buflock);

        waud = raud = 0;
    }
}

/**
 * Main routine for the output thread
 */
void AudioOutputBase::run(void)
{
    RunProlog();
    VBAUDIO(QString("kickoffOutputAudioLoop: pid = %1").arg(getpid()));
    OutputAudioLoop();
    VBAUDIO("kickoffOutputAudioLoop exiting");
    RunEpilog();
}

int AudioOutputBase::readOutputData(unsigned char*, int)
{
    VBERROR("AudioOutputBase should not be getting asked to readOutputData()");
    return 0;
}
