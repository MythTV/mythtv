// C++ headers
#include <algorithm>
#include <cmath>
#include <limits>

using namespace std;

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
#include "mythconfig.h"

// AC3 encode currently disabled for Android
#if defined(Q_OS_ANDROID)
#define DISABLE_AC3_ENCODE
#endif

#define LOC QString("AOBase: ")

#define WPOS (m_audiobuffer + org_waud)
#define RPOS (m_audiobuffer + m_raud)
#define ABUF m_audiobuffer
#define STST soundtouch::SAMPLETYPE
#define AOALIGN(x) (((long)&(x) + 15) & ~0xf);

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
    m_main_device(settings.GetMainDevice()),
    m_passthru_device(settings.GetPassthruDevice()),
    source(settings.m_source),
    m_set_initial_vol(settings.m_set_initial_vol)
{
    m_src_in = (float *)AOALIGN(m_src_in_buf);

    if (m_main_device.startsWith("OpenMAX:")
        || m_main_device.startsWith("AudioTrack:"))
        m_usesSpdif = false;
    // Handle override of SRC quality settings
    if (gCoreContext->GetBoolSetting("SRCQualityOverride", false))
    {
        m_src_quality = gCoreContext->GetNumSetting("SRCQuality", QUALITY_MEDIUM);
        // Extra test to keep backward compatibility with earlier SRC setting
        if (m_src_quality > QUALITY_HIGH)
            m_src_quality = QUALITY_HIGH;

        VBAUDIO(QString("SRC quality = %1").arg(quality_string(m_src_quality)));
    }
}

/**
 * Destructor
 *
 * You must kill the output thread via KillAudio() prior to destruction
 */
AudioOutputBase::~AudioOutputBase()
{
    if (!m_killaudio)
        VBERROR("Programmer Error: "
                "~AudioOutputBase called, but KillAudio has not been called!");

    // We got this from a subclass, delete it
    delete m_output_settings;
    delete m_output_settingsraw;
    if (m_output_settings != m_output_settingsdigital)
    {
        delete m_output_settingsdigital;
        delete m_output_settingsdigitalraw;
    }

    if (m_kAudioSRCOutputSize > 0)
        delete[] m_src_out;

#ifndef NDEBUG
    assert(m_memory_corruption_test0 == 0xdeadbeef);
    assert(m_memory_corruption_test1 == 0xdeadbeef);
    assert(m_memory_corruption_test2 == 0xdeadbeef);
    assert(m_memory_corruption_test3 == 0xdeadbeef);
#else
    Q_UNUSED(m_memory_corruption_test0);
    Q_UNUSED(m_memory_corruption_test1);
    Q_UNUSED(m_memory_corruption_test2);
    Q_UNUSED(m_memory_corruption_test3);
#endif
}

void AudioOutputBase::InitSettings(const AudioSettings &settings)
{
    if (settings.m_custom)
    {
            // got a custom audio report already, use it
            // this was likely provided by the AudioTest utility
        m_output_settings = new AudioOutputSettings;
        *m_output_settings = *settings.m_custom;
        m_output_settingsdigital = m_output_settings;
        m_max_channels = m_output_settings->BestSupportedChannels();
        m_configured_channels = m_max_channels;
        return;
    }

    // Ask the subclass what we can send to the device
    m_output_settings = GetOutputSettingsUsers(false);
    m_output_settingsdigital = GetOutputSettingsUsers(true);

    m_max_channels = max(m_output_settings->BestSupportedChannels(),
                       m_output_settingsdigital->BestSupportedChannels());
    m_configured_channels = m_max_channels;

    m_upmix_default = m_max_channels > 2 ?
        gCoreContext->GetBoolSetting("AudioDefaultUpmix", false) :
        false;
    if (settings.m_upmixer == 1) // music, upmixer off
        m_upmix_default = false;
    else if (settings.m_upmixer == 2) // music, upmixer on
        m_upmix_default = true;
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
        if (m_output_settingsraw)
            return m_output_settingsraw;
    }
    else if (m_output_settingsdigitalraw)
        return m_output_settingsdigitalraw;

    AudioOutputSettings* aosettings = GetOutputSettings(digital);
    if (aosettings)
        aosettings->GetCleaned();
    else
        aosettings = new AudioOutputSettings(true);

    if (digital)
        return (m_output_settingsdigitalraw = aosettings);
    return (m_output_settingsraw = aosettings);
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
        if (m_output_settings)
            return m_output_settings;
    }
    else if (m_output_settingsdigital)
        return m_output_settingsdigital;

    AudioOutputSettings* aosettings = new AudioOutputSettings;

    *aosettings = *GetOutputSettingsCleaned(digital);
    aosettings->GetUsers();

    if (digital)
        return (m_output_settingsdigital = aosettings);
    return (m_output_settings = aosettings);
}

/**
 * Test if we can output digital audio and if sample rate is supported
 */
bool AudioOutputBase::CanPassthrough(int samplerate, int channels,
                                     AVCodecID codec, int profile) const
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
        default:
            arg = FEATURE_NONE;
            break;
    }
    // we can't passthrough any other codecs than those defined above
    ret &= m_output_settingsdigital->canFeature(arg);
    ret &= m_output_settingsdigital->IsSupportedFormat(FORMAT_S16);
    ret &= m_output_settingsdigital->IsSupportedRate(samplerate);
    // if we must resample to 48kHz ; we can't passthrough
    ret &= !((samplerate != 48000) &&
             gCoreContext->GetBoolSetting("Audio48kOverride", false));
    // Don't know any cards that support spdif clocked at < 44100
    // Some US cable transmissions have 2ch 32k AC-3 streams
    ret &= samplerate >= 44100;
    if (!ret)
        return false;
    // Will passthrough if surround audio was defined. Amplifier will
    // do the downmix if required
    bool willupmix = m_max_channels >= 6 && (channels <= 2 && m_upmix_default);
    ret &= !willupmix;
    // unless audio is configured for stereo. We can passthrough otherwise
    ret |= m_max_channels == 2;

    return ret;
}

/**
 * Set the bitrate of the source material, reported in periodic OutputEvents
 */
void AudioOutputBase::SetSourceBitrate(int rate)
{
    if (rate > 0)
        m_source_bitrate = rate;
}

/**
 * Set the timestretch factor
 *
 * You must hold the audio_buflock to call this safely
 */
void AudioOutputBase::SetStretchFactorLocked(float lstretchfactor)
{
    if (m_stretchfactor == lstretchfactor && m_pSoundStretch)
        return;

    m_stretchfactor = lstretchfactor;

    int channels = m_needs_upmix || m_needs_downmix ?
        m_configured_channels : m_source_channels;
    if (channels < 1 || channels > 8 || !m_configure_succeeded)
        return;

    bool willstretch = m_stretchfactor < 0.99F || m_stretchfactor > 1.01F;
    m_eff_stretchfactor = lroundf(100000.0F * lstretchfactor);

    if (m_pSoundStretch)
    {
        if (!willstretch && m_forcedprocessing)
        {
            m_forcedprocessing = false;
            m_processing = false;
            delete m_pSoundStretch;
            m_pSoundStretch = nullptr;
            VBGENERAL(QString("Cancelling time stretch"));
            m_bytes_per_frame = m_previousbpf;
            m_waud = m_raud = 0;
            m_reset_active.Ref();
        }
        else
        {
            VBGENERAL(QString("Changing time stretch to %1")
                      .arg(m_stretchfactor));
            m_pSoundStretch->setTempo(m_stretchfactor);
        }
    }
    else if (willstretch)
    {
        VBGENERAL(QString("Using time stretch %1").arg(m_stretchfactor));
        m_pSoundStretch = new soundtouch::SoundTouch();
        m_pSoundStretch->setSampleRate(m_samplerate);
        m_pSoundStretch->setChannels(channels);
        m_pSoundStretch->setTempo(m_stretchfactor);
#if ARCH_ARM || defined(Q_OS_ANDROID)
        // use less demanding settings for Raspberry pi
        m_pSoundStretch->setSetting(SETTING_SEQUENCE_MS, 82);
        m_pSoundStretch->setSetting(SETTING_USE_AA_FILTER, 0);
        m_pSoundStretch->setSetting(SETTING_USE_QUICKSEEK, 1);
#else
        m_pSoundStretch->setSetting(SETTING_SEQUENCE_MS, 35);
#endif
        /* If we weren't already processing we need to turn on float conversion
           adjust sample and frame sizes accordingly and dump the contents of
           the audiobuffer */
        if (!m_processing)
        {
            m_processing = true;
            m_forcedprocessing = true;
            m_previousbpf = m_bytes_per_frame;
            m_bytes_per_frame = m_source_channels *
                              AudioOutputSettings::SampleSize(FORMAT_FLT);
            m_audbuf_timecode = m_audiotime = m_frames_buffered = 0;
            m_waud = m_raud = 0;
            m_reset_active.Ref();
            m_was_paused = m_pauseaudio;
            m_pauseaudio = true;
            m_actually_paused = false;
            m_unpause_when_ready = true;
        }
    }
}

/**
 * Set the timestretch factor
 */
void AudioOutputBase::SetStretchFactor(float lstretchfactor)
{
    QMutexLocker lock(&m_audio_buflock);
    SetStretchFactorLocked(lstretchfactor);
}

/**
 * Get the timetretch factor
 */
float AudioOutputBase::GetStretchFactor(void) const
{
    return m_stretchfactor;
}

/**
 * Source is currently being upmixed
 */
bool AudioOutputBase::IsUpmixing(void)
{
    return m_needs_upmix && m_upmixer;
}

/**
 * Toggle between stereo and upmixed 5.1 if the source material is stereo
 */
bool AudioOutputBase::ToggleUpmix(void)
{
    // Can only upmix from mono/stereo to 6 ch
    if (m_max_channels == 2 || m_source_channels > 2)
        return false;

    m_upmix_default = !m_upmix_default;

    const AudioSettings settings(m_format, m_source_channels, m_codec,
                                 m_source_samplerate,
                                 m_upmix_default ? false : m_passthru);
    Reconfigure(settings);
    return IsUpmixing();
}

/**
 * Upmixing of the current source is available if requested
 */
bool AudioOutputBase::CanUpmix(void)
{
    return m_source_channels <= 2 && m_max_channels > 2;
}

/*
 * Setup samplerate and number of channels for passthrough
 * Create SPDIF encoder and true if successful
 */
bool AudioOutputBase::SetupPassthrough(AVCodecID codec, int codec_profile,
                                       int &samplerate_tmp, int &channels_tmp)
{
    if (codec == AV_CODEC_ID_DTS &&
        !m_output_settingsdigital->canFeature(FEATURE_DTSHD))
    {
        // We do not support DTS-HD bitstream so force extraction of the
        // DTS core track instead
        codec_profile = FF_PROFILE_DTS;
    }
    QString log = AudioOutputSettings::GetPassthroughParams(
        codec, codec_profile,
        samplerate_tmp, channels_tmp,
        m_output_settingsdigital->GetMaxHDRate() == 768000);
    VBAUDIO("Setting " + log + " passthrough");

    delete m_spdifenc;

    // No spdif encoder needed for certain devices
    if (m_usesSpdif)
        m_spdifenc = new SPDIFEncoder("spdif", codec);
    else
        m_spdifenc = nullptr;
    if (m_spdifenc && m_spdifenc->Succeeded() && codec == AV_CODEC_ID_DTS)
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

    if (m_spdifenc && !m_spdifenc->Succeeded())
    {
        delete m_spdifenc;
        m_spdifenc = nullptr;
        return false;
    }
    return true;
}

AudioOutputSettings *AudioOutputBase::OutputSettings(bool digital)
{
    if (digital)
        return m_output_settingsdigital;
    return m_output_settings;
}

/**
 * (Re)Configure AudioOutputBase
 *
 * Must be called from concrete subclasses
 */
void AudioOutputBase::Reconfigure(const AudioSettings &orig_settings)
{
    AudioSettings settings    = orig_settings;
    int  lsource_channels     = settings.m_channels;
    int  lconfigured_channels = m_configured_channels;
    bool lneeds_upmix         = false;
    bool lneeds_downmix       = false;
    bool lreenc               = false;
    bool lenc                 = false;

    if (!settings.m_use_passthru)
    {
        // Do we upmix stereo or mono?
        lconfigured_channels =
            (m_upmix_default && lsource_channels <= 2) ? 6 : lsource_channels;
        bool cando_channels =
            m_output_settings->IsSupportedChannels(lconfigured_channels);

        // check if the number of channels could be transmitted via AC3 encoding
#ifndef DISABLE_AC3_ENCODE
        lenc = m_output_settingsdigital->canFeature(FEATURE_AC3) &&
            (!m_output_settings->canFeature(FEATURE_LPCM) &&
             lconfigured_channels > 2 && lconfigured_channels <= 6);
#endif
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
                    lconfigured_channels = m_upmix_default ? 6 : 2;
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
        lconfigured_channels = min(lconfigured_channels, m_max_channels);
        /* Encode to AC-3 if we're allowed to passthru but aren't currently
           and we have more than 2 channels but multichannel PCM is not
           supported or if the device just doesn't support the number of
           channels */
#ifndef DISABLE_AC3_ENCODE
        lenc = m_output_settingsdigital->canFeature(FEATURE_AC3) &&
            ((!m_output_settings->canFeature(FEATURE_LPCM) &&
              lconfigured_channels > 2) ||
             !m_output_settings->IsSupportedChannels(lconfigured_channels));
        /* Might we reencode a bitstream that's been decoded for timestretch?
           If the device doesn't support the number of channels - see below */
        if (m_output_settingsdigital->canFeature(FEATURE_AC3) &&
            (settings.m_codec == AV_CODEC_ID_AC3 ||
             settings.m_codec == AV_CODEC_ID_DTS))
        {
            lreenc = true;
        }
#endif
        // Enough channels? Upmix if not, but only from mono/stereo/5.0 to 5.1
        if (IS_VALID_UPMIX_CHANNEL(settings.m_channels) &&
            settings.m_channels < lconfigured_channels)
        {
            VBAUDIO(QString("Needs upmix from %1 -> %2 channels")
                    .arg(settings.m_channels).arg(lconfigured_channels));
            settings.m_channels = lconfigured_channels;
            lneeds_upmix = true;
        }
        else if (settings.m_channels > lconfigured_channels)
        {
            VBAUDIO(QString("Needs downmix from %1 -> %2 channels")
                    .arg(settings.m_channels).arg(lconfigured_channels));
            settings.m_channels = lconfigured_channels;
            lneeds_downmix = true;
        }
    }

    ClearError();

    bool general_deps = true;

    /* Set samplerate_tmp and channels_tmp to appropriate values
       if passing through */
    int samplerate_tmp = 0, channels_tmp = 0;
    if (settings.m_use_passthru)
    {
        samplerate_tmp = settings.m_samplerate;
        SetupPassthrough(settings.m_codec, settings.m_codec_profile,
                         samplerate_tmp, channels_tmp);
        general_deps = m_samplerate == samplerate_tmp && m_channels == channels_tmp;
        general_deps &= m_format == m_output_format && m_format == FORMAT_S16;
    }
    else
    {
        general_deps =
            settings.m_format == m_format && lsource_channels == m_source_channels;
    }

    // Check if anything has changed
    general_deps &=
        settings.m_samplerate  == m_source_samplerate &&
        settings.m_use_passthru == m_passthru &&
        lconfigured_channels == m_configured_channels &&
        lneeds_upmix == m_needs_upmix && lreenc == m_reenc &&
        lneeds_downmix == m_needs_downmix;

    if (general_deps && m_configure_succeeded)
    {
        VBAUDIO("Reconfigure(): No change -> exiting");
        // if passthrough, source channels may have changed
        m_source_channels = lsource_channels;
        return;
    }

    KillAudio();

    QMutexLocker lock(&m_audio_buflock);
    QMutexLocker lockav(&m_avsync_lock);

    m_waud = m_raud = 0;
    m_reset_active.Clear();
    m_actually_paused = m_processing = m_forcedprocessing = false;

    m_channels               = settings.m_channels;
    m_source_channels        = lsource_channels;
    m_reenc                  = lreenc;
    m_codec                  = settings.m_codec;
    m_passthru               = settings.m_use_passthru;
    m_configured_channels    = lconfigured_channels;
    m_needs_upmix            = lneeds_upmix;
    m_needs_downmix          = lneeds_downmix;
    m_format                 = m_output_format   = settings.m_format;
    m_source_samplerate      = m_samplerate      = settings.m_samplerate;
    m_enc                    = lenc;

    m_killaudio = m_pauseaudio = false;
    m_was_paused = true;

    // Don't try to do anything if audio hasn't been
    // initialized yet (e.g. rubbish was provided)
    if (m_source_channels <= 0 || m_format <= 0 || m_samplerate <= 0)
    {
        SilentError(QString("Aborting Audio Reconfigure. ") +
                    QString("Invalid audio parameters ch %1 fmt %2 @ %3Hz")
                    .arg(m_source_channels).arg(m_format).arg(m_samplerate));
        return;
    }

    VBAUDIO(QString("Original codec was %1, %2, %3 kHz, %4 channels")
            .arg(ff_codec_id_string(m_codec))
            .arg(m_output_settings->FormatToString(m_format))
            .arg(m_samplerate/1000)
            .arg(m_source_channels));

    if (m_needs_downmix && m_source_channels > 8)
    {
        Error(QObject::tr("Aborting Audio Reconfigure. "
              "Can't handle audio with more than 8 channels."));
        return;
    }

    VBAUDIO(QString("enc(%1), passthru(%2), features (%3) "
                    "configured_channels(%4), %5 channels supported(%6) "
                    "max_channels(%7)")
            .arg(m_enc)
            .arg(m_passthru)
            .arg(m_output_settingsdigital->FeaturesToString())
            .arg(m_configured_channels)
            .arg(m_channels)
            .arg(OutputSettings(m_enc || m_passthru)->IsSupportedChannels(m_channels))
            .arg(m_max_channels));

    int dest_rate = 0;

    // Force resampling if we are encoding to AC3 and sr > 48k
    // or if 48k override was checked in settings
    if ((m_samplerate != 48000 &&
         gCoreContext->GetBoolSetting("Audio48kOverride", false)) ||
         (m_enc && (m_samplerate > 48000)))
    {
        VBAUDIO("Forcing resample to 48 kHz");
        if (m_src_quality < 0)
            m_src_quality = QUALITY_MEDIUM;
        m_need_resampler = true;
        dest_rate = 48000;
    }
        // this will always be false for passthrough audio as
        // CanPassthrough() already tested these conditions
    else if ((m_need_resampler =
              !OutputSettings(m_enc || m_passthru)->IsSupportedRate(m_samplerate)))
    {
        dest_rate = OutputSettings(m_enc)->NearestSupportedRate(m_samplerate);
    }

    if (m_need_resampler && m_src_quality > QUALITY_DISABLED)
    {
        m_samplerate = dest_rate;

        VBGENERAL(QString("Resampling from %1 kHz to %2 kHz with quality %3")
                .arg(settings.m_samplerate/1000).arg(m_samplerate/1000)
                .arg(quality_string(m_src_quality)));

        int chans = m_needs_downmix ? m_configured_channels : m_source_channels;

        int error = 0;
        m_src_ctx = src_new(2-m_src_quality, chans, &error);
        if (error)
        {
            Error(QObject::tr("Error creating resampler: %1")
                  .arg(src_strerror(error)));
            m_src_ctx = nullptr;
            return;
        }

        m_src_data.src_ratio = (double)m_samplerate / settings.m_samplerate;
        m_src_data.data_in   = m_src_in;
        int newsize        = (int)(kAudioSRCInputSize * m_src_data.src_ratio + 15)
                             & ~0xf;

        if (m_kAudioSRCOutputSize < newsize)
        {
            m_kAudioSRCOutputSize = newsize;
            VBAUDIO(QString("Resampler allocating %1").arg(newsize));
            delete[] m_src_out;
            m_src_out = new float[m_kAudioSRCOutputSize];
        }
        m_src_data.data_out       = m_src_out;
        m_src_data.output_frames  = m_kAudioSRCOutputSize / chans;
        m_src_data.end_of_input = 0;
    }

    if (m_enc)
    {
        if (m_reenc)
            VBAUDIO("Reencoding decoded AC-3/DTS to AC-3");

        VBAUDIO(QString("Creating AC-3 Encoder with sr = %1, ch = %2")
                .arg(m_samplerate).arg(m_configured_channels));

        m_encoder = new AudioOutputDigitalEncoder();
        if (!m_encoder->Init(AV_CODEC_ID_AC3, 448000, m_samplerate,
                           m_configured_channels))
        {
            Error(QObject::tr("AC-3 encoder initialization failed"));
            delete m_encoder;
            m_encoder = nullptr;
            m_enc = false;
            // upmixing will fail if we needed the encoder
            m_needs_upmix = false;
        }
    }

    if (m_passthru)
    {
        //AC3, DTS, DTS-HD MA and TrueHD all use 16 bits samples
        m_channels = channels_tmp;
        m_samplerate = samplerate_tmp;
        m_format = m_output_format = FORMAT_S16;
        m_source_bytes_per_frame = m_channels *
            AudioOutputSettings::SampleSize(m_format);
    }
    else
    {
        m_source_bytes_per_frame = m_source_channels *
            AudioOutputSettings::SampleSize(m_format);
    }

    // Turn on float conversion?
    if (m_need_resampler || m_needs_upmix || m_needs_downmix ||
        m_stretchfactor != 1.0F || (internal_vol && SWVolume()) ||
        (m_enc && m_output_format != FORMAT_S16) ||
        !OutputSettings(m_enc || m_passthru)->IsSupportedFormat(m_output_format))
    {
        VBAUDIO("Audio processing enabled");
        m_processing  = true;
        if (m_enc)
            m_output_format = FORMAT_S16;  // Output s16le for AC-3 encoder
        else
            m_output_format = m_output_settings->BestSupportedFormat();
    }

    m_bytes_per_frame =  m_processing ?
        sizeof(float) : AudioOutputSettings::SampleSize(m_format);
    m_bytes_per_frame *= m_channels;

    if (m_enc)
        m_channels = 2; // But only post-encoder

    m_output_bytes_per_frame = m_channels *
                             AudioOutputSettings::SampleSize(m_output_format);

    VBGENERAL(
        QString("Opening audio device '%1' ch %2(%3) sr %4 sf %5 reenc %6")
        .arg(m_main_device).arg(m_channels).arg(m_source_channels).arg(m_samplerate)
        .arg(m_output_settings->FormatToString(m_output_format)).arg(m_reenc));

    m_audbuf_timecode = m_audiotime = m_frames_buffered = 0;
    m_current_seconds = m_source_bitrate = -1;
    m_effdsp = m_samplerate * 100;

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

    VBAUDIO(QString("Audio fragment size: %1").arg(m_fragment_size));

    // Only used for software volume
    if (m_set_initial_vol && internal_vol && SWVolume())
    {
        VBAUDIO("Software volume enabled");
        m_volumeControl  = gCoreContext->GetSetting("MixerControl", "PCM");
        m_volumeControl += "MixerVolume";
        m_volume = gCoreContext->GetNumSetting(m_volumeControl, 80);
    }

    VolumeBase::SetChannels(m_configured_channels);
    VolumeBase::SyncVolume();
    VolumeBase::UpdateVolume();

    if (m_needs_upmix && m_configured_channels > 2)
    {
        m_surround_mode = gCoreContext->GetNumSetting("AudioUpmixType", QUALITY_HIGH);
        m_upmixer = new FreeSurround(m_samplerate, source == AUDIOOUTPUT_VIDEO,
                                   (FreeSurround::SurroundMode)m_surround_mode);
        VBAUDIO(QString("Create %1 quality upmixer done")
                .arg(quality_string(m_surround_mode)));
    }

    VBAUDIO(QString("Audio Stretch Factor: %1").arg(m_stretchfactor));
    SetStretchFactorLocked(m_old_stretchfactor);

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();

    if (m_unpause_when_ready)
        m_pauseaudio = m_actually_paused = true;

    m_configure_succeeded = true;

    StartOutputThread();

    VBAUDIO("Ending Reconfigure()");
}

bool AudioOutputBase::StartOutputThread(void)
{
    if (m_audio_thread_exists)
        return true;

    start();
    m_audio_thread_exists = true;

    return true;
}


void AudioOutputBase::StopOutputThread(void)
{
    if (m_audio_thread_exists)
    {
        wait();
        m_audio_thread_exists = false;
    }
}

/**
 * Kill the output thread and cleanup
 */
void AudioOutputBase::KillAudio()
{
    m_killAudioLock.lock();

    VBAUDIO("Killing AudioOutputDSP");
    m_killaudio = true;
    StopOutputThread();
    QMutexLocker lock(&m_audio_buflock);

    if (m_pSoundStretch)
    {
        delete m_pSoundStretch;
        m_pSoundStretch = nullptr;
        m_old_stretchfactor = m_stretchfactor;
        m_stretchfactor = 1.0F;
    }

    if (m_encoder)
    {
        delete m_encoder;
        m_encoder = nullptr;
    }

    if (m_upmixer)
    {
        delete m_upmixer;
        m_upmixer = nullptr;
    }

    if (m_src_ctx)
    {
        src_delete(m_src_ctx);
        m_src_ctx = nullptr;
    }

    m_needs_upmix = m_need_resampler = m_enc = false;

    CloseDevice();

    m_killAudioLock.unlock();
}

void AudioOutputBase::Pause(bool paused)
{
    if (!paused && m_unpause_when_ready)
        return;
    VBAUDIO(QString("Pause %1").arg(paused));
    if (m_pauseaudio != paused)
        m_was_paused = m_pauseaudio;
    m_pauseaudio = paused;
    m_unpause_when_ready = false;
    m_actually_paused = false;
}

void AudioOutputBase::PauseUntilBuffered()
{
    Reset();
    Pause(true);
    m_unpause_when_ready = true;
}

/**
 * Reset the audiobuffer, timecode and mythmusic visualisation
 */
void AudioOutputBase::Reset()
{
    QMutexLocker lock(&m_audio_buflock);
    QMutexLocker lockav(&m_avsync_lock);

    m_audbuf_timecode = m_audiotime = m_frames_buffered = 0;
    if (m_encoder)
    {
        m_waud = m_raud = 0;    // empty ring buffer
        memset(m_audiobuffer, 0, kAudioRingBufferSize);
    }
    else
    {
        m_waud = m_raud;        // empty ring buffer
    }
    m_reset_active.Ref();
    m_current_seconds = -1;
    m_was_paused = !m_pauseaudio;
    // clear any state that could remember previous audio in any active filters
    if (m_needs_upmix && m_upmixer)
        m_upmixer->flush();
    if (m_pSoundStretch)
        m_pSoundStretch->clear();
    if (m_encoder)
        m_encoder->clear();

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
    m_audbuf_timecode = m_audiotime = timecode;
    m_frames_buffered = (timecode * m_source_samplerate) / 1000;
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
    m_effdsp = dsprate;
}

/**
 * Get the number of bytes in the audiobuffer
 */
inline int AudioOutputBase::audiolen() const
{
    if (m_waud >= m_raud)
        return m_waud - m_raud;
    return kAudioRingBufferSize - (m_raud - m_waud);
}

/**
 * Get the free space in the audiobuffer in bytes
 */
int AudioOutputBase::audiofree() const
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
int AudioOutputBase::audioready() const
{
    if (m_passthru || m_enc || m_bytes_per_frame == m_output_bytes_per_frame)
        return audiolen();
    return audiolen() * m_output_bytes_per_frame / m_bytes_per_frame;
}

/**
 * Calculate the timecode of the samples that are about to become audible
 */
int64_t AudioOutputBase::GetAudiotime(void)
{
    if (m_audbuf_timecode == 0 || !m_configure_succeeded)
        return 0;

    // output bits per 10 frames
    int64_t obpf = 0;

    if (m_passthru && !usesSpdif())
        obpf = m_source_bitrate * 10 / m_source_samplerate;
    else if (m_enc && !usesSpdif())
    {
        // re-encode bitrate is hardcoded at 448000
        obpf = 448000 * 10 / m_source_samplerate;
    }
    else
        obpf = m_output_bytes_per_frame * 80;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       Which is leaving the sound card at this instant.

       We use these variables:

       'effdsp' is 100 * frames/sec

       'audbuf_timecode' is the timecode in milliseconds of the
       audio that has just been written into the buffer.

       'eff_stretchfactor' is stretch factor * 100,000

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer. */


    QMutexLocker lockav(&m_avsync_lock);

    int64_t soundcard_buffer = GetBufferedOnSoundcard(); // bytes

    /* audioready tells us how many bytes are in audiobuffer
       scaled appropriately if output format != internal format */
    int64_t main_buffer = audioready();

    int64_t oldaudiotime = m_audiotime;

    /* timecode is the stretch adjusted version
       of major post-stretched buffer contents
       processing latencies are catered for in AddData/SetAudiotime
       to eliminate race */

    m_audiotime = m_audbuf_timecode - (m_effdsp && obpf ?
        ((main_buffer + soundcard_buffer) * int64_t(m_eff_stretchfactor)
        * 80 / int64_t(m_effdsp) / obpf) : 0);

    /* audiotime should never go backwards, but we might get a negative
       value if GetBufferedOnSoundcard() isn't updated by the driver very
       quickly (e.g. ALSA) */
    if (m_audiotime < oldaudiotime)
        m_audiotime = oldaudiotime;

    VBAUDIOTS(QString("GetAudiotime audt=%1 abtc=%2 mb=%3 sb=%4 tb=%5 "
                      "sr=%6 obpf=%7 bpf=%8 esf=%9 edsp=%10 sbr=%11")
              .arg(m_audiotime).arg(m_audbuf_timecode)  // 1, 2
              .arg(main_buffer)                     // 3
              .arg(soundcard_buffer)                // 4
              .arg(main_buffer+soundcard_buffer)    // 5
              .arg(m_samplerate).arg(obpf)            // 6, 7
              .arg(m_bytes_per_frame)                 // 8
              .arg(m_eff_stretchfactor)               // 9
              .arg(m_effdsp).arg(m_source_bitrate)      // 10, 11
              );

    return m_audiotime;
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
    int64_t old_audbuf_timecode       = m_audbuf_timecode;

    if (!m_configure_succeeded)
        return;

    if (m_needs_upmix && m_upmixer)
        processframes_unstretched -= m_upmixer->frameLatency();

    if (m_pSoundStretch)
    {
        processframes_unstretched -= m_pSoundStretch->numUnprocessedSamples();
        processframes_stretched   -= m_pSoundStretch->numSamples();
    }

    if (m_encoder)
    {
        processframes_stretched -= m_encoder->Buffered();
    }

    m_audbuf_timecode =
        timecode + (m_effdsp ? ((frames + processframes_unstretched) * 100000 +
                    (processframes_stretched * m_eff_stretchfactor)
                   ) / m_effdsp : 0);

    // check for timecode wrap and reset audiotime if detected
    // timecode will always be monotonic asc if not seeked and reset
    // happens if seek or pause happens
    if (m_audbuf_timecode < old_audbuf_timecode)
        m_audiotime = 0;

    VBAUDIOTS(QString("SetAudiotime atc=%1 tc=%2 f=%3 pfu=%4 pfs=%5")
              .arg(m_audbuf_timecode)
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
    int64_t ret = m_audbuf_timecode - GetAudiotime();
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
    m_volume = new_volume;
    if (save && m_volumeControl != nullptr)
        gCoreContext->SaveSetting(m_volumeControl, m_volume);
}

/**
 * Get the volume for software volume control
 */
int AudioOutputBase::GetSWVolume()
{
    return m_volume;
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
    int bpf   = m_bytes_per_frame;
    int len   = frames * bpf;
    int afree = audiofree();

    if (len <= afree)
        return len;

    VBERROR(QString("Audio buffer overflow, %1 frames lost!")
            .arg(frames - (afree / bpf)));

    frames = afree / bpf;
    len = frames * bpf;

    if (!m_src_ctx)
        return len;

    int error = src_reset(m_src_ctx);
    if (error)
    {
        VBERROR(QString("Error occurred while resetting resampler: %1")
                .arg(src_strerror(error)));
        m_src_ctx = nullptr;
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
    int bpf   = m_bytes_per_frame;
    int off   = 0;

    if (!m_needs_upmix)
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
    if (!m_upmixer)
    {
        // we're always in the case
        // m_configured_channels == 2 && m_source_channels == 1
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
    off =  m_processing ? sizeof(float) : AudioOutputSettings::SampleSize(m_format);
    off *= m_source_channels;

    int i = 0;
    len = 0;
    while (i < frames)
    {
        i += m_upmixer->putFrames(buffer + i * off, frames - i, m_source_channels);
        int nFrames = m_upmixer->numFrames();
        if (!nFrames)
            continue;

        len += CheckFreeSpace(nFrames);

        int bdFrames = (kAudioRingBufferSize - org_waud) / bpf;
        if (bdFrames < nFrames)
        {
            if ((org_waud % bpf) != 0)
            {
                VBERROR(QString("Upmixing: org_waud = %1 (bpf = %2)")
                        .arg(org_waud)
                        .arg(bpf));
            }
            m_upmixer->receiveFrames((float *)(WPOS), bdFrames);
            nFrames -= bdFrames;
            org_waud = 0;
        }
        if (nFrames > 0)
            m_upmixer->receiveFrames((float *)(WPOS), nFrames);

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
    return AddData(in_buffer, in_frames * m_source_bytes_per_frame, timecode,
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
    int frames   = in_len / m_source_bytes_per_frame;
    int bpf      = m_bytes_per_frame;
    int len      = in_len;
    bool music   = false;

    if (!m_configure_succeeded)
    {
        LOG(VB_GENERAL, LOG_ERR, "AddData called with audio framework not "
                                 "initialised");
        m_length_last_data = 0;
        return false;
    }

    /* See if we're waiting for new samples to be buffered before we unpause
       post channel change, seek, etc. Wait for 4 fragments to be buffered */
    if (m_unpause_when_ready && m_pauseaudio && audioready() > m_fragment_size << 2)
    {
        m_unpause_when_ready = false;
        Pause(false);
    }

    // Don't write new samples if we're resetting the buffer or reconfiguring
    QMutexLocker lock(&m_audio_buflock);

    uint org_waud = m_waud;
    int  afree    = audiofree();
    int  used     = kAudioRingBufferSize - afree;

    if (m_passthru && m_spdifenc)
    {
        if (m_processing)
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
            in_buffer = m_spdifenc->GetProcessedBuffer();
            m_spdifenc->Reset();
            frames = len / m_source_bytes_per_frame;
        }
        else
            frames = 0;
    }
    m_length_last_data = (int64_t)
        ((double)(len * 1000) / (m_source_samplerate * m_source_bytes_per_frame));

    VBAUDIOTS(QString("AddData frames=%1, bytes=%2, used=%3, free=%4, "
                      "timecode=%5 needsupmix=%6")
              .arg(frames).arg(len).arg(used).arg(afree).arg(timecode)
              .arg(m_needs_upmix));

    // Mythmusic doesn't give us timestamps
    if (timecode < 0)
    {
        timecode = (m_frames_buffered * 1000) / m_source_samplerate;
        m_frames_buffered += frames;
        music = true;
    }

    if (hasVisual())
    {
        // Send original samples to any attached visualisations
        dispatchVisual((uchar *)in_buffer, len, timecode, m_source_channels,
                       AudioOutputSettings::FormatToBits(m_format));
    }

    // Calculate amount of free space required in ringbuffer
    if (m_processing)
    {
        int sampleSize = AudioOutputSettings::SampleSize(m_format);
        if (sampleSize <= 0)
        {
            // Would lead to division by zero (or unexpected results if negative)
            VBERROR("Sample size is <= 0, AddData returning false");
            return false;
        }

        // Final float conversion space requirement
        len = sizeof(*m_src_in_buf) / sampleSize * len;

        // Account for changes in number of channels
        if (m_needs_downmix)
            len = (len * m_configured_channels ) / m_source_channels;

        // Check we have enough space to write the data
        if (m_need_resampler && m_src_ctx)
            len = lround(ceil(static_cast<double>(len) * m_src_data.src_ratio));

        if (m_needs_upmix)
            len = (len * m_configured_channels ) / m_source_channels;

        // Include samples in upmix buffer that may be flushed
        if (m_needs_upmix && m_upmixer)
            len += m_upmixer->numUnprocessedFrames() * bpf;

        // Include samples in soundstretch buffers
        if (m_pSoundStretch)
            len += (m_pSoundStretch->numUnprocessedSamples() +
                    (int)(m_pSoundStretch->numSamples() / m_stretchfactor)) * bpf;
    }

    if (len > afree)
    {
        VBERROR("Buffer is full, AddData returning false");
        return false; // would overflow
    }

    int frames_remaining = frames;
    int frames_final = 0;
    int maxframes = (kAudioSRCInputSize / m_source_channels) & ~0xf;
    int offset = 0;

    while(frames_remaining > 0)
    {
        void *buffer = (char *)in_buffer + offset;
        frames = frames_remaining;
        len = frames * m_source_bytes_per_frame;

        if (m_processing)
        {
            if (frames > maxframes)
            {
                frames = maxframes;
                len = frames * m_source_bytes_per_frame;
                offset += len;
            }
            // Convert to floats
            AudioOutputUtil::toFloat(m_format, m_src_in, buffer, len);
        }

        frames_remaining -= frames;

        // Perform downmix if necessary
        if (m_needs_downmix)
            if(AudioOutputDownmix::DownmixFrames(m_source_channels,
                                                 m_configured_channels,
                                                 m_src_in, m_src_in, frames) < 0)
                VBERROR("Error occurred while downmixing");

        // Resample if necessary
        if (m_need_resampler && m_src_ctx)
        {
            m_src_data.input_frames = frames;
            int error = src_process(m_src_ctx, &m_src_data);

            if (error)
                VBERROR(QString("Error occurred while resampling audio: %1")
                        .arg(src_strerror(error)));

            buffer = m_src_out;
            frames = m_src_data.output_frames_gen;
        }
        else if (m_processing)
            buffer = m_src_in;

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

        int bdiff = kAudioRingBufferSize - m_waud;
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

        if (m_pSoundStretch)
        {
            // does not change the timecode, only the number of samples
            org_waud     = m_waud;
            int bdFrames = bdiff / bpf;

            if (bdiff < len)
            {
                m_pSoundStretch->putSamples((STST *)(WPOS), bdFrames);
                m_pSoundStretch->putSamples((STST *)ABUF, (len - bdiff) / bpf);
            }
            else
                m_pSoundStretch->putSamples((STST *)(WPOS), frames);

            int nFrames = m_pSoundStretch->numSamples();
            if (nFrames > frames)
                CheckFreeSpace(nFrames);

            len = nFrames * bpf;

            if (nFrames > bdFrames)
            {
                nFrames -= m_pSoundStretch->receiveSamples((STST *)(WPOS),
                                                         bdFrames);
                org_waud = 0;
            }
            if (nFrames > 0)
                nFrames = m_pSoundStretch->receiveSamples((STST *)(WPOS),
                                                        nFrames);

            org_waud = (org_waud + nFrames * bpf) % kAudioRingBufferSize;
        }

        if (internal_vol && SWVolume())
        {
            org_waud    = m_waud;
            int num     = len;

            if (bdiff <= num)
            {
                AudioOutputUtil::AdjustVolume(WPOS, bdiff, m_volume,
                                              music, m_needs_upmix && m_upmixer);
                num -= bdiff;
                org_waud = 0;
            }
            if (num > 0)
                AudioOutputUtil::AdjustVolume(WPOS, num, m_volume,
                                              music, m_needs_upmix && m_upmixer);
            org_waud = (org_waud + num) % kAudioRingBufferSize;
        }

        if (m_encoder)
        {
            org_waud            = m_waud;
            int to_get          = 0;

            if (bdiff < len)
            {
                m_encoder->Encode(WPOS, bdiff, m_processing ? FORMAT_FLT : m_format);
                to_get = m_encoder->Encode(ABUF, len - bdiff,
                                         m_processing ? FORMAT_FLT : m_format);
            }
            else
            {
                to_get = m_encoder->Encode(WPOS, len,
                                         m_processing ? FORMAT_FLT : m_format);
            }

            if (bdiff <= to_get)
            {
                m_encoder->GetFrames(WPOS, bdiff);
                to_get -= bdiff ;
                org_waud = 0;
            }
            if (to_get > 0)
                m_encoder->GetFrames(WPOS, to_get);

            org_waud = (org_waud + to_get) % kAudioRingBufferSize;
        }

        m_waud = org_waud;
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

    if (m_source_bitrate == -1)
        m_source_bitrate = m_source_samplerate * m_source_channels *
                         AudioOutputSettings::FormatToBits(m_format);

    if (ct / 1000 != m_current_seconds)
    {
        m_current_seconds = ct / 1000;
        OutputEvent e(m_current_seconds, ct, m_source_bitrate, m_source_samplerate,
                      AudioOutputSettings::FormatToBits(m_format), m_source_channels);
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
    uchar *zeros        = new uchar[m_fragment_size];
    uchar *fragment_buf = new uchar[m_fragment_size + 16];
    uchar *fragment     = (uchar *)AOALIGN(fragment_buf[0]);
    memset(zeros, 0, m_fragment_size);

    // to reduce startup latency, write silence in 8ms chunks
    int zero_fragment_size = 8 * m_samplerate * m_output_bytes_per_frame / 1000;
    if (zero_fragment_size > m_fragment_size)
        zero_fragment_size = m_fragment_size;

    while (!m_killaudio)
    {
        if (m_pauseaudio)
        {
            if (!m_actually_paused)
            {
                VBAUDIO("OutputAudioLoop: audio paused");
                OutputEvent e(OutputEvent::Paused);
                dispatch(e);
                m_was_paused = true;
            }

            m_actually_paused = true;
            m_audiotime = 0; // mark 'audiotime' as invalid.

            WriteAudio(zeros, zero_fragment_size);
            continue;
        }

        if (m_was_paused)
        {
            VBAUDIO("OutputAudioLoop: Play Event");
            OutputEvent e(OutputEvent::Playing);
            dispatch(e);
            m_was_paused = false;
        }

        /* do audio output */
        int ready = audioready();

        // wait for the buffer to fill with enough to play
        if (m_fragment_size > ready)
        {
            if (ready > 0)  // only log if we're sending some audio
                VBAUDIOTS(QString("audio waiting for buffer to fill: "
                                  "have %1 want %2")
                          .arg(ready).arg(m_fragment_size));

            usleep(10000);
            continue;
        }

#ifdef AUDIOTSTESTING
        VBAUDIOTS("WriteAudio Start");
#endif
        Status();

        // delay setting raud until after phys buffer is filled
        // so GetAudiotime will be accurate without locking
        m_reset_active.TestAndDeref();
        volatile uint next_raud = m_raud;
        if (GetAudioData(fragment, m_fragment_size, true, &next_raud))
        {
            if (!m_reset_active.TestAndDeref())
            {
                WriteAudio(fragment, m_fragment_size);
                if (!m_reset_active.TestAndDeref())
                    m_raud = next_raud;
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

#define LRPOS (m_audiobuffer + *local_raud)
    // re-check audioready() in case things changed.
    // for example, ClearAfterSeek() might have run
    int avail_size   = audioready();
    int frag_size    = size;
    int written_size = size;

    if (local_raud == nullptr)
        local_raud = &m_raud;

    if (!full_buffer && (size > avail_size))
    {
        // when full_buffer is false, return any available data
        frag_size = avail_size;
        written_size = frag_size;
    }

    if (!avail_size || (frag_size > avail_size))
        return 0;

    int bdiff = kAudioRingBufferSize - m_raud;

    int obytes = AudioOutputSettings::SampleSize(m_output_format);

    if (obytes <= 0)
        return 0;

    bool fromFloats = m_processing && !m_enc && m_output_format != FORMAT_FLT;

    // Scale if necessary
    if (fromFloats && obytes != sizeof(float))
        frag_size *= sizeof(float) / obytes;

    int off = 0;

    if (bdiff <= frag_size)
    {
        if (fromFloats)
            off = AudioOutputUtil::fromFloat(m_output_format, buffer,
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
            AudioOutputUtil::fromFloat(m_output_format, buffer + off,
                                       LRPOS, frag_size);
        else
            memcpy(buffer + off, LRPOS, frag_size);
    }

    *local_raud += frag_size;

    // Mute individual channels through mono->stereo duplication
    MuteState mute_state = GetMuteState();
    if (!m_enc && !m_passthru &&
        written_size && m_configured_channels > 1 &&
        (mute_state == kMuteLeft || mute_state == kMuteRight))
    {
        AudioOutputUtil::MuteChannel(obytes << 3, m_configured_channels,
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
    while (!m_pauseaudio && audioready() > m_fragment_size)
        usleep(1000);
    if (m_pauseaudio)
    {
        // Audio is paused and can't be drained, clear ringbuffer
        QMutexLocker lock(&m_audio_buflock);

        m_waud = m_raud = 0;
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

int AudioOutputBase::readOutputData(unsigned char* /*read_buffer*/, int /*max_length*/)
{
    VBERROR("AudioOutputBase should not be getting asked to readOutputData()");
    return 0;
}
