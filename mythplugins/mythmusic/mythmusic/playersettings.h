#ifndef PLAYERSETTINGS_H
#define PLAYERSETTINGS_H

#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>

class PlayerSettings : public MythScreenType
{
    Q_OBJECT
public:
    PlayerSettings(MythScreenStack *parent, const char *name = nullptr);
    ~PlayerSettings() = default;

    bool Create(void) override; // MythScreenType

private:
    MythUIButtonList   *m_resumeMode;
    MythUIButtonList   *m_resumeModeEditor;
    MythUIButtonList   *m_resumeModeRadio;
    MythUIButtonList   *m_exitAction;
    MythUIButtonList   *m_jumpAction;
    MythUICheckBox     *m_autoLookupCD;
    MythUICheckBox     *m_autoPlayCD;
    MythUIButton       *m_saveButton;
    MythUIButton       *m_cancelButton;

private slots:
    void slotSave(void);

};

#endif // PLAYERSETTINGS_H
