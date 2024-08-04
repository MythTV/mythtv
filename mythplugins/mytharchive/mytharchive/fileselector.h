#ifndef FILESELECTOR_H_
#define FILESELECTOR_H_

#include <iostream>
#include <utility>

// qt
#include <QKeyEvent>
#include <QString>
#include <QStringList>

// myth
#include <libmythui/mythscreentype.h>

// mytharchive
#include "archiveutil.h"

struct FileData
{
    bool directory   { false };
    bool selected    { false };
    QString filename;
    int64_t size     { 0 };
};

enum FSTYPE : std::uint8_t
{
    FSTYPE_FILELIST = 0,
    FSTYPE_FILE = 1,
    FSTYPE_DIRECTORY = 2
};

class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

class FileSelector : public MythScreenType
{
    Q_OBJECT

  public:
    FileSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList,
                 FSTYPE type, QString startDir, QString filemask)
        : MythScreenType(parent, "FileSelector"),
          m_selectorType(type),
          m_filemask(std::move(filemask)),
          m_curDirectory(std::move(startDir)),
          m_archiveList(archiveList) {}
    ~FileSelector() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

    QString getSelected(void);

  signals:
    void haveResult(bool ok);            // used in FSTYPE_FILELIST mode
    void haveResult(QString filename);   // used in FSTYPE_FILE or FSTYPE_DIRECTORY mode

  protected slots:
    void OKPressed(void);
    void cancelPressed(void);
    void backPressed(void);
    void homePressed(void);
    void itemClicked(MythUIButtonListItem *item);
    void locationEditLostFocus(void);

  protected:
    void updateFileList(void);
    void updateSelectedList(void);
    void updateWidgets(void);
    void wireUpTheme(void);
    void updateScrollArrows(void);

    FSTYPE                m_selectorType;
    QString               m_filemask;
    QString               m_curDirectory;
    QList<FileData *>     m_fileData;
    QStringList           m_selectedList;
    QList<ArchiveItem *> *m_archiveList;
    //
    //  GUI stuff
    //
    MythUIText       *m_titleText      {nullptr};
    MythUIButtonList *m_fileButtonList {nullptr};
    MythUITextEdit   *m_locationEdit   {nullptr};
    MythUIButton     *m_okButton       {nullptr};
    MythUIButton     *m_cancelButton   {nullptr};
    MythUIButton     *m_backButton     {nullptr};
    MythUIButton     *m_homeButton     {nullptr};
};

Q_DECLARE_METATYPE(FileData *)

#endif
