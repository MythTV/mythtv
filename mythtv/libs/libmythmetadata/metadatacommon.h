#ifndef METADATACOMMON_H_
#define METADATACOMMON_H_

#include <QList>
#include <QPair>
#include <QMultiMap>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QDate>
#include <QDomElement>
#include <QEvent>

#include "mythtypes.h"
#include "mythmetaexp.h"
#include "metadataimagehelper.h"
#include "referencecounterlist.h"

class ProgramInfo;

enum LookupStep {
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
enum MetadataType {
    kMetadataVideo = 0,
    kMetadataRecording = 1,
    kMetadataMusic = 2,
    kMetadataGame = 3
};

// Determines the lookup fallback strategy
enum LookupType {
    kProbableTelevision = 0,
    kProbableGenericTelevision = 1,
    kProbableMovie = 2,
    kUnknownVideo = 3,
    kProbableMusic = 4,
    kProbableGame = 5
};

// Actual type of content
enum VideoContentType {
    kContentMovie = 0,
    kContentTelevision = 1,
    kContentAdult = 2,
    kContentMusicVideo = 3,
    kContentHomeMovie = 4,
    kContentUnknown = 5
};

enum PeopleType {
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

typedef QMap< VideoArtworkType, ArtworkInfo > DownloadMap;

typedef QMultiMap< PeopleType, PersonInfo > PeopleMap;

class META_PUBLIC MetadataLookup : public QObject, public ReferenceCounter
{
  public:
    MetadataLookup(void);
    ~MetadataLookup();

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
        const QString &host,
        const QString &filename,
        const QString &title,
        const QString &network,
        const QString &status,
        const QStringList &categories,
        const float userrating,
        uint ratingcount,
        const QString &language,
        const QString &subtitle,
        const QString &tagline,
        const QString &description,
        uint season,
        uint episode,
        const uint chanid,
        const QString &channum,
        const QString &chansign,
        const QString &channame,
        const QString &chanplaybackfilters,
        const QString &recgroup,
        const QString &playgroup,
        const QString &seriesid,
        const QString &programid,
        const QString &storagegroup,
        const QDateTime &startts,
        const QDateTime &endts,
        const QDateTime &recstartts,
        const QDateTime &recendts,
        const uint programflags,
        const uint audioproperties,
        const uint videoproperties,
        const uint subtitletype,
        const QString &certification,
        const QStringList &countries,
        const uint popularity,
        const uint budget,
        const uint revenue,
        const QString &album,
        uint tracknum,
        const QString &system,
        const uint year,
        const QDate &releasedate,
        const QDateTime &lastupdated,
        const uint runtime,
        const uint runtimesecs,
        const QString &inetref,
        const QString &collectionref,
        const QString &tmsref,
        const QString &imdb,
        const PeopleMap people,
        const QStringList &studios,
        const QString &homepage,
        const QString &trailerURL,
        const ArtworkMap artwork,
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
        const QString &host,
        const QString &filename,
        const QString &title,
        const QStringList &categories,
        const float userrating,
        const QString &subtitle,
        const QString &description,
        uint chanid,
        const QString &channum,
        const QString &chansign,
        const QString &channame,
        const QString &chanplaybackfilters,
        const QString &recgroup,
        const QString &playgroup,
        const QString &seriesid,
        const QString &programid,
        const QString &storagegroup,
        const QDateTime &startts,
        const QDateTime &endts,
        const QDateTime &recstartts,
        const QDateTime &recendts,
        uint programflags,
        uint audioproperties,
        uint videoproperties,
        uint subtitletype,
        const uint year,
        const QDate &releasedate,
        const QDateTime &lastupdated,
        const uint runtime,
        const uint runtimesecs);

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
        const QString &host,
        const QString &filename,
        const QString &title,
        const QStringList &categories,
        const float userrating,
        const QString &subtitle,
        const QString &tagline,
        const QString &description,
        uint season,
        uint episode,
        const QString &certification,
        const uint year,
        const QDate releasedate,
        const uint runtime,
        const uint runtimesecs,
        const QString &inetref,
        const PeopleMap people,
        const QString &trailerURL,
        const ArtworkMap artwork,
        DownloadMap downloads);

    void toMap(InfoMap &map);

    // SETS (Only a limited number needed)

    // Must set a type, data, and step.
    void SetType(MetadataType type) { m_type = type; };
    // For some lookup, it helps to know the subtype (TV vs. Movie)
    void SetSubtype(LookupType subtype) { m_subtype = subtype; };
    // Reference value- when the event comes back, need to associate with an item.
    void SetData(QVariant data) { m_data = data; };
    // Steps: SEARCH, GETDATA
    void SetStep(LookupStep step) { m_step = step; };
    // Don't prompt the user, just make an educated decision.
    void SetAutomatic(bool autom) { m_automatic = autom; };

    // Sets for image download handling
    void SetHandleImages(bool handle) { m_handleimages = handle; };
    void SetAllowOverwrites(bool allow) { m_allowoverwrites = allow; };
    void SetAllowGeneric(bool allow) { m_allowgeneric = allow; };
    void SetHost(const QString &host) { m_host = host; };
    void SetDownloads(ArtworkMap map) { m_downloads = map; };

    // General Sets
    void SetTitle(const QString &title) { m_title = title; };
    void SetFilename(const QString &filename) { m_filename = filename; };

    // General Sets - Video
    void SetSubtitle(const QString &subtitle) { m_subtitle = subtitle; };
    void SetSeason(uint season) { m_season = season; };
    void SetEpisode(uint episode) { m_episode = episode; };
    void SetInetref(const QString &inetref) { m_inetref = inetref; };
    void SetCollectionref(const QString &collectionref)
                             { m_collectionref = collectionref; };
    void SetTMSref(const QString &tmsref) { m_tmsref = tmsref; };
    void SetPreferDVDOrdering(bool preferdvdorder)
                             { m_dvdorder = preferdvdorder; };

    // General Sets - Music
    void SetAlbum(const QString &album) { m_album = album; };
    void SetTrack(uint track) { m_tracknum = track; };

    // General Sets - Games
    void SetSystem(const QString &system) { m_system = system; };

    // GETS

    MetadataType GetType() const { return m_type; };
    LookupType GetSubtype() const { return m_subtype; };
    QVariant GetData() const { return m_data; };
    LookupStep GetStep() const { return m_step; };
    bool GetAutomatic() const { return m_automatic; };

    // Image Handling Gets
    bool GetHandleImages() const { return m_handleimages; };
    bool GetAllowOverwrites() const { return m_allowoverwrites; };
    bool GetAllowGeneric() const { return m_allowgeneric; };

    // General
    QString GetFilename() const { return m_filename; };
    QString GetTitle() const { return m_title; };
    QStringList GetCategories() const { return m_categories; };
    float GetUserRating() const { return m_userrating; };
    uint GetRatingCount() const { return m_ratingcount; };
    QString GetLanguage() const { return m_language; };
    QString GetHost() const { return m_host; };

    // General - Video & ProgramInfo
    QString GetNetwork() const { return m_network; };
    QString GetStatus() const { return m_status; };
    QString GetSubtitle() const { return m_subtitle; };
    QString GetTagline() const { return m_tagline; };
    QString GetDescription() const { return m_description; };
    bool GetPreferDVDOrdering() const { return m_dvdorder; };
    uint GetSeason() const { return m_season; };
    uint GetEpisode() const { return m_episode; };
    uint GetChanId() const { return m_chanid; };
    QString GetChanNum() const { return m_channum; };
    QString GetChanSign() const { return m_chansign; };
    QString GetChanName() const { return m_channame; };
    QString GetChanPlaybackFilters() const { return m_chanplaybackfilters; };
    QString GetRecGroup() const { return m_recgroup; };
    QString GetPlayGroup() const { return m_playgroup; };
    QString GetSeriesId() const { return m_seriesid; };
    QString GetProgramId() const { return m_programid; };
    QString GetStorageGroup() const { return m_storagegroup; };
    QDateTime GetStartTS() const { return m_startts; };
    QDateTime GetEndTS() const { return m_endts; };
    QDateTime GetRecStartTS() const { return m_recstartts; };
    QDateTime GetRecEndTS() const { return m_recendts; };
    uint GetProgramFlags() const { return m_programflags; };
    uint GetAudioProperties() const { return m_audioproperties; };
    uint GetVideoProperties() const { return m_videoproperties; };
    uint GetSubtitleType() const { return m_subtitletype; };

    QString GetCertification() const { return m_certification; };
    QStringList GetCountries() const { return m_countries; };
    uint GetPopularity() const { return m_popularity; };
    uint GetBudget() const { return m_budget; };
    uint GetRevenue() const { return m_revenue; };

    // General - Music
    QString GetAlbumTitle() const { return m_album; };
    uint GetTrackNumber() const { return m_tracknum; };

    // General - Game
    QString GetSystem() const { return m_system; };

    // Times
    uint GetYear() const { return m_year; };
    QDate GetReleaseDate() const { return m_releasedate; };
    QDateTime GetLastUpdated() const { return m_lastupdated; };
    uint GetRuntime() const { return m_runtime; };
    uint GetRuntimeSeconds() const { return m_runtimesecs; };

    // Inetref
    QString GetInetref() const { return m_inetref; };
    QString GetCollectionref() const { return m_collectionref; };
    QString GetIMDB() const { return m_imdb; };
    QString GetTMSref() const { return m_tmsref; };

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
    MetadataType m_type;
    LookupType m_subtype;
    QVariant m_data;
    LookupStep m_step;
    bool m_automatic;
    bool m_handleimages;
    bool m_allowoverwrites;
    bool m_allowgeneric;
    bool m_dvdorder;
    QString m_host;

    QString m_filename;
    QString m_title;
    QString m_network;
    QString m_status;
    const QStringList m_categories;
    float m_userrating;
    uint m_ratingcount;
    const QString m_language;

    // General - Video & ProgramInfo
    QString m_subtitle;
    const QString m_tagline;
    const QString m_description;
    uint m_season;
    uint m_episode;
    uint m_chanid;
    const QString m_channum;
    const QString m_chansign;
    const QString m_channame;
    const QString m_chanplaybackfilters;
    const QString m_recgroup;
    const QString m_playgroup;
    const QString m_seriesid;
    const QString m_programid;
    const QString m_storagegroup;
    const QDateTime m_startts;
    const QDateTime m_endts;
    const QDateTime m_recstartts;
    const QDateTime m_recendts;
    uint m_programflags;
    uint m_audioproperties;
    uint m_videoproperties;
    uint m_subtitletype;

    const QString m_certification;
    const QStringList m_countries;
    uint m_popularity;
    uint m_budget;
    uint m_revenue;

    // General - Music
    QString m_album;
    uint m_tracknum;

    // General - Game
    QString m_system;

    // Times
    uint m_year;
    const QDate m_releasedate;
    const QDateTime m_lastupdated;
    uint m_runtime;
    uint m_runtimesecs;

    // Inetref
    QString m_inetref;
    QString m_collectionref;
    QString m_tmsref;
    QString m_imdb;

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

typedef RefCountedList<MetadataLookup> MetadataLookupList;

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
META_PUBLIC PeopleMap ParsePeople(QDomElement people);
META_PUBLIC ArtworkMap ParseArtwork(QDomElement artwork);

META_PUBLIC int editDistance(const QString &s, const QString &t);
META_PUBLIC QString nearestName(const QString &actual,
                                const QStringList &candidates);

META_PUBLIC QDateTime RFC822TimeToQDateTime(const QString &t);

enum GrabberType {
    kGrabberMovie = 0,
    kGrabberTelevision = 1,
    kGrabberMusic = 2,
    kGrabberGame = 3
};

class META_PUBLIC MetaGrabberScript : public QObject
{
  public:
    MetaGrabberScript();
    ~MetaGrabberScript();

    MetaGrabberScript(
        const QString &name,
        const QString &author,
        const QString &thumbnail,
        const QString &command,
        const GrabberType type,
        const QString &typestring,
        const QString &description,
        const float version);

    QString GetName(void) const { return m_name; };
    QString GetAuthor(void) const { return m_author; };
    QString GetThumbnail(void) const { return m_thumbnail; };
    QString GetCommand(void) const { return m_command; };
    GrabberType GetType(void) const { return m_type; };
    QString GetTypeString(void) const { return m_typestring; };
    QString GetDescription(void) const { return m_description; };
    float GetVersion(void) const { return m_version; };

    void toMap(InfoMap &metadataMap);

  private:
    QString m_name;
    QString m_author;
    QString m_thumbnail;
    QString m_command;
    GrabberType m_type;
    QString m_typestring;
    QString m_description;
    float m_version;
};

META_PUBLIC MetaGrabberScript *ParseGrabberVersion(const QDomElement &item);

Q_DECLARE_METATYPE(MetaGrabberScript*)
Q_DECLARE_METATYPE(MetadataLookup*)
Q_DECLARE_METATYPE(RefCountHandler<MetadataLookup>)

#endif // METADATACOMMON_H_
