#ifndef _EITCACHEDVB_H_
#define _EITCACHEDVB_H_

#include "compat.h"

// Qt includes
#include <QMap>

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
        
        bool ProcessSection(const DVBEventInformationTable *eit);
		bool ProcessSection(const DVBEventInformationTable *eit,
							const bool actual);
        
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
		typedef RunningStatus::type  RunningStatusEnum;
		
		struct TableStatus
		{
			enum type
			{
				UNINITIALISED = 0,
				VALID = 1,
				EMPTY = 2
			};
		};
		typedef TableStatus::type  TableStatusEnum;
#endif
			
		struct Event
		{
			Event() : running_status(EitCacheDVB::RunningStatusEnum::UNDEFINED) {}
			Event(uint event_id,
					time_t start_time,
					time_t end_time,
					RunningStatusEnum runningStatus,
					bool isScrambled,
					TableStatusEnum eventStatus) :
					event_id(event_id),
					start_time(start_time),
					end_time(end_time),
					running_status(runningStatus),
					is_scrambled(isScrambled),
					event_status(eventStatus) {}

			uint event_id;
			time_t start_time;
			time_t end_time;			
			RunningStatusEnum running_status;
			bool is_scrambled;			
			TableStatusEnum event_status;
			// TODO Need to decide how/whether to handle descriptors
			
			bool operator== (const Event &e2) const
			{
				return (event_id == e2.event_id) &&
						(start_time == e2.start_time) &&
						(end_time == e2.end_time) &&
						(running_status == e2.running_status) &&
						(is_scrambled == e2.is_scrambled) &&
						(event_status == e2.event_status);
			}
		};
		
		struct Section
		{
			Section() : section_status(
						EitCacheDVB::TableStatusEnum::UNINITIALISED) 
						{}
						
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
			QList<struct Event> events;
		};
		
		struct Segment
		{
			Segment() : segment_status(
						EitCacheDVB::TableStatusEnum::UNINITIALISED),
						section_count(0) {}
						
			Segment(const Segment &other)
			{
				*this = other;
			}

			Segment& operator = (const Segment &other)
			{
				segment_status = other.segment_status;
				section_count = other.section_count;
				for (uint i = 0; i < 8; i++)
					sections[i] = other.sections[i];
				return *this;
			}

			TableStatusEnum segment_status;
			uint section_count;
			Section sections[8];
		};
		
		struct SubTable
		{
			SubTable() : subtable_status(
						EitCacheDVB::TableStatusEnum::UNINITIALISED)
						{}
						
			SubTable(const SubTable &other)
			{
				*this = other;
			}

			SubTable& operator = (const SubTable &other)
			{
				subtable_status = other.subtable_status;
				for (uint i = 0; i < 32; i++)
					segments[i] = other.segments[i];
				return *this;
			}
			
			TableStatusEnum subtable_status;
			Segment segments[32];
		};
		
		struct TableBase
		{
			TableBase() : current_version_number(VERSION_UNINITIALISED)
							{}
			
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
			
			static const uint VERSION_UNINITIALISED = 32;
			uint original_network_id;
			uint transport_stream_id;
			uint service_id;
			uint current_version_number;
		};
		
		struct ScheduleTable : public TableBase
		{
			ScheduleTable() : subtable_count(0) {}
			bool ProcessSection(const DVBEventInformationTable *eit,
								const bool actual);

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
			// Methods
			PfTable();	
			bool ProcessSection(const DVBEventInformationTable *eit,
								const bool actual);
		private: // Methods
			bool ValidateEventTimes(const DVBEventInformationTable *eit,
					uint event_count,
					uint section_number);
				
		private: // Data			
			struct Event present;
			struct Event following;
		};
		
	private: // Methods
        EitCacheDVB() {};
#if __cplusplus < 201103L
        EitCacheDVB(EitCacheDVB const&); // Do NOT implement
        void operator=(EitCacheDVB const&); // Do NOT implement
#endif
	private: // Data
		// The first paragraph of ETR 211 section 4.1.4 statues that
		// for each "service" a separate EIT sub-table exists.
		// ETR 211 section 4.1.1 states that each "service" can be
		// uniquely referenced through the path
		// original_network_id/transport_stream_id/service_id
		// I use that 48 bit path to index the following tables 
		QMap<long, struct PfTable> pfTables;
		QMap<long, struct ScheduleTable> scheduleTables;
};

#endif // _EITCACHEDVB_H_
