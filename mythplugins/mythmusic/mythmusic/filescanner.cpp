// POSIX headers
#include <sys/stat.h>

// Qt headers
#include <QApplication>
#include <QDir>

// MythTV headers
#include <mythdate.h>
#include <mythdb.h>
#include <mythcontext.h>
#include <mythdialogs.h>
#include <mythscreenstack.h>
#include <mythprogressdialog.h>
#include <musicmetadata.h>
#include <metaio.h>

// MythMusic headers
#include "filescanner.h"

FileScanner::FileScanner()
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

FileScanner::~FileScanner ()
{

}

/*!
 * \brief Builds a list of all the files found descending recursively
 *        into the given directory
 *
 * \param directory Directory to begin search
 * \param music_files A pointer to the MusicLoadedMap to store the results
 * \param parentid The id of the parent directory in the music_directories
 *                 table. The root directory should have an id of 0
 *
 * \returns Nothing.
 */
void FileScanner::BuildFileList(QString &directory, MusicLoadedMap &music_files, int parentid)
{
    QDir d(directory);

    if (!d.exists())
        return;

    d.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    /* Recursively traverse directory, calling QApplication::processEvents()
       every now and then to ensure the UI updates */
    int update_interval = 0;
    int newparentid = 0;
    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        QString filename = fi->absoluteFilePath();
        if (fi->isDir())
        {

            QString dir(filename);
            dir.remove(0, m_startdir.length());

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

            BuildFileList(filename, music_files, newparentid);

            qApp->processEvents ();
        }
        else
        {
            if (++update_interval > 100)
            {
                qApp->processEvents();
                update_interval = 0;
            }

            music_files[filename] = FileScanner::kFileSystem;
        }
    }
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
int FileScanner::GetDirectoryId(const QString &directory, const int &parentid)
{
    if (directory.isEmpty())
        return 0;

    MSqlQuery query(MSqlQuery::InitCon());

    // Load the directory id or insert it and get the id
    query.prepare("SELECT directory_id FROM music_directories "
                "WHERE path = :DIRECTORY ;");
    query.bindValue(":DIRECTORY", directory);

    if (query.exec() && query.next())
    {
            return query.value(0).toInt();
    }
    else
    {
        query.prepare("INSERT INTO music_directories (path, parent_id) "
                    "VALUES (:DIRECTORY, :PARENTID);");
        query.bindValue(":DIRECTORY", directory);
        query.bindValue(":PARENTID", parentid);

        if (!query.exec() || !query.isActive()
        || query.numRowsAffected() <= 0)
        {
            MythDB::DBError("music insert directory", query);
            return -1;
        }
        return query.lastInsertId().toInt();
    }

    MythDB::DBError("music select directory id", query);
    return -1;
}

/*!
 * \brief Check if file has been modified since given date/time
 *
 * \param filename File to examine
 * \param date_modified Date to use in comparison
 *
 * \returns True if file has been modified, otherwise false
 */
bool FileScanner::HasFileChanged(
    const QString &filename, const QString &date_modified)
{
    QFileInfo fi(filename);
    QDateTime dt = fi.lastModified();
    if (dt.isValid())
    {
        QDateTime old_dt = MythDate::fromString(date_modified);
        return !old_dt.isValid() || (dt > old_dt);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to stat file: %1")
                .arg(filename));
        return false;
    }
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
 *
 * \returns Nothing.
 */
void FileScanner::AddFileToDB(const QString &filename)
{
    QString extension = filename.section( '.', -1 ) ;
    QString directory = filename;
    directory.remove(0, m_startdir.length());
    directory = directory.section( '/', 0, -2);

    QString nameFilter = gCoreContext->GetSetting("AlbumArtFilter", "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    // If this file is an image, insert the details into the music_albumart table
    if (nameFilter.indexOf(extension.toLower()) > -1)
    {
        QString name = filename.section( '/', -1);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO music_albumart SET filename = :FILE, "
                      "directory_id = :DIRID, imagetype = :TYPE;");
        query.bindValue(":FILE", name);
        query.bindValue(":DIRID", m_directoryid[directory]);
        query.bindValue(":TYPE", AlbumArtImages::guessImageType(name));

        if (!query.exec() || query.numRowsAffected() <= 0)
        {
            MythDB::DBError("music insert artwork", query);
        }
        return;
    }

    LOG(VB_FILE, LOG_INFO,
        QString("Reading metadata from %1").arg(filename));
    MusicMetadata *data = MetaIO::readMetadata(filename);
    data->setFileSize((quint64)QFileInfo(filename).size());
    if (data)
    {
        QString album_cache_string;

        // Set values from cache
        int did = m_directoryid[directory];
        if (did > 0)
            data->setDirectoryId(did);

        int aid = m_artistid[data->Artist().toLower()];
        if (aid > 0)
        {
            data->setArtistId(aid);

            // The album cache depends on the artist id
            album_cache_string = data->getArtistId() + "#"
                + data->Album().toLower();

            if (m_albumid[album_cache_string] > 0)
                data->setAlbumId(m_albumid[album_cache_string]);
        }

        int gid = m_genreid[data->Genre().toLower()];
        if (gid > 0)
            data->setGenreId(gid);

        // Commit track info to database
        data->dumpToDatabase();

        // Update the cache
        m_artistid[data->Artist().toLower()] =
            data->getArtistId();

        m_genreid[data->Genre().toLower()] =
            data->getGenreId();

        album_cache_string = data->getArtistId() + "#"
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
    }
}

/*!
 * \brief Clear orphaned entries from the genre, artist, album and albumart
 *        tables
 *
 * \returns Nothing.
 */
void FileScanner::cleanDB()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = QObject::tr("Cleaning music database");
    MythUIProgressDialog *clean_progress = new MythUIProgressDialog(message,
                                                    popupStack,
                                                    "cleaningprogressdialog");

    if (clean_progress->Create())
    {
        popupStack->AddScreen(clean_progress, false);
        clean_progress->SetTotal(4);
    }
    else
    {
        delete clean_progress;
        clean_progress = NULL;
    }

    uint counter = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery deletequery(MSqlQuery::InitCon());

    if (!query.exec("SELECT g.genre_id FROM music_genres g "
                    "LEFT JOIN music_songs s ON g.genre_id=s.genre_id "
                    "WHERE s.genre_id IS NULL;"))
        MythDB::DBError("FileScanner::cleanDB - select music_genres", query);
    while (query.next())
    {
        int genreid = query.value(0).toInt();
        deletequery.prepare("DELETE FROM music_genres WHERE genre_id=:GENREID");
        deletequery.bindValue(":GENREID", genreid);
        if (!deletequery.exec())
            MythDB::DBError("FileScanner::cleanDB - delete music_genres",
                            deletequery);
    }

    if (clean_progress)
        clean_progress->SetProgress(++counter);

    if (!query.exec("SELECT a.album_id FROM music_albums a "
                    "LEFT JOIN music_songs s ON a.album_id=s.album_id "
                    "WHERE s.album_id IS NULL;"))
        MythDB::DBError("FileScanner::cleanDB - select music_albums", query);
    while (query.next())
    {
        int albumid = query.value(0).toInt();
        deletequery.prepare("DELETE FROM music_albums WHERE album_id=:ALBUMID");
        deletequery.bindValue(":ALBUMID", albumid);
        if (!deletequery.exec())
            MythDB::DBError("FileScanner::cleanDB - delete music_albums",
                            deletequery);
    }

    if (clean_progress)
        clean_progress->SetProgress(++counter);

    if (!query.exec("SELECT a.artist_id FROM music_artists a "
                    "LEFT JOIN music_songs s ON a.artist_id=s.artist_id "
                    "LEFT JOIN music_albums l ON a.artist_id=l.artist_id "
                    "WHERE s.artist_id IS NULL AND l.artist_id IS NULL"))
        MythDB::DBError("FileScanner::cleanDB - select music_artists", query);
    while (query.next())
    {
        int artistid = query.value(0).toInt();
        deletequery.prepare("DELETE FROM music_artists WHERE artist_id=:ARTISTID");
        deletequery.bindValue(":ARTISTID", artistid);
        if (!deletequery.exec())
            MythDB::DBError("FileScanner::cleanDB - delete music_artists",
                            deletequery);
    }

    if (clean_progress)
        clean_progress->SetProgress(++counter);

    if (!query.exec("SELECT a.albumart_id FROM music_albumart a LEFT JOIN "
                    "music_songs s ON a.song_id=s.song_id WHERE "
                    "embedded='1' AND s.song_id IS NULL;"))
        MythDB::DBError("FileScanner::cleanDB - select music_albumart", query);
    while (query.next())
    {
        int albumartid = query.value(0).toInt();
        deletequery.prepare("DELETE FROM music_albumart WHERE albumart_id=:ALBUMARTID");
        deletequery.bindValue(":ALBUMARTID", albumartid);
        if (!deletequery.exec())
            MythDB::DBError("FileScanner::cleanDB - delete music_albumart",
                            deletequery);
    }

    if (clean_progress)
    {
        clean_progress->SetProgress(++counter);
        clean_progress->Close();
    }
}

/*!
 * \brief Removes a file from the database.
 *
 * \param filename Full path to file.
 *
 * \returns Nothing.
 */
void FileScanner::RemoveFileFromDB (const QString &filename)
{
    QString sqlfilename(filename);
    sqlfilename.remove(0, m_startdir.length());
    // We know that the filename will not contain :// as the SQL limits this
    QString directory = sqlfilename.section( '/', 0, -2 ) ;
    sqlfilename = sqlfilename.section( '/', -1 ) ;

    QString extension = sqlfilename.section( '.', -1 ) ;

    QString nameFilter = gCoreContext->GetSetting("AlbumArtFilter",
                                              "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    if (nameFilter.indexOf(extension) > -1)
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
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_songs WHERE filename = :NAME ;");
    query.bindValue(":NAME", sqlfilename);
    if (!query.exec())
        MythDB::DBError("FileScanner::RemoveFileFromDB - deleting music_songs",
                        query);
}

/*!
 * \brief Updates a file in the database.
 *
 * \param filename Full path to file.
 *
 * \returns Nothing.
 */
void FileScanner::UpdateFileInDB(const QString &filename)
{
    QString directory = filename;
    directory.remove(0, m_startdir.length());
    directory = directory.section( '/', 0, -2);

    MusicMetadata *db_meta   = MetaIO::getMetadata(filename);
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
            album_cache_string = disk_meta->getArtistId() + "#" +
                disk_meta->Album().toLower();

            if (m_albumid[album_cache_string] > 0)
                disk_meta->setAlbumId(m_albumid[album_cache_string]);
        }

        int gid = m_genreid[disk_meta->Genre().toLower()];
        if (gid > 0)
            disk_meta->setGenreId(gid);

        disk_meta->setFileSize((quint64)QFileInfo(filename).size());

        // Commit track info to database
        disk_meta->dumpToDatabase();

        // Update the cache
        m_artistid[disk_meta->Artist().toLower()]
            = disk_meta->getArtistId();
        m_genreid[disk_meta->Genre().toLower()]
            = disk_meta->getGenreId();
        album_cache_string = disk_meta->getArtistId() + "#" +
            disk_meta->Album().toLower();
        m_albumid[album_cache_string] = disk_meta->getAlbumId();
    }

    if (disk_meta)
        delete disk_meta;

    if (db_meta)
        delete db_meta;
}

/*!
 * \brief Scan a directory recursively for music and albumart.
 *        Inserts, updates and removes any files any files found in the
 *        database.
 *
 * \param directory Directory to scan
 *
 * \returns Nothing.
 */
void FileScanner::SearchDir(QString &directory)
{

    m_startdir = directory;

    MusicLoadedMap music_files;
    MusicLoadedMap::Iterator iter;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = QObject::tr("Searching for music files");

    MythUIBusyDialog *busy = new MythUIBusyDialog(message, popupStack,
                                                  "musicscanbusydialog");

    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = NULL;

    BuildFileList(m_startdir, music_files, 0);

    if (busy)
        busy->Close();

    ScanMusic(music_files);
    ScanArtwork(music_files);

    message = QObject::tr("Updating music database");
    MythUIProgressDialog *file_checking = new MythUIProgressDialog(message,
                                                    popupStack,
                                                    "scalingprogressdialog");

    if (file_checking->Create())
    {
        popupStack->AddScreen(file_checking, false);
        file_checking->SetTotal(music_files.size());
    }
    else
    {
        delete file_checking;
        file_checking = NULL;
    }

     /*
       This can be optimised quite a bit by consolidating all commands
       via a lot of refactoring.

       1) group all files of the same decoder type, and don't
       create/delete a Decoder pr. AddFileToDB. Or make Decoders be
       singletons, it should be a fairly simple change.

       2) RemoveFileFromDB should group the remove into one big SQL.

       3) UpdateFileInDB, same as 1.
     */

    uint counter = 0;
    for (iter = music_files.begin(); iter != music_files.end(); iter++)
    {
        if (*iter == FileScanner::kFileSystem)
            AddFileToDB(iter.key());
        else if (*iter == FileScanner::kDatabase)
            RemoveFileFromDB(iter.key ());
        else if (*iter == FileScanner::kNeedUpdate)
            UpdateFileInDB(iter.key());

        if (file_checking)
        {
            file_checking->SetProgress(++counter);
            qApp->processEvents();
        }
    }
    if (file_checking)
        file_checking->Close();

    // Cleanup orphaned entries from the database
    cleanDB();
}

/*!
 * \brief Check a list of files against musics files already in the database
 *
 * \param music_files MusicLoadedMap
 *
 * \returns Nothing.
 */
void FileScanner::ScanMusic(MusicLoadedMap &music_files)
{
    MusicLoadedMap::Iterator iter;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec("SELECT CONCAT_WS('/', path, filename), date_modified "
                    "FROM music_songs LEFT JOIN music_directories ON "
                    "music_songs.directory_id=music_directories.directory_id "
                    "WHERE filename NOT LIKE ('%://%')"))
        MythDB::DBError("FileScanner::ScanMusic", query);

    uint counter = 0;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = QObject::tr("Scanning music files");
    MythUIProgressDialog *file_checking = new MythUIProgressDialog(message,
                                                    popupStack,
                                                    "scalingprogressdialog");

    if (file_checking->Create())
    {
        popupStack->AddScreen(file_checking, false);
        file_checking->SetTotal(query.size());
    }
    else
    {
        delete file_checking;
        file_checking = NULL;
    }

    QString name;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            name = m_startdir + query.value(0).toString();

            if (name != QString::null)
            {
                if ((iter = music_files.find(name)) != music_files.end())
                {
                    if (music_files[name] == FileScanner::kDatabase)
                    {
                        if (file_checking)
                        {
                            file_checking->SetProgress(++counter);
                            qApp->processEvents();
                        }
                        continue;
                    }
                    else if (HasFileChanged(name, query.value(1).toString()))
                        music_files[name] = FileScanner::kNeedUpdate;
                    else
                        music_files.erase(iter);
                }
                else
                {
                    music_files[name] = FileScanner::kDatabase;
                }
            }

            if (file_checking)
            {
                file_checking->SetProgress(++counter);
                qApp->processEvents();
            }
        }
    }

    if (file_checking)
        file_checking->Close();
}

/*!
 * \brief Check a list of files against images already in the database
 *
 * \param music_files MusicLoadedMap
 *
 * \returns Nothing.
 */
void FileScanner::ScanArtwork(MusicLoadedMap &music_files)
{
    MusicLoadedMap::Iterator iter;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec("SELECT CONCAT_WS('/', path, filename) "
                    "FROM music_albumart LEFT JOIN music_directories ON "
                    "music_albumart.directory_id=music_directories.directory_id"
                    " WHERE music_albumart.embedded=0"))
        MythDB::DBError("FileScanner::ScanArtwork", query);

    uint counter = 0;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = QObject::tr("Scanning Album Artwork");
    MythUIProgressDialog *file_checking = new MythUIProgressDialog(message,
                                                    popupStack,
                                                    "albumprogressdialog");

    if (file_checking->Create())
    {
        popupStack->AddScreen(file_checking, false);
        file_checking->SetTotal(query.size());
    }
    else
    {
        delete file_checking;
        file_checking = NULL;
    }

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString name;

            name = m_startdir + query.value(0).toString();

            if (name != QString::null)
            {
                if ((iter = music_files.find(name)) != music_files.end())
                {
                    if (music_files[name] == FileScanner::kDatabase)
                    {
                        if (file_checking)
                        {
                            file_checking->SetProgress(++counter);
                            qApp->processEvents();
                        }
                        continue;
                    }
                    else
                        music_files.erase(iter);
                }
                else
                {
                    music_files[name] = FileScanner::kDatabase;
                }
            }
            if (file_checking)
            {
                file_checking->SetProgress(++counter);
                qApp->processEvents();
            }
        }
    }

    if (file_checking)
        file_checking->Close();
}
