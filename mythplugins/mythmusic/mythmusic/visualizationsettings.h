#ifndef VISUALIZATIONSETTINGS_H
#define VISUALIZATIONSETTINGS_H

#include <mythscreentype.h>
#include <mythuispinbox.h>
#include <mythuibutton.h>
#include <mythuicheckbox.h>

class VisualizationSettings : public MythScreenType
{
    Q_OBJECT
public:
    VisualizationSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~VisualizationSettings() = default;

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
