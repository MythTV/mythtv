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

MythScreenSaverAndroid::MythScreenSaverAndroid(QObject *Parent)
  : MythScreenSaver(Parent)
{
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
    if (!activity.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Screen Saver Disable: activity is not valid");
        return;
    }
    QAndroidJniObject::callStaticMethod<void>(
        "org/mythtv/Utility",
        "setSuspendSleep",
        "(Landroid/app/Activity;)V",
        activity.object()
    );
    LOG(VB_GENERAL, LOG_INFO, LOC + "Screen Saver Disabled");
}

void MythScreenSaverAndroid::Restore()
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QAndroidJniObject activity = QtAndroid::androidActivity();
#else
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
#endif
    if (!activity.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Screen Saver Restore: activity is not valid");
        return;
    }
    QAndroidJniObject::callStaticMethod<void>(
        "org/mythtv/Utility",
        "setAllowSleep",
        "(Landroid/app/Activity;)V",
        activity.object()
    );
    LOG(VB_GENERAL, LOG_INFO, LOC + "Screen Saver Restored");
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

#include "moc_mythscreensaverandroid.cpp"
