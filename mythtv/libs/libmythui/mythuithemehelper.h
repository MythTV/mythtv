#ifndef MYTHUITHEMEHELPER_H
#define MYTHUITHEMEHELPER_H

// MythTV
#include "themeinfo.h"

static constexpr const char* DEFAULT_UI_THEME  { "MythCenter-wide" };
static constexpr const char* FALLBACK_UI_THEME { "Terra"           };

class MUI_PUBLIC MythUIThemeHelper
{
  public:
    void        InitThemeHelper();
    QString     FindThemeDir(const QString& ThemeName, bool Fallback = true);
    QString     FindMenuThemeDir(const QString& MenuName);
    bool        FindThemeFile(QString& Path);
    QString     GetThemeDir();
    QString     GetThemeName();
    QStringList GetThemeSearchPath();
    QString     GetMenuThemeDir();
    QSize       GetBaseSize() const;
    QList<ThemeInfo> GetThemes(ThemeType Type);

  private:
    QStringList m_searchPaths;
    QString     m_menuthemepathname;
    QString     m_themepathname;
    QString     m_themename;
    QString     m_userThemeDir;
    QSize       m_baseSize { 800, 600 };
    bool        m_isWide   { false    };
};

#endif
