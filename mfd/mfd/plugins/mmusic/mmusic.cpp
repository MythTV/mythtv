/*
	mmusic.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Stay on top of MythMusic data

*/

#include <qdir.h>
#include <qfileinfo.h>
#include <qregexp.h>
#include <qdatetime.h>

#include "mmusic.h"
#include "mfd_events.h"
#include "settings.h"
#include "decoder.h"



MMusicWatcher::MMusicWatcher(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 3689, "mythmusic watcher", false)
{
    //
    //  Get a reference to the metadata server
    //
    
    metadata_server = parent->getMetadataServer();
    
    //
    //  On startup, we want it to sweep
    //

    first_time = true;
    force_sweep = true;

    //
    //  At startup we have no new and no current data. Note that we have 2
    //  metadata lists, but only 1 playlist one. The metadata can change on
    //  disc (someone drops in a file), but the playlists can't. At some
    //  point (once there's a client!), the playlists will get changed and
    //  we'll need to persist them back. For now, we just load them and
    //  leave it at that
    //

    current_metadata = new QIntDict<Metadata>;
    new_metadata = new QIntDict<Metadata>;
    current_playlists = NULL;


    //
    //  Put these collections into a new container that the metadata server
    //  provides us
    //
    
    metadata_container = metadata_server->createContainer(MCCT_audio, MCLT_host);
    container_id = metadata_container->getIdentifier();
}

void MMusicWatcher::run()
{
    //
    //  pointer to the database
    //
    
    db = parent->getDatabase();
    if(!db)
    {
        warning("cant't talk to database ... I'm out of here");
        return;
    }

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

        if( metadata_sweep_time.elapsed() >  5000 || force_sweep)
        //if( metadata_sweep_time.elapsed() >  300000 || force_sweep)
        {
            //
            //  Toggle forced sweeping back off
            //

            force_sweep_mutex.lock();
                force_sweep = false;
            force_sweep_mutex.unlock();
            
            //
            //  Check to see if anything has changed
            //
            
            if(sweepMetadata())
            {
                //
                //  Do the atomic switchero (sing it with me now ...)
                //
                
                metadata_server->doAtomicDataSwap(  metadata_container, 
                                                    new_metadata, 
                                                    metadata_additions,
                                                    metadata_deletions,
                                                    current_playlists
                                                 );
                
                //
                //  Out with the old, in with the new
                //
                
                if(current_metadata)
                {
                    delete current_metadata;
                    current_metadata = NULL;
                }
                
                current_metadata = new_metadata;
                new_metadata = NULL;

                //
                //  Something changed. Fire off an event (this will tell the
                //  container "above" me that it's time to update
                //
                
                MetadataChangeEvent *mce = new MetadataChangeEvent(unique_identifier);
                QApplication::postEvent(parent, mce);    
            }
            
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

bool MMusicWatcher::sweepMetadata()
{

    //
    //  Make sure we have somewhere to look
    //

    QString startdir = mfdContext->GetSetting("MusicLocation");
    if(startdir.length() < 1)
    {
        warning("cannot look for music file paths "
                "starting with a null string");
        return false;
    }
    startdir = QDir::cleanDirPath(startdir);
    if(!startdir.endsWith("/"));
    {
        startdir += "/";
    }

    //
    //  Make sure the place to look exists, is a directory, and is readable
    //            

    QFileInfo starting_directory(startdir);
    if (!starting_directory.exists())
    {
        warning(QString("cannot look for files, directory "
                        "does not exist: \"%1\"")
                        .arg(startdir));
        return false;
    }

    if (!starting_directory.isDir())
    {
        warning(QString("cannot look for files, starting "
                        "point is not a directory: \"%1\"")
                        .arg(startdir));
        return false;
    }

    if (!starting_directory.isReadable())
    {
        warning(QString("cannot look for files, starting "
                        "directory is not readable "
                        "(permissions?): \"%1\"")
                        .arg(startdir));
        return false;
    }


    QTime sweep_timer;
    sweep_timer.start();
    log("beginning content sweep", 3);
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
        
        QString host_name = mfdContext->getHostName();
        QString pl_query_string = QString("SELECT name, songlist, playlistid FROM musicplaylist WHERE "
                                          "name != \"backup_playlist_storage\" "
                                          "AND name != \"default_playlist_storage\" "
                                          "AND hostname = \"%1\" ;")
                                          .arg(host_name);


        QSqlQuery pl_query = db->exec(pl_query_string);

        current_playlists = new QIntDict<Playlist>;
        current_playlists->setAutoDelete(true);
        
                                                                                                                                                                                                                                                                                                                                     
        
        if(pl_query.isActive())
        {
            if(pl_query.numRowsAffected() > 0)
            {
                while(pl_query.next())
                {
                    Playlist *new_playlist = new Playlist(
                                                            container_id,
                                                            pl_query.value(0).toString(), 
                                                            pl_query.value(1).toString(),
                                                            pl_query.value(2).toUInt()
                                                         );
                    current_playlists->insert(pl_query.value(2).toUInt(), new_playlist);
                }
            }
            else
            {
                warning("found no playlists");
            }
        }
        else
        {
            warning("fairly certain your playlist table is hosed");
        }
        
        
    }
    else
    {
        //
        //  As this is not the first time, we want to persist what's in memory
        //  back to the database
        //
        //  ... errr ... real soon now
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
    
    QSqlQuery query("SELECT filename, intid FROM musicmetadata;", db);

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
        warning("could not seem to open your musicmetadata table");
    }


    //
    //  Next step is to make the database reflect what's on the file system.
    //

    int files_scanned = 0;
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
            log(QString("removed metadata from db, file no longer exists: \"%1\"")
                        .arg(name), 8);
            name.replace(quote_regex, "\"\"");
            name.remove(0,startdir.length());
            QString querystr = QString("DELETE FROM musicmetadata WHERE "
                                       "filename=\"%1\"").arg(name);
            query.exec(querystr);
            music_files.remove(mf_iterator);
            something_changed = true;
            
        }
        else if(*mf_iterator == MFL_on_file_system)
        {
            if(checkNewMusicFile(mf_iterator.key(), startdir))
            {
                something_changed = true;
                
                //
                //  If we've scanned say ... 20 files ... someone may well
                //  be waiting for music data to show up ... so we stop. The
                //  rest will get caught on the next sweep. Really. And the next
                //  sweep will come soon, because we set to sweep again right away.
                //
                //  Plus, this is also a good way to get back to the main
                //  run() loop fairly frequently, where we check if the mfd
                //  is trying to shut us down.
                //
                
                ++files_scanned;
                if(files_scanned >= 20)
                {
                    force_sweep_mutex.lock();
                        force_sweep = true;
                    force_sweep_mutex.unlock();
                    break;
                }
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
                                        container_id,
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
            
            //
            //  Ah Ha ... so ... we have a new set of metadata ... how does
            //  it compare to the old set (what are the deltas!)
            //
            
            
            metadata_additions.clear();
            metadata_deletions.clear();
            
            //
            //  find everything that's new
            //

            QIntDictIterator<Metadata> iter(*new_metadata);
            for (; iter.current(); ++iter)
            {
                int an_integer = iter.currentKey();
                if(!current_metadata->find(an_integer))
                {
                    metadata_additions.push_back(an_integer);
                }
            }
            
            //
            //  find everything that's old
            //
            
            QIntDictIterator<Metadata> another_iter(*current_metadata);
            for (; another_iter.current(); ++another_iter)
            {
                int an_integer = another_iter.currentKey();
                if(!new_metadata->find(an_integer))
                {
                    metadata_deletions.push_back(an_integer);
                }
            }
            
            log(QString("found %1 file(s) and %2 playlist(s) "
                        "in %3 second(s) with %4 additions and %5 deletions ")
                       .arg(new_metadata->count())
                       .arg(current_playlists->count())
                       .arg(sweep_timer.elapsed() / 1000.0)
                       .arg(metadata_additions.count())
                       .arg(metadata_deletions.count())
                       , 2);
                       
            
            return true;
        }
        else
        {
            if(query.isActive())
            {
                warning("found no entries while sweeping audio database");
                return false;
            }
            else
            {
                warning("fairly certain the audio database is screwed up");
                return false;
            }
        }
        
    }
    else
    {
        log(QString("sweep completed with no changes found in %1 second(s)")
                    .arg(sweep_timer.elapsed() / 1000.0), 3);
    }
    
    return false;
}

void MMusicWatcher::buildFileList(QString &directory, MusicLoadedMap &music_files)
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


bool MMusicWatcher::checkNewMusicFile(const QString &filename, const QString &startdir)
{
    //
    //  Have we seen this puppy before ?
    //
    
    if(files_to_ignore.find(filename) != files_to_ignore.end())
    {
        return false;
    }
    
    //
    //  We have found a new file. Either it should get added to the database
    //  (if it's supported and decodable) or we log a warning and then ignore it.
    //

    
    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);
    
    if(decoder)
    {
        AudioMetadata *new_metadata = decoder->getMetadata();
        if(new_metadata)
        {
            //
            //  We have a valid, playable new file, we need to stick it in
            //  the database
            //
            
            QString insert_artist = new_metadata->getArtist();
            QString insert_album =  new_metadata->getAlbum();
            QString insert_title =  new_metadata->getTitle();
            QString insert_genre =  new_metadata->getGenre();
            QString insert_filename = filename;
                    insert_filename.remove(0, startdir.length());
            
            
            if(insert_title == "")
            {
                insert_title = filename;
            }
            
            insert_artist.replace(QRegExp("\""), QString("\\\""));
            insert_album.replace(QRegExp("\""), QString("\\\""));
            insert_title.replace(QRegExp("\""), QString("\\\""));
            insert_genre.replace(QRegExp("\""), QString("\\\""));
            insert_filename.replace(QRegExp("\""), QString("\\\""));

            QString thequery = QString("INSERT INTO musicmetadata (artist,album,title,"
                               "genre,year,tracknum,length,filename) VALUES "
                               "(\"%1\",\"%2\",\"%3\",\"%4\",%5,%6,%7,\"%8\");")
                              .arg(insert_artist.latin1())
                              .arg(insert_album.latin1())
                              .arg(insert_title.latin1())
                              .arg(insert_genre.latin1())
                              .arg(new_metadata->getYear())
                              .arg(new_metadata->getTrack())
                              .arg(new_metadata->getLength())
                              .arg(insert_filename);
            db->exec(thequery);
            log(QString("found and inserted new music item called \"%1\"")
                        .arg(insert_title), 8);
            delete new_metadata;
            return true;
        }
        else
        {
            warning(QString("found file that is not openable/decodable: \"%1\"")
                            .arg(filename));
            files_to_ignore.push_back(filename);
        }
    }
    else
    {
        //
        //  Put it on the ignore list
        //   
        
        warning(QString("can not find a decoder for this file: \"%1\"")
                .arg(filename));
        files_to_ignore.push_back(filename);
    }
    return false;
}


MMusicWatcher::~MMusicWatcher()
{
    if(current_metadata)
    {
        delete current_metadata;
    }
    if(new_metadata)
    {
        delete new_metadata;
    }
    if(current_playlists)
    {
        delete current_playlists;
    }
}
