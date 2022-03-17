#include "scheduledrecording.h"
#include "libmythbase/mythcorecontext.h"

void ScheduledRecording::SendReschedule(const QStringList &request)
{
    if (gCoreContext->IsBackend())
    {
        MythEvent me(QString("RESCHEDULE_RECORDINGS"), request);
        gCoreContext->dispatch(me);
    }
    else
    {
        QStringList slist;
        slist << QString("RESCHEDULE_RECORDINGS");
        slist << request;
        if (!gCoreContext->SendReceiveStringList(slist))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error rescheduling %1 in "
                        "ScheduledRecording::SendReschedule").arg(request[0]));
        }
    }
}

QStringList ScheduledRecording::BuildMatchRequest(uint recordid,
                uint sourceid, uint mplexid, const QDateTime &maxstarttime,
                const QString &why)
{
    return QStringList(QString("MATCH %1 %2 %3 %4 %5")
                       .arg(recordid).arg(sourceid).arg(mplexid)
                       .arg(maxstarttime.isValid()
                            ? maxstarttime.toString(Qt::ISODate)
                            : "-",
                            why));
};

QStringList ScheduledRecording::BuildCheckRequest(const RecordingInfo &recinfo,
                                                  const QString &why)
{
    return QStringList(QString("CHECK %1 %2 %3 %4")
                       .arg(recinfo.GetRecordingStatus())
                       .arg(recinfo.GetParentRecordingRuleID() ?
                            recinfo.GetParentRecordingRuleID() :
                            recinfo.GetRecordingRuleID())
                       .arg(recinfo.GetFindID())
                       .arg(why))
        << recinfo.GetTitle()
        << recinfo.GetSubtitle()
        << recinfo.GetDescription()
        << recinfo.GetProgramID();
};

QStringList ScheduledRecording::BuildPlaceRequest(const QString &why)
{
    return QStringList(QString("PLACE %1").arg(why));
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
