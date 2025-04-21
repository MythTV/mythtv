#ifndef APPLEREMOTELISTENER
#define APPLEREMOTELISTENER

#include "devices/AppleRemote.h"

class AppleRemoteListener: public AppleRemote::Listener
{
public:
    explicit AppleRemoteListener(QObject* mainWindow_)
        : mainWindow(mainWindow_) {}

    // virtual
    void appleRemoteButton(AppleRemote::Event button, bool pressedDown) override;

private:
    QObject *mainWindow;
};

#endif // APPLEREMOTELISTENER
