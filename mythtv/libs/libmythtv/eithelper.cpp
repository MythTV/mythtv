// -*- Mode: c++ -*-

// Std C headers
#include <time.h>

// Std C++ headers
#include <algorithm>
using namespace std;

// MythTV includes
#include "eithelper.h"
#include "eitfixup.h"
#include "eitcache.h"
#include "mythdb.h"
#include "atsctables.h"
#include "dvbtables.h"
#include "premieretables.h"
#include "dishdescriptors.h"
#include "premieredescriptors.h"
#include "mythdate.h"
#include "programdata.h"
#include "programinfo.h" // for subtitle types and audio and video properties
#include "compat.h" // for gmtime_r on windows.

const uint EITHelper::kChunkSize = 20;
EITCache *EITHelper::eitcache = new EITCache();

static uint get_chan_id_from_db_atsc(uint sourceid,
                                     uint atscmajor, uint atscminor);
static uint get_chan_id_from_db_dvb(uint sourceid,  uint serviceid,
                                    uint networkid, uint transportid);
static uint get_chan_id_from_db_dtv(uint sourceid,
                                    uint programnumber, uint tunedchanid);
static void init_fixup(QMap<uint64_t,uint> &fix);

#define LOC QString("EITHelper: ")

EITHelper::EITHelper() :
    eitfixup(new EITFixUp()),
    gps_offset(-1 * GPS_LEAP_SECONDS),
    sourceid(0), channelid(0)
{
    init_fixup(fixup);
}

EITHelper::~EITHelper()
{
    QMutexLocker locker(&eitList_lock);
    while (db_events.size())
        delete db_events.dequeue();

    delete eitfixup;
}

uint EITHelper::GetListSize(void) const
{
    QMutexLocker locker(&eitList_lock);
    return db_events.size();
}

/** \fn EITHelper::ProcessEvents(void)
 *  \brief Inserts events in EIT list.
 *
 *  \return Returns number of events inserted into DB.
 */
uint EITHelper::ProcessEvents(void)
{
    QMutexLocker locker(&eitList_lock);
    uint insertCount = 0;

    if (!db_events.size())
        return 0;

    MSqlQuery query(MSqlQuery::InitCon());
    for (uint i = 0; (i < kChunkSize) && (db_events.size() > 0); i++)
    {
        DBEventEIT *event = db_events.dequeue();
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
        LOG(VB_EIT, LOG_INFO,
            LOC + QString("Added %1 events -- complete(%2) "
                          "incomplete(%3) unmatched(%4)")
                .arg(insertCount).arg(db_events.size())
                .arg(incomplete_events.size()).arg(unmatched_etts.size()));
    }
    else
    {
        LOG(VB_EIT, LOG_INFO,
            LOC + QString("Added %1 events").arg(insertCount));
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
            uint language_key   = iso639_str3_to_key(*it);
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

void EITHelper::SetChannelID(uint _channelid)
{
    QMutexLocker locker(&eitList_lock);
    channelid = _channelid;
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

static void parse_dvb_event_descriptors(desc_list_t list, uint fix,
                                        QMap<uint,uint> languagePreferences,
                                        QString &title, QString &subtitle,
                                        QString &description)
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

static inline void parse_dvb_component_descriptors(desc_list_t list,
                                                   unsigned char &subtitle_type,
                                                   unsigned char &audio_properties,
                                                   unsigned char &video_properties)
{
    desc_list_t components =
        MPEGDescriptor::FindAll(list, DescriptorID::component);
    for (uint j = 0; j < components.size(); j++)
    {
        ComponentDescriptor component(components[j]);
        video_properties |= component.VideoProperties();
        audio_properties |= component.AudioProperties();
        subtitle_type    |= component.SubtitleType();
    }
}

void EITHelper::AddEIT(const DVBEventInformationTable *eit)
{
    uint chanid = 0;
    if ((eit->TableID() == TableID::PF_EIT) ||
        ((eit->TableID() >= TableID::SC_EITbeg) && (eit->TableID() <= TableID::SC_EITend)))
    {
        // EITa(ctive)
        chanid = GetChanID(eit->ServiceID());
    }
    else
    {
        // EITo(ther)
        chanid = GetChanID(eit->ServiceID(), eit->OriginalNetworkID(), eit->TSID());
    }
    if (!chanid)
        return;

    uint descCompression = (eit->TableID() > 0x80) ? 2 : 1;
    uint fix = fixup.value(eit->OriginalNetworkID() << 16);
    fix |= fixup.value((((uint64_t)eit->TSID()) << 32) |
                 (eit->OriginalNetworkID() << 16));
    fix |= fixup.value((eit->OriginalNetworkID() << 16) | eit->ServiceID());
    fix |= fixup.value((((uint64_t)eit->TSID()) << 32) |
                 (uint64_t)(eit->OriginalNetworkID() << 16) |
                  (uint64_t)eit->ServiceID());
    fix |= EITFixUp::kFixGenericDVB;

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

        QString title         = QString("");
        QString subtitle      = QString("");
        QString description   = QString("");
        QString category      = QString("");
        uint category_type = kCategoryNone;
        unsigned char subtitle_type=0, audio_props=0, video_props=0;

        // Parse descriptors
        desc_list_t list = MPEGDescriptor::Parse(
            eit->Descriptors(i), eit->DescriptorsLength(i));

        const unsigned char *dish_event_name = NULL;
        if (EITFixUp::kFixDish & fix)
        {
            dish_event_name = MPEGDescriptor::Find(
                    list, PrivateDescriptorID::dish_event_name);
        }

        if (dish_event_name)
        {
            DishEventNameDescriptor dend(dish_event_name);
            if (dend.HasName())
                title = dend.Name(descCompression);

            const unsigned char *dish_event_description =
                MPEGDescriptor::Find(
                    list, PrivateDescriptorID::dish_event_description);
            if (dish_event_description)
            {
                DishEventDescriptionDescriptor dedd(dish_event_description);
                if (dedd.HasDescription())
                    description = dedd.Description(descCompression);
            }
        }
        else
        {
            parse_dvb_event_descriptors(list, fix, languagePreferences,
                                        title, subtitle, description);
        }

        parse_dvb_component_descriptors(list, subtitle_type, audio_props,
                                        video_props);

        QString programId = QString("");
        QString seriesId  = QString("");
        QString rating    = QString("");
        QString rating_system = QString("");
        QString advisory = QString("");
        float stars = 0.0;
        QDate originalairdate;

        if (EITFixUp::kFixDish & fix)
        {
            const unsigned char *mpaa_data = MPEGDescriptor::Find(
                list, PrivateDescriptorID::dish_event_mpaa);
            if (mpaa_data)
            {
                DishEventMPAADescriptor mpaa(mpaa_data);
                stars = mpaa.stars();

                if (stars) // Only movies for now
                {
                    rating = mpaa.rating();
                    rating_system = "MPAA";
                    advisory = mpaa.advisory();
                }
            }

            if (!stars) // Not MPAA rated, check VCHIP
            {
                const unsigned char *vchip_data = MPEGDescriptor::Find(
                    list, PrivateDescriptorID::dish_event_vchip);
                if (vchip_data)
                {
                    DishEventVCHIPDescriptor vchip(vchip_data);
                    rating = vchip.rating();
                    rating_system = "VCHIP";
                    advisory = vchip.advisory();
                }
            }

            if (!advisory.isEmpty() && !rating.isEmpty())
                rating += ", " + advisory;
            else if (!advisory.isEmpty())
            {
                rating = advisory;
                rating_system = "advisory";
            }

            const unsigned char *tags_data = MPEGDescriptor::Find(
                list, PrivateDescriptorID::dish_event_tags);
            if (tags_data)
            {
                DishEventTagsDescriptor tags(tags_data);
                seriesId  = tags.seriesid();
                programId = tags.programid();
                originalairdate = tags.originalairdate(); // future use

                if (programId.startsWith("MV") || programId.startsWith("SP"))
                    seriesId = "";
            }

            const unsigned char *properties_data = MPEGDescriptor::Find(
                list, PrivateDescriptorID::dish_event_properties);
            if (properties_data)
            {
                DishEventPropertiesDescriptor properties(properties_data);
                subtitle_type |= properties.SubtitleProperties(descCompression);
                audio_props   |= properties.AudioProperties(descCompression);
            }
        }

        const unsigned char *content_data =
            MPEGDescriptor::Find(list, DescriptorID::content);
        if (content_data)
        {
            if ((EITFixUp::kFixDish & fix) || (EITFixUp::kFixBell & fix))
            {
                DishContentDescriptor content(content_data);
                category_type = content.GetTheme();
                if (EITFixUp::kFixDish & fix)
                    category  = content.GetCategory();
            }
            else
            {
                ContentDescriptor content(content_data);
                category      = content.GetDescription(0);
                category_type = content.GetMythCategory(0);
            }
        }

        desc_list_t contentIds =
            MPEGDescriptor::FindAll(list, DescriptorID::dvb_content_identifier);
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

        QDateTime starttime = eit->StartTimeUTC(i);
        // fix starttime only if the duration is a multiple of a minute
        if (!(eit->DurationInSeconds(i) % 60))
            EITFixUp::TimeFix(starttime);
        QDateTime endtime   = starttime.addSecs(eit->DurationInSeconds(i));

        DBEventEIT *event = new DBEventEIT(
            chanid,
            title,     subtitle,      description,
            category,  category_type,
            starttime, endtime,       fix,
            subtitle_type,
            audio_props,
            video_props, stars,
            seriesId,  programId);

        db_events.enqueue(event);
    }
}

// This function gets special EIT data from the German provider Premiere
// for the option channels Premiere Sport and Premiere Direkt
void EITHelper::AddEIT(const PremiereContentInformationTable *cit)
{
    // set fixup for Premiere
    uint fix = fixup.value(133 << 16);
    fix |= EITFixUp::kFixGenericDVB;

    QString title         = QString("");
    QString subtitle      = QString("");
    QString description   = QString("");
    QString category      = QString("");
    MythCategoryType category_type = kCategoryNone;
    unsigned char subtitle_type=0, audio_props=0, video_props=0;

    // Parse descriptors
    desc_list_t list = MPEGDescriptor::Parse(
        cit->Descriptors(), cit->DescriptorsLength());

    parse_dvb_event_descriptors(list, fix, languagePreferences,
                                title, subtitle, description);

    parse_dvb_component_descriptors(list, subtitle_type, audio_props,
                                    video_props);

    const unsigned char *content_data =
        MPEGDescriptor::Find(list, DescriptorID::content);
    if (content_data)
    {
        ContentDescriptor content(content_data);
        // fix events without real content data
        if (content.Nibble(0)==0x00)
        {
            if(content.UserNibble(0)==0x1)
            {
                category_type = kCategoryMovie;
            }
            else if(content.UserNibble(0)==0x0)
            {
                category_type = kCategorySports;
                category = QObject::tr("Sports");
            }
        }
        else
        {
            category_type = content.GetMythCategory(0);
            category      = content.GetDescription(0);
        }
    }

    uint tableid   = cit->TableID();
    uint version   = cit->Version();
    uint contentid = cit->ContentID();
    // fake endtime
    uint endtime   = MythDate::current().addDays(1).toTime_t();

    // Find Transmissions
    desc_list_t transmissions =
        MPEGDescriptor::FindAll(
            list, PrivateDescriptorID::premiere_content_transmission);
    for(uint j=0; j< transmissions.size(); j++)
    {
        PremiereContentTransmissionDescriptor transmission(transmissions[j]);
        uint networkid = transmission.OriginalNetworkID();
        uint tsid      = transmission.TSID();
        uint serviceid = transmission.ServiceID();

        uint chanid = GetChanID(serviceid, networkid, tsid);

        if (!chanid)
        {
            LOG(VB_EIT, LOG_INFO, LOC +
                QString("Premiere EIT for NIT %1, TID %2, SID %3, "
                        "count %4, title: %5. Channel not found!")
                    .arg(networkid).arg(tsid).arg(serviceid)
                    .arg(transmission.TransmissionCount()).arg(title));
            continue;
        }

        // Skip event if we have already processed it before...
        if (!eitcache->IsNewEIT(chanid, tableid, version, contentid, endtime))
        {
            continue;
        }

        for (uint k=0; k<transmission.TransmissionCount(); ++k)
        {
            QDateTime starttime = transmission.StartTimeUTC(k);
            // fix starttime only if the duration is a multiple of a minute
            if (!(cit->DurationInSeconds() % 60))
                EITFixUp::TimeFix(starttime);
            QDateTime endtime   = starttime.addSecs(cit->DurationInSeconds());

            DBEventEIT *event = new DBEventEIT(
                chanid,
                title,     subtitle,      description,
                category,  category_type,
                starttime, endtime,       fix,
                subtitle_type,
                audio_props,
                video_props, 0.0,
                "",  "");

            db_events.enqueue(event);
        }
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

    QDateTime starttime = MythDate::fromTime_t(
        event.start_time + GPS_EPOCH + gps_offset);

    // fix starttime only if the duration is a multiple of a minute
    if (!(event.length % 60))
        EITFixUp::TimeFix(starttime);
    QDateTime endtime = starttime.addSecs(event.length);

    desc_list_t list = MPEGDescriptor::Parse(event.desc, event.desc_length);
    unsigned char subtitle_type =
        MPEGDescriptor::Find(list, DescriptorID::caption_service) ?
        SUB_HARDHEAR : SUB_UNKNOWN;
    unsigned char audio_properties = AUD_UNKNOWN;
    unsigned char video_properties = VID_UNKNOWN;

    uint atsc_key = (atsc_major << 16) | atsc_minor;

    QMutexLocker locker(&eitList_lock);
    QString title = event.title;
    QString subtitle = ett;
    db_events.enqueue(new DBEventEIT(chanid, title, subtitle,
                                     starttime, endtime,
                                     fixup.value(atsc_key), subtitle_type,
                                     audio_properties, video_properties));
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

    uint chanid = get_chan_id_from_db_atsc(sourceid, atsc_major, atsc_minor);
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

    uint chanid = get_chan_id_from_db_dvb(sourceid, serviceid, networkid, tsid);
    srv_to_chanid[key] = chanid;

    return chanid;
}

uint EITHelper::GetChanID(uint program_number)
{
    uint64_t key;
    key  = ((uint64_t) sourceid);
    key |= ((uint64_t) program_number) << 16;
    key |= ((uint64_t) channelid)      << 32;

    ServiceToChanID::const_iterator it = srv_to_chanid.find(key);
    if (it != srv_to_chanid.end())
        return max(*it, 0);

    uint chanid = get_chan_id_from_db_dtv(sourceid, program_number, channelid);
    srv_to_chanid[key] = chanid;

    return chanid;
}

static uint get_chan_id_from_db_atsc(uint sourceid,
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
        MythDB::DBError("Looking up chanid 1", query);
    else if (query.next())
    {
        bool useOnAirGuide = query.value(1).toBool();
        return (useOnAirGuide) ? query.value(0).toUInt() : 0;
    }

    return 0;
}

// Figure out the chanid for this channel
static uint get_chan_id_from_db_dvb(uint sourceid, uint serviceid,
                                uint networkid, uint transportid)
{
    uint chanid = 0;
    bool useOnAirGuide = false;
    MSqlQuery query(MSqlQuery::InitCon());

    // DVB Link to chanid
    QString qstr =
        "SELECT chanid, useonairguide, channel.sourceid "
        "FROM channel, dtv_multiplex "
        "WHERE serviceid        = :SERVICEID   AND "
        "      networkid        = :NETWORKID   AND "
        "      transportid      = :TRANSPORTID AND "
        "      channel.mplexid  = dtv_multiplex.mplexid";

    query.prepare(qstr);
    query.bindValue(":SERVICEID",   serviceid);
    query.bindValue(":NETWORKID",   networkid);
    query.bindValue(":TRANSPORTID", transportid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Looking up chanID", query);

    if (query.size() == 0) {
        // Attempt fuzzy matching, by skipping the tsid
        // DVB Link to chanid
        QString qstr =
            "SELECT chanid, useonairguide, channel.sourceid "
            "FROM channel, dtv_multiplex "
            "WHERE serviceid        = :SERVICEID   AND "
            "      networkid        = :NETWORKID   AND "
            "      channel.mplexid  = dtv_multiplex.mplexid";

        query.prepare(qstr);
        query.bindValue(":SERVICEID",   serviceid);
        query.bindValue(":NETWORKID",   networkid);
        if (!query.exec() || !query.isActive())
            MythDB::DBError("Looking up chanID in fuzzy mode", query);
    }

    while (query.next())
    {
        // Check to see if we are interested in this channel
        chanid        = query.value(0).toUInt();
        useOnAirGuide = query.value(1).toBool();
        if (sourceid == query.value(2).toUInt())
            return useOnAirGuide ? chanid : 0;
    }

    if (query.size() > 1)
    {
        LOG(VB_EIT, LOG_INFO,
            LOC + QString("found %1 channels for networdid %2, "
                          "transportid %3, serviceid %4 but none "
                          "for current sourceid %5.")
                .arg(query.size()).arg(networkid).arg(transportid)
                .arg(serviceid).arg(sourceid));
    }

    return useOnAirGuide ? chanid : 0;
}

/* Figure out the chanid for this channel from the sourceid,
 * program_number/service_id and the chanid of the channel we are tuned to
 *
 * TODO for SPTS (e.g. HLS / IPTV) it would be useful to match without an entry
 * in dtv_multiplex
 */
static uint get_chan_id_from_db_dtv(uint sourceid, uint serviceid,
                                uint tunedchanid)
{
    uint chanid = 0;
    bool useOnAirGuide = false;
    MSqlQuery query(MSqlQuery::InitCon());

    // DVB Link to chanid
    QString qstr =
        "SELECT c1.chanid, c1.useonairguide, c1.sourceid "
        "FROM channel c1, dtv_multiplex m, channel c2 "
        "WHERE c1.serviceid        = :SERVICEID   AND "
        "      c1.mplexid  = m.mplexid AND "
        "      m.mplexid = c2.mplexid AND "
        "      c2.chanid = :CHANID";

    query.prepare(qstr);
    query.bindValue(":SERVICEID",   serviceid);
    query.bindValue(":CHANID", tunedchanid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Looking up chanID", query);

    while (query.next())
    {
        // Check to see if we are interested in this channel
        chanid        = query.value(0).toUInt();
        useOnAirGuide = query.value(1).toBool();
        if (sourceid == query.value(2).toUInt())
            return useOnAirGuide ? chanid : 0;
    }

    if (query.size() > 1)
    {
        LOG(VB_EIT, LOG_INFO,
            LOC + QString("found %1 channels for multiplex of chanid %2, "
                          "serviceid %3 but none "
                          "for current sourceid %4.")
                .arg(query.size()).arg(tunedchanid)
                .arg(serviceid).arg(sourceid));
    }

    return useOnAirGuide ? chanid : 0;
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
    // UK Freesat
    fix[ 2013LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2017LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2018LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2026LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2041LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2042LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2043LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2044LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2045LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2046LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2047LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2048LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2049LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2050LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2051LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2053LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2054LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2056LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2057LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2063LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2068LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2301LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2302LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2303LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2304LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2305LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2306LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2311LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2312LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2313LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2314LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2401LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2412LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2413LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2602LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2612LL << 32 | 2U << 16] = EITFixUp::kFixUK;

    // ComHem Sweden
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
    fix[      1089LL << 32 |     1  << 16] = // DVB-S
        fix[   773LL << 32 |  8468U << 16] = // DVB-T Berlin/Brandenburg
        fix[  2819LL << 32 |  8468U << 16] = // DVB-T Niedersachsen + Bremen
        fix[  8706LL << 32 |  8468U << 16] = // DVB-T NRW
        fix[ 12801LL << 32 |  8468U << 16] = // DVB-T Bayern
        EITFixUp::kFixRTL | EITFixUp::kFixCategory;

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

    // Netherlands
    fix[ 1000U << 16] = EITFixUp::kFixNL;

    // Finland
    fix[      8438U << 16] = // DVB-T Espoo
        fix[ 42249U << 16] = // DVB-C Welho
        fix[    15U << 16] = // DVB-C Welho
        EITFixUp::kFixFI | EITFixUp::kFixCategory;

    // DVB-S(2) Thor 0.8W Norwegian
    fix[70U << 16] = EITFixUp::kFixNO;

    // DVB-T NTV/NRK (Norway)
    fix[910LL << 32 | 8770U  << 16 | 0x006f] = EITFixUp::kFixNRK_DVBT;  //NRK Folkemusikk
    fix[910LL << 32 | 8770U  << 16 | 0x0070] = EITFixUp::kFixNRK_DVBT;  //NRK Stortinget
    fix[910LL << 32 | 8770U  << 16 | 0x0071] = EITFixUp::kFixNRK_DVBT;  //NRK Super
    fix[910LL << 32 | 8770U  << 16 | 0x0072] = EITFixUp::kFixNRK_DVBT;  //NRK Sport
    fix[910LL << 32 | 8770U  << 16 | 0x0073] = EITFixUp::kFixNRK_DVBT;  //NRK Gull
    fix[910LL << 32 | 8770U  << 16 | 0x0074] = EITFixUp::kFixNRK_DVBT;  //NRK Jazz
    fix[910LL << 32 | 8770U  << 16 | 0x0067] = EITFixUp::kFixNRK_DVBT;  //NRK Super / NRK3
    fix[910LL << 32 | 8770U  << 16 | 0x0068] = EITFixUp::kFixNRK_DVBT;  //NRK Tegnspr�
    fix[910LL << 32 | 8770U  << 16 | 0x0069] = EITFixUp::kFixNRK_DVBT;  //NRK P2
    fix[910LL << 32 | 8770U  << 16 | 0x006a] = EITFixUp::kFixNRK_DVBT;  //NRK P3
    fix[910LL << 32 | 8770U  << 16 | 0x006b] = EITFixUp::kFixNRK_DVBT;  //NRK Alltid Nyheter
    fix[910LL << 32 | 8770U  << 16 | 0x006c] = EITFixUp::kFixNRK_DVBT;  //NRK mP3
    fix[910LL << 32 | 8770U  << 16 | 0x006d] = EITFixUp::kFixNRK_DVBT;  //NRK Klassisk
    fix[910LL << 32 | 8770U  << 16 | 0x006e] = EITFixUp::kFixNRK_DVBT;  //NRK S�i Radio
    fix[910LL << 32 | 8770U  << 16 | 0x0066] = EITFixUp::kFixNRK_DVBT;  //NRK2
    fix[910LL << 32 | 8770U  << 16 | 0x03f0] = EITFixUp::kFixNRK_DVBT;  //NRK1 M�e og Romsdal
    fix[910LL << 32 | 8770U  << 16 | 0x0455] = EITFixUp::kFixNRK_DVBT;  //NRK P1 Tr�delag
    fix[910LL << 32 | 8770U  << 16 | 0x03f1] = EITFixUp::kFixNRK_DVBT;  //NRK1 Midtnytt

    ///////////////////////////////////////////////////////////////////////////
    // Special Early fixups for providers that break DVB EIT spec.
    // transport_id<<32 | network_id<<16 | service_id

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

    //DVB-T Germany Berlin HSE/MonA TV
    fix[  772LL << 32 | 8468 << 16 | 16387] = EITFixUp::kEFixForceISO8859_15;
    //DVB-T Germany Ruhrgebiet Tele 5
    fix[ 8707LL << 32 | 8468 << 16 | 16413] = EITFixUp::kEFixForceISO8859_15;

    // DVB-C Kabel Deutschland encoding fixes Germany
    fix[   112LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10000LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10001LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10002LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10003LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10006LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10009LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    fix[ 10010LL << 32 | 61441U << 16] = EITFixUp::kEFixForceISO8859_15;
    // Mark program on the HD transponders as HDTV
    fix[ 10012LL << 32 | 61441U << 16] = EITFixUp::kFixHDTV;
    fix[ 10013LL << 32 | 61441U << 16] = EITFixUp::kFixHDTV;
    // On transport 10004 only DMAX needs no fixing:
    fix[10004LL<<32 | 61441U << 16 | 50403] = // BBC World Service
    fix[10004LL<<32 | 61441U << 16 | 53101] = // BBC Prime (engl)
    fix[10004LL<<32 | 61441U << 16 | 53108] = // Toon Disney (engl)
    fix[10004LL<<32 | 61441U << 16 | 53109] = // Sky News (engl)
    fix[10004LL<<32 | 61441U << 16 | 53406] = // BBC Prime
    fix[10004LL<<32 | 61441U << 16 | 53407] = // Boomerang (engl)
    fix[10004LL<<32 | 61441U << 16 | 53404] = // Boomerang
    fix[10004LL<<32 | 61441U << 16 | 53408] = // TCM Classic Movies (engl)
    fix[10004LL<<32 | 61441U << 16 | 53409] = // Extreme Sports
    fix[10004LL<<32 | 61441U << 16 | 53410] = // CNBC Europe (engl)
    fix[10004LL<<32 | 61441U << 16 | 53503] = // Detski Mir
    fix[10004LL<<32 | 61441U << 16 | 53411] = // Sat.1 Comedy
    fix[10004LL<<32 | 61441U << 16 | 53412] = // kabel eins classics
    fix[10004LL<<32 | 61441U << 16 | 53112] = // Extreme Sports (engl)
    fix[10004LL<<32 | 61441U << 16 | 53513] = // Playhouse Disney (engl)
    fix[10004LL<<32 | 61441U << 16 | 53618] = // K1010
    fix[10004LL<<32 | 61441U << 16 | 53619] = // GemsTV
        EITFixUp::kEFixForceISO8859_15;
    // On transport 10005 QVC and Giga Digital  needs no fixing:
    fix[10005LL<<32 | 61441U << 16 | 50104] = // E! Entertainment
    fix[10005LL<<32 | 61441U << 16 | 50107] = // 13th Street (KD)
    fix[10005LL<<32 | 61441U << 16 | 50301] = // ESPN Classic
    fix[10005LL<<32 | 61441U << 16 | 50302] = // VH1 Classic
    fix[10005LL<<32 | 61441U << 16 | 50303] = // Wein TV
    fix[10005LL<<32 | 61441U << 16 | 50304] = // AXN
    fix[10005LL<<32 | 61441U << 16 | 50305] = // Silverline
    fix[10005LL<<32 | 61441U << 16 | 50306] = // NASN
    fix[10005LL<<32 | 61441U << 16 | 50307] = // Disney Toon
    fix[10005LL<<32 | 61441U << 16 | 53105] = // NASN (engl)
    fix[10005LL<<32 | 61441U << 16 | 53115] = // VH1 Classic (engl)
    fix[10005LL<<32 | 61441U << 16 | 53405] = // ESPN Classic (engl)
    fix[10005LL<<32 | 61441U << 16 | 53402] = // AXN (engl)
    fix[10005LL<<32 | 61441U << 16 | 53613] = // CNN (engl)
    fix[10005LL<<32 | 61441U << 16 | 53516] = // Voyages Television
    fix[10005LL<<32 | 61441U << 16 | 53611] = // Der Schmuckkanal
    fix[10005LL<<32 | 61441U << 16 | 53104] = // Jukebox
        EITFixUp::kEFixForceISO8859_15;
    // On transport 10007 only following channels need fixing:
    fix[10007LL<<32| 61441U << 16 | 53607] = // Eurosport
    fix[10007LL<<32| 61441U << 16 | 53608] = // Das Vierte
    fix[10007LL<<32| 61441U << 16 | 53609] = // Viva
    fix[10007LL<<32| 61441U << 16 | 53628] = // COMEDY CENTRAL
        EITFixUp::kEFixForceISO8859_15;
    // RTL Subtitle parsing
    fix[10007LL<<32| 61441U << 16 | 53601] = // RTL
    fix[10007LL<<32| 61441U << 16 | 53602] = // Super RTL
    fix[10007LL<<32| 61441U << 16 | 53604] = // VOX
    fix[10007LL<<32| 61441U << 16 | 53606] = // n-tv
        EITFixUp::kFixRTL | EITFixUp::kFixCategory;
    // On transport 10008 only following channels need fixing:
    fix[    10008LL<<32 | 61441U << 16 | 53002] = // Tele 5
        EITFixUp::kEFixForceISO8859_15;

    // DVB-S Astra 19.2E DMAX Germany
    fix[  1113LL << 32 | 1 << 16 | 12602] = EITFixUp::kEFixForceISO8859_15;

    // Premiere
    fix[133 << 16] = EITFixUp::kEFixForceISO8859_15;

    // DVB-S Astra 19.2E French channels
    fix[     1022LL << 32 | 1 << 16 |  6901 ] = // DIRECT 8
        fix[ 1022LL << 32 | 1 << 16 |  6905 ] = // France 24 (en Francais)
        fix[ 1022LL << 32 | 1 << 16 |  6911 ] = // DIRECT 8
        fix[ 1072LL << 32 | 1 << 16 |  8201 ] = // CANAL+
        fix[ 1070LL << 32 | 1 << 16 |  8004 ] = // EURONEWS
        fix[ 1091LL << 32 | 1 << 16 | 31220 ] = // EuroNews
        fix[ 1094LL << 32 | 1 << 16 | 17027 ] = // LCP
        fix[ 1094LL << 32 | 1 << 16 | 17028 ] = // NT1
        fix[ 1100LL << 32 | 1 << 16 |  8710 ] = // NRJ 12
        EITFixUp::kEFixForceISO8859_15;
}
