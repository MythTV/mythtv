#ifndef MYTHUIHELPERS_H_
#define MYTHUIHELPERS_H_

#include <QString>
#include <QFont>

#include "mythexp.h"

class MythUIHelperPrivate;
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

    QImage *GetImageFromCache(const QString &url);
    QImage *CacheImage(const QString &url, QImage &im);

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
    void CacheThemeImages(void);
    void CacheThemeImagesDirectory(const QString &dirname,
                                   const QString &subdirname = "");
    void RemoveCacheDir(const QString &dirname);

    MythUIHelperPrivate *d;
};

MPUBLIC MythUIHelper *GetMythUI();
MPUBLIC void DestroyMythUI();

#endif

