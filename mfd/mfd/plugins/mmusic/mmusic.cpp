/*
	mmusic.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Stay on top of MythMusic data

*/

#include <qdir.h>
#include <qfileinfo.h>
#include <qregexp.h>

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
    //  We just came into existence, so obviously we have not sent any
    //  warning yet
    //
    
    sent_directory_warning = false;
    sent_dir_is_not_dir_warning = false;
    sent_dir_is_not_readable_warning = false;
    sent_musicmetadata_table_warning = false;
    sent_playlist_table_warning = false;
    sent_database_version_warning = false;

    //
    //  This is a "magic" number signifying what we want to see
    //
    
    desired_database_version = "1001";

    //
    //  At startup we have no new data and no history. We create these lists
    //  (during a sweep), but the metadata server deletes them
    //

    new_metadata = NULL;
    previous_metadata.clear();

    new_playlists = NULL;
    previous_playlists.clear();

    //
    //  Get a metadata
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
        //  Check to see if our sweep interval has elapsed (default is 5 minutes)
        //

        //  int sweep_wait = mfdContext->getNumSetting("music_sweep_time", 5) * 60 * 1000;  
        int sweep_wait = mfdContext->getNumSetting("music_sweep_time", 5) * 1000;  
        if( metadata_sweep_time.elapsed() > sweep_wait || force_sweep)
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
                                                    new_playlists,
                                                    playlist_additions,
                                                    playlist_deletions
                                                 );
                
                //
                //  Something changed. Fire off an event (this will tell the
                //  container "above" me that it's time to update
                //
                
                MetadataChangeEvent *mce = new MetadataChangeEvent(container_id);
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

bool MMusicWatcher::checkDataSources(const QString &startdir, QSqlDatabase *a_db)
{

    //
    //  Make sure the place to look exists, is a directory, and is readable
    //            

    QFileInfo starting_directory(startdir);
    if (!starting_directory.exists())
    {
        if(!sent_directory_warning)
        {
            warning(QString("cannot look for files, directory "
                            "does not exist: \"%1\"")
                            .arg(startdir));
            sent_directory_warning = true;
        }
        return false;
    }

    if (!starting_directory.isDir())
    {
        if(!sent_dir_is_not_dir_warning)
        {
            warning(QString("cannot look for files, starting "
                            "point is not a directory: \"%1\"")
                            .arg(startdir));
            sent_dir_is_not_dir_warning = true;
        }
        return false;
    }

    if (!starting_directory.isReadable())
    {
        if(!sent_dir_is_not_readable_warning)
        {
            warning(QString("cannot look for files, starting "
                            "directory is not readable "
                            "(permissions?): \"%1\"")
                            .arg(startdir));
            sent_dir_is_not_readable_warning = true;
        }
        return false;
    }
    
    //
    //  Check the database version 
    //

    QString dbver = mfdContext->GetSetting("MusicDBSchemaVer");

    if (dbver != desired_database_version)
    {
        if(!sent_database_version_warning)
        {
            warning(QString("got desired (%1) versus actual (%2) "
                            "database mismatch, will not touch db")
                            .arg(desired_database_version)
                            .arg(dbver));
            sent_database_version_warning = true;
        }
        return false;
    }
                
    //
    //  Make sure the db exists, and we can see the two tables
    //
    
    QSqlQuery query("SELECT COUNT(filename) FROM musicmetadata;", a_db);
    
    if(!query.isActive())
    {
        if(!sent_musicmetadata_table_warning)
        {
            warning("cannot get data from a table called musicmetadata");
            sent_musicmetadata_table_warning = true;
        }
        return false;
        
    }
    
    QSqlQuery pl_query("SELECT COUNT(playlistid) FROM musicplaylist ", a_db);

    if(!pl_query.isActive())
    {
        if(!sent_playlist_table_warning)
        {
            warning("cannot get data from a table called musisplaylist");
            sent_playlist_table_warning = true;
        }
        return false;
        
    }
    
    sent_directory_warning = false;
    sent_dir_is_not_dir_warning = false;
    sent_dir_is_not_readable_warning = false;
    sent_musicmetadata_table_warning = false;
    sent_playlist_table_warning = false;
    sent_database_version_warning = false;
    return true;
}

bool MMusicWatcher::sweepMetadata()
{

    //
    //  Figure out where the file are.
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
    //  Check that the file location is valid and that the db is there and
    //  sensible
    //

    if(!checkDataSources(startdir, db))
    {
        prepareNewData();
        return doDeltas();
    }

    sweep_timer.start();
    log("beginning content sweep", 7);

    bool something_changed = false;
    
    if(first_time)
    {
        first_time = false;
        something_changed = true;
    }
    
    //
    //  Do a quick check of the playlists. This is a bit of a hack at the moment.
    //
    
    
    QString my_host_name = mfdContext->getHostName();
    QString quick_pl_query_string = QString("SELECT count(playlistid) FROM musicplaylist WHERE "
                                      "name != \"backup_playlist_storage\" "
                                      "AND name != \"default_playlist_storage\" "
                                      "AND hostname = \"%1\" ;")
                                      .arg(my_host_name);


    QSqlQuery quick_pl_query = db->exec(quick_pl_query_string);

        
    if(quick_pl_query.isActive())
    {
        if(quick_pl_query.numRowsAffected() > 0)
        {
            quick_pl_query.next();
            uint numb_db_playlists = quick_pl_query.value(0).toUInt();
            if(numb_db_playlists != previous_playlists.count())
            {
                something_changed = true;
            }
        }
        else
        {
            warning("got no playlist count");
        }
    }
    else
    {
        warning("fairly certain your playlist table is hosed ... giving up");
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
                        music_files[name] = MFL_both_file_and_database;
                    }
                    else
                    {
                        music_files[name] = MFL_in_myth_database;
                        something_changed = true;
                    }
                }
            }
        }
    }
    else
    {
        warning("could not seem to open your musicmetadata table ... giving up");
        return false;
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
                        .arg(name), 2);
            name.replace(quote_regex, "\"\"");
            name.remove(0,startdir.length());
            QString querystr = QString("DELETE FROM musicmetadata WHERE "
                                       "filename=\"%1\"").arg(name);
            query.exec(querystr);
            //music_files.remove(mf_iterator);
            something_changed = true;
        }
        else if(*mf_iterator == MFL_on_file_system)
        {
            if(checkNewMusicFile(mf_iterator.key(), startdir))
            {
                //
                //  If we've scanned say ... 10 files ... someone or
                //  something may well be waiting for music data to show up
                //  ... so we stop. The rest will get caught on the next
                //  sweep. Really. And the next sweep will come soon,
                //  because we set to sweep again right away.
                //
                //  Plus, this is also a good way to get back to the main
                //  run() loop fairly frequently, where we check if the mfd
                //  is trying to shut us down.
                //
                
                something_changed = true;
                ++files_scanned;
                if(files_scanned >= 10)
                {
                    force_sweep_mutex.lock();
                        force_sweep = true;
                    force_sweep_mutex.unlock();
                    break;
                }
            }
        }
    }

    if(!something_changed)
    {
        log(QString("sweep results: no changes in %1 second(s)")
                    .arg(sweep_timer.elapsed() / 1000.0), 6);
        return false;
    }

    //
    //  Make storage for new data, and zero out the deltas
    //
    
    prepareNewData();
    
    //
    //  Load the playlists
    //
        
    QString host_name = mfdContext->getHostName();
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
                                                        container_id,
                                                        pl_query.value(0).toString(), 
                                                        pl_query.value(1).toString(),
                                                        pl_query.value(2).toUInt()
                                                     );
                new_playlists->insert(pl_query.value(2).toUInt(), new_playlist);
            }
        }
        else
        {
            warning("found no playlists");
        }
    }
    else
    {
        warning("fairly certain your playlist table is hosed ... not good");
    }
    


    //
    //  Load the metadata
    //
        
    QString aquery =    "SELECT intid, artist, album, title, genre, "
                        "year, tracknum, length, filename, rating, "
                        "lastplay, playcount FROM musicmetadata ";

    QSqlQuery m_query = db->exec(aquery);

    if(m_query.isActive() && m_query.numRowsAffected() > 0)
    {
        while(m_query.next())
        {
            //
            //  fiddle with the fact that Qt and MySQL can't agree on a friggin
            //  DateTime format
            //
            
            QString timestamp = m_query.value(10).toString();

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
                                    m_query.value(0).toInt(),
                                    QUrl(startdir + m_query.value(8).toString()),
                                    m_query.value(9).toInt(),
                                    QDateTime::fromString(timestamp, Qt::ISODate),
                                    m_query.value(11).toInt(),
                                    false,
                                    m_query.value(1).toString(),
                                    m_query.value(2).toString(),
                                    m_query.value(3).toString(),
                                    m_query.value(4).toString(),
                                    m_query.value(5).toInt(),
                                    m_query.value(6).toInt(),
                                    m_query.value(7).toInt()
                                );

            new_metadata->insert(new_audio->getId(), new_audio);

        }
        
    }
    else
    {
        if(m_query.isActive())
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

    //
    //  Now, within this collection of metadata and playlists, we have a
    //  complete set. Time to map the numbers the playlists have to the
    //  metadata id's that exist now in memory
    //
            
    QIntDictIterator<Playlist> pl_it( *new_playlists );
    for ( ; pl_it.current(); ++pl_it )
    {
        pl_it.current()->mapDatabaseToId(new_metadata);
    }

    return doDeltas();

}

void MMusicWatcher::prepareNewData()
{

    new_metadata = new QIntDict<Metadata>;
    new_metadata->setAutoDelete(true);
    new_playlists = new QIntDict<Playlist>;
    new_metadata->setAutoDelete(true);
    
    metadata_additions.clear();
    metadata_deletions.clear();
    playlist_additions.clear();
    playlist_deletions.clear();
       
    
}

bool MMusicWatcher::doDeltas()
{

    //
    //  Ah Ha ... so ... we have a new set of metadata ... how does
    //  it compare to the old set (what are the deltas?)
    //
            
            
    //
    //  find new metadata (additions)
    //

    QIntDictIterator<Metadata> iter(*new_metadata);
    for (; iter.current(); ++iter)
    {
        int an_integer = iter.currentKey();
        if(previous_metadata.find(an_integer) == previous_metadata.end())
        {
            metadata_additions.push_back(an_integer);
        }
    }
            
    //
    //  find old metadata (deletions)
    //
            
    QValueList<int>::iterator other_iter;
    for ( other_iter = previous_metadata.begin(); other_iter != previous_metadata.end(); ++other_iter )
    {
        int an_integer = (*other_iter);
        if(!new_metadata->find(an_integer))
        {
            metadata_deletions.push_back(an_integer);
        }
    }


    //
    //  find new playlists (additions)
    //

    QIntDictIterator<Playlist> apl_iter(*new_playlists);
    for (; apl_iter.current(); ++apl_iter)
    {
        int an_integer = apl_iter.currentKey();
        if(previous_playlists.find(an_integer) == previous_playlists.end())
        {
            playlist_additions.push_back(an_integer);
        }
    }
            
    //
    //  find old playlists (deletions)
    //
            
    QValueList<int>::iterator ysom_iter;
    for ( ysom_iter = previous_playlists.begin(); ysom_iter != previous_playlists.end(); ++ysom_iter )
    {
        int an_integer = (*ysom_iter);
        if(!new_playlists->find(an_integer))
        {
            playlist_deletions.push_back(an_integer);
        }
    }

    //
    //  Make copies for next time through
    //    
    
    previous_metadata.clear();
    QIntDictIterator<Metadata> yano_iter(*new_metadata);
    for (; yano_iter.current(); ++yano_iter)
    {
        int an_integer = yano_iter.currentKey();
        previous_metadata.push_back(an_integer);
    }
            
    
    previous_playlists.clear();
    QIntDictIterator<Playlist> syano_iter(*new_playlists);
    for (; syano_iter.current(); ++syano_iter)
    {
        int an_integer = syano_iter.currentKey();
        previous_playlists.push_back(an_integer);
    }
            
    
    //
    //  Ok, final sanity check here. something_changed got set true, so we
    //  went through the whole exercise of rebuilding the metadata ... if
    //  our logic flawed, this if will fail
    //
    
    if(metadata_additions.count() != 0 ||
       metadata_deletions.count() != 0 ||
       playlist_additions.count() != 0 ||
       playlist_deletions.count() != 0 )
    {

        log(QString("sweep results: "
                    "%1 file(s) (+%2/-%3), "
                    "%4 playlists (+%5/-%6) "
                    "in %7 seconds.")

                    .arg(new_metadata->count())
                    .arg(metadata_additions.count())
                    .arg(metadata_deletions.count())
                    
                    .arg(new_playlists->count())
                    .arg(playlist_additions.count())
                    .arg(playlist_deletions.count())
        
                    .arg(sweep_timer.elapsed() / 1000.0)
                    , 8);

        return true;
    }

    //
    //  We're going to return false, which means this storage is not going
    //  to get used
    //
    
    delete new_metadata;
    new_metadata = NULL;
    delete new_playlists;
    new_playlists = NULL;
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
                        .arg(insert_title), 2);
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
    if(new_metadata)
    {
        delete new_metadata;
        new_metadata = NULL;
    }
    if(new_playlists)
    {
        delete new_playlists;
        new_playlists = NULL;
    }
    
}
