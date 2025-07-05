// -*- Mode: c++ -*-

// Std C++ headers
#include <algorithm>

// MythTV includes
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h" // for subtitle types and audio and video properties

#include "channelutil.h"
#include "eitcache.h"
#include "eitfixup.h"
#include "eithelper.h"
#include "mpeg/atsctables.h"
#include "mpeg/dishdescriptors.h"
#include "mpeg/dvbtables.h"
#include "mpeg/premieredescriptors.h"
#include "mpeg/premieretables.h"
#include "programdata.h"
#include "scheduledrecording.h"  // for ScheduledRecording

const uint EITHelper::kMaxQueueSize   = 10000;

EITCache *EITHelper::s_eitCache = new EITCache();

static uint get_chan_id_from_db_atsc(uint sourceid,
                                     uint atsc_major, uint atsc_minor);
static uint get_chan_id_from_db_dvb(uint sourceid,  uint serviceid,
                                    uint networkid, uint transportid);
static uint get_chan_id_from_db_dtv(uint sourceid,
                                    uint serviceid, uint tunedchanid);
static void init_fixup(FixupMap &fix);

#define LOC QString("EITHelper: ")
#define LOC_ID QString("EITHelper[%1]: ").arg(m_cardnum)

EITHelper::EITHelper(uint cardnum) :
    m_cardnum(cardnum)
{
    m_chunkSize = gCoreContext->GetNumSetting("EITEventChunkSize", 20);
    m_queueSize = std::min(m_chunkSize * 50, kMaxQueueSize);
    LOG(VB_EIT, LOG_INFO, LOC_ID +
        QString("EITHelper chunk size %1 and queue size %2 events")
            .arg(m_chunkSize).arg(m_queueSize));

    // Save EIT cache in database table eit_cache iff true
    bool persistent = gCoreContext->GetBoolSetting("EITCachePersistent", true);
    s_eitCache->SetPersistent(persistent);
    LOG(VB_EIT, LOG_INFO, LOC_ID +
        QString("EITCache %1")
            .arg(persistent ? "in memory, backup to database" : "in memory only"));

    init_fixup(m_fixup);
}

EITHelper::~EITHelper()
{
    QMutexLocker locker(&m_eitListLock);
    while (!m_dbEvents.empty())
        delete m_dbEvents.dequeue();
}

uint EITHelper::GetListSize(void) const
{
    QMutexLocker locker(&m_eitListLock);
    return m_dbEvents.size();
}

bool EITHelper::EventQueueFull(void) const
{
    uint listsize = GetListSize();
    bool full = listsize > m_queueSize;
    return full;
}

/** \fn EITHelper::ProcessEvents(void)
 *  \brief Get events from queue and insert into DB after processing.
 *
 * Process a maximum of kChunkSize events at a time
 * to avoid clogging the machine.
 *
 *  \return Returns number of events inserted into DB.
 */
uint EITHelper::ProcessEvents(void)
{
    QMutexLocker locker(&m_eitListLock);

    if (m_dbEvents.empty())
        return 0;

    MSqlQuery query(MSqlQuery::InitCon());

    uint eventCount = 0;
    uint insertCount = 0;
    for (; (eventCount < m_chunkSize) && (!m_dbEvents.empty()); eventCount++)
    {
        DBEventEIT *event = m_dbEvents.dequeue();
        m_eitListLock.unlock();

        EITFixUp::Fix(*event);

        insertCount += event->UpdateDB(query, 1000);
        m_maxStarttime = std::max (m_maxStarttime, event->m_starttime);

        delete event;
        m_eitListLock.lock();
    }

    if (!insertCount)
        return 0;

    if (!m_incompleteEvents.empty())
    {
        LOG(VB_EIT, LOG_DEBUG, LOC_ID +
            QString("Added %1 events -- complete: %2 incomplete: %3")
                .arg(insertCount).arg(m_dbEvents.size())
                .arg(m_incompleteEvents.size()));
    }
    else
    {
        LOG(VB_EIT, LOG_DEBUG, LOC_ID +
            QString("Added %1/%2 events, queued: %3")
                .arg(insertCount).arg(eventCount).arg(m_dbEvents.size()));
    }

    return insertCount;
}

void EITHelper::SetFixup(uint atsc_major, uint atsc_minor, FixupValue eitfixup)
{
    QMutexLocker locker(&m_eitListLock);
    FixupKey atsc_key = (atsc_major << 16) | atsc_minor;
    m_fixup[atsc_key] = eitfixup;
}

void EITHelper::SetLanguagePreferences(const QStringList &langPref)
{
    QMutexLocker locker(&m_eitListLock);

    uint priority = 1;
    QStringList::const_iterator it;
    for (it = langPref.begin(); it != langPref.end(); ++it)
    {
        if (!(*it).isEmpty())
        {
            uint language_key   = iso639_str3_to_key(*it);
            uint canonoical_key = iso639_key_to_canonical_key(language_key);
            m_languagePreferences[canonoical_key] = priority++;
        }
    }
}

void EITHelper::SetSourceID(uint sourceid)
{
    QMutexLocker locker(&m_eitListLock);
    m_sourceid = sourceid;
}

void EITHelper::SetChannelID(uint channelid)
{
    QMutexLocker locker(&m_eitListLock);
    m_channelid = channelid;
}

void EITHelper::AddEIT(uint atsc_major, uint atsc_minor,
                       const EventInformationTable *eit)
{
    uint atsc_key = (atsc_major << 16) | atsc_minor;
    EventIDToATSCEvent &events = m_incompleteEvents[atsc_key];

    for (uint i = 0; i < eit->EventCount(); i++)
    {
        ATSCEvent ev(eit->StartTimeRaw(i), eit->LengthInSeconds(i),
                     eit->ETMLocation(i),
                     eit->title(i).GetBestMatch(m_languagePreferences),
                     eit->Descriptors(i), eit->DescriptorsLength(i));

        // Create an event immediately if the ETM_location specifies
        // that there is no ETT event coming; otherwise save the EIT
        // event in the incomplete_events for this channel.
        if (!ev.m_etm)
        {
            CompleteEvent(atsc_major, atsc_minor, ev, "");
        }
        else
        {
            // If there is an existing EIT event with this event_id then
            // delete the descriptor to avoid a memory leak.
            EventIDToATSCEvent::iterator it = events.find(eit->EventID(i));
            if (it != events.end())
            {
                delete [] (*it).m_desc;
            }

            // Save the EIT event in the incomplete_events for this channel.
            auto *tmp = new unsigned char[ev.m_descLength];
            memcpy(tmp, eit->Descriptors(i), ev.m_descLength);
            ev.m_desc = tmp;
            events.insert(eit->EventID(i), ev);
        }
    }
}

void EITHelper::AddETT(uint atsc_major, uint atsc_minor,
                       const ExtendedTextTable *ett)
{
    // Find the matching incomplete EIT event for this ETT
    // If we have no EIT event then just discard the ETT.
    uint atsc_key = (atsc_major << 16) | atsc_minor;
    ATSCSRCToEvents::iterator eits_it = m_incompleteEvents.find(atsc_key);
    if (eits_it != m_incompleteEvents.end())
    {
        EventIDToATSCEvent::iterator it = (*eits_it).find(ett->EventID());
        if (it != (*eits_it).end())
        {
            // Only consider EIT events from the very recent past.
            if (!it->IsStale()) {
              CompleteEvent(
                  atsc_major, atsc_minor, *it,
                  ett->ExtendedTextMessage().GetBestMatch(m_languagePreferences));
            }

            // Remove EIT event from the incomplete_event list.
            delete [] (*it).m_desc;
            (*eits_it).erase(it);
        }
    }
}

static void parse_dvb_event_descriptors(const desc_list_t& list, FixupValue fix,
                                        QMap<uint,uint> languagePreferences,
                                        QString &title, QString &subtitle,
                                        QString &description, QMultiMap<QString,QString> &items)
{
    const unsigned char *bestShortEvent =
        MPEGDescriptor::FindBestMatch(
            list, DescriptorID::short_event, languagePreferences);

    // from EN 300 468, Appendix A.2 - Selection of character table
    const enc_override enc_1  { 0x10, 0x00, 0x01 };
    const enc_override enc_2  { 0x10, 0x00, 0x02 };
    const enc_override enc_7  { 0x10, 0x00, 0x07 }; // Latin/Greek Alphabet
    const enc_override enc_9  { 0x10, 0x00, 0x09 }; // could use { 0x05 } instead
    const enc_override enc_15 { 0x10, 0x00, 0x0f }; // could use { 0x0B } instead
    const enc_override enc_none {};
    enc_override enc = enc_none;

    // Is this BellExpressVU EIT (Canada) ?
    // Use an encoding override of ISO 8859-1 (Latin1)
    if (fix & EITFixUp::kEFixForceISO8859_1)
    {
        enc = enc_1;
    }

    // Is this broken DVB provider in Central Europe?
    // Use an encoding override of ISO 8859-2 (Latin2)
    if (fix & EITFixUp::kEFixForceISO8859_2)
    {
        enc = enc_2;
    }

    // Is this broken DVB provider in Western Europe?
    // Use an encoding override of ISO 8859-9 (Latin5)
    if (fix & EITFixUp::kEFixForceISO8859_9)
    {
        enc = enc_9;
    }

    // Is this broken DVB provider in Western Europe?
    // Use an encoding override of ISO 8859-15 (Latin9)
    if (fix & EITFixUp::kEFixForceISO8859_15)
    {
        enc = enc_15;
    }

    // Is this broken DVB provider in Greece?
    // Use an encoding override of ISO 8859-7 (Latin/Greek)
    if (fix & EITFixUp::kEFixForceISO8859_7)
    {
        enc = enc_7;
    }

    if (bestShortEvent)
    {
        ShortEventDescriptor sed(bestShortEvent);
        if (sed.IsValid())
        {
            title    = sed.EventName(enc);
            subtitle = sed.Text(enc);
        }
    }

    std::vector<const unsigned char*> bestExtendedEvents =
        MPEGDescriptor::FindBestMatches(
            list, DescriptorID::extended_event, languagePreferences);

    description = "";
    for (auto & best_event : bestExtendedEvents)
    {
        if (!best_event)
        {
            description = "";
            break;
        }

        ExtendedEventDescriptor eed(best_event);
        if (eed.IsValid())
        {
            description += eed.Text(enc);
        }
        // add items from the descriptor to the items
        items.unite (eed.Items());
    }
}

static inline void parse_dvb_component_descriptors(const desc_list_t& list,
                                                   unsigned char &subtitle_type,
                                                   unsigned char &audio_properties,
                                                   unsigned char &video_properties)
{
    desc_list_t components =
        MPEGDescriptor::FindAll(list, DescriptorID::component);
    for (auto & comp : components)
    {
        ComponentDescriptor component(comp);
        if (!component.IsValid())
            continue;
        video_properties |= component.VideoProperties();
        audio_properties |= component.AudioProperties();
        subtitle_type    |= component.SubtitleType();
    }
}

void EITHelper::AddEIT(const DVBEventInformationTable *eit)
{
    // Discard event if incoming event queue full
    if (EventQueueFull())
        return;

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
            m_seenEITother = true;
        }
    }
    if (!chanid)
        return;

    uint descCompression = (eit->TableID() > 0x80) ? 2 : 1;
    FixupValue fix = m_fixup.value((FixupKey)eit->OriginalNetworkID() << 16);
    fix |= m_fixup.value((((FixupKey)eit->TSID()) << 32) |
                 ((FixupKey)eit->OriginalNetworkID() << 16));
    fix |= m_fixup.value(((FixupKey)eit->OriginalNetworkID() << 16) |
                 (FixupKey)eit->ServiceID());
    fix |= m_fixup.value((((FixupKey)eit->TSID()) << 32) |
                 ((FixupKey)eit->OriginalNetworkID() << 16) |
                  (FixupKey)eit->ServiceID());
    fix |= EITFixUp::kFixGenericDVB;

    uint tableid   = eit->TableID();
    uint version   = eit->Version();
    for (uint i = 0; i < eit->EventCount(); i++)
    {
        // Skip event if we have already processed it before...
        if (!s_eitCache->IsNewEIT(chanid, tableid, version, eit->EventID(i),
                              eit->EndTimeUnixUTC(i)))
        {
            continue;
        }

        QString title         = QString("");
        QString subtitle      = QString("");
        QString description   = QString("");
        QString category      = QString("");
        ProgramInfo::CategoryType category_type = ProgramInfo::kCategoryNone;
        unsigned char subtitle_type=0;
        unsigned char audio_props=0;
        unsigned char video_props=0;
        uint season = 0;
        uint episode = 0;
        uint totalepisodes = 0;
        QMultiMap<QString,QString> items;

        // Parse descriptors
        desc_list_t list = MPEGDescriptor::Parse(
            eit->Descriptors(i), eit->DescriptorsLength(i));

        const unsigned char *dish_event_name = nullptr;
        if (EITFixUp::kFixDish & fix)
        {
            dish_event_name = MPEGDescriptor::Find(
                    list, PrivateDescriptorID::dish_event_name);
        }

        if (dish_event_name)
        {
            DishEventNameDescriptor dend(dish_event_name);
            if (dend.IsValid() && dend.HasName())
                title = dend.Name(descCompression);

            const unsigned char *dish_event_description =
                MPEGDescriptor::Find(
                    list, PrivateDescriptorID::dish_event_description);
            if (dish_event_description)
            {
                DishEventDescriptionDescriptor dedd(dish_event_description);
                if (dedd.IsValid() && dedd.HasDescription())
                    description = dedd.Description(descCompression);
            }
        }
        else
        {
            parse_dvb_event_descriptors(list, fix, m_languagePreferences,
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
                if (mpaa.IsValid())
                    stars = mpaa.stars();

                if (stars != 0.0F) // Only movies for now
                {
                    rating = mpaa.rating();
                    rating_system = "MPAA";
                    advisory = mpaa.advisory();
                }
            }

            if (stars == 0.0F) // Not MPAA rated, check VCHIP
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
                static const std::array<const std::string,16> s_auGenres
                    {/* 0*/"Unknown", "Movie", "News", "Entertainment",
                     /* 4*/"Sport", "Children", "Music", "Arts/Culture",
                     /* 8*/"Current Affairs", "Education", "Infotainment",
                     /*11*/"Special", "Comedy", "Drama", "Documentary",
                     /*15*/"Unknown"};
                ContentDescriptor content(content_data);
                if (content.IsValid())
                {
                    category = QString::fromStdString(s_auGenres[content.Nibble1(0)]);
                    category_type = content.GetMythCategory(0);
                }
            }
            else if (EITFixUp::kFixGreekEIT & fix)//Greek
            {
                static const std::array<const std::string,16>s_grGenres
                    {/* 0*/"Unknown",  "Ταινία", "Ενημερωτικό", "Unknown",
                     /* 4*/"Αθλητικό", "Παιδικό", "Unknown", "Unknown",
                     /* 8*/"Unknown", "Ντοκιμαντέρ", "Unknown", "Unknown",
                     /*12*/"Unknown", "Unknown", "Unknown", "Unknown"};
                ContentDescriptor content(content_data);
                if (content.IsValid())
                {
                    category = QString::fromStdString(s_grGenres[content.Nibble2(0)]);
                    category_type = content.GetMythCategory(2);
                }
            }
            else
            {
                ContentDescriptor content(content_data);
                if (content.IsValid())
                {
                    category      = content.GetDescription(0);
#if 0 /* there is no category_type in DVB EIT */
                    category_type = content.GetMythCategory(0);
#endif
                }
            }
        }

        desc_list_t contentIds =
            MPEGDescriptor::FindAll(list, DescriptorID::dvb_content_identifier);
        for (auto & id : contentIds)
        {
            DVBContentIdentifierDescriptor desc(id);
            if (!desc.IsValid())
                continue;
            for (size_t k = 0; k < desc.CRIDCount(); k++)
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
            for (auto & specifier : private_data_specifiers) {
                PrivateDataSpecifierDescriptor desc(specifier);
                if (!desc.IsValid())
                    continue;
                if (desc.PrivateDataSpecifier() == PrivateDataSpecifierID::UPC1) {
                    isUPC = true;
                }
            }

            if (isUPC) {
                desc_list_t subtitles = MPEGDescriptor::FindAll(list, PrivateDescriptorID::upc_event_episode_title);
                for (auto & st : subtitles) {
                    PrivateUPCCablecomEpisodeTitleDescriptor desc(st);
                    if (!desc.IsValid())
                        continue;
                    subtitle = desc.Text();
                }
            }
        }


        QDateTime starttime = eit->StartTimeUTC(i);
        // fix starttime only if the duration is a multiple of a minute
        if (!(eit->DurationInSeconds(i) % 60))
            EITFixUp::TimeFix(starttime);
        QDateTime endtime   = starttime.addSecs(eit->DurationInSeconds(i));

        auto *event = new DBEventEIT(
            chanid,
            title,     subtitle,      description,
            category,  category_type,
            starttime, endtime,       fix,
            subtitle_type,
            audio_props,
            video_props, stars,
            seriesId,  programId,
            season, episode, totalepisodes);
        event->m_items = items;

        m_dbEvents.enqueue(event);
    }
}

// This function gets special EIT data from the German provider Premiere
// for the option channels Premiere Sport and Premiere Direkt
void EITHelper::AddEIT(const PremiereContentInformationTable *cit)
{
    // Discard event if incoming event queue full
    if (EventQueueFull())
        return;

    // set fixup for Premiere
    FixupValue fix = m_fixup.value(133 << 16);
    fix |= EITFixUp::kFixGenericDVB;

    QString title         = QString("");
    QString subtitle      = QString("");
    QString description   = QString("");
    QString category      = QString("");
    ProgramInfo::CategoryType category_type = ProgramInfo::kCategoryNone;
    unsigned char subtitle_type=0;
    unsigned char audio_props=0;
    unsigned char video_props=0;
    uint season = 0;
    uint episode = 0;
    uint totalepisodes = 0;
    QMultiMap<QString,QString> items;

    // Parse descriptors
    desc_list_t list = MPEGDescriptor::Parse(
        cit->Descriptors(), cit->DescriptorsLength());

    parse_dvb_event_descriptors(list, fix, m_languagePreferences,
                                title, subtitle, description, items);

    parse_dvb_component_descriptors(list, subtitle_type, audio_props,
                                    video_props);

    const unsigned char *content_data =
        MPEGDescriptor::Find(list, DescriptorID::content);
    if (content_data)
    {
        ContentDescriptor content(content_data);
        // fix events without real content data
        if (content.IsValid() && (content.Nibble(0)==0x00))
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
        else if (content.IsValid())
        {
            category_type = content.GetMythCategory(0);
            category      = content.GetDescription(0);
        }
        else
        {
            category_type = ProgramInfo::kCategoryNone;
            category      = "Unknown";
        }
    }

    uint tableid   = cit->TableID();
    uint version   = cit->Version();
    uint contentid = cit->ContentID();
    // fake endtime
    uint endtime   = MythDate::current().addDays(1).toSecsSinceEpoch();

    // Find Transmissions
    desc_list_t transmissions =
        MPEGDescriptor::FindAll(
            list, PrivateDescriptorID::premiere_content_transmission);
    for (auto & trans : transmissions)
    {
        PremiereContentTransmissionDescriptor transmission(trans);
        if (!transmission.IsValid())
            continue;
        uint networkid = transmission.OriginalNetworkID();
        uint tsid      = transmission.TSID();
        uint serviceid = transmission.ServiceID();

        uint chanid = GetChanID(serviceid, networkid, tsid);

        if (!chanid)
        {
            LOG(VB_EIT, LOG_INFO, LOC_ID +
                QString("Premiere EIT for NIT %1, TID %2, SID %3, "
                        "count %4, title: %5. Channel not found!")
                    .arg(networkid).arg(tsid).arg(serviceid)
                    .arg(transmission.TransmissionCount()).arg(title));
            continue;
        }

        // Skip event if we have already processed it before...
        if (!s_eitCache->IsNewEIT(chanid, tableid, version, contentid, endtime))
        {
            continue;
        }

        for (uint k=0; k<transmission.TransmissionCount(); ++k)
        {
            QDateTime txstart = transmission.StartTimeUTC(k);
            // fix txstart only if the duration is a multiple of a minute
            if (!(cit->DurationInSeconds() % 60))
                EITFixUp::TimeFix(txstart);
            QDateTime txend = txstart.addSecs(cit->DurationInSeconds());

            auto *event = new DBEventEIT(
                chanid,
                title,     subtitle,      description,
                category,  category_type,
                txstart,   txend,         fix,
                subtitle_type,
                audio_props,
                video_props, 0.0,
                "",  "",
                season, episode, totalepisodes);
            event->m_items = items;

            m_dbEvents.enqueue(event);
        }
    }
}


void EITHelper::PruneEITCache(uint timestamp)
{
    s_eitCache->PruneOldEntries(timestamp);
}

void EITHelper::WriteEITCache(void)
{
    s_eitCache->WriteToDB();
}

//////////////////////////////////////////////////////////////////////
// private methods and functions below this line                    //
//////////////////////////////////////////////////////////////////////

void EITHelper::CompleteEvent(uint atsc_major, uint atsc_minor,
                              const ATSCEvent &event,
                              const QString   &ett)
{
    // Discard event if incoming event queue full
    if (EventQueueFull())
        return;

    uint chanid = GetChanID(atsc_major, atsc_minor);
    if (!chanid)
        return;

    QDateTime starttime = MythDate::fromSecsSinceEpoch(
        event.m_startTime + GPS_EPOCH + m_gpsOffset);

    // fix starttime only if the duration is a multiple of a minute
    if (!(event.m_length % 60))
        EITFixUp::TimeFix(starttime);
    QDateTime endtime = starttime.addSecs(event.m_length);

    desc_list_t list = MPEGDescriptor::Parse(event.m_desc, event.m_descLength);
    unsigned char subtitle_type =
        MPEGDescriptor::Find(list, DescriptorID::caption_service) ?
        SUB_HARDHEAR : SUB_UNKNOWN;
    unsigned char audio_properties = AUD_UNKNOWN;
    unsigned char video_properties = VID_UNKNOWN;

    uint atsc_key = (atsc_major << 16) | atsc_minor;

    QMutexLocker locker(&m_eitListLock);
    QString title = event.m_title;
    const QString& subtitle = ett;
    m_dbEvents.enqueue(new DBEventEIT(chanid, title, subtitle,
                                      starttime, endtime,
                                      m_fixup.value(atsc_key), subtitle_type,
                                      audio_properties, video_properties));
}

uint EITHelper::GetChanID(uint atsc_major, uint atsc_minor)
{
    uint sourceid = m_sourceid;
    if (sourceid == 0)
        return 0;

    uint64_t key  = sourceid;
    key |= ((uint64_t) atsc_minor) << 16;
    key |= ((uint64_t) atsc_major) << 32;

    ServiceToChanID::const_iterator it = m_srvToChanid.constFind(key);
    if (it != m_srvToChanid.constEnd())
        return *it;

    uint chanid = get_chan_id_from_db_atsc(sourceid, atsc_major, atsc_minor);
    m_srvToChanid[key] = chanid;

    return chanid;
}

uint EITHelper::GetChanID(uint serviceid, uint networkid, uint tsid)
{
    uint sourceid = m_sourceid;
    if (sourceid == 0)
        return 0;

    uint64_t key  = sourceid;
    key |= ((uint64_t) serviceid) << 16;
    key |= ((uint64_t) networkid) << 32;
    key |= ((uint64_t) tsid)      << 48;

    ServiceToChanID::const_iterator it = m_srvToChanid.constFind(key);
    if (it != m_srvToChanid.constEnd())
        return *it;

    uint chanid = get_chan_id_from_db_dvb(sourceid, serviceid, networkid, tsid);
    m_srvToChanid[key] = chanid;

    return chanid;
}

uint EITHelper::GetChanID(uint program_number)
{
    uint sourceid = m_sourceid;
    if (sourceid == 0)
        return 0;

    uint64_t key  = sourceid;
    key |= ((uint64_t) program_number) << 16;
    key |= ((uint64_t) m_channelid)    << 32;

    ServiceToChanID::const_iterator it = m_srvToChanid.constFind(key);
    if (it != m_srvToChanid.constEnd())
        return *it;

    uint chanid = get_chan_id_from_db_dtv(sourceid, program_number, m_channelid);
    m_srvToChanid[key] = chanid;

    return chanid;
}

static uint get_chan_id_from_db_atsc(uint sourceid,
                                     uint atsc_major, uint atsc_minor)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
            "SELECT chanid, useonairguide "
            "FROM channel "
            "WHERE deleted         IS NULL AND "
            "      atsc_major_chan = :MAJORCHAN AND "
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
    MSqlQuery query(MSqlQuery::InitCon());

    // DVB Link to chanid
    QString qstr =
        "SELECT chanid, useonairguide "
        "FROM channel, dtv_multiplex "
        "WHERE deleted          IS NULL        AND "
        "      serviceid        = :SERVICEID   AND "
        "      networkid        = :NETWORKID   AND "
        "      transportid      = :TRANSPORTID AND "
        "      channel.sourceid = :SOURCEID    AND "
        "      channel.mplexid  = dtv_multiplex.mplexid";

    query.prepare(qstr);
    query.bindValue(":SERVICEID",   serviceid);
    query.bindValue(":NETWORKID",   networkid);
    query.bindValue(":TRANSPORTID", transportid);
    query.bindValue(":SOURCEID",    sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Looking up chanID", query);
        return 0;
    }

    if (query.size() > 1)
    {
        LOG(VB_EIT, LOG_ERR, LOC +
            QString("Found %1 channels for sourceid %1 networkid %2 "
                    "transportid %3 serviceid %4 but only one expected")
                .arg(query.size())
                .arg(sourceid).arg(networkid).arg(transportid).arg(serviceid));
    }

    while (query.next())
    {
        uint chanid        = query.value(0).toUInt();
        bool useOnAirGuide = query.value(1).toBool();
        return useOnAirGuide ? chanid : 0;
    }

    // EIT information for channels that are not present such as encrypted
    // channels when only FTA channels are selected etc.
    LOG(VB_EIT, LOG_DEBUG, LOC +
        QString("No channel found for sourceid %1 networkid %2 "
                "transportid %3 serviceid %4")
            .arg(sourceid).arg(networkid).arg(transportid).arg(serviceid));

    return 0;
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
    uint db_sourceid = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    // DVB Link to chanid
    QString qstr =
        "SELECT c1.chanid, c1.useonairguide, c1.sourceid "
        "FROM channel c1, dtv_multiplex m, channel c2 "
        "WHERE c1.deleted IS NULL AND "
        "      c1.serviceid = :SERVICEID   AND "
        "      c1.mplexid  = m.mplexid AND "
        "      m.mplexid = c2.mplexid AND "
        "      c2.chanid = :CHANID";

    query.prepare(qstr);
    query.bindValue(":SERVICEID",   serviceid);
    query.bindValue(":CHANID", tunedchanid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Looking up chanID", query);
        return 0;
    }

    while (query.next())
    {
        // Check to see if we are interested in this channel
        uint chanid        = query.value(0).toUInt();
        bool useOnAirGuide = query.value(1).toBool();
        db_sourceid        = query.value(2).toUInt();
        if (sourceid == db_sourceid)
            return useOnAirGuide ? chanid : 0;
    }

    if (query.size() > 0)
    {
        LOG(VB_EIT, LOG_DEBUG,
            LOC + QString("Found %1 channels for multiplex of chanid %2, "
                          "serviceid %3, sourceid %4 in database but none "
                          "for current sourceid %5.")
                .arg(query.size()).arg(tunedchanid)
                .arg(serviceid).arg(db_sourceid).arg(sourceid));
    }

    return 0;
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
    for (int i = 2061; i <= 2069; ++i)
       fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2071LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2076LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    fix[ 2081LL << 32 | 2U << 16] = EITFixUp::kFixUK;
    for (int i = 2089; i <= 2116; ++i)
       fix[ (long long)i << 32 | 2U << 16] = EITFixUp::kFixUK;
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
        EITFixUp::kFixRTL;

    // Mark HD+ channels as HDTV
    fix[   1041LL << 32 |  1 << 16] = EITFixUp::kFixHDTV;
    fix[   1055LL << 32 |  1 << 16] = EITFixUp::kFixHDTV;
    fix[   1057LL << 32 |  1 << 16] = EITFixUp::kFixHDTV;
    fix[   1109LL << 32 |  1 << 16] = EITFixUp::kFixHDTV;

    // PRO7/SAT.1 DVB-S
    fix[   1017LL << 32 |    1 << 16] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1; // DVB-S ProSiebenSat.1 Austria transponder
    fix[   1031LL << 32 |    1 << 16 | 5300] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1; // SAT.1 HD Austria
    fix[   1031LL << 32 |    1 << 16 | 5301] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1; // ProSieben HD Austria
    fix[   1031LL << 32 |    1 << 16 | 5302] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1; // kabel eins HD Austria
    fix[   1031LL << 32 |    1 << 16 | 5303] = EITFixUp::kFixHDTV | EITFixUp::kFixP7S1; // PULS 4 HD Austria
    fix[   1031LL << 32 |    1 << 16 | 5310] = EITFixUp::kFixP7S1; // SAT.1 Gold Austria
    fix[   1031LL << 32 |    1 << 16 | 5311] = EITFixUp::kFixP7S1; // Pro7 MAXX Austria
    fix[   1107LL << 32 |    1 << 16] = EITFixUp::kFixP7S1; // DVB-S ProSiebenSat.1 Germany transponder
    fix[   1082LL << 32 |    1 << 16] = EITFixUp::kFixP7S1; // DVB-S ProSiebenSat.1 Switzerland transponder
    fix[      5LL << 32 |  133 << 16 |   776] = EITFixUp::kFixP7S1; // DVB-S ProSiebenSat.1 Germany transponder

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

    // DVB-C Vodafone Germany
    fix[ 10000LL<<32 | 61441U << 16 | 53626 ] = EITFixUp::kFixP7S1; // SAT.1
    fix[ 10006LL<<32 | 61441U << 16 | 50019 ] = EITFixUp::kFixP7S1; // sixx HD
    fix[ 10008LL<<32 | 61441U << 16 | 53621 ] = EITFixUp::kFixP7S1; // ProSieben
    fix[ 10008LL<<32 | 61441U << 16 | 53622 ] = EITFixUp::kFixP7S1; // kabel eins
    fix[ 10008LL<<32 | 61441U << 16 | 50700 ] = EITFixUp::kFixP7S1; // sixx
    fix[ 10011LL<<32 | 61441U << 16 | 50056 ] = EITFixUp::kFixP7S1; // kabel eins CLASSICS HD
    fix[ 10011LL<<32 | 61441U << 16 | 50058 ] = EITFixUp::kFixP7S1; // SAT.1 emotions HD
    fix[ 10013LL<<32 | 61441U << 16 | 50015 ] = EITFixUp::kFixP7S1; // ProSieben HD
    fix[ 10013LL<<32 | 61441U << 16 | 50057 ] = EITFixUp::kFixP7S1; // ProSieben FUN HD
    fix[ 10013LL<<32 | 61441U << 16 | 50086 ] = EITFixUp::kFixP7S1; // Kabel eins Doku HD
    fix[ 10014LL<<32 | 61441U << 16 | 50086 ] = EITFixUp::kFixP7S1; // kabel eins HD
    fix[ 10017LL<<32 | 61441U << 16 | 50122 ] = EITFixUp::kFixP7S1; // kabel eins Doku
    fix[ 10017LL<<32 | 61441U << 16 | 53009 ] = EITFixUp::kFixP7S1; // ProSieben MAXX
    fix[ 10017LL<<32 | 61441U << 16 | 53324 ] = EITFixUp::kFixP7S1; // SAT.1 Gold
    fix[ 10019LL<<32 | 61441U << 16 | 50018 ] = EITFixUp::kFixP7S1; // SAT.1 HD
    fix[ 10020LL<<32 | 61441U << 16 | 50046 ] = EITFixUp::kFixP7S1; // ProSieben MAXX HD
    fix[ 10020LL<<32 | 61441U << 16 | 50074 ] = EITFixUp::kFixP7S1; // SAT.1 Gold HD

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
        EITFixUp::kEFixForceISO8859_7 |
        EITFixUp::kFixGreekEIT |
        EITFixUp::kFixGreekCategories;
    fix[    2LL << 32 |  8492LL << 16 ] = // N1,Nplus,NHD,Vouli
        EITFixUp::kEFixForceISO8859_7 |   // it is encoded in cp-1253
        EITFixUp::kFixGreekSubtitle |     // Subtitle has too much info and is
        EITFixUp::kFixGreekEIT |              // cut in db. Will move to descr.
        EITFixUp::kFixGreekCategories;

    //DVB-S & S2 Greek Nova Provider
    // Hotbird 11823H DVB-S
    fix[ 5500LL << 32 |  318LL << 16 ] = EITFixUp::kEFixForceISO8859_7;
    // Hotbird 11938H DVB-S
    fix[ 6100LL << 32 |  318LL << 16 ] = EITFixUp::kEFixForceISO8859_7;
    // Hotbird 12130H DVB-S2
    fix[ 7100LL << 32 |  318LL << 16 ] = EITFixUp::kEFixForceISO8859_7;
    // Hotbird 12169H DVB-S2
    fix[ 7300LL << 32 |  318LL << 16 ] = EITFixUp::kEFixForceISO8859_7;

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

    // Eutelsat Satellite System at 7°E
    fix[ 126U << 16 ] = EITFixUp::kEFixForceISO8859_9;
}

/** \fn EITHelper::RescheduleRecordings(void)
 *  \brief Tells scheduler about programming changes.
 *
 */
void EITHelper::RescheduleRecordings(void)
{
    ScheduledRecording::RescheduleMatch(
        0, m_sourceid, m_seenEITother ? 0 : ChannelUtil::GetMplexID(m_channelid),
        m_maxStarttime, "EITScanner");
    m_seenEITother = false;
    m_maxStarttime = QDateTime();
}
