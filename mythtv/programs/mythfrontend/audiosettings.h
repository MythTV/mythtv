#ifndef MYTHAUDIOSETTINGS_H
#define MYTHAUDIOSETTINGS_H

#include <QStringList>
#include <QObject>

#include "settings.h"
#include "mythcontext.h"

#include <QMutex>
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

  private:
    AudioDeviceComboBox  *OutputDevice();
    HostComboBox         *MaxAudioChannels();
    HostCheckBox         *AudioUpmix();
    HostComboBox         *AudioUpmixType();
    HostCheckBox         *AC3PassThrough();
    HostCheckBox         *DTSPassThrough();
    HostCheckBox         *MPCM();
    HostCheckBox         *AdvancedAudioSettings();
    HostCheckBox         *SRCQualityOverride();
    HostComboBox         *SRCQuality();
    HostCheckBox         *Audio48kOverride();
    HostCheckBox         *PassThroughOverride();
    HostComboBox         *PassThroughOutputDevice();

    ConfigurationGroup   *m_cgsettings;
    AudioDeviceComboBox  *m_OutputDevice;
    HostComboBox         *m_MaxAudioChannels;
    HostCheckBox         *m_AudioUpmix;
    HostComboBox         *m_AudioUpmixType;
    TransCheckBoxSetting *m_triggerDigital;
    TransCheckBoxSetting *m_triggerMPCM;
    HostCheckBox         *m_AC3PassThrough;
    HostCheckBox         *m_DTSPassThrough;
    HostCheckBox         *m_MPCM;
    HostCheckBox         *m_AdvancedAudioSettings;
    HostCheckBox         *m_PassThroughOverride;
    ADCMap                audiodevs;
    AudioOutput::ADCVect  devices;
    QMutex                slotlock;
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
    HostComboBox        *MixerDevice();
    HostComboBox        *MixerControl();
    HostSlider          *MixerVolume();
    HostSlider          *PCMVolume();
    static const char   *MixerControlControls[];
};

#endif
