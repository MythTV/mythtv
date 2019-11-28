// MythTV
#include "mythdisplaywindows.h"

MythDisplayWindows::MythDisplayWindows()
  : MythDisplay()
{
}

MythDisplayWindows::~MythDisplayWindows()
{
}

DisplayInfo MythDisplayWindows::GetDisplayInfo(int VideoRate)
{
    DisplayInfo ret;
    HDC hdc = GetDC((HWND)GetWindowID());
    int rate = 0;
    if (hdc)
    {
        rate       = GetDeviceCaps(hdc, VREFRESH);
        int width  = GetDeviceCaps(hdc, HORZSIZE);
        int height = GetDeviceCaps(hdc, VERTSIZE);
        ret.m_size = QSize((uint)width, (uint)height);
        width      = GetDeviceCaps(hdc, HORZRES);
        height     = GetDeviceCaps(hdc, VERTRES);
        ret.m_res  = QSize((uint)width, (uint)height);
    }

    if (VALID_RATE(rate))
    {
        // see http://support.microsoft.com/kb/2006076
        switch (rate)
        {
            case 23:  ret.m_rate = 41708; break; // 23.976Hz
            case 29:  ret.m_rate = 33367; break; // 29.970Hz
            case 47:  ret.m_rate = 20854; break; // 47.952Hz
            case 59:  ret.m_rate = 16683; break; // 59.940Hz
            case 71:  ret.m_rate = 13903; break; // 71.928Hz
            case 119: ret.m_rate = 8342;  break; // 119.880Hz
            default:  ret.m_rate = 1000000.0F / (float)rate;
        }
    }
    else
        ret.m_rate = SanitiseRefreshRate(VideoRate);
    return ret;
}
