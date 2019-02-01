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
    DisplayInfo(void)  : size(QSize(0,0)), res(QSize(0,0)), rate(-1) { }
    explicit DisplayInfo(int r) : size(QSize(0,0)), res(QSize(0,0)), rate(r)  { }

    int Rate(void) const { return lroundf(rate); }
    QSize size;
    QSize res;
    float rate;
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
