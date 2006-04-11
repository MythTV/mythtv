// -*- Mode: c++ -*-

#include <algorithm>
using namespace std;

// MythTV includes
#include "eithelper.h"
#include "eitfixup.h"
#include "mythdbcon.h"
#include "atsctables.h"
#include "util.h"

const uint EITHelper::kChunkSize = 20;

static int get_chan_id_from_db(uint sourceid, uint atscsrcid);
static int get_chan_id_from_db(int tid_db, const Event&, bool ignore_source);
static uint update_eit_in_db(MSqlQuery&, MSqlQuery&, int, const Event&);
static uint delete_overlapping_in_db(MSqlQuery&, MSqlQuery&,
                                     int, const Event&);
static bool has_program(MSqlQuery&, int, const Event&);
static bool insert_program(MSqlQuery&, int, const Event&);

static const QString timeFmtDB  = "yyyy-MM-dd hh:mm:ss";
static const QString timeFmtDB2 = "yyyy-MM-dd hh:mm:ss";

#define LOC QString("EITHelper: ")
#define LOC_ERR QString("EITHelper, Error: ")

EITHelper::EITHelper() :
    eitfixup(new EITFixUp()), gps_offset(-13), sourceid(0)
{
}

EITHelper::~EITHelper()
{
    ClearList();

    QMutexLocker locker(&eitList_lock);
    for (uint i = 0; i < db_events.size(); i++)
        delete db_events.dequeue();

    delete eitfixup;
}

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
    return eitList.size() + db_events.size();
}

/** \fn EITHelper::ProcessEvents(uint)
 *  \brief Inserts events in EIT list.
 *
 *  \param mplexid multiplex we are inserting events for.
 *  \return Returns number of events inserted into DB.
 */
uint EITHelper::ProcessEvents(uint sourceid, bool ignore_source)
{
    QMutexLocker locker(&eitList_lock);

    uint insertCount = 0;

    if (eitList.size())
    {
        if (eitList.front()->size() <= kChunkSize)
        {
            QList_Events *events = eitList.front();
            eitList.pop_front();

            eitList_lock.unlock();
            insertCount += UpdateEITList(sourceid, *events, ignore_source);
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
            insertCount += UpdateEITList(sourceid, subset, ignore_source);
            QList_Events::iterator it = subset.begin();
            for (; it != subset.end(); ++it)
                delete *it;
            eitList_lock.lock();
        }
    }

    if (db_events.size())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        for (uint i = 0; (i < kChunkSize) && (i < db_events.size()); i++)
        {
            DBEvent *event = db_events.dequeue();
            eitList_lock.unlock();

            eitfixup->Fix(*event);

            insertCount += event->UpdateDB(query, 1000);

            delete event;
            eitList_lock.lock();
        }
    }

    if (!insertCount)
        return 0;

    if (incomplete_events.size() || unmatched_etts.size())
    {
        VERBOSE(VB_EIT, LOC +
                QString("Added %1 events -- complete(%2) "
                        "incomplete(%3) unmatched(%4)")
                .arg(insertCount).arg(db_events.size())
                .arg(incomplete_events.size()).arg(unmatched_etts.size()));
    }
    else
    {
        VERBOSE(VB_EIT, LOC + QString("Added %1 events").arg(insertCount));
    }

    return insertCount;
}

void EITHelper::SetFixup(uint atscsrcid, uint eitfixup)
{
    QMutexLocker locker(&eitList_lock);
    fixup[atscsrcid] = eitfixup;
}

void EITHelper::SetLanguagePreferences(const QStringList &langPref)
{
    QMutexLocker locker(&eitList_lock);

    uint priority = 1;
    QStringList::const_iterator it;
    for (it = langPref.begin(); it != langPref.end(); ++it)
    {
        if (!(*it).isEmpty())
        {
            uint language_key   = iso639_str3_to_key((*it).ascii());
            uint canonoical_key = iso639_key_to_canonical_key(language_key);
            languagePreferences[canonoical_key] = priority++;
        }
    }
}

void EITHelper::SetSourceID(uint _sourceid)
{
    QMutexLocker locker(&eitList_lock);
    sourceid = _sourceid;
}

void EITHelper::AddEIT(uint atscsrcid, const EventInformationTable *eit)
{
    EventIDToATSCEvent &events = incomplete_events[atscsrcid];
    EventIDToETT &etts = unmatched_etts[atscsrcid];

    for (uint i = 0; i < eit->EventCount(); i++)
    {
        ATSCEvent ev(eit->StartTimeRaw(i), eit->LengthInSeconds(i),
                     eit->ETMLocation(i),
                     eit->title(i).GetBestMatch(languagePreferences),
                     eit->Descriptors(i), eit->DescriptorsLength(i));

        EventIDToETT::iterator it = etts.find(eit->EventID(i));

        if (it != etts.end())
        {
            CompleteEvent(atscsrcid, ev, *it);
            etts.erase(it);
        }
        else if (!ev.etm)
        {
            CompleteEvent(atscsrcid, ev, QString::null);
        }
        else
        {
            unsigned char *tmp = new unsigned char[ev.desc_length];
            memcpy(tmp, eit->Descriptors(i), ev.desc_length);
            ev.desc = tmp;
            events[eit->EventID(i)] = ev;
        }
    }
}

void EITHelper::AddETT(uint atscsrcid, const ExtendedTextTable *ett)
{
    // Try to complete an Event
    ATSCSRCToEvents::iterator eits_it = incomplete_events.find(atscsrcid);
    if (eits_it != incomplete_events.end())
    {
        EventIDToATSCEvent::iterator it = (*eits_it).find(ett->EventID());
        if (it != (*eits_it).end())
        {
            CompleteEvent(atscsrcid, *it, ett->ExtendedTextMessage()
                          .GetBestMatch(languagePreferences));
            if ((*it).desc)
                delete [] (*it).desc;
            (*eits_it).erase(it);
            return;
        }
    }

    // Couldn't find matching EIT. If not yet in unmatched ETT map, insert it.
    EventIDToETT &elist = unmatched_etts[atscsrcid];
    if (elist.find(ett->EventID()) == elist.end())
    {
        elist[ett->EventID()] = ett->ExtendedTextMessage()
            .GetBestMatch(languagePreferences);
    }
}

//////////////////////////////////////////////////////////////////////
// private methods and functions below this line                    //
//////////////////////////////////////////////////////////////////////

void EITHelper::CompleteEvent(uint atscsrcid,
                              const ATSCEvent &event,
                              const QString   &ett)
{
    uint chanid = GetChanID(sourceid, atscsrcid);
    if (!chanid)
        return;

    QDateTime starttime;
    int t = secs_Between_1Jan1970_6Jan1980 + gps_offset + event.start_time;
    starttime.setTime_t(t, Qt::UTC);
    starttime = MythUTCToLocal(starttime);
    QDateTime endtime = starttime.addSecs(event.length);

    desc_list_t list = MPEGDescriptor::Parse(event.desc, event.desc_length);
    bool captioned = MPEGDescriptor::Find(list, DescriptorID::caption_service);
    bool stereo = false;

    QMutexLocker locker(&eitList_lock);
    db_events.enqueue(new DBEvent(chanid, QDeepCopy<QString>(event.title),
                                  QDeepCopy<QString>(ett),
                                  starttime, endtime,
                                  fixup[atscsrcid], captioned, stereo));
}

uint EITHelper::GetChanID(uint sourceid, uint atscsrcid)
{
    uint key = sourceid << 16 | atscsrcid;
    ServiceToChanID::const_iterator it = srv_to_chanid.find(key);
    if (it != srv_to_chanid.end())
        return max(*it, 0);

    int chanid = get_chan_id_from_db(sourceid, atscsrcid);
    if (chanid != 0)
        srv_to_chanid[key] = chanid;

    return max(chanid, 0);
}

int EITHelper::GetChanID(uint sourceid, const Event &event,
                         bool ignore_source) const
{
    unsigned long long srv;
    srv  = ((unsigned long long) sourceid);
    srv |= ((unsigned long long) event.ServiceID)   << 16;
    srv |= ((unsigned long long) event.NetworkID)   << 32;
    srv |= ((unsigned long long) event.TransportID) << 48;

    int chanid = srv_to_chanid[srv];
    if (chanid == 0)
    {
        srv_to_chanid[srv] = chanid = get_chan_id_from_db(
            sourceid, event, ignore_source);
    }
    return chanid;
}

uint EITHelper::UpdateEITList(uint sourceid, const QList_Events &events,
                              bool ignore_source) const
{
    MSqlQuery query1(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());

    int  chanid  = 0;
    uint counter = 0;

    QList_Events::const_iterator e = events.begin();
    for (; e != events.end(); ++e)
        if ((chanid = GetChanID(sourceid, **e, ignore_source)) > 0)
            counter += update_eit_in_db(query1, query2, chanid, **e);

    return counter;
}

static int get_chan_id_from_db(uint sourceid, uint atscsrcid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
            "SELECT chanid, useonairguide "
            "FROM channel "
            "WHERE atscsrcid = :ATSCSRCID AND "
            "      sourceid  = :SOURCEID");
    query.bindValue(":ATSCSRCID", atscsrcid);
    query.bindValue(":SOURCEID",  sourceid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Looking up chanid 1", query);
    else if (query.next())
    {
        bool useOnAirGuide = query.value(1).toBool();
        return (useOnAirGuide) ? query.value(0).toInt() : -1;
    }

    return -1;
}

// Figure out the chanid for this channel
static int get_chan_id_from_db(int sourceid, const Event &event,
                               bool ignore_source)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (event.ATSC)
    {
        // ATSC Link to chanid
        query.prepare(
            "SELECT chanid, useonairguide "
            "FROM channel "
            "WHERE atscsrcid = :ATSCSRCID AND "
            "      sourceid  = :SOURCEID");
        query.bindValue(":ATSCSRCID", event.ServiceID);
    }
    else
    {
        // DVB Link to chanid
        QString qstr =
            "SELECT chanid, useonairguide "
            "FROM channel, dtv_multiplex "
            "WHERE serviceid        = :SERVICEID   AND "
            "      networkid        = :NETWORKID   AND "
            "      transportid      = :TRANSPORTID AND "
            "      channel.mplexid  = dtv_multiplex.mplexid";

        if (!ignore_source)
            qstr += " AND channel.sourceid = :SOURCEID";

        query.prepare(qstr);
        query.bindValue(":SERVICEID",   event.ServiceID);
        query.bindValue(":NETWORKID",   event.NetworkID);
        query.bindValue(":TRANSPORTID", event.TransportID);
    }
    if (!ignore_source)
        query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Looking up chanID", query);
    else if (query.next())
    {
        // Check to see if we are interseted in this channel
        bool useOnAirGuide = query.value(1).toBool();
        return (useOnAirGuide) ? query.value(0).toInt() : -1;        
    }

    if (ignore_source)
    {
        VERBOSE(VB_EIT, "EITHelper: " +
                QString("chanid not found for service %1 on network %2,")
                .arg(event.ServiceID).arg(event.NetworkID) +
                "\n\t\t\tso event updates were skipped.");
        return -1;
    }

    VERBOSE(VB_EIT, "EITHelper: " +
            QString("chanid not found for service %1 on source %2,")
            .arg(event.ServiceID).arg(sourceid) +
            "\n\t\t\tso event updates were skipped.");

    return -1;
}

static uint update_eit_in_db(MSqlQuery &query, MSqlQuery &query2,
                             int chanid, const Event &event)
{
    if (has_program(query, chanid, event))
        return 0;

    delete_overlapping_in_db(query, query2, chanid, event);
    return insert_program(query, chanid, event) ? 1 : 0;
}

static uint delete_overlapping_in_db(
    MSqlQuery &query, MSqlQuery &delq, int chanid, const Event &event)
{
    uint counter = 0;
    query.prepare(
        "SELECT starttime, endtime, title, subtitle "
        "FROM program "
        "WHERE chanid = :CHANID AND manualid = 0 AND "
        "      ( starttime >= :STIME AND starttime < :ETIME )");

    query.bindValue(":CHANID", chanid);
    query.bindValue(":STIME",  event.StartTime.toString(timeFmtDB));
    query.bindValue(":ETIME",  event.EndTime.toString(timeFmtDB));

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Checking Rescheduled Event", query);
        return 0;
    }

    delq.prepare(
        "DELETE FROM program "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME AND "
        "      endtime   = :ENDTIME AND "
        "      title     = :TITLE");

    while (query.next())
    {
        counter++;
        // New guide data overriding existing
        // Possibly more than one conflict
        VERBOSE(VB_EIT, QString("Schedule Change on Channel %1")
                .arg(chanid));
        VERBOSE(VB_EIT, QString("Old: %1 %2 %3: %4")
                .arg(query.value(0).toString())
                .arg(query.value(1).toString())
                .arg(query.value(2).toString())
                .arg(query.value(3).toString()));
        VERBOSE(VB_EIT, QString("New: %1 %2 %3: %4")
                .arg(event.StartTime.toString(timeFmtDB2))
                .arg(event.EndTime.toString(timeFmtDB2))
                .arg(event.Event_Name.utf8())
                .arg(event.Event_Subtitle.utf8()));

        // Delete the old program row
        delq.bindValue(":CHANID",    chanid);
        delq.bindValue(":STARTTIME", query.value(0).toString());
        delq.bindValue(":ENDTIME",   query.value(1).toString());
        delq.bindValue(":TITLE",     query.value(2).toString());
        if (!delq.exec() || !delq.isActive())
            MythContext::DBError("Deleting Rescheduled Event", delq);
    }

    return counter;
}

static bool has_program(MSqlQuery &query, int chanid, const Event &event)
{
    query.prepare(
        "SELECT subtitle, description "
        "FROM program "
        "WHERE chanid    = :CHANID    AND "
        "      starttime = :STARTTIME AND "
        "      endtime   = :ENDTIME   AND "
        "      title     = :TITLE");
    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", event.StartTime.toString(timeFmtDB2));
    query.bindValue(":ENDTIME",   event.EndTime.toString(timeFmtDB2));
    query.bindValue(":TITLE",     event.Event_Name.utf8());

    if (!query.exec())
    {
        MythContext::DBError("Checking If Event Exists", query);
        return true; // return true on error
    }

    if (!query.size())
        return false; // if there is nothing in db, then we don't have program

    if (!query.next())
        return true; // return true on error

    QString dbDescription = query.value(1).toString();
    if (event.Description.utf8().length() > dbDescription.length())
    {
        VERBOSE(VB_EIT, "EITHelper: Update DB description " +
                QString("oldsize=%1 newsize=%2")
                .arg(dbDescription.length())
                .arg(event.Description.length()));
        return false; // description needs to be updated
    }

    QString eSubtitle = event.Event_Subtitle.utf8().lower();
    if (eSubtitle.isEmpty())
        return query.size(); // assume subtitle would be the same

    QString dbSubtitle = query.value(0).toString().lower();

    if (dbSubtitle != eSubtitle)
        VERBOSE(VB_EIT, "EITHelper: Subtitles are different");
    return dbSubtitle == eSubtitle; // return true on match...
}

static bool insert_program(MSqlQuery &query, int chanid, const Event &event)
{
    query.prepare(
        "REPLACE INTO program ("
        "  chanid,         starttime,  endtime,    title, "
        "  description,    subtitle,   category,   stereo, "
        "  closecaptioned, hdtv,       airdate,    originalairdate, "
        "  partnumber,     parttotal,  category_type) "
        "VALUES ("
        "  :CHANID,        :STARTTIME, :ENDTIME,   :TITLE, "
        "  :DESCRIPTION,   :SUBTITLE,  :CATEGORY,  :STEREO, "
        "  :CC,            :HDTV,      :AIRDATE,   :ORIGAIRDATE, "
        "  :PARTNUMBER,    :PARTTOTAL, :CATTYPE)");

    QString begTime = event.StartTime.toString(timeFmtDB2);
    QString endTime = event.EndTime.toString(timeFmtDB2);
    QString airDate = event.OriginalAirDate.toString("yyyy-MM-dd");

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
        query.bindValue(":STARTTIME", event.StartTime.toString(timeFmtDB));
        query.bindValue(":ROLE", (*it).role.utf8());
        query.bindValue(":NAME", (*it).name.utf8());

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Adding Event (Credits)", query);
    }
    return true;
}
