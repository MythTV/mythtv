#ifndef SELECTDESTINATION_H_
#define SELECTDESTINATION_H_

// qt
#include <QKeyEvent>

// mythtv
#include <mythscreentype.h>

// mytharchive
#include "archiveutil.h"

class MythUIText;
class MythUIButton;
class MythUICheckBox;
class MythUIButtonList;
class MythUITextEdit;
class MythUIButtonListItem;

class SelectDestination : public MythScreenType
{

  Q_OBJECT

  public:
    SelectDestination(MythScreenStack *parent, bool nativeMode, QString name);
    ~SelectDestination(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  public slots:

    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);

    void handleFind(void);
    void filenameEditLostFocus(void);
    void setDestination(MythUIButtonListItem *item);
    void fileFinderClosed(QString filename);

  private:
    void loadConfiguration(void);
    void saveConfiguration(void);

    bool               m_nativeMode;

    ArchiveDestination m_archiveDestination;
    int                m_freeSpace;

    MythUIButton      *m_nextButton;
    MythUIButton      *m_prevButton;
    MythUIButton      *m_cancelButton;

    MythUIButtonList  *m_destinationSelector;
    MythUIText        *m_destinationText;

    MythUIText        *m_freespaceText;

    MythUITextEdit    *m_filenameEdit;
    MythUIButton      *m_findButton;

    MythUICheckBox    *m_createISOCheck;
    MythUICheckBox    *m_doBurnCheck;
    MythUICheckBox    *m_eraseDvdRwCheck;
    MythUIText        *m_createISOText;
    MythUIText        *m_doBurnText;
    MythUIText        *m_eraseDvdRwText;
};

#endif
