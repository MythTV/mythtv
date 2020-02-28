#include "screensaver-android.h"
#include <QtAndroidExtras>
#include <mythlogging.h>

// call in java is :
//
// getWindow().addFlags(
//   WindowManager.
//      LayoutParams.FLAG_FULLSCREEN |
//      WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

#define LOC      QString("ScreenSaverAndroid: ")

#define MODE 1
ScreenSaverAndroid::ScreenSaverAndroid()
{
    //jint keepScreenOn = QAndroidJniObject::getStaticObjectField<jint>("android/view/WindowManager/", "ACTION_CALL");
}

ScreenSaverAndroid::~ScreenSaverAndroid()
{
    ScreenSaverAndroid::Restore();
}

void ScreenSaverAndroid::Disable(void)
{
    QAndroidJniObject activity = QtAndroid::androidActivity();
    LOG(VB_GENERAL, LOG_INFO, LOC + "disable");
    if (activity.isValid()) {
        LOG(VB_GENERAL, LOG_INFO, LOC + "disable 1");
#if MODE
        activity.callMethod<void>("setSuspendSleep", "()V");
#else
        QAndroidJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");

        if (window.isValid()) {
            const int FLAG_KEEP_SCREEN_ON = 128;
            //QAndroidJniObject keepScreenOn = QAndroidJniObject::fromString("WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON");
            window.callObjectMethod("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
            //window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
            LOG(VB_GENERAL, LOG_INFO, LOC + "disable 2");
        }
#endif
    }
}

void ScreenSaverAndroid::Restore(void)
{
    QAndroidJniObject activity = QtAndroid::androidActivity();
    LOG(VB_GENERAL, LOG_INFO, LOC + "restore");
    if (activity.isValid()) {
        LOG(VB_GENERAL, LOG_INFO, LOC + "restore 1");
#if MODE
        activity.callMethod<void>("setAllowSleep", "()V");
#else
        QAndroidJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (window.isValid()) {
            const int FLAG_KEEP_SCREEN_ON = 128;
            //QAndroidJniObject keepScreenOn = QAndroidJniObject::fromString("WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON");
            window.callObjectMethod("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
            //window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
            LOG(VB_GENERAL, LOG_INFO, LOC + "restore 2");
        }
#endif
    }
}

void ScreenSaverAndroid::Reset(void)
{
    // Wake up the screen saver now.
    LOG(VB_GENERAL, LOG_INFO, LOC + "reset");
}

bool ScreenSaverAndroid::Asleep(void)
{
    return false;
}

