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

    bool UsingVideoModes(void) override;
    const std::vector<MythDisplayMode>& GetVideoModes(void) override;
    bool SwitchToVideoMode(int Width, int Height, double DesiredRate) override;
};

#endif // MYTHDISPLAYOSX_H
