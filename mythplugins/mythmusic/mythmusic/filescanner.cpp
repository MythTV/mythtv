#include <sys/stat.h>

#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>

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

void FileScanner::BuildFileList(QString &directory, MusicLoadedMap &music_files, int parentid)
{
    QDir d(directory);

    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    /* Recursively traverse directory, calling QApplication::processEvents()
       every now and then to ensure the UI updates */
    int update_interval = 0;
    int newparentid = 0;
    while ((fi = it.current()) != 0)
    {
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

int FileScanner::GetDirectoryId(const QString &directory, const int &parentid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Load the directory id or insert it and get the id
    query.prepare("SELECT directory_id FROM music_directories "
                "WHERE path = :DIRECTORY ;");
    query.bindValue(":DIRECTORY", directory.utf8());

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
            query.bindValue(":DIRECTORY", directory.utf8());
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
            .arg(filename.local8Bit()));
    }
    return false;
}

void FileScanner::AddFileToDB(const QString &filename)
{
    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);

    if (decoder)
    {
        Metadata *data = decoder->readMetadata();
        if (data) {
            QString directory = filename;
            directory.remove(0, m_startdir.length());
            directory = directory.section( '/', 0, -2);

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

// Remove a file from the database
void FileScanner::RemoveFileFromDB (const QString &directory,
const QString &filename)
{
    QString sqlfilename(filename);
    // We know that the filename will not contain :// as the SQL limits this
    sqlfilename.remove(0, directory.length());
    sqlfilename = sqlfilename.section( '/', -1 ) ;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_songs WHERE "
                  "filename = :NAME ;");
    query.bindValue(":NAME", sqlfilename.utf8());
    query.exec();
}

void FileScanner::UpdateFileInDB(const QString &filename)
{
    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);

    if (decoder)
    {
        Metadata *db_meta   = decoder->getMetadata();
        Metadata *disk_meta = decoder->readMetadata();

        if (db_meta && disk_meta)
        {
            QString directory = filename;
            directory.remove(0, m_startdir.length());
            directory = directory.section( '/', 0, -2);

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

void FileScanner::SearchDir(QString &directory)
{

    m_startdir = directory;

    MusicLoadedMap music_files;
    MusicLoadedMap::Iterator iter;

    MythBusyDialog *busy = new MythBusyDialog(
        QObject::tr("Searching for music files"));

    busy->start();
    BuildFileList(m_startdir, music_files, 0);
    busy->Close();
    delete busy;

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT filename, date_modified, path "
               "FROM music_songs, music_directories "
               "WHERE filename NOT LIKE ('%://%') AND "
               "music_songs.directory_id=music_directories.directory_id");

    int counter = 0;

    MythProgressDialog *file_checking;
    file_checking = new MythProgressDialog(
        QObject::tr("Scanning music files"), query.numRowsAffected());

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString name;

            if (query.value(2).toString() != "")
            {
                name = m_startdir +
                    QString::fromUtf8(query.value(2).toString()) + "/" +
                    QString::fromUtf8(query.value(0).toString());
            }
            else
            {
                name = m_startdir +
                    QString::fromUtf8(query.value(0).toString());
            }

            if (name != QString::null)
            {
                if ((iter = music_files.find(name)) != music_files.end())
                {
                    if (music_files[name] == kDatabase)
                    {
                        file_checking->setProgress(++counter);
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
            file_checking->setProgress(++counter);
        }
    }

    file_checking->Close();
    delete file_checking;

    file_checking = new MythProgressDialog(
        QObject::tr("Updating music database"), music_files.size());

     /*
       This can be optimised quite a bit by consolidating all commands
       via a lot of refactoring.

       1) group all files of the same decoder type, and don't
       create/delete a Decoder pr. AddFileToDB. Or make Decoders be
       singletons, it should be a fairly simple change.

       2) RemoveFileFromDB should group the remove into one big SQL.

       3) UpdateFileInDB, same as 1.
     */

    counter = 0;
    for (iter = music_files.begin(); iter != music_files.end(); iter++)
    {
        if (*iter == kFileSystem)
            AddFileToDB(iter.key());
        else if (*iter == kDatabase)
            RemoveFileFromDB(directory, iter.key ());
        else if (*iter == kNeedUpdate)
            UpdateFileInDB(iter.key());

        file_checking->setProgress(++counter);
    }
    file_checking->Close();
    delete file_checking;
}
