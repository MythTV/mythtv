#include <iostream>
#include <cmath>

#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QPalette>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QPainter>
#include <QDesktopWidget>

#include "mythdirs.h"
#include "mythuihelper.h"
#include "mythverbose.h"
#include "oldsettings.h"
#include "screensaver.h"
#include "mythmainwindow.h"
#include "mythdb.h"
#include "themeinfo.h"
#include "x11colors.h"
#include "util-x11.h"
#include "DisplayRes.h"
#include "mythprogressdialog.h"

static MythUIHelper *mythui = NULL;
static QMutex uiLock;
QString MythUIHelper::x11_display = QString::null;

MythUIHelper *MythUIHelper::getMythUI(void)
{
    if (mythui)
        return mythui;

    uiLock.lock();
    if (!mythui)
        mythui = new MythUIHelper();
    uiLock.unlock();

    return mythui;
}

void MythUIHelper::destroyMythUI(void)
{
    uiLock.lock();
    if (mythui)
        delete mythui;
    mythui = NULL;
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

class MythUIHelperPrivate
{
  public:
    MythUIHelperPrivate(MythUIHelper *p);
   ~MythUIHelperPrivate();

    void Init();

    bool IsWideMode() const {return (m_baseWidth == 1280);}
    void SetWideMode() {m_baseWidth = 1280; m_baseHeight = 720;}
    bool IsSquareMode() const {return (m_baseWidth == 800);}
    void SetSquareMode() {m_baseWidth = 800; m_baseHeight = 600;}

    void GetScreenBounds(void);
    void StoreGUIsettings(void);

    Settings *m_qtThemeSettings;   ///< everything else theme-related

    bool      m_themeloaded;       ///< To we have a palette and pixmap to use?
    QString   m_menuthemepathname;
    QString   m_themepathname;
    QPalette  m_palette;           ///< Colour scheme

    QString language;

    float m_wmult, m_hmult;

    // Drawable area of the full screen. May cover several screens,
    // or exclude windowing system fixtures (like Mac menu bar)
    int m_xbase, m_ybase;
    int m_height, m_width;

    // Dimensions of the theme
    int m_baseWidth, m_baseHeight;

    QMap<QString, QImage> imageCache;

    // The part of the screen(s) allocated for the GUI. Unless
    // overridden by the user, defaults to drawable area above.
    int m_screenxbase, m_screenybase;
    int m_screenwidth, m_screenheight;

    // Command-line GUI size, which overrides both the above sets of sizes
    int m_geometry_x, m_geometry_y;
    int m_geometry_w, m_geometry_h;

    bool m_geometryOverridden;

    QString themecachedir;

    int bigfontsize, mediumfontsize, smallfontsize;

    ScreenSaverControl *screensaver;
    bool screensaverEnabled;

    DisplayRes *display_res;
    bool screenSetup;

    MythUIMenuCallbacks callbacks;

    MythUIHelper *parent;
};

MythUIHelperPrivate::MythUIHelperPrivate(MythUIHelper *p)
    : m_qtThemeSettings(new Settings()),
      m_themeloaded(false),
      m_menuthemepathname(QString::null), m_themepathname(QString::null),
      language(""),
      m_wmult(1.0), m_hmult(1.0),
      m_xbase(0), m_ybase(0), m_height(0), m_width(0),
      m_baseWidth(800), m_baseHeight(600),
      m_screenxbase(0), m_screenybase(0), m_screenwidth(0), m_screenheight(0),
      m_geometry_x(0), m_geometry_y(0), m_geometry_w(0), m_geometry_h(0),
      m_geometryOverridden(false),
      themecachedir(QString::null),
      bigfontsize(0), mediumfontsize(0), smallfontsize(0),
      screensaver(NULL), screensaverEnabled(false), display_res(NULL),
      screenSetup(false), parent(p)
{
}

MythUIHelperPrivate::~MythUIHelperPrivate()
{
    imageCache.clear();

    if (m_qtThemeSettings)
        delete m_qtThemeSettings;
    if (screensaver)
        delete screensaver;
}

void MythUIHelperPrivate::Init(void)
{
    screensaver = ScreenSaverControl::get();
    GetScreenBounds();
    StoreGUIsettings();
    screenSetup = true;
}

/**
 * \brief Get screen size from Qt, respecting for user's multiple screen prefs
 *
 * If the windowing system environment has multiple screens
 * (%e.g. Xinerama or Mac OS X), QApplication::desktop()->%width() will span
 * all of them, so we usually need to get the geometry of a specific screen.
 */
void MythUIHelperPrivate::GetScreenBounds()
{
    if (m_geometryOverridden)
    {
        // Geometry on the command-line overrides everything
        m_xbase  = m_geometry_x;
        m_ybase  = m_geometry_y;
        m_width  = m_geometry_w;
        m_height = m_geometry_h;
        return;
    }

    QDesktopWidget * desktop = QApplication::desktop();
    bool             hasXinerama = GetNumberOfXineramaScreens() > 1;
    int              numScreens  = desktop->numScreens();
    int              screen;

    if (hasXinerama)
        VERBOSE(VB_GENERAL+VB_EXTRA,
                QString("Total desktop dim: %1x%2, over %3 screen[s].")
                .arg(desktop->width()).arg(desktop->height()).arg(numScreens));
    if (numScreens > 1)
        for (screen = 0; screen < numScreens; ++screen)
        {
            QRect dim = desktop->screenGeometry(screen);
            VERBOSE(VB_GENERAL+VB_EXTRA,
                    QString("Screen %1 dim: %2x%3.")
                    .arg(screen).arg(dim.width()).arg(dim.height()));
        }

    screen = desktop->primaryScreen();
    VERBOSE(VB_GENERAL, QString("Primary screen: %1.").arg(screen));

    if (hasXinerama)
        screen = GetMythDB()->GetNumSetting("XineramaScreen", screen);

    if (screen == -1)       // Special case - span all screens
    {
        VERBOSE(VB_GENERAL+VB_EXTRA, QString("Using all %1 screens.")
                                     .arg(numScreens));
        m_xbase  = 0;
        m_ybase  = 0;
        m_width  = desktop->width();
        m_height = desktop->height();

        VERBOSE(VB_GENERAL, QString("Total width = %1, height = %2")
                            .arg(m_width).arg(m_height));
        return;
    }

    if (hasXinerama)        // User specified a single screen
    {
        if (screen < 0 || screen >= numScreens)
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "Xinerama screen %1 was specified,"
                        " but only %2 available, so using screen 0.")
                    .arg(screen).arg(numScreens));
            screen = 0;
        }
    }


    {
        QRect bounds;

        bool inWindow = GetMythDB()->GetNumSetting("RunFrontendInWindow", 0);

        if (inWindow)
            VERBOSE(VB_IMPORTANT, QString("Running in a window"));

        if (inWindow)
            // This doesn't include the area occupied by the
            // Windows taskbar, or the Mac OS X menu bar and Dock
            bounds = desktop->availableGeometry(screen);
        else
            bounds = desktop->screenGeometry(screen);

        m_xbase  = bounds.x();
        m_ybase  = bounds.y();
        m_width  = bounds.width();
        m_height = bounds.height();

        VERBOSE(VB_GENERAL, QString("Using screen %1, %2x%3 at %4,%5")
                            .arg(screen).arg(m_width).arg(m_height)
                            .arg(m_xbase).arg(m_ybase));
    }
}

/**
 * Apply any user overrides to the screen geometry
 */
void MythUIHelperPrivate::StoreGUIsettings()
{
    if (m_geometryOverridden)
    {
        // Geometry on the command-line overrides everything
        m_screenxbase  = m_geometry_x;
        m_screenybase  = m_geometry_y;
        m_screenwidth  = m_geometry_w;
        m_screenheight = m_geometry_h;
    }
    else
    {
        m_screenxbase  = GetMythDB()->GetNumSetting("GuiOffsetX");
        m_screenybase  = GetMythDB()->GetNumSetting("GuiOffsetY");
        m_screenwidth = m_screenheight = 0;
        GetMythDB()->GetResolutionSetting("Gui", m_screenwidth, m_screenheight);
    }

    // If any of these was _not_ set by the user,
    // (i.e. they are 0) use the whole-screen defaults

    if (!m_screenxbase)
        m_screenxbase = m_xbase;
    if (!m_screenybase)
        m_screenybase = m_ybase;
    if (!m_screenwidth)
        m_screenwidth = m_width;
    if (!m_screenheight)
        m_screenheight = m_height;

    if (m_screenheight < 160 || m_screenwidth < 160)
    {
        VERBOSE(VB_IMPORTANT, "Somehow, your screen size settings are bad.");
        VERBOSE(VB_IMPORTANT, QString("GuiResolution: %1")
                        .arg(GetMythDB()->GetSetting("GuiResolution")));
        VERBOSE(VB_IMPORTANT, QString("  old GuiWidth: %1")
                        .arg(GetMythDB()->GetNumSetting("GuiWidth")));
        VERBOSE(VB_IMPORTANT, QString("  old GuiHeight: %1")
                        .arg(GetMythDB()->GetNumSetting("GuiHeight")));
        VERBOSE(VB_IMPORTANT, QString("m_width: %1").arg(m_width));
        VERBOSE(VB_IMPORTANT, QString("m_height: %1").arg(m_height));
        VERBOSE(VB_IMPORTANT, "Falling back to 640x480");

        m_screenwidth  = 640;
        m_screenheight = 480;
    }

    m_wmult = m_screenwidth  / (float)m_baseWidth;
    m_hmult = m_screenheight / (float)m_baseHeight;

    QFont font = QFont("Arial");
    if (!font.exactMatch())
        font = QFont();
    font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
    font.setPointSize((int)floor(14 * m_hmult));

    QApplication::setFont(font);

    //VERBOSE(VB_IMPORTANT, QString("GUI multipliers are: width %1, height %2").arg(m_wmult).arg(m_hmult));
}

MythUIHelper::MythUIHelper()
{
    d = new MythUIHelperPrivate(this);
}

MythUIHelper::~MythUIHelper()
{
    delete d;
}

void MythUIHelper::Init(MythUIMenuCallbacks &cbs)
{
    d->Init();
    d->callbacks = cbs;
}

MythUIMenuCallbacks *MythUIHelper::GetMenuCBs(void)
{
    return &(d->callbacks);
}

bool MythUIHelper::IsScreenSetup(void)
{
    return d->screenSetup;
}

void MythUIHelper::LoadQtConfig(void)
{
    d->language.clear();
    d->themecachedir.clear();

    DisplayRes *dispRes = DisplayRes::GetDisplayRes(); // create singleton
    if (dispRes && GetMythDB()->GetNumSetting("UseVideoModes", 0))
    {
        d->display_res = dispRes;
        // Make sure DisplayRes has current context info
        d->display_res->Initialize();

        // Switch to desired GUI resolution
        d->display_res->SwitchToGUI();
    }

    // Note the possibly changed screen settings
    d->GetScreenBounds();


    if (d->m_qtThemeSettings)
        delete d->m_qtThemeSettings;

    d->m_qtThemeSettings = new Settings;

    QString style = GetMythDB()->GetSetting("Style", "");
    if (!style.isEmpty())
        qApp->setStyle(style);

    QString themename = GetMythDB()->GetSetting("Theme");
    QString themedir = FindThemeDir(themename);

    ThemeInfo *themeinfo = new ThemeInfo(themedir);

    if (themeinfo && themeinfo->IsWide())
    {
        VERBOSE(VB_IMPORTANT,
                QString("Switching to wide mode (%1)").arg(themename));
        d->SetWideMode();
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Switching to square mode (%1)").arg(themename));
        d->SetSquareMode();
    }

    if (themeinfo)
        delete themeinfo;

    // Recalculate GUI dimensions
    d->StoreGUIsettings();

    d->m_themepathname = themedir + "/";

    themedir += "/qtlook.txt";
    d->m_qtThemeSettings->ReadSettings(themedir);
    d->m_themeloaded = false;

    themename = GetMythDB()->GetSetting("MenuTheme");
    d->m_menuthemepathname = FindMenuThemeDir(themename) +"/";

    d->bigfontsize    = GetMythDB()->GetNumSetting("QtFontBig",    25);
    d->mediumfontsize = GetMythDB()->GetNumSetting("QtFontMedium", 16);
    d->smallfontsize  = GetMythDB()->GetNumSetting("QtFontSmall",  12);
}

Settings *MythUIHelper::qtconfig(void)
{
    return d->m_qtThemeSettings;
}

void MythUIHelper::UpdateImageCache(void)
{
    d->imageCache.clear();

    ClearOldImageCache();
    CacheThemeImages();
}

QImage *MythUIHelper::GetImageFromCache(const QString &url)
{
    if (d->imageCache.contains(url))
        return &(d->imageCache[url]);
    return NULL;
}

QImage *MythUIHelper::CacheImage(const QString &url, QImage &im)
{
    d->imageCache[url] = im;
    return &(d->imageCache[url]);
}

QString MythUIHelper::GetThemeCacheDir(void)
{
    QString cachedirname = GetConfDir() + "/themecache/";

    QString tmpcachedir = cachedirname + GetMythDB()->GetSetting("Theme") + "." +
                          QString::number(d->m_screenwidth) + "." +
                          QString::number(d->m_screenheight);

    return tmpcachedir;
}

void MythUIHelper::ClearOldImageCache(void)
{
    QString cachedirname = GetConfDir() + "/themecache/";

    d->themecachedir = GetThemeCacheDir();

    QDir dir(cachedirname);

    if (!dir.exists())
        dir.mkdir(cachedirname);

    QString themecachedir = d->themecachedir;

    d->themecachedir += "/";

    dir.setPath(themecachedir);
    if (!dir.exists())
        dir.mkdir(themecachedir);

    dir.setPath(cachedirname);

    QFileInfoList list = dir.entryInfoList();

    QFileInfoList::const_iterator it = list.begin();
    QMap<QDateTime, QString> dirtimes;
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it++);
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isDir() && !fi->isSymLink())
        {
            if (fi->absoluteFilePath() == themecachedir)
                continue;
            dirtimes[fi->lastModified()] = fi->absoluteFilePath();
        }
    }

    const size_t max_cached = GetMythDB()->GetNumSetting("ThemeCacheSize", 1);
    while ((size_t)dirtimes.size() >= max_cached)
    {
        VERBOSE(VB_FILE, QString("Removing cache dir: %1")
                .arg(dirtimes.begin().value()));
        RemoveCacheDir(dirtimes.begin().value());
        dirtimes.erase(dirtimes.begin());
    }

    QMap<QDateTime, QString>::const_iterator dit = dirtimes.begin();
    for (; dit != dirtimes.end(); ++dit)
    {
        VERBOSE(VB_FILE, QString("Keeping cache dir: %1")
                .arg(*dit));
    }
}

void MythUIHelper::RemoveCacheDir(const QString &dirname)
{
    QString cachedirname = GetConfDir() + "/themecache/";

    if (!dirname.startsWith(cachedirname))
        return;

    VERBOSE(VB_IMPORTANT,
            QString("Removing stale cache dir: %1").arg(dirname));

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
            QFile file(fi->absoluteFilePath());
            file.remove();
        }
        else if (fi->isDir() && !fi->isSymLink())
        {
            RemoveCacheDir(fi->absoluteFilePath());
        }
    }

    dir.rmdir(dirname);
}

void MythUIHelper::CacheThemeImages(void)
{
    if (d->m_screenwidth == d->m_baseWidth &&
        d->m_screenheight == d->m_baseHeight)
        return;

    if (d->IsWideMode())
        CacheThemeImagesDirectory(GetThemesParentDir() + "default-wide/");
    CacheThemeImagesDirectory(GetThemesParentDir() + "default/");
}

void MythUIHelper::CacheThemeImagesDirectory(const QString &dirname,
                                             const QString &subdirname)
{
    QDir dir(dirname);

    if (!dir.exists())
        return;

    MythUIProgressDialog *caching = NULL;
    QFileInfoList list = dir.entryInfoList();

    if (subdirname.length() == 0)
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        QString message = QObject::tr("Initializing MythTV");
        caching = new MythUIProgressDialog(message, popupStack,
                                                    "scalingprogressdialog");

        if (caching->Create())
        {
            popupStack->AddScreen(caching, false);
            caching->SetTotal(list.count());
        }
        else
        {
            delete caching;
            caching = NULL;
        }
    }
    uint progress = 0;

    // This is just to make the progressbar show activity
    for (progress = 0; progress <= list.count(); progress++) {
        if (caching)
            caching->SetProgress(progress);
        qApp->processEvents();
    }

/*
    QString destdir = d->themecachedir;
    if (subdirname.length() > 0)
        destdir += subdirname + "/";

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it++);
        if (caching)
            caching->SetProgress(progress);
        qApp->processEvents();
        progress++;

        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;

        if (fi->isDir() && subdirname.length() == 0)
        {
            QString newdirname = fi->fileName();
            QDir newsubdir(d->themecachedir + newdirname);
            if (!newsubdir.exists())
                newsubdir.mkdir(d->themecachedir + newdirname);

            CacheThemeImagesDirectory(dirname + "/" + newdirname,
                                      newdirname);
            continue;
        }
        else if (fi->isDir())
            continue;

        if (fi->completeSuffix().toLower() != "png" &&
            fi->completeSuffix().toLower() != "jpg" &&
            fi->completeSuffix().toLower() != "gif" &&
            fi->completeSuffix().toLower() != "jpeg")
            continue;

        QString filename = fi->fileName();
        QFileInfo cacheinfo(destdir + filename);

        if (!cacheinfo.exists() ||
            (cacheinfo.lastModified() < fi->lastModified()))
        {
            VERBOSE(VB_FILE, QString("generating cache image for: %1")
                    .arg(fi->absoluteFilePath()));

            QImage *tmpimage = LoadScaleImage(fi->absoluteFilePath(), false);

            if (tmpimage && tmpimage->width() > 0 && tmpimage->height() > 0)
            {
                if (!tmpimage->save(destdir + filename, "PNG"))
                {
                    VERBOSE(VB_IMPORTANT,
                            QString("Failed to save cached image: %1")
                            .arg(d->themecachedir + filename));
                }

                delete tmpimage;
            }
        }
    }
*/
    if (caching)
        caching->Close();
}

void MythUIHelper::GetScreenBounds(int &xbase, int &ybase,
                                  int &width, int &height)
{
    xbase  = d->m_xbase;
    ybase  = d->m_ybase;

    width  = d->m_width;
    height = d->m_height;
}

void MythUIHelper::GetScreenSettings(float &wmult, float &hmult)
{
    wmult = d->m_wmult;
    hmult = d->m_hmult;
}

void MythUIHelper::GetScreenSettings(int &width, float &wmult,
                                    int &height, float &hmult)
{
    height = d->m_screenheight;
    width = d->m_screenwidth;

    wmult = d->m_wmult;
    hmult = d->m_hmult;
}

void MythUIHelper::GetScreenSettings(int &xbase, int &width, float &wmult,
                                    int &ybase, int &height, float &hmult)
{
    xbase  = d->m_screenxbase;
    ybase  = d->m_screenybase;

    height = d->m_screenheight;
    width = d->m_screenwidth;

    wmult = d->m_wmult;
    hmult = d->m_hmult;
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
bool MythUIHelper::ParseGeometryOverride(const QString geometry)
{
    QRegExp     sre("^(\\d+)x(\\d+)$");
    QRegExp     lre("^(\\d+)x(\\d+)([+-]\\d+)([+-]\\d+)$");
    QStringList geo;
    bool        longForm = false;

    if (sre.exactMatch(geometry))
    {
        sre.indexIn(geometry);
        geo = sre.capturedTexts();
    }
    else if (lre.exactMatch(geometry))
    {
        lre.indexIn(geometry);
        geo = lre.capturedTexts();
        longForm = true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Geometry does not match either form -");
        VERBOSE(VB_IMPORTANT, "WIDTHxHEIGHT or WIDTHxHEIGHT+XOFF+YOFF");
        return false;
    }

    bool parsed;

    d->m_geometry_w = geo[1].toInt(&parsed);
    if (!parsed)
    {
        VERBOSE(VB_IMPORTANT, "Could not parse width of geometry override");
        return false;
    }

    d->m_geometry_h = geo[2].toInt(&parsed);
    if (!parsed)
    {
        VERBOSE(VB_IMPORTANT, "Could not parse height of geometry override");
        return false;
    }

    if (longForm)
    {
        d->m_geometry_x = geo[3].toInt(&parsed);
        if (!parsed)
        {
            VERBOSE(VB_IMPORTANT,
                    "Could not parse horizontal offset of geometry override");
            return false;
        }

        d->m_geometry_y = geo[4].toInt(&parsed);
        if (!parsed)
        {
            VERBOSE(VB_IMPORTANT,
                    "Could not parse vertical offset of geometry override");
            return false;
        }
    }

    VERBOSE(VB_IMPORTANT, QString("Overriding GUI, width=%1,"
                                  " height=%2 at %3,%4")
                          .arg(d->m_geometry_w).arg(d->m_geometry_h)
                          .arg(d->m_geometry_x).arg(d->m_geometry_y));

    d->m_geometryOverridden = true;

    return true;
}

bool MythUIHelper::IsGeometryOverridden(void)
{
    return d->m_geometryOverridden;
}

/** \fn FindThemeDir(const QString &themename)
 *  \brief Returns the full path to the theme denoted by themename
 *
 *   If the theme cannot be found falls back to the G.A.N.T theme.
 *   If the G.A.N.T theme doesn't exist then returns an empty string.
 *  \param themename The theme name.
 *  \return Path to theme or empty string.
 */
QString MythUIHelper::FindThemeDir(const QString &themename)
{
    QString testdir = GetConfDir() + "/themes/" + themename;

    QDir dir(testdir);
    if (dir.exists())
        return testdir;
    else
        VERBOSE(VB_IMPORTANT+VB_EXTRA, "No theme dir: " + dir.absolutePath());

    testdir = GetThemesParentDir() + themename;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;
    else
        VERBOSE(VB_IMPORTANT+VB_EXTRA, "No theme dir: " + dir.absolutePath());

    testdir = GetThemesParentDir() + "G.A.N.T";
    dir.setPath(testdir);
    if (dir.exists())
    {
        VERBOSE(VB_IMPORTANT, QString("Could not find theme: %1 - "
                "Switching to G.A.N.T").arg(themename));
        GetMythDB()->SaveSetting("Theme", "G.A.N.T");
        return testdir;
    }
    else
        VERBOSE(VB_IMPORTANT+VB_EXTRA, "No theme dir: " + dir.absolutePath());

    VERBOSE(VB_IMPORTANT, QString("Could not find theme: %1").arg(themename));
    return QString();
}

/** \fn FindMenuThemeDir(const QString &menuname)
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

    if (menuname == "default")
    {
        testdir = GetShareDir();
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;
    }

    testdir = GetConfDir() + "/themes/" + menuname;

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
        VERBOSE(VB_IMPORTANT, QString("Could not find theme: %1 - "
                "Switching to default").arg(menuname));
        GetMythDB()->SaveSetting("MenuTheme", "default");
        return testdir;
    }
    else {
        VERBOSE(VB_IMPORTANT, QString("Could not find menu theme: %1 - Fallback to default failed.").arg(menuname));
    }

    return QString();
}

QString MythUIHelper::GetMenuThemeDir(void)
{
    return d->m_menuthemepathname;
}

QString MythUIHelper::GetThemeDir(void)
{
    return d->m_themepathname;
}

QList<QString> MythUIHelper::GetThemeSearchPath(void)
{
    QList<QString> searchpath;

    searchpath.append(GetThemeDir());
    if (d->IsWideMode())
        searchpath.append(GetThemesParentDir() + "default-wide/");
    searchpath.append(GetThemesParentDir() + "default/");
    searchpath.append("/tmp/");
    return searchpath;
}

void MythUIHelper::SetPalette(QWidget *widget)
{
    QPalette pal = widget->palette();

    const QString names[] = { "Foreground", "Button", "Light", "Midlight",
                              "Dark", "Mid", "Text", "BrightText", "ButtonText",
                              "Base", "Background", "Shadow", "Highlight",
                              "HighlightedText" };

    QString type = "Active";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (!color.isEmpty())
            pal.setColor(QPalette::Active, (QPalette::ColorRole) i,
                         createColor(color));
    }

    type = "Disabled";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (!color.isEmpty())
            pal.setColor(QPalette::Disabled, (QPalette::ColorRole) i,
                         createColor(color));
    }

    type = "Inactive";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (!color.isEmpty())
            pal.setColor(QPalette::Inactive, (QPalette::ColorRole) i,
                         createColor(color));
    }

    widget->setPalette(pal);
}

void MythUIHelper::ThemeWidget(QWidget *widget)
{
    if (d->m_themeloaded)
    {
        widget->setPalette(d->m_palette);
        return;
    }

    SetPalette(widget);
    d->m_palette = widget->palette();

    QPixmap *bgpixmap = NULL;

    if (!d->m_qtThemeSettings->GetSetting("BackgroundPixmap").isEmpty())
    {
        QString pmapname = d->m_themepathname +
                           d->m_qtThemeSettings->GetSetting("BackgroundPixmap");

        bgpixmap = LoadScalePixmap(pmapname);
        if (bgpixmap)
        {
            d->m_palette.setBrush(widget->backgroundRole(), QBrush(*bgpixmap));
            widget->setPalette(d->m_palette);
        }
    }
    else if (!d->m_qtThemeSettings->GetSetting("TiledBackgroundPixmap").isEmpty())
    {
        QString pmapname = d->m_themepathname +
                      d->m_qtThemeSettings->GetSetting("TiledBackgroundPixmap");

        int width, height;
        float wmult, hmult;

        GetScreenSettings(width, wmult, height, hmult);

        bgpixmap = LoadScalePixmap(pmapname);
        if (bgpixmap)
        {
            QPixmap background(width, height);
            QPainter tmp(&background);

            tmp.drawTiledPixmap(0, 0, width, height, *bgpixmap);
            tmp.end();

            d->m_palette.setBrush(widget->backgroundRole(), QBrush(background));
            widget->setPalette(d->m_palette);
        }
    }

    d->m_themeloaded = true;

    if (bgpixmap)
        delete bgpixmap;
}

bool MythUIHelper::FindThemeFile(QString &path)
{
    QFileInfo fi(path);
    if (fi.isAbsolute() && fi.exists())
        return true;

    QString file;
    bool foundit = false;
    QList<QString> searchpath = GetThemeSearchPath();
    for (QList<QString>::const_iterator ii = searchpath.begin();
        ii != searchpath.end(); ii++)
    {
        if (fi.isRelative())
        {
            file = *ii + fi.filePath();
        }
        else if (fi.isAbsolute() && !fi.isRoot())
        {
            file = *ii + fi.fileName();
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

QImage *MythUIHelper::LoadScaleImage(QString filename, bool fromcache)
{
    if (filename.left(5) == "myth:" || filename.isEmpty())
        return NULL;

    if (!d->themecachedir.isEmpty() && fromcache)
    {
        QString cachefilepath;
        bool bFound = false;

        // Is absolute path in theme directory.
        if (!bFound)
        {
            if (!d->m_themepathname.isEmpty() &&
                    filename.left(d->m_themepathname.length()) ==
                        d->m_themepathname)
            {
                QString tmpfilename = filename;
                tmpfilename.remove(0, d->m_themepathname.length());
                cachefilepath = d->themecachedir + tmpfilename;
                QFile cachecheck(cachefilepath);
                if (cachecheck.exists())
                    bFound = true;
            }
        }

        // Is relative path in theme directory.
        if (!bFound)
        {
            cachefilepath = d->themecachedir + filename;
            QFile cachecheck(cachefilepath);
            if (cachecheck.exists())
                bFound = true;
        }

        // Is in top level cache directory.
        if (!bFound)
        {
            QFileInfo fi(filename);
            cachefilepath = d->themecachedir + fi.fileName();
            QFile cachecheck(cachefilepath);
            if (cachecheck.exists())
                bFound = true;
        }

        if (bFound)
        {
            QImage *ret = new QImage(cachefilepath);
            if (ret)
                return ret;
        }
    }

    if (!FindThemeFile(filename))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to find image file: %1").arg(filename));

        return NULL;
    }

    QImage *ret = NULL;

    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (width != d->m_baseWidth || height != d->m_baseHeight)
    {
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            VERBOSE(VB_IMPORTANT,
                    "Error loading image to scale, from file: " + filename);

            return NULL;
        }
        QImage tmp2 = tmpimage.scaled((int)(tmpimage.width() * wmult),
                                           (int)(tmpimage.height() * hmult),
                                           Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
        ret = new QImage(tmp2);
    }
    else
    {
        ret = new QImage(filename);
        if (ret->width() == 0)
        {
            VERBOSE(VB_IMPORTANT, "Error loading image from file: "
                                  + filename + " - QImage->width()=0");

            delete ret;
            return NULL;
        }
    }

    return ret;
}

QPixmap *MythUIHelper::LoadScalePixmap(QString filename, bool fromcache)
{
    if (filename.left(5) == "myth:")
        return NULL;

    if (!d->themecachedir.isEmpty() && fromcache)
    {
        QString cachefilepath;
        bool bFound = false;

        // Is absolute path in theme directory.
        if (!bFound)
        {
            if (filename.left(d->m_themepathname.length()) ==
                d->m_themepathname)
            {
                QString tmpfilename = filename;
                tmpfilename.remove(0, d->m_themepathname.length());
                cachefilepath = d->themecachedir + tmpfilename;
                QFile cachecheck(cachefilepath);
                if (cachecheck.exists())
                    bFound = true;
            }
        }

        // Is relative path in theme directory.
        if (!bFound)
        {
            cachefilepath = d->themecachedir + filename;
            QFile cachecheck(cachefilepath);
            if (cachecheck.exists())
                bFound = true;
        }

        // Is in top level cache directory.
        if (!bFound)
        {
            QFileInfo fi(filename);
            cachefilepath = d->themecachedir + fi.fileName();
            QFile cachecheck(cachefilepath);
            if (cachecheck.exists())
                bFound = true;
        }

        if (bFound)
        {
            QPixmap *ret = new QPixmap(cachefilepath);
            if (ret)
                return ret;
        }
    }

    if (!FindThemeFile(filename))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Unable to find image file: %1").arg(filename));

        return NULL;
    }

    QPixmap *ret = NULL;

    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (width != d->m_baseWidth || height != d->m_baseHeight)
    {
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Error loading image file: %1").arg(filename));

            return NULL;
        }
        QImage tmp2 = tmpimage.scaled((int)(tmpimage.width() * wmult),
                                           (int)(tmpimage.height() * hmult),
                                           Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
        ret = new QPixmap(QPixmap::fromImage(tmp2));
    }
    else
    {
        ret = new QPixmap();
        if (!ret->load(filename))
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Error loading image file: %1").arg(filename));

            delete ret;
            return NULL;
        }
    }

    return ret;
}

QFont MythUIHelper::GetBigFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(d->bigfontsize));
    font.setWeight(QFont::Bold);

    return font;
}

QFont MythUIHelper::GetMediumFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(d->mediumfontsize));
    font.setWeight(QFont::Bold);

    return font;
}

QFont MythUIHelper::GetSmallFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(d->smallfontsize));
    font.setWeight(QFont::Bold);

    return font;
}

/** \fn MythUIHelper::GetLanguage()
 *  \brief Returns two character ISO-639 language descriptor for UI language.
 *  \sa iso639_get_language_list()
 */
QString MythUIHelper::GetLanguage(void)
{
    return GetLanguageAndVariant().left(2);
}

/** \fn MythUIHelper::GetLanguageAndVariant()
 *  \brief Returns the user-set language and variant.
 *
 *   The string has the form ll or ll_vv, where ll is the two character
 *   ISO-639 language code, and vv (which may not exist) is the variant.
 *   Examples include en_AU, en_CA, en_GB, en_US, fr_CH, fr_DE, pt_BR, pt_PT.
 */
QString MythUIHelper::GetLanguageAndVariant(void)
{
    if (d->language == QString::null || d->language.isEmpty())
        d->language = GetMythDB()->GetSetting("Language", "EN").toLower();

    return d->language;
}

void MythUIHelper::DisableScreensaver(void)
{
    QApplication::postEvent(GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetDisable));
}

void MythUIHelper::RestoreScreensaver(void)
{
    QApplication::postEvent(GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetRestore));
}

void MythUIHelper::ResetScreensaver(void)
{
    QApplication::postEvent(GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetReset));
}

void MythUIHelper::DoDisableScreensaver(void)
{
    if (d->screensaver)
    {
        d->screensaver->Disable();
        d->screensaverEnabled = false;
    }
}

void MythUIHelper::DoRestoreScreensaver(void)
{
    if (d->screensaver)
    {
        d->screensaver->Restore();
        d->screensaverEnabled = true;
    }
}

void MythUIHelper::DoResetScreensaver(void)
{
    if (d->screensaver)
    {
        d->screensaver->Reset();
        d->screensaverEnabled = false;
    }
}

bool MythUIHelper::GetScreensaverEnabled(void)
{
    return d->screensaverEnabled;
}

bool MythUIHelper::GetScreenIsAsleep(void)
{
    if (!d->screensaver)
        return false;
    return d->screensaver->Asleep();
}

/// This needs to be set before MythUIHelper is initialized so
/// that the MythUIHelper::Init() can detect Xinerama setups.
void MythUIHelper::SetX11Display(const QString &display)
{
    x11_display = display;
    x11_display.detach();
}

QString MythUIHelper::GetX11Display(void)
{
    QString ret = x11_display;
    ret.detach();
    return ret;
}

