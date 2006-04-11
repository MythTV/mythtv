// -*- Mode: c++ -*-

#ifndef EIT_HELPER_H
#define EIT_HELPER_H
#ifdef USING_DVB

// Qt includes
#include <qobject.h>
#include <qstring.h>

// MythTV includes
#include "dvbtypes.h"
#include "sitypes.h"
#include "mythdeque.h"

class MSqlQuery;

class ATSCEvent
{
  public:
    ATSCEvent() {}
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
typedef QMap<unsigned long long,int>       ServiceToChanID;

class EITFixUp;
class EITCache;

class EventInformationTable;
class ExtendedTextTable;
class DVBEventInformationTable;

class EITHelper
{
  public:
    EITHelper();
    virtual ~EITHelper();

    uint GetListSize(void) const;
    uint ProcessEvents(void);

    uint GetGPSOffset(void) const { return (uint) (0 - gps_offset); }

    void SetGPSOffset(uint _gps_offset) { gps_offset = 0 - _gps_offset; }
    void SetFixup(uint atscsrcid, uint eitfixup);
    void SetLanguagePreferences(const QStringList &langPref);
    void SetSourceID(uint _sourceid);

    void AddEIT(uint atscsrcid, const EventInformationTable *eit);
    void AddETT(uint atscsrcid, const ExtendedTextTable     *ett);

    void AddEIT(const DVBEventInformationTable *eit);

  private:
    uint GetChanID(uint atscsrcid);
    uint GetChanID(uint serviceid, uint networkid, uint transportid);

    void CompleteEvent(uint atscsrcid,
                       const ATSCEvent &event,
                       const QString   &ett);

        //QListList_Events  eitList;      ///< Event Information Tables List
    mutable QMutex    eitList_lock; ///< EIT List lock
    mutable ServiceToChanID srv_to_chanid;

    EITFixUp               *eitfixup;
    EITCache               *eitcache;

    int                     gps_offset;
    uint                    sourceid;
    QMap<uint,uint>         fixup;
    ATSCSRCToEvents         incomplete_events;
    ATSCSRCToETTs           unmatched_etts;

    MythDeque<DBEvent*>     db_events;

    QMap<uint,uint>         languagePreferences;

    /// Maximum number of DB inserts per ProcessEvents call.
    static const uint kChunkSize;
};

#endif // USING_DVB

#endif // EIT_HELPER_H
