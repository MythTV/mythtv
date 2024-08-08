// C++ headers
#include <chrono>

// Qt headers
#include <QCoreApplication>
#include <QRegularExpression>
#include <QRunnable>

// MythTV headers
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/storagegroup.h"
#include "libmythbase/unziputil.h" // for extractZIP
#include "libmythtv/mythsystemevent.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreenstack.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuigroup.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuiprogressbar.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"

// Theme Chooser headers
#include "themechooser.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
#define capturedView capturedRef
#endif

#define LOC QString("ThemeChooser: ")
#define LOC_WARN QString("ThemeChooser, Warning: ")
#define LOC_ERR QString("ThemeChooser, Error: ")

static const QRegularExpression kVersionDateRE{"\\.[0-9]{8,}.*"};

/*!
* \class RemoteFileDownloadThread
*/
class ThemeExtractThread : public QRunnable
{
public:
    ThemeExtractThread(ThemeChooser *parent,
                       QString srcFile, QString destDir) : m_parent(parent),
                                                           m_srcFile(std::move(srcFile)),
                                                           m_destDir(std::move(destDir)) {}

    void run() override // QRunnable
    {
        extractZIP(m_srcFile, m_destDir);

        auto *me = new MythEvent("THEME_INSTALLED", QStringList(m_srcFile));
        QCoreApplication::postEvent(m_parent, me);
    }

private:
    ThemeChooser *m_parent{nullptr};
    QString m_srcFile;
    QString m_destDir;
};

/** \brief Creates a new ThemeChooser Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
ThemeChooser::ThemeChooser(MythScreenStack *parent,
                           const QString &name) : MythScreenType(parent, name)
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
            dynamic_cast<MythUIGroup *>(m_fullPreviewStateType->GetChild("fullscreen"));
        if (state)
        {
            m_fullScreenName =
                dynamic_cast<MythUIText *>(state->GetChild("fullscreenname"));
            m_fullScreenPreview =
                dynamic_cast<MythUIImage *>(state->GetChild("fullscreenpreview"));
        }
    }

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot load screen 'themechooser'");
        return false;
    }

    connect(m_themes, &MythUIButtonList::itemClicked,
            this, qOverload<MythUIButtonListItem *>(&ThemeChooser::saveAndReload));
    connect(m_themes, &MythUIButtonList::itemSelected,
            this, &ThemeChooser::itemChanged);

    BuildFocusList();

    LoadInBackground();

    return true;
}

void ThemeChooser::Load(void)
{
    SetBusyPopupMessage(tr("Loading Installed Themes"));

    QStringList themesSeen;
    QDir themes(m_userThemeDir);
    themes.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    m_infoList = themes.entryInfoList();

    for (const auto &theme : std::as_const(m_infoList))
    {
        if (loadThemeInfo(theme))
        {
            themesSeen << theme.fileName();
            m_themeStatuses[theme.fileName()] = "default";
        }
    }

    themes.setPath(GetThemesParentDir());
    QFileInfoList sharedThemes = themes.entryInfoList();
    for (const auto &sharedTheme : std::as_const(sharedThemes))
    {
        if ((!themesSeen.contains(sharedTheme.fileName())) &&
            (loadThemeInfo(sharedTheme)))
        {
            m_infoList << sharedTheme;
            themesSeen << sharedTheme.fileName();
            m_themeStatuses[sharedTheme.fileName()] = "default";
        }
    }

    // MYTH_SOURCE_VERSION - examples v29-pre-574-g92517f5, v29-Pre, v29.1-21-ge26a33c
    QString MythVersion(GetMythSourceVersion());
    static const QRegularExpression trunkver{"\\Av[0-9]+-pre.*\\z", QRegularExpression::CaseInsensitiveOption};
    static const QRegularExpression validver{
        "\\Av[0-9]+.*\\z", QRegularExpression::CaseInsensitiveOption};

    auto match = validver.match(MythVersion);
    if (!match.hasMatch())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Invalid MythTV version %1, will use themes from trunk").arg(MythVersion));
        MythVersion = "trunk";
    }
    match = trunkver.match(MythVersion);
    if (match.hasMatch())
        MythVersion = "trunk";

    if (MythVersion == "trunk")
    {
        LoadVersion(MythVersion, themesSeen, true);
        LOG(VB_GUI, LOG_INFO, QString("Loading themes for %1").arg(MythVersion));
    }
    else
    {
        MythVersion = MYTH_BINARY_VERSION; // Example: 29.20161017-1
        // Remove the date part and the rest, eg 29.20161017-1 -> 29
        MythVersion.remove(kVersionDateRE);
        LOG(VB_GUI, LOG_INFO, QString("Loading themes for %1").arg(MythVersion));
        if (LoadVersion(MythVersion, themesSeen, true))
        {

            // If a version of the theme for this tag exists, use it...
            // MYTH_SOURCE_VERSION - examples v29-pre-574-g92517f5, v29-Pre, v29.1-21-ge26a33c
            static const QRegularExpression subexp{"v[0-9]+\\.([0-9]+)-*", QRegularExpression::CaseInsensitiveOption};
            // This captures the subversion, i.e. the number after a dot
            match = subexp.match(GetMythSourceVersion());
            if (match.hasMatch())
            {
                QString subversion;
                LOG(VB_GUI, LOG_INFO, QString("Loading version %1").arg(match.capturedView(1).toInt()));
                for (int idx = match.capturedView(1).toInt(); idx > 0; --idx)
                {
                    subversion = MythVersion + "." + QString::number(idx);
                    LOG(VB_GUI, LOG_INFO, QString("Loading themes for %1").arg(subversion));
                    LoadVersion(subversion, themesSeen, false);
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Failed to match theme for %1").arg(GetMythSourceVersion()));
            }

            ResetBusyPopup();

            std::sort(m_infoList.begin(), m_infoList.end(), sortThemeNames);
        }
        else
        {

            LOG(VB_GENERAL, LOG_INFO, QString("Failed to load themes for %1, trying trunk").arg(MythVersion));
            MythVersion = "trunk";
            if (!LoadVersion(MythVersion, themesSeen, true))
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Failed to load themes for %1").arg(MythVersion));
            }
        }
    }
}

bool ThemeChooser::LoadVersion(const QString &version,
                               QStringList &themesSeen, bool alert_user)
{
    QString remoteThemesFile = GetConfDir();
    remoteThemesFile.append("/tmp/themes.zip");
    QString themeSite = QString("%1/%2")
                            .arg(gCoreContext->GetSetting("ThemeRepositoryURL",
                                                          "http://themes.mythtv.org/themes/repository"),
                                 version);
    QString destdir = GetCacheDir().append("/themechooser");
    QString versiondir = QString("%1/%2").arg(destdir, version);
    QDir remoteThemesDir(versiondir);

    int downloadFailures =
        gCoreContext->GetNumSetting("ThemeInfoDownloadFailures", 0);
    if (QFile::exists(remoteThemesFile))
    {
        QFileInfo finfo(remoteThemesFile);
        if (finfo.lastModified().toUTC() <
            MythDate::current().addSecs(-600))
        {
            LOG(VB_GUI, LOG_INFO, LOC + QString("%1 is over 10 minutes old, forcing "
                                                "remote theme list download")
                                            .arg(remoteThemesFile));
            m_refreshDownloadableThemes = true;
        }

        if (!remoteThemesDir.exists())
        {
            m_refreshDownloadableThemes = true;
        }
    }
    else if (downloadFailures < 2) // (and themes.zip does not exist)
    {
        LOG(VB_GUI, LOG_INFO, LOC + QString("%1 does not exist, forcing remote theme "
                                            "list download")
                                        .arg(remoteThemesFile));
        m_refreshDownloadableThemes = true;
    }

    if (m_refreshDownloadableThemes)
    {
        QFile test(remoteThemesFile);
        if (test.open(QIODevice::WriteOnly))
            test.remove();
        else
        {
            ShowOkPopup(tr("Unable to create '%1'").arg(remoteThemesFile));
            return false;
        }

        SetBusyPopupMessage(tr("Refreshing Downloadable Themes Information"));

        QString url = themeSite;
        url.append("/themes.zip");
        if (!removeThemeDir(versiondir))
            ShowOkPopup(tr("Unable to remove '%1'").arg(versiondir));
        QDir dir;
        if (!dir.mkpath(destdir))
            ShowOkPopup(tr("Unable to create '%1'").arg(destdir));
        bool result = GetMythDownloadManager()->download(url, remoteThemesFile, true);

        LOG(VB_GUI, LOG_INFO, LOC + QString("Downloading '%1' to '%2'").arg(url, remoteThemesFile));

        SetBusyPopupMessage(tr("Extracting Downloadable Themes Information"));

        if (!result || !extractZIP(remoteThemesFile, destdir))
        {
            QFile::remove(remoteThemesFile);

            downloadFailures++;
            gCoreContext->SaveSetting("ThemeInfoDownloadFailures",
                                      downloadFailures);

            if (!result)
            {
                LOG(VB_GUI, LOG_ERR, LOC + QString("Failed to download '%1'").arg(url));
                if (alert_user)
                    ShowOkPopup(tr("Failed to download '%1'").arg(url));
            }
            else
            {
                LOG(VB_GUI, LOG_ERR, LOC + QString("Failed to unzip '%1' to '%2'").arg(remoteThemesFile, destdir));
                if (alert_user)
                    ShowOkPopup(tr("Failed to unzip '%1' to '%2'")
                                    .arg(remoteThemesFile, destdir));
            }
            return false;
        }
        LOG(VB_GUI, LOG_INFO, LOC + QString("Unzipped '%1' to '%2'").arg(remoteThemesFile, destdir));
    }

    QDir themes;
    themes.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    if ((QFile::exists(remoteThemesFile)) &&
        (remoteThemesDir.exists()))
    {
        SetBusyPopupMessage(tr("Loading Downloadable Themes"));

        LOG(VB_GUI, LOG_INFO, LOC + QString("%1 and %2 exist, using cached remote themes list").arg(remoteThemesFile, remoteThemesDir.absolutePath()));

        QString themesPath = remoteThemesDir.absolutePath();
        themes.setPath(themesPath);

        QFileInfoList downloadableThemes = themes.entryInfoList();
        for (const auto &dtheme : std::as_const(downloadableThemes))
        {
            QString dirName = dtheme.fileName();
            QString themeName = dirName;
            QString remoteDir = themeSite;
            remoteDir.append("/").append(dirName);
            QString localDir = themes.absolutePath();
            localDir.append("/").append(dirName);

            ThemeInfo remoteTheme(dtheme.absoluteFilePath());

            if (themesSeen.contains(dirName))
            {
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
                    if (loadThemeInfo(dtheme))
                    {
                        LOG(VB_GUI, LOG_DEBUG, LOC + QString("'%1' old version %2.%3, new version %4.%5").arg(themeName).arg(locMaj).arg(locMin).arg(rmtMaj).arg(rmtMin));

                        m_infoList << dtheme;
                        m_themeStatuses[themeName] = "updateavailable";

                        QFileInfo finfo(remoteTheme.GetPreviewPath());
                        GetMythDownloadManager()->queueDownload(
                            remoteDir.append("/").append(finfo.fileName()),
                            localDir.append("/").append(finfo.fileName()),
                            nullptr);
                    }
                }
                else if ((rmtMaj == locMaj) &&
                         (rmtMin == locMin))
                {
                    LOG(VB_GUI, LOG_DEBUG, LOC + QString("'%1' up to date (%2.%3)").arg(themeName).arg(locMaj).arg(locMin));

                    m_themeStatuses[themeName] = "uptodate";
                }
            }
            else
            {
                LOG(VB_GUI, LOG_DEBUG, LOC + QString("'%1' (%2.%3) available").arg(themeName).arg(remoteTheme.GetMajorVersion()).arg(remoteTheme.GetMinorVersion()));

                ThemeInfo *tmpTheme = loadThemeInfo(dtheme);
                if (tmpTheme)
                {
                    themeName = tmpTheme->GetName();
                    themesSeen << dirName;
                    m_infoList << dtheme;
                    m_themeStatuses[themeName] = "updateavailable";

                    QFileInfo finfo(tmpTheme->GetPreviewPath());
                    GetMythDownloadManager()->queueDownload(
                        remoteDir.append("/").append(finfo.fileName()),
                        localDir.append("/").append(finfo.fileName()),
                        nullptr);
                }
            }
        }
        return true;
    }
    return false;
}

void ThemeChooser::Init(void)
{
    QString curTheme = gCoreContext->GetSetting("Theme");
    ThemeInfo *themeinfo = nullptr;
    ThemeInfo *curThemeInfo = nullptr;
    MythUIButtonListItem *item = nullptr;

    m_themes->Reset();
    for (const auto &theme : std::as_const(m_infoList))
    {
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
            item->SetData(QVariant::fromValue(themeinfo));

            QString thumbnail = themeinfo->GetPreviewPath();
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
        m_themes->SetValueByData(QVariant::fromValue(curThemeInfo));

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
                       "not writable.")
                        .arg(m_userThemeDir));
    }
}

ThemeInfo *ThemeChooser::loadThemeInfo(const QFileInfo &theme)
{
    if (theme.fileName() == "default" || theme.fileName() == "default-wide")
        return nullptr;

    ThemeInfo *themeinfo = nullptr;
    if (theme.exists()) // local directory vs http:// or remote URL
        themeinfo = new ThemeInfo(theme.absoluteFilePath());
    else
        themeinfo = new ThemeInfo(theme.filePath());

    if (!themeinfo)
        return nullptr;

    if (themeinfo->GetName().isEmpty() || ((themeinfo->GetType() & THEME_UI) == 0))
    {
        delete themeinfo;
        return nullptr;
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

    connect(m_popupMenu, &MythDialogBox::Closed, this, &ThemeChooser::popupClosed);

    if (m_popupMenu->Create())
        popupStack->AddScreen(m_popupMenu);
    else
    {
        delete m_popupMenu;
        m_popupMenu = nullptr;
        return;
    }

    m_popupMenu->SetReturnEvent(this, "popupmenu");

    if (m_fullPreviewStateType)
    {
        if (m_fullPreviewShowing)
        {
            m_popupMenu->AddButton(tr("Hide Fullscreen Preview"),
                                   &ThemeChooser::toggleFullscreenPreview);
        }
        else
        {
            m_popupMenu->AddButton(tr("Show Fullscreen Preview"),
                                   &ThemeChooser::toggleFullscreenPreview);
        }
    }

    m_popupMenu->AddButton(tr("Refresh Downloadable Themes"),
                           &ThemeChooser::refreshDownloadableThemes);

    MythUIButtonListItem *current = m_themes->GetItemCurrent();
    if (current)
    {
        auto *info = current->GetData().value<ThemeInfo *>();

        if (info)
        {
            m_popupMenu->AddButton(tr("Select Theme"),
                                   qOverload<>(&ThemeChooser::saveAndReload));

            if (info->GetPreviewPath().startsWith(m_userThemeDir))
                m_popupMenu->AddButton(tr("Delete Theme"),
                                       &ThemeChooser::removeTheme);
        }
    }

    if (gCoreContext->GetBoolSetting("ThemeUpdateNofications", true))
    {
        m_popupMenu->AddButton(tr("Disable Theme Update Notifications"),
                               &ThemeChooser::toggleThemeUpdateNotifications);
    }
    else
    {
        m_popupMenu->AddButton(tr("Enable Theme Update Notifications"),
                               &ThemeChooser::toggleThemeUpdateNotifications);
    }
}

void ThemeChooser::popupClosed([[maybe_unused]] const QString &which,
                               [[maybe_unused]] int result)
{
    m_popupMenu = nullptr;
}

bool ThemeChooser::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Theme Chooser", event, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        const QString& action = actions[i];
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
        {
            handled = false;
        }
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
            auto *info = item->GetData().value<ThemeInfo *>();
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
    if (gCoreContext->GetBoolSetting("ThemeUpdateNofications", true))
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
    auto *info = item->GetData().value<ThemeInfo *>();

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
                           "not writable.")
                            .arg(m_userThemeDir));
            return;
        }

        QString downloadURL = info->GetDownloadURL();
        LOG(VB_FILE, LOG_INFO, QString("Download url is %1").arg(downloadURL));
        QFileInfo qfile(downloadURL);
        QString baseName = qfile.fileName();

        if (!gCoreContext->GetSetting("ThemeDownloadURL").isEmpty())
        {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
            QStringList tokens =
                gCoreContext->GetSetting("ThemeDownloadURL")
                    .split(";", QString::SkipEmptyParts);
#else
            QStringList tokens =
                gCoreContext->GetSetting("ThemeDownloadURL")
                    .split(";", Qt::SkipEmptyParts);
#endif
            QString origURL = downloadURL;
            downloadURL.replace(tokens[0], tokens[1]);
            LOG(VB_FILE, LOG_WARNING, LOC + QString("Theme download URL overridden from %1 to %2.").arg(origURL, downloadURL));
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
    auto *info = item->GetData().value<ThemeInfo *>();

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
        {
            m_preview->Reset();
        }
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
            {
                m_fullScreenPreview->Reset();
            }
        }

        if (m_fullScreenName)
            m_fullScreenName->SetText(info->GetName());
    }

    MythUIStateType *themeLocation =
        dynamic_cast<MythUIStateType *>(GetChild("themelocation"));
    if (themeLocation)
    {
        if (info->GetDownloadURL().isEmpty())
            themeLocation->DisplayState("local");
        else
            themeLocation->DisplayState("remote");
    }

    MythUIStateType *aspectState =
        dynamic_cast<MythUIStateType *>(GetChild("aspectstate"));
    if (aspectState)
        aspectState->DisplayState(info->GetAspect());
}

void ThemeChooser::updateProgressBar(int bytesReceived,
                                     int bytesTotal)
{
    MythUIProgressBar *progressBar =
        dynamic_cast<MythUIProgressBar *>(GetChild("downloadprogressbar"));

    if (!progressBar)
        return;

    progressBar->SetUsed(bytesReceived);
    progressBar->SetTotal(bytesTotal);
}

void ThemeChooser::customEvent(QEvent *e)
{
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(e);
        if (me == nullptr)
            return;

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        QStringList tokens = me->Message().split(" ", QString::SkipEmptyParts);
#else
        QStringList tokens = me->Message().split(" ", Qt::SkipEmptyParts);
#endif

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
                int fileSize = args[2].toInt();
                int errorCode = args[4].toInt();

                CloseBusyPopup();

                QFileInfo file(m_downloadFile);
                if ((m_downloadState == dsDownloadingOnBackend) &&
                    (m_downloadFile.startsWith("myth://")))
                {
                    // The backend download is finished so start the
                    // frontend download
                    LOG(VB_FILE, LOG_INFO, QString("Download done MBE %1 %2").arg(errorCode).arg(fileSize));
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
                    LOG(VB_FILE, LOG_INFO, QString("Download done MFE %1 %2").arg(errorCode).arg(fileSize));
                    // moved error is ok
                    if ((errorCode == 0) &&
                        (fileSize > 0))
                    {
                        m_downloadState = dsExtractingTheme;
                        auto *extractThread =
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
                        ShowOkPopup(tr("ERROR downloading theme package from frontend."));
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

            if (!args.isEmpty() && !args[0].isEmpty())
                QFile::remove(args[0]);

            QString event = QString("THEME_INSTALLED PATH %1")
                                .arg(m_userThemeDir +
                                     m_downloadTheme->GetDirectoryName());
            gCoreContext->SendSystemEvent(event);

            gCoreContext->SaveSetting("Theme", m_downloadTheme->GetDirectoryName());

            // Send a message to ourself so we trigger a reload our next chance
            auto *me2 = new MythEvent("THEME_RELOAD");
            qApp->postEvent(this, me2);
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

    auto *info = current->GetData().value<ThemeInfo *>();
    if (!info)
    {
        ShowOkPopup(tr("Error, unable to find current theme."));
        return;
    }

    if (!info->GetPreviewPath().startsWith(m_userThemeDir))
    {
        ShowOkPopup(tr("%1 is not a user-installed theme and can not "
                       "be deleted.")
                        .arg(info->GetName()));
        return;
    }

    removeThemeDir(m_userThemeDir + info->GetDirectoryName());

    ReloadInBackground();
}

bool ThemeChooser::removeThemeDir(const QString &dirname)
{
    if ((!dirname.startsWith(m_userThemeDir)) &&
        (!dirname.startsWith(GetMythUI()->GetThemeCacheDir())))
        return true;

    QDir dir(dirname);

    if (!dir.exists())
        return true;

    dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = dir.entryInfoList();

    for (const auto &fi : std::as_const(list))
    {
        if (fi.isFile() && !fi.isSymLink())
        {
            if (!QFile::remove(fi.absoluteFilePath()))
                return false;
        }
        else if (fi.isDir() && !fi.isSymLink())
        {
            if (!removeThemeDir(fi.absoluteFilePath()))
                return false;
        }
    }

    return dir.rmdir(dirname);
}

//////////////////////////////////////////////////////////////////////////////

ThemeUpdateChecker::ThemeUpdateChecker(void) : m_updateTimer(new QTimer(this))
{
    QString version = GetMythSourcePath();

    if (!version.isEmpty() && !version.startsWith("fixes/"))
    {
        // Treat devel branches as master
        m_mythVersions << "trunk";
    }
    else
    {
        version = MYTH_BINARY_VERSION; // Example: 0.25.20101017-1
        version.remove(kVersionDateRE);

        // If a version of the theme for this tag exists, use it...
        static const QRegularExpression subexp{"v[0-9]+\\.([0-9]+)-*", QRegularExpression::CaseInsensitiveOption};
        auto match = subexp.match(GetMythSourceVersion());
        if (match.hasMatch())
        {
            for (int idx = match.capturedView(1).toInt(); idx > 0; --idx)
                m_mythVersions << version + "." + QString::number(idx);
        }
        m_mythVersions << version;
    }

    m_infoPackage = MythCoreContext::GenMythURL(gCoreContext->GetMasterHostName(),
                                                MythCoreContext::GetMasterServerPort(),
                                                "remotethemes/themes.zip",
                                                "Temp");

    gCoreContext->SaveSetting("ThemeUpdateStatus", "");

    connect(m_updateTimer, &QTimer::timeout, this, &ThemeUpdateChecker::checkForUpdate);

    if (qEnvironmentVariableIsSet("MYTHTV_DEBUGMDM"))
    {
        LOG(VB_GENERAL, LOG_INFO, "Checking for theme updates every minute");
        m_updateTimer->start(1min);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Checking for theme updates every hour");
        m_updateTimer->start(1h);
    }

    // Run once 15 seconds from now
    QTimer::singleShot(15s, this, &ThemeUpdateChecker::checkForUpdate);
}

ThemeUpdateChecker::~ThemeUpdateChecker()
{
    if (m_updateTimer)
    {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }
}

void ThemeUpdateChecker::checkForUpdate(void)
{
    if (GetMythUI()->GetCurrentLocation(false, true) != "mainmenu")
        return;

    ThemeInfo *localTheme = nullptr;

    if (RemoteFile::Exists(m_infoPackage))
    {
        QStringList::iterator Iversion;

        for (Iversion = m_mythVersions.begin();
             Iversion != m_mythVersions.end(); ++Iversion)
        {

            QString remoteThemeDir =
                MythCoreContext::GenMythURL(gCoreContext->GetMasterHostName(),
                                            MythCoreContext::GetMasterServerPort(),
                                            QString("remotethemes/%1/%2")
                                                .arg(*Iversion,
                                                     GetMythUI()->GetThemeName()),
                                            "Temp");

            QString infoXML = remoteThemeDir;
            infoXML.append("/themeinfo.xml");

            LOG(VB_GUI, LOG_INFO, QString("ThemeUpdateChecker Loading '%1'").arg(infoXML));

            if (RemoteFile::Exists(infoXML))
            {
                int locMaj = 0;
                int locMin = 0;

                auto *remoteTheme = new ThemeInfo(remoteThemeDir);
                if (!remoteTheme || remoteTheme->GetType() & THEME_UNKN)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("ThemeUpdateChecker::checkForUpdate(): "
                                "Unable to create ThemeInfo for %1")
                            .arg(infoXML));
                    delete remoteTheme;
                    remoteTheme = nullptr;
                    return;
                }

                if (!localTheme)
                {
                    localTheme = new ThemeInfo(GetMythUI()->GetThemeDir());
                    if (!localTheme || localTheme->GetType() & THEME_UNKN)
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            "ThemeUpdateChecker::checkForUpdate(): "
                            "Unable to create ThemeInfo for current theme");
                        delete localTheme;
                        localTheme = nullptr;
                        return;
                    }
                    locMaj = localTheme->GetMajorVersion();
                    locMin = localTheme->GetMinorVersion();
                }

                int rmtMaj = remoteTheme->GetMajorVersion();
                int rmtMin = remoteTheme->GetMinorVersion();

                delete remoteTheme;
                remoteTheme = nullptr;

                if ((rmtMaj > locMaj) ||
                    ((rmtMaj == locMaj) &&
                     (rmtMin > locMin)))
                {
                    m_lastKnownThemeVersion =
                        QString("%1-%2.%3").arg(GetMythUI()->GetThemeName()).arg(rmtMaj).arg(rmtMin);

                    QString status = gCoreContext->GetSetting("ThemeUpdateStatus");
                    QString currentLocation = GetMythUI()->GetCurrentLocation(false, true);

                    if ((!status.startsWith(m_lastKnownThemeVersion)) &&
                        (currentLocation == "mainmenu"))
                    {
                        m_currentVersion = QString("%1.%2")
                                               .arg(locMaj)
                                               .arg(locMin);
                        m_newVersion = QString("%1.%2").arg(rmtMaj).arg(rmtMin);

                        gCoreContext->SaveSetting("ThemeUpdateStatus",
                                                  m_lastKnownThemeVersion + " notified");

                        QString message = tr("Version %1 of the %2 theme is now "
                                             "available in the Theme Chooser. "
                                             "The currently installed version "
                                             "is %3.")
                                              .arg(m_newVersion,
                                                   GetMythUI()->GetThemeName(),
                                                   m_currentVersion);

                        ShowOkPopup(message);
                        break;
                    }
                }
            }
        }
    }

    delete localTheme;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
