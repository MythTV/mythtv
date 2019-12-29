// MythTV
#include "mythdisplaywindows.h"

MythDisplayWindows::MythDisplayWindows()
  : MythDisplay()
{
    Initialise();
}

MythDisplayWindows::~MythDisplayWindows()
{
}

void MythDisplayWindows::UpdateCurrentMode(void)
{
    HDC hdc = GetDC((HWND)GetWindowID());
    if (!hdc)
    {
        MythDisplay::UpdateCurrentMode();
        return;
    }

    int rate   = GetDeviceCaps(hdc, VREFRESH);
    m_physicalSize = QSize(GetDeviceCaps(hdc, HORZSIZE),
                           GetDeviceCaps(hdc, VERTSIZE));
    m_resolution = QSize(GetDeviceCaps(hdc, HORZRES),
                         GetDeviceCaps(hdc, VERTRES));
    m_modeComplete = true;

    // see http://support.microsoft.com/kb/2006076
    switch (rate)
    {
        case 23:  m_refreshRate = 23.976;  break;
        case 29:  m_refreshRate = 29.970;  break;
        case 47:  m_refreshRate = 47.952;  break;
        case 59:  m_refreshRate = 59.940;  break;
        case 71:  m_refreshRate = 71.928;  break;
        case 119: m_refreshRate = 119.880; break;
        default:  m_refreshRate = static_cast<double>(rate);
    }
}
