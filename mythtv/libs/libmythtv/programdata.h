#ifndef _PROGRAMDATA_H_
#define _PROGRAMDATA_H_

// C++ headers
#include <cstdint>
#include <vector>
using namespace std;

// Qt headers
#include <QString>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QStringList>

// MythTV headers
#include "mythtvexp.h"
#include "listingsources.h"
#include "programinfo.h"
#include "eithelper.h" /* for FixupValue */

class MSqlQuery;

class MTV_PUBLIC DBPerson
{
  public:
    enum Role
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
    };

    DBPerson(const DBPerson&);
    DBPerson(Role _role, QString _name);
    DBPerson(const QString &_role, QString _name);

    QString GetRole(void) const;

    uint InsertDB(MSqlQuery &query, uint chanid,
                  const QDateTime &starttime) const;

  private:
    uint GetPersonDB(MSqlQuery &query) const;
    uint InsertPersonDB(MSqlQuery &query) const;
    uint InsertCreditsDB(MSqlQuery &query, uint personid, uint chanid,
                         const QDateTime &starttime) const;

  private:
    Role    m_role;
    QString m_name;
};
using DBCredits = vector<DBPerson>;

class MTV_PUBLIC EventRating
{
  public:
    QString m_system;
    QString m_rating;
};

class MTV_PUBLIC DBEvent
{
  public:
    explicit DBEvent(uint listingsource) :
        m_listingsource(listingsource) {}

    DBEvent(const QString   &_title,     const QString   &_subtitle,
            const QString   &_desc,
            const QString   &_category,  ProgramInfo::CategoryType _category_type,
            const QDateTime &_start,     const QDateTime &_end,
            unsigned char    _subtitleType,
            unsigned char    _audioProps,
            unsigned char    _videoProps,
            float            _stars,
            const QString   &_seriesId,  const QString   &_programId,
            uint32_t         _listingsource,
            uint _season,                uint _episode,
            uint _totalepisodes ) :
        m_title(_title),           m_subtitle(_subtitle),
        m_description(_desc),
        m_category(_category),
        m_starttime(_start),       m_endtime(_end),
        m_subtitleType(_subtitleType),
        m_audioProps(_audioProps), m_videoProps(_videoProps),
        m_stars(_stars),
        m_categoryType(_category_type),
        m_seriesId(_seriesId),
        m_programId(_programId),
        m_listingsource(_listingsource),
        m_season(_season),
        m_episode(_episode),
        m_totalepisodes(_totalepisodes)
    {
    }

    virtual ~DBEvent() { delete m_credits; }

    void AddPerson(DBPerson::Role, const QString &name);
    void AddPerson(const QString &role, const QString &name);

    uint UpdateDB(MSqlQuery &query, uint chanid, int match_threshold) const;

    bool HasCredits(void) const { return m_credits; }
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
        MSqlQuery&, uint chanid, const DBEvent &prog) const;
    virtual uint InsertDB(MSqlQuery&, uint chanid) const;
    virtual void Squeeze(void);

  public:
    QString                   m_title;
    QString                   m_subtitle;
    QString                   m_description;
    QString                   m_category;
    QDateTime                 m_starttime;
    QDateTime                 m_endtime;
    uint16_t                  m_airdate         {0}; ///< movie year / production year
    QDate                     m_originalairdate;     ///< origial broadcast date
    DBCredits                *m_credits         {nullptr};
    uint16_t                  m_partnumber      {0};
    uint16_t                  m_parttotal       {0};
    QString                   m_syndicatedepisodenumber;
    unsigned char             m_subtitleType    {0};
    unsigned char             m_audioProps      {0};
    unsigned char             m_videoProps      {0};
    float                     m_stars           {0.0};
    ProgramInfo::CategoryType m_categoryType    {ProgramInfo::kCategoryNone};
    QString                   m_seriesId;
    QString                   m_programId;
    QString                   m_inetref;
    bool                      m_previouslyshown {false};
    uint32_t                  m_listingsource;
    QList<EventRating>        m_ratings;
    QStringList               m_genres;
    uint                      m_season          {0};
    uint                      m_episode         {0};
    uint                      m_totalepisodes   {0};
};

class MTV_PUBLIC DBEventEIT : public DBEvent
{
  public:
    DBEventEIT(uint             _chanid,
               const QString   &_title,     const QString   &_subtitle,
               const QString   &_desc,
               const QString   &_category,  ProgramInfo::CategoryType _category_type,
               const QDateTime &_start,     const QDateTime &_end,
               FixupValue       _fixup,
               unsigned char    _subtitleType,
               unsigned char    _audioProps,
               unsigned char    _videoProps,
               float            _stars,
               const QString   &_seriesId,  const QString   &_programId,
               uint _season,                uint _episode,
               uint _totalepisodes ) :
        DBEvent(_title, _subtitle, _desc, _category, _category_type,
                _start, _end, _subtitleType, _audioProps, _videoProps,
                _stars, _seriesId, _programId, kListingSourceEIT,
                _season, _episode, _totalepisodes),
        m_chanid(_chanid), m_fixup(_fixup)
    {
    }

    DBEventEIT(uint             _chanid,
               const QString   &_title,     const QString   &_desc,
               const QDateTime &_start,     const QDateTime &_end,
               FixupValue       _fixup,
               unsigned char    _subtitleType,
               unsigned char    _audioProps,
               unsigned char    _videoProps) :
        DBEvent(_title, QString(), _desc, QString(), ProgramInfo::kCategoryNone,
                _start, _end, _subtitleType, _audioProps, _videoProps,
                0.0, QString(), QString(), kListingSourceEIT, 0, 0, 0), // Season, Episode and Total Episodes are not set with this constructor!
        m_chanid(_chanid), m_fixup(_fixup)
    {
    }

    uint UpdateDB(MSqlQuery &query, int match_threshold) const
    {
        return DBEvent::UpdateDB(query, m_chanid, match_threshold);
    }

  public:
    uint32_t              m_chanid;
    FixupValue            m_fixup;
    QMap<QString,QString> m_items;
};

class MTV_PUBLIC ProgInfo : public DBEvent
{
  public:
    ProgInfo() :
        DBEvent(kListingSourceXMLTV) { }

    ProgInfo(const ProgInfo &other);

    uint InsertDB(MSqlQuery &query, uint chanid) const override; // DBEvent

    void Squeeze(void) override; // DBEvent

    ProgInfo &operator=(const ProgInfo&);

  public:
    // extra XMLTV stuff
    QString       m_channel;
    QString       m_startts;
    QString       m_endts;
    QString       m_title_pronounce;
    QString       m_showtype;
    QString       m_colorcode;
    QString       m_clumpidx;
    QString       m_clumpmax;
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
