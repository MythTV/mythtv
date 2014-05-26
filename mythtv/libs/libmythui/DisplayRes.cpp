
#include "DisplayRes.h"
#include "config.h"
#include "mythlogging.h"
#include "mythdb.h"

#ifdef USING_XRANDR
#include "DisplayResX.h"
#endif
#if CONFIG_DARWIN
#include "DisplayResOSX.h"
#endif


DisplayRes * DisplayRes::m_instance = NULL;
bool         DisplayRes::m_locked   = false;

DisplayRes * DisplayRes::GetDisplayRes(bool lock)
{
    if (lock && m_locked)
        return NULL;

    if (!m_instance)
    {
#ifdef USING_XRANDR
        m_instance = new DisplayResX();
#elif CONFIG_DARWIN
        m_instance = new DisplayResOSX();
#endif
    }

    if (m_instance && lock)
        m_locked = true;

    return m_instance;
}

void DisplayRes::Unlock(void)
{
    m_locked = false;
}

void DisplayRes::SwitchToDesktop()
{
    if (m_instance)
        m_instance->SwitchToGUI(DESKTOP);
}

bool DisplayRes::Initialize(void)
{
    int tW = 0, tH = 0, tW_mm = 0, tH_mm = 0;
    double tAspect = 0.0;
    double tRate = 0.0;

    m_last.Init();
    m_curMode = GUI;
    m_pixelAspectRatio = 1.0;

    // Initialise DESKTOP mode
    GetDisplayInfo(tW, tH, tW_mm, tH_mm, tRate, m_pixelAspectRatio);
    m_mode[DESKTOP].Init();
    m_mode[DESKTOP] = DisplayResScreen(tW, tH, tW_mm, tH_mm, -1.0, tRate);
    LOG(VB_GENERAL, LOG_NOTICE, QString("Desktop video mode: %1x%2 %3 Hz")
        .arg(tW).arg(tH).arg(tRate, 0, 'f', 3));

    // Initialize GUI mode
    m_mode[GUI].Init();
    tW = tH = 0;
    GetMythDB()->GetResolutionSetting("GuiVidMode", tW, tH, tAspect, tRate);
    GetMythDB()->GetResolutionSetting("DisplaySize", tW_mm, tH_mm);
    m_mode[GUI] = DisplayResScreen(tW, tH, tW_mm, tH_mm, -1.0, tRate);

    if (m_mode[DESKTOP] == m_mode[GUI] && tRate == 0)
    {
            // same resolution as current desktop
        m_last = m_mode[DESKTOP];
    }

    // Initialize default VIDEO mode
    tW = tH = 0;
    GetMythDB()->GetResolutionSetting("TVVidMode", tW, tH, tAspect, tRate);
    m_mode[VIDEO] = DisplayResScreen(tW, tH, tW_mm, tH_mm, tAspect, tRate);


    // Initialize video override mode
    m_inSizeToOutputMode.clear();

    for (int i = 0; true; ++i)
    {
        int iw = 0, ih = 0, ow = 0, oh = 0;
        double iaspect = 0.0, oaspect = 0.0;
        double irate = 0.0, orate = 0.0;

        GetMythDB()->GetResolutionSetting("VidMode", iw, ih, iaspect, irate, i);
        GetMythDB()->GetResolutionSetting("TVVidMode", ow, oh, oaspect, orate, i);

        if (!(iw || ih || irate))
            break;

        if (!(ih && ow && oh))
            break;

        uint64_t key = DisplayResScreen::CalcKey(iw, ih, irate);

        DisplayResScreen scr(ow, oh, tW_mm, tH_mm, oaspect, orate);

        m_inSizeToOutputMode[key] = scr;
    }

    // Find maximum resolution, needed for initializing X11 window
    const DisplayResVector& screens = GetVideoModes();

    for (uint i = 0; i < screens.size(); ++i)
    {
        m_maxWidth = std::max(m_maxWidth, screens[i].Width());
        m_maxHeight = std::max(m_maxHeight, screens[i].Height());
    }

    LOG(VB_PLAYBACK, LOG_INFO, QString("max_width: %1 max_height: %2")

        .arg(m_maxWidth).arg(m_maxHeight));

    return true;
}

bool DisplayRes::SwitchToVideo(int iwidth, int iheight, double frate)
{
    tmode next_mode = VIDEO; // default VIDEO mode
    DisplayResScreen next = m_mode[next_mode];
    double target_rate = 0.0;

    // try to find video override mode
    uint64_t key = DisplayResScreen::FindBestScreen(m_inSizeToOutputMode,
                   iwidth, iheight, frate);

    if (key != 0)
    {
        m_mode[next_mode = CUSTOM_VIDEO] = next = m_inSizeToOutputMode[key];
        LOG(VB_PLAYBACK, LOG_INFO, QString("Found custom screen override %1x%2")
            .arg(next.Width()).arg(next.Height()));
    }

    // If requested refresh rate is 0, attempt to match video fps
    if ((int) next.RefreshRate() == 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Trying to match best refresh rate %1Hz")
            .arg(frate, 0, 'f', 3));
        next.SetRefreshRate(frate);
    }

    // need to change video mode?
    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);

    bool chg = !(next == m_last) ||
               !(DisplayResScreen::compare_rates(m_last.RefreshRate(),
                                                 target_rate));
    
    LOG(VB_GENERAL, LOG_INFO, QString("%1 %2x%3 %4 Hz")
        .arg(chg ? "Changing to" : "Using")
        .arg(next.Width()).arg(next.Height())
        .arg(target_rate, 0, 'f', 3));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("SwitchToVideo: Video size %1 x %2: "
                                         "xrandr failed for %3 x %4")
            .arg(iwidth).arg(iheight)
            .arg(next.Width()).arg(next.Height()));
        return false;
    }

    m_curMode = next_mode;

    m_last = next;
    m_last.SetRefreshRate(target_rate);

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("SwitchToVideo: Video size %1 x %2: \n"
                "    %7 displaying resolution %3 x %4, %5mm x %6mm")
        .arg(iwidth).arg(iheight).arg(GetWidth()).arg(GetHeight())
        .arg(GetPhysicalWidth()).arg(GetPhysicalHeight())
        .arg((chg) ? "Switched to" : "Already"));

    return chg;
}

bool DisplayRes::SwitchToGUI(tmode next_mode)
{
    DisplayResScreen next = m_mode[next_mode];
    double target_rate;

    // need to change video mode?
    // If GuiVidModeRefreshRate is 0, assume any refresh rate is good enough.
    if (next.RefreshRate() == 0)
    {
        next.SetRefreshRate(m_last.RefreshRate());
    }

    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);
    bool chg = !(next == m_last) ||
               !(DisplayResScreen::compare_rates(m_last.RefreshRate(),
                                                 target_rate));

    LOG(VB_GENERAL, LOG_INFO, QString("%1 %2x%3 %4 Hz")
        .arg(chg ? "Changing to" : "Using")
        .arg(next.Width()).arg(next.Height())
        .arg(target_rate, 0, 'f', 3));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("SwitchToGUI: xrandr failed for %1x%2 %3  Hz")
            .arg(next.Width()).arg(next.Height())
            .arg(next.RefreshRate(), 0, 'f', 3));
        return false;
    }

    m_curMode = next_mode;

    m_last = next;
    m_last.SetRefreshRate(target_rate);

    LOG(VB_GENERAL, LOG_INFO, QString("SwitchToGUI: Switched to %1x%2 %3 Hz")
        .arg(GetWidth()).arg(GetHeight()).arg(GetRefreshRate(), 0, 'f', 3));

    return chg;
}

bool DisplayRes::SwitchToCustomGUI(int width, int height, short rate)
{
    m_mode[CUSTOM_GUI] = DisplayResScreen(width, height, m_mode[GUI].Width_mm(),
                                          m_mode[GUI].Height_mm(), -1.0,
                                          (double)rate);
    return SwitchToGUI(CUSTOM_GUI);
}

const std::vector<double> DisplayRes::GetRefreshRates(int width,
        int height) const
{
    double tr;
    std::vector<double> empty;

    const DisplayResScreen drs(width, height, 0, 0, -1.0, 0.0);
    const DisplayResVector& drv = GetVideoModes();
    int t = DisplayResScreen::FindBestMatch(drv, drs, tr);

    if (t < 0)
        return empty;

    return drv[t].RefreshRates();
}

/** \fn GetVideoModes(void)
 *  \relates DisplayRes
 *  \brief Returns all video modes available.
 *
 *   This is a conveniance class that instanciates a DisplayRes
 *   class if needed, and returns a copy of vector returned by
 *   DisplayRes::GetVideoModes(void).
 */
const DisplayResVector GetVideoModes(void)
{
    DisplayRes *display_res = DisplayRes::GetDisplayRes();

    if (display_res)
        return display_res->GetVideoModes();

    DisplayResVector empty;

    return empty;
}
