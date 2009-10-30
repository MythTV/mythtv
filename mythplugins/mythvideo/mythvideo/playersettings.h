#ifndef PLAYERSETTINGS_H
#define PLAYERSETTINGS_H

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

// libmythui
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythdialogbox.h>

class PlayerSettings : public MythScreenType
{
  Q_OBJECT

  public:

    PlayerSettings(MythScreenStack *parent, const char *name = 0);
    ~PlayerSettings();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    MythUITextEdit   *m_defaultPlayerEdit;
    MythUITextEdit   *m_dvdPlayerEdit;
    MythUITextEdit   *m_vcdPlayerEdit;
    MythUITextEdit   *m_altPlayerEdit;

    MythUIText       *m_helpText;

    MythUICheckBox   *m_altCheck;

    MythUIButton     *m_okButton;
    MythUIButton     *m_cancelButton;

  private slots:
    void slotSave(void);
    void slotFocusChanged(void);
    void toggleAlt(void);
};

#endif

