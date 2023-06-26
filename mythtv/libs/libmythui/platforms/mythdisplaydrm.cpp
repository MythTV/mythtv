// MythTV
#include "libmythbase/mythcorecontext.h"
#include "mythmainwindow.h"
#include "platforms/mythdrmdevice.h"
#include "platforms/mythdisplaydrm.h"

#define LOC QString("DispDRM: ")

void MythDisplayDRM::MainWindowReady()
{
#ifdef USING_QTPRIVATEHEADERS
    if (m_device)
        m_device->MainWindowReady();
#endif
}

bool MythDisplayDRM::DirectRenderingAvailable()
{
#ifdef USING_QTPRIVATEHEADERS
    if (!HasMythMainWindow())
        return false;

    if (auto *mainwindow = GetMythMainWindow(); mainwindow)
    {
        if (auto *drmdisplay = dynamic_cast<MythDisplayDRM*>(mainwindow->GetDisplay()); drmdisplay)
        {
            if (auto drm = drmdisplay->GetDevice(); drm && drm->Atomic() && drm->Authenticated())
            {
                if (auto plane = drm->GetVideoPlane(); plane && plane->m_id)
                    return true;
            }
        }
    }
#endif
    return false;
}

MythDisplayDRM::MythDisplayDRM([[maybe_unused]] MythMainWindow* MainWindow)
{
    m_device = MythDRMDevice::Create(m_screen);
    Initialise();
#ifdef USING_QTPRIVATEHEADERS
    if (MainWindow && m_device && m_device->GetVideoPlane())
        connect(MainWindow, &MythMainWindow::SignalWindowReady, this, &MythDisplayDRM::MainWindowReady);
#endif
}

MythDisplayDRM::~MythDisplayDRM()
{
    m_device = nullptr;
}

MythDRMPtr MythDisplayDRM::GetDevice()
{
    return m_device;
}

// FIXME - I doubt this slot is being called correctly but the screen won't
// change if we are using a fullscreen platform plugin (e.g. eglfs)
void MythDisplayDRM::ScreenChanged(QScreen *qScreen)
{
    MythDisplay::ScreenChanged(qScreen);

    if (m_device && m_device->GetScreen() != m_screen)
        m_device = nullptr;

    if (!m_device)
        m_device = MythDRMDevice::Create(m_screen);

    emit screenChanged();
}

bool MythDisplayDRM::VideoModesAvailable()
{
    return m_device && m_device->CanSwitchModes();
}

bool MythDisplayDRM::IsPlanar()
{
#ifdef USING_QTPRIVATEHEADERS
    return m_device && m_device->Authenticated() && m_device->Atomic() &&
           m_device->GetVideoPlane() && m_device->GetVideoPlane()->m_id;
#else
    return false;
#endif
}

bool MythDisplayDRM::UsingVideoModes()
{
    if (gCoreContext && m_device && m_device->CanSwitchModes())
        return gCoreContext->GetBoolSetting("UseVideoModes", false);
    return false;
}

void MythDisplayDRM::UpdateCurrentMode()
{
    if (m_device)
    {
        // Ensure video modes are fetched early
        GetVideoModes();
        m_refreshRate  = m_device->GetRefreshRate();
        m_resolution   = m_device->GetResolution();
        m_physicalSize = m_device->GetPhysicalSize();
        m_edid         = m_device->GetEDID();
        m_modeComplete = true;
        return;
    }
    MythDisplay::UpdateCurrentMode();
}

const MythDisplayModes& MythDisplayDRM::GetVideoModes()
{
    if (!m_videoModes.empty())
        return m_videoModes;

    m_videoModes.clear();
    m_modeMap.clear();
    if (!m_screen || !m_device || !m_device->CanSwitchModes())
        return m_videoModes;

    auto mainresolution = m_device->GetResolution();
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Filtering out modes that aren't %1x%2")
        .arg(mainresolution.width()).arg(mainresolution.height()));

    DisplayModeMap screenmap;
    auto modes = m_device->GetModes();
    auto physicalsize = m_device->GetPhysicalSize();
    for (const auto & mode : modes)
    {
        auto width = mode->m_width;
        auto height = mode->m_height;
        auto rate = mode->m_rate;

        // Filter out interlaced modes
        if ((mode->m_flags & DRM_MODE_FLAG_INTERLACE) != 0U)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Ignoring interlaced mode %1x%2 %3i")
                .arg(width).arg(height).arg(rate, 2, 'f', 2, '0'));
            continue;
        }

        // Filter out anything that is not the same size as our current screen
        // i.e. refresh rate changes only
        if (auto size = QSize(width, height); size != mainresolution)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Ignoring mode %1x%2 %3")
                .arg(width).arg(height).arg(rate, 2, 'f', 2, '0'));
            continue;
        }

        QSize resolution(width, height);
        auto key = MythDisplayMode::CalcKey(resolution, 0.0);
        if (screenmap.find(key) == screenmap.end())
            screenmap[key] = MythDisplayMode(resolution, physicalsize, -1.0, rate);
        else
            screenmap[key].AddRefreshRate(rate);
        m_modeMap.insert(MythDisplayMode::CalcKey(resolution, rate), mode->m_index);
    }

    for (auto & it : screenmap)
        m_videoModes.push_back(it.second);

    DebugModes();
    return m_videoModes;
}

bool MythDisplayDRM::SwitchToVideoMode(QSize Size, double DesiredRate)
{
    if (!m_screen || !m_device || !m_device->CanSwitchModes() || m_videoModes.empty())
        return false;

    auto rate = static_cast<double>(NAN);
    QSize dummy(0, 0);
    MythDisplayMode desired(Size, dummy, -1.0, DesiredRate);
    int index = MythDisplayMode::FindBestMatch(m_videoModes, desired, rate);

    if (index < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Desired resolution and frame rate not found.");
        return false;
    }

    auto mode = MythDisplayMode::CalcKey(Size, rate);
    if (!m_modeMap.contains(mode))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find mode");
        return false;
    }

    return m_device->SwitchMode(m_modeMap.value(mode));
}
