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
    VisualizationSettings(MythScreenStack *parent, const char *name = 0);
    ~VisualizationSettings();

    bool Create(void);

private:
    MythUICheckBox     *m_changeOnSongChange;
    MythUICheckBox     *m_randomizeOrder;
    MythUISpinBox      *m_scaleWidth;
    MythUISpinBox      *m_scaleHeight;
    MythUIButton       *m_saveButton;
    MythUIButton       *m_cancelButton;

private slots:
    void slotSave(void);
};

#endif // VISUALIZATIONSETTINGS_H
