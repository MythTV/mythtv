#include <QCoreApplication>
#include "mythevent.h"
#include "mythuistatetracker.h"
#include "mythmainwindow.h"
#include "frontend.h"

DTC::FrontendStatus* Frontend::GetStatus(void)
{
    DTC::FrontendStatus *status = new DTC::FrontendStatus();
    MythUIStateTracker::GetFreshState(status->State());
    return status;
}

bool Frontend::SendMessage(const QString &Message)
{
    if (Message.isEmpty())
        return false;

    qApp->postEvent(GetMythMainWindow(),
                    new MythEvent(MythEvent::MythUserMessage, Message));
    return true;
}
