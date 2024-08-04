#ifndef EXPORTNATIVE_H_
#define EXPORTNATIVE_H_

// c++
#include <vector>

// qt
#include <QKeyEvent>

// mythtv
#include <libmythui/mythscreentype.h>

// mytharchive
#include "archiveutil.h"

enum NativeItemType : std::uint8_t
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
                 ArchiveDestination &archiveDestination, const QString& name)
        : MythScreenType(parent, name),
          m_previousScreen(previousScreen),
          m_archiveDestination(archiveDestination) {}
    ~ExportNative(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    void createConfigFile(const QString &filename);

  public slots:

    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);
    void handleAddRecording(void);
    void handleAddVideo(void);

    void titleChanged(MythUIButtonListItem *item);
    void ShowMenu(void) override; // MythScreenType
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

    MythScreenType    *m_previousScreen     {nullptr};

    ArchiveDestination m_archiveDestination;
    uint               m_usedSpace          {0};

    QList<ArchiveItem *> m_archiveList;

    bool    m_bCreateISO                    {false};
    bool    m_bDoBurn                       {false};
    bool    m_bEraseDvdRw                   {false};
    QString m_saveFilename;

    MythUIButtonList  *m_archiveButtonList  {nullptr};
    MythUIButton      *m_nextButton         {nullptr};
    MythUIButton      *m_prevButton         {nullptr};
    MythUIButton      *m_cancelButton       {nullptr};
    MythUIButton      *m_addrecordingButton {nullptr};
    MythUIButton      *m_addvideoButton     {nullptr};
    MythUIText        *m_freespaceText      {nullptr};
    MythUIText        *m_titleText          {nullptr};
    MythUIText        *m_datetimeText       {nullptr};
    MythUIText        *m_descriptionText    {nullptr};
    MythUIText        *m_filesizeText       {nullptr};
    MythUIText        *m_nofilesText        {nullptr};
    MythUIText        *m_maxsizeText        {nullptr};
    MythUIText        *m_minsizeText        {nullptr};
    MythUIText        *m_currsizeText       {nullptr};
    MythUIText        *m_currsizeErrText    {nullptr};
    MythUIProgressBar *m_sizeBar            {nullptr};
};

#endif
