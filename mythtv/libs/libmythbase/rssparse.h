#ifndef RSSPARSE_H
#define RSSPARSE_H

#include <vector>

#include <QString>
#include <QStringList>
#include <QList>
#include <QObject>
#include <QDomDocument>
#include <QDateTime>
#include <QPair>
#include <QMap>
#include <QVariant>
#include <sys/types.h>

#include "libmythbase/mythbaseexp.h"
#include "libmythbase/mythtypes.h"

enum ArticleType : std::uint8_t {
    VIDEO_FILE = 0,
    VIDEO_PODCAST = 1,
    AUDIO_FILE = 2,
    AUDIO_PODCAST = 3
};

/** Describes an enclosure associated with an item.
 */
struct Enclosure
{
    QString URL;
    QString Type;
    qint64 Length;
    QString Lang;
};

struct MRSSThumbnail
{
    QString URL;
    int Width;
    int Height;
    QString Time;
};

struct MRSSCredit
{
    QString Role;
    QString Who;
};

struct MRSSComment
{
    QString Type;
    QString Comment;
};

struct MRSSPeerLink
{
    QString Type;
    QString Link;
};

struct MRSSScene
{
    QString Title;
    QString Description;
    QString StartTime;
    QString EndTime;
};

struct MRSSEntry
{
    QString URL;
    qint64  Size         {0};
    QString Type;
    QString Medium;
    bool    IsDefault    {false};
    QString Expression;
    int     Bitrate      {0};
    double  Framerate    {0.0};
    double  SamplingRate {0.0};
    int     Channels     {0};
    int     Duration     {0};
    int     Width        {0};
    int     Height       {0};
    QString Lang;
    int     Group        {0};
    QString Rating;
    QString RatingScheme;
    QString Title;
    QString Description;
    QString Keywords;
    QString CopyrightURL;
    QString CopyrightText;
    int     RatingAverage {0};
    int     RatingCount   {0};
    int     RatingMin     {0};
    int     RatingMax     {0};
    int     Views         {0};
    int     Favs          {0};
    QString Tags;
    QList<MRSSThumbnail> Thumbnails;
    QList<MRSSCredit> Credits;
    QList<MRSSComment> Comments;
    QList<MRSSPeerLink> PeerLinks;
    QList<MRSSScene> Scenes;
};

class MBASE_PUBLIC ResultItem
{

  public:

    using resultList = QList<ResultItem *>;
    using List = std::vector<ResultItem>;

    ResultItem(QString title, QString sortTitle,
              QString subtitle, QString sortSubtitle,
              QString desc, QString URL,
              QString thumbnail, QString mediaURL,
              QString author, const QDateTime& date, const QString& time,
              const QString& rating, off_t filesize,
              const QString& player, const QStringList& playerargs,
              const QString& download, const QStringList& downloadargs,
              uint width, uint height, const QString& language,
              bool downloadable, const QStringList& countries,
              uint season, uint episode, bool customhtml);
    ResultItem() = default;
    ~ResultItem() = default;

    void ensureSortFields(void);
    void toMap(InfoMap &metadataMap);

    const QString& GetTitle() const { return m_title; }
    const QString& GetSortTitle() const { return m_sorttitle; }
    const QString& GetSubtitle() const { return m_subtitle; }
    const QString& GetSortSubtitle() const { return m_sortsubtitle; }
    const QString& GetDescription() const { return m_desc; }
    const QString& GetURL() const { return m_url; }
    const QString& GetThumbnail() const { return m_thumbnail; }
    const QString& GetMediaURL() const { return m_mediaURL; }
    const QString& GetAuthor() const { return m_author; }
    const QDateTime& GetDate() const { return m_date; }
    const QString& GetTime() const { return m_time; }
    const QString& GetRating() const { return m_rating; }
    const off_t& GetFilesize() const { return m_filesize; }
    const QString& GetPlayer() const { return m_player; }
    const QStringList& GetPlayerArguments() const { return m_playerargs; }
    const QString& GetDownloader() const { return m_download; }
    const QStringList& GetDownloaderArguments() const { return m_downloadargs; }
    const uint& GetWidth() const { return m_width; }
    const uint& GetHeight() const { return m_height; }
    const QString& GetLanguage() const { return m_language; }
    const bool& GetDownloadable() const { return m_downloadable; }
    const QStringList& GetCountries() const { return m_countries; }
    const uint& GetSeason() const { return m_season; }
    const uint& GetEpisode() const { return m_episode; }
    const bool& GetCustomHTML() const { return m_customhtml; }

  private:
    QString      m_title;
    QString      m_sorttitle;
    QString      m_subtitle;
    QString      m_sortsubtitle;
    QString      m_desc;
    QString      m_url;
    QString      m_thumbnail;
    QString      m_mediaURL;
    QString      m_author;
    QDateTime    m_date;
    QString      m_time;
    QString      m_rating;
    off_t        m_filesize     {0};
    QString      m_player;
    QStringList  m_playerargs;
    QString      m_download;
    QStringList  m_downloadargs;
    uint         m_width        {0};
    uint         m_height       {0};
    QString      m_language;
    bool         m_downloadable {false};
    QStringList  m_countries;
    uint         m_season       {0};
    uint         m_episode      {0};
    bool         m_customhtml   {false};
};

class MBASE_PUBLIC Parse : public QObject
{
    Q_OBJECT
    friend class MRSSParser;

  public:
    Parse() = default;
    ~Parse() override = default;

    static ResultItem::resultList parseRSS(const QDomDocument& domDoc);
    static ResultItem* ParseItem(const QDomElement& item) ;

    static QString GetLink(const QDomElement& parent);
    static QString GetAuthor(const QDomElement& parent);
    static QString GetCommentsRSS(const QDomElement& parent);
    static QString GetCommentsLink(const QDomElement& parent);
    static QDateTime GetDCDateTime(const QDomElement& parent);
    static QDateTime FromRFC3339(const QString& t);
    static QDateTime RFC822TimeToQDateTime (const QString& parent);
    QStringList GetAllCategories (const QDomElement&) const;
    static QList<MRSSEntry> GetMediaRSS (const QDomElement& item);
    static QList<Enclosure> GetEnclosures(const QDomElement& entry);
    static QString UnescapeHTML (const QString& escaped);

  private:
    static QMap<QString, int> m_timezoneOffsets;

  protected:
    static const QString kDC;
    static const QString kWFW;
    static const QString kAtom;
    static const QString kRDF;
    static const QString kSlash;
    static const QString kEnc;
    static const QString kITunes;
    static const QString kGeoRSSSimple;
    static const QString kGeoRSSW3;
    static const QString kMediaRSS;
    static const QString kMythRSS;
};

Q_DECLARE_METATYPE(ResultItem*)

#endif // RSSPARSE_H
