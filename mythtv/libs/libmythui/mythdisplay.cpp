#include "compat.h"
#include "mythcorecontext.h"
#include "mythdisplay.h"
#include "mythmainwindow.h"

#include <QApplication>

#if defined(Q_OS_MAC)
#import "util-osx.h"
#elif USING_X11
#include "mythxdisplay.h"
#endif

#ifdef Q_OS_ANDROID
#include <QtAndroidExtras>
#endif

#include <mythlogging.h>
#define LOC      QString("DispInfo: ")

#define VALID_RATE(rate) ((rate) > 20.0F && (rate) < 200.0F)

static float fix_rate(int video_rate)
{
    static const float default_rate = 1000000.0F / 60.0F;
    float fixed = default_rate;
    if (video_rate > 0)
    {
        fixed = static_cast<float>(video_rate) / 2.0F;
        if (fixed < default_rate)
            fixed = default_rate;
    }
    return fixed;
}

WId MythDisplay::GetWindowID(void)
{
    WId win = 0;
    QWidget *main_widget = (QWidget*)MythMainWindow::getMainWindow();
    if (main_widget)
        win = main_widget->winId();
    return win;
}

DisplayInfo MythDisplay::GetDisplayInfo(int video_rate)
{
    DisplayInfo ret;

#if defined(Q_OS_MAC)
    CGDirectDisplayID disp = GetOSXDisplay(GetWindowID());
    if (!disp)
        return ret;

    CFDictionaryRef ref = CGDisplayCurrentMode(disp);
    float rate = 0.0F;
    if (ref)
        rate = get_float_CF(ref, kCGDisplayRefreshRate);

    if (VALID_RATE(rate))
        ret.m_rate = 1000000.0F / rate;
    else
        ret.m_rate = fix_rate(video_rate);

    CGSize size_in_mm = CGDisplayScreenSize(disp);
    ret.m_size = QSize((uint) size_in_mm.width, (uint) size_in_mm.height);

    uint width  = (uint)CGDisplayPixelsWide(disp);
    uint height = (uint)CGDisplayPixelsHigh(disp);
    ret.m_res   = QSize(width, height);

#elif defined(Q_OS_WIN)
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
        ret.m_rate = fix_rate(video_rate);

#elif USING_X11
    MythXDisplay *disp = OpenMythXDisplay();
    if (!disp)
        return ret;

    float rate = disp->GetRefreshRate();
    if (VALID_RATE(rate))
        ret.m_rate = 1000000.0F / rate;
    else
        ret.m_rate = fix_rate(video_rate);
    ret.m_res  = disp->GetDisplaySize();
    ret.m_size = disp->GetDisplayDimensions();
    delete disp;
#elif defined(Q_OS_ANDROID)
    QAndroidJniEnvironment env;
    QAndroidJniObject activity = QtAndroid::androidActivity();
    QAndroidJniObject windowManager =  activity.callObjectMethod("getWindowManager", "()Landroid/view/WindowManager;");
    QAndroidJniObject display =  windowManager.callObjectMethod("getDefaultDisplay", "()Landroid/view/Display;");
    QAndroidJniObject displayMetrics("android/util/DisplayMetrics");
    display.callMethod<void>("getRealMetrics", "(Landroid/util/DisplayMetrics;)V", displayMetrics.object());
    // check if passed or try a different method
    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
        display.callMethod<void>("getMetrics", "(Landroid/util/DisplayMetrics;)V", displayMetrics.object());
    }
    float xdpi = displayMetrics.getField<jfloat>("xdpi");
    float ydpi = displayMetrics.getField<jfloat>("ydpi");
    int height = displayMetrics.getField<jint>("heightPixels");
    int width = displayMetrics.getField<jint>("widthPixels");
    float rate = display.callMethod<jfloat>("getRefreshRate");
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("rate:%1 h:%2 w:%3 xdpi:%4 ydpi:%5")
        .arg(rate).arg(height).arg(width)
        .arg(xdpi).arg(ydpi)
        );

    if (VALID_RATE(rate))
        ret.m_rate = 1000000.0F / rate;
    else
        ret.m_rate = fix_rate(video_rate);
    ret.m_res = QSize((uint)width, (uint)height);
    ret.m_size = QSize((uint)width, (uint)height);
    if (xdpi > 0 && ydpi > 0)
        ret.m_size = QSize((uint)width/xdpi*25.4F, (uint)height/ydpi*25.4F);
#endif
    return ret;
}

/**
 * \brief Return the number of available screens.
 *
 * This is the number of screens configured to be part of the users
 * desktop, at the time the call is made.  If the user closes the
 * laptop lid, or connects an external screen, this number may be
 * different the next time this function is called.
 */
int MythDisplay::GetNumberOfScreens(void)
{
    return qGuiApp->screens().size();
}

/**
 * \brief Return true if the MythTV windows should span all screens.
 */
bool MythDisplay::SpanAllScreens(void)
{
    return gCoreContext->GetSetting("XineramaScreen", nullptr) == "-1";
}

/**
 * \brief Return a pointer to the screen to use.
 *
 * This function looks at the users screen preference, and will return
 * that screen if possible.  If not, i.e. the screen isn't plugged in,
 * then this function returns the system's primary screen.
 *
 * Note: There is no special case here for the case of MythTV spanning
 * all screens, as all screen have access to the virtual desktop
 * attributes.  The check for spanning screens must be made when the
 * screen size/geometry accessed, and the proper physical/virtual
 * size/geometry retrieved.
 */
QScreen* MythDisplay::GetScreen(void)
{
    // Lookup by name
    QString name = gCoreContext->GetSetting("XineramaScreen", nullptr);
    foreach (QScreen *qscreen, qGuiApp->screens()) {
        if (name == qscreen->name()) {
            LOG(VB_GUI, LOG_INFO, LOC +
                QString("found screen %1").arg(name));
            return qscreen;
        }
    }

    // No name match.  These were previously numbers.
    bool ok = false;
    int screen_num = name.toInt(&ok);
    QList<QScreen *>screens = qGuiApp->screens();
    if (ok && (screen_num >= 0) && (screen_num < screens.size())) {
        LOG(VB_GUI, LOG_INFO, LOC +
            QString("found screen number %1 (%2)")
            .arg(name).arg(screens[screen_num]->name()));
        return screens[screen_num];
    }

    // For aything else, return the primary screen.
    QScreen *primary = qGuiApp->primaryScreen();
    if (name != "-1")
        LOG(VB_GUI, LOG_INFO,
            QString("screen %1 not found, defaulting to primary screen (%2)")
            .arg(name).arg(primary->name()));
    return primary;
}

QString MythDisplay::GetExtraScreenInfo(QScreen *qscreen)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    QString mfg = qscreen->manufacturer();
    if (mfg.isEmpty())
        mfg = "unknown";
    QString model = qscreen->model();
    if (model.isEmpty())
        model = "unknown";
    return QString("(%1 %2)").arg(mfg).arg(model);
#else
    Q_UNUSED(qscreen);
    return QString();
#endif
}
