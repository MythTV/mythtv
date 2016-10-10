#include "eitcachedvb.h"
#include "mpeg/dvbtables.h"
#include "mythlogging.h"

// Qt includes
#include <QDateTime>
#include <QStringList>

/* Extract from ETSI ETR 211
 * 
 * 4.1.4 Event Information Table (EIT) information
 * 
 * The Event Information Table (EIT) is used to transmit information
 * about present, following and furtherfuture events. For each service
 * a separate EIT sub-table exists.
 * 
 * 4.1.4.1 EIT Present/Following information
 * 
 * The following rule simplifies the acquisition of the EIT
 * Present/Following information. The SI specification states that an
 * EIT section has a maximum size of 4096 bytes.
 * 
 * The SI bit stream shall have two sections per service for an EIT
 * Present/Following with the section_number 0x00 reserved for the
 * description of the present event and section_number 0x01 for the
 * following event. These constraints do not apply in the case of an
 * NVOD reference service which may have more than one event
 * description per section, and may have more than two sections in the
 * EIT Present/Following. It is recommended that the event descriptions
 * be given in ascending order of event_id.
 * 
 * The SI bit stream shall have maximum of 4 096 bytes to describe a
 * single event in a section.
 * 
 * The organization of the EIT Present/Following is based on the
 * concept of present and following events. Which event is the present
 * one can be determined using the following scheme:
 * 
 * a) at each instant in time, there is at most one present event;
 * 
 * b) when there is a present event, this event shall be described in
 * section 0 of the EIT Present/Following;
 * 
 * c) when there is no present event (e.g. in the case of a gap in the
 * schedule) an empty section 0 of the EIT Present/Following shall be
 * transmitted;
 * 
 * d) the running_status field in the description of the present event
 * shall be given the interpretation in table 1:
 * 
 * Table 1: running_status of the present event
 * 
 * UNDEFINED - No information except the nominal status is provided.
 * IRDs and VCRs shall treat the present event as running.
 * 
 * RUNNING - IRDs and VCRs shall treat the present event as running.
 * 
 * NOT RUNNING - IRDs and VCRs shall treat the present event as not
 * running. In other words, this event is nominally the present one,
 * but at this time has either not started or already ended.
 * 
 * PAUSING - IRDs and VCRs shall treat the present event as pausing. In
 * other words, this event is nominally the present one and has already
 * started, but at this time the material being broadcast is not a part
 * of the event itself. The transmission of event material shall resume
 * at a later time.
 * 
 * STARTS In A FEW SECONDS - IRDs and VCRs shall prepare for the
 * change of event status to "running" in a few seconds.
 * 
 * The duration of an event as encoded in the field duration of the EIT
 * shall also include the duration of all times when the event has the
 * status "not running" or "paused". The start time of an event as
 * encoded in the field start_time of the EIT shall be the start time
 * of the entire event, i.e. not the
 * start time after the pause has finished;
 * 
 * e) at each point in time, there shall be at most one following event;
 * 
 * f) if a following event exists, it shall be described in section 1
 * of the EIT Present/Following;
 * 
 * g) if no following event exists, an empty section 1 of the EIT
 * Present/Following shall be transmitted;
 * 
 * h) the running_status field in the definition of the following
 * event shall be given the following interpretation:
 * 
 * Table 2: running_status of the following event
 * 
 * UNDEFINED - No information except the nominal status is provided.
 * IRDs and VCRs shall treat the following event as not running.
 * 
 * RUNNING - Not allowed.
 * 
 * NOT RUNNING - IRDs and VCRs shall treat the present event as not
 * running.
 * 
 * PAUSING - This status is intended to indicate that the "following"
 * event has been running at some time, but is now overlapped by
 * another event. In such a case, during the whole time that the
 * "following" event has status "pausing", one and the same
 * overlapping event shall be encoded in section 0 of the EIT
 * Present/Following. Furthermore, an event which has the status
 * "pausing" shall acquire the status "running" at a later time, then
 * replacing the overlapping event in section 0 of the EIT
 * Present/Following.
 * 
 * STARTS IN A FEW SECONDS - IRDs and VCRs shall prepare for the
 * status of the following event to change to running within a few
 * seconds.
 * 
 * The duration of an event as encoded in the field duration of the
 * EIT shall also include the duration of all times when the event has
 * the status "not running" or "paused". The start time of an event as
 * encoded in the field start_time of the EIT shall be the start time
 * of the entire event, i.e. not the start time after the pause has
 * finished.
 * 
 * NOTE 1: The start time of one event plus its duration may be
 * smaller than the start time of the following event. In other words,
 * gaps between events are allowed. In such a case, the following event
 * is considered to be the event scheduled to begin after the gap. This
 * event shall be encoded in section 1 of the EIT Present/Following.
 * 
 * NOTE 2: The start time and duration are scheduled times. Some
 * broadcasters may update this information if the schedule is running
 * late, whereas others may prefer to keep the indicated start time
 * unchanged, e.g. to avoid having an event called "The News at 8" from
 * being indicated as starting at 8:01:23, instead of 8:00:00.
 * 
 * 4.1.4.2 EIT Schedule information
 * 
 * 4.1.4.2.1 EIT Schedule structure
 * 
 * The EIT Schedule information is structured in such a way that it is
 * easy to access the EIT data in a flexible manner. The EIT Schedule
 * Tables shall obey the following rules:
 * 
 * a) the EIT/Schedule is distributed over 16 table_ids, being
 * 0x50 - 0x5F for the actual TS, and 0x60 - 0x6F for other TSs, which
 * are ordered chronologically;
 * 
 * b) the 256 sections under each sub-table are divided into 32
 * segments of 8 sections each. Segment #0, thus, comprises sections
 * 0 to 7, segment #1 section 8 to 15 etc.;
 * 
 * c) each segment contains information about events that start
 * (see below) anywhere within a three-hour period;
 * 
 * d) the information about separate events is ordered chronologically
 * within segments;
 * 
 * e) if only n < 8 sections of a segment are used, the information
 * shall be placed in the first n sections of the segment. To signal
 * that the last sections of the segment are not used, the value
 * s0 + n - 1, where s0 is the first section number of the segment,
 * shall be encoded in the field segment_last_section_number of the EIT
 * header. As an example, if segment 2 contains only 2 sections, the
 * field segment_last_section_number shall contain the value
 * 8 + 2 - 1 = 9 in those two sections;
 * 
 * f) segments that contain all their sections shall have the value
 * s0 + 7 encoded in the field segment_last_section_number;
 * 
 * g) entirely empty segments shall be represented by an empty section,
 * (i.e. a section which does not contain any loop over events) with
 * the value s0 + 0 encoded in the field segment_last_section_number;
 * 
 * h) the placing of events in segments is done referring to a time t0.
 * t0 is "last midnight" in Universal Time Coordinated (UTC) time.
 * Suppose, for instance, that it is 5.00 PM in the time zone UTC-6.
 * It is then 11.00 PM in the time zone UTC+0, which makes it 23 hours
 * since "last midnight". Therefore, t0 is 6.00 PM the previous day in
 * UTC-6;
 * 
 * i) segment #0 of table_id 0x50 (0x60 for other TSs) shall contain
 * information about events that start between midnight (UTC Time) and
 * 02:59:59 (UTC Time) of "today". Segment #1 shall contain events that
 * start between 03:00:00 and 05:59:59 UTC time, and so on. This means
 * that the first sub-table (table_id 0x50, or 0x60 for other TSs)
 * contains information about the first four days ofschedule, starting
 * today at midnight UTC time;
 * 
 * j) the field last_section_number is used to indicate the end of the
 * sub-table. Empty segments that fall outside the section range
 * indicated by last_section_number shall not be represented by empty
 * sections;
 * 
 * k) the field last_table_id is used to indicate the end of the entire
 * EIT/Schedule structure. Emptysegments that fall outside the table_id
 * range indicated by last_table_id shall not be represented by empty
 * sections;
 * 
 * l) segments that correspond to events in the past may be replaced
 * by empty segments (see rule g));
 * 
 * m) the running_status field of event definitions contained in the
 * EIT/Schedule shall be set to undefined (0x00);
 * 
 * n) EIT/Schedule tables are not applicable to NVOD Reference
 * Services, since these have events with undefined start times.
 * 
 * 4.1.4.2.2 EIT scrambling
 * 
 * The EIT Schedule Tables may be scrambled. In order to provide an
 * association with the Conditional Access (CA) streams, it is
 * necessary to allocate a service_id (= MPEG-2 program_number) which
 * is used in the Program Specific Information (PSI) to describe
 * scrambled EIT Schedule Tables. The EIT is identified in the Program
 * Map Table (PMT) section for this service_id as a programme
 * consisting of one private stream, and this PMT section includes one
 * or more CA_descriptors to identify the associated CA streams. The
 * service_id value 0xFFFF is reserved in DVB applications for this
 * purpose.
 */

#define LOC QString("EitCacheDVB: ")

static QStringList RSTNames(QStringList() << "Undefined"
                                << "not running"
                                << "starts in a few seconds"
                                << "pausing"
                                << "running"
                                << "service off-air"
                                << "reserved"
                                << "reserved");


bool EitCacheDVB::ProcessSection(const DVBEventInformationTable *eit,
                                 bool &section_version_changed)
{
    // Single thread this function
    QMutexLocker lock(&tableLock);
    
    long long original_network_id = eit->OriginalNetworkID();
    uint transport_stream_id = eit->TSID();
    uint service_id = eit->ServiceID();
    uint table_id = eit->TableID();
    
    section_version_changed = false;

    // See if this section is for a currently tuned transport stream
    bool actual = (table_id == TableID::PF_EIT) ||
        ((table_id >= TableID::SC_EITbeg) && 
        (table_id <= TableID::SC_EITend));
        
    // See if this section is for the pf table or the schedule table
    unsigned long long key = original_network_id << 32 | transport_stream_id  << 16 | service_id;
    if ((table_id == TableID::PF_EIT) || (table_id == TableID::PF_EITo))
    {
        if (!pfTables.contains(key))
        {
            PfTable new_table;
            new_table.SetOriginalNetworkId(original_network_id);
            new_table.SetTransportStreamId(transport_stream_id);
            new_table.SetServiceId(service_id);
            pfTables.insert(key, new_table);
        }

        return pfTables[key].ProcessSection(eit, actual, section_version_changed);
    }
    else if (((table_id >= TableID::SC_EITbeg) &&
            (table_id <= TableID::SC_EITend)) ||
            ((table_id >= TableID::SC_EITbego) &&
            (table_id <= TableID::SC_EITendo)))
    {
        if (!scheduleTables.contains(key))
        {
            ScheduleTable new_table;
            new_table.SetOriginalNetworkId(original_network_id);
            new_table.SetTransportStreamId(transport_stream_id);
            new_table.SetServiceId(service_id);
            scheduleTables.insert(key, new_table);
        }
        
        return scheduleTables[key].ProcessSection(eit, actual, section_version_changed);
    }
    else if (table_id == TableID::RST)
    {
        // The long lost running status table has finally been found
        //  send for the doctor. "Doctor Who?". "Of Course!".
        LOG(VB_EITRS, LOG_INFO, LOC + QString(
                "The long lost running status table has finally been found, "
                "send for the doctor. \"Doctor Who?\". \"Of Course!\"."));
        return true;
    }
    else
        return false;
}

bool EitCacheDVB::PfTable::ValidateEventTimes(
                const DVBEventInformationTable *eit, 
                uint event_count,
                uint section_number)
{
    if (event_count > 0)
    {
        time_t current_time = QDateTime::currentDateTimeUtc()
            .toTime_t();
        time_t start_time = eit->StartTimeUnixUTC(0);
        time_t end_time = eit->EndTimeUnixUTC(0);
        if (0 == section_number)
        {
            // "Present" section (0)
            // Event (0) should span the current time
            if ((start_time > current_time) || (end_time < current_time))
            {
                LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                    "Present event does not span current time %1 %2 %3")
                    .arg(QDateTime::fromTime_t(start_time)
                            .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(end_time)
                            .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(current_time)
                            .toString(Qt::SystemLocaleShortDate)));
                return false;
            }
        }
        else
        {
            // "Following" section (1) 
            // If Event (0) does not have the running_status
            // "Paused" it should be in the future.
            if (RunningStatusEnum::PAUSING
                        == RunningStatusEnum(eit->RunningStatus(0)))
            {
                // Event is paused it should start some time in the
                // past and finish some time in the future
                if ((start_time >= current_time) ||
                        (end_time <= current_time))
                {
                    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                        "Paused following event must "
                        "start some time in the past and finish "
                        "some time in the future %1 %2 %3")
                    .arg(QDateTime::fromTime_t(start_time)
                            .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(end_time)
                            .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(current_time)
                            .toString(Qt::SystemLocaleShortDate)));
                    return false;
                }
            }
            else
            {
                // Event must be in the the future
                if (start_time < current_time)
                {
                    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                        "Following event must be in the "
                        "future %1 %2 %3")
                    .arg(QDateTime::fromTime_t(start_time)
                            .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(end_time)
                            .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(current_time)
                            .toString(Qt::SystemLocaleShortDate)));
                    return false;
                }
            }
        }
    }
    return true;
}

bool EitCacheDVB::PfTable::ProcessSection(
                const DVBEventInformationTable *eit, const bool actual,
                bool &section_version_changed)
{
    // Validate against ETSI EN 300 468 V1.11.1 and
    // and ETSI ETR 211 second edition
    
    section_version_changed = false;

    // Basic checks
    uint table_id = eit->TableID();
    uint section_number = eit->Section();
    uint version_number = eit->Version();
    uint last_table_id = eit->LastTableID();
    uint segment_last_section_number = eit->SegmentLastSectionNumber();
    uint last_section_number = eit->LastSection();
    uint event_count = eit->EventCount();
    
    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString("Processing present"
                        "/following section "
                        "original_network_id %1 "
                        "transport_stream_id %2 "
                        "service_id %3 "
                        "table_id %4 "
                        "section_number %5 "
                        "version_number %6 "
                        "last_table_id %7 "
                        "segment_last_section_number %8 "
                        "last_section_number %9 "
                        "event_count %10 "
                        "actual %11")
                        .arg(original_network_id)
                        .arg(transport_stream_id)
                        .arg(service_id)
                        .arg(table_id)
                        .arg(section_number)
                        .arg(version_number)
                        .arg(last_table_id)
                        .arg(segment_last_section_number)
                        .arg(last_section_number)
                        .arg(event_count)
                        .arg(actual));

        
    if ((section_number > 1) || (last_section_number > 1) ||
            (event_count > 1) || (section_number > last_section_number)
            || (segment_last_section_number != last_section_number))
    {
        LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                "Bad section %1 or last section %2"
                " or event_count %3 - actual %4")
                .arg(section_number)
                .arg(last_section_number)
                .arg(event_count)
                .arg(actual));
        return false;
    }
    
    PfEvent& pfEvent  =  (0 == section_number) ? present : following;

    // Check the version number against the current table if any
    if (table_status == TableStatusEnum::UNINITIALISED)
    {
        // No previous data, initialise the table if
        // the event is valid.
        if (!ValidateEventTimes(eit, event_count, section_number))
        {
            LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                "Table uninitialised but bad event times - reject"));
            return false;
        }
        LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
            "Table uninitialised - accepting "
            "incoming data"));
    }
    else
    {
        // Previous table version exists
        // Check on the status of this section
        if (table_version == VERSION_UNINITIALISED)
        {
            // This should not happen :-)
            LOG(VB_EITDVBPF, LOG_ERR, LOC + QString(
                "Table version uninitialized"));
            return false;            
        }

        if (pfEvent.event_status == TableStatusEnum::UNINITIALISED)
        {
            // No previous event section - I will initialise the version number
            // from this section provided the event times (if present) are
            // reasonable.
            if (!ValidateEventTimes(eit, event_count, section_number))
            {
                LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                    "Event uninitialised but bad event times - reject"));
                return false;
            }
            LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                "Event uninitialised - accepting "
                "incoming data"));
        }
        else
        {
            // Something to check against
            if ((table_version + 1) % 32 == version_number)
            {
                // Ignore if event times are invalid
                if (!ValidateEventTimes(eit, event_count, section_number))
                    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                        "PF potential next version but bad "
                        "event times - reject"));
                    return false;
                LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                    "PF potential next version invalidate everything "
                    "and accept"));
                // Invalidate everything it will be set right later
                table_status = TableStatusEnum::UNINITIALISED;
                present.event_status = TableStatusEnum::UNINITIALISED;
                present.section_version = VERSION_UNINITIALISED;
                following.event_status = TableStatusEnum::UNINITIALISED;
                following.section_version = VERSION_UNINITIALISED;
            }
            else
            {
                // It is either a duplicate or a discontinuity
                if (table_version == version_number)
                {
                    // Possible duplicate
                    if (event_count == 0)
                    {
                        // Same version number no incoming events
                        if (pfEvent.event_status == TableStatusEnum::EMPTY)
                        {
                            // No incoming events 
                            // table valid but empty
                            LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                                "PF table same version no incoming "
                                "and no stored events present - reject duplicate"));
                            return false;
                        }
                        // At this point I have the same version number,
                        // no events in the incoming section,
                        // some events in the stored section.
                        if (!actual)
                        {
                            // TODO I am not sure if this is a bit harsh
                            // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                            // On the "other" stream reject as duplicate
                            LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                                "PF table same version no incoming events "
                            "not on actual stream - reject"));
                            return false;
                        }
                        // On the "actual" stream accept
                        // the incoming section as a wrap around
                        // discontinuity.
                        // Invalidate verything it will be set right later
                        LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                            "PF table same version no incoming events "
                        "on actual stream - accept"));
                        table_status = TableStatusEnum::UNINITIALISED;
                        present.event_status = TableStatusEnum::UNINITIALISED;
                        present.section_version = VERSION_UNINITIALISED;
                        following.event_status = TableStatusEnum::UNINITIALISED;
                        following.section_version = VERSION_UNINITIALISED;
                    }
                    else
                    {
                        // Same version number some incoming events
                        if (pfEvent.event_status == TableStatusEnum::VALID)
                        {
                            // Some events to compare
                            if (PfEvent(eit->EventID(0),
                                    eit->StartTimeUnixUTC(0),
                                    eit->EndTimeUnixUTC(0),
                                    RunningStatus(eit->RunningStatus(0)),
                                    eit->IsScrambled(0),
                                    TableStatusEnum::VALID) ==
                                    pfEvent)
                            {
                                LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                                    "PF table same version some incoming "
                                    "events - duplicate"));
                                return false;
                            }
                        }
                        // Ignore if event times are invalid
                        if (!ValidateEventTimes(eit, event_count,
                                                section_number))
                        {
                            LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                                "PF table incoming "
                                "events invalid times"));
                            return false;
                        }
                        // Some data is better than no data
                        // fall through
                        LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                                    "Some data is better than no data"));
                    }
                }
                else
                {
                    // Discontinuity
                    if (!ValidateEventTimes(eit, event_count,
                                            section_number))
                    {
                        LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                            "PF table discontinuity incoming "
                            "events invalid times"));
                        return false;
                    }
                    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                        "PF table discontinuity accepted"));
                }
            }
        }
    }
     // Section has passed all the checks
    bool version_change = table_version != version_number;
    table_version = version_number;
    if (pfEvent.section_version != version_number)
        section_version_changed = true;
    pfEvent.section_version = version_number;
    uint  event_id = eit->EventID(0);
    time_t start_time = eit->StartTimeUnixUTC(0);
    time_t end_time = eit->EndTimeUnixUTC(0);
    RunningStatusEnum running_status = RunningStatusEnum(eit->RunningStatus(0));
    
    if (running_status != pfEvent.running_status)
    {
        LOG(VB_EITRS, LOG_INFO, LOC + QString(
                        "Running status for %1/%2/%3/%4 change in PD %5 data - "
                        "was %6 is %7")
                .arg(original_network_id)
                .arg(transport_stream_id)
                .arg(service_id)
                .arg(event_id)
                .arg(section_number == 0 ? "present" : "following")
                .arg(RSTNames[uint(pfEvent.running_status)])
                .arg(RSTNames[uint(running_status)]));
    }
    uint is_scrambled = eit->IsScrambled(0);
    
    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
            "EIT PF change - "
            "original_network_id %1 "
            "transport_stream_id %2 "
            "service_id %3 "
            "section_number %4 "
            "version_number %5%6 "
            "event_count %7 "
            "event_id %8 "
            "start_time %9%10 "
            "end_time %11%12 "
            "running_status %13%14 "
            "is_scrambled %15 "
            "actual %16")
            .arg(original_network_id)
            .arg(transport_stream_id)
            .arg(service_id)
            .arg(section_number)
            .arg(version_number).arg(version_change ? "*" : "")
            .arg(event_count)
            .arg(event_id)
            .arg(QDateTime::fromTime_t(start_time)
                .toString(Qt::SystemLocaleShortDate))
                .arg(start_time != pfEvent.start_time ? "*" : "")
            .arg(QDateTime::fromTime_t(end_time)
                .toString(Qt::SystemLocaleShortDate))
                .arg(end_time != pfEvent.end_time ? "*" : "")
            .arg(RSTNames[uint(running_status)]).arg(
                running_status != pfEvent.running_status ? "*" : "")
            .arg(is_scrambled)
            .arg(actual));

    table_status = TableStatusEnum::VALID;
    pfEvent = PfEvent(event_id,
                        start_time,
                        end_time,
                        running_status,
                        is_scrambled,
                        (event_count > 0) ?
                            TableStatusEnum::VALID :
                            TableStatusEnum::EMPTY);
    
    return true;
}


bool EitCacheDVB::ScheduleTable::ValidateEventStartTimes(
                const DVBEventInformationTable *eit, 
                uint event_count,
                uint section_number,
                bool actual)
{
    static const uint SECONDS_IN_A_DAY = 86400;
    static const uint SECONDS_IN_A_FULL_SUB_TABLE = 345600; // 96 hours
    static const uint SECONDS_IN_A_SEGMENT = 10800; // 3 hours
    
    
    // Calculate the time span for this section
    uint last_midnight = (QDateTime::currentDateTimeUtc().toTime_t()
                    / SECONDS_IN_A_DAY) * SECONDS_IN_A_DAY;
    uint sub_table_index = eit->TableID() - (actual ? 0x50 : 0x60);
    uint segment_index = section_number / 8;
    uint sub_table_start_time = last_midnight
                                + (SECONDS_IN_A_FULL_SUB_TABLE 
                                * sub_table_index);
    uint start_time_span = sub_table_start_time
                            + (SECONDS_IN_A_SEGMENT * segment_index);
    uint end_time_span = sub_table_start_time
                        + (SECONDS_IN_A_SEGMENT
                        * (segment_index + 1)) - 1;
    
    for (uint ievent = 0; ievent < event_count; ievent++)
    {
        time_t start_time = eit->StartTimeUnixUTC(ievent);
        if ((start_time < start_time_span)
            || (start_time > end_time_span))
            {
                LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                    "Schedule table event %1 failed time span check "
                    "last_midnight %2 "
                    "sub_table_index %3 "
                    "segment_index %4 "
                    "sub_table_start_time %5 "
                    "start_time_span %6 "
                    "end_time_span %7 "
                    "start_time %8")
                    .arg(eit->EventID(ievent))
                    .arg(last_midnight)
                    .arg(sub_table_index)
                    .arg(segment_index)
                    .arg(QDateTime::fromTime_t(sub_table_start_time)
                        .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(start_time_span)
                        .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(end_time_span)
                        .toString(Qt::SystemLocaleShortDate))
                    .arg(QDateTime::fromTime_t(start_time)
                        .toString(Qt::SystemLocaleShortDate)));
                return false;
            }
        }

    return true;
}

void EitCacheDVB::ScheduleTable::InvalidateEverything()
{
    
    subtable_count = 0;
    
    for (uint isubtable = 0; isubtable < 8; isubtable++)
    {
        SubTable& subtable = subtables[isubtable];
        subtable.subtable_status = TableStatusEnum::UNINITIALISED;
        for (uint isegment = 0; isegment < 32; isegment++)
        {
            Segment& segment = subtable.segments[isegment];
            segment.section_count = 0;
            for (uint isection = 0; isection < 8; isection++)
            {
                Section& section = segment.sections[isection];
                section.section_status = TableStatusEnum::UNINITIALISED;
                section.section_version = VERSION_UNINITIALISED;
            }
        }
    }
}

bool EitCacheDVB::ScheduleTable::ProcessSection(
                const DVBEventInformationTable *eit, const bool actual,
                bool &section_version_changed)
{
    // Validate against ETSI EN 300 468 V1.11.1 and
    // and ETSI ETR 211 second edition
    
    // Basic checks
    uint table_id = eit->TableID();
    uint version_number = eit->Version();
    uint section_number = eit->Section();
    uint last_section_number = eit->LastSection();
    uint segment_last_section_number = eit->SegmentLastSectionNumber();
    uint last_table_id = eit->LastTableID();
    uint event_count = eit->EventCount();
    uint subtable_index = table_id - (actual ? 0x50 : 0x60);
    uint segment_index = section_number / 8;
    uint section_index = section_number - (segment_index * 8);
    uint incoming_subtable_count = last_table_id 
                                - (actual ? 0x50 : 0x60)
                                + 1;
    uint incoming_section_count = segment_last_section_number + 1
                                - 8 * (segment_last_section_number / 8);

    SubTable& current_subtable = subtables[subtable_index];
    uint subtable_version = current_subtable.subtable_version;
    Segment& current_segment = current_subtable.segments[segment_index];
    Section& current_section = current_segment.sections[section_index];
    TableStatusEnum section_status = current_section.section_status;
    uint& current_section_version = current_section.section_version;

    LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString("Processing schedule section "
                        "original_network_id %1 "
                        "transport_stream_id %2 "
                        "service_id %3 "
                        "table_id %4 "
                        "section_number %5 "
                        "version_number %6 "
                        "last_table_id %7 "
                        "segment_last_section_number %8 "
                        "last_section_number %9 "
                        "subtable_index %10 "
                        "segment_index %11 "
                        "section_index %12 "
                        "subtable_count %13 "
                        "section_count %14 "
                        "event_count %15 "
                        "actual %16")
                        .arg(original_network_id)
                        .arg(transport_stream_id)
                        .arg(service_id)
                        .arg(table_id)
                        .arg(section_number)
                        .arg(version_number)
                        .arg(last_table_id)
                        .arg(segment_last_section_number)
                        .arg(last_section_number)
                        .arg(subtable_index)
                        .arg(segment_index)
                        .arg(section_index)
                        .arg(incoming_subtable_count)
                        .arg(incoming_section_count)
                        .arg(event_count)
                        .arg(actual));

    if (table_id > last_table_id)
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString("Bad table_id "
                            "table_id %1 "
                            "last_table_id %2 "
                            "segment_last_section_number %3 "
                            "last_section_number %4 "
                            "actual %5")
                            .arg(table_id)
                            .arg(last_table_id)
                            .arg(segment_last_section_number)
                            .arg(last_section_number)
                            .arg(section_number)
                            .arg(actual));
        return false;
    }
    
    
    if (segment_last_section_number > last_section_number)
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                            "Bad segment_last_section_number "
                            "table_id %1 "
                            "last_table_id %2 "
                            "segment_last_section_number %3 "
                            "last_section_number %4 "
                            "actual %5")
                            .arg(table_id)
                            .arg(last_table_id)
                            .arg(segment_last_section_number)
                            .arg(last_section_number)
                            .arg(section_number)
                            .arg(actual));
        return false;
    }
    
    if   (section_number > segment_last_section_number)
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                            "Bad section_number "
                            "table_id %1 "
                            "last_table_id %2 "
                            "segment_last_section_number %3 "
                            "last_section_number %4 "
                            "actual %5")
                            .arg(table_id)
                            .arg(last_table_id)
                            .arg(segment_last_section_number)
                            .arg(last_section_number)
                            .arg(section_number)
                            .arg(actual));
        return false;
    }
    
    
    // Check the version number against the current table if any
    if (subtables[subtable_index]
        .subtable_status == TableStatusEnum::UNINITIALISED)
    {
        // No previous data - I will initialise the subtable_version
        // number from this section provided the event times
        // (if present) are reasonable.
        if (!ValidateEventStartTimes(eit, event_count, section_number,
                                actual))
        {
            LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                "Subtable uninitialised but bad event times - reject"));
            return false;
        }
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
            "Subtable uninitialised - accepting "
            "incoming data"));
    }
    else
    {
        // Previous sub table version exists
        // Check on the status of this section
        if (subtable_version == VERSION_UNINITIALISED)
        {
            // This should not happen :-)
            LOG(VB_EITDVBSCH, LOG_ERR, LOC + QString(
                "Subtable version uninitialized"));
            return false;            
        }

        if (section_status == TableStatusEnum::UNINITIALISED)
        {
            // No previous data - I will initialise the subtable_version
            // number from this section provided the event times
            // (if present) are reasonable.
            if (!ValidateEventStartTimes(eit, event_count, section_number,
                                    actual))
            {
                LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                "Section uninitialised but bad event times - reject"));
                return false;
            }
            LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                "Section uninitialised - accepting "
                "incoming data"));
        }
        else
        {
            // Something to check against
            if ((subtable_version + 1) % 32 == version_number)
            {
                // Ignore if event times are invalid
                if (!ValidateEventStartTimes(eit, event_count, section_number,
                                        actual))
                {
                    LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                        "Schedule potential next version but bad "
                        "event times - reject"));
                    return false;
                }
                LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                    "Schedule potential next version invalidate everything "
                    "and accept"));
                // New table version
                InvalidateEverything();
            }
            else
            {
                // It is either a duplicate or a discontinuity
                if (subtable_version == version_number)
                {
                    // Possible duplicate
                    if (event_count == 0)
                    {
                        // Same version number no incoming events
                        if ((subtables[subtable_index]
                            .subtable_status == TableStatusEnum::VALID)
                            && (subtables[subtable_index]
                            .segments[segment_index]
                            .sections[section_index]
                            .section_status == TableStatusEnum::VALID)
                            && (subtables[subtable_index]
                            .segments[segment_index]
                            .sections[section_index]
                            .events.size() == 0))
                        {
                            // No incoming events and no stored ones
                            LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                                "Schedule table same version no incoming "
                                "and no stored events present - reject duplicate"));
                            return false;
                        }
                        // At this point I have the same version number,
                        // no events in the incoming section,
                        // some events in the stored section.
                        if (!actual)
                        {
                            // On the "other" stream reject as duplicate
                            LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                                "Schedule table on other stream "
                                "same version no incoming "
                                "and some stored events present - reject"));
                            return false;
                        }
                        // On the "actual" stream accept
                        // the incoming section as a wrap around
                        // discontinuity.
                        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                                "Schedule table on actual stream "
                                "same version no incoming "
                                "invalidate everything and "
                                "accept as discontinuity"));
                        InvalidateEverything();
                    }
                    else
                    {
                        // Same version number some incoming events
                        // Throwing the baby away with the bath water
                        // need to accept the section
                        // if the event table is empty

                        if ((subtables[subtable_index]
                            .subtable_status == TableStatusEnum::VALID)
                            && (subtables[subtable_index]
                            .segments[segment_index]
                            .sections[section_index]
                            .section_status == TableStatusEnum::VALID))
                        {
                            Section& current_section = 
                                    subtables[subtable_index]
                                    .segments[segment_index]
                                    .sections[section_index];
                            // Some events to compare
                            if (current_section.events.size() > 0)
                            {
                                if (event_count == uint(current_section.events.size()))
                                {
                                    uint ievent;
                                    for (ievent = 0; ievent < event_count; ievent++)
                                        if (!(Event(eit->EventID(ievent),
                                                eit->StartTimeUnixUTC(ievent),
                                                eit->EndTimeUnixUTC(ievent),
                                                RunningStatus(
                                                    eit->RunningStatus(ievent)),
                                                eit->IsScrambled(ievent)) ==
                                                current_section.events[ievent]))
                                            break;
                                    if (event_count == ievent)
                                    {
                                        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                                            "Schedule same version some"
                                            " incoming events - duplicate"));
                                        return false;
                                    }
                                }
                                else
                                {
                                    LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                                        "Schedule same version some"
                                        " incoming events but "
                                        "different number of events in "
                                        "old table. I will accept this "
                                        "but this may prove problematic"
                                        "!!!!!!!!!!!!!!!!!"));
                                }
                            }
                            else
                            {
                                LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                                    "Schedule same version some "
                                    "incoming events but no stored ones "
                                    "- reject"));
                                return false;
                            }
                            // Ignore if event times are invalid
                            if (!ValidateEventStartTimes(eit, event_count,
                                                    section_number,
                                                    actual))
                            {
                                LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                                    "Schedule same version "
                                    "but bad event times - reject"));
                                return false;
                            }
                        }
                        // Some data is better than no data
                        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                                "Some data is better than no data"));
                    }
                }
                else
                {
                    // Discontinuity
                    if (!ValidateEventStartTimes(eit, event_count,
                                            section_number,
                                            actual))
                    {
                        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                            "Schedule table discontinuity incoming "
                            "events invalid times"));
                        return false;
                    }
                    LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                        "Schedule table discontinuity accepted"));
                }
            }
        }
    }
    
    // Section has passed all the checks
    bool version_changed = false;
    bool subtable_count_changed = false;
    uint old_subtable_count;
    bool section_count_changed = false;
    uint old_section_count;
    bool event_count_changed = false;
    uint old_event_count;

    if (subtable_version != version_number)
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
            "Schedule table version number changed old %1 new %2")
            .arg(subtable_version)
            .arg(version_number));
        current_subtable.subtable_version = version_number;
        version_changed = true;
    }

    if (subtable_count != incoming_subtable_count)
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
            "Schedule table subtable_count changed old %1 new %2")
            .arg(subtable_count)
            .arg(incoming_subtable_count));
        old_subtable_count = subtable_count;
        subtable_count = incoming_subtable_count;
        subtable_count_changed = true;
    }

    current_subtable.subtable_status = TableStatusEnum::VALID;

    if (current_segment.section_count != incoming_section_count)
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
            "Schedule table section_count changed old %1 new %2")
            .arg(current_segment.section_count)
            .arg(incoming_section_count));
        old_section_count = current_segment.section_count;
        current_segment.section_count = incoming_section_count;
        section_count_changed = true;
    }

    current_section.section_status = TableStatusEnum::VALID;

    if (current_section_version != version_number)
        section_version_changed = true;
    current_section_version = version_number;

    if (uint(current_section.events.size()) != event_count )
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
            "Schedule table event_count changed old %1 new %2")
            .arg(uint(current_section.events.size()))
            .arg(event_count));
        old_event_count = uint(current_section.events.size());
        event_count_changed = true;
    }
    if (!current_section.events.isEmpty())
        current_section.events.clear();
    for (uint ievent = 0; ievent < event_count; ievent++)
        current_section.events.append(
            Event(eit->EventID(ievent),
                        eit->StartTimeUnixUTC(ievent),
                        eit->EndTimeUnixUTC(ievent),
                        RunningStatus(eit->RunningStatus(ievent)),
                        eit->IsScrambled(ievent)));
    if (!version_changed &&
            !subtable_count_changed &&
            !section_count_changed &&
            !event_count_changed)
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString("EIT WDF nothing major changed!"));

    LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString("EIT schedule table change "
                        "original_network_id %1 "
                        "transport_stream_id %2 "
                        "service_id %3 "
                        "table_id %4 "
                        "section_number %5 "
                        "version_number %6%7 "
                        "last_table_id %8 "
                        "segment_last_section_number %12 "
                        "last_section_number %13 "
                        "subtable_index %14 "
                        "segment_index %15 "
                        "section_index %16 "
                        "subtable_count %17%18 "
                        "section_count %19%20 "
                        "event_count %21%22 "
                        "actual %23")
                        .arg(original_network_id)
                        .arg(transport_stream_id)
                        .arg(service_id)
                        .arg(table_id)
                        .arg(section_number)
                        .arg(version_number)
                            .arg(version_changed ? QString("* %1")
                                .arg(subtable_version) : "")
                        .arg(last_table_id)
                        .arg(segment_last_section_number)
                        .arg(last_section_number)
                        .arg(subtable_index)
                        .arg(segment_index)
                        .arg(section_index)
                        .arg(incoming_subtable_count)
                            .arg(subtable_count_changed ? QString("* %1")
                                .arg(old_subtable_count) : "")
                        .arg(incoming_section_count)
                            .arg(section_count_changed ? QString("* %1")
                                .arg(old_section_count) : "")
                        .arg(event_count)
                            .arg(event_count_changed ? QString("* %1")
                                .arg(old_event_count) : "")
                        .arg(actual));

    return true;
}

