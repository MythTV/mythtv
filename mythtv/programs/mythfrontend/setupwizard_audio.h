#ifndef AUDIOSETUPWIZARD_H
#define AUDIOSETUPWIZARD_H

#include <uitypes.h>
#include <mythwidgets.h>
#include <mythdialogs.h>

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
    MythUICheckBox      *m_hdCheck;
    MythUICheckBox      *m_hdplusCheck;

    MythUIButton        *m_testSpeakerButton;

    MythUIButton        *m_nextButton;
    MythUIButton        *m_prevButton;

  private slots:
    void slotNext(void);
    void slotPrevious(void);

    void toggleSpeakers(void);
};

#endif
