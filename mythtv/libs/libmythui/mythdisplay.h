#ifndef MYTHDISPLAY_H
#define MYTHDISPLAY_H

#include <cmath>

#include <QWidget> // for WId
#include <QSize>
#include <QScreen>

#include "mythuiexp.h"

class DisplayInfo
{
  public:
    DisplayInfo(void) = default;
    explicit DisplayInfo(int r) : m_rate(r)  { }

    int Rate(void) const { return lroundf(m_rate); }
    QSize m_size {0,0};
    QSize m_res  {0,0};
    float m_rate {-1};
};

class MUI_PUBLIC MythDisplay
{
  public:
    static DisplayInfo GetDisplayInfo(int video_rate = 0);
    static WId GetWindowID(void);
    static int GetNumberOfScreens(void);
    static QScreen* GetScreen(void);
    static bool SpanAllScreens(void);
    static QString GetExtraScreenInfo(QScreen *qscreen);
};

#endif // MYTHDISPLAY_H
