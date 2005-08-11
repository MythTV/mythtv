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

typedef QValueList<Event> QList_Events;
typedef QValueList<QList_Events*> QListList_Events;

class EITHelper : public QObject
{
    Q_OBJECT
  public:
    EITHelper() : QObject(NULL, "EITHelper") { ; }

    void ClearList(void);
    uint GetListSize(void) const;
    void ProcessEvents(int mplexid);

  public slots:
    void HandleEITs(QMap_Events* events);

  private:
    QListList_Events  eitList;      ///< Event Information Tables List
    mutable QMutex    eitList_lock; ///< EIT List lock
};

#endif // USING_DVB

#endif // EIT_HELPER_H
