#ifndef SELECTDESTINATION_H_
#define SELECTDESTINATION_H_

// qt
#include <QKeyEvent>

// mythtv
#include <libmythui/mythscreentype.h>

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
    SelectDestination(MythScreenStack *parent, bool nativeMode, const QString& name)
        : MythScreenType(parent, name), m_nativeMode(nativeMode) {};
    ~SelectDestination(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  public slots:

    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);

    void handleFind(void);
    void filenameEditLostFocus(void);
    void setDestination(MythUIButtonListItem *item);
    void fileFinderClosed(const QString& filename);

  private:
    void loadConfiguration(void);
    void saveConfiguration(void);

    bool               m_nativeMode;

    ArchiveDestination m_archiveDestination  {AD_FILE, nullptr, nullptr, 0LL};
    int                m_freeSpace           {0};

    MythUIButton      *m_nextButton          {nullptr};
    MythUIButton      *m_prevButton          {nullptr};
    MythUIButton      *m_cancelButton        {nullptr};

    MythUIButtonList  *m_destinationSelector {nullptr};
    MythUIText        *m_destinationText     {nullptr};

    MythUIText        *m_freespaceText       {nullptr};

    MythUITextEdit    *m_filenameEdit        {nullptr};
    MythUIButton      *m_findButton          {nullptr};

    MythUICheckBox    *m_createISOCheck      {nullptr};
    MythUICheckBox    *m_doBurnCheck         {nullptr};
    MythUICheckBox    *m_eraseDvdRwCheck     {nullptr};
    MythUIText        *m_createISOText       {nullptr};
    MythUIText        *m_doBurnText          {nullptr};
    MythUIText        *m_eraseDvdRwText      {nullptr};
};

#endif
