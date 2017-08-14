
#include "config.h"
#include "DisplayResX.h"

#include <cmath>

#include "mythlogging.h"
#include "mythdb.h"
#include "mythdisplay.h"
#include "mythxdisplay.h"
#include "util-nvctrl.h"

#include <X11/extensions/Xrandr.h> // this has to be after util-x11.h (Qt bug)

static XRRScreenResources *GetScreenResources(MythXDisplay*& display);
static XRROutputInfo *GetOutputInfo(MythXDisplay*& display, XRRScreenResources*& rsc);
RRCrtc crtc;
/* Width x Height x Resolution = X mode ID. */
std::map<std::tuple<int, int, double>, RRMode> modeIdMatch;

DisplayResX::DisplayResX(void)
{
    Initialize();
}

DisplayResX::~DisplayResX(void)
{
}

bool DisplayResX::GetDisplayInfo(int &w_pix, int &h_pix, int &w_mm,
                                 int &h_mm, double &rate, double &par) const
{
    DisplayInfo info = MythDisplay::GetDisplayInfo();
    w_mm   = info.size.width();
    h_mm   = info.size.height();
    w_pix  = info.res.width();
    h_pix  = info.res.height();
    rate   = 1000000.0f / info.rate;
    par    = 1.0;

    if (w_mm > 0 && h_mm > 0 && w_pix > 0 && h_pix > 0)
        par = ((double)w_mm  / (double)w_pix) / ((double)h_mm / (double)h_pix);

    return true;
}

bool DisplayResX::SwitchToVideoMode(int width, int height, double desired_rate)
{
    double rate;
    DisplayResScreen desired_screen(width, height, 0, 0, -1.0, desired_rate);
    int idx = DisplayResScreen::FindBestMatch(m_videoModesUnsorted,
              desired_screen, rate);

    if (idx >= 0)
    {
        RRMode desiredMode;
        std::tuple<int, int, double> widthHeightRate = std::make_tuple(
            m_videoModesUnsorted[idx].Width(),
            m_videoModesUnsorted[idx].Height(),
            rate);
        std::map<std::tuple<int, int, double>, RRMode>::iterator modeIdIter = modeIdMatch.find(widthHeightRate);
        if (modeIdIter != modeIdMatch.end())
        {
            desiredMode = modeIdIter->second;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Desired modeline for Resolution and FrameRate not found.");
            return false;
        }

        MythXDisplay *display = NULL;

        XRRScreenResources *rsc = GetScreenResources(display);
        if (!rsc)
            return false;

        XRRCrtcInfo *origCrtcInfo = XRRGetCrtcInfo(display->GetDisplay(), rsc, crtc);
        Status status = XRRSetCrtcConfig(
            display->GetDisplay(),
            rsc,
            crtc,
            CurrentTime,
            origCrtcInfo->x,
            origCrtcInfo->y,
            desiredMode,
            origCrtcInfo->rotation,
            origCrtcInfo->outputs,
            origCrtcInfo->noutput);

        XRRFreeCrtcInfo(origCrtcInfo);
        XRRFreeScreenResources(rsc);

        // Force refresh of xf86VidMode current modeline
        XRRScreenConfiguration *cfg = XRRGetScreenInfo(display->GetDisplay(), display->GetRoot());
        if (cfg)
        {
            XRRFreeScreenConfigInfo(cfg);
        }

        delete display;

        if (RRSetConfigSuccess != status)
            LOG(VB_GENERAL, LOG_ERR,
                "XRRSetCrtcConfig() call failed.");

        return RRSetConfigSuccess == status;
    }

    LOG(VB_GENERAL, LOG_ERR, "Desired Resolution and FrameRate not found.");

    return false;
}

const DisplayResVector& DisplayResX::GetVideoModes(void) const
{
    if (!m_videoModes.empty())
        return m_videoModes;

    MythXDisplay *display = NULL;

    /* All screen resources for all monitors. */
    XRRScreenResources *rsc = GetScreenResources(display);
    if (!rsc)
        return m_videoModes;

    /* The primary monitor alone. */
    XRROutputInfo *output = GetOutputInfo(display, rsc);
    if (!output)
        return m_videoModes;

    /* Needed to set the modeline later. */
    crtc = output->crtc;
    modeIdMatch.clear();

    /* Sort these out by screen size */
    std::map<std::pair<int, int>, std::vector<XRRModeInfo>> modesBySize;
    for (int i = 0; i < output->nmode; i++)
    {
        /* ID of a valid mode for this monitor. */
        RRMode id = output->modes[i];
        XRRModeInfo mode;
        bool found = false;
        for (int j=0; j < rsc->nmode; j++)
        {
            mode = rsc->modes[j];
            if(id == mode.id)
            {
                found = true;
                break;
            }
        }

        /* Shouldn't happen if XRandr is consistent. */
        if (!found)
        {
            continue;
        }

        std::pair<int, int> widthHeight = std::make_pair(mode.width, mode.height);
        std::map<std::pair<int, int>, std::vector<XRRModeInfo>>::iterator heightWidthIter = modesBySize.find(widthHeight);
        if (heightWidthIter == modesBySize.end())
        {
            std::vector<XRRModeInfo> modesForSize;
            modesForSize.push_back(mode);
            modesBySize[widthHeight] = modesForSize;
        }
        else
        {
            heightWidthIter->second.push_back(mode);
        }
    }

    /* Generate the screen sizes with a list of refresh rates for each. */
    map<std::pair<int, int>, std::vector<XRRModeInfo>>::iterator screenSizesIter = modesBySize.begin();
    for (screenSizesIter = modesBySize.begin(); screenSizesIter != modesBySize.end(); ++screenSizesIter)
    {
        std::pair<int, int> widthHeight = screenSizesIter->first;
        std::vector<XRRModeInfo> widthHeightModes = screenSizesIter->second;
        std::vector<double> rates;
        for (uint i = 0; i < widthHeightModes.size(); i++)
        {
            XRRModeInfo widthHeightMode = widthHeightModes[i];
            double vTotal = widthHeightMode.vTotal;
            if (widthHeightMode.modeFlags & RR_DoubleScan)
            {
                vTotal *= 2;
            }

            if (widthHeightMode.modeFlags & RR_Interlace)
            {
                vTotal /= 2;
            }

            /* Some "auto" modes may not have such info. */
            if (widthHeightMode.hTotal && vTotal)
            {
                double rate = ((double) widthHeightMode.dotClock /
                    ((double) widthHeightMode.hTotal * (double) vTotal));
                rates.push_back(rate);
                std::tuple<int, int, double> widthHeightRate = std::make_tuple(widthHeight.first, widthHeight.second, rate);
                modeIdMatch[widthHeightRate] = widthHeightMode.id;
            }
        }

        DisplayResScreen scr(widthHeight.first, widthHeight.second,
                             output->mm_width, output->mm_height,
                             rates);
        m_videoModes.push_back(scr);
    }

    m_videoModesUnsorted = m_videoModes;

    std::sort(m_videoModes.begin(), m_videoModes.end());
    XRRFreeOutputInfo(output);
    XRRFreeScreenResources(rsc);
    delete display;

    return m_videoModes;
}

static XRRScreenResources *GetScreenResources(MythXDisplay*& display)
{
    display = OpenMythXDisplay();

    if (!display)
    {
        LOG(VB_GENERAL, LOG_ERR, "DisplayResX: MythXOpenDisplay call failed");
        return NULL;
    }

    Window root = RootWindow(display->GetDisplay(), display->GetScreen());

    XRRScreenResources *rsc = NULL;
    int event_basep = 0, error_basep = 0;

    if (XRRQueryExtension(display->GetDisplay(), &event_basep, &error_basep))
        rsc = XRRGetScreenResourcesCurrent(display->GetDisplay(), root);

    if (!rsc)
    {
        delete display;
        display = NULL;
        LOG(VB_GENERAL, LOG_ERR, "DisplayResX: Unable to XRRGetScreenResourcesCurrent");
    }

    return rsc;
}

static XRROutputInfo *GetOutputInfo(MythXDisplay*& display, XRRScreenResources*& rsc)
{
    Window root = RootWindow(display->GetDisplay(), display->GetScreen());
    RROutput primary = XRRGetOutputPrimary(display->GetDisplay(), root);
    if (primary == 0 && rsc->noutput > 0) {
        /* If primary is not set (and it usually is not for single displays),
         * pick the first monitor. */
        primary = rsc->outputs[0];
    }

    return XRRGetOutputInfo(display->GetDisplay(), rsc, primary);
}
