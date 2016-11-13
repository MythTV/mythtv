#include "eitcachedvb.h"
#include "mpeg/dvbtables.h"
#include "mythlogging.h"

// Qt includes
#include <QDateTime>
#include <QStringList>



// This code processes sections from the Event Information Tables(EIT).
// according to the rules defined in ETSI EN 300 468 V1.15.1
// and ETSI TS 101 211 V1.12.1

// There are two Event Information Tables, the Present / Following (P/F)
// table and the Schedule table.
//
// When encoded into transport stream packets these tables are identified
// by unique table id numbers. For encoding, each table is split into
// numbered sections, each section is carried in a separate transport packet.
// These sections carry the body of the table, which is made up a events
// each with a numeric identifier that is unique for a particular transport
// service. A transport service corresponds to mythtv's concept of a channel.
//
// The P/F table is identified by two numbers, PF_EIT (0x4e decimal 78)
// and PF_EITo (0x4f decimal 79). Both these numbers refer to the SAME
// table. PF_EIT is used when the table is being carried in the same
// transport stream as the one its content refers to. PF_EITo is used when
// the table is being carried in a different transport stream than the
// one its content refers to.
//
// The Schedule table is a bit more complex. It is identified by by two
// sets of table id numbers. SC_EITbeg (0x50 80) to SC_EITend (0x5f 95)
// and SC_EITbeg0 (0x60 96) to SC_EITendo (0x6f 111). Once again both
// these sets of numbers refer to the SAME table, and significance is the
// same as it is for the P/F table. The different table numbers correspond
// to consecutive 96 hour periods starting from the UTC midnight on the
// present day. So sections in table 0x50 (or 0x60) will only carry events
// starting in the present day and 3 following days, this of course means
// that as well as events that are due to be broadcast later it can carry
// at most one event that is currently being broadcast (running) and zero
// or more events that are already in the past depending on the current
// time. Table 0x51 (0x61) carries events starting 4 days in the future ,
// and so on up to table 0x5f (0x6f) that carries events starting 60 to
// 63 days into the future. A particular event can enter the table at any
// point depending on how far into the future the broadcaster publishes
// schedule data, at that point it will migrate downwards over time towards
// table 0x50 (0x60). The sections of the table are ordered chronologically
// as are the events in the sections. The 256 possible numbered sections of
// the table are also grouped into 32  segments each containing up to 8
// sections. Each segment contains events for a 3 hour period. Segments may contain
// less than eight sections but the sections must be numbered consecutively
// starting at the base number defined for the segment.
//
// The base numbers and time periods are as follows.
//
// Segment   Base section   Time period (UTC)  Day
// ===============================================
//    0           0             0:0 - 2:59     0
//    1           8             3:0 - 5:59     0
//    2           16            6:0 - 8:59     0
//    3           24            9:0 - 11:59    0
//    4           32            12:0 - 14:59   0
//    5           40            15:0 - 17:59   0
//    6           48            18:0 - 20:59   0
//    7           56            21:0 - 23:59   0
//    8           64            0:0 - 2:59     1
//    9           72            3:0 - 5:59     1
//    10          80            6:0 - 8:59     1
//    11          88            9:0 - 11:59    1
//    12          96            12:0 - 14:59   1
//    13          104           15:0 - 17:59   1
//    14          112           18:0 - 20:59   1
//    15          120           21:0 - 23:59   1
//    16          128           0:0 - 2:59     2
//    17          136           3:0 - 5:59     2
//    18          144           6:0 - 8:59     2
//    19          152           9:0 - 11:59    2
//    20          160           12:0 - 14:59   2
//    21          168           15:0 - 17:59   2
//    22          176           18:0 - 20:59   2
//    23          184           21:0 - 23:59   2
//    24          192           0:0 - 2:59     3
//    25          200           3:0 - 5:59     3
//    26          208           6:0 - 8:59     3
//    27          216           9:0 - 11:59    3
//    28          224           12:0 - 14:59   3
//    29          232           15:0 - 17:59   3
//    30          240           18:0 - 20:59   3
//    31          248           21:0 - 23:59   3
//
// Table version numbers
// =====================
//
// A strict interpretation of the ETSI standard means that
// the version number of a table for a particular service
// shall be the same whether it is carried in an "actual"
// transport stream or it is carried in an "other" transport
// stream.
//
// However the United Kingdom Digital Television Group
// have applied what they refer to as a severe "relaxation"
// of the standard on the UK digital terrestrial television
// network (original network id 9018). On the UK network there
// is no requirement that the version number of actual and
// other versions of the same table shall carry the same version
// numbers.
//
// This means it is impossible to synchronise the data between
// these tables. The implication is that I have to maintain a complete
// table structure of the "actual" data and for each occurrence of
// the same table seen on a different "other" transport streams,
// An algorithm is needed when an up to date "actual" version of a
// table is not available to determine which of the set of
// "other" tables can be used as "second best".
//
// DTG have stated in the past that it should be possible to
// synchronise version numbers between the same "other" tables
// carried on different multiplexes within the same network.
//
// I have asked them to clarfy the current situation but so far
// have received no response. (7 November 2016)

#define LOC QString("EitCacheDVB: ")

// Networks that do not maintain consistent version numbers
// across actual and other tables
enum {UK_DTT_NETWORK,
      NETWORKS_WITH_NON_CONFORMANT_VERSION_NUMBERS};
static const uint ncv_networks[] = {[UK_DTT_NETWORK] = 0x233a};

static QStringList RSTNames(QStringList() << "Undefined"
                                << "not running"
                                << "starts in a few seconds"
                                << "pausing"
                                << "running"
                                << "service off-air"
                                << "reserved"
                                << "reserved");
                                
EitCacheDVB::Event::Event(uint event_id,
                time_t start_time,
                time_t end_time,
                RunningStatusEnum running_status,
                bool is_scrambled,
                EventTable& events,
                unsigned long long table_key) :
                event_id(event_id),
                start_time(start_time),
                end_time(end_time),
                running_status(running_status),
                is_scrambled(is_scrambled)
{
    // Construct the key
    if (0 != table_key)
    {
        // Handle shared table
        unsigned long long key = table_key << 16 | event_id;
/*
        if (events.contains(key))
        {
            LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                "Event is already in events table %1")
                .arg(events[key]->ref._q_value));
                
            // Take a temp ref on the existing event
            QExplicitlySharedDataPointer<Event> old_entry
                = events[key];
            
            LOG(VB_EITDVBPF, LOG_INFO, LOC + QString(
                "old_entry ref before insert %1")
                .arg(old_entry->ref._q_value));
                
            events.insert(key,
                            QExplicitlySharedDataPointer<Event>(this));
                            
            LOG(VB_EITDVBPF, LOG_INFO, LOC + QString(
                "old_entry ref after insert %1")
                .arg(old_entry->ref._q_value));
                
            LOG(VB_EITDVBPF, LOG_INFO, LOC + QString(
                "new entry refcount after insert %1")
                .arg(events[key]->ref._q_value));
        }
        else
        {
            LOG(VB_EITDVBPF, LOG_INFO, LOC + QString(
                "Event is not in events table %1")
                .arg(ref._q_value));
            events.insert(key,
                            QExplicitlySharedDataPointer<Event>(this));
            LOG(VB_EITDVBPF, LOG_INFO, LOC + QString(
                "Ref count after  insert %1")
                .arg(events[key]->ref._q_value));
        }
*/
        // If you uncomment the debug stuff above remember to
        // comment this out
        events.insert(key,
                        QExplicitlySharedDataPointer<Event>(this));
    }
    
}

//EitCacheDVB::Event::~Event()
//{
//    LOG(VB_EITDVBPF, LOG_INFO, LOC + QString(
//    "Destroying event %1").arg(quintptr(this),
//                    QT_POINTER_SIZE * 2, 16, QChar('0')));
//}

bool EitCacheDVB::ProcessSection(const DVBEventInformationTable *eit,
                                 uint current_tsid,
                                 bool &section_version_changed,
                                 bool &table_version_change_complete)
{
    // Single thread this function
    QMutexLocker lock(&tableLock);
    
    long long original_network_id = eit->OriginalNetworkID();
    uint transport_stream_id = eit->TSID();
    uint service_id = eit->ServiceID();
    uint table_id = eit->TableID();
    bool section_handled = false;
    section_version_changed = false;
    table_version_change_complete = false;
    
    // See if this section is for a currently tuned transport stream
    bool actual = (table_id == TableID::PF_EIT) ||
        ((table_id >= TableID::SC_EITbeg) && 
        (table_id <= TableID::SC_EITend));
        
    // See if this section is for the pf table or the schedule table
    unsigned long long key = original_network_id << 32 |
                    transport_stream_id  << 16 | service_id;
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

        section_handled = pfTables[key].ProcessSection(eit, current_tsid, actual, events,
                                        section_version_changed,
                                        table_version_change_complete, key);
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
        
        section_handled = scheduleTables[key].ProcessSection(eit, current_tsid, actual, events,
                                        section_version_changed,
                                        table_version_change_complete, key);
    }
    else if (table_id == TableID::RST)
    {
        LOG(VB_EITRS, LOG_INFO, LOC + QString(
                "The long lost running status table has finally been "
                "found, send for the doctor. \"Doctor Who?\". \"Of "
                "Course!\"."));
        section_handled = true;
    }

    return section_handled;
}

bool EitCacheDVB::PfTable::ProcessSection(
                const DVBEventInformationTable *eit,
                uint current_tsid,
                const bool actual,
                Event::EventTable& events,
                bool &section_version_changed,
                bool &table_version_change_complete,
                unsigned long long table_key)
{
    // Validate against ETSI EN 300 468 V1.15.1
    // and ETSI TS 101 211 V1.12.1
    
    // Basic checks
    uint table_id = eit->TableID();
    uint section_number = eit->Section();
    uint version_number = eit->Version();
    uint last_table_id = eit->LastTableID();
    uint segment_last_section_number = eit->SegmentLastSectionNumber();
    uint last_section_number = eit->LastSection();
    uint event_count = eit->EventCount();
    
    // Some networks need special handling because they
    // do not have consistent version numbers when
    // tables are carried in "other" transport streams.
    // This implies having version data stored for
    // each different transport stream we see data on.
    // I use the the current_tsid passed in for this.
    // Until this work is complete I am ignoring "other" table
    // sections for these networks.
    if (!actual)
    {
        for (int i = 0; i < NETWORKS_WITH_NON_CONFORMANT_VERSION_NUMBERS; i++)
        {
            if (ncv_networks[i] == original_network_id)
            {
                //LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                //        "Original network %1 ditching other").arg(original_network_id));
                return false;
            }
        }
    }

    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString("Processing present"
                        "/following section "
                        "original_network_id 0x%1 "
                        "transport_stream_id 0x%2 "
                        "service_id 0x%3 "
                        "table_id %4 "
                        "section_number %5 "
                        "version_number %6 "
                        "last_table_id %7 "
                        "segment_last_section_number %8 "
                        "last_section_number %9 "
                        "event_count %10 "
                        "actual %11 "
                        "current_tsid %12")
                        .arg(original_network_id,0,16)
                        .arg(transport_stream_id,0,16)
                        .arg(service_id,0,16)
                        .arg(table_id)
                        .arg(section_number)
                        .arg(version_number)
                        .arg(last_table_id)
                        .arg(segment_last_section_number)
                        .arg(last_section_number)
                        .arg(event_count)
                        .arg(actual)
                        .arg(current_tsid,0,16));

        
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
    
    // TODO validate running status
    
    EventWrapper& pfEvent  =  (0 == section_number)
                                ? present : following;

    // Check the version number against the current table if any
    if (table_status == TableStatusEnum::UNINITIALISED)
    {
        // No previous data, initialise the table
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
            // No previous event section - I will initialise the
            //version number from this section
            LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                "Event uninitialised - accepting "
                "incoming data"));
        }
        else
        {
            // Something to check against
            if ((table_version + 1) % 32 == version_number)
            {
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
                        if (pfEvent.event_status
                                    == TableStatusEnum::EMPTY)
                        {
                            // No incoming events 
                            // table valid but empty
                            LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                                "PF table same version no incoming "
                                "and no stored events present - reject"
                                " duplicate"));
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
                                "PF table same version no incoming "
                                "events not on actual stream "
                                "- reject"));
                            return false;
                        }
                        // On the "actual" stream accept
                        // the incoming section as a wrap around
                        // discontinuity.
                        // Invalidate verything it will be set right
                        // later
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
                            if (Event(eit->EventID(0),
                                    eit->StartTimeUnixUTC(0),
                                    eit->EndTimeUnixUTC(0),
                                    RunningStatus(eit->RunningStatus(0)),
                                    eit->IsScrambled(0), events, 0) ==
                                    *pfEvent.event)
                            {
                                LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                                    "PF table same version some incoming "
                                    "events - duplicate"));
                                return false;
                            }
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
                    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                        "PF table discontinuity accepted %1")
                        .arg(actual ? "actual" : "other"));
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

    if (present.section_version == following.section_version)
        table_version_change_complete = true;

    if (section_version_changed)
        LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
                "Pf table version change %1")
                .arg(table_version_change_complete
                     ? "complete" : "incomplete"));

    uint  event_id = eit->EventID(0);
    time_t start_time = eit->StartTimeUnixUTC(0);
    time_t end_time = eit->EndTimeUnixUTC(0);
    RunningStatusEnum running_status = RunningStatusEnum(eit->RunningStatus(0));
    
    if ((pfEvent.event_status == TableStatusEnum::VALID)
            && (running_status != pfEvent.event->running_status))
    {
        LOG(VB_EITRS, LOG_INFO, LOC + QString(
                        "Running status for 0x%1/0x%2/0x%3/%4 change in PD %5 data - "
                        "was %6 is %7")
                .arg(original_network_id,0,16)
                .arg(transport_stream_id,0,16)
                .arg(service_id,0,16)
                .arg(event_id)
                .arg(section_number == 0 ? "present" : "following")
                .arg(RSTNames[uint(pfEvent.event->running_status)])
                .arg(RSTNames[uint(running_status)]));
    }
    
    uint is_scrambled = eit->IsScrambled(0);
    
    LOG(VB_EITDVBPF, LOG_DEBUG, LOC + QString(
            "EIT PF change - "
            "original_network_id 0x%1 "
            "transport_stream_id 0x%2 "
            "service_id 0x%3 "
            "section_number %4 "
            "version_number %5%6 "
            "event_count %7 "
            "event_id %8 "
            "start_time %9%10 "
            "end_time %11%12 "
            "running_status %13%14 "
            "is_scrambled %15 "
            "actual %16")
            .arg(original_network_id,0,16)
            .arg(transport_stream_id,0,16)
            .arg(service_id,0,16)
            .arg(section_number)
            .arg(version_number).arg(version_change ? "*" : "")
            .arg(event_count)
            .arg(event_id)
            .arg(QDateTime::fromTime_t(start_time)
                .toString(Qt::SystemLocaleShortDate))
                .arg(((pfEvent.event_status
                    == TableStatusEnum::VALID)
                    && (start_time != pfEvent.event->start_time))
                    ? "*" : "")
            .arg(QDateTime::fromTime_t(end_time)
                .toString(Qt::SystemLocaleShortDate))
                .arg(((pfEvent.event_status
                    == TableStatusEnum::VALID)
                    && (end_time != pfEvent.event->end_time))
                    ? "*" : "")
            .arg(RSTNames[uint(running_status)]).arg(
                ((pfEvent.event_status
                    == TableStatusEnum::VALID)
                    && (running_status
                    != pfEvent.event->running_status))
                    ? "*" : "")
            .arg(is_scrambled)
            .arg(actual));

    table_status = TableStatusEnum::VALID;

    pfEvent.event_status = event_count > 0 ?
                            TableStatusEnum::VALID :
                            TableStatusEnum::EMPTY;
    pfEvent.event = new Event(event_id,
                        start_time,
                        end_time,
                        running_status,
                        is_scrambled,
                        events, table_key);
                        
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
                    const DVBEventInformationTable *eit,
                    uint current_tsid,
                    const bool actual,
                    Event::EventTable& events,
                    bool &section_version_changed,
                    bool &table_version_change_complete,
                    unsigned long long table_key)
{
    // Validate against ETSI EN 300 468 V1.15.1
    // and ETSI TS 101 211 V1.12.1
    
    // Basic checks
    uint version_number = eit->Version();

    uint table_id = eit->TableID();
    uint last_table_id = eit->LastTableID();
    uint subtable_index = table_id - (actual ? 0x50 : 0x60);
    uint incoming_subtable_count = last_table_id 
                                - (actual ? 0x50 : 0x60)
                                + 1;

    uint section_number = eit->Section();
    uint segment_index = section_number / 8;
    uint section_index = section_number - (segment_index * 8);
    uint segment_last_section_number = eit->SegmentLastSectionNumber();
    uint incoming_section_count = segment_last_section_number
                                    - (segment_index * 8) + 1;

    uint last_section_number = eit->LastSection();
    uint incoming_segment_count = (last_section_number / 8) + 1;

    uint event_count = eit->EventCount();

    SubTable& current_subtable = subtables[subtable_index];
    TableStatusEnum& current_subtable_status = current_subtable.subtable_status;
    uint& current_subtable_version = current_subtable.subtable_version;

    Segment& current_segment = current_subtable.segments[segment_index];
    TableStatusEnum& current_segment_status = current_segment.segment_status;
    uint& current_segment_version = current_segment.segment_version;

    Section& current_section = current_segment.sections[section_index];
    TableStatusEnum& current_section_status = current_section.section_status;
    uint& current_section_version = current_section.section_version;

    // Some networks need special handling because they
    // do not have consistent version numbers when
    // tables are carried in "other" transport streams.
    // This implies having version data stored for
    // each different transport stream we see data on.
    // I use the the current_tsid passed in for this.
    // Until this work is complete I am ignoring "other" table
    // sections for these networks.
    if (!actual)
    {
        for (int i = 0; i < NETWORKS_WITH_NON_CONFORMANT_VERSION_NUMBERS; i++)
        {
            if (ncv_networks[i] == original_network_id)
            {
                //LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
                //        "Original network %1 ditching other").arg(original_network_id));
                current_subtable.nonconformant_versions.insert(transport_stream_id, version_number);
                return false;
            }
        }
    }

    LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString("Processing schedule section "
                        "0x%1/0x%2/0x%3/%4 "
                        "section_number %5 "
                        "version_number %6 "
                        "last_table_id %7 "
                        "segment_last_section_number %8 "
                        "last_section_number %9 "
                        "subtable_count %10 "
                        "subtable_index %11 "
                        "segment_count %12 "
                        "segment_index %13 "
                        "section_count %14 "
                        "section_index %15 "
                        "event_count %16 "
                        "actual %17 "
                        "current_tsid %18")
                        .arg(original_network_id,0,16)
                        .arg(transport_stream_id,0,16)
                        .arg(service_id,0,16)
                        .arg(table_id)
                        .arg(section_number)
                        .arg(version_number)
                        .arg(last_table_id)
                        .arg(segment_last_section_number)
                        .arg(last_section_number)
                        .arg(incoming_subtable_count)
                        .arg(subtable_index)
                        .arg(incoming_segment_count)
                        .arg(segment_index)
                        .arg(incoming_section_count)
                        .arg(section_index)
                        .arg(event_count)
                        .arg(actual)
                        .arg(current_tsid,0,16));

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
        if (current_subtable_version == VERSION_UNINITIALISED)
        {
            // This should not happen :-)
            LOG(VB_EITDVBSCH, LOG_ERR, LOC + QString(
                "Subtable version uninitialized"));
            return false;            
        }

        if (current_section_status == TableStatusEnum::UNINITIALISED)
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
            if ((current_subtable_version + 1) % 32 == version_number)
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
                if (current_subtable_version == version_number)
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
                                                eit->IsScrambled(ievent), events, 0) ==
                                                *current_section.events[ievent]))
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
    bool segment_count_changed = false;
    uint old_segment_count;

    if (current_subtable_version != version_number)
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
            "Schedule table version number changed old %1 new %2")
            .arg(current_subtable_version)
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

    if (current_subtable.segment_count != incoming_segment_count)
    {
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString(
            "Schedule table segment_count changed old %1 new %2")
            .arg(current_subtable.segment_count)
            .arg(incoming_segment_count));
        old_segment_count = current_subtable.segment_count;
        current_subtable.segment_count = incoming_segment_count;
        segment_count_changed = true;
    }

    current_subtable_status = TableStatusEnum::VALID;
    current_subtable_version = version_number;

    current_segment_status = TableStatusEnum::VALID;
    current_segment_version = version_number;

    current_section_status = TableStatusEnum::VALID;
    if (current_section_version != version_number)
    {
        section_version_changed = true;
        current_section_version = version_number;
    }

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
            QExplicitlySharedDataPointer<Event>(new Event(eit->EventID(ievent),
                        eit->StartTimeUnixUTC(ievent),
                        eit->EndTimeUnixUTC(ievent),
                        RunningStatus(eit->RunningStatus(ievent)),
                        eit->IsScrambled(ievent),
                        events, table_key)));

    if (!version_changed &&
            !subtable_count_changed &&
            !segment_count_changed &&
            !section_count_changed &&
            !event_count_changed)
        LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString("EIT nothing major changed!"));

    LOG(VB_EITDVBSCH, LOG_DEBUG, LOC + QString("EIT schedule table change "
                        "version_number %1%2 "
                        "subtable_count %3%4 "
                        "segment_count %5%6 "
                        "section_count %7%8 "
                        "event_count %9%10 ")
                        .arg(version_number)
                            .arg(version_changed ? QString("* %1")
                                .arg(current_subtable_version) : "")
                        .arg(incoming_subtable_count)
                            .arg(subtable_count_changed ? QString("* %1")
                                .arg(old_subtable_count) : "")
                        .arg(incoming_segment_count)
                            .arg(segment_count_changed ? QString("* %1")
                                .arg(old_segment_count) : "")
                        .arg(incoming_section_count)
                            .arg(section_count_changed ? QString("* %1")
                                .arg(old_section_count) : "")
                        .arg(event_count)
                            .arg(event_count_changed ? QString("* %1")
                                .arg(old_event_count) : ""));

    table_version_change_complete = true;
    if (section_version_changed)
    {
        int i(-1), j(-1);
        if ((current_subtable.subtable_status == TableStatusEnum::UNINITIALISED)
                || (current_subtable.subtable_version == TableBase::VERSION_UNINITIALISED)
                || (current_subtable.subtable_version != version_number))
        {
            LOG(VB_EITDVBSCH, LOG_INFO, LOC
                + QString("0x%1/%2 Early bailout l1 %3/%4")
                .arg(service_id,0,16).arg(table_id)
                .arg(uint(current_subtable.subtable_status))
                .arg(current_subtable.subtable_version));
            table_version_change_complete = false;
        }
        else
        {
            for (i = 0; (i < int(current_subtable.segment_count))
                                && table_version_change_complete; i++)
            {
                Segment& segment = current_subtable.segments[i];
                if ((segment.segment_status == TableStatusEnum::UNINITIALISED)
                        || (segment.segment_version == TableBase::VERSION_UNINITIALISED)
                        || (segment.segment_version != version_number))
                {
                    table_version_change_complete = false;
                    LOG(VB_EITDVBSCH, LOG_INFO, LOC
                        + QString("0x%1/%2 Early bailout l2 %3/%4/%5/%6")
                        .arg(service_id,0,16).arg(table_id)
                        .arg(i).arg(j)
                        .arg(uint(segment.segment_status))
                        .arg(segment.segment_version));
                }
                for (j = 0; (j < int(segment.section_count))
                                    && table_version_change_complete; j++)
                {
                    Section& section = segment.sections[j];
                    if ((section.section_status == TableStatusEnum::UNINITIALISED)
                             || (section.section_version == TableBase::VERSION_UNINITIALISED)
                             || (section.section_version != version_number))
                    {
                        LOG(VB_EITDVBSCH, LOG_INFO, LOC
                            + QString("0x%1/%2 Bailout l3 %3/%4/%5/%6")
                            .arg(service_id,0,16).arg(table_id)
                            .arg(i).arg(j)
                            .arg(uint(section.section_status))
                            .arg(section.section_version));
                        table_version_change_complete = false;
                    }
                }
            }
        }
        if (table_version_change_complete)
            LOG(VB_EITDVBSCH, LOG_INFO, LOC
                + QString("EIT_SCH 0x%1/0x%2/0x%3/%4 complete "
                        "v %5 "
                        "s %6 "
                        "slsn %7 "
                        "lsn %8 "
                        "sgi %9 "
                        "sci %10")
                .arg(original_network_id,0,16)
                .arg(transport_stream_id,0,16)
                .arg(service_id,0,16)
                .arg(table_id)
                .arg(version_number)
                .arg(section_number)
                .arg(segment_last_section_number)
                .arg(last_section_number)
                .arg(segment_index)
                .arg(section_index));
        else
            LOG(VB_EITDVBSCH, LOG_INFO, LOC
                + QString("EIT_SCH 0x%1/0x%2/0x%3/%4 incomplete "
                        "v %5 "
                        "s %6 "
                        "slsn %7 "
                        "lsn %8 "
                        "sgi %9 "
                        "sci %10 "
                        "i %11 "
                        "j %12")
                .arg(original_network_id,0,16)
                .arg(transport_stream_id,0,16)
                .arg(service_id,0,16)
                .arg(table_id)
                .arg(version_number)
                .arg(section_number)
                .arg(segment_last_section_number)
                .arg(last_section_number)
                .arg(segment_index)
                .arg(section_index)
                .arg(i)
                .arg(j));
    }

    return true;
}
