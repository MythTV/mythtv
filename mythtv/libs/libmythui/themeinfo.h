#ifndef THEMEINFO_H
#define THEMEINFO_H

#include <QString>
#include <QSize>
#include <QMetaType>
#include <QHash>
#include <QFileInfo>

#include "mythuiexp.h"

#include "xmlparsebase.h" // for VERBOSE_XML && Xml Parsing helpers

typedef enum {
    THEME_UNKN  = 0x00,
    THEME_UI    = 0x01,
    THEME_OSD   = 0x02,
    THEME_MENU  = 0x04
} ThemeType;

class MUI_PUBLIC ThemeInfo : public XMLParseBase
{
  public:
    ThemeInfo(QString theme);
    ~ThemeInfo();

    bool IsWide() const;
    QString GetAspect() const { return m_aspect; }
    const QSize   *GetBaseRes() const { return &m_baseres; }
    QString GetName() const { return m_name; }
    QString GetDescription() const { return m_description; }
    QString GetErrata() const { return m_errata; }
    QString GetPreviewPath() const { return m_previewpath; }
    int GetType() const { return m_type; }
    int GetMajorVersion() const { return m_majorver; }
    int GetMinorVersion() const { return m_minorver; }

    QString GetDownloadURL() const { return m_downloadurl; }
    QString GetThemeWebSite() const { return m_themesite; }

    QString GetLocalURL() const { return m_themeurl; }
    QString GetDirectoryName() const { return m_theme.fileName(); }

    void ToMap(QHash<QString, QString> &infoMap) const;

  private:
    bool parseThemeInfo();

    QString   m_themeurl;
    QFileInfo m_theme;
    int       m_type;
    QString   m_aspect;
    QSize     m_baseres;
    QString   m_name;
    QString   m_previewpath;
    QString   m_description;
    QString   m_errata;
    int       m_majorver;
    int       m_minorver;

    QString   m_authorName;
    QString   m_authorEmail;

    QString   m_downloadurl;  // URL to download theme package from
    QString   m_themesite;    // Theme's website
};

Q_DECLARE_METATYPE(ThemeInfo*)

#endif
