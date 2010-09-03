#include "mythuihelper.h"

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
#include <QStyleFactory>
#include <QThreadPool>
#include <QSize>
#include <QFile>

#include "mythdirs.h"
#include "mythverbose.h"
#include "mythdownloadmanager.h"
#include "oldsettings.h"
#include "screensaver.h"
#include "mythmainwindow.h"
#include "mythdb.h"
#include "themeinfo.h"
#include "x11colors.h"
#include "mythxdisplay.h"
#include "DisplayRes.h"
#include "mythprogressdialog.h"
#include "mythimage.h"
#include "remotefile.h"
#include "mythcorecontext.h"

#define LOC      QString("MythUIHelper: ")
#define LOC_ERR  QString("MythUIHelper, Error: ")
#define LOC_WARN QString("MythUIHelper, Warning: ")

static MythUIHelper *mythui = NULL;
static QMutex uiLock;
QString MythUIHelper::x11_display;

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

    void GetScreenBounds(void);
    void StoreGUIsettings(void);

    double GetPixelAspectRatio(void);

    Settings *m_qtThemeSettings;   ///< Text/button/background colours, etc

    bool      m_themeloaded;       ///< Do we have a palette and pixmap to use?
    QString   m_menuthemepathname;
    QString   m_themepathname;
    QString   m_themename;
    QPalette  m_palette;           ///< Colour scheme

    float m_wmult, m_hmult;
    float m_pixelAspectRatio;

    // Drawable area of the full screen. May cover several screens,
    // or exclude windowing system fixtures (like Mac menu bar)
    int m_xbase, m_ybase;
    int m_height, m_width;

    // Dimensions of the theme
    int m_baseWidth, m_baseHeight;
    bool m_isWide;

    QMap<QString, MythImage*> imageCache;
    QMap<QString, uint> CacheTrack;
    QMutex *m_cacheLock;
    size_t m_cacheSize;
    QMutex *m_cacheSizeLock;

    uint maxImageCacheSize;

    // The part of the screen(s) allocated for the GUI. Unless
    // overridden by the user, defaults to drawable area above.
    int m_screenxbase, m_screenybase;

    // The part of the screen(s) allocated for the GUI. Unless
    // overridden by the user, defaults to drawable area above.
    int m_screenwidth, m_screenheight;

    // Command-line GUI size, which overrides both the above sets of sizes
    static int x_override, y_override, w_override, h_override;

    QString themecachedir;

    ScreenSaverControl *screensaver;
    bool screensaverEnabled;

    DisplayRes *display_res;
    bool screenSetup;

    QThreadPool *m_imageThreadPool;

    MythUIMenuCallbacks callbacks;

    MythUIHelper *parent;
};

int MythUIHelperPrivate::x_override = -1;
int MythUIHelperPrivate::y_override = -1;
int MythUIHelperPrivate::w_override = -1;
int MythUIHelperPrivate::h_override = -1;

MythUIHelperPrivate::MythUIHelperPrivate(MythUIHelper *p)
    : m_qtThemeSettings(new Settings()),
      m_themeloaded(false),
      m_wmult(1.0), m_hmult(1.0), m_pixelAspectRatio(-1.0),
      m_xbase(0), m_ybase(0), m_height(0), m_width(0),
      m_baseWidth(800), m_baseHeight(600), m_isWide(false),
      m_cacheLock(new QMutex(QMutex::Recursive)), m_cacheSize(0),
      m_cacheSizeLock(new QMutex(QMutex::Recursive)),
      m_screenxbase(0), m_screenybase(0), m_screenwidth(0), m_screenheight(0),
      screensaver(NULL), screensaverEnabled(false), display_res(NULL),
      screenSetup(false), m_imageThreadPool(new QThreadPool()), parent(p)
{
}

MythUIHelperPrivate::~MythUIHelperPrivate()
{
    QMutableMapIterator<QString, MythImage*> i(imageCache);
    while (i.hasNext())
    {
        i.next();
        i.value()->SetIsInCache(false);
        i.value()->DownRef();
        i.remove();
    }

    CacheTrack.clear();

    delete m_cacheLock;
    delete m_cacheSizeLock;
    delete m_imageThreadPool;
    delete m_qtThemeSettings;
    delete screensaver;
    if(display_res)
        DisplayRes::SwitchToDesktop();
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
    QDesktopWidget * desktop = QApplication::desktop();
    bool             hasXinerama = GetNumberXineramaScreens() > 1;
    int              numScreens  = desktop->numScreens();
    int              screen;

    if (hasXinerama)
    {
        VERBOSE(VB_GUI, LOC +
                QString("Total desktop dim: %1x%2, over %3 screen[s].")
                .arg(desktop->width()).arg(desktop->height()).arg(numScreens));
    }

    if (numScreens > 1)
    {
        for (screen = 0; screen < numScreens; ++screen)
        {
            QRect dim = desktop->screenGeometry(screen);
            VERBOSE(VB_GUI, LOC +
                    QString("Screen %1 dim: %2x%3.")
                    .arg(screen).arg(dim.width()).arg(dim.height()));
        }
    }

    screen = desktop->primaryScreen();
    VERBOSE(VB_GUI, LOC + QString("Primary screen: %1.").arg(screen));

    if (hasXinerama)
        screen = GetMythDB()->GetNumSetting("XineramaScreen", screen);

    if (screen == -1)       // Special case - span all screens
    {
        m_xbase  = 0;
        m_ybase  = 0;
        m_width  = desktop->width();
        m_height = desktop->height();

        VERBOSE(VB_GUI, LOC + QString("Using all %1 screens. ")
                .arg(numScreens) + QString("Dimensions: %1x%2")
                .arg(m_width).arg(m_height));

        return;
    }

    if (hasXinerama)        // User specified a single screen
    {
        if (screen < 0 || screen >= numScreens)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN + QString(
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
            VERBOSE(VB_GUI, LOC + QString("Running in a window"));

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

        VERBOSE(VB_GUI, LOC + QString("Using screen %1, %2x%3 at %4,%5")
                .arg(screen).arg(m_width).arg(m_height)
                .arg(m_xbase).arg(m_ybase));
    }
}

/**
 * Apply any user overrides to the screen geometry
 */
void MythUIHelperPrivate::StoreGUIsettings()
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

    m_screenxbase  = GetMythDB()->GetNumSetting("GuiOffsetX");
    m_screenybase  = GetMythDB()->GetNumSetting("GuiOffsetY");

    m_screenwidth = m_screenheight = 0;
    GetMythDB()->GetResolutionSetting("Gui", m_screenwidth, m_screenheight);

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
        VERBOSE(VB_IMPORTANT, LOC +
                "Somehow, your screen size settings are bad.\n\t\t\t" +
                QString("GuiResolution: %1\n\t\t\t")
                .arg(GetMythDB()->GetSetting("GuiResolution")) +
                QString("  old GuiWidth: %1\n\t\t\t")
                .arg(GetMythDB()->GetNumSetting("GuiWidth")) +
                QString("  old GuiHeight: %1\n\t\t\t")
                .arg(GetMythDB()->GetNumSetting("GuiHeight")) +
                QString("m_width: %1").arg(m_width) +
                QString("m_height: %1\n\t\t\t").arg(m_height) +
                "Falling back to 640x480");

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
    font.setStretch((int)(100 / GetPixelAspectRatio()));

    QApplication::setFont(font);
}

double MythUIHelperPrivate::GetPixelAspectRatio(void)
{
    if (m_pixelAspectRatio < 0)
    {
        if (!display_res)
        {
            DisplayRes *dispRes = DisplayRes::GetDisplayRes(); // create singleton
            if (dispRes)
                m_pixelAspectRatio = dispRes->GetPixelAspectRatio();
            else
                m_pixelAspectRatio = 1.0;
        }
        else
            m_pixelAspectRatio = display_res->GetPixelAspectRatio();
    }

    return m_pixelAspectRatio;
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

    d->m_cacheSizeLock->lock();
    d->maxImageCacheSize = GetMythDB()->GetNumSetting("UIImageCacheSize", 20)
                                * 1024 * 1024;
    d->m_cacheSizeLock->unlock();

    VERBOSE(VB_GUI, LOC + QString("MythUI Image Cache size set to %1 bytes")
            .arg(d->maxImageCacheSize));
}

MythUIMenuCallbacks *MythUIHelper::GetMenuCBs(void)
{
    return &(d->callbacks);
}

bool MythUIHelper::IsScreenSetup(void)
{
    return d->screenSetup;
}

bool MythUIHelper::IsTopScreenInitialized(void)
{
    return GetMythMainWindow()->GetMainStack()->GetTopScreen()->IsInitialized();
}

void MythUIHelper::LoadQtConfig(void)
{
    gCoreContext->ResetLanguage();
    d->themecachedir.clear();

    if (GetMythDB()->GetNumSetting("UseVideoModes", 0))
    {
        DisplayRes *dispRes = DisplayRes::GetDisplayRes(); // create singleton
        if (dispRes)
        {
            d->display_res = dispRes;
            // Make sure DisplayRes has current context info
            d->display_res->Initialize();
            // Switch to desired GUI resolution
            d->display_res->SwitchToGUI();
        }
    }

    // Note the possibly changed screen settings
    d->GetScreenBounds();

    delete d->m_qtThemeSettings;

    d->m_qtThemeSettings = new Settings;

    qApp->setStyle("Windows");

    QString themename = GetMythDB()->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = FindThemeDir(themename);

    ThemeInfo *themeinfo = new ThemeInfo(themedir);

    if (themeinfo)
    {
        d->m_isWide = themeinfo->IsWide();
        d->m_baseWidth = themeinfo->GetBaseRes()->width();
        d->m_baseHeight = themeinfo->GetBaseRes()->height();
        d->m_themename = themeinfo->GetName();
        VERBOSE(VB_GUI, LOC + QString("Using theme base resolution of %1x%2")
                .arg(d->m_baseWidth).arg(d->m_baseHeight));
        delete themeinfo;
    }

    // Recalculate GUI dimensions
    d->StoreGUIsettings();

    d->m_themepathname = themedir + '/';

    themedir += "/qtlook.txt";
    d->m_qtThemeSettings->ReadSettings(themedir);
    d->m_themeloaded = false;

    themename = GetMythDB()->GetSetting("MenuTheme", "defaultmenu");
    if (themename == "default")
        themename = "defaultmenu";
    d->m_menuthemepathname = FindMenuThemeDir(themename) + '/';
}

Settings *MythUIHelper::qtconfig(void)
{
    return d->m_qtThemeSettings;
}

void MythUIHelper::UpdateImageCache(void)
{
    QMutexLocker locker(d->m_cacheLock);

    QMutableMapIterator<QString, MythImage*> i(d->imageCache);
    while (i.hasNext())
    {
        i.next();
        i.value()->SetIsInCache(false);
        i.value()->DownRef();
        i.remove();
    }

    d->CacheTrack.clear();

    d->m_cacheSizeLock->lock();
    d->m_cacheSize = 0;
    d->m_cacheSizeLock->unlock();

    ClearOldImageCache();
}

MythImage *MythUIHelper::GetImageFromCache(const QString &url)
{
    QMutexLocker locker(d->m_cacheLock);
    if (d->imageCache.contains(url))
    {
        d->CacheTrack[url] = QDateTime::currentDateTime().toTime_t();
        return d->imageCache[url];
    }

/*
    if (QFileInfo(url).exists())
    {
        MythImage *im = GetMythPainter()->GetFormatImage();
        im->Load(url,false);
        return im;
    }
*/

    return NULL;
}

void MythUIHelper::IncludeInCacheSize(MythImage *im)
{
    if (!im)
        return;

    d->m_cacheSizeLock->lock();
    d->m_cacheSize += im->numBytes();
    d->m_cacheSizeLock->unlock();
}

void MythUIHelper::ExcludeFromCacheSize(MythImage *im)
{
    if (!im)
        return;

    d->m_cacheSizeLock->lock();
    d->m_cacheSize -= im->numBytes();
    d->m_cacheSizeLock->unlock();
}

MythImage *MythUIHelper::CacheImage(const QString &url, MythImage *im,
                                    bool nodisk)
{
    if (!im)
        return NULL;

    if (!nodisk)
    {
        QString dstfile = GetMythUI()->GetThemeCacheDir() + '/' + url;

        VERBOSE(VB_GUI|VB_FILE,
                LOC + QString("Saved to Cache (%1)").arg(dstfile));

        // This would probably be better off somewhere else before any
        // Load() calls at all.
        QDir themedir(GetMythUI()->GetThemeCacheDir());
        if (!themedir.exists())
            themedir.mkdir(GetMythUI()->GetThemeCacheDir());

        // Save to disk cache
        im->save(dstfile,"PNG");
    }

    // delete the oldest cached images until we fall below threshold.
    QMutexLocker locker(d->m_cacheLock);
    d->m_cacheSizeLock->lock();
    while (d->m_cacheSize + im->numBytes() >= d->maxImageCacheSize &&
           d->imageCache.size())
    {
        d->m_cacheSizeLock->unlock();
        QMap<QString, MythImage*>::iterator it = d->imageCache.begin();
        uint oldestTime = QDateTime::currentDateTime().toTime_t();
        QString oldestKey = it.key();

        int count = 0;
        for ( ; it != d->imageCache.end(); ++it)
        {
            if (d->CacheTrack[it.key()] < oldestTime &&
                (it.value()->RefCount() == 1))
            {
                oldestTime = d->CacheTrack[it.key()];
                oldestKey = it.key();
                count++;
            }
        }

        VERBOSE(VB_GUI|VB_FILE, LOC +
                QString("%1 images are eligible for expiry").arg(count));

        if (count > 0)
        {
            VERBOSE(VB_GUI|VB_FILE, LOC +
                    QString("Cache too big (%1), removing :%2:")
                    .arg(d->m_cacheSize + im->numBytes()).arg(oldestKey));

            d->imageCache[oldestKey]->DownRef();
            d->imageCache.remove(oldestKey);
            d->CacheTrack.remove(oldestKey);

            d->m_cacheSizeLock->lock();
        }
        else
        {
            d->m_cacheSizeLock->lock();
            break;
        }
    }
    d->m_cacheSizeLock->unlock();

    QMap<QString, MythImage*>::iterator it = d->imageCache.find(url);
    if (it == d->imageCache.end())
    {
        im->UpRef();
        d->imageCache[url] = im;
        d->CacheTrack[url] = QDateTime::currentDateTime().toTime_t();

        im->SetIsInCache(true);
        VERBOSE(VB_GUI|VB_FILE, LOC +
                QString("NOT IN RAM CACHE, Adding, and adding to size "
                        ":%1: :%2:").arg(url).arg(im->numBytes()));
    }

    VERBOSE(VB_GUI|VB_FILE, LOC +
            QString("MythUIHelper::CacheImage : Cache Count = :%1: "
                    "size :%2:")
            .arg(d->imageCache.count()).arg(d->m_cacheSize));

    return d->imageCache[url];
}

void MythUIHelper::RemoveFromCacheByURL(const QString &url)
{
    QMutexLocker locker(d->m_cacheLock);
    QMap<QString, MythImage*>::iterator it = d->imageCache.find(url);

    if (it != d->imageCache.end())
    {
        d->imageCache[url]->SetIsInCache(false);
        d->imageCache[url]->DownRef();
        d->imageCache.remove(url);
        d->CacheTrack.remove(url);
    }

    QString dstfile;

    dstfile = GetThemeCacheDir() + '/' + url;
    VERBOSE(VB_GUI|VB_FILE, LOC +
            QString("RemoveFromCacheByURL removed :%1: "
                    "from cache").arg(dstfile));
    QFile::remove(dstfile);
}

void MythUIHelper::RemoveFromCacheByFile(const QString &fname)
{
    QList<QString>::iterator it;

    QString partialKey = fname;
    partialKey.replace('/', '-');

    d->m_cacheLock->lock();
    QList<QString> imageCacheKeys = d->imageCache.keys();
    d->m_cacheLock->unlock();
    for (it = imageCacheKeys.begin(); it != imageCacheKeys.end(); ++it)
    {
        if ((*it).contains(partialKey))
            RemoveFromCacheByURL(*it);
    }

    // Loop through files to cache any that were not caught by
    // RemoveFromCacheByURL
    QDir dir(GetThemeCacheDir());
    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i)
    {
        QFileInfo fileInfo = list.at(i);
        if (fileInfo.fileName().contains(partialKey))
        {
            VERBOSE(VB_GUI|VB_FILE, LOC +
                    QString("RemoveFromCacheByFile removed: %1: "
                            "from cache").arg(fileInfo.fileName()));
            if (!dir.remove(fileInfo.fileName()))
                VERBOSE(VB_IMPORTANT, QString("Failed to delete %1 from the "
                                              "theme cache")
                                              .arg(fileInfo.fileName()));
        }
     }
}

bool MythUIHelper::IsImageInCache(const QString &url)
{
    QMutexLocker locker(d->m_cacheLock);

    if (d->imageCache.contains(url))
        return true;

    if (QFileInfo(url).exists())
        return true;

    return false;
}

QString MythUIHelper::GetThemeCacheDir(void)
{
    QString cachedirname = GetConfDir() + "/themecache/";

    QString tmpcachedir = cachedirname +
                          GetMythDB()->GetSetting("Theme", DEFAULT_UI_THEME) +
                          "." + QString::number(d->m_screenwidth) +
                          "." + QString::number(d->m_screenheight);

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

    d->themecachedir += '/';

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

    // Cache two themes/resolutions to allow sampling other themes without
    // incurring a penalty. Especially for those writing new themes or testing
    // changes of an existing theme. The space used is neglible when compared
    // against the average video
    while ((size_t)dirtimes.size() >= 2)
    {
        VERBOSE(VB_GUI|VB_FILE, LOC + QString("Removing cache dir: %1")
                .arg(dirtimes.begin().value()));

        RemoveCacheDir(dirtimes.begin().value());
        dirtimes.erase(dirtimes.begin());
    }

    QMap<QDateTime, QString>::const_iterator dit = dirtimes.begin();
    for (; dit != dirtimes.end(); ++dit)
    {
        VERBOSE(VB_GUI|VB_FILE, LOC +
                QString("Keeping cache dir: %1").arg(*dit));
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
void MythUIHelper::ParseGeometryOverride(const QString &geometry)
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Geometry does not match either form -\n\t\t\t"
                "WIDTHxHEIGHT or WIDTHxHEIGHT+XOFF+YOFF");
        return;
    }

    bool parsed;
    int tmp_w, tmp_h;

    tmp_w = geo[1].toInt(&parsed);
    if (!parsed)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not parse width of geometry override");
    }

    if (parsed)
    {
        tmp_h = geo[2].toInt(&parsed);
        if (!parsed)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Could not parse height of geometry override");
        }
    }

    if (parsed)
    {
        MythUIHelperPrivate::w_override = tmp_w;
        MythUIHelperPrivate::h_override = tmp_h;
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Overriding GUI size: width=%1 height=%2")
                .arg(tmp_w).arg(tmp_h));
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to override GUI size.");
    }

    if (longForm)
    {
        int tmp_x, tmp_y;
        tmp_x = geo[3].toInt(&parsed);
        if (!parsed)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Could not parse horizontal offset of geometry override");
        }

        if (parsed)
        {
            tmp_y = geo[4].toInt(&parsed);
            if (!parsed)
            {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Could not parse vertical offset of geometry override");
            }
        }

        if (parsed)
        {
            MythUIHelperPrivate::x_override = tmp_x;
            MythUIHelperPrivate::y_override = tmp_y;
            VERBOSE(VB_GENERAL, LOC +
                    QString("Overriding GUI offset: x=%1 y=%2")
                    .arg(tmp_x).arg(tmp_y));
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to override GUI offset.");
        }
    }
}

bool MythUIHelper::IsGeometryOverridden(void)
{
    return (MythUIHelperPrivate::x_override >= 0 ||
            MythUIHelperPrivate::y_override >= 0 ||
            MythUIHelperPrivate::w_override >= 0 ||
            MythUIHelperPrivate::h_override >= 0);
}

/**
 *  \brief Returns the full path to the theme denoted by themename
 *
 *   If the theme cannot be found falls back to the DEFAULT_UI_THEME.
 *   If the DEFAULT_UI_THEME doesn't exist then returns an empty string.
 *  \param themename The theme name.
 *  \return Path to theme or empty string.
 */
QString MythUIHelper::FindThemeDir(const QString &themename)
{
    QString testdir;
    QDir dir;

    if (!themename.isEmpty())
    {
        testdir = GetConfDir() + "/themes/" + themename;

        dir.setPath(testdir);
        if (dir.exists())
            return testdir;

        testdir = GetThemesParentDir() + themename;
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;

        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("No theme dir: '%1'")
                .arg(dir.absolutePath()));
    }

    testdir = GetThemesParentDir() + DEFAULT_UI_THEME;
    dir.setPath(testdir);
    if (dir.exists())
    {
        VERBOSE(VB_IMPORTANT, QString("Could not find theme: %1 - "
                "Switching to %2").arg(themename).arg(DEFAULT_UI_THEME));
        GetMythDB()->OverrideSettingForSession("Theme", DEFAULT_UI_THEME);
        return testdir;
    }

    VERBOSE(VB_IMPORTANT, LOC_WARN +
            QString("No default theme dir: '%1'")
            .arg(dir.absolutePath()));

    testdir = GetThemesParentDir() + FALLBACK_UI_THEME;
    dir.setPath(testdir);
    if (dir.exists())
    {
        VERBOSE(VB_IMPORTANT, QString("Could not find theme: %1 - "
                "Switching to %2").arg(themename).arg(FALLBACK_UI_THEME));
        GetMythDB()->OverrideSettingForSession("Theme", FALLBACK_UI_THEME);
        return testdir;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("No fallback GUI theme dir: '%1'")
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
        VERBOSE(VB_IMPORTANT, LOC + QString(
                    "Could not find menu theme: %1 - Switching to default")
                .arg(menuname));

        GetMythDB()->SaveSetting("MenuTheme", "default");
        return testdir;
    }

    VERBOSE(VB_IMPORTANT, LOC + QString(
                "Could not find menu theme: %1 - Fallback to default failed.")
            .arg(menuname));

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

QString MythUIHelper::GetThemeName(void)
{
    return d->m_themename;
}

QStringList MythUIHelper::GetThemeSearchPath(void)
{
    QStringList searchpath;

    searchpath.append(GetThemeDir());
    if (d->m_isWide)
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
    else if (!d->m_qtThemeSettings
                 ->GetSetting("TiledBackgroundPixmap").isEmpty())
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

    delete bgpixmap;
}

bool MythUIHelper::FindThemeFile(QString &path)
{
    QFileInfo fi(path);
    if (fi.isAbsolute() && fi.exists())
        return true;

    QString file;
    bool foundit = false;
    const QStringList searchpath = GetThemeSearchPath();
    for (QStringList::const_iterator ii = searchpath.begin();
        ii != searchpath.end(); ++ii)
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
    VERBOSE(VB_GUI|VB_FILE, LOC + QString("LoadScaleImage(%1)").arg(filename));

    if (filename.isEmpty() || filename == "none")
        return NULL;

    if ((!FindThemeFile(filename)) &&
        (!filename.startsWith("http://")) &&
        (!filename.startsWith("https://")) &&
        (!filename.startsWith("ftp://")) &&
        (!filename.startsWith("myth://")))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("LoadScaleImage(%1)")
                .arg(filename) + "Unable to find image file");

        return NULL;
    }

    QImage *ret = NULL;
    QImage tmpimage;
    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (filename.startsWith("myth://"))
    {
        RemoteFile *rf = new RemoteFile(filename, false, false, 0);

        QByteArray data;
        bool loaded = rf->SaveAs(data);
        delete rf;

        if (loaded)
            tmpimage.loadFromData(data);
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("LoadScaleImage(%1) failed to load remote image")
                    .arg(filename));
        }
    }
    else if ((filename.startsWith("http://")) ||
             (filename.startsWith("https://")) ||
             (filename.startsWith("ftp://")))
    {
        QByteArray data;
        if (GetMythDownloadManager()->download(filename, &data))
            tmpimage.loadFromData(data);
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("LoadScaleImage(%1) failed to load remote image")
                    .arg(filename));
        }
    }
    else
    {
        tmpimage.load(filename);
    }

    if (width != d->m_baseWidth || height != d->m_baseHeight)
    {
        if (tmpimage.isNull())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("LoadScaleImage(%1) failed to load image")
                    .arg(filename));

            return NULL;
        }

        QImage tmp2 = tmpimage.scaled(
            (int)(tmpimage.width() * wmult),
            (int)(tmpimage.height() * hmult),
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        ret = new QImage(tmp2);
    }
    else
    {
        ret = new QImage(tmpimage);
        if (!ret->width() || !ret->height())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("LoadScaleImage(%1) invalid image dimensions")
                    .arg(filename));

            delete ret;
            return NULL;
        }
    }

    return ret;
}

QPixmap *MythUIHelper::LoadScalePixmap(QString filename, bool fromcache)
{
    VERBOSE(VB_GUI|VB_FILE, LOC + QString("LoadScalePixmap(%1)").arg(filename));

    if (filename.isEmpty() || filename == "none")
        return NULL;

    if (!FindThemeFile(filename) && (!filename.startsWith("myth:")))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("LoadScalePixmap(%1)")
                .arg(filename) + "Unable to find image file");

        return NULL;
    }

    QPixmap *ret = NULL;
    QImage tmpimage;
    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);
    if (filename.startsWith("myth://"))
    {
        RemoteFile *rf = new RemoteFile(filename, false, false, 0);

        QByteArray data;
        bool loaded = rf->SaveAs(data);
        delete rf;

        if (loaded)
        {
            tmpimage.loadFromData(data);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("LoadScalePixmap(%1): failed to load remote image")
                    .arg(filename));
        }
    }
    else
    {
        tmpimage.load(filename);
    }

    if (width != d->m_baseWidth || height != d->m_baseHeight)
    {
        if (tmpimage.isNull())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("LoadScalePixmap(%1) failed to load image")
                    .arg(filename));

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
        ret = new QPixmap(QPixmap::fromImage(tmpimage));
        if (!ret->width() || !ret->height())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("LoadScalePixmap(%1) invalid image dimensions")
                    .arg(filename));

            delete ret;
            return NULL;
        }
    }

    return ret;
}

MythImage *MythUIHelper::LoadCacheImage(QString srcfile, QString label,
                                        ImageCacheMode cacheMode)
{
    VERBOSE(VB_GUI|VB_FILE, LOC +
            QString("LoadCacheImage(%1,%2)").arg(srcfile).arg(label));

    if (srcfile.isEmpty() || label.isEmpty())
        return NULL;

    if (!(kCacheForceStat & cacheMode))
    {
        // Some screens include certain images dozens or even hundreds of
        // times.  Even if the image is in the cache, there is still a
        // stat system call on the original file to see if it has changed.
        // This code relaxes the original-file check so that the check
        // isn't repeated if it was already done within kImageCacheTimeout
        // seconds.
        const uint kImageCacheTimeout = 5;
        uint now = QDateTime::currentDateTime().toTime_t();
        if (d->imageCache.contains(label) &&
            d->CacheTrack[label] + kImageCacheTimeout > now)
        {
            return d->imageCache[label];
        }
    }

    QString cachefilepath = GetThemeCacheDir() + '/' + label;
    QFileInfo fi(cachefilepath);

    MythImage *ret = NULL;

    if (!!(cacheMode & kCacheIgnoreDisk) || fi.exists())
    {
        // Now compare the time on the source versus our cached copy
        if (!(cacheMode & kCacheIgnoreDisk))
            FindThemeFile(srcfile);

        QDateTime srcLastModified;
        QFileInfo original(srcfile);

        if ((srcfile.startsWith("http://")) ||
            (srcfile.startsWith("https://")) ||
            (srcfile.startsWith("ftp://")))
        {
            srcLastModified =
                GetMythDownloadManager()->GetLastModified(srcfile);
        }
        else if (srcfile.startsWith("myth://"))
            srcLastModified = RemoteFile::LastModified(srcfile);
        else if (original.exists())
            srcLastModified = original.lastModified();

        if (!!(cacheMode & kCacheIgnoreDisk) ||
            (fi.lastModified() > srcLastModified))
        {
            // Check Memory Cache
            ret = GetImageFromCache(label);
            if (!ret && (!!(kCacheNormal & cacheMode)))
            {
                // Load file from disk cache to memory cache
                ret = GetMythPainter()->GetFormatImage();
                if (!ret->Load(cachefilepath, false))
                {
                    VERBOSE(VB_GUI|VB_FILE, LOC_WARN +
                            QString("LoadCacheImage: "
                                    "Could not load :%1").arg(cachefilepath));

                    ret->DownRef();
                    ret = NULL;
                }
                else
                {
                    // Add to ram cache, and skip saving to disk since that is
                    // where we found this in the first place.
                    CacheImage(label, ret, true);
                }
            }
        }
        else
        {
            // If file has changed on disk, then remove it from the memory
            // and disk cache
            RemoveFromCacheByURL(label);
        }
    }

    return ret;
}

QFont MythUIHelper::GetBigFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(25));
    font.setWeight(QFont::Bold);

    return font;
}

QFont MythUIHelper::GetMediumFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(16));
    font.setWeight(QFont::Bold);

    return font;
}

QFont MythUIHelper::GetSmallFont(void)
{
    QFont font = QApplication::font();
    font.setPointSize(GetMythMainWindow()->NormalizeFontSize(12));
    font.setWeight(QFont::Bold);

    return font;
}

void MythUIHelper::DisableScreensaver(void)
{
    if (QApplication::type() == QApplication::GuiClient)
    {
        QCoreApplication::postEvent(
            GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetDisable));
    }
}

void MythUIHelper::RestoreScreensaver(void)
{
    if (QApplication::type() == QApplication::GuiClient)
    {
        QCoreApplication::postEvent(
            GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetRestore));
    }
}

void MythUIHelper::ResetScreensaver(void)
{
    if (QApplication::type() == QApplication::GuiClient)
    {
        QCoreApplication::postEvent(
            GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetReset));
    }
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

void MythUIHelper::AddCurrentLocation(QString location)
{
    QMutexLocker locker(&m_locationLock);
    if (m_currentLocation.isEmpty() || m_currentLocation.last() != location)
        m_currentLocation.push_back(location);
}

QString MythUIHelper::RemoveCurrentLocation(void)
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

QThreadPool *MythUIHelper::GetImageThreadPool(void)
{
    return d->m_imageThreadPool;
}

double MythUIHelper::GetPixelAspectRatio(void) const
{
    return d->GetPixelAspectRatio();
}

QSize MythUIHelper::GetBaseSize(void) const
{
    return QSize(d->m_baseWidth, d->m_baseHeight);
}
