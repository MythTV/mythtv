#ifndef MFDCONTENT_H_
#define MFDCONTENT_H_
/*
	mfdcontent.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	thing that holds all content, playlists, tree's etc., and gets handed
	over to client code

*/

#include <qintdict.h>

class Metadata;
class AudioMetadata;
class ClientPlaylist;
class GenericTree;
class UIListGenericTree;
class MetadataCollection;

class MfdContentCollection
{
  public:
  
     MfdContentCollection(int an_id, int client_screen_width = 800, int client_screen_height = 600);
    ~MfdContentCollection();

    void addMetadata(Metadata *an_item, const QString &collection_name);
    void addPlaylist(ClientPlaylist *a_playlist, MetadataCollection *collection);
    void addNewPlaylistAbility(const QString &collection_name);
    void recursivelyAddSubPlaylist(
                                    UIListGenericTree *where_to_add, 
                                    MetadataCollection *collection, 
                                    int playlist_id, 
                                    int spot_counter
                                  );
    void addItemToAudioArtistTree(AudioMetadata *item, GenericTree *starting_point);
    void addItemToAudioGenreTree(AudioMetadata *item, GenericTree *starting_point);
    void addItemToAudioCollectionTree(AudioMetadata *item, const QString &collection_name);

    UIListGenericTree* getAudioArtistTree(){     return audio_artist_tree;     }
    UIListGenericTree* getAudioGenreTree(){      return audio_genre_tree;      }
    UIListGenericTree* getAudioPlaylistTree(){   return audio_playlist_tree;   }
    UIListGenericTree* getAudioCollectionTree(){ return audio_collection_tree; }
    UIListGenericTree* getNewPlaylistTree(){     return new_playlist_tree;     }
    UIListGenericTree* getEditablePlaylistTree(){return editable_playlist_tree;}
    
    AudioMetadata*  getAudioItem(int which_collection, int which_id);

    void sort();
    void setupPixmaps();
    
  private:

    int collection_id;

    QIntDict<AudioMetadata> audio_item_dictionary;

    UIListGenericTree *audio_artist_tree;
    UIListGenericTree *audio_genre_tree;
    UIListGenericTree *audio_playlist_tree;
    UIListGenericTree *audio_collection_tree;
    UIListGenericTree *new_playlist_tree;
    UIListGenericTree *editable_playlist_tree;
    
    int   client_width;
    int   client_height;
    float client_wmult;
    float client_hmult;
};

#endif
