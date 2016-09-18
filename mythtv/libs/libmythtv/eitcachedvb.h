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
		enum RunningStatus
		{
			UNDEFINED = 0,
			NOT_RUNNING = 1,
			STARTS_IN_A_FEW_SECONDS = 2,
			PAUSING = 3,
			RUNNING = 4
		};
		
		enum EventStatus
		{
			UNINITIALISED = 0,
			VALID = 1,
			EMPTY = 3
		};
			
		struct Event
		{
			Event(){}
			Event(uint event_id, time_t start_time,
					uint durationInSeconds,
					RunningStatus runningStatus,
					bool isScrambled, bool isValid) :
						event_id(event_id), start_time(start_time),
						durationInSeconds(durationInSeconds),
						isScrambled(isScrambled),
						runningStatus(runningStatus),
						eventStatus(EventStatus::UNINITIALISED) {}
					
			uint event_id;
			time_t start_time;
			uint durationInSeconds;
			bool isScrambled;
			
			RunningStatus runningStatus;
			
			EventStatus eventStatus;
			// TODO Need to decide how/whether to handle descriptors
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
			void SetTransportStreamId(uint id)
			{
				transport_stream_id = id;
			}
			void SetOriginalNetworkId(uint id)
			{
				original_network_id = id;
			}
			static const uint VERSION_UNINITIALISED = 32;
			static const time_t EVENT_TIME_LEEWAY = 2;
			uint transport_stream_id;
			uint original_network_id;
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
			// Functions
			PfTable();	
			bool ProcessSection(const DVBEventInformationTable *eit,
								const bool actual);
		private: // Declarations
				// None
				
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
		QMap<int, struct PfTable> pfTables;
		QMap<int, struct ScheduleTable> scheduleTables;
};

#endif // _EITCACHEDVB_H_
