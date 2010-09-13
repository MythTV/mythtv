
#include "AppleRemoteListener.h"

#include <QCoreApplication>
#include <QKeySequence>
#include "lircevent.h"

AppleRemoteListener::AppleRemoteListener(QObject* mainWindow_)
                   : mainWindow(mainWindow_)
{
}

void AppleRemoteListener::appleRemoteButton(AppleRemote::Event button,
                                            bool pressedDown)
{
    char* code = 0;
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
    }
    QKeySequence a(code);
    int keycode = 0;
    for (unsigned int i = 0; i < a.count(); i++)
    {
        keycode = a[i];

        if (pressedDown)
            QCoreApplication::postEvent(mainWindow, new LircKeycodeEvent(
                QEvent::KeyPress,   keycode, Qt::NoModifier, code, code));

        if (!separateRelease || !pressedDown)
            QCoreApplication::postEvent(mainWindow, new LircKeycodeEvent(
                QEvent::KeyRelease, keycode, Qt::NoModifier, code, code));
    }
  
}
