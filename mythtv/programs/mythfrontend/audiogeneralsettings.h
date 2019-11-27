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
    void Load(void) override; // StandardSettingDialog
    void Init(void) override; // StandardSettingDialog
};

class AudioConfigSettings : public GroupSetting
{
    Q_OBJECT

  public:
    AudioConfigSettings();
    void Load() override; // StandardSetting

    using ADCMap = QMap<QString,AudioOutput::AudioDeviceConfig>;

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
    static HostComboBoxSetting *MaxAudioChannels();
    static HostCheckBoxSetting *AudioUpmix();
    static HostComboBoxSetting *AudioUpmixType();
    static HostCheckBoxSetting *AC3PassThrough();
    static HostCheckBoxSetting *DTSPassThrough();
    static HostCheckBoxSetting *EAC3PassThrough();
    static HostCheckBoxSetting *TrueHDPassThrough();
    static HostCheckBoxSetting *DTSHDPassThrough();
    static HostCheckBoxSetting *MythControlsVolume();
    static HostComboBoxSetting *MixerDevice();
    static HostComboBoxSetting *MixerControl();
    static HostSpinBoxSetting  *MixerVolume();
    static HostSpinBoxSetting  *PCMVolume();

    //advanced setting
    static HostCheckBoxSetting *MPCM();
    static HostCheckBoxSetting *SRCQualityOverride();
    static HostComboBoxSetting *SRCQuality();
    static HostCheckBoxSetting *Audio48kOverride();
    static HostCheckBoxSetting *PassThroughOverride();
    static HostComboBoxSetting *PassThroughOutputDevice();
    static HostCheckBoxSetting *SPDIFRateOverride();
    static HostCheckBoxSetting *HBRPassthrough();

    bool                CheckPassthrough();

    AudioDeviceComboBox *m_OutputDevice              {nullptr};
    HostComboBoxSetting *m_MaxAudioChannels          {nullptr};
    HostCheckBoxSetting *m_AudioUpmix                {nullptr};
    HostComboBoxSetting *m_AudioUpmixType            {nullptr};

    // digital settings
    GroupSetting        *m_triggerDigital            {nullptr};
    HostCheckBoxSetting *m_AC3PassThrough            {nullptr};
    HostCheckBoxSetting *m_DTSPassThrough            {nullptr};
    HostCheckBoxSetting *m_EAC3PassThrough           {nullptr};
    HostCheckBoxSetting *m_TrueHDPassThrough         {nullptr};
    HostCheckBoxSetting *m_DTSHDPassThrough          {nullptr};
    //advanced setting
    HostCheckBoxSetting *m_MPCM                      {nullptr};
    HostCheckBoxSetting *m_PassThroughOverride       {nullptr};
    HostComboBoxSetting *m_PassThroughDeviceOverride {nullptr};

    AudioTest           *m_audioTest                 {nullptr};

    ADCMap               audiodevs;
    AudioOutput::ADCVect devices;
    QMutex               slotlock;

    int                  m_maxspeakers               {0};
    QString              m_lastAudioDevice;
    static const char   *MixerControlControls[];
};

class AudioDeviceComboBox : public HostComboBoxSetting
{
    Q_OBJECT
  public:
    explicit AudioDeviceComboBox(AudioConfigSettings*);
    void AudioRescan();

    void edit(MythScreenType * screen) override; // MythUIComboBoxSetting

  private slots:
    void AudioDescriptionHelp(StandardSetting * setting);

  private:
    AudioConfigSettings *m_parent {nullptr};
};

class ChannelChangedEvent : public QEvent
{
  public:
    ChannelChangedEvent(const QString& channame, bool fulltest) :
        QEvent(kEventType), m_channel(channame), m_fulltest(fulltest) {}
    ~ChannelChangedEvent() = default;

    QString m_channel;
    bool    m_fulltest;

    static Type kEventType;
};

class AudioTestThread : public MThread
{
    Q_DECLARE_TR_FUNCTIONS(AudioTestThread);

  public:

    AudioTestThread(QObject *parent, QString main, QString passthrough,
                    int channels, AudioOutputSettings &settings, bool hd);
    ~AudioTestThread();

    void cancel();
    QString result();
    void setChannel(int channel);

  protected:
    void run() override; // MThread

  private:
    QObject                *m_parent      {nullptr};
    AudioOutput            *m_audioOutput {nullptr};
    int                     m_channels;
    QString                 m_device;
    QString                 m_passthrough;
    bool                    m_interrupted {false};
    int                     m_channel     {-1};
    bool                    m_hd          {false};
    int                     m_samplerate  {48000};
    AudioFormat             m_format      {FORMAT_S16};
};

class AudioTest : public GroupSetting
{
    Q_OBJECT
  public:
    AudioTest();
    ~AudioTest();
    void UpdateCapabilities(const QString &main, const QString &passthrough,
                            int channels, const AudioOutputSettings &settings);
    bool event(QEvent *event) override; // QObject

  private:
    int                         m_channels      {2};
    ButtonStandardSetting      *m_frontleft     {nullptr};
    ButtonStandardSetting      *m_frontright    {nullptr};
    ButtonStandardSetting      *m_center        {nullptr};
    ButtonStandardSetting      *m_surroundleft  {nullptr};
    ButtonStandardSetting      *m_surroundright {nullptr};
    ButtonStandardSetting      *m_rearleft      {nullptr};
    ButtonStandardSetting      *m_rearright     {nullptr};
    ButtonStandardSetting      *m_lfe           {nullptr};
    AudioTestThread            *m_at            {nullptr};
    ButtonStandardSetting      *m_startButton   {nullptr};
    TransMythUICheckBoxSetting *m_hd            {nullptr};
    QString                     m_main;
    QString                     m_passthrough;
    AudioOutputSettings         m_settings;
    bool                        m_quality       {false};

  private slots:
    void toggle();
    void togglequality(void);
    void cancelTest();
    void prepareTest();
};
#endif
