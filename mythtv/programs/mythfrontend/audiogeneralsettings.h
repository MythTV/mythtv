#ifndef MYTHAUDIOSETTINGS_H
#define MYTHAUDIOSETTINGS_H

#include <QStringList>
#include <QObject>
#include <QMutex>

#include "mythuistatetype.h"
#include "mythscreentype.h"
#include "mythdialogbox.h"
#include "mythuibutton.h"
#include "audiooutput.h"
#include "mythcontext.h"
#include "settings.h"
#include "mthread.h"

class AudioDeviceComboBox;

class AudioConfigSettings : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    AudioConfigSettings(ConfigurationWizard *);

    typedef QMap<QString,AudioOutput::AudioDeviceConfig> ADCMap;

    ADCMap &AudioDeviceMap(void) { return audiodevs; };
    AudioOutput::ADCVect &AudioDeviceVect(void) { return devices; };

  private slots:
    void UpdateVisibility(const QString&);
    AudioOutputSettings UpdateCapabilities(const QString&,
                                           bool restore = true,
                                           bool AC3 = false);
    AudioOutputSettings UpdateCapabilitiesAC3(void);
    void AudioRescan();
    void AudioAdvanced();
    void StartAudioTest();

  private:
    AudioDeviceComboBox *OutputDevice();
    HostComboBox        *MaxAudioChannels();
    HostCheckBox        *AudioUpmix();
    HostComboBox        *AudioUpmixType();
    HostCheckBox        *AC3PassThrough();
    HostCheckBox        *DTSPassThrough();
    HostCheckBox        *EAC3PassThrough();
    HostCheckBox        *TrueHDPassThrough();
    HostCheckBox        *DTSHDPassThrough();
    bool                 CheckPassthrough();

    ConfigurationGroup  *m_cgsettings;
    AudioDeviceComboBox *m_OutputDevice;
    HostComboBox        *m_MaxAudioChannels;
    HostCheckBox        *m_AudioUpmix;
    HostComboBox        *m_AudioUpmixType;
    CheckBoxSetting     *m_triggerDigital;
    HostCheckBox        *m_AC3PassThrough;
    HostCheckBox        *m_DTSPassThrough;
    HostCheckBox        *m_EAC3PassThrough;
    HostCheckBox        *m_TrueHDPassThrough;
    HostCheckBox        *m_DTSHDPassThrough;
    ADCMap               audiodevs;
    AudioOutput::ADCVect devices;
    QMutex               slotlock;
    ConfigurationWizard *m_parent;
    int                  m_maxspeakers;
    QString              m_lastAudioDevice;
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
    Q_OBJECT

  public:
    AudioMixerSettings();
  private:
    HostCheckBox       *MythControlsVolume();
    HostComboBox       *MixerDevice();
    HostComboBox       *MixerControl();
    HostSlider         *MixerVolume();
    HostSlider         *PCMVolume();
    static const char  *MixerControlControls[];
};

class AudioGeneralSettings : public ConfigurationWizard
{
  public:
    AudioGeneralSettings();
};

class AudioAdvancedSettings : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    AudioAdvancedSettings(bool mpcm);

    HostCheckBox       *MPCM();
    HostCheckBox       *SRCQualityOverride();
    HostComboBox       *SRCQuality();
    HostCheckBox       *Audio48kOverride();
    HostCheckBox       *PassThroughOverride();
    HostComboBox       *PassThroughOutputDevice();
    HostCheckBox       *SPDIFRateOverride();
    HostCheckBox       *HBRPassthrough();

    HostCheckBox       *m_PassThroughOverride;
};

class AudioAdvancedSettingsGroup : public ConfigurationWizard
{
  public:
    AudioAdvancedSettingsGroup(bool mpcm);
};

class AudioTestGroup : public ConfigurationWizard
{
  public:
    AudioTestGroup(QString main, QString passthrough,
                   int channels, AudioOutputSettings &settings);
};

class ChannelChangedEvent : public QEvent
{
  public:
    ChannelChangedEvent(QString channame, bool fulltest) :
        QEvent(kEventType), channel(channame), fulltest(fulltest) {}
    ~ChannelChangedEvent() {}

    QString channel;
    bool    fulltest;

    static Type kEventType;
};

class AudioTestThread : public MThread
{
    Q_DECLARE_TR_FUNCTIONS(AudioTestThread)

  public:

    AudioTestThread(QObject *parent, QString main, QString passthrough,
                    int channels, AudioOutputSettings &settings, bool hd);
    ~AudioTestThread();

    void cancel();
    QString result();
    void setChannel(int channel);

  protected:
    void run();

  private:
    QObject                *m_parent;
    AudioOutput            *m_audioOutput;
    int                     m_channels;
    QString                 m_device;
    QString                 m_passthrough;
    bool                    m_interrupted;
    int                     m_channel;
    bool                    m_hd;
    int                     m_samplerate;
    AudioFormat             m_format;
};

class AudioTest : public VerticalConfigurationGroup
{
    Q_OBJECT
  public:
    AudioTest(QString main, QString passthrough,
              int channels, AudioOutputSettings &settings);
    ~AudioTest();
    bool event(QEvent *event);

  private:
    int                     m_channels;
    TransButtonSetting     *m_frontleft;
    TransButtonSetting     *m_frontright;
    TransButtonSetting     *m_center;
    TransButtonSetting     *m_surroundleft;
    TransButtonSetting     *m_surroundright;
    TransButtonSetting     *m_rearleft;
    TransButtonSetting     *m_rearright;
    TransButtonSetting     *m_lfe;
    AudioTestThread        *m_at;
    TransButtonSetting     *m_button;
    TransCheckBoxSetting   *m_hd;
    QString                 m_main;
    QString                 m_passthrough;
    AudioOutputSettings     m_settings;
    bool                    m_quality;

  private slots:
    void toggle(QString str);
    void togglequality(void);
};
#endif
