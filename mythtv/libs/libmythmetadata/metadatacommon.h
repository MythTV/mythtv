#ifndef METADATACOMMON_H_
#define METADATACOMMON_H_

// c++
#include <utility>

// qt
#include <QDate>
#include <QDateTime>
#include <QDomElement>
#include <QEvent>
#include <QList>
#include <QMultiMap>
#include <QPair>
#include <QString>
#include <QStringList>

#include "libmythbase/mythchrono.h"
#include "libmythbase/mythtypes.h"
#include "libmythbase/referencecounterlist.h"
#include "libmythmetadata/metadatagrabber.h"
#include "libmythmetadata/mythmetaexp.h"
#include "libmythtv/metadataimagehelper.h"

class ProgramInfo;

enum LookupStep : std::uint8_t {
    kLookupSearch = 0,
    kLookupData = 1,
    kLookupCollection = 2
};

struct PersonInfo
{
    QString name;
    QString role;
    QString thumbnail;
    QString url;
};

// What type of grabber script to use
enum MetadataType : std::uint8_t {
    kMetadataVideo = 0,
    kMetadataRecording = 1,
    kMetadataMusic = 2,
    kMetadataGame = 3
};

// Determines the lookup fallback strategy
enum LookupType : std::uint8_t {
    kProbableTelevision = 0,
    kProbableGenericTelevision = 1,
    kProbableMovie = 2,
    kUnknownVideo = 3,
    kProbableMusic = 4,
    kProbableGame = 5
};

// Actual type of content
enum VideoContentType : std::uint8_t {
    kContentMovie = 0,
    kContentTelevision = 1,
    kContentAdult = 2,
    kContentMusicVideo = 3,
    kContentHomeMovie = 4,
    kContentUnknown = 5
};

enum PeopleType : std::uint8_t {
    kPersonActor = 0,
    kPersonAuthor = 1,
    kPersonDirector = 2,
    kPersonProducer = 3,
    kPersonExecProducer = 4,
    kPersonCinematographer = 5,
    kPersonComposer = 6,
    kPersonEditor = 7,
    kPersonCastingDirector = 8,
    kPersonArtist = 9,
    kPersonAlbumArtist = 10,
    kPersonGuestStar = 11
};

using DownloadMap = QMap< VideoArtworkType, ArtworkInfo >;
using PeopleMap   = QMultiMap< PeopleType, PersonInfo >;

class META_PUBLIC MetadataLookup : public QObject, public ReferenceCounter
{
  public:
    MetadataLookup(void) : ReferenceCounter("MetadataLookup") {}
    ~MetadataLookup() override = default;

    MetadataLookup(
        MetadataType type,
        LookupType subtype,
        QVariant data,
        LookupStep step,
        bool automatic,
        bool handleimages,
        bool allowoverwrites,
        bool allowgeneric,
        bool preferdvdorder,
        QString host,
        QString filename,
        const QString &title,
        QString network,
        QString status,
        QStringList categories,
        float userrating,
        uint ratingcount,
        QString language,
        QString subtitle,
        QString tagline,
        QString description,
        uint season,
        uint episode,
        uint chanid,
        QString channum,
        QString chansign,
        QString channame,
        QString chanplaybackfilters,
        QString recgroup,
        QString playgroup,
        QString seriesid,
        QString programid,
        QString storagegroup,
        QDateTime startts,
        QDateTime endts,
        QDateTime recstartts,
        QDateTime recendts,
        uint programflags,
        uint audioproperties,
        uint videoproperties,
        uint subtitletype,
        QString certification,
        QStringList countries,
        float popularity,
        uint budget,
        uint revenue,
        QString album,
        uint tracknum,
        QString system,
        uint year,
        QDate releasedate,
        QDateTime lastupdated,
        std::chrono::minutes runtime,
        std::chrono::seconds runtimesecs,
        QString inetref,
        QString collectionref,
        QString tmsref,
        QString imdb,
        PeopleMap  people,
        QStringList studios,
        QString homepage,
        QString trailerURL,
        ArtworkMap  artwork,
        DownloadMap downloads);

    //ProgramInfo Constructor
    MetadataLookup(
        MetadataType type,
        LookupType subtype,
        QVariant data,
        LookupStep step,
        bool automatic,
        bool handleimages,
        bool allowoverwrites,
        bool allowgeneric,
        bool preferdvdorder,
        QString host,
        QString filename,
        const QString &title,
        QStringList categories,
        float userrating,
        QString subtitle,
        QString description,
        uint chanid,
        QString channum,
        QString chansign,
        QString channame,
        QString chanplaybackfilters,
        QString recgroup,
        QString playgroup,
        QString seriesid,
        QString programid,
        QString storagegroup,
        QDateTime startts,
        QDateTime endts,
        QDateTime recstartts,
        QDateTime recendts,
        uint programflags,
        uint audioproperties,
        uint videoproperties,
        uint subtitletype,
        uint year,
        QDate releasedate,
        QDateTime lastupdated,
        std::chrono::minutes runtime,
        std::chrono::seconds runtimesecs);

    // XBMC NFO Constructor
    MetadataLookup(
        MetadataType type,
        LookupType subtype,
        QVariant data,
        LookupStep step,
        bool automatic,
        bool handleimages,
        bool allowoverwrites,
        bool allowgeneric,
        bool preferdvdorder,
        QString host,
        QString filename,
        const QString &title,
        QStringList categories,
        float userrating,
        QString subtitle,
        QString tagline,
        QString description,
        uint season,
        uint episode,
        QString certification,
        uint year,
        QDate releasedate,
        std::chrono::minutes runtime,
        std::chrono::seconds runtimesecs,
        QString inetref,
        PeopleMap  people,
        QString trailerURL,
        ArtworkMap  artwork,
        DownloadMap downloads);

    void toMap(InfoMap &map);

    // SETS (Only a limited number needed)

    // Must set a type, data, and step.
    void SetType(MetadataType type) { m_type = type; };
    // For some lookup, it helps to know the subtype (TV vs. Movie)
    void SetSubtype(LookupType subtype) { m_subtype = subtype; };
    // Reference value- when the event comes back, need to associate with an item.
    void SetData(QVariant data) { m_data = std::move(data); };
    // Steps: SEARCH, GETDATA
    void SetStep(LookupStep step) { m_step = step; };
    // Don't prompt the user, just make an educated decision.
    void SetAutomatic(bool autom) { m_automatic = autom; };

    // Sets for image download handling
    void SetHandleImages(bool handle) { m_handleImages = handle; };
    void SetAllowOverwrites(bool allow) { m_allowOverwrites = allow; };
    void SetAllowGeneric(bool allow) { m_allowGeneric = allow; };
    void SetHost(const QString &host) { m_host = host; };
    void SetDownloads(DownloadMap map) { m_downloads = std::move(map); };

    // General Sets
    void SetTitle(const QString &title)
    {
        m_title = title;
        QString manRecSuffix = QString(" (%1)").arg(QObject::tr("Manual Record"));
        m_baseTitle = title;  // m_title with " (Manual Record)" stripped.
        m_baseTitle.replace(manRecSuffix,"");
    };
    void SetFilename(const QString &filename) { m_filename = filename; };

    // General Sets - Video
    void SetSubtitle(const QString &subtitle) { m_subtitle = subtitle; };
    void SetSeason(uint season) { m_season = season; };
    void SetEpisode(uint episode) { m_episode = episode; };
    void SetInetref(const QString &inetref) { m_inetRef = inetref; };
    void SetCollectionref(const QString &collectionref)
                             { m_collectionRef = collectionref; };
    void SetTMSref(const QString &tmsref) { m_tmsRef = tmsref; };
    void SetIsCollection(bool collection) { m_isCollection = collection; };
    void SetPreferDVDOrdering(bool preferdvdorder)
                             { m_dvdOrder = preferdvdorder; };

    // General Sets - Music
    void SetAlbum(const QString &album) { m_album = album; };
    void SetTrack(uint track) { m_trackNum = track; };

    // General Sets - Games
    void SetSystem(const QString &system) { m_system = system; };

    // GETS

    MetadataType GetType() const { return m_type; };
    LookupType GetSubtype() const { return m_subtype; };
    QVariant GetData() const { return m_data; };
    LookupStep GetStep() const { return m_step; };
    bool GetAutomatic() const { return m_automatic; };

    // Image Handling Gets
    bool GetHandleImages() const { return m_handleImages; };
    bool GetAllowOverwrites() const { return m_allowOverwrites; };
    bool GetAllowGeneric() const { return m_allowGeneric; };

    // General
    QString GetFilename() const { return m_filename; };
    QString GetTitle() const { return m_title; };
    QString GetBaseTitle() const { return m_baseTitle; };
    QStringList GetCategories() const { return m_categories; };
    float GetUserRating() const { return m_userRating; };
    uint GetRatingCount() const { return m_ratingCount; };
    QString GetLanguage() const { return m_language; };
    QString GetHost() const { return m_host; };

    // General - Video & ProgramInfo
    QString GetNetwork() const { return m_network; };
    QString GetStatus() const { return m_status; };
    QString GetSubtitle() const { return m_subtitle; };
    QString GetTagline() const { return m_tagline; };
    QString GetDescription() const { return m_description; };
    bool GetPreferDVDOrdering() const { return m_dvdOrder; };
    uint GetSeason() const { return m_season; };
    uint GetEpisode() const { return m_episode; };
    uint GetChanId() const { return m_chanId; };
    QString GetChanNum() const { return m_chanNum; };
    QString GetChanSign() const { return m_chanSign; };
    QString GetChanName() const { return m_chanName; };
    QString GetChanPlaybackFilters() const { return m_chanPlaybackFilters; };
    QString GetRecGroup() const { return m_recGroup; };
    QString GetPlayGroup() const { return m_playGroup; };
    QString GetSeriesId() const { return m_seriesId; };
    QString GetProgramId() const { return m_programid; };
    QString GetStorageGroup() const { return m_storageGroup; };
    QDateTime GetStartTS() const { return m_startTs; };
    QDateTime GetEndTS() const { return m_endTs; };
    QDateTime GetRecStartTS() const { return m_recStartTs; };
    QDateTime GetRecEndTS() const { return m_recEndTs; };
    uint GetProgramFlags() const { return m_programFlags; };
    uint GetAudioProperties() const { return m_audioProperties; };
    uint GetVideoProperties() const { return m_videoProperties; };
    uint GetSubtitleType() const { return m_subtitleType; };

    QString GetCertification() const { return m_certification; };
    QStringList GetCountries() const { return m_countries; };
    float GetPopularity() const { return m_popularity; };
    uint GetBudget() const { return m_budget; };
    uint GetRevenue() const { return m_revenue; };

    // General - Music
    QString GetAlbumTitle() const { return m_album; };
    uint GetTrackNumber() const { return m_trackNum; };

    // General - Game
    QString GetSystem() const { return m_system; };

    // Times
    uint GetYear() const { return m_year; };
    QDate GetReleaseDate() const { return m_releaseDate; };
    QDateTime GetLastUpdated() const { return m_lastUpdated; };
    std::chrono::minutes GetRuntime() const { return m_runtime; };
    std::chrono::seconds GetRuntimeSeconds() const { return m_runtimeSecs; };

    // Inetref
    QString GetInetref() const { return m_inetRef; };
    QString GetCollectionref() const { return m_collectionRef; };
    QString GetIMDB() const { return m_imdb; };
    QString GetTMSref() const { return m_tmsRef; };
    bool    GetIsCollection() const { return m_isCollection; };

    // People
    QList<PersonInfo> GetPeople(PeopleType type) const;
    QStringList GetStudios() const { return m_studios; };

    // Web references
    QString GetHomepage() const { return m_homepage; };
    QString GetTrailerURL() const { return m_trailerURL; };

    // Artwork
    ArtworkList GetArtwork(VideoArtworkType type) const;
    DownloadMap GetDownloads() const { return m_downloads; };

  private:
    // General
    MetadataType m_type     {kMetadataVideo};
    LookupType m_subtype    {kUnknownVideo};
    QVariant m_data;
    LookupStep m_step       {kLookupSearch};
    bool m_automatic        {false};
    bool m_handleImages     {false};
    bool m_allowOverwrites  {false};
    bool m_allowGeneric     {false};
    bool m_dvdOrder         {false};
    QString m_host;

    QString m_filename;
    QString m_title;
    QString m_baseTitle;  // m_title with " (Manual Record)" stripped.
    QString m_network;
    QString m_status;
    const QStringList m_categories;
    float m_userRating      {0.0};
    uint m_ratingCount      {0};
    const QString m_language;

    // General - Video & ProgramInfo
    QString m_subtitle;
    const QString m_tagline;
    const QString m_description;
    uint          m_season  {0};
    uint          m_episode {0};
    uint          m_chanId  {0};
    const QString m_chanNum;
    const QString m_chanSign;
    const QString m_chanName;
    const QString m_chanPlaybackFilters;
    const QString m_recGroup;
    const QString m_playGroup;
    const QString m_seriesId;
    const QString m_programid;
    const QString m_storageGroup;
    const QDateTime m_startTs;
    const QDateTime m_endTs;
    const QDateTime m_recStartTs;
    const QDateTime m_recEndTs;
    uint m_programFlags     {0};
    uint m_audioProperties  {0};
    uint m_videoProperties  {0};
    uint m_subtitleType     {0};

    const QString m_certification;
    const QStringList m_countries;
    float m_popularity      {0};
    uint  m_budget          {0};
    uint  m_revenue         {0};

    // General - Music
    QString m_album;
    uint    m_trackNum      {0};

    // General - Game
    QString m_system;

    // Times
    uint m_year             {0};
    const QDate m_releaseDate;
    const QDateTime m_lastUpdated;
    std::chrono::minutes m_runtime          {0min};
    std::chrono::seconds m_runtimeSecs      {0s};

    // Inetref
    QString m_inetRef;
    QString m_collectionRef;
    QString m_tmsRef;
    QString m_imdb;
    bool    m_isCollection  {false};

    // People - Video
    const PeopleMap m_people;
    const QStringList m_studios;

    // Web references
    const QString m_homepage;
    const QString m_trailerURL;

    // Artwork
    const ArtworkMap m_artwork;
    DownloadMap m_downloads;
};

using MetadataLookupList = RefCountedList<MetadataLookup>;

META_PUBLIC QDomDocument CreateMetadataXML(MetadataLookupList list);
META_PUBLIC QDomDocument CreateMetadataXML(MetadataLookup *lookup);
META_PUBLIC QDomDocument CreateMetadataXML(ProgramInfo *pginfo);

META_PUBLIC void CreateMetadataXMLItem(MetadataLookup *lookup,
                                       QDomElement placetoadd,
                                       QDomDocument docroot);

META_PUBLIC void AddCertifications(MetadataLookup *lookup,
                                   QDomElement placetoadd,
                                   QDomDocument docroot);
META_PUBLIC void AddCategories(MetadataLookup *lookup,
                               QDomElement placetoadd,
                               QDomDocument docroot);
META_PUBLIC void AddStudios(MetadataLookup *lookup,
                            QDomElement placetoadd,
                            QDomDocument docroot);
META_PUBLIC void AddCountries(MetadataLookup *lookup,
                              QDomElement placetoadd,
                              QDomDocument docroot);

META_PUBLIC MetadataLookup *LookupFromProgramInfo(ProgramInfo *pginfo);

META_PUBLIC MetadataLookup *ParseMetadataItem(const QDomElement &item,
                                              MetadataLookup *lookup,
                                              bool passseas = true);
META_PUBLIC MetadataLookup *ParseMetadataMovieNFO(const QDomElement &item,
                                                  MetadataLookup *lookup);
META_PUBLIC PeopleMap ParsePeople(const QDomElement& people);
META_PUBLIC ArtworkMap ParseArtwork(const QDomElement& artwork);

META_PUBLIC int editDistance(const QString &s, const QString &t);
META_PUBLIC QString nearestName(const QString &actual,
                                const QStringList &candidates);

META_PUBLIC QDateTime RFC822TimeToQDateTime(const QString &t);

Q_DECLARE_METATYPE(MetadataLookup*)
Q_DECLARE_METATYPE(RefCountHandler<MetadataLookup>)

#endif // METADATACOMMON_H_
