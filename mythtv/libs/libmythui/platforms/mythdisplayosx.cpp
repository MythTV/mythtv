// MythTV
#include "config.h"
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#include "mythdisplayosx.h"
#import  "mythosxutils.h"

#define LOC QString("DisplayOSX: ")

MythDisplayOSX::MythDisplayOSX()
  : MythDisplay()
{
    InitialiseModes();
}

MythDisplayOSX::~MythDisplayOSX()
{
    ClearModes();
}

DisplayInfo MythDisplayOSX::GetDisplayInfo(int VideoRate)
{
    DisplayInfo ret;
    if (!HasMythMainWindow())
        return ret;

    WId win = (qobject_cast<QWidget*>(MythMainWindow::getMainWindow()))->winId();
    CGDirectDisplayID disp = GetOSXDisplay(win);
    if (!disp)
        return ret;
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(disp);
    if (!mode)
        return ret;
    double rate     = CGDisplayModeGetRefreshRate(mode);
    //bool interlaced = CGDisplayModeGetIOFlags(mode) & kDisplayModeInterlacedFlag;
    size_t width    = CGDisplayModeGetWidth(mode);
    size_t height   = CGDisplayModeGetHeight(mode);
    CGDisplayModeRelease(mode);

    if (VALID_RATE(static_cast<float>(rate)))
        ret.m_rate = 1000000.0F / static_cast<float>(rate);
    else
        ret.m_rate = SanitiseRefreshRate(VideoRate);

    CGSize sizemm = CGDisplayScreenSize(disp);
    ret.m_size = QSize(static_cast<int>(sizemm.width), static_cast<int>(sizemm.height));
    ret.m_res  = QSize(static_cast<int>(width), static_cast<int>(height));
    return ret;
}

bool MythDisplayOSX::UsingVideoModes(void)
{
    if (gCoreContext)
        return gCoreContext->GetBoolSetting("UseVideoModes", false);
    return false;
}

const std::vector<MythDisplayMode>& MythDisplayOSX::GetVideoModes(void)
{
    if (!m_videoModes.empty() || !HasMythMainWindow())
        return m_videoModes;

    ClearModes();

    WId win = (qobject_cast<QWidget*>(MythMainWindow::getMainWindow()))->winId();
    CGDirectDisplayID disp = GetOSXDisplay(win);
    if (!disp)
        return m_videoModes;
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(disp, nullptr);
    if (!modes)
        return m_videoModes;

    DisplayModeMap screen_map;
    CGSize sizemm = CGDisplayScreenSize(disp);

    for (int i = 0; i < CFArrayGetCount(modes); ++i)
    {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
        double rate     = CGDisplayModeGetRefreshRate(mode);
        bool interlaced = CGDisplayModeGetIOFlags(mode) & kDisplayModeInterlacedFlag;
        size_t width    = CGDisplayModeGetWidth(mode);
        size_t height   = CGDisplayModeGetHeight(mode);

        // See note in MythDisplayX11
        if (interlaced)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Ignoring interlaced mode %1x%2 %3i")
                .arg(width).arg(height).arg(rate, 2, 'f', 2, '0'));
            continue;
        }

        uint64_t key = MythDisplayMode::CalcKey(width, height, 0.0);
        if (screen_map.find(key) == screen_map.end())
            screen_map[key] = MythDisplayMode(width, height, sizemm.width,
                                              sizemm.height, -1.0, rate);
        else
            screen_map[key].AddRefreshRate(rate);
        m_modeMap.insert(MythDisplayMode::CalcKey(width, height, rate),
                         CGDisplayModeRetain(mode));
    }

    CFRelease(modes);

    for (auto it = screen_map.begin(); screen_map.end() != it; ++it)
        m_videoModes.push_back(it->second);
    DebugModes();
    return m_videoModes;
}

bool MythDisplayOSX::SwitchToVideoMode(int Width, int Height, double DesiredRate)
{
    if (!HasMythMainWindow())
        return false;
    WId win = (qobject_cast<QWidget*>(MythMainWindow::getMainWindow()))->winId();
    CGDirectDisplayID disp = GetOSXDisplay(win);
    if (!disp)
        return false;

    auto rate = static_cast<double>(NAN);
    MythDisplayMode desired(Width, Height, 0, 0, -1.0, DesiredRate);
    int idx = MythDisplayMode::FindBestMatch(m_videoModes, desired, rate);
    if (idx < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Desired resolution and frame rate not found.");
        return false;
    }

    auto mode = MythDisplayMode::CalcKey(Width, Height, rate);
    if (!m_modeMap.contains(mode))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find mode");
        return false;
    }

    // switch mode and return success
    CGDisplayCapture(disp);
    CGDisplayConfigRef cfg;
    CGBeginDisplayConfiguration(&cfg);
    CGConfigureDisplayFadeEffect(cfg, 0.3f, 0.5f, 0, 0, 0);
    CGDisplaySetDisplayMode(disp, m_modeMap.value(mode), nullptr);
    CGError err = CGCompleteDisplayConfiguration(cfg, kCGConfigureForAppOnly);
    CGDisplayRelease(disp);
    return err == kCGErrorSuccess;
}

void MythDisplayOSX::ClearModes(void)
{
    for (auto it = m_modeMap.cbegin(); it != m_modeMap.cend(); ++it)
        CGDisplayModeRelease(it.value());
    m_modeMap.clear();
    m_videoModes.clear();
}
