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
                int my_id
            );
            
    ClientPlaylist(
                    int l_collection_id,
                    QString new_name
                  );

    virtual ~ClientPlaylist();

    int                         getId(){return id;}
    int                         getCollectionId(){return collection_id;}    
    void                        setCollectionId(int new_id){ collection_id = new_id; }
    QString                     getName(){return name;}
    int                         getCount(){return entries.count();}
    void                        setActualTrackCount(int x){ actual_track_count = x;}
    int                         getActualTrackCount(){ return actual_track_count; }
    QValueList<PlaylistEntry>*  getListPtr(){return &entries;}
    bool                        containsItem(int item_id);
    bool                        isEditable(){return is_editable;}
    void                        isEditable(bool y_or_n){ is_editable = y_or_n; }
    bool                        isRipable(){return is_ripable;}
    void                        isRipable(bool y_or_n){ is_ripable = y_or_n; }
    bool                        isBeingRipped(){ return is_being_ripped; }
    void                        setBeingRipped(bool y_or_n){ is_being_ripped = y_or_n; }

  private:
  
    QString           name;
    PlaylistEntryList entries;
    int               id;
    int               collection_id;
    int               actual_track_count;
    bool              is_editable;
    bool              is_ripable;
    bool              is_being_ripped;
};

#endif
