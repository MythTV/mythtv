#ifndef METADATASETTINGS_H
#define METADATASETTINGS_H

#include <uitypes.h>
#include <xmlparse.h>
#include <oldsettings.h>
#include <mythwidgets.h>
#include <mythdialogs.h>

// libmythui
#include <mythuispinbox.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuicheckbox.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>

class MetadataSettings : public MythScreenType
{
  Q_OBJECT

  public:

    MetadataSettings(MythScreenStack *parent, const char *name = 0);
    ~MetadataSettings();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    MythUISpinBox      *m_trailerSpin;

    MythUIText         *m_helpText;

    MythUICheckBox     *m_unknownFileCheck;
    MythUICheckBox     *m_autoMetaUpdateCheck;
    MythUICheckBox     *m_treeLoadsMetaCheck;
    MythUICheckBox     *m_randomTrailerCheck;

    MythUIButton       *m_okButton;
    MythUIButton       *m_cancelButton;

  private slots:
    void slotSave(void);
    void slotFocusChanged(void);
    void toggleTrailers(void);
};

#endif

