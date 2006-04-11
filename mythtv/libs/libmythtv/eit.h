#ifndef _EIT_H_
#define _EIT_H_

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
            bool             _captioned, bool _subtitled,
            bool             _stereo,    bool _hdtv) :
        title(_title),           subtitle(_subtitle),
        description(_desc),
        category(_category),
        starttime(_start),       endtime(_end),
        credits(NULL),
        chanid(_chanid),
        partnumber(0),           parttotal(0),
        fixup(_fixup),           flags(0),
        category_type(_category_type)
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
        category(QString::null),
        starttime(_start),       endtime(_end),
        credits(NULL),
        chanid(_chanid),
        partnumber(0),           parttotal(0),
        fixup(_fixup),           flags(0),
        category_type(0/*kCategoryNone*/)
    {
        flags |= (_captioned) ? kCaptioned : 0;
        flags |= (_stereo)    ? kStereo    : 0;
    }

    ~DBEvent() { if (credits) delete credits; }

    void AddPerson(DBPerson::Role, const QString &name);

    uint UpdateDB(MSqlQuery &query, int match_threshold) const;

    bool IsCaptioned(void) const { return flags & kCaptioned; }
    bool IsSubtitled(void) const { return flags & kSubtitled; }
    bool IsStereo(void)    const { return flags & kStereo;    }
    bool IsHDTV(void)      const { return flags & kHDTV;      }
    bool HasCredits(void)  const { return credits;            }

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
    QDateTime     starttime;
    QDateTime     endtime;
    QDate         originalairdate;
    DBCredits    *credits;
    uint32_t      chanid;
    uint16_t      partnumber;
    uint16_t      parttotal;
    unsigned char fixup;
    unsigned char flags;
    unsigned char category_type;

    static const unsigned char kCaptioned = 0x1;
    static const unsigned char kSubtitled = 0x2;
    static const unsigned char kStereo    = 0x4;
    static const unsigned char kHDTV      = 0x8;
};

#endif // _EIT_H_
