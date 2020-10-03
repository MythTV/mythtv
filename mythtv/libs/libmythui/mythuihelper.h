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
    bool IsScreenSetup() const;

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

  protected:
    Q_DISABLE_COPY(MythUIHelper)
    MythUIHelper() = default;
   ~MythUIHelper() = default;

  private:
    QMutex m_locationLock;
    QStringList m_currentLocation;

    QString   m_menuthemepathname;
    QString   m_themepathname;
    QString   m_themename;

    // Dimensions of the theme
    QSize m_baseSize  { 800, 600 };
    bool m_isWide     { false };
    QString m_userThemeDir;
    MythUIMenuCallbacks m_callbacks { nullptr,nullptr,nullptr,nullptr,nullptr };
    QStringList m_searchPaths;
    bool m_screenSetup { false };
};

MUI_PUBLIC MythUIHelper *GetMythUI();
MUI_PUBLIC void DestroyMythUI();

#endif

