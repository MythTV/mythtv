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
    PlayerSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~PlayerSettings() = default;

    bool Create(void) override; // MythScreenType

private:
    MythUIButtonList   *m_resumeMode       {nullptr};
    MythUIButtonList   *m_resumeModeEditor {nullptr};
    MythUIButtonList   *m_resumeModeRadio  {nullptr};
    MythUIButtonList   *m_exitAction       {nullptr};
    MythUIButtonList   *m_jumpAction       {nullptr};
    MythUICheckBox     *m_autoLookupCD     {nullptr};
    MythUICheckBox     *m_autoPlayCD       {nullptr};
    MythUIButton       *m_saveButton       {nullptr};
    MythUIButton       *m_cancelButton     {nullptr};

private slots:
    void slotSave(void);

};

#endif // PLAYERSETTINGS_H
