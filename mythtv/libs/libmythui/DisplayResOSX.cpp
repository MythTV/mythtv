#include <QWidget>

#include "mythlogging.h"
#include "mythmainwindow.h"
#include "DisplayResOSX.h"

#import <IOKit/graphics/IOGraphicsLib.h> // for IODisplayCreateInfoDictionary()

#include "mythdisplay.h"
#include "util-osx.h"
#include "util-osx-cocoa.h"

#define LOC QString("DisplResOSX: ")

DisplayResOSX::DisplayResOSX(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Created");
    Initialize();
}

DisplayResOSX::~DisplayResOSX()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleted");
}

bool DisplayResOSX::SwitchToVideoMode(int width, int height, double refreshrate)
{
    if (!HasMythMainWindow())
        return false;

    WId win = (qobject_cast<QWidget*>(MythMainWindow::getMainWindow()))->winId();
    CGDirectDisplayID d = GetOSXDisplay(win);
    CFDictionaryRef dispMode = nullptr;
    boolean_t match = 0;

    // find mode that matches the desired size

    if (refreshrate)
        dispMode = CGDisplayBestModeForParametersAndRefreshRate(
                       d, 32, width, height,
                       (CGRefreshRate)((short)refreshrate), &match);

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
    if (!m_videoModes.empty() || !HasMythMainWindow())
        return m_videoModes;

    WId win = (qobject_cast<QWidget*>(MythMainWindow::getMainWindow()))->winId();
    CGDirectDisplayID d = GetOSXDisplay(win);

    CFArrayRef displayModes = CGDisplayAvailableModes(d);

    if (nullptr == displayModes)
        return m_videoModes;

    DisplayResMap screen_map;

    for (int i = 0; i < CFArrayGetCount(displayModes); ++i)
    {
        CFDictionaryRef displayMode = (CFDictionaryRef)
                                      CFArrayGetValueAtIndex(displayModes, i);
        int width   = get_int_CF(displayMode, kCGDisplayWidth);
        int height  = get_int_CF(displayMode, kCGDisplayHeight);
        int refresh = get_int_CF(displayMode, kCGDisplayRefreshRate);

        uint64_t key = DisplayResScreen::CalcKey(width, height, 0.0);

        if (screen_map.find(key) == screen_map.end())
            screen_map[key] = DisplayResScreen(width, height,
                                               0, 0, -1.0, (double) refresh);
        else
            screen_map[key].AddRefreshRate(refresh);
    }

    //CFRelease(displayModes); // this release causes a segfault

    DisplayResMapCIt it = screen_map.begin();

    for (; screen_map.end() != it; ++it)
        m_videoModes.push_back(it->second);

    return m_videoModes;
}
