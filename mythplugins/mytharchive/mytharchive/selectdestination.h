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
    SelectDestination(MythScreenStack *parent, QString name);
    ~SelectDestination(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void setSaveFilename(QString filename) {m_saveFilename = filename;}
    void createConfigFile(const QString &filename);

  public slots:

    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);

    void toggleCreateISO(bool state) { m_bCreateISO = state; };
    void toggleDoBurn(bool state) { m_bDoBurn = state; };
    void toggleEraseDvdRw(bool state) { m_bEraseDvdRw = state; };
    void handleFind(void);
    void filenameEditLostFocus(void);
    void setDestination(MythUIButtonListItem *item);

  private:
    MythUIText   *GetMythUIText(const QString &name, bool optional = false);
    MythUIButton *GetMythUIButton(const QString &name, bool optional = false);
    MythUIButtonList *GetMythUIButtonList(const QString &name, bool optional = false);
    MythUICheckBox *GetMythUICheckBox(const QString &name, bool optional = false);
    MythUITextEdit* GetMythUITextEdit(const QString &name, bool optional = false);

    void loadConfiguration(void);
    void saveConfiguration(void);

    ArchiveDestination m_archiveDestination;
    int                m_freeSpace;

    bool               m_bCreateISO;
    bool               m_bDoBurn;
    bool               m_bEraseDvdRw;

    QString            m_saveFilename;

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
