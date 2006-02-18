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

typedef MythDeque<Event*>                  QList_Events;
typedef MythDeque<QList_Events*>           QListList_Events;

class EITHelper : public QObject
{
    Q_OBJECT

  public:
    EITHelper() : QObject(NULL, "EITHelper") { ; }

    void ClearList(void);
    uint GetListSize(void) const;
    uint ProcessEvents(uint sourceid);

  public slots:
    void HandleEITs(QMap_Events* events);

  private:
    int  GetChanID(uint sourceid, const Event &event) const;
    uint UpdateEITList(uint sourceid, const QList_Events &events) const;

    QListList_Events  eitList;      ///< Event Information Tables List
    mutable QMutex    eitList_lock; ///< EIT List lock
    mutable QMap<unsigned long long, uint> srv_to_chanid;

    /// Maximum number of DB inserts per ProcessEvents call.
    static const uint kChunkSize;
};

#endif // USING_DVB

#endif // EIT_HELPER_H
