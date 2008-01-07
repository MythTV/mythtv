#ifndef _EIT_H_
#define _EIT_H_

#include <stdint.h>
#include <vector>
using namespace std;

#include <qmap.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qstringlist.h>

class MSqlQuery;

class DBPerson
{
  public:
    typedef enum
    {
        kActor = 0,
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

    DBPerson(Role _role, const QString &_name);

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

class DBEvent
{
  public:
    DBEvent(uint             _chanid,
            const QString   &_title,     const QString   &_subtitle,
            const QString   &_desc,
            const QString   &_category,  uint             _category_type,
            const QDateTime &_start,     const QDateTime &_end,
            uint             _fixup,
            unsigned char    _subtitleType,
            unsigned char    _audioProps,
            unsigned char    _videoProps,
            const QString   &_seriesId,  const QString   &_programId) :
        title(_title),           subtitle(_subtitle),
        description(_desc),
        category(_category),
        starttime(_start),       endtime(_end),
        airdate(QString::null),
        credits(NULL),
        chanid(_chanid),
        partnumber(0),           parttotal(0),
        syndicatedepisodenumber(QString::null),
        fixup(_fixup),
        subtitleType(_subtitleType),
        audioProps(_audioProps), videoProps(_videoProps),
        category_type(_category_type),
        seriesId(_seriesId),
        programId(_programId),
        previouslyshown(false)
    {
    }

    DBEvent(uint             _chanid,
            const QString   &_title,     const QString   &_desc,
            const QDateTime &_start,     const QDateTime &_end,
            uint             _fixup,
            unsigned char    _subtitleType,
            unsigned char    _audioProps,
            unsigned char    _videoProps) :
        title(_title),           subtitle(QString::null),
        description(_desc),
        category(QString::null),
        starttime(_start),       endtime(_end),
        airdate(QString::null),
        credits(NULL),
        chanid(_chanid),
        partnumber(0),           parttotal(0),
        syndicatedepisodenumber(QString::null),
        fixup(_fixup),
        subtitleType(_subtitleType),
        audioProps(_audioProps), videoProps(_videoProps),
        category_type(0/*kCategoryNone*/), previouslyshown(false)
    {
    }

    ~DBEvent() { if (credits) delete credits; }

    void AddPerson(DBPerson::Role, const QString &name);

    uint UpdateDB(MSqlQuery &query, int match_threshold) const;

    bool HasCredits(void)  const { return credits;            }

  private:
    uint GetOverlappingPrograms(MSqlQuery&, vector<DBEvent> &programs) const;
    int  GetMatch(const vector<DBEvent> &programs, int &bestmatch) const;
    uint UpdateDB(MSqlQuery&, const vector<DBEvent> &p, int match) const;
    uint UpdateDB(MSqlQuery&, const DBEvent &match) const;
    bool MoveOutOfTheWayDB(MSqlQuery&, const DBEvent &nonmatch) const;
    uint InsertDB(MSqlQuery&) const;
    QString AddAuthority(const QString &, MSqlQuery &) const;

  public:
    QString       title;
    QString       subtitle;
    QString       description;
    QString       category;
    QDateTime     starttime;
    QDateTime     endtime;
    QString       airdate;          ///< movie year / production year
    QDate         originalairdate;  ///< origial broadcast date
    DBCredits    *credits;
    uint32_t      chanid;
    uint16_t      partnumber;
    uint16_t      parttotal;
    QString       syndicatedepisodenumber;
    uint32_t      fixup;
    unsigned char subtitleType;
    unsigned char audioProps;
    unsigned char videoProps;
    unsigned char category_type;
    QString       seriesId;
    QString       programId;
    bool          previouslyshown;
};

#endif // _EIT_H_
