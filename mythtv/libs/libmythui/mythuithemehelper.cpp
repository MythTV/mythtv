// Qt
#include <QDir>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/storagegroup.h"

#include "mythuithemehelper.h"

#define LOC QString("ThemeHelper: ")

void MythUIThemeHelper::InitThemeHelper()
{
    StorageGroup sgroup("Themes", gCoreContext->GetHostName());
    m_userThemeDir = sgroup.GetFirstDir(true);

    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir  = FindThemeDir(themename);

    ThemeInfo themeinfo(themedir);
    m_isWide    = themeinfo.IsWide();
    m_baseSize  = themeinfo.GetBaseRes();
    m_themename = themeinfo.GetName();
    LOG(VB_GUI, LOG_INFO, LOC + QString("Using theme base resolution of %1x%2")
        .arg(m_baseSize.width()).arg(m_baseSize.height()));

    m_themepathname = themedir + '/';
    m_searchPaths.clear();

    themename = gCoreContext->GetSetting("MenuTheme", "defaultmenu");
    if (themename == "default")
        themename = "defaultmenu";
    m_menuthemepathname = FindMenuThemeDir(themename);
}

/**
 *  \brief Returns the full path to the theme denoted by themename
 *
 *   If the theme cannot be found falls back to the DEFAULT_UI_THEME.
 *   If the DEFAULT_UI_THEME doesn't exist then returns an empty string.
 *  \param ThemeName The theme name.
 *  \param Fallback If true and the theme isn't found, allow fallback to the
 * default theme.  If false and the theme isn't found, then return an empty string.
 *  \return Path to theme or empty string.
 */
QString MythUIThemeHelper::FindThemeDir(const QString& ThemeName, bool Fallback)
{
    QString testdir;
    QDir dir;

    if (!ThemeName.isEmpty())
    {
        testdir = m_userThemeDir + ThemeName;
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;

        testdir = GetThemesParentDir() + ThemeName;
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;

        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No theme dir: '%1'").arg(dir.absolutePath()));
    }

    if (!Fallback)
        return {};

    testdir = GetThemesParentDir() + DEFAULT_UI_THEME;
    dir.setPath(testdir);
    if (dir.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not find theme: %1 - Switching to %2")
            .arg(ThemeName, DEFAULT_UI_THEME));
        gCoreContext->OverrideSettingForSession("Theme", DEFAULT_UI_THEME);
        return testdir;
    }

    LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No default theme dir: '%1'")
        .arg(dir.absolutePath()));

    testdir = GetThemesParentDir() + FALLBACK_UI_THEME;
    dir.setPath(testdir);
    if (dir.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not find theme: %1 - Switching to %2")
            .arg(ThemeName, FALLBACK_UI_THEME));
        gCoreContext->OverrideSettingForSession("Theme", FALLBACK_UI_THEME);
        return testdir;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("No fallback GUI theme dir: '%1'").arg(dir.absolutePath()));
    return {};
}

/**
 *  \brief Returns the full path to the menu theme denoted by menuname
 *
 *   If the theme cannot be found falls back to the default menu.
 *   If the default menu theme doesn't exist then returns an empty string.
 *  \param MenuName The menutheme name.
 *  \return Path to theme or empty string.
 */
QString MythUIThemeHelper::FindMenuThemeDir(const QString& MenuName)
{
    QString testdir;
    QDir dir;

    testdir = m_userThemeDir + MenuName;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    testdir = GetThemesParentDir() + MenuName;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    testdir = GetShareDir();
    dir.setPath(testdir);
    if (dir.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not find menu theme: %1 - Switching to default")
            .arg(MenuName));
        gCoreContext->SaveSetting("MenuTheme", "default");
        return testdir;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not find menu theme: %1 - Fallback to default failed.")
        .arg(MenuName));
    return {};
}

QString MythUIThemeHelper::GetMenuThemeDir()
{
    return m_menuthemepathname;
}

QString MythUIThemeHelper::GetThemeDir()
{
    return m_themepathname;
}

QString MythUIThemeHelper::GetThemeName()
{
    return m_themename;
}

QStringList MythUIThemeHelper::GetThemeSearchPath()
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
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not find ui theme location: %1").arg(themedir));
            }
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

QList<ThemeInfo> MythUIThemeHelper::GetThemes(ThemeType Type)
{
    QDir themeDirs(GetThemesParentDir());
    themeDirs.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    themeDirs.setSorting(QDir::Name | QDir::IgnoreCase);
    QFileInfoList fileList { themeDirs.entryInfoList() };
    themeDirs.setPath(m_userThemeDir);
    fileList.append(themeDirs.entryInfoList());

    QList<ThemeInfo> themeList;
    for (const auto & theme : std::as_const(fileList))
    {
        if (theme.baseName() == "default" || theme.baseName() == "default-wide" ||
            theme.baseName() == "Slave")
        {
            continue;
        }

        ThemeInfo themeInfo(theme.absoluteFilePath());

        if (themeInfo.GetType() & Type)
            themeList.append(themeInfo);
    }

    return themeList;
}

bool MythUIThemeHelper::FindThemeFile(QString& Path)
{
    QFileInfo fi(Path);

    if (fi.isAbsolute() && fi.exists())
        return true;
#ifdef Q_OS_ANDROID
    if (Path.startsWith("assets:/") && fi.exists())
        return true;
#endif

    QString file;
    bool foundit = false;
    const QStringList searchpath = GetThemeSearchPath();

    for (const auto & ii : std::as_const(searchpath))
    {
        if (fi.isRelative())
            file = ii + fi.filePath();
        else if (fi.isAbsolute() && !fi.isRoot())
            file = ii + fi.fileName();

        if (QFile::exists(file))
        {
            Path = file;
            foundit = true;
            break;
        }
    }

    return foundit;
}

QSize MythUIThemeHelper::GetBaseSize() const
{
    return m_baseSize;
}


