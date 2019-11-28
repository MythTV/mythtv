#ifndef MYTHDISPLAYOSX_H
#define MYTHDISPLAYOSX_H

// MythTV
#include "mythdisplay.h"

class MythDisplayOSX : public MythDisplay
{
  public:
    MythDisplayOSX();
   ~MythDisplayOSX() override;

    DisplayInfo GetDisplayInfo(int VideoRate = 0) override;
};

#endif // MYTHDISPLAYOSX_H
