// -*- Mode: c++ -*-

#ifndef EIT_HELPER_H
#define EIT_HELPER_H

// C+ headers
#include <cstdint>
#include <utility>

// Qt includes
#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>

// MythTV includes
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythdeque.h"
#include "libmythtv/mpeg/mpegtables.h" // for GPS_LEAP_SECONDS

class MSqlQuery;

// An entry from the EIT table containing event details.
class ATSCEvent
{
  public:
    ATSCEvent(uint a, uint b, uint c, QString  d,
              const unsigned char *e, uint f)
        : m_startTime(a), m_length(b), m_etm(c), m_descLength(f),
          m_title(std::move(d)), m_desc(e),
          m_scanTime(SystemClock::now()) {}

    bool IsStale() const {
        // The minimum recommended repetition time for EIT events according to
        // http://atsc.org/wp-content/uploads/2015/03/Program-and-system-information-protocol-implementation-guidelines-for-broadcaster.pdf
        // is one minute. Consider any EIT event seen > 2 minutes in the past as stale.
        return m_scanTime + 2min < SystemClock::now();
    }

    uint32_t             m_startTime;
    uint32_t             m_length;
    uint32_t             m_etm;
    uint32_t             m_descLength;
    QString              m_title;
    const unsigned char *m_desc {nullptr};

  private:
    // The time the event was created.
    SystemTime           m_scanTime;
};

// An entry from the ETT table containing description text for an event.
class ATSCEtt
{
  public:
    explicit ATSCEtt(QString  text) :
        m_ett_text(std::move(text)), m_scanTime(SystemClock::now()) {}

    bool IsStale() const {
        // The minimum recommended repetition time for ETT events according to
        // http://atsc.org/wp-content/uploads/2015/03/Program-and-system-information-protocol-implementation-guidelines-for-broadcaster.pdf
        // is one minute. Consider any ETT event seen > 2 minutes in the past as stale.
        return m_scanTime + 2min < SystemClock::now();
    }

    QString m_ett_text;

  private:
    // The time the ETT was created.
    SystemTime m_scanTime;
};

using EventIDToATSCEvent = QMap<uint,ATSCEvent> ;
using EventIDToETT       = QMap<uint,ATSCEtt>;
using ATSCSRCToEvents    = QMap<uint,EventIDToATSCEvent>;
using ServiceToChanID    = QMap<uint64_t,uint>;

using FixupKey   = uint64_t;
using FixupValue = uint64_t;
using FixupMap   = QMap<FixupKey, FixupValue>;

class DBEventEIT;
class EITFixUp;
class EITCache;

class EventInformationTable;
class ExtendedTextTable;
class DVBEventInformationTable;
class PremiereContentInformationTable;

class EITHelper
{
  public:
    explicit EITHelper(uint cardnum);
    EITHelper &operator=(const EITHelper &) = delete;
    EITHelper(const EITHelper& rhs);
    virtual ~EITHelper(void);

    uint GetListSize(void) const;
    uint ProcessEvents(void);
    bool EventQueueFull(void) const;

    uint GetGPSOffset(void) const { return (uint) (0 - m_gpsOffset); }

    void SetChannelID(uint channelid);
    void SetGPSOffset(uint gps_offset) { m_gpsOffset = 0 - gps_offset; }
    void SetFixup(uint atsc_major, uint atsc_minor, FixupValue eitfixup);
    void SetLanguagePreferences(const QStringList &langPref);
    void SetSourceID(uint sourceid);
    void RescheduleRecordings(void);

#if CONFIG_BACKEND
    void AddEIT(uint atsc_major, uint atsc_minor,
                const EventInformationTable *eit);
    void AddETT(uint atsc_major, uint atsc_minor,
                const ExtendedTextTable     *ett);
    void AddEIT(const DVBEventInformationTable *eit);
    void AddEIT(const PremiereContentInformationTable *cit);
#else // !CONFIG_BACKEND
    void AddEIT(uint /*atsc_major*/, uint /*atsc_minor*/, const EventInformationTable */*eit*/) {}
    void AddETT(uint /*atsc_major*/, uint /*atsc_minor*/, const ExtendedTextTable */*ett*/) {}
    void AddEIT(const DVBEventInformationTable */*eit*/) {}
    void AddEIT(const PremiereContentInformationTable */*cit*/) {}
#endif // !CONFIG_BACKEND

    // EIT cache handling
    static void PruneEITCache(uint timestamp);
    static void WriteEITCache(void);

  private:
    uint GetChanID(uint atsc_major, uint atsc_minor);           // Only ATSC
    uint GetChanID(uint serviceid, uint networkid, uint tsid);  // Only DVB
    uint GetChanID(uint program_number);                        // Any DTV

    void CompleteEvent(uint atsc_major, uint atsc_minor,        // Only ATSC
                       const ATSCEvent &event,
                       const QString   &ett);

    mutable QMutex          m_eitListLock;
    mutable ServiceToChanID m_srvToChanid;

    static EITCache        *s_eitCache;

    int                     m_gpsOffset    {-1 * GPS_LEAP_SECONDS};

    // Carry some values to optimize channel lookup and reschedules
    uint                    m_cardnum      {0};       // Card ID
    uint                    m_sourceid     {0};       // Video source ID
    uint                    m_channelid    {0};       // Channel ID
    QDateTime               m_maxStarttime;           // Latest starttime of changed events
    bool                    m_seenEITother {false};   // If false we only reschedule the active mplex
    uint                    m_chunkSize    {20};      // Maximum number of DB inserts per ProcessEvents call
    uint                    m_queueSize    {1000};    // Maximum number of events waiting to be processed

    FixupMap                m_fixup;
    ATSCSRCToEvents         m_incompleteEvents;

    MythDeque<DBEventEIT*>  m_dbEvents;

    QMap<uint,uint>         m_languagePreferences;

    static const uint       kMaxQueueSize;            // Maximum queue size for events waiting to be processed
};

#endif // EIT_HELPER_H
