
#include "eitcachedvb.h"
#include "mpeg/dvbtables.h"
#include "mythlogging.h"

// Qt includes
#include <QDateTime>

#define LOC QString("EitCacheDVB: ")


EitCacheDVB::PfTable::PfTable()
{
    LOG(VB_EIT, LOG_DEBUG, LOC + QString("Constructing a PfTable"));
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
    uint transport_stream_id = eit->TSID();
    uint original_network_id = eit->OriginalNetworkID();
    int key = transport_stream_id  << 16 | original_network_id;
    if ((tableid == TableID::PF_EIT) || (tableid == TableID::PF_EITo))
    {
        if (!pfTables.contains(key))
        {
            struct PfTable new_table;
            new_table.SetTransportStreamId(transport_stream_id);
            new_table.SetOriginalNetworkId(original_network_id);
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
            new_table.SetTransportStreamId(transport_stream_id);
            new_table.SetOriginalNetworkId(original_network_id);
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
    // Basic checks
    uint version_number = eit->Version();
    uint section_number = eit->Section();
    uint last_section_number = eit->LastSection();
    uint event_count = eit->EventCount();
    if ((section_number > 1) || (1 != last_section_number) ||
            (event_count > 1))
    {
        LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                "PfTable %1/%2 bad section %3 or last section %4"
                "event_count %5 - actual %6")
                .arg(transport_stream_id)
                .arg(original_network_id)
                .arg(section_number)
                .arg(last_section_number)
                .arg(event_count)
                .arg(actual));
        return false;
    }
    // Check the version number against the current table if any
    if (VERSION_UNINITIALISED == current_version_number)
    {
        // No previous table - I will use this one wherever it came from
        // provided the event times (if present) are reasonable
        if (event_count > 0)
        {
            //time_t current_time = QDateTime::currentDateTimeUtc()
            //    .toTime_t();
            //time_t start_time = eit->StartTimeUnixUTC(0);
            //time_t end_time = eit->StartTimeUnixUTC(0);
            current_version_number = version_number;
        }
        else
            current_version_number = version_number; // Do I need to flush events here
            
        LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                "PfTable %1/%2 intialising version number to %3 - actual %4")
                .arg(transport_stream_id)
                .arg(original_network_id)
                .arg(version_number)
                .arg(actual));
        definitive_source = actual;
    }
    else
    {
        // Previous table version exists
        if ((current_version_number + 1) % 32 == version_number)
        {
            LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                    "PfTable %1/%2 new version %3 -n actual %3")
                    .arg(transport_stream_id)
                    .arg(original_network_id)
                    .arg(version_number)
                    .arg(actual));
            current_version_number = version_number;
        }
        else if (current_version_number == version_number)
        {
            // TODO - Allow same version on actual to update to the section
            // Ignore duplicate
            return false;
        }
        else
        {
            // discontinuity
            // If actual transport then allow jump
            if (actual)
            {
                LOG(VB_EIT, LOG_DEBUG, LOC + QString(
                        "PfTable %1/%2 new version %3 - actual %4")
                        .arg(transport_stream_id)
                        .arg(original_network_id)
                        .arg(version_number)
                        .arg(actual));
                current_version_number = version_number;
            }
            else
            {
                // Section for other transport stream
                // TODO algorithm for checking best candidate
                LOG(VB_EIT, LOG_DEBUG, LOC + QString("PfTable %1/%2 "
                    "ProcessSection-version discontinuity "
                    " current %3 new %4 - actual %5")
                    .arg(transport_stream_id)
                    .arg(original_network_id)
                    .arg(current_version_number)
                    .arg(version_number)
                    .arg(actual));
                return false;
            }
        }
    }
    definitive_source = actual;
    return true;
}


bool EitCacheDVB::ScheduleTable::ProcessSection(
                const DVBEventInformationTable *eit, const bool actual)
{
    //LOG(VB_EIT, LOG_DEBUG, LOC + QString("ScheduleTable::"
    //      "ProcessSection placeholder"));
    return true;
}

