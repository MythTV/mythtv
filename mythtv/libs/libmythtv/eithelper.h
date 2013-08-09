// -*- Mode: c++ -*-

#ifndef EIT_HELPER_H
#define EIT_HELPER_H

#include <stdint.h>

// Qt includes
#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>

// MythTV includes
#include "mythdeque.h"

class MSqlQuery;

class ATSCEvent
{
  public:
    /// This empty constructor is needed for the QMap<> to work, it is
    /// not intended to be used to initialize an ATSC Event.
    /// Since we immediately initialize the value inserted into the
    /// QMap this is safe in that use.
    ATSCEvent() : start_time(0), length(0), etm(0), desc_length(0), desc(NULL) {}
    /// This is the only valid constructor for ATSCEvent.
    ATSCEvent(uint a, uint b, uint c, QString d,
              const unsigned char *e, uint f)
        : start_time(a), length(b), etm(c), desc_length(f), title(d), desc(e)
    {
    }

  public:
    uint32_t start_time;
    uint32_t length;
    uint32_t etm;
    uint32_t desc_length;
    QString  title;
    const unsigned char *desc;
};

typedef QMap<uint,ATSCEvent>               EventIDToATSCEvent;
typedef QMap<uint,QString>                 EventIDToETT;
typedef QMap<uint,EventIDToATSCEvent>      ATSCSRCToEvents;
typedef QMap<uint,EventIDToETT>            ATSCSRCToETTs;
typedef QMap<unsigned long long,uint>      ServiceToChanID;

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
    EITHelper();
    virtual ~EITHelper();

    uint GetListSize(void) const;
    uint ProcessEvents(void);

    uint GetGPSOffset(void) const { return (uint) (0 - gps_offset); }

    void SetChannelID(uint _channelid);
    void SetGPSOffset(uint _gps_offset) { gps_offset = 0 - _gps_offset; }
    void SetFixup(uint atsc_major, uint atsc_minor, uint eitfixup);
    void SetLanguagePreferences(const QStringList &langPref);
    void SetSourceID(uint _sourceid);
    void RescheduleRecordings(void);

#ifdef USING_BACKEND
    void AddEIT(uint atsc_major, uint atsc_minor,
                const EventInformationTable *eit);
    void AddETT(uint atsc_major, uint atsc_minor,
                const ExtendedTextTable     *ett);
    void AddEIT(const DVBEventInformationTable *eit);
    void AddEIT(const PremiereContentInformationTable *eit);
#else // if !USING_BACKEND
    void AddEIT(uint, uint, const EventInformationTable*) {}
    void AddETT(uint, uint, const ExtendedTextTable*) {}
    void AddEIT(const DVBEventInformationTable*) {}
    void AddEIT(const PremiereContentInformationTable*) {}
#endif // !USING_BACKEND

    // EIT cache handling
    void PruneEITCache(uint timestamp);
    void WriteEITCache(void);

  private:
    // only ATSC
    uint GetChanID(uint atsc_major, uint atsc_minor);
    // only DVB
    uint GetChanID(uint serviceid, uint networkid, uint transportid);
    // any DTV
    uint GetChanID(uint program_number);

    void CompleteEvent(uint atsc_major, uint atsc_minor,
                       const ATSCEvent &event,
                       const QString   &ett);

        //QListList_Events  eitList;      ///< Event Information Tables List
    mutable QMutex    eitList_lock; ///< EIT List lock
    mutable ServiceToChanID srv_to_chanid;

    EITFixUp               *eitfixup;
    static EITCache        *eitcache;

    int                     gps_offset;

    /* carry some values to optimize channel lookup and reschedules */
    uint                    sourceid;            ///< id of the video source
    uint                    channelid;           ///< id of the channel
    QDateTime               maxStarttime;        ///< latest starttime of changed events
    bool                    seenEITother;        ///< if false we only reschedule the active mplex

    QMap<uint64_t,uint>     fixup;
    ATSCSRCToEvents         incomplete_events;
    ATSCSRCToETTs           unmatched_etts;

    MythDeque<DBEventEIT*>     db_events;

    QMap<uint,uint>         languagePreferences;

    /// Maximum number of DB inserts per ProcessEvents call.
    static const uint kChunkSize;
};

#endif // EIT_HELPER_H
