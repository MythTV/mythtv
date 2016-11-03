#ifndef _EITCACHEDVB_H_
#define _EITCACHEDVB_H_

#include "compat.h"

// Qt includes
#include <QMap>
#include <QMutex>
#include <QSharedData>

/** \file eitcachedvb.h
 *  \code
 *  \endcode
 */

class DVBEventInformationTable;

class EitCacheDVB
{
public: // Declarations
        // NONE
public: // Methods
#if __cplusplus >= 201103L
    EitCacheDVB(EitCacheDVB const&) = delete;
        
    void operator=(EitCacheDVB const&) = delete;
#endif

    static EitCacheDVB& GetInstance()
    {
        static EitCacheDVB instance;
        return instance;
    }
    
    bool ProcessSection(const DVBEventInformationTable *eit,
                        bool &section_version_changed,
                        bool &table_version_change_complete);
    
public: // Data
    // NONE
protected: // Declarations
    // NONE
protected: // Methods
    // NONE
protected: // Data
    // NONE
private: // Declarations
#if __cplusplus >= 201103L
    typedef enum class RunningStatus
    {
        UNDEFINED = 0,
        NOT_RUNNING = 1,
        STARTS_IN_A_FEW_SECONDS = 2,
        PAUSING = 3,
        RUNNING = 4
    } RunningStatusEnum;
    
    typedef enum class TableStatus
    {
        UNINITIALISED = 0,
        VALID = 1,
        EMPTY = 2
    } TableStatusEnum;
#else
    struct RunningStatus
    {
        enum type
        {
            UNDEFINED = 0,
            NOT_RUNNING = 1,
            STARTS_IN_A_FEW_SECONDS = 2,
            PAUSING = 3,
            RUNNING = 4
        };	
    };
    typedef RunningStatus::type RunningStatusEnum;
    
    struct TableStatus
    {
        enum type
        {
            UNINITIALISED = 0,
            VALID = 1,
            EMPTY = 2
        };
    };
    typedef TableStatus::type TableStatusEnum;
#endif

    struct Event : public QSharedData
    {

        Event() : event_id(0),
                    start_time(0),
                    end_time(0),
                    running_status
                        (EitCacheDVB::RunningStatusEnum::UNDEFINED),
                    is_scrambled(false) {}

        typedef QMap<unsigned long long, struct QExplicitlySharedDataPointer<Event> > EventTable;
        
        Event(uint event_id,
                time_t start_time,
                time_t end_time,
                RunningStatusEnum running_status,
                bool is_scrambled,
                EventTable& events,
                unsigned long long table_key);

        ~Event();
        
        uint event_id;
        time_t start_time;
        time_t end_time;			
        RunningStatusEnum running_status;
        bool is_scrambled;			
        // TODO Need to decide how/whether to handle descriptors
        
        bool operator == (const Event &e2) const
        {
            return (event_id == e2.event_id) &&
                    (start_time == e2.start_time) &&
                    (end_time == e2.end_time) &&
                    (running_status == e2.running_status) &&
                    (is_scrambled == e2.is_scrambled);
        }
    };
        
    struct Section
    {
        Section() :
            section_status(EitCacheDVB::TableStatusEnum::UNINITIALISED),
            section_version(TableBase::VERSION_UNINITIALISED) {}
                    
        Section(const Section &other)
        {
            *this = other;
        }

        Section& operator = (const Section &other)
        {
            section_status = other.section_status;
            events = other.events;
            return *this;
        }

        TableStatusEnum section_status;
        uint section_version;
        QList< QExplicitlySharedDataPointer<Event> > events;
    };
    
    struct Segment
    {
        Segment() :
            section_count(0),
            segment_status(EitCacheDVB::TableStatusEnum::UNINITIALISED),
            segment_version(TableBase::VERSION_UNINITIALISED) {}
                    
        Segment(const Segment &other)
        {
            *this = other;
        }

        Segment& operator = (const Segment &other)
        {
            segment_status = other.segment_status;
            segment_version = other.segment_version;
            section_count = other.section_count;
            for (uint i = 0; i < 8; i++)
                sections[i] = other.sections[i];
            return *this;
        }

        uint section_count;
        TableStatusEnum segment_status;
        uint segment_version;
        Section sections[8];
    };
    
    struct SubTable
    {
        SubTable() : subtable_status(
                    EitCacheDVB::TableStatusEnum::UNINITIALISED),
                    subtable_version(TableBase::VERSION_UNINITIALISED),
                    segment_count(0)
                    {}
                    
        SubTable(const SubTable &other)
        {
            *this = other;
        }

        SubTable& operator = (const SubTable &other)
        {
            subtable_status = other.subtable_status;
            subtable_version = other.subtable_version;
            for (uint i = 0; i < 32; i++)
                segments[i] = other.segments[i];
            return *this;
        }
        
        TableStatusEnum subtable_status;
        Segment segments[32];
        uint subtable_version; // TODO check whether the same version is always
                                // common across all subtables
        uint segment_count;
    };
    
    struct TableBase
    {
        TableBase() : original_network_id(0),
                        transport_stream_id(0),
                        service_id(0) {}
                
        TableBase(const TableBase &other)
        {
            *this = other;
        }

        TableBase& operator = (const TableBase &other)
        {
            original_network_id = other.original_network_id;
            transport_stream_id = other.transport_stream_id;
            service_id = other.service_id;
            return *this;
        }
        
        void SetOriginalNetworkId(uint id)
        {
            original_network_id = id;
        }
        
        void SetTransportStreamId(uint id)
        {
            transport_stream_id = id;
        }
        
        void SetServiceId(uint id)
        {
            service_id = id;
        }
        
        virtual bool ProcessSection(const DVBEventInformationTable *eit,
                            const bool actual, Event::EventTable& events,
                            bool &section_version_changed,
                            bool &table_version_change_complete,
                            unsigned long long table_key) = 0;

        static const uint VERSION_UNINITIALISED = 32;
        uint original_network_id;
        uint transport_stream_id;
        uint service_id;
    };
    
    struct ScheduleTable : public TableBase
    {
        ScheduleTable() : subtable_count(0) {}
                                            
        ScheduleTable(const ScheduleTable &other)
        {
            *this = other;
        }

        ScheduleTable& operator = (const ScheduleTable &other)
        {
            TableBase::operator = (other);
            return *this;
        }
        
        virtual bool ProcessSection(const DVBEventInformationTable *eit,
                            const bool actual, Event::EventTable& events,
                            bool &section_version_changed,
                            bool &table_version_change_complete,
                            unsigned long long table_key);

    private: // Methods
        bool ValidateEventStartTimes(const DVBEventInformationTable *eit,
                uint event_count,
                uint section_number,
                bool actual);
        void InvalidateEverything();
                
    private: // Data
        uint subtable_count;
        SubTable subtables[8];
    };
    
    struct PfTable : public TableBase
    {
        struct EventWrapper
        {
            EventWrapper() :
                event_status(TableStatusEnum::UNINITIALISED),
                section_version(TableBase::VERSION_UNINITIALISED) {}
                
            
            EventWrapper(const EventWrapper &other)
            {
                *this = other;
            }

            EventWrapper& operator = (const EventWrapper &other)
            {
                return *this;
            }

            TableStatusEnum event_status;
            uint section_version;
            QExplicitlySharedDataPointer<Event> event;
        };
            
        // Methods
        
        PfTable() :
            table_version(VERSION_UNINITIALISED),
            table_status(EitCacheDVB::TableStatusEnum::UNINITIALISED)
            {}
            
        PfTable(const PfTable &other) : TableBase(other),
                                        present(other.present),
                                        following(other.following)
        {
            *this = other;
        }

        PfTable& operator = (const PfTable &other)
        {
            TableBase::operator = (other);
            return *this;
        }

        virtual bool ProcessSection(const DVBEventInformationTable *eit,
                            const bool actual, Event::EventTable& events,
                            bool &section_version_changed,
                            bool &table_version_change_complete,
                            unsigned long long table_key);

    private: // Methods
        bool ValidateEventTimes(const DVBEventInformationTable *eit,
                uint event_count,
                uint section_number);
            
    private: // Data
        uint table_version;
        TableStatusEnum table_status;
        EventWrapper present;
        EventWrapper following;
    };
    
private: // Methods
    EitCacheDVB() {};
#if __cplusplus < 201103L
    EitCacheDVB(EitCacheDVB const&); // Do NOT implement
    void operator=(EitCacheDVB const&); // Do NOT implement
#endif
private: // Data
    // The first paragraph of ETR 211 section 4.1.4 statues that
    // for each "service" a PF table and set of schedule
    // sub-tables exists.
    // ETR 211 section 4.1.1 states that each "service" can be
    // uniquely referenced through the path
    // original_network_id/transport_stream_id/service_id
    // I use that 48 bit path to index the following tables
    QMap<unsigned long long, struct PfTable> pfTables;
    QMap<unsigned long long, struct ScheduleTable> scheduleTables;
    
    // I want to maintain all the event data in a single shared space
    // indexed by original_network_id/transport_stream_id/service_id/eventId
    
    Event::EventTable events;
    
    QMutex tableLock;
};

#endif // _EITCACHEDVB_H_

