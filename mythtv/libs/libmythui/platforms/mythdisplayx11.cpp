// MythTV
#include "mythdisplayx11.h"
#include "mythxdisplay.h"

MythDisplayX11::MythDisplayX11()
  : MythDisplay()
{
}

MythDisplayX11::~MythDisplayX11()
{
}

bool MythDisplayX11::IsAvailable(void)
{
    static bool checked = false;
    static bool available = false;
    if (!checked)
    {
        checked = true;
        MythXDisplay display;
        available = display.Open();
    }
    return available;
}

DisplayInfo MythDisplayX11::GetDisplayInfo(int VideoRate)
{
    DisplayInfo ret;
    MythXDisplay *disp = OpenMythXDisplay();
    if (!disp)
        return ret;

    float rate = disp->GetRefreshRate();
    if (VALID_RATE(rate))
        ret.m_rate = 1000000.0F / rate;
    else
        ret.m_rate = SanitiseRefreshRate(VideoRate);
    ret.m_res  = disp->GetDisplaySize();
    ret.m_size = disp->GetDisplayDimensions();
    delete disp;
    return ret;
}
