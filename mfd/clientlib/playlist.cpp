/*
	playlist.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	a list of things to play

*/

#include <iostream>
using namespace std;

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
    is_editable = false;

    PlaylistEntryList::iterator it;
    for(it = my_entries->begin(); it != my_entries->end(); ++it)
    {
        entries.push_back((*it));
    }

}

bool ClientPlaylist::containsItem(int item_id)
{
    PlaylistEntryList::iterator it;
    for(it = entries.begin(); it != entries.end(); ++it)
    {
        if(item_id < 0)
        {
            if ( (*it).isAnotherPlaylist() && (item_id * -1) == (*it).getId())
            {
                return true;
            }
        }
        else
        {
            if ( item_id == (*it).getId())
            {
                return true;
            }
        }
    }
    return false;
}

ClientPlaylist::~ClientPlaylist()
{
}
