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

int MythUIHelper::x_override = -1;
int MythUIHelper::y_override = -1;
int MythUIHelper::w_override = -1;
int MythUIHelper::h_override = -1;

/**
 * Apply any user overrides to the screen geometry
 */
void MythUIHelper::StoreGUIsettings()
{
    if (x_override >= 0 && y_override >= 0)
    {
        GetMythDB()->OverrideSettingForSession("GuiOffsetX", QString::number(x_override));
        GetMythDB()->OverrideSettingForSession("GuiOffsetY", QString::number(y_override));
    }

    if (w_override > 0 && h_override > 0)
    {
        GetMythDB()->OverrideSettingForSession("GuiWidth", QString::number(w_override));
        GetMythDB()->OverrideSettingForSession("GuiHeight", QString::number(h_override));
    }

    int x = GetMythDB()->GetNumSetting("GuiOffsetX");
    int y = GetMythDB()->GetNumSetting("GuiOffsetY");
    int width = 0;
    int height = 0;
    GetMythDB()->GetResolutionSetting("Gui", width, height);

    if (!m_display)
        m_display = MythDisplay::AcquireRelease();
    QRect screenbounds = m_display->GetScreenBounds();

    // As per MythMainWindow::Init, fullscreen is indicated by all zero's in settings
    if (x == 0 && y == 0 && width == 0 && height == 0)
        m_screenRect = screenbounds;
    else
        m_screenRect = QRect(x, y, width, height);

    if (m_screenRect.width() < 160 || m_screenRect.height() < 160)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Strange screen size: %1x%2 - forcing 640x480")
            .arg(m_screenRect.width()).arg(m_screenRect.height()));
        m_screenRect.setSize(QSize(640, 480));
    }

    m_wmult = m_screenRect.width()  / static_cast<float>(m_baseSize.width());
    m_hmult = m_screenRect.height() / static_cast<float>(m_baseSize.height());

    MythUIThemeCache::SetScreenSize(m_screenRect.size());

    // Default font, _ALL_ fonts inherit from this!
    // e.g All fonts will be 19 pixels unless a new size is explicitly defined.
    QFont font = QFont("Arial");
    if (!font.exactMatch())
        font = QFont();

    font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
    font.setPixelSize(static_cast<int>(lroundf(19.0F * m_hmult)));
    int stretch = static_cast<int>(100 / m_display->GetPixelAspectRatio());
    font.setStretch(stretch); // QT
    m_fontStretch = stretch; // MythUI

    QApplication::setFont(font);
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
    StoreGUIsettings();
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
    StoreGUIsettings();
    m_screenSetup = true;
    StorageGroup sgroup("Themes", gCoreContext->GetHostName());
    m_userThemeDir = sgroup.GetFirstDir(true);
}

MythUIMenuCallbacks *MythUIHelper::GetMenuCBs()
{
    return &m_callbacks;
}

bool MythUIHelper::IsScreenSetup()
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

    // Recalculate GUI dimensions
    StoreGUIsettings();

    m_themepathname = themedir + '/';
    m_searchPaths.clear();

    themename = GetMythDB()->GetSetting("MenuTheme", "defaultmenu");

    if (themename == "default")
        themename = "defaultmenu";

    m_menuthemepathname = FindMenuThemeDir(themename);
}

void MythUIHelper::UpdateScreenSettings()
{
    StoreGUIsettings();
}

QRect MythUIHelper::GetScreenSettings()
{
    return m_screenRect;
}

void MythUIHelper::GetScreenSettings(float &XFactor, float &YFactor)
{
    XFactor = m_wmult;
    YFactor = m_hmult;
}

void MythUIHelper::GetScreenSettings(QRect &Rect, float &XFactor, float &YFactor)
{
    XFactor = m_wmult;
    YFactor = m_hmult;
    Rect    = m_screenRect;
}

/**
 * \brief Parse an X11 style command line geometry string
 *
 * Accepts strings like
 *  -geometry 800x600
 * or
 *  -geometry 800x600+112+22
 * to override the fullscreen and user default screen dimensions
 */
void MythUIHelper::ParseGeometryOverride(const QString &geometry)
{
    QRegExp     sre("^(\\d+)x(\\d+)$");
    QRegExp     lre(R"(^(\d+)x(\d+)([+-]\d+)([+-]\d+)$)");
    QStringList geo;
    bool        longForm = false;

    if (sre.exactMatch(geometry))
    {
        geo = sre.capturedTexts();
    }
    else if (lre.exactMatch(geometry))
    {
        geo = lre.capturedTexts();
        longForm = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Geometry does not match either form -\n\t\t\t"
            "WIDTHxHEIGHT or WIDTHxHEIGHT+XOFF+YOFF");
        return;
    }

    bool parsed = false;
    int tmp_h = 0;
    int tmp_w = geo[1].toInt(&parsed);

    if (!parsed)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Could not parse width of geometry override");
    }

    if (parsed)
    {
        tmp_h = geo[2].toInt(&parsed);

        if (!parsed)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Could not parse height of geometry override");
        }
    }

    if (parsed)
    {
        MythUIHelper::w_override = tmp_w;
        MythUIHelper::h_override = tmp_h;
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Overriding GUI size: width=%1 height=%2")
            .arg(tmp_w).arg(tmp_h));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to override GUI size.");
    }

    if (longForm)
    {
        int tmp_x = geo[3].toInt(&parsed);
        if (!parsed)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Could not parse horizontal offset of geometry override");
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to override GUI offset.");
            return;
        }

        int tmp_y = geo[4].toInt(&parsed);
        if (!parsed)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Could not parse vertical offset of geometry override");
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to override GUI offset.");
            return;
        }

        MythUIHelper::x_override = tmp_x;
        MythUIHelper::y_override = tmp_y;
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Overriding GUI offset: x=%1 y=%2")
            .arg(tmp_x).arg(tmp_y));
    }
}

bool MythUIHelper::IsGeometryOverridden()
{
    return (MythUIHelper::x_override >= 0 ||
            MythUIHelper::y_override >= 0 ||
            MythUIHelper::w_override >= 0 ||
            MythUIHelper::h_override >= 0);
}

/*! \brief Return the raw geometry override rectangle.
 *
 * Used before MythUIHelper has been otherwise initialised (and hence GetScreenSettings
 * does not return a valid result).
*/
QRect MythUIHelper::GetGeometryOverride()
{
    // NB Call IsGeometryOverridden first to ensure this is valid
    return {MythUIHelper::x_override, MythUIHelper::y_override,
            MythUIHelper::w_override, MythUIHelper::h_override};
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

void MythUIHelper::SetFontStretch(int stretch)
{
    m_fontStretch = stretch;
}

int MythUIHelper::GetFontStretch() const
{
    return m_fontStretch;
}
