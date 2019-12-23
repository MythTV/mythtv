#ifndef AUDIOSETUPWIZARD_H
#define AUDIOSETUPWIZARD_H

// libmythui
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>
#include <audiooutput.h>

class AudioTestThread;

class AudioSetupWizard : public MythScreenType
{
  Q_OBJECT

  public:

    AudioSetupWizard(MythScreenStack *parent, MythScreenType *generalScreen,
                     const char *name = nullptr)
        : MythScreenType(parent, name),
          m_generalScreen(generalScreen) {}
    ~AudioSetupWizard() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    void save(void);

  private:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType

    QVector<AudioOutput::AudioDeviceConfig> *m_outputlist {nullptr};
    AudioTestThread     *m_testThread              {nullptr};

    MythScreenType      *m_generalScreen           {nullptr};

    MythUIButtonList    *m_audioDeviceButtonList   {nullptr};
    MythUIButtonList    *m_speakerNumberButtonList {nullptr};

    MythUICheckBox      *m_dtsCheck                {nullptr};
    MythUICheckBox      *m_ac3Check                {nullptr};
    MythUICheckBox      *m_eac3Check               {nullptr};
    MythUICheckBox      *m_truehdCheck             {nullptr};
    MythUICheckBox      *m_dtshdCheck              {nullptr};

    MythUIButton        *m_testSpeakerButton       {nullptr};

    MythUIButton        *m_nextButton              {nullptr};
    MythUIButton        *m_prevButton              {nullptr};
    int                  m_maxspeakers             {2};
    QString              m_lastAudioDevice;

  private slots:
    AudioOutputSettings UpdateCapabilities(bool restore = true,
                                           bool AC3 = false);
    AudioOutputSettings UpdateCapabilities(MythUIButtonListItem *item);
    AudioOutputSettings UpdateCapabilitiesAC3(void);
    void slotNext(void);
    void slotPrevious(void);

    void toggleSpeakers(void);
};

#endif
