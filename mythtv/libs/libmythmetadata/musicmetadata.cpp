// qt
#include <QApplication>
#include <QRegExp>
#include <QDateTime>
#include <QDir>
#include <QScopedPointer>

// mythtv
#include <mythcontext.h>
#include <mythwidgets.h>
#include <mythdate.h>
#include <mythdb.h>
#include <mythdirs.h>
#include <mythprogressdialog.h>
#include <mythdownloadmanager.h>
#include <mythlogging.h>
#include <mythdate.h>
#include <remotefile.h>
#include <storagegroup.h>
#include <mythsystem.h>

// libmythmetadata
#include "musicmetadata.h"
#include "metaio.h"
#include "metaioid3.h"
#include "metaiomp4.h"
#include "metaioavfcomment.h"
#include "metaiooggvorbis.h"
#include "metaioflacvorbis.h"
#include "metaiowavpack.h"
#include "musicutils.h"

static QString thePrefix = "the ";

bool operator==(MusicMetadata& a, MusicMetadata& b)
{
    if (a.Filename() == b.Filename())
        return true;
    return false;
}

bool operator!=(MusicMetadata& a, MusicMetadata& b)
{
    if (a.Filename() != b.Filename())
        return true;
    return false;
}

// this ctor is for radio streams
MusicMetadata::MusicMetadata(int lid, QString lstation, QString lchannel, QString lurl,
                   QString llogourl, QString lgenre, QString lmetaformat, QString lformat)
         :  m_artist(""),
            m_compilation_artist(""),
            m_album(""),
            m_title(""),
            m_formattedartist(""),
            m_formattedtitle(""),
            m_genre(lgenre),
            m_format(lformat),
            m_year(0),
            m_tracknum(0),
            m_trackCount(0),
            m_length(0),
            m_rating(0),
            m_directoryid(-1),
            m_artistid(-1),
            m_compartistid(-1),
            m_albumid(-1),
            m_genreid(-1),
            m_lastplay(QDateTime()),
            m_templastplay(QDateTime()),
            m_dateadded(QDateTime()),
            m_playcount(0),
            m_tempplaycount(0),
            m_compilation(0),
            m_albumArt(NULL),
            m_id(lid),
            m_filename(lurl),
            m_hostname(""),
            m_fileSize(0),
            m_changed(false),
            m_station(lstation),
            m_channel(lchannel),
            m_logoUrl(llogourl),
            m_metaFormat(lmetaformat)
{
    setRepo(RT_Radio);
}

MusicMetadata::~MusicMetadata()
{
    if (m_albumArt)
    {
        delete m_albumArt;
        m_albumArt = NULL;
    }
}


MusicMetadata& MusicMetadata::operator=(const MusicMetadata &rhs)
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
    m_trackCount = rhs.m_trackCount;
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
    m_actualFilename = rhs.m_actualFilename;
    m_hostname = rhs.m_hostname;
    m_directoryid = rhs.m_directoryid;
    m_artistid = rhs.m_artistid;
    m_compartistid = rhs.m_compartistid;
    m_albumid = rhs.m_albumid;
    m_genreid = rhs.m_genreid;
    m_albumArt = NULL;
    m_format = rhs.m_format;
    m_changed = rhs.m_changed;
    m_fileSize = rhs.m_fileSize;
    m_station = rhs.m_station;
    m_channel = rhs.m_channel;
    m_logoUrl = rhs.m_logoUrl;
    m_metaFormat = rhs.m_metaFormat;

    return *this;
}

// return true if this == mdata
bool MusicMetadata::compare(MusicMetadata *mdata) const
{
    return (
        m_artist == mdata->m_artist &&
        m_compilation_artist == mdata->m_compilation_artist &&
        m_album == mdata->m_album &&
        m_title == mdata->m_title &&
        m_year == mdata->m_year &&
        m_tracknum == mdata->m_tracknum &&
        m_trackCount == mdata->m_trackCount &&
        //m_length == mdata->m_length &&
        m_rating == mdata->m_rating &&
        m_lastplay == mdata->m_lastplay &&
        m_playcount == mdata->m_playcount &&
        m_compilation == mdata->m_compilation &&
        m_filename == mdata->m_filename &&
        m_directoryid == mdata->m_directoryid &&
        m_artistid == mdata->m_artistid &&
        m_compartistid == mdata->m_compartistid &&
        m_albumid == mdata->m_albumid &&
        m_genreid == mdata->m_genreid &&
        m_format == mdata->m_format &&
        m_fileSize == mdata->m_fileSize
    );
}

void MusicMetadata::persist()
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

    m_changed = false;
}

void MusicMetadata::saveHostname(void)
{
    if (m_id < 1)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE music_songs SET hostname = :HOSTNAME "
                  "WHERE song_id = :ID ;");
    query.bindValue(":HOSTNAME", m_hostname);
    query.bindValue(":ID", m_id);

    if (!query.exec())
        MythDB::DBError("music save hostname", query);
}

// static
MusicMetadata *MusicMetadata::createFromFilename(const QString &filename)
{
    // find the trackid for this filename
    QString sqldir = filename.section('/', 0, -2);

    QString sqlfilename = filename.section('/', -1);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT song_id FROM music_songs "
        "LEFT JOIN music_directories ON music_songs.directory_id=music_directories.directory_id "
        "WHERE music_songs.filename = :FILENAME "
        "AND music_directories.path = :DIRECTORY ;");
    query.bindValue(":FILENAME", sqlfilename);
    query.bindValue(":DIRECTORY", sqldir);

    if (!query.exec())
    {
        MythDB::DBError("MusicMetadata::createFromFilename", query);
        return NULL;
    }

    if (!query.next())
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("MusicMetadata::createFromFilename: Could not find '%1'")
                .arg(filename));
        return NULL;
    }

    int songID = query.value(0).toInt();

    return MusicMetadata::createFromID(songID);
}

// static
MusicMetadata *MusicMetadata::createFromID(int trackid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT music_artists.artist_name, "
    "music_comp_artists.artist_name AS compilation_artist, "
    "music_albums.album_name, music_songs.name, music_genres.genre, "
    "music_songs.year, music_songs.track, music_songs.length, "
    "music_songs.song_id, music_songs.rating, music_songs.numplays, "
    "music_songs.lastplay, music_albums.compilation, music_songs.format, "
    "music_songs.track_count, music_songs.size, music_songs.date_entered, "
    "CONCAT_WS('/', music_directories.path, music_songs.filename) AS filename, "
    "music_songs.hostname "
    "FROM music_songs "
    "LEFT JOIN music_directories ON music_songs.directory_id=music_directories.directory_id "
    "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
    "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
    "LEFT JOIN music_artists AS music_comp_artists ON music_albums.artist_id=music_comp_artists.artist_id "
    "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
    "WHERE music_songs.song_id = :SONGID; ");
    query.bindValue(":SONGID", trackid);

    if (query.exec() && query.next())
    {
        MusicMetadata *mdata = new MusicMetadata();
        mdata->m_artist = query.value(0).toString();
        mdata->m_compilation_artist = query.value(1).toString();
        mdata->m_album = query.value(2).toString();
        mdata->m_title = query.value(3).toString();
        mdata->m_genre = query.value(4).toString();
        mdata->m_year = query.value(5).toInt();
        mdata->m_tracknum = query.value(6).toInt();
        mdata->m_length = query.value(7).toInt();
        mdata->m_id = query.value(8).toUInt();
        mdata->m_rating = query.value(9).toInt();
        mdata->m_playcount = query.value(10).toInt();
        mdata->m_lastplay = query.value(11).toDateTime();
        mdata->m_compilation = (query.value(12).toInt() > 0);
        mdata->m_format = query.value(13).toString();
        mdata->m_trackCount = query.value(14).toInt();
        mdata->m_fileSize = (quint64)query.value(15).toULongLong();
        mdata->m_dateadded = query.value(16).toDateTime();
        mdata->m_filename = query.value(17).toString();
        mdata->m_hostname = query.value(18).toString();

        if (!QHostAddress(mdata->m_hostname).isNull()) // A bug caused an IP to replace hostname, reset and it will fix itself
        {
            mdata->m_hostname = "";
            mdata->saveHostname();
        }

        return mdata;
    }

    return NULL;
}

void MusicMetadata::reloadMetadata(void)
{
    MusicMetadata *mdata = MusicMetadata::createFromID(m_id);

    if (!mdata)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MusicMetadata: Asked to reload metadata "
                                         "for trackID: %1 but not found!").arg(m_id));

        return;
    }

    *this = *mdata;

    delete mdata;

    m_directoryid = -1;
    m_artistid = -1;
    m_compartistid = -1;
    m_albumid = -1;
    m_genreid = -1;
}

int MusicMetadata::getDirectoryId()
{
    if (m_directoryid < 0)
    {
        QString sqldir = m_filename.section('/', 0, -2);
        QString sqlfilename = m_filename.section('/', -1);

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
                return -1;
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
                    return -1;
                }
                m_directoryid = query.lastInsertId().toInt();
            }
        }
    }

    return m_directoryid;
}

int MusicMetadata::getArtistId()
{
    if (m_artistid < 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        // Load the artist id
        query.prepare("SELECT artist_id FROM music_artists "
                    "WHERE artist_name = :ARTIST ;");
        query.bindValue(":ARTIST", m_artist);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select artist id", query);
            return -1;
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
                return -1;
            }
            m_artistid = query.lastInsertId().toInt();
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
                return -1;
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
                    return -1 ;
                }
                m_compartistid = query.lastInsertId().toInt();
            }
        }
    }

    return m_artistid;
}

int MusicMetadata::getAlbumId()
{
    if (m_albumid < 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT album_id FROM music_albums "
                    "WHERE artist_id = :COMP_ARTIST_ID "
                    " AND album_name = :ALBUM ;");
        query.bindValue(":COMP_ARTIST_ID", m_compartistid);
        query.bindValue(":ALBUM", m_album);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select album id", query);
            return -1;
        }
        if (query.next())
        {
            m_albumid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_albums (artist_id, album_name, compilation, year) "
                          "VALUES (:COMP_ARTIST_ID, :ALBUM, :COMPILATION, :YEAR);");
            query.bindValue(":COMP_ARTIST_ID", m_compartistid);
            query.bindValue(":ALBUM", m_album);
            query.bindValue(":COMPILATION", m_compilation);
            query.bindValue(":YEAR", m_year);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert album", query);
                return -1;
            }
            m_albumid = query.lastInsertId().toInt();
        }
    }

    return m_albumid;
}

int MusicMetadata::getGenreId()
{
    if (m_genreid < 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT genre_id FROM music_genres "
                    "WHERE genre = :GENRE ;");
        query.bindValue(":GENRE", m_genre);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select genre id", query);
            return -1;
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
                return -1;
            }
            m_genreid = query.lastInsertId().toInt();
        }
    }

    return m_genreid;
}

void MusicMetadata::dumpToDatabase()
{
    if (m_directoryid < 0)
        getDirectoryId();

    if (m_artistid < 0)
        getArtistId();

    if (m_albumid < 0)
        getAlbumId();

    if (m_genreid < 0)
        getGenreId();

    // We have all the id's now. We can insert it.
    QString strQuery;
    if (m_id < 1)
    {
        strQuery = "INSERT INTO music_songs ( directory_id,"
                   " artist_id, album_id,    name,         genre_id,"
                   " year,      track,       length,       filename,"
                   " rating,    format,      date_entered, date_modified,"
                   " numplays,  track_count, size,         hostname) "
                   "VALUES ( "
                   " :DIRECTORY, "
                   " :ARTIST,   :ALBUM,      :TITLE,       :GENRE,"
                   " :YEAR,     :TRACKNUM,   :LENGTH,      :FILENAME,"
                   " :RATING,   :FORMAT,     :DATE_ADD,    :DATE_MOD,"
                   " :PLAYCOUNT,:TRACKCOUNT, :SIZE,        :HOSTNAME );";
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
                   ", track_count = :TRACKCOUNT "
                   ", size = :SIZE "
                   ", hostname = :HOSTNAME "
                   "WHERE song_id= :ID ;";
    }

    QString sqldir = m_filename.section('/', 0, -2);
    QString sqlfilename = m_filename.section('/', -1);

    MSqlQuery query(MSqlQuery::InitCon());

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
    query.bindValue(":DATE_MOD", MythDate::current());
    query.bindValue(":PLAYCOUNT", m_playcount);

    if (m_id < 1)
        query.bindValue(":DATE_ADD",  MythDate::current());
    else
        query.bindValue(":ID", m_id);

    query.bindValue(":TRACKCOUNT", m_trackCount);
    query.bindValue(":SIZE", (quint64)m_fileSize);
    query.bindValue(":HOSTNAME", m_hostname);

    if (!query.exec())
        MythDB::DBError("MusicMetadata::dumpToDatabase - updating music_songs",
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
QString MusicMetadata::m_formatnormalfileartist      = "ARTIST";
QString MusicMetadata::m_formatnormalfiletrack       = "TITLE";
QString MusicMetadata::m_formatnormalcdartist        = "ARTIST";
QString MusicMetadata::m_formatnormalcdtrack         = "TITLE";
QString MusicMetadata::m_formatcompilationfileartist = "COMPARTIST";
QString MusicMetadata::m_formatcompilationfiletrack  = "TITLE (ARTIST)";
QString MusicMetadata::m_formatcompilationcdartist   = "COMPARTIST";
QString MusicMetadata::m_formatcompilationcdtrack    = "TITLE (ARTIST)";

void MusicMetadata::setArtistAndTrackFormats()
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


bool MusicMetadata::determineIfCompilation(bool cd)
{
    m_compilation = (!m_compilation_artist.isEmpty()
                   && m_artist != m_compilation_artist);
    setCompilationFormatting(cd);
    return m_compilation;
}


inline QString MusicMetadata::formatReplaceSymbols(const QString &format)
{
  QString rv = format;
  rv.replace("COMPARTIST", m_compilation_artist);
  rv.replace("ARTIST", m_artist);
  rv.replace("TITLE", m_title);
  rv.replace("TRACK", QString("%1").arg(m_tracknum, 2));
  return rv;
}

void MusicMetadata::checkEmptyFields()
{
    if (m_artist.isEmpty())
        m_artist = tr("Unknown Artist", "Default artist if no artist");
    // This should be the same as Artist if it's a compilation track or blank
    if (!m_compilation || m_compilation_artist.isEmpty())
        m_compilation_artist = m_artist;
    if (m_album.isEmpty())
        m_album = tr("Unknown Album", "Default album if no album");
    if (m_title.isEmpty())
        m_title = m_filename;
    if (m_genre.isEmpty())
        m_genre = tr("Unknown Genre", "Default genre if no genre");

}

inline void MusicMetadata::setCompilationFormatting(bool cd)
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


QString MusicMetadata::FormatArtist()
{
    if (m_formattedartist.isEmpty())
        setCompilationFormatting();

    return m_formattedartist;
}


QString MusicMetadata::FormatTitle()
{
    if (m_formattedtitle.isEmpty())
        setCompilationFormatting();

    return m_formattedtitle;
}

void MusicMetadata::setFilename(const QString& lfilename)
{
    m_filename = lfilename;
    m_actualFilename.clear();
}

QString MusicMetadata::Filename(bool find)
{
    // if not asked to find the file just return the raw filename from the DB
    if (find == false)
        return m_filename;

    if (!m_actualFilename.isEmpty())
        return m_actualFilename;

    // check for a cd track
    if (m_filename.endsWith(".cda"))
    {
        m_actualFilename = m_filename;
        return m_filename;
    }

    // check for http urls etc
    if (m_filename.contains("://"))
    {
        m_actualFilename = m_filename;
        return m_filename;
    }

    // first check to see if the filename is complete
    if (QFile::exists(m_filename))
    {
        m_actualFilename = m_filename;
        return m_filename;
    }

    // maybe it's in our 'Music' storage group
    QString mythUrl = RemoteFile::FindFile(m_filename, m_hostname, "Music");
    if (!mythUrl.isEmpty())
    {
        m_actualFilename = mythUrl;

        QUrl url(mythUrl);
        if (url.host() != m_hostname &&
            QHostAddress(url.host()).isNull()) // Check that it's not an IP address
        {
            m_hostname = url.host();
            saveHostname();
        }

        return mythUrl;
    }

    // not found
    LOG(VB_GENERAL, LOG_ERR, QString("MusicMetadata: Asked to get the filename for a track but no file found: %1")
                                     .arg(m_filename));

    m_actualFilename = METADATA_INVALID_FILENAME;

    return m_actualFilename;
}

/// try to find the track on the local file system
QString MusicMetadata::getLocalFilename(void)
{
    // try the file name as is first
    if (QFile::exists(m_filename))
        return m_filename;

    // not found so now try to find the file in the local 'Music' storage group
    StorageGroup storageGroup("Music", gCoreContext->GetHostName(), false);
    return storageGroup.FindFile(m_filename);
}

void MusicMetadata::setField(const QString &field, const QString &data)
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
    else if (field == "trackcount")
        m_trackCount = data.toInt();
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

void MusicMetadata::getField(const QString &field, QString *data)
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

void MusicMetadata::toMap(InfoMap &metadataMap, const QString &prefix)
{
    using namespace MythDate;
    metadataMap[prefix + "artist"] = m_artist;
    metadataMap[prefix + "formatartist"] = FormatArtist();
    metadataMap[prefix + "compilationartist"] = m_compilation_artist;

    if (m_album.isEmpty() && ID_TO_REPO(m_id) == RT_Radio)
        metadataMap[prefix + "album"] = QString("%1 - %2").arg(m_station).arg(m_channel);
    else
        metadataMap[prefix + "album"] = m_album;

    metadataMap[prefix + "title"] = m_title;
    metadataMap[prefix + "formattitle"] = FormatTitle();
    metadataMap[prefix + "tracknum"] = (m_tracknum > 0 ? QString("%1").arg(m_tracknum) : "");
    metadataMap[prefix + "trackcount"] = (m_trackCount > 0 ? QString("%1").arg(m_trackCount) : "");
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
        metadataMap[prefix + "lastplayed"] =
            MythDate::toString(m_lastplay, kDateFull | kSimplify | kAddYear);
    else
        metadataMap[prefix + "lastplayed"] = tr("Never Played");

    metadataMap[prefix + "dateadded"] = MythDate::toString(
        m_dateadded, kDateFull | kSimplify | kAddYear);

    metadataMap[prefix + "playcount"] = QString::number(m_playcount);

    QLocale locale = gCoreContext->GetQLocale();
    QString tmpSize = locale.toString(m_fileSize *
                                      (1.0 / (1024.0 * 1024.0)), 'f', 2);
    metadataMap[prefix + "filesize"] = tmpSize;

    metadataMap[prefix + "filename"] = m_filename;

    // radio stream
    metadataMap[prefix + "station"] = m_station;
    metadataMap[prefix + "channel"] = m_channel;
    metadataMap[prefix + "genre"] = m_genre;

    if (isRadio())
    {
        QUrl url(m_filename);
        metadataMap[prefix + "url"] = url.toString(QUrl::RemoveUserInfo);
    }
    else
        metadataMap[prefix + "url"] = m_filename;

    metadataMap[prefix + "logourl"] = m_logoUrl;
    metadataMap[prefix + "metadataformat"] = m_metaFormat;
}

void MusicMetadata::decRating()
{
    if (m_rating > 0)
    {
        m_rating--;
    }
    m_changed = true;
}

void MusicMetadata::incRating()
{
    if (m_rating < 10)
    {
        m_rating++;
    }
    m_changed = true;
}

void MusicMetadata::setLastPlay(QDateTime lastPlay)
{
    m_templastplay = MythDate::as_utc(lastPlay);
    m_changed = true;
}

void MusicMetadata::setLastPlay()
{
    m_templastplay = MythDate::current();
    m_changed = true;
}

void MusicMetadata::incPlayCount()
{
    m_tempplaycount = m_playcount + 1;
    m_changed = true;
}

void MusicMetadata::setEmbeddedAlbumArt(AlbumArtList &albumart)
{
    // add the images found in the tag to the ones we got from the DB

    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this, false);

    for (int x = 0; x < albumart.size(); x++)
    {
        AlbumArtImage *image = albumart.at(x);
        image->filename = QString("%1-%2").arg(m_id).arg(image->filename);
        m_albumArt->addImage(albumart.at(x));
    }

    m_changed = true;
}

QStringList MusicMetadata::fillFieldList(QString field)
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

QString MusicMetadata::getAlbumArtFile(void)
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
    if (!res.isEmpty() && albumart_image)
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
            // check for the image in the storage group
            QUrl url(res);

            if (url.path().isEmpty() || url.host().isEmpty() || url.userName().isEmpty())
            {
                return QString("");
            }

            if (!RemoteFile::Exists(res))
            {
                if (albumart_image->embedded)
                {
                    if (gCoreContext->IsMasterBackend() &&
                        url.host() == gCoreContext->GetMasterHostName())
                    {
                        QStringList paramList;
                        paramList.append(QString("--songid='%1'").arg(ID()));
                        paramList.append(QString("--imagetype='%1'").arg(albumart_image->imageType));

                        QString command = "mythutil --extractimage " + paramList.join(" ");

                        QScopedPointer<MythSystem> cmd(MythSystem::Create(command,
                                                    kMSAutoCleanup | kMSRunBackground |
                                                    kMSDontDisableDrawing | kMSProcessEvents |
                                                    kMSDontBlockInputDevs));
                    }
                    else
                    {
                        QStringList slist;
                        slist << "MUSIC_TAG_GETIMAGE"
                            << Hostname()
                            << QString::number(ID())
                            << QString::number(albumart_image->imageType);
                        gCoreContext->SendReceiveStringList(slist);
                    }
                }
            }
        }

        return res;
    }

    return QString("");
}

QString MusicMetadata::getAlbumArtFile(ImageType type)
{
    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this);

    AlbumArtImage *albumart_image = m_albumArt->getImage(type);
    if (albumart_image)
        return albumart_image->filename;

    return QString("");
}

AlbumArtImages *MusicMetadata::getAlbumArtImages(void)
{
    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this);

    return m_albumArt;
}

void MusicMetadata::reloadAlbumArtImages(void)
{
    delete m_albumArt;
    m_albumArt = NULL; //new AlbumArtImages(this);
}


// create a MetaIO for the file to read/write any tags etc
// NOTE the caller is responsible for deleting it
MetaIO* MusicMetadata::getTagger(void)
{
    // the taggers require direct file access so try to find
    // the file on the local filesystem

    QString filename = getLocalFilename();

    if (!filename.isEmpty())
    {
        LOG(VB_FILE, LOG_INFO, QString("MusicMetadata::getTagger - creating tagger for %1").arg(filename));
        return MetaIO::createTagger(filename);
    }

    LOG(VB_GENERAL, LOG_ERR, QString("MusicMetadata::getTagger - failed to find %1 on the local filesystem").arg(Filename(false)));
    return NULL;
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

AllMusic::AllMusic(void) :
    m_numPcs(0),
    m_numLoaded(0),
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

/// resync our cache with the database
void AllMusic::resync()
{
    uint added = 0, removed = 0, changed = 0;

    m_done_loading = false;

    QString aquery = "SELECT music_songs.song_id, music_artists.artist_id, music_artists.artist_name, "
                     "music_comp_artists.artist_name AS compilation_artist, "
                     "music_albums.album_id, music_albums.album_name, music_songs.name, music_genres.genre, music_songs.year, "
                     "music_songs.track, music_songs.length, music_songs.directory_id, "
                     "CONCAT_WS('/', music_directories.path, music_songs.filename) AS filename, "
                     "music_songs.rating, music_songs.numplays, music_songs.lastplay, music_songs.date_entered, "
                     "music_albums.compilation, music_songs.format, music_songs.track_count, "
                     "music_songs.size, music_songs.hostname "
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
    QList<MusicMetadata::IdType> idList;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            MusicMetadata::IdType id = query.value(0).toInt();

            idList.append(id);

            MusicMetadata *dbMeta = new MusicMetadata(
                query.value(12).toString(),    // filename
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

            dbMeta->setDirectoryId(query.value(11).toInt());
            dbMeta->setArtistId(query.value(1).toInt());
            dbMeta->setAlbumId(query.value(4).toInt());
            dbMeta->setTrackCount(query.value(19).toInt());
            dbMeta->setFileSize((quint64)query.value(20).toULongLong());
            dbMeta->setHostname(query.value(21).toString());

            if (!music_map.contains(id))
            {
                // new track

                //  Don't delete dbMeta, as the MetadataPtrList now owns it
                m_all_music.append(dbMeta);

                music_map[id] = dbMeta;

                added++;
            }
            else
            {
                // existing track, check for any changes
                MusicMetadata *cacheMeta = music_map[id];

                if (cacheMeta && !cacheMeta->compare(dbMeta))
                {
                    cacheMeta->reloadMetadata();
                    changed++;
                }

                // we already have this track in the cache so don't need dbMeta anymore
                delete dbMeta;
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
         LOG(VB_GENERAL, LOG_ERR, "MythMusic hasn't found any tracks!");
    }

    // get a list of tracks in our cache that's now not in the database
    QList<MusicMetadata::IdType> deleteList;
    for (int x = 0; x < m_all_music.size(); x++)
    {
        if (!idList.contains(m_all_music.at(x)->ID()))
        {
            deleteList.append(m_all_music.at(x)->ID());
        }
    }

    // remove the no longer available tracks
    for (int x = 0; x < deleteList.size(); x++)
    {
        MusicMetadata::IdType id = deleteList.at(x);
        MusicMetadata *mdata = music_map[id];
        m_all_music.removeAll(mdata);
        music_map.remove(id);
        removed++;
        delete mdata;
    }

    // tell any listeners a resync has just finished and they may need to reload/resync
    LOG(VB_GENERAL, LOG_DEBUG, QString("AllMusic::resync sending MUSIC_RESYNC_FINISHED added: %1, removed: %2, changed: %3")
                                      .arg(added).arg(removed).arg(changed));
    gCoreContext->SendMessage(QString("MUSIC_RESYNC_FINISHED %1 %2 %3").arg(added).arg(removed).arg(changed));

    m_done_loading = true;
}

MusicMetadata* AllMusic::getMetadata(int an_id)
{
    if (music_map.contains(an_id))
        return music_map[an_id];

    return NULL;
}

bool AllMusic::isValidID(int an_id)
{
    return music_map.contains(an_id);
}

bool AllMusic::updateMetadata(int an_id, MusicMetadata *the_track)
{
    if (an_id > 0)
    {
        MusicMetadata *mdata = getMetadata(an_id);
        if (mdata)
        {
            *mdata = *the_track;
            return true;
        }
    }
    return false;
}

/// \brief Check each MusicMetadata entry and save those that have changed (ratings, etc.)
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
        MusicMetadata *mdata = m_cdData.back();
        if (music_map.contains(mdata->ID()))
            music_map.remove(mdata->ID());

        delete m_cdData.back();
        m_cdData.pop_back();
    }

    m_cdTitle = tr("CD -- none");
}

void AllMusic::addCDTrack(const MusicMetadata &the_track)
{
    MusicMetadata *mdata = new MusicMetadata(the_track);
    mdata->setID(m_cdData.count() + 1);
    mdata->setRepo(RT_CD);
    m_cdData.append(mdata);
    music_map[mdata->ID()] = mdata;
}

bool AllMusic::checkCDTrack(MusicMetadata *the_track)
{
    if (m_cdData.count() < 1)
        return false;

    if (m_cdData.last()->FormatTitle() == the_track->FormatTitle())
        return true;

    return false;
}

MusicMetadata* AllMusic::getCDMetadata(int the_track)
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

AllStream::AllStream(void)
{
    loadStreams();
}

AllStream::~AllStream(void)
{
    while (!m_streamList.empty())
    {
        delete m_streamList.back();
        m_streamList.pop_back();
    }
}

bool AllStream::isValidID(MusicMetadata::IdType an_id)
{
    for (int x = 0; x < m_streamList.count(); x++)
    {
        if (m_streamList.at(x)->ID() == an_id)
            return true;
    }

    return false;
}

MusicMetadata *AllStream::getMetadata(MusicMetadata::IdType an_id)
{
    for (int x = 0; x < m_streamList.count(); x++)
    {
        if (m_streamList.at(x)->ID() == an_id)
            return m_streamList.at(x);
    }

    return NULL;
}

void AllStream::loadStreams(void)
{
    while (!m_streamList.empty())
    {
        delete m_streamList.back();
        m_streamList.pop_back();
    }

    QString aquery = "SELECT intid, station, channel, url, logourl, genre, metaformat, format "
                     "FROM music_radios "
                     "ORDER BY station,channel;";

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec(aquery))
        MythDB::DBError("AllStream::loadStreams", query);

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            MusicMetadata *mdata = new MusicMetadata(
                    query.value(0).toInt(),        // intid
                    query.value(1).toString(),     // station
                    query.value(2).toString(),     // channel
                    query.value(3).toString(),     // url
                    query.value(4).toString(),     // logourl
                    query.value(5).toString(),     // genre
                    query.value(6).toString(),     // metadataformat
                    query.value(7).toString());    // format

            mdata->setRepo(RT_Radio);

            m_streamList.append(mdata);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "MythMusic hasn't found any radio streams!");
    }
}

void AllStream::addStream(MusicMetadata* mdata)
{
    // add the stream to the db
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO music_radios (station, channel, url, logourl, genre, format, metaformat) " 
                  "VALUES (:STATION, :CHANNEL, :URL, :LOGOURL, :GENRE, :FORMAT, :METAFORMAT);");
    query.bindValue(":STATION", mdata->Station());
    query.bindValue(":CHANNEL", mdata->Channel());
    query.bindValue(":URL", mdata->Url());
    query.bindValue(":LOGOURL", mdata->LogoUrl());
    query.bindValue(":GENRE", mdata->Genre());
    query.bindValue(":FORMAT", mdata->Format());
    query.bindValue(":METAFORMAT", mdata->MetadataFormat());

    if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
    {
        MythDB::DBError("music insert radio", query);
        return;
    }

    mdata->setID(query.lastInsertId().toInt());
    mdata->setRepo(RT_Radio);

    loadStreams();
//    createPlaylist();
}

void AllStream::removeStream(MusicMetadata* mdata)
{
    // remove the stream from the db
    int id = ID_TO_ID(mdata->ID());
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_radios WHERE intid = :ID");
    query.bindValue(":ID", id);

    if (!query.exec() || query.numRowsAffected() <= 0)
    {
        MythDB::DBError("AllStream::removeStream", query);
        return;
    }

    loadStreams();
//    createPlaylist();
}

void AllStream::updateStream(MusicMetadata* mdata)
{
    // update the stream in the db
    int id = ID_TO_ID(mdata->ID());
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE music_radios set station = :STATION, channel = :CHANNEL, url = :URL, "
                  "logourl = :LOGOURL, genre = :GENRE, format = :FORMAT, metaformat = :METAFORMAT " 
                  "WHERE intid = :ID");
    query.bindValue(":STATION", mdata->Station());
    query.bindValue(":CHANNEL", mdata->Channel());
    query.bindValue(":URL", mdata->Url());
    query.bindValue(":LOGOURL", mdata->LogoUrl());
    query.bindValue(":GENRE", mdata->Genre());
    query.bindValue(":FORMAT", mdata->Format());
    query.bindValue(":METAFORMAT", mdata->MetadataFormat());
    query.bindValue(":ID", id);

    if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
    {
        MythDB::DBError("AllStream::updateStream", query);
        return;
    }

    loadStreams();
//    createPlaylist();
}

#if 0
void AllStream::createPlaylist(void)
{
    gMusicData->all_playlists->getStreamPlaylist()->disableSaves();

    gMusicData->all_playlists->getStreamPlaylist()->removeAllTracks();

    for (int x = 0; x < m_streamList.count(); x++)
    {
        MusicMetadata *mdata = m_streamList.at(x);
        gMusicData->all_playlists->getStreamPlaylist()->addTrack(mdata->ID(), false);
    }

    gMusicData->all_playlists->getStreamPlaylist()->enableSaves();
}
#endif

/**************************************************************************/

AlbumArtImages::AlbumArtImages(MusicMetadata *metadata, bool loadFromDB)
    : m_parent(metadata)
{
    if (loadFromDB)
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
        query.prepare("SELECT logourl FROM music_radios WHERE url = :URL;");
        query.bindValue(":URL", m_parent->Filename());
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
                image->hostname = "";
                m_imageList.push_back(image);
            }
        }
    }
    else
    {
        if (trackid == 0)
            return;

        QFileInfo fi(m_parent->Filename(false));
        QString dir = fi.path();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT albumart_id, CONCAT_WS('/', music_directories.path, "
                "music_albumart.filename), music_albumart.filename, music_albumart.imagetype, "
                "music_albumart.embedded, music_albumart.hostname "
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

                QUrl url(m_parent->Filename(true));

                if (embedded)
                {
                    if (url.scheme() == "myth")
                        image->filename = gCoreContext->GenMythURL(url.host(), url.port(),
                                                                   QString("AlbumArt/") + query.value(1).toString(),
                                                                   "MusicArt");
                    else
                        image->filename = query.value(1).toString();
                }
                else
                {
                    if (url.scheme() == "myth")
                        image->filename =  gCoreContext->GenMythURL(url.host(), url.port(),
                                                                    query.value(1).toString(),
                                                                    "Music");
                    else
                        image->filename = query.value(1).toString();
                }

                image->imageType = (ImageType) query.value(3).toInt();
                image->embedded = embedded;
                image->hostname = query.value(5).toString();

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

void AlbumArtImages::scanForImages()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUIBusyDialog *busy = new MythUIBusyDialog(tr("Scanning for music album art..."),
                                                  popupStack, "scanbusydialog");

    if (busy->Create())
    {
        popupStack->AddScreen(busy, false);
    }
    else
    {
        delete busy;
        busy = NULL;
    }

    QStringList strList;
    strList << "MUSIC_FIND_ALBUMART"
            << m_parent->Hostname()
            << QString::number(m_parent->ID())
            << "1";

    AlbumArtScannerThread *scanThread = new AlbumArtScannerThread(strList);
    scanThread->start();

    while (scanThread->isRunning())
    {
        qApp->processEvents();
        usleep(1000);
    }

    strList = scanThread->getResult();

    delete scanThread;

    if (busy)
        busy->Close();

    while (!m_imageList.empty())
    {
        delete m_imageList.back();
        m_imageList.pop_back();
    }

    for (int x = 2; x < strList.count(); x += 6)
    {
        AlbumArtImage *image = new AlbumArtImage;
        image->id = strList[x].toInt();
        image->imageType = (ImageType) strList[x + 1].toInt();
        image->embedded = (strList[x + 2].toInt() == 1);
        image->description = strList[x + 3];

        if (image->embedded)
        {
            image->filename = gCoreContext->GenMythURL(m_parent->Hostname(), 0,
                                                       QString("AlbumArt/") + strList[x + 4],
                                                       "MusicArt");
        }
        else
        {
            image->filename =  gCoreContext->GenMythURL(m_parent->Hostname(), 0,
                                                        strList[x + 4],
                                                        "Music");
        }

        image->hostname = strList[x + 5];

        LOG(VB_FILE, LOG_INFO, "AlbumArtImages::scanForImages found image");
        LOG(VB_FILE, LOG_INFO, QString("ID: %1").arg(image->id));
        LOG(VB_FILE, LOG_INFO, QString("ImageType: %1").arg(image->imageType));
        LOG(VB_FILE, LOG_INFO, QString("Embedded: %1").arg(image->embedded));
        LOG(VB_FILE, LOG_INFO, QString("Description: %1").arg(image->description));
        LOG(VB_FILE, LOG_INFO, QString("Filename: %1").arg(image->filename));
        LOG(VB_FILE, LOG_INFO, QString("Hostname: %1").arg(image->hostname));
        LOG(VB_FILE, LOG_INFO, "-------------------------------");

        addImage(image);

        delete image;
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

AlbumArtImage *AlbumArtImages::getImageByID(int imageID)
{
    AlbumArtList::iterator it = m_imageList.begin();
    for (; it != m_imageList.end(); ++it)
    {
        if ((*it)->id == imageID)
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

    return QCoreApplication::translate("AlbumArtImages",
                                       type_strings[type]);
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

    return QCoreApplication::translate("AlbumArtImages",
                                       filename_strings[type]);
}

// static method to guess the image type from the filename
ImageType AlbumArtImages::guessImageType(const QString &filename)
{
    ImageType type = IT_FRONTCOVER;

    if (filename.contains("front", Qt::CaseInsensitive) ||
             filename.contains(tr("front"), Qt::CaseInsensitive))
        type = IT_FRONTCOVER;
    else if (filename.contains("back", Qt::CaseInsensitive) ||
             filename.contains(tr("back"),  Qt::CaseInsensitive))
        type = IT_BACKCOVER;
    else if (filename.contains("inlay", Qt::CaseInsensitive) ||
             filename.contains(tr("inlay"), Qt::CaseInsensitive))
        type = IT_INLAY;
    else if (filename.contains("cd", Qt::CaseInsensitive) ||
             filename.contains(tr("cd"), Qt::CaseInsensitive))
        type = IT_CD;
    else if (filename.contains("cover", Qt::CaseInsensitive) ||
             filename.contains(tr("cover"), Qt::CaseInsensitive))
        type = IT_FRONTCOVER;

    return type;
}

// static method to get image type from the type name
ImageType AlbumArtImages::getImageTypeFromName(const QString &name)
{
    ImageType type = IT_UNKNOWN;

    if (name.toLower() == "front")
        type = IT_FRONTCOVER;
    else if (name.toLower() == "back")
        type = IT_BACKCOVER;
    else if (name.toLower() == "inlay")
        type = IT_INLAY;
    else if (name.toLower() == "cd")
        type = IT_CD;
    else if (name.toLower() == "artist")
        type = IT_ARTIST;
    else if (name.toLower() == "unknown")
        type = IT_UNKNOWN;

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
        image->hostname = newImage.hostname;
    }
}

/// saves or updates the image details in the DB
void AlbumArtImages::dumpToDatabase(void)
{
    MusicMetadata::IdType trackID = ID_TO_ID(m_parent->ID());
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

    if (!query.exec())
    {
        MythDB::DBError("AlbumArtImages::dumpToDatabase - "
                            "deleting existing albumart", query);
    }

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
                          "filename, imagetype, song_id, directory_id, embedded, hostname ) "
                          "VALUES ( :ID, :FILENAME, :TYPE, :SONGID, :DIRECTORYID, :EMBED, :HOSTNAME );");
            query.bindValue(":ID", image->id);
        }
        else
        {
            query.prepare("INSERT INTO music_albumart ( filename, "
                        "imagetype, song_id, directory_id, embedded, hostname ) VALUES ( "
                        ":FILENAME, :TYPE, :SONGID, :DIRECTORYID, :EMBED, :HOSTNAME );");
        }

        QFileInfo fi(image->filename);
        query.bindValue(":FILENAME", fi.fileName());

        query.bindValue(":TYPE", image->imageType);
        query.bindValue(":SONGID", image->embedded ? trackID : 0);
        query.bindValue(":DIRECTORYID", image->embedded ? 0 : directoryID);
        query.bindValue(":EMBED", image->embedded);
        query.bindValue(":HOSTNAME", image->hostname);

        if (!query.exec())
            MythDB::DBError("AlbumArtImages::dumpToDatabase - "
                            "add/update music_albumart", query);
        else
        {
            if (image->id <= 0)
                image->id = query.lastInsertId().toInt();
        }
    }
}
