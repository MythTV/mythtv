#include <cmath> // for isnan()

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegExp>

#include "mythcorecontext.h"
#include "mythmiscutil.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "storagegroup.h"
#include "remotefile.h"
#include "remoteutil.h"
#include "mythdate.h"
#include "mythlogging.h"
#include "globals.h"
#include "dbaccess.h"
#include "videometadatalistmanager.h"
#include "videoutils.h"
#include "programinfo.h" // for format_season_and_episode

struct SortData
{
    SortData(const QString &title, const QString &filename, const QString &id) :
        m_title(title), m_filename(filename), m_id(id)
    {
    }

    QString m_title;
    QString m_filename;
    QString m_id;
};

static bool operator<(const SortData &lhs, const SortData &rhs)
{
    int ret = QString::localeAwareCompare(lhs.m_title, rhs.m_title);

    if (ret == 0)
        ret = QString::localeAwareCompare(lhs.m_filename, rhs.m_filename);

    if (ret == 0)
        ret = QString::localeAwareCompare(lhs.m_id, rhs.m_id);

    return ret < 0;
}

VideoMetadata::SortKey::SortKey() : m_sd(NULL)
{
}

VideoMetadata::SortKey::SortKey(const SortData &data)
{
    m_sd = new SortData(data);
}

VideoMetadata::SortKey::SortKey(const SortKey &other) : m_sd(NULL)
{
    *this = other;
}

VideoMetadata::SortKey &VideoMetadata::SortKey::operator=(const SortKey &rhs)
{
    if (this != &rhs)
    {
        Clear();
        if (rhs.m_sd)
            m_sd = new SortData(*rhs.m_sd);
    }

    return *this;
}

VideoMetadata::SortKey::~SortKey()
{
    Clear();
}

bool VideoMetadata::SortKey::isSet() const
{
    return m_sd != 0;
}

void VideoMetadata::SortKey::Clear()
{
    delete m_sd;
    m_sd = 0;
}

class VideoMetadataImp
{
  public:
    typedef VideoMetadata::genre_list genre_list;
    typedef VideoMetadata::country_list country_list;
    typedef VideoMetadata::cast_list cast_list;

  public:
    VideoMetadataImp(const QString &filename, const QString &hash, const QString &trailer,
             const QString &coverfile, const QString &screenshot, const QString &banner,
             const QString &fanart, const QString &title, const QString &subtitle,
             const QString &tagline, int year, const QDate &releasedate,
             const QString &inetref, int collectionref, const QString &homepage,
             const QString &director, const QString &studio,
             const QString &plot, float userrating,
             const QString &rating, int length, int playcount,
             int season, int episode, const QDate &insertdate,
             int id, ParentalLevel::Level showlevel, int categoryID,
             int childID, bool browse, bool watched,
             const QString &playcommand, const QString &category,
             const genre_list &genres,
             const country_list &countries,
             const cast_list &cast,
             const QString &host = "",
             bool processed = false,
             VideoContentType contenttype = kContentUnknown) :
        m_title(title), m_subtitle(subtitle), m_tagline(tagline),
        m_inetref(inetref), m_collectionref(collectionref), m_homepage(homepage),
        m_director(director), m_studio(studio),
        m_plot(plot), m_rating(rating), m_playcommand(playcommand), m_category(category),
        m_genres(genres), m_countries(countries), m_cast(cast),
        m_filename(filename), m_hash(hash), m_trailer(trailer), m_coverfile(coverfile),
        m_screenshot(screenshot), m_banner(banner), m_fanart(fanart),
        m_host(host), m_categoryID(categoryID), m_childID(childID),
        m_year(year), m_releasedate(releasedate), m_length(length), m_playcount(playcount),
        m_season(season), m_episode(episode), m_insertdate(insertdate), m_showlevel(showlevel),
        m_browse(browse), m_watched(watched), m_id(id),
        m_userrating(userrating), m_processed(processed),
        m_contenttype(contenttype)
    {
        VideoCategory::GetCategory().get(m_categoryID, m_category);
    }

    VideoMetadataImp(MSqlQuery &query)
    {
        fromDBRow(query);
    }

    VideoMetadataImp(const VideoMetadataImp &other)
    {
        *this = other;
    }

    VideoMetadataImp &operator=(const VideoMetadataImp &rhs)
    {
        if (this != &rhs)
        {
            m_title = rhs.m_title;
            m_subtitle = rhs.m_subtitle;
            m_tagline = rhs.m_tagline;
            m_inetref = rhs.m_inetref;
            m_collectionref = rhs.m_collectionref;
            m_homepage = rhs.m_homepage;
            m_director = rhs.m_director;
            m_studio = rhs.m_studio;
            m_plot = rhs.m_plot;
            m_rating = rhs.m_rating;
            m_playcommand = rhs.m_playcommand;
            m_category = rhs.m_category;
            m_genres = rhs.m_genres;
            m_countries = rhs.m_countries;
            m_cast = rhs.m_cast;
            m_filename = rhs.m_filename;
            m_hash = rhs.m_hash;
            m_trailer = rhs.m_trailer;
            m_coverfile = rhs.m_coverfile;
            m_screenshot = rhs.m_screenshot;
            m_banner = rhs.m_banner;
            m_fanart = rhs.m_fanart;

            m_categoryID = rhs.m_categoryID;
            m_childID = rhs.m_childID;
            m_year = rhs.m_year;
            m_releasedate = rhs.m_releasedate;
            m_length = rhs.m_length;
            m_playcount = rhs.m_playcount;
            m_season = rhs.m_season;
            m_episode = rhs.m_episode;
            m_insertdate = rhs.m_insertdate;
            m_showlevel = rhs.m_showlevel;
            m_browse = rhs.m_browse;
            m_watched = rhs.m_watched;
            m_id = rhs.m_id;
            m_userrating = rhs.m_userrating;
            m_host = rhs.m_host;
            m_processed = rhs.m_processed;
            m_contenttype = rhs.m_contenttype;

            // No DB vars
            m_sort_key = rhs.m_sort_key;
            m_prefix = rhs.m_prefix;
        }

        return *this;
    }

  public:
    bool HasSortKey() const { return m_sort_key.isSet(); }
    const VideoMetadata::SortKey &GetSortKey() const { return m_sort_key; }
    void SetSortKey(const VideoMetadata::SortKey &sort_key)
    {
        m_sort_key = sort_key;
    }

    const QString &GetPrefix() const { return m_prefix; }
    void SetPrefix(const QString &prefix) { m_prefix = prefix; }

    const QString &getTitle() const { return m_title; }
    void SetTitle(const QString& title)
    {
        if (gCoreContext->HasGUI())
            m_sort_key.Clear();
        m_title = title;
    }

    const QString &getSubtitle() const { return m_subtitle; }
    void SetSubtitle(const QString &subtitle) { m_subtitle = subtitle; }

    const QString &GetTagline() const { return m_tagline; }
    void SetTagline(const QString &tagline) { m_tagline = tagline; }

    const QString &GetInetRef() const { return m_inetref; }
    void SetInetRef(const QString &inetRef) { m_inetref = inetRef; }

    int GetCollectionRef() const { return m_collectionref; }
    void SetCollectionRef(int collectionref) { m_collectionref = collectionref; }

    const QString &GetHomepage() const { return m_homepage; }
    void SetHomepage(const QString &homepage) { m_homepage = homepage; }

    const QString &getDirector() const { return m_director; }
    void SetDirector(const QString &director) { m_director = director; }

    const QString &getStudio() const { return m_studio; }
    void SetStudio(const QString &studio) { m_studio = studio; }

    const QString &getPlot() const { return m_plot; }
    void SetPlot(const QString &plot) { m_plot = plot; }

    const QString &GetRating() const { return m_rating; }
    void SetRating(const QString &rating) { m_rating = rating; }

    const QString &getPlayCommand() const { return m_playcommand; }
    void SetPlayCommand(const QString &playCommand)
    {
        m_playcommand = playCommand;
    }

    const QString &GetCategory() const { return m_category; }
//    void SetCategory(const QString &category) { m_category = category; }

    const genre_list &getGenres() const { return m_genres; }
    void SetGenres(const genre_list &genres) { m_genres = genres; }

    const country_list &GetCountries() const { return m_countries; }
    void SetCountries(const country_list &countries)
    {
        m_countries = countries;
    }

    const cast_list &GetCast() const { return m_cast; }
    void SetCast(const cast_list &cast) { m_cast = cast; }

    const QString &GetHost() const { return m_host; }
    void SetHost(const QString &host) { m_host = host; }

    const QString &getFilename() const { return m_filename; }
    void SetFilename(const QString &filename) { m_filename = filename; }

    const QString &GetHash() const { return m_hash; }
    void SetHash(const QString &hash) { m_hash = hash; }

    const QString &GetTrailer() const { return m_trailer; }
    void SetTrailer(const QString &trailer) { m_trailer = trailer; }

    const QString &GetCoverFile() const { return m_coverfile; }
    void SetCoverFile(const QString &coverFile) { m_coverfile = coverFile; }

    const QString &GetScreenshot() const { return m_screenshot; }
    void SetScreenshot(const QString &screenshot) { m_screenshot = screenshot; }

    const QString &GetBanner() const { return m_banner; }
    void SetBanner(const QString &banner) { m_banner = banner; }

    const QString &GetFanart() const { return m_fanart; }
    void SetFanart(const QString &fanart) { m_fanart = fanart; }

    int GetCategoryID() const
    {
        return m_categoryID;
    }
    void SetCategoryID(int id);

    int GetChildID() const { return m_childID; }
    void SetChildID(int childID) { m_childID = childID; }

    int getYear() const { return m_year; }
    void SetYear(int year) { m_year = year; }

    QDate getReleaseDate() const { return m_releasedate; }
    void SetReleaseDate(QDate releasedate) { m_releasedate = releasedate; }

    int GetLength() const { return m_length; }
    void SetLength(int length) { m_length = length; }

    unsigned int GetPlayCount() const { return m_playcount; }
    void SetPlayCount(int playcount) { m_playcount = playcount; }

    int GetSeason() const { return m_season; }
    void SetSeason(int season) { m_season = season; }

    int GetEpisode() const { return m_episode; }
    void SetEpisode(int episode) { m_episode = episode; }

    QDate GetInsertdate() const { return m_insertdate;}
    void SetInsertdate(QDate date) { m_insertdate = date;}

    ParentalLevel::Level GetShowLevel() const { return m_showlevel; }
    void SetShowLevel(ParentalLevel::Level showLevel)
    {
        m_showlevel = ParentalLevel(showLevel).GetLevel();
    }

    bool GetBrowse() const { return m_browse; }
    void SetBrowse(bool browse) { m_browse = browse; }

    bool GetWatched() const { return m_watched; }
    void SetWatched(bool watched) { m_watched = watched; }

    unsigned int GetID() const { return m_id; }
    void SetID(int id) { m_id = id; }

    float GetUserRating() const { return m_userrating; }
    void SetUserRating(float userRating) { m_userrating = userRating; }

    bool GetProcessed() const { return m_processed; }
    void SetProcessed(bool processed) { m_processed = processed; }

    VideoContentType GetContentType() const { return m_contenttype; }
    void SetContentType(VideoContentType contenttype) { m_contenttype = contenttype; }

    ////////////////////////////////

    void SaveToDatabase();
    void UpdateDatabase();
    bool DeleteFromDatabase();

    bool DeleteFile();

    void Reset();

    bool IsHostSet() const;

    void GetImageMap(InfoMap &imageMap) const;

  private:
    void fillCountries();
    void updateCountries();
    void fillGenres();
    void fillCast();
    void updateGenres();
    void updateCast();
    bool removeDir(const QString &dirName);
    void fromDBRow(MSqlQuery &query);
    void saveToDatabase();

  private:
    QString m_title;
    QString m_subtitle;
    QString m_tagline;
    QString m_inetref;
    int m_collectionref;
    QString m_homepage;
    QString m_director;
    QString m_studio;
    QString m_plot;
    QString m_rating;
    QString m_playcommand;
    QString m_category;
    genre_list m_genres;
    country_list m_countries;
    cast_list m_cast;
    QString m_filename;
    QString m_hash;
    QString m_trailer;
    QString m_coverfile;
    QString m_screenshot;
    QString m_banner;
    QString m_fanart;
    QString m_host;

    int m_categoryID;
    int m_childID;
    int m_year;
    QDate m_releasedate;
    int m_length;
    int m_playcount;
    int m_season;
    int m_episode;
    QDate m_insertdate;
    ParentalLevel::Level m_showlevel;
    bool m_browse;
    bool m_watched;
    unsigned int m_id;  // videometadata.intid
    float m_userrating;
    bool m_processed;
    VideoContentType m_contenttype;

    // not in DB
    VideoMetadata::SortKey m_sort_key;
    QString m_prefix;
};

/////////////////////////////
/////////
/////////////////////////////
bool VideoMetadataImp::removeDir(const QString &dirName)
{
    QDir d(dirName);
    
    d.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList contents = d.entryInfoList();
    if (!contents.size())
    {
        return d.rmdir(dirName);
    }

    for (QFileInfoList::iterator p = contents.begin(); p != contents.end(); ++p)
    {
        if (p->isDir())
        {
            QString fileName = p->fileName();
            if (!removeDir(fileName))
                return false;
        }
        else
        {
            if (!QFile(p->fileName()).remove())
                return false;
        }
    }
    return d.rmdir(dirName);
}

/// Deletes the file associated with a metadata entry
bool VideoMetadataImp::DeleteFile()
{
    bool isremoved = false;

    if (!m_host.isEmpty())
    {
        QString url = generate_file_url("Videos", m_host, m_filename);
        isremoved = RemoteFile::DeleteFile(url);
    }
    else
    {
        QFileInfo fi(m_filename);
        if (fi.isDir())
        {
            isremoved = removeDir(m_filename);
        }
        else
        {
            isremoved = QFile::remove(m_filename);
        }
    }

    if (!isremoved)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Could not delete file: %1")
                                       .arg(m_filename));
    }

    return isremoved;
}

void VideoMetadataImp::Reset()
{
    VideoMetadataImp tmp(m_filename, VideoMetadata::VideoFileHash(m_filename, m_host), VIDEO_TRAILER_DEFAULT,
                    VIDEO_COVERFILE_DEFAULT, VIDEO_SCREENSHOT_DEFAULT, VIDEO_BANNER_DEFAULT,
                    VIDEO_FANART_DEFAULT, VideoMetadata::FilenameToMeta(m_filename, 1), QString(),
                    VideoMetadata::FilenameToMeta(m_filename, 4), VIDEO_YEAR_DEFAULT,
                    QDate(), VIDEO_INETREF_DEFAULT, -1, QString(), VIDEO_DIRECTOR_DEFAULT,
                    QString(), VIDEO_PLOT_DEFAULT, 0.0,
                    VIDEO_RATING_DEFAULT, 0, 0,
                    VideoMetadata::FilenameToMeta(m_filename, 2).toInt(),
                    VideoMetadata::FilenameToMeta(m_filename, 3).toInt(), QDate(), m_id,
                    ParentalLevel::plLowest, 0, -1, true, false, "", "",
                    VideoMetadata::genre_list(), VideoMetadata::country_list(),
                    VideoMetadata::cast_list(), m_host, false);
    tmp.m_prefix = m_prefix;

    *this = tmp;
}

bool VideoMetadataImp::IsHostSet() const
{
    return !m_host.isEmpty();
}

void VideoMetadataImp::fillGenres()
{
    m_genres.clear();
    VideoGenreMap &vgm = VideoGenreMap::getGenreMap();
    VideoGenreMap::entry genres;
    if (vgm.get(m_id, genres))
    {
        VideoGenre &vg = VideoGenre::getGenre();
        for (VideoGenreMap::entry::values_type::const_iterator p =
             genres.values.begin(); p != genres.values.end(); ++p)
        {
            // Just add empty string for no-name genres
            QString name;
            vg.get(*p, name);
            m_genres.push_back(genre_list::value_type(*p, name));
        }
    }
}

void VideoMetadataImp::fillCountries()
{
    m_countries.clear();
    VideoCountryMap &vcm = VideoCountryMap::getCountryMap();
    VideoCountryMap::entry countries;
    if (vcm.get(m_id, countries))
    {
        VideoCountry &vc = VideoCountry::getCountry();
        for (VideoCountryMap::entry::values_type::const_iterator p =
             countries.values.begin(); p != countries.values.end(); ++p)
        {
            // Just add empty string for no-name countries
            QString name;
            vc.get(*p, name);
            m_countries.push_back(country_list::value_type(*p, name));
        }
    }
}

void VideoMetadataImp::fillCast()
{
    m_cast.clear();
    VideoCastMap &vcm = VideoCastMap::getCastMap();
    VideoCastMap::entry cast;
    if (vcm.get(m_id, cast))
    {
        VideoCast &vc = VideoCast::GetCast();
        for (VideoCastMap::entry::values_type::const_iterator p =
             cast.values.begin(); p != cast.values.end(); ++p)
        {
            // Just add empty string for no-name cast
            QString name;
            vc.get(*p, name);
            m_cast.push_back(cast_list::value_type(*p, name));
        }
    }
}

/// Sets metadata from a DB row
void VideoMetadataImp::fromDBRow(MSqlQuery &query)
{
    m_title = query.value(0).toString();
    m_director = query.value(1).toString();
    m_studio = query.value(2).toString();
    m_plot = query.value(3).toString();
    m_rating = query.value(4).toString();
    m_year = query.value(5).toInt();
    m_releasedate = query.value(6).toDate();
    m_userrating = (float)query.value(7).toDouble();
    if (isnan(m_userrating) || m_userrating < 0)
        m_userrating = 0.0;
    if (m_userrating > 10.0)
        m_userrating = 10.0;
    m_length = query.value(8).toInt();
    m_playcount = query.value(9).toInt();
    m_filename = query.value(10).toString();
    m_hash = query.value(11).toString();
    m_showlevel = ParentalLevel(query.value(12).toInt()).GetLevel();
    m_coverfile = query.value(13).toString();
    m_inetref = query.value(14).toString();
    m_collectionref = query.value(15).toUInt();
    m_homepage = query.value(16).toString();
    m_childID = query.value(17).toUInt();
    m_browse = query.value(18).toBool();
    m_watched = query.value(19).toBool();
    m_playcommand = query.value(20).toString();
    m_categoryID = query.value(21).toInt();
    m_id = query.value(22).toInt();
    m_trailer = query.value(23).toString();
    m_screenshot = query.value(24).toString();
    m_banner = query.value(25).toString();
    m_fanart = query.value(26).toString();
    m_subtitle = query.value(27).toString();
    m_tagline = query.value(28).toString();
    m_season = query.value(29).toInt();
    m_episode = query.value(30).toInt();
    m_host = query.value(31).toString();
    m_insertdate = query.value(32).toDate();
    m_processed = query.value(33).toBool();

    m_contenttype = ContentTypeFromString(query.value(34).toString());

    VideoCategory::GetCategory().get(m_categoryID, m_category);

    // Genres
    fillGenres();

    //Countries
    fillCountries();

    // Cast
    fillCast();
}

void VideoMetadataImp::saveToDatabase()
{
    if (m_title.isEmpty())
        m_title = VideoMetadata::FilenameToMeta(m_filename, 1);
    if (m_hash.isEmpty())
        m_hash = VideoMetadata::VideoFileHash(m_filename, m_host);
    if (m_subtitle.isEmpty())
        m_subtitle = VideoMetadata::FilenameToMeta(m_filename, 4);
    if (m_director.isEmpty())
        m_director = VIDEO_DIRECTOR_UNKNOWN;
    if (m_plot.isEmpty())
        m_plot = VIDEO_PLOT_DEFAULT;
    if (m_rating.isEmpty())
        m_rating = VIDEO_RATING_DEFAULT;

    InfoMap metadataMap;
    GetImageMap(metadataMap);
    QString coverfile   = metadataMap["coverfile"];
    QString screenshot  = metadataMap["screenshotfile"];
    QString bannerfile  = metadataMap["bannerfile"];
    QString fanartfile  = metadataMap["fanartfile"];

    if (coverfile.isEmpty() || !RemoteFile::Exists(coverfile))
        m_coverfile = VIDEO_COVERFILE_DEFAULT;
    if (screenshot.isEmpty() || !RemoteFile::Exists(screenshot))
        m_screenshot = VIDEO_SCREENSHOT_DEFAULT;
    if (bannerfile.isEmpty() || !RemoteFile::Exists(bannerfile))
        m_banner = VIDEO_BANNER_DEFAULT;
    if (fanartfile.isEmpty() || !RemoteFile::Exists(fanartfile))
        m_fanart = VIDEO_FANART_DEFAULT;
    if (m_trailer.isEmpty())
        m_trailer = VIDEO_TRAILER_DEFAULT;
    if (m_inetref.isEmpty())
        m_inetref = VIDEO_INETREF_DEFAULT;
    if (isnan(m_userrating))
        m_userrating = 0.0;
    if (m_userrating < -10.0 || m_userrating > 10.0)
        m_userrating = 0.0;
    if (m_releasedate.toString().isEmpty())
        m_releasedate = QDate::fromString("0000-00-00", "YYYY-MM-DD");
    if (m_contenttype == kContentUnknown)
    {
        if (m_season > 0 || m_episode > 0)
            m_contenttype = kContentTelevision;
        else
            m_contenttype = kContentMovie;
    }

    bool inserting = m_id == 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (inserting)
    {
        m_browse = 1;

        m_watched = 0;

        query.prepare("INSERT INTO videometadata (title,subtitle,tagline,director,studio,plot,"
                      "rating,year,userrating,length,season,episode,filename,hash,"
                      "showlevel,coverfile,inetref,homepage,browse,watched,trailer,"
                      "screenshot,banner,fanart,host,processed,contenttype) VALUES (:TITLE, :SUBTITLE, "
                      ":TAGLINE, :DIRECTOR, :STUDIO, :PLOT, :RATING, :YEAR, :USERRATING, "
                      ":LENGTH, :SEASON, :EPISODE, :FILENAME, :HASH, :SHOWLEVEL, "
                      ":COVERFILE, :INETREF, :HOMEPAGE, :BROWSE, :WATCHED, "
                      ":TRAILER, :SCREENSHOT, :BANNER, :FANART, :HOST, :PROCESSED, :CONTENTTYPE)");
    }
    else
    {
        query.prepare("UPDATE videometadata SET title = :TITLE, subtitle = :SUBTITLE, "
                      "tagline = :TAGLINE, director = :DIRECTOR, studio = :STUDIO, "
                      "plot = :PLOT, rating= :RATING, year = :YEAR, "
                      "releasedate = :RELEASEDATE, userrating = :USERRATING, "
                      "length = :LENGTH, playcount = :PLAYCOUNT, season = :SEASON, "
                      "episode = :EPISODE, filename = :FILENAME, hash = :HASH, trailer = :TRAILER, "
                      "showlevel = :SHOWLEVEL, coverfile = :COVERFILE, "
                      "screenshot = :SCREENSHOT, banner = :BANNER, fanart = :FANART, "
                      "inetref = :INETREF, collectionref = :COLLECTION, homepage = :HOMEPAGE, "
                      "browse = :BROWSE, watched = :WATCHED, host = :HOST, playcommand = :PLAYCOMMAND, "
                      "childid = :CHILDID, category = :CATEGORY, processed = :PROCESSED, "
                      "contenttype = :CONTENTTYPE WHERE intid = :INTID");

        query.bindValue(":PLAYCOMMAND", m_playcommand);
        query.bindValue(":CHILDID", m_childID);
        query.bindValue(":CATEGORY", m_categoryID);
        query.bindValue(":INTID", m_id);
    }

    query.bindValue(":TITLE", m_title.isNull() ? "" : m_title);
    query.bindValue(":SUBTITLE", m_subtitle.isNull() ? "" : m_subtitle);
    query.bindValue(":TAGLINE", m_tagline);
    query.bindValue(":DIRECTOR", m_director.isNull() ? "" : m_director);
    query.bindValue(":STUDIO", m_studio);
    query.bindValue(":PLOT", m_plot);
    query.bindValue(":RATING", m_rating.isNull() ? "" : m_rating);
    query.bindValue(":YEAR", m_year);
    query.bindValue(":RELEASEDATE", m_releasedate);
    query.bindValue(":USERRATING", m_userrating);
    query.bindValue(":LENGTH", m_length);
    query.bindValue(":PLAYCOUNT", m_playcount);
    query.bindValue(":SEASON", m_season);
    query.bindValue(":EPISODE", m_episode);
    query.bindValue(":FILENAME", m_filename);
    query.bindValue(":HASH", m_hash);
    query.bindValue(":TRAILER", m_trailer.isNull() ? "" : m_trailer);
    query.bindValue(":SHOWLEVEL", m_showlevel);
    query.bindValue(":COVERFILE", m_coverfile.isNull() ? "" : m_coverfile);
    query.bindValue(":SCREENSHOT", m_screenshot.isNull() ? "" : m_screenshot);
    query.bindValue(":BANNER", m_banner.isNull() ? "" : m_banner);
    query.bindValue(":FANART", m_fanart.isNull() ? "" : m_fanart);
    query.bindValue(":INETREF", m_inetref.isNull() ? "" : m_inetref);
    query.bindValue(":COLLECTION", m_collectionref);
    query.bindValue(":HOMEPAGE", m_homepage.isNull() ? "" : m_homepage);
    query.bindValue(":BROWSE", m_browse);
    query.bindValue(":WATCHED", m_watched);
    query.bindValue(":HOST", m_host);
    query.bindValue(":PROCESSED", m_processed);
    query.bindValue(":CONTENTTYPE", ContentTypeToString(m_contenttype));

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("video metadata update", query);
        return;
    }

    if (inserting)
    {
        // Must make sure we have 'id' filled before we call updateGenres or
        // updateCountries

        if (!query.exec("SELECT LAST_INSERT_ID()") || !query.next())
        {
            MythDB::DBError("metadata id get", query);
            return;
        }

        m_id = query.value(0).toUInt();

        if (0 == m_id)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("%1: The id of the last inserted row to "
                        "videometadata seems to be 0. This is odd.")
                    .arg(__FILE__));
            return;
        }
    }

    updateGenres();
    updateCountries();
    updateCast();
}

void VideoMetadataImp::SaveToDatabase()
{
    saveToDatabase();
}

void VideoMetadataImp::UpdateDatabase()
{
    saveToDatabase();
}

bool VideoMetadataImp::DeleteFromDatabase()
{
    VideoGenreMap::getGenreMap().remove(m_id);
    VideoCountryMap::getCountryMap().remove(m_id);
    VideoCastMap::getCastMap().remove(m_id);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM videometadata WHERE intid = :ID");
    query.bindValue(":ID", m_id);
    if (!query.exec())
    {
        MythDB::DBError("delete from videometadata", query);
    }

    query.prepare("DELETE FROM filemarkup WHERE filename = :FILENAME");
    query.bindValue(":FILENAME", m_filename);
    if (!query.exec())
    {
        MythDB::DBError("delete from filemarkup", query);
    }

    return true;
}

void VideoMetadataImp::SetCategoryID(int id)
{
    if (id == 0)
    {
        m_category = "";
        m_categoryID = id;
    }
    else
    {
        if (m_categoryID != id)
        {
            QString cat;
            if (VideoCategory::GetCategory().get(id, cat))
            {
                m_category = cat;
                m_categoryID = id;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Unknown category id");
            }
        }
    }
}

void VideoMetadataImp::updateGenres()
{
    VideoGenreMap::getGenreMap().remove(m_id);

    // ensure that all genres we have are in the DB
    genre_list::iterator genre = m_genres.begin();
    while (genre != m_genres.end())
    {
        if (genre->second.trimmed().length())
        {
            genre->first = VideoGenre::getGenre().add(genre->second);
            VideoGenreMap::getGenreMap().add(m_id, genre->first);
            ++genre;
        }
        else
        {
            genre = m_genres.erase(genre);
        }
    }
}

void VideoMetadataImp::updateCountries()
{
    // remove countries for this video
    VideoCountryMap::getCountryMap().remove(m_id);

    country_list::iterator country = m_countries.begin();
    while (country != m_countries.end())
    {
        if (country->second.trimmed().length())
        {
            country->first = VideoCountry::getCountry().add(country->second);
            VideoCountryMap::getCountryMap().add(m_id, country->first);
            ++country;
        }
        else
        {
            country = m_countries.erase(country);
        }
    }
}

void VideoMetadataImp::updateCast()
{
    VideoCastMap::getCastMap().remove(m_id);

    // ensure that all cast we have are in the DB
    cast_list::iterator cast = m_cast.begin();
    while (cast != m_cast.end())
    {
        if (cast->second.trimmed().length())
        {
            cast->first = VideoCast::GetCast().add(cast->second);
            VideoCastMap::getCastMap().add(m_id, cast->first);
            ++cast;
        }
        else
        {
            cast = m_cast.erase(cast);
        }
    }
}

void VideoMetadataImp::GetImageMap(InfoMap &imageMap) const
{
    QString coverfile;
    if (IsHostSet()
        && !GetCoverFile().startsWith("/")
        && !GetCoverFile().isEmpty()
        && !IsDefaultCoverFile(GetCoverFile()))
    {
        coverfile = generate_file_url("Coverart", GetHost(),
                                      GetCoverFile());
    }
    else
    {
        coverfile = GetCoverFile();
    }

    imageMap["coverfile"] = coverfile;
    imageMap["coverart"] = coverfile;

    QString screenshotfile;
    if (IsHostSet() && !GetScreenshot().startsWith("/")
        && !GetScreenshot().isEmpty())
    {
        screenshotfile = generate_file_url("Screenshots",
                                           GetHost(), GetScreenshot());
    }
    else
    {
        screenshotfile = GetScreenshot();
    }

    imageMap["screenshotfile"] = screenshotfile;
    imageMap["screenshot"] = screenshotfile;

    QString bannerfile;
    if (IsHostSet() && !GetBanner().startsWith("/")
        && !GetBanner().isEmpty())
    {
        bannerfile = generate_file_url("Banners", GetHost(),
                                       GetBanner());
    }
    else
    {
        bannerfile = GetBanner();
    }

    imageMap["bannerfile"] = bannerfile;
    imageMap["banner"] = bannerfile;

    QString fanartfile;
    if (IsHostSet() && !GetFanart().startsWith("/")
        && !GetFanart().isEmpty())
    {
        fanartfile = generate_file_url("Fanart", GetHost(),
                                       GetFanart());
    }
    else
    {
        fanartfile = GetFanart();
    }

    imageMap["fanartfile"] = fanartfile;
    imageMap["fanart"] = fanartfile;

    QString smartimage = coverfile;
    if (!screenshotfile.isEmpty () && (GetSeason() > 0 || GetEpisode() > 0))
        smartimage = screenshotfile;
    imageMap["smartimage"] = smartimage;
}

////////////////////////////////////////
//// Metadata
////////////////////////////////////////
VideoMetadata::SortKey VideoMetadata::GenerateDefaultSortKey(const VideoMetadata &m,
                                                   bool ignore_case)
{
    QString title(ignore_case ? m.GetTitle().toLower() : m.GetTitle());
    title = TrimTitle(title, ignore_case);

    return SortKey(SortData(title, m.GetFilename(),
                         QString().sprintf("%.7d", m.GetID())));
}

namespace
{
    QString eatBraces(const QString &title, const QString &left_brace,
                      const QString &right_brace)
    {
        QString ret(title);
        bool keep_checking = true;

        while (keep_checking)
        {
            int left_position = ret.indexOf(left_brace);
            int right_position = ret.indexOf(right_brace);
            if (left_position == -1 || right_position == -1)
            {
                //
                //  No matching sets of these braces left.
                //

                keep_checking = false;
            }
            else
            {
                if (left_position < right_position)
                {
                    //
                    //  We have a matching set like:  (  foo  )
                    //

                    ret = ret.left(left_position) +
                            ret.right(ret.length() - right_position - 1);
                }
                else if (left_position > right_position)
                {
                    //
                    //  We have a matching set like:  )  foo  (
                    //

                    ret = ret.left(right_position) +
                            ret.right(ret.length() - left_position - 1);
                }
            }
        }

        return ret;
    }
}

int VideoMetadata::UpdateHashedDBRecord(const QString &hash,
                                   const QString &file_name,
                                   const QString &host)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT intid,filename FROM videometadata WHERE "
                  "hash = :HASH");
    query.bindValue(":HASH", hash);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Video hashed metadata update", query);
        return -1;
    }

    if (!query.next())
        return -1;

    int intid = query.value(0).toInt();
    QString oldfilename = query.value(1).toString();

    query.prepare("UPDATE videometadata SET filename = :FILENAME, "
                  "host = :HOST WHERE intid = :INTID");
    query.bindValue(":FILENAME", file_name);
    query.bindValue(":HOST", host);
    query.bindValue(":INTID", intid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Video hashed metadata update (videometadata)", query);
        return -1;
    }

    query.prepare("UPDATE filemarkup SET filename = :FILENAME "
                  "WHERE filename = :OLDFILENAME");
    query.bindValue(":FILENAME", file_name);
    query.bindValue(":OLDFILENAME", oldfilename);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Video hashed metadata update (filemarkup)", query);
        return -1;
    }

    return intid;
}

QString VideoMetadata::VideoFileHash(const QString &file_name,
                           const QString &host)
{
    if (!host.isEmpty() && !gCoreContext->IsMasterHost(host))
    {
        QString url = generate_file_url("Videos", host, file_name);
        return RemoteFile::GetFileHash(url);
    }
    else if (!host.isEmpty())
    {
        StorageGroup sgroup("Videos", host);
        QString fullname = sgroup.FindFile(file_name);
        return FileHash(fullname);
    }
    else
        return FileHash(file_name);
}

QString VideoMetadata::FilenameToMeta(const QString &file_name, int position)
{
    // position 1 returns title, 2 returns season,
    //          3 returns episode, 4 returns subtitle

    QString cleanFilename = file_name.left(file_name.lastIndexOf('.'));
    cleanFilename.replace(QRegExp("%20"), " ");
    cleanFilename.replace(QRegExp("_"), " ");
    cleanFilename.replace(QRegExp("\\."), " ");

    /*: Word(s) which should be recognized as "season" when parsing a video
     * file name. To list more than one word, separate them with a '|'.
     */
    QString season_translation = tr("Season", "Metadata file name parsing");

    /*: Word(s) which should be recognized as "episode" when parsing a video
     * file name. To list more than one word, separate them with a '|'.
     */
    QString episode_translation = tr("Episode", "Metadata file name parsing");

    // Primary Regexp
    QString separator = "(?:\\s?(?:-|/)?\\s?)?";
    QString regexp = QString(
                  "^(.*[^s0-9])" // Title
                  "%1" // optional separator
                  "(?:s|(?:Season|%2))?" // season marker
                  "%1" // optional separator
                  "(\\d{1,4})" // Season
                  "%1" // optional separator
                  "(?:[ex/]|Episode|%3)" // episode marker
                  "%1" // optional separator
                  "(\\d{1,3})" // Episode
                  "%1" // optional separator
                  "(.*)$" // Subtitle
                  ).arg(separator)
                   .arg(season_translation).arg(episode_translation);
    QRegExp filename_parse(regexp,
                  Qt::CaseInsensitive, QRegExp::RegExp2);

    // Cleanup Regexp
    QString regexp2 = QString("(%1(?:(?:Season|%2)%1\\d*%1)*%1)$")
                             .arg(separator).arg(season_translation);
    QRegExp title_parse(regexp2, Qt::CaseInsensitive, QRegExp::RegExp2);

    int pos = filename_parse.indexIn(cleanFilename);
    if (pos > -1)
    {
        QString title = filename_parse.cap(1);
        QString season = filename_parse.cap(2);
        QString episode = filename_parse.cap(3);
        QString subtitle = filename_parse.cap(4);

        // Clean up the title
        int pos2 = title_parse.indexIn(title);
        if (pos2 > -1)
            title = title.left(pos2);
        title = title.right(title.length() -
                     title.lastIndexOf('/') -1);

        // Return requested value
        if (position == 1 && !title.isEmpty())
            return title.trimmed();
        else if (position == 2)
            return season.trimmed();
        else if (position == 3)
            return episode.trimmed();
        else if (position == 4)
            return subtitle.trimmed();
    }
    else if (position == 1)
    {
        QString title = cleanFilename;

        // Clean up the title
        title = title.right(title.length() -
                     title.lastIndexOf('/') -1);

        title = eatBraces(title, "[", "]");
        title = eatBraces(title, "(", ")");
        title = eatBraces(title, "{", "}");
        return title.trimmed();
    }
    else if (position == 2 || position == 3)
        return QString("0");

    return QString();
}

namespace
{
    const QRegExp &getTitleTrim(bool ignore_case)
    {
        static QString pattern(VideoMetadata::tr("^(The |A |An )"));
        static QRegExp prefixes_case(pattern, Qt::CaseSensitive);
        static QRegExp prefixes_nocase(pattern, Qt::CaseInsensitive);
        return ignore_case ? prefixes_nocase : prefixes_case;
    }
}

QString VideoMetadata::TrimTitle(const QString &title, bool ignore_case)
{
    QString ret(title);
    ret.remove(getTitleTrim(ignore_case));
    return ret;
}

VideoMetadata::VideoMetadata(const QString &filename, const QString &hash,
             const QString &trailer, const QString &coverfile,
             const QString &screenshot, const QString &banner, const QString &fanart,
             const QString &title, const QString &subtitle, const QString &tagline,
             int year, const QDate &releasedate, const QString &inetref, int collectionref,
             const QString &homepage, const QString &director, const QString &studio,
             const QString &plot, float userrating, const QString &rating,
             int length, int playcount, int season, int episode, const QDate &insertdate,
             int id, ParentalLevel::Level showlevel, int categoryID,
             int childID, bool browse, bool watched,
             const QString &playcommand, const QString &category,
             const genre_list &genres,
             const country_list &countries,
             const cast_list &cast,
             const QString &host, bool processed,
             VideoContentType contenttype)
{
    m_imp = new VideoMetadataImp(filename, hash, trailer, coverfile, screenshot, banner,
                            fanart, title, subtitle, tagline, year, releasedate, inetref,
                            collectionref, homepage, director, studio, plot, userrating, rating,
                            length, playcount, season, episode, insertdate, id, showlevel, categoryID,
                            childID, browse, watched, playcommand, category, genres, countries,
                            cast, host, processed, contenttype);
}

VideoMetadata::~VideoMetadata()
{
    delete m_imp;
}

VideoMetadata::VideoMetadata(MSqlQuery &query)
{
    m_imp = new VideoMetadataImp(query);
}

VideoMetadata::VideoMetadata(const VideoMetadata &rhs) : m_imp(NULL)
{
    *this = rhs;
}

VideoMetadata &VideoMetadata::operator=(const VideoMetadata &rhs)
{
    if (this != &rhs)
    {
        m_imp = new VideoMetadataImp(*(rhs.m_imp));
    }

    return *this;
}

void VideoMetadata::toMap(InfoMap &metadataMap)
{
    if (this == NULL)
        return;

    GetImageMap(metadataMap);

    metadataMap["filename"] = GetFilename();
    metadataMap["title"] = GetTitle();
    metadataMap["subtitle"] = GetSubtitle();
    metadataMap["tagline"] = GetTagline();
    metadataMap["director"] = GetDirector();
    metadataMap["studio"] = GetStudio();
    metadataMap["description"] = GetPlot();
    metadataMap["genres"] = GetDisplayGenres(*this);
    metadataMap["countries"] = GetDisplayCountries(*this);
    metadataMap["cast"] = GetDisplayCast(*this).join(", ");
    metadataMap["rating"] = GetDisplayRating(GetRating());
    metadataMap["length"] = GetDisplayLength(GetLength());
    metadataMap["playcount"] = QString::number(GetPlayCount());
    metadataMap["year"] = GetDisplayYear(GetYear());

    metadataMap["releasedate"] = MythDate::toString(
        GetReleaseDate(), MythDate::kDateFull | MythDate::kAddYear);

    metadataMap["userrating"] = GetDisplayUserRating(GetUserRating());

    if (GetSeason() > 0 || GetEpisode() > 0)
    {
        metadataMap["season"] = format_season_and_episode(GetSeason(), 1);
        metadataMap["episode"] = format_season_and_episode(GetEpisode(), 1);
        metadataMap["s##e##"] = QString("s%1e%2")
            .arg(format_season_and_episode(GetSeason(), 2))
            .arg(format_season_and_episode(GetEpisode(), 2));
        metadataMap["##x##"] = QString("%1x%2")
            .arg(format_season_and_episode(GetSeason(), 1))
            .arg(format_season_and_episode(GetEpisode(), 2));
    }
    else
    {
        metadataMap["s##e##"] = metadataMap["##x##"] = QString();
        metadataMap["season"] = metadataMap["episode"] = QString();
    }

    GetStateMap(metadataMap);

    metadataMap["insertdate"] = MythDate::toString(
        GetInsertdate(), MythDate::kDateFull | MythDate::kAddYear);
    metadataMap["inetref"] = GetInetRef();
    metadataMap["homepage"] = GetHomepage();
    metadataMap["child_id"] = QString::number(GetChildID());
    metadataMap["browseable"] = GetDisplayBrowse(GetBrowse());
    metadataMap["watched"] = GetDisplayWatched(GetWatched());
    metadataMap["processed"] = GetDisplayProcessed(GetProcessed());
    metadataMap["category"] = GetCategory();
}


void VideoMetadata::GetStateMap(InfoMap &stateMap)
{
    stateMap["trailerstate"] = TrailerToState(GetTrailer());
    stateMap["userratingstate"] =
            QString::number((int)(GetUserRating()));
    stateMap["watchedstate"] = WatchedToState(GetWatched());
    stateMap["videolevel"] = ParentalLevelToState(GetShowLevel());
}

void VideoMetadata::GetImageMap(InfoMap &imageMap)
{
    m_imp->GetImageMap(imageMap);
}

void ClearMap(InfoMap &metadataMap)
{
    metadataMap["coverfile"] = "";
    metadataMap["screenshotfile"] = "";
    metadataMap["bannerfile"] = "";
    metadataMap["fanartfile"] = "";
    metadataMap["filename"] = "";
    metadataMap["title"] = "";
    metadataMap["subtitle"] = "";
    metadataMap["tagline"] = "";
    metadataMap["director"] = "";
    metadataMap["studio"] = "";
    metadataMap["description"] = "";
    metadataMap["genres"] = "";
    metadataMap["countries"] = "";
    metadataMap["cast"] = "";
    metadataMap["rating"] = "";
    metadataMap["length"] = "";
    metadataMap["playcount"] = "";
    metadataMap["year"] = "";
    metadataMap["releasedate"] = "";
    metadataMap["userrating"] = "";
    metadataMap["season"] = "";
    metadataMap["episode"] = "";
    metadataMap["s##e##"] = "";
    metadataMap["##x##"] = "";
    metadataMap["trailerstate"] = "";
    metadataMap["userratingstate"] = "";
    metadataMap["watchedstate"] = "";
    metadataMap["videolevel"] = "";
    metadataMap["insertdate"] = "";
    metadataMap["inetref"] = "";
    metadataMap["homepage"] = "";
    metadataMap["child_id"] = "";
    metadataMap["browseable"] = "";
    metadataMap["watched"] = "";
    metadataMap["category"] = "";
    metadataMap["processed"] = "";
}

bool VideoMetadata::HasSortKey() const
{
    return m_imp->HasSortKey();
}

const VideoMetadata::SortKey &VideoMetadata::GetSortKey() const
{
    return m_imp->GetSortKey();
}

void VideoMetadata::SetSortKey(const VideoMetadata::SortKey &sort_key)
{
    m_imp->SetSortKey(sort_key);
}

const QString &VideoMetadata::GetPrefix() const
{
    return m_imp->GetPrefix();
}

void VideoMetadata::SetPrefix(const QString &prefix)
{
    m_imp->SetPrefix(prefix);
}

const QString &VideoMetadata::GetTitle() const
{
    return m_imp->getTitle();
}

void VideoMetadata::SetTitle(const QString &title)
{
    m_imp->SetTitle(title);
}

const QString &VideoMetadata::GetSubtitle() const
{
    return m_imp->getSubtitle();
}

void VideoMetadata::SetSubtitle(const QString &subtitle)
{
    m_imp->SetSubtitle(subtitle);
}

const QString &VideoMetadata::GetTagline() const
{
    return m_imp->GetTagline();
}

void VideoMetadata::SetTagline(const QString &tagline)
{
    m_imp->SetTagline(tagline);
}

int VideoMetadata::GetYear() const
{
    return m_imp->getYear();
}

void VideoMetadata::SetYear(int year)
{
    m_imp->SetYear(year);
}

QDate VideoMetadata::GetReleaseDate() const
{
    return m_imp->getReleaseDate();
}

void VideoMetadata::SetReleaseDate(QDate releasedate)
{
    m_imp->SetReleaseDate(releasedate);
}

const QString &VideoMetadata::GetInetRef() const
{
    return m_imp->GetInetRef();
}

void VideoMetadata::SetInetRef(const QString &inetRef)
{
    m_imp->SetInetRef(inetRef);
}

int VideoMetadata::GetCollectionRef() const
{
    return m_imp->GetCollectionRef();
}

void VideoMetadata::SetCollectionRef(int collectionref)
{
    m_imp->SetCollectionRef(collectionref);
}

const QString &VideoMetadata::GetHomepage() const
{
    return m_imp->GetHomepage();
}

void VideoMetadata::SetHomepage(const QString &homepage)
{
    m_imp->SetHomepage(homepage);
}

const QString &VideoMetadata::GetDirector() const
{
    return m_imp->getDirector();
}

void VideoMetadata::SetDirector(const QString &director)
{
    m_imp->SetDirector(director);
}

const QString &VideoMetadata::GetStudio() const
{
    return m_imp->getStudio();
}

void VideoMetadata::SetStudio(const QString &studio)
{
    m_imp->SetStudio(studio);
}

const QString &VideoMetadata::GetPlot() const
{
    return m_imp->getPlot();
}

void VideoMetadata::SetPlot(const QString &plot)
{
    m_imp->SetPlot(plot);
}

float VideoMetadata::GetUserRating() const
{
    return m_imp->GetUserRating();
}

void VideoMetadata::SetUserRating(float userRating)
{
    m_imp->SetUserRating(userRating);
}

const QString &VideoMetadata::GetRating() const
{
    return m_imp->GetRating();
}

void VideoMetadata::SetRating(const QString &rating)
{
    m_imp->SetRating(rating);
}

int VideoMetadata::GetLength() const
{
    return m_imp->GetLength();
}

void VideoMetadata::SetLength(int length)
{
    m_imp->SetLength(length);
}

unsigned int VideoMetadata::GetPlayCount() const
{
    return m_imp->GetPlayCount();
}

void VideoMetadata::SetPlayCount(int count)
{
    m_imp->SetPlayCount(count);
}

int VideoMetadata::GetSeason() const
{
    return m_imp->GetSeason();
}

void VideoMetadata::SetSeason(int season)
{
    m_imp->SetSeason(season);
}

int VideoMetadata::GetEpisode() const
{
    return m_imp->GetEpisode();
}

void VideoMetadata::SetEpisode(int episode)
{
    m_imp->SetEpisode(episode);
}

QDate VideoMetadata::GetInsertdate() const
{
    return m_imp->GetInsertdate();
}

void VideoMetadata::SetInsertdate(QDate date)
{
    m_imp->SetInsertdate(date);
}

unsigned int VideoMetadata::GetID() const
{
    return m_imp->GetID();
}

void VideoMetadata::SetID(int id)
{
    m_imp->SetID(id);
}

int VideoMetadata::GetChildID() const
{
    return m_imp->GetChildID();
}

void VideoMetadata::SetChildID(int childID)
{
    m_imp->SetChildID(childID);
}

bool VideoMetadata::GetBrowse() const
{
    return m_imp->GetBrowse();
}

void VideoMetadata::SetBrowse(bool browse)
{
    m_imp->SetBrowse(browse);
}

bool VideoMetadata::GetWatched() const
{
    return m_imp->GetWatched();
}

void VideoMetadata::SetWatched(bool watched)
{
    m_imp->SetWatched(watched);
}

bool VideoMetadata::GetProcessed() const
{
    return m_imp->GetProcessed();
}

void VideoMetadata::SetProcessed(bool processed)
{
    m_imp->SetProcessed(processed);
}

VideoContentType VideoMetadata::GetContentType() const
{
    return m_imp->GetContentType();
}

void VideoMetadata::SetContentType(VideoContentType contenttype)
{
    m_imp->SetContentType(contenttype);
}

const QString &VideoMetadata::GetPlayCommand() const
{
    return m_imp->getPlayCommand();
}

void VideoMetadata::SetPlayCommand(const QString &playCommand)
{
    m_imp->SetPlayCommand(playCommand);
}

ParentalLevel::Level VideoMetadata::GetShowLevel() const
{
    return m_imp->GetShowLevel();
}

void VideoMetadata::SetShowLevel(ParentalLevel::Level showLevel)
{
    m_imp->SetShowLevel(showLevel);
}

const QString &VideoMetadata::GetFilename() const
{
    return m_imp->getFilename();
}

const QString &VideoMetadata::GetHash() const
{
    return m_imp->GetHash();
}

void VideoMetadata::SetHash(const QString &hash)
{
    return m_imp->SetHash(hash);
}

const QString &VideoMetadata::GetHost() const
{
    return m_imp->GetHost();
}

void VideoMetadata::SetHost(const QString &host)
{
        m_imp->SetHost(host);
}

void VideoMetadata::SetFilename(const QString &filename)
{
    m_imp->SetFilename(filename);
}

const QString &VideoMetadata::GetTrailer() const
{
    return m_imp->GetTrailer();
}

void VideoMetadata::SetTrailer(const QString &trailer)
{
    m_imp->SetTrailer(trailer);
}

const QString &VideoMetadata::GetCoverFile() const
{
    return m_imp->GetCoverFile();
}

void VideoMetadata::SetCoverFile(const QString &coverFile)
{
    m_imp->SetCoverFile(coverFile);
}

const QString &VideoMetadata::GetScreenshot() const
{
    return m_imp->GetScreenshot();
}

void VideoMetadata::SetScreenshot(const QString &screenshot)
{
    m_imp->SetScreenshot(screenshot);
}

const QString &VideoMetadata::GetBanner() const
{
    return m_imp->GetBanner();
}

void VideoMetadata::SetBanner(const QString &banner)
{
    m_imp->SetBanner(banner);
}

const QString &VideoMetadata::GetFanart() const
{
    return m_imp->GetFanart();
}

void VideoMetadata::SetFanart(const QString &fanart)
{
    m_imp->SetFanart(fanart);
}

const QString &VideoMetadata::GetCategory() const
{
    return m_imp->GetCategory();
}

//void VideoMetadata::SetCategory(const QString &category)
//{
//    m_imp->SetCategory(category);
//}

const VideoMetadata::genre_list &VideoMetadata::GetGenres() const
{
    return m_imp->getGenres();
}

void VideoMetadata::SetGenres(const genre_list &genres)
{
    m_imp->SetGenres(genres);
}

const VideoMetadata::cast_list &VideoMetadata::GetCast() const
{
    return m_imp->GetCast();
}

void VideoMetadata::SetCast(const cast_list &cast)
{
    m_imp->SetCast(cast);
}

const VideoMetadata::country_list &VideoMetadata::GetCountries() const
{
    return m_imp->GetCountries();
}

void VideoMetadata::SetCountries(const country_list &countries)
{
    m_imp->SetCountries(countries);
}

int VideoMetadata::GetCategoryID() const
{
    return m_imp->GetCategoryID();
}

void VideoMetadata::SetCategoryID(int id)
{
    m_imp->SetCategoryID(id);
}

void VideoMetadata::SaveToDatabase()
{
    m_imp->SaveToDatabase();
}

void VideoMetadata::UpdateDatabase()
{
    m_imp->UpdateDatabase();
}

bool VideoMetadata::DeleteFromDatabase()
{
    return m_imp->DeleteFromDatabase();
}

#if 0
bool VideoMetadata::fillDataFromID(const VideoMetadataListManager &cache)
{
    if (m_imp->getID() == 0)
        return false;

    VideoMetadataListManager::VideoMetadataPtr mp = cache.byID(m_imp->getID());
    if (mp.get())
    {
        *this = *mp;
        return true;
    }

    return false;
}
#endif

bool VideoMetadata::FillDataFromFilename(const VideoMetadataListManager &cache)
{
    if (m_imp->getFilename().isEmpty())
        return false;

    VideoMetadataListManager::VideoMetadataPtr mp =
            cache.byFilename(m_imp->getFilename());
    if (mp)
    {
        *this = *mp;
        return true;
    }

    return false;
}

bool VideoMetadata::DeleteFile()
{
    return m_imp->DeleteFile();
}

void VideoMetadata::Reset()
{
    m_imp->Reset();
}

bool VideoMetadata::IsHostSet() const
{
    return m_imp->IsHostSet();
}

bool operator==(const VideoMetadata& a, const VideoMetadata& b)
{
    if (a.GetFilename() == b.GetFilename())
        return true;
    return false;
}

bool operator!=(const VideoMetadata& a, const VideoMetadata& b)
{
    if (a.GetFilename() != b.GetFilename())
        return true;
    return false;
}

bool operator<(const VideoMetadata::SortKey &lhs, const VideoMetadata::SortKey &rhs)
{
    if (lhs.m_sd && rhs.m_sd)
        return *lhs.m_sd < *rhs.m_sd;
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Error: Bug, Metadata item with empty sort key compared");
        return lhs.m_sd < rhs.m_sd;
    }
}
