using namespace std;

#import <CoreGraphics/CGBase.h>
#import <CoreGraphics/CGDisplayConfiguration.h>
#import <Carbon/Carbon.h>

#include "DisplayResOSX.h"

DisplayResOSX::DisplayResOSX(void)
{
    Initialize();
}

DisplayResOSX::~DisplayResOSX(void)
{
}

bool DisplayResOSX::get_display_size(int & width_mm, int & height_mm)
{
    // The mm settings are not applicable on Mac OS X.
    width_mm = 0;
    height_mm = 0;
    return true;
}

bool DisplayResOSX::switch_res(int width, int height)
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

    // find mode that matches the desired size
    CFDictionaryRef dispMode;
    int exactMatch = 0;
    dispMode = CGDisplayBestModeForParameters(d, 32, width, height,
                                              &exactMatch);
    if (!exactMatch)
        dispMode = CGDisplayBestModeForParameters(d, 16, width, height,
                                                  &exactMatch);
    if (!exactMatch)
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
