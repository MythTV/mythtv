/*
	playlist.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	a list of things to play

*/

#include "playlist.h"

ClientPlaylist::ClientPlaylist(
                                int l_collection_id, 
                                QString new_name, 
                                QValueList<PlaylistEntry> *my_entries, 
                                uint my_id
                              )
{

    collection_id = l_collection_id;
    id = my_id;
    name = new_name;

    PlaylistEntryList::iterator it;
    for(it = my_entries->begin(); it != my_entries->end(); ++it)
    {
        entries.push_back((*it));
    }

}

ClientPlaylist::~ClientPlaylist()
{
}
