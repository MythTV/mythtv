#ifndef THEMEINFO_H
#define THEMEINFO_H

#include <qobject.h>
#include <qstring.h>
#include <qdom.h>
#include <qsize.h>

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

    bool IsWide();
    QString Aspect() const { return m_aspect; }
    QSize   *BaseRes() { return &m_baseres; }
    QString Name() const { return m_name; }
    QString Description() const { return m_description; }
    QString Errata() const { return m_errata; }
    QString PreviewPath() const { return m_previewpath; }
    int Type() const { return m_type; }
    int MajorVersion() const { return m_majorver; }
    int MinorVersion() const { return m_minorver; }

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
