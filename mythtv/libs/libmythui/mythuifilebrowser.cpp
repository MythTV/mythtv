#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "mythlogging.h"

#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuiutils.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuistatetype.h"
#include "mythuifilebrowser.h"
#include "mythcorecontext.h"

/////////////////////////////////////////////////////////////////////
MFileInfo::MFileInfo(QString fileName, QString sgDir, bool isDir, qint64 size)
{
    init(fileName, sgDir, isDir, size);
}

MFileInfo::~MFileInfo()
{
}

void MFileInfo::init(QString fileName, QString sgDir, bool isDir,
                     qint64 size)
{
    m_fileName = fileName;
    m_isRemote = false;
    m_isParentDir = false;

    if (fileName.startsWith("myth://"))
    {
        QUrl qurl(fileName);
        m_hostName = qurl.host();
        m_storageGroup = qurl.userName();
        m_storageGroupDir = sgDir;
        m_subDir = qurl.path();

        if (!qurl.fragment().isEmpty())
            m_subDir += "#" + qurl.fragment();

        if (m_subDir.startsWith("/"))
            m_subDir.remove(0, 1);

        m_isRemote = true;

        m_isDir = isDir;
        m_isFile = !isDir;
        m_size = size;
    }

    if (!fileName.isEmpty())
        QFileInfo::setFile(fileName);
}

MFileInfo &MFileInfo::operator=(const MFileInfo &other)
{
    QString sgDir = other.storageGroupDir();
    bool isDir    = other.isDir();
    qint64 size   = other.size();
    init(other.fileName(), sgDir, isDir, size);

    return *this;
}

QString MFileInfo::fileName(void) const
{
    if (m_isRemote)
        return m_fileName;
    else
        return QFileInfo::fileName();
}

QString MFileInfo::filePath(void) const
{
    if (m_isRemote)
        return m_fileName;
    else
        return QFileInfo::filePath();
}

bool MFileInfo::isDir(void) const
{
    if (m_isRemote)
        return m_isDir;
    else
        return QFileInfo::isDir();
}

bool MFileInfo::isFile(void) const
{
    if (m_isRemote)
        return m_isFile;
    else
        return QFileInfo::isFile();
}

bool MFileInfo::isParentDir(void) const
{
    if (m_isRemote)
        return m_isParentDir;
    else
        return (QFileInfo::fileName() == "..");
}

bool MFileInfo::isExecutable(void) const
{
    if (m_isRemote)
        return false;
    else
        return QFileInfo::isExecutable();
}

QString MFileInfo::absoluteFilePath(void) const
{
    if (m_isRemote)
        return m_fileName;
    else
        return QFileInfo::absoluteFilePath();
}

qint64 MFileInfo::size(void) const
{
    if (m_isRemote)
        return m_size;
    else
        return QFileInfo::size();
}

/////////////////////////////////////////////////////////////////////

/** \fn MythUIFileBrowser::MythUIFileBrowser(MythScreenStack*, const QString&)
 *  \brief Browse a local filesystem or remote Storage Group
 *         Returns the selected file. Includes previews of images and
 *         file metadata.
 *
 */

MythUIFileBrowser::MythUIFileBrowser(MythScreenStack *parent,
                                     const QString &startPath)
    : MythScreenType(parent, "mythuifilebrowser")
{
    m_retObject = NULL;

    SetPath(startPath);

    m_typeFilter = (QDir::AllDirs | QDir::Drives | QDir::Files |
                    QDir::Readable | QDir::Writable | QDir::Executable);
    m_nameFilter.clear();
    m_nameFilter << "*";

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    connect(m_previewTimer, SIGNAL(timeout()), SLOT(LoadPreview()));
}

MythUIFileBrowser::~MythUIFileBrowser()
{
}

void MythUIFileBrowser::SetPath(const QString &startPath)
{
    if (startPath.startsWith("myth://"))
    {
        m_isRemote = true;

        QUrl qurl(startPath);

        if (!qurl.path().isEmpty())
        {
            // Force browing of remote SG's to start at their root
            m_baseDirectory = gCoreContext->GenMythURL(qurl.host(),
                                                       0,
                                                       "",
                                                       qurl.userName());

        }
        else
        {
            m_baseDirectory = startPath;

            if (m_baseDirectory.endsWith("/"))
                m_baseDirectory.remove(m_baseDirectory.length() - 1, 1);
        }

        m_subDirectory = "";
        m_storageGroupDir = "";
    }
    else
    {
        m_isRemote = false;
        m_baseDirectory = "";
        m_subDirectory = startPath;
    }
}

bool MythUIFileBrowser::Create()
{
    if (!CopyWindowFromBase("MythFileBrowser", this))
        return false;

    m_fileList = dynamic_cast<MythUIButtonList *>(GetChild("filelist"));
    m_locationEdit = dynamic_cast<MythUITextEdit *>(GetChild("location"));
    m_okButton = dynamic_cast<MythUIButton *>(GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *>(GetChild("cancel"));
    m_backButton = dynamic_cast<MythUIButton *>(GetChild("back"));
    m_homeButton = dynamic_cast<MythUIButton *>(GetChild("home"));
    m_previewImage = dynamic_cast<MythUIImage *>(GetChild("preview"));
    m_infoText = dynamic_cast<MythUIText *>(GetChild("info"));
    m_filenameText = dynamic_cast<MythUIText *>(GetChild("filename"));
    m_fullpathText = dynamic_cast<MythUIText *>(GetChild("fullpath"));

    if (!m_fileList || !m_locationEdit || !m_okButton || !m_cancelButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "MythUIFileBrowser: Your theme is missing"
            " some UI elements! Bailing out.");
        return false;
    }

    connect(m_fileList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(PathClicked(MythUIButtonListItem *)));
    connect(m_fileList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(PathSelected(MythUIButtonListItem *)));
    connect(m_locationEdit, SIGNAL(LosingFocus()), SLOT(editLostFocus()));
    connect(m_okButton, SIGNAL(Clicked()), SLOT(OKPressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(cancelPressed()));

    if (m_backButton)
        connect(m_backButton, SIGNAL(Clicked()), SLOT(backPressed()));

    if (m_homeButton)
        connect(m_homeButton, SIGNAL(Clicked()), SLOT(homePressed()));

    BuildFocusList();
    updateFileList();

    return true;
}

void MythUIFileBrowser::SetReturnEvent(QObject *retobject,
                                       const QString &resultid)
{
    m_retObject = retobject;
    m_id = resultid;
}

void MythUIFileBrowser::LoadPreview()
{
    if (m_previewImage)
        m_previewImage->Load();
}

void MythUIFileBrowser::PathSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_previewImage)
        m_previewImage->Reset();

    MFileInfo finfo = qVariantValue<MFileInfo>(item->GetData());

    if (finfo.isParentDir())
    {
        if (m_infoText)
            m_infoText->Reset();

        if (m_filenameText)
            m_filenameText->Reset();

        if (m_fullpathText)
            m_fullpathText->Reset();
    }
    else
    {
        if (IsImage(finfo.suffix()) && m_previewImage)
        {
            m_previewImage->SetFilename(finfo.absoluteFilePath());
            m_previewTimer->start(250);
        }

        if (m_infoText)
            m_infoText->SetText(FormatSize(finfo.size()));

        if (m_filenameText)
            m_filenameText->SetText(finfo.fileName());

        if (m_fullpathText)
            m_fullpathText->SetText(finfo.absoluteFilePath());
    }
}

void MythUIFileBrowser::PathClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MFileInfo finfo = qVariantValue<MFileInfo>(item->GetData());

    if (finfo.isFile())
    {
        if (m_retObject)
        {
            DialogCompletionEvent *dce =
                new DialogCompletionEvent(m_id, 0, finfo.filePath(),
                                          item->GetData());
            QCoreApplication::postEvent(m_retObject, dce);
        }

        Close();
        return;
    }

    if (!finfo.isDir())
        return;

    if (finfo.isParentDir())
    {
        backPressed();
    }
    else
    {
        if (finfo.isRemote())
        {
            m_subDirectory = finfo.subDir();
            m_storageGroupDir = finfo.storageGroupDir();
        }
        else
        {
            m_subDirectory = finfo.filePath();
            m_storageGroupDir = "";
        }
    }

    updateFileList();
}

bool MythUIFileBrowser::IsImage(QString extension)
{
    if (extension.isEmpty())
        return false;

    extension = extension.toLower();

    QList<QByteArray> formats = QImageReader::supportedImageFormats();

    if (formats.contains(extension.toAscii()))
        return true;

    return false;
}

void MythUIFileBrowser::editLostFocus()
{
    QString newPath = m_locationEdit->GetText();

    SetPath(newPath);

    updateFileList();
}

void MythUIFileBrowser::backPressed()
{
    if (m_isRemote)
    {
        m_subDirectory = m_parentDir;

        if (m_subDirectory.startsWith(m_baseDirectory))
        {
            m_subDirectory.remove(0, m_baseDirectory.length());

            if (m_subDirectory.startsWith("/"))
                m_subDirectory.remove(0, 1);
        }

        m_storageGroupDir = m_parentSGDir;
    }
    else
    {
        // move up one directory
        int pos = m_subDirectory.lastIndexOf('/');

        if (pos > 0)
            m_subDirectory = m_subDirectory.left(pos);
        else
            m_subDirectory = "/";
    }

    updateFileList();
}

void MythUIFileBrowser::homePressed()
{
    if (m_isRemote)
    {
        m_subDirectory = "";
        m_storageGroupDir = "";
    }
    else
    {
        char *home = getenv("HOME");
        m_subDirectory = home;
    }

    updateFileList();
}

void MythUIFileBrowser::OKPressed()
{
    MythUIButtonListItem *item = m_fileList->GetItemCurrent();
    MFileInfo finfo = qVariantValue<MFileInfo>(item->GetData());

    if (m_retObject)
    {
        QString selectedPath = m_locationEdit->GetText();
        DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, 0,
                                                               selectedPath,
                                                               item->GetData());
        QCoreApplication::postEvent(m_retObject, dce);
    }

    Close();
}

void MythUIFileBrowser::cancelPressed()
{
    Close();
}

void MythUIFileBrowser::updateFileList()
{
    m_fileList->Reset();

    if (m_isRemote)
        updateRemoteFileList();
    else
        updateLocalFileList();
}

void MythUIFileBrowser::updateRemoteFileList()
{
    QStringList sgdirlist;
    QString     sgdir;
    QStringList slist;

    if (!m_baseDirectory.endsWith("/"))
        m_baseDirectory.append("/");

    QString dirURL = QString("%1%2").arg(m_baseDirectory)
                     .arg(m_subDirectory);

    if (!GetRemoteFileList(m_baseDirectory, sgdir, sgdirlist))
    {
        LOG(VB_GENERAL, LOG_ERR, "GetRemoteFileList failed to get "
            "Storage Group dirs");
        return;
    }

    if ((sgdirlist.size() == 1) &&
        (sgdirlist[0].startsWith("sgdir::")))
    {
        QStringList tokens = sgdirlist[0].split("::");

        m_storageGroupDir = tokens[1];
    }

    if (!GetRemoteFileList(dirURL, m_storageGroupDir, slist))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("GetRemoteFileList failed for '%1' in '%2' SG dir")
            .arg(dirURL).arg(m_storageGroupDir));
        return;
    }

    m_locationEdit->SetText(dirURL);

    QString displayName;
    QString dataName;
    QString type;

    if ((sgdirlist.size() > 1 && !m_storageGroupDir.isEmpty()) ||
        (!m_subDirectory.isEmpty()))
    {
        displayName = tr("Parent");
        type = "upfolder";

        m_parentDir = m_baseDirectory;

        if (!m_subDirectory.isEmpty())
        {
            m_parentDir += "/" + m_subDirectory;

            int pos = m_parentDir.lastIndexOf('/');

            if (pos > 0)
                m_parentDir = m_parentDir.left(pos);
        }


        MFileInfo finfo(m_parentDir, m_storageGroupDir, true);
        m_parentSGDir = m_storageGroupDir;

        if (m_subDirectory.isEmpty() && m_parentDir == m_baseDirectory)
        {
            finfo.setSGDir("");
            m_parentSGDir = "";
        }

        MythUIButtonListItem *item = new MythUIButtonListItem(
            m_fileList, displayName,
            qVariantFromValue(finfo));

        item->SetText(QString("0"), "filesize");
        item->SetText(m_parentDir, "fullpath");
        item->DisplayState(type, "nodetype");

        if (m_backButton)
            m_backButton->SetEnabled(true);
    }
    else
    {
        if (m_backButton)
            m_backButton->SetEnabled(false);
    }

    QStringList::const_iterator it = slist.begin();

    while (it != slist.end())
    {
        QStringList tokens = (*it).split("::");

        if (tokens.size() < 2)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("failed to parse '%1'.").arg(*it));
            ++it;
            continue;
        }

        displayName = tokens[1];

        if (tokens[0] == "sgdir")
            dataName = m_baseDirectory;
        else if (m_subDirectory.isEmpty())
            dataName = QString("%1%2").arg(m_baseDirectory)
                       .arg(displayName);
        else
            dataName = QString("%1%2/%3").arg(m_baseDirectory)
                       .arg(m_subDirectory).arg(displayName);

        MFileInfo finfo(dataName, m_storageGroupDir);

        if ((tokens[0] == "dir") &&
            (m_typeFilter & (QDir::Dirs | QDir::AllDirs)))
        {
            type = "folder";
            finfo.setIsDir(true);
            finfo.setSGDir(m_storageGroupDir);
            finfo.setSize(0);
        }
        else if ((tokens[0] == "sgdir") &&
                 (m_typeFilter & (QDir::Dirs | QDir::AllDirs)))
        {
            type = "folder";
            finfo.setIsDir(true);
            finfo.setSGDir(displayName);
            finfo.setSize(0);
        }
        else if ((tokens[0] == "file") &&
                 (m_typeFilter & QDir::Files))
        {
            finfo.setIsDir(false);
            finfo.setSize(tokens[2].toInt());

            if (IsImage(finfo.suffix()))
                type = "image";
            else
                type = "file";
        }
        else
        {
            // unknown type or filtered out
            ++it;
            continue;
        }

        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_fileList, displayName,
                                     qVariantFromValue(finfo));

        if (finfo.size())
            item->SetText(FormatSize(finfo.size()), "filesize");

        if (type == "image")
            item->SetImage(dataName);

        item->SetText(dataName, "fullpath");
        item->DisplayState(type, "nodetype");

        ++it;
    }
}

void MythUIFileBrowser::updateLocalFileList()
{
    QDir d;

    d.setPath(m_subDirectory);
    d.setNameFilters(m_nameFilter);
    d.setFilter(m_typeFilter);
    d.setSorting(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

    if (!d.exists())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MythUIFileBrowser: current directory does not exist!");
        m_locationEdit->SetText("/");
        m_subDirectory = "/";
        d.setPath("/");
    }

    QFileInfoList list = d.entryInfoList();
    bool showBackButton = false;

    if (list.isEmpty())
    {
        MythUIButtonListItem *item = new MythUIButtonListItem(m_fileList,
                                                              tr("Parent Directory"));
        item->DisplayState("upfolder", "nodetype");
    }
    else
    {
        QFileInfoList::const_iterator it = list.begin();
        const QFileInfo *fi;

        while (it != list.end())
        {
            fi = &(*it);
            MFileInfo finfo(fi->filePath());

            if (finfo.fileName() == ".")
            {
                ++it;
                continue;
            }

            QString displayName = finfo.fileName();
            QString type;

            if (displayName == "..")
            {
                if (m_subDirectory.endsWith("/"))
                {
                    ++it;
                    continue;
                }

                displayName = tr("Parent");
                type = "upfolder";
                showBackButton = true;
            }
            else if (finfo.isDir())
            {
                type = "folder";
            }
            else if (finfo.isExecutable())
            {
                type = "executable";
            }
            else if (finfo.isFile())
            {
                type = "file";
            }

            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_fileList, displayName,
                                         qVariantFromValue(finfo));

            if (IsImage(finfo.suffix()))
            {
                item->SetImage(finfo.absoluteFilePath());
                type = "image";
            }

            item->SetText(FormatSize(finfo.size()), "filesize");
            item->SetText(finfo.absoluteFilePath(), "fullpath");
            item->DisplayState(type, "nodetype");

            ++it;
        }
    }

    if (m_backButton)
        m_backButton->SetEnabled(showBackButton);

    m_locationEdit->SetText(m_subDirectory);
}

QString MythUIFileBrowser::FormatSize(int size)
{
    QString filesize("%L1 %2");

    if (size < 1000000)
        filesize = filesize.arg((double)(size / 1000)).arg("KB");
    else if (size < 1000000000)
        filesize = filesize.arg((double)(size / 1000000)).arg("MB");
    else
        filesize = filesize.arg((double)(size / 1000000000)).arg("GB");

    return filesize;
}

bool MythUIFileBrowser::GetRemoteFileList(const QString &url,
                                          const QString &sgDir,
                                          QStringList &list)
{
    QUrl qurl(url);
    QString storageGroup = qurl.userName();

    list.clear();

    if (storageGroup.isEmpty())
        storageGroup = "Default";

    list << "QUERY_SG_GETFILELIST";
    list << qurl.host();
    list << storageGroup;

    QString path = sgDir + qurl.path();

    if (!qurl.fragment().isEmpty())
        path += "#" + qurl.fragment();

    list << path;
    list << "0";

    bool ok = gCoreContext->SendReceiveStringList(list);

    if ((list.size() == 1) && (list[0] == "EMPTY LIST"))
        list.clear();

    return ok;

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
