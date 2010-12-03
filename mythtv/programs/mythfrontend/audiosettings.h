#ifndef MYTHAUDIOSETTINGS_H
#define MYTHAUDIOSETTINGS_H

#include <QStringList>

#include <QObject>
#include <QMutex>
#include <QThread>

#include "settings.h"
#include "mythcontext.h"

// libmythui
#include <mythuibutton.h>
#include <mythuistatetype.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>

#include "audiooutput.h"

class AudioDeviceComboBox;

class AudioConfigSettings : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    AudioConfigSettings();

    typedef QMap<QString,AudioOutput::AudioDeviceConfig> ADCMap;

    ADCMap &AudioDeviceMap(void) { return audiodevs; };
    AudioOutput::ADCVect &AudioDeviceVect(void) { return devices; };

  private slots:
    void UpdateVisibility(const QString&);
    void UpdateCapabilities(const QString&);
    void AudioRescan();
    void AudioAdvanced();
    void AudioTest();

  private:
    AudioDeviceComboBox  *OutputDevice();
    HostComboBox         *MaxAudioChannels();
    HostCheckBox         *AudioUpmix();
    HostComboBox         *AudioUpmixType();
    HostCheckBox         *AC3PassThrough();
    HostCheckBox         *DTSPassThrough();
    HostCheckBox         *EAC3PassThrough();
    HostCheckBox         *TrueHDPassThrough();
    bool                  CheckPassthrough();

    ConfigurationGroup   *m_cgsettings;
    AudioDeviceComboBox  *m_OutputDevice;
    HostComboBox         *m_MaxAudioChannels;
    HostCheckBox         *m_AudioUpmix;
    HostComboBox         *m_AudioUpmixType;
    TransCheckBoxSetting *m_triggerDigital;
    TransCheckBoxSetting *m_triggerMPCM;
    HostCheckBox         *m_AC3PassThrough;
    HostCheckBox         *m_DTSPassThrough;
    HostCheckBox         *m_EAC3PassThrough;
    HostCheckBox         *m_TrueHDPassThrough;
    ADCMap                audiodevs;
    AudioOutput::ADCVect  devices;
    QMutex                slotlock;
    bool                  m_passthrough8;
};

class AudioDeviceComboBox : public HostComboBox
{
    Q_OBJECT
  public:
    AudioDeviceComboBox(AudioConfigSettings*);
    void AudioRescan();

  private slots:
    void AudioDescriptionHelp(const QString&);

  private:
    AudioConfigSettings *m_parent;
};

class AudioMixerSettings : public TriggeredConfigurationGroup
{
  public:
    AudioMixerSettings();
  private:
    HostCheckBox        *MythControlsVolume();
    HostComboBox        *MixerDevice();
    HostComboBox        *MixerControl();
    HostSlider          *MixerVolume();
    HostSlider          *PCMVolume();
    static const char   *MixerControlControls[];
};

class AudioGeneralSettings : public ConfigurationWizard
{
  public:
    AudioGeneralSettings();
};

class AudioAdvancedSettings : public VerticalConfigurationGroup
{
  public:
    AudioAdvancedSettings(bool mpcm);
    HostCheckBox         *MPCM();
    HostCheckBox         *SRCQualityOverride();
    HostComboBox         *SRCQuality();
    HostCheckBox         *Audio48kOverride();
    HostCheckBox         *PassThroughOverride();
    HostComboBox         *PassThroughOutputDevice();

    TransCheckBoxSetting *m_triggerMPCM;
    HostCheckBox         *m_MPCM;
    HostCheckBox         *m_PassThroughOverride;
};

class AudioAdvancedSettingsGroup : public ConfigurationWizard
{
  public:
    AudioAdvancedSettingsGroup(bool mpcm);
};

class ChannelChangedEvent : public QEvent
{
  public:
    ChannelChangedEvent(QString channame) :
                 QEvent(kEventType),
                 channel(channame) {}
    ~ChannelChangedEvent() {}

    QString channel;

    static Type kEventType;
};

class AudioTestThread : public QThread
{
  public:

    AudioTestThread(QObject *parent, QString main, QString passthrough,
                    int channels);
    ~AudioTestThread();

    void cancel();
    QString result();

  protected:
    void run();

  private:
    QObject              *m_parent;
    AudioOutput          *m_audioOutput;
    int                   m_channels;
    QString               m_device;
    QString               m_passthrough;
    bool                  m_interrupted;
};

class SpeakerTest : public MythScreenType
{
  Q_OBJECT

  public:

    SpeakerTest(MythScreenStack *parent, QString name,
                QString main, QString passthrough, int channels);
    ~SpeakerTest();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *event);

  private:
    MythUIStateType          *m_speakerLocation;
    MythUIButton             *m_testButton;

    AudioTestThread          *m_testSpeakers;

  private slots:
    void toggleTest(void);
};

#endif
