#ifndef PLAYLIST_H_
#define PLAYLIST_H_
/*
	playlist.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	a list of things to play

*/

#include <qstring.h>
#include <qvaluelist.h>

#include "playlistentry.h"




class ClientPlaylist
{
  public:
    
    typedef QValueList<PlaylistEntry> PlaylistEntryList;
      
    ClientPlaylist(
                int l_collection_id, 
                QString new_name, 
                PlaylistEntryList *my_entries, 
                uint my_id
            );

    virtual ~ClientPlaylist();

    uint                       getId(){return id;}
    uint                       getCollectionId(){return collection_id;}    
    QString                    getName(){return name;}
    uint                       getCount(){return entries.count();}
    QValueList<PlaylistEntry>* getListPtr(){return &entries;}

  private:
  
    QString           name;
    PlaylistEntryList entries;
    uint              id;
    uint              collection_id;

};

#endif
