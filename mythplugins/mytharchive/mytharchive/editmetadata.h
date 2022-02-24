#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

#include <iostream>

// qt
#include <QKeyEvent>

// myth
#include <libmythui/mythscreentype.h>

// mytharchive
#include "archiveutil.h"

class MythUITextEdit;
class MythUIButton;

class EditMetadataDialog : public MythScreenType
{

  Q_OBJECT

  public:

    EditMetadataDialog(MythScreenStack *parent, ArchiveItem *source_metadata)
        : MythScreenType(parent, "EditMetadataDialog"),
          m_sourceMetadata(source_metadata) {}
    ~EditMetadataDialog() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

  signals:
    void haveResult(bool ok, ArchiveItem *item);

  public slots:

    void okPressed(void);
    void cancelPressed(void);
  private:
    ArchiveItem    *m_sourceMetadata  {nullptr};

    MythUITextEdit *m_titleEdit       {nullptr};
    MythUITextEdit *m_subtitleEdit    {nullptr};
    MythUITextEdit *m_descriptionEdit {nullptr};
    MythUITextEdit *m_startdateEdit   {nullptr};
    MythUITextEdit *m_starttimeEdit   {nullptr};
    MythUIButton   *m_cancelButton    {nullptr};
    MythUIButton   *m_okButton        {nullptr};
};

#endif
