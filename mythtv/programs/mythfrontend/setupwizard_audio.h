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
                     const char *name = 0);
    ~AudioSetupWizard();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void save(void);

  private:
    virtual void Load(void);
    virtual void Init(void);

    QVector<AudioOutput::AudioDeviceConfig> *m_outputlist;
    AudioTestThread     *m_testThread;

    MythScreenType      *m_generalScreen;

    MythUIButtonList    *m_audioDeviceButtonList;
    MythUIButtonList    *m_speakerNumberButtonList;

    MythUICheckBox      *m_dtsCheck;
    MythUICheckBox      *m_ac3Check;
    MythUICheckBox      *m_eac3Check;
    MythUICheckBox      *m_truehdCheck;
    MythUICheckBox      *m_dtshdCheck;

    MythUIButton        *m_testSpeakerButton;

    MythUIButton        *m_nextButton;
    MythUIButton        *m_prevButton;
    int                  m_maxspeakers;
    QString              m_lastAudioDevice;

  private slots:
    AudioOutputSettings UpdateCapabilities(bool restore = true,
                                           bool AC3 = false);
    AudioOutputSettings UpdateCapabilities(MythUIButtonListItem*);
    AudioOutputSettings UpdateCapabilitiesAC3(void);
    void slotNext(void);
    void slotPrevious(void);

    void toggleSpeakers(void);
};

#endif
