#include "mythuistatetracker.h"
#include "frontend.h"

DTC::FrontendStatus* Frontend::GetStatus(void)
{
    DTC::FrontendStatus *status = new DTC::FrontendStatus();
    MythUIStateTracker::GetFreshState(status->State());
    return status;
}
