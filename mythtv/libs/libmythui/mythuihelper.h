#ifndef MYTHUIHELPERS_H_
#define MYTHUIHELPERS_H_

#include <QStringList>
#include <QString>
#include <QFont>
#include <QMutex>

#include "mythuiexp.h"
#include "themeinfo.h"
#include "mythuithemecache.h"

#define DEFAULT_UI_THEME "MythCenter"
#define FALLBACK_UI_THEME "Terra"

class MythUIHelperPrivate;

struct MUI_PUBLIC MythUIMenuCallbacks
{
    void (*exec_program)(const QString &cmd);
    void (*exec_program_tv)(const QString &cmd);
    void (*configplugin)(const QString &cmd);
    void (*plugin)(const QString &cmd);
    void (*eject)();
};

class MUI_PUBLIC MythUIHelper : public MythUIThemeCache
{
  public:
    void Init(MythUIMenuCallbacks &cbs);
    void Init();

    MythUIMenuCallbacks *GetMenuCBs();

    void LoadQtConfig();
    bool IsScreenSetup();
    void UpdateScreenSettings();
    // which the user may have set to be different from the raw screen size
    QRect GetScreenSettings();
    void GetScreenSettings(QRect &Rect, float &XFactor, float &YFactor);
    void GetScreenSettings(float &XFactor, float &YFactor);

    // Parse an X11 style command line (-geometry) string
    static void ParseGeometryOverride(const QString &geometry);
    static bool IsGeometryOverridden();
    static QRect GetGeometryOverride();

    QString FindThemeDir(const QString &themename, bool doFallback = true);
    QString FindMenuThemeDir(const QString &menuname);
    QString GetThemeDir();
    QString GetThemeName();
    QStringList GetThemeSearchPath();
    QString GetMenuThemeDir();
    QList<ThemeInfo> GetThemes(ThemeType type);

    bool FindThemeFile(QString &path);

    static MythUIHelper *getMythUI();
    static void destroyMythUI();

    void AddCurrentLocation(const QString& location);
    QString RemoveCurrentLocation();
    QString GetCurrentLocation(bool fullPath = false, bool mainStackOnly = true);
    QSize GetBaseSize() const;

    void SetFontStretch(int stretch);
    int GetFontStretch() const;

  protected:
    MythUIHelper();
   ~MythUIHelper();

  private:
    MythUIHelperPrivate *d {nullptr}; // NOLINT(readability-identifier-naming)
    QMutex m_locationLock;
    QStringList m_currentLocation;
};

MUI_PUBLIC MythUIHelper *GetMythUI();
MUI_PUBLIC void DestroyMythUI();

#endif

