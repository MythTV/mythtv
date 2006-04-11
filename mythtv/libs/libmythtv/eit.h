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
            const QString   &_category,  const QString   &_category_type,
            const QDateTime &_start,     const QDateTime &_end,
            uint             _fixup,
            bool             _captioned, bool _subtitled,
            bool             _stereo,    bool _hdtv) :
        title(_title),           subtitle(_subtitle),
        description(_desc),
        category(_category),     category_type(_category_type),
        starttime(_start),       endtime(_end),
        chanid(_chanid),         fixup(_fixup),
        flags(0)
    {
        flags |= (_captioned) ? kCaptioned : 0;
        flags |= (_subtitled) ? kSubtitled : 0;
        flags |= (_stereo)    ? kStereo    : 0;
        flags |= (_hdtv)      ? kHDTV      : 0;
    }

    DBEvent(uint             _chanid,
            const QString   &_title,     const QString   &_desc,
            const QDateTime &_start,     const QDateTime &_end,
            uint             _fixup,
            bool             _captioned, bool             _stereo) :
        title(_title),           subtitle(QString::null),
        description(_desc),
        category(QString::null), category_type(QString::null),
        starttime(_start),       endtime(_end),
        chanid(_chanid),         fixup(_fixup),
        flags(0)
    {
        flags |= (_captioned) ? kCaptioned : 0;
        flags |= (_stereo)    ? kStereo    : 0;
    }

    uint UpdateDB(MSqlQuery &query, int match_threshold) const;

    bool IsCaptioned(void) const { return flags & kCaptioned; }
    bool IsSubtitled(void) const { return flags & kSubtitled; }
    bool IsStereo(void)    const { return flags & kStereo;    }
    bool IsHDTV(void)      const { return flags & kHDTV;      }

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
    QString       category;
    QString       category_type;
    QDateTime     starttime;
    QDateTime     endtime;
    uint32_t      chanid;
    unsigned char fixup;
    unsigned char flags;

    static const unsigned char kCaptioned = 0x1;
    static const unsigned char kSubtitled = 0x2;
    static const unsigned char kStereo    = 0x4;
    static const unsigned char kHDTV      = 0x8;
};

#endif // _EIT_H_
