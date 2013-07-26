
// Theme Chooser headers
#include "themechooser.h"

// Qt headers
#include <QCoreApplication>
#include <QRunnable>
#include <QRegExp>

// MythTV headers
#include "mythcorecontext.h"
#include "mythcoreutil.h"
#include "mthreadpool.h"
#include "remotefile.h"
#include "mythdownloadmanager.h"
#include "programtypes.h"
#include "mythsystemevent.h"
#include "mythdate.h"
#include "mythversion.h"
#include "mythlogging.h"
#include "storagegroup.h"

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
    m_themes(NULL),
    m_preview(NULL),
    m_fullPreviewShowing(false),
    m_fullPreviewStateType(NULL),
    m_fullScreenName(NULL),
    m_fullScreenPreview(NULL),
    m_refreshDownloadableThemes(false),
    m_downloadTheme(NULL),
    m_downloadState(dsIdle),
    m_popupMenu(NULL)
{
    gCoreContext->addListener(this);

    StorageGroup sgroup("Themes", gCoreContext->GetHostName());
    m_userThemeDir = sgroup.GetFirstDir(true);
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
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot load screen 'themechooser'");
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
    SetBusyPopupMessage(tr("Loading Installed Themes"));

    QString MythVersion = MYTH_SOURCE_PATH;
    QStringList themesSeen;
    QDir themes(m_userThemeDir);
    themes.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    // Treat devel branches as master
    if (MythVersion.startsWith("devel/"))
        MythVersion = "master";

    // FIXME: For now, treat git master the same as svn trunk
    if (MythVersion == "master")
        MythVersion = "trunk";

    if (MythVersion != "trunk")
    {
        MythVersion = MYTH_BINARY_VERSION; // Example: 0.25.20101017-1
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

    QString remoteThemesFile = GetConfDir();
    remoteThemesFile.append("/tmp/themes.zip");
    QString themeSite = QString("%1/%2")
        .arg(gCoreContext->GetSetting("ThemeRepositoryURL",
             "http://themes.mythtv.org/themes/repository")).arg(MythVersion);
    QDir remoteThemesDir(GetMythUI()->GetThemeCacheDir()
                             .append("/themechooser/").append(MythVersion));

    int downloadFailures =
        gCoreContext->GetNumSetting("ThemeInfoDownloadFailures", 0);
    if (QFile::exists(remoteThemesFile))
    {
        QFileInfo finfo(remoteThemesFile);
        if (finfo.lastModified().toUTC() <
            MythDate::current().addSecs(-600))
        {
            LOG(VB_GUI, LOG_INFO, LOC +
                QString("%1 is over 10 minutes old, forcing "
                        "remote theme list download").arg(remoteThemesFile));
            m_refreshDownloadableThemes = true;
        }

        if (!remoteThemesDir.exists())
            m_refreshDownloadableThemes = true;
    }
    else if (downloadFailures < 2) // (and themes.zip does not exist)
    {
        LOG(VB_GUI, LOG_INFO, LOC +
            QString("%1 does not exist, forcing remote theme "
                    "list download").arg(remoteThemesFile));
        m_refreshDownloadableThemes = true;
    }

    if (m_refreshDownloadableThemes)
    {
        SetBusyPopupMessage(tr("Refreshing Downloadable Themes Information"));

        QString url = themeSite;
        url.append("/themes.zip");
        QString destdir = GetMythUI()->GetThemeCacheDir();
        destdir.append("/themechooser");
        QString versiondir = QString("%1/%2").arg(destdir).arg(MythVersion);
        removeThemeDir(versiondir);
        QDir dir;
        dir.mkpath(destdir);
        bool result = GetMythDownloadManager()->download(url, remoteThemesFile);

        SetBusyPopupMessage(tr("Extracting Downloadable Themes Information"));

        if (!result || !extractZIP(remoteThemesFile, destdir))
        {
            QFile::remove(remoteThemesFile);

            downloadFailures++;
            gCoreContext->SaveSetting("ThemeInfoDownloadFailures",
                                      downloadFailures);
        }
    }

    if ((QFile::exists(remoteThemesFile)) &&
        (remoteThemesDir.exists()))
    {
        SetBusyPopupMessage(tr("Loading Downloadable Themes"));

        LOG(VB_GUI, LOG_INFO, LOC +
            QString("%1 and %2 exist, using cached remote themes list")
                .arg(remoteThemesFile).arg(remoteThemesDir.absolutePath()));

        QString themesPath = remoteThemesDir.absolutePath();
        themes.setPath(themesPath);

        QFileInfoList downloadableThemes = themes.entryInfoList();
        for( QFileInfoList::iterator it =  downloadableThemes.begin();
                                     it != downloadableThemes.end();
                                   ++it )
        {
            QString dirName = (*it).fileName();
            QString themeName = dirName;
            QString remoteDir = themeSite;
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

    ResetBusyPopup();

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
            InfoMap infomap;
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
    }

    SetFocusWidget(m_themes);

    if (curThemeInfo)
        m_themes->SetValueByData(qVariantFromValue(curThemeInfo));

    MythUIButtonListItem *current = m_themes->GetItemCurrent();
    if (current)
        itemChanged(current);

    QString testFile = m_userThemeDir + "/.test";
    QFile test(testFile);
    if (test.open(QIODevice::WriteOnly))
        test.remove();
    else
    {
        ShowOkPopup(tr("Error creating test file, %1 themes directory is "
                       "not writable.").arg(m_userThemeDir));
    }
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

    m_popupMenu->AddButton(tr("Refresh Downloadable Themes"),
                           SLOT(refreshDownloadableThemes()));

    MythUIButtonListItem *current = m_themes->GetItemCurrent();
    if (current)
    {
        ThemeInfo *info = current->GetData().value<ThemeInfo *>();

        if (info)
        {
            m_popupMenu->AddButton(tr("Select Theme"),
                                   SLOT(saveAndReload()));

            if (info->GetPreviewPath().startsWith(m_userThemeDir))
                m_popupMenu->AddButton(tr("Delete Theme"),
                                       SLOT(removeTheme()));
        }
    }

    if (gCoreContext->GetNumSetting("ThemeUpdateNofications", 1))
        m_popupMenu->AddButton(tr("Disable Theme Update Notifications"),
                               SLOT(toggleThemeUpdateNotifications()));
    else
        m_popupMenu->AddButton(tr("Enable Theme Update Notifications"),
                               SLOT(toggleThemeUpdateNotifications()));
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
            ThemeInfo *info = item->GetData().value<ThemeInfo*>();
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

void ThemeChooser::toggleThemeUpdateNotifications(void)
{
    if (gCoreContext->GetNumSetting("ThemeUpdateNofications", 1))
        gCoreContext->SaveSettingOnHost("ThemeUpdateNofications", "0", "");
    else
        gCoreContext->SaveSettingOnHost("ThemeUpdateNofications", "1", "");
}

void ThemeChooser::refreshDownloadableThemes(void)
{
    LOG(VB_GUI, LOG_INFO, LOC + "Forcing remote theme list refresh");
    m_refreshDownloadableThemes = true;
    gCoreContext->SaveSetting("ThemeInfoDownloadFailures", 0);
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
    ThemeInfo *info = item->GetData().value<ThemeInfo *>();

    if (!info)
        return;

    if (!info->GetDownloadURL().isEmpty())
    {
        QString testFile = m_userThemeDir + "/.test";
        QFile test(testFile);
        if (test.open(QIODevice::WriteOnly))
            test.remove();
        else
        {
            ShowOkPopup(tr("Unable to install theme, %1 themes directory is "
                           "not writable.").arg(m_userThemeDir));
            return;
        }

        QString downloadURL = info->GetDownloadURL();
        QFileInfo qfile(downloadURL);
        QString baseName = qfile.fileName();

        if (!gCoreContext->GetSetting("ThemeDownloadURL").isEmpty())
        {
            QStringList tokens =
                gCoreContext->GetSetting("ThemeDownloadURL")
                    .split(";", QString::SkipEmptyParts);
            QString origURL = downloadURL;
            downloadURL.replace(tokens[0], tokens[1]);
            LOG(VB_FILE, LOG_WARNING, LOC +
                QString("Theme download URL overridden from %1 to %2.")
                    .arg(origURL).arg(downloadURL));
        }

        OpenBusyPopup(tr("Downloading %1 Theme").arg(info->GetName()));
        m_downloadTheme = info;
#if 0
        m_downloadFile = RemoteDownloadFile(downloadURL,
                                            "Temp", baseName);
        m_downloadState = dsDownloadingOnBackend;
#else
        QString localFile = GetConfDir() + "/tmp/" + baseName;
        GetMythDownloadManager()->queueDownload(downloadURL, localFile, this);
        m_downloadFile = localFile;
        m_downloadState = dsDownloadingOnFrontend;
#endif
    }
    else
    {
        gCoreContext->SaveSetting("Theme", info->GetDirectoryName());
        GetMythMainWindow()->JumpTo("Reload Theme");
    }
}

void ThemeChooser::itemChanged(MythUIButtonListItem *item)
{
    ThemeInfo *info = item->GetData().value<ThemeInfo*>();

    if (!info)
        return;

    QFileInfo preview(info->GetPreviewPath());
    InfoMap infomap;
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
                            m_downloadFile = localFile;
                        }
                        else
                        {
                            GetMythDownloadManager()->queueDownload(
                                m_downloadFile, localFile, this);
                            OpenBusyPopup(tr("Copying %1 Theme Package")
                                          .arg(m_downloadTheme->GetName()));
                            m_downloadFile = localFile;
                            return;
                        }
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
                                                   m_userThemeDir);
                        MThreadPool::globalInstance()->start(
                            extractThread, "ThemeExtract");

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

            QString event = QString("THEME_INSTALLED PATH %1")
                                    .arg(m_userThemeDir +
                                         m_downloadTheme->GetDirectoryName());
            gCoreContext->SendSystemEvent(event);

            gCoreContext->SaveSetting("Theme", m_downloadTheme->GetDirectoryName());

            // Send a message to ourself so we trigger a reload our next chance
            MythEvent *me = new MythEvent("THEME_RELOAD");
            qApp->postEvent(this, me);
        }
        else if ((me->Message() == "THEME_RELOAD") &&
                 (m_downloadState == dsIdle))
        {
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

    ThemeInfo *info = current->GetData().value<ThemeInfo *>();
    if (!info)
    {
        ShowOkPopup(tr("Error, unable to find current theme."));
        return;
    }

    if (!info->GetPreviewPath().startsWith(m_userThemeDir))
    {
        ShowOkPopup(tr("%1 is not a user-installed theme and can not "
                       "be deleted.").arg(info->GetName()));
        return;
    }

    removeThemeDir(m_userThemeDir + info->GetDirectoryName());

    ReloadInBackground();
}

void ThemeChooser::removeThemeDir(const QString &dirname)
{
    if ((!dirname.startsWith(m_userThemeDir)) &&
        (!dirname.startsWith(GetMythUI()->GetThemeCacheDir())))
        return;

    QDir dir(dirname);

    if (!dir.exists())
        return;
    
    dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = dir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it++);
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

//////////////////////////////////////////////////////////////////////////////

ThemeUpdateChecker::ThemeUpdateChecker() :
    m_updateTimer(new QTimer(this))
{
    m_mythVersion = MYTH_SOURCE_PATH;

    // Treat devel branches as master
    if (m_mythVersion.startsWith("devel/"))
        m_mythVersion = "master";

    // FIXME: For now, treat git master the same as svn trunk
    if (m_mythVersion == "master")
        m_mythVersion = "trunk";

    if (m_mythVersion != "trunk")
    {
        m_mythVersion = MYTH_BINARY_VERSION; // Example: 0.25.20101017-1
        m_mythVersion.replace(QRegExp("\\.[0-9]{8,}.*"), "");
    }

    m_infoPackage = gCoreContext->GenMythURL(gCoreContext->GetSetting("MasterServerIP"),
                                             0,
                                             "remotethemes/themes.zip",
                                             "Temp");

    gCoreContext->SaveSetting("ThemeUpdateStatus", "");
                                 
    connect(m_updateTimer, SIGNAL(timeout()), SLOT(checkForUpdate()));
    m_updateTimer->start(60 * 60 * 1000); // Run once an hour

    // Run once 15 seconds from now
    QTimer::singleShot(15 * 1000, this, SLOT(checkForUpdate()));
}

ThemeUpdateChecker::~ThemeUpdateChecker()
{
    if (m_updateTimer)
    {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = NULL;
    }
}

void ThemeUpdateChecker::checkForUpdate(void)
{
    if (GetMythUI()->GetCurrentLocation(false, true) != "mainmenu")
        return;

    if (RemoteFile::Exists(m_infoPackage))
    {
        QString remoteThemeDir =
            gCoreContext->GenMythURL(gCoreContext->GetSetting("MasterServerIP"),
                                     0,
                                     QString("remotethemes/%1/%2")
                                             .arg(m_mythVersion)
                                             .arg(GetMythUI()->GetThemeName()),
                                     "Temp");

        QString infoXML = remoteThemeDir;
        infoXML.append("/themeinfo.xml");

        if (RemoteFile::Exists(infoXML))
        {
            ThemeInfo *remoteTheme = new ThemeInfo(remoteThemeDir);
            if (!remoteTheme)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("ThemeUpdateChecker::checkForUpdate(): "
                            "Unable to create ThemeInfo for %1")
                        .arg(infoXML));
                return;
            }
                
            ThemeInfo *localTheme = new ThemeInfo(GetMythUI()->GetThemeDir());
            if (!localTheme)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "ThemeUpdateChecker::checkForUpdate(): " 
                    "Unable to create ThemeInfo for current theme");
                return;
            }

            int rmtMaj = remoteTheme->GetMajorVersion();
            int rmtMin = remoteTheme->GetMinorVersion();
            int locMaj = localTheme->GetMajorVersion();
            int locMin = localTheme->GetMinorVersion();

            if ((rmtMaj > locMaj) ||
                ((rmtMaj == locMaj) &&
                 (rmtMin > locMin)))
            {
                m_lastKnownThemeVersion =
                    QString("%1-%2.%3").arg(GetMythUI()->GetThemeName())
                                       .arg(rmtMaj).arg(rmtMin);

                QString status = gCoreContext->GetSetting("ThemeUpdateStatus");
                QString currentLocation = GetMythUI()->GetCurrentLocation(false, true);

                if ((!status.startsWith(m_lastKnownThemeVersion)) &&
                    (currentLocation == "mainmenu"))
                {
                    m_currentVersion = QString("%1.%2").arg(locMaj).arg(locMin);
                    m_newVersion = QString("%1.%2").arg(rmtMaj).arg(rmtMin);

                    gCoreContext->SaveSetting("ThemeUpdateStatus",
                                         m_lastKnownThemeVersion + " notified");

                    QString message = tr("Version %1 of the %2 theme is now "
                                         "available in the Theme Chooser.  The "
                                         "currently installed version is %3.")
                                         .arg(m_newVersion)
                                         .arg(GetMythUI()->GetThemeName())
                                         .arg(m_currentVersion);

                    ShowOkPopup(message);
                }
            }
                
            delete remoteTheme;
            delete localTheme;
        }
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

