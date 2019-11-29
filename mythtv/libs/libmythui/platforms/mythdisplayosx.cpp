// MythTV
#include "config.h"
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#include "mythdisplayosx.h"
#import "util-osx.h"

MythDisplayOSX::MythDisplayOSX()
  : MythDisplay()
{
    InitialiseModes();
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

bool MythDisplayOSX::UsingVideoModes(void)
{
    return gCoreContext->GetBoolSetting("UseVideoModes", false);
}

const std::vector<DisplayResScreen>& MythDisplayOSX::GetVideoModes(void)
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

bool MythDisplayOSX::SwitchToVideoMode(int Width, int Height, double DesiredRate)
{
    if (!HasMythMainWindow())
        return false;

    WId win = (qobject_cast<QWidget*>(MythMainWindow::getMainWindow()))->winId();
    CGDirectDisplayID d = GetOSXDisplay(win);
    CFDictionaryRef dispMode = nullptr;
    boolean_t match = 0;

    // find mode that matches the desired size

    if (DesiredRate)
        dispMode = CGDisplayBestModeForParametersAndRefreshRate(
                       d, 32, Width, Height,
                       (CGRefreshRate)((short)DesiredRate), &match);

    if (!match)
        dispMode =
            CGDisplayBestModeForParameters(d, 32, Width, Height, &match);

    if (!match)
        dispMode =
            CGDisplayBestModeForParameters(d, 16, Width, Height, &match);

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
