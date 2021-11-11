
#include "devices/AppleRemoteListener.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QString>
#include "devices/lircevent.h"

void AppleRemoteListener::appleRemoteButton(AppleRemote::Event button,
                                            bool pressedDown)
{
    QString code = nullptr;
    bool separateRelease = false;

    switch (button)
    {
        case AppleRemote::Up:
            code="Up";
            separateRelease=true;
            break;
        case AppleRemote::Down:
            code="Down";
            separateRelease=true;
            break;
        case AppleRemote::Menu:
            code="Esc";
            break;
        case AppleRemote::Select:
            code="Enter";
            break;
        case AppleRemote::Right:
            code="Right";
            break;
        case AppleRemote::Left:
            code="Left";
            break;
        case AppleRemote::RightHold:
            code="End";
            separateRelease=true;
            break;
        case AppleRemote::LeftHold:
            code="Home";
            separateRelease=true;
            break;
        case AppleRemote::MenuHold:
            code="M";
            break;
        case AppleRemote::PlayPause:
        case AppleRemote::PlayHold:
            code="P";
            break;
        case AppleRemote::ControlSwitched:
            return;
        case AppleRemote::Undefined:
            break;
    }
    QKeySequence a(code);
    for (int i = 0; i < a.count(); i++)
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int keycode = a[i];
#else
        int keycode = a[i].toCombined();
#endif
        if (pressedDown)
            QCoreApplication::postEvent(mainWindow, new LircKeycodeEvent(
                QEvent::KeyPress,   keycode, Qt::NoModifier, code, code));

        if (!separateRelease || !pressedDown)
            QCoreApplication::postEvent(mainWindow, new LircKeycodeEvent(
                QEvent::KeyRelease, keycode, Qt::NoModifier, code, code));
    }
}
