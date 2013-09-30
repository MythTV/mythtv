#ifndef GRABBERSETTINGS_H
#define GRABBERSETTINGS_H

// libmythui
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuicheckbox.h"
#include "mythscreentype.h"
#include "mythdialogbox.h"

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

    QList<MetaGrabberScript*> m_movieGrabberList;
    QList<MetaGrabberScript*> m_tvGrabberList;
    QList<MetaGrabberScript*> m_gameGrabberList;

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

