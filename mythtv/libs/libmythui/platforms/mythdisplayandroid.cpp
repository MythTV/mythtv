// Qt
#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QtAndroidExtras>
#else
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#define QAndroidJniEnvironment QJniEnvironment
#define QAndroidJniObject QJniObject
#endif

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythdisplayandroid.h"

#define LOC QString("Display: ")

MythDisplayAndroid::MythDisplayAndroid()
  : MythDisplay()
{
    Initialise();
}

MythDisplayAndroid::~MythDisplayAndroid()
{
}

void MythDisplayAndroid::UpdateCurrentMode(void)
{
    QAndroidJniEnvironment env;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QAndroidJniObject activity = QtAndroid::androidActivity();
#else
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
#endif
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

    m_refreshRate  = static_cast<double>(rate);
    m_resolution   = QSize(width, height);
    m_physicalSize = QSize(width, height);
    m_modeComplete = true;
    if (xdpi > 0 && ydpi > 0)
    {
        m_physicalSize = QSize(static_cast<int>(width  / xdpi * 25.4F),
                               static_cast<int>(height / ydpi * 25.4F));
    }
}
