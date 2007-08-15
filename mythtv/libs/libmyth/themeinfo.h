#ifndef THEMEINFO_H
#define THEMEINFO_H

#include <qobject.h>
#include <qstring.h>
#include <qdom.h>

typedef enum {
    THEME_UNKN  = 0x00,
    THEME_UI    = 0x01,
    THEME_OSD   = 0x02,
    THEME_MENU  = 0x04
} ThemeType;

class QFileInfo;

class ThemeInfo
{

  public:
    ThemeInfo(QString theme);
    ~ThemeInfo();

    bool IsWide();
    QString Aspect() const { return m_aspect; }
    QString Name() const { return m_name; }
    QString PreviewPath() const { return m_previewpath; }
    int Type() const { return m_type; }

  private:
    bool parseThemeInfo();

    QFileInfo  *m_theme;
    int       m_type;
    QString   m_aspect;
    QString   m_name;
    QString   m_previewpath;
};

#endif
