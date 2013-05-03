#ifndef MYTHDISPLAY_H
#define MYTHDISPLAY_H

#include <QWidget> // for WId
#include <QSize>

#include "mythuiexp.h"

class DisplayInfo
{
  public:
    DisplayInfo(void)  : size(QSize(0,0)), res(QSize(0,0)), rate(-1) { }
    DisplayInfo(int r) : size(QSize(0,0)), res(QSize(0,0)), rate(r)  { }

    int Rate(void) const { return (int)(rate + 0.5); }
    QSize size;
    QSize res;
    float rate;
};

class MUI_PUBLIC MythDisplay
{
  public:
    static DisplayInfo GetDisplayInfo(int video_rate = 0);
    static WId GetWindowID(void);
    static int GetNumberXineramaScreens(void);
};

#endif // MYTHDISPLAY_H
