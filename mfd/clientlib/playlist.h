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

/*
    uint             getCount(){return song_references.count();}
    QValueList<int>  getList(){return song_references;}
    QValueList<int>* getDbList(){return &db_references;}
    void             addToList(int an_id);
    bool             removeFromList(int an_id);

    bool             internalChange(){ return internal_change; }
    void             internalChange(bool uh_huh_or_nope_not_me){internal_change = uh_huh_or_nope_not_me;}
    bool             waitingForList(){ return waiting_for_list; }
    void             waitingForList(bool uh_huh_or_nope_not_me){waiting_for_list = uh_huh_or_nope_not_me;}
    void             checkAgainstMetadata(QIntDict<Metadata> *the_metadata);
    void             addToIndirectMap(int key, int value);
    int              getFromIndirectMap(int key);
    void             removeFromIndirectMap(int key);
    QMap<int, int>*  getIndirectMap(){return &indirect_map;}
*/

  private:
  
    QString           name;
    PlaylistEntryList entries;
    uint              id;
    uint              collection_id;

};

#endif
