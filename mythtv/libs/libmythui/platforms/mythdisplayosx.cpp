// MythTV
#include "mythdisplayosx.h"
#import "util-osx.h"

MythDisplayOSX::MythDisplayOSX()
  : MythDisplay()
{
}

MythDisplayOSX::~MythDisplayOSX()
{
}

DisplayInfo MythDisplayOSX::GetDisplayInfo(int VideoRate)
{
    DisplayInfo ret;

    CGDirectDisplayID disp = GetOSXDisplay(GetWindowID());
    if (!disp)
        return ret;

    CFDictionaryRef ref = CGDisplayCurrentMode(disp);
    float rate = 0.0F;
    if (ref)
        rate = get_float_CF(ref, kCGDisplayRefreshRate);

    if (VALID_RATE(rate))
        ret.m_rate = 1000000.0F / rate;
    else
        ret.m_rate = SanitiseRefreshRate(VideoRate);

    CGSize size_in_mm = CGDisplayScreenSize(disp);
    ret.m_size = QSize((uint) size_in_mm.width, (uint) size_in_mm.height);

    int width  = CGDisplayPixelsWide(disp);
    int height = CGDisplayPixelsHigh(disp);
    ret.m_res  = QSize(width, height);
    return ret;
}
