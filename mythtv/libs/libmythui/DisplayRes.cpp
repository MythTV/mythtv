
#include "DisplayRes.h"
#include "config.h"
#include "mythverbose.h"
#include "mythdb.h"

#ifdef USING_XRANDR
#include "DisplayResX.h"
#endif
#if CONFIG_DARWIN
#include "DisplayResOSX.h"
#endif


DisplayRes * DisplayRes::instance = NULL;
bool         DisplayRes::locked   = false;

DisplayRes * DisplayRes::GetDisplayRes(bool lock)
{
    if (lock && locked)
        return NULL;

    if (!instance)
    {
#ifdef USING_XRANDR
        instance = new DisplayResX();
#elif CONFIG_DARWIN
        instance = new DisplayResOSX();
#endif
    }

    if (instance && lock)
        locked = true;

    return instance;
}

void DisplayRes::Unlock(void)
{
    locked = false;
}

void DisplayRes::SwitchToDesktop()
{
    if (instance)
        instance->SwitchToGUI(DESKTOP);
}

bool DisplayRes::Initialize(void)
{
    int tW = 0, tH = 0, tW_mm = 0, tH_mm = 0;
    double tAspect = 0.0;
    double tRate = 0.0;

    last.Init();
    cur_mode = GUI;
    pixelAspectRatio = 1.0;

    // Initialise DESKTOP mode
    GetDisplayInfo(tW, tH, tW_mm, tH_mm, tRate, pixelAspectRatio);
    mode[DESKTOP].Init();
    mode[DESKTOP] = DisplayResScreen(tW, tH, tW_mm, tH_mm, -1.0, tRate);
    VERBOSE(VB_GENERAL, QString("Desktop video mode: %1x%2 %3 Hz")
            .arg(tW).arg(tH).arg(tRate));

    // Initialize GUI mode
    mode[GUI].Init();
    tW = tH = 0;
    GetMythDB()->GetResolutionSetting("GuiVidMode", tW, tH, tAspect, tRate);
    GetMythDB()->GetResolutionSetting("DisplaySize", tW_mm, tH_mm);
    mode[GUI] = DisplayResScreen(tW, tH, tW_mm, tH_mm, -1.0, tRate);


    // Initialize default VIDEO mode
    tW = tH = 0;
    GetMythDB()->GetResolutionSetting("TVVidMode", tW, tH, tAspect, tRate);
    mode[VIDEO] = DisplayResScreen(tW, tH, tW_mm, tH_mm, tAspect, tRate);


    // Initialize video override mode
    in_size_to_output_mode.clear();
    for (int i = 0; true; ++i)
    {
        int iw = 0, ih = 0, ow = 0, oh = 0;
        double iaspect = 0.0, oaspect = 0.0;
        double irate = 0.0, orate = 0.0;

        GetMythDB()->GetResolutionSetting("VidMode", iw, ih, iaspect, irate, i);
        GetMythDB()->GetResolutionSetting("TVVidMode", ow, oh, oaspect, orate, i);

        if (!((iw || ih) && ow && oh))
            break;

        uint64_t key = DisplayResScreen::CalcKey(iw, ih, irate);
        DisplayResScreen scr(ow, oh, tW_mm, tH_mm, oaspect, orate);
        in_size_to_output_mode[key] = scr;            
    }


    // Find maximum resolution, needed for initializing X11 window
    const DisplayResVector& screens = GetVideoModes();
    for (uint i=0; i<screens.size(); ++i)
    {
        max_width = std::max(max_width, screens[i].Width());
        max_height = std::max(max_height, screens[i].Height());
    }
    VERBOSE(VB_PLAYBACK, QString("max_width: %1 max_height: %2")
            .arg(max_width).arg(max_height));

    return true;
}

bool DisplayRes::SwitchToVideo(int iwidth, int iheight, double frate)
{
    tmode next_mode = VIDEO; // default VIDEO mode
    DisplayResScreen next = mode[next_mode];

    // try to find video override mode
    uint64_t key = DisplayResScreen::CalcKey(iwidth, iheight, frate);
    DisplayResMapCIt it = in_size_to_output_mode.find(key);
    if (it != in_size_to_output_mode.end())
        mode[next_mode = CUSTOM_VIDEO] = next = it->second;
    else
    {
        // Try to find custom resolution, ignoring refresh rate
        key = DisplayResScreen::CalcKey(iwidth, iheight, 0);
        it = in_size_to_output_mode.find(key);
        if (it != in_size_to_output_mode.end())
            mode[next_mode = CUSTOM_VIDEO] = next = it->second;
        else
        {
                // Try to find resolution, ignoring X
            for (it = in_size_to_output_mode.begin();
                 it != in_size_to_output_mode.end();
                 it++)
            {
                int w, h;
                double rate;

                key = it->first;
                rate = (key & ((1<<18) - 1)) / 1000.0;
                h = (key >> 18) & ((1<<16) - 1);
                w = (key >> 34) & ((1<<16) - 1);
                if (h != iheight)
                    continue;

                mode[next_mode = CUSTOM_VIDEO] = next = it->second;
                break;
            }
        }
    }

    // If requested refresh rate is 0, attempt to match video fps
    if ((int) next.RefreshRate() == 0)
    {
        VERBOSE(VB_PLAYBACK, QString("Trying to match best refresh rate %1Hz")
                .arg(frate));
        next.AddRefreshRate(frate);
    }

    // need to change video mode?
    double target_rate = 0.0;
    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);
    bool chg = !(next == last) || !(DisplayResScreen::compare_rates(last.RefreshRate(),target_rate));

    VERBOSE(VB_PLAYBACK, QString("Trying %1x%2 %3 Hz")
            .arg(next.Width()).arg(next.Height()).arg(target_rate));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        VERBOSE(VB_IMPORTANT,
                QString("SwitchToVideo: Video size %1 x %2: "
                        "xrandr failed for %3 x %4")
                .arg(iwidth).arg(iheight).arg(next.Width()).arg(next.Height()));
        return false;
    }

    cur_mode = next_mode;
    last = next;

    VERBOSE(VB_PLAYBACK,
            QString("SwitchToVideo: Video size %1 x %2: \n"
                    "    %7 displaying resolution %3 x %4, %5mm x %6mm")
            .arg(iwidth).arg(iheight).arg(GetWidth()).arg(GetHeight())
            .arg(GetPhysicalWidth()).arg(GetPhysicalHeight())
            .arg((chg) ? "Switched to" : "Already"));

    return chg;
}

bool DisplayRes::SwitchToGUI(tmode next_mode)
{
    DisplayResScreen next = mode[next_mode];

    // need to change video mode?
    double target_rate = next.RefreshRate();
    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);
    // If GuiVidModeRefreshRate is 0, assume any refresh rate is good enough.
    bool chg = (!(next == last) || (((int) next.RefreshRate()) !=0
                && !(DisplayResScreen::compare_rates(last.RefreshRate(), target_rate)))); 

    VERBOSE(VB_GENERAL, QString("Trying %1x%2 %3 Hz")
            .arg(next.Width()).arg(next.Height()).arg(target_rate));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        VERBOSE(VB_IMPORTANT,
                QString("SwitchToGUI: xrandr failed for %1x%2 %3  Hz")
                .arg(next.Width()).arg(next.Height()).arg(next.RefreshRate()));
        return false;
    }

    cur_mode = next_mode;
    last = next;

    VERBOSE(VB_GENERAL, QString("SwitchToGUI: Switched to %1x%2 %3 Hz")
            .arg(GetWidth()).arg(GetHeight()).arg(GetRefreshRate()));

    return chg;
}

bool DisplayRes::SwitchToCustomGUI(int width, int height, short rate)
{
    mode[CUSTOM_GUI] = DisplayResScreen(width, height, mode[GUI].Width_mm(),
                                        mode[GUI].Height_mm(), -1.0, (double)rate);
    return SwitchToGUI(CUSTOM_GUI);
}

const std::vector<double> DisplayRes::GetRefreshRates(int width, int height) const {
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
const std::vector<DisplayResScreen> GetVideoModes(void)
{
    DisplayRes *display_res = DisplayRes::GetDisplayRes();
    if (display_res)
        return display_res->GetVideoModes();

    std::vector<DisplayResScreen> empty;
    return empty;
}
