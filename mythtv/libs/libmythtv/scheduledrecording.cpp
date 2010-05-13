#include "scheduledrecording.h"
#include "mythcorecontext.h"

ScheduledRecording::ScheduledRecording()
{
}

ScheduledRecording::~ScheduledRecording()
{
}

void ScheduledRecording::signalChange(int recordid)
{
    if (gCoreContext->IsBackend())
    {
        MythEvent me(QString("RESCHEDULE_RECORDINGS %1").arg(recordid));
        gCoreContext->dispatch(me);
    }
    else
    {
        QStringList slist;
        slist << QString("RESCHEDULE_RECORDINGS %1").arg(recordid);
        if (!gCoreContext->SendReceiveStringList(slist))
            VERBOSE(VB_IMPORTANT, QString("Error rescheduling id %1 "
                                    "in ScheduledRecording::signalChange")
                                    .arg(recordid));
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
