/*
	mfdcontent.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	thing that holds all content, playlists, tree's etc., and gets handed
	over to client code

*/

#include <iostream>
using namespace std;

#include <qpair.h>

#include <mythtv/uilistbtntype.h>
#include "mfdcontent.h"
#include "../mfdlib/metadata.h"
#include "playlist.h"

MfdContentCollection::MfdContentCollection(int an_id)
{
    collection_id = an_id;
    
    audio_item_dictionary.resize(9973); //  large prime
    audio_item_dictionary.setAutoDelete(true);
    
    //
    //  Make the core tree branches
    //
    
    audio_artist_tree = new UIListGenericTree(NULL, "All by Artist");
    audio_genre_tree = new UIListGenericTree(NULL, "All by Genre");
    audio_playlist_tree = new UIListGenericTree(NULL, "All Playlists");
    audio_collection_tree =  new UIListGenericTree(NULL, "Grouped by Collection");
}

void MfdContentCollection::addMetadata(Metadata *new_item, const QString &collection_name)
{
    //
    //  At this point, we only understand audio metadata. If it's not audio
    //  metadata, complain
    //
    
    if(new_item->getType() != MDT_audio)
    {
        cerr << "mfdcontent.o: no idea what to do with "
             << "non audio content"
             << endl;
        return;
    }
    
    if(AudioMetadata *copy = dynamic_cast<AudioMetadata*>(new_item) )
    {
        //
        //  Because this object gets constructed and then passed off to the
        //  actual client, we need to make a copy of everything
        //
    
        AudioMetadata *audio_copy = new AudioMetadata(*copy);

        //
        //  Stuff this into the audio item dictionary with an int key of
        //  it's container id and item id. It goes into a QIntDict 'cause
        //  that makes it fast to look it up from the client code (ie. user
        //  navigates a tree to a node, hits enter; signal comes back with
        //  item id and container id, use those to look up the full item
        //  from this dictionary)
        //

        int dictionary_key =   audio_copy->getCollectionId()
                            * METADATA_UNIVERSAL_ID_DIVIDER
                            + audio_copy->getId();

        audio_item_dictionary.insert(dictionary_key, audio_copy);
        
        //
        //  Now, put it on all the required tree places.
        //

        if(!audio_copy->isDuplicate())
        {
            addItemToAudioArtistTree(audio_copy, audio_artist_tree);
            addItemToAudioGenreTree(audio_copy, audio_genre_tree);
        }

        addItemToAudioCollectionTree(audio_copy, collection_name);

    }
    else
    {
        cerr << "mfdcontent.o failed to cast audio content as "
             << "audio content. Bah!"
             << endl;
    }

}

void MfdContentCollection::addPlaylist(ClientPlaylist *new_playlist, const QString &collection_name)
{

    //
    //  Create a node for this playlist in the audio playlist tree
    //

    UIListGenericTree *playlist_node = new UIListGenericTree(audio_playlist_tree, new_playlist->getName());
    playlist_node->setAttribute(0, 0);
    playlist_node->setAttribute(1, 0);

    //
    //  Find or "create as we go" another node for this playlist in the "By
    //  Collection Tree"
    //
    
    GenericTree *by_playlist_node = NULL;

    cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Am trying to add a playlist called \""
         << new_playlist->getName()
         << "\" to a collection called \""
         << collection_name
         << "\""
         << endl;
         

    GenericTree *collection_node = audio_collection_tree->getChildByName(collection_name);
    if(!collection_node)
    {
        cout << "created collection_node" << endl;
        collection_node = new UIListGenericTree(audio_collection_tree, collection_name);
        
        //
        //  We just made the node for this collection, it needs Playlist node
        //

        by_playlist_node = new UIListGenericTree((UIListGenericTree *)collection_node, "Playlist");
        cout << "created by_playlist_node" << endl;
    }
    else
    {
        by_playlist_node = collection_node->getChildByName("Playlist");
        if(!by_playlist_node)
        {
            by_playlist_node = by_playlist_node = new UIListGenericTree((UIListGenericTree *)collection_node, "Playlist");
        }
    }

    UIListGenericTree *collection_playlist_node = new UIListGenericTree((UIListGenericTree *)by_playlist_node, new_playlist->getName());

    //
    //  Iterate over the entries in this playlist
    //
            
    int counter = 0;
    QValueList<PlaylistEntry> *the_list = new_playlist->getListPtr();
    QValueList<PlaylistEntry>::iterator l_it;
    for(l_it = the_list->begin(); l_it != the_list->end(); ++l_it)
    {
        UIListGenericTree *track_node = new UIListGenericTree(playlist_node, (*l_it).getName());
        track_node->setInt(counter);
        track_node->setAttribute(0, collection_id);
        track_node->setAttribute(1, 2);
        track_node->setAttribute(2, new_playlist->getId());

        UIListGenericTree *o_track_node = new UIListGenericTree(collection_playlist_node, (*l_it).getName());
        o_track_node->setInt(counter);
        o_track_node->setAttribute(0, collection_id);
        o_track_node->setAttribute(1, 2);
        o_track_node->setAttribute(2, new_playlist->getId());
        ++counter;
    }
    
}

void MfdContentCollection::addItemToAudioArtistTree(AudioMetadata *item, GenericTree *starting_point)
{
    QString album  = item->getAlbum();
    QString artist = item->getArtist();
    QString title  = item->getTitle();
            
    //
    // The Artist --> Album --> Track   branch
    //
            
    GenericTree *artist_node = starting_point->getChildByName(artist);
    if(!artist_node)
    {
        artist_node = new UIListGenericTree((UIListGenericTree *) starting_point, artist);
        artist_node->setAttribute(0, 0);
        artist_node->setAttribute(1, 0);
    }


    GenericTree *album_node = artist_node->getChildByName(album);
    if(!album_node)
    {
        album_node = new UIListGenericTree((UIListGenericTree *) artist_node, album);
        album_node->setAttribute(0, 0);
        album_node->setAttribute(1, 0);
    }
            
    UIListGenericTree *title_node = new UIListGenericTree((UIListGenericTree *) album_node, title);
    title_node->setInt(item->getId());
    title_node->setAttribute(0, item->getCollectionId());
    title_node->setAttribute(1, 1);

}

void MfdContentCollection::addItemToAudioGenreTree(AudioMetadata *item, GenericTree *starting_point)
{
    QString genre  = item->getGenre();
    QString album  = item->getAlbum();
    QString artist = item->getArtist();
    QString title  = item->getTitle();
            
    //
    // The Genre --> Artist --> Album --> Track   branch
    //

    GenericTree *genre_node = starting_point->getChildByName(genre);
    if(!genre_node)
    {
        genre_node = new UIListGenericTree((UIListGenericTree *)starting_point, genre);
        genre_node->setAttribute(0, 0);
        genre_node->setAttribute(1, 0);
    }

    GenericTree *artist_node = genre_node->getChildByName(artist);
    if(!artist_node)
    {
        artist_node = new UIListGenericTree((UIListGenericTree *)genre_node, artist);
        artist_node->setAttribute(0, 0);
        artist_node->setAttribute(1, 0);
    }

    GenericTree *album_node = artist_node->getChildByName(album);
    if(!album_node)
    {
        album_node = new UIListGenericTree((UIListGenericTree *) artist_node, album);
        album_node->setAttribute(0, 0);
        album_node->setAttribute(1, 0);
    }
            
    UIListGenericTree *title_node = new UIListGenericTree((UIListGenericTree *) album_node, title);
    title_node->setInt(item->getId());
    title_node->setAttribute(0, item->getCollectionId());
    title_node->setAttribute(1, 1);
}

void MfdContentCollection::addItemToAudioCollectionTree(AudioMetadata *item, const QString &collection_name)
{

    QString genre  = item->getGenre();
    QString album  = item->getAlbum();
    QString artist = item->getArtist();
    QString title  = item->getTitle();

    GenericTree *by_artist_node = NULL;
    GenericTree *by_genre_node = NULL;
     
            
    //
    // By Collection --> Genre/Artist, etc.
    //
            
    GenericTree *collection_node = audio_collection_tree->getChildByName(collection_name);
    if(!collection_node)
    {
        collection_node = new UIListGenericTree(audio_collection_tree, collection_name);
        
        //
        //  We just made the node for this collection, it needs Artist and Genre subnodes
        //

        by_artist_node = new UIListGenericTree((UIListGenericTree *)collection_node, "By Artist");
        by_genre_node = new UIListGenericTree((UIListGenericTree *)collection_node, "By Genre");
    }
    else
    {
        by_artist_node = collection_node->getChildByName("By Artist");
        by_genre_node = collection_node->getChildByName("By Genre");
    }

    //
    //  Ok, the collection node exists, and it now has "By Artist" and "By
    //  Genre" subnodes. But the new item on the "By Artist" branch.
    //

    addItemToAudioArtistTree(item, by_artist_node);        

    //
    //  Now put it on the "By Genre:
    //    
    
    addItemToAudioGenreTree(item, by_genre_node);

}

AudioMetadata* MfdContentCollection::getAudioItem(int which_collection, int which_id)
{
    //
    //  What could be simpler. Fast and easy.
    //

    int dictionary_key =   which_collection
                        * METADATA_UNIVERSAL_ID_DIVIDER
                        + which_id;


    return audio_item_dictionary[dictionary_key];
    
}

MfdContentCollection::~MfdContentCollection()
{
    audio_item_dictionary.clear();

    if(audio_artist_tree)
    {
        delete audio_artist_tree;
        audio_artist_tree = NULL;
    }

    if(audio_genre_tree)
    {
        delete audio_genre_tree;
        audio_genre_tree = NULL;
    }

    if(audio_playlist_tree)
    {
        delete audio_playlist_tree;
        audio_playlist_tree = NULL;
    }

    if(audio_collection_tree)
    {
        delete audio_collection_tree;
        audio_collection_tree = NULL;
    }
}

