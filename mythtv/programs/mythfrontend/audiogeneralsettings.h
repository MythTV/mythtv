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
#include "standardsettings.h"
#include "mthread.h"

class AudioDeviceComboBox;
class AudioTest;

class AudioConfigScreen : public StandardSettingDialog
{
  public:
    AudioConfigScreen(MythScreenStack *parent, const char *name,
                      GroupSetting *groupSetting);
  protected:
    virtual void Load(void);
    virtual void Init(void);
};

class AudioConfigSettings : public GroupSetting
{
    Q_OBJECT

  public:
    AudioConfigSettings();
    virtual void Load();

    typedef QMap<QString,AudioOutput::AudioDeviceConfig> ADCMap;

    ADCMap &AudioDeviceMap(void) { return audiodevs; };
    AudioOutput::ADCVect &AudioDeviceVect(void) { return devices; };

    void CheckConfiguration(void);

  private slots:
    void UpdateVisibility(StandardSetting *);
    AudioOutputSettings UpdateCapabilities(bool restore = true,
                                           bool AC3 = false);
    AudioOutputSettings UpdateCapabilitiesAC3(void);
    void AudioRescan();
    void UpdateAudioTest();

  private:
    void setMPCMEnabled(bool flag);

    AudioDeviceComboBox *OutputDevice();
    HostComboBoxSetting *MaxAudioChannels();
    HostCheckBoxSetting *AudioUpmix();
    HostComboBoxSetting *AudioUpmixType();
    HostCheckBoxSetting *AC3PassThrough();
    HostCheckBoxSetting *DTSPassThrough();
    HostCheckBoxSetting *EAC3PassThrough();
    HostCheckBoxSetting *TrueHDPassThrough();
    HostCheckBoxSetting *DTSHDPassThrough();
    HostCheckBoxSetting *MythControlsVolume();
    HostComboBoxSetting *MixerDevice();
    HostComboBoxSetting *MixerControl();
    HostSpinBoxSetting  *MixerVolume();
    HostSpinBoxSetting  *PCMVolume();

    //advanced setting
    HostCheckBoxSetting *MPCM();
    HostCheckBoxSetting *SRCQualityOverride();
    HostComboBoxSetting *SRCQuality();
    HostCheckBoxSetting *Audio48kOverride();
    HostCheckBoxSetting *PassThroughOverride();
    HostComboBoxSetting *PassThroughOutputDevice();
    HostCheckBoxSetting *SPDIFRateOverride();
    HostCheckBoxSetting *HBRPassthrough();

    bool                CheckPassthrough();

    AudioDeviceComboBox *m_OutputDevice;
    HostComboBoxSetting *m_MaxAudioChannels;
    HostCheckBoxSetting *m_AudioUpmix;
    HostComboBoxSetting *m_AudioUpmixType;

    // digital settings
    GroupSetting        *m_triggerDigital;
    HostCheckBoxSetting *m_AC3PassThrough;
    HostCheckBoxSetting *m_DTSPassThrough;
    HostCheckBoxSetting *m_EAC3PassThrough;
    HostCheckBoxSetting *m_TrueHDPassThrough;
    HostCheckBoxSetting *m_DTSHDPassThrough;
    //advanced setting
    HostCheckBoxSetting       *m_MPCM;
    HostCheckBoxSetting       *m_PassThroughOverride;
    HostComboBoxSetting       *m_PassThroughDeviceOverride;

    AudioTest           *m_audioTest;

    ADCMap               audiodevs;
    AudioOutput::ADCVect devices;
    QMutex               slotlock;

    int                  m_maxspeakers;
    QString              m_lastAudioDevice;
    static const char   *MixerControlControls[];
};

class AudioDeviceComboBox : public HostComboBoxSetting
{
    Q_OBJECT
  public:
    AudioDeviceComboBox(AudioConfigSettings*);
    void AudioRescan();

    virtual void edit(MythScreenType * screen);

  private slots:
    void AudioDescriptionHelp(StandardSetting * setting);

  private:
    AudioConfigSettings *m_parent;
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

class AudioTest : public GroupSetting
{
    Q_OBJECT
  public:
    AudioTest();
    ~AudioTest();
    void UpdateCapabilities(const QString &main, const QString &passthrough,
                            int channels, const AudioOutputSettings &settings);
    bool event(QEvent *event);

  private:
    int                         m_channels;
    ButtonStandardSetting      *m_frontleft;
    ButtonStandardSetting      *m_frontright;
    ButtonStandardSetting      *m_center;
    ButtonStandardSetting      *m_surroundleft;
    ButtonStandardSetting      *m_surroundright;
    ButtonStandardSetting      *m_rearleft;
    ButtonStandardSetting      *m_rearright;
    ButtonStandardSetting      *m_lfe;
    AudioTestThread            *m_at;
    ButtonStandardSetting      *m_startButton;
    TransMythUICheckBoxSetting *m_hd;
    QString                     m_main;
    QString                     m_passthrough;
    AudioOutputSettings         m_settings;
    bool                        m_quality;

  private slots:
    void toggle();
    void togglequality(void);
    void cancelTest();
    void prepareTest();
};
#endif
