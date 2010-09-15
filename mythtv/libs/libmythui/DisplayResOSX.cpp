
#include "DisplayResOSX.h"

#import <CoreGraphics/CGBase.h>
#import <CoreGraphics/CGDisplayConfiguration.h>
#import <Carbon/Carbon.h>
#import <IOKit/graphics/IOGraphicsLib.h> // for IODisplayCreateInfoDictionary()

#include "mythdisplay.h"
#include "util-osx.h"

DisplayResOSX::DisplayResOSX(void)
{
    Initialize();
}

DisplayResOSX::~DisplayResOSX(void)
{
}

bool DisplayResOSX::GetDisplayInfo(int &w_pix, int &h_pix, int &w_mm,
                                   int &h_mm, double &rate, double &par) const
{
    DisplayInfo info = MythDisplay::GetDisplayInfo();
    w_mm   = info.size.width();
    h_mm   = info.size.height();
    w_pix  = info.res.width();
    h_pix  = info.res.height();
    rate   = 1000000.0f / info.rate;
    par    = 1.0;
    return true;
}

bool DisplayResOSX::SwitchToVideoMode(int width, int height, double refreshrate)
{
    CGDirectDisplayID d = GetOSXDisplay(MythDisplay::GetWindowID());
    CFDictionaryRef dispMode = NULL;
    boolean_t match = 0;

    // find mode that matches the desired size
    if (refreshrate)
        dispMode = CGDisplayBestModeForParametersAndRefreshRate(
            d, 32, width, height, (CGRefreshRate)((short)refreshrate), &match);

    if (!match)
        dispMode = 
            CGDisplayBestModeForParameters(d, 32, width, height, &match);

    if (!match)
        dispMode = 
            CGDisplayBestModeForParameters(d, 16, width, height, &match);

    if (!match)
        return false;
    
    // switch mode and return success
    CGDisplayCapture(d);
    CGDisplayConfigRef cfg;
    CGBeginDisplayConfiguration(&cfg);
    CGConfigureDisplayFadeEffect(cfg, 0.3f, 0.5f, 0, 0, 0);
    CGConfigureDisplayMode(cfg, d, dispMode);
    CGError err = CGCompleteDisplayConfiguration(cfg, kCGConfigureForAppOnly);
    CGDisplayRelease(d);
    return (err == kCGErrorSuccess);
}

const DisplayResVector& DisplayResOSX::GetVideoModes() const
{
    if (m_video_modes.size())
        return m_video_modes;

    CGDirectDisplayID d = GetOSXDisplay(MythDisplay::GetWindowID());
    CFArrayRef displayModes = CGDisplayAvailableModes(d);
    if (NULL == displayModes)
        return m_video_modes;

    DisplayResMap screen_map;
    for (int i=0; i<CFArrayGetCount(displayModes); ++i)
    {
        CFDictionaryRef displayMode = (CFDictionaryRef) 
            CFArrayGetValueAtIndex(displayModes, i);
        int width   = get_int_CF(displayMode, kCGDisplayWidth);
        int height  = get_int_CF(displayMode, kCGDisplayHeight);
        int refresh = get_int_CF(displayMode, kCGDisplayRefreshRate);

        uint64_t key = DisplayResScreen::CalcKey(width, height, 0.0);

    if (screen_map.find(key)==screen_map.end())
            screen_map[key] = DisplayResScreen(width, height,
                                               0, 0, -1.0, (double) refresh);
        else
            screen_map[key].AddRefreshRate(refresh);
    }
    //CFRelease(displayModes); // this release causes a segfault

    DisplayResMapCIt it = screen_map.begin();
    for (; screen_map.end() != it; ++it)
        m_video_modes.push_back(it->second);

    return m_video_modes;
}
