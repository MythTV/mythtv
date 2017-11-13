// -*- Mode: c++ -*-

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
#include "channelutil.h"        // for ChannelUtil
#include "mythdate.h"
#include "programdata.h"
#include "programinfo.h" // for subtitle types and audio and video properties
#include "scheduledrecording.h" // for ScheduledRecording
#include "compat.h" // for gmtime_r on windows.

const uint EITHelper::kChunkSize = 20;
EITCache *EITHelper::eitcache = new EITCache();

static uint get_chan_id_from_db_atsc(uint sourceid,
                                     uint atscmajor, uint atscminor);
static uint get_chan_id_from_db_dvb(uint sourceid,  uint serviceid,
                                    uint networkid, uint transportid);
static uint get_chan_id_from_db_dtv(uint sourceid,
                                    uint programnumber, uint tunedchanid);
static void init_fixup(FixupMap &fix);

#define LOC QString("EITHelper: ")

EITHelper::EITHelper() :
    eitfixup(new EITFixUp()),
    gps_offset(-1 * GPS_LEAP_SECONDS),
    sourceid(0), channelid(0),
    maxStarttime(QDateTime()), seenEITother(false)
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

    if (db_events.empty())
        return 0;

    MSqlQuery query(MSqlQuery::InitCon());
    for (uint i = 0; (i < kChunkSize) && (db_events.size() > 0); i++)
    {
        DBEventEIT *event = db_events.dequeue();
        eitList_lock.unlock();

        eitfixup->Fix(*event);

        insertCount += event->UpdateDB(query, 1000);
        maxStarttime = max (maxStarttime, event->starttime);

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

void EITHelper::SetFixup(uint atsc_major, uint atsc_minor, FixupValue eitfixup)
{
    QMutexLocker locker(&eitList_lock);
    FixupKey atsc_key = (atsc_major << 16) | atsc_minor;
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

        // Look to see if there has been a recent ett message with the same event id.
        EventIDToETT::iterator it = etts.find(eit->EventID(i));
        QString ett_text = QString::null;
        bool found_matching_ett = false;
        if (it != etts.end())
        {
            // Don't use an ett description if it was scanned long in the past.
            if (!it->IsStale()) {
              ett_text = it->ett_text;
              found_matching_ett = true;
            }
            etts.erase(it);
        }

        // Create an event immediately if a matching ett description was found,
        // or if item is false, indicating that no ett description should be
        // expected.
        if (found_matching_ett || !ev.etm)
        {
            CompleteEvent(atsc_major, atsc_minor, ev, ett_text);
        }
        else
        {
            unsigned char *tmp = new unsigned char[ev.desc_length];
            memcpy(tmp, eit->Descriptors(i), ev.desc_length);
            ev.desc = tmp;
            events.insert(eit->EventID(i), ev);
        }
    }
}

void EITHelper::AddETT(uint atsc_major, uint atsc_minor,
                       const ExtendedTextTable *ett)
{
    uint atsc_key = (atsc_major << 16) | atsc_minor;
    // Try to match up the ett with an eit event.
    ATSCSRCToEvents::iterator eits_it = incomplete_events.find(atsc_key);
    if (eits_it != incomplete_events.end())
    {
        EventIDToATSCEvent::iterator it = (*eits_it).find(ett->EventID());
        if (it != (*eits_it).end())
        {
            bool completed_event = false;
            // Only consider eit events from the recent past.
            if (!it->IsStale()) {
              completed_event = true;
              CompleteEvent(
                  atsc_major, atsc_minor, *it,
                  ett->ExtendedTextMessage().GetBestMatch(languagePreferences));
            }

            if ((*it).desc)
                delete [] (*it).desc;

            (*eits_it).erase(it);

            if (completed_event) return;
        }
    }

    // Report if an unmatched ett was previously noted and overwrite it.
    // See also https://code.mythtv.org/trac/ticket/11739
    EventIDToETT &elist = unmatched_etts[atsc_key];
    EventIDToETT::iterator existing_unmatched_ett_it =
        elist.find(ett->EventID());
    const QString next_ett_text = ett->ExtendedTextMessage()
        .GetBestMatch(languagePreferences);
    if (existing_unmatched_ett_it != elist.end() &&
        existing_unmatched_ett_it->ett_text != next_ett_text)
    {
       LOG(VB_EIT, LOG_DEBUG, LOC +
           QString("Overwriting previously unmatched ett. stale: %1 major: %2 "
                   "minor: %3 old ett: %4  new ett: %5")
               .arg(existing_unmatched_ett_it->IsStale())
               .arg(atsc_major)
               .arg(atsc_minor)
               .arg(existing_unmatched_ett_it->ett_text)
               .arg(next_ett_text));
    }
    elist.insert(ett->EventID(), ATSCEtt(next_ett_text));
}

static void parse_dvb_event_descriptors(desc_list_t list, FixupValue fix,
                                        QMap<uint,uint> languagePreferences,
                                        QString &title, QString &subtitle,
                                        QString &description, QMap<QString,QString> &items)
{
    const unsigned char *bestShortEvent =
        MPEGDescriptor::FindBestMatch(
            list, DescriptorID::short_event, languagePreferences);

    // from EN 300 468, Appendix A.2 - Selection of character table
    unsigned char enc_1[3]  = { 0x10, 0x00, 0x01 };
    unsigned char enc_2[3]  = { 0x10, 0x00, 0x02 };
    unsigned char enc_7[3]  = { 0x10, 0x00, 0x07 }; // Latin/Greek Alphabet
    unsigned char enc_9[3]  = { 0x10, 0x00, 0x09 }; // could use { 0x05 } instead
    unsigned char enc_15[3] = { 0x10, 0x00, 0x0f }; // could use { 0x0B } instead
    int enc_len = 0;
    const unsigned char *enc = NULL;

    // Is this BellExpressVU EIT (Canada) ?
    // Use an encoding override of ISO 8859-1 (Latin1)
    if (fix & EITFixUp::kEFixForceISO8859_1)
    {
        enc = enc_1;
        enc_len = sizeof(enc_1);
    }

    // Is this broken DVB provider in Central Europe?
    // Use an encoding override of ISO 8859-2 (Latin2)
    if (fix & EITFixUp::kEFixForceISO8859_2)
    {
        enc = enc_2;
        enc_len = sizeof(enc_2);
    }

    // Is this broken DVB provider in Western Europe?
    // Use an encoding override of ISO 8859-9 (Latin5)
    if (fix & EITFixUp::kEFixForceISO8859_9)
    {
        enc = enc_9;
        enc_len = sizeof(enc_9);
    }

    // Is this broken DVB provider in Western Europe?
    // Use an encoding override of ISO 8859-15 (Latin9)
    if (fix & EITFixUp::kEFixForceISO8859_15)
    {
        enc = enc_15;
        enc_len = sizeof(enc_15);
    }

    // Is this broken DVB provider in Greece?
    // Use an encoding override of ISO 8859-7 (Latin/Greek)
    if (fix & EITFixUp::kEFixForceISO8859_7)
    {
        enc = enc_7;
        enc_len = sizeof(enc_7);
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

        // add items from the decscriptor to the items
        items.unite (eed.Items());
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
        // do not reschedule if its only present+following
        if (eit->TableID() != TableID::PF_EITo)
        {
            seenEITother = true;
        }
    }
    if (!chanid)
        return;

    uint descCompression = (eit->TableID() > 0x80) ? 2 : 1;
    FixupValue fix = fixup.value((FixupKey)eit->OriginalNetworkID() << 16);
    fix |= fixup.value((((FixupKey)eit->TSID()) << 32) |
                 ((FixupKey)eit->OriginalNetworkID() << 16));
    fix |= fixup.value(((FixupKey)eit->OriginalNetworkID() << 16) |
                 (FixupKey)eit->ServiceID());
    fix |= fixup.value((((FixupKey)eit->TSID()) << 32) |
                 ((FixupKey)eit->OriginalNetworkID() << 16) |
                  (FixupKey)eit->ServiceID());
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
        ProgramInfo::CategoryType category_type = ProgramInfo::kCategoryNone;
        unsigned char subtitle_type=0, audio_props=0, video_props=0;
        uint season = 0, episode = 0, totalepisodes = 0;
        QMap<QString,QString> items;

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
                                        title, subtitle, description, items);
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
                switch (content.GetTheme())
                {
                    case kThemeMovie :
                        category_type = ProgramInfo::kCategoryMovie;
                        break;
                    case kThemeSeries :
                        category_type = ProgramInfo::kCategorySeries;
                        break;
                    case kThemeSports :
                        category_type = ProgramInfo::kCategorySports;
                        break;
                    default :
                        category_type = ProgramInfo::kCategoryNone;
                }
                if (EITFixUp::kFixDish & fix)
                    category  = content.GetCategory();
            }
            else if (EITFixUp::kFixAUDescription & fix)//AU Freeview assigned genres
            {
                ContentDescriptor content(content_data);
                switch (content.Nibble1(0))
                {
                    case 0x01:
                        category = "Movie";
                        break;
                    case 0x02:
                        category = "News";
                        break;
                    case 0x03:
                        category = "Entertainment";
                        break;
                    case 0x04:
                        category = "Sport";
                        break;
                    case 0x05:
                        category = "Children";
                        break;
                    case 0x06:
                        category = "Music";
                        break;
                    case 0x07:
                        category = "Arts/Culture";
                        break;
                    case 0x08:
                        category = "Current Affairs";
                        break;
                    case 0x09:
                        category = "Education";
                        break;
                    case 0x0A:
                        category = "Infotainment";
                        break;
                    case 0x0B:
                        category = "Special";
                        break;
                    case 0x0C:
                        category = "Comedy";
                        break;
                    case 0x0D:
                        category = "Drama";
                        break;
                    case 0x0E:
                        category = "Documentary";
                        break;
                    default:
                        category = "";
                        break;
                }
                category_type = content.GetMythCategory(0);
            }
            else if (EITFixUp::kFixGreekEIT & fix)//Greek
            {
                ContentDescriptor content(content_data);
                switch (content.Nibble2(0))
                {
                    case 0x01:
                        category = "Ταινία";       // confirmed
                        break;
                    case 0x02:
                        category = "Ενημερωτικό";  // confirmed
                        break;
                    case 0x04:
                        category = "Αθλητικό";     // confirmed
                        break;
                    case 0x05:
                        category = "Παιδικό";      // confirmed
                        break;
                    case 0x09:
                        category = "Ντοκιμαντέρ";  // confirmed
                        break;
                    default:
                        category = "";
                        break;
                }
                category_type = content.GetMythCategory(2);
            }
            else
            {
                ContentDescriptor content(content_data);
                category      = content.GetDescription(0);
#if 0 /* there is no category_type in DVB EIT */
                category_type = content.GetMythCategory(0);
#endif
            }
        }

        desc_list_t contentIds =
            MPEGDescriptor::FindAll(list, DescriptorID::dvb_content_identifier);
        for (uint j = 0; j < contentIds.size(); j++)
        {
            DVBContentIdentifierDescriptor desc(contentIds[j]);
            for (uint k = 0; k < desc.CRIDCount(); k++)
            {
                if (desc.ContentEncoding(k) == 0)
                {
                    // The CRID is a URI.  It could contain UTF8 sequences encoded
                    // as %XX but there's no advantage in decoding them.
                    // The BBC currently uses private types 0x31 and 0x32.
                    // IDs from the authority eventis.nl are not fit for our scheduler
                    if (desc.ContentType(k) == 0x01 || desc.ContentType(k) == 0x31)
                    {
                        if (!desc.ContentId(k).startsWith ("eventis.nl/"))
                        {
                            programId = desc.ContentId(k);
                        }
                    }
                    else if (desc.ContentType(k) == 0x02 || desc.ContentType(k) == 0x32)
                    {
                        if (!desc.ContentId(k).startsWith ("eventis.nl/"))
                        {
                            seriesId = desc.ContentId(k);
                        }
                        category_type = ProgramInfo::kCategorySeries;
                    }
                }
            }
        }

        /* if we don't have a subtitle, try to parse one from private descriptors */
        if (subtitle.isEmpty()) {
            bool isUPC = false;
            /* is this event carrying UPC private data? */
            desc_list_t private_data_specifiers = MPEGDescriptor::FindAll(list, DescriptorID::private_data_specifier);
            for (uint j = 0; j < private_data_specifiers.size(); j++) {
                PrivateDataSpecifierDescriptor desc(private_data_specifiers[j]);
                if (desc.PrivateDataSpecifier() == PrivateDataSpecifierID::UPC1) {
                    isUPC = true;
                }
            }

            if (isUPC) {
                desc_list_t subtitles = MPEGDescriptor::FindAll(list, PrivateDescriptorID::upc_event_episode_title);
                for (uint j = 0; j < subtitles.size(); j++) {
                    PrivateUPCCablecomEpisodeTitleDescriptor desc(subtitles[j]);

                    subtitle = desc.Text();
                }
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
            seriesId,  programId,
            season, episode, totalepisodes);
        event->items = items;

        db_events.enqueue(event);
    }
}

// This function gets special EIT data from the German provider Premiere
// for the option channels Premiere Sport and Premiere Direkt
void EITHelper::AddEIT(const PremiereContentInformationTable *cit)
{
    // set fixup for Premiere
    FixupValue fix = fixup.value(133 << 16);
    fix |= EITFixUp::kFixGenericDVB;

    QString title         = QString("");
    QString subtitle      = QString("");
    QString description   = QString("");
    QString category      = QString("");
    ProgramInfo::CategoryType category_type = ProgramInfo::kCategoryNone;
    unsigned char subtitle_type=0, audio_props=0, video_props=0;
    uint season = 0, episode = 0, totalepisodes = 0;
    QMap<QString,QString> items;

    // Parse descriptors
    desc_list_t list = MPEGDescriptor::Parse(
        cit->Descriptors(), cit->DescriptorsLength());

    parse_dvb_event_descriptors(list, fix, languagePreferences,
                                title, subtitle, description, items);

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
                category_type = ProgramInfo::kCategoryMovie;
            }
            else if(content.UserNibble(0)==0x0)
            {
                category_type = ProgramInfo::kCategorySports;
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
                "",  "",
                season, episode, totalepisodes);
            event->items = items;

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
        return *it;

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
        return *it;

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
        return *it;

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

static void init_fixup(FixupMap &fix)
{
    ///////////////////////////////////////////////////////////////////////////
    // Fixups to make EIT provided listings more useful
    // transport_id<<32 | original_network_id<<16 | service_id

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

    // United Kingdom - DVB-T/T2
    fix[ 9018U << 16] = EITFixUp::kFixUK;

    // UK - Astra 28.2
    for (int i = 2001; i <= 2035; ++i)
       fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;

    fix[ 2036LL << 32 | 2U << 16] = EITFixUp::kFixUK | EITFixUp::kFixHTML;

    for (int i = 2037; i <= 2057; ++i)
       fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2059LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2061LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    for (int i = 2063; i <= 2069; ++i)
       fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2071LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2076LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2081LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    for (int i = 2089; i <= 2092; ++i)
       fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;
    for (int i = 2094; i <= 2099; ++i)
       fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;
    for (int i = 2102; i <= 2110; ++i)
       fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2112LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2116LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2301LL << 32 | 2U << 16] = EITFixUp::kFixUK | EITFixUp::kFixHTML;
    fix[ 2302LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2303LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2304LL << 32 | 2U << 16] = EITFixUp::kFixUK | EITFixUp::kFixHTML;
    fix[ 2305LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2306LL << 32 | 2U << 16] = EITFixUp::kFixUK | EITFixUp::kFixHTML;
    fix[ 2311LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2312LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2313LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2314LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2315LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2316LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    for (int i = 2401; i <= 2413; ++i)
        fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2601LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2602LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2603LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2604LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2612LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2614LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2611LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2612LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2613LL << 32 | 2U << 16] = EITFixUp::kFixUK;

    // R.caroline
    fix[ 2315LL << 32 | 59U << 16] = EITFixUp::kFixUK;

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
    fix[ 4096U  << 16] = EITFixUp::kFixAUStar;
    fix[ 4096U  << 16] = EITFixUp::kFixAUStar;
    fix[ 4112U << 16]  = EITFixUp::kFixAUDescription | EITFixUp::kFixAUFreeview; // ABC Brisbane
    fix[ 4114U << 16]  = EITFixUp::kFixAUDescription | EITFixUp::kFixAUFreeview | EITFixUp::kFixAUNine;; // Nine Brisbane
    fix[ 4115U  << 16] = EITFixUp::kFixAUDescription | EITFixUp::kFixAUSeven; //Seven
    fix[ 4116U  << 16] = EITFixUp::kFixAUDescription; //Ten
    fix[ 12801U << 16] = EITFixUp::kFixAUFreeview | EITFixUp::kFixAUDescription; //ABC
    fix[ 12802U << 16] = EITFixUp::kFixAUDescription; //SBS
    fix[ 12803U << 16] = EITFixUp::kFixAUFreeview | EITFixUp::kFixAUDescription | EITFixUp::kFixAUNine; //Nine
    fix[ 12842U << 16] = EITFixUp::kFixAUDescription; // 31 Brisbane
    fix[ 12862U  << 16] = EITFixUp::kFixAUDescription; //WestTV

    // MultiChoice Africa
    fix[ 6144U << 16] = EITFixUp::kFixMCA;

    // RTL Subtitle parsing
    fix[      1089LL << 32 |     1  << 16] = // DVB-S
    fix[ 1041LL << 32 | 1 << 16] = // DVB-S RTL Group HD Austria Transponder
    fix[ 1057LL << 32 | 1 << 16] = // DVB-S RTL Group HD Transponder
        fix[   773LL << 32 |  8468U << 16] = // DVB-T Berlin/Brandenburg
        fix[  2819LL << 32 |  8468U << 16] = // DVB-T Niedersachsen + Bremen
        fix[  8706LL << 32 |  8468U << 16] = // DVB-T NRW
        fix[ 12801LL << 32 |  8468U << 16] = // DVB-T Bayern
        EITFixUp::kFixRTL;

    // Mark HD+ channels as HDTV
    fix[   1041LL << 32 |  1 << 16] = EITFixUp::kFixHDTV;
    fix[   1055LL << 32 |  1 << 16] = EITFixUp::kFixHDTV;
    fix[   1057LL << 32 |  1 << 16] = EITFixUp::kFixHDTV;
    fix[   1109LL << 32 |  1 << 16] = EITFixUp::kFixHDTV;

    // PRO7/SAT.1
    fix[   1017LL << 32 |    1 << 16] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1;
    fix[   1031LL << 32 |    1 << 16 | 5300] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1;
    fix[   1031LL << 32 |    1 << 16 | 5301] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1;
    fix[   1031LL << 32 |    1 << 16 | 5302] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1;
    fix[   1031LL << 32 |    1 << 16 | 5303] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1;
    fix[   1031LL << 32 |    1 << 16 | 5310] = EITFixUp::kFixP7S1;
    fix[   1031LL << 32 |    1 << 16 | 5311] = EITFixUp::kFixP7S1;
    fix[   1107LL << 32 |    1 << 16] = EITFixUp::kFixP7S1;
    fix[   1082LL << 32 |    1 << 16] = EITFixUp::kFixP7S1;
    fix[      5LL << 32 |  133 << 16 |   776] = EITFixUp::kFixP7S1;
    fix[                  8468 << 16 | 16426] = EITFixUp::kFixP7S1; // ProSieben MAXX - DVB-T Rhein/Main
    fix[   8707LL << 32 | 8468 << 16]         = EITFixUp::kFixP7S1; // ProSieben Sat.1 Mux - DVB-T Rhein/Main

    // ATV / ATV2
    fix[   1117LL << 32 |  1 << 16 | 13012 ] = EITFixUp::kFixATV; // ATV
    fix[   1003LL << 32 |  1 << 16 | 13223 ] = EITFixUp::kFixATV; // ATV2
    fix[   1003LL << 32 |  1 << 16 | 13228 ] = EITFixUp::kFixHDTV | EITFixUp::kFixATV; // ATV HD

    // Disney Channel Germany
    fix[   1055LL << 32 |  1 << 16 | 5500 ] = EITFixUp::kFixHDTV | EITFixUp::kFixDisneyChannel; // Disney Channel HD
    fix[   1055LL << 32 |  1 << 16 | 5510 ] = EITFixUp::kFixHDTV | EITFixUp::kFixDisneyChannel; // Disney Channel HD Austria
    fix[   5LL << 32 |  133 << 16 | 1793 ] = EITFixUp::kFixDisneyChannel; // Disney Channel HD Austria
    fix[   1109LL << 32 |  1 << 16 | 5401 ] = EITFixUp::kFixHDTV | EITFixUp::kFixDisneyChannel; // Tele 5 HD
    fix[   1109LL << 32 |  1 << 16 | 5421 ] = EITFixUp::kFixHDTV | EITFixUp::kFixDisneyChannel; // Tele 5 HD Austria
    fix[   33LL << 32 |  133 << 16 | 51 ] = EITFixUp::kFixDisneyChannel; // Tele 5

    // Premiere EIT processing
    fix[   1LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   2LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   3LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   4LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[   6LL << 32 |  133 << 16] = EITFixUp::kFixPremiere | EITFixUp::kFixHDTV;
    fix[   8LL << 32 |  133 << 16] = EITFixUp::kFixPremiere | EITFixUp::kFixHDTV;
    fix[   9LL << 32 |  133 << 16] = EITFixUp::kFixPremiere | EITFixUp::kFixHDTV;
    fix[  10LL << 32 |  133 << 16] = EITFixUp::kFixPremiere | EITFixUp::kFixHDTV;
    fix[  11LL << 32 |  133 << 16] = EITFixUp::kFixPremiere | EITFixUp::kFixHDTV;
    fix[  12LL << 32 |  133 << 16] = EITFixUp::kFixPremiere | EITFixUp::kFixHDTV;
    fix[  13LL << 32 |  133 << 16] = EITFixUp::kFixPremiere | EITFixUp::kFixHDTV;
    fix[  14LL << 32 |  133 << 16] = EITFixUp::kFixPremiere | EITFixUp::kFixHDTV;
    fix[  15LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    fix[  17LL << 32 |  133 << 16] = EITFixUp::kFixPremiere;
    // Mark Premiere HD, AXN HD and Discovery HD as HDTV
    fix[   6LL << 32 |  133 << 16 | 129] = EITFixUp::kFixHDTV;
    fix[   6LL << 32 |  133 << 16 | 130] = EITFixUp::kFixHDTV;
    fix[  10LL << 32 |  133 << 16 | 125] = EITFixUp::kFixHDTV;

    // Netherlands DVB-C
    fix[ 1000U << 16] = EITFixUp::kFixNL;
    // Canal Digitaal DVB-S 19.2 Dutch/Belgian ONID 53 covers all CanalDigitaal TiD
    fix[   53U << 16] = EITFixUp::kFixNL;
    // Canal Digitaal DVB-S 23.5 Dutch/Belgian
    fix[  3202LL << 32 | 3U << 16] = EITFixUp::kFixNL;
    fix[  3208LL << 32 | 3U << 16] = EITFixUp::kFixNL;
    fix[  3211LL << 32 | 3U << 16] = EITFixUp::kFixNL;
    fix[  3222LL << 32 | 3U << 16] = EITFixUp::kFixNL;
    fix[  3225LL << 32 | 3U << 16] = EITFixUp::kFixNL;

    // Finland
    fix[      8438U << 16] = // DVB-T Espoo
        fix[ 42249U << 16] = // DVB-C Welho
        fix[    15U << 16] = // DVB-C Welho
        EITFixUp::kFixFI;

    // DVB-C YouSee (Denmark)
    fix[65024U << 16] = EITFixUp::kFixDK;

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
    //fix[ 8707LL << 32 | 8468 << 16 | 16413] = EITFixUp::kEFixForceISO8859_15; // they are sending the ISO 8859-9 signalling now
    // ANIXE
    fix[ 8707LL << 32 | 8468U << 16 | 16426 ] = // DVB-T Rhein-Main
        EITFixUp::kEFixForceISO8859_9;

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

    // DVB-C Unitymedia Germany
    fix[ 9999 << 16 |   161LL << 32 | 12101 ] = // RTL Television
    fix[ 9999 << 16 |   161LL << 32 | 12104 ] = // VOX
    fix[ 9999 << 16 |   161LL << 32 | 12107 ] = // Super RTL
    fix[ 9999 << 16 |   161LL << 32 | 12109 ] = // n-tv
    fix[ 9999 << 16 |   301LL << 32 | 30114 ] = // RTL NITRO
        EITFixUp::kFixRTL;
    fix[ 9999 << 16 |   191LL << 32 | 11102 ] = // DAS VIERTE
        EITFixUp::kEFixForceISO8859_15;
    // on this transport are only HD services, two TBD, arte and ServusTV, I think arte properly signals HD!
    fix[ 9999 << 16 |   401LL << 32 | 29109 ] = // ServusTV HD
        EITFixUp::kFixHDTV;
    // generic Unitymedia / Liberty Global / Eventis.nl?
    fix[ 9999 << 16 |   121LL << 32 | 12107 ] = // Super RTL
    fix[ 9999 << 16 |   151LL << 32 | 15110 ] = // Bibel TV
    fix[ 9999 << 16 |   161LL << 32 | 12107 ] = // Super RTL
    fix[ 9999 << 16 |   161LL << 32 | 12109 ] = // n-tv
    fix[ 9999 << 16 |   171LL << 32 | 17119 ] = // RiC
    fix[ 9999 << 16 |   171LL << 32 | 27102 ] = // DELUXE MUSIC
    fix[ 9999 << 16 |   181LL << 32 | 24108 ] = // DMAX
    fix[ 9999 << 16 |   181LL << 32 | 25102 ] = // TV5MONDE Europe
    fix[ 9999 << 16 |   191LL << 32 | 11102 ] = // Disney SD
    fix[ 9999 << 16 |   191LL << 32 | 12110 ] = // N24
    fix[ 9999 << 16 |   191LL << 32 | 12111 ] = // Tele 5
    fix[ 9999 << 16 |   201LL << 32 | 27103 ] = // TLC
    fix[ 9999 << 16 |   211LL << 32 | 29108 ] = // Astro TV
    fix[ 9999 << 16 |   231LL << 32 | 23117 ] = // Deutsches Musik Fernsehen
    fix[ 9999 << 16 |   231LL << 32 | 23115 ] = // Family TV
    fix[ 9999 << 16 |   271LL << 32 | 27101 ] = // DIE NEUE ZEIT TV
    fix[ 9999 << 16 |   541LL << 32 | 54101 ] = // HR HD
        EITFixUp::kFixUnitymedia;

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

    // DVB-C T-Kábel Hungary
    // FIXME this should be more specific. Is the encoding really wrong for all services?
    fix[  100 << 16] = EITFixUp::kEFixForceISO8859_2;

    // DVB-T Greece
    // Pelion Transmitter
    // transport_id<<32 | netword_id<<16 | service_id
    fix[  100LL << 32 |  8492LL << 16 ] = // Ant1,Alpha,Art,10E
    fix[  102LL << 32 |  8492LL << 16 ] = // Mega,Star,SKAI,M.tv
    fix[  320LL << 32 |  8492LL << 16 ] = // Astra,Thessalia,TRT,TV10,ZEYS
        EITFixUp::kFixGreekEIT |
        EITFixUp::kFixGreekCategories;
    fix[    2LL << 32 |  8492LL << 16 ] = // N1,Nplus,NHD,Vouli
        EITFixUp::kEFixForceISO8859_7 |   // it is encoded in cp-1253
        EITFixUp::kFixGreekSubtitle |     // Subtitle has too much info and is
        EITFixUp::kFixGreekEIT |              // cut in db. Will move to descr.
        EITFixUp::kFixGreekCategories;

    // DVB-S Star One C2 70W (Brazil)
    // it has original_network_id = 1 like Astra on 19.2E, but transport_id does
    // not collide at the moment
    fix[  1LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[  2LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[  3LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[  4LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 50LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 51LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 52LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 53LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 54LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 55LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 56LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 57LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 58LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
    fix[ 59LL << 32 | 1LL << 16 ] = EITFixUp::kEFixForceISO8859_1;
}

/** \fn EITHelper::RescheduleRecordings(void)
 *  \brief Tells scheduler about programming changes.
 *
 */
void EITHelper::RescheduleRecordings(void)
{
    ScheduledRecording::RescheduleMatch(
        0, sourceid, seenEITother ? 0 : ChannelUtil::GetMplexID(channelid),
        maxStarttime, "EITScanner");
    seenEITother = false;
    maxStarttime = QDateTime();
}
