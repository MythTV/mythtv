/*
    recordingselector.h

    header for the recording selector interface screen
*/

#ifndef RECORDINGSELECTOR_H_
#define RECORDINGSELECTOR_H_

// c++
#include <vector>

// qt
#include <QList>
#include <QStringList>

// mythtv
#include <libmythui/mythscreentype.h>

// mytharchive
#include "archiveutil.h"

class ProgramInfo;
class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

class RecordingSelector : public MythScreenType
{

  Q_OBJECT

  public:
    RecordingSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList)
        : MythScreenType(parent, "RecordingSelector"),
          m_archiveList(archiveList) {}
    ~RecordingSelector(void) override;

    bool Create() override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

  signals:
    void haveResult(bool ok);

  public slots:
    void OKPressed(void);
    void cancelPressed(void);

    void ShowMenu(void) override; // MythScreenType
    void selectAll(void);
    void clearAll(void);

    void setCategory(MythUIButtonListItem *item);
    void titleChanged(MythUIButtonListItem *item);
    void toggleSelected(MythUIButtonListItem *item);

  private:
    void Init(void) override; // MythScreenType
    void updateRecordingList(void);
    void updateSelectedList(void);
    void updateCategorySelector(void);
    void getRecordingList(void);

    QList<ArchiveItem *>        *m_archiveList   {nullptr};
    std::vector<ProgramInfo *>  *m_recordingList {nullptr};
    QList<ProgramInfo *>         m_selectedList;
    QStringList                  m_categories;

    MythUIButtonList   *m_recordingButtonList    {nullptr};
    MythUIButton       *m_okButton               {nullptr};
    MythUIButton       *m_cancelButton           {nullptr};
    MythUIButtonList   *m_categorySelector       {nullptr};
    MythUIText         *m_titleText              {nullptr};
    MythUIText         *m_datetimeText           {nullptr};
    MythUIText         *m_filesizeText           {nullptr};
    MythUIText         *m_descriptionText        {nullptr};
    MythUIImage        *m_previewImage           {nullptr};
    MythUIImage        *m_cutlistImage           {nullptr};

    friend class GetRecordingListThread;
};

#endif
