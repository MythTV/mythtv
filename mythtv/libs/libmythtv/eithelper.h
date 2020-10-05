// -*- Mode: c++ -*-

#ifndef EIT_HELPER_H
#define EIT_HELPER_H

// C+ headers
#include <cstdint>
#include <ctime>
#include <utility>

// Qt includes
#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>

// MythTV includes
#include "mythdeque.h"
#include "mpegtables.h" // for GPS_LEAP_SECONDS

class MSqlQuery;

// An entry from the EIT table containing event details.
class ATSCEvent
{
  public:
    ATSCEvent(uint a, uint b, uint c, QString  d,
              const unsigned char *e, uint f)
        : m_startTime(a), m_length(b), m_etm(c), m_descLength(f),
          m_title(std::move(d)), m_desc(e),
          m_scanTime(time(nullptr)) {}

    bool IsStale() const {
        // The minimum recommended repetition time for EIT events according to
        // http://atsc.org/wp-content/uploads/2015/03/Program-and-system-information-protocol-implementation-guidelines-for-broadcaster.pdf
        // is one minute. Consider any EIT event seen > 2 minutes in the past as stale.
        return m_scanTime + 2 * 60 < time(nullptr);
    }

    uint32_t             m_startTime;
    uint32_t             m_length;
    uint32_t             m_etm;
    uint32_t             m_descLength;
    QString              m_title;
    const unsigned char *m_desc {nullptr};

  private:
    // The time the event was created.
    time_t               m_scanTime;
};

// An entry from the ETT table containing description text for an event.
class ATSCEtt
{
  public:
    explicit ATSCEtt(QString  text) :
        m_ett_text(std::move(text)), m_scanTime(time(nullptr)) {}

    bool IsStale() const {
        // The minimum recommended repetition time for ETT events according to
        // http://atsc.org/wp-content/uploads/2015/03/Program-and-system-information-protocol-implementation-guidelines-for-broadcaster.pdf
        // is one minute. Consider any ETT event seen > 2 minutes in the past as stale.
        return m_scanTime + 2 * 60 < time(nullptr);
    }

    QString m_ett_text;

  private:
    // The time the ETT was created.
    time_t m_scanTime;
};

using EventIDToATSCEvent = QMap<uint,ATSCEvent> ;
using EventIDToETT       = QMap<uint,ATSCEtt>;
using ATSCSRCToEvents    = QMap<uint,EventIDToATSCEvent>;
using ServiceToChanID    = QMap<unsigned long long,uint>;

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
    EITHelper(void);
    EITHelper(const EITHelper& rhs);
    virtual ~EITHelper(void);

    uint GetListSize(void) const;
    uint ProcessEvents(void);

    uint GetGPSOffset(void) const { return (uint) (0 - m_gpsOffset); }

    void SetChannelID(uint _channelid);
    void SetGPSOffset(uint _gps_offset) { m_gpsOffset = 0 - _gps_offset; }
    void SetFixup(uint atsc_major, uint atsc_minor, FixupValue eitfixup);
    void SetLanguagePreferences(const QStringList &langPref);
    void SetSourceID(uint _sourceid);
    void RescheduleRecordings(void);

#ifdef USING_BACKEND
    void AddEIT(uint atsc_major, uint atsc_minor,
                const EventInformationTable *eit);
    void AddETT(uint atsc_major, uint atsc_minor,
                const ExtendedTextTable     *ett);
    void AddEIT(const DVBEventInformationTable *eit);
    void AddEIT(const PremiereContentInformationTable *cit);
#else // if !USING_BACKEND
    void AddEIT(uint /*atsc_major*/, uint /*atsc_minor*/, const EventInformationTable */*eit*/) {}
    void AddETT(uint /*atsc_major*/, uint /*atsc_minor*/, const ExtendedTextTable */*ett*/) {}
    void AddEIT(const DVBEventInformationTable */*eit*/) {}
    void AddEIT(const PremiereContentInformationTable */*cit*/) {}
#endif // !USING_BACKEND

    // EIT cache handling
    static void PruneEITCache(uint timestamp);
    static void WriteEITCache(void);

  private:
    EITHelper &operator=(const EITHelper &) = delete;

    // only ATSC
    uint GetChanID(uint atsc_major, uint atsc_minor);
    // only DVB
    uint GetChanID(uint serviceid, uint networkid, uint tsid);
    // any DTV
    uint GetChanID(uint program_number);

    void CompleteEvent(uint atsc_major, uint atsc_minor,
                       const ATSCEvent &event,
                       const QString   &ett);

        //QListList_Events  m_eitList;     ///< Event Information Tables List
    mutable QMutex          m_eitListLock; ///< EIT List lock
    mutable ServiceToChanID m_srvToChanid;

    EITFixUp               *m_eitFixup     {nullptr};
    static EITCache        *s_eitCache;

    int                     m_gpsOffset    {-1 * GPS_LEAP_SECONDS};

    /* carry some values to optimize channel lookup and reschedules */
    uint                    m_sourceid     {0};    ///< id of the video source
    uint                    m_channelid    {0};    ///< id of the channel
    QDateTime               m_maxStarttime;        ///< latest starttime of changed events
    bool                    m_seenEITother {false};///< if false we only reschedule the active mplex

    FixupMap                m_fixup;
    ATSCSRCToEvents         m_incompleteEvents;

    MythDeque<DBEventEIT*>  m_dbEvents;

    QMap<uint,uint>         m_languagePreferences;

    /// Maximum number of DB inserts per ProcessEvents call.
    static const uint kChunkSize;
};

#endif // EIT_HELPER_H
