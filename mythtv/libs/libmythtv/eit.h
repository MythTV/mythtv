#ifndef _EIT_H_
#define _EIT_H_

#include <qmap.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qstringlist.h>

class Person
{
  public:
    Person() {};
    Person(const QString &r, const QString &n) : role(r), name(n) {};

    QString role;
    QString name;
};

/** Used to hold information captured from EIT and ETT tables. */
class Event
{
  public:
    Event() { Reset(); }

    void deepCopy(const Event&);

    void Reset(void);
    void clearEventValues();

    /// Used in ATSC for Checking for ETT PID being filtered
    uint    SourcePID;
    uint    TransportID;
    uint    NetworkID;
    uint    TableID;
    uint    ServiceID;    ///< NOT the Virtual Channel Number used by ATSC
    uint    EventID;
    bool    Stereo;
    bool    HDTV;
    bool    SubTitled;
    int     ETM_Location; ///< Used to flag still waiting ETTs for ATSC
    bool    ATSC;
    uint    PartNumber;   ///< Episode number in series.
    uint    PartTotal;    ///< Number of episodes in series.


    QDateTime   StartTime;
    QDateTime   EndTime;
    QDate       OriginalAirDate;

    QString     LanguageCode;
    QString     Event_Name;
    QString     Event_Subtitle;
    QString     Description;
    QString     ContentDescription;
    QString     Year;
    /// One of these four strings: "movie", "series", "sports" or "tvshow"
    QString     CategoryType;
    QStringList Actors;

    QValueList<Person> Credits;
};
typedef QMap<uint,Event>       QMap_Events;
typedef QMap<uint,QMap_Events> QMap2D_Events;

#endif // _EIT_H_
