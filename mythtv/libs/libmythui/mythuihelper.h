#ifndef MYTHUIHELPERS_H_
#define MYTHUIHELPERS_H_

#include <QString>
#include <QFont>
#include <QMutex>

#include "mythexp.h"

class MythUIHelperPrivate;
class MythImage;
class QImage;
class QWidget;
class Settings;
class QPixmap;

struct MPUBLIC MythUIMenuCallbacks
{
    void (*exec_program)(const QString &cmd);
    void (*exec_program_tv)(const QString &cmd);
    void (*configplugin)(const QString &cmd);
    void (*plugin)(const QString &cmd);
    void (*eject)(void);
    bool (*password_dialog)(const QString &text, const QString &pwd);
};

class MPUBLIC MythUIHelper
{
  public:
    void Init(MythUIMenuCallbacks &cbs);

    MythUIMenuCallbacks *GetMenuCBs(void);

    void LoadQtConfig(void);
    void UpdateImageCache(void);

    MythImage *GetImageFromCache(const QString &url);
    MythImage *CacheImage(const QString &url, MythImage *im,
                          bool nodisk = false);
    void RemoveFromCacheByURL(const QString &url);
    void RemoveFromCacheByFile(const QString &fname);
    bool IsImageInCache(const QString &url);
    QString GetThemeCacheDir(void);

    void IncludeInCacheSize(MythImage *im);
    void ExcludeFromCacheSize(MythImage *im);

    Settings *qtconfig(void);

    bool IsScreenSetup(void);

    // which the user may have set to be different from the raw screen size
    void GetScreenSettings(float &wmult, float &hmult);
    void GetScreenSettings(int &width, float &wmult,
                           int &height, float &hmult);
    void GetScreenSettings(int &xbase, int &width, float &wmult,
                           int &ybase, int &height, float &hmult);

    // This returns the raw (drawable) screen size
    void GetScreenBounds(int &xbase, int &ybase, int &width, int &height);

    // Parse an X11 style command line (-geometry) string
    bool ParseGeometryOverride(const QString geometry);
    bool IsGeometryOverridden(void);

    QPixmap *LoadScalePixmap(QString filename, bool fromcache = true);
    QImage *LoadScaleImage(QString filename, bool fromcache = true);
    MythImage *LoadCacheImage(QString srcfile, QString label);

    void ThemeWidget(QWidget *widget);

    int  NormalizeFontSize(const int pointSize);

    QString FindThemeDir(const QString &themename);
    QString FindMenuThemeDir(const QString &menuname);
    QString GetThemeDir(void);
    QList<QString> GetThemeSearchPath(void);
    QString GetMenuThemeDir(void);

    bool FindThemeFile(QString &filename);

    QString GetLanguage(void);
    QString GetLanguageAndVariant(void);

    QFont GetBigFont(void);
    QFont GetMediumFont(void);
    QFont GetSmallFont(void);

    // event wrappers
    void DisableScreensaver(void);
    void RestoreScreensaver(void);
    // Reset screensaver idle time, for input events that X doesn't see
    // (e.g., lirc)
    void ResetScreensaver(void);

    // actually do it
    void DoDisableScreensaver(void);
    void DoRestoreScreensaver(void);
    void DoResetScreensaver(void);

    // get the current status
    bool GetScreensaverEnabled(void);
    bool GetScreenIsAsleep(void);

    static void SetX11Display(const QString &display);
    static QString GetX11Display(void);

    static QString x11_display;

    static MythUIHelper *getMythUI(void);
    static void destroyMythUI(void);

  protected:
    MythUIHelper();
   ~MythUIHelper();

  private:
    void SetPalette(QWidget *widget);
    void InitializeScreenSettings(void);

    void ClearOldImageCache(void);
    void RemoveCacheDir(const QString &dirname);

    MythUIHelperPrivate *d;

    size_t m_cacheSize;
};

MPUBLIC MythUIHelper *GetMythUI();
MPUBLIC void DestroyMythUI();

#endif

