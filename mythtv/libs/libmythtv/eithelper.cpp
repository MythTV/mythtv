// -*- Mode: c++ -*-

// MythTV includes
#include "eithelper.h"
#include "mythdbcon.h"
#include "scheduledrecording.h"

static int get_chan_id_for_event(int tid_db, const Event &event);
static void update_eit_list_in_db(int mplexid, const QList_Events *events);
static uint update_eit_in_db(MSqlQuery &query, MSqlQuery &query2,
                             int chanid, const Event &event);

void EITHelper::HandleEITs(QMap_Events* eventList)
{
    VERBOSE(VB_IMPORTANT, "HandleEITs()");
    QList_Events* events = new QList_Events();
    eitList_lock.lock();
    QMap_Events::Iterator e;
    for (e = eventList->begin() ; e != eventList->end() ; ++e)
        events->append(*e);
    eitList.append(events);
    eitList_lock.unlock();
}

uint EITHelper::GetListSize(void) const
{
    eitList_lock.lock();
    uint size = eitList.size();
    eitList_lock.unlock();
    return size;
}

void EITHelper::ProcessEvents(int mplexid)
{
    VERBOSE(VB_IMPORTANT, "ProcessEvents("<<mplexid<<")");
    while (GetListSize())
    {
        eitList_lock.lock();
        QList_Events *events = eitList.first();
        eitList.pop_front();
        eitList_lock.unlock();

        update_eit_list_in_db(mplexid, events);

        events->clear();
        delete events;
    }
}

void update_eit_list_in_db(int mplexid, const QList_Events *events)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());
    // All events in list are assumed to be for the same channel here
    int chanID = get_chan_id_for_event(mplexid, *(events->begin()));

    if (chanID != -1)
    {
        uint counter = 0;
        QList_Events::const_iterator e = events->begin();
        for (; e != events->end(); ++e)
            counter += update_eit_in_db(query, query2, chanID, *e);

        if (counter > 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Added %1 scheduler events, running scheduler "
                            "to check for updates").arg(counter));
            ScheduledRecording::signalChange(-1);
            QString msg = QString("Added %1 scheduler events").arg(counter);
            gContext->LogEntry("DVB/ATSC Guide Scanner", LP_INFO, msg, "");
        }
    }
}

static int get_chan_id_for_event(int tid_db, const Event &event)
{
    MSqlQuery query(MSqlQuery::InitCon());
    // Now figure out the chanid for this
    // networkid/serviceid/sourceid/transport?
    // if the event is associated with an ATSC channel the
    // linkage to chanid is different than DVB
    if (event.ATSC)
    {
        query.prepare(QString("SELECT chanid, useonairguide FROM channel "
                              "WHERE atscsrcid = %1 AND mplexid = %2")
                      .arg(event.ServiceID)
                      .arg(tid_db));
    }
    else
    {
        /* DVB Link to chanid */
        query.prepare(QString("SELECT chanid, useonairguide "
                              "FROM channel, dtv_multiplex "
                              "WHERE serviceid = %1 AND "
                              "      networkid = %2 AND "
                              "      channel.mplexid = dtv_multiplex.mplexid")
                      .arg(event.ServiceID).arg(event.NetworkID));
    }

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Looking up chanID", query);

    if (query.size() <= 0)
    {
        VERBOSE(VB_IMPORTANT, "ChanID not found "
                "so event updates were skipped.");
        return -1;
    }

    // Check to see if we are interseted in this channel
    query.next();
    bool useOnAirGuide = query.value(1).toBool();
    return (useOnAirGuide) ? query.value(0).toInt() : -1;
}

static uint update_eit_in_db(MSqlQuery &query, MSqlQuery &query2,
                             int ChanID, const Event &event)
{
    uint counter = 0;

    query.prepare(
        "SELECT starttime, endtime, title "
        "FROM program "
        "WHERE chanid=:CHANID AND "
        "      ( ( starttime>=:STIME AND starttime<:ETIME ) AND NOT "
        "        ( starttime=:STIME AND endtime=:ETIME AND title=:TITLE ) )");

    query.bindValue(":CHANID", ChanID);
    query.bindValue(":STIME", event.StartTime.
                    toString(QString("yyyy-MM-dd hh:mm:00")));
    query.bindValue(":ETIME", event.EndTime.
                    toString(QString("yyyy-MM-dd hh:mm:00")));
    query.bindValue(":TITLE", event.Event_Name.utf8());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Checking Rescheduled Event", query);

    for (query.first(); query.isValid(); query.next())
    {
        // New guide data overriding existing
        // Possibly more than one conflict
        VERBOSE(VB_GENERAL, QString("Schedule Change on Channel %1")
                .arg(ChanID));
        VERBOSE(VB_GENERAL, QString("Old: %1 %2 %3")
                .arg(query.value(0).toString())
                .arg(query.value(1).toString())
                .arg(query.value(2).toString()));
        VERBOSE(VB_GENERAL, QString("New: %1 %2 %3")
                .arg(event.StartTime.
                     toString(QString("yyyy-MM-dd hh:mm:00")))
                .arg(event.EndTime.
                     toString(QString("yyyy-MM-dd hh:mm:00")))
                .arg(event.Event_Name.utf8()));
        // Delete old EPG record.
        query2.prepare("DELETE FROM program "
                       "WHERE chanid=:CHANID AND "
                       "      starttime=:STARTTIME AND "
                       "      endtime=:ENDTIME AND "
                       "      title=:TITLE");
        query2.bindValue(":CHANID", ChanID);
        query2.bindValue(":STARTTIME", query.value(0).toString());
        query2.bindValue(":ENDTIME", query.value(1).toString());
        query2.bindValue(":TITLE", query.value(2).toString());
        if (!query2.exec())
            MythContext::DBError("Deleting Rescheduled Event", query2);
        if (!query2.isActive())
            MythContext::DBError("Deleting Rescheduled Event", query2);
    }

    query.prepare("SELECT 1 FROM program "
                  "WHERE chanid=:CHANID AND "
                  "      starttime=:STARTTIME AND "
                  "      endtime=:ENDTIME AND "
                  "      title=:TITLE");
    query.bindValue(":CHANID", ChanID);
    query.bindValue(":STARTTIME", event.StartTime.
                    toString(QString("yyyy-MM-dd hh:mm:00")));
    query.bindValue(":ENDTIME", event.EndTime.
                    toString(QString("yyyy-MM-dd hh:mm:00")));
    query.bindValue(":TITLE", event.Event_Name.utf8());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Checking If Event Exists", query);

    if (query.size() <= 0)
    {
        counter++;

        query.prepare("REPLACE INTO program (chanid, starttime, "
                      "endtime, title, description, subtitle, "
                      "category, stereo, closecaptioned, hdtv, "
                      "airdate, originalairdate) "
                      "VALUES (:CHANID, :STARTTIME, "
                      ":ENDTIME, :TITLE, :DESCRIPTION, :SUBTITLE, "
                      ":CATEGORY, :STEREO, :CLOSECAPTIONED, :HDTV, "
                      ":AIRDATE, :ORIGINALAIRDATE);");
        query.bindValue(":CHANID", ChanID);
        query.bindValue(":STARTTIME", event.StartTime.
                        toString(QString("yyyy-MM-dd hh:mm:00")));
        query.bindValue(":ENDTIME", event.EndTime.
                        toString(QString("yyyy-MM-dd hh:mm:00")));
        query.bindValue(":TITLE", event.Event_Name.utf8());
        query.bindValue(":DESCRIPTION", event.Description.utf8());
        query.bindValue(":SUBTITLE", event.Event_Subtitle.utf8());
        query.bindValue(":CATEGORY", event.ContentDescription.utf8());
        query.bindValue(":STEREO", event.Stereo);
        query.bindValue(":CLOSECAPTIONED", event.SubTitled);
        query.bindValue(":HDTV", event.HDTV);
        query.bindValue(":AIRDATE", event.Year);
        query.bindValue(":ORIGINALAIRDATE", event.OriginalAirDate.
                        toString(QString("yyyy-MM-dd")));

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Adding Event", query);

        QValueList<Person>::const_iterator it = event.Credits.begin();
        for (;it != event.Credits.end(); it++)
        {
            query.prepare("INSERT IGNORE INTO people (name) "
                          "VALUES (:NAME);");
            query.bindValue(":NAME", (*it).name.utf8());

            if (!query.exec() || !query.isActive())
                MythContext::DBError("Adding Event (People)", query);

            query.prepare("INSERT INTO credits "
                          "(person, chanid, starttime, role) "
                          "SELECT person, :CHANID, :STARTTIME, :ROLE "
                          "FROM people WHERE people.name=:NAME");
            query.bindValue(":CHANID", ChanID);
            query.bindValue(":STARTTIME", event.StartTime.
                            toString(QString("yyyy-MM-dd hh:mm:00")));
            query.bindValue(":ROLE", (*it).role.utf8());
            query.bindValue(":NAME", (*it).name.utf8());

            if (!query.exec() || !query.isActive())
                MythContext::DBError("Adding Event (Credits)", query);
        }
    }
    return counter;
}
