// MythTV
#include "platforms/mythdrmdevice.h"
#include "platforms/mythdisplaydrm.h"

#define LOC QString("DispDRM: ")

MythDisplayDRM::MythDisplayDRM()
{
    m_device = MythDRMDevice::Create(m_screen);
    if (!m_device->IsValid())
        m_device = nullptr;
    Initialise();
}

MythDisplayDRM::~MythDisplayDRM()
{
    m_device = nullptr;
}

MythDRMPtr MythDisplayDRM::GetDevice()
{
    return m_device;
}

// FIXME - I doubt this slot is being called correctly
void MythDisplayDRM::ScreenChanged(QScreen *qScreen)
{
    MythDisplay::ScreenChanged(qScreen);

    if (m_device && m_device->GetScreen() != m_screen)
        m_device = nullptr;

    if (!m_device)
    {
        m_device = MythDRMDevice::Create(m_screen);
        if (!m_device->IsValid())
            m_device = nullptr;
    }

    emit screenChanged();
}

void MythDisplayDRM::UpdateCurrentMode()
{
    if (m_device)
    {
        m_refreshRate  = m_device->GetRefreshRate();
        m_resolution   = m_device->GetResolution();
        m_physicalSize = m_device->GetPhysicalSize();
        m_edid         = m_device->GetEDID();
        m_modeComplete = true;
        return;
    }
    MythDisplay::UpdateCurrentMode();
}
