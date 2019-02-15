#ifndef PLAYERSETTINGS_H
#define PLAYERSETTINGS_H

// libmythui
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuicheckbox.h"
#include "mythscreentype.h"
#include "mythdialogbox.h"

class PlayerSettings : public MythScreenType
{
  Q_OBJECT

  public:

    PlayerSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~PlayerSettings() = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

  private:
    MythUITextEdit   *m_defaultPlayerEdit {nullptr};
    MythUITextEdit   *m_dvdPlayerEdit     {nullptr};
    MythUITextEdit   *m_dvdDriveEdit      {nullptr};
    MythUITextEdit   *m_blurayMountEdit   {nullptr};
    MythUITextEdit   *m_altPlayerEdit     {nullptr};

    MythUIButtonList *m_blurayRegionList  {nullptr};

    MythUICheckBox   *m_altCheck          {nullptr};

    MythUIButton     *m_okButton          {nullptr};
    MythUIButton     *m_cancelButton      {nullptr};

  private slots:
    void slotSave(void);
    void toggleAlt(void);
    void fillRegionList(void);
};

#endif

