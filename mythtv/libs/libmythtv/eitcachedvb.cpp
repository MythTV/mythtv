
#include "eitcachedvb.h"
#include "mpeg/dvbtables.h"
#include "mythlogging.h"

// Qt includes
#include <QDateTime>

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

EitCacheDVB::PfTable::PfTable()
{
    LOG(VB_EIT, LOG_DEBUG, LOC + QString("Constructing a PfTable"));
    present.eventStatus = EventStatusEnum::UNINITIALISED;
    following.eventStatus = EventStatusEnum::UNINITIALISED;
}

bool EitCacheDVB::ProcessSection(const DVBEventInformationTable *eit)
{
    int tableid = eit->TableID();
    // See if this section is for a currently tuned transport stream
    bool actual = (tableid == TableID::PF_EIT) ||
        ((tableid >= TableID::SC_EITbeg) && 
        (tableid <= TableID::SC_EITend));

	return ProcessSection(eit, actual);
}
	
bool EitCacheDVB::ProcessSection(const DVBEventInformationTable *eit,
bool actual)
{
    // See if this section is for the pf table or the schedule table
    uint tableid = eit->TableID();
    long original_network_id = eit->OriginalNetworkID();
    uint transport_stream_id = eit->TSID();
    uint service_id = eit->ServiceID();
    long key = original_network_id << 32 | transport_stream_id  << 16 | service_id;
    if ((tableid == TableID::PF_EIT) || (tableid == TableID::PF_EITo))
    {
        if (!pfTables.contains(key))
        {
            struct PfTable new_table;
            new_table.SetOriginalNetworkId(original_network_id);
            new_table.SetTransportStreamId(transport_stream_id);
            new_table.SetServiceId(service_id);
            pfTables.insert(key, new_table);
        }

        return pfTables[key].ProcessSection(eit, actual);
    }
    else if (((tableid >= TableID::SC_EITbeg) &&
            (tableid <= TableID::SC_EITend)) ||
            ((tableid >= TableID::SC_EITbego) &&
            (tableid <= TableID::SC_EITendo)))
    {
        if (!scheduleTables.contains(key))
        {
            struct ScheduleTable new_table;
            new_table.SetOriginalNetworkId(original_network_id);
            new_table.SetTransportStreamId(transport_stream_id);
            new_table.SetServiceId(service_id);
            scheduleTables.insert(key, new_table);
        }
        
        return scheduleTables[key].ProcessSection(eit, actual);
    }
    else
        return false;
}

bool EitCacheDVB::PfTable::ProcessSection(
                const DVBEventInformationTable *eit, const bool actual)
{
    // Validate against ETSI EN 300 468 V1.11.1 and
    // and ETSI ETR 211 second edition
    
    // Basic checks
    uint version_number = eit->Version();
    uint section_number = eit->Section();
    uint last_section_number = eit->LastSection();
    uint event_count = eit->EventCount();
    struct Event *pfEvent;
    EventStatus candidateEventStatus = EventStatusEnum::UNINITIALISED;
    
    LOG(VB_EIT, LOG_DEBUG, LOC + QString(
        "Processing Table %1/%2/%3")
        .arg(original_network_id)
        .arg(transport_stream_id)
        .arg(service_id));
        
    if ((section_number > 1) || (last_section_number > 1) ||
            (event_count > 1))
    {
        // TODO - more stringent checking of the last_section_number
        // based on the incoming section_number
        LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                "Bad section %3 or last section %4"
                " or event_count %5 - actual %6")
                .arg(section_number)
                .arg(last_section_number)
                .arg(event_count)
                .arg(actual));
        return false;
    }
    // Check the version number against the current table if any
    if (VERSION_UNINITIALISED == current_version_number)
    {
        // No previous table - I will initilaise the version number
        // from this section provided the event times (if present) are
        // reasonable.
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
                    LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                        "Present event does not span current time %1 %2 %3")
                        .arg(start_time)
                        .arg(end_time)
                        .arg(current_time));
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
                        LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                            "Paused following event must "
                            "start some time in the past and finish "
                            "some time in the future %1 %2 %3")
                            .arg(start_time)
                            .arg(end_time)
                            .arg(current_time));
                        return false;
                    }
                }
                else
                {
                    // Event must be in the the future
                    if (start_time < current_time)
                    {
                        LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                            "Following event must be in the "
                            "future %1 %2 %3")
                            .arg(start_time)
                            .arg(end_time)
                            .arg(current_time));
                        return false;
                    }
                }
            }
            candidateEventStatus = EventStatusEnum::VALID;
        }
        else
        {
            LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                    "No previous table - no events in incoming section"));
            candidateEventStatus = EventStatusEnum::EMPTY;
        }
        current_version_number = version_number; // for tidyness
    }
    else
    {
        // Previous table version exists
        if ((current_version_number + 1) % 32 == version_number)
        {
            // Invalidate the other section
            if (0 == section_number)
                following.eventStatus = EventStatusEnum::UNINITIALISED;
            else
                present.eventStatus = EventStatusEnum::UNINITIALISED;
        }
        else if (current_version_number == version_number)
        {
            // TODO more work needed on this logic. !!!!!!!!!
            // It needs to discard real duplicates
            // and treat everything else as a discontinuity
            if (((0 == section_number)
                    && (EventStatusEnum::UNINITIALISED
                            != present.eventStatus))
                || ((0 != section_number) 
                    && (EventStatusEnum::UNINITIALISED
                            != following.eventStatus)))
            {
                LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                        "PfTable same version existing section "
                        "not UNITIALISED"));
                return false;
            }
                
            if (!actual)
            {
                LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                        "PfTable same version not actual - ignore"));
                return false;
            }
            else
                LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                        "Same version actual - allow update")); // TODO I think I should check the event times here
        }
        else
        {
            // discontinuity
            // If actual transport then allow jump
            if (!actual)
            {
                LOG(VB_EIT, LOG_DEBUG, LOC + QString("PfTable %1/%2/%3 "
                    "ProcessSection-version discontinuity "
                    " current %4 new %5 - actual %6")
                    .arg(original_network_id)
                    .arg(transport_stream_id)
                    .arg(service_id)
                    .arg(current_version_number)
                    .arg(version_number)
                    .arg(actual));
                
                return false;
            }
        }
        if (event_count > 0)
            candidateEventStatus = EventStatusEnum::VALID;
        else
            candidateEventStatus = EventStatusEnum::EMPTY;

    }
    
    // Section has passed all the checks - it should contain zero or one event
    
    LOG(VB_EIT, LOG_DEBUG, LOC + QString(
            "Updating PfTable %1/%2/%3 new version %4 - actual %5")
            .arg(original_network_id)
            .arg(transport_stream_id)
            .arg(service_id)
            .arg(version_number)
            .arg(actual));
            
    version_from_actual = actual;
    current_version_number = version_number;
    
    if (0 == section_number)
        pfEvent = &present;
    else
        pfEvent = &following;
        
    LOG(VB_EIT, LOG_DEBUG, LOC + QString(
            "section_number %1 event id %2 start time %3 end time %4 "
            "running status %5 scrambled %6 event status %7")
            .arg(section_number)
            .arg(eit->EventID(0))
            .arg(eit->StartTimeUnixUTC(0))
            .arg(eit->EndTimeUnixUTC(0))
            .arg(eit->RunningStatus(0))
            .arg(eit->IsScrambled(0))
            .arg(int(candidateEventStatus)));
            
    *pfEvent = Event(eit->EventID(0),
                        eit->StartTimeUnixUTC(0),
                        eit->EndTimeUnixUTC(0),
                        RunningStatus(eit->RunningStatus(0)),
                        eit->IsScrambled(0), candidateEventStatus);
    
    return true;
}


bool EitCacheDVB::ScheduleTable::ProcessSection(
                const DVBEventInformationTable *eit, const bool actual)
{
    //LOG(VB_EIT, LOG_DEBUG, LOC + QString("ScheduleTable::"
    //      "ProcessSection placeholder"));
    return true;
}

