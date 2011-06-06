// qt
#include <QApplication>
#include <QRegExp>
#include <QDateTime>
#include <QDir>

// mythtv
#include <mythcontext.h>
#include <mythwidgets.h>
#include <mythdb.h>
#include <mythdirs.h>
#include <mythprogressdialog.h>
#include <mythdownloadmanager.h>

// mythmusic
#include "metadata.h"
#include "metaio.h"
#include "metaioid3.h"
#include "metaiomp4.h"
#include "metaioavfcomment.h"
#include "metaiooggvorbis.h"
#include "metaioflacvorbis.h"
#include "metaiowavpack.h"
#include "treebuilders.h"
#include "playlist.h"
#include "playlistcontainer.h"
#include "mythlogging.h"


// this is the global MusicData object shared thoughout MythMusic
MusicData  *gMusicData = NULL;

static QString thePrefix = "the ";

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

static bool meta_less_than(const Metadata *item1, const Metadata *item2)
{
    return item1->compare(item2) < 0;
}

static bool music_less_than(const MusicNode *itemA, const MusicNode *itemB)
{
    QString title1 = itemA->getTitle().toLower();
    QString title2 = itemB->getTitle().toLower();

    // Cut "the " off the front of titles
    if (title1.left(4) == thePrefix)
        title1 = title1.mid(4);
    if (title2.left(4) == thePrefix)
        title2 = title2.mid(4);

    return title1.localeAwareCompare(title2) < 0;
}

/**************************************************************************/

Metadata::~Metadata(void)
{
    if (m_albumArt)
    {
        delete m_albumArt;
        m_albumArt = NULL;
    }
 }

Metadata& Metadata::operator=(const Metadata &rhs)
{
    m_artist = rhs.m_artist;
    m_compilation_artist = rhs.m_compilation_artist;
    m_album = rhs.m_album;
    m_title = rhs.m_title;
    m_formattedartist = rhs.m_formattedartist;
    m_formattedtitle = rhs.m_formattedtitle;
    m_genre = rhs.m_genre;
    m_year = rhs.m_year;
    m_tracknum = rhs.m_tracknum;
    m_length = rhs.m_length;
    m_rating = rhs.m_rating;
    m_lastplay = rhs.m_lastplay;
    m_dateadded = rhs.m_dateadded;
    m_playcount = rhs.m_playcount;
    m_compilation = rhs.m_compilation;
    m_id = rhs.m_id;
    m_filename = rhs.m_filename;
    m_directoryid = rhs.m_directoryid;
    m_artistid = rhs.m_artistid;
    m_compartistid = rhs.m_compartistid;
    m_albumid = rhs.m_albumid;
    m_genreid = rhs.m_genreid;
    m_albumArt = NULL;
    m_format = rhs.m_format;

    m_changed = rhs.m_changed;

    return *this;
}

QString Metadata::m_startdir;

void Metadata::SetStartdir(const QString &dir)
{
    Metadata::m_startdir = dir;
}

void Metadata::persist() const
{
    if (m_id < 1)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE music_songs set rating = :RATING , "
                  "numplays = :PLAYCOUNT , lastplay = :LASTPLAY "
                  "where song_id = :ID ;");
    query.bindValue(":RATING", m_rating);
    query.bindValue(":PLAYCOUNT", m_playcount);
    query.bindValue(":LASTPLAY", m_lastplay);
    query.bindValue(":ID", m_id);

    if (!query.exec())
        MythDB::DBError("music persist", query);
}


void Metadata::UpdateModTime() const
{
    if (m_id < 1)
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE music_songs SET date_modified = :DATE_MOD "
                  "WHERE song_id= :ID ;");

    query.bindValue(":DATE_MOD", QDateTime::currentDateTime());
    query.bindValue(":ID", m_id);

    if (!query.exec())
        MythDB::DBError("Metadata::UpdateModTime",
                        query);
}

int Metadata::compare(const Metadata *other) const
{
    if (m_format == "cast")
    {
        int artist_cmp = Artist().toLower().localeAwareCompare(
            other->Artist().toLower());

        if (artist_cmp == 0)
            return Title().toLower().localeAwareCompare(
                other->Title().toLower());

        return artist_cmp;
    }
    else
    {
        int track_cmp = Track() - other->Track();

        if (track_cmp == 0)
            return Title().toLower().localeAwareCompare(
                other->Title().toLower());

        return track_cmp;
    }
}

bool Metadata::isInDatabase()
{
    bool retval = false;

    QString sqlfilepath(m_filename);
    if (!sqlfilepath.contains("://"))
    {
        sqlfilepath.remove(0, m_startdir.length());
    }
    QString sqldir = sqlfilepath.section( '/', 0, -2);
    QString sqlfilename =sqlfilepath.section( '/', -1 ) ;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT music_artists.artist_name, "
    "music_comp_artists.artist_name AS compilation_artist, "
    "music_albums.album_name, music_songs.name, music_genres.genre, "
    "music_songs.year, music_songs.track, music_songs.length, "
    "music_songs.song_id, music_songs.rating, music_songs.numplays, "
    "music_songs.lastplay, music_albums.compilation, music_songs.format "
    "FROM music_songs "
    "LEFT JOIN music_directories "
    "ON music_songs.directory_id=music_directories.directory_id "
    "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
    "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
    "LEFT JOIN music_artists AS music_comp_artists "
    "ON music_albums.artist_id=music_comp_artists.artist_id "
    "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
    "WHERE music_songs.filename = :FILENAME "
    "AND music_directories.path = :DIRECTORY ;");
    query.bindValue(":FILENAME", sqlfilename);
    query.bindValue(":DIRECTORY", sqldir);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        m_artist = query.value(0).toString();
        m_compilation_artist = query.value(1).toString();
        m_album = query.value(2).toString();
        m_title = query.value(3).toString();
        m_genre = query.value(4).toString();
        m_year = query.value(5).toInt();
        m_tracknum = query.value(6).toInt();
        m_length = query.value(7).toInt();
        m_id = query.value(8).toUInt();
        m_rating = query.value(9).toInt();
        m_playcount = query.value(10).toInt();
        m_lastplay = query.value(11).toDateTime();
        m_compilation = (query.value(12).toInt() > 0);
        m_format = query.value(13).toString();

        retval = true;
    }

    return retval;
}

void Metadata::dumpToDatabase()
{
    QString sqlfilepath(m_filename);
    if (!sqlfilepath.contains("://"))
    {
        sqlfilepath.remove(0, m_startdir.length());
    }
    QString sqldir = sqlfilepath.section( '/', 0, -2);
    QString sqlfilename = sqlfilepath.section( '/', -1 ) ;

    checkEmptyFields();

    MSqlQuery query(MSqlQuery::InitCon());

    if (sqldir.isEmpty())
    {
        m_directoryid = 0;
    }
    else if (m_directoryid < 0)
    {
        // Load the directory id
        query.prepare("SELECT directory_id FROM music_directories "
                    "WHERE path = :DIRECTORY ;");
        query.bindValue(":DIRECTORY", sqldir);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select directory id", query);
            return;
        }
        if (query.next())
        {
            m_directoryid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_directories (path) VALUES (:DIRECTORY);");
            query.bindValue(":DIRECTORY", sqldir);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert directory", query);
                return;
            }
            m_directoryid = query.lastInsertId().toInt();
        }
    }

    if (m_artistid < 0)
    {
        // Load the artist id
        query.prepare("SELECT artist_id FROM music_artists "
                    "WHERE artist_name = :ARTIST ;");
        query.bindValue(":ARTIST", m_artist);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select artist id", query);
            return;
        }
        if (query.next())
        {
            m_artistid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_artists (artist_name) VALUES (:ARTIST);");
            query.bindValue(":ARTIST", m_artist);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert artist", query);
                return;
            }
            m_artistid = query.lastInsertId().toInt();
        }
    }

    // Compilation Artist
    if (m_artist == m_compilation_artist)
    {
        m_compartistid = m_artistid;
    }
    else
    {
        query.prepare("SELECT artist_id FROM music_artists "
                    "WHERE artist_name = :ARTIST ;");
        query.bindValue(":ARTIST", m_compilation_artist);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select compilation artist id", query);
            return;
        }
        if (query.next())
        {
            m_compartistid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_artists (artist_name) VALUES (:ARTIST);");
            query.bindValue(":ARTIST", m_compilation_artist);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert compilation artist", query);
                return;
            }
            m_compartistid = query.lastInsertId().toInt();
        }
    }

    // Album
    if (m_albumid < 0)
    {
        query.prepare("SELECT album_id FROM music_albums "
                    "WHERE artist_id = :COMP_ARTIST_ID "
                    " AND album_name = :ALBUM ;");
        query.bindValue(":COMP_ARTIST_ID", m_compartistid);
        query.bindValue(":ALBUM", m_album);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select album id", query);
            return;
        }
        if (query.next())
        {
            m_albumid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_albums (artist_id, album_name, compilation, year) VALUES (:COMP_ARTIST_ID, :ALBUM, :COMPILATION, :YEAR);");
            query.bindValue(":COMP_ARTIST_ID", m_compartistid);
            query.bindValue(":ALBUM", m_album);
            query.bindValue(":COMPILATION", m_compilation);
            query.bindValue(":YEAR", m_year);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert album", query);
                return;
            }
            m_albumid = query.lastInsertId().toInt();
        }
    }

    if (m_genreid < 0)
    {
        // Genres
        query.prepare("SELECT genre_id FROM music_genres "
                    "WHERE genre = :GENRE ;");
        query.bindValue(":GENRE", m_genre);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select genre id", query);
            return;
        }
        if (query.next())
        {
            m_genreid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_genres (genre) VALUES (:GENRE);");
            query.bindValue(":GENRE", m_genre);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert genre", query);
                return;
            }
            m_genreid = query.lastInsertId().toInt();
        }
    }

    // We have all the id's now. We can insert it.
    QString strQuery;
    if (m_id < 1)
    {
        strQuery = "INSERT INTO music_songs ( directory_id,"
                   " artist_id, album_id,  name,         genre_id,"
                   " year,      track,     length,       filename,"
                   " rating,    format,    date_entered, date_modified,"
                   " numplays) "
                   "VALUES ( "
                   " :DIRECTORY, "
                   " :ARTIST,   :ALBUM,    :TITLE,       :GENRE,"
                   " :YEAR,     :TRACKNUM, :LENGTH,      :FILENAME,"
                   " :RATING,   :FORMAT,   :DATE_ADD,    :DATE_MOD,"
                   " :PLAYCOUNT );";
    }
    else
    {
        strQuery = "UPDATE music_songs SET"
                   " directory_id = :DIRECTORY"
                   ", artist_id = :ARTIST"
                   ", album_id = :ALBUM"
                   ", name = :TITLE"
                   ", genre_id = :GENRE"
                   ", year = :YEAR"
                   ", track = :TRACKNUM"
                   ", length = :LENGTH"
                   ", filename = :FILENAME"
                   ", rating = :RATING"
                   ", format = :FORMAT"
                   ", date_modified = :DATE_MOD "
                   ", numplays = :PLAYCOUNT "
                   "WHERE song_id= :ID ;";
    }

    query.prepare(strQuery);

    query.bindValue(":DIRECTORY", m_directoryid);
    query.bindValue(":ARTIST", m_artistid);
    query.bindValue(":ALBUM", m_albumid);
    query.bindValue(":TITLE", m_title);
    query.bindValue(":GENRE", m_genreid);
    query.bindValue(":YEAR", m_year);
    query.bindValue(":TRACKNUM", m_tracknum);
    query.bindValue(":LENGTH", m_length);
    query.bindValue(":FILENAME", sqlfilename);
    query.bindValue(":RATING", m_rating);
    query.bindValue(":FORMAT", m_format);
    query.bindValue(":DATE_MOD", QDateTime::currentDateTime());
    query.bindValue(":PLAYCOUNT", m_playcount);

    if (m_id < 1)
        query.bindValue(":DATE_ADD",  QDateTime::currentDateTime());
    else
        query.bindValue(":ID", m_id);

    if (!query.exec())
        MythDB::DBError("Metadata::dumpToDatabase - updating music_songs",
                        query);

    if (m_id < 1 && query.isActive() && 1 == query.numRowsAffected())
        m_id = query.lastInsertId().toInt();

    // save the albumart to the db
    if (m_albumArt)
        m_albumArt->dumpToDatabase();

    // make sure the compilation flag is updated
    query.prepare("UPDATE music_albums SET compilation = :COMPILATION, year = :YEAR "
                  "WHERE music_albums.album_id = :ALBUMID");
    query.bindValue(":ALBUMID", m_albumid);
    query.bindValue(":COMPILATION", m_compilation);
    query.bindValue(":YEAR", m_year);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("music compilation update", query);
        return;
    }
}

// Default values for formats
// NB These will eventually be customizable....
QString Metadata::m_formatnormalfileartist      = "ARTIST";
QString Metadata::m_formatnormalfiletrack       = "TITLE";
QString Metadata::m_formatnormalcdartist        = "ARTIST";
QString Metadata::m_formatnormalcdtrack         = "TITLE";
QString Metadata::m_formatcompilationfileartist = "COMPARTIST";
QString Metadata::m_formatcompilationfiletrack  = "TITLE (ARTIST)";
QString Metadata::m_formatcompilationcdartist   = "COMPARTIST";
QString Metadata::m_formatcompilationcdtrack    = "TITLE (ARTIST)";

void Metadata::setArtistAndTrackFormats()
{
    QString tmp;

    tmp = gCoreContext->GetSetting("MusicFormatNormalFileArtist");
    if (!tmp.isEmpty())
        m_formatnormalfileartist = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatNormalFileTrack");
    if (!tmp.isEmpty())
        m_formatnormalfiletrack = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatNormalCDArtist");
    if (!tmp.isEmpty())
        m_formatnormalcdartist = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatNormalCDTrack");
    if (!tmp.isEmpty())
        m_formatnormalcdtrack = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatCompilationFileArtist");
    if (!tmp.isEmpty())
        m_formatcompilationfileartist = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatCompilationFileTrack");
    if (!tmp.isEmpty())
        m_formatcompilationfiletrack = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatCompilationCDArtist");
    if (!tmp.isEmpty())
        m_formatcompilationcdartist = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatCompilationCDTrack");
    if (!tmp.isEmpty())
        m_formatcompilationcdtrack = tmp;
}


bool Metadata::determineIfCompilation(bool cd)
{
    m_compilation = (!m_compilation_artist.isEmpty()
                   && m_artist != m_compilation_artist);
    setCompilationFormatting(cd);
    return m_compilation;
}


inline QString Metadata::formatReplaceSymbols(const QString &format)
{
  QString rv = format;
  rv.replace("COMPARTIST", m_compilation_artist);
  rv.replace("ARTIST", m_artist);
  rv.replace("TITLE", m_title);
  rv.replace("TRACK", QString("%1").arg(m_tracknum, 2));
  return rv;
}

void Metadata::checkEmptyFields()
{
    if (m_artist.isEmpty())
        m_artist = QObject::tr("Unknown Artist");
    // This should be the same as Artist if it's a compilation track or blank
    if (!m_compilation || m_compilation_artist.isEmpty())
        m_compilation_artist = m_artist;
    if (m_album.isEmpty())
        m_album = QObject::tr("Unknown Album");
    if (m_title.isEmpty())
        m_title = m_filename;
    if (m_genre.isEmpty())
        m_genre = QObject::tr("Unknown Genre");

}

inline void Metadata::setCompilationFormatting(bool cd)
{
    QString format_artist, format_title;

    if (!m_compilation
        || "" == m_compilation_artist
        || m_artist == m_compilation_artist)
    {
        if (!cd)
        {
          format_artist = m_formatnormalfileartist;
          format_title  = m_formatnormalfiletrack;
        }
        else
        {
          format_artist = m_formatnormalcdartist;
          format_title  = m_formatnormalcdtrack;
        }
    }
    else
    {
        if (!cd)
        {
          format_artist = m_formatcompilationfileartist;
          format_title  = m_formatcompilationfiletrack;
        }
        else
        {
          format_artist = m_formatcompilationcdartist;
          format_title  = m_formatcompilationcdtrack;
        }
    }

    // NB Could do some comparisons here to save memory with shallow copies...
    m_formattedartist = formatReplaceSymbols(format_artist);
    m_formattedtitle = formatReplaceSymbols(format_title);
}


QString Metadata::FormatArtist()
{
    if (m_formattedartist.isEmpty())
        setCompilationFormatting();

    return m_formattedartist;
}


QString Metadata::FormatTitle()
{
    if (m_formattedtitle.isEmpty())
        setCompilationFormatting();

    return m_formattedtitle;
}


void Metadata::setField(const QString &field, const QString &data)
{
    if (field == "artist")
        m_artist = data;
    else if (field == "compilation_artist")
      m_compilation_artist = data;
    else if (field == "album")
        m_album = data;
    else if (field == "title")
        m_title = data;
    else if (field == "genre")
        m_genre = data;
    else if (field == "filename")
        m_filename = data;
    else if (field == "year")
        m_year = data.toInt();
    else if (field == "tracknum")
        m_tracknum = data.toInt();
    else if (field == "length")
        m_length = data.toInt();
    else if (field == "compilation")
        m_compilation = (data.toInt() > 0);

    else
    {
        VERBOSE(VB_IMPORTANT, QString("Something asked me to set data "
                              "for a field called %1").arg(field));
    }
}

void Metadata::getField(const QString &field, QString *data)
{
    if (field == "artist")
        *data = FormatArtist();
    else if (field == "album")
        *data = m_album;
    else if (field == "title")
        *data = FormatTitle();
    else if (field == "genre")
        *data = m_genre;
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Something asked me to return data "
                              "about a field called %1").arg(field));
        *data = "I Dunno";
    }
}

void Metadata::toMap(MetadataMap &metadataMap)
{
    metadataMap["artist"] = m_artist;
    metadataMap["formatartist"] = FormatArtist();
    metadataMap["compilationartist"] = m_compilation_artist;
    metadataMap["album"] = m_album;
    metadataMap["title"] = m_title;
    metadataMap["tracknum"] = (m_tracknum > 0 ? QString("%1").arg(m_tracknum) : "");
    metadataMap["genre"] = m_genre;
    metadataMap["year"] = (m_year > 0 ? QString("%1").arg(m_year) : "");
    metadataMap["artisttitle"] = QObject::tr("%1  by  %2",
                                             "Music track 'title by artist'")
                                             .arg(FormatTitle())
                                             .arg(FormatArtist());
    int len = m_length / 1000;
    int eh = len / 3600;
    int em = (len / 60) % 60;
    int es = len % 60;
    if (eh > 0)
        metadataMap["length"] = QString().sprintf("%d:%02d:%02d", eh, em, es);
    else
        metadataMap["length"] = QString().sprintf("%02d:%02d", em, es);

    QString dateFormat = gCoreContext->GetSetting("DateFormat", "ddd MMMM d");
    QString fullDateFormat = dateFormat;
    if (!fullDateFormat.contains("yyyy"))
        fullDateFormat += " yyyy";
    metadataMap["lastplayed"] = m_lastplay.toString(fullDateFormat);

    metadataMap["playcount"] = QString("%1").arg(m_playcount);
    metadataMap["filename"] = m_filename;
}

void Metadata::decRating()
{
    if (m_rating > 0)
    {
        m_rating--;
    }
    m_changed = true;
}

void Metadata::incRating()
{
    if (m_rating < 10)
    {
        m_rating++;
    }
    m_changed = true;
}

QDateTime Metadata::LastPlay()
{
    return m_lastplay;
}

void Metadata::setLastPlay()
{
    m_lastplay = QDateTime::currentDateTime();
    m_changed = true;
}

void Metadata::incPlayCount()
{
    m_playcount++;
    m_changed = true;
}

void Metadata::setEmbeddedAlbumArt(AlbumArtList &albumart)
{
    // add the images found in the tag to the ones we got from the DB

    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this);

    for (int x = 0; x < albumart.size(); x++)
    {
        m_albumArt->addImage(albumart.at(x));
    }

    m_changed = true;
}

QStringList Metadata::fillFieldList(QString field)
{
    QStringList searchList;
    searchList.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    if ("artist" == field)
    {
        query.prepare("SELECT artist_name FROM music_artists ORDER BY artist_name;");
    }
    else if ("compilation_artist" == field)
    {
        query.prepare("SELECT DISTINCT artist_name FROM music_artists, music_albums where "
                "music_albums.artist_id=music_artists.artist_id ORDER BY artist_name");
    }
    else if ("album" == field)
    {
        query.prepare("SELECT album_name FROM music_albums ORDER BY album_name;");
    }
    else if ("title" == field)
    {
        query.prepare("SELECT name FROM music_songs ORDER BY name;");
    }
    else if ("genre" == field)
    {
        query.prepare("SELECT genre FROM music_genres ORDER BY genre;");
    }
    else
    {
        return searchList;
    }

    if (query.exec() && query.isActive())
    {
        while (query.next())
        {
            searchList << query.value(0).toString();
        }
    }
    return searchList;
}

QString Metadata::getAlbumArtFile(void)
{
    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this);

    AlbumArtImage *albumart_image = NULL;
    QString res;

    if ((albumart_image = m_albumArt->getImage(IT_FRONTCOVER)))
        res = albumart_image->filename;
    else if ((albumart_image = m_albumArt->getImage(IT_UNKNOWN)))
        res = albumart_image->filename;
    else if ((albumart_image = m_albumArt->getImage(IT_BACKCOVER)))
        res = albumart_image->filename;
    else if ((albumart_image = m_albumArt->getImage(IT_INLAY)))
        res = albumart_image->filename;
    else if ((albumart_image = m_albumArt->getImage(IT_CD)))
        res = albumart_image->filename;

    // check file exists
    if (!res.isEmpty())
    {
        int repo = ID_TO_REPO(m_id);
        if (repo == RT_Radio)
        {
            // image is a radio station icon, check if we have already downloaded and cached it
            QString path = GetConfDir() + "/MythMusic/AlbumArt/";
            QFileInfo fi(res);
            QString filename = QString("%1-%2.%3").arg(m_id).arg("front").arg(fi.suffix());

            albumart_image->filename = path + filename;

            if (!QFile::exists(albumart_image->filename))
            {
                // file does not exist so try to download and cache it
                if (!GetMythDownloadManager()->download(res, albumart_image->filename))
                {
                    m_albumArt->getImageList()->removeAll(albumart_image);
                    return QString("");
                }
            }

            res = albumart_image->filename;
        }
        else
        {
            if (!QFile::exists(res))
            {
                if (albumart_image->embedded && getTagger()->supportsEmbeddedImages())
                {
                    // image is embedded try to extract it from the tag and cache it for latter
                    QImage *image = getTagger()->getAlbumArt(m_filename, albumart_image->imageType);
                    if (image)
                    {
                        image->save(res);
                        delete image;
                        return res;
                    }
                }
                else
                {
                    // image is in a file but couldn't be found!
                    m_albumArt->getImageList()->removeAll(albumart_image);
                    return QString("");
                }
            }
        }

        return res;
    }

    return QString("");
}

QString Metadata::getAlbumArtFile(ImageType type)
{
    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this);

    AlbumArtImage *albumart_image = m_albumArt->getImage(type);
    if (albumart_image)
        return albumart_image->filename;

    return QString("");
}

AlbumArtImages *Metadata::getAlbumArtImages(void)
{
    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this);

    return m_albumArt;
}

void Metadata::reloadAlbumArtImages(void)
{
    delete m_albumArt;
    m_albumArt = NULL; //new AlbumArtImages(this);
}


/// create a MetaIO for the file to read/write any tags etc
MetaIO* Metadata::getTagger(void)
{
    static MetaIOID3 metaIOID3;
    static MetaIOOggVorbis metaIOOggVorbis;
    static MetaIOFLACVorbis metaIOFLACVorbis;
    static MetaIOMP4 metaIOMP4;
    static MetaIOWavPack metaIOWavPack;
    static MetaIOAVFComment metaIOAVFComment;

    QFileInfo fi(m_filename);
    QString extension = fi.suffix();

    if (extension == "mp3" || extension == "mp2")
        return &metaIOID3;
    else if (extension == "ogg" || extension == "oga")
        return &metaIOOggVorbis;
    else if (extension == "flac")
        return &metaIOFLACVorbis;
    else if (extension == "m4a")
        return &metaIOMP4;
    else if (extension == "wv")
        return &metaIOWavPack;
    else
        return &metaIOAVFComment;
}

//--------------------------------------------------------------------------

MetadataLoadingThread::MetadataLoadingThread(AllMusic *parent_ptr)
{
    parent = parent_ptr;
}

void MetadataLoadingThread::run()
{
    threadRegister("MetadataLoading");
    //if you want to simulate a big music collection load
    //sleep(3);
    parent->resync();
    threadDeregister();
}

AllMusic::AllMusic(QString path_assignment, QString a_startdir)
{
    m_startdir = a_startdir;
    m_done_loading = false;
    m_numPcs = m_numLoaded = 0;

    m_cd_title = QObject::tr("CD -- none");

    //  How should we sort?
    setSorting(path_assignment);

    m_root_node = new MusicNode(QObject::tr("All My Music"), m_paths);

    //
    //  Start a thread to do data
    //  loading and sorting
    //

    m_metadata_loader = NULL;
    startLoading();

    m_last_listed = -1;
}

AllMusic::~AllMusic()
{
    while (!m_all_music.empty())
    {
        delete m_all_music.back();
        m_all_music.pop_back();
    }

    delete m_root_node;

    m_metadata_loader->wait();
    delete m_metadata_loader;
}

bool AllMusic::cleanOutThreads()
{
    //  If this is still running, the user
    //  probably selected mythmusic and then
    //  escaped out right away

    if(m_metadata_loader->isFinished())
    {
        return true;
    }

    m_metadata_loader->wait();
    return false;
}

/** \fn AllMusic::startLoading(void)
 *  \brief Start loading metadata.
 *
 *  Makes the AllMusic object run it's resync in a thread.
 *  Once done, the doneLoading() method will return true.
 *
 *  \note Alternatively, this could be made to emit a signal
 *        so the caller won't have to poll for completion.
 *
 *  \returns true if the loader thread was started
 */
bool AllMusic::startLoading(void)
{
    // Set this to false early rather than letting it be
    // delayed till the thread calls resync.
    m_done_loading = false;

    if (m_metadata_loader)
    {
        cleanOutThreads();
        delete m_metadata_loader;
    }

    m_metadata_loader = new MetadataLoadingThread(this);
    m_metadata_loader->start();

    return true;
}

void AllMusic::resync()
{
    m_done_loading = false;

    QString aquery = "SELECT music_songs.song_id, music_artists.artist_id, music_artists.artist_name, "
                     "music_comp_artists.artist_name AS compilation_artist, "
                     "music_albums.album_id, music_albums.album_name, music_songs.name, music_genres.genre, music_songs.year, "
                     "music_songs.track, music_songs.length, music_songs.directory_id, "
                     "CONCAT_WS('/', music_directories.path, music_songs.filename) AS filename, "
                     "music_songs.rating, music_songs.numplays, music_songs.lastplay, music_songs.date_entered, "
                     "music_albums.compilation, music_songs.format "
                     "FROM music_songs "
                     "LEFT JOIN music_directories ON music_songs.directory_id=music_directories.directory_id "
                     "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
                     "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                     "LEFT JOIN music_artists AS music_comp_artists ON music_albums.artist_id=music_comp_artists.artist_id "
                     "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
                     "ORDER BY music_songs.song_id;";

    QString filename, artist, album, title, compartist;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec(aquery))
        MythDB::DBError("AllMusic::resync", query);

    m_root_node->clear();
    m_all_music.clear();

    m_numPcs = query.size() * 2;
    m_numLoaded = 0;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            int id = query.value(0).toInt();

            if (!music_map.contains(id))
            {
                filename = query.value(12).toString();
                if (!filename.contains("://"))
                    filename = m_startdir + filename;

                Metadata *mdata = new Metadata(
                    filename,
                    query.value(2).toString(),     // artist
                    query.value(3).toString(),     // compilation artist
                    query.value(5).toString(),     // album
                    query.value(6).toString(),     // title
                    query.value(7).toString(),     // genre
                    query.value(8).toInt(),        // year
                    query.value(9).toInt(),        // track no.
                    query.value(10).toInt(),       // length
                    query.value(0).toInt(),        // id
                    query.value(13).toInt(),       // rating
                    query.value(14).toInt(),       // playcount
                    query.value(15).toDateTime(),  // lastplay
                    query.value(16).toDateTime(),  // date_entered
                    (query.value(17).toInt() > 0), // compilation
                    query.value(18).toString());   // format

                mdata->setDirectoryId(query.value(11).toInt());
                mdata->setArtistId(query.value(1).toInt());
                mdata->setAlbumId(query.value(4).toInt());

                //  Don't delete mdata, as PtrList now owns it
                m_all_music.append(mdata);

                music_map[id] = mdata;
            }

            // compute max/min playcount,lastplay for all music
            if (query.at() == 0)
            {
                // first song
                m_playcountMin = m_playcountMax = query.value(13).toInt();
                m_lastplayMin  = m_lastplayMax  = query.value(14).toDateTime().toTime_t();
            }
            else
            {
                int playCount = query.value(13).toInt();
                double lastPlay = query.value(14).toDateTime().toTime_t();

                m_playcountMin = min(playCount, m_playcountMin);
                m_playcountMax = max(playCount, m_playcountMax);
                m_lastplayMin  = min(lastPlay,  m_lastplayMin);
                m_lastplayMax  = max(lastPlay,  m_lastplayMax);
            }
            m_numLoaded++;
        }
    }
    else
    {
         VERBOSE(VB_IMPORTANT, "MythMusic hasn't found any tracks! "
                               "That's ok with me if it's ok with you.");
    }

    //  Build a tree to reflect current state of
    //  the metadata. Once built, sort it.

    buildTree();
    sortTree();
    m_done_loading = true;
}

void AllMusic::sortTree()
{
    m_root_node->sort();
}

void AllMusic::buildTree()
{
    //
    //  Given "paths" and loaded metadata,
    //  build a tree (nodes, leaves, and all)
    //  that reflects the desired structure
    //  of the metadata. This is a structure
    //  that makes it easy (and QUICK) to
    //  display metadata on (for example) a
    //  Select Music screen
    //

    MetadataPtrList list;

    MetadataPtrList::iterator it = m_all_music.begin();
    for (; it != m_all_music.end(); ++it)
    {
        if ((*it)->isVisible())
            list.append(*it);
        m_numLoaded++;
    }

    MusicTreeBuilder *builder = MusicTreeBuilder::createBuilder(m_paths);
    builder->makeTree(m_root_node, list);
    delete builder;
}

void AllMusic::writeTree(GenericTree *tree_to_write_to)
{
    m_root_node->writeTree(tree_to_write_to, 0);
}

bool AllMusic::putYourselfOnTheListView(TreeCheckItem *where)
{
    m_root_node->putYourselfOnTheListView(where, false);
    return true;
}

void AllMusic::putCDOnTheListView(CDCheckItem *where)
{
    ValueMetadata::iterator anit;
    for(anit = m_cd_data.begin(); anit != m_cd_data.end(); ++anit)
    {
        QString title_string;
        if((*anit).Title().length() > 0)
        {
            title_string = (*anit).FormatTitle();
        }
        else
        {
            title_string = QObject::tr("Unknown");
        }
        QString title_temp = QString("%1 - %2").arg((*anit).Track()).arg(title_string);
        QString level_temp = QObject::tr("title");
        CDCheckItem *new_item = new CDCheckItem(where, title_temp, level_temp,
                                                -(*anit).Track());
        new_item->setCheck(false); //  Avoiding -Wall
    }
}

QString AllMusic::getLabel(int an_id, bool *error_flag)
{
    QString a_label;
    if(an_id > 0)
    {

        if (!music_map.contains(an_id))
        {
            a_label = QString(QObject::tr("Missing database entry: %1")).arg(an_id);
            *error_flag = true;
            return a_label;
        }

        a_label += music_map[an_id]->FormatArtist();
        a_label += " ~ ";
        a_label += music_map[an_id]->FormatTitle();


        if(a_label.length() < 1)
        {
            a_label = QObject::tr("Ooops");
            *error_flag = true;
        }
        else
        {
            *error_flag = false;
        }
        return a_label;
    }
    else
    {
        ValueMetadata::iterator anit;
        for(anit = m_cd_data.begin(); anit != m_cd_data.end(); ++anit)
        {
            if( (*anit).Track() == an_id * -1)
            {
                a_label = QString("(CD) %1 ~ %2").arg((*anit).FormatArtist())
                                                 .arg((*anit).FormatTitle());
                *error_flag = false;
                return a_label;
            }
        }
    }

    a_label.clear();
    *error_flag = true;
    return a_label;
}

Metadata* AllMusic::getMetadata(int an_id)
{
    if(an_id > 0)
    {
        if (music_map.contains(an_id))
        {
            return music_map[an_id];
        }
    }
    else if(an_id < 0)
    {
        ValueMetadata::iterator anit;
        for(anit = m_cd_data.begin(); anit != m_cd_data.end(); ++anit)
        {
            if( (*anit).Track() == an_id * -1)
            {
                return &(*anit);
            }
        }
    }
    return NULL;
}

bool AllMusic::updateMetadata(int an_id, Metadata *the_track)
{
    if (an_id > 0)
    {
        Metadata *mdata = getMetadata(an_id);
        if (mdata)
        {
            *mdata = *the_track;
            return true;
        }
    }
    return false;
}

/// \brief Check each Metadata entry and save those that have changed (ratings, etc.)
void AllMusic::save(void)
{
    MetadataPtrList::iterator it = m_all_music.begin();
    for (; it != m_all_music.end(); ++it)
    {
        if ((*it)->hasChanged())
            (*it)->persist();
    }
}

void AllMusic::clearCDData()
{
    m_cd_data.clear();
    m_cd_title = QObject::tr("CD -- none");
}

void AllMusic::addCDTrack(Metadata *the_track)
{
    m_cd_data.append(*the_track);
}

bool AllMusic::checkCDTrack(Metadata *the_track)
{
    if (m_cd_data.count() < 1)
    {
        return false;
    }
    if (m_cd_data.last().FormatTitle() == the_track->FormatTitle())
    {
        return true;
    }
    return false;
}

bool AllMusic::getCDMetadata(int the_track, Metadata *some_metadata)
{
    ValueMetadata::iterator anit;
    for (anit = m_cd_data.begin(); anit != m_cd_data.end(); ++anit)
    {
        if ((*anit).Track() == the_track)
        {
            *some_metadata = (*anit);
            return true;
        }

    }
    return false;
}

void AllMusic::setSorting(QString a_paths)
{
    m_paths = a_paths;
    MusicNode::SetStaticData(m_startdir, m_paths);

    if (m_paths == "directory")
        return;

    //  Error checking
    QStringList tree_levels = m_paths.split(" ");
    QStringList::const_iterator it = tree_levels.begin();
    for (; it != tree_levels.end(); ++it)
    {
        if (*it != "genre"        &&
            *it != "artist"       &&
            *it != "splitartist"  &&
            *it != "splitartist1" &&
            *it != "album"        &&
            *it != "title")
        {
            VERBOSE(VB_IMPORTANT, QString("AllMusic::setSorting() "
                    "Unknown tree level '%1'").arg(*it));
        }
    }
}

void AllMusic::setAllVisible(bool visible)
{
    MetadataPtrList::iterator it = m_all_music.begin();
    for (; it != m_all_music.end(); ++it)
        (*it)->setVisible(visible);
}

MusicNode::MusicNode(const QString &a_title, const QString &tree_level)
{
    my_title = a_title;
    my_level = tree_level;
    setPlayCountMin(0);
    setPlayCountMax(0);
    setLastPlayMin(0);
    setLastPlayMax(0);
}

MusicNode::~MusicNode()
{
    clear();
}

// static member vars

QString MusicNode::m_startdir;
QString MusicNode::m_paths;
int MusicNode::m_RatingWeight = 2;
int MusicNode::m_PlayCountWeight = 2;
int MusicNode::m_LastPlayWeight = 2;
int MusicNode::m_RandomWeight = 2;

void MusicNode::SetStaticData(const QString &startdir, const QString &paths)
{
    m_startdir = startdir;
    m_paths = paths;
    m_RatingWeight = gCoreContext->GetNumSetting("IntelliRatingWeight", 2);
    m_PlayCountWeight = gCoreContext->GetNumSetting("IntelliPlayCountWeight", 2);
    m_LastPlayWeight = gCoreContext->GetNumSetting("IntelliLastPlayWeight", 2);
    m_RandomWeight = gCoreContext->GetNumSetting("IntelliRandomWeight", 2);
}

void MusicNode::putYourselfOnTheListView(TreeCheckItem *parent, bool show_node)
{
    TreeCheckItem *current_parent;

    if (show_node)
    {
        QString title_temp = my_title;
        QString level_temp = my_level;
        current_parent = new TreeCheckItem(parent, title_temp, level_temp, 0);
    }
    else
    {
        current_parent = parent;
    }


    MetadataPtrList::iterator it = my_tracks.begin();
    for (; it != my_tracks.end(); ++it)
    {
        QString title_temp = QString(QObject::tr("%1 - %2"))
            .arg((*it)->Track()).arg((*it)->Title());
        QString level_temp = QObject::tr("title");
        TreeCheckItem *new_item = new TreeCheckItem(
            current_parent, title_temp, level_temp, (*it)->ID());
        new_item->setCheck(false); //  Avoiding -Wall
    }

    MusicNodePtrList::iterator mit = my_subnodes.begin();
    for (; mit != my_subnodes.end(); ++mit)
        (*mit)->putYourselfOnTheListView(current_parent, true);

}

void MusicNode::writeTree(GenericTree *tree_to_write_to, int a_counter)
{

    GenericTree *sub_node = tree_to_write_to->addNode(my_title);
    sub_node->setAttribute(0, 0);
    sub_node->setAttribute(1, a_counter);
    sub_node->setAttribute(2, a_counter);
    sub_node->setAttribute(3, a_counter);
    sub_node->setAttribute(4, a_counter);
    sub_node->setAttribute(5, a_counter);

    int track_counter = 0;

    MetadataPtrList::iterator it = my_tracks.begin();
    for (; it != my_tracks.end(); ++it)
    {
        QString title_temp = QObject::tr("%1 - %2")
            .arg((*it)->Track()).arg((*it)->Title());
        GenericTree *subsub_node = sub_node->addNode(title_temp, (*it)->ID(), true);
        subsub_node->setAttribute(0, 1);
        subsub_node->setAttribute(1, track_counter);    // regular order
        subsub_node->setAttribute(2, rand());           // random order

        //
        //  "Intelligent" ordering
        //
        int rating = (*it)->Rating();
        int playcount = (*it)->PlayCount();
        double lastplaydbl = (*it)->LastPlay().toTime_t();
        double ratingValue = (double)(rating) / 10;
        double playcountValue, lastplayValue;

        if (m_playcountMax == m_playcountMin)
        {
            playcountValue = 0;
        }
        else
        {
            playcountValue = ((m_playcountMin - (double)playcount) /
                              (m_playcountMax - m_playcountMin) + 1);
        }

        if (m_lastplayMax == m_lastplayMin)
        {
            lastplayValue = 0;
        }
        else
        {
            lastplayValue = ((m_lastplayMin - lastplaydbl) /
                             (m_lastplayMax - m_lastplayMin) + 1);
        }

        double rating_value =
            m_RatingWeight * ratingValue +
            m_PlayCountWeight * playcountValue +
            m_LastPlayWeight * lastplayValue +
            m_RandomWeight * (double)rand() / (RAND_MAX + 1.0);

        int integer_rating = (int) (4000001 - rating_value * 10000);
        subsub_node->setAttribute(3, integer_rating);   //  "intelligent" order
        ++track_counter;
    }

    MusicNodePtrList::const_iterator sit = my_subnodes.begin();
    for (int another_counter = 0; sit != my_subnodes.end(); ++sit)
    {
        (*sit)->setPlayCountMin(m_playcountMin);
        (*sit)->setPlayCountMax(m_playcountMax);
        (*sit)->setLastPlayMin(m_lastplayMin);
        (*sit)->setLastPlayMax(m_lastplayMax);
        (*sit)->writeTree(sub_node, another_counter);
        ++another_counter;
    }
}


void MusicNode::sort()
{
    //  Sort any tracks
    qStableSort(my_tracks.begin(), my_tracks.end(), meta_less_than);

    //  Sort any subnodes
    qStableSort(my_subnodes.begin(), my_subnodes.end(), music_less_than);

    //  Tell any subnodes to sort themselves
    MusicNodePtrList::const_iterator sit = my_subnodes.begin();
    for (; sit != my_subnodes.end(); ++sit)
        (*sit)->sort();
}

/**************************************************************************/

AlbumArtImages::AlbumArtImages(Metadata *metadata)
    : m_parent(metadata)
{
    findImages();
}

AlbumArtImages::~AlbumArtImages()
{
    while (!m_imageList.empty())
    {
        delete m_imageList.back();
        m_imageList.pop_back();
    }
}

void AlbumArtImages::findImages(void)
{
    while (!m_imageList.empty())
    {
        delete m_imageList.back();
        m_imageList.pop_back();
    }

    if (m_parent == NULL)
        return;

    int trackid = ID_TO_ID(m_parent->ID());
    int repo = ID_TO_REPO(m_parent->ID());

    if (repo == RT_Radio)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT logourl FROM music_radios WHERE intid = :ID;");
        query.bindValue(":ID", trackid);
        if (query.exec())
        {
            while (query.next())
            {
                QString logoUrl = query.value(0).toString();
                AlbumArtImage *image = new AlbumArtImage();
                image->id = -1;
                image->filename = logoUrl;
                image->imageType = IT_FRONTCOVER;
                image->embedded = false;

                m_imageList.push_back(image);
            }
        }
    }
    else
    {
        if (trackid == 0)
            return;

        QFileInfo fi(m_parent->Filename());
        QString dir = fi.absolutePath();
        dir.remove(0, Metadata::GetStartdir().length());

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT albumart_id, CONCAT_WS('/', music_directories.path, "
                "music_albumart.filename), music_albumart.filename, music_albumart.imagetype, "
                "music_albumart.embedded "
                "FROM music_albumart "
                "LEFT JOIN music_directories ON "
                "music_directories.directory_id = music_albumart.directory_id "
                "WHERE music_directories.path = :DIR "
                "OR song_id = :SONGID "
                "ORDER BY music_albumart.imagetype;");
        query.bindValue(":DIR", dir);
        query.bindValue(":SONGID", trackid);
        if (query.exec())
        {
            while (query.next())
            {
                AlbumArtImage *image = new AlbumArtImage();
                bool embedded = (query.value(4).toInt() == 1);
                image->id = query.value(0).toInt();

                if (embedded)
                    image->filename = GetConfDir() + "/MythMusic/AlbumArt/" + query.value(1).toString();
                else
                    image->filename = Metadata::GetStartdir() + query.value(1).toString();

                image->imageType = (ImageType) query.value(3).toInt();
                image->embedded = embedded;

                m_imageList.push_back(image);
            }
        }
    }
}

AlbumArtImage *AlbumArtImages::getImage(ImageType type)
{
    AlbumArtList::iterator it = m_imageList.begin();
    for (; it != m_imageList.end(); ++it)
    {
        if ((*it)->imageType == type)
            return *it;
    }

    return NULL;
}

QStringList AlbumArtImages::getImageFilenames(void) const
{
    QStringList paths;

    AlbumArtList::const_iterator it = m_imageList.begin();
    for (; it != m_imageList.end(); ++it)
        paths += (*it)->filename;

    return paths;
}

AlbumArtImage *AlbumArtImages::getImageAt(uint index)
{
    if (index < (uint)m_imageList.size())
        return m_imageList[index];

    return NULL;
}

bool AlbumArtImages::saveImageType(const int id, ImageType type)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE music_albumart SET imagetype = :TYPE "
                    "WHERE albumart_id = :ID");
    query.bindValue(":TYPE", type);
    query.bindValue(":ID", id);
    return (query.exec());
}

// static method to get a translated type name from an ImageType
QString AlbumArtImages::getTypeName(ImageType type)
{
    // these const's should match the ImageType enum's
    static const char* type_strings[] = {
        QT_TR_NOOP("Unknown"),            // IT_UNKNOWN
        QT_TR_NOOP("Front Cover"),        // IT_FRONTCOVER
        QT_TR_NOOP("Back Cover"),         // IT_BACKCOVER
        QT_TR_NOOP("CD"),                 // IT_CD
        QT_TR_NOOP("Inlay")               // IT_INLAY
    };

    return QObject::tr(type_strings[type]);
}

// static method to get a filename from an ImageType
QString AlbumArtImages::getTypeFilename(ImageType type)
{
    // these const's should match the ImageType enum's
    static const char* filename_strings[] = {
        QT_TR_NOOP("unknown"),      // IT_UNKNOWN
        QT_TR_NOOP("front"),        // IT_FRONTCOVER
        QT_TR_NOOP("back"),         // IT_BACKCOVER
        QT_TR_NOOP("cd"),           // IT_CD
        QT_TR_NOOP("inlay")         // IT_INLAY
    };

    return QObject::tr(filename_strings[type]);
}
// static method to guess the image type from the filename
ImageType AlbumArtImages::guessImageType(const QString &filename)
{
    ImageType type = IT_FRONTCOVER;

    if (filename.contains("front", Qt::CaseInsensitive) ||
             filename.contains(QObject::tr("front"), Qt::CaseInsensitive))
        type = IT_FRONTCOVER;
    else if (filename.contains("back", Qt::CaseInsensitive) ||
             filename.contains(QObject::tr("back"),  Qt::CaseInsensitive))
        type = IT_BACKCOVER;
    else if (filename.contains("inlay", Qt::CaseInsensitive) ||
             filename.contains(QObject::tr("inlay"), Qt::CaseInsensitive))
        type = IT_INLAY;
    else if (filename.contains("cd", Qt::CaseInsensitive) ||
             filename.contains(QObject::tr("cd"), Qt::CaseInsensitive))
        type = IT_CD;
    else if (filename.contains("cover", Qt::CaseInsensitive) ||
             filename.contains(QObject::tr("cover"), Qt::CaseInsensitive))
        type = IT_FRONTCOVER;

    return type;
}

void AlbumArtImages::addImage(const AlbumArtImage &newImage)
{
    // do we already have an image of this type?
    AlbumArtImage *image = NULL;

    AlbumArtList::iterator it = m_imageList.begin();
    for (; it != m_imageList.end(); ++it)
    {
        if ((*it)->imageType == newImage.imageType && (*it)->embedded == newImage.embedded)
        {
            image = *it;
            break;
        }
    }

    if (!image)
    {
        // not found so just add it to the list
        image = new AlbumArtImage(newImage);
        m_imageList.push_back(image);
    }
    else
    {
        // we already have an image of this type so just update it with the new info
        image->filename = newImage.filename;
        image->description = newImage.description;
    }

    // if this is an embedded image copy it to disc to speed up its display
    if (image->embedded && m_parent->getTagger()->supportsEmbeddedImages())
    {
        QString path = GetConfDir() + "/MythMusic/AlbumArt/";
        QDir dir(path);

        QString filename = QString("%1-%2.jpg").arg(m_parent->ID()).arg(AlbumArtImages::getTypeFilename(image->imageType));
        if (!QFile::exists(path + filename))
        {
            if (!dir.exists())
                dir.mkpath(path);

            QImage *saveImage = m_parent->getTagger()->getAlbumArt(m_parent->Filename(), image->imageType);
            if (saveImage)
            {
                saveImage->save(path + filename, "JPEG");
                delete saveImage;
            }
        }

        image->filename = path + filename;
    }
}

/// saves or updates the image details in the DB 
void AlbumArtImages::dumpToDatabase(void)
{
    Metadata::IdType trackID = ID_TO_ID(m_parent->ID());
    int directoryID = m_parent->getDirectoryId();

    // sanity check we have a valid songid and directoryid
    if (trackID == 0 || directoryID == -1)
    {
        VERBOSE(VB_ALL, "AlbumArtImages: Asked to save to the DB but have invalid songid or directoryid");
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    AlbumArtList::iterator it = m_imageList.begin();
    for (; it != m_imageList.end(); ++it)
    {
        AlbumArtImage *image = (*it);

        // first check to see if the image is already in the db
        if (image->embedded)
        {
            query.prepare("SELECT albumart_id FROM music_albumart "
                          "WHERE song_id = :SONGID AND imagetype = :TYPE;");
            query.bindValue(":TYPE", image->imageType);
            query.bindValue(":SONGID", trackID);
        }
        else
        {
            query.prepare("SELECT albumart_id FROM music_albumart "
                          "WHERE directory_id = :DIRECTORYID AND imagetype = :TYPE;");
            query.bindValue(":TYPE", image->imageType);
            query.bindValue(":DIRECTORYID", directoryID);
        }

        if (query.exec() && query.next())
        {
            // update the existing record
            int artid = query.value(0).toInt();

            query.prepare("UPDATE music_albumart SET "
                          "filename = :FILENAME, imagetype = :TYPE, "
                          "song_id = :SONGID, directory_id = :DIRECTORYID, embedded = :EMBED "
                          "WHERE albumart_id = :ARTID");

            query.bindValue(":ARTID", artid);
        }
        else
        {
            // add new record for this image
            query.prepare("INSERT INTO music_albumart ( filename, "
                        "imagetype, song_id, directory_id, embedded ) VALUES ( "
                        ":FILENAME, :TYPE, :SONGID, :DIRECTORYID, :EMBED );");
        }

        QFileInfo fi(image->filename);
        query.bindValue(":FILENAME", fi.fileName());

        query.bindValue(":TYPE", image->imageType);
        query.bindValue(":SONGID", image->embedded ? trackID : 0);
        query.bindValue(":DIRECTORYID", image->embedded ? 0 : directoryID);
        query.bindValue(":EMBED", image->embedded);

        if (!query.exec())
            MythDB::DBError("AlbumArtImages::dumpToDatabase - "
                            "add/update music_albumart", query);
    }
}


/**************************************************************************/

MusicData::MusicData(void)
{
    all_playlists = NULL;
    all_music = NULL;
    initialized = false;
}

MusicData::~MusicData(void)
{
    if (all_playlists)
    {
        delete all_playlists;
        all_playlists = NULL;
    }

    if (all_music)
    {
        delete all_music;
        all_music = NULL;
    }
}

/// reload music after a scan, rip or import
void MusicData::reloadMusic(void)
{
    if (!all_music || !all_playlists)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = QObject::tr("Rebuilding music tree");

    MythUIBusyDialog *busy = new MythUIBusyDialog(message, popupStack,
                                                  "musicscanbusydialog");

    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = NULL;

    all_music->startLoading();
    while (!all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }
    all_playlists->postLoad();

    if (busy)
        busy->Close();
}

