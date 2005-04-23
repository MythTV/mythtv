#include "config.h"
#include "DisplayRes.h"
#include "mythcontext.h"

#ifdef USING_XRANDR
#include "DisplayResX.h"
#endif
#ifdef CONFIG_DARWIN
#include "DisplayResOSX.h"
#endif

DisplayRes * DisplayRes::instance = NULL;

DisplayRes * DisplayRes::GetDisplayRes(void)
{
    if (!instance)
    {
#ifdef USING_XRANDR
        instance = new DisplayResX();
#elif defined(CONFIG_DARWIN)
        instance = new DisplayResOSX();
#endif
    }
    return instance;
}

bool DisplayRes::Initialize(void)
{
    int tW = 0, tH = 0, tW_mm = 0, tH_mm = 0;
    double tAspect = 0.0;
    short tRate = 0;

    last.Init();
    cur_mode = GUI;


    // Initialize GUI mode
    mode[GUI].Init();
    tW = tH = 0;
    gContext->GetResolutionSetting("GuiVidMode", tW, tH);
    GetDisplaySize(tW_mm, tH_mm);
    gContext->GetResolutionSetting("DisplaySize", tW_mm, tH_mm);
    mode[GUI] = DisplayResScreen(tW, tH, tW_mm, tH_mm, -1.0, 0);


    // Initialize default VIDEO mode
    tW = tH = 0;
    gContext->GetResolutionSetting("TVVidMode", tW, tH, tAspect, tRate);
    mode[VIDEO] = DisplayResScreen(tW, tH, tW_mm, tH_mm, tAspect, tRate);


    // Initialize video override mode
    in_size_to_output_mode.clear();
    for (int i = 0; true; ++i)
    {
        int iw = 0, ih = 0, ow = 0, oh = 0;
        double iaspect = 0.0, oaspect = 0.0;
        short irate = 0, orate = 0;

        gContext->GetResolutionSetting("VidMode", iw, ih, iaspect, irate, i);
        gContext->GetResolutionSetting("TVVidMode", ow, oh, oaspect, orate, i);

        if (!(iw && ih && ow && oh))
            break;

        uint key = DisplayResScreen::CalcKey(iw, ih, irate);
        DisplayResScreen scr(ow, oh, tW_mm, tH_mm, oaspect, orate);
        in_size_to_output_mode[key] = scr;            
    }


    // Find maximum resolution, needed for initializing X11 window
    const DisplayResVector& screens = GetVideoModes();
    for (uint i=0; i<screens.size(); ++i)
    {
        max_width = max(max_width, screens[i].Width());
        max_height = max(max_height, screens[i].Height());
    }
    VERBOSE(VB_PLAYBACK, QString("max_width: %1 max_height: %2")
            .arg(max_width).arg(max_height));

    return true;
}

bool DisplayRes::SwitchToVideo(int iwidth, int iheight, short irate)
{
    tmode next_mode = VIDEO; // default VIDEO mode
    DisplayResScreen next = mode[next_mode];

    // try to find video override mode
    uint key = DisplayResScreen::CalcKey(iwidth, iheight, irate);
    DisplayResMapCIt it = in_size_to_output_mode.find(key);
    if (it != in_size_to_output_mode.end())
        mode[next_mode = CUSTOM_VIDEO] = next = it->second;

    // need to change video mode?
    short target_rate = 0;
    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);
    bool chg = !(next == last) || !(last.RefreshRate() == target_rate);

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
    short target_rate = 0;
    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);
    bool chg = !(next == last) || !(last.RefreshRate() == target_rate);

    VERBOSE(VB_PLAYBACK, QString("Trying %1x%2 %3 Hz")
            .arg(next.Width()).arg(next.Height()).arg(target_rate));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        VERBOSE(VB_IMPORTANT, QString("SwitchToGUI: xrandr failed for %1 x %2")
                .arg(next.Width()).arg(next.Height()));
        return false;
    }

    cur_mode = next_mode;
    last = next;

    VERBOSE(VB_PLAYBACK, QString("SwitchToGUI: Switched to %1 x %2")
            .arg(GetWidth()).arg(GetHeight()));

    return chg;
}

bool DisplayRes::SwitchToCustomGUI(int width, int height, short rate)
{
    mode[CUSTOM_GUI] = DisplayResScreen(width, height, mode[GUI].Width_mm(),
                                        mode[GUI].Height_mm(), -1.0, rate);
    return SwitchToGUI(CUSTOM_GUI);
}

const vector<short> DisplayRes::GetRefreshRates(int width, int height) const {
    short tr;
    vector<short> empty;

    const DisplayResScreen drs(width, height, 0, 0, -1.0, 0);
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
const vector<DisplayResScreen> GetVideoModes(void)
{
    DisplayRes *display_res = DisplayRes::GetDisplayRes();
    if (display_res)
        return display_res->GetVideoModes();

    vector<DisplayResScreen> empty;
    return empty;
}
