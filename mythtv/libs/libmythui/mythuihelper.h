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

class MythDisplay;

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
    QRect GetScreenRect();
    void GetThemeScales(float &XFactor, float &YFactor);

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
    Q_DISABLE_COPY(MythUIHelper)
    MythUIHelper() = default;
   ~MythUIHelper();

  private:
    void StoreGUIsettings();
    QMutex m_locationLock;
    QStringList m_currentLocation;

    QString   m_menuthemepathname;
    QString   m_themepathname;
    QString   m_themename;

    // The part of the screen(s) allocated for the GUI. Unless
    // overridden by the user, defaults to the full drawable area.
    QRect m_screenRect { 0, 0, 0, 0 };

    // Command-line GUI size, which overrides both the above sets of sizes
    static int x_override;
    static int y_override;
    static int w_override;
    static int h_override;

    float m_wmult     { 1.0F };
    float m_hmult     { 1.0F };
    int m_fontStretch { 100 };

    // Dimensions of the theme
    QSize m_baseSize  { 800, 600 };
    bool m_isWide     { false };
    QString m_userThemeDir;
    MythDisplay *m_display { nullptr };
    MythUIMenuCallbacks m_callbacks { nullptr,nullptr,nullptr,nullptr,nullptr };
    QStringList m_searchPaths;
    bool m_screenSetup { false };
};

MUI_PUBLIC MythUIHelper *GetMythUI();
MUI_PUBLIC void DestroyMythUI();

#endif

