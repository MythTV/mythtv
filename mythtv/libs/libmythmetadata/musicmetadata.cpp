
#include "musicmetadata.h"

// qt
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QDomDocument>
#include <QScopedPointer>
#include <utility>

// mythtv
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsorthelper.h"
#include "libmythbase/mythsystem.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/storagegroup.h"
#include "libmythbase/unziputil.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythprogressdialog.h"

// libmythmetadata
#include "metaio.h"
#include "metaioid3.h"
#include "metaiomp4.h"
#include "metaioavfcomment.h"
#include "metaiooggvorbis.h"
#include "metaioflacvorbis.h"
#include "metaiowavpack.h"
#include "musicutils.h"
#include "lyricsdata.h"

static QString thePrefix = "the ";

bool operator==(MusicMetadata& a, MusicMetadata& b)
{
    return a.Filename() == b.Filename();
}

bool operator!=(MusicMetadata& a, MusicMetadata& b)
{
    return a.Filename() != b.Filename();
}

// this ctor is for radio streams
MusicMetadata::MusicMetadata(int lid, QString lbroadcaster, QString lchannel, QString ldescription,
                             const UrlList &lurls, QString llogourl, QString lgenre, QString lmetaformat,
                             QString lcountry, QString llanguage, QString lformat)
         :  m_genre(std::move(lgenre)),
            m_format(std::move(lformat)),
            m_id(lid),
            m_filename(lurls[0]),
            m_broadcaster(std::move(lbroadcaster)),
            m_channel(std::move(lchannel)),
            m_description(std::move(ldescription)),
            m_urls(lurls),
            m_logoUrl(std::move(llogourl)),
            m_metaFormat(std::move(lmetaformat)),
            m_country(std::move(lcountry)),
            m_language(std::move(llanguage))
{
    setRepo(RT_Radio);
    ensureSortFields();
}

MusicMetadata::~MusicMetadata()
{
    if (m_albumArt)
    {
        delete m_albumArt;
        m_albumArt = nullptr;
    }

    if (m_lyricsData)
    {
        delete m_lyricsData;
        m_lyricsData = nullptr;
    }
}


MusicMetadata& MusicMetadata::operator=(const MusicMetadata &rhs)
{
    if (this == &rhs)
        return *this;

    m_artist = rhs.m_artist;
    m_artistSort = rhs.m_artistSort;
    m_compilationArtist = rhs.m_compilationArtist;
    m_compilationArtistSort = rhs.m_compilationArtistSort;
    m_album = rhs.m_album;
    m_albumSort = rhs.m_albumSort;
    m_title = rhs.m_title;
    m_titleSort = rhs.m_titleSort;
    m_formattedArtist = rhs.m_formattedArtist;
    m_formattedTitle = rhs.m_formattedTitle;
    m_genre = rhs.m_genre;
    m_year = rhs.m_year;
    m_trackNum = rhs.m_trackNum;
    m_trackCount = rhs.m_trackCount;
    m_discNum = rhs.m_discNum;
    m_discCount = rhs.m_discCount;
    m_length = rhs.m_length;
    m_rating = rhs.m_rating;
    m_lastPlay = rhs.m_lastPlay;
    m_tempLastPlay = rhs.m_tempLastPlay;
    m_dateAdded = rhs.m_dateAdded;
    m_playCount = rhs.m_playCount;
    m_tempPlayCount = rhs.m_tempPlayCount;
    m_compilation = rhs.m_compilation;
    m_id = rhs.m_id;
    m_filename = rhs.m_filename;
    m_actualFilename = rhs.m_actualFilename;
    m_hostname = rhs.m_hostname;
    m_directoryId = rhs.m_directoryId;
    m_artistId = rhs.m_artistId;
    m_compartistId = rhs.m_compartistId;
    m_albumId = rhs.m_albumId;
    m_genreId = rhs.m_genreId;
    if (rhs.m_albumArt)
    {
        m_albumArt = new AlbumArtImages(this, *rhs.m_albumArt);
    }
    m_lyricsData = nullptr;
    m_format = rhs.m_format;
    m_changed = rhs.m_changed;
    m_fileSize = rhs.m_fileSize;
    m_broadcaster = rhs.m_broadcaster;
    m_channel = rhs.m_channel;
    m_description = rhs.m_description;

    m_urls = rhs.m_urls;
    m_logoUrl = rhs.m_logoUrl;
    m_metaFormat = rhs.m_metaFormat;
    m_country = rhs.m_country;
    m_language = rhs.m_language;

    return *this;
}

// return true if this == mdata
bool MusicMetadata::compare(MusicMetadata *mdata) const
{
    return (
        m_artist == mdata->m_artist &&
        m_compilationArtist == mdata->m_compilationArtist &&
        m_album == mdata->m_album &&
        m_title == mdata->m_title &&
        m_year == mdata->m_year &&
        m_trackNum == mdata->m_trackNum &&
        m_trackCount == mdata->m_trackCount &&
        m_discNum == mdata->m_discNum &&
        m_discCount == mdata->m_discCount &&
        //m_length == mdata->m_length &&
        m_rating == mdata->m_rating &&
        m_lastPlay == mdata->m_lastPlay &&
        m_playCount == mdata->m_playCount &&
        m_compilation == mdata->m_compilation &&
        m_filename == mdata->m_filename &&
        m_directoryId == mdata->m_directoryId &&
        m_artistId == mdata->m_artistId &&
        m_compartistId == mdata->m_compartistId &&
        m_albumId == mdata->m_albumId &&
        m_genreId == mdata->m_genreId &&
        m_format == mdata->m_format &&
        m_fileSize == mdata->m_fileSize
    );
}

void MusicMetadata::persist()
{
    if (m_id < 1)
        return;

    if (m_tempLastPlay.isValid())
    {
        m_lastPlay = m_tempLastPlay;
        m_playCount = m_tempPlayCount;

        m_tempLastPlay = QDateTime();
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE music_songs set rating = :RATING , "
                  "numplays = :PLAYCOUNT , lastplay = :LASTPLAY "
                  "where song_id = :ID ;");
    query.bindValue(":RATING", m_rating);
    query.bindValue(":PLAYCOUNT", m_playCount);
    query.bindValue(":LASTPLAY", m_lastPlay);
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
        return nullptr;
    }

    if (!query.next())
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("MusicMetadata::createFromFilename: Could not find '%1'")
                .arg(filename));
        return nullptr;
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
    "music_songs.disc_number, music_songs.disc_count, "
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
        auto *mdata = new MusicMetadata();
        mdata->m_artist = query.value(0).toString();
        mdata->m_compilationArtist = query.value(1).toString();
        mdata->m_album = query.value(2).toString();
        mdata->m_title = query.value(3).toString();
        mdata->m_genre = query.value(4).toString();
        mdata->m_year = query.value(5).toInt();
        mdata->m_trackNum = query.value(6).toInt();
        mdata->m_length = std::chrono::milliseconds(query.value(7).toInt());
        mdata->m_id = query.value(8).toUInt();
        mdata->m_rating = query.value(9).toInt();
        mdata->m_playCount = query.value(10).toInt();
        mdata->m_lastPlay = query.value(11).toDateTime();
        mdata->m_compilation = (query.value(12).toInt() > 0);
        mdata->m_format = query.value(13).toString();
        mdata->m_trackCount = query.value(14).toInt();
        mdata->m_fileSize = query.value(15).toULongLong();
        mdata->m_dateAdded = query.value(16).toDateTime();
        mdata->m_discNum = query.value(17).toInt();
        mdata->m_discCount = query.value(18).toInt();
        mdata->m_filename = query.value(19).toString();
        mdata->m_hostname = query.value(20).toString();
        mdata->ensureSortFields();

        if (!QHostAddress(mdata->m_hostname).isNull()) // A bug caused an IP to replace hostname, reset and it will fix itself
        {
            mdata->m_hostname = "";
            mdata->saveHostname();
        }

        return mdata;
    }

    return nullptr;
}

// static
bool MusicMetadata::updateStreamList(void)
{
    // we are only interested in the global setting so remove any local host setting just in case
    GetMythDB()->ClearSetting("MusicStreamListModified");

    // make sure we are not already doing an update
    if (gCoreContext->GetSetting("MusicStreamListModified") == "Updating")
    {
        LOG(VB_GENERAL, LOG_ERR, "MusicMetadata: looks like we are already updating the radio streams list");
        return false;
    }

    QByteArray compressedData;
    QByteArray uncompressedData;

    // check if the streamlist has been updated since we last checked
    QDateTime lastModified = GetMythDownloadManager()->GetLastModified(QString(STREAMUPDATEURL));

    QDateTime lastUpdate = QDateTime::fromString(gCoreContext->GetSetting("MusicStreamListModified"), Qt::ISODate);

    if (lastModified <= lastUpdate)
    {
        LOG(VB_GENERAL, LOG_INFO, "MusicMetadata: radio streams list is already up to date");
        return true;
    }

    gCoreContext->SaveSettingOnHost("MusicStreamListModified", "Updating", nullptr);

    LOG(VB_GENERAL, LOG_INFO, "MusicMetadata: downloading radio streams list");

    // download compressed stream file
    if (!GetMythDownloadManager()->download(QString(STREAMUPDATEURL), &compressedData, false))
    {
        LOG(VB_GENERAL, LOG_ERR, "MusicMetadata: failed to download radio stream list");
        gCoreContext->SaveSettingOnHost("MusicStreamListModified", "", nullptr);
        return false;
    }

    // uncompress the data
    uncompressedData = gzipUncompress(compressedData);

    // load the xml
    QDomDocument domDoc;

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!domDoc.setContent(uncompressedData, false, &errorMsg,
                           &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MusicMetadata: Could not read content of streams.xml" +
                QString("\n\t\t\tError parsing %1").arg(STREAMUPDATEURL) +
                QString("\n\t\t\tat line: %1  column: %2 msg: %3")
                .arg(errorLine).arg(errorColumn).arg(errorMsg));
        gCoreContext->SaveSettingOnHost("MusicStreamListModified", "", nullptr);
        return false;
    }
#else
    auto parseResult = domDoc.setContent(uncompressedData);
    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MusicMetadata: Could not read content of streams.xml" +
                QString("\n\t\t\tError parsing %1").arg(STREAMUPDATEURL) +
                QString("\n\t\t\tat line: %1  column: %2 msg: %3")
                .arg(parseResult.errorLine).arg(parseResult.errorColumn)
                .arg(parseResult.errorMessage));
        gCoreContext->SaveSettingOnHost("MusicStreamListModified", "", nullptr);
        return false;
    }
#endif

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_streams;");
    if (!query.exec() || !query.isActive() || query.numRowsAffected() < 0)
    {
        MythDB::DBError("music delete radio streams", query);
        gCoreContext->SaveSettingOnHost("MusicStreamListModified", "", nullptr);
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, "MusicMetadata: processing radio streams list");

    QDomNodeList itemList = domDoc.elementsByTagName("item");

    QDomNode itemNode;
    for (int i = 0; i < itemList.count(); i++)
    {
        itemNode = itemList.item(i);

        query.prepare("INSERT INTO music_streams (broadcaster, channel, description, url1, url2, url3, url4, url5,"
                      "  logourl, genre, metaformat, country, language) "
                      "VALUES (:BROADCASTER, :CHANNEL, :DESC, :URL1, :URL2, :URL3, :URL4, :URL5,"
                      " :LOGOURL, :GENRE, :META, :COUNTRY, :LANG);");

        query.bindValue(":BROADCASTER", itemNode.namedItem(QString("broadcaster")).toElement().text());
        query.bindValue(":CHANNEL",     itemNode.namedItem(QString("channel")).toElement().text());
        query.bindValue(":DESC",        itemNode.namedItem(QString("description")).toElement().text());
        query.bindValue(":URL1",        itemNode.namedItem(QString("url1")).toElement().text());
        query.bindValue(":URL2",        itemNode.namedItem(QString("url2")).toElement().text());
        query.bindValue(":URL3",        itemNode.namedItem(QString("url3")).toElement().text());
        query.bindValue(":URL4",        itemNode.namedItem(QString("url4")).toElement().text());
        query.bindValue(":URL5",        itemNode.namedItem(QString("url5")).toElement().text());
        query.bindValue(":LOGOURL",     itemNode.namedItem(QString("logourl")).toElement().text());
        query.bindValue(":GENRE",       itemNode.namedItem(QString("genre")).toElement().text());
        query.bindValue(":META",        itemNode.namedItem(QString("metadataformat")).toElement().text());
        query.bindValue(":COUNTRY",     itemNode.namedItem(QString("country")).toElement().text());
        query.bindValue(":LANG",        itemNode.namedItem(QString("language")).toElement().text());

        if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
        {
            MythDB::DBError("music insert radio stream", query);
            gCoreContext->SaveSettingOnHost("MusicStreamListModified", "", nullptr);
            return false;
        }
    }

    gCoreContext->SaveSettingOnHost("MusicStreamListModified", lastModified.toString(Qt::ISODate), nullptr);

    LOG(VB_GENERAL, LOG_INFO, "MusicMetadata: updating radio streams list completed OK");

    return true;
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

    m_directoryId = -1;
    m_artistId = -1;
    m_compartistId = -1;
    m_albumId = -1;
    m_genreId = -1;
}

int MusicMetadata::getDirectoryId()
{
    if (m_directoryId < 0)
    {
        QString sqldir = m_filename.section('/', 0, -2);

        checkEmptyFields();

        MSqlQuery query(MSqlQuery::InitCon());

        if (sqldir.isEmpty())
        {
            m_directoryId = 0;
        }
        else if (m_directoryId < 0)
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
                m_directoryId = query.value(0).toInt();
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
                m_directoryId = query.lastInsertId().toInt();
            }
        }
    }

    return m_directoryId;
}

int MusicMetadata::getArtistId()
{
    if (m_artistId < 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        // Load the artist id
        query.prepare("SELECT artist_id FROM music_artists "
                    "WHERE artist_name = :ARTIST ;");
        query.bindValueNoNull(":ARTIST", m_artist);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select artist id", query);
            return -1;
        }
        if (query.next())
        {
            m_artistId = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_artists (artist_name) VALUES (:ARTIST);");
            query.bindValueNoNull(":ARTIST", m_artist);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert artist", query);
                return -1;
            }
            m_artistId = query.lastInsertId().toInt();
        }
    }

    return m_artistId;
}

int MusicMetadata::getCompilationArtistId()
{
    if (m_compartistId < 0) {
        MSqlQuery query(MSqlQuery::InitCon());

        // Compilation Artist
        if (m_artist == m_compilationArtist)
        {
            m_compartistId = getArtistId();
        }
        else
        {
            query.prepare("SELECT artist_id FROM music_artists "
                        "WHERE artist_name = :ARTIST ;");
            query.bindValueNoNull(":ARTIST", m_compilationArtist);
            if (!query.exec() || !query.isActive())
            {
                MythDB::DBError("music select compilation artist id", query);
                return -1;
            }
            if (query.next())
            {
                m_compartistId = query.value(0).toInt();
            }
            else
            {
                query.prepare("INSERT INTO music_artists (artist_name) VALUES (:ARTIST);");
                query.bindValueNoNull(":ARTIST", m_compilationArtist);

                if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
                {
                    MythDB::DBError("music insert compilation artist", query);
                    return -1 ;
                }
                m_compartistId = query.lastInsertId().toInt();
            }
        }
    }

    return m_compartistId;
}

int MusicMetadata::getAlbumId()
{
    if (m_albumId < 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT album_id FROM music_albums "
                    "WHERE artist_id = :COMP_ARTIST_ID "
                    " AND album_name = :ALBUM ;");
        query.bindValueNoNull(":COMP_ARTIST_ID", m_compartistId);
        query.bindValueNoNull(":ALBUM", m_album);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select album id", query);
            return -1;
        }
        if (query.next())
        {
            m_albumId = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_albums (artist_id, album_name, compilation, year) "
                          "VALUES (:COMP_ARTIST_ID, :ALBUM, :COMPILATION, :YEAR);");
            query.bindValueNoNull(":COMP_ARTIST_ID", m_compartistId);
            query.bindValueNoNull(":ALBUM", m_album);
            query.bindValue(":COMPILATION", m_compilation);
            query.bindValue(":YEAR", m_year);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert album", query);
                return -1;
            }
            m_albumId = query.lastInsertId().toInt();
        }
    }

    return m_albumId;
}

int MusicMetadata::getGenreId()
{
    if (m_genreId < 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT genre_id FROM music_genres "
                    "WHERE genre = :GENRE ;");
        query.bindValueNoNull(":GENRE", m_genre);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("music select genre id", query);
            return -1;
        }
        if (query.next())
        {
            m_genreId = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_genres (genre) VALUES (:GENRE);");
            query.bindValueNoNull(":GENRE", m_genre);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythDB::DBError("music insert genre", query);
                return -1;
            }
            m_genreId = query.lastInsertId().toInt();
        }
    }

    return m_genreId;
}

void MusicMetadata::setUrl(const QString& url, size_t index)
{
    if (index < STREAMURLCOUNT)
        m_urls[index] = url;
}

QString MusicMetadata::Url(size_t index)
{
    if (index < STREAMURLCOUNT)
        return m_urls[index];

    return {};
}

void MusicMetadata::dumpToDatabase()
{
    checkEmptyFields();

    if (m_directoryId < 0)
        getDirectoryId();

    if (m_artistId < 0)
        getArtistId();

    if (m_compartistId < 0)
        getCompilationArtistId();

    if (m_albumId < 0)
        getAlbumId();

    if (m_genreId < 0)
        getGenreId();

    // We have all the id's now. We can insert it.
    QString strQuery;
    if (m_id < 1)
    {
        strQuery = "INSERT INTO music_songs ( directory_id,"
                   " artist_id, album_id,    name,         genre_id,"
                   " year,      track,       length,       filename,"
                   " rating,    format,      date_entered, date_modified,"
                   " numplays,  track_count, disc_number,  disc_count,"
                   " size,      hostname) "
                   "VALUES ( "
                   " :DIRECTORY, "
                   " :ARTIST,   :ALBUM,      :TITLE,       :GENRE,"
                   " :YEAR,     :TRACKNUM,   :LENGTH,      :FILENAME,"
                   " :RATING,   :FORMAT,     :DATE_ADD,    :DATE_MOD,"
                   " :PLAYCOUNT,:TRACKCOUNT, :DISC_NUMBER, :DISC_COUNT,"
                   " :SIZE,     :HOSTNAME );";
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
                   ", disc_number = :DISC_NUMBER "
                   ", disc_count = :DISC_COUNT "
                   ", size = :SIZE "
                   ", hostname = :HOSTNAME "
                   "WHERE song_id= :ID ;";
    }

    QString sqlfilename = m_filename.section('/', -1);

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(strQuery);

    query.bindValue(":DIRECTORY", m_directoryId);
    query.bindValue(":ARTIST", m_artistId);
    query.bindValue(":ALBUM", m_albumId);
    query.bindValue(":TITLE", m_title);
    query.bindValue(":GENRE", m_genreId);
    query.bindValue(":YEAR", m_year);
    query.bindValue(":TRACKNUM", m_trackNum);
    query.bindValue(":LENGTH", static_cast<qint64>(m_length.count()));
    query.bindValue(":FILENAME", sqlfilename);
    query.bindValue(":RATING", m_rating);
    query.bindValueNoNull(":FORMAT", m_format);
    query.bindValue(":DATE_MOD", MythDate::current());
    query.bindValue(":PLAYCOUNT", m_playCount);

    if (m_id < 1)
        query.bindValue(":DATE_ADD",  MythDate::current());
    else
        query.bindValue(":ID", m_id);

    query.bindValue(":TRACKCOUNT", m_trackCount);
    query.bindValue(":DISC_NUMBER", m_discNum);
    query.bindValue(":DISC_COUNT",m_discCount);
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

    // update the album
    query.prepare("UPDATE music_albums SET album_name = :ALBUM_NAME, "
                  "artist_id = :COMP_ARTIST_ID, compilation = :COMPILATION, "
                  "year = :YEAR "
                  "WHERE music_albums.album_id = :ALBUMID");
    query.bindValue(":ALBUMID", m_albumId);
    query.bindValue(":ALBUM_NAME", m_album);
    query.bindValue(":COMP_ARTIST_ID", m_compartistId);
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
QString MusicMetadata::s_formatNormalFileArtist      = "ARTIST";
QString MusicMetadata::s_formatNormalFileTrack       = "TITLE";
QString MusicMetadata::s_formatNormalCdArtist        = "ARTIST";
QString MusicMetadata::s_formatNormalCdTrack         = "TITLE";
QString MusicMetadata::s_formatCompilationFileArtist = "COMPARTIST";
QString MusicMetadata::s_formatCompilationFileTrack  = "TITLE (ARTIST)";
QString MusicMetadata::s_formatCompilationCdArtist   = "COMPARTIST";
QString MusicMetadata::s_formatCompilationCdTrack    = "TITLE (ARTIST)";

void MusicMetadata::setArtistAndTrackFormats()
{
    QString tmp;

    tmp = gCoreContext->GetSetting("MusicFormatNormalFileArtist");
    if (!tmp.isEmpty())
        s_formatNormalFileArtist = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatNormalFileTrack");
    if (!tmp.isEmpty())
        s_formatNormalFileTrack = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatNormalCDArtist");
    if (!tmp.isEmpty())
        s_formatNormalCdArtist = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatNormalCDTrack");
    if (!tmp.isEmpty())
        s_formatNormalCdTrack = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatCompilationFileArtist");
    if (!tmp.isEmpty())
        s_formatCompilationFileArtist = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatCompilationFileTrack");
    if (!tmp.isEmpty())
        s_formatCompilationFileTrack = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatCompilationCDArtist");
    if (!tmp.isEmpty())
        s_formatCompilationCdArtist = tmp;

    tmp = gCoreContext->GetSetting("MusicFormatCompilationCDTrack");
    if (!tmp.isEmpty())
        s_formatCompilationCdTrack = tmp;
}


bool MusicMetadata::determineIfCompilation(bool cd)
{
    m_compilation = (!m_compilationArtist.isEmpty()
                   && m_artist != m_compilationArtist);
    setCompilationFormatting(cd);
    return m_compilation;
}


inline QString MusicMetadata::formatReplaceSymbols(const QString &format)
{
  QString rv = format;
  rv.replace("COMPARTIST", m_compilationArtist);
  rv.replace("ARTIST", m_artist);
  rv.replace("TITLE", m_title);
  rv.replace("TRACK", QString("%1").arg(m_trackNum, 2));
  return rv;
}

void MusicMetadata::checkEmptyFields()
{
    if (m_artist.isEmpty())
    {
        m_artist = tr("Unknown Artist", "Default artist if no artist");
        m_artistId = -1;
    }
    // This should be the same as Artist if it's a compilation track or blank
    if (!m_compilation || m_compilationArtist.isEmpty())
    {
        m_compilationArtist = m_artist;
        m_compartistId = -1;
    }
    if (m_album.isEmpty())
    {
        m_album = tr("Unknown Album", "Default album if no album");
        m_albumId = -1;
    }
    if (m_title.isEmpty())
        m_title = m_filename;
    if (m_genre.isEmpty())
    {
        m_genre = tr("Unknown Genre", "Default genre if no genre");
        m_genreId = -1;
    }
    ensureSortFields();
}

void MusicMetadata::ensureSortFields()
{
    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();

    if (m_artistSort.isEmpty() and not m_artist.isEmpty())
        m_artistSort = sh->doTitle(m_artist);
    if (m_compilationArtistSort.isEmpty() and not m_compilationArtist.isEmpty())
        m_compilationArtistSort = sh->doTitle(m_compilationArtist);
    if (m_albumSort.isEmpty() and not m_album.isEmpty())
        m_albumSort = sh->doTitle(m_album);
    if (m_titleSort.isEmpty() and not m_title.isEmpty())
        m_titleSort = sh->doTitle(m_title);
}

inline void MusicMetadata::setCompilationFormatting(bool cd)
{
    QString format_artist;
    QString format_title;

    if (!m_compilation
        || "" == m_compilationArtist
        || m_artist == m_compilationArtist)
    {
        if (!cd)
        {
          format_artist = s_formatNormalFileArtist;
          format_title  = s_formatNormalFileTrack;
        }
        else
        {
          format_artist = s_formatNormalCdArtist;
          format_title  = s_formatNormalCdTrack;
        }
    }
    else
    {
        if (!cd)
        {
          format_artist = s_formatCompilationFileArtist;
          format_title  = s_formatCompilationFileTrack;
        }
        else
        {
          format_artist = s_formatCompilationCdArtist;
          format_title  = s_formatCompilationCdTrack;
        }
    }

    // NB Could do some comparisons here to save memory with shallow copies...
    m_formattedArtist = formatReplaceSymbols(format_artist);
    m_formattedTitle = formatReplaceSymbols(format_title);
}


QString MusicMetadata::FormatArtist()
{
    if (m_formattedArtist.isEmpty())
        setCompilationFormatting();

    return m_formattedArtist;
}


QString MusicMetadata::FormatTitle()
{
    if (m_formattedTitle.isEmpty())
        setCompilationFormatting();

    return m_formattedTitle;
}

void MusicMetadata::setFilename(const QString& lfilename)
{
    m_filename = lfilename;
    m_actualFilename.clear();
}

QString MusicMetadata::Filename(bool find)
{
    // FIXME: for now just use the first url for radio streams
    if (isRadio())
        return m_urls[0];

    // if not asked to find the file just return the raw filename from the DB
    if (!find)
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
      m_compilationArtist = data;
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
        m_trackNum = data.toInt();
    else if (field == "trackcount")
        m_trackCount = data.toInt();
    else if (field == "discnum")
        m_discNum = data.toInt();
    else if (field == "disccount")
        m_discCount = data.toInt();
    else if (field == "length")
        m_length = std::chrono::milliseconds(data.toInt());
    else if (field == "compilation")
        m_compilation = (data.toInt() > 0);
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Something asked me to set data "
                                         "for a field called %1").arg(field));
    }
    ensureSortFields();
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
    metadataMap[prefix + "songid"] = QString::number(m_id);
    metadataMap[prefix + "artist"] = m_artist;
    metadataMap[prefix + "formatartist"] = FormatArtist();
    metadataMap[prefix + "compilationartist"] = m_compilationArtist;

    if (m_album.isEmpty() && ID_TO_REPO(m_id) == RT_Radio)
    {
        if (m_broadcaster.isEmpty())
            metadataMap[prefix + "album"] = m_channel;
        else
            metadataMap[prefix + "album"] = QString("%1 - %2").arg(m_broadcaster, m_channel);
    }
    else
    {
        metadataMap[prefix + "album"] = m_album;
    }

    metadataMap[prefix + "title"] = m_title;
    metadataMap[prefix + "formattitle"] = FormatTitle();
    metadataMap[prefix + "tracknum"] = (m_trackNum > 0 ? QString("%1").arg(m_trackNum) : "");
    metadataMap[prefix + "trackcount"] = (m_trackCount > 0 ? QString("%1").arg(m_trackCount) : "");
    metadataMap[prefix + "discnum"] = (m_discNum > 0 ? QString("%1").arg(m_discNum) : "");
    metadataMap[prefix + "disccount"] = (m_discCount > 0 ? QString("%1").arg(m_discCount) : "");
    metadataMap[prefix + "genre"] = m_genre;
    metadataMap[prefix + "year"] = (m_year > 0 ? QString("%1").arg(m_year) : "");

    QString fmt = (m_length >= 1h) ? "H:mm:ss" : "mm:ss";
    metadataMap[prefix + "length"] = MythDate::formatTime(m_length, fmt);

    if (m_lastPlay.isValid())
    {
        metadataMap[prefix + "lastplayed"] =
            MythDate::toString(m_lastPlay, kDateFull | kSimplify | kAddYear);
    }
    else
    {
        metadataMap[prefix + "lastplayed"] = tr("Never Played");
    }

    metadataMap[prefix + "dateadded"] = MythDate::toString(
        m_dateAdded, kDateFull | kSimplify | kAddYear);

    metadataMap[prefix + "playcount"] = QString::number(m_playCount);

    QLocale locale = gCoreContext->GetQLocale();
    QString tmpSize = locale.toString(m_fileSize *
                                      (1.0 / (1024.0 * 1024.0)), 'f', 2);
    metadataMap[prefix + "filesize"] = tmpSize;

    metadataMap[prefix + "filename"] = m_filename;

    // radio stream
    if (!m_broadcaster.isEmpty())
        metadataMap[prefix + "broadcasterchannel"] = m_broadcaster + " - " + m_channel;
    else
        metadataMap[prefix + "broadcasterchannel"] = m_channel;
    metadataMap[prefix + "broadcaster"] = m_broadcaster;
    metadataMap[prefix + "channel"] = m_channel;
    metadataMap[prefix + "genre"] = m_genre;
    metadataMap[prefix + "country"] = m_country;
    metadataMap[prefix + "language"] = m_language;
    metadataMap[prefix + "description"] = m_description;

    if (isRadio())
    {
        QUrl url(m_urls[0]);
        metadataMap[prefix + "url"] = url.toString(QUrl::RemoveUserInfo);
    }
    else
    {
        metadataMap[prefix + "url"] = m_filename;
    }

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

void MusicMetadata::setLastPlay(const QDateTime& lastPlay)
{
    m_tempLastPlay = MythDate::as_utc(lastPlay);
    m_changed = true;
}

void MusicMetadata::setLastPlay()
{
    m_tempLastPlay = MythDate::current();
    m_changed = true;
}

void MusicMetadata::incPlayCount()
{
    m_tempPlayCount = m_playCount + 1;
    m_changed = true;
}

void MusicMetadata::setEmbeddedAlbumArt(AlbumArtList &albumart)
{
    // add the images found in the tag to the ones we got from the DB

    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this, false);

    for (auto *art : std::as_const(albumart))
    {
        art->m_filename = QString("%1-%2").arg(m_id).arg(art->m_filename);
        m_albumArt->addImage(art);
    }

    m_changed = true;
}

QStringList MusicMetadata::fillFieldList(const QString& field)
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

    AlbumArtImage *albumart_image = nullptr;
    QString res;

    for (ImageType Type : {IT_FRONTCOVER, IT_UNKNOWN, IT_BACKCOVER, IT_INLAY, IT_CD})
    {
        albumart_image = m_albumArt->getImage(Type);
        if (albumart_image == nullptr)
            continue;
        res = albumart_image->m_filename;
        break;
    }

    // check file exists
    if (!res.isEmpty() && albumart_image)
    {
        int repo = ID_TO_REPO(m_id);
        if (repo == RT_Radio)
        {
            // image is a radio station icon, check if we have already downloaded and cached it
            QString path = GetConfDir() + "/MythMusic/AlbumArt/";
            QFileInfo fi(res);
            QString filename = QString("%1-%2.%3").arg(m_id).arg("front", fi.suffix());

            albumart_image->m_filename = path + filename;

            if (!QFile::exists(albumart_image->m_filename))
            {
                // file does not exist so try to download and cache it
                if (!GetMythDownloadManager()->download(res, albumart_image->m_filename))
                {
                    m_albumArt->getImageList()->removeAll(albumart_image);
                    return {""};
                }
            }

            res = albumart_image->m_filename;
        }
        else if (repo == RT_CD)
        {
            // CD tracks can only be played locally, so coverart is local too
            return res;
        }
        else
        {
            // check for the image in the storage group
            QUrl url(res);

            if (url.path().isEmpty() || url.host().isEmpty() || url.userName().isEmpty())
            {
                return {""};
            }

            if (!RemoteFile::Exists(res))
            {
                if (albumart_image->m_embedded)
                {
                    if (gCoreContext->IsMasterBackend() &&
                        url.host() == gCoreContext->GetMasterHostName())
                    {
                        QStringList paramList;
                        paramList.append(QString("--songid='%1'").arg(ID()));
                        paramList.append(QString("--imagetype='%1'").arg(albumart_image->m_imageType));

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
                            << QString::number(albumart_image->m_imageType);
                        gCoreContext->SendReceiveStringList(slist);
                    }
                }
            }
        }

        return res;
    }

    return {""};
}

QString MusicMetadata::getAlbumArtFile(ImageType type)
{
    if (!m_albumArt)
        m_albumArt = new AlbumArtImages(this);

    AlbumArtImage *albumart_image = m_albumArt->getImage(type);
    if (albumart_image)
        return albumart_image->m_filename;

    return {""};
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
    m_albumArt = nullptr; //new AlbumArtImages(this);
}

LyricsData* MusicMetadata::getLyricsData(void)
{
    if (!m_lyricsData)
        m_lyricsData = new LyricsData(this);

    return m_lyricsData;
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
    return nullptr;
}

//--------------------------------------------------------------------------

void MetadataLoadingThread::run()
{
    RunProlog();
    //if you want to simulate a big music collection load
    //sleep(3);
    m_parent->resync();
    RunEpilog();
}

AllMusic::AllMusic(void)
{
    //  Start a thread to do data loading and sorting
    startLoading();
}

AllMusic::~AllMusic()
{
    while (!m_allMusic.empty())
    {
        delete m_allMusic.back();
        m_allMusic.pop_back();
    }

    while (!m_cdData.empty())
    {
        delete m_cdData.back();
        m_cdData.pop_back();
    }

    m_metadataLoader->wait();
    delete m_metadataLoader;
}

bool AllMusic::cleanOutThreads()
{
    //  If this is still running, the user
    //  probably selected mythmusic and then
    //  escaped out right away

    if (m_metadataLoader->isFinished())
    {
        return true;
    }

    m_metadataLoader->wait();
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
    m_doneLoading = false;

    if (m_metadataLoader)
    {
        cleanOutThreads();
        delete m_metadataLoader;
    }

    m_metadataLoader = new MetadataLoadingThread(this);
    m_metadataLoader->start();

    return true;
}

/// resync our cache with the database
void AllMusic::resync()
{
    uint added = 0;
    uint removed = 0;
    uint changed = 0;

    m_doneLoading = false;

    QString aquery = "SELECT music_songs.song_id, music_artists.artist_id, music_artists.artist_name, "
                     "music_comp_artists.artist_name AS compilation_artist, "
                     "music_albums.album_id, music_albums.album_name, music_songs.name, music_genres.genre, music_songs.year, "
                     "music_songs.track, music_songs.length, music_songs.directory_id, "
                     "CONCAT_WS('/', music_directories.path, music_songs.filename) AS filename, "
                     "music_songs.rating, music_songs.numplays, music_songs.lastplay, music_songs.date_entered, "
                     "music_albums.compilation, music_songs.format, music_songs.track_count, "
                     "music_songs.size, music_songs.hostname, music_songs.disc_number, music_songs.disc_count "
                     "FROM music_songs "
                     "LEFT JOIN music_directories ON music_songs.directory_id=music_directories.directory_id "
                     "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
                     "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                     "LEFT JOIN music_artists AS music_comp_artists ON music_albums.artist_id=music_comp_artists.artist_id "
                     "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
                     "ORDER BY music_songs.song_id;";

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

            auto *dbMeta = new MusicMetadata(
                query.value(12).toString(),    // filename
                query.value(2).toString(),     // artist
                query.value(3).toString(),     // compilation artist
                query.value(5).toString(),     // album
                query.value(6).toString(),     // title
                query.value(7).toString(),     // genre
                query.value(8).toInt(),        // year
                query.value(9).toInt(),        // track no.
                std::chrono::milliseconds(query.value(10).toInt()),       // length
                query.value(0).toInt(),        // id
                query.value(13).toInt(),       // rating
                query.value(14).toInt(),       // playcount
                query.value(15).toDateTime(),  // lastplay
                query.value(16).toDateTime(),  // date_entered
                (query.value(17).toInt() > 0), // compilation
                query.value(18).toString());   // format

            dbMeta->setDirectoryId(query.value(11).toInt());
            dbMeta->setArtistId(query.value(1).toInt());
            dbMeta->setCompilationArtistId(query.value(3).toInt());
            dbMeta->setAlbumId(query.value(4).toInt());
            dbMeta->setTrackCount(query.value(19).toInt());
            dbMeta->setFileSize(query.value(20).toULongLong());
            dbMeta->setHostname(query.value(21).toString());
            dbMeta->setDiscNumber(query.value(22).toInt());
            dbMeta->setDiscCount(query.value(23).toInt());

            if (!m_musicMap.contains(id))
            {
                // new track

                //  Don't delete dbMeta, as the MetadataPtrList now owns it
                m_allMusic.append(dbMeta);

                m_musicMap[id] = dbMeta;

                added++;
            }
            else
            {
                // existing track, check for any changes
                MusicMetadata *cacheMeta = m_musicMap[id];

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
                m_playCountMin = m_playCountMax = query.value(13).toInt();
                m_lastPlayMin  = m_lastPlayMax  = query.value(14).toDateTime().toSecsSinceEpoch();
            }
            else
            {
                int playCount = query.value(13).toInt();
                qint64 lastPlay = query.value(14).toDateTime().toSecsSinceEpoch();

                m_playCountMin = std::min(playCount, m_playCountMin);
                m_playCountMax = std::max(playCount, m_playCountMax);
                m_lastPlayMin  = std::min(lastPlay,  m_lastPlayMin);
                m_lastPlayMax  = std::max(lastPlay,  m_lastPlayMax);
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
    for (const auto *track : std::as_const(m_allMusic))
    {
        if (!idList.contains(track->ID()))
        {
            deleteList.append(track->ID());
        }
    }

    // remove the no longer available tracks
    for (uint id : deleteList)
    {
        MusicMetadata *mdata = m_musicMap[id];
        m_allMusic.removeAll(mdata);
        m_musicMap.remove(id);
        removed++;
        delete mdata;
    }

    // tell any listeners a resync has just finished and they may need to reload/resync
    LOG(VB_GENERAL, LOG_DEBUG, QString("AllMusic::resync sending MUSIC_RESYNC_FINISHED added: %1, removed: %2, changed: %3")
                                      .arg(added).arg(removed).arg(changed));
    gCoreContext->SendMessage(QString("MUSIC_RESYNC_FINISHED %1 %2 %3").arg(added).arg(removed).arg(changed));

    m_doneLoading = true;
}

MusicMetadata* AllMusic::getMetadata(int an_id)
{
    if (m_musicMap.contains(an_id))
        return m_musicMap[an_id];

    return nullptr;
}

bool AllMusic::isValidID(int an_id)
{
    return m_musicMap.contains(an_id);
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
    for (auto *item : std::as_const(m_allMusic))
    {
        if (item->hasChanged())
            item->persist();
    }
}

// cd stuff
void AllMusic::clearCDData(void)
{
    while (!m_cdData.empty())
    {
        MusicMetadata *mdata = m_cdData.back();
        if (m_musicMap.contains(mdata->ID()))
            m_musicMap.remove(mdata->ID());

        delete m_cdData.back();
        m_cdData.pop_back();
    }

    m_cdTitle = tr("CD -- none");
}

void AllMusic::addCDTrack(const MusicMetadata &the_track)
{
    auto *mdata = new MusicMetadata(the_track);
    mdata->setID(m_cdData.count() + 1);
    mdata->setRepo(RT_CD);
    m_cdData.append(mdata);
    m_musicMap[mdata->ID()] = mdata;
}

bool AllMusic::checkCDTrack(MusicMetadata *the_track)
{
    if (m_cdData.count() < 1)
        return false;

    return m_cdData.last()->FormatTitle() == the_track->FormatTitle();
}

MusicMetadata* AllMusic::getCDMetadata(int the_track)
{
    for (auto *anit : std::as_const(m_cdData))
    {
        if (anit->Track() == the_track)
        {
            return anit;
        }
    }

    return nullptr;
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

    return nullptr;
}

void AllStream::loadStreams(void)
{
    while (!m_streamList.empty())
    {
        delete m_streamList.back();
        m_streamList.pop_back();
    }

    QString aquery = "SELECT intid, broadcaster, channel, description, url1, url2, url3, url4, url5,"
                     "logourl, genre, metaformat, country, language, format "
                     "FROM music_radios "
                     "ORDER BY broadcaster,channel;";

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec(aquery))
        MythDB::DBError("AllStream::loadStreams", query);

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            UrlList urls;
            for (size_t x = 0; x < STREAMURLCOUNT; x++)
                urls[x] = query.value(4 + x).toString();

            auto *mdata = new MusicMetadata(
                    query.value(0).toInt(),        // intid
                    query.value(1).toString(),     // broadcaster
                    query.value(2).toString(),     // channel
                    query.value(3).toString(),     // description
                    urls,                          // array of 5 urls
                    query.value(9).toString(),     // logourl
                    query.value(10).toString(),     // genre
                    query.value(11).toString(),     // metadataformat
                    query.value(12).toString(),     // country
                    query.value(13).toString(),     // language
                    query.value(14).toString());    // format

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
    query.prepare("INSERT INTO music_radios (broadcaster, channel, description, "
                   "url1, url2, url3, url4, url5, "
                  "logourl, genre, country, language, format, metaformat) "
                  "VALUES (:BROADCASTER, :CHANNEL, :DESCRIPTION, :URL1, :URL2, :URL3, :URL4, :URL5, "
                  ":LOGOURL, :GENRE, :COUNTRY, :LANGUAGE, :FORMAT, :METAFORMAT);");
    query.bindValueNoNull(":BROADCASTER", mdata->Broadcaster());
    query.bindValueNoNull(":CHANNEL", mdata->Channel());
    query.bindValueNoNull(":DESCRIPTION", mdata->Description());
    query.bindValueNoNull(":URL1", mdata->Url(0));
    query.bindValueNoNull(":URL2", mdata->Url(1));
    query.bindValueNoNull(":URL3", mdata->Url(2));
    query.bindValueNoNull(":URL4", mdata->Url(3));
    query.bindValueNoNull(":URL5", mdata->Url(4));
    query.bindValueNoNull(":LOGOURL", mdata->LogoUrl());
    query.bindValueNoNull(":GENRE", mdata->Genre());
    query.bindValueNoNull(":COUNTRY", mdata->Country());
    query.bindValueNoNull(":LANGUAGE", mdata->Language());
    query.bindValueNoNull(":FORMAT", mdata->Format());
    query.bindValueNoNull(":METAFORMAT", mdata->MetadataFormat());

    if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
    {
        MythDB::DBError("music insert radio", query);
        return;
    }

    mdata->setID(query.lastInsertId().toInt());
    mdata->setRepo(RT_Radio);

    loadStreams();
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
}

void AllStream::updateStream(MusicMetadata* mdata)
{
    // update the stream in the db
    int id = ID_TO_ID(mdata->ID());
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE music_radios set broadcaster = :BROADCASTER, channel = :CHANNEL, description = :DESCRIPTION, "
                   "url1 = :URL1, url2 = :URL2, url3 = :URL3, url4 = :URL4, url5 = :URL5, "
                  "logourl = :LOGOURL, genre = :GENRE, country = :COUNTRY, language = :LANGUAGE, "
                  "format = :FORMAT, metaformat = :METAFORMAT "
                  "WHERE intid = :ID");
    query.bindValueNoNull(":BROADCASTER", mdata->Broadcaster());
    query.bindValueNoNull(":CHANNEL", mdata->Channel());
    query.bindValueNoNull(":DESCRIPTION", mdata->Description());
    query.bindValueNoNull(":URL1", mdata->Url(0));
    query.bindValueNoNull(":URL2", mdata->Url(1));
    query.bindValueNoNull(":URL3", mdata->Url(2));
    query.bindValueNoNull(":URL4", mdata->Url(3));
    query.bindValueNoNull(":URL5", mdata->Url(4));
    query.bindValueNoNull(":LOGOURL", mdata->LogoUrl());
    query.bindValueNoNull(":GENRE", mdata->Genre());
    query.bindValueNoNull(":COUNTRY", mdata->Country());
    query.bindValueNoNull(":LANGUAGE", mdata->Language());
    query.bindValueNoNull(":FORMAT", mdata->Format());
    query.bindValueNoNull(":METAFORMAT", mdata->MetadataFormat());
    query.bindValue(":ID", id);

    if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
    {
        MythDB::DBError("AllStream::updateStream", query);
        return;
    }

    loadStreams();
}

/**************************************************************************/

AlbumArtImages::AlbumArtImages(MusicMetadata *metadata, bool loadFromDB)
    : m_parent(metadata)
{
    if (loadFromDB)
        findImages();
}

AlbumArtImages::AlbumArtImages(MusicMetadata *metadata, const AlbumArtImages &other)
    : m_parent(metadata)
{
    for (const auto &srcImage : std::as_const(other.m_imageList))
    {
        m_imageList.append(new AlbumArtImage(srcImage));
    }
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

    if (m_parent == nullptr)
        return;

    int trackid = ID_TO_ID(m_parent->ID());
    int repo = ID_TO_REPO(m_parent->ID());

    if (repo == RT_Radio)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        //FIXME: this should work with the alternate urls as well eg url2, url3 etc
        query.prepare("SELECT logourl FROM music_radios WHERE url1 = :URL;");
        query.bindValue(":URL", m_parent->Filename());
        if (query.exec())
        {
            while (query.next())
            {
                QString logoUrl = query.value(0).toString();

                auto *image = new AlbumArtImage();
                image->m_id = -1;
                image->m_filename = logoUrl;
                image->m_imageType = IT_FRONTCOVER;
                image->m_embedded = false;
                image->m_hostname = "";
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
                auto *image = new AlbumArtImage();
                bool embedded = (query.value(4).toInt() == 1);
                image->m_id = query.value(0).toInt();

                QUrl url(m_parent->Filename(true));

                if (embedded)
                {
                    if (url.scheme() == "myth")
                    {
                        image->m_filename = MythCoreContext::GenMythURL(url.host(), url.port(),
                                                                        QString("AlbumArt/") + query.value(1).toString(),
                                                                        "MusicArt");
                    }
                    else
                    {
                        image->m_filename = query.value(1).toString();
                    }
                }
                else
                {
                    if (url.scheme() == "myth")
                    {
                        image->m_filename =  MythCoreContext::GenMythURL(url.host(), url.port(),
                                                                         query.value(1).toString(),
                                                                         "Music");
                    }
                    else
                    {
                        image->m_filename = query.value(1).toString();
                    }
                }

                image->m_imageType = (ImageType) query.value(3).toInt();
                image->m_embedded = embedded;
                image->m_hostname = query.value(5).toString();

                m_imageList.push_back(image);
            }
        }

        // add any artist images
        QString artist = m_parent->Artist().toLower();
        if (findIcon("artist", artist) != QString())
        {
            auto *image = new AlbumArtImage();
            image->m_id = -1;
            image->m_filename = findIcon("artist", artist);
            image->m_imageType = IT_ARTIST;
            image->m_embedded = false;

            m_imageList.push_back(image);
        }
    }
}

void AlbumArtImages::scanForImages()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *busy = new MythUIBusyDialog(tr("Scanning for music album art..."),
                                      popupStack, "scanbusydialog");

    if (busy->Create())
    {
        popupStack->AddScreen(busy, false);
    }
    else
    {
        delete busy;
        busy = nullptr;
    }

    QStringList strList;
    strList << "MUSIC_FIND_ALBUMART"
            << m_parent->Hostname()
            << QString::number(m_parent->ID())
            << "1";

    auto *scanThread = new AlbumArtScannerThread(strList);
    scanThread->start();

    while (scanThread->isRunning())
    {
        QCoreApplication::processEvents();
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
        auto *image = new AlbumArtImage;
        image->m_id = strList[x].toInt();
        image->m_imageType = (ImageType) strList[x + 1].toInt();
        image->m_embedded = (strList[x + 2].toInt() == 1);
        image->m_description = strList[x + 3];

        if (image->m_embedded)
        {
            image->m_filename = MythCoreContext::GenMythURL(m_parent->Hostname(), 0,
                                                            QString("AlbumArt/") + strList[x + 4],
                                                            "MusicArt");
        }
        else
        {
            image->m_filename =  MythCoreContext::GenMythURL(m_parent->Hostname(), 0,
                                                             strList[x + 4],
                                                             "Music");
        }

        image->m_hostname = strList[x + 5];

        LOG(VB_FILE, LOG_INFO, "AlbumArtImages::scanForImages found image");
        LOG(VB_FILE, LOG_INFO, QString("ID: %1").arg(image->m_id));
        LOG(VB_FILE, LOG_INFO, QString("ImageType: %1").arg(image->m_imageType));
        LOG(VB_FILE, LOG_INFO, QString("Embedded: %1").arg(image->m_embedded));
        LOG(VB_FILE, LOG_INFO, QString("Description: %1").arg(image->m_description));
        LOG(VB_FILE, LOG_INFO, QString("Filename: %1").arg(image->m_filename));
        LOG(VB_FILE, LOG_INFO, QString("Hostname: %1").arg(image->m_hostname));
        LOG(VB_FILE, LOG_INFO, "-------------------------------");

        addImage(image);

        delete image;
    }
}

AlbumArtImage *AlbumArtImages::getImage(ImageType type)
{
    for (auto *item : std::as_const(m_imageList))
    {
        if (item->m_imageType == type)
            return item;
    }

    return nullptr;
}

AlbumArtImage *AlbumArtImages::getImageByID(int imageID)
{
    for (auto *item : std::as_const(m_imageList))
    {
        if (item->m_id == imageID)
            return item;
    }

    return nullptr;
}

QStringList AlbumArtImages::getImageFilenames(void) const
{
    QStringList paths;

    for (const auto *item : std::as_const(m_imageList))
        paths += item->m_filename;

    return paths;
}

AlbumArtImage *AlbumArtImages::getImageAt(uint index)
{
    if (index < (uint)m_imageList.size())
        return m_imageList[index];

    return nullptr;
}

// static method to get a translated type name from an ImageType
QString AlbumArtImages::getTypeName(ImageType type)
{
    // these const's should match the ImageType enum's
    static const std::array<const std::string,6> s_typeStrings {
        QT_TR_NOOP("Unknown"),            // IT_UNKNOWN
        QT_TR_NOOP("Front Cover"),        // IT_FRONTCOVER
        QT_TR_NOOP("Back Cover"),         // IT_BACKCOVER
        QT_TR_NOOP("CD"),                 // IT_CD
        QT_TR_NOOP("Inlay"),              // IT_INLAY
        QT_TR_NOOP("Artist"),             // IT_ARTIST
    };

    return QCoreApplication::translate("AlbumArtImages",
                                       s_typeStrings[type].c_str());
}

// static method to get a filename from an ImageType
QString AlbumArtImages::getTypeFilename(ImageType type)
{
    // these const's should match the ImageType enum's
    static const std::array<const std::string,6> s_filenameStrings {
        QT_TR_NOOP("unknown"),      // IT_UNKNOWN
        QT_TR_NOOP("front"),        // IT_FRONTCOVER
        QT_TR_NOOP("back"),         // IT_BACKCOVER
        QT_TR_NOOP("cd"),           // IT_CD
        QT_TR_NOOP("inlay"),        // IT_INLAY
        QT_TR_NOOP("artist")        // IT_ARTIST
    };

    return QCoreApplication::translate("AlbumArtImages",
                                       s_filenameStrings[type].c_str());
}

// static method to guess the image type from the filename
ImageType AlbumArtImages::guessImageType(const QString &filename)
{
    ImageType type = IT_FRONTCOVER;

    if (filename.contains("front", Qt::CaseInsensitive) ||
             filename.contains(tr("front"), Qt::CaseInsensitive) ||
             filename.contains("cover", Qt::CaseInsensitive) ||
             filename.contains(tr("cover"), Qt::CaseInsensitive))
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

void AlbumArtImages::addImage(const AlbumArtImage * const newImage)
{
    // do we already have an image of this type?
    AlbumArtImage *image = nullptr;

    for (auto *item : std::as_const(m_imageList))
    {
        if (item->m_imageType == newImage->m_imageType
            && item->m_embedded == newImage->m_embedded)
        {
            image = item;
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
        image->m_filename    = newImage->m_filename;
        image->m_imageType   = newImage->m_imageType;
        image->m_embedded    = newImage->m_embedded;
        image->m_description = newImage->m_description;
        image->m_hostname    = newImage->m_hostname;
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
    for (auto *image : std::as_const(m_imageList))
    {
        //TODO: for the moment just ignore artist images
        if (image->m_imageType == IT_ARTIST)
            continue;

        if (image->m_id > 0)
        {
            // re-use the same id this image had before
            query.prepare("INSERT INTO music_albumart ( albumart_id, "
                          "filename, imagetype, song_id, directory_id, embedded, hostname ) "
                          "VALUES ( :ID, :FILENAME, :TYPE, :SONGID, :DIRECTORYID, :EMBED, :HOSTNAME );");
            query.bindValue(":ID", image->m_id);
        }
        else
        {
            query.prepare("INSERT INTO music_albumart ( filename, "
                        "imagetype, song_id, directory_id, embedded, hostname ) VALUES ( "
                        ":FILENAME, :TYPE, :SONGID, :DIRECTORYID, :EMBED, :HOSTNAME );");
        }

        QFileInfo fi(image->m_filename);
        query.bindValue(":FILENAME", fi.fileName());

        query.bindValue(":TYPE", image->m_imageType);
        query.bindValue(":SONGID", image->m_embedded ? trackID : 0);
        query.bindValue(":DIRECTORYID", image->m_embedded ? 0 : directoryID);
        query.bindValue(":EMBED", image->m_embedded);
        query.bindValue(":HOSTNAME", image->m_hostname);

        if (!query.exec())
        {
            MythDB::DBError("AlbumArtImages::dumpToDatabase - "
                            "add/update music_albumart", query);
        }
        else
        {
            if (image->m_id <= 0)
                image->m_id = query.lastInsertId().toInt();
        }
    }
}
