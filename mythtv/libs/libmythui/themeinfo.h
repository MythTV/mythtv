#ifndef THEMEINFO_H
#define THEMEINFO_H

#include <QString>
#include <QSize>
#include <QMetaType>
#include <QFileInfo>

#include "libmythbase/mythtypes.h"
#include "libmythui/mythuiexp.h"
#include "libmythui/xmlparsebase.h" // for VERBOSE_XML && Xml Parsing helpers

enum ThemeType : std::uint8_t {
    THEME_UNKN  = 0x00,
    THEME_UI    = 0x01,
    THEME_OSD   = 0x02,
    THEME_MENU  = 0x04
};

class MUI_PUBLIC ThemeInfo : public XMLParseBase
{
  public:
    explicit ThemeInfo(const QString& theme);
    ~ThemeInfo() = default;

    bool IsWide() const;
    QString GetAspect() const { return m_aspect; }
    QSize GetBaseRes() const { return m_baseres; }
    QString GetName() const { return m_name; }
    QString GetBaseTheme() const { return m_baseTheme; }
    QString GetDescription() const { return m_description; }
    QString GetErrata() const { return m_errata; }
    QString GetPreviewPath() const { return m_previewpath; }
    int GetType() const { return m_type; }
    int GetMajorVersion() const { return m_majorver; }
    int GetMinorVersion() const { return m_minorver; }

    QString GetDownloadURL() const { return m_downloadurl; }
    QString GetThemeWebSite() const { return m_themesite; }

    QString GetLocalURL() const { return m_themeurl; }
    QString GetDirectoryName() const;

    void ToMap(InfoMap &infoMap) const;

  private:
    bool parseThemeInfo();

    QString   m_themeurl;
    QFileInfo m_theme;
    QString   m_baseTheme;
    int       m_type         {THEME_UNKN};
    QString   m_aspect;
    QSize     m_baseres      {800,600};
    QString   m_name;
    QString   m_previewpath;
    QString   m_description;
    QString   m_errata;
    int       m_majorver     {0};
    int       m_minorver     {0};

    QString   m_authorName;
    QString   m_authorEmail;

    QString   m_downloadurl;  // URL to download theme package from
    QString   m_themesite;    // Theme's website
};

Q_DECLARE_METATYPE(ThemeInfo*)

#endif
