#ifndef MYTHDISPLAY_H
#define MYTHDISPLAY_H

#include <QWindowsStyle>
#include <QWidget>
#include <QSize>

#include "mythexp.h"

class DisplayInfo
{
  public:
    DisplayInfo(void)  : size(QSize(0,0)), res(QSize(0,0)), rate(-1) { }
    DisplayInfo(int r) : size(QSize(0,0)), res(QSize(0,0)), rate(r)  { }

    QSize size;
    QSize res;
    float rate;
};

class MPUBLIC MythDisplay
{
  public:
    static DisplayInfo GetDisplayInfo(void);
    static WId GetWindowID(void);
};

#endif // MYTHDISPLAY_H
