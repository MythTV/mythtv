#ifndef VIDEOSETUPWIZARD_H
#define VIDEOSETUPWIZARD_H

#include <uitypes.h>
#include <mythwidgets.h>
#include <mythdialogs.h>

// Utility headers
#include <videodisplayprofile.h>

// libmythui
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>

class VideoSetupWizard : public MythScreenType
{
  Q_OBJECT

  public:

    VideoSetupWizard(MythScreenStack *parent, MythScreenType *general,
                     MythScreenType *audio, const char *name = 0);
    ~VideoSetupWizard();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void save(void);

  private:
    MythScreenType      *m_generalScreen;
    MythScreenType      *m_audioScreen;

    MythUIButtonList    *m_playbackProfileButtonList;

    MythUIButton        *m_testSDButton;
    MythUIButton        *m_testHDButton;

    MythUIButton        *m_nextButton;
    MythUIButton        *m_prevButton;

    VideoDisplayProfile *m_vdp;

  private slots:
    void slotNext(void);
    void slotPrevious(void);
    void loadData(void);

    void testSDVideo(void);
    void testHDVideo(void);
};

#endif
