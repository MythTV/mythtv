#ifndef MYTHUIHELPERS_H_
#define MYTHUIHELPERS_H_

#include <QStringList>
#include <QString>
#include <QFont>
#include <QMutex>

#include "mythuiexp.h"
#include "themeinfo.h"

#define DEFAULT_UI_THEME "MythCenter"
#define FALLBACK_UI_THEME "Terra"

class MythUIHelperPrivate;
class MThreadPool;
class MythPainter;
class MythImage;
class QImage;
class QWidget;
class QPixmap;
class QSize;

enum ImageCacheMode
{
    kCacheNormal          = 0x0,
    kCacheIgnoreDisk      = 0x1,
    kCacheCheckMemoryOnly = 0x2,
    kCacheForceStat       = 0x4,
};

struct MUI_PUBLIC MythUIMenuCallbacks
{
    void (*exec_program)(const QString &cmd);
    void (*exec_program_tv)(const QString &cmd);
    void (*configplugin)(const QString &cmd);
    void (*plugin)(const QString &cmd);
    void (*eject)(void);
};

class MUI_PUBLIC MythUIHelper
{
  public:
    void Init(MythUIMenuCallbacks &cbs);
    void Init(void);

    MythUIMenuCallbacks *GetMenuCBs(void);

    void LoadQtConfig(void);
    void UpdateImageCache(void);

    /// Returns a reference counted image base on the URL.
    /// \note The reference count is set for one use call DecrRef() to delete.
    MythImage *GetImageFromCache(const QString &url);
    MythImage *CacheImage(const QString &url, MythImage *im,
                          bool nodisk = false);
    void RemoveFromCacheByURL(const QString &url);
    void RemoveFromCacheByFile(const QString &fname);
    bool IsImageInCache(const QString &url);
    QString GetThemeCacheDir(void);
    QString GetCacheDirByUrl(const QString& url);

    void IncludeInCacheSize(MythImage *im);
    void ExcludeFromCacheSize(MythImage *im);

    bool IsScreenSetup(void);
    static bool IsTopScreenInitialized(void);

    void UpdateScreenSettings(void);
    // which the user may have set to be different from the raw screen size
    QRect GetScreenSettings(void);
    void GetScreenSettings(QRect &Rect, float &XFactor, float &YFactor);
    void GetScreenSettings(float &XFactor, float &YFactor);

    // Parse an X11 style command line (-geometry) string
    static void ParseGeometryOverride(const QString &geometry);
    static bool IsGeometryOverridden(void);
    static QRect GetGeometryOverride(void);

    /// Returns a reference counted image from the cache.
    /// \note The reference count is set for one use call DecrRef() to delete.
    MythImage *LoadCacheImage(QString srcfile, const QString& label,
                              MythPainter *painter,
                              ImageCacheMode cacheMode = kCacheNormal);

    QString FindThemeDir(const QString &themename, bool doFallback = true);
    QString FindMenuThemeDir(const QString &menuname);
    QString GetThemeDir(void);
    QString GetThemeName(void);
    QStringList GetThemeSearchPath(void);
    QString GetMenuThemeDir(void);
    QList<ThemeInfo> GetThemes(ThemeType type);

    bool FindThemeFile(QString &path);

    static QFont GetBigFont(void);
    static QFont GetMediumFont(void);
    static QFont GetSmallFont(void);

    // event wrappers
    static void DisableScreensaver(void);
    static void RestoreScreensaver(void);
    // Reset screensaver idle time, for input events that X doesn't see
    // (e.g., lirc)
    static void ResetScreensaver(void);

    // actually do it
    void DoDisableScreensaver(void);
    void DoRestoreScreensaver(void);
    void DoResetScreensaver(void);

    // get the current status
    bool GetScreensaverEnabled(void);
    bool GetScreenIsAsleep(void);

    static MythUIHelper *getMythUI(void);
    static void destroyMythUI(void);

    void AddCurrentLocation(const QString& location);
    QString RemoveCurrentLocation(void);
    QString GetCurrentLocation(bool fullPath = false, bool mainStackOnly = true);

    MThreadPool *GetImageThreadPool(void);
    QSize GetBaseSize(void) const;

    void SetFontStretch(int stretch);
    int GetFontStretch(void) const;

  protected:
    MythUIHelper();
   ~MythUIHelper();

  private:
    void InitializeScreenSettings(void);

    void ClearOldImageCache(void);
    void RemoveCacheDir(const QString &dirname);
    static void PruneCacheDir(const QString& dirname);

    MythUIHelperPrivate *d {nullptr}; // NOLINT(readability-identifier-naming)

    QMutex m_locationLock;
    QStringList m_currentLocation;
};

MUI_PUBLIC MythUIHelper *GetMythUI();
MUI_PUBLIC void DestroyMythUI();

#endif

