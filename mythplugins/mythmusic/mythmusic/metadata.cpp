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
#include <mythlogging.h>
#include <util.h>

// mythmusic
#include "metadata.h"
#include "metaio.h"
#include "metaioid3.h"
#include "metaiomp4.h"
#include "metaioavfcomment.h"
#include "metaiooggvorbis.h"
#include "metaioflacvorbis.h"
#include "metaiowavpack.h"
#include "playlist.h"
#include "playlistcontainer.h"
#include "musicutils.h"



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

Metadata::~Metadata()
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
    m_templastplay = rhs.m_templastplay;
    m_dateadded = rhs.m_dateadded;
    m_playcount = rhs.m_playcount;
    m_tempplaycount = rhs.m_tempplaycount;
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

void Metadata::persist()
{
    if (m_id < 1)
        return;

    if (m_templastplay.isValid())
    {
        m_lastplay = m_templastplay;
        m_playcount = m_tempplaycount;

        m_templastplay = QDateTime();
    }

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

    gPlayer->sendTrackStatsChangedEvent(ID());

    m_changed = false;
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
        LOG(VB_GENERAL, LOG_ERR, QString("Something asked me to set data "
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
        LOG(VB_GENERAL, LOG_ERR, QString("Something asked me to return data "
                                         "about a field called %1").arg(field));
        *data = "I Dunno";
    }
}

void Metadata::toMap(MetadataMap &metadataMap, const QString &prefix)
{
    metadataMap[prefix + "artist"] = m_artist;
    metadataMap[prefix + "formatartist"] = FormatArtist();
    metadataMap[prefix + "compilationartist"] = m_compilation_artist;
    metadataMap[prefix + "album"] = m_album;
    metadataMap[prefix + "title"] = m_title;
    metadataMap[prefix + "formattitle"] = FormatTitle();
    metadataMap[prefix + "tracknum"] = (m_tracknum > 0 ? QString("%1").arg(m_tracknum) : "");
    metadataMap[prefix + "genre"] = m_genre;
    metadataMap[prefix + "year"] = (m_year > 0 ? QString("%1").arg(m_year) : "");

    int len = m_length / 1000;
    int eh = len / 3600;
    int em = (len / 60) % 60;
    int es = len % 60;
    if (eh > 0)
        metadataMap[prefix + "length"] = QString().sprintf("%d:%02d:%02d", eh, em, es);
    else
        metadataMap[prefix + "length"] = QString().sprintf("%02d:%02d", em, es);

    if (m_lastplay.isValid())
        metadataMap[prefix + "lastplayed"] = MythDateTimeToString(m_lastplay,
                                                kDateFull | kSimplify | kAddYear);
    else
        metadataMap[prefix + "lastplayed"] = QObject::tr("Never Played");

    metadataMap[prefix + "dateadded"] = MythDateTimeToString(m_dateadded,
                                              kDateFull | kSimplify | kAddYear);

    metadataMap[prefix + "playcount"] = QString::number(m_playcount);
    metadataMap[prefix + "filename"] = m_filename;
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

void Metadata::setLastPlay()
{
    m_templastplay = QDateTime::currentDateTime();
    m_changed = true;
}

void Metadata::incPlayCount()
{
    m_tempplaycount = m_playcount + 1;
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
    {
        if (metaIOID3.TagExists(m_filename))
            return &metaIOID3;
        else
            return &metaIOFLACVorbis;
    }
    else if (extension == "m4a")
        return &metaIOMP4;
    else if (extension == "wv")
        return &metaIOWavPack;
    else
        return &metaIOAVFComment;
}

//--------------------------------------------------------------------------

MetadataLoadingThread::MetadataLoadingThread(AllMusic *parent_ptr) :
    MThread("MetadataLoading"), parent(parent_ptr)
{
}

void MetadataLoadingThread::run()
{
    RunProlog();
    //if you want to simulate a big music collection load
    //sleep(3);
    parent->resync();
    RunEpilog();
}

AllMusic::AllMusic(QString a_startdir) :
    m_numPcs(0),
    m_numLoaded(0),
    m_startdir(a_startdir),
    m_metadata_loader(NULL),
    m_done_loading(false),
    m_last_listed(-1),

    m_playcountMin(0),
    m_playcountMax(0),
    m_lastplayMin(0.0),
    m_lastplayMax(0.0)
{
    //  Start a thread to do data loading and sorting
    startLoading();
}

AllMusic::~AllMusic()
{
    while (!m_all_music.empty())
    {
        delete m_all_music.back();
        m_all_music.pop_back();
    }

    while (!m_cdData.empty())
    {
        delete m_cdData.back();
        m_cdData.pop_back();
    }

    m_metadata_loader->wait();
    delete m_metadata_loader;
}

bool AllMusic::cleanOutThreads()
{
    //  If this is still running, the user
    //  probably selected mythmusic and then
    //  escaped out right away

    if (m_metadata_loader->isFinished())
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

// NOTE we don't clear the existing tracks just load any new ones found in the DB
// maybe we should also check for any removed tracks?
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
         LOG(VB_GENERAL, LOG_ERR, "MythMusic hasn't found any tracks! "
                                  "That's ok with me if it's ok with you.");
    }

    m_done_loading = true;
}

Metadata* AllMusic::getMetadata(int an_id)
{
    if (music_map.contains(an_id))
        return music_map[an_id];

    return NULL;
}

bool AllMusic::isValidID(int an_id)
{
    return music_map.contains(an_id);
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

// cd stuff
void AllMusic::clearCDData(void)
{
    while (!m_cdData.empty())
    {
        delete m_cdData.back();
        m_cdData.pop_back();
    }

    m_cdTitle = QObject::tr("CD -- none");
}

void AllMusic::addCDTrack(const Metadata &the_track)
{
    Metadata *mdata = new Metadata(the_track);
    mdata->setID(m_cdData.count() + 1);
    mdata->setRepo(RT_CD);
    m_cdData.append(mdata);
    music_map[mdata->ID()] = mdata;
}

bool AllMusic::checkCDTrack(Metadata *the_track)
{
    if (m_cdData.count() < 1)
        return false;

    if (m_cdData.last()->FormatTitle() == the_track->FormatTitle())
        return true;

    return false;
}

Metadata* AllMusic::getCDMetadata(int the_track)
{
    MetadataPtrList::iterator anit;
    for (anit = m_cdData.begin(); anit != m_cdData.end(); ++anit)
    {
        if ((*anit)->Track() == the_track)
        {
            return (*anit);
        }
    }

    return NULL;
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

        // add any artist images
        QString artist = m_parent->Artist().toLower();
        if (findIcon("artist", artist) != QString())
        {
            AlbumArtImage *image = new AlbumArtImage();
            image->id = -1;
            image->filename = findIcon("artist", artist);
            image->imageType = IT_ARTIST;
            image->embedded = false;

            m_imageList.push_back(image);
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

// static method to get a translated type name from an ImageType
QString AlbumArtImages::getTypeName(ImageType type)
{
    // these const's should match the ImageType enum's
    static const char* type_strings[] = {
        QT_TR_NOOP("Unknown"),            // IT_UNKNOWN
        QT_TR_NOOP("Front Cover"),        // IT_FRONTCOVER
        QT_TR_NOOP("Back Cover"),         // IT_BACKCOVER
        QT_TR_NOOP("CD"),                 // IT_CD
        QT_TR_NOOP("Inlay"),              // IT_INLAY
        QT_TR_NOOP("Artist"),             // IT_ARTIST
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
        QT_TR_NOOP("inlay"),        // IT_INLAY
        QT_TR_NOOP("artist")        // IT_ARTIST
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
        image->imageType = newImage.imageType;
        image->embedded = newImage.embedded;
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
        LOG(VB_GENERAL, LOG_ERR, "AlbumArtImages: Asked to save to the DB but "
                                 "have invalid songid or directoryid");
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    // remove all albumart for this track from the db
    query.prepare("DELETE FROM music_albumart "
                  "WHERE song_id = :SONGID "
                  "OR (embedded = 0 AND directory_id = :DIRECTORYID)");

    query.bindValue(":SONGID", trackID);
    query.bindValue(":DIRECTORYID", directoryID);

    query.exec();

    // now add the albumart to the db
    AlbumArtList::iterator it = m_imageList.begin();
    for (; it != m_imageList.end(); ++it)
    {
        AlbumArtImage *image = (*it);

        //TODO: for the moment just ignore artist images
        if (image->imageType == IT_ARTIST)
            continue;

        if (image->id > 0)
        {
            // re-use the same id this image had before
            query.prepare("INSERT INTO music_albumart ( albumart_id, "
                          "filename, imagetype, song_id, directory_id, embedded ) "
                          "VALUES ( :ID, :FILENAME, :TYPE, :SONGID, :DIRECTORYID, :EMBED );");
            query.bindValue(":ID", image->id);
        }
        else
        {
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

