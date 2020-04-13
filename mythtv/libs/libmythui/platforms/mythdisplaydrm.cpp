// MythTV
#include "mythdrmdevice.h"
#include "mythdisplaydrm.h"

#define LOC QString("DispDRM: ")

MythDisplayDRM::MythDisplayDRM()
{
    m_device = new MythDRMDevice(m_screen);
    if (!m_device->IsValid())
    {
        delete m_device;
        m_device = nullptr;
    }
    Initialise();
}

MythDisplayDRM::~MythDisplayDRM()
{
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
    {
        m_device = new MythDRMDevice(m_screen);
        if (!m_device->IsValid())
        {
            delete m_device;
            m_device = nullptr;
        }
    }
}

void MythDisplayDRM::UpdateCurrentMode(void)
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
