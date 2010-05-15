#include <cmath> // for isnan()

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegExp>

#include <mythcontext.h>
#include <mythdb.h>
#include <remotefile.h>
#include <remoteutil.h>
#include <util.h>
#include <mythverbose.h>

#include "globals.h"
#include "dbaccess.h"
#include "metadatalistmanager.h"
#include "videoutils.h"

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

bool operator<(const SortData &lhs, const SortData &rhs)
{
    int ret = QString::localeAwareCompare(lhs.m_title, rhs.m_title);

    if (ret == 0)
        ret = QString::localeAwareCompare(lhs.m_filename, rhs.m_filename);

    if (ret == 0)
        ret = QString::localeAwareCompare(lhs.m_id, rhs.m_id);

    return ret < 0;
}

Metadata::SortKey::SortKey() : m_sd(0)
{
}

Metadata::SortKey::SortKey(const SortData &data)
{
    m_sd = new SortData(data);
}

Metadata::SortKey::SortKey(const SortKey &other)
{
    *this = other;
}

Metadata::SortKey &Metadata::SortKey::operator=(const SortKey &rhs)
{
    if (this != &rhs)
    {
        Clear();
        if (rhs.m_sd)
            m_sd = new SortData(*rhs.m_sd);
    }

    return *this;
}

Metadata::SortKey::~SortKey()
{
    Clear();
}

bool Metadata::SortKey::isSet() const
{
    return m_sd != 0;
}

void Metadata::SortKey::Clear()
{
    delete m_sd;
    m_sd = 0;
}

class MetadataImp
{
  public:
    typedef Metadata::genre_list genre_list;
    typedef Metadata::country_list country_list;
    typedef Metadata::cast_list cast_list;

  public:
    MetadataImp(const QString &filename, const QString &hash, const QString &trailer,
             const QString &coverfile, const QString &screenshot, const QString &banner,
             const QString &fanart, const QString &title, const QString &subtitle,
             const QString &tagline, int year, const QDate &releasedate,
             const QString &inetref, const QString &homepage,
             const QString &director, const QString &plot, float userrating,
             const QString &rating, int length,
             int season, int episode, const QDate &insertdate,
             int id, ParentalLevel::Level showlevel, int categoryID,
             int childID, bool browse, bool watched,
             const QString &playcommand, const QString &category,
             const genre_list &genres,
             const country_list &countries,
             const cast_list &cast,
             const QString &host = "") :
        m_title(title), m_subtitle(subtitle), m_tagline(tagline),
        m_inetref(inetref), m_homepage(homepage), m_director(director), m_plot(plot),
        m_rating(rating), m_playcommand(playcommand), m_category(category),
        m_genres(genres), m_countries(countries), m_cast(cast),
        m_filename(filename), m_hash(hash), m_trailer(trailer), m_coverfile(coverfile),
        m_screenshot(screenshot), m_banner(banner), m_fanart(fanart),
        m_host(host), m_categoryID(categoryID), m_childID(childID),
        m_year(year), m_releasedate(releasedate), m_length(length), m_season(season),
        m_episode(episode), m_insertdate(insertdate), m_showlevel(showlevel),
        m_browse(browse), m_watched(watched), m_id(id), 
        m_userrating(userrating)
    {
        VideoCategory::GetCategory().get(m_categoryID, m_category);
    }

    MetadataImp(MSqlQuery &query)
    {
        fromDBRow(query);
    }

    MetadataImp(const MetadataImp &other)
    {
        *this = other;
    }

    MetadataImp &operator=(const MetadataImp &rhs)
    {
        if (this != &rhs)
        {
            m_title = rhs.m_title;
            m_subtitle = rhs.m_subtitle;
            m_tagline = rhs.m_tagline;
            m_inetref = rhs.m_inetref;
            m_homepage = rhs.m_homepage;
            m_director = rhs.m_director;
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
            m_season = rhs.m_season;
            m_episode = rhs.m_episode;
            m_insertdate = rhs.m_insertdate;
            m_showlevel = rhs.m_showlevel;
            m_browse = rhs.m_browse;
            m_watched = rhs.m_watched;
            m_id = rhs.m_id;
            m_userrating = rhs.m_userrating;
            m_host = rhs.m_host;

            // No DB vars
            m_sort_key = rhs.m_sort_key;
            m_prefix = rhs.m_prefix;
        }

        return *this;
    }

  public:
    bool HasSortKey() const { return m_sort_key.isSet(); }
    const Metadata::SortKey &GetSortKey() const { return m_sort_key; }
    void SetSortKey(const Metadata::SortKey &sort_key)
    {
        m_sort_key = sort_key;
    }

    const QString &GetPrefix() const { return m_prefix; }
    void SetPrefix(const QString &prefix) { m_prefix = prefix; }

    const QString &getTitle() const { return m_title; }
    void SetTitle(const QString& title)
    {
        m_sort_key.Clear();
        m_title = title;
    }

    const QString &getSubtitle() const { return m_subtitle; }
    void SetSubtitle(const QString &subtitle) { m_subtitle = subtitle; }

    const QString &GetTagline() const { return m_tagline; }
    void SetTagline(const QString &tagline) { m_tagline = tagline; }

    const QString &GetInetRef() const { return m_inetref; }
    void SetInetRef(const QString &inetRef) { m_inetref = inetRef; }

    const QString &GetHomepage() const { return m_homepage; }
    void SetHomepage(const QString &homepage) { m_homepage = homepage; }

    const QString &getDirector() const { return m_director; }
    void SetDirector(const QString &director) { m_director = director; }

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

    ////////////////////////////////

    void SaveToDatabase();
    void UpdateDatabase();
    bool DeleteFromDatabase();

    bool DeleteFile(class VideoList &dummy);

    void Reset();

    bool IsHostSet() const;

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
    QString m_homepage;
    QString m_director;
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
    int m_season;
    int m_episode;
    QDate m_insertdate;
    ParentalLevel::Level m_showlevel;
    bool m_browse;
    bool m_watched;
    unsigned int m_id;  // videometadata.intid
    float m_userrating;

    // not in DB
    Metadata::SortKey m_sort_key;
    QString m_prefix;
};

/////////////////////////////
/////////
/////////////////////////////
bool MetadataImp::removeDir(const QString &dirName)
{
    QDir d(dirName);

    QFileInfoList contents = d.entryInfoList();
    if (!contents.size())
    {
        return d.rmdir(dirName);
    }

    for (QFileInfoList::iterator p = contents.begin(); p != contents.end(); ++p)
    {
        if (p->fileName() == "." ||
            p->fileName() == "..")
        {
            continue;
        }
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
bool MetadataImp::DeleteFile(class VideoList &dummy)
{
    (void) dummy;
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
        VERBOSE(VB_IMPORTANT, QString("Could not delete file: %1")
                                                            .arg(m_filename));
    }

    return isremoved;
}

void MetadataImp::Reset()
{
    MetadataImp tmp(m_filename, Metadata::VideoFileHash(m_filename, m_host), VIDEO_TRAILER_DEFAULT,
                    VIDEO_COVERFILE_DEFAULT, VIDEO_SCREENSHOT_DEFAULT, VIDEO_BANNER_DEFAULT,
                    VIDEO_FANART_DEFAULT, Metadata::FilenameToMeta(m_filename, 1), QString(),
                    Metadata::FilenameToMeta(m_filename, 4), VIDEO_YEAR_DEFAULT, 
                    QDate(), VIDEO_INETREF_DEFAULT, QString(), VIDEO_DIRECTOR_DEFAULT, 
                    VIDEO_PLOT_DEFAULT, 0.0,
                    VIDEO_RATING_DEFAULT, 0, 
                    Metadata::FilenameToMeta(m_filename, 2).toInt(), 
                    Metadata::FilenameToMeta(m_filename, 3).toInt(), QDate(), m_id,
                    ParentalLevel::plLowest, 0, -1, true, false, "", "",
                    Metadata::genre_list(), Metadata::country_list(),
                    Metadata::cast_list(), m_host);
    tmp.m_prefix = m_prefix;

    *this = tmp;
}

bool MetadataImp::IsHostSet() const
{
    return !m_host.isEmpty();
}

void MetadataImp::fillGenres()
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

void MetadataImp::fillCountries()
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

void MetadataImp::fillCast()
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
void MetadataImp::fromDBRow(MSqlQuery &query)
{
    m_title = query.value(0).toString();
    m_director = query.value(1).toString();
    m_plot = query.value(2).toString();
    m_rating = query.value(3).toString();
    m_year = query.value(4).toInt();
    m_releasedate = query.value(5).toDate();
    m_userrating = (float)query.value(6).toDouble();
    if (isnan(m_userrating) || m_userrating < 0)
        m_userrating = 0.0;
    if (m_userrating > 10.0)
        m_userrating = 10.0;
    m_length = query.value(7).toInt();
    m_filename = query.value(8).toString();
    m_hash = query.value(9).toString();
    m_showlevel = ParentalLevel(query.value(10).toInt()).GetLevel();
    m_coverfile = query.value(11).toString();
    m_inetref = query.value(12).toString();
    m_homepage = query.value(13).toString();
    m_childID = query.value(14).toUInt();
    m_browse = query.value(15).toBool();
    m_watched = query.value(16).toBool();
    m_playcommand = query.value(17).toString();
    m_categoryID = query.value(18).toInt();
    m_id = query.value(19).toInt();
    m_trailer = query.value(20).toString();
    m_screenshot = query.value(21).toString();
    m_banner = query.value(22).toString();
    m_fanart = query.value(23).toString();
    m_subtitle = query.value(24).toString();
    m_tagline = query.value(25).toString();
    m_season = query.value(26).toInt();
    m_episode = query.value(27).toInt();
    m_host = query.value(28).toString();
    m_insertdate = query.value(29).toDate();

    VideoCategory::GetCategory().get(m_categoryID, m_category);

    // Genres
    fillGenres();

    //Countries
    fillCountries();

    // Cast
    fillCast();
}

void MetadataImp::saveToDatabase()
{
    if (m_title.isEmpty())
        m_title = Metadata::FilenameToMeta(m_filename, 1);
    if (m_hash.isEmpty())
        m_hash = Metadata::VideoFileHash(m_filename, m_host);
    if (m_subtitle.isEmpty())
        m_subtitle = Metadata::FilenameToMeta(m_filename, 4);
    if (m_director.isEmpty())
        m_director = VIDEO_DIRECTOR_UNKNOWN;
    if (m_plot.isEmpty())
        m_plot = VIDEO_PLOT_DEFAULT;
    if (m_rating.isEmpty())
        m_rating = VIDEO_RATING_DEFAULT;
    if (m_coverfile.isEmpty())
        m_coverfile = VIDEO_COVERFILE_DEFAULT;
    if (m_screenshot.isEmpty())
        m_screenshot = VIDEO_SCREENSHOT_DEFAULT;
    if (m_banner.isEmpty())
        m_banner = VIDEO_BANNER_DEFAULT;
    if (m_fanart.isEmpty())
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

    bool inserting = m_id == 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (inserting)
    {
        m_browse = 1;

        m_watched = 0;

        query.prepare("INSERT INTO videometadata (title,subtitle, tagline,director,plot,"
                      "rating,year,userrating,length,season,episode,filename,hash,"
                      "showlevel,coverfile,inetref,homepage,browse,watched,trailer,"
                      "screenshot,banner,fanart,host) VALUES (:TITLE, :SUBTITLE, "
                      ":TAGLINE, :DIRECTOR, :PLOT, :RATING, :YEAR, :USERRATING, "
                      ":LENGTH, :SEASON, :EPISODE, :FILENAME, :HASH, :SHOWLEVEL, "
                      ":COVERFILE, :INETREF, :HOMEPAGE, :BROWSE, :WATCHED, "
                      ":TRAILER, :SCREENSHOT, :BANNER, :FANART, :HOST)");
    }
    else
    {
        query.prepare("UPDATE videometadata SET title = :TITLE, subtitle = :SUBTITLE, "
                      "tagline = :TAGLINE, director = :DIRECTOR, plot = :PLOT, rating= :RATING, "
                      "year = :YEAR, releasedate = :RELEASEDATE, userrating = :USERRATING, "
                      "length = :LENGTH, season = :SEASON, episode = :EPISODE, "
                      "filename = :FILENAME, hash = :HASH, trailer = :TRAILER, "
                      "showlevel = :SHOWLEVEL, coverfile = :COVERFILE, "
                      "screenshot = :SCREENSHOT, banner = :BANNER, fanart = :FANART, "
                      "inetref = :INETREF, homepage = :HOMEPAGE, browse = :BROWSE, "
                      "watched = :WATCHED, host = :HOST, playcommand = :PLAYCOMMAND, "
                      "childid = :CHILDID, category = :CATEGORY WHERE intid = :INTID");

        query.bindValue(":PLAYCOMMAND", m_playcommand);
        query.bindValue(":CHILDID", m_childID);
        query.bindValue(":CATEGORY", m_categoryID);
        query.bindValue(":INTID", m_id);
    }

    query.bindValue(":TITLE", m_title);
    query.bindValue(":SUBTITLE", m_subtitle);
    query.bindValue(":TAGLINE", m_tagline);
    query.bindValue(":DIRECTOR", m_director);
    query.bindValue(":PLOT", m_plot);
    query.bindValue(":RATING", m_rating);
    query.bindValue(":YEAR", m_year);
    query.bindValue(":RELEASEDATE", m_releasedate);
    query.bindValue(":USERRATING", m_userrating);
    query.bindValue(":LENGTH", m_length);
    query.bindValue(":SEASON", m_season);
    query.bindValue(":EPISODE", m_episode);
    query.bindValue(":FILENAME", m_filename);
    query.bindValue(":HASH", m_hash);
    query.bindValue(":TRAILER", m_trailer);
    query.bindValue(":SHOWLEVEL", m_showlevel);
    query.bindValue(":COVERFILE", m_coverfile);
    query.bindValue(":SCREENSHOT", m_screenshot);
    query.bindValue(":BANNER", m_banner);
    query.bindValue(":FANART", m_fanart);
    query.bindValue(":INETREF", m_inetref);
    query.bindValue(":HOMEPAGE", m_homepage);
    query.bindValue(":BROWSE", m_browse);
    query.bindValue(":WATCHED", m_watched);
    query.bindValue(":HOST", m_host);

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
            VERBOSE(VB_IMPORTANT,
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

void MetadataImp::SaveToDatabase()
{
    saveToDatabase();
}

void MetadataImp::UpdateDatabase()
{
    saveToDatabase();
}

bool MetadataImp::DeleteFromDatabase()
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

void MetadataImp::SetCategoryID(int id)
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
                VERBOSE(VB_IMPORTANT, QString("Unknown category id"));
            }
        }
    }
}

void MetadataImp::updateGenres()
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

void MetadataImp::updateCountries()
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

void MetadataImp::updateCast()
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

////////////////////////////////////////
//// Metadata
////////////////////////////////////////
Metadata::SortKey Metadata::GenerateDefaultSortKey(const Metadata &m,
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

int Metadata::UpdateHashedDBRecord(const QString &hash,
                                   const QString &file_name,
                                   const QString &host)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT intid FROM videometadata WHERE "
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

    query.prepare("UPDATE videometadata SET filename = :FILENAME, "
                  "host = :HOST WHERE intid = :INTID");
    query.bindValue(":FILENAME", file_name);
    query.bindValue(":HOST", host);
    query.bindValue(":INTID", intid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Video hashed metadata update", query);
        return -1;
    }

    return intid;
}

QString Metadata::VideoFileHash(const QString &file_name,
                           const QString &host)
{
    if (!host.isEmpty())
    {
        QString url = generate_file_url("Videos", host, file_name);
        return RemoteFile::GetFileHash(url);
    }
    else
        return FileHash(file_name);
}

QString Metadata::FilenameToMeta(const QString &file_name, int position)
{
    // position 1 returns title, 2 returns season,
    //          3 returns episode, 4 returns subtitle

    QString cleanFilename = file_name.left(file_name.lastIndexOf('.'));
    cleanFilename.replace(QRegExp("%20"), " ");
    cleanFilename.replace(QRegExp("_"), " ");
    cleanFilename.replace(QRegExp("\\."), " ");

    QString season_translation = QObject::tr("Season");
    QString episode_translation = QObject::tr("Episode");

    // Primary Regexp
    QString separator = "(?:\\s?(?:-|/)?\\s?)?";
    QString regexp = QString(
                  "^(.*[^s0-9])" // Title
                  "%1" // optional separator
                  "(?:s|(?:%2))?" // season marker
                  "%1" // optional separator
                  "(\\d{1,4})" // Season
                  "%1" // optional separator
                  "(?:[ex/]|%3)" // episode marker
                  "%1" // optional separator
                  "(\\d{1,3})" // Episode
                  "%1" // optional separator
                  "(.*)$" // Subtitle
                  ).arg(separator)
                   .arg(season_translation).arg(episode_translation);
    QRegExp filename_parse(regexp,
                  Qt::CaseInsensitive, QRegExp::RegExp2);

    // Cleanup Regexp
    QString regexp2 = QString("(%1(?:%2%1\\d*%1)*%1)$")
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
        static QString pattern(QObject::tr("^(The |A |An )"));
        static QRegExp prefixes_case(pattern, Qt::CaseSensitive);
        static QRegExp prefixes_nocase(pattern, Qt::CaseInsensitive);
        return ignore_case ? prefixes_nocase : prefixes_case;
    }
}

QString Metadata::TrimTitle(const QString &title, bool ignore_case)
{
    QString ret(title);
    ret.remove(getTitleTrim(ignore_case));
    return ret;
}

Metadata::Metadata(const QString &filename, const QString &hash,
             const QString &trailer, const QString &coverfile,
             const QString &screenshot, const QString &banner, const QString &fanart,
             const QString &title, const QString &subtitle, const QString &tagline,
             int year, const QDate &releasedate, const QString &inetref,
             const QString &homepage, const QString &director,
             const QString &plot, float userrating, const QString &rating,
             int length, int season, int episode, const QDate &insertdate,
             int id, ParentalLevel::Level showlevel, int categoryID,
             int childID, bool browse, bool watched,
             const QString &playcommand, const QString &category,
             const genre_list &genres,
             const country_list &countries,
             const cast_list &cast,
             const QString &host)
{
    m_imp = new MetadataImp(filename, hash, trailer, coverfile, screenshot, banner,
                            fanart, title, subtitle, tagline, year, releasedate, inetref,
                            homepage, director, plot, userrating, rating, length, season, episode,
                            insertdate, id, showlevel, categoryID, childID, browse, watched, 
                            playcommand, category, genres, countries, cast, host);
}

Metadata::~Metadata()
{
    delete m_imp;
}

Metadata::Metadata(MSqlQuery &query)
{
    m_imp = new MetadataImp(query);
}

Metadata::Metadata(const Metadata &rhs)
{
    *this = rhs;
}

Metadata &Metadata::operator=(const Metadata &rhs)
{
    if (this != &rhs)
    {
        m_imp = new MetadataImp(*(rhs.m_imp));
    }

    return *this;
}

void Metadata::toMap(MetadataMap &metadataMap)
{
    if (this == NULL)
        return;

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

    metadataMap["coverfile"] = coverfile;

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

    metadataMap["screenshotfile"] = screenshotfile;

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

    metadataMap["bannerfile"] = bannerfile;

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

    metadataMap["fanartfile"] = fanartfile;

    metadataMap["filename"] = GetFilename();
    metadataMap["title"] = GetTitle();
    metadataMap["subtitle"] = GetSubtitle();
    metadataMap["tagline"] = GetTagline();
    metadataMap["director"] = GetDirector();
    metadataMap["description"] = GetPlot();
    metadataMap["genres"] = GetDisplayGenres(*this);
    metadataMap["countries"] = GetDisplayCountries(*this);
    metadataMap["cast"] = GetDisplayCast(*this).join(", ");
    metadataMap["rating"] = GetDisplayRating(GetRating());
    metadataMap["length"] = GetDisplayLength(GetLength());
    metadataMap["year"] = GetDisplayYear(GetYear());

    QString formatLongDate = gCoreContext->GetSetting("DateFormat", "ddd MMMM d");
    metadataMap["releasedate"] = GetReleaseDate().toString(formatLongDate);

    metadataMap["userrating"] = GetDisplayUserRating(GetUserRating());
    metadataMap["season"] = GetDisplaySeasonEpisode(GetSeason(), 1);
    metadataMap["episode"] = GetDisplaySeasonEpisode(GetEpisode(), 1);

    if (GetSeason() > 0 || GetEpisode() > 0)
    {
        metadataMap["s##e##"] = QString("s%1e%2").arg(GetDisplaySeasonEpisode
                                             (GetSeason(), 2))
                        .arg(GetDisplaySeasonEpisode(GetEpisode(), 2));
        metadataMap["##x##"] = QString("%1x%2").arg(GetDisplaySeasonEpisode
                                             (GetSeason(), 1))
                        .arg(GetDisplaySeasonEpisode(GetEpisode(), 2));
    }
    else
        metadataMap["s##e##"] = metadataMap["##x##"] = QString();

    metadataMap["trailerstate"] = TrailerToState(GetTrailer());
    metadataMap["userratingstate"] =
            QString::number((int)(GetUserRating()));
    metadataMap["watchedstate"] = WatchedToState(GetWatched());

    metadataMap["videolevel"] = ParentalLevelToState(GetShowLevel());

    metadataMap["insertdate"] = GetInsertdate()
                             .toString(gCoreContext->GetSetting("DateFormat"));
    metadataMap["inetref"] = GetInetRef();
    metadataMap["homepage"] = GetHomepage();
    metadataMap["child_id"] = QString::number(GetChildID());
    metadataMap["browseable"] = GetDisplayBrowse(GetBrowse());
    metadataMap["watched"] = GetDisplayWatched(GetWatched());
    metadataMap["category"] = GetCategory();
}

void ClearMap(MetadataMap &metadataMap)
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
    metadataMap["description"] = "";
    metadataMap["genres"] = "";
    metadataMap["countries"] = "";
    metadataMap["cast"] = "";
    metadataMap["rating"] = "";
    metadataMap["length"] = "";
    metadataMap["year"] = "";
    metadataMap["releasedate"] = "";
    metadataMap["userrating"] = "";
    metadataMap["season"] = "";
    metadataMap["episode"] = "";
    metadataMap["s##e##"] = "";
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
}

bool Metadata::HasSortKey() const
{
    return m_imp->HasSortKey();
}

const Metadata::SortKey &Metadata::GetSortKey() const
{
    return m_imp->GetSortKey();
}

void Metadata::SetSortKey(const Metadata::SortKey &sort_key)
{
    m_imp->SetSortKey(sort_key);
}

const QString &Metadata::GetPrefix() const
{
    return m_imp->GetPrefix();
}

void Metadata::SetPrefix(const QString &prefix)
{
    m_imp->SetPrefix(prefix);
}

const QString &Metadata::GetTitle() const
{
    return m_imp->getTitle();
}

void Metadata::SetTitle(const QString &title)
{
    m_imp->SetTitle(title);
}

const QString &Metadata::GetSubtitle() const
{
    return m_imp->getSubtitle();
}
 
void Metadata::SetSubtitle(const QString &subtitle)
{
    m_imp->SetSubtitle(subtitle);
}

const QString &Metadata::GetTagline() const
{
    return m_imp->GetTagline();
}

void Metadata::SetTagline(const QString &tagline)
{
    m_imp->SetTagline(tagline);
}

int Metadata::GetYear() const
{
    return m_imp->getYear();
}

void Metadata::SetYear(int year)
{
    m_imp->SetYear(year);
}

QDate Metadata::GetReleaseDate() const
{
    return m_imp->getReleaseDate();
}
             
void Metadata::SetReleaseDate(QDate releasedate)
{
    m_imp->SetReleaseDate(releasedate);
}

const QString &Metadata::GetInetRef() const
{
    return m_imp->GetInetRef();
}

void Metadata::SetInetRef(const QString &inetRef)
{
    m_imp->SetInetRef(inetRef);
}

const QString &Metadata::GetHomepage() const
{
    return m_imp->GetHomepage();
}

void Metadata::SetHomepage(const QString &homepage)
{
    m_imp->SetHomepage(homepage);
}

const QString &Metadata::GetDirector() const
{
    return m_imp->getDirector();
}

void Metadata::SetDirector(const QString &director)
{
    m_imp->SetDirector(director);
}

const QString &Metadata::GetPlot() const
{
    return m_imp->getPlot();
}

void Metadata::SetPlot(const QString &plot)
{
    m_imp->SetPlot(plot);
}

float Metadata::GetUserRating() const
{
    return m_imp->GetUserRating();
}

void Metadata::SetUserRating(float userRating)
{
    m_imp->SetUserRating(userRating);
}

const QString &Metadata::GetRating() const
{
    return m_imp->GetRating();
}

void Metadata::SetRating(const QString &rating)
{
    m_imp->SetRating(rating);
}

int Metadata::GetLength() const
{
    return m_imp->GetLength();
}

void Metadata::SetLength(int length)
{
    m_imp->SetLength(length);
}

int Metadata::GetSeason() const
{
    return m_imp->GetSeason();
}

void Metadata::SetSeason(int season)
{
    m_imp->SetSeason(season);
}

int Metadata::GetEpisode() const
{
    return m_imp->GetEpisode();
}

void Metadata::SetEpisode(int episode)
{
    m_imp->SetEpisode(episode);
}

QDate Metadata::GetInsertdate() const
{
	return m_imp->GetInsertdate();
}

void Metadata::SetInsertdate(QDate date)
{
	m_imp->SetInsertdate(date);
}

unsigned int Metadata::GetID() const
{
    return m_imp->GetID();
}

void Metadata::SetID(int id)
{
    m_imp->SetID(id);
}

int Metadata::GetChildID() const
{
    return m_imp->GetChildID();
}

void Metadata::SetChildID(int childID)
{
    m_imp->SetChildID(childID);
}

bool Metadata::GetBrowse() const
{
    return m_imp->GetBrowse();
}

void Metadata::SetBrowse(bool browse)
{
    m_imp->SetBrowse(browse);
}

bool Metadata::GetWatched() const
{
    return m_imp->GetWatched();
}

void Metadata::SetWatched(bool watched)
{
    m_imp->SetWatched(watched);
}

const QString &Metadata::GetPlayCommand() const
{
    return m_imp->getPlayCommand();
}

void Metadata::SetPlayCommand(const QString &playCommand)
{
    m_imp->SetPlayCommand(playCommand);
}

ParentalLevel::Level Metadata::GetShowLevel() const
{
    return m_imp->GetShowLevel();
}

void Metadata::SetShowLevel(ParentalLevel::Level showLevel)
{
    m_imp->SetShowLevel(showLevel);
}

const QString &Metadata::GetFilename() const
{
    return m_imp->getFilename();
}

const QString &Metadata::GetHash() const
{
    return m_imp->GetHash();
}

void Metadata::SetHash(const QString &hash)
{
    return m_imp->SetHash(hash);
}

const QString &Metadata::GetHost() const
{
    return m_imp->GetHost();
}

void Metadata::SetHost(const QString &host)
{
        m_imp->SetHost(host);
}

void Metadata::SetFilename(const QString &filename)
{
    m_imp->SetFilename(filename);
}

const QString &Metadata::GetTrailer() const
{
    return m_imp->GetTrailer();
}

void Metadata::SetTrailer(const QString &trailer)
{
    m_imp->SetTrailer(trailer);
}

const QString &Metadata::GetCoverFile() const
{
    return m_imp->GetCoverFile();
}

void Metadata::SetCoverFile(const QString &coverFile)
{
    m_imp->SetCoverFile(coverFile);
}

const QString &Metadata::GetScreenshot() const
{
    return m_imp->GetScreenshot();
}

void Metadata::SetScreenshot(const QString &screenshot)
{
    m_imp->SetScreenshot(screenshot);
}

const QString &Metadata::GetBanner() const
{
    return m_imp->GetBanner();
}

void Metadata::SetBanner(const QString &banner)
{
    m_imp->SetBanner(banner);
}

const QString &Metadata::GetFanart() const
{
    return m_imp->GetFanart();
}

void Metadata::SetFanart(const QString &fanart)
{
    m_imp->SetFanart(fanart);
}

const QString &Metadata::GetCategory() const
{
    return m_imp->GetCategory();
}

//void Metadata::SetCategory(const QString &category)
//{
//    m_imp->SetCategory(category);
//}

const Metadata::genre_list &Metadata::GetGenres() const
{
    return m_imp->getGenres();
}

void Metadata::SetGenres(const genre_list &genres)
{
    m_imp->SetGenres(genres);
}

const Metadata::cast_list &Metadata::GetCast() const
{
    return m_imp->GetCast();
}

void Metadata::SetCast(const cast_list &cast)
{
    m_imp->SetCast(cast);
}

const Metadata::country_list &Metadata::GetCountries() const
{
    return m_imp->GetCountries();
}

void Metadata::SetCountries(const country_list &countries)
{
    m_imp->SetCountries(countries);
}

int Metadata::GetCategoryID() const
{
    return m_imp->GetCategoryID();
}

void Metadata::SetCategoryID(int id)
{
    m_imp->SetCategoryID(id);
}

void Metadata::SaveToDatabase()
{
    m_imp->SaveToDatabase();
}

void Metadata::UpdateDatabase()
{
    m_imp->UpdateDatabase();
}

bool Metadata::DeleteFromDatabase()
{
    return m_imp->DeleteFromDatabase();
}

#if 0
bool Metadata::fillDataFromID(const MetadataListManager &cache)
{
    if (m_imp->getID() == 0)
        return false;

    MetadataListManager::MetadataPtr mp = cache.byID(m_imp->getID());
    if (mp.get())
    {
        *this = *mp;
        return true;
    }

    return false;
}
#endif

bool Metadata::FillDataFromFilename(const MetadataListManager &cache)
{
    if (m_imp->getFilename().isEmpty())
        return false;

    MetadataListManager::MetadataPtr mp =
            cache.byFilename(m_imp->getFilename());
    if (mp)
    {
        *this = *mp;
        return true;
    }

    return false;
}

bool Metadata::DeleteFile(class VideoList &dummy)
{
    return m_imp->DeleteFile(dummy);
}

void Metadata::Reset()
{
    m_imp->Reset();
}

bool Metadata::IsHostSet() const
{
    return m_imp->IsHostSet();
}

bool operator==(const Metadata& a, const Metadata& b)
{
    if (a.GetFilename() == b.GetFilename())
        return true;
    return false;
}

bool operator!=(const Metadata& a, const Metadata& b)
{
    if (a.GetFilename() != b.GetFilename())
        return true;
    return false;
}

bool operator<(const Metadata::SortKey &lhs, const Metadata::SortKey &rhs)
{
    if (lhs.m_sd && rhs.m_sd)
        return *lhs.m_sd < *rhs.m_sd;
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Error: Bug, Metadata item with empty "
                                      "sort key compared"));
        return lhs.m_sd < rhs.m_sd;
    }
}
