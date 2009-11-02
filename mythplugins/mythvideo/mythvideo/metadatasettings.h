#ifndef METADATASETTINGS_H
#define METADATASETTINGS_H

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

// libmythui
#include <libmythui/mythuispinbox.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythdialogbox.h>

class MetadataSettings : public MythScreenType
{
  Q_OBJECT

  public:

    MetadataSettings(MythScreenStack *parent, const char *name = 0);
    ~MetadataSettings();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    MythUIButtonList   *m_movieGrabberButtonList;
    MythUIButtonList   *m_tvGrabberButtonList;
    MythUISpinBox      *m_trailerSpin;

    MythUIText         *m_helpText;

    MythUICheckBox     *m_unknownFileCheck;
    MythUICheckBox     *m_treeLoadsMetaCheck;
    MythUICheckBox     *m_randomTrailerCheck;

    MythUIButton       *m_okButton;
    MythUIButton       *m_cancelButton;

  private slots:
    void slotSave(void);
    void slotFocusChanged(void);
    void toggleTrailers(void);
    void loadData(void);
};

#endif

