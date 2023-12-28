#ifndef MYTHUISCREENBOUNDS_H
#define MYTHUISCREENBOUNDS_H

// Qt
#include <QWidget>

// MythTV
#include "mythuiexp.h"

class MythDisplay;

class MUI_PUBLIC MythUIScreenBounds : public QWidget
{
    Q_OBJECT

  signals:
    void  UIScreenRectChanged(const QRect& Rect);

  public:
    static bool  GeometryIsOverridden();
    static void  ParseGeometryOverride(const QString& Geometry);
    static QRect GetGeometryOverride();
    static bool  WindowIsAlwaysFullscreen();

    void  UpdateScreenSettings(MythDisplay* mDisplay);
    QRect GetUIScreenRect();
    void  SetUIScreenRect(QRect Rect);
    QRect GetScreenRect();
    QSize NormSize(QSize Size) const;
    int   NormX(int X) const;
    int   NormY(int Y) const;
    void  GetScalingFactors(float& Horizontal, float& Vertical) const;
    void  SetScalingFactors(float  Horizontal, float  Vertical);
    QSize GetThemeSize();
    int   GetFontStretch() const;
    void  SetFontStretch(int Stretch);

  protected:
    MythUIScreenBounds();
    void InitScreenBounds();

    QSize m_themeSize        { 1920, 1080 };
    QRect m_uiScreenRect     { 0, 0, 1920, 1080 };
    QRect m_screenRect       { 0, 0, 1920, 1080 };
    float m_screenHorizScale { 1.0   };
    float m_screenVertScale  { 1.0   };
    bool  m_wantWindow       { false };
    bool  m_wantFullScreen   { true  };
    bool  m_qtFullScreen     { false };
    bool  m_alwaysOnTop      { false };
    int   m_fontStretch      { 100   };
    bool  m_forceFullScreen  { false };

  private:
    static int s_XOverride;
    static int s_YOverride;
    static int s_WOverride;
    static int s_HOverride;
};

#endif
