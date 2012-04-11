#ifndef SCHEDULEDRECORDING_H
#define SCHEDULEDRECORDING_H

#include "mythtvexp.h"
#include "qdatetime.h"
#include "recordinginfo.h"

class MTV_PUBLIC ScheduledRecording
{
    friend class Scheduler;

  public:
    // Use when a recording rule or program data changes.  Use 0 for
    // recordid when all recordids are potentially affected, Use
    // invalid starttime and 0 for chanids when not time nor channel
    // specific.
    static void RescheduleMatch(uint recordid, uint sourceid, uint mplexid,
                             const QDateTime &maxstarttime, const QString &why)
        { SendReschedule(BuildMatchRequest(recordid, sourceid, mplexid,
                                           maxstarttime, why)); };

    // Use when previous or current recorded duplicate status changes.
    static void RescheduleCheck(const RecordingInfo &recinfo, 
                                const QString &why)
        { SendReschedule(BuildCheckRequest(recinfo, why)); };

    // Use when none of recording rule, program data or duplicate
    // status changes.
    static void ReschedulePlace(const QString &why)
        { SendReschedule(BuildPlaceRequest(why)); };

  private:
    ScheduledRecording();
    ~ScheduledRecording();

    static void SendReschedule(const QStringList &request);
    static QStringList BuildMatchRequest(uint recordid, uint sourceid, 
              uint mplexid, const QDateTime &maxstarttime, const QString &why);
    static QStringList BuildCheckRequest(const RecordingInfo &recinfo,
                                         const QString &why);
    static QStringList BuildPlaceRequest(const QString &why);
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

