// C++ headers
#include <algorithm>
#include <cmath>
#include <limits>

// POSIX headers
#include <unistd.h>
#include <sys/time.h>

// SoundTouch
#if __has_include(<soundtouch/SoundTouch.h>)
#include <soundtouch/SoundTouch.h>
#else
#include <SoundTouch.h>
#endif

// Qt headers
#include <QtGlobal>
#include <QMutexLocker>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "audiooutputbase.h"
#include "audiooutputdigitalencoder.h"
#include "audiooutputdownmix.h"
#include "audiooutpututil.h"
#include "freesurround.h"
#include "spdifencoder.h"

// AC3 encode currently disabled for Android
#if defined(Q_OS_ANDROID)
#define DISABLE_AC3_ENCODE
#endif

#define LOC QString("AOBase: ")

// Replacing "m_audioBuffer + org_waud" with
// "&m_audioBuffer[org_waud]" should provide bounds
// checking with c++17 arrays.
#define WPOS (&m_audioBuffer[org_waud])
#define RPOS (&m_audioBuffer[m_raud])
#define ABUF (m_audioBuffer.data())
#define STST soundtouch::SAMPLETYPE

// 1,2,5 and 7 channels are currently valid for upmixing if required
static constexpr int UPMIX_CHANNEL_MASK { (1<<1)|(1<<2)|(1<<5)|(1<<7) };
static constexpr bool IS_VALID_UPMIX_CHANNEL(int ch)
{ return ((1 << ch) & UPMIX_CHANNEL_MASK) != 0; }

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
    m_mainDevice(settings.GetMainDevice()),
    m_passthruDevice(settings.GetPassthruDevice()),
    m_source(settings.m_source),
    m_setInitialVol(settings.m_setInitialVol)
{
    m_srcIn = m_srcInBuf.data();

    if (m_mainDevice.startsWith("AudioTrack:"))
        m_usesSpdif = false;
    // Handle override of SRC quality settings
    if (gCoreContext->GetBoolSetting("SRCQualityOverride", false))
    {
        m_srcQuality = gCoreContext->GetNumSetting("SRCQuality", QUALITY_MEDIUM);
        // Extra test to keep backward compatibility with earlier SRC setting
        m_srcQuality = std::min<int>(m_srcQuality, QUALITY_HIGH);

        LOG(VB_AUDIO, LOG_INFO, LOC + QString("SRC quality = %1").arg(quality_string(m_srcQuality)));
    }
}

/**
 * Destructor
 *
 * You must kill the output thread via KillAudio() prior to destruction
 */
AudioOutputBase::~AudioOutputBase()
{
    if (!m_killAudio)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Programmer Error: "
                "~AudioOutputBase called, but KillAudio has not been called!");

    // We got this from a subclass, delete it
    delete m_outputSettings;
    delete m_outputSettingsRaw;
    if (m_outputSettings != m_outputSettingsDigital)
    {
        delete m_outputSettingsDigital;
        delete m_outputSettingsDigitalRaw;
    }

    if (m_kAudioSRCOutputSize > 0)
        delete[] m_srcOut;

#ifndef NDEBUG
    assert(m_memoryCorruptionTest0 == 0xdeadbeef);
    assert(m_memoryCorruptionTest1 == 0xdeadbeef);
    assert(m_memoryCorruptionTest2 == 0xdeadbeef);
    assert(m_memoryCorruptionTest3 == 0xdeadbeef);
#endif
}

void AudioOutputBase::InitSettings(const AudioSettings &settings)
{
    if (settings.m_custom)
    {
            // got a custom audio report already, use it
            // this was likely provided by the AudioTest utility
        m_outputSettings = new AudioOutputSettings;
        *m_outputSettings = *settings.m_custom;
        m_outputSettingsDigital = m_outputSettings;
        m_maxChannels = m_outputSettings->BestSupportedChannels();
        m_configuredChannels = m_maxChannels;
        return;
    }

    // Ask the subclass what we can send to the device
    m_outputSettings = GetOutputSettingsUsers(false);
    m_outputSettingsDigital = GetOutputSettingsUsers(true);

    m_maxChannels = std::max(m_outputSettings->BestSupportedChannels(),
                       m_outputSettingsDigital->BestSupportedChannels());
    m_configuredChannels = m_maxChannels;

    m_upmixDefault = m_maxChannels > 2 ?
        gCoreContext->GetBoolSetting("AudioDefaultUpmix", false) :
        false;
    if (settings.m_upmixer == 1) // music, upmixer off
        m_upmixDefault = false;
    else if (settings.m_upmixer == 2) // music, upmixer on
        m_upmixDefault = true;
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
    if (!m_discreteDigital || !digital)
    {
        digital = false;
        if (m_outputSettingsRaw)
            return m_outputSettingsRaw;
    }
    else if (m_outputSettingsDigitalRaw)
    {
        return m_outputSettingsDigitalRaw;
    }

    AudioOutputSettings* aosettings = GetOutputSettings(digital);
    if (aosettings)
        aosettings->GetCleaned();
    else
        aosettings = new AudioOutputSettings(true);

    if (digital)
        return (m_outputSettingsDigitalRaw = aosettings);
    return (m_outputSettingsRaw = aosettings);
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3, DTS, E-AC3 and TrueHD) as well as the user settings
 */
AudioOutputSettings* AudioOutputBase::GetOutputSettingsUsers(bool digital)
{
    if (!m_discreteDigital || !digital)
    {
        digital = false;
        if (m_outputSettings)
            return m_outputSettings;
    }
    else if (m_outputSettingsDigital)
    {
        return m_outputSettingsDigital;
    }

    auto* aosettings = new AudioOutputSettings;

    *aosettings = *GetOutputSettingsCleaned(digital);
    aosettings->GetUsers();

    if (digital)
        return (m_outputSettingsDigital = aosettings);
    return (m_outputSettings = aosettings);
}

/**
 * Test if we can output digital audio and if sample rate is supported
 */
bool AudioOutputBase::CanPassthrough(int samplerate, int channels,
                                     AVCodecID codec, int profile) const
{
    DigitalFeature arg = FEATURE_NONE;
    bool           ret = !(m_internalVol && SWVolume());

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
    ret &= m_outputSettingsDigital->canFeature(arg);
    ret &= m_outputSettingsDigital->IsSupportedFormat(FORMAT_S16);
    ret &= m_outputSettingsDigital->IsSupportedRate(samplerate);
    // if we must resample to 48kHz ; we can't passthrough
    ret &= (samplerate == 48000) ||
             !gCoreContext->GetBoolSetting("Audio48kOverride", false);
    // Don't know any cards that support spdif clocked at < 44100
    // Some US cable transmissions have 2ch 32k AC-3 streams
    ret &= samplerate >= 44100;
    if (!ret)
        return false;
    // Will passthrough if surround audio was defined. Amplifier will
    // do the downmix if required
    bool willupmix = m_maxChannels >= 6 && (channels <= 2 && m_upmixDefault);
    ret &= !willupmix;
    // unless audio is configured for stereo. We can passthrough otherwise
    ret |= m_maxChannels == 2;

    return ret;
}

/**
 * Set the bitrate of the source material, reported in periodic OutputEvents
 */
void AudioOutputBase::SetSourceBitrate(int rate)
{
    if (rate > 0)
        m_sourceBitRate = rate;
}

/**
 * Set the timestretch factor
 *
 * You must hold the audio_buflock to call this safely
 */
void AudioOutputBase::SetStretchFactorLocked(float lstretchfactor)
{
    if (m_stretchFactor == lstretchfactor && m_pSoundStretch)
        return;

    m_stretchFactor = lstretchfactor;

    int channels = m_needsUpmix || m_needsDownmix ?
        m_configuredChannels : m_sourceChannels;
    if (channels < 1 || channels > 8 || !m_configureSucceeded)
        return;

    bool willstretch = m_stretchFactor < 0.99F || m_stretchFactor > 1.01F;
    m_effStretchFactor = lroundf(100000.0F * lstretchfactor);

    if (m_pSoundStretch)
    {
        if (!willstretch && m_forcedProcessing)
        {
            m_forcedProcessing = false;
            m_processing = false;
            delete m_pSoundStretch;
            m_pSoundStretch = nullptr;
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Cancelling time stretch"));
            m_bytesPerFrame = m_previousBpf;
            m_waud = m_raud = 0;
            m_resetActive.Ref();
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Changing time stretch to %1")
                      .arg(m_stretchFactor));
            m_pSoundStretch->setTempo(m_stretchFactor);
        }
    }
    else if (willstretch)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using time stretch %1").arg(m_stretchFactor));
        m_pSoundStretch = new soundtouch::SoundTouch();
        m_pSoundStretch->setSampleRate(m_sampleRate);
        m_pSoundStretch->setChannels(channels);
        m_pSoundStretch->setTempo(m_stretchFactor);
#if defined(Q_PROCESSOR_ARM) || defined(Q_OS_ANDROID)
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
            m_forcedProcessing = true;
            m_previousBpf = m_bytesPerFrame;
            m_bytesPerFrame = m_sourceChannels *
                              AudioOutputSettings::SampleSize(FORMAT_FLT);
            m_audbufTimecode = m_audioTime = 0ms;
            m_framesBuffered = 0;
            m_waud = m_raud = 0;
            m_resetActive.Ref();
            m_wasPaused = m_pauseAudio;
            m_pauseAudio = true;
            m_actuallyPaused = false;
            m_unpauseWhenReady = true;
        }
    }
}

/**
 * Set the timestretch factor
 */
void AudioOutputBase::SetStretchFactor(float lstretchfactor)
{
    QMutexLocker lock(&m_audioBufLock);
    SetStretchFactorLocked(lstretchfactor);
}

/**
 * Get the timetretch factor
 */
float AudioOutputBase::GetStretchFactor(void) const
{
    return m_stretchFactor;
}

/**
 * Source is currently being upmixed
 */
bool AudioOutputBase::IsUpmixing(void)
{
    return m_needsUpmix && m_upmixer;
}

/**
 * Toggle between stereo and upmixed 5.1 if the source material is stereo
 */
bool AudioOutputBase::ToggleUpmix(void)
{
    // Can only upmix from mono/stereo to 6 ch
    if (m_maxChannels == 2 || m_sourceChannels > 2)
        return false;

    m_upmixDefault = !m_upmixDefault;

    const AudioSettings settings(m_format, m_sourceChannels, m_codec,
                                 m_sourceSampleRate,
                                 m_upmixDefault ? false : m_passthru);
    Reconfigure(settings);
    return IsUpmixing();
}

/**
 * Upmixing of the current source is available if requested
 */
bool AudioOutputBase::CanUpmix(void)
{
    return m_sourceChannels <= 2 && m_maxChannels > 2;
}

/*
 * Setup samplerate and number of channels for passthrough
 * Create SPDIF encoder and true if successful
 */
bool AudioOutputBase::SetupPassthrough(AVCodecID codec, int codec_profile,
                                       int &samplerate_tmp, int &channels_tmp)
{
    if (codec == AV_CODEC_ID_DTS &&
        !m_outputSettingsDigital->canFeature(FEATURE_DTSHD))
    {
        // We do not support DTS-HD bitstream so force extraction of the
        // DTS core track instead
        codec_profile = FF_PROFILE_DTS;
    }
    QString log = AudioOutputSettings::GetPassthroughParams(
        codec, codec_profile,
        samplerate_tmp, channels_tmp,
        m_outputSettingsDigital->GetMaxHDRate() == 768000);
    LOG(VB_AUDIO, LOG_INFO, LOC + "Setting " + log + " passthrough");

    delete m_spdifEnc;

    // No spdif encoder needed for certain devices
    if (m_usesSpdif)
        m_spdifEnc = new SPDIFEncoder("spdif", codec);
    else
        m_spdifEnc = nullptr;
    if (m_spdifEnc && m_spdifEnc->Succeeded() && codec == AV_CODEC_ID_DTS)
    {
        switch(codec_profile)
        {
            case FF_PROFILE_DTS:
            case FF_PROFILE_DTS_ES:
            case FF_PROFILE_DTS_96_24:
                m_spdifEnc->SetMaxHDRate(0);
                break;
            case FF_PROFILE_DTS_HD_HRA:
            case FF_PROFILE_DTS_HD_MA:
                m_spdifEnc->SetMaxHDRate(samplerate_tmp * channels_tmp / 2);
                break;
        }
    }

    if (m_spdifEnc && !m_spdifEnc->Succeeded())
    {
        delete m_spdifEnc;
        m_spdifEnc = nullptr;
        return false;
    }
    return true;
}

AudioOutputSettings *AudioOutputBase::OutputSettings(bool digital)
{
    if (digital)
        return m_outputSettingsDigital;
    return m_outputSettings;
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
    int  lconfigured_channels = m_configuredChannels;
    bool lneeds_upmix         = false;
    bool lneeds_downmix       = false;
    bool lreenc               = false;
    bool lenc                 = false;

    if (!settings.m_usePassthru)
    {
        // Do we upmix stereo or mono?
        lconfigured_channels =
            (m_upmixDefault && lsource_channels <= 2) ? 6 : lsource_channels;
        bool cando_channels =
            m_outputSettings->IsSupportedChannels(lconfigured_channels);

        // check if the number of channels could be transmitted via AC3 encoding
#ifndef DISABLE_AC3_ENCODE
        lenc = m_outputSettingsDigital->canFeature(FEATURE_AC3) &&
            (!m_outputSettings->canFeature(FEATURE_LPCM) &&
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
                    lconfigured_channels = m_upmixDefault ? 6 : 2;
                    break;
                default:
                    lconfigured_channels = 2;
                    break;
            }
        }
        // Make sure we never attempt to output more than what we can
        // the upmixer can only upmix to 6 channels when source < 6
        if (lsource_channels <= 6)
            lconfigured_channels = std::min(lconfigured_channels, 6);
        lconfigured_channels = std::min(lconfigured_channels, m_maxChannels);
        /* Encode to AC-3 if we're allowed to passthru but aren't currently
           and we have more than 2 channels but multichannel PCM is not
           supported or if the device just doesn't support the number of
           channels */
#ifndef DISABLE_AC3_ENCODE
        lenc = m_outputSettingsDigital->canFeature(FEATURE_AC3) &&
            ((!m_outputSettings->canFeature(FEATURE_LPCM) &&
              lconfigured_channels > 2) ||
             !m_outputSettings->IsSupportedChannels(lconfigured_channels));
        /* Might we reencode a bitstream that's been decoded for timestretch?
           If the device doesn't support the number of channels - see below */
        if (m_outputSettingsDigital->canFeature(FEATURE_AC3) &&
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
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Needs upmix from %1 -> %2 channels")
                    .arg(settings.m_channels).arg(lconfigured_channels));
            settings.m_channels = lconfigured_channels;
            lneeds_upmix = true;
        }
        else if (settings.m_channels > lconfigured_channels)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Needs downmix from %1 -> %2 channels")
                    .arg(settings.m_channels).arg(lconfigured_channels));
            settings.m_channels = lconfigured_channels;
            lneeds_downmix = true;
        }
    }

    ClearError();

    bool general_deps = true;

    /* Set samplerate_tmp and channels_tmp to appropriate values
       if passing through */
    int samplerate_tmp = 0;
    int channels_tmp = 0;
    if (settings.m_usePassthru)
    {
        samplerate_tmp = settings.m_sampleRate;
        SetupPassthrough(settings.m_codec, settings.m_codecProfile,
                         samplerate_tmp, channels_tmp);
        general_deps = m_sampleRate == samplerate_tmp && m_channels == channels_tmp;
        general_deps &= m_format == m_outputFormat && m_format == FORMAT_S16;
    }
    else
    {
        general_deps =
            settings.m_format == m_format && lsource_channels == m_sourceChannels;
    }

    // Check if anything has changed
    general_deps &=
        settings.m_sampleRate  == m_sourceSampleRate &&
        settings.m_usePassthru == m_passthru &&
        lconfigured_channels == m_configuredChannels &&
        lneeds_upmix == m_needsUpmix && lreenc == m_reEnc &&
        lneeds_downmix == m_needsDownmix;

    if (general_deps && m_configureSucceeded)
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "Reconfigure(): No change -> exiting");
        // if passthrough, source channels may have changed
        m_sourceChannels = lsource_channels;
        return;
    }

    KillAudio();

    QMutexLocker lock(&m_audioBufLock);
    QMutexLocker lockav(&m_avsyncLock);

    m_waud = m_raud = 0;
    m_resetActive.Clear();
    m_actuallyPaused = m_processing = m_forcedProcessing = false;

    m_channels               = settings.m_channels;
    m_sourceChannels         = lsource_channels;
    m_reEnc                  = lreenc;
    m_codec                  = settings.m_codec;
    m_passthru               = settings.m_usePassthru;
    m_configuredChannels     = lconfigured_channels;
    m_needsUpmix             = lneeds_upmix;
    m_needsDownmix           = lneeds_downmix;
    m_format                 = m_outputFormat    = settings.m_format;
    m_sourceSampleRate       = m_sampleRate      = settings.m_sampleRate;
    m_enc                    = lenc;

    m_killAudio = m_pauseAudio = false;
    m_wasPaused = true;

    // Don't try to do anything if audio hasn't been
    // initialized yet (e.g. rubbish was provided)
    if (m_sourceChannels <= 0 || m_format <= 0 || m_sampleRate <= 0)
    {
        SilentError(QString("Aborting Audio Reconfigure. ") +
                    QString("Invalid audio parameters ch %1 fmt %2 @ %3Hz")
                    .arg(m_sourceChannels).arg(m_format).arg(m_sampleRate));
        return;
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Original codec was %1, %2, %3 kHz, %4 channels")
            .arg(avcodec_get_name(m_codec),
                 m_outputSettings->FormatToString(m_format))
            .arg(m_sampleRate/1000)
            .arg(m_sourceChannels));

    if (m_needsDownmix && m_sourceChannels > 8)
    {
        Error(QObject::tr("Aborting Audio Reconfigure. "
              "Can't handle audio with more than 8 channels."));
        return;
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("enc(%1), passthru(%2), features (%3) "
                    "configured_channels(%4), %5 channels supported(%6) "
                    "max_channels(%7)")
            .arg(m_enc)
            .arg(m_passthru)
            .arg(m_outputSettingsDigital->FeaturesToString())
            .arg(m_configuredChannels)
            .arg(m_channels)
            .arg(OutputSettings(m_enc || m_passthru)->IsSupportedChannels(m_channels))
            .arg(m_maxChannels));

    int dest_rate = 0;

    // Force resampling if we are encoding to AC3 and sr > 48k
    // or if 48k override was checked in settings
    if ((m_sampleRate != 48000 &&
         gCoreContext->GetBoolSetting("Audio48kOverride", false)) ||
         (m_enc && (m_sampleRate > 48000)))
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "Forcing resample to 48 kHz");
        if (m_srcQuality < 0)
            m_srcQuality = QUALITY_MEDIUM;
        m_needResampler = true;
        dest_rate = 48000;
    }
        // this will always be false for passthrough audio as
        // CanPassthrough() already tested these conditions
    else
    {
        m_needResampler =
            !OutputSettings(m_enc || m_passthru)->IsSupportedRate(m_sampleRate);
        if (m_needResampler)
        {
            dest_rate = OutputSettings(m_enc)->NearestSupportedRate(m_sampleRate);
        }
    }

    if (m_needResampler && m_srcQuality > QUALITY_DISABLED)
    {
        m_sampleRate = dest_rate;

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Resampling from %1 kHz to %2 kHz with quality %3")
                .arg(settings.m_sampleRate/1000).arg(m_sampleRate/1000)
                .arg(quality_string(m_srcQuality)));

        int chans = m_needsDownmix ? m_configuredChannels : m_sourceChannels;

        int error = 0;
        m_srcCtx = src_new(2-m_srcQuality, chans, &error);
        if (error)
        {
            Error(QObject::tr("Error creating resampler: %1")
                  .arg(src_strerror(error)));
            m_srcCtx = nullptr;
            return;
        }

        m_srcData.src_ratio = (double)m_sampleRate / settings.m_sampleRate;
        m_srcData.data_in   = m_srcIn;
        int newsize        = (int)((kAudioSRCInputSize * m_srcData.src_ratio) + 15)
                             & ~0xf;

        if (m_kAudioSRCOutputSize < newsize)
        {
            m_kAudioSRCOutputSize = newsize;
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Resampler allocating %1").arg(newsize));
            delete[] m_srcOut;
            m_srcOut = new float[m_kAudioSRCOutputSize];
        }
        m_srcData.data_out       = m_srcOut;
        m_srcData.output_frames  = m_kAudioSRCOutputSize / chans;
        m_srcData.end_of_input = 0;
    }

    if (m_enc)
    {
        if (m_reEnc)
            LOG(VB_AUDIO, LOG_INFO, LOC + "Reencoding decoded AC-3/DTS to AC-3");

        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Creating AC-3 Encoder with sr = %1, ch = %2")
                .arg(m_sampleRate).arg(m_configuredChannels));

        m_encoder = new AudioOutputDigitalEncoder();
        if (!m_encoder->Init(AV_CODEC_ID_AC3, 448000, m_sampleRate,
                           m_configuredChannels))
        {
            Error(QObject::tr("AC-3 encoder initialization failed"));
            delete m_encoder;
            m_encoder = nullptr;
            m_enc = false;
            // upmixing will fail if we needed the encoder
            m_needsUpmix = false;
        }
    }

    if (m_passthru)
    {
        //AC3, DTS, DTS-HD MA and TrueHD all use 16 bits samples
        m_channels = channels_tmp;
        m_sampleRate = samplerate_tmp;
        m_format = m_outputFormat = FORMAT_S16;
        m_sourceBytesPerFrame = m_channels *
            AudioOutputSettings::SampleSize(m_format);
    }
    else
    {
        m_sourceBytesPerFrame = m_sourceChannels *
            AudioOutputSettings::SampleSize(m_format);
    }

    // Turn on float conversion?
    if (m_needResampler || m_needsUpmix || m_needsDownmix ||
        m_stretchFactor != 1.0F || (m_internalVol && SWVolume()) ||
        (m_enc && m_outputFormat != FORMAT_S16) ||
        !OutputSettings(m_enc || m_passthru)->IsSupportedFormat(m_outputFormat))
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "Audio processing enabled");
        m_processing  = true;
        if (m_enc)
            m_outputFormat = FORMAT_S16;  // Output s16le for AC-3 encoder
        else
            m_outputFormat = m_outputSettings->BestSupportedFormat();
    }

    m_bytesPerFrame =  m_processing ?
        sizeof(float) : AudioOutputSettings::SampleSize(m_format);
    m_bytesPerFrame *= m_channels;

    if (m_enc)
        m_channels = 2; // But only post-encoder

    m_outputBytesPerFrame = m_channels *
                             AudioOutputSettings::SampleSize(m_outputFormat);

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Opening audio device '%1' ch %2(%3) sr %4 sf %5 reenc %6")
        .arg(m_mainDevice).arg(m_channels).arg(m_sourceChannels).arg(m_sampleRate)
        .arg(m_outputSettings->FormatToString(m_outputFormat)).arg(m_reEnc));

    m_audbufTimecode = m_audioTime = 0ms;
    m_framesBuffered = 0;
    m_currentSeconds = -1s;
    m_sourceBitRate = -1;
    m_effDsp = m_sampleRate * 100;

    // Actually do the device specific open call
    if (!OpenDevice())
    {
        if (GetError().isEmpty())
            Error(QObject::tr("Aborting reconfigure"));
        else
            LOG(VB_GENERAL, LOG_INFO, LOC + "Aborting reconfigure");
        m_configureSucceeded = false;
        return;
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Audio fragment size: %1").arg(m_fragmentSize));

    // Only used for software volume
    if (m_setInitialVol && m_internalVol && SWVolume())
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "Software volume enabled");
        m_volumeControl  = gCoreContext->GetSetting("MixerControl", "PCM");
        m_volumeControl += "MixerVolume";
        m_volume = gCoreContext->GetNumSetting(m_volumeControl, 80);
    }

    VolumeBase::SetChannels(m_configuredChannels);
    VolumeBase::SyncVolume();
    VolumeBase::UpdateVolume();

    if (m_needsUpmix && m_configuredChannels > 2)
    {
        m_surroundMode = gCoreContext->GetNumSetting("AudioUpmixType", QUALITY_HIGH);
        m_upmixer = new FreeSurround(m_sampleRate, m_source == AUDIOOUTPUT_VIDEO,
                                   (FreeSurround::SurroundMode)m_surroundMode);
        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Create %1 quality upmixer done")
                .arg(quality_string(m_surroundMode)));
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Audio Stretch Factor: %1").arg(m_stretchFactor));
    SetStretchFactorLocked(m_oldStretchFactor);

    // Setup visualisations, zero the visualisations buffers
    prepareVisuals();

    if (m_unpauseWhenReady)
        m_pauseAudio = m_actuallyPaused = true;

    m_configureSucceeded = true;

    StartOutputThread();

    LOG(VB_AUDIO, LOG_INFO, LOC + "Ending Reconfigure()");
}

bool AudioOutputBase::StartOutputThread(void)
{
    if (m_audioThreadExists)
        return true;

    start();
    m_audioThreadExists = true;

    return true;
}


void AudioOutputBase::StopOutputThread(void)
{
    if (m_audioThreadExists)
    {
        wait();
        m_audioThreadExists = false;
    }
}

/**
 * Kill the output thread and cleanup
 */
void AudioOutputBase::KillAudio()
{
    m_killAudioLock.lock();

    LOG(VB_AUDIO, LOG_INFO, LOC + "Killing AudioOutputDSP");
    m_killAudio = true;
    StopOutputThread();
    QMutexLocker lock(&m_audioBufLock);

    if (m_pSoundStretch)
    {
        delete m_pSoundStretch;
        m_pSoundStretch = nullptr;
        m_oldStretchFactor = m_stretchFactor;
        m_stretchFactor = 1.0F;
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

    if (m_srcCtx)
    {
        src_delete(m_srcCtx);
        m_srcCtx = nullptr;
    }

    m_needsUpmix = m_needResampler = m_enc = false;

    CloseDevice();

    m_killAudioLock.unlock();
}

void AudioOutputBase::Pause(bool paused)
{
    if (!paused && m_unpauseWhenReady)
        return;
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Pause %1").arg(paused));
    if (m_pauseAudio != paused)
        m_wasPaused = m_pauseAudio;
    m_pauseAudio = paused;
    m_unpauseWhenReady = false;
    m_actuallyPaused = false;
}

void AudioOutputBase::PauseUntilBuffered()
{
    Reset();
    Pause(true);
    m_unpauseWhenReady = true;
}

/**
 * Reset the audiobuffer, timecode and mythmusic visualisation
 */
void AudioOutputBase::Reset()
{
    QMutexLocker lock(&m_audioBufLock);
    QMutexLocker lockav(&m_avsyncLock);

    m_audbufTimecode = m_audioTime = 0ms;
    m_framesBuffered = 0;
    if (m_encoder)
    {
        m_waud = m_raud = 0;    // empty ring buffer
        m_audioBuffer.fill(0);
    }
    else
    {
        m_waud = m_raud;        // empty ring buffer
    }
    m_resetActive.Ref();
    m_currentSeconds = -1s;
    m_wasPaused = !m_pauseAudio;
    m_unpauseWhenReady = false;
    // clear any state that could remember previous audio in any active filters
    if (m_needsUpmix && m_upmixer)
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
void AudioOutputBase::SetTimecode(std::chrono::milliseconds timecode)
{
    m_audbufTimecode = m_audioTime = timecode;
    m_framesBuffered = (timecode.count() * m_sourceSampleRate) / 1000;
}

/**
 * Set the effective DSP rate
 *
 * Equal to 100 * samples per second
 * NuppelVideo sets this every sync frame to achieve av sync
 */
void AudioOutputBase::SetEffDsp(int dsprate)
{
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("SetEffDsp: %1").arg(dsprate));
    m_effDsp = dsprate;
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
    /* There is one wasted byte in the buffer. The case where m_waud = m_raud is
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
    if (m_passthru || m_enc || m_bytesPerFrame == m_outputBytesPerFrame)
        return audiolen();
    return audiolen() * m_outputBytesPerFrame / m_bytesPerFrame;
}

/**
 * Calculate the timecode of the samples that are about to become audible
 */
std::chrono::milliseconds AudioOutputBase::GetAudiotime(void)
{
    if (m_audbufTimecode == 0ms || !m_configureSucceeded)
        return 0ms;

    // output bits per 10 frames
    int64_t obpf = 0;

    if (m_passthru && !usesSpdif())
        obpf = m_sourceBitRate * 10 / m_sourceSampleRate;
    else if (m_enc && !usesSpdif())
    {
        // re-encode bitrate is hardcoded at 448000
        obpf = 448000 * 10 / m_sourceSampleRate;
    }
    else
    {
        obpf = static_cast<int64_t>(m_outputBytesPerFrame) * 80;
    }

    /* We want to calculate 'm_audioTime', which is the timestamp of the audio
       Which is leaving the sound card at this instant.

       We use these variables:

       'm_effDsp' is 100 * frames/sec

       'm_audbufTimecode' is the timecode in milliseconds of the
       audio that has just been written into the buffer.

       'm_effStretchFactor' is stretch factor * 100,000

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer. */


    QMutexLocker lockav(&m_avsyncLock);

    int64_t soundcard_buffer = GetBufferedOnSoundcard(); // bytes

    /* audioready tells us how many bytes are in audiobuffer
       scaled appropriately if output format != internal format */
    int64_t main_buffer = audioready();

    std::chrono::milliseconds oldaudiotime = m_audioTime;

    /* timecode is the stretch adjusted version
       of major post-stretched buffer contents
       processing latencies are catered for in AddData/SetAudiotime
       to eliminate race */

    m_audioTime = m_audbufTimecode - std::chrono::milliseconds(m_effDsp && obpf ?
        ((main_buffer + soundcard_buffer) * int64_t(m_effStretchFactor)
        * 80 / int64_t(m_effDsp) / obpf) : 0);

    /* audiotime should never go backwards, but we might get a negative
       value if GetBufferedOnSoundcard() isn't updated by the driver very
       quickly (e.g. ALSA) */
    if (m_audioTime < oldaudiotime)
        m_audioTime = oldaudiotime;

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + QString("GetAudiotime audt=%1 abtc=%2 mb=%3 sb=%4 tb=%5 "
                      "sr=%6 obpf=%7 bpf=%8 esf=%9 edsp=%10 sbr=%11")
              .arg(m_audioTime.count())                // 1
              .arg(m_audbufTimecode.count())           // 2
              .arg(main_buffer)                        // 3
              .arg(soundcard_buffer)                   // 4
              .arg(main_buffer+soundcard_buffer)       // 5
              .arg(m_sampleRate).arg(obpf)             // 6, 7
              .arg(m_bytesPerFrame)                    // 8
              .arg(m_effStretchFactor)                 // 9
              .arg(m_effDsp).arg(m_sourceBitRate)      // 10, 11
              );

    return m_audioTime;
}

/**
 * Set the timecode of the top of the ringbuffer
 * Exclude all other processing elements as they dont vary
 * between AddData calls
 */
void AudioOutputBase::SetAudiotime(int frames, std::chrono::milliseconds timecode)
{
    int64_t processframes_stretched   = 0;
    int64_t processframes_unstretched = 0;
    std::chrono::milliseconds old_audbuf_timecode = m_audbufTimecode;

    if (!m_configureSucceeded)
        return;

    if (m_needsUpmix && m_upmixer)
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

    m_audbufTimecode =
        timecode + std::chrono::milliseconds(m_effDsp ? ((frames + processframes_unstretched) * 100000 +
                    (processframes_stretched * m_effStretchFactor)
                   ) / m_effDsp : 0);

    // check for timecode wrap and reset audiotime if detected
    // timecode will always be monotonic asc if not seeked and reset
    // happens if seek or pause happens
    if (m_audbufTimecode < old_audbuf_timecode)
        m_audioTime = 0ms;

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + QString("SetAudiotime atc=%1 tc=%2 f=%3 pfu=%4 pfs=%5")
              .arg(m_audbufTimecode.count())
              .arg(timecode.count())
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
std::chrono::milliseconds AudioOutputBase::GetAudioBufferedTime(void)
{
    std::chrono::milliseconds ret = m_audbufTimecode - GetAudiotime();
    // Pulse can give us values that make this -ve
    if (ret < 0ms)
        return 0ms;
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
    int bpf   = m_bytesPerFrame;
    int len   = frames * bpf;
    int afree = audiofree();

    if (len <= afree)
        return len;

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Audio buffer overflow, %1 frames lost!")
            .arg(frames - (afree / bpf)));

    frames = afree / bpf;
    len = frames * bpf;

    if (!m_srcCtx)
        return len;

    int error = src_reset(m_srcCtx);
    if (error)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error occurred while resetting resampler: %1")
                .arg(src_strerror(error)));
        m_srcCtx = nullptr;
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
    int bpf   = m_bytesPerFrame;
    ptrdiff_t off = 0;

    if (!m_needsUpmix)
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
        // m_configuredChannels == 2 && m_sourceChannels == 1
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
    off *= m_sourceChannels;

    int i = 0;
    len = 0;
    while (i < frames)
    {
        i += m_upmixer->putFrames(buffer + (i * off), frames - i, m_sourceChannels);
        int nFrames = m_upmixer->numFrames();
        if (!nFrames)
            continue;

        len += CheckFreeSpace(nFrames);

        int bdFrames = (kAudioRingBufferSize - org_waud) / bpf;
        if (bdFrames < nFrames)
        {
            if ((org_waud % bpf) != 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Upmixing: org_waud = %1 (bpf = %2)")
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
                                std::chrono::milliseconds timecode)
{
    return AddData(in_buffer, in_frames * m_sourceBytesPerFrame, timecode,
                   in_frames);
}

/**
 * Add data to the audiobuffer and perform any required processing
 *
 * Returns false if there's not enough space right now
 */
bool AudioOutputBase::AddData(void *in_buffer, int in_len,
                              std::chrono::milliseconds timecode,
                              int /*in_frames*/)
{
    int frames   = in_len / m_sourceBytesPerFrame;
    int bpf      = m_bytesPerFrame;
    int len      = in_len;
    bool music   = false;

    if (!m_configureSucceeded)
    {
        LOG(VB_GENERAL, LOG_ERR, "AddData called with audio framework not "
                                 "initialised");
        m_lengthLastData = 0ms;
        return false;
    }

    /* See if we're waiting for new samples to be buffered before we unpause
       post channel change, seek, etc. Wait for 4 fragments to be buffered */
    if (m_unpauseWhenReady && m_pauseAudio && audioready() > m_fragmentSize << 2)
    {
        m_unpauseWhenReady = false;
        Pause(false);
    }

    // Don't write new samples if we're resetting the buffer or reconfiguring
    QMutexLocker lock(&m_audioBufLock);

    uint org_waud = m_waud;
    int  afree    = audiofree();
    int  used     = kAudioRingBufferSize - afree;

    if (m_passthru && m_spdifEnc)
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
        m_spdifEnc->WriteFrame((unsigned char *)in_buffer, len);
        len = m_spdifEnc->GetProcessedSize();
        if (len > 0)
        {
            in_buffer = m_spdifEnc->GetProcessedBuffer();
            m_spdifEnc->Reset();
            frames = len / m_sourceBytesPerFrame;
        }
        else
        {
            frames = 0;
        }
    }
    m_lengthLastData = millisecondsFromFloat
        ((double)(len * 1000) / (m_sourceSampleRate * m_sourceBytesPerFrame));

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + QString("AddData frames=%1, bytes=%2, used=%3, free=%4, "
                      "timecode=%5 needsupmix=%6")
              .arg(frames).arg(len).arg(used).arg(afree).arg(timecode.count())
              .arg(m_needsUpmix));

    // Mythmusic doesn't give us timestamps
    if (timecode < 0ms)
    {
        timecode = std::chrono::milliseconds((m_framesBuffered * 1000) / m_sourceSampleRate);
        m_framesBuffered += frames;
        music = true;
    }

    if (hasVisual())
    {
        // Send original samples to any attached visualisations
        dispatchVisual((uchar *)in_buffer, len, timecode, m_sourceChannels,
                       AudioOutputSettings::FormatToBits(m_format));
    }

    // Calculate amount of free space required in ringbuffer
    if (m_processing)
    {
        int sampleSize = AudioOutputSettings::SampleSize(m_format);
        if (sampleSize <= 0)
        {
            // Would lead to division by zero (or unexpected results if negative)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Sample size is <= 0, AddData returning false");
            return false;
        }

        // Final float conversion space requirement
        len = sizeof(m_srcInBuf[0]) / sampleSize * len;

        // Account for changes in number of channels
        if (m_needsDownmix)
            len = (len * m_configuredChannels ) / m_sourceChannels;

        // Check we have enough space to write the data
        if (m_needResampler && m_srcCtx)
            len = lround(ceil(static_cast<double>(len) * m_srcData.src_ratio));

        if (m_needsUpmix)
            len = (len * m_configuredChannels ) / m_sourceChannels;

        // Include samples in upmix buffer that may be flushed
        if (m_needsUpmix && m_upmixer)
            len += m_upmixer->numUnprocessedFrames() * bpf;

        // Include samples in soundstretch buffers
        if (m_pSoundStretch)
            len += (m_pSoundStretch->numUnprocessedSamples() +
                    (int)(m_pSoundStretch->numSamples() / m_stretchFactor)) * bpf;
    }

    if (len > afree)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Buffer is full, AddData returning false");
        return false; // would overflow
    }

    int frames_remaining = frames;
    int frames_final = 0;
    int maxframes = (kAudioSRCInputSize / m_sourceChannels) & ~0xf;
    int offset = 0;

    while(frames_remaining > 0)
    {
        void *buffer = (char *)in_buffer + offset;
        frames = frames_remaining;
        len = frames * m_sourceBytesPerFrame;

        if (m_processing)
        {
            if (frames > maxframes)
            {
                frames = maxframes;
                len = frames * m_sourceBytesPerFrame;
                offset += len;
            }
            // Convert to floats
            AudioOutputUtil::toFloat(m_format, m_srcIn, buffer, len);
        }

        frames_remaining -= frames;

        // Perform downmix if necessary
        if (m_needsDownmix)
        {
            if(AudioOutputDownmix::DownmixFrames(m_sourceChannels,
                                                 m_configuredChannels,
                                                 m_srcIn, m_srcIn, frames) < 0)
                LOG(VB_GENERAL, LOG_ERR, LOC + "Error occurred while downmixing");
        }

        // Resample if necessary
        if (m_needResampler && m_srcCtx)
        {
            m_srcData.input_frames = frames;
            int error = src_process(m_srcCtx, &m_srcData);

            if (error)
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error occurred while resampling audio: %1")
                        .arg(src_strerror(error)));

            buffer = m_srcOut;
            frames = m_srcData.output_frames_gen;
        }
        else if (m_processing)
        {
            buffer = m_srcIn;
        }

        /* we want the timecode of the last sample added but we are given the
           timecode of the first - add the time in ms that the frames added
           represent */

        // Copy samples into audiobuffer, with upmix if necessary
        len = CopyWithUpmix((char *)buffer, frames, org_waud);
        if (len <= 0)
        {
            continue;
        }

        frames = len / bpf;
        frames_final += frames;

        int bdiff = kAudioRingBufferSize - m_waud;
        if ((len % bpf) != 0 && bdiff < len)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("AddData: Corruption likely: len = %1 (bpf = %2)")
                    .arg(len)
                    .arg(bpf));
        }
        if ((bdiff % bpf) != 0 && bdiff < len)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("AddData: Corruption likely: bdiff = %1 (bpf = %2)")
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
            {
                m_pSoundStretch->putSamples((STST *)(WPOS), frames);
            }

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

        if (m_internalVol && SWVolume())
        {
            org_waud    = m_waud;
            int num     = len;

            if (bdiff <= num)
            {
                AudioOutputUtil::AdjustVolume(WPOS, bdiff, m_volume,
                                              music, m_needsUpmix && m_upmixer);
                num -= bdiff;
                org_waud = 0;
            }
            if (num > 0)
                AudioOutputUtil::AdjustVolume(WPOS, num, m_volume,
                                              music, m_needsUpmix && m_upmixer);
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
    std::chrono::milliseconds ct = GetAudiotime();

    if (ct < 0ms)
        ct = 0ms;

    if (m_sourceBitRate == -1)
        m_sourceBitRate = static_cast<long>(m_sourceSampleRate) * m_sourceChannels *
                         AudioOutputSettings::FormatToBits(m_format);

    if (duration_cast<std::chrono::seconds>(ct) != m_currentSeconds)
    {
        m_currentSeconds = duration_cast<std::chrono::seconds>(ct);
        OutputEvent e(m_currentSeconds, ct.count(), m_sourceBitRate, m_sourceSampleRate,
                      AudioOutputSettings::FormatToBits(m_format), m_sourceChannels);
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
    auto *zeros        = new(std::align_val_t(16)) uchar[m_fragmentSize];
    auto *fragment     = new(std::align_val_t(16)) uchar[m_fragmentSize];
    memset(zeros, 0, m_fragmentSize);

    // to reduce startup latency, write silence in 8ms chunks
    int zero_fragment_size = 8 * m_sampleRate * m_outputBytesPerFrame / 1000;
    zero_fragment_size = std::min(zero_fragment_size, m_fragmentSize);

    while (!m_killAudio)
    {
        if (m_pauseAudio)
        {
            if (!m_actuallyPaused)
            {
                LOG(VB_AUDIO, LOG_INFO, LOC + "OutputAudioLoop: audio paused");
                OutputEvent e(OutputEvent::kPaused);
                dispatch(e);
                m_wasPaused = true;
            }

            m_actuallyPaused = true;
            m_audioTime = 0ms; // mark 'audiotime' as invalid.

            WriteAudio(zeros, zero_fragment_size);
            continue;
        }

        if (m_wasPaused)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + "OutputAudioLoop: Play Event");
            OutputEvent e(OutputEvent::kPlaying);
            dispatch(e);
            m_wasPaused = false;
        }

        /* do audio output */
        int ready = audioready();

        // wait for the buffer to fill with enough to play
        if (m_fragmentSize > ready)
        {
            if (ready > 0)  // only log if we're sending some audio
            {
                LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + QString("audio waiting for buffer to fill: "
                                  "have %1 want %2")
                          .arg(ready).arg(m_fragmentSize));
            }

            usleep(10ms);
            continue;
        }

#ifdef AUDIOTSTESTING
        LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + "WriteAudio Start");
#endif
        Status();

        // delay setting raud until after phys buffer is filled
        // so GetAudiotime will be accurate without locking
        m_resetActive.TestAndDeref();
        volatile uint next_raud = m_raud;
        if (GetAudioData(fragment, m_fragmentSize, true, &next_raud))
        {
            if (!m_resetActive.TestAndDeref())
            {
                WriteAudio(fragment, m_fragmentSize);
                if (!m_resetActive.TestAndDeref())
                    m_raud = next_raud;
            }
        }
#ifdef AUDIOTSTESTING
        GetAudiotime();
        LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + "WriteAudio Done");
#endif

    }

    ::operator delete[] (zeros, std::align_val_t(16));
    ::operator delete[] (fragment, std::align_val_t(16));
    LOG(VB_AUDIO, LOG_INFO, LOC + "OutputAudioLoop: Stop Event");
    OutputEvent e(OutputEvent::kStopped);
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

#define LRPOS (&m_audioBuffer[*local_raud])
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

    int obytes = AudioOutputSettings::SampleSize(m_outputFormat);

    if (obytes <= 0)
        return 0;

    bool fromFloats = m_processing && !m_enc && m_outputFormat != FORMAT_FLT;

    // Scale if necessary
    if (fromFloats && obytes != sizeof(float))
        frag_size *= sizeof(float) / obytes;

    int off = 0;

    if (bdiff <= frag_size)
    {
        if (fromFloats)
        {
            off = AudioOutputUtil::fromFloat(m_outputFormat, buffer,
                                             LRPOS, bdiff);
        }
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
        {
            AudioOutputUtil::fromFloat(m_outputFormat, buffer + off,
                                       LRPOS, frag_size);
        }
        else
        {
            memcpy(buffer + off, LRPOS, frag_size);
        }
    }

    *local_raud += frag_size;

    // Mute individual channels through mono->stereo duplication
    MuteState mute_state = GetMuteState();
    if (!m_enc && !m_passthru &&
        written_size && m_configuredChannels > 1 &&
        (mute_state == kMuteLeft || mute_state == kMuteRight))
    {
        AudioOutputUtil::MuteChannel(obytes << 3, m_configuredChannels,
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
    while (!m_pauseAudio && audioready() > m_fragmentSize)
        usleep(1ms);
    if (m_pauseAudio)
    {
        // Audio is paused and can't be drained, clear ringbuffer
        QMutexLocker lock(&m_audioBufLock);

        m_waud = m_raud = 0;
    }
}

/**
 * Main routine for the output thread
 */
void AudioOutputBase::run(void)
{
    RunProlog();
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("kickoffOutputAudioLoop: pid = %1").arg(getpid()));
    OutputAudioLoop();
    LOG(VB_AUDIO, LOG_INFO, LOC + "kickoffOutputAudioLoop exiting");
    RunEpilog();
}

int AudioOutputBase::readOutputData(unsigned char* /*read_buffer*/, size_t /*max_length*/)
{
    LOG(VB_GENERAL, LOG_ERR, LOC + "AudioOutputBase should not be getting asked to readOutputData()");
    return 0;
}
