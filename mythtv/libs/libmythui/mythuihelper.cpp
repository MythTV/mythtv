#include "mythuihelper.h"

#include <cmath>
#include <unistd.h>
#include <iostream>

#include <QMutex>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QTimer>

// mythbase headers
#include "mythdirs.h"
#include "mythlogging.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "storagegroup.h"

// mythui headers
#include "mythprogressdialog.h"
#include "mythimage.h"
#include "mythmainwindow.h"
#include "themeinfo.h"
#include "x11colors.h"
#include "mythdisplay.h"

#define LOC QString("MythUIHelper: ")

static MythUIHelper *mythui = nullptr;
static QMutex uiLock;

MythUIHelper *MythUIHelper::getMythUI()
{
    if (mythui)
        return mythui;

    uiLock.lock();

    if (!mythui)
        mythui = new MythUIHelper();

    uiLock.unlock();

    // These directories should always exist.  Don't test first as
    // there's no harm in trying to create an existing directory.
    QDir dir;
    dir.mkdir(GetThemeBaseCacheDir());
    dir.mkdir(GetRemoteCacheDir());
    dir.mkdir(GetThumbnailDir());

    return mythui;
}

void MythUIHelper::destroyMythUI()
{
    uiLock.lock();
    delete mythui;
    mythui = nullptr;
    uiLock.unlock();
}

MythUIHelper *GetMythUI()
{
    return MythUIHelper::getMythUI();
}

void DestroyMythUI()
{
    MythUIHelper::destroyMythUI();
}

MythUIHelper::~MythUIHelper()
{
    if (m_display)
    {
        // N.B. we always call this to ensure the desktop mode is restored
        // in case the setting was changed
        m_display->SwitchToDesktop();
        MythDisplay::AcquireRelease(false);
    }
}

void MythUIHelper::Init(MythUIMenuCallbacks &cbs)
{
    if (!m_display)
        m_display = MythDisplay::AcquireRelease();
    m_screenSetup = true;
    StorageGroup sgroup("Themes", gCoreContext->GetHostName());
    m_userThemeDir = sgroup.GetFirstDir(true);

    m_callbacks = cbs;
}

// This init is used for showing the startup UI that is shown
// before the database is initialized. The above init is called later,
// after the DB is available.
// This class does not mind being Initialized twice.
void MythUIHelper::Init()
{
    if (!m_display)
        m_display = MythDisplay::AcquireRelease();
    m_screenSetup = true;
    StorageGroup sgroup("Themes", gCoreContext->GetHostName());
    m_userThemeDir = sgroup.GetFirstDir(true);
}

MythUIMenuCallbacks *MythUIHelper::GetMenuCBs()
{
    return &m_callbacks;
}

bool MythUIHelper::IsScreenSetup() const
{
    return m_screenSetup;
}

void MythUIHelper::LoadQtConfig()
{
    gCoreContext->ResetLanguage();
    MythUIThemeCache::ClearThemeCacheDir();

    if (!m_display)
        m_display = MythDisplay::AcquireRelease();

    // Switch to desired GUI resolution
    if (m_display->UsingVideoModes())
        m_display->SwitchToGUI(true);

    QApplication::setStyle("Windows");

    QString themename = GetMythDB()->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = FindThemeDir(themename);

    auto *themeinfo = new ThemeInfo(themedir);
    if (themeinfo)
    {
        m_isWide    = themeinfo->IsWide();
        m_baseSize  = themeinfo->GetBaseRes();
        m_themename = themeinfo->GetName();
        LOG(VB_GUI, LOG_INFO, LOC + QString("Using theme base resolution of %1x%2")
            .arg(m_baseSize.width()).arg(m_baseSize.height()));
        delete themeinfo;
    }

    m_themepathname = themedir + '/';
    m_searchPaths.clear();

    themename = GetMythDB()->GetSetting("MenuTheme", "defaultmenu");
    if (themename == "default")
        themename = "defaultmenu";

    m_menuthemepathname = FindMenuThemeDir(themename);
}

/**
 *  \brief Returns the full path to the theme denoted by themename
 *
 *   If the theme cannot be found falls back to the DEFAULT_UI_THEME.
 *   If the DEFAULT_UI_THEME doesn't exist then returns an empty string.
 *  \param themename The theme name.
 *  \param doFallback If true and the theme isn't found, allow
 *                    fallback to the default theme.  If false and the
 *                    theme isn't found, then return an empty string.
 *  \return Path to theme or empty string.
 */
QString MythUIHelper::FindThemeDir(const QString &themename, bool doFallback)
{
    QString testdir;
    QDir dir;

    if (!themename.isEmpty())
    {
        testdir = m_userThemeDir + themename;

        dir.setPath(testdir);

        if (dir.exists())
            return testdir;

        testdir = GetThemesParentDir() + themename;
        dir.setPath(testdir);

        if (dir.exists())
            return testdir;

        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No theme dir: '%1'")
            .arg(dir.absolutePath()));
    }

    if (!doFallback)
        return QString();

    testdir = GetThemesParentDir() + DEFAULT_UI_THEME;
    dir.setPath(testdir);

    if (dir.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not find theme: %1 - Switching to %2")
            .arg(themename).arg(DEFAULT_UI_THEME));
        GetMythDB()->OverrideSettingForSession("Theme", DEFAULT_UI_THEME);
        return testdir;
    }

    LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No default theme dir: '%1'")
        .arg(dir.absolutePath()));

    testdir = GetThemesParentDir() + FALLBACK_UI_THEME;
    dir.setPath(testdir);

    if (dir.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not find theme: %1 - Switching to %2")
            .arg(themename).arg(FALLBACK_UI_THEME));
        GetMythDB()->OverrideSettingForSession("Theme", FALLBACK_UI_THEME);
        return testdir;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("No fallback GUI theme dir: '%1'")
        .arg(dir.absolutePath()));

    return QString();
}

/**
 *  \brief Returns the full path to the menu theme denoted by menuname
 *
 *   If the theme cannot be found falls back to the default menu.
 *   If the default menu theme doesn't exist then returns an empty string.
 *  \param menuname The menutheme name.
 *  \return Path to theme or empty string.
 */
QString MythUIHelper::FindMenuThemeDir(const QString &menuname)
{
    QString testdir;
    QDir dir;

    testdir = m_userThemeDir + menuname;

    dir.setPath(testdir);

    if (dir.exists())
        return testdir;

    testdir = GetThemesParentDir() + menuname;
    dir.setPath(testdir);

    if (dir.exists())
        return testdir;

    testdir = GetShareDir();
    dir.setPath(testdir);

    if (dir.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not find menu theme: %1 - Switching to default")
            .arg(menuname));

        GetMythDB()->SaveSetting("MenuTheme", "default");
        return testdir;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Could not find menu theme: %1 - Fallback to default failed.")
        .arg(menuname));

    return QString();
}

QString MythUIHelper::GetMenuThemeDir()
{
    return m_menuthemepathname;
}

QString MythUIHelper::GetThemeDir()
{
    return m_themepathname;
}

QString MythUIHelper::GetThemeName()
{
    return m_themename;
}

QStringList MythUIHelper::GetThemeSearchPath()
{
    if (!m_searchPaths.isEmpty())
        return m_searchPaths;

    // traverse up the theme inheritance list adding their location to the search path
    QList<ThemeInfo> themeList = GetThemes(THEME_UI);
    bool found = true;
    QString themeName = m_themename;
    QString baseName;
    QString dirName;

    while (found && !themeName.isEmpty())
    {
        // find the ThemeInfo for this theme
        found = false;
        baseName = "";
        dirName = "";

        for (int x = 0; x < themeList.count(); x++)
        {
            if (themeList.at(x).GetName() == themeName)
            {
                found = true;
                baseName = themeList.at(x).GetBaseTheme();
                dirName = themeList.at(x).GetDirectoryName();
                break;
            }
        }

        // try to find where the theme is installed
        if (found)
        {
            QString themedir = FindThemeDir(dirName, false);
            if (!themedir.isEmpty())
            {
                LOG(VB_GUI, LOG_INFO, LOC + QString("Adding path '%1' to theme search paths").arg(themedir));
                m_searchPaths.append(themedir + '/');
            }
            else
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not find ui theme location: %1").arg(themedir));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not find inherited theme: %1").arg(themeName));
        }

        themeName = baseName;
    }

    if (m_isWide)
        m_searchPaths.append(GetThemesParentDir() + "default-wide/");

    m_searchPaths.append(GetThemesParentDir() + "default/");
    m_searchPaths.append("/tmp/");
    return m_searchPaths;
}

QList<ThemeInfo> MythUIHelper::GetThemes(ThemeType type)
{
    QFileInfoList fileList;
    QList<ThemeInfo> themeList;
    QDir themeDirs(GetThemesParentDir());
    themeDirs.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    themeDirs.setSorting(QDir::Name | QDir::IgnoreCase);

    fileList.append(themeDirs.entryInfoList());

    themeDirs.setPath(m_userThemeDir);

    fileList.append(themeDirs.entryInfoList());

    for (const auto & theme : qAsConst(fileList))
    {
        if (theme.baseName() == "default" ||
            theme.baseName() == "default-wide" ||
            theme.baseName() == "Slave")
            continue;

        ThemeInfo themeInfo(theme.absoluteFilePath());

        if (themeInfo.GetType() & type)
            themeList.append(themeInfo);
    }

    return themeList;
}

bool MythUIHelper::FindThemeFile(QString &path)
{
    QFileInfo fi(path);

    if (fi.isAbsolute() && fi.exists())
        return true;
#ifdef Q_OS_ANDROID
    if (path.startsWith("assets:/") && fi.exists())
        return true;
#endif

    QString file;
    bool foundit = false;
    const QStringList searchpath = GetThemeSearchPath();

    for (const auto & ii : qAsConst(searchpath))
    {
        if (fi.isRelative())
        {
            file = ii + fi.filePath();
        }
        else if (fi.isAbsolute() && !fi.isRoot())
        {
            file = ii + fi.fileName();
        }

        if (QFile::exists(file))
        {
            path = file;
            foundit = true;
            break;
        }
    }

    return foundit;
}

void MythUIHelper::AddCurrentLocation(const QString& location)
{
    QMutexLocker locker(&m_locationLock);

    if (m_currentLocation.isEmpty() || m_currentLocation.last() != location)
        m_currentLocation.push_back(location);
}

QString MythUIHelper::RemoveCurrentLocation()
{
    QMutexLocker locker(&m_locationLock);

    if (m_currentLocation.isEmpty())
        return QString("UNKNOWN");

    return m_currentLocation.takeLast();
}

QString MythUIHelper::GetCurrentLocation(bool fullPath, bool mainStackOnly)
{
    QString result;
    QMutexLocker locker(&m_locationLock);

    if (fullPath)
    {
        // get main stack top screen
        MythScreenStack *stack = GetMythMainWindow()->GetMainStack();
        result = stack->GetLocation(true);

        if (!mainStackOnly)
        {
            // get popup stack main screen
            stack = GetMythMainWindow()->GetStack("popup stack");

            if (!stack->GetLocation(true).isEmpty())
                result += '/' + stack->GetLocation(false);
        }

        // if there's a location in the stringlist add that (non mythui screen or external app running)
        if (!m_currentLocation.isEmpty())
        {
            for (int x = 0; x < m_currentLocation.count(); x++)
                result += '/' + m_currentLocation[x];
        }
    }
    else
    {
        // get main stack top screen
        MythScreenStack *stack = GetMythMainWindow()->GetMainStack();
        result = stack->GetLocation(false);

        if (!mainStackOnly)
        {
            // get popup stack top screen
            stack = GetMythMainWindow()->GetStack("popup stack");

            if (!stack->GetLocation(false).isEmpty())
                result = stack->GetLocation(false);
        }

        // if there's a location in the stringlist use that (non mythui screen or external app running)
        if (!m_currentLocation.isEmpty())
            result = m_currentLocation.last();
    }

    if (result.isEmpty())
        result = "UNKNOWN";

    return result;
}

QSize MythUIHelper::GetBaseSize() const
{
    return m_baseSize;
}
