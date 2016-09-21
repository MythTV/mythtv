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
		
		typedef enum class EventStatus
		{
			UNINITIALISED = 0,
			VALID = 1,
			EMPTY = 3
		} EventStatusEnum;
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
		
		struct EventStatus
		{
			enum type
			{
				UNINITIALISED = 0,
				VALID = 1,
				EMPTY = 3
			};
		};
		typedef EventStatus::type  EventStatusEnum;
#endif
			
		struct Event
		{
			Event() {}
			Event(uint event_id,
					time_t start_time,
					time_t end_time,
					RunningStatusEnum runningStatus,
					bool isScrambled,
					EventStatusEnum eventStatus) :
					event_id(event_id),
					start_time(start_time),
					end_time(end_time),
					runningStatus(runningStatus),
					isScrambled(isScrambled),
					eventStatus(eventStatus) {}

			uint event_id;
			time_t start_time;
			time_t end_time;			
			RunningStatusEnum runningStatus;
			bool isScrambled;			
			EventStatusEnum eventStatus;
			// TODO Need to decide how/whether to handle descriptors
			
			bool operator== (const Event &e2) const
			{
				return (event_id == e2.event_id) &&
						(start_time == e2.start_time) &&
						(end_time == e2.end_time) &&
						(runningStatus == e2.runningStatus) &&
						(isScrambled == e2.isScrambled) &&
						(eventStatus == e2.eventStatus);
			}
		};
		
		struct Section
		{
			QMap<time_t, struct Event*> events;
		};
		
		struct Segment
		{
			QMap<int, struct Section> sections;
		};
		
		struct TableBase
		{
			TableBase() : current_version_number(VERSION_UNINITIALISED)
			{
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
			
			static const uint VERSION_UNINITIALISED = 32;
			static const time_t EVENT_TIME_LEEWAY = 2;
			uint original_network_id;
			uint transport_stream_id;
			uint service_id;
			uint current_version_number;
			bool version_from_actual; // True if the section contains
									// a version from the "actual" transport
									// stream and original network.
		};
		
		struct ScheduleTable : public TableBase
		{
			bool ProcessSection(const DVBEventInformationTable *eit,
								const bool actual);

		private: // Data
			QMap<int, struct Segment> segments;
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
					uint section_number,
					EventStatus& candidateEventStatus);
				
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
		// ETR 211 section 4.1.1 states that eache "service" can be
		// uniquely referenced through the path
		// original_network_id/transport_stream_id/service_id
		// I use that 48 bit path to index the following tables 
		QMap<long, struct PfTable> pfTables;
		QMap<long, struct ScheduleTable> scheduleTables;
};

#endif // _EITCACHEDVB_H_
