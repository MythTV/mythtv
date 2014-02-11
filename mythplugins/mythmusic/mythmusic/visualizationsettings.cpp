// Qt
#include <QString>

// MythTV
#include <mythcorecontext.h>

#include "visualizationsettings.h"

VisualizationSettings::VisualizationSettings(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
    m_changeOnSongChange(NULL),
    m_randomizeOrder(NULL),
    m_scaleWidth(NULL),
    m_scaleHeight(NULL),
    m_saveButton(NULL),
    m_cancelButton(NULL)
{
}

VisualizationSettings::~VisualizationSettings()
{

}

bool VisualizationSettings::Create()
{
    bool err = false;

    // Load the theme for this screen
    if (!LoadWindowFromXML("musicsettings-ui.xml", "visualizationsettings", this))
        return false;

    UIUtilE::Assign(this, m_changeOnSongChange, "cycleonsongchange", &err);
    UIUtilE::Assign(this, m_randomizeOrder, "randomizeorder", &err);
    UIUtilE::Assign(this, m_scaleWidth, "scalewidth", &err);
    UIUtilE::Assign(this, m_scaleHeight, "scaleheight", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'visualizationsettings'");
        return false;
    }

    int changeOnSongChange = gCoreContext->GetNumSetting("VisualCycleOnSongChange", 0);
    if (changeOnSongChange == 1)
        m_changeOnSongChange->SetCheckState(MythUIStateType::Full);
    int randomizeorder = gCoreContext->GetNumSetting("VisualRandomize", 0);
    if (randomizeorder == 1)
        m_randomizeOrder->SetCheckState(MythUIStateType::Full);

    m_scaleWidth->SetRange(1,4,1);
    m_scaleWidth->SetValue(gCoreContext->GetNumSetting("VisualScaleWidth"));
    m_scaleHeight->SetRange(1,4,1);
    m_scaleHeight->SetValue(gCoreContext->GetNumSetting("VisualScaleHeight"));

    m_changeOnSongChange->SetHelpText(tr("Change the visualizer when the song changes."));
    m_randomizeOrder->SetHelpText(tr("On changing the visualizer pick a new one at random."));
    m_scaleWidth->SetHelpText(tr("If set to \"2\", visualizations will be "
                 "scaled in half. Currently only used by "
                 "the goom visualization. Reduces CPU load "
                 "on slower machines."));
    m_scaleHeight->SetHelpText(tr("If set to \"2\", visualizations will be "
                 "scaled in half. Currently only used by "
                 "the goom visualization. Reduces CPU load "
                 "on slower machines."));
    m_cancelButton->SetHelpText(tr("Exit without saving settings"));
    m_saveButton->SetHelpText(tr("Save settings and Exit"));

    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    BuildFocusList();

    SetFocusWidget(m_cancelButton);

    return true;
}

void VisualizationSettings::slotSave(void)
{
    int changeOnSongChange = (m_changeOnSongChange->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("VisualCycleOnSongChange", changeOnSongChange);
    int randomizeorder = (m_randomizeOrder->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("VisualRandomize", randomizeorder);

    gCoreContext->SaveSetting("VisualScaleWidth", m_scaleWidth->GetIntValue());
    gCoreContext->SaveSetting("VisualScaleHeight", m_scaleHeight->GetIntValue());

    gCoreContext->dispatch(MythEvent(QString("MUSIC_SETTINGS_CHANGED VISUALIZATION_SETTINGS")));

    Close();
}
