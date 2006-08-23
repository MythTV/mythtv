#include <qfile.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qregexp.h>

#include <cmath>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "globals.h"
#include "metadata.h"
#include "metadatalistmanager.h"
#include "dbaccess.h"
#include "videoutils.h"

class MetadataImp
{
  public:
    typedef Metadata::genre_list genre_list;
    typedef Metadata::country_list country_list;

  public:
    MetadataImp(const QString &filename, const QString &coverfile,
             const QString &title, int year,
             const QString &inetref, const QString &director,
             const QString &plot, float userrating,
             const QString &rating, int length,
             int id, int showlevel, int categoryID,
             int childID, bool browse,
             const QString &playcommand, const QString &category,
             const genre_list &genres,
             const country_list &countries) :
        m_title(title),
        m_inetref(inetref), m_director(director), m_plot(plot),
        m_rating(rating), m_playcommand(playcommand), m_category(category),
        m_genres(genres), m_countries(countries),
        m_filename(filename), m_coverfile(coverfile),
        m_categoryID(categoryID), m_childID(childID), m_year(year),
        m_length(length), m_showlevel(showlevel), m_browse(browse), m_id(id),
        m_userrating(userrating), m_has_sort_key(false)
    {
        VideoCategory::getCategory().get(m_categoryID, m_category);
    }

    MetadataImp(MSqlQuery &query) : m_has_sort_key(false)
    {
        fromDBRow(query);
    }

    MetadataImp(const MetadataImp &other)
    {
        if (this != &other)
        {
            *this = other;
        }
    }

    MetadataImp &operator=(const MetadataImp &rhs)
    {
        m_title = rhs.m_title;
        m_inetref = rhs.m_inetref;
        m_director = rhs.m_director;
        m_plot = rhs.m_plot;
        m_rating = rhs.m_rating;
        m_playcommand = rhs.m_playcommand;
        m_category = rhs.m_category;
        m_genres = rhs.m_genres;
        m_countries = rhs.m_countries;
        m_filename = rhs.m_filename;
        m_coverfile = rhs.m_coverfile;

        m_categoryID = rhs.m_categoryID;
        m_childID = rhs.m_childID;
        m_year = rhs.m_year;
        m_length = rhs.m_length;
        m_showlevel = rhs.m_showlevel;
        m_browse = rhs.m_browse;
        m_id = rhs.m_id;
        m_userrating = rhs.m_userrating;

        m_has_sort_key = rhs.m_has_sort_key;
        m_sort_key = rhs.m_sort_key;
        m_prefix = rhs.m_prefix;

        return *this;
    }

  public:
    bool hasSortKey() const { return m_has_sort_key; }
    const QString &getSortKey() const { return m_sort_key; }
    void setSortKey(const QString &sort_key)
    {
        m_has_sort_key = true;
        m_sort_key = sort_key;
    }

    void setFlatIndex(int index) { m_flat_index = index; }
    int getFlatIndex() const { return m_flat_index; }

    const QString &getPrefix() const { return m_prefix; }
    void setPrefix(const QString &prefix) { m_prefix = prefix; }

    const QString &getTitle() const { return m_title; }
    void setTitle(const QString& title)
    {
        m_title = title;
        m_has_sort_key = false;
    }

    const QString &getInetRef() const { return m_inetref; }
    void setInetRef(const QString &inetRef) { m_inetref = inetRef; }

    const QString &getDirector() const { return m_director; }
    void setDirector(const QString &director) { m_director = director; }

    const QString &getPlot() const { return m_plot; }
    void setPlot(const QString &plot) { m_plot = plot; }

    const QString &getRating() const { return m_rating; }
    void setRating(const QString &rating) { m_rating = rating; }

    const QString &getPlayCommand() const { return m_playcommand; }
    void setPlayCommand(const QString &playCommand)
    {
        m_playcommand = playCommand;
    }

    const QString &getCategory() const { return m_category; }
//    void setCategory(const QString &category) { m_category = category; }

    const genre_list &getGenres() const { return m_genres; }
    void setGenres(const genre_list &genres) { m_genres = genres; }

    const country_list &getCountries() const { return m_countries; }
    void setCountries(const country_list &countries)
    {
        m_countries = countries;
    }

    const QString &getFilename() const { return m_filename; }
    void setFilename(const QString &filename) { m_filename = filename; }

    QString getFilenameNoPrefix() const
    {
        QString ret(m_filename);
        if (ret.startsWith(m_prefix + "/"))
        {
            ret.remove(0, m_prefix.length() + 1);
        }
        else if (ret.startsWith(m_prefix))
        {
            ret.remove(0, m_prefix.length());
        }

        return ret;
    }

    const QString &getCoverFile() const { return m_coverfile; }
    void setCoverFile(const QString &coverFile) { m_coverfile = coverFile; }

    int getCategoryID() const
    {
        return m_categoryID;
    }
    void setCategoryID(int id);

    int getChildID() const { return m_childID; }
    void setChildID(int childID) { m_childID = childID; }

    int getYear() const { return m_year; }
    void setYear(int year) { m_year = year; }

    int getLength() const { return m_length; }
    void setLength(int length) { m_length = length; }

    int getShowLevel() const { return m_showlevel; }
    void setShowLevel(int showLevel) { m_showlevel = showLevel; }

    bool getBrowse() const { return m_browse; }
    void setBrowse(bool browse) { m_browse = browse; }

    unsigned int getID() const { return m_id; }
    void setID(int id) { m_id = id; }

    float getUserRating() const { return m_userrating; }
    void setUserRating(float userRating) { m_userrating = userRating; }

    ////////////////////////////////

    void dumpToDatabase();
    void updateDatabase();

    bool deleteFile();
    bool dropFromDB();

  private:
    void fillCountries();
    void updateCountries();
    void fillGenres();
    void updateGenres();
    bool removeDir(const QString &dirName);
    void fromDBRow(MSqlQuery &query);
    void saveToDatabase();

  private:
    QString m_title;
    QString m_inetref;
    QString m_director;
    QString m_plot;
    QString m_rating;
    QString m_playcommand;
    QString m_category;
    genre_list m_genres;
    country_list m_countries;
    QString m_filename;
    QString m_coverfile;

    int m_categoryID;
    int m_childID;
    int m_year;
    int m_length;
    int m_showlevel;
    bool m_browse;
    unsigned int m_id;  // videometadata.intid
    float m_userrating;

    // not in DB
    bool m_has_sort_key;
    QString m_sort_key;
    QString m_prefix;
    int m_flat_index;
};

/////////////////////////////
/////////
/////////////////////////////
bool MetadataImp::removeDir(const QString &dirName)
{
    QDir d(dirName);

    const QFileInfoList *contents = d.entryInfoList();
    if (!contents)
    {
        return d.rmdir(dirName);
    }

    const QFileInfoListIterator it(*contents);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        if (fi->fileName() == "." ||
            fi->fileName() == "..")
        {
            continue;
        }
        if (fi->isDir())
        {
            QString fileName = fi->fileName();
            if (!removeDir(fileName))
                return false;
        }
        else
        {
            if (!QFile(fi->fileName()).remove())
                return false;
        }
    }
    return d.rmdir(dirName);
}

/// Deletes the file associated with a metadata entry
bool MetadataImp::deleteFile()
{
    bool isremoved = false;
    QFileInfo fi(m_filename);
    if (fi.isDir())
    {
        isremoved = removeDir(m_filename);
    }
    else
    {
        QFile videofile;
        videofile.setName(m_filename);
        isremoved = videofile.remove();
    }

    if (!isremoved)
    {
        VERBOSE(VB_IMPORTANT, QString("impossible de supprimer le fichier"));
    }

    return isremoved;
}

bool MetadataImp::dropFromDB()
{
    VideoGenreMap::getGenreMap().remove(m_id);
    VideoCountryMap::getCountryMap().remove(m_id);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM videometadata WHERE intid = :ID");
    query.bindValue(":ID", m_id);
    if (!query.exec())
    {
        MythContext::DBError("delete from videometadata", query);
    }

    query.prepare("DELETE FROM filemarkup WHERE filename = :FILENAME");
    query.bindValue(":FILENAME", m_filename.utf8());
    if (!query.exec())
    {
        MythContext::DBError("delete from filemarkup", query);
    }

    return true;
}

void MetadataImp::fillGenres()
{
    m_genres.clear();
    VideoGenreMap &vgm = VideoGenreMap::getGenreMap();
    VideoGenreMap::entry genres;
    if (vgm.get(m_id, genres))
    {
        VideoGenre &vg = VideoGenre::getGenre();
        for (VideoGenreMap::entry::values_type::iterator p =
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
        for (VideoCountryMap::entry::values_type::iterator p =
             countries.values.begin(); p != countries.values.end(); ++p)
        {
            // Just add empty string for no-name countries
            QString name;
            vc.get(*p, name);
            m_countries.push_back(country_list::value_type(*p, name));
        }
    }
}

/// Sets metadata from a DB row
void MetadataImp::fromDBRow(MSqlQuery &query)
{
    m_title = QString::fromUtf8(query.value(0).toString());
    m_director = QString::fromUtf8(query.value(1).toString());
    m_plot = QString::fromUtf8(query.value(2).toString());
    m_rating = query.value(3).toString();
    m_year = query.value(4).toInt();
    m_userrating = (float)query.value(5).toDouble();
    if (isnan(m_userrating))
        m_userrating = 0.0;
    if (m_userrating < -10.0 || m_userrating >= 10.0)
        m_userrating = 0.0;
    m_length = query.value(6).toInt();
    m_filename = QString::fromUtf8(query.value(7).toString());
    m_showlevel = query.value(8).toInt();
    m_coverfile = QString::fromUtf8(query.value(9).toString());
    m_inetref = QString::fromUtf8(query.value(10).toString());
    m_childID = query.value(11).toUInt();
    m_browse = query.value(12).toBool();
    m_playcommand = query.value(13).toString();
    m_categoryID = query.value(14).toInt();
    m_id = query.value(15).toInt();

    VideoCategory::getCategory().get(m_categoryID, m_category);

    // Genres
    fillGenres();

    //Countries
    fillCountries();
}

void MetadataImp::saveToDatabase()
{
    if (m_title == "")
        m_title = Metadata::FilenameToTitle(m_filename);
    if (m_director == "")
        m_director = VIDEO_DIRECTOR_UNKNOWN;
    if (m_plot == "")
        m_plot = VIDEO_PLOT_DEFAULT;
    if (m_rating == "")
        m_rating = VIDEO_RATING_DEFAULT;
    if (m_coverfile == "")
        m_coverfile = VIDEO_COVERFILE_DEFAULT;
    if (m_inetref == "")
        m_inetref = VIDEO_INETREF_DEFAULT;
    if (isnan(m_userrating))
        m_userrating = 0.0;
    if (m_userrating < -10.0 || m_userrating >= 10.0)
        m_userrating = 0.0;

    bool inserting = m_id == 0;

    MSqlQuery query(MSqlQuery::InitCon());

    if (inserting)
    {
        m_browse = gContext->GetNumSetting("VideoNewBrowsable", 1);

        query.prepare("INSERT INTO videometadata (title,director,plot,"
                      "rating,year,userrating,length,filename,showlevel,"
                      "coverfile,inetref,browse) VALUES (:TITLE, :DIRECTOR, "
                      ":PLOT, :RATING, :YEAR, :USERRATING, :LENGTH, "
                      ":FILENAME, :SHOWLEVEL, :COVERFILE, :INETREF, :BROWSE)");

    }
    else
    {
        query.prepare("UPDATE videometadata SET title = :TITLE, "
                      "director = :DIRECTOR, plot = :PLOT, rating= :RATING, "
                      "year = :YEAR, userrating = :USERRATING, "
                      "length = :LENGTH, filename = :FILENAME, "
                      "showlevel = :SHOWLEVEL, coverfile = :COVERFILE, "
                      "inetref = :INETREF, browse = :BROWSE, "
                      "playcommand = :PLAYCOMMAND, childid = :CHILDID, "
                      "category = :CATEGORY WHERE intid = :INTID");

        query.bindValue(":PLAYCOMMAND", m_playcommand.utf8());
        query.bindValue(":CHILDID", m_childID);
        query.bindValue(":CATEGORY", m_categoryID);
        query.bindValue(":INTID", m_id);
    }

    query.bindValue(":TITLE", m_title.utf8());
    query.bindValue(":DIRECTOR", m_director.utf8());
    query.bindValue(":PLOT", m_plot.utf8());
    query.bindValue(":RATING", m_rating.utf8());
    query.bindValue(":YEAR", m_year);
    query.bindValue(":USERRATING", m_userrating);
    query.bindValue(":LENGTH", m_length);
    query.bindValue(":FILENAME", m_filename.utf8());
    query.bindValue(":SHOWLEVEL", m_showlevel);
    query.bindValue(":COVERFILE", m_coverfile.utf8());
    query.bindValue(":INETREF", m_inetref.utf8());
    query.bindValue(":BROWSE", m_browse);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("video metadata update", query);
        return;
    }

    if (inserting)
    {
        // Must make sure we have 'id' filled before we call updateGenres or
        // updateCountries
        query.exec("SELECT LAST_INSERT_ID()");

        if (!query.isActive() || query.size() < 1)
        {
            MythContext::DBError("metadata id get", query);
            return;
        }

        query.next();
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
}

void MetadataImp::dumpToDatabase()
{
    saveToDatabase();
}

void MetadataImp::updateDatabase()
{
    saveToDatabase();
}

void MetadataImp::setCategoryID(int id)
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
            if (VideoCategory::getCategory().get(id, cat))
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
        if (genre->second.stripWhiteSpace().length())
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
        if (country->second.stripWhiteSpace().length())
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

////////////////////////////////////////
//// Metadata
////////////////////////////////////////
QString Metadata::GenerateDefaultSortKey(const Metadata &m, bool ignore_case)
{
    QString title(ignore_case ? m.Title().lower() : m.Title());
    title = trimTitle(title, ignore_case);
    QString ret(title + m.Filename() + QString().sprintf("%.7d", m.ID()));

    return ret;
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
            int left_position = ret.find(left_brace);
            int right_position = ret.find(right_brace);
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

QString Metadata::FilenameToTitle(const QString &file_name)
{
    QString title = file_name.right(file_name.length() -
                                    file_name.findRev("/") - 1);
    title.replace(QRegExp("_"), " ");
    title.replace(QRegExp("%20"), " ");
    title = title.left(title.findRev("."));
    title.replace(QRegExp("\\."), " ");

    title = eatBraces(title, "[", "]");
    title = eatBraces(title, "(", ")");
    title = eatBraces(title, "{", "}");

    return title.stripWhiteSpace();
}

namespace
{
    const QRegExp &getTitleTrim(bool ignore_case)
    {
        static QString pattern(QObject::tr("^(The |A |An )"));
        static QRegExp prefixes_case(pattern, true);
        static QRegExp prefixes_nocase(pattern, false);
        return ignore_case ? prefixes_nocase : prefixes_case;
    }
}

QString Metadata::trimTitle(const QString &title, bool ignore_case)
{
    QString ret(title);
    ret.remove(getTitleTrim(ignore_case));
    return ret;
}

QString Metadata::getPlayer(const Metadata *item)
{
    QString empty;
    return getPlayer(item, empty);
}

QString Metadata::getPlayer(const Metadata *item, QString &internal_mrl)
{
    if (!item) return "";

    internal_mrl = item->Filename();

    if (item->PlayCommand().length()) return item->PlayCommand();

    QString extension = item->Filename().section(".", -1, -1);

    QDir dir_test(item->Filename());
    if (dir_test.exists())
    {
        dir_test.setPath(item->Filename() + "/VIDEO_TS");
        if (dir_test.exists())
        {
            extension = "VIDEO_TS";
        }
    }

    QString type_player;
    bool use_default = true;
    if (getPlayer(extension, type_player, use_default))
    {
        if (!use_default) return type_player;
    }

    return gContext->GetSetting("VideoDefaultPlayer");
}

QString Metadata::getPlayCommand(const Metadata *item)
{
    if (!item) return "";

    QString filename = item->Filename();
    QString handler = getPlayer(item);

    QString esc_fname =
            QString(item->Filename()).replace(QRegExp("\""), "\\\"");
    QString arg = QString("\"%1\"").arg(esc_fname);

    QString command = "";

    // If handler contains %d, substitute default player command
    // This would be used to add additional switches to the default without
    // needing to retype the whole default command.  But, if the
    // command and the default command both contain %s, drop the %s from
    // the default since the new command already has it
    //
    // example: default: mplayer -fs %s
    //          custom : %d -ao alsa9:spdif %s
    //          result : mplayer -fs -ao alsa9:spdif %s
    if (handler.contains("%d"))
    {
        QString default_handler = gContext->GetSetting("VideoDefaultPlayer");
        if (handler.contains("%s") && default_handler.contains("%s"))
        {
            default_handler = default_handler.replace(QRegExp("%s"), "");
        }
        command = handler.replace(QRegExp("%d"), default_handler);
    }

    if (handler.contains("%s"))
    {
        command = handler.replace(QRegExp("%s"), arg);
    }
    else
    {
        command = handler + " " + arg;
    }

    return command;
}

// returns true if info for the extension was found
bool Metadata::getPlayer(const QString &extension, QString &player,
                         bool &use_default)
{
    use_default = true;

    const FileAssociations::association_list fa_list =
            FileAssociations::getFileAssociation().getList();
    for (FileAssociations::association_list::const_iterator p = fa_list.begin();
         p != fa_list.end(); ++p)
    {
        if (p->extension.lower() == extension.lower())
        {
            player = p->playcommand;
            use_default = p->use_default;
            return true;
        }
    }

    return false;
}

Metadata::Metadata(const QString &filename, const QString &coverfile,
             const QString &title, int year,
             const QString &inetref, const QString &director,
             const QString &plot, float userrating,
             const QString &rating, int length,
             int id, int showlevel, int categoryID,
             int childID, bool browse,
             const QString &playcommand, const QString &category,
             const genre_list &genres,
             const country_list &countries)
{
    m_imp = new MetadataImp(filename, coverfile, title, year, inetref, director,
                            plot, userrating, rating, length, id, showlevel,
                            categoryID, childID, browse, playcommand, category,
                            genres, countries);
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

bool Metadata::hasSortKey() const
{
    return m_imp->hasSortKey();
}

const QString &Metadata::getSortKey() const
{
    return m_imp->getSortKey();
}

void Metadata::setSortKey(const QString &sort_key)
{
    m_imp->setSortKey(sort_key);
}

void Metadata::setFlatIndex(int index)
{
    m_imp->setFlatIndex(index);
}

int Metadata::getFlatIndex() const
{
    return m_imp->getFlatIndex();
}


const QString &Metadata::getPrefix() const
{
    return m_imp->getPrefix();
}

void Metadata::setPrefix(const QString &prefix)
{
    m_imp->setPrefix(prefix);
}

const QString &Metadata::Title() const
{
    return m_imp->getTitle();
}

void Metadata::setTitle(const QString &title)
{
    m_imp->setTitle(title);
}

int Metadata::Year() const
{
    return m_imp->getYear();
}

void Metadata::setYear(int year)
{
    m_imp->setYear(year);
}

const QString &Metadata::InetRef() const
{
    return m_imp->getInetRef();
}

void Metadata::setInetRef(const QString &inetRef)
{
    m_imp->setInetRef(inetRef);
}

const QString &Metadata::Director() const
{
    return m_imp->getDirector();
}

void Metadata::setDirector(const QString &director)
{
    m_imp->setDirector(director);
}

const QString &Metadata::Plot() const
{
    return m_imp->getPlot();
}

void Metadata::setPlot(const QString &plot)
{
    m_imp->setPlot(plot);
}

float Metadata::UserRating() const
{
    return m_imp->getUserRating();
}

void Metadata::setUserRating(float userRating)
{
    m_imp->setUserRating(userRating);
}

const QString &Metadata::Rating() const
{
    return m_imp->getRating();
}

void Metadata::setRating(const QString &rating)
{
    m_imp->setRating(rating);
}

int Metadata::Length() const
{
    return m_imp->getLength();
}

void Metadata::setLength(int length)
{
    m_imp->setLength(length);
}

unsigned int Metadata::ID() const
{
    return m_imp->getID();
}

void Metadata::setID(int id)
{
    m_imp->setID(id);
}

int Metadata::ChildID() const
{
    return m_imp->getChildID();
}

void Metadata::setChildID(int childID)
{
    m_imp->setChildID(childID);
}

bool Metadata::Browse() const
{
    return m_imp->getBrowse();
}

void Metadata::setBrowse(bool browse)
{
    m_imp->setBrowse(browse);
}

const QString &Metadata::PlayCommand() const
{
    return m_imp->getPlayCommand();
}

void Metadata::setPlayCommand(const QString &playCommand)
{
    m_imp->setPlayCommand(playCommand);
}

int Metadata::ShowLevel() const
{
    return m_imp->getShowLevel();
}

void Metadata::setShowLevel(int showLevel)
{
    m_imp->setShowLevel(showLevel);
}

const QString &Metadata::Filename() const
{
    return m_imp->getFilename();
}

void Metadata::setFilename(const QString &filename)
{
    m_imp->setFilename(filename);
}

QString Metadata::getFilenameNoPrefix() const
{
    return m_imp->getFilenameNoPrefix();
}

const QString &Metadata::CoverFile() const
{
    return m_imp->getCoverFile();
}

void Metadata::setCoverFile(const QString &coverFile)
{
    m_imp->setCoverFile(coverFile);
}

const QString &Metadata::Category() const
{
    return m_imp->getCategory();
}

//void Metadata::setCategory(const QString &category)
//{
//    m_imp->setCategory(category);
//}

const Metadata::genre_list &Metadata::Genres() const
{
    return m_imp->getGenres();
}

void Metadata::setGenres(const genre_list &genres)
{
    m_imp->setGenres(genres);
}

const Metadata::country_list &Metadata::Countries() const
{
    return m_imp->getCountries();
}

void Metadata::setCountries(const country_list &countries)
{
    m_imp->setCountries(countries);
}

int Metadata::getCategoryID() const
{
    return m_imp->getCategoryID();
}

void Metadata::setCategoryID(int id)
{
    m_imp->setCategoryID(id);
}

void Metadata::dumpToDatabase()
{
    m_imp->dumpToDatabase();
}

void Metadata::updateDatabase()
{
    m_imp->updateDatabase();
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

bool Metadata::fillDataFromFilename(const MetadataListManager &cache)
{
    if (m_imp->getFilename() == "")
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

bool Metadata::deleteFile()
{
    return m_imp->deleteFile();
}

bool Metadata::dropFromDB()
{
    return m_imp->dropFromDB();
}

bool operator==(const Metadata& a, const Metadata& b)
{
    if (a.Filename() == b.Filename())
        return true;
    return false;
}

bool operator!=(const Metadata& a, const Metadata& b)
{
    if (a.Filename() != b.Filename())
        return true;
    return false;
}
