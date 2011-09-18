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
#include <mythscreentype.h>

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
    RecordingSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList);

    ~RecordingSelector(void);

    bool Create();
    bool keyPressEvent(QKeyEvent *e);

  signals:
    void haveResult(bool ok);

  public slots:
    void OKPressed(void);
    void cancelPressed(void);

    void showMenu(void);
    void selectAll(void);
    void clearAll(void);

    void setCategory(MythUIButtonListItem *item);
    void titleChanged(MythUIButtonListItem *item);
    void toggleSelected(MythUIButtonListItem *item);

  private:
    void Init(void);
    void updateRecordingList(void);
    void updateSelectedList(void);
    void updateCategorySelector(void);
    void getRecordingList(void);

    QList<ArchiveItem *>        *m_archiveList;
    std::vector<ProgramInfo *>  *m_recordingList;
    QList<ProgramInfo *>         m_selectedList;
    QStringList                  m_categories;

    MythUIButtonList   *m_recordingButtonList;
    MythUIButton       *m_okButton;
    MythUIButton       *m_cancelButton;
    MythUIButtonList   *m_categorySelector;
    MythUIText         *m_titleText;
    MythUIText         *m_datetimeText;
    MythUIText         *m_filesizeText;
    MythUIText         *m_descriptionText;
    MythUIImage        *m_previewImage;
    MythUIImage        *m_cutlistImage;

    friend class GetRecordingListThread;
};

#endif
