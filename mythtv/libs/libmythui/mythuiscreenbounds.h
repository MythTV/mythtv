#ifndef MYTHUISCREENBOUNDS_H
#define MYTHUISCREENBOUNDS_H

// Qt
#include <QRect>

// MythTV
#include "mythuiexp.h"

class MUI_PUBLIC MythUIScreenBounds
{
  public:
    static bool  GeometryIsOverridden();
    static void  ParseGeometryOverride(const QString& Geometry);
    static QRect GetGeometryOverride();
    static bool  WindowIsAlwaysFullscreen();

    void  UpdateScreenSettings();
    QRect GetUIScreenRect();
    void  SetUIScreenRect(const QRect& Rect);
    QRect GetScreenRect();
    QSize NormSize(const QSize& Size);
    int   NormX(int X);
    int   NormY(int Y);
    void  GetScalingFactors(float& Horizontal, float& Vertical);
    void  SetScalingFactors(float  Horizontal, float  Vertical);
    QSize GetThemeSize();
    int   GetFontStretch();
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

  private:
    static int s_XOverride;
    static int s_YOverride;
    static int s_WOverride;
    static int s_HOverride;
};

#endif
