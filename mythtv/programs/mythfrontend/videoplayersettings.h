#ifndef PLAYERSETTINGS_H
#define PLAYERSETTINGS_H

// MythTV
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythscreentype.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuicheckbox.h"

class PlayerSettings : public MythScreenType
{
  Q_OBJECT

  public:

    explicit PlayerSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~PlayerSettings() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

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

