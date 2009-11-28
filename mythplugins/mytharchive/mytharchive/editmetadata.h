#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

#include <iostream>

// qt
#include <QKeyEvent>

// myth
#include <mythscreentype.h>

// mytharchive
#include "archiveutil.h"

class MythUITextEdit;
class MythUIButton;

class EditMetadataDialog : public MythScreenType
{

  Q_OBJECT

  public:

    EditMetadataDialog(MythScreenStack *parent, ArchiveItem *source_metadata);
    ~EditMetadataDialog();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *e);

  signals:
    void haveResult(bool ok, ArchiveItem *item);

  public slots:

    void okPressed(void);
    void cancelPressed(void);
  private:
    ArchiveItem    *m_sourceMetadata;

    MythUITextEdit *m_titleEdit;
    MythUITextEdit *m_subtitleEdit;
    MythUITextEdit *m_descriptionEdit;
    MythUITextEdit *m_startdateEdit;
    MythUITextEdit *m_starttimeEdit;
    MythUIButton   *m_cancelButton;
    MythUIButton   *m_okButton;
};

#endif
