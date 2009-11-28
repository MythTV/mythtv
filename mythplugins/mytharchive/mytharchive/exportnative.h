#ifndef EXPORTNATIVE_H_
#define EXPORTNATIVE_H_

// c++
#include <vector>

// qt
#include <QKeyEvent>

// mythtv
#include <mythscreentype.h>

// mytharchive
#include "archiveutil.h"

enum NativeItemType
{
    RT_RECORDING = 1,
    RT_VIDEO,
    RT_FILE
};

class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIProgressBar;

class ExportNative : public MythScreenType
{

  Q_OBJECT

  public:
    ExportNative(MythScreenStack *parent, MythScreenType *previousScreen,
                 ArchiveDestination archiveDestination, QString name);

    ~ExportNative(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void createConfigFile(const QString &filename);

  public slots:

    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);
    void handleAddRecording(void);
    void handleAddVideo(void);

    void titleChanged(MythUIButtonListItem *item);
    void showMenu(void);
    void removeItem(void);
    void selectorClosed(bool ok);

  private:
    void updateArchiveList(void);
    void getArchiveList(void);
    void updateSizeBar(void);
    void loadConfiguration(void);
    void saveConfiguration(void);
    void getArchiveListFromDB(void);
    void runScript();

    MythScreenType    *m_previousScreen;

    ArchiveDestination m_archiveDestination;
    uint               m_usedSpace;

    QList<ArchiveItem *> m_archiveList;

    bool    m_bCreateISO;
    bool    m_bDoBurn;
    bool    m_bEraseDvdRw;
    QString m_saveFilename;

    MythUIButtonList  *m_archiveButtonList;
    MythUIButton      *m_nextButton;
    MythUIButton      *m_prevButton;
    MythUIButton      *m_cancelButton;
    MythUIButton      *m_addrecordingButton;
    MythUIButton      *m_addvideoButton;
    MythUIText        *m_freespaceText;
    MythUIText        *m_titleText;
    MythUIText        *m_datetimeText;
    MythUIText        *m_descriptionText;
    MythUIText        *m_filesizeText;
    MythUIText        *m_nofilesText;
    MythUIText        *m_maxsizeText;
    MythUIText        *m_minsizeText;
    MythUIText        *m_currsizeText;
    MythUIText        *m_currsizeErrText;
    MythUIProgressBar *m_sizeBar;
};

#endif
