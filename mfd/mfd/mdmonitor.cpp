/*
	mdmonitor.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	metadata monitors

*/

#include "../config.h"

#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qdir.h>
#include <qmap.h>
#include <qregexp.h>

#ifdef MYTHLIB_SUPPORT
#include <mythtv/mythcontext.h>
#endif


#include "mdmonitor.h"
#include "mfd.h"
#include "../mfdlib/mfd_events.h"

MetadataMonitor::MetadataMonitor(MFD *l_parent, int l_unique_identifier)
{
    parent = l_parent;
    keep_going = true;
    first_time = true;
    unique_identifier = l_unique_identifier;


    //
    //  On startup, we want it to sweep
    //

    force_sweep = true;


    //
    //  Create new (and empty) current metadata collections and playlists
    //

    current_metadata = new QIntDict<Metadata>;
    current_metadata->setAutoDelete(true);
    new_metadata = NULL;

    current_playlists = new QIntDict<Playlist>;
    current_playlists->setAutoDelete(true);

}


void MetadataMonitor::forceSweep()
{
    force_sweep_mutex.lock();
        force_sweep = true;
    force_sweep_mutex.unlock();
    main_wait_condition.wakeAll();
}


bool MetadataMonitor::sweepMetadata()
{
    warning("something called sweepMetadata() in the base MetadataMonitor() class");
    return false;
}

void MetadataMonitor::run()
{
    //
    //  Set a time stamp
    //

    metadata_sweep_time.start();
    
    while(keep_going)
    {
        //
        //  Check the metadata every 5 minutes, or whenever something has
        //  set force sweep on.
        //

        //if( metadata_sweep_time.elapsed() >  5000 || force_sweep)
        if( metadata_sweep_time.elapsed() >  300000 || force_sweep)
        {
            //
            //  Check to see if anything has changed
            //
            
            if(sweepMetadata())
            {
                //
                //  Something changed. Fire off an event (this will tell the
                //  container "above" me that it's time to update
                //
                
                MetadataChangeEvent *mce = new MetadataChangeEvent(unique_identifier);
                QApplication::postEvent(parent, mce);    
            }
            
            //
            //  Toggle forced sweeping back off
            //
            
            force_sweep_mutex.lock();
                force_sweep = false;
            force_sweep_mutex.unlock();
            
            //
            //  In any case, reset the time stamp
            //

            metadata_sweep_time.restart();

        }
        else
        {
            //
            //  Sleep for a while, unless someone wakes us up
            //

            main_wait_condition.wait(1000);
        }
    }
}

void MetadataMonitor::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
        main_wait_condition.wakeAll();
    keep_going_mutex.unlock();
}

void MetadataMonitor::log(const QString &log_message, int verbosity)
{
    log_mutex.lock();
        if(parent)
        {
            LoggingEvent *le = new LoggingEvent(log_message, verbosity);
            QApplication::postEvent(parent, le);    
        }
    log_mutex.unlock();
}

void MetadataMonitor::warning(const QString &warning_message)
{
    warning_mutex.lock();
        QString warn_string = warning_message;
        warn_string.prepend("WARNING: ");
        log(warn_string, 1);
    warning_mutex.unlock();
}


bool MetadataMonitor::getCurrentGeneration(QIntDict<Metadata>*  &metadata_pointer, QIntDict<Playlist>* &playlists_pointer)
{
    //
    //  We lock everything and switch over to the new metadata, if there's
    //  something to switch to
    //

    if(!new_metadata)
    {
        warning("metadata monitor asked to switch over to new metadata, but there isn't any");
        return false;
    }

    metadata_mutex.lock();
    
    
        //
        //  Swap over the metadata
        //

        QIntDict<Metadata>  *temp_metadata;
        temp_metadata = current_metadata;
        current_metadata = new_metadata;

        if(temp_metadata)
        {
            temp_metadata->clear();
            delete temp_metadata;
        }
        metadata_pointer = current_metadata;
        new_metadata = NULL;
    
        //
        //  Playlist is not swapped, just pointed to
        //

        playlists_pointer = current_playlists;
    
    metadata_mutex.unlock();

    return true;
}


/*
---------------------------------------------------------------------
*/

MetadataMythMusicMonitor::MetadataMythMusicMonitor(MFD *l_parent, int l_unique_identifier, QSqlDatabase *l_db)
                         :MetadataMonitor(l_parent, l_unique_identifier)
{
    db = l_db;

    
}

void MetadataMythMusicMonitor::buildFileList(QString &directory, MusicLoadedMap &music_files)
{
    //
    //  Recursively search to get all music files
    //

    QDir current_directory(directory);

    if (!current_directory.exists())
    {
        return;
    }

    const QFileInfoList *list = current_directory.entryInfoList();
    if (!list)
    {
        return;
    }

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
        {
            continue;
        }
        QString filename = fi->absFilePath();
        if (fi->isDir())
        {
            //
            //  Recursively call myself
            //

            buildFileList(filename, music_files);
        }
        else
        {
            //
            //  If this is a music file (based on file extensions),
            //  then include it in the list
            //
            
            if(
                fi->extension(FALSE) == "ogg"  ||
                fi->extension(FALSE) == "flac" ||
                fi->extension(FALSE) == "mp3"  ||
                fi->extension(FALSE) == "aac"  ||
                fi->extension(FALSE) == "m4a"  ||
                fi->extension(FALSE) == "mp4"
              )
            {
                music_files[filename] = MFL_on_file_system;
            }
        }
    }
}


bool MetadataMythMusicMonitor::sweepMetadata()
{

    log("metadata mythmusic monitor is beginning content sweep", 3);

    bool something_changed = false;
    if(first_time)
    {
        //
        //  We know we have to rebuild the in-memory Metadata if this is our
        //  first run
        //
        
        first_time = false;
        something_changed = true;
        
        //
        //  Since this is the first time, we need to load the playlists
        //
        
        QString host_name = "";
#ifdef MYTHLIB_SUPPORT
        host_name = gContext->GetHostName();
#else
        warning("metadata mythmusic monitor has no gContext ... this should not happen ... very bad");
        return false;
#endif

        QString pl_query_string = QString("SELECT name, songlist, playlistid FROM musicplaylist WHERE "
                                          "name != \"backup_playlist_storage\" "
                                          "AND name != \"default_playlist_storage\" "
                                          "AND hostname = \"%1\" ;")
                                          .arg(host_name);


        QSqlQuery pl_query = db->exec(pl_query_string);


                                                                                                                                                                                                                                                                                                                                     
        
        if(pl_query.isActive())
        {
            if(pl_query.numRowsAffected() > 0)
            {
                while(pl_query.next())
                {
                    Playlist *new_playlist = new Playlist(
                                                            pl_query.value(0).toString(), 
                                                            pl_query.value(1).toString(),
                                                            parent->bumpPlaylistId(), 
                                                            pl_query.value(2).toUInt()
                                                         );
                    current_playlists->insert(new_playlist->getId(), new_playlist);
                }
            }
            else
            {
                warning("metadata mythmusic monitor found no playlists");
            }
        }
        else
        {
            warning("metadata mythmusic monitor is fairly certain your playlist table is hosed");
        }
        
        
    }
    else
    {
        //
        //  As this is not the first time, we want to persist what's in memory
        //  back to the database
        //


        //  soon
    }
    
    QString startdir = "";
#ifdef MYTHLIB_SUPPORT
    startdir = gContext->GetSetting("MusicLocation");
#else
    warning("metadata mythmusic monitor has no gContext ... this should not happen ... things will now get wonky");
    return false;
#endif

    if(startdir.length() < 1)
    {
        warning("metadata mythmusic monitor cannot look for music file paths starting with a null string");
        return false;
    }

    //
    //  Create a collection of variables that store flags about whether a
    //  file is on disk, in the database, etc. using the filename as the
    //  key, and in iterator to iterate over them;
    //
    
    MusicLoadedMap music_files;
    MusicLoadedMap::Iterator mf_iterator;

    //
    //  Call buildFileList(), which will recurse through all directories
    //  below "startdir" and put any music file it finds int the music_files
    //  list
    //

    buildFileList(startdir, music_files);

    //
    //  Now, see what's in the database
    //    
    
    QSqlQuery query("SELECT filename FROM musicmetadata;", db);

    if (query.isActive())
    {
        if(query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                QString name = startdir;
                name += query.value(0).toString();
                if (name != QString::null)
                {
                    if ((mf_iterator = music_files.find(name)) != music_files.end())
                    {
                        music_files.remove(mf_iterator);
                    }
                    else
                    {
                        music_files[name] = MFL_in_myth_database;
                    }
                }
            }
        }
    }
    else
    {
        warning("metadata mythmusic monitor could not seem to open your musicmetadata table");
    }


    //
    //  Next step is to make the database reflect what's on the file system.
    //

    QRegExp quote_regex("\"");
    for (mf_iterator = music_files.begin(); mf_iterator != music_files.end(); ++mf_iterator)
    {

        if (*mf_iterator == MFL_in_myth_database)
        {
            //
            //  It's in the database, but not on the file system, so delete
            //  from database
            //
            
            
            QString name(mf_iterator.key());
            name.replace(quote_regex, "\"\"");
            QString querystr = QString("DELETE FROM musicmetadata WHERE "
                                       "filename=\"%1\"").arg(name);
            query.exec(querystr);
            music_files.remove(mf_iterator);
            something_changed = true;
            
        }
        else if(*mf_iterator == MFL_on_file_system)
        {
            if(checkNewMusicFile(mf_iterator.key()))
            {
                something_changed = true;
            }
        }

    }


    //
    //  OK, build the new metadata list of music files in memory if any of
    //  the above actually showed that the status of music files has changed
    //

    if(something_changed)
    {
    
        //
        //  Make a new collection
        //

        metadata_mutex.lock();
        if(new_metadata)
        {
            //
            //  We seem to be building collections faster than the rest of
            //  the mfd can switch over to them
            //
            
            warning("metadata mythmusic monitor is buidling a new metadata collection on top of an existing one");
            delete new_metadata;
        }

        new_metadata = new QIntDict<Metadata>;
        new_metadata->setAutoDelete(true);
    
        //
        //  Load the metadata
        //
        
        QString aquery =    "SELECT intid, artist, album, title, genre, "
                            "year, tracknum, length, filename, rating, "
                            "lastplay, playcount FROM musicmetadata ";

        QSqlQuery query = db->exec(aquery);

        if(query.isActive() && query.numRowsAffected() > 0)
        {
            while(query.next())
            {
                //
                //  fiddle with the fact that Qt and MySQL can't agree on a friggin
                //  DateTime format
                //
            
                QString timestamp = query.value(10).toString();

                if(timestamp.contains('-') < 1)
                {
                    timestamp.insert(4, '-');
                    timestamp.insert(7, '-');
                    timestamp.insert(10, 'T');
                    timestamp.insert(13, ':');
                    timestamp.insert(16, ':');
                }

                AudioMetadata *new_audio = new AudioMetadata
                                    (
                                        parent->bumpMetadataId(),
                                        query.value(0).toInt(),
                                        QUrl(startdir + query.value(8).toString()),
                                        query.value(9).toInt(),
                                        QDateTime::fromString(timestamp, Qt::ISODate),
                                        query.value(11).toInt(),
                                        false,
                                        query.value(1).toString(),
                                        query.value(2).toString(),
                                        query.value(3).toString(),
                                        query.value(4).toString(),
                                        query.value(5).toInt(),
                                        query.value(6).toInt(),
                                        query.value(7).toInt()
                                    );
            
                new_metadata->insert(new_audio->getId(), new_audio);

            }
        
            //
            //  Now, within this collection of metadata and playlists, we have a
            //  complete set. Time to map the numbers the playlists have to the
            //  metadata id's that exist now in memory
            //
            
            QIntDictIterator<Playlist> pl_it( *current_playlists );
            for ( ; pl_it.current(); ++pl_it )
            {
                pl_it.current()->mapDatabaseToId(new_metadata);
            }
            
            metadata_mutex.unlock();
            log(QString("metadata mythmusic monitor found %1 audio files and %2 playlists in latest sweep")
                       .arg(new_metadata->count())
                       .arg(current_playlists->count()), 2);
            return true;
        }
        else
        {
            if(query.isActive())
            {
                warning("metadata monitor found no entries while sweeping audio database");
                return false;
            }
            else
            {
                warning("metadata monitor is fairly certain the audio database is screwed up");
                return false;
            }
        }
        
    }
    else
    {
        log("metadata mythmusic monitor sweep completed with no changes found ", 3);
    }
    
    
    
    
    
    return false;

}

bool MetadataMythMusicMonitor::checkNewMusicFile(const QString &filename)
{
    cout << "Checking: " << filename << endl;
    return true;
}

