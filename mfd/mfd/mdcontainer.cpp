/*
	mdcontainer.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	metadata container(s) and thread(s)

*/

#include "../config.h"

#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qdir.h>

#ifdef MYTHLIB_SUPPORT
#include <mythtv/mythcontext.h>
#endif


#include "mdcontainer.h"
#include "mfd.h"
#include "../mfdlib/mfd_events.h"

MetadataMonitor::MetadataMonitor(MFD *owner, QSqlDatabase *ldb)
{
    parent = owner;
    db = ldb;
    keep_going = true;
    metadata_generation = 0;

    //
    //  On startup, we want it to sweep
    //

    force_sweep = true;

    current_metadata = new QIntDict<AudioMetadata>;
    current_metadata_generation = 0;
    new_metadata = NULL;
    
    current_playlists = new QPtrList<MPlaylist>;
    
}

void MetadataMonitor::forceSweep()
{
    force_sweep_mutex.lock();
        force_sweep = true;
    force_sweep_mutex.unlock();
    main_wait_condition.wakeAll();
}

QIntDict<AudioMetadata>* MetadataMonitor::getCurrent(int *generation_value)
{
    *generation_value = current_metadata_generation;
    return current_metadata;
}

void MetadataMonitor::switchToNextGeneration()
{

    QIntDict<AudioMetadata> *old_metadata = current_metadata;
    current_metadata = new_metadata;
    if(old_metadata)
    {
        delete old_metadata;
    }
    current_metadata_generation = bumpMetadataGeneration();
    log(QString("metadata is now on generation %1").arg(current_metadata_generation), 3);
}


bool MetadataMonitor::sweepMetadata()
{

    bool return_value = false;

    log("metadata monitor is beginning a sweep", 2);

    //
    //  For the time being (temp) just load the metadata
    //
    
    new_metadata = new QIntDict<AudioMetadata>;
    
    metadata_item_mutex.lock();
        metadata_item = 0;
    metadata_item_mutex.unlock();    


    if(sweepAudio())
    {
        return_value = true;
    }
    
    
    force_sweep_mutex.lock();
        force_sweep = false;
    force_sweep_mutex.unlock();
    
    return return_value;
}

void MetadataMonitor::run()
{
    metadata_sweep_time.start();
    
    //
    //  NB: this is all pretty temporary at the moment
    //   
   
//    while(keep_going) // HACK
//    {
        //
        //  Check the metadata every every hour, or whenever something has
        //  set force sweep on. This is all a bit hacky temporary at the
        //  moment.
        //

        //if( metadata_sweep_time.elapsed() > 360000 
        //    || force_sweep)
        
        
        if( force_sweep)
        {
            if(sweepMetadata())
            {
                switchToNextGeneration();
            }
            metadata_sweep_time.restart();
        }
        else
        {
            main_wait_condition.wait(1000);
        }
//    }
}

void MetadataMonitor::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
}

int MetadataMonitor::bumpMetadataGeneration()
{
    metadata_generation_mutex.lock();
        ++metadata_generation;
    metadata_generation_mutex.unlock();
    return metadata_generation;
}

int MetadataMonitor::bumpMetadataItem()
{
    metadata_item_mutex.lock();
        ++metadata_item;
    metadata_item_mutex.unlock();
    return metadata_item;
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


void MetadataMonitor::buildFileList(QString &directory)
{
    QDir d(directory);

    if (!d.exists())
    {
        return;
    }

    const QFileInfoList *list = d.entryInfoList();

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
            buildFileList(filename);
        }
        else
        {
            //
            //  Change this to ask if there's an audio decoder that
            //  can handle this file
            //
            
            if( fi->extension(false) == "ogg" ||
                fi->extension(false) == "mp3" ||
                fi->extension(false) == "flac")
            {
                metadata_files[filename] = MFL_FileSystem;
            }
        }
    }

}

bool MetadataMonitor::checkAudio()
{
    //
    //  Build the file list
    //

    metadata_files.clear();
    
    //
    //  Make fallbacks for location much smarter
    //

    QString startdir = "/mnt/store";
#ifdef MYTHLIB_SUPPORT
        startdir = gContext->GetSetting("MusicLocation");
#endif
    if(startdir.length() > 0)
    {
        buildFileList(startdir);    
    }
    else
    {
        warning("no location for music files in database settings");
    }

    //
    //  Return true if something looks different from what's in
    //  current_metadata. This whole method is supposed to do light tests
    //  what's in the database versus what's on the disk, and only force a
    //  more expensive load if it looks likely to be worth it.
    //
    
    if(!db)
    {
        return true;
    }
    QString   q_string = "SELECT filename FROM musicmetadata ";
    QSqlQuery query(q_string, db);
    
    if(query.isActive() && query.numRowsAffected() > 0)
    {
/*
        while(query.next())
        {
            QString name = query.value(0).toString();
            if (name != QString::null)
            {
                if ((iter = music_files.find(name)) != music_files.end())
                {
                    music_files.remove(iter);
                else
                    music_files[name] = kDatabase;
            }
  
        }
*/
    }
    else
    {
        warning("metadata monitor can't seem to access the musicmetadata table");
    }    
    
    return true;
}

bool MetadataMonitor::sweepAudio()
{
    if(!db)
    {
        return false;
    }

    QString aquery =    "SELECT intid, artist, album, title, genre, "
                        "year, tracknum, length, filename, rating, "
                        "lastplay, playcount FROM musicmetadata ";

    QSqlQuery query = db->exec(aquery);

    if(!checkAudio())
    {
        return false;
    }

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
                                        query.value(0).toInt(),
                                        query.value(0).toInt(),
                                        QUrl(query.value(8).toString()),
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
        log(QString("metadata monitor found %1 audio files in latest sweep").arg(new_metadata->count()), 2);
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


#ifdef MYTHLIB_SUPPORT
    QString b_query = QString("SELECT name, songlist FROM musicplaylist WHERE "
                             "name != \"backup_playlist_storage\" "
                             "AND hostname = \"%1\" ;")
                             .arg(gContext->GetHostName());


    QSqlQuery bquery = db->exec(b_query);


        
    uint how_many = 1;
    MPlaylist *all_music_playlist = new MPlaylist("Everything", "-1", how_many);
    current_playlists->append(all_music_playlist);
    if(bquery.isActive() && bquery.numRowsAffected() > 0)
    {
        while(bquery.next())
        {
            ++how_many;
            MPlaylist *new_playlist = new MPlaylist(bquery.value(0).toString(), bquery.value(1).toString(), how_many);
            current_playlists->append(new_playlist);
        }
    }
#endif
    
    

    return true;
}


/*
---------------------------------------------------------------------
*/

MetadataContainer::MetadataContainer(MFD *owner, QSqlDatabase *ldb)
{
    parent = owner;
    db = ldb;
    metadata_monitor = new MetadataMonitor(parent, db);
    metadata_monitor->start();
    metadata_monitor->wait();   //  HACK
    current_audio_metadata = metadata_monitor->getCurrent(&current_generation);
    
    current_audio_playlists = metadata_monitor->getCurrentPlaylists();
    
    current_metadata_mutex = new QMutex(true);
    
    
    
    cout << "metadata has loaded and the metadata container sees a collection of size " << current_audio_metadata->count() << endl;
    mp3_count = 0;
}

void MetadataContainer::lockMetadata()
{
    current_metadata_mutex->lock();
}

void MetadataContainer::unlockMetadata()
{
    current_metadata_mutex->unlock();
}

void MetadataContainer::shutDown()
{
    metadata_monitor->stop();
    metadata_monitor->wait();
}

AudioMetadata* MetadataContainer::getMetadata(int *generation_value, int metadata_index)
{
    if(*generation_value == current_generation)
    {
        return current_audio_metadata->find(metadata_index);
    }
    *generation_value = current_generation;
    return NULL;
}

uint MetadataContainer::getMetadataCount()
{
    if(current_audio_metadata)
    {
        return current_audio_metadata->count();
    }
    return 0;
}

uint MetadataContainer::getMP3Count()
{
    
    uint return_value = mp3_count;

    if(return_value == 0)
    {
        QIntDictIterator<AudioMetadata> it(*current_audio_metadata); 
        for ( ; it.current(); ++it )
        {
            QUrl file_url = it.current()->getUrl();
            if(file_url.fileName().section('.',-1,-1) == "mp3")
            {
                ++return_value;
            }
        }
    }
    return return_value;
}

QIntDict<AudioMetadata>* MetadataContainer::getCurrentMetadata()
{
    return current_audio_metadata;
}

MetadataContainer::~MetadataContainer()
{
    if(metadata_monitor)
    {
        delete metadata_monitor;
        metadata_monitor = NULL;
    }
    if(current_metadata_mutex)
    {
        while(!current_metadata_mutex->tryLock())
        {
            current_metadata_mutex->unlock();
        }
        current_metadata_mutex->unlock();
        delete current_metadata_mutex;
        current_metadata_mutex = NULL;
    }
}
