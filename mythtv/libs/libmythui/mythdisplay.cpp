#include "mythdisplay.h"
#include "mythmainwindow.h"

#if defined(Q_WS_MAC) 
#import "util-osx.h"
#elif defined(Q_WS_WIN)
#include <windows.h>
#elif defined(Q_WS_X11)
#include "mythxdisplay.h"
#elif USING_DIRECTFB
#include <linux/fb.h>
extern "C" {
#include <directfb.h>
#include <directfb_version.h>
}
#endif

WId MythDisplay::GetWindowID(void)
{
    WId win = 0;
    QWidget *main_widget = (QWidget*)MythMainWindow::getMainWindow();
    if (main_widget)
        win = main_widget->winId();
    return win;
}

DisplayInfo MythDisplay::GetDisplayInfo(void)
{
    DisplayInfo ret;

#if defined(Q_WS_MAC)
    CGDirectDisplayID disp = GetOSXDisplay(GetWindowID());
    if (!disp)
        return ret;

    CFDictionaryRef ref = CGDisplayCurrentMode(disp);
    if (ref)
    {
        float rate = get_float_CF(ref, kCGDisplayRefreshRate);
        // N.B. A rate of zero typically indicates the internal macbook
        // lcd display which does not have a rate in the traditional sense
        if (rate > 20 && rate < 200)
            ret.rate = 1000000.0f / rate;
    }

    CGSize size_in_mm = CGDisplayScreenSize(disp);
    ret.size = QSize((uint) size_in_mm.width, (uint) size_in_mm.height);

    uint width  = (uint)CGDisplayPixelsWide(disp);
    uint height = (uint)CGDisplayPixelsHigh(disp);
    ret.res     = QSize(width, height);

#elif defined(Q_WS_WIN)
    HDC hdc = GetDC(GetWindowID());
    if (hdc)
    {
        int rate = GetDeviceCaps(hdc, VREFRESH);
        if (rate > 20 && rate < 200)
        {
            // see http://support.microsoft.com/kb/2006076
            switch (rate)
            {
                case 23:  ret.rate = 41708; break; // 23.976Hz
                case 29:  ret.rate = 33367; break; // 29.970Hz
                case 47:  ret.rate = 20854; break; // 47.952Hz
                case 59:  ret.rate = 16683; break; // 59.940Hz
                case 71:  ret.rate = 13903; break; // 71.928Hz
                case 119: ret.rate = 8342;  break; // 119.880Hz
                default:  ret.rate = 1000000.0f / (float)rate;
            }
        }
        int width  = GetDeviceCaps(hdc, HORZSIZE);
        int height = GetDeviceCaps(hdc, VERTSIZE);
        ret.size   = QSize((uint)width, (uint)height);
        width      = GetDeviceCaps(hdc, HORZRES);
        height     = GetDeviceCaps(hdc, VERTRES);
        ret.res    = QSize((uint)width, (uint)height);
    }

#elif defined (Q_WS_X11)
    MythXDisplay *disp = OpenMythXDisplay();
    if (!disp)
        return ret;
    
    ret.rate = disp->GetRefreshRate();
    ret.res  = disp->GetDisplaySize();
    ret.size = disp->GetDisplayDimensions();
    delete disp;
#elif USING_DIRECTFB
    int fh, v;
    struct fb_var_screeninfo si;
    double drate;
    double hrate;
    double vrate;
    long htotal;
    long vtotal;
    const char *fb_dev_name = NULL;
    if (!(fb_dev_name = getenv("FRAMEBUFFER")))
        fb_dev_name = "/dev/fb0";

    fh = open(fb_dev_name, O_RDONLY);
    if (-1 == fh)
        return ret;

    if (ioctl(fh, FBIOGET_VSCREENINFO, &si))
    {
        close(fh);
        return ret;
    }

    htotal = si.left_margin + si.xres + si.right_margin + si.hsync_len;
    vtotal = si.upper_margin + si.yres + si.lower_margin + si.vsync_len;

    switch (si.vmode & FB_VMODE_MASK)
    {
        case FB_VMODE_INTERLACED:
            break;
        case FB_VMODE_DOUBLE:
            vtotal <<= 2;
            break;
        default:
            vtotal <<= 1;
    }

    drate = 1E12 / si.pixclock;
    hrate = drate / htotal;
    vrate = hrate / vtotal * 2;

    v = (int)(1E3 / vrate + 0.5);
    /* h = hrate / 1E3; */
    ret.rate = v;
    close(fh);
#endif
    return ret;
}



