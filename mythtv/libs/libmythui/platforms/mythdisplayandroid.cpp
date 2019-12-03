// Qt
#include <QtAndroidExtras>

// MythTV
#include "mythlogging.h"
#include "mythdisplayandroid.h"

#define LOC QString("Display: ")

MythDisplayAndroid::MythDisplayAndroid()
  : MythDisplay()
{
}

MythDisplayAndroid::~MythDisplayAndroid()
{
}

DisplayInfo MythDisplayAndroid::GetDisplayInfo(int VideoRate)
{
    DisplayInfo ret;
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
        ret.m_rate = SanitiseRefreshRate(VideoRate);
    ret.m_res = QSize((uint)width, (uint)height);
    ret.m_size = QSize((uint)width, (uint)height);
    if (xdpi > 0 && ydpi > 0)
        ret.m_size = QSize((uint)width/xdpi*25.4F, (uint)height/ydpi*25.4F);
    return ret;
}
