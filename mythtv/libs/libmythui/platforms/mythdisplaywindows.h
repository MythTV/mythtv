#ifndef MYTHDISPLAYWINDOWS_H
#define MYTHDISPLAYWINDOWS_H

// MythTV
#include "mythdisplay.h"

class MythDisplayWindows : public MythDisplay
{
  public:
    MythDisplayWindows();
   ~MythDisplayWindows() override;

    DisplayInfo GetDisplayInfo(int VideoRate = 0) override;
};

#endif // MYTHDISPLAYWINDOWS_H
