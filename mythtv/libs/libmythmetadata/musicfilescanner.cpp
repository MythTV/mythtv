// POSIX headers
#include <sys/stat.h>
#include <unistd.h>

// Qt headers
#include <QDir>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

#include "musicmetadata.h"
#include "metaio.h"
#include "musicfilescanner.h"

MusicFileScanner::MusicFileScanner(bool force) : m_forceupdate{force}
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Cache the directory ids from the database
    query.prepare("SELECT directory_id, path FROM music_directories");
    if (query.exec())
    {
        while(query.next())
        {
            m_directoryid[query.value(1).toString()] = query.value(0).toInt();
        }
    }

    // Cache the genre ids from the database
    query.prepare("SELECT genre_id, LOWER(genre) FROM music_genres");
    if (query.exec())
    {
        while(query.next())
        {
            m_genreid[query.value(1).toString()] = query.value(0).toInt();
        }
    }

    // Cache the artist ids from the database
    query.prepare("SELECT artist_id, LOWER(artist_name) FROM music_artists");
    if (query.exec() || query.isActive())
    {
        while(query.next())
        {
            m_artistid[query.value(1).toString()] = query.value(0).toInt();
        }
    }

    // Cache the album ids from the database
    query.prepare("SELECT album_id, artist_id, LOWER(album_name) FROM music_albums");
    if (query.exec())
    {
        while(query.next())
        {
            m_albumid[query.value(1).toString() + "#" + query.value(2).toString()] = query.value(0).toInt();
        }
    }
}

/*!
 * \brief Builds a list of all the files found descending recursively
 *        into the given directory
 *
 * \param directory Directory to begin search
 * \param music_files A pointer to the MusicLoadedMap to store the results
 * \param art_files   A pointer to the MusicLoadedMap to store the results
 * \param parentid The id of the parent directory in the music_directories
 *                 table. The root directory should have an id of 0
 *
 * \returns Nothing.
 */
void MusicFileScanner::BuildFileList(QString &directory, MusicLoadedMap &music_files, MusicLoadedMap &art_files, int parentid)
{
    QDir d(directory);

    if (!d.exists())
        return;

    d.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
        return;

    // Recursively traverse directory
    int newparentid = 0;
    for (const auto& fi : std::as_const(list))
    {
        QString filename = fi.absoluteFilePath();
        if (fi.isDir())
        {

            QString dir(filename);
            dir.remove(0, m_startDirs.last().length());

            newparentid = m_directoryid[dir];

            if (newparentid == 0)
            {
                int id = GetDirectoryId(dir, parentid);
                m_directoryid[dir] = id;

                if (id > 0)
                {
                    newparentid = id;
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Failed to get directory id for path %1")
                            .arg(dir));
                }
            }

            BuildFileList(filename, music_files, art_files, newparentid);
        }
        else
        {
            if (IsArtFile(filename))
            {
                MusicFileData fdata;
                fdata.startDir = m_startDirs.last();
                fdata.location = MusicFileScanner::kFileSystem;
                art_files[filename] = fdata;
            }
            else if (IsMusicFile(filename))
            {
                MusicFileData fdata;
                fdata.startDir = m_startDirs.last();
                fdata.location = MusicFileScanner::kFileSystem;
                music_files[filename] = fdata;
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO,
                        QString("Found file with unsupported extension %1")
                            .arg(filename));
            }
        }
    }
}

bool MusicFileScanner::IsArtFile(const QString &filename)
{
    QFileInfo fi(filename);
    QString extension = fi.suffix().toLower();
    QString nameFilter = gCoreContext->GetSetting("AlbumArtFilter", "*.png;*.jpg;*.jpeg;*.gif;*.bmp");


    return !extension.isEmpty() && nameFilter.indexOf(extension.toLower()) > -1;
}

bool MusicFileScanner::IsMusicFile(const QString &filename)
{
    QFileInfo fi(filename);
    QString extension = fi.suffix().toLower();
    QString nameFilter = MetaIO::kValidFileExtensions;

    return !extension.isEmpty() && nameFilter.indexOf(extension.toLower()) > -1;
}

/*!
 * \brief Get an ID for the given directory from the database.
 *        If it doesn't already exist in the database, insert it.
 *
 * \param directory Relative path to directory, from base dir
 * \param parentid The id of the parent directory in the music_directories
 *                 table. The root directory should have an id of 0
 *
 * \returns Directory id
 */
int MusicFileScanner::GetDirectoryId(const QString &directory, int parentid)
{
    if (directory.isEmpty())
        return 0;

    MSqlQuery query(MSqlQuery::InitCon());

    // Load the directory id or insert it and get the id
    query.prepare("SELECT directory_id FROM music_directories "
                "WHERE path = BINARY :DIRECTORY ;");
    query.bindValue(":DIRECTORY", directory);

    if (!query.exec())
    {
        MythDB::DBError("music select directory id", query);
        return -1;
    }

    if (query.next())
    {
        // we have found the directory already in the DB
        return query.value(0).toInt();
    }

    // directory is not in the DB so insert it
    query.prepare("INSERT INTO music_directories (path, parent_id) "
                "VALUES (:DIRECTORY, :PARENTID);");
    query.bindValue(":DIRECTORY", directory);
    query.bindValue(":PARENTID", parentid);

    if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
    {
        MythDB::DBError("music insert directory", query);
        return -1;
    }

    return query.lastInsertId().toInt();
}

/*!
 * \brief Check if file has been modified since given date/time
 *
 * \param filename File to examine
 * \param date_modified Date to use in comparison
 *
 * \returns True if file has been modified, otherwise false
 */
bool MusicFileScanner::HasFileChanged(
    const QString &filename, const QString &date_modified)
{
    QFileInfo fi(filename);
    QDateTime dt = fi.lastModified();
    if (dt.isValid())
    {
        QDateTime old_dt = MythDate::fromString(date_modified);
        return !old_dt.isValid() || (dt > old_dt);
    }
    LOG(VB_GENERAL, LOG_ERR, QString("Failed to stat file: %1")
        .arg(filename));
    return false;
}

/*!
 * \brief Insert file details into database.
 *        If it is an audio file, read the metadata and insert
 *        that information at the same time.
 *
 *        If it is an image file, just insert the filename and
 *        type.
 *
 * \param filename Full path to file.
 * \param startDir The starting directory fir the search. This will be
 *                 removed making the stored name relative to the
 *                 storage directory where it was found.
 *
 * \returns Nothing.
 */
void MusicFileScanner::AddFileToDB(const QString &filename, const QString &startDir)
{
    QString extension = filename.section( '.', -1 ) ;
    QString directory = filename;
    directory.remove(0, startDir.length());
    directory = directory.section( '/', 0, -2);

    QString nameFilter = gCoreContext->GetSetting("AlbumArtFilter", "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    // If this file is an image, insert the details into the music_albumart table
    if (nameFilter.indexOf(extension.toLower()) > -1)
    {
        QString name = filename.section( '/', -1);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO music_albumart "
                       "SET filename = :FILE, directory_id = :DIRID, "
                       "imagetype = :TYPE, hostname = :HOSTNAME;");

        query.bindValue(":FILE", name);
        query.bindValue(":DIRID", m_directoryid[directory]);
        query.bindValue(":TYPE", AlbumArtImages::guessImageType(name));
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

        if (!query.exec() || query.numRowsAffected() <= 0)
        {
            MythDB::DBError("music insert artwork", query);
        }

        ++m_coverartAdded;

        return;
    }

    if (extension.isEmpty() || !MetaIO::kValidFileExtensions.contains(extension.toLower()))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Ignoring filename with unsupported filename: '%1'").arg(filename));
        return;
    }

    LOG(VB_FILE, LOG_INFO, QString("Reading metadata from %1").arg(filename));
    MusicMetadata *data = MetaIO::readMetadata(filename);
    if (data)
    {
        data->setFileSize((quint64)QFileInfo(filename).size());
        data->setHostname(gCoreContext->GetHostName());

        QString album_cache_string;

        // Set values from cache
        int did = m_directoryid[directory];
        if (did >= 0)
            data->setDirectoryId(did);

        int aid = m_artistid[data->Artist().toLower()];
        if (aid > 0)
        {
            data->setArtistId(aid);

            // The album cache depends on the artist id
            album_cache_string = QString::number(data->getArtistId()) + "#"
                + data->Album().toLower();

            if (m_albumid[album_cache_string] > 0)
                data->setAlbumId(m_albumid[album_cache_string]);
        }

        int caid = m_artistid[data->CompilationArtist().toLower()];
        if (caid > 0)
            data->setCompilationArtistId(caid);

        int gid = m_genreid[data->Genre().toLower()];
        if (gid > 0)
            data->setGenreId(gid);

        // Commit track info to database
        data->dumpToDatabase();

        // Update the cache
        m_artistid[data->Artist().toLower()] =
            data->getArtistId();

        m_artistid[data->CompilationArtist().toLower()] =
            data->getCompilationArtistId();

        m_genreid[data->Genre().toLower()] =
            data->getGenreId();

        album_cache_string = QString::number(data->getArtistId()) + "#"
            + data->Album().toLower();
        m_albumid[album_cache_string] = data->getAlbumId();

        // read any embedded images from the tag
        MetaIO *tagger = MetaIO::createTagger(filename);

        if (tagger)
        {
            if (tagger->supportsEmbeddedImages())
            {
                AlbumArtList artList = tagger->getAlbumArtList(data->Filename());
                data->setEmbeddedAlbumArt(artList);
                data->getAlbumArtImages()->dumpToDatabase();
            }
            delete tagger;
        }

        delete data;

        ++m_tracksAdded;
    }
}

/*!
 * \brief Clear orphaned entries from the genre, artist, album and albumart
 *        tables
 *
 * \returns Nothing.
 */
void MusicFileScanner::cleanDB()
{
    LOG(VB_GENERAL, LOG_INFO, "Cleaning old entries from music database");

    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery deletequery(MSqlQuery::InitCon());

    // delete unused genre_ids from music_genres
    if (!query.exec("SELECT g.genre_id FROM music_genres g "
                    "LEFT JOIN music_songs s ON g.genre_id=s.genre_id "
                    "WHERE s.genre_id IS NULL;"))
        MythDB::DBError("MusicFileScanner::cleanDB - select music_genres", query);

    deletequery.prepare("DELETE FROM music_genres WHERE genre_id=:GENREID");
    while (query.next())
    {
        int genreid = query.value(0).toInt();
        deletequery.bindValue(":GENREID", genreid);
        if (!deletequery.exec())
            MythDB::DBError("MusicFileScanner::cleanDB - delete music_genres",
                            deletequery);
    }

    // delete unused album_ids from music_albums
    if (!query.exec("SELECT a.album_id FROM music_albums a "
                    "LEFT JOIN music_songs s ON a.album_id=s.album_id "
                    "WHERE s.album_id IS NULL;"))
        MythDB::DBError("MusicFileScanner::cleanDB - select music_albums", query);

    deletequery.prepare("DELETE FROM music_albums WHERE album_id=:ALBUMID");
    while (query.next())
    {
        int albumid = query.value(0).toInt();
        deletequery.bindValue(":ALBUMID", albumid);
        if (!deletequery.exec())
            MythDB::DBError("MusicFileScanner::cleanDB - delete music_albums",
                            deletequery);
    }

    // delete unused artist_ids from music_artists
    if (!query.exec("SELECT a.artist_id FROM music_artists a "
                    "LEFT JOIN music_songs s ON a.artist_id=s.artist_id "
                    "LEFT JOIN music_albums l ON a.artist_id=l.artist_id "
                    "WHERE s.artist_id IS NULL AND l.artist_id IS NULL"))
        MythDB::DBError("MusicFileScanner::cleanDB - select music_artists", query);


    deletequery.prepare("DELETE FROM music_artists WHERE artist_id=:ARTISTID");
    while (query.next())
    {
        int artistid = query.value(0).toInt();
        deletequery.bindValue(":ARTISTID", artistid);
        if (!deletequery.exec())
            MythDB::DBError("MusicFileScanner::cleanDB - delete music_artists",
                            deletequery);
    }

    // delete unused directory_ids from music_directories
    //
    // Get a list of directory_ids not referenced in music_songs.
    // This list will contain any directory that is only used for
    // organization.  I.E. If your songs are organized by artist and
    // then by album, this will contain all of the artist directories.
    if (!query.exec("SELECT d.directory_id, d.parent_id FROM music_directories d "
                    "LEFT JOIN music_songs s ON d.directory_id=s.directory_id "
                    "WHERE s.directory_id IS NULL ORDER BY directory_id DESC;"))
        MythDB::DBError("MusicFileScanner::cleanDB - select music_directories", query);

    deletequery.prepare("DELETE FROM music_directories WHERE directory_id=:DIRECTORYID");

    MSqlQuery parentquery(MSqlQuery::InitCon());
    parentquery.prepare("SELECT COUNT(*) FROM music_directories "
                        "WHERE parent_id=:DIRECTORYID ");

    MSqlQuery dirnamequery(MSqlQuery::InitCon());
    dirnamequery.prepare("SELECT path FROM music_directories "
                         "WHERE directory_id=:DIRECTORYID ");

    int deletedCount = 1;

    while (deletedCount > 0)
    {
        deletedCount = 0;
        query.seek(-1);

        // loop through the list of unused directory_ids deleting any which
        // aren't referenced by any other directories parent_id
        while (query.next())
        {
            int directoryid = query.value(0).toInt();

            // have we still got references to this directory_id from other directories
            parentquery.bindValue(":DIRECTORYID", directoryid);
            if (!parentquery.exec())
            {
                MythDB::DBError("MusicFileScanner::cleanDB - get parent directory count",
                                parentquery);
                continue;
            }
            if (!parentquery.next())
                continue;
            int parentCount = parentquery.value(0).toInt();
            if (parentCount != 0)
                // Still has child directories
                continue;
            if(VERBOSE_LEVEL_CHECK(VB_GENERAL, LOG_DEBUG))
            {
                dirnamequery.bindValue(":DIRECTORYID", directoryid);
                if (dirnamequery.exec() && dirnamequery.next())
                {
                    LOG(VB_GENERAL, LOG_DEBUG,
                        QString("MusicFileScanner deleted directory %1  %2")
                        .arg(directoryid,5).arg(dirnamequery.value(0).toString()));
                }
            }
            deletequery.bindValue(":DIRECTORYID", directoryid);
            if (!deletequery.exec())
                MythDB::DBError("MusicFileScanner::cleanDB - delete music_directories",
                                deletequery);
            deletedCount += deletequery.numRowsAffected();
        }
        LOG(VB_GENERAL, LOG_INFO,
            QString("MusicFileScanner deleted %1 directory entries")
                .arg(deletedCount));
    }

    // delete unused albumart_ids from music_albumart (embedded images)
    if (!query.exec("SELECT a.albumart_id FROM music_albumart a LEFT JOIN "
                    "music_songs s ON a.song_id=s.song_id WHERE "
                    "embedded='1' AND s.song_id IS NULL;"))
        MythDB::DBError("MusicFileScanner::cleanDB - select music_albumart", query);

    deletequery.prepare("DELETE FROM music_albumart WHERE albumart_id=:ALBUMARTID");
    while (query.next())
    {
        int albumartid = query.value(0).toInt();
        deletequery.bindValue(":ALBUMARTID", albumartid);
        if (!deletequery.exec())
            MythDB::DBError("MusicFileScanner::cleanDB - delete music_albumart",
                            deletequery);
    }
}

/*!
 * \brief Removes a file from the database.
 *
 * \param filename Full path to file.
 * \param startDir The starting directory fir the search. This will be
 *                 removed making the stored name relative to the
 *                 storage directory where it was found.
 *
 * \returns Nothing.
 */
void MusicFileScanner::RemoveFileFromDB(const QString &filename, const QString &startDir)
{
    QString sqlfilename(filename);
    sqlfilename.remove(0, startDir.length());
    // We know that the filename will not contain :// as the SQL limits this
    QString directory = sqlfilename.section( '/', 0, -2 ) ;
    sqlfilename = sqlfilename.section( '/', -1 ) ;

    QString extension = sqlfilename.section( '.', -1 ) ;

    QString nameFilter = gCoreContext->GetSetting("AlbumArtFilter",
                                              "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    if (nameFilter.indexOf(extension.toLower()) > -1)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM music_albumart WHERE filename= :FILE AND "
                      "directory_id= :DIRID;");
        query.bindValue(":FILE", sqlfilename);
        query.bindValue(":DIRID", m_directoryid[directory]);

        if (!query.exec() || query.numRowsAffected() <= 0)
        {
            MythDB::DBError("music delete artwork", query);
        }

        ++m_coverartRemoved;

        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_songs WHERE filename = :NAME ;");
    query.bindValue(":NAME", sqlfilename);
    if (!query.exec())
        MythDB::DBError("MusicFileScanner::RemoveFileFromDB - deleting music_songs",
                        query);

    ++m_tracksRemoved;
}

/*!
 * \brief Updates a file in the database.
 *
 * \param filename Full path to file.
 * \param startDir The starting directory fir the search. This will be
 *                 removed making the stored name relative to the
 *                 storage directory where it was found.
 *
 * \returns Nothing.
 */
void MusicFileScanner::UpdateFileInDB(const QString &filename, const QString &startDir)
{
    QString dbFilename = filename;
    dbFilename.remove(0, startDir.length());

    QString directory = filename;
    directory.remove(0, startDir.length());
    directory = directory.section( '/', 0, -2);

    MusicMetadata *db_meta   = MetaIO::getMetadata(dbFilename);
    MusicMetadata *disk_meta = MetaIO::readMetadata(filename);

    if (db_meta && disk_meta)
    {
        if (db_meta->ID() <= 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Asked to update track with "
                                                "invalid ID - %1")
                                            .arg(db_meta->ID()));
            delete disk_meta;
            delete db_meta;
            return;
        }

        disk_meta->setID(db_meta->ID());
        disk_meta->setRating(db_meta->Rating());
        if (db_meta->PlayCount() > disk_meta->PlayCount())
            disk_meta->setPlaycount(db_meta->Playcount());

        QString album_cache_string;

        // Set values from cache
        int did = m_directoryid[directory];
        if (did > 0)
            disk_meta->setDirectoryId(did);

        int aid = m_artistid[disk_meta->Artist().toLower()];
        if (aid > 0)
        {
            disk_meta->setArtistId(aid);

            // The album cache depends on the artist id
            album_cache_string = QString::number(disk_meta->getArtistId()) + "#" +
                disk_meta->Album().toLower();

            if (m_albumid[album_cache_string] > 0)
                disk_meta->setAlbumId(m_albumid[album_cache_string]);
        }

        int caid = m_artistid[disk_meta->CompilationArtist().toLower()];
        if (caid > 0)
            disk_meta->setCompilationArtistId(caid);

        int gid = m_genreid[disk_meta->Genre().toLower()];
        if (gid > 0)
            disk_meta->setGenreId(gid);

        disk_meta->setFileSize((quint64)QFileInfo(filename).size());

        disk_meta->setHostname(gCoreContext->GetHostName());

        // Commit track info to database
        disk_meta->dumpToDatabase();

        // Update the cache
        m_artistid[disk_meta->Artist().toLower()]
            = disk_meta->getArtistId();
        m_artistid[disk_meta->CompilationArtist().toLower()]
            = disk_meta->getCompilationArtistId();
        m_genreid[disk_meta->Genre().toLower()]
            = disk_meta->getGenreId();
        album_cache_string = QString::number(disk_meta->getArtistId()) + "#" +
            disk_meta->Album().toLower();
        m_albumid[album_cache_string] = disk_meta->getAlbumId();
    }

    delete disk_meta;
    delete db_meta;
}

/*!
 * \brief Scan a list of directories recursively for music and albumart.
 *        Inserts, updates and removes any files any files found in the
 *        database.
 *
 * \param dirList List of directories to scan
 *
 * \returns Nothing.
 */
void MusicFileScanner::SearchDirs(const QStringList &dirList)
{
    QString host = gCoreContext->GetHostName();

    if (IsRunning())
    {
        // check how long the scanner has been running
        // if it's more than 60 minutes assume something went wrong
        QString lastRun =  gCoreContext->GetSetting("MusicScannerLastRunStart", "");
        if (!lastRun.isEmpty())
        {
            QDateTime dtLastRun = QDateTime::fromString(lastRun, Qt::ISODate);
            if (dtLastRun.isValid())
            {
                static constexpr int64_t kOneHour {60LL * 60};
                if (MythDate::current() > dtLastRun.addSecs(kOneHour))
                {
                    LOG(VB_GENERAL, LOG_INFO, "Music file scanner has been running for more than 60 minutes. Lets reset and try again");
                    gCoreContext->SendMessage(QString("MUSIC_SCANNER_ERROR %1 %2").arg(host, "Stalled"));

                    // give the user time to read the notification before restarting the scan
                    sleep(5);
                }
                else
                {
                    LOG(VB_GENERAL, LOG_INFO, "Music file scanner is already running");
                    gCoreContext->SendMessage(QString("MUSIC_SCANNER_ERROR %1 %2").arg(host, "Already_Running"));
                    return;
                }
            }
        }
    }

    //TODO: could sanity check the directory exists and is readable here?

    LOG(VB_GENERAL, LOG_INFO, "Music file scanner started");
    gCoreContext->SendMessage(QString("MUSIC_SCANNER_STARTED %1").arg(host));

    updateLastRunStart();
    QString status = QString("running");
    updateLastRunStatus(status);

    m_tracksTotal = m_tracksAdded = m_tracksUnchanged = m_tracksRemoved = m_tracksUpdated = 0;
    m_coverartTotal = m_coverartAdded = m_coverartUnchanged = m_coverartRemoved = m_coverartUpdated = 0;

    MusicLoadedMap music_files;
    MusicLoadedMap art_files;
    MusicLoadedMap::Iterator iter;

    for (int x = 0; x < dirList.count(); x++)
    {
        QString startDir = dirList[x];
        m_startDirs.append(startDir + '/');
        LOG(VB_GENERAL, LOG_INFO, QString("Searching '%1' for music files").arg(startDir));

        BuildFileList(startDir, music_files, art_files, 0);
    }

    m_tracksTotal = music_files.count();
    m_coverartTotal = art_files.count();

    ScanMusic(music_files);
    ScanArtwork(art_files);

    LOG(VB_GENERAL, LOG_INFO, "Updating database");

        /*
        This can be optimised quite a bit by consolidating all commands
        via a lot of refactoring.

        1) group all files of the same decoder type, and don't
        create/delete a Decoder pr. AddFileToDB. Or make Decoders be
        singletons, it should be a fairly simple change.

        2) RemoveFileFromDB should group the remove into one big SQL.

        3) UpdateFileInDB, same as 1.
        */

    for (iter = music_files.begin(); iter != music_files.end(); iter++)
    {
        if ((*iter).location == MusicFileScanner::kFileSystem)
            AddFileToDB(iter.key(), (*iter).startDir);
        else if ((*iter).location == MusicFileScanner::kDatabase)
            RemoveFileFromDB(iter.key(), (*iter).startDir);
        else if ((*iter).location == MusicFileScanner::kNeedUpdate)
        {
            UpdateFileInDB(iter.key(), (*iter).startDir);
            ++m_tracksUpdated;
        }
    }

    for (iter = art_files.begin(); iter != art_files.end(); iter++)
    {
        if ((*iter).location == MusicFileScanner::kFileSystem)
            AddFileToDB(iter.key(), (*iter).startDir);
        else if ((*iter).location == MusicFileScanner::kDatabase)
            RemoveFileFromDB(iter.key(), (*iter).startDir);
        else if ((*iter).location == MusicFileScanner::kNeedUpdate)
        {
            UpdateFileInDB(iter.key(), (*iter).startDir);
            ++m_coverartUpdated;
        }
    }

    // Cleanup orphaned entries from the database
    cleanDB();

    QString trackStatus = QString("total tracks found: %1 (unchanged: %2, added: %3, removed: %4, updated %5)")
                                  .arg(m_tracksTotal).arg(m_tracksUnchanged).arg(m_tracksAdded)
                                  .arg(m_tracksRemoved).arg(m_tracksUpdated);
    QString coverartStatus = QString("total coverart found: %1 (unchanged: %2, added: %3, removed: %4, updated %5)")
                                     .arg(m_coverartTotal).arg(m_coverartUnchanged).arg(m_coverartAdded)
                                     .arg(m_coverartRemoved).arg(m_coverartUpdated);


    LOG(VB_GENERAL, LOG_INFO, "Music file scanner finished ");
    LOG(VB_GENERAL, LOG_INFO, trackStatus);
    LOG(VB_GENERAL, LOG_INFO, coverartStatus);

    gCoreContext->SendMessage(QString("MUSIC_SCANNER_FINISHED %1 %2 %3 %4 %5")
                                      .arg(host).arg(m_tracksTotal).arg(m_tracksAdded)
                                      .arg(m_coverartTotal).arg(m_coverartAdded));

    updateLastRunEnd();
    status = QString("success - %1 - %2").arg(trackStatus, coverartStatus);
    updateLastRunStatus(status);
}

/*!
 * \brief Check a list of files against musics files already in the database
 *
 * \param music_files MusicLoadedMap
 *
 * \returns Nothing.
 */
void MusicFileScanner::ScanMusic(MusicLoadedMap &music_files)
{
    MusicLoadedMap::Iterator iter;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT CONCAT_WS('/', path, filename), date_modified "
                  "FROM music_songs LEFT JOIN music_directories ON "
                  "music_songs.directory_id=music_directories.directory_id "
                  "WHERE filename NOT LIKE BINARY ('%://%') "
                  "AND hostname = :HOSTNAME");

    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec())
        MythDB::DBError("MusicFileScanner::ScanMusic", query);

    LOG(VB_GENERAL, LOG_INFO, "Checking tracks");

    QString name;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            for (int x = 0; x < m_startDirs.count(); x++)
            {
                name = m_startDirs[x] + query.value(0).toString();
                iter = music_files.find(name);
                if (iter != music_files.end())
                    break;
            }

            if (iter != music_files.end())
            {
                if (music_files[name].location == MusicFileScanner::kDatabase)
                    continue;
                if (m_forceupdate || HasFileChanged(name, query.value(1).toString()))
                    music_files[name].location = MusicFileScanner::kNeedUpdate;
                else
                {
                    ++m_tracksUnchanged;
                    music_files.erase(iter);
                }
            }
            else
            {
                music_files[name].location = MusicFileScanner::kDatabase;
            }
        }
    }
}

/*!
 * \brief Check a list of files against images already in the database
 *
 * \param music_files MusicLoadedMap
 *
 * \returns Nothing.
 */
void MusicFileScanner::ScanArtwork(MusicLoadedMap &music_files)
{
    MusicLoadedMap::Iterator iter;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT CONCAT_WS('/', path, filename) "
                  "FROM music_albumart "
                  "LEFT JOIN music_directories ON music_albumart.directory_id=music_directories.directory_id "
                  "WHERE music_albumart.embedded = 0 "
                  "AND music_albumart.hostname = :HOSTNAME");

    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec())
        MythDB::DBError("MusicFileScanner::ScanArtwork", query);

    LOG(VB_GENERAL, LOG_INFO, "Checking artwork");

    QString name;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            for (int x = 0; x < m_startDirs.count(); x++)
            {
                name = m_startDirs[x] + query.value(0).toString();
                iter = music_files.find(name);
                if (iter != music_files.end())
                    break;
            }

            if (iter != music_files.end())
            {
                if (music_files[name].location == MusicFileScanner::kDatabase)
                    continue;
                ++m_coverartUnchanged;
                music_files.erase(iter);
            }
            else
            {
                music_files[name].location = MusicFileScanner::kDatabase;
            }
        }
    }
}

// static
bool MusicFileScanner::IsRunning(void)
{
   return gCoreContext->GetSetting("MusicScannerLastRunStatus", "") == "running";
}

void MusicFileScanner::updateLastRunEnd(void)
{
    QDateTime qdtNow = MythDate::current();
    gCoreContext->SaveSetting("MusicScannerLastRunEnd", qdtNow.toString(Qt::ISODate));
}

void MusicFileScanner::updateLastRunStart(void)
{
    QDateTime qdtNow = MythDate::current();
    gCoreContext->SaveSetting("MusicScannerLastRunStart", qdtNow.toString(Qt::ISODate));
}

void MusicFileScanner::updateLastRunStatus(QString &status)
{
    gCoreContext->SaveSetting("MusicScannerLastRunStatus", status);
}
