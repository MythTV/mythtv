#ifndef VIDEOMETADATASETTINGS_H
#define VIDEOMETADATASETTINGS_H

// libmythui
#include "mythuispinbox.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuicheckbox.h"
#include "mythscreentype.h"
#include "mythdialogbox.h"

class MetadataSettings : public MythScreenType
{
  Q_OBJECT

  public:

    explicit MetadataSettings(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~MetadataSettings() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    MythUISpinBox      *m_trailerSpin         {nullptr};

    MythUICheckBox     *m_unknownFileCheck    {nullptr};
    MythUICheckBox     *m_autoMetaUpdateCheck {nullptr};
    MythUICheckBox     *m_treeLoadsMetaCheck  {nullptr};
    MythUICheckBox     *m_randomTrailerCheck  {nullptr};

    MythUIButton       *m_okButton            {nullptr};
    MythUIButton       *m_cancelButton        {nullptr};

  private slots:
    void slotSave(void);
    void toggleTrailers(void);
};

#endif

