
#include "config.h"
#include "DisplayResX.h"

#include <cmath>

#include "mythlogging.h"
#include "mythdb.h"
#include "mythmainwindow.h"
#include "mythdisplay.h"
#include "mythxdisplay.h"
#include "util-nvctrl.h"

#include <QWindow>
#include <QScreen>

#include <X11/extensions/Xrandr.h> // this has to be after util-x11.h (Qt bug)

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
    if (!m_crtc)
        (void)GetVideoModes();

    if (!m_crtc)
        return false;

    auto rate = static_cast<double>(NAN);
    DisplayResScreen desired(width, height, 0, 0, -1.0, desired_rate);
    int idx = DisplayResScreen::FindBestMatch(m_videoModes, desired, rate);

    if (idx < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Desired resolution and frame rate not found."));
        return false;
    }

    auto mode = DisplayResScreen::CalcKey(width, height, rate);
    if (!m_modeMap.contains(mode))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find mode"));
        return false;
    }

    MythXDisplay *display = OpenMythXDisplay();
    if (!display)
        return false;

    Status status = RRSetConfigFailed;
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(display->GetDisplay(), display->GetRoot());
    if (res)
    {
        XRRCrtcInfo *currentcrtc = XRRGetCrtcInfo(display->GetDisplay(), res, m_crtc);
        if (currentcrtc)
        {
            status = XRRSetCrtcConfig(display->GetDisplay(), res, m_crtc, CurrentTime,
                                      currentcrtc->x, currentcrtc->y, m_modeMap.value(mode),
                                      currentcrtc->rotation, currentcrtc->outputs,
                                      currentcrtc->noutput);
            XRRFreeCrtcInfo(currentcrtc);
            XRRScreenConfiguration *config = XRRGetScreenInfo(display->GetDisplay(), display->GetRoot());
            if (config)
                XRRFreeScreenConfigInfo(config);
        }

        XRRFreeScreenResources(res);
    }

    delete display;

    if (RRSetConfigSuccess != status)
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to set video mode"));

    return RRSetConfigSuccess == status;
}

const DisplayResVector& DisplayResX::GetVideoModes(void)
{
    if (!m_videoModes.empty())
        return m_videoModes;

    MythXDisplay *display = OpenMythXDisplay();

    if (!display)
        return m_videoModes;

    QScreen *screen = nullptr;
    MythMainWindow *main = GetMythMainWindow();
    if (main && main->windowHandle())
        screen = main->windowHandle()->screen();

    if (!screen)
        return m_videoModes;

    RROutput current = 0;
    XRROutputInfo *output = nullptr;
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(display->GetDisplay(), display->GetRoot());
    for (int i = 0; i < res->noutput; ++i)
    {
        if (output)
        {
            XRRFreeOutputInfo(output);
            output = nullptr;
        }

        output = XRRGetOutputInfo(display->GetDisplay(), res, res->outputs[i]);
        if (!output || output->nameLen < 1)
            continue;

        if (output->connection != RR_Connected)
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Output '%1' is disconnected")
                .arg(output->name));
            continue;
        }

        QString name(output->name);
        if (name == screen->name())
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Matched '%1' to output %2")
                .arg(screen->name()).arg(res->outputs[i]));
            current = res->outputs[i];
            break;
        }
    }

    if (!current)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find an output that matches '%1'")
            .arg(QString(screen->name())));
        XRRFreeOutputInfo(output);
        XRRFreeScreenResources(res);
        delete display;
        return m_videoModes;
    }

    int mmwidth = static_cast<int>(output->mm_width);
    int mmheight = static_cast<int>(output->mm_height);
    m_crtc = output->crtc;

    DisplayResMap screenmap;
    for (int i = 0; i < output->nmode; ++i)
    {
        RRMode rrmode = output->modes[i];
        XRRModeInfo mode = res->modes[i];
        if (mode.id != rrmode)
            continue;

        if (!(mode.dotClock > 1 && mode.vTotal > 1 && mode.hTotal > 1))
            continue;

        int width = static_cast<int>(mode.width);
        int height = static_cast<int>(mode.height);
        double rate = static_cast<double>(mode.dotClock / (mode.vTotal * mode.hTotal));
        bool interlaced = mode.modeFlags & RR_Interlace;
        if (interlaced)
            rate *= 2.0;

        if (interlaced)
        {
            LOG(VB_PLAYBACK, LOG_INFO, QString("Ignoring interlaced mode %1x%2 %3i")
                .arg(width).arg(height).arg(rate));
            continue;
        }

        uint64_t key = DisplayResScreen::CalcKey(width, height, 0.0);
        if (screenmap.find(key) == screenmap.end())
            screenmap[key] = DisplayResScreen(width, height, mmwidth, mmheight, -1.0, rate);
        else
            screenmap[key].AddRefreshRate(rate);

        m_modeMap.insert(DisplayResScreen::CalcKey(width, height, rate), rrmode);
    }

    for (auto it = screenmap.begin(); screenmap.end() != it; ++it)
        m_videoModes.push_back(it->second);

    std::sort(m_videoModes.begin(), m_videoModes.end());

    XRRFreeOutputInfo(output);
    XRRFreeScreenResources(res);
    delete display;

    return m_videoModes;
}
