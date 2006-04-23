// -*- Mode: c++ -*-

// Std C++ headers
#include <algorithm>
using namespace std;

// MythTV includes
#include "eit.h"
#include "eithelper.h"
#include "eitfixup.h"
#include "eitcache.h"
#include "mythdbcon.h"
#include "atsctables.h"
#include "dvbtables.h"
#include "dishdescriptors.h"
#include "util.h"

const uint EITHelper::kChunkSize = 20;

static int get_chan_id_from_db(uint sourceid,  uint atscsrcid);
static int get_chan_id_from_db(uint sourceid,  uint serviceid,
                               uint networkid, uint transportid);
static void init_fixup(QMap<uint64_t,uint> &fix);
static int calc_eit_utc_offset(void);

#define LOC QString("EITHelper: ")
#define LOC_ERR QString("EITHelper, Error: ")

EITHelper::EITHelper() :
    eitfixup(new EITFixUp()), eitcache(new EITCache()),
    gps_offset(-1 * GPS_LEAP_SECONDS),          utc_offset(0),
    sourceid(0)
{
    init_fixup(fixup);

    utc_offset = calc_eit_utc_offset();

    int sign    = utc_offset < 0 ? -1 : +1;
    int diff    = abs(utc_offset);
    int hours   = diff / (60 * 60);
    int minutes = ((diff) / 60) % 60;
    int seconds = diff % 60;
    VERBOSE(VB_IMPORTANT, LOC + QString("localtime offset %1%2:%3%4:%5%6 ")
            .arg((sign < 0) ? "-" : "")
            .arg(hours).arg(minutes/10).arg(minutes%10)
            .arg(seconds/10).arg(seconds%10));
}

EITHelper::~EITHelper()
{
    QMutexLocker locker(&eitList_lock);
    for (uint i = 0; i < db_events.size(); i++)
        delete db_events.dequeue();

    delete eitfixup;
    delete eitcache;
}

uint EITHelper::GetListSize(void) const
{
    QMutexLocker locker(&eitList_lock);
    return db_events.size();
}

/** \fn EITHelper::ProcessEvents(void)
 *  \brief Inserts events in EIT list.
 *
 *  \param mplexid multiplex we are inserting events for.
 *  \return Returns number of events inserted into DB.
 */
uint EITHelper::ProcessEvents(void)
{
    QMutexLocker locker(&eitList_lock);
    uint insertCount = 0;

    if (!db_events.size())
        return 0;

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

void EITHelper::AddEIT(const DVBEventInformationTable *eit)
{
    uint descCompression = (eit->TableID() > 0x80) ? 2 : 1;
    uint fix = fixup[eit->OriginalNetworkID() << 16];
    fix |= fixup[(((uint64_t)eit->TSID()) << 32) |
                 (eit->OriginalNetworkID() << 16)];
    fix |= fixup[(eit->OriginalNetworkID() << 16) | eit->ServiceID()];
    fix |= EITFixUp::kFixGenericDVB;

    for (uint i = 0; i < eit->EventCount(); i++)
    {
        // Skip event if we have already processed it before...
        if (!eitcache->IsNewEIT(
                eit->TSID(),      eit->EventID(i),
                eit->ServiceID(), eit->TableID(),
                eit->Version(),
                eit->Descriptors(i), eit->DescriptorsLength(i)))
        {
            continue;
        }

        QString title         = QString::null;
        QString subtitle      = QString::null;
        QString description   = QString::null;
        QString category      = QString::null;
        MythCategoryType category_type = kCategoryNone;
        bool hdtv = false, stereo = false, subtitled = false;

        // Parse descriptors
        desc_list_t list = MPEGDescriptor::Parse(
            eit->Descriptors(i), eit->DescriptorsLength(i));

        const unsigned char *dish_event_name =
            MPEGDescriptor::Find(list, DescriptorID::dish_event_name);

        if (dish_event_name)
        {
            DishEventNameDescriptor dend(dish_event_name);
            if (dend.HasName())
                title = dend.Name(descCompression);

            const unsigned char *dish_event_description =
                MPEGDescriptor::Find(list,
                                     DescriptorID::dish_event_description);
            if (dish_event_description)
            {
                DishEventDescriptionDescriptor dedd(dish_event_description);
                if (dedd.HasDescription())
                    description = dedd.Description(descCompression);
            }
        }
        else
        {
            const unsigned char *bestShortEvent =
                MPEGDescriptor::FindBestMatch(
                    list, DescriptorID::short_event, languagePreferences);

            unsigned char enc_ch[1] = { 0x05 };
            const unsigned char *enc =
                (fix & EITFixUp::kEFixPro7Sat) ? enc_ch : NULL;

            if (bestShortEvent)
            {
                ShortEventDescriptor sed(bestShortEvent);
                if (enc)
                {
                    title    = sed.EventName(enc, 1);
                    subtitle = sed.Text(enc, 1);
                }
                else
                {
                    title    = sed.EventName();
                    subtitle = sed.Text();
                }
            }

            vector<const unsigned char*> bestExtendedEvents =
                MPEGDescriptor::FindBestMatches(
                    list, DescriptorID::extended_event, languagePreferences);

            description = "";
            for (uint j = 0; j < bestExtendedEvents.size(); j++)
            {
                if (!bestExtendedEvents[j])
                {
                    description = "";
                    break;
                }

                ExtendedEventDescriptor eed(bestExtendedEvents[j]);
                if (enc)
                    description += eed.Text(enc, 1);
                else
                    description += eed.Text();
            }
        }

        desc_list_t components =
            MPEGDescriptor::FindAll(list, DescriptorID::component);
        for (uint j = 0; j < components.size(); j++)
        {
            ComponentDescriptor component(components[j]);
            hdtv      |= component.IsHDTV();
            stereo    |= component.IsStereo();
            subtitled |= component.IsReallySubtitled();
        }

        const unsigned char *content_data =
            MPEGDescriptor::Find(list, DescriptorID::content);
        if (content_data)
        {
            ContentDescriptor content(content_data);
            category      = content.GetDescription(0);
            category_type = content.GetMythCategory(0);
        }

        uint chanid = GetChanID(
            eit->ServiceID(), eit->OriginalNetworkID(), eit->TSID());

        if (!chanid)
            continue;

        QDateTime starttime = MythUTCToLocal(eit->StartTimeUTC(i));
        EITFixUp::TimeFix(starttime);
        QDateTime endtime   = starttime.addSecs(eit->DurationInSeconds(i));

        DBEvent *event = new DBEvent(chanid,
                                     title,     subtitle,      description,
                                     category,  category_type,
                                     starttime, endtime,       fix,
                                     false,     subtitled,
                                     stereo,    hdtv);
        db_events.enqueue(event);
    }
}

//////////////////////////////////////////////////////////////////////
// private methods and functions below this line                    //
//////////////////////////////////////////////////////////////////////

void EITHelper::CompleteEvent(uint atscsrcid,
                              const ATSCEvent &event,
                              const QString   &ett)
{
    uint chanid = GetChanID(atscsrcid);
    if (!chanid)
        return;

    QDateTime starttime;
    int off = secs_Between_1Jan1970_6Jan1980 + gps_offset + utc_offset;
    starttime.setTime_t(off + event.start_time, Qt::LocalTime);
    EITFixUp::TimeFix(starttime);
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

uint EITHelper::GetChanID(uint atscsrcid)
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

uint EITHelper::GetChanID(uint serviceid, uint networkid, uint tsid)
{
    uint64_t key;
    key  = ((uint64_t) sourceid);
    key |= ((uint64_t) serviceid) << 16;
    key |= ((uint64_t) networkid) << 32;
    key |= ((uint64_t) tsid)      << 48;

    ServiceToChanID::const_iterator it = srv_to_chanid.find(key);
    if (it != srv_to_chanid.end())
        return max(*it, 0);

    int chanid = get_chan_id_from_db(sourceid, serviceid, networkid, tsid);
    if (chanid != 0)
        srv_to_chanid[key] = chanid;

    return max(chanid, 0);
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
static int get_chan_id_from_db(uint sourceid, uint serviceid,
                               uint networkid, uint transportid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // DVB Link to chanid
    QString qstr =
        "SELECT chanid, useonairguide "
        "FROM channel, dtv_multiplex "
        "WHERE serviceid        = :SERVICEID   AND "
        "      networkid        = :NETWORKID   AND "
        "      transportid      = :TRANSPORTID AND "
        "      channel.mplexid  = dtv_multiplex.mplexid";

    if (sourceid)
        qstr += " AND channel.sourceid = :SOURCEID";

    query.prepare(qstr);
    query.bindValue(":SERVICEID",   serviceid);
    query.bindValue(":NETWORKID",   networkid);
    query.bindValue(":TRANSPORTID", transportid);

    if (sourceid)
        query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Looking up chanID", query);
    else if (query.next())
    {
        // Check to see if we are interseted in this channel
        bool useOnAirGuide = query.value(1).toBool();
        return (useOnAirGuide) ? query.value(0).toInt() : -1;        
    }

    return -1;
}

static void init_fixup(QMap<uint64_t,uint> &fix)
{
    // transport_id<<32 | netword_id<<16 | service_id
    fix[  256 << 16] = EITFixUp::kFixBell;
    fix[  257 << 16] = EITFixUp::kFixBell;
    fix[ 4100 << 16] = EITFixUp::kFixBell;
    fix[ 4101 << 16] = EITFixUp::kFixBell;
    fix[ 4102 << 16] = EITFixUp::kFixBell;
    fix[ 4103 << 16] = EITFixUp::kFixBell;
    fix[ 4104 << 16] = EITFixUp::kFixBell;
    fix[ 4105 << 16] = EITFixUp::kFixBell;
    fix[ 4106 << 16] = EITFixUp::kFixBell;
    fix[ 4107 << 16] = EITFixUp::kFixBell;
    fix[ 4097 << 16] = EITFixUp::kFixBell;
    fix[ 4098 << 16] = EITFixUp::kFixBell;

    fix[ 9018 << 16] = EITFixUp::kFixUK;

    fix[40999 << 16] = EITFixUp::kFixComHem;
    fix[40999 << 16 | 1070] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1308] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1041] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1306] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1307] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1030] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1016] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1131] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1068] = EITFixUp::kFixSubtitle;
    fix[40999 << 16 | 1069] = EITFixUp::kFixSubtitle;

    fix[ 4096 << 16] = EITFixUp::kFixAUStar;
    fix[ 4096 << 16] = EITFixUp::kFixAUStar;

    fix[769LL << 32 | 8468 << 16] = EITFixUp::kEFixPro7Sat;
}

static int calc_eit_utc_offset(void)
{
    QString config_offset = gContext->GetSetting("EITTimeOffset", "Auto");
    if (config_offset == "Auto")
    {
        QDateTime loc = QDateTime::currentDateTime(Qt::LocalTime);
        QDateTime utc = QDateTime::currentDateTime(Qt::UTC);

        int utc_offset = MythSecsTo(utc, loc);

        // clamp to nearest minute if within 10 seconds
        int off = utc_offset % 60;
        if (abs(off) < 10)
            utc_offset -= off;
        if (off < -50 && off > -60)
            utc_offset -= 60 + off;
        if (off > +50 && off < +60)
            utc_offset += 60 - off;

        return utc_offset;
    }
    if (config_offset == "None")
        return 0;

    int sign    = config_offset.left(1) == "-" ? -1 : +1;
    int hours   = config_offset.mid(1,2).toInt();
    int minutes = config_offset.right(2).toInt();
    return sign * (hours * 60 * 60) + (minutes * 60);
}
