
// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// Qt headers
#include <QCoreApplication>
#include <QEvent>
#include <QDir>

// MythTV headers
#include "mythconfig.h"
#include "mythcorecontext.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "audiooutpututil.h"
#include "audiosettings.h"
#include "mythdialogbox.h"

class TriggeredItem : public TriggeredConfigurationGroup
{
  public:
    TriggeredItem(Setting *checkbox, ConfigurationGroup *group) :
        TriggeredConfigurationGroup(false, false, false, false)
    {
        setTrigger(checkbox);

        addTarget("1", group);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
    TriggeredItem(Setting *checkbox, Setting *setting) :
        TriggeredConfigurationGroup(false, false, false, false)
    {
        setTrigger(checkbox);

        addTarget("1", setting);
        addTarget("0", new VerticalConfigurationGroup(false, false));
    }
};

AudioDeviceComboBox::AudioDeviceComboBox(AudioConfigSettings *parent) :
    HostComboBox("AudioOutputDevice", true), m_parent(parent)
{
    setLabel(QObject::tr("Audio output device"));
#ifdef USING_ALSA
    QString dflt = "ALSA:default";
#elif USING_PULSEOUTPUT
    QString dflt = "PulseAudio:default";
#elif CONFIG_DARWIN
    QString dflt = "CoreAudio:";
#elif USING_MINGW
    QString dflt = "Windows:";
#else
    QString dflt = "NULL";
#endif
    QString current = gCoreContext->GetSetting(QString("AudioOutputDevice"),
                                               dflt);
    addSelection(current, current, true);

    connect(this, SIGNAL(valueChanged(const QString&)),
            this, SLOT(AudioDescriptionHelp(const QString&)));
}

void AudioDeviceComboBox::AudioRescan()
{
    AudioOutput::ADCVect &vect = m_parent->AudioDeviceVect();
    AudioOutput::ADCVect::const_iterator it;

    if (vect.empty())
        return;

    QString value = getValue();
    clearSelections();
    resetMaxCount(vect.size());

    bool found = false;
    for (it = vect.begin(); it != vect.end(); it++)
        addSelection(it->name, it->name,
                     value == it->name ? (found = true) : false);
    if (!found)
    {
        resetMaxCount(vect.size()+1);
        addSelection(value, value, true);
    }
        // For some reason, it adds an empty entry, remove it
    removeSelection(QString::null);
}

void AudioDeviceComboBox::AudioDescriptionHelp(const QString &device)
{
    QString desc = m_parent->AudioDeviceMap().value(device).desc;
    setHelpText(desc);
}

AudioConfigSettings::AudioConfigSettings() :
    VerticalConfigurationGroup(false, true, false, false),
    m_OutputDevice(NULL),   m_MaxAudioChannels(NULL),
    m_AudioUpmix(NULL),     m_AudioUpmixType(NULL),
    m_AC3PassThrough(NULL), m_DTSPassThrough(NULL),
    m_EAC3PassThrough(NULL),m_TrueHDPassThrough(NULL),
    m_passthrough8(false)
{
    setLabel(QObject::tr("Audio System"));
    setUseLabel(false);

    ConfigurationGroup *devicegroup = new HorizontalConfigurationGroup(false,
                                                                       false);
    devicegroup->addChild((m_OutputDevice = new AudioDeviceComboBox(this)));
        // Rescan button
    TransButtonSetting *rescan = new TransButtonSetting("rescan");
    rescan->setLabel(QObject::tr("Rescan"));
    rescan->setHelpText(QObject::tr("Rescan for available audio devices. "
                                    "Current entry will be checked and "
                                    "capability entries populated."));
    devicegroup->addChild(rescan);
    connect(rescan, SIGNAL(pressed()), this, SLOT(AudioRescan()));
    addChild(devicegroup);

    QString name = m_OutputDevice->getValue();
    AudioOutput::AudioDeviceConfig *adc =
        AudioOutput::GetAudioDeviceConfig(name, name, true);
    if (adc->settings.IsInvalid())
    {
        VERBOSE(VB_IMPORTANT, QString("Audio device %1 isn't usable "
                                      "Check audio configuration").arg(name));
    }
    audiodevs.insert(name, *adc);
    devices.append(*adc);

    delete adc;
    CheckPassthrough();

    ConfigurationGroup *maingroup = new VerticalConfigurationGroup(false,
                                                                   false);
    addChild(maingroup);

    m_triggerDigital = new TransCheckBoxSetting();
    m_AC3PassThrough = AC3PassThrough();
    m_DTSPassThrough = DTSPassThrough();
    m_EAC3PassThrough = EAC3PassThrough();
    m_TrueHDPassThrough = TrueHDPassThrough();

    m_cgsettings = new HorizontalConfigurationGroup();
    m_cgsettings->setLabel(QObject::tr("Digital Audio Capabilities"));
    m_cgsettings->addChild(m_AC3PassThrough);
    m_cgsettings->addChild(m_DTSPassThrough);
    m_cgsettings->addChild(m_EAC3PassThrough);
    m_cgsettings->addChild(m_TrueHDPassThrough);

    TriggeredItem *sub1 = new TriggeredItem(m_triggerDigital, m_cgsettings);

    maingroup->addChild(sub1);

    maingroup->addChild((m_MaxAudioChannels = MaxAudioChannels()));
    maingroup->addChild((m_AudioUpmix = AudioUpmix()));
    maingroup->addChild((m_AudioUpmixType = AudioUpmixType()));

    TransButtonSetting *test = new TransButtonSetting("test");
    test->setLabel(QObject::tr("Test"));
    test->setHelpText(QObject::tr("Will play a test pattern on all configured "
                                  "speakers"));
    connect(test, SIGNAL(pressed()), this, SLOT(AudioTest()));
    addChild(test);

    TransButtonSetting *advanced = new TransButtonSetting("advanced");
    advanced->setLabel(QObject::tr("Advanced Audio Settings"));
    advanced->setHelpText(QObject::tr("Enable extra audio settings. Under most "
                                  "usage all options should be unchecked"));
    connect(advanced, SIGNAL(pressed()), this, SLOT(AudioAdvanced()));
    addChild(advanced);

        // Set slots
    connect(m_MaxAudioChannels, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateVisibility(const QString&)));
    connect(m_OutputDevice, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateCapabilities(const QString&)));
    connect(m_AC3PassThrough, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateCapabilities(const QString&)));
    connect(m_DTSPassThrough, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateCapabilities(const QString&)));
    connect(m_EAC3PassThrough, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateCapabilities(const QString&)));
    connect(m_TrueHDPassThrough, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateCapabilities(const QString&)));
    AudioRescan();
}

void AudioConfigSettings::AudioRescan()
{
    if (!slotlock.tryLock())
        return;

    QVector<AudioOutput::AudioDeviceConfig>* list =
        AudioOutput::GetOutputList();
    QVector<AudioOutput::AudioDeviceConfig>::const_iterator it;

    audiodevs.clear();
    for (it = list->begin(); it != list->end(); it++)
        audiodevs.insert(it->name, *it);

    devices = *list;
    delete list;

    QString name = m_OutputDevice->getValue();
    if (!audiodevs.contains(name))
    {
            // Scan for possible custom entry that isn't in the list
        AudioOutput::AudioDeviceConfig *adc =
            AudioOutput::GetAudioDeviceConfig(name, name, true);
        if (adc->settings.IsInvalid())
        {
            QString msg = name + QObject::tr(" is invalid or not useable.");
            MythPopupBox::showOkPopup(
                GetMythMainWindow(), QObject::tr("Warning"), msg);
            VERBOSE(VB_IMPORTANT, QString("Audio device %1 isn't usable ")
                    .arg(name));
        }
        audiodevs.insert(name, *adc);
        devices.append(*adc);
        delete adc;
    }
    m_OutputDevice->AudioRescan();
    slotlock.unlock();
    UpdateCapabilities(QString::null);
}

void AudioConfigSettings::UpdateVisibility(const QString &device)
{
    if (!m_MaxAudioChannels && !m_AudioUpmix && !m_AudioUpmixType)
        return;

    int cur_speakers = m_MaxAudioChannels->getValue().toInt();
    m_AudioUpmix->setEnabled(cur_speakers > 2);
    m_AudioUpmixType->setEnabled(cur_speakers > 2);
}

void AudioConfigSettings::UpdateCapabilities(const QString &device)
{
    int max_speakers = 8;
    bool invalid = false;
    AudioOutputSettings settings;

        // Test if everything is set yet
    if (!m_OutputDevice    || !m_MaxAudioChannels   ||
        !m_AC3PassThrough  || !m_DTSPassThrough     ||
        !m_EAC3PassThrough || !m_TrueHDPassThrough)
        return;

    if (!slotlock.tryLock()) // Doing a rescan of channels
        return;

    bool bForceDigital = gCoreContext->GetNumSetting("PassThruDeviceOverride",
                                                     false);

    QString out = m_OutputDevice->getValue();
    if (!audiodevs.contains(out))
    {
        VERBOSE(VB_AUDIO, QString("Update not found (%1)").arg(out));
        invalid = true;
    }
    else
    {
        settings = audiodevs.value(out).settings;

        max_speakers = settings.BestSupportedChannels();

        bool bAC3  = (settings.canAC3() || bForceDigital) &&
            m_AC3PassThrough->boolValue();
        bool bDTS  = (settings.canDTS() || bForceDigital) &&
            m_DTSPassThrough->boolValue();
        bool bLPCM = settings.canPassthrough() == -1 ||
            (settings.canLPCM() &&
             !gCoreContext->GetNumSetting("StereoPCM", false));

        if (max_speakers > 2 && !bLPCM)
            max_speakers = 2;
        if (max_speakers == 2 && (bAC3 || bDTS))
            max_speakers = 6;
    }

    m_triggerDigital->setValue(invalid || bForceDigital ||
                               settings.canAC3() || settings.canDTS());
    m_EAC3PassThrough->setEnabled(invalid || m_passthrough8 ||
                                  (settings.canPassthrough() >= 0 &&
                                   max_speakers >= 8));
    m_TrueHDPassThrough->setEnabled(invalid || m_passthrough8 ||
                                    (settings.canPassthrough() >= 0 &&
                                     max_speakers >= 8));
    if (m_EAC3PassThrough->boolValue() || m_TrueHDPassThrough->boolValue())
    {
        max_speakers = 8;
    }

    int cur_speakers = m_MaxAudioChannels->getValue().toInt();

    if (cur_speakers > max_speakers)
    {
        VERBOSE(VB_AUDIO, QString("Reset device %1").arg(out));
        cur_speakers = max_speakers;
    }

        // Remove everything and re-add available channels
    m_MaxAudioChannels->clearSelections();
    m_MaxAudioChannels->resetMaxCount(3);
    for (int i = 1; i <= max_speakers; i++)
    {
        if (invalid || settings.IsSupportedChannels(i) ||
            (bForceDigital && i >= 6))
        {
            QString txt;

            switch (i)
            {
                case 2:
                    txt = QObject::tr("Stereo");
                    break;
                case 6:
                    txt = QObject::tr("5.1");
                    break;
                case 8:
                    txt = QObject::tr("7.1");
                    break;
                default:
                    continue;
            }
            m_MaxAudioChannels->addSelection(txt, QString::number(i),
                                             i == cur_speakers);
        }
    }
    slotlock.unlock();
}

void AudioConfigSettings::AudioAdvanced()
{
    QString out  = m_OutputDevice->getValue();
    bool invalid = false;
    AudioOutputSettings settings;

    if (!audiodevs.contains(out))
    {
        invalid = true;
    }
    else
    {
        settings = audiodevs.value(out).settings;
    }

    AudioAdvancedSettingsGroup audiosettings(invalid ||
                                             (settings.canLPCM() &&
                                              settings.canPassthrough() >= 0));

    if (audiosettings.exec() == kDialogCodeAccepted)
    {
        CheckPassthrough();
        UpdateCapabilities(QString::null);
    }
}

HostComboBox *AudioConfigSettings::MaxAudioChannels()
{
    QString name = "MaxChannels";
    HostComboBox *gc = new HostComboBox(name, false);
    gc->setLabel(QObject::tr("Speaker configuration"));
    gc->addSelection(QObject::tr("Stereo"), "2", true); // default
    gc->setHelpText(QObject::tr("Select the maximum number of audio "
                                "channels supported by your receiver "
                                "and speakers."));
    return gc;
}

HostCheckBox *AudioConfigSettings::AudioUpmix()
{
    HostCheckBox *gc = new HostCheckBox("AudioDefaultUpmix");
    gc->setLabel(QObject::tr("Upconvert stereo to 5.1 surround"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, MythTV will upconvert stereo "
                    "to 5.1 audio. You can enable or disable "
                    "the upconversion during playback at any time."));
    return gc;
}

HostComboBox *AudioConfigSettings::AudioUpmixType()
{
    HostComboBox *gc = new HostComboBox("AudioUpmixType",false);
    gc->setLabel(QObject::tr("Upmix Quality"));
    gc->addSelection(QObject::tr("Good"), "1");
    gc->addSelection(QObject::tr("Best"), "2", true);  // default
    gc->setHelpText(QObject::tr("Set the audio surround-upconversion quality."));
    return gc;
}

HostCheckBox *AudioConfigSettings::AC3PassThrough()
{
    HostCheckBox *gc = new HostCheckBox("AC3PassThru");
    gc->setLabel(QObject::tr("Dolby Digital"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable if your amplifier or sound decoder "
                    "supports AC3/Dolby Digital. You must use a digital "
                    "connection. Uncheck if using an analog connection."));
    return gc;
}

HostCheckBox *AudioConfigSettings::DTSPassThrough()
{
    HostCheckBox *gc = new HostCheckBox("DTSPassThru");
    gc->setLabel(QObject::tr("DTS"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable if your amplifier or sound decoder "
                    "supports DTS. You must use a digital connection. Uncheck "
                    "if using an analog connection"));
    return gc;
}

HostCheckBox *AudioConfigSettings::EAC3PassThrough()
{
    HostCheckBox *gc = new HostCheckBox("EAC3PassThru");
    gc->setLabel(QObject::tr("E-AC3/DTS-HD"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable if your amplifier or sound decoder "
                    "supports E-AC3 (DD+) or DTS-HD. You must use a hdmi "
                    "connection."));
    return gc;
}

HostCheckBox *AudioConfigSettings::TrueHDPassThrough()
{
    HostCheckBox *gc = new HostCheckBox("TrueHDPassThru");
    gc->setLabel(QObject::tr("TrueHD"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable if your amplifier or sound decoder "
                    "supports Dolby TrueHD. You must use a hdmi connection."));
    return gc;
}

bool AudioConfigSettings::CheckPassthrough()
{
    m_passthrough8 = false;
    if (gCoreContext->GetNumSetting("PassThruDeviceOverride", false))
    {
        QString name = gCoreContext->GetSetting("PassThruOutputDevice");
        AudioOutput::AudioDeviceConfig *adc =
            AudioOutput::GetAudioDeviceConfig(name, name, true);
        if (adc->settings.IsInvalid())
        {
            VERBOSE(VB_IMPORTANT, QString("Passthru device %1 isn't usable "
                                 "Check audio configuration").arg(name));
        }
        else
        {
            if (adc->settings.BestSupportedChannels() >= 8)
            {
                m_passthrough8 = true;
            }
        }
        delete adc;
    }
    return m_passthrough8;
}

void AudioConfigSettings::AudioTest()
{
    QString out  = m_OutputDevice->getValue();
    QString name;
    int channels = m_MaxAudioChannels->getValue().toInt();
    QString errMsg;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    SpeakerTest *st = new SpeakerTest(
        mainStack, "Speaker Test", out, name, 2);
    if (st->Create())
    {
        mainStack->AddScreen(st);
    }
    else
        delete st;
}

QEvent::Type ChannelChangedEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

// ---------------------------------------------------


AudioTestThread::AudioTestThread(QObject *parent,
                                 QString main, QString passthrough,
                                 int channels) :
    m_channels(channels), m_device(main), m_passthrough(passthrough),
    m_interrupted(false)
{
    m_parent = parent;
    m_audioOutput = AudioOutput::OpenAudio(m_device,
                                           m_passthrough,
                                           FORMAT_S16, m_channels,
                                           0, 48000,
                                           AUDIOOUTPUT_VIDEO,
                                           true, false);
}

AudioTestThread::~AudioTestThread()
{
    cancel();
    wait();
    if (m_audioOutput)
        delete m_audioOutput;
}

QString AudioTestThread::result()
{
    QString errMsg;
    if (!m_audioOutput)
        errMsg = QObject::tr("Unable to create AudioOutput.");
    else
        errMsg = m_audioOutput->GetError();
    return errMsg;
}

void AudioTestThread::cancel()
{
    m_interrupted = true;
}

void AudioTestThread::run()
{
    m_interrupted = false;

    if (m_audioOutput)
    {
        char *frames_in = new char[m_channels * 48000 * sizeof(int16_t) + 15];
        char *frames = (char *)(((long)frames_in + 15) & ~0xf);
        while (!m_interrupted)
        {
            for (int i = 0; i < m_channels && !m_interrupted; i++)
            {
                AudioOutputUtil::GeneratePinkSamples(frames, m_channels,
                                                     i, 48000);
                if (m_parent)
                {
                    QString channel;

                    switch(i)
                    {
                        case 0:
                            channel = "frontleft";
                            break;
                        case 1:
                            channel = "frontright";
                            break;
                        case 2:
                            channel = "center";
                            break;
                        case 3:
                            channel = "lfe";
                            break;
                        case 4:
                            if (m_channels == 6)
                                channel = "leftsurround";
                            else
                                channel = "rearleft";
                            break;
                        case 5:
                            if (m_channels == 6)
                                channel = "rightsurround";
                            else
                                channel = "rearleft";
                            break;
                        case 6:
                            channel = "leftsurround";
                            break;
                        case 7:
                            channel = "rightsurround";
                            break;
                    }
                    QCoreApplication::postEvent(
                        m_parent, new ChannelChangedEvent(channel));
                }
                if (!m_audioOutput->AddFrames(frames, 48000, -1))
                {
                    VERBOSE(VB_AUDIO, "AddAudioData() "
                            "Audio buffer overflow, audio data lost!");
                }
                for (int j = 0; j < 16 && !m_interrupted; j++)
                {
                    usleep(62500);  //1/16th of a second
                }
                usleep(500000); //.5s
            }
        }
        delete[] frames_in;
    }
}

SpeakerTest::SpeakerTest(MythScreenStack *parent, QString name,
                         QString main, QString passthrough, int channels)
    : MythScreenType(parent, name),
      m_speakerLocation(NULL), m_testButton(NULL), m_testSpeakers(NULL)
{
    m_testSpeakers = new AudioTestThread(this, main, passthrough, channels);
}

bool SpeakerTest::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "audiosettings", this);

    if (!foundtheme)
        return false;

    if (!m_testSpeakers->result().isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Not working.");
        return false;
    }

    m_testButton = dynamic_cast<MythUIButton *> (GetChild("test"));
    m_speakerLocation = dynamic_cast<MythUIStateType *> (GetChild("speakerlocation"));

    if (!m_testButton || !m_speakerLocation)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_testButton, SIGNAL(Clicked()), this, SLOT(toggleTest()));

    BuildFocusList();

    return true;
}

SpeakerTest::~SpeakerTest()
{
    if (m_testSpeakers)
    {
        m_testSpeakers->cancel();
        m_testSpeakers->wait();
        delete m_testSpeakers;
        m_testSpeakers = NULL;
    }
}

void SpeakerTest::toggleTest(void)
{
    if (m_testSpeakers->isRunning())
        m_testSpeakers->cancel();
    else if (m_testSpeakers)
        m_testSpeakers->start();
}

bool SpeakerTest::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void SpeakerTest::customEvent(QEvent *event)
{
    if (event->type() == ChannelChangedEvent::kEventType)
    {
        ChannelChangedEvent *cce = (ChannelChangedEvent*)(event);

        QString channel   = cce->channel;

        if (!channel.isEmpty())
            m_speakerLocation->DisplayState(channel);
    }
}

HostCheckBox *AudioMixerSettings::MythControlsVolume()
{
    HostCheckBox *gc = new HostCheckBox("MythControlsVolume");
    gc->setLabel(QObject::tr("Use internal volume controls"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, MythTV will control the PCM and "
                    "master mixer volume. Disable this option if you prefer "
                    "to control the volume externally (for example, using "
                    "your amplifier) or if you use an external mixer program."));
    return gc;
}

HostComboBox *AudioMixerSettings::MixerDevice()
{
    HostComboBox *gc = new HostComboBox("MixerDevice", true);
    gc->setLabel(QObject::tr("Mixer device"));

#ifdef USING_OSS
    QDir dev("/dev", "mixer*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        gc->fillSelectionsFromDir(dev);
    }
#endif
#ifdef USING_ALSA
    gc->addSelection("ALSA:default", "ALSA:default");
#endif
#ifdef USING_MINGW
    gc->addSelection("DirectX:", "DirectX:");
    gc->addSelection("Windows:", "Windows:");
#endif
#if !defined(USING_MINGW)
    gc->addSelection("software", "software");
    gc->setHelpText(QObject::tr("Setting the mixer device to \"software\" "
                    "lets MythTV control the volume of all audio at the "
                    "expense of a slight quality loss."));
#endif

    return gc;
}

const char* AudioMixerSettings::MixerControlControls[] = { "PCM",
                                                           "Master" };

HostComboBox *AudioMixerSettings::MixerControl()
{
    HostComboBox *gc = new HostComboBox("MixerControl", true);
    gc->setLabel(QObject::tr("Mixer controls"));
    for (unsigned int i = 0; i < sizeof(MixerControlControls) / sizeof(char*);
         ++i)
    {
        gc->addSelection(QObject::tr(MixerControlControls[i]),
                         MixerControlControls[i]);
    }

    gc->setHelpText(QObject::tr("Changing the volume adjusts the selected mixer."));
    return gc;
}

HostSlider *AudioMixerSettings::MixerVolume()
{
    HostSlider *gs = new HostSlider("MasterMixerVolume", 0, 100, 1);
    gs->setLabel(QObject::tr("Master mixer volume"));
    gs->setValue(70);
    gs->setHelpText(QObject::tr("Initial volume for the Master mixer. "
                    "This affects all sound created by the audio device. "
                    "Note: Do not set this too low."));
    return gs;
}

HostSlider *AudioMixerSettings::PCMVolume()
{
    HostSlider *gs = new HostSlider("PCMMixerVolume", 0, 100, 1);
    gs->setLabel(QObject::tr("PCM mixer volume"));
    gs->setValue(70);
    gs->setHelpText(QObject::tr("Initial volume for PCM output. Using the "
                    "volume keys in MythTV will adjust this parameter."));
    return gs;
}

AudioMixerSettings::AudioMixerSettings() :
    TriggeredConfigurationGroup(false, true, false, false)
{
    setLabel(QObject::tr("Audio Mixer"));
    setUseLabel(false);

    Setting *volumeControl = MythControlsVolume();
    addChild(volumeControl);

        // Mixer settings
    ConfigurationGroup *settings =
        new VerticalConfigurationGroup(false, true, false, false);
    settings->addChild(MixerDevice());
    settings->addChild(MixerControl());
    settings->addChild(MixerVolume());
    settings->addChild(PCMVolume());

    ConfigurationGroup *dummy =
        new VerticalConfigurationGroup(false, true, false, false);

        // Show Mixer config only if internal volume controls enabled
    setTrigger(volumeControl);
    addTarget("0", dummy);
    addTarget("1", settings);
}

AudioGeneralSettings::AudioGeneralSettings()
{
    addChild(new AudioConfigSettings());
    addChild(new AudioMixerSettings());
}

HostCheckBox *AudioAdvancedSettings::MPCM()
{
    HostCheckBox *gc = new HostCheckBox("StereoPCM");
    gc->setLabel(QObject::tr("Stereo PCM Only"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable if your amplifier or sound decoder "
                    "only supports 2 channels PCM (typically an old HDMI 1.0 "
                    "device). Multi-channels audio will be re-encoded to AC3 "
                    "when required"));
    return gc;
}

HostCheckBox *AudioAdvancedSettings::SRCQualityOverride()
{
    HostCheckBox *gc = new HostCheckBox("SRCQualityOverride");
    gc->setLabel(QObject::tr("Override SRC quality"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable to override audio sample rate "
                    "conversion quality."));
    return gc;
}

HostComboBox *AudioAdvancedSettings::SRCQuality()
{
    HostComboBox *gc = new HostComboBox("SRCQuality", false);
    gc->setLabel(QObject::tr("Sample rate conversion"));
    gc->addSelection(QObject::tr("Disabled"), "-1");
    gc->addSelection(QObject::tr("Fastest"), "0");
    gc->addSelection(QObject::tr("Good"), "1", true); // default
    gc->addSelection(QObject::tr("Best"), "2");
    gc->setHelpText(QObject::tr("Set the quality of audio sample-rate "
                    "conversion. \"Good\" (default) provides the best "
                    "compromise between CPU usage and quality. \"Disabled\" "
                    "lets the audio device handle sample-rate conversion."));
    return gc;
}

HostCheckBox *AudioAdvancedSettings::Audio48kOverride()
{
    HostCheckBox *gc = new HostCheckBox("Audio48kOverride");
    gc->setLabel(QObject::tr("Force audio device output to 48kHz"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Force audio sample rate to 48kHz. "
                                "Some audio devices will report various rates, "
                                "but they ultimately crash."));
    return gc;
}

HostCheckBox *AudioAdvancedSettings::PassThroughOverride()
{
    HostCheckBox *gc = new HostCheckBox("PassThruDeviceOverride");
    gc->setLabel(QObject::tr("Separate digital output device"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Use a distinct digital output device from "
                                "default."));
    return gc;
}

HostComboBox *AudioAdvancedSettings::PassThroughOutputDevice()
{
    HostComboBox *gc = new HostComboBox("PassThruOutputDevice", true);

    gc->setLabel(QObject::tr("Digital output device"));
    gc->addSelection(QObject::tr("Default"), "Default");
#ifdef USING_MINGW
    gc->addSelection("DirectX:Primary Sound Driver");
#else
    gc->addSelection("ALSA:iec958:{ AES0 0x02 }",
                     "ALSA:iec958:{ AES0 0x02 }");
    gc->addSelection("ALSA:hdmi", "ALSA:hdmi");
    gc->addSelection("ALSA:plughw:0,3", "ALSA:plughw:0,3");
#endif

    gc->setHelpText(QObject::tr("Deprecated. Audio output device to use for "
                        "digital audio. This value is currently only used "
                        "with ALSA and DirectX sound output."));
    return gc;
}

AudioAdvancedSettings::AudioAdvancedSettings(bool mpcm)
{
    ConfigurationGroup *settings3 =
        new HorizontalConfigurationGroup(false, false);

    m_PassThroughOverride = PassThroughOverride();
    TriggeredItem *sub3 =
        new TriggeredItem(m_PassThroughOverride, PassThroughOutputDevice());
    settings3->addChild(m_PassThroughOverride);
    settings3->addChild(sub3);

    ConfigurationGroup *settings4 =
        new HorizontalConfigurationGroup(false, false);
    Setting *srcqualityoverride = SRCQualityOverride();
    TriggeredItem *sub4 =
        new TriggeredItem(srcqualityoverride, SRCQuality());
    settings4->addChild(srcqualityoverride);
    settings4->addChild(sub4);

    ConfigurationGroup *settings5 =
        new HorizontalConfigurationGroup(false, false);
    settings5->addChild(Audio48kOverride());

    ConfigurationGroup *settings6;

    if (mpcm)
    {
        settings6 = new HorizontalConfigurationGroup(false, false);
        settings6->addChild(MPCM());
    }
    
    addChild(settings4);
    addChild(settings5);
    addChild(settings3);
    if (mpcm)
    {
        addChild(settings6);
    }
}

AudioAdvancedSettingsGroup::AudioAdvancedSettingsGroup(bool mpcm)
{
    addChild(new AudioAdvancedSettings(mpcm));
}

// vim:set sw=4 ts=4 expandtab:
