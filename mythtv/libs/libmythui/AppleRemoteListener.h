#ifndef APPLEREMOTELISTENER
#define APPLEREMOTELISTENER

#include "AppleRemote.h"

class AppleRemoteListener: public AppleRemote::Listener
{
public:
    explicit AppleRemoteListener(QObject* mainWindow_)
        : mainWindow(mainWindow_) {}

    // virtual
    void appleRemoteButton(AppleRemote::Event button, bool pressedDown);

private:
    QObject *mainWindow;
};

#endif // APPLEREMOTELISTENER
