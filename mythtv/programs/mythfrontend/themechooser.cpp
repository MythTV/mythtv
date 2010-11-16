
// Theme Chooser headers
#include "themechooser.h"

// Qt headers
#include <QCoreApplication>
#include <QThreadPool>
#include <QRegExp>

// MythTV headers
#include "mythverbose.h"
#include "mythcorecontext.h"
#include "mythcoreutil.h"
#include "remotefile.h"
#include "mythdownloadmanager.h"
#include "programtypes.h"

// LibMythUI headers
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythuiprogressbar.h"
#include "mythdialogbox.h"
#include "mythuibuttonlist.h"
#include "mythscreenstack.h"
#include "mythuistatetype.h"

#define LOC      QString("ThemeChooser: ")
#define LOC_WARN QString("ThemeChooser, Warning: ")
#define LOC_ERR  QString("ThemeChooser, Error: ")

// Use this to determine what directories to look in on the download site
extern const char *myth_source_path;
extern const char *myth_binary_version;

/*!
* \class RemoteFileDownloadThread
*/
class ThemeExtractThread : public QRunnable
{
  public:
    ThemeExtractThread(ThemeChooser *parent,
                       const QString &srcFile, const QString &destDir) :
        m_parent(parent),
        m_srcFile(srcFile),
        m_destDir(destDir)
    {
        m_srcFile.detach();
        m_destDir.detach();
    }

    void run()
    {
        extractZIP(m_srcFile, m_destDir);

        MythEvent *me =
             new MythEvent("THEME_INSTALLED", QStringList(m_srcFile));
        QCoreApplication::postEvent(m_parent, me);
    }

  private:
    ThemeChooser        *m_parent;
    QString              m_srcFile;
    QString              m_destDir;
};


/** \brief Creates a new ThemeChooser Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
ThemeChooser::ThemeChooser(MythScreenStack *parent,
                           const QString name) :
    MythScreenType(parent, name),
    m_fullPreviewShowing(false),
    m_fullPreviewStateType(NULL),
    m_fullScreenName(NULL),
    m_fullScreenPreview(NULL),
    m_includeDownloadableThemes(false),
    m_downloadTheme(NULL),
    m_downloadState(dsIdle),
    m_popupMenu(NULL)
{
    gCoreContext->addListener(this);
}

ThemeChooser::~ThemeChooser()
{
    gCoreContext->removeListener(this);
}

static bool sortThemeNames(const QFileInfo &s1, const QFileInfo &s2)
{
    return s1.fileName().toLower() < s2.fileName().toLower();
}


bool ThemeChooser::Create(void)
{
    // Load the theme for this screen
    if (!LoadWindowFromXML("settings-ui.xml", "themechooser", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_themes, "themes", &err);

    UIUtilW::Assign(this, m_preview, "preview");
    UIUtilW::Assign(this, m_fullPreviewStateType, "fullpreviewstate");

    if (m_fullPreviewStateType)
    {
        MythUIGroup *state =
            dynamic_cast<MythUIGroup*>
                (m_fullPreviewStateType->GetChild("fullscreen"));
        if (state)
        {
            m_fullScreenName =
               dynamic_cast<MythUIText*>(state->GetChild("fullscreenname"));
            m_fullScreenPreview =
               dynamic_cast<MythUIImage*>(state->GetChild("fullscreenpreview"));
        }
    }

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'themechooser'");
        return false;
    }

    connect(m_themes, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(saveAndReload(MythUIButtonListItem*)));
    connect(m_themes, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(itemChanged(MythUIButtonListItem*)));

    BuildFocusList();

    LoadInBackground();

    return true;
}

void ThemeChooser::Load(void)
{
    QString MythVersion = myth_source_path;
    QStringList themesSeen;
    QDir themes(GetConfDir() + "/themes");
    themes.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    if (MythVersion != "trunk")
    {
        MythVersion = myth_binary_version; // Example: 0.25.20101017-1
        MythVersion.replace(QRegExp("\\.[0-9]{8,}.*"), "");
    }

    m_infoList = themes.entryInfoList();

    for( QFileInfoList::iterator it =  m_infoList.begin();
                                 it != m_infoList.end();
                               ++it )
    {
        if (loadThemeInfo(*it))
        {
            themesSeen << (*it).fileName();
            m_themeStatuses[(*it).fileName()] = "default";
        }
    }

    themes.setPath(GetThemesParentDir());
    QFileInfoList sharedThemes = themes.entryInfoList();
    for( QFileInfoList::iterator it =  sharedThemes.begin();
                                 it != sharedThemes.end();
                               ++it )
    {
        if ((!themesSeen.contains((*it).fileName())) &&
            (loadThemeInfo(*it)))
        {
            m_infoList << *it;
            themesSeen << (*it).fileName();
            m_themeStatuses[(*it).fileName()] = "default";
        }
    }

    if (m_includeDownloadableThemes)
    {
        QString site = QString("http://themes.mythtv.org/themes/repository/%1")
                               .arg(MythVersion);
        QString url = site;
        url.append("/themes.zip");
        QString destdir = GetMythUI()->GetThemeCacheDir();
        destdir.append("/themechooser");
        QDir dir;
        dir.mkpath(destdir);
        QString localFile = GetConfDir();
        localFile.append("/tmp/themes.zip");
        bool result = GetMythDownloadManager()->download(url, localFile);

        if (result && extractZIP(localFile, destdir))
        {
            QString themesPath = destdir;
            themesPath.append("/").append(MythVersion);
            themes.setPath(themesPath);

            QFileInfoList downloadableThemes = themes.entryInfoList();
            for( QFileInfoList::iterator it =  downloadableThemes.begin();
                                         it != downloadableThemes.end();
                                       ++it )
            {
                QString dirName = (*it).fileName();
                QString themeName = dirName;
                QString remoteDir = site;
                remoteDir.append("/").append(dirName);
                QString localDir = themes.absolutePath();
                localDir.append("/").append(dirName);

                if (themesSeen.contains(dirName))
                {
                    ThemeInfo remoteTheme((*it).absoluteFilePath());
                    ThemeInfo *localTheme = m_themeNameInfos[dirName];

                    themeName = remoteTheme.GetName();

                    int rmtMaj = remoteTheme.GetMajorVersion();
                    int rmtMin = remoteTheme.GetMinorVersion();
                    int locMaj = localTheme->GetMajorVersion();
                    int locMin = localTheme->GetMinorVersion();

                    if ((rmtMaj > locMaj) ||
                        ((rmtMaj == locMaj) &&
                         (rmtMin > locMin)))
                    {
                        if (loadThemeInfo(*it))
                        {
                            m_infoList << *it;
                            m_themeStatuses[themeName] = "updateavailable";

                            QFileInfo finfo(remoteTheme.GetPreviewPath());
                            GetMythDownloadManager()->queueDownload(
                                remoteDir.append("/").append(finfo.fileName()),
                                localDir.append("/").append(finfo.fileName()),
                                NULL);
                        }
                    }
                    else if ((rmtMaj == locMaj) &&
                             (rmtMin == locMin))
                    {
                        m_themeStatuses[themeName] = "uptodate";
                    }
                }
                else
                {
                    ThemeInfo *remoteTheme = loadThemeInfo(*it);
                    if (remoteTheme)
                    {
                        themeName = remoteTheme->GetName();
                        themesSeen << dirName;
                        m_infoList << *it;
                        m_themeStatuses[themeName] = "updateavailable";

                        QFileInfo finfo(remoteTheme->GetPreviewPath());
                        GetMythDownloadManager()->queueDownload(
                            remoteDir.append("/").append(finfo.fileName()),
                            localDir.append("/").append(finfo.fileName()),
                            NULL);
                    }
                }
            }
        }
        QFile::remove(localFile);
    }

    qSort(m_infoList.begin(), m_infoList.end(), sortThemeNames);
}

void ThemeChooser::Init(void)
{
    QString curTheme = gCoreContext->GetSetting("Theme");
    ThemeInfo *themeinfo = NULL;
    ThemeInfo *curThemeInfo = NULL;
    MythUIButtonListItem *item = NULL;

    m_themes->Reset();
    for( QFileInfoList::iterator it =  m_infoList.begin();
                                 it != m_infoList.end();
                               ++it )
    {
        QFileInfo  &theme = *it;

        if (!m_themeFileNameInfos.contains(theme.filePath()))
            continue;

        themeinfo = m_themeFileNameInfos[theme.filePath()];
        if (!themeinfo)
            continue;

        QString buttonText = QString("%1 %2.%3")
                                .arg(themeinfo->GetName())
                                .arg(themeinfo->GetMajorVersion())
                                .arg(themeinfo->GetMinorVersion());

        item = new MythUIButtonListItem(m_themes, buttonText);
        if (item)
        {
            if (themeinfo->GetDownloadURL().isEmpty())
                item->DisplayState("local", "themelocation");
            else
                item->DisplayState("remote", "themelocation");

            item->DisplayState(themeinfo->GetAspect(), "aspectstate");

            item->DisplayState(m_themeStatuses[themeinfo->GetName()],
                               "themestatus");
            QHash<QString, QString> infomap;
            themeinfo->ToMap(infomap);
            item->SetTextFromMap(infomap);
            item->SetData(qVariantFromValue(themeinfo));

            QString thumbnail = themeinfo->GetPreviewPath();
            QFileInfo fInfo(thumbnail);
            // Downloadable themeinfos have thumbnail copies of their preview images
            if (!themeinfo->GetDownloadURL().isEmpty())
                thumbnail = thumbnail.append(".thumb.jpg");
            item->SetImage(thumbnail);

            if (curTheme == themeinfo->GetDirectoryName())
                curThemeInfo = themeinfo;
        }
        else
            delete item;
    }

    SetFocusWidget(m_themes);

    if (curThemeInfo)
        m_themes->SetValueByData(qVariantFromValue(curThemeInfo));

    MythUIButtonListItem *current = m_themes->GetItemCurrent();
    if (current)
        itemChanged(current);
}

ThemeInfo *ThemeChooser::loadThemeInfo(QFileInfo &theme)
{
    if (theme.fileName() == "default" || theme.fileName() == "default-wide")
        return NULL;

    ThemeInfo *themeinfo = NULL;
    if (theme.exists()) // local directory vs http:// or remote URL
        themeinfo = new ThemeInfo(theme.absoluteFilePath());
    else
        themeinfo = new ThemeInfo(theme.filePath());

    if (!themeinfo)
        return NULL;

    if (themeinfo->GetName().isEmpty() || !(themeinfo->GetType() & THEME_UI))
    {
        delete themeinfo;
        return NULL;
    }

    m_themeFileNameInfos[theme.filePath()] = themeinfo;
    m_themeNameInfos[theme.fileName()] = themeinfo;

    return themeinfo;
}

void ThemeChooser::showPopupMenu(void)
{
    if (m_popupMenu)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString label = tr("Theme Chooser Menu");

    m_popupMenu =
        new MythDialogBox(label, popupStack, "themechoosermenupopup");

    connect(m_popupMenu, SIGNAL(Closed(QString, int)), SLOT(popupClosed(QString, int)));

    if (m_popupMenu->Create())
        popupStack->AddScreen(m_popupMenu);
    else
    {
        delete m_popupMenu;
        m_popupMenu = NULL;
        return;
    }

    m_popupMenu->SetReturnEvent(this, "popupmenu");

    if (m_fullPreviewStateType)
    {
        if (m_fullPreviewShowing)
            m_popupMenu->AddButton(tr("Hide Fullscreen Preview"),
                                   SLOT(toggleFullscreenPreview()));
        else
            m_popupMenu->AddButton(tr("Show Fullscreen Preview"),
                                   SLOT(toggleFullscreenPreview()));
    }

    if (m_includeDownloadableThemes)
        m_popupMenu->AddButton(tr("Hide Downloadable Themes"),
                               SLOT(toggleDownloadableThemes()));
    else
        m_popupMenu->AddButton(tr("Show Downloadable Themes"),
                               SLOT(toggleDownloadableThemes()));

    MythUIButtonListItem *current = m_themes->GetItemCurrent();
    if (current)
    {
        m_popupMenu->AddButton(tr("Select Theme"),
                               SLOT(saveAndReload()));

        ThemeInfo *info = qVariantValue<ThemeInfo *>(current->GetData());
        if (info)
        {
            QString themeDir = GetConfDir() + "/themes/";

            if (info->GetPreviewPath().startsWith(themeDir))
                m_popupMenu->AddButton(tr("Delete Theme"),
                                       SLOT(removeTheme()));
        }
    }
}

void ThemeChooser::popupClosed(QString which, int result)
{
    (void)which;
    (void)result;

    m_popupMenu = NULL;
}

bool ThemeChooser::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Theme Chooser", event, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            showPopupMenu();
        else if (action == "DELETE")
            removeTheme();
        else if ((action == "ESCAPE") &&
                 (m_fullPreviewShowing))
        {
            toggleFullscreenPreview();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ThemeChooser::toggleFullscreenPreview(void)
{
    if (m_fullPreviewStateType)
    {
        if (m_fullPreviewShowing)
        {
            if (m_fullScreenPreview)
                m_fullScreenPreview->Reset();

            if (m_fullScreenName)
                m_fullScreenName->Reset();

            m_fullPreviewStateType->Reset();
            m_fullPreviewShowing = false;
        }
        else
        {
            MythUIButtonListItem *item = m_themes->GetItemCurrent();
            ThemeInfo *info = qVariantValue<ThemeInfo*>(item->GetData());
            if (info)
            {
                if (m_fullScreenPreview)
                {
                    m_fullScreenPreview->SetFilename(info->GetPreviewPath());
                    m_fullScreenPreview->Load();
                }

                if (m_fullScreenName)
                    m_fullScreenName->SetText(info->GetName());

                m_fullPreviewStateType->DisplayState("fullscreen");
                m_fullPreviewShowing = true;
            }
        }
    }
}

void ThemeChooser::toggleDownloadableThemes(void)
{
    m_includeDownloadableThemes = !m_includeDownloadableThemes;
    ReloadInBackground();
}

void ThemeChooser::saveAndReload(void)
{
    MythUIButtonListItem *current = m_themes->GetItemCurrent();
    if (current)
        saveAndReload(current);
}

void ThemeChooser::saveAndReload(MythUIButtonListItem *item)
{
    ThemeInfo *info = qVariantValue<ThemeInfo *>(item->GetData());

    if (!info->GetDownloadURL().isEmpty())
    {
        QFileInfo qfile(info->GetDownloadURL());
        QString baseName = qfile.fileName();

        OpenBusyPopup(tr("Downloading %1 Theme").arg(info->GetName()));
        m_downloadTheme = info;
        m_downloadFile = RemoteDownloadFile(info->GetDownloadURL(),
                                            "Temp", baseName);
        m_downloadState = dsDownloadingOnBackend;
    }
    else
    {
        gCoreContext->SaveSetting("Theme", info->GetDirectoryName());
        GetMythMainWindow()->JumpTo("Reload Theme");
    }
}

void ThemeChooser::itemChanged(MythUIButtonListItem *item)
{
    ThemeInfo *info = qVariantValue<ThemeInfo*>(item->GetData());

    if (!info)
        return;

    QFileInfo preview(info->GetPreviewPath());
    QHash<QString, QString> infomap;
    info->ToMap(infomap);
    SetTextFromMap(infomap);
    if (m_preview)
    {
        if (preview.exists())
        {
            m_preview->SetFilename(info->GetPreviewPath());
            m_preview->Load();
        }
        else
            m_preview->Reset();
    }
    if (m_fullPreviewShowing && m_fullPreviewStateType)
    {
        if (m_fullScreenPreview)
        {
            if (preview.exists())
            {
                m_fullScreenPreview->SetFilename(info->GetPreviewPath());
                m_fullScreenPreview->Load();
            }
            else
                m_fullScreenPreview->Reset();
        }

        if (m_fullScreenName)
            m_fullScreenName->SetText(info->GetName());
    }

    MythUIStateType *themeLocation =
                    dynamic_cast<MythUIStateType*>(GetChild("themelocation"));
    if (themeLocation)
    {
        if (info->GetDownloadURL().isEmpty())
            themeLocation->DisplayState("local");
        else
            themeLocation->DisplayState("remote");
    }

    MythUIStateType *aspectState =
                        dynamic_cast<MythUIStateType*>(GetChild("aspectstate"));
    if (aspectState)
        aspectState->DisplayState(info->GetAspect());
}

void ThemeChooser::updateProgressBar(int bytesReceived,
                                     int bytesTotal)
{
    MythUIProgressBar *progressBar   =
        dynamic_cast<MythUIProgressBar *>(GetChild("downloadprogressbar"));

    if (!progressBar)
        return;

    progressBar->SetUsed(bytesReceived);
    progressBar->SetTotal(bytesTotal);
}

void ThemeChooser::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QStringList tokens = me->Message().split(" ", QString::SkipEmptyParts);

        if (tokens.isEmpty())
            return;

        if (tokens[0] == "DOWNLOAD_FILE")
        {
            QStringList args = me->ExtraDataList();
            if ((m_downloadState == dsIdle) ||
                (tokens.size() != 2) ||
                (!m_downloadTheme) ||
                (args[1] != m_downloadFile))
                return;

            if (tokens[1] == "UPDATE")
            {
                updateProgressBar(args[2].toInt(), args[3].toInt());
            }
            else if (tokens[1] == "FINISHED")
            {
                bool remoteFileIsLocal = false;
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();

                CloseBusyPopup();

                QFileInfo file(m_downloadFile);
                if ((m_downloadState == dsDownloadingOnBackend) &&
                    (m_downloadFile.startsWith("myth://")))
                {
                    // The backend download is finished so start the
                    // frontend download
                    if ((errorCode == 0) &&
                        (fileSize > 0))
                    {
                        m_downloadState = dsDownloadingOnFrontend;
                        QString localFile = GetConfDir() + "/tmp/" +
                            file.fileName();
                        file.setFile(localFile);

                        if (file.exists())
                        {
                            remoteFileIsLocal = true;
                        }
                        else
                        {
                            GetMythDownloadManager()->queueDownload(
                                m_downloadFile, localFile, this);
                            OpenBusyPopup(tr("Copying %1 Theme Package")
                                          .arg(m_downloadTheme->GetName()));
                        }
                        m_downloadFile = localFile;
                    }
                    else
                    {
                        m_downloadState = dsIdle;
                        ShowOkPopup(tr("ERROR downloading theme package on master backend."));
                    }
                }

                if ((m_downloadState == dsDownloadingOnFrontend) &&
                    (file.exists()))
                {
                    // The frontend download is finished
                    if ((errorCode == 0) &&
                        (fileSize > 0))
                    {
                        m_downloadState = dsExtractingTheme;
                        ThemeExtractThread *extractThread =
                            new ThemeExtractThread(this, m_downloadFile,
                                                   GetConfDir() + "/themes");
                        QThreadPool::globalInstance()->start(extractThread);

                        if (!remoteFileIsLocal)
                            RemoteFile::DeleteFile(args[0]);

                        OpenBusyPopup(tr("Installing %1 Theme")
                                      .arg(m_downloadTheme->GetName()));
                    }
                    else
                    {
                        m_downloadState = dsIdle;
                        ShowOkPopup(tr("ERROR downloading theme package from master backend."));
                    }
                }
            }
        }
        else if ((me->Message() == "THEME_INSTALLED") &&
                 (m_downloadTheme) &&
                 (m_downloadState == dsExtractingTheme))
        {
            m_downloadState = dsIdle;
            CloseBusyPopup();
            QStringList args = me->ExtraDataList();
            QFile::remove(args[0]);

            gCoreContext->SaveSetting("Theme", m_downloadTheme->GetDirectoryName());
            GetMythMainWindow()->JumpTo("Reload Theme");
        }
    }
}

void ThemeChooser::removeTheme(void)
{
    MythUIButtonListItem *current = m_themes->GetItemCurrent();
    if (!current)
    {
        ShowOkPopup(tr("Error, no theme selected."));
        return;
    }

    ThemeInfo *info = qVariantValue<ThemeInfo *>(current->GetData());
    if (!info)
    {
        ShowOkPopup(tr("Error, unable to find current theme."));
        return;
    }

    QString themeDir = GetConfDir() + "/themes/";

    if (!info->GetPreviewPath().startsWith(themeDir))
    {
        ShowOkPopup(tr("%1 is not a user-installed theme and can not "
                       "be deleted.").arg(info->GetName()));
        return;
    }

    themeDir.append(info->GetDirectoryName());
    removeThemeDir(themeDir);

    ReloadInBackground();
}

void ThemeChooser::removeThemeDir(const QString &dirname)
{
    QString themeDir = GetConfDir() + "/themes/";

    if (!dirname.startsWith(themeDir))
        return;

    QDir dir(dirname);

    if (!dir.exists())
        return;

    QFileInfoList list = dir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it++);
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isFile() && !fi->isSymLink())
        {
            QFile::remove(fi->absoluteFilePath());
        }
        else if (fi->isDir() && !fi->isSymLink())
        {
            removeThemeDir(fi->absoluteFilePath());
        }
    }

    dir.rmdir(dirname);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

