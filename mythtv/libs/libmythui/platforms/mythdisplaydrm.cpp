// MythTV
#include "mythdrmdevice.h"
#include "mythdisplaydrm.h"

#define LOC QString("DispDRM: ")

MythDisplayDRM::MythDisplayDRM()
  : MythDisplay()
{
    m_device = new MythDRMDevice(m_screen);
}

MythDisplayDRM::~MythDisplayDRM()
{
    if (m_device)
        delete m_device;
}

void MythDisplayDRM::ScreenChanged(QScreen *qScreen)
{
    MythDisplay::ScreenChanged(qScreen);

    if (m_device && m_device->GetScreen() != m_screen)
    {
        delete m_device;
        m_device = nullptr;
    }

    if (!m_device)
        m_device = new MythDRMDevice(m_screen);
}

DisplayInfo MythDisplayDRM::GetDisplayInfo(int VideoRate)
{
    DisplayInfo result;
    if (!m_device)
        return result;

    result.m_size = m_device->GetPhysicalSize();
    result.m_res  = m_device->GetResolution();
    float rate = m_device->GetRefreshRate();
    if (VALID_RATE(rate))
        result.m_rate = 1000000.0F / rate;
    else
        result.m_rate = SanitiseRefreshRate(VideoRate);
    return result;
}
