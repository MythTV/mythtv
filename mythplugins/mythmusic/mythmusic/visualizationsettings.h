#ifndef VISUALIZATIONSETTINGS_H
#define VISUALIZATIONSETTINGS_H

#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuispinbox.h>

class VisualizationSettings : public MythScreenType
{
    Q_OBJECT
public:
    explicit VisualizationSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~VisualizationSettings() override = default;

    bool Create(void) override; // MythScreenType

private:
    MythUICheckBox     *m_changeOnSongChange {nullptr};
    MythUICheckBox     *m_randomizeOrder     {nullptr};
    MythUISpinBox      *m_scaleWidth         {nullptr};
    MythUISpinBox      *m_scaleHeight        {nullptr};
    MythUIButton       *m_saveButton         {nullptr};
    MythUIButton       *m_cancelButton       {nullptr};

private slots:
    void slotSave(void);
};

#endif // VISUALIZATIONSETTINGS_H
