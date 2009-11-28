#include "scheduledrecording.h"
#include "mythcontext.h"

ScheduledRecording::ScheduledRecording()
{
}

ScheduledRecording::~ScheduledRecording()
{
}

void ScheduledRecording::signalChange(int recordid)
{
    if (gContext->IsBackend())
    {
        MythEvent me(QString("RESCHEDULE_RECORDINGS %1").arg(recordid));
        gContext->dispatch(me);
    }
    else
    {
        QStringList slist;
        slist << QString("RESCHEDULE_RECORDINGS %1").arg(recordid);
        if (!gContext->SendReceiveStringList(slist))
            VERBOSE(VB_IMPORTANT, QString("Error rescheduling id %1 "
                                    "in ScheduledRecording::signalChange")
                                    .arg(recordid));
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
