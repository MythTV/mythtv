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

MetadataContainer::MetadataContainer(
                                        MFD *l_parent, 
                                        int l_unique_identifier,
                                        MetadataCollectionContentType  l_content_type,
                                        MetadataCollectionLocationType l_location_type
                                    )
{
    parent = l_parent;
    unique_identifier = l_unique_identifier;
    content_type = l_content_type;
    location_type = l_location_type;
}

void MetadataContainer::log(const QString &log_message, int verbosity)
{
    if(parent)
    {
        LoggingEvent *le = new LoggingEvent(log_message, verbosity);
        QApplication::postEvent(parent, le);
    }
}

void MetadataContainer::warning(const QString &warning_message)
{
    QString warn_string = warning_message;
    warn_string.prepend("WARNING: ");
    log(warn_string, 1);
}

bool MetadataContainer::tryToUpdate()
{
    warning("someone called tryToUpdate() on MetadataContainer base class");
    return false;
}

bool MetadataContainer::isAudio()
{
    if(content_type == MCCT_audio)
    {
        return true;
    }
    return false;
}


bool MetadataContainer::isLocal()
{
    if(location_type == MCLT_host)
    {
        return true;
    }
    return false;
}

uint MetadataContainer::getMetadataCount()
{
    if(current_metadata)
    {
        return current_metadata->count();
    }
    return 0;
}

uint MetadataContainer::getPlaylistCount()
{
    if(current_playlists)
    {
        return current_playlists->count();
    }
    return 0;
}

MetadataContainer::~MetadataContainer()
{
}

/*
---------------------------------------------------------------------
*/

MetadataMythDBContainer::MetadataMythDBContainer(
                                        MFD *l_parent, 
                                        int l_unique_identifier,
                                        MetadataCollectionContentType  l_content_type,
                                        MetadataCollectionLocationType l_location_type,
                                        QSqlDatabase *l_db
                                    )
                        :MetadataContainer(l_parent, l_unique_identifier, l_content_type, l_location_type)
{
    db = l_db;
    if(!db)
    {
        warning("metadata container was told to load metadata from a non existent database");
    }
    
    if(content_type == MCCT_audio)
    {
        //
        //  Ah, a mythmusic collection
        //

        metadata_monitor = new MetadataMythMusicMonitor(parent, unique_identifier, db);
        metadata_monitor->start();
    }

    current_metadata = NULL;
}

bool MetadataMythDBContainer::tryToUpdate()
{
    parent->lockMetadata();
    parent->lockPlaylists();

    bool return_value = metadata_monitor->getCurrentGeneration(current_metadata, current_playlists);

    parent->unlockMetadata();
    parent->unlockPlaylists();

    if(return_value)
    {
        log(QString("metadata mythdb container with id of %1 has updated its metadata (items = %2, playlists = %3)")
            .arg(unique_identifier)
            .arg(getMetadataCount())
            .arg(getPlaylistCount()), 4);

    }
    else
    {
        warning("metadata mythdb container wanted to update metadata, but there doesn't seem to be any new metadata");
    }
    return return_value;
}

MetadataMythDBContainer::~MetadataMythDBContainer()
{
}

