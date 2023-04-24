#include "libmythbase/mythlogging.h"
#include "platforms/mythscreensaverandroid.h"
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QtAndroidExtras>
#else
#include <QCoreApplication>
#include <QJniObject>
#define QAndroidJniObject QJniObject
#endif

// call in java is :
//
// getWindow().addFlags(
//   WindowManager.
//      LayoutParams.FLAG_FULLSCREEN |
//      WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

#define LOC      QString("ScreenSaverAndroid: ")

#define MODE 1
MythScreenSaverAndroid::MythScreenSaverAndroid(QObject *Parent)
  : MythScreenSaver(Parent)
{
    //jint keepScreenOn = QAndroidJniObject::getStaticObjectField<jint>("android/view/WindowManager/", "ACTION_CALL");
}

MythScreenSaverAndroid::~MythScreenSaverAndroid()
{
    MythScreenSaverAndroid::Restore();
}

void MythScreenSaverAndroid::Disable()
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QAndroidJniObject activity = QtAndroid::androidActivity();
#else
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
#endif
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

void MythScreenSaverAndroid::Restore()
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QAndroidJniObject activity = QtAndroid::androidActivity();
#else
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
#endif
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

void MythScreenSaverAndroid::Reset()
{
    // Wake up the screen saver now.
    LOG(VB_GENERAL, LOG_INFO, LOC + "reset");
}

bool MythScreenSaverAndroid::Asleep()
{
    return false;
}

