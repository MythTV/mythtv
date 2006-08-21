#include <qapplication.h>
#include <qkeysequence.h>
#include "lircevent.h"

#include "AppleRemoteListener.h"

AppleRemoteListener::AppleRemoteListener(QObject* mainWindow_)
                   : mainWindow(mainWindow_)
{
}

void
AppleRemoteListener::appleRemoteButton(AppleRemote::Event button,
                                       bool pressedDown)
{
    char* code = 0;
    bool separateRelease = false;

    switch (button)
    {
        case AppleRemote::VolumePlus:
        {
            code="Up";
            separateRelease=true;
            break;
        }
        case AppleRemote::VolumeMinus:
        {
            code="Down";
            separateRelease=true;
            break;
        }
        case AppleRemote::Menu:
        {
            code="Esc";
            break;
        }
        case AppleRemote::Play: {
            code="Enter";
            break;
        }
        case AppleRemote::Right:
        {
            code="Right";
            break;
        }
        case AppleRemote::Left:
        {
            code="Left";
            break;
        }
        case AppleRemote::RightHold:
        {
            code="End";
            separateRelease=true;
            break;
        }
        case AppleRemote::LeftHold:
        {
            code="Home";
            separateRelease=true;
            break;
        }
        case AppleRemote::MenuHold:
        {
            code="M";
            break;
        }
        case AppleRemote::PlaySleep:
        {
            code="P";
            break;
        }
        case AppleRemote::ControlSwitched:
            return;
    }
    QKeySequence a(code);
    int keycode = 0;
    for (unsigned int i = 0; i < a.count(); i++)
    {
        keycode = a[i];

        QApplication::postEvent(mainWindow, new LircKeycodeEvent(code, keycode, pressedDown));

        if (!separateRelease)
            QApplication::postEvent(mainWindow, new LircKeycodeEvent(code, keycode, false));
    }
  
}
