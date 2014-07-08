#ifndef GRABBERSETTINGS_H
#define GRABBERSETTINGS_H

#include "uitypes.h"
#include "mythwidgets.h"
#include "mythdialogs.h"

// libmythui
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuicheckbox.h"
#include "mythscreentype.h"
#include "mythdialogbox.h"
#include "metadatagrabber.h"

class MetaGrabberScript;
class GrabberSettings : public MythScreenType
{
  Q_OBJECT

  public:

    GrabberSettings(MythScreenStack *parent, const char *name = 0);
    ~GrabberSettings();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    virtual void Load(void);
    virtual void Init(void);

    GrabberList m_movieGrabberList;
    GrabberList m_tvGrabberList;
    GrabberList m_gameGrabberList;

    MythUIButtonList   *m_movieGrabberButtonList;
    MythUIButtonList   *m_tvGrabberButtonList;
    MythUIButtonList   *m_gameGrabberButtonList;

    MythUICheckBox     *m_dailyUpdatesCheck;

    MythUIButton       *m_okButton;
    MythUIButton       *m_cancelButton;

  private slots:
    void slotSave(void);
};

#endif

