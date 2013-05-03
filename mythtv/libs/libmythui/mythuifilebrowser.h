
#ifndef MYTHUIFILEBROWSER_H_
#define MYTHUIFILEBROWSER_H_

#include <QDir>
#include <QEvent>
#include <QFileInfo>

#include "mythscreentype.h"
#include "mythuitextedit.h"

class QString;
class QStringList;
class QTimer;

class MythUIButtonListItem;
class MythUIButtonList;
class MythUIButton;
class MythUITextEdit;
class MythUIImage;
class MythUIStateType;

class MUI_PUBLIC MFileInfo : public QFileInfo
{
  public:
    MFileInfo(QString fileName = "", QString sgDir = "", bool isDir = false,
              qint64 size = 0);
   ~MFileInfo();

    MFileInfo &operator=(const MFileInfo &fileinfo);

    void init(QString fileName = "", QString sgDir = "", bool isDir = false,
              qint64 size = 0);

    QString fileName(void) const;
    QString filePath(void) const;
    bool isRemote(void) { return m_isRemote; }
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
    void setSGDir(QString sgDir) { m_storageGroupDir = sgDir; }

    QString hostName(void) const { return m_hostName; }
    QString storageGroup(void) const { return m_storageGroup; }
    QString storageGroupDir(void) const { return m_storageGroupDir; }
    QString subDir(void) const { return m_subDir; }

  private:

    bool m_isRemote;
    bool m_isDir;
    bool m_isFile;
    bool m_isParentDir;

    QString m_hostName;
    QString m_storageGroup;
    QString m_storageGroupDir;
    QString m_fileName;
    QString m_subDir;

    qint64  m_size;
};

Q_DECLARE_METATYPE(MFileInfo);

class MUI_PUBLIC MythUIFileBrowser : public MythScreenType
{
    Q_OBJECT

  public:
    MythUIFileBrowser(MythScreenStack *parent, const QString &startPath);
   ~MythUIFileBrowser();

    bool Create(void);

    void SetReturnEvent(QObject *retobject, const QString &resultid);

    void SetTypeFilter(QDir::Filters filter) { m_typeFilter = filter; }
    void SetNameFilter(QStringList filter) { m_nameFilter = filter; }

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
    bool GetRemoteFileList(const QString &url, const QString &sgDir,
                           QStringList &list);
    void updateFileList(void);
    void updateRemoteFileList(void);
    void updateLocalFileList(void);
    void updateSelectedList(void);
    void updateWidgets(void);

    bool IsImage(QString extension);
    QString FormatSize(int size);

    bool               m_isRemote;

    QTimer            *m_previewTimer;

    QString            m_baseDirectory;
    QString            m_subDirectory;
    QString            m_storageGroupDir;
    QString            m_parentDir;
    QString            m_parentSGDir;

    QDir::Filters      m_typeFilter;
    QStringList        m_nameFilter;

    MythUIButtonList  *m_fileList;
    MythUITextEdit    *m_locationEdit;
    MythUIButton      *m_okButton;
    MythUIButton      *m_cancelButton;
    MythUIButton      *m_backButton;
    MythUIButton      *m_homeButton;
    MythUIImage       *m_previewImage;
    MythUIText        *m_infoText;
    MythUIText        *m_filenameText;
    MythUIText        *m_fullpathText;

    QObject           *m_retObject;
    QString            m_id;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */
