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
#include "metadatacollection.h"

//
//  Include pixmap images for kinds of things on a list (playlist, artist,
//  album, etc.)
//

#include "pixmaps/container_pix.xpm"
#include "pixmaps/edit_playlist_pix.xpm"
#include "pixmaps/new_playlist_pix.xpm"
#include "pixmaps/playlist_pix.xpm"
#include "pixmaps/artist_pix.xpm"
#include "pixmaps/genre_pix.xpm"
#include "pixmaps/album_pix.xpm"
#include "pixmaps/track_pix.xpm"

//
//  Statis QPixmap's to hold each of the pixmaps included above.
//

static bool pixmaps_are_setup = false;
static QPixmap *pixcontainer = NULL;
static QPixmap *pixeditplaylist = NULL;
static QPixmap *pixnewplaylist = NULL;
static QPixmap *pixplaylist = NULL;
static QPixmap *pixartist = NULL;
static QPixmap *pixgenre = NULL;
static QPixmap *pixalbum = NULL;
static QPixmap *pixtrack = NULL;

//
//  Static function to scale pixmaps (if we are not running 800x600)
//

static QPixmap *scalePixmap(const char **xpmdata, float wmult, float hmult)
{
    QImage tmpimage(xpmdata);
    QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                       (int)(tmpimage.height() * hmult));
    QPixmap *ret = new QPixmap();
    ret->convertFromImage(tmp2);
                                                                                                                                                     
    return ret;
}


MfdContentCollection::MfdContentCollection(
                                            int an_id,
                                            int client_screen_width,
                                            int client_screen_height
                                          )
{
    collection_id = an_id;
    
    client_width = client_screen_width;
    client_height = client_screen_height;
    client_wmult = client_screen_width / 800.0;
    client_hmult = client_screen_height / 600.0;
    
    audio_item_dictionary.resize(9973); //  large prime
    audio_item_dictionary.setAutoDelete(true);
    
    //
    //  Prep the pixmaps
    //

    setupPixmaps();

    //
    //  Make the core tree branches
    //
    
    audio_artist_tree = new UIListGenericTree(NULL, "All by Artist");
    audio_artist_tree->setPixmap(pixartist);
    
    audio_genre_tree = new UIListGenericTree(NULL, "All by Genre");
    audio_genre_tree->setPixmap(pixgenre);
    
    audio_playlist_tree = new UIListGenericTree(NULL, "All Playlists");
    audio_playlist_tree->setPixmap(pixplaylist);
    
    audio_collection_tree =  new UIListGenericTree(NULL, "Grouped by Collection");
    audio_collection_tree->setPixmap(pixcontainer);
    
    new_playlist_tree = NULL;
    editable_playlist_tree = NULL;

    //
    //  Set some core attributes
    //
    
    audio_artist_tree->setAttribute(1, 1);
    audio_genre_tree->setAttribute(1, 1);
    audio_playlist_tree->setAttribute(1, 2);
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

void MfdContentCollection::addPlaylist(ClientPlaylist *new_playlist, MetadataCollection *collection)
{

    QString collection_name = collection->getName();
    
    //
    //  Create a node for this playlist in the audio playlist tree
    //

    UIListGenericTree *playlist_node = new UIListGenericTree(audio_playlist_tree, new_playlist->getName());

    playlist_node->setPixmap(pixplaylist);   
    playlist_node->setAttribute(0, 0);  //  collection id is 0
    playlist_node->setAttribute(1, 2); 
    playlist_node->setAttribute(2, 0); 

    //
    //  If it's editable, add it to the list of editable playlists
    //

    if(collection->isEditable())
    {
        //
        //  Make the root editable playlists node (this could be the first
        //  editable playlist)
        //
        
        if(!editable_playlist_tree)
        {
            editable_playlist_tree = new UIListGenericTree(NULL, "Edit Playlists");
            editable_playlist_tree->setPixmap(pixeditplaylist);
            editable_playlist_tree->setAttribute(1, 5);
        }

        UIListGenericTree *edit_node = new UIListGenericTree(editable_playlist_tree, new_playlist->getName());
        edit_node->setPixmap(pixeditplaylist);
        edit_node->setInt(new_playlist->getId()); 
        edit_node->setAttribute(0, new_playlist->getCollectionId());
        edit_node->setAttribute(1, 5);  //  magic number for edit playlist command
    }
    

    //
    //  Find or "create as we go" another node for this playlist in the "By
    //  Collection Tree"
    //
    
    UIListGenericTree *by_playlist_node = NULL;

    /*
    cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Am trying to add a playlist called \""
         << new_playlist->getName()
         << "\" to a collection called \""
         << collection_name
         << "\""
         << ", and the collection's id is "
         << new_playlist->getCollectionId()
         << endl;
    */   

    UIListGenericTree *collection_node = NULL;
    GenericTree *gt_collection_node = audio_collection_tree->getChildByName(collection_name);
    if(gt_collection_node)
    {
        collection_node = (UIListGenericTree *)gt_collection_node;
    }
    if(!collection_node)
    {
        collection_node = new UIListGenericTree(audio_collection_tree, collection_name);
        collection_node->setPixmap(pixcontainer);
        
        //
        //  We just made the node for this collection, it needs Playlist node
        //

        by_playlist_node = new UIListGenericTree((UIListGenericTree *)collection_node, "Playlist");
        by_playlist_node->setPixmap(pixplaylist);
        by_playlist_node->setAttribute(1, 2);
    }
    else
    {
        GenericTree *gt_by_playlist_node = collection_node->getChildByName("Playlist");
        by_playlist_node = (UIListGenericTree *) gt_by_playlist_node;
        if(!by_playlist_node)
        {
            by_playlist_node = new UIListGenericTree((UIListGenericTree *)collection_node, "Playlist");
            by_playlist_node->setPixmap(pixplaylist);
            by_playlist_node->setAttribute(1, 2);
        }
    }

    UIListGenericTree *collection_playlist_node = new UIListGenericTree((UIListGenericTree *)by_playlist_node, new_playlist->getName());
    collection_playlist_node->setPixmap(pixplaylist);
    collection_playlist_node->setAttribute(1, 2);
    
    //
    //  Iterate over the entries in this playlist
    //
            
    int counter = 0;
    QValueList<PlaylistEntry> *the_list = new_playlist->getListPtr();
    QValueList<PlaylistEntry>::iterator l_it;
    for(l_it = the_list->begin(); l_it != the_list->end(); ++l_it)
    {
        if( (*l_it).isAnotherPlaylist())
        {
            recursivelyAddSubPlaylist(  
                                        playlist_node,
                                        collection, (*l_it).getId(), 
                                        counter
                                     );
            recursivelyAddSubPlaylist(
                                        collection_playlist_node, 
                                        collection, 
                                        (*l_it).getId(), 
                                        counter
                                     );
        }
        else
        {
            UIListGenericTree *track_node = new UIListGenericTree(playlist_node, (*l_it).getName());
            track_node->setPixmap(pixtrack);
            track_node->setInt(new_playlist->getId());
            track_node->setAttribute(0, new_playlist->getCollectionId());
            track_node->setAttribute(1, 2);
            track_node->setAttribute(2, counter);

            UIListGenericTree *o_track_node = new UIListGenericTree(collection_playlist_node, (*l_it).getName());
            o_track_node->setInt(new_playlist->getId());
            o_track_node->setPixmap(pixtrack);
            o_track_node->setAttribute(0, new_playlist->getCollectionId());
            o_track_node->setAttribute(1, 2);
            o_track_node->setAttribute(2, counter);
        }
        ++counter;
    }
    
}

void MfdContentCollection::addNewPlaylistAbility(const QString &collection_name)
{
    if(!new_playlist_tree)
    {
        new_playlist_tree = new UIListGenericTree(NULL, "New Playlist");
        new_playlist_tree->setPixmap(pixnewplaylist);
        new_playlist_tree->setAttribute(1, 4);
    }
    
    UIListGenericTree *new_node = new UIListGenericTree(new_playlist_tree, QString("in %1").arg(collection_name));
    new_node->setPixmap(pixnewplaylist);
    new_node->setAttribute(1, 4);
}

void MfdContentCollection::recursivelyAddSubPlaylist(
                                                        UIListGenericTree *where_to_add, 
                                                        MetadataCollection *collection, 
                                                        int playlist_id,
                                                        int spot_counter
                                                    )
{
    //
    //  Use the playlist_id to find the playlist in this collection
    //
    
    ClientPlaylist *playlist = collection->getPlaylistById(playlist_id);
    if(!playlist)
    {
        cerr << "mfdcontent.o: there is no playlist with an id of " <<  playlist_id << endl;
        return;
    }

    //
    //  Create the parent node of all the nodes on this playlist
    //

    UIListGenericTree *playlist_node = new UIListGenericTree(where_to_add, playlist->getName());
    playlist_node->setPixmap(pixplaylist);
    playlist_node->setAttribute(1, 2);
    playlist_node->setAttribute(2, spot_counter);

    int counter = 0;
    QValueList<PlaylistEntry> *the_list = playlist->getListPtr();
    QValueList<PlaylistEntry>::iterator l_it;
    for(l_it = the_list->begin(); l_it != the_list->end(); ++l_it)
    {
        if( (*l_it).isAnotherPlaylist())
        {
            recursivelyAddSubPlaylist(
                                        playlist_node, 
                                        collection, 
                                        (*l_it).getId(), 
                                        counter
                                     );
        }
        else
        {
            UIListGenericTree *track_node = new UIListGenericTree(playlist_node, (*l_it).getName());
            track_node->setPixmap(pixtrack);
            track_node->setInt(playlist->getId());
            track_node->setAttribute(0, playlist->getCollectionId());
            track_node->setAttribute(1, 2);
            track_node->setAttribute(2, counter);
        }
        ++counter;
    }
}

void MfdContentCollection::addItemToAudioArtistTree(AudioMetadata *item, GenericTree *starting_point)
{
    QString album  = item->getAlbum();
    QString artist = item->getArtist();
    QString title  = item->getTitle();
    int track_no   = item->getTrack();
            
    //
    // The Artist --> Album --> Track   branch
    //
            
    UIListGenericTree *artist_node = NULL;
    artist_node = (UIListGenericTree *)starting_point->getChildByName(artist);
    if(!artist_node)
    {
        artist_node = new UIListGenericTree((UIListGenericTree *) starting_point, artist);
        artist_node->setPixmap(pixartist);
        artist_node->setAttribute(0, 0);
        artist_node->setAttribute(1, 1);
        artist_node->setAttribute(2, 0);
    }


    UIListGenericTree *album_node = NULL;
    album_node = (UIListGenericTree *)artist_node->getChildByName(album);
    if(!album_node)
    {
        album_node = new UIListGenericTree((UIListGenericTree *) artist_node, album);
        album_node->setPixmap(pixalbum);
        album_node->setAttribute(0, 0);
        album_node->setAttribute(1, 1);
        album_node->setAttribute(2, 0);
    }
            
    UIListGenericTree *title_node = new UIListGenericTree((UIListGenericTree *) album_node, 
                                        QString("%1. %2").arg(track_no).arg(title));
    title_node->setPixmap(pixtrack);
    title_node->setInt(item->getId());
    title_node->setAttribute(0, item->getCollectionId());
    title_node->setAttribute(1, 1);
    title_node->setAttribute(2, track_no);
}

void MfdContentCollection::addItemToAudioGenreTree(AudioMetadata *item, GenericTree *starting_point)
{
    QString genre  = item->getGenre();
    QString album  = item->getAlbum();
    QString artist = item->getArtist();
    QString title  = item->getTitle();
    int track_no   = item->getTrack();
            
    //
    // The Genre --> Artist --> Album --> Track   branch
    //

    UIListGenericTree *genre_node = NULL;
    genre_node = (UIListGenericTree *)starting_point->getChildByName(genre);
    if(!genre_node)
    {
        genre_node = new UIListGenericTree((UIListGenericTree *)starting_point, genre);
        genre_node->setPixmap(pixgenre);
        genre_node->setAttribute(0, 0);
        genre_node->setAttribute(1, 1);
        genre_node->setAttribute(2, 0);
    }

    UIListGenericTree *artist_node = NULL;
    artist_node = (UIListGenericTree *) genre_node->getChildByName(artist);
    if(!artist_node)
    {
        artist_node = new UIListGenericTree((UIListGenericTree *)genre_node, artist);
        artist_node->setPixmap(pixartist);
        artist_node->setAttribute(0, 0);
        artist_node->setAttribute(1, 1);
        artist_node->setAttribute(2, 0);
    }

    UIListGenericTree *album_node = NULL;
    album_node = (UIListGenericTree *) artist_node->getChildByName(album);
    
    if(!album_node)
    {
        album_node = new UIListGenericTree(artist_node, album);
        album_node->setPixmap(pixalbum);
        album_node->setAttribute(0, 0);
        album_node->setAttribute(1, 1);
        album_node->setAttribute(2, 0);
    }
            
    UIListGenericTree *title_node = new UIListGenericTree((UIListGenericTree *) album_node, 
                                        QString("%1. %2").arg(track_no).arg(title));
    title_node->setPixmap(pixtrack);
    title_node->setInt(item->getId());
    title_node->setAttribute(0, item->getCollectionId());
    title_node->setAttribute(1, 1);
    title_node->setAttribute(2, track_no);
}

void MfdContentCollection::addItemToAudioCollectionTree(AudioMetadata *item, const QString &collection_name)
{

    QString genre  = item->getGenre();
    QString album  = item->getAlbum();
    QString artist = item->getArtist();
    QString title  = item->getTitle();

    UIListGenericTree *by_artist_node = NULL;
    UIListGenericTree *by_genre_node = NULL;
     
            
    //
    // By Collection --> Genre/Artist, etc.
    //
    
    UIListGenericTree *collection_node = (UIListGenericTree *)audio_collection_tree->getChildByName(collection_name);
    if(!collection_node)
    {
        collection_node = new UIListGenericTree(audio_collection_tree, collection_name);
        collection_node->setPixmap(pixcontainer);
        
        //
        //  We just made the node for this collection, it needs Artist and Genre subnodes
        //

        by_artist_node = new UIListGenericTree((UIListGenericTree *)collection_node, "By Artist");
        by_artist_node->setPixmap(pixartist);
        by_artist_node->setAttribute(1, 1);
        by_genre_node = new UIListGenericTree((UIListGenericTree *)collection_node, "By Genre");
        by_genre_node->setPixmap(pixgenre);
        by_genre_node->setAttribute(1, 1);
    }
    else
    {
        by_artist_node = (UIListGenericTree *)collection_node->getChildByName("By Artist");
        by_genre_node = (UIListGenericTree *)collection_node->getChildByName("By Genre");
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

void MfdContentCollection::sort()
{
    //
    //  we now (presumably) have all our metadata and everything in place.
    //  Sort it.
    //

    audio_artist_tree->sortByAttributeThenByString(2);
    audio_artist_tree->reOrderAsSorted();
    
    audio_genre_tree->sortByAttributeThenByString(2);
    audio_genre_tree->reOrderAsSorted();
    
    audio_playlist_tree->sortByAttributeThenByString(2);
    audio_playlist_tree->reOrderAsSorted();
    
    audio_collection_tree->sortByAttributeThenByString(2);
    audio_collection_tree->reOrderAsSorted();
    
    if(editable_playlist_tree)
    {
        editable_playlist_tree->sortByString();
        editable_playlist_tree->reOrderAsSorted();
    }

}

void MfdContentCollection::setupPixmaps()
{
    if(!pixmaps_are_setup)
    {
        if (client_height != 600 || client_width != 800)
        {
            pixcontainer = scalePixmap((const char **)container_pix_xpm, client_wmult, client_hmult);
            pixeditplaylist = scalePixmap((const char **)edit_playlist_pix_xpm, client_wmult, client_hmult);
            pixnewplaylist = scalePixmap((const char **)new_playlist_pix_xpm, client_wmult, client_hmult);
            pixplaylist = scalePixmap((const char **)playlist_pix_xpm, client_wmult, client_hmult);
            pixartist = scalePixmap((const char **)artist_pix_xpm, client_wmult, client_hmult);
            pixgenre = scalePixmap((const char **)genre_pix_xpm, client_wmult, client_hmult);
            pixalbum = scalePixmap((const char **)album_pix_xpm, client_wmult, client_hmult);
            pixtrack = scalePixmap((const char **)track_pix_xpm, client_wmult, client_hmult);
        }
        else
        {
            pixcontainer = new QPixmap((const char **)container_pix_xpm);
            pixeditplaylist = new QPixmap((const char **)edit_playlist_pix_xpm);
            pixnewplaylist = new QPixmap((const char **)new_playlist_pix_xpm);
            pixplaylist = new QPixmap((const char **)playlist_pix_xpm);
            pixartist = new QPixmap((const char **)artist_pix_xpm);
            pixgenre = new QPixmap((const char **)genre_pix_xpm);
            pixalbum = new QPixmap((const char **)album_pix_xpm);
            pixtrack = new QPixmap((const char **)track_pix_xpm);
        }
        pixmaps_are_setup = true;
    }
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
    
    if(new_playlist_tree)
    {
        delete new_playlist_tree;
        new_playlist_tree = NULL;
    }

    if(editable_playlist_tree)
    {
        delete editable_playlist_tree;
        editable_playlist_tree = NULL;
    }
}

