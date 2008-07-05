// POSIX headers
#include <sys/stat.h>

// Qt headers
#include <qapplication.h>
#include <qdir.h>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>

// MythUI headers
#include <mythtv/libmythui/mythscreenstack.h>
#include <mythtv/libmythui/mythprogressdialog.h>

// MythMusic headers
#include "decoder.h"
#include "maddecoder.h"
#include "vorbisdecoder.h"
#include "filescanner.h"
#include "metadata.h"

FileScanner::FileScanner ()
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Cache the directory ids from the database
    query.prepare("SELECT directory_id, LOWER(path) FROM music_directories");
    if (query.exec() || query.isActive())
    {
        while(query.next())
        {
            m_directoryid[query.value(1).toString()] = query.value(0).toInt();
        }
    }

    // Cache the genre ids from the database
    query.prepare("SELECT genre_id, LOWER(genre) FROM music_genres");
    if (query.exec() || query.isActive())
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
    if (query.exec() || query.isActive())
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
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();
        if (fi->isDir())
        {

            QString dir(filename);
            dir.remove(0, m_startdir.length());

            newparentid = m_directoryid[dir.utf8().lower()];

            if (newparentid == 0) {
                if ((m_directoryid[dir.utf8().lower()] = GetDirectoryId(dir, parentid)) > 0)
                {
                    newparentid = m_directoryid[dir.utf8().lower()];
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("Failed to get directory id for path %1").arg(dir).arg(m_directoryid[dir.utf8().lower()]));
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

            music_files[filename] = kFileSystem;
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

    if (query.exec() || query.isActive())
    {
        if (query.next())
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
                MythContext::DBError("music insert directory", query);
                return -1;
            }
            return query.lastInsertId().toInt();
        }
    }
    else
    {
        MythContext::DBError("music select directory id", query);
        return -1;
    }

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
bool FileScanner::HasFileChanged(const QString &filename, const QString &date_modified)
{
    struct stat stbuf;

    if (stat(filename.local8Bit(), &stbuf) == 0)
    {
        if (date_modified.isEmpty() ||
            stbuf.st_mtime >
            (time_t)QDateTime::fromString(date_modified,
                                          Qt::ISODate).toTime_t())
        {
            return true;
        }
    }
    else {
        VERBOSE(VB_IMPORTANT, QString("Failed to stat file: %1")
            .arg(filename));
    }
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
 *
 * \returns Nothing.
 */
void FileScanner::AddFileToDB(const QString &filename)
{
    QString extension = filename.section( '.', -1 ) ;
    QString directory = filename;
    directory.remove(0, m_startdir.length());
    directory = directory.section( '/', 0, -2);

    QString nameFilter = gContext->GetSetting("AlbumArtFilter",
                                              "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    // If this file is an image, insert the details into the music_albumart table
    if (nameFilter.find(extension.lower()) > -1)
    {
        QString name = filename.section( '/', -1);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO music_albumart SET filename = :FILE, "
                      "directory_id = :DIRID, imagetype = :TYPE;");
        query.bindValue(":FILE", name);
        query.bindValue(":DIRID", m_directoryid[directory.utf8().lower()]);
        query.bindValue(":TYPE", AlbumArtImages::guessImageType(name));

        if (!query.exec() || query.numRowsAffected() <= 0)
        {
                MythContext::DBError("music insert artwork", query);
        }
        return;
    }

    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);

    if (decoder)
    {
        VERBOSE(VB_FILE, QString("Reading metadata from %1").arg(filename));
        Metadata *data = decoder->readMetadata();
        if (data) {

            QString album_cache_string;

            // Set values from cache
            if (m_directoryid[directory.utf8().lower()] > 0)
                data->setDirectoryId(m_directoryid[directory.utf8().lower()]);
            if (m_artistid[data->Artist().utf8().lower()] > 0)
            {
                data->setArtistId(m_artistid[data->Artist().utf8().lower()]);

                // The album cache depends on the artist id
                album_cache_string = data->getArtistId() + "#"
                    + data->Album().utf8().lower();

                if (m_albumid[album_cache_string] > 0)
                    data->setAlbumId(m_albumid[album_cache_string]);
            }
            if (m_genreid[data->Genre().utf8().lower()] > 0)
                data->setGenreId(m_genreid[data->Genre().utf8().lower()]);

            // Commit track info to database
            data->dumpToDatabase();

            // Update the cache
            m_artistid[data->Artist().utf8().lower()] = data->getArtistId();
            m_genreid[data->Genre().utf8().lower()] = data->getGenreId();
            album_cache_string = data->getArtistId() + "#"
                + data->Album().utf8().lower();
            m_albumid[album_cache_string] = data->getAlbumId();
            delete data;
        }

        delete decoder;
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

    query.exec("SELECT g.genre_id FROM music_genres g LEFT JOIN music_songs s "
               "ON g.genre_id=s.genre_id WHERE s.genre_id IS NULL;");
    while (query.next())
    {
        int genreid = query.value(0).toInt();
        deletequery.prepare("DELETE FROM music_genres WHERE genre_id=:GENREID");
        deletequery.bindValue(":GENREID", genreid);
        deletequery.exec();
    }

    if (clean_progress)
        clean_progress->SetProgress(++counter);

    query.exec("SELECT a.album_id FROM music_albums a LEFT JOIN music_songs s "
               "ON a.album_id=s.album_id WHERE s.album_id IS NULL;");
    while (query.next())
    {
        int albumid = query.value(0).toInt();
        deletequery.prepare("DELETE FROM music_albums WHERE album_id=:ALBUMID");
        deletequery.bindValue(":ALBUMID", albumid);
        deletequery.exec();
    }

    if (clean_progress)
        clean_progress->SetProgress(++counter);

    query.exec("SELECT a.artist_id FROM music_artists a "
               "LEFT JOIN music_songs s ON a.artist_id=s.artist_id "
               "LEFT JOIN music_albums l ON a.artist_id=l.artist_id "
               "WHERE s.artist_id IS NULL AND l.artist_id IS NULL");
    while (query.next())
    {
        int artistid = query.value(0).toInt();
        deletequery.prepare("DELETE FROM music_artists WHERE artist_id=:ARTISTID");
        deletequery.bindValue(":ARTISTID", artistid);
        deletequery.exec();
    }

    if (clean_progress)
        clean_progress->SetProgress(++counter);

    query.exec("SELECT a.albumart_id FROM music_albumart a LEFT JOIN "
               "music_songs s ON a.song_id=s.song_id WHERE "
               "embedded='1' AND s.song_id IS NULL;");
    while (query.next())
    {
        int albumartid = query.value(0).toInt();
        deletequery.prepare("DELETE FROM music_albumart WHERE albumart_id=:ALBUMARTID");
        deletequery.bindValue(":ALBUMARTID", albumartid);
        deletequery.exec();
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

    QString nameFilter = gContext->GetSetting("AlbumArtFilter",
                                              "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    if (nameFilter.find(extension) > -1)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM music_albumart WHERE filename= :FILE AND "
                      "directory_id= :DIRID;");
        query.bindValue(":FILE", sqlfilename);
        query.bindValue(":DIRID", m_directoryid[directory.utf8().lower()]);
        if (!query.exec() || query.numRowsAffected() <= 0)
        {
                MythContext::DBError("music delete artwork", query);
        }
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_songs WHERE "
                  "filename = :NAME ;");
    query.bindValue(":NAME", sqlfilename);
    query.exec();
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

    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);

    if (decoder)
    {
        Metadata *db_meta   = decoder->getMetadata();
        Metadata *disk_meta = decoder->readMetadata();

        if (db_meta && disk_meta)
        {
            disk_meta->setID(db_meta->ID());
            disk_meta->setRating(db_meta->Rating());

            QString album_cache_string;

            // Set values from cache
            if (m_directoryid[directory.utf8().lower()] > 0)
                disk_meta->setDirectoryId(m_directoryid[directory
                    .utf8().lower()]);
            if (m_artistid[disk_meta->Artist().utf8().lower()] > 0)
            {
                disk_meta->setArtistId(m_artistid[disk_meta->Artist()
                    .utf8().lower()]);

                // The album cache depends on the artist id
                album_cache_string = disk_meta->getArtistId() + "#"
                    + disk_meta->Album().utf8().lower();

                if (m_albumid[album_cache_string] > 0)
                    disk_meta->setAlbumId(m_albumid[album_cache_string]);
            }
            if (m_genreid[disk_meta->Genre().utf8().lower()] > 0)
                disk_meta->setGenreId(m_genreid[disk_meta->Genre()
                    .utf8().lower()]);

            // Commit track info to database
            disk_meta->dumpToDatabase();

            // Update the cache
            m_artistid[disk_meta->Artist().utf8().lower()]
                = disk_meta->getArtistId();
            m_genreid[disk_meta->Genre().utf8().lower()]
                = disk_meta->getGenreId();
            album_cache_string = disk_meta->getArtistId() + "#"
                + disk_meta->Album().utf8().lower();
            m_albumid[album_cache_string] = disk_meta->getAlbumId();
        }

        if (disk_meta)
            delete disk_meta;

        if (db_meta)
            delete db_meta;

        delete decoder;
    }
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
        if (*iter == kFileSystem)
            AddFileToDB(iter.key());
        else if (*iter == kDatabase)
            RemoveFileFromDB(iter.key ());
        else if (*iter == kNeedUpdate)
            UpdateFileInDB(iter.key());

        if (file_checking)
            file_checking->SetProgress(++counter);
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
    query.exec("SELECT CONCAT_WS('/', path, filename), date_modified "
               "FROM music_songs LEFT JOIN music_directories "
               "ON music_songs.directory_id=music_directories.directory_id "
               "WHERE filename NOT LIKE ('%://%')");

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
                    if (music_files[name] == kDatabase)
                    {
                        file_checking->SetProgress(++counter);
                        continue;
                    }
                    else if (HasFileChanged(name, query.value(1).toString()))
                        music_files[name] = kNeedUpdate;
                    else
                        music_files.remove(iter);
                }
                else
                {
                    music_files[name] = kDatabase;
                }
            }
            file_checking->SetProgress(++counter);
        }
    }

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
    query.exec("SELECT CONCAT_WS('/', path, filename) "
               "FROM music_albumart LEFT JOIN music_directories "
               "ON music_albumart.directory_id=music_directories.directory_id "
               "WHERE music_albumart.embedded=0");

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
                    if (music_files[name] == kDatabase)
                    {
                        if (file_checking)
                            file_checking->SetProgress(++counter);
                        continue;
                    }
                    else
                        music_files.remove(iter);
                }
                else
                {
                    music_files[name] = kDatabase;
                }
            }
            if (file_checking)
                file_checking->SetProgress(++counter);
        }
    }

    if (file_checking)
        file_checking->Close();
}
