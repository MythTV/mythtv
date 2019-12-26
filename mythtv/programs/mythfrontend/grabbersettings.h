#ifndef GRABBERSETTINGS_H
#define GRABBERSETTINGS_H

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

    explicit GrabberSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~GrabberSettings() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType

    GrabberList m_movieGrabberList;
    GrabberList m_tvGrabberList;
    GrabberList m_gameGrabberList;

    MythUIButtonList   *m_movieGrabberButtonList {nullptr};
    MythUIButtonList   *m_tvGrabberButtonList    {nullptr};
    MythUIButtonList   *m_gameGrabberButtonList  {nullptr};

    MythUICheckBox     *m_dailyUpdatesCheck      {nullptr};

    MythUIButton       *m_okButton               {nullptr};
    MythUIButton       *m_cancelButton           {nullptr};

  private slots:
    void slotSave(void);
};

#endif

