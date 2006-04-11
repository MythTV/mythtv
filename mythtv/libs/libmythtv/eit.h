#ifndef _EIT_H_
#define _EIT_H_

#include <vector>
using namespace std;

#include <qmap.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qstringlist.h>

class MSqlQuery;

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

class DBEvent
{
  public:
    DBEvent(uint             _chanid,
            const QString   &_title,     const QString   &_subtitle,
            const QString   &_desc,
            const QDateTime &_start,     const QDateTime &_end,
            uint             _fixup,
            bool             _captioned, bool             _stereo) :
        title(_title),           subtitle(_subtitle),
        description(_desc),
        starttime(_start),       endtime(_end),
        chanid(_chanid),         fixup(_fixup),
        captioned(_captioned),   stereo(_stereo)
    {
    }
    DBEvent(uint             _chanid,
            const QString   &_title,     const QString   &_desc,
            const QDateTime &_start,     const QDateTime &_end,
            uint             _fixup,
            bool             _captioned, bool             _stereo) :
        title(_title),           subtitle(QString::null),
        description(_desc),
        starttime(_start),       endtime(_end),
        chanid(_chanid),         fixup(_fixup),
        captioned(_captioned),   stereo(_stereo)
    {
    }
    uint UpdateDB(MSqlQuery &query, int match_threshold) const;

  private:
    uint GetOverlappingPrograms(MSqlQuery&, vector<DBEvent> &programs) const;
    int  GetMatch(const vector<DBEvent> &programs, int &bestmatch) const;
    uint UpdateDB(MSqlQuery&, const vector<DBEvent> &p, int match) const;
    uint UpdateDB(MSqlQuery&, const DBEvent &match) const;
    bool MoveOutOfTheWayDB(MSqlQuery&, const DBEvent &nonmatch) const;
    uint InsertDB(MSqlQuery&) const;

  public:
    QString       title;
    QString       subtitle;
    QString       description;
    QDateTime     starttime;
    QDateTime     endtime;
    uint32_t      chanid;
    unsigned char fixup;
    bool          captioned;
    bool          stereo;
};

#endif // _EIT_H_
