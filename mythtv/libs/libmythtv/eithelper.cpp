// -*- Mode: c++ -*-

// Std C headers
#include <time.h>

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

static uint get_chan_id_from_db(uint sourceid,
                                uint atscmajor, uint atscminor);
static uint get_chan_id_from_db(uint sourceid,  uint serviceid,
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

void EITHelper::SetFixup(uint atsc_major, uint atsc_minor, uint eitfixup)
{
    QMutexLocker locker(&eitList_lock);
    uint atsc_key = (atsc_major << 16) | atsc_minor;
    fixup[atsc_key] = eitfixup;
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

void EITHelper::AddEIT(uint atsc_major, uint atsc_minor,
                       const EventInformationTable *eit)
{
    uint atsc_key = (atsc_major << 16) | atsc_minor;
    EventIDToATSCEvent &events = incomplete_events[atsc_key];
    EventIDToETT &etts = unmatched_etts[atsc_key];

    for (uint i = 0; i < eit->EventCount(); i++)
    {
        ATSCEvent ev(eit->StartTimeRaw(i), eit->LengthInSeconds(i),
                     eit->ETMLocation(i),
                     eit->title(i).GetBestMatch(languagePreferences),
                     eit->Descriptors(i), eit->DescriptorsLength(i));

        EventIDToETT::iterator it = etts.find(eit->EventID(i));

        if (it != etts.end())
        {
            CompleteEvent(atsc_major, atsc_minor, ev, *it);
            etts.erase(it);
        }
        else if (!ev.etm)
        {
            CompleteEvent(atsc_major, atsc_minor, ev, QString::null);
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

void EITHelper::AddETT(uint atsc_major, uint atsc_minor,
                       const ExtendedTextTable *ett)
{
    uint atsc_key = (atsc_major << 16) | atsc_minor;
    // Try to complete an Event
    ATSCSRCToEvents::iterator eits_it = incomplete_events.find(atsc_key);
    if (eits_it != incomplete_events.end())
    {
        EventIDToATSCEvent::iterator it = (*eits_it).find(ett->EventID());
        if (it != (*eits_it).end())
        {
            CompleteEvent(
                atsc_major, atsc_minor, *it,
                ett->ExtendedTextMessage().GetBestMatch(languagePreferences));

            if ((*it).desc)
                delete [] (*it).desc;

            (*eits_it).erase(it);

            return;
        }
    }

    // Couldn't find matching EIT. If not yet in unmatched ETT map, insert it.
    EventIDToETT &elist = unmatched_etts[atsc_key];
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
    fix |= fixup[(((uint64_t)eit->TSID()) << 32) |
                 (uint64_t)(eit->OriginalNetworkID() << 16) |
		 (uint64_t)eit->ServiceID()];
    fix |= EITFixUp::kFixGenericDVB;

    uint chanid = GetChanID(eit->ServiceID(), eit->OriginalNetworkID(),
                            eit->TSID());
    if (!chanid)
        return;

    uint tableid   = eit->TableID();
    uint version   = eit->Version();
    for (uint i = 0; i < eit->EventCount(); i++)
    {
        // Skip event if we have already processed it before...
        if (!eitcache->IsNewEIT(chanid, tableid, version, eit->EventID(i),
                              eit->EndTimeUnixUTC(i)))
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

            unsigned char enc_1[3]  = { 0x10, 0x00, 0x01 };
            unsigned char enc_15[3] = { 0x10, 0x00, 0x0f };
            int enc_len = 0;
            const unsigned char *enc = NULL;

            // Is this BellExpressVU EIT (Canada) ?
            // Use an encoding override of ISO 8859-1 (Latin1)
            if (fix & EITFixUp::kEFixForceISO8859_1)
            {
                enc = enc_1;
                enc_len = sizeof(enc_1);
            }

            // Is this broken DVB provider in Western Europe?
            // Use an encoding override of ISO 8859-15 (Latin6)
            if (fix & EITFixUp::kEFixForceISO8859_15)
            {
                enc = enc_15;
                enc_len = sizeof(enc_15);
            }

            if (bestShortEvent)
            {
                ShortEventDescriptor sed(bestShortEvent);
                if (enc)
                {
                    title    = sed.EventName(enc, enc_len);
                    subtitle = sed.Text(enc, enc_len);
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
                    description += eed.Text(enc, enc_len);
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

        desc_list_t contentIds =
            MPEGDescriptor::FindAll(list, DescriptorID::dvb_content_identifier);
        QString programId = "", seriesId = "";
        for (uint j = 0; j < contentIds.size(); j++)
        {
            DVBContentIdentifierDescriptor desc(contentIds[j]);
            if (desc.ContentEncoding() == 0)
            {
                // The CRID is a URI.  It could contain UTF8 sequences encoded
                // as %XX but there's no advantage in decoding them.
                // The BBC currently uses private types 0x31 and 0x32.
                if (desc.ContentType() == 0x01 || desc.ContentType() == 0x31)
                    programId = desc.ContentId();
                else if (desc.ContentType() == 0x02 || desc.ContentType() == 0x32)
                    seriesId = desc.ContentId();
            }
        }

        QDateTime starttime = MythUTCToLocal(eit->StartTimeUTC(i));
        EITFixUp::TimeFix(starttime);
        QDateTime endtime   = starttime.addSecs(eit->DurationInSeconds(i));

        DBEvent *event = new DBEvent(chanid,
                                     title,     subtitle,      description,
                                     category,  category_type,
                                     starttime, endtime,       fix,
                                     false,     subtitled,
                                     stereo,    hdtv,
                                     seriesId,  programId);
        db_events.enqueue(event);
    }
}

void EITHelper::PruneEITCache(uint timestamp)
{
    eitcache->PruneOldEntries(timestamp);
}

void EITHelper::WriteEITCache(void)
{
    eitcache->WriteToDB();
}

//////////////////////////////////////////////////////////////////////
// private methods and functions below this line                    //
//////////////////////////////////////////////////////////////////////

void EITHelper::CompleteEvent(uint atsc_major, uint atsc_minor,
                              const ATSCEvent &event,
                              const QString   &ett)
{
    uint chanid = GetChanID(atsc_major, atsc_minor);
    if (!chanid)
        return;

    QDateTime starttime;
    time_t off = secs_Between_1Jan1970_6Jan1980 + gps_offset + utc_offset;
    time_t tmp = event.start_time + off;
    tm result;

    if (gmtime_r(&tmp, &result))
    {
        starttime.setDate(QDate(result.tm_year + 1900,
                                result.tm_mon + 1,
                                result.tm_mday));
        starttime.setTime(QTime(result.tm_hour, result.tm_min, result.tm_sec));
    }
    else
    {
        starttime.setTime_t(tmp - utc_offset, Qt::LocalTime);
    }

    EITFixUp::TimeFix(starttime);
    QDateTime endtime = starttime.addSecs(event.length);

    desc_list_t list = MPEGDescriptor::Parse(event.desc, event.desc_length);
    bool captioned = MPEGDescriptor::Find(list, DescriptorID::caption_service);
    bool stereo = false;

    uint atsc_key = (atsc_major << 16) | atsc_minor;

    QMutexLocker locker(&eitList_lock);
    db_events.enqueue(new DBEvent(chanid, QDeepCopy<QString>(event.title),
                                  QDeepCopy<QString>(ett),
                                  starttime, endtime,
                                  fixup[atsc_key], captioned, stereo));
}

uint EITHelper::GetChanID(uint atsc_major, uint atsc_minor)
{
    uint64_t key;
    key  = ((uint64_t) sourceid);
    key |= ((uint64_t) atsc_minor) << 16;
    key |= ((uint64_t) atsc_major) << 32;

    ServiceToChanID::const_iterator it = srv_to_chanid.find(key);
    if (it != srv_to_chanid.end())
        return max(*it, 0);

    uint chanid = get_chan_id_from_db(sourceid, atsc_major, atsc_minor);
    if (chanid)
        srv_to_chanid[key] = chanid;

    return chanid;
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

    uint chanid = get_chan_id_from_db(sourceid, serviceid, networkid, tsid);
    if (chanid)
        srv_to_chanid[key] = chanid;

    return chanid;
}

static uint get_chan_id_from_db(uint sourceid,
                                uint atsc_major, uint atsc_minor)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
            "SELECT chanid, useonairguide "
            "FROM channel "
            "WHERE atsc_major_chan = :MAJORCHAN AND "
            "      atsc_minor_chan = :MINORCHAN AND "
            "      sourceid        = :SOURCEID");
    query.bindValue(":MAJORCHAN", atsc_major);
    query.bindValue(":MINORCHAN", atsc_minor);
    query.bindValue(":SOURCEID",  sourceid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Looking up chanid 1", query);
    else if (query.next())
    {
        bool useOnAirGuide = query.value(1).toBool();
        return (useOnAirGuide) ? query.value(0).toUInt() : 0;
    }

    return 0;
}

// Figure out the chanid for this channel
static uint get_chan_id_from_db(uint sourceid, uint serviceid,
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
        return (useOnAirGuide) ? query.value(0).toUInt() : 0;
    }

    return 0;
}

static void init_fixup(QMap<uint64_t,uint> &fix)
{
    ///////////////////////////////////////////////////////////////////////////
    // Fixups to make EIT provided listings more useful
    // transport_id<<32 | netword_id<<16 | service_id

    // Bell Express VU Canada
    fix[  256U << 16] = EITFixUp::kFixBell;
    fix[  257U << 16] = EITFixUp::kFixBell;
    fix[ 4100U << 16] = EITFixUp::kFixBell;
    fix[ 4101U << 16] = EITFixUp::kFixBell;
    fix[ 4102U << 16] = EITFixUp::kFixBell;
    fix[ 4103U << 16] = EITFixUp::kFixBell;
    fix[ 4104U << 16] = EITFixUp::kFixBell;
    fix[ 4105U << 16] = EITFixUp::kFixBell;
    fix[ 4106U << 16] = EITFixUp::kFixBell;
    fix[ 4107U << 16] = EITFixUp::kFixBell;
    fix[ 4097U << 16] = EITFixUp::kFixBell;
    fix[ 4098U << 16] = EITFixUp::kFixBell;

    // United Kingdom
    fix[ 9018U << 16] = EITFixUp::kFixUK;

    // ComHem Sweeden
    fix[40999U << 16       ] = EITFixUp::kFixComHem;
    fix[40999U << 16 | 1070] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1308] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1041] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1306] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1307] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1030] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1016] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1131] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1068] = EITFixUp::kFixSubtitle;
    fix[40999U << 16 | 1069] = EITFixUp::kFixSubtitle;

    // Australia
    fix[ 4096U << 16] = EITFixUp::kFixAUStar;
    fix[ 4096U << 16] = EITFixUp::kFixAUStar;

    // MultiChoice Africa
    fix[ 6144U << 16] = EITFixUp::kFixMCA;

    // RTL Subtitle parsing
    fix[  1089LL << 32 |     1  << 16] = EITFixUp::kFixRTL;// DVB-S
    fix[   773LL << 32 |  8468U << 16] = EITFixUp::kFixRTL;// DVB-T Berlin

    // Premiere EIT processing
    fix[   1LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   2LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   3LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   4LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   5LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   6LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[  17LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    // Mark Premiere HD and Discovery HD as HDTV
    fix[   6LL << 32 |  133 << 16 | 129] = EITFixUp::kFixHDTV;
    fix[   6LL << 32 |  133 << 16 | 130] = EITFixUp::kFixHDTV;

    ///////////////////////////////////////////////////////////////////////////
    // Special Early fixups for providers that break DVB EIT spec.
    // transport_id<<32 | netword_id<<16 | service_id

    // Bell Express VU Canada
    fix[ 256U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[ 257U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4100U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4101U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4102U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4103U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4104U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4105U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4106U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4107U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4097U << 16] |= EITFixUp::kEFixForceISO8859_1;
    fix[4098U << 16] |= EITFixUp::kEFixForceISO8859_1;

    // DVB-T Berlin Germany
    fix[   769LL << 32 |  8468U << 16] = EITFixUp::kEFixForceISO8859_15;
    // DVB-T Hamburg Germany
    fix[  3074LL << 32 |  8468U << 16] = EITFixUp::kEFixForceISO8859_15;
    // DVB-T Bremen Germany
    fix[  3075LL << 32 |  8468U << 16] = EITFixUp::kEFixForceISO8859_15;
    // DVB-T Hessen Germany
    fix[  8705LL << 32 |  8468U << 16] = EITFixUp::kEFixForceISO8859_15;
    // DVB-T Munich Germany
    fix[ 13057LL << 32 |  8468U << 16] = EITFixUp::kEFixForceISO8859_15;
    // DVB-T Berlin dsf Germany
    fix[   774LL << 32 |  8468U << 16 | 16392] =
        EITFixUp::kEFixForceISO8859_15;

    // DVB-C Kabel Deutschland encoding fixes Germany
    fix[   112LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10000LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10001LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10002LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10003LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10004LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10005LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10006LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10008LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10009LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    // On the multiplex with RTL only following channels need fixing:
    //   terranova, Eurosport, DasVierte, and Viva, resp.
    fix[    10007LL<<32| 61441U << 16 | 53605] =
        fix[10007LL<<32| 61441U << 16 | 53607] =
        fix[10007LL<<32| 61441U << 16 | 53608] =
        fix[10007LL<<32| 61441U << 16 | 53609] =
        EITFixUp::kEFixForceISO8859_15;

    // DVB-S Pro7 Swiss
    fix[  1082LL << 32 | 1 << 16 | 20001] = EITFixUp::kEFixForceISO8859_15;
    // DVB-S Pro7 Austria
    fix[  1082LL << 32 | 1 << 16 | 20002] = EITFixUp::kEFixForceISO8859_15;
    // DVB-S Kabel1 Swiss
    fix[  1082LL << 32 | 1 << 16 | 20003] = EITFixUp::kEFixForceISO8859_15;
    // DVB-S Kabel1 Austria
    fix[  1082LL << 32 | 1 << 16 | 20004] = EITFixUp::kEFixForceISO8859_15;
    // DVB-S Sat.1 Austria
    fix[  1082LL << 32 | 1 << 16 | 20005] = EITFixUp::kEFixForceISO8859_15;
    // DVB-S Astra 19.2E DMAX Germany
    fix[  1113LL << 32 | 1 << 16 | 12602] = EITFixUp::kEFixForceISO8859_15;

    // Premiere and Pro7/Sat.1
    fix[133 << 16] = EITFixUp::kEFixForceISO8859_15;
}

static int calc_eit_utc_offset(void)
{
    QString config_offset = gContext->GetSetting("EITTimeOffset", "Auto");

    if (config_offset == "Auto")
        return calc_utc_offset();

    if (config_offset == "None")
        return 0;

    int sign    = config_offset.left(1) == "-" ? -1 : +1;
    int hours   = config_offset.mid(1,2).toInt();
    int minutes = config_offset.right(2).toInt();
    return sign * (hours * 60 * 60) + (minutes * 60);
}
