
#ifndef MYTHUIFILEBROWSER_H_
#define MYTHUIFILEBROWSER_H_

// C++ headers
#include <utility>

// QT headers
#include <QDir>
#include <QEvent>
#include <QFileInfo>

#include "mythscreentype.h"
#include "mythuitextedit.h"

class MythUIButtonListItem;
class MythUIButtonList;
class MythUIButton;
class MythUITextEdit;
class MythUIImage;
class MythUIStateType;

class MUI_PUBLIC MFileInfo : public QFileInfo
{
  public:
    explicit MFileInfo(const QString& fileName = "", QString sgDir = "", bool isDir = false,
              qint64 size = 0);
   ~MFileInfo() = default;

    MFileInfo(const MFileInfo &other);
    MFileInfo &operator=(const MFileInfo &other);

    void init(const QString& fileName = "", QString sgDir = "", bool isDir = false,
              qint64 size = 0);

    QString fileName(void) const;
    QString filePath(void) const;
    bool isRemote(void) const { return m_isRemote; }
    bool isDir(void) const;
    bool isFile(void) const;
    bool isParentDir(void) const;
    bool isExecutable(void) const;
    QString absoluteFilePath(void) const;
    qint64 size(void) const;

    void setFile(const QString &file) { init(file); }
    void setSize(qint64 size) { m_size = size; }
    void setIsDir(bool isDir) { m_isDir = isDir; m_isFile = !isDir; }
    void setIsFile(bool isFile) { m_isFile = isFile; m_isDir = !isFile; }
    void setIsParentDir(bool isParentDir) { m_isParentDir = isParentDir; }
    void setSGDir(QString sgDir) { m_storageGroupDir = std::move(sgDir); }

    QString hostName(void) const { return m_hostName; }
    QString storageGroup(void) const { return m_storageGroup; }
    QString storageGroupDir(void) const { return m_storageGroupDir; }
    QString subDir(void) const { return m_subDir; }

  private:

    bool m_isRemote           {false};
    bool m_isDir              {false};
    bool m_isFile             {true};
    bool m_isParentDir        {false};

    QString m_hostName;
    QString m_storageGroup;
    QString m_storageGroupDir;
    QString m_fileName;
    QString m_subDir;

    qint64  m_size            {0};
};

Q_DECLARE_METATYPE(MFileInfo);

class MUI_PUBLIC MythUIFileBrowser : public MythScreenType
{
    Q_OBJECT

  public:
    MythUIFileBrowser(MythScreenStack *parent, const QString &startPath);
   ~MythUIFileBrowser() override = default;

    bool Create(void) override; // MythScreenType

    void SetReturnEvent(QObject *retobject, const QString &resultid);

    void SetTypeFilter(QDir::Filters filter) { m_typeFilter = filter; }
    void SetNameFilter(QStringList filter) { m_nameFilter = std::move(filter); }

  private slots:
    void OKPressed(void);
    void cancelPressed(void);
    void backPressed(void);
    void homePressed(void);
    void editLostFocus(void);
    void PathSelected(MythUIButtonListItem *item);
    void PathClicked(MythUIButtonListItem *item);
    void LoadPreview(void);

  private:
    void SetPath(const QString &startPath);
    static bool GetRemoteFileList(const QString &url, const QString &sgDir,
                           QStringList &list);
    void updateFileList(void);
    void updateRemoteFileList(void);
    void updateLocalFileList(void);
    void updateSelectedList(void);
    void updateWidgets(void);

    static bool IsImage(QString extension);
    static QString FormatSize(int64_t size);

    bool               m_isRemote        {false};

    QTimer            *m_previewTimer    {nullptr};

    QString            m_baseDirectory;
    QString            m_subDirectory;
    QString            m_storageGroupDir;
    QString            m_parentDir;
    QString            m_parentSGDir;

    QDir::Filters      m_typeFilter      {QDir::AllDirs | QDir::Drives |
                                          QDir::Files | QDir::Readable |
                                          QDir::Writable | QDir::Executable};
    QStringList        m_nameFilter;

    MythUIButtonList  *m_fileList        {nullptr};
    MythUITextEdit    *m_locationEdit    {nullptr};
    MythUIButton      *m_okButton        {nullptr};
    MythUIButton      *m_cancelButton    {nullptr};
    MythUIButton      *m_backButton      {nullptr};
    MythUIButton      *m_homeButton      {nullptr};
    MythUIImage       *m_previewImage    {nullptr};
    MythUIText        *m_infoText        {nullptr};
    MythUIText        *m_filenameText    {nullptr};
    MythUIText        *m_fullpathText    {nullptr};

    QObject           *m_retObject       {nullptr};
    QString            m_id;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */
