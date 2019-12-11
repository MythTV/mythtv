#include "mythuihelper.h"

#include <cmath>
#include <unistd.h>

#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QPalette>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QPainter>
#include <QStyleFactory>
#include <QSize>
#include <QFile>
#include <QAtomicInt>
#include <QEventLoop>
#include <QTimer>
#include <QScreen>

// mythbase headers
#include "mythdirs.h"
#include "mythlogging.h"
#include "mythdownloadmanager.h"
#include "mythdb.h"
#include "remotefile.h"
#include "mythcorecontext.h"
#include "storagegroup.h"
#include "mythdate.h"
#include "mthreadpool.h"

// mythui headers
#include "mythprogressdialog.h"
#include "mythimage.h"
#include "screensaver.h"
#include "mythmainwindow.h"
#include "themeinfo.h"
#include "x11colors.h"
#include "mythdisplay.h"

#define LOC      QString("MythUIHelper: ")

static MythUIHelper *mythui = nullptr;
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

    // These directories should always exist.  Don't test first as
    // there's no harm in trying to create an existing directory.
    QDir dir;
    dir.mkdir(GetThemeBaseCacheDir());
    dir.mkdir(GetRemoteCacheDir());
    dir.mkdir(GetThumbnailDir());

    return mythui;
}

void MythUIHelper::destroyMythUI(void)
{
    MythUIHelper::PruneCacheDir(GetRemoteCacheDir());
    MythUIHelper::PruneCacheDir(GetThumbnailDir());
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

class MythUIHelperPrivate
{
public:
    explicit MythUIHelperPrivate(MythUIHelper *p)
    : m_cacheLock(new QMutex(QMutex::Recursive)),
      m_imageThreadPool(new MThreadPool("MythUIHelper")),
      m_parent(p) {}
    ~MythUIHelperPrivate();

    void Init();
    void GetScreenBounds(void);
    void StoreGUIsettings(void);
    double GetPixelAspectRatio(void);

    bool      m_themeloaded {false}; ///< Do we have a palette and pixmap to use?
    QString   m_menuthemepathname;
    QString   m_themepathname;
    QString   m_themename;
    QPalette  m_palette;           ///< Colour scheme

    float m_wmult                            {1.0F};
    float m_hmult                            {1.0F};
    float m_pixelAspectRatio                 {-1.0F};

    // Drawable area of the full screen. May cover several screens,
    // or exclude windowing system fixtures (like Mac menu bar)
    int m_xbase                              {0};
    int m_ybase                              {0};
    int m_height                             {0};
    int m_width                              {0};

    // Dimensions of the theme
    int m_baseWidth                          {800};
    int m_baseHeight                         {600};
    bool m_isWide                            {false};

    QMap<QString, MythImage *> m_imageCache;
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
    QMap<QString, uint> m_cacheTrack;
#else
    QMap<QString, qint64> m_cacheTrack;
#endif
    QMutex *m_cacheLock                      {nullptr};

#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
    QAtomicInt m_cacheSize                   {0};
    QAtomicInt m_maxCacheSize                {30 * 1024 * 1024};
#else
    // This change is because of the QImage change from byteCount() to
    // sizeInBytes(), the latter returning a 64bit value.
    QAtomicInteger<qint64> m_cacheSize       {0};
    QAtomicInteger<qint64> m_maxCacheSize    {30 * 1024 * 1024};
#endif

    // The part of the screen(s) allocated for the GUI. Unless
    // overridden by the user, defaults to drawable area above.
    int m_screenxbase                        {0};
    int m_screenybase                        {0};

    // The part of the screen(s) allocated for the GUI. Unless
    // overridden by the user, defaults to drawable area above.
    int m_screenwidth                        {0};
    int m_screenheight                       {0};

    // Command-line GUI size, which overrides both the above sets of sizes
    static int x_override;
    static int y_override;
    static int w_override;
    static int h_override;

    QString m_themecachedir;
    QString m_userThemeDir;

    ScreenSaverControl *m_screensaver        {nullptr};
    bool                m_screensaverEnabled {false};

    MythDisplay *m_display                   {nullptr};
    bool         m_screenSetup               {false};

    MThreadPool *m_imageThreadPool           {nullptr};

    MythUIMenuCallbacks m_callbacks          {nullptr,nullptr,nullptr,nullptr,nullptr};

    MythUIHelper *m_parent                   {nullptr};

    int m_fontStretch                        {100};

    QStringList m_searchPaths;
};

int MythUIHelperPrivate::x_override = -1;
int MythUIHelperPrivate::y_override = -1;
int MythUIHelperPrivate::w_override = -1;
int MythUIHelperPrivate::h_override = -1;

MythUIHelperPrivate::~MythUIHelperPrivate()
{
    QMutableMapIterator<QString, MythImage *> i(m_imageCache);

    while (i.hasNext())
    {
        i.next();
        i.value()->SetIsInCache(false);
        i.value()->DecrRef();
        i.remove();
    }

    m_cacheTrack.clear();

    delete m_cacheLock;
    delete m_imageThreadPool;
    delete m_screensaver;

    if (m_display)
    {
        if (m_display->UsingVideoModes())
            m_display->SwitchToDesktop();
        MythDisplay::AcquireRelease(false);
    }
}

void MythUIHelperPrivate::Init(void)
{
    if (!m_display)
        m_display = MythDisplay::AcquireRelease();
    m_screensaver = new ScreenSaverControl();
    GetScreenBounds();
    StoreGUIsettings();
    m_screenSetup = true;

    StorageGroup sgroup("Themes", gCoreContext->GetHostName());
    m_userThemeDir = sgroup.GetFirstDir(true);
}

/**
 * \brief Get screen size from Qt, respecting for user's multiple screen prefs
 *
 * If the windowing system environment has multiple screens, then use
 * QScreen::virtualSize() to get the size of the virtual desktop.
 * Otherwise QScreen::size() or QScreen::availableSize() will provide
 * the size of an individual screen.
 */
void MythUIHelperPrivate::GetScreenBounds()
{
    foreach (QScreen *screen, qGuiApp->screens())
    {
        QRect dim = screen->geometry();
        QString extra = MythDisplay::GetExtraScreenInfo(screen);
        LOG(VB_GUI, LOG_INFO, LOC + QString("Screen %1 dim: %2x%3 %4")
            .arg(screen->name())
            .arg(dim.width()).arg(dim.height())
            .arg(extra));
    }

    QScreen *primary = qGuiApp->primaryScreen();
    LOG(VB_GUI, LOG_INFO, LOC +
        QString("Primary screen: %1.").arg(primary->name()));

    int numScreens = m_display->GetScreenCount();
    QSize dim = primary->virtualSize();
    LOG(VB_GUI, LOG_INFO, LOC +
        QString("Total desktop dim: %1x%2, over %3 screen[s].")
        .arg(dim.width()).arg(dim.height()).arg(numScreens));

    if (MythDisplay::SpanAllScreens())
    {
        LOG(VB_GUI, LOG_INFO, LOC + QString("Using entire desktop."));
        m_xbase  = 0;
        m_ybase  = 0;
        m_width  = dim.width();
        m_height = dim.height();
        return;
    }

    QRect bounds;
    QScreen *screen = m_display->GetCurrentScreen();
    if (GetMythDB()->GetBoolSetting("RunFrontendInWindow", false))
    {
        LOG(VB_GUI, LOG_INFO, LOC + "Running in a window");
        // This doesn't include the area occupied by the
        // Windows taskbar, or the Mac OS X menu bar and Dock
        bounds = screen->availableGeometry();
    }
    else
    {
        bounds = screen->geometry();
    }
    m_xbase  = bounds.x();
    m_ybase  = bounds.y();
    m_width  = bounds.width();
    m_height = bounds.height();

    LOG(VB_GUI, LOG_INFO, LOC + QString("Using screen %1, %2x%3 at %4,%5")
        .arg(screen->name()).arg(m_width).arg(m_height)
        .arg(m_xbase).arg(m_ybase));
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
        LOG(VB_GENERAL, LOG_ERR, LOC +
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

    // Default font, _ALL_ fonts inherit from this!
    // e.g All fonts will be 19 pixels unless a new size is explicitly defined.
    QFont font = QFont("Arial");

    if (!font.exactMatch())
        font = QFont();

    font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
    font.setPixelSize(lroundf(19.0F * m_hmult));
    int stretch = (int)(100 / GetPixelAspectRatio());
    font.setStretch(stretch); // QT
    m_fontStretch = stretch; // MythUI

    QApplication::setFont(font);
}

double MythUIHelperPrivate::GetPixelAspectRatio(void)
{
    if (m_pixelAspectRatio < 0)
    {
        if (!m_display)
            m_display = MythDisplay::AcquireRelease();
        m_pixelAspectRatio = static_cast<float>(m_display->GetPixelAspectRatio());
    }
    return static_cast<double>(m_pixelAspectRatio);
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
    d->m_callbacks = cbs;

    d->m_maxCacheSize.fetchAndStoreRelease(
        GetMythDB()->GetNumSetting("UIImageCacheSize", 30) * 1024 * 1024);

    LOG(VB_GUI, LOG_INFO, LOC +
        QString("MythUI Image Cache size set to %1 bytes")
        .arg(d->m_maxCacheSize.fetchAndAddRelease(0)));
}

// This init is used for showing the startup UI that is shown
// before the database is initialized. The above init is called later,
// after the DB is available.
// This class does not mind being Initialized twice.
void MythUIHelper::Init(void)
{
    d->Init();
}

MythUIMenuCallbacks *MythUIHelper::GetMenuCBs(void)
{
    return &(d->m_callbacks);
}

bool MythUIHelper::IsScreenSetup(void)
{
    return d->m_screenSetup;
}

bool MythUIHelper::IsTopScreenInitialized(void)
{
    return GetMythMainWindow()->GetMainStack()->GetTopScreen()->IsInitialized();
}

void MythUIHelper::LoadQtConfig(void)
{
    gCoreContext->ResetLanguage();
    d->m_themecachedir.clear();

    if (!d->m_display)
        d->m_display = MythDisplay::AcquireRelease();

    // Switch to desired GUI resolution
    if (d->m_display->UsingVideoModes())
        d->m_display->SwitchToGUI(MythDisplay::GUI, true);

    // Note the possibly changed screen settings
    d->GetScreenBounds();

    qApp->setStyle("Windows");

    QString themename = GetMythDB()->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = FindThemeDir(themename);

    auto *themeinfo = new ThemeInfo(themedir);
    if (themeinfo)
    {
        d->m_isWide = themeinfo->IsWide();
        d->m_baseWidth = themeinfo->GetBaseRes()->width();
        d->m_baseHeight = themeinfo->GetBaseRes()->height();
        d->m_themename = themeinfo->GetName();
        LOG(VB_GUI, LOG_INFO, LOC +
            QString("Using theme base resolution of %1x%2")
            .arg(d->m_baseWidth).arg(d->m_baseHeight));
        delete themeinfo;
    }

    // Recalculate GUI dimensions
    d->StoreGUIsettings();

    d->m_themepathname = themedir + '/';
    d->m_searchPaths.clear();

    d->m_themeloaded = false;

    themename = GetMythDB()->GetSetting("MenuTheme", "defaultmenu");

    if (themename == "default")
        themename = "defaultmenu";

    d->m_menuthemepathname = FindMenuThemeDir(themename);
}

void MythUIHelper::UpdateImageCache(void)
{
    QMutexLocker locker(d->m_cacheLock);

    QMutableMapIterator<QString, MythImage *> i(d->m_imageCache);

    while (i.hasNext())
    {
        i.next();
        i.value()->SetIsInCache(false);
        i.value()->DecrRef();
        i.remove();
    }

    d->m_cacheTrack.clear();

    d->m_cacheSize.fetchAndStoreOrdered(0);

    ClearOldImageCache();
    PruneCacheDir(GetRemoteCacheDir());
    PruneCacheDir(GetThumbnailDir());
}

MythImage *MythUIHelper::GetImageFromCache(const QString &url)
{
    QMutexLocker locker(d->m_cacheLock);

    if (d->m_imageCache.contains(url))
    {
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        d->m_cacheTrack[url] = MythDate::current().toTime_t();
#else
        d->m_cacheTrack[url] = MythDate::current().toSecsSinceEpoch();
#endif
        d->m_imageCache[url]->IncrRef();
        return d->m_imageCache[url];
    }

    /*
        if (QFileInfo(url).exists())
        {
            MythImage *im = GetMythPainter()->GetFormatImage();
            im->Load(url,false);
            return im;
        }
    */

    return nullptr;
}

void MythUIHelper::IncludeInCacheSize(MythImage *im)
{
    if (im)
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
        d->m_cacheSize.fetchAndAddOrdered(im->byteCount());
#else
        d->m_cacheSize.fetchAndAddOrdered(im->sizeInBytes());
#endif
}

void MythUIHelper::ExcludeFromCacheSize(MythImage *im)
{
    if (im)
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
        d->m_cacheSize.fetchAndAddOrdered(-im->byteCount());
#else
        d->m_cacheSize.fetchAndAddOrdered(-im->sizeInBytes());
#endif
}

MythImage *MythUIHelper::CacheImage(const QString &url, MythImage *im,
                                    bool nodisk)
{
    if (!im)
        return nullptr;

    if (!nodisk)
    {
        QString dstfile = GetCacheDirByUrl(url) + '/' + url;

        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
            QString("Saved to Cache (%1)").arg(dstfile));

        // Save to disk cache
        im->save(dstfile, "PNG");
    }

    // delete the oldest cached images until we fall below threshold.
    QMutexLocker locker(d->m_cacheLock);

    while ((d->m_cacheSize.fetchAndAddOrdered(0) +
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
	   im->byteCount()
#else
	   im->sizeInBytes()
#endif
	    ) >=
           d->m_maxCacheSize.fetchAndAddOrdered(0) &&
           !d->m_imageCache.empty())
    {
        QMap<QString, MythImage *>::iterator it = d->m_imageCache.begin();
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        uint oldestTime = MythDate::current().toTime_t();
#else
        qint64 oldestTime = MythDate::current().toSecsSinceEpoch();
#endif
        QString oldestKey = it.key();

        int count = 0;

        for (; it != d->m_imageCache.end(); ++it)
        {
            if (d->m_cacheTrack[it.key()] < oldestTime)
            {
                if ((2 == it.value()->IncrRef()) && (it.value() != im))
                {
                    oldestTime = d->m_cacheTrack[it.key()];
                    oldestKey = it.key();
                    count++;
                }
                it.value()->DecrRef();
            }
        }

        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
            QString("%1 images are eligible for expiry").arg(count));

        if (count > 0)
        {
            LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
                QString("Cache too big (%1), removing :%2:")
                .arg(d->m_cacheSize.fetchAndAddOrdered(0) +
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
		     im->byteCount()
#else
		     im->sizeInBytes()
#endif
		     )
                .arg(oldestKey));

            d->m_imageCache[oldestKey]->SetIsInCache(false);
            d->m_imageCache[oldestKey]->DecrRef();
            d->m_imageCache.remove(oldestKey);
            d->m_cacheTrack.remove(oldestKey);
        }
        else
        {
            break;
        }
    }

    QMap<QString, MythImage *>::iterator it = d->m_imageCache.find(url);

    if (it == d->m_imageCache.end())
    {
        im->IncrRef();
        d->m_imageCache[url] = im;
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        d->m_cacheTrack[url] = MythDate::current().toTime_t();
#else
        d->m_cacheTrack[url] = MythDate::current().toSecsSinceEpoch();
#endif

        im->SetIsInCache(true);
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
            QString("NOT IN RAM CACHE, Adding, and adding to size :%1: :%2:")
            .arg(url)
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
	    .arg(im->byteCount())
#else
	    .arg(im->sizeInBytes())
#endif
	    );
    }

    LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
        QString("MythUIHelper::CacheImage : Cache Count = :%1: size :%2:")
        .arg(d->m_imageCache.count())
        .arg(d->m_cacheSize.fetchAndAddRelaxed(0)));

    return d->m_imageCache[url];
}

void MythUIHelper::RemoveFromCacheByURL(const QString &url)
{
    QMutexLocker locker(d->m_cacheLock);
    QMap<QString, MythImage *>::iterator it = d->m_imageCache.find(url);

    if (it != d->m_imageCache.end())
    {
        d->m_imageCache[url]->SetIsInCache(false);
        d->m_imageCache[url]->DecrRef();
        d->m_imageCache.remove(url);
        d->m_cacheTrack.remove(url);
    }

    QString dstfile;

    dstfile = GetCacheDirByUrl(url) + '/' + url;
    LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
        QString("RemoveFromCacheByURL removed :%1: from cache").arg(dstfile));
    QFile::remove(dstfile);
}

void MythUIHelper::RemoveFromCacheByFile(const QString &fname)
{
    QList<QString>::iterator it;

    QString partialKey = fname;
    partialKey.replace('/', '-');

    d->m_cacheLock->lock();
    QList<QString> m_imageCacheKeys = d->m_imageCache.keys();
    d->m_cacheLock->unlock();

    for (it = m_imageCacheKeys.begin(); it != m_imageCacheKeys.end(); ++it)
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
        const QFileInfo& fileInfo = list.at(i);

        if (fileInfo.fileName().contains(partialKey))
        {
            LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
                QString("RemoveFromCacheByFile removed: %1: from cache")
                .arg(fileInfo.fileName()));

            if (!dir.remove(fileInfo.fileName()))
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Failed to delete %1 from the theme cache")
                    .arg(fileInfo.fileName()));
        }
    }
}

bool MythUIHelper::IsImageInCache(const QString &url)
{
    QMutexLocker locker(d->m_cacheLock);

    if (d->m_imageCache.contains(url))
        return true;

    if (QFileInfo(url).exists())
        return true;

    return false;
}

QString MythUIHelper::GetThemeCacheDir(void)
{
    static QString s_oldcachedir;
    QString tmpcachedir = GetThemeBaseCacheDir() + "/" +
                          GetMythDB()->GetSetting("Theme", DEFAULT_UI_THEME) +
                          "." + QString::number(d->m_screenwidth) +
                          "." + QString::number(d->m_screenheight);

    if (tmpcachedir != s_oldcachedir)
    {
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
            QString("Creating cache dir: %1").arg(tmpcachedir));
        QDir dir;
        dir.mkdir(tmpcachedir);
        s_oldcachedir = tmpcachedir;
    }
    return tmpcachedir;
}

/**
 * Look at the url being read and decide whether the cached version
 * should go into the theme cache or the thumbnail cache.
 *
 * \param url The resource being read.
 * \returns The path name of the appropriate cache directory.
 */
QString MythUIHelper::GetCacheDirByUrl(const QString& url)
{
    if (url.startsWith("myth:") || url.startsWith("-"))
        return GetThumbnailDir();
    return GetThemeCacheDir();
}

void MythUIHelper::ClearOldImageCache(void)
{
    d->m_themecachedir = GetThemeCacheDir();

    QString themecachedir = d->m_themecachedir;

    d->m_themecachedir += '/';

    QDir dir(GetThemeBaseCacheDir());
    dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = dir.entryInfoList();

    QFileInfoList::const_iterator it = list.begin();
    QMap<QDateTime, QString> dirtimes;

    while (it != list.end())
    {
        const QFileInfo *fi = &(*it++);

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
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC + QString("Removing cache dir: %1")
            .arg(dirtimes.begin().value()));

        RemoveCacheDir(dirtimes.begin().value());
        dirtimes.erase(dirtimes.begin());
    }

    QMap<QDateTime, QString>::const_iterator dit = dirtimes.begin();

    for (; dit != dirtimes.end(); ++dit)
    {
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
            QString("Keeping cache dir: %1").arg(*dit));
    }
}

void MythUIHelper::RemoveCacheDir(const QString &dirname)
{
    QString cachedirname = GetThemeBaseCacheDir();

    if (!dirname.startsWith(cachedirname))
        return;

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Removing stale cache dir: %1").arg(dirname));

    QDir dir(dirname);

    if (!dir.exists())
        return;

    dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = dir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();

    while (it != list.end())
    {
        const QFileInfo *fi = &(*it++);

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

/**
 * Remove all files in the cache that haven't been accessed in a user
 * configurable number of days.  The default number of days is seven.
 *
 * \param dirname The directory to prune.
 */
void MythUIHelper::PruneCacheDir(const QString& dirname)
{
    int days = GetMythDB()->GetNumSetting("UIDiskCacheDays", 7);
    if (days == -1) {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Pruning cache directory: %1 is disabled").arg(dirname));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Pruning cache directory: %1").arg(dirname));
    QDateTime cutoff = MythDate::current().addDays(-days);
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
    qint64 cutoffsecs = cutoff.toMSecsSinceEpoch()/1000;
#else
    qint64 cutoffsecs = cutoff.toSecsSinceEpoch();
#endif

    LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
        QString("Removing files not accessed since %1")
        .arg(cutoff.toLocalTime().toString(Qt::ISODate)));

    int kept = 0;
    int deleted = 0;
    int errcnt = 0;
    QDir dir(dirname);
    dir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::NoSort);

    // Trying to save every cycle possible within this loop.  The
    // stat() call seems significantly faster than the fi.fileRead()
    // method.  The documentation for QFileInfo says that the
    // fi.absoluteFilePath() method has to query the file system, so
    // use fi.filePath() method here and then add the directory if
    // needed.  Using dir.entryList() and adding the dirname each time
    // is also slower just using dir.entryInfoList().
    foreach (const QFileInfo &fi, dir.entryInfoList())
    {
        struct stat buf {};
        QString fullname = fi.filePath();
        if (not fullname.startsWith('/'))
            fullname = dirname + "/" + fullname;
        QByteArray fullname8 = fullname.toLocal8Bit();
        int rc = stat(fullname8, &buf);
        if (rc == -1)
        {
            errcnt += 1;
            continue;
        }
        if (buf.st_atime >= cutoffsecs)
        {
            kept += 1;
            LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC +
                QString("%1 Keep   %2")
                .arg(fi.lastRead().toLocalTime().toString(Qt::ISODate))
                .arg(fi.fileName()));
            continue;
        }
        deleted += 1;
        LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC +
            QString("%1 Delete %2")
            .arg(fi.lastRead().toLocalTime().toString(Qt::ISODate))
            .arg(fi.fileName()));
        unlink(fullname8);
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Kept %1 files, deleted %2 files, stat error on %3 files")
        .arg(kept).arg(deleted).arg(errcnt));
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
        MythUIHelperPrivate::w_override = tmp_w;
        MythUIHelperPrivate::h_override = tmp_h;
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

        MythUIHelperPrivate::x_override = tmp_x;
        MythUIHelperPrivate::y_override = tmp_y;
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Overriding GUI offset: x=%1 y=%2")
            .arg(tmp_x).arg(tmp_y));
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
        testdir = d->m_userThemeDir + themename;

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

    testdir = d->m_userThemeDir + menuname;

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
    if (!d->m_searchPaths.isEmpty())
        return d->m_searchPaths;

    // traverse up the theme inheritance list adding their location to the search path
    QList<ThemeInfo> themeList = GetThemes(THEME_UI);
    bool found = true;
    QString themeName = d->m_themename;
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
                d->m_searchPaths.append(themedir + '/');
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

    if (d->m_isWide)
        d->m_searchPaths.append(GetThemesParentDir() + "default-wide/");

    d->m_searchPaths.append(GetThemesParentDir() + "default/");
    d->m_searchPaths.append("/tmp/");
    return d->m_searchPaths;
}

QList<ThemeInfo> MythUIHelper::GetThemes(ThemeType type)
{
    QFileInfoList fileList;
    QList<ThemeInfo> themeList;
    QDir themeDirs(GetThemesParentDir());
    themeDirs.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    themeDirs.setSorting(QDir::Name | QDir::IgnoreCase);

    fileList.append(themeDirs.entryInfoList());

    themeDirs.setPath(d->m_userThemeDir);

    fileList.append(themeDirs.entryInfoList());

    for (QFileInfoList::iterator it =  fileList.begin();
         it != fileList.end(); ++it)
    {
        QFileInfo  &theme = *it;

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

MythImage *MythUIHelper::LoadCacheImage(QString srcfile, const QString& label,
                                        MythPainter *painter,
                                        ImageCacheMode cacheMode)
{
    LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
        QString("LoadCacheImage(%1,%2)").arg(srcfile).arg(label));

    if (srcfile.isEmpty() || label.isEmpty())
        return nullptr;

    if (!(kCacheForceStat & cacheMode))
    {
        // Some screens include certain images dozens or even hundreds of
        // times.  Even if the image is in the cache, there is still a
        // stat system call on the original file to see if it has changed.
        // This code relaxes the original-file check so that the check
        // isn't repeated if it was already done within kImageCacheTimeout
        // seconds.

        // This only applies to the MEMORY cache
        const uint kImageCacheTimeout = 60;
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        uint now = MythDate::current().toTime_t();
#else
        qint64 now = MythDate::current().toSecsSinceEpoch();
#endif

        QMutexLocker locker(d->m_cacheLock);

        if (d->m_imageCache.contains(label) &&
            d->m_cacheTrack[label] + kImageCacheTimeout > now)
        {
            d->m_imageCache[label]->IncrRef();
            return d->m_imageCache[label];
        }
    }

    MythImage *ret = nullptr;

    // Check Memory Cache
    ret = GetImageFromCache(label);

    // If the image is in the memory or we are not ignoring the disk cache
    // then proceed to check whether the source file is newer than our cached
    // copy
    if (ret || !(cacheMode & kCacheIgnoreDisk))
    {
        // Create url to image in disk cache
        QString cachefilepath;
        cachefilepath = GetCacheDirByUrl(label) + '/' + label;
        QFileInfo cacheFileInfo(cachefilepath);

        // If the file isn't in the disk cache, then we don't want to bother
        // checking the last modified times of the original
        if (!cacheFileInfo.exists())
            return nullptr;

        // Now compare the time on the source versus our cached copy
        QDateTime srcLastModified;

        // For internet images this involves querying the headers of the remote
        // image. This is slow even without redownloading the whole image
        if ((srcfile.startsWith("http://")) ||
            (srcfile.startsWith("https://")) ||
            (srcfile.startsWith("ftp://")))
        {
            // If the image is in the memory cache then skip the last modified
            // check, since memory cached images are loaded in the foreground
            // this can cause an intolerable delay. The images won't stay in
            // the cache forever and so eventually they will be checked.
            if (ret)
                srcLastModified = cacheFileInfo.lastModified();
            else
            {
                srcLastModified =
                    GetMythDownloadManager()->GetLastModified(srcfile);
            }
        }
        else if (srcfile.startsWith("myth://"))
            srcLastModified = RemoteFile::LastModified(srcfile);
        else
        {
            if (!FindThemeFile(srcfile))
                return nullptr;

            QFileInfo original(srcfile);

            if (original.exists())
                srcLastModified = original.lastModified();
        }

        // Now compare the timestamps, if the cached image is newer than the
        // source image we can use it, otherwise we want to remove it from the
        // cache
        if (cacheFileInfo.lastModified() >= srcLastModified)
        {
            // If we haven't already loaded the image from the memory cache
            // and we're not ignoring the disk cache, then it's time to load
            // it from there instead
            if (!ret && (cacheMode == kCacheNormal))
            {

                if (painter)
                {
                    ret = painter->GetFormatImage();

                    // Load file from disk cache to memory cache
                    if (ret->Load(cachefilepath))
                    {
                        // Add to ram cache, and skip saving to disk since that is
                        // where we found this in the first place.
                        CacheImage(label, ret, true);
                    }
                    else
                    {
                        LOG(VB_GUI | VB_FILE, LOG_WARNING, LOC +
                            QString("LoadCacheImage: Could not load :%1")
                            .arg(cachefilepath));

                        ret->SetIsInCache(false);
                        ret->DecrRef();
                        ret = nullptr;
                    }
                }
            }
        }
        else
        {
            ret = nullptr;
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
    if (qobject_cast<QApplication*>(qApp))
    {
        QCoreApplication::postEvent(
            GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetDisable));
    }
}

void MythUIHelper::RestoreScreensaver(void)
{
    if (qobject_cast<QApplication*>(qApp))
    {
        QCoreApplication::postEvent(
            GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetRestore));
    }
}

void MythUIHelper::ResetScreensaver(void)
{
    if (qobject_cast<QApplication*>(qApp))
    {
        QCoreApplication::postEvent(
            GetMythMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetReset));
    }
}

void MythUIHelper::DoDisableScreensaver(void)
{
    if (d->m_screensaver)
    {
        d->m_screensaver->Disable();
        d->m_screensaverEnabled = false;
    }
}

void MythUIHelper::DoRestoreScreensaver(void)
{
    if (d->m_screensaver)
    {
        d->m_screensaver->Restore();
        d->m_screensaverEnabled = true;
    }
}

void MythUIHelper::DoResetScreensaver(void)
{
    if (d->m_screensaver)
    {
        d->m_screensaver->Reset();
        d->m_screensaverEnabled = false;
    }
}

bool MythUIHelper::GetScreensaverEnabled(void)
{
    return d->m_screensaverEnabled;
}

bool MythUIHelper::GetScreenIsAsleep(void)
{
    if (!d->m_screensaver)
        return false;

    return d->m_screensaver->Asleep();
}

/// This needs to be set before MythUIHelper is initialized so
/// that the MythUIHelper::Init() can detect Xinerama setups.
void MythUIHelper::SetX11Display(const QString &display)
{
    x11_display = display;
}

QString MythUIHelper::GetX11Display(void)
{
    return x11_display;
}

void MythUIHelper::AddCurrentLocation(const QString& location)
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

MThreadPool *MythUIHelper::GetImageThreadPool(void)
{
    return d->m_imageThreadPool;
}

double MythUIHelper::GetPixelAspectRatio(void) const
{
    return d->GetPixelAspectRatio();
}

QSize MythUIHelper::GetBaseSize(void) const
{
    return {d->m_baseWidth, d->m_baseHeight};
}

void MythUIHelper::SetFontStretch(int stretch)
{
    d->m_fontStretch = stretch;
}

int MythUIHelper::GetFontStretch(void) const
{
    return d->m_fontStretch;
}
