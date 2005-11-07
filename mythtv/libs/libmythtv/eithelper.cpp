// -*- Mode: c++ -*-

// MythTV includes
#include "eithelper.h"
#include "mythdbcon.h"

const uint EITHelper::kChunkSize = 20;

static int get_chan_id_from_db(int tid_db, const Event&);
static uint update_eit_in_db(MSqlQuery&, MSqlQuery&, int, const Event&);
static uint delete_invalid_programs_in_db(MSqlQuery&, MSqlQuery&, int, const Event&);
static bool has_program(MSqlQuery&, int, const Event&);
static bool insert_program(MSqlQuery&, int, const Event&);

void EITHelper::HandleEITs(QMap_Events* eventList)
{
    QList_Events* events = new QList_Events();
    QMap_Events::const_iterator it = eventList->begin();
    for (; it != eventList->end(); ++it)
    {
        Event *event = new Event();
        event->deepCopy((*it));
        events->push_back(event);
    }

    eitList_lock.lock();
    eitList.push_back(events);
    eitList_lock.unlock();
}

void EITHelper::ClearList(void)
{
    QMutexLocker locker(&eitList_lock);

    while (eitList.size())
    {
        QList_Events *events = eitList.front();
        eitList.pop_front();

        eitList_lock.unlock();
        QList_Events::iterator it = events->begin();
        for (; it != events->end(); ++it)
            delete *it;
        delete events;
        eitList_lock.lock();
    }
}

uint EITHelper::GetListSize(void) const
{
    QMutexLocker locker(&eitList_lock);
    return eitList.size();
}

/** \fn EITHelper::ProcessEvents(int)
 *  \brief Inserts events in EIT list.
 *
 *  NOTE: This currently takes a mplexid key for the dtv_multiplex table,
 *        but it should just take a sourceid key for the channel table.
 *
 *  \param mplexid multiplex we are inserting events for.
 *  \return Returns number of events inserted into DB.
 */
uint EITHelper::ProcessEvents(int mplexid)
{
    QMutexLocker locker(&eitList_lock);

    if (!eitList.size())
        return 0;

    uint insertCount = 0;
    if (eitList.front()->size() <= kChunkSize)
    {
        QList_Events *events = eitList.front();
        eitList.pop_front();

        eitList_lock.unlock();
        insertCount += UpdateEITList(mplexid, *events);
        QList_Events::iterator it = events->begin();
        for (; it != events->end(); ++it)
            delete *it;
        delete events;
        eitList_lock.lock();
    }
    else
    {
        QList_Events *events = eitList.front();
        QList_Events  subset;

        QList_Events::iterator subset_end = events->begin();
        for (uint i = 0; i < kChunkSize; ++i) ++subset_end;
        subset.insert(subset.end(), events->begin(), subset_end);
        events->erase(events->begin(), subset_end);
        
        eitList_lock.unlock();
        insertCount += UpdateEITList(mplexid, subset);
        QList_Events::iterator it = subset.begin();
        for (; it != subset.end(); ++it)
            delete *it;
        eitList_lock.lock();
    }

    if (insertCount != 0)
    {
        VERBOSE(VB_EIT, QString ("EITHelper: Added %1 events")
                .arg(insertCount));
    }
    return insertCount;
}

int EITHelper::GetChanID(int mplexid, const Event &event) const
{
    unsigned long long srv = event.ServiceID;
    srv |= ((unsigned long long) event.TransportID) << 32;
    srv |= (event.ATSC) ? (mplexid << 16) : (event.NetworkID << 16);

    int chanid = srv_to_chanid[srv];
    if (chanid == 0)
        srv_to_chanid[srv] = chanid = get_chan_id_from_db(mplexid, event);
    return chanid;
}

uint EITHelper::UpdateEITList(int mplexid, const QList_Events &events)
{
    MSqlQuery query1(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());

    uint counter = 0;
    QList_Events::const_iterator e = events.begin();
    for (; e != events.end(); ++e)
    {
        int chanid = get_chan_id_from_db(mplexid, **e);
        if (chanid <= 0)
            continue;
        counter += update_eit_in_db(query1, query2, chanid, **e);
    }
    return counter;
}

static int get_chan_id_from_db(int mplexid, const Event &event)
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
                      .arg(mplexid));
    }
    else
    {
        /* DVB Link to chanid */
        query.prepare(QString("SELECT chanid, useonairguide "
                              "FROM channel, dtv_multiplex "
                              "WHERE serviceid = %1 AND "
                              "      networkid = %2 AND "
                              "      transportid = %3 AND "
                              "      channel.mplexid = dtv_multiplex.mplexid")
                      .arg(event.ServiceID)
                      .arg(event.NetworkID)
                      .arg(event.TransportID));
    }

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Looking up chanID", query);

    if (query.size() <= 0)
    {
        VERBOSE(VB_EIT, QString(
                    "EITHelper: chanid not found for atscsrcid = %1 "
                    "and mplexid = %2, \n\t\t\tso event updates were skipped.")
            .arg(event.ServiceID).arg(mplexid));
        return -1;
    }

    // Check to see if we are interseted in this channel
    query.next();
    bool useOnAirGuide = query.value(1).toBool();
    return (useOnAirGuide) ? query.value(0).toInt() : -1;
}

static uint update_eit_in_db(MSqlQuery &query, MSqlQuery &query2,
                             int chanid, const Event &event)
{
    delete_invalid_programs_in_db(query, query2, chanid, event);

    bool ok = has_program(query, chanid, event);
    if (!ok)
        ok = insert_program(query, chanid, event);
    return (ok) ? 1 : 0;
}

static uint delete_invalid_programs_in_db(
    MSqlQuery &query, MSqlQuery &query2, int chanid, const Event &event)
{
    uint counter = 0;
    query.prepare(
        "SELECT starttime, endtime, title "
        "FROM program "
        "WHERE chanid=:CHANID AND "
        "      ( ( starttime>=:STIME AND starttime<:ETIME ) AND NOT "
        "        ( starttime=:STIME AND endtime=:ETIME AND title=:TITLE ) AND "
        "          manualid=0 );");

    query.bindValue(":CHANID", chanid);
    query.bindValue(":STIME", event.StartTime.
                    toString(QString("yyyy-MM-dd hh:mm:00")));
    query.bindValue(":ETIME", event.EndTime.
                    toString(QString("yyyy-MM-dd hh:mm:00")));
    query.bindValue(":TITLE", event.Event_Name.utf8());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Checking Rescheduled Event", query);

    query2.prepare("DELETE FROM program "
                   "WHERE chanid=:CHANID AND "
                   "      starttime=:STARTTIME AND "
                   "      endtime=:ENDTIME AND "
                   "      title=:TITLE");
    for (query.first(); query.isValid(); query.next())
    {
        counter++;
        // New guide data overriding existing
        // Possibly more than one conflict
        VERBOSE(VB_EIT, QString("Schedule Change on Channel %1")
                .arg(chanid));
        VERBOSE(VB_EIT, QString("Old: %1 %2 %3")
                .arg(query.value(0).toString())
                .arg(query.value(1).toString())
                .arg(query.value(2).toString()));
        VERBOSE(VB_EIT, QString("New: %1 %2 %3")
                .arg(event.StartTime.
                     toString(QString("yyyy-MM-dd hh:mm:00")))
                .arg(event.EndTime.
                     toString(QString("yyyy-MM-dd hh:mm:00")))
                .arg(event.Event_Name.utf8()));
        // Delete old EPG record.
        query2.bindValue(":CHANID", chanid);
        query2.bindValue(":STARTTIME", query.value(0).toString());
        query2.bindValue(":ENDTIME", query.value(1).toString());
        query2.bindValue(":TITLE", query.value(2).toString());
        if (!query2.exec() || !query2.isActive())
            MythContext::DBError("Deleting Rescheduled Event", query2);
    }
    return counter;
}

static bool has_program(MSqlQuery &query, int chanid, const Event &event)
{
    query.prepare("SELECT * FROM program "
                  "WHERE chanid=:CHANID AND "
                  "      starttime=:STARTTIME AND "
                  "      endtime=:ENDTIME AND "
                  "      title=:TITLE LIMIT 1");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", event.StartTime.toString("yyyy-MM-ddThh:mm:00"));
    query.bindValue(":ENDTIME", event.EndTime.toString("yyyy-MM-ddThh:mm:00"));
    query.bindValue(":TITLE", event.Event_Name.utf8());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Checking If Event Exists", query);

    return query.size() > 0;
}

static bool insert_program(MSqlQuery &query, int chanid, const Event &event)
{
    query.prepare("REPLACE INTO program ("
                  "  chanid,         starttime,  endtime,    title, "
                  "  description,    subtitle,   category,   stereo, "
                  "  closecaptioned, hdtv,       airdate,    originalairdate, "
                  "  partnumber,     parttotal,  category_type) "
                  "VALUES ("
                  "  :CHANID,        :STARTTIME, :ENDTIME,   :TITLE, "
                  "  :DESCRIPTION,   :SUBTITLE,  :CATEGORY,  :STEREO, "
                  "  :CC,            :HDTV,      :AIRDATE,   :ORIGAIRDATE, "
                  "  :PARTNUMBER,    :PARTTOTAL, :CATTYPE)");

    QString begTime = event.StartTime.toString(QString("yyyy-MM-ddThh:mm:00"));
    QString endTime = event.EndTime.toString(  QString("yyyy-MM-ddThh:mm:00"));
    QString airDate = event.OriginalAirDate.toString(QString("yyyy-MM-dd"));

    query.bindValue(":CHANID",      chanid);
    query.bindValue(":STARTTIME",   begTime);
    query.bindValue(":ENDTIME",     endTime);
    query.bindValue(":TITLE",       event.Event_Name.utf8());
    query.bindValue(":DESCRIPTION", event.Description.utf8());
    query.bindValue(":SUBTITLE",    event.Event_Subtitle.utf8());
    query.bindValue(":CATEGORY",    event.ContentDescription.utf8());
    query.bindValue(":STEREO",      event.Stereo);
    query.bindValue(":CC",          event.SubTitled);
    query.bindValue(":HDTV",        event.HDTV);
    query.bindValue(":AIRDATE",     event.Year);
    query.bindValue(":ORIGAIRDATE", airDate);
    query.bindValue(":PARTNUMBER",  event.PartNumber);
    query.bindValue(":PARTTOTAL",   event.PartTotal);
    query.bindValue(":CATTYPE",     event.CategoryType.utf8());

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Adding Event", query);
        return false;
    }

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
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", event.StartTime.
                        toString(QString("yyyy-MM-dd hh:mm:00")));
        query.bindValue(":ROLE", (*it).role.utf8());
        query.bindValue(":NAME", (*it).name.utf8());

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Adding Event (Credits)", query);
    }
    return true;
}
