/*
	playlist_entry.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	little thing that holds name, id, (etc?) of an entry on a playlist

*/

#include "playlistentry.h"

PlaylistEntry::PlaylistEntry()
{
    id = -1;
    name = "";
}

PlaylistEntry::PlaylistEntry(int an_id, const QString &a_name)
{
    id = an_id;
    name = a_name;
}

PlaylistEntry::~PlaylistEntry()
{
}
