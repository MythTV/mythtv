// Qt
#include <QThread>
#include <QApplication>
#include <QElapsedTimer>

// MythTV
#include "DisplayRes.h"
#include "config.h"
#include "mythlogging.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "mythdisplay.h"

#ifdef USING_XRANDR
#include "DisplayResX.h"
#endif
#if CONFIG_DARWIN
#include "DisplayResOSX.h"
#endif

// Std
#include <cmath>

#define LOC QString("DispRes: ")

/** \class DisplayRes
 *  \brief The DisplayRes module allows for the display resolution
 * and refresh rate to be changed "on the fly".
 *
 * Resolution and refresh rate can be changed using an explicit call or based on
 * the video resolution.
 *
 * The SwitchToVideoMode routine takes care of the actual work involved in
 * changing the display mode. There are currently XRandR and OS X Carbon
 * implementation so this works for X (Linux/BSD/UNIX) and Mac OS X.
 *
 * Currently, MythUIHelper holds the 'master' reference (i.e. it typically creates
 * the first instance and releases it last).
*/

/*! \brief Retrieve a pointer to the DisplayRes singleton.
 *
* DisplaRes is a reference counted object with a single, static instance on
* supported platforms.
* Retrieve a pointer with AcquireRelease and release it with AcquireRelease(false).
*
* \return A pointer to the platform specific singleton or nullptr on unsupported
* platforms.
*/
DisplayRes* DisplayRes::AcquireRelease(bool Acquire /*=true*/)
{
    static QMutex s_lock(QMutex::Recursive);
    static DisplayRes* s_instance = nullptr;

    QMutexLocker locker(&s_lock);
    if (Acquire)
    {
        if (s_instance)
        {
            s_instance->IncrRef();
        }
        else
        {
#ifdef USING_XRANDR
            if (DisplayResX::IsAvailable())
                s_instance = new DisplayResX();
#elif CONFIG_DARWIN
            s_instance = new DisplayResOSX();
#endif
        }
    }
    else
    {
        if (s_instance)
            if (s_instance->DecrRef() == 0)
                s_instance = nullptr;
    }
    return s_instance;
}

std::vector<DisplayResScreen> DisplayRes::GetModes(void)
{
    std::vector<DisplayResScreen> result;
    DisplayRes *dispres = DisplayRes::AcquireRelease();
    if (dispres)
    {
        result = dispres->GetVideoModes();
        DisplayRes::AcquireRelease(false);
    }
    return result;
}

DisplayRes::DisplayRes()
  : ReferenceCounter("DisplRes"),
    m_display(MythDisplay::AcquireRelease())
{
}

DisplayRes::~DisplayRes()
{
    MythDisplay::AcquireRelease(false);
}

/// \brief Return the screen to the original desktop settings
void DisplayRes::SwitchToDesktop()
{
    SwitchToGUI(DESKTOP);
}

bool DisplayRes::Initialize(void)
{
    m_last.Init();
    m_curMode = GUI;

    // Initialise DESKTOP mode
    DisplayInfo info = m_display->GetDisplayInfo();
    int pixelwidth = info.m_res.width();
    int pixelheight = info.m_res.height();
    int mmwidth = info.m_size.width();
    int mmheight = info.m_size.height();
    double refreshrate = 1000000.0 / static_cast<double>(info.m_rate);

    m_mode[DESKTOP].Init();
    m_mode[DESKTOP] = DisplayResScreen(pixelwidth, pixelheight, mmwidth, mmheight, -1.0, refreshrate);
    LOG(VB_GENERAL, LOG_NOTICE, LOC + QString("Desktop video mode: %1x%2 %3 Hz")
        .arg(pixelwidth).arg(pixelheight).arg(refreshrate, 0, 'f', 3));

    // Initialize GUI mode
    m_mode[GUI].Init();
    pixelwidth = pixelheight = 0;
    double aspectratio = 0.0;
    GetMythDB()->GetResolutionSetting("GuiVidMode", pixelwidth, pixelheight, aspectratio, refreshrate);
    GetMythDB()->GetResolutionSetting("DisplaySize", mmwidth, mmheight);
    m_mode[GUI] = DisplayResScreen(pixelwidth, pixelheight, mmwidth, mmheight, -1.0, refreshrate);

    if (m_mode[DESKTOP] == m_mode[GUI] && qFuzzyIsNull(refreshrate))
    {
        // same resolution as current desktop
        m_last = m_mode[DESKTOP];
    }

    // Initialize default VIDEO mode
    pixelwidth = pixelheight = 0;
    GetMythDB()->GetResolutionSetting("TVVidMode", pixelwidth, pixelheight, aspectratio, refreshrate);
    m_mode[VIDEO] = DisplayResScreen(pixelwidth, pixelheight, mmwidth, mmheight, aspectratio, refreshrate);

    // Initialize video override mode
    m_inSizeToOutputMode.clear();

    for (int i = 0; true; ++i)
    {
        int iw = 0, ih = 0, ow = 0, oh = 0;
        double iaspect = 0.0, oaspect = 0.0;
        double irate = 0.0, orate = 0.0;

        GetMythDB()->GetResolutionSetting("VidMode", iw, ih, iaspect, irate, i);
        GetMythDB()->GetResolutionSetting("TVVidMode", ow, oh, oaspect, orate, i);

        if (!(iw || ih || !qFuzzyIsNull(irate)))
            break;

        if (!(ih && ow && oh))
            break;

        uint64_t key = DisplayResScreen::CalcKey(iw, ih, irate);

        DisplayResScreen scr(ow, oh, mmwidth, mmheight, oaspect, orate);

        m_inSizeToOutputMode[key] = scr;
    }

    return true;
}

/** \brief Switches to the resolution and refresh rate defined in the
 * database for the specified video resolution and frame rate.
*/
bool DisplayRes::SwitchToVideo(int Width, int Height, double Rate)
{
    Mode next_mode = VIDEO; // default VIDEO mode
    DisplayResScreen next = m_mode[next_mode];
    double target_rate = 0.0;

    // try to find video override mode
    uint64_t key = DisplayResScreen::FindBestScreen(m_inSizeToOutputMode,
                   Width, Height, Rate);

    if (key != 0)
    {
        m_mode[next_mode = CUSTOM_VIDEO] = next = m_inSizeToOutputMode[key];
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found custom screen override %1x%2")
            .arg(next.Width()).arg(next.Height()));
    }

    // If requested refresh rate is 0, attempt to match video fps
    if (qFuzzyIsNull(next.RefreshRate()))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Trying to match best refresh rate %1Hz")
            .arg(Rate, 0, 'f', 3));
        next.SetRefreshRate(Rate);
    }

    // need to change video mode?
    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);

    bool chg = !(next == m_last) ||
               !(DisplayResScreen::compare_rates(m_last.RefreshRate(),
                                                 target_rate));

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 %2x%3 %4 Hz")
        .arg(chg ? "Changing to" : "Using")
        .arg(next.Width()).arg(next.Height())
        .arg(target_rate, 0, 'f', 3));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("SwitchToVideo: Video size %1 x %2: "
                                               "xrandr failed for %3 x %4")
            .arg(Width).arg(Height).arg(next.Width()).arg(next.Height()));
        return false;
    }

    m_curMode = next_mode;

    m_last = next;
    m_last.SetRefreshRate(target_rate);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SwitchToVideo: Video size %1x%2")
        .arg(Width).arg(Height));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 displaying resolution %2x%3, %4mmx%5mm")
        .arg((chg) ? "Switched to" : "Already")
        .arg(GetWidth()).arg(GetHeight())
        .arg(GetPhysicalWidth()).arg(GetPhysicalHeight()));

    if (chg)
        PauseForModeSwitch();

    return chg;
}

/** \brief Switches to the GUI resolution specified.
 *
 * If which_gui is GUI then this switches to the resolution and refresh rate set
 * in the database for the GUI. If which_gui is set to CUSTOM_GUI then we switch
 * to the resolution and refresh rate specified in the last call to SwitchToCustomGUI.
 *
 * \param which_gui either regular GUI or CUSTOM_GUI
 * \sa SwitchToCustomGUI
 */
bool DisplayRes::SwitchToGUI(Mode NextMode)
{
    DisplayResScreen next = m_mode[NextMode];
    double target_rate = static_cast<double>(NAN);

    // need to change video mode?
    // If GuiVidModeRefreshRate is 0, assume any refresh rate is good enough.
    if (qFuzzyIsNull(next.RefreshRate()))
        next.SetRefreshRate(m_last.RefreshRate());

    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);
    bool chg = !(next == m_last) ||
               !(DisplayResScreen::compare_rates(m_last.RefreshRate(),
                                                 target_rate));

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 %2x%3 %4 Hz")
        .arg(chg ? "Changing to" : "Using")
        .arg(next.Width()).arg(next.Height())
        .arg(target_rate, 0, 'f', 3));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("SwitchToGUI: xrandr failed for %1x%2 %3  Hz")
            .arg(next.Width()).arg(next.Height())
            .arg(next.RefreshRate(), 0, 'f', 3));
        return false;
    }

    m_curMode = NextMode;

    m_last = next;
    m_last.SetRefreshRate(target_rate);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("SwitchToGUI: Switched to %1x%2 %3 Hz")
        .arg(GetWidth()).arg(GetHeight()).arg(GetRefreshRate(), 0, 'f', 3));

    return chg;
}

/** \brief Switches to the custom GUI resolution specified.
 *
 * This switches to the specified resolution, and refresh rate if specified. It
 * also makes saves this resolution as the CUSTOM_GUI resolution, so that it can
 * be recalled with SwitchToGUI(CUSTOM_GUI) in later calls.
 *
 * \sa SwitchToGUI
*/
bool DisplayRes::SwitchToCustomGUI(int Width, int Height, short Rate)
{
    m_mode[CUSTOM_GUI] = DisplayResScreen(Width, Height, m_mode[GUI].Width_mm(),
                                          m_mode[GUI].Height_mm(), -1.0,
                                          static_cast<double>(Rate));
    return SwitchToGUI(CUSTOM_GUI);
}

int DisplayRes::GetWidth(void) const
{
    return m_last.Width();
}

int DisplayRes::GetHeight(void) const
{
    return m_last.Height();
}

int DisplayRes::GetPhysicalWidth(void) const
{
    return m_last.Width_mm();
}

int DisplayRes::GetPhysicalHeight(void) const
{
    return m_last.Height_mm();
}

double DisplayRes::GetRefreshRate(void) const
{
    return m_last.RefreshRate();
}

/** \brief Returns current screen aspect ratio.
 *
 * If there is an aspect overide in the database that aspect ratio is returned
 * instead of the actual screen aspect ratio.
*/
double DisplayRes::GetAspectRatio(void) const
{
    return m_last.AspectRatio();
}

std::vector<double> DisplayRes::GetRefreshRates(int Width, int Height) const
{
    double tr = static_cast<double>(NAN);
    std::vector<double> empty;

    const DisplayResScreen drs(Width, Height, 0, 0, -1.0, 0.0);
    const DisplayResVector& drv = GetVideoModes();
    int t = DisplayResScreen::FindBestMatch(drv, drs, tr);

    if (t < 0)
        return empty;

    return drv[static_cast<size_t>(t)].RefreshRates();
}

void DisplayRes::PauseForModeSwitch(void)
{
    int pauselengthinms = gCoreContext->GetNumSetting("VideoModeChangePauseMS", 0);
    if (pauselengthinms)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Pausing %1ms for video mode switch").arg(pauselengthinms));
        QElapsedTimer timer;
        timer.start();
        while (!timer.hasExpired(pauselengthinms))
        {
            QThread::msleep(100);
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }
}
