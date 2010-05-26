#ifndef THEMEINFO_H
#define THEMEINFO_H

#include <QString>
#include <QSize>

#include "mythexp.h"

typedef enum {
    THEME_UNKN  = 0x00,
    THEME_UI    = 0x01,
    THEME_OSD   = 0x02,
    THEME_MENU  = 0x04
} ThemeType;

class QFileInfo;

class MPUBLIC ThemeInfo
{

  public:
    ThemeInfo(QString theme);
    ~ThemeInfo();

    bool IsWide() const;
    QString GetAspect() const { return m_aspect; }
    QSize   *GetBaseRes() { return &m_baseres; }
    QString GetName() const { return m_name; }
    QString GetDescription() const { return m_description; }
    QString GetErrata() const { return m_errata; }
    QString GetPreviewPath() const { return m_previewpath; }
    int GetType() const { return m_type; }
    int GetMajorVersion() const { return m_majorver; }
    int GetMinorVersion() const { return m_minorver; }

  private:
    bool parseThemeInfo();

    QFileInfo  *m_theme;
    int       m_type;
    QString   m_aspect;
    QSize     m_baseres;
    QString   m_name;
    QString   m_previewpath;
    QString   m_description;
    QString   m_errata;
    int       m_majorver;
    int       m_minorver;
};

#endif
