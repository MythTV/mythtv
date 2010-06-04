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

#include "mythexp.h"

enum LookupStep {
    SEARCH = 0,
    GETDATA = 1
};

enum ArtworkType {
    COVERART = 0,
    FANART = 1,
    BANNER = 2,
    SCREENSHOT = 3,
    POSTER = 4,
    BACKCOVER = 5,
    INSIDECOVER = 6,
    CDIMAGE = 7
};
Q_DECLARE_METATYPE(ArtworkType)

struct PersonInfo
{
    QString name;
    QString role;
    QString thumbnail;
    QString url;
};

struct ArtworkInfo
{
    QString label;
    QString thumbnail;
    QString url;
    uint width;
    uint height;
};
Q_DECLARE_METATYPE(ArtworkInfo)

enum MetadataType {
    VID = 0,
    MUSIC = 1,
    GAME = 2
};

enum PeopleType {
    ACTOR = 0,
    AUTHOR = 1,
    DIRECTOR = 2,
    PRODUCER = 3,
    EXECPRODUCER = 4,
    CINEMATOGRAPHER = 5,
    COMPOSER = 6,
    EDITOR = 7,
    CASTINGDIRECTOR = 8,
    ARTIST = 9,
    ALBUMARTIST = 10,
    GUESTSTAR = 11
};

typedef QList< ArtworkInfo > ArtworkList;

typedef QMultiMap< ArtworkType, ArtworkInfo > ArtworkMap;

typedef QMap< ArtworkType, ArtworkInfo > DownloadMap;

typedef QMultiMap< PeopleType, PersonInfo > PeopleMap;

typedef QHash<QString,QString> MetadataMap;

class MPUBLIC MetadataLookup : public QObject
{
  public:
    MetadataLookup(void);
    ~MetadataLookup();

    MetadataLookup(
        MetadataType type,
        QVariant data,
        LookupStep step,
        bool automatic,
        bool handleimages,
        bool allowoverwrites,
        bool preferdvdorder,
        QString host,
        QString title,
        const QStringList categories,
        const float userrating,
        const QString language,
        QString subtitle,
        const QString tagline,
        const QString description,
        uint season,
        uint episode,
        const QString certification,
        const QStringList countries,
        const uint popularity,
        const uint budget,
        const uint revenue,
        QString album,
        uint tracknum,
        const QString system,
        const uint year,
        const QDate releasedate,
        const QDateTime lastupdated,
        const uint runtime,
        const uint runtimesecs,
        QString inetref,
        QString tmsref,
        QString imdb,
        const PeopleMap people,
        const QStringList studios,
        const QString homepage,
        const QString trailerURL,
        const ArtworkMap artwork,
        DownloadMap downloads);

    void toMap(MetadataMap &map);

    // SETS (Only a limited number needed)

    // Must set a type, data, and step.
    void SetType(MetadataType type) { m_type = type; };
    // Reference value- when the event comes back, need to associate with an item.
    void SetData(QVariant data) { m_data = data; };
    // Steps: SEARCH, GETDATA
    void SetStep(LookupStep step) { m_step = step; };
    // Don't prompt the user, just make an educated decision.
    void SetAutomatic(bool autom) { m_automatic = autom; };

    // Sets for image download handling
    void SetHandleImages(bool handle) { m_handleimages = handle; };
    void SetAllowOverwrites(bool allow) { m_allowoverwrites = allow; };
    void SetHost(QString host) { m_host = host; };
    void SetDownloads(ArtworkMap map) { m_downloads = map; };

    // General Sets
    void SetTitle(QString title) { m_title = title; };

    // General Sets - Video
    void SetSubtitle(QString subtitle) { m_subtitle = subtitle; };
    void SetSeason(uint season) { m_season = season; };
    void SetEpisode(uint episode) { m_episode = episode; };
    void SetInetref(QString inetref) { m_inetref = inetref; };
    void SetTMSref(QString tmsref) { m_tmsref = tmsref; };
    void SetPreferDVDOrdering(bool preferdvdorder)
                             { m_dvdorder = preferdvdorder; };

    // General Sets - Music
    void SetAlbum(QString album) { m_album = album; };
    void SetTrack(uint track) { m_tracknum = track; };

    // GETS

    MetadataType GetType() const { return m_type; };
    QVariant GetData() const { return m_data; };
    LookupStep GetStep() const { return m_step; };
    bool GetAutomatic() const { return m_automatic; };

    // Image Handling Gets
    bool GetHandleImages() const { return m_handleimages; };
    bool GetAllowOverwrites() const { return m_allowoverwrites; };

    // General
    QString GetTitle() const { return m_title; };
    QStringList GetCategories() const { return m_categories; };
    float GetUserRating() const { return m_userrating; };
    QString GetLanguage() const { return m_language; };
    QString GetHost() const { return m_host; };

    // General - Video
    QString GetSubtitle() const { return m_subtitle; };
    QString GetTagline() const { return m_tagline; };
    QString GetDescription() const { return m_description; };
    bool GetPreferDVDOrdering() const { return m_dvdorder; };
    uint GetSeason() const { return m_season; };
    uint GetEpisode() const { return m_episode; };
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
    QString GetIMDB() const { return m_imdb; };
    QString GetTMSref() const { return m_tmsref; };

    // People
    QList<PersonInfo> GetPeople(PeopleType type) const;
    QStringList GetStudios() const { return m_studios; };

    // Web references
    QString GetHomepage() const { return m_homepage; };
    QString GetTrailerURL() const { return m_trailerURL; };

    // Artwork
    ArtworkList GetArtwork(ArtworkType type) const;
    DownloadMap GetDownloads() const { return m_downloads; };

  private:
    // General
    MetadataType m_type;
    QVariant m_data;
    LookupStep m_step;
    bool m_automatic;
    bool m_handleimages;
    bool m_allowoverwrites;
    bool m_dvdorder;
    QString m_host;

    QString m_title;
    const QStringList m_categories;
    float m_userrating;
    const QString m_language;

    // General - Video
    QString m_subtitle;
    const QString m_tagline;
    const QString m_description;
    uint m_season;
    uint m_episode;
    const QString m_certification;
    const QStringList m_countries;
    uint m_popularity;
    uint m_budget;
    uint m_revenue;

    // General - Music
    QString m_album;
    uint m_tracknum;

    // General - Game
    const QString m_system;

    // Times
    uint m_year;
    const QDate m_releasedate;
    const QDateTime m_lastupdated;
    uint m_runtime;
    uint m_runtimesecs;

    // Inetref
    QString m_inetref;
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
Q_DECLARE_METATYPE(MetadataLookup*)

MPUBLIC MetadataLookup* ParseMetadataItem(const QDomElement& item,
                                          MetadataLookup *lookup,
                                          bool passseas = true);
MPUBLIC PeopleMap ParsePeople(QDomElement people);
MPUBLIC ArtworkMap ParseArtwork(QDomElement artwork);

MPUBLIC int editDistance(const QString& s, const QString& t);
MPUBLIC QString nearestName(const QString& actual,
                            const QStringList& candidates);

MPUBLIC QDateTime RFC822TimeToQDateTime(const QString& t);

#endif // METADATACOMMON_H_
