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

#include "mdcontainer.h"
#include "../mfd/mfd.h"
#include "mfd_events.h"

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
    current_metadata = NULL;
    current_playlists = NULL;
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

bool MetadataContainer::isAudio()
{
    if(content_type == MCCT_audio)
    {
        return true;
    }
    return false;
}


bool MetadataContainer::isVideo()
{
    if(content_type == MCCT_video)
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

Metadata* MetadataContainer::getMetadata(int item_id)
{
    if(current_metadata)
    {
        return current_metadata->find(item_id);
    }
    return NULL;
}

Playlist* MetadataContainer::getPlaylist(int pl_id)
{
    if(current_playlists)
    {
        return current_playlists->find(pl_id);
    }
    return NULL;
}

void MetadataContainer::dataSwap(
                                    QIntDict<Metadata>* new_metadata, 
                                    QValueList<int> metadata_in,
                                    QValueList<int> metadata_out,
                                    QIntDict<Playlist>* new_playlists
                                )
{
    current_metadata = new_metadata;
    current_playlists = new_playlists;
    metadata_additions = metadata_in;
    metadata_deletions = metadata_out;
}

MetadataContainer::~MetadataContainer()
{
}

