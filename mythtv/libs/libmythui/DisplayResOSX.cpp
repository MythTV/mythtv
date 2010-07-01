
#include "DisplayResOSX.h"

#import <CoreGraphics/CGBase.h>
#import <CoreGraphics/CGDisplayConfiguration.h>
#import <Carbon/Carbon.h>
#import <IOKit/graphics/IOGraphicsLib.h> // for IODisplayCreateInfoDictionary()

#include "util-osx.h"

CGDirectDisplayID mythtv_display();


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
    CGDirectDisplayID d = mythtv_display();

    io_connect_t port = CGDisplayIOServicePort(d);
    if (port == MACH_PORT_NULL )
        return false;

    CFDictionaryRef disp_dict = IODisplayCreateInfoDictionary(port, 0);
    CFDictionaryRef mode_dict = CGDisplayCurrentMode(d);
    if (!disp_dict || !mode_dict)
        return false;

    w_mm   = get_int_CF(disp_dict, CFSTR(kDisplayHorizontalImageSize));
    h_mm   = get_int_CF(disp_dict, CFSTR(kDisplayVerticalImageSize));
    w_pix  = get_int_CF(mode_dict, kCGDisplayWidth);
    h_pix  = get_int_CF(mode_dict, kCGDisplayHeight);
    rate   = (double) get_int_CF(mode_dict, kCGDisplayRefreshRate);
    par    = 1.0;

    //CFRelease(dict); // this release causes a segfault
    
    return true;
}

CGDirectDisplayID mythtv_display()
{
    CGDirectDisplayID d = NULL;
    
    // Find the display containing the MythTV main window
    Rect windowBounds;
    if (!GetWindowBounds(FrontNonFloatingWindow(),
                         kWindowContentRgn,
                         &windowBounds))
    {
        CGPoint pt;
        pt.x = windowBounds.left;
        pt.y = windowBounds.top;
        
        CGDisplayCount ct;
        if (CGGetDisplaysWithPoint(pt, 1, &d, &ct))
        {
            d = NULL;   // window is offscreen?
        }
    }
    if (!d)
    {
        d = CGMainDisplayID();
    }
    return d;
}

bool DisplayResOSX::SwitchToVideoMode(int width, int height, double refreshrate)
{
    CGDirectDisplayID d = mythtv_display();
    CFDictionaryRef dispMode = NULL;
    int match = 0;

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

    CGDirectDisplayID d = mythtv_display();
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

        uint key = DisplayResScreen::CalcKey(width, height, 0.0);

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
