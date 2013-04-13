#ifndef _PROGRAMDATA_H_
#define _PROGRAMDATA_H_

// C++ headers
#include <vector>
using namespace std;

// POSIX headers
#include <stdint.h>

// Qt headers
#include <QString>
#include <QDateTime>
#include <QList>
#include <QMap>

// MythTV headers
#include "mythtvexp.h"
#include "listingsources.h"
#include "programinfo.h"

class MSqlQuery;

class MTV_PUBLIC DBPerson
{
  public:
    typedef enum
    {
        kUnknown = 0,
        kActor,
        kDirector,
        kProducer,
        kExecutiveProducer,
        kWriter,
        kGuestStar,
        kHost,
        kAdapter,
        kPresenter,
        kCommentator,
        kGuest,
    } Role;

    DBPerson(const DBPerson&);
    DBPerson(Role _role, const QString &_name);
    DBPerson(const QString &_role, const QString &_name);

    QString GetRole(void) const;

    uint InsertDB(MSqlQuery &query, uint chanid,
                  const QDateTime &starttime) const;

  private:
    uint GetPersonDB(MSqlQuery &query) const;
    uint InsertPersonDB(MSqlQuery &query) const;
    uint InsertCreditsDB(MSqlQuery &query, uint personid, uint chanid,
                         const QDateTime &starttime) const;

  private:
    Role    role;
    QString name;
};
typedef vector<DBPerson> DBCredits;

class MTV_PUBLIC EventRating
{
  public:
    QString system;
    QString rating;
};

class MTV_PUBLIC DBEvent
{
  public:
    DBEvent(uint _listingsource) :
        title(),
        subtitle(),
        description(),
        category(),
        /*starttime, endtime,*/
        airdate(0),
        credits(NULL),
        partnumber(0),
        parttotal(0),
        syndicatedepisodenumber(),
        subtitleType(0),
        audioProps(0),
        videoProps(0),
        stars(0.0),
        categoryType(ProgramInfo::kCategoryNone),
        seriesId(),
        programId(),
        previouslyshown(false),
        listingsource(_listingsource) {}

    DBEvent(const QString   &_title,     const QString   &_subtitle,
            const QString   &_desc,
            const QString   &_category,  ProgramInfo::CategoryType _category_type,
            const QDateTime &_start,     const QDateTime &_end,
            unsigned char    _subtitleType,
            unsigned char    _audioProps,
            unsigned char    _videoProps,
            float            _stars,
            const QString   &_seriesId,  const QString   &_programId,
            uint32_t         _listingsource) :
        title(_title),           subtitle(_subtitle),
        description(_desc),
        category(_category),
        starttime(_start),       endtime(_end),
        airdate(0),
        credits(NULL),
        partnumber(0),           parttotal(0),
        syndicatedepisodenumber(),
        subtitleType(_subtitleType),
        audioProps(_audioProps), videoProps(_videoProps),
        stars(_stars),
        categoryType(_category_type),
        seriesId(_seriesId),
        programId(_programId),
        previouslyshown(false),
        listingsource(_listingsource)
    {
    }

    virtual ~DBEvent() { delete credits; }

    void AddPerson(DBPerson::Role, const QString &name);
    void AddPerson(const QString &role, const QString &name);

    uint UpdateDB(MSqlQuery &query, uint chanid, int match_threshold) const;

    bool HasCredits(void) const { return credits; }
    bool HasTimeConflict(const DBEvent &other) const;

    DBEvent &operator=(const DBEvent&);

  protected:
    uint GetOverlappingPrograms(
        MSqlQuery&, uint chanid, vector<DBEvent> &programs) const;
    int  GetMatch(
        const vector<DBEvent> &programs, int &bestmatch) const;
    uint UpdateDB(
        MSqlQuery&, uint chanid, const vector<DBEvent> &p, int match) const;
    uint UpdateDB(
        MSqlQuery&, uint chanid, const DBEvent &match) const;
    bool MoveOutOfTheWayDB(
        MSqlQuery&, uint chanid, const DBEvent &nonmatch) const;
    virtual uint InsertDB(MSqlQuery&, uint chanid) const;
    virtual void Squeeze(void);

  public:
    QString       title;
    QString       subtitle;
    QString       description;
    QString       category;
    QDateTime     starttime;
    QDateTime     endtime;
    uint16_t      airdate;          ///< movie year / production year
    QDate         originalairdate;  ///< origial broadcast date
    DBCredits    *credits;
    uint16_t      partnumber;
    uint16_t      parttotal;
    QString       syndicatedepisodenumber;
    unsigned char subtitleType;
    unsigned char audioProps;
    unsigned char videoProps;
    float         stars;
    ProgramInfo::CategoryType categoryType;
    QString       seriesId;
    QString       programId;
    bool          previouslyshown;
    uint32_t      listingsource;
    QList<EventRating> ratings;
};

class MTV_PUBLIC DBEventEIT : public DBEvent
{
  public:
    DBEventEIT(uint             _chanid,
               const QString   &_title,     const QString   &_subtitle,
               const QString   &_desc,
               const QString   &_category,  ProgramInfo::CategoryType _category_type,
               const QDateTime &_start,     const QDateTime &_end,
               uint             _fixup,
               unsigned char    _subtitleType,
               unsigned char    _audioProps,
               unsigned char    _videoProps,
               float            _stars,
               const QString   &_seriesId,  const QString   &_programId) :
        DBEvent(_title, _subtitle, _desc, _category, _category_type,
                _start, _end, _subtitleType, _audioProps, _videoProps,
                _stars, _seriesId, _programId, kListingSourceEIT),
        chanid(_chanid), fixup(_fixup)
    {
    }

    DBEventEIT(uint             _chanid,
               const QString   &_title,     const QString   &_desc,
               const QDateTime &_start,     const QDateTime &_end,
               uint             _fixup,
               unsigned char    _subtitleType,
               unsigned char    _audioProps,
               unsigned char    _videoProps) :
        DBEvent(_title, QString(), _desc, QString(), ProgramInfo::kCategoryNone,
                _start, _end, _subtitleType, _audioProps, _videoProps,
                0.0, QString(), QString(), kListingSourceEIT),
        chanid(_chanid), fixup(_fixup)
    {
    }

    uint UpdateDB(MSqlQuery &query, int match_threshold) const
    {
        return DBEvent::UpdateDB(query, chanid, match_threshold);
    }

  public:
    uint32_t      chanid;
    uint32_t      fixup;
};

class MTV_PUBLIC ProgInfo : public DBEvent
{
  public:
    ProgInfo() :
        DBEvent(kListingSourceXMLTV),
        // extra XMLTV stuff
        channel(QString::null),
        startts(QString::null),
        endts(QString::null),
        stars(QString::null),
        title_pronounce(QString::null),
        showtype(QString::null),
        colorcode(QString::null),
        clumpidx(QString::null),
        clumpmax(QString::null) { }

    ProgInfo(const ProgInfo &other);

    uint InsertDB(MSqlQuery &query, uint chanid) const;

    void Squeeze(void);

    ProgInfo &operator=(const ProgInfo&);

  public:
    // extra XMLTV stuff
    QString       channel;
    QString       startts;
    QString       endts;
    QString       stars;
    QString       title_pronounce;
    QString       showtype;
    QString       colorcode;
    QString       clumpidx;
    QString       clumpmax;
};

class MTV_PUBLIC ProgramData
{
  public:
    static void HandlePrograms(uint sourceid,
                               QMap<QString, QList<ProgInfo> > &proglist);

    static int  fix_end_times(void);
    static bool ClearDataByChannel(
        uint chanid,
        const QDateTime &from,
        const QDateTime &to,
        bool use_channel_time_offset);
    static bool ClearDataBySource(
        uint sourceid,
        const QDateTime &from,
        const QDateTime &to,
        bool use_channel_time_offset);

  private:
    static void FixProgramList(QList<ProgInfo*> &fixlist);
    static void HandlePrograms(
        MSqlQuery &query, uint chanid,
        const QList<ProgInfo*> &sortlist,
        uint &unchanged, uint &updated);
    static bool IsUnchanged(
        MSqlQuery &query, uint chanid, const ProgInfo &pi);
    static bool DeleteOverlaps(
        MSqlQuery &query, uint chanid, const ProgInfo &pi);
};

#endif // _PROGRAMDATA_H_
