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
#include "pixmaps/edit_track_pix.xpm"
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
static QPixmap *pixedittrack = NULL;
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
    
    audio_playlist_dictionary.resize(9973);
    audio_playlist_dictionary.setAutoDelete(true);
    
    selectable_content_trees.setAutoDelete(true);

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

void MfdContentCollection::addMetadata(Metadata *new_item, const QString &collection_name, MetadataCollection *collection)
{
    //
    //  At this point, we only understand audio metadata. If it's not audio
    //  metadata, complain
    //
    
    if (new_item->getType() != MDT_audio)
    {
        cerr << "mfdcontent.o: no idea what to do with "
             << "non audio content"
             << endl;
        return;
    }
    
    if (AudioMetadata *copy = dynamic_cast<AudioMetadata*>(new_item) )
    {
        //
        //  Because this object gets constructed and then passed off to the
        //  actual client, we need to make a copy of everything
        //
    
        AudioMetadata *audio_copy = new AudioMetadata(*copy);

        //
        //  Stuff this into the audio item dictionary with an int key of
        //  it's container id and item id. It goes into a QIntDict
        //  'cause that makes it fast to look it up from the client code
        //  (ie. user navigates a tree to a node, hits enter; signal
        //  comes back with item id and container id, use those to look
        //  up the full item from this dictionary)
        //

        int dictionary_key =   audio_copy->getCollectionId()
                               * METADATA_UNIVERSAL_ID_DIVIDER
                               + audio_copy->getId();

        audio_item_dictionary.insert(dictionary_key, audio_copy);
        
        //
        //  Now, put it on all the required tree places.
        //

        if (!new_item->isDuplicate())
        {
            addItemToAudioArtistTree(copy, audio_artist_tree);
            addItemToAudioGenreTree(copy, audio_genre_tree);
        }

        addItemToAudioCollectionTree(copy, collection_name);

        //
        //  Add the same thing to the tree that contains items the user can
        //  turn on and off while editing a playlist if the collection it
        //  comes from allows editing
        //
        
        if(collection->isEditable())
        {
            addItemToSelectableTrees(copy);
        }
        
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
    //
    //  Make a copy and store it in our playlist dictionary
    //
    
    ClientPlaylist *playlist_copy = new ClientPlaylist(*new_playlist);

    //
    //  Stuff this into the audio playlist dictionary with an int key of
    //  it's container id and playlist id. It goes into a QIntDict
    //  'cause that makes it fast to look it up from the client code.
    //

    int dictionary_key =   playlist_copy->getCollectionId()
                            * METADATA_UNIVERSAL_ID_DIVIDER
                            + playlist_copy->getId();

    audio_playlist_dictionary.insert(dictionary_key, playlist_copy);
        

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
    //  If it's editable, add it to the list of editable playlists (that is,
    //  list of playlists users can navigate to and select to _begin_
    //  editing a playlist)
    //

    if (collection->isEditable())
    {
        //
        //  Make the root editable playlists node (this could be the first
        //  editable playlist)
        //
        
        if (!editable_playlist_tree)
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

        //
        //  Create an entry in the checkable list user's use to edit playlists
        //
    
        addPlaylistToSelectableTrees(new_playlist);

    }
    

    //
    //  Find or "create as we go" another node for this playlist in the "By
    //  Collection Tree"
    //
    
    UIListGenericTree *by_playlist_node = NULL;
    UIListGenericTree *collection_node = NULL;

    GenericTree *gt_collection_node = audio_collection_tree->getChildByName(collection_name);
    if (gt_collection_node)
    {
        collection_node = (UIListGenericTree *)gt_collection_node;
    }
    if (!collection_node)
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
        if (!by_playlist_node)
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
        if ( (*l_it).isAnotherPlaylist())
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
    if (!new_playlist_tree)
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
    if (!playlist)
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
        if ( (*l_it).isAnotherPlaylist())
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

void MfdContentCollection::addItemToAudioArtistTree(AudioMetadata *item, GenericTree *starting_point, bool do_checks)
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
    if (!artist_node)
    {
        artist_node = new UIListGenericTree((UIListGenericTree *) starting_point, artist);
        artist_node->setPixmap(pixartist);
        artist_node->setAttribute(0, 0);
        artist_node->setAttribute(1, 1);
        artist_node->setAttribute(2, 0);
        artist_node->setAttribute(3, 2);    // att3 == 2 implies "Artist"
    }


    UIListGenericTree *album_node = NULL;
    album_node = (UIListGenericTree *)artist_node->getChildByName(album);
    if (!album_node)
    {
        album_node = new UIListGenericTree((UIListGenericTree *) artist_node, album);
        album_node->setPixmap(pixalbum);
        album_node->setAttribute(0, 0);
        album_node->setAttribute(1, 1);
        album_node->setAttribute(2, 0);
        album_node->setAttribute(3, 3);    // att3 == 3 implies "Album"
    }
            
    UIListGenericTree *title_node = new UIListGenericTree((UIListGenericTree *) album_node, 
                                        QString("%1. %2").arg(track_no).arg(title));
    title_node->setPixmap(pixtrack);
    title_node->setInt(item->getId());
    title_node->setAttribute(0, item->getCollectionId());
    title_node->setAttribute(1, 1);
    title_node->setAttribute(2, track_no);
    
    if (do_checks)
    {
        //
        //  We add this node to our selectable_content_map so that we
        //  can find it quickly based on the state of a playlist
        //

        selectable_content_map.insert(make_pair((item->getCollectionId() * METADATA_UNIVERSAL_ID_DIVIDER) + item->getId(), title_node));
    }
}

void MfdContentCollection::addItemToAudioGenreTree(AudioMetadata *item, GenericTree *starting_point, bool do_checks)
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
    if (!genre_node)
    {
        genre_node = new UIListGenericTree((UIListGenericTree *)starting_point, genre);
        genre_node->setPixmap(pixgenre);
        genre_node->setAttribute(0, 0);
        genre_node->setAttribute(1, 1);
        genre_node->setAttribute(2, 0);
        genre_node->setAttribute(3, 1);    // att3 == 1 implies "Genre"
    }

    UIListGenericTree *artist_node = NULL;
    artist_node = (UIListGenericTree *) genre_node->getChildByName(artist);
    if (!artist_node)
    {
        artist_node = new UIListGenericTree((UIListGenericTree *)genre_node, artist);
        artist_node->setPixmap(pixartist);
        artist_node->setAttribute(0, 0);
        artist_node->setAttribute(1, 1);
        artist_node->setAttribute(2, 0);
        artist_node->setAttribute(3, 2);    // att3 == 1 implies "Artist"
    }

    UIListGenericTree *album_node = NULL;
    album_node = (UIListGenericTree *) artist_node->getChildByName(album);
    
    if (!album_node)
    {
        album_node = new UIListGenericTree(artist_node, album);
        album_node->setPixmap(pixalbum);
        album_node->setAttribute(0, 0);
        album_node->setAttribute(1, 1);
        album_node->setAttribute(2, 0);
        album_node->setAttribute(3, 3);    // att3 == 3 implies "Album"
    }
            
    UIListGenericTree *title_node = new UIListGenericTree((UIListGenericTree *) album_node, 
                                        QString("%1. %2").arg(track_no).arg(title));
    title_node->setPixmap(pixtrack);
    title_node->setInt(item->getId());
    title_node->setAttribute(0, item->getCollectionId());
    title_node->setAttribute(1, 1);
    title_node->setAttribute(2, track_no);
    if (do_checks)
    {
        selectable_content_map.insert(make_pair((item->getCollectionId() * METADATA_UNIVERSAL_ID_DIVIDER) + item->getId(), title_node));
    }
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
    if (!collection_node)
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
    //  Genre" subnodes. Put the new item on the "By Artist" branch.
    //

    addItemToAudioArtistTree(item, by_artist_node);        

    //
    //  Now put it on the "By Genre" branch
    //    
    
    addItemToAudioGenreTree(item, by_genre_node);

}

void MfdContentCollection::addItemToSelectableTrees(AudioMetadata *item)
{
    
    //
    //  There is one tree of selectable content per collection
    //
    
    int which_collection = item->getCollectionId();
    
    UIListGenericTree *collection_tree = NULL;
    UIListGenericTree *artist_branch = NULL;
    UIListGenericTree *genre_branch = NULL;
    UIListGenericTree *playlist_branch = NULL;
    
    collection_tree = selectable_content_trees[which_collection];
    
    
    
    if (!collection_tree)
    {
        collection_tree = new UIListGenericTree(NULL, "user never sees this");

        artist_branch = new UIListGenericTree(collection_tree, "Artist");
        artist_branch->setPixmap(pixartist);
    
        genre_branch = new UIListGenericTree(collection_tree, "Genre");
        genre_branch->setPixmap(pixgenre);
    
        playlist_branch = new UIListGenericTree(collection_tree, "Playlist");
        playlist_branch->setPixmap(pixplaylist);
    
        selectable_content_trees.insert(which_collection, collection_tree);
    }
    else
    {
        artist_branch = (UIListGenericTree *)collection_tree->getChildByName("Artist");
        genre_branch = (UIListGenericTree *)collection_tree->getChildByName("Genre");
    }
    
    addItemToAudioArtistTree(item, artist_branch, true);
    addItemToAudioGenreTree(item, genre_branch, true);
    
}

void MfdContentCollection::addPlaylistToSelectableTrees(ClientPlaylist *playlist)
{
    
    //
    //  There is one tree of selectable content per collection
    //
    
    int which_collection = playlist->getCollectionId();

    UIListGenericTree *collection_tree = NULL;
    UIListGenericTree *artist_branch = NULL;
    UIListGenericTree *genre_branch = NULL;
    UIListGenericTree *playlist_branch = NULL;
    
    collection_tree = selectable_content_trees[which_collection];
    
    
    
    if (!collection_tree)
    {
        collection_tree = new UIListGenericTree(NULL, "user never sees this");

        artist_branch = new UIListGenericTree(collection_tree, "Artist");
        artist_branch->setPixmap(pixartist);
    
        genre_branch = new UIListGenericTree(collection_tree, "Genre");
        genre_branch->setPixmap(pixgenre);
    
        playlist_branch = new UIListGenericTree(collection_tree, "Playlist");
        playlist_branch->setPixmap(pixplaylist);
    
        selectable_content_trees.insert(which_collection, collection_tree);
    }
    else
    {
        playlist_branch = (UIListGenericTree *)collection_tree->getChildByName("Playlist");
    }
    
    UIListGenericTree *pl_edit_node = new UIListGenericTree(playlist_branch, playlist->getName());
    
    pl_edit_node->setInt(playlist->getId() * -1);
    pl_edit_node->setPixmap(pixplaylist);   
    pl_edit_node->setAttribute(0, playlist->getCollectionId());
    pl_edit_node->setAttribute(1, 2); 
    pl_edit_node->setAttribute(2, 0); 

    long node_key = ((playlist->getCollectionId() * 
                    METADATA_UNIVERSAL_ID_DIVIDER )
                    + playlist->getId()) * -1;

    selectable_content_map.insert(make_pair(node_key , pl_edit_node));
}

void MfdContentCollection::tallyPlaylists()
{
    //
    //  Iterate over each playlist, and find out how many tracks (ie. root
    //  level nodes after all playlists within playlists are resolved) each
    //  has.
    //
    QIntDictIterator<ClientPlaylist> it( audio_playlist_dictionary ); 
    for ( ; it.current(); ++it )
    {
        int numb_tracks = countPlaylistTracks(it.current(), 0);
        it.current()->setActualTrackCount(numb_tracks);
    }
}

uint MfdContentCollection::countPlaylistTracks(ClientPlaylist *playlist, uint counter)
{

    QValueList<PlaylistEntry> *the_list = playlist->getListPtr();
    QValueList<PlaylistEntry>::iterator l_it;
    for(l_it = the_list->begin(); l_it != the_list->end(); ++l_it)
    {
        if ( (*l_it).isAnotherPlaylist())
        {
        
            ClientPlaylist *sub_list = getAudioPlaylist(
                                                        playlist->getCollectionId(),
                                                        (*l_it).getId()
                                                       );

            counter = countPlaylistTracks( sub_list, counter);
        }
        else
        {
            ++counter;
        }
    }  

    return counter;
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

ClientPlaylist* MfdContentCollection::getAudioPlaylist(int which_collection, int which_id)
{
    int dictionary_key =   which_collection
                        * METADATA_UNIVERSAL_ID_DIVIDER
                        + which_id;


    return audio_playlist_dictionary[dictionary_key];
}

UIListGenericTree* MfdContentCollection::constructPlaylistTree(
                                                                int which_collection, 
                                                                int which_playlist
                                                              )
{
    //
    //  This is (only?) called when the user is going to edit a playlist. We
    //  build a tree that reflects the state of the given playlist so that
    //  they can change it.
    //
    //  NB: it is the client's obligation to delete this tree !!
    //
    
    ClientPlaylist *playlist = getAudioPlaylist(which_collection, which_playlist);
    
    if (!playlist)
    {
        cerr << "mfdcontent.o: Asked to constrcut playlist tree, but "
             << "playlist does not exist"
             << endl;
        return NULL;
    }
    
    UIListGenericTree *new_root = new UIListGenericTree(NULL, "User should not see this");
    
    int counter = 0;
    QValueList<PlaylistEntry> *playlist_entries = playlist->getListPtr();

    QValueList<PlaylistEntry>::iterator l_it;
    for ( l_it = playlist_entries->begin(); l_it != playlist_entries->end(); ++l_it )    
    {
        UIListGenericTree *pl_node = new UIListGenericTree(new_root, (*l_it).getName());

        if ( (*l_it).isAnotherPlaylist())
        {
            pl_node->setPixmap(pixplaylist);
            pl_node->setAttribute(3, -1);
        }
        else
        {
            pl_node->setPixmap(pixtrack);
            pl_node->setAttribute(3, 1);
        }
        pl_node->setInt((*l_it).getId());
        pl_node->setAttribute(0, playlist->getCollectionId());
        pl_node->setAttribute(1, 2);
        pl_node->setAttribute(2, counter);
        ++counter;
    }

    return new_root;        
}

UIListGenericTree* MfdContentCollection::constructContentTree(
                                                                int which_collection, 
                                                                int which_playlist
                                                              )
{

    //
    //  Given the collection id, find the content tree.
    //


    UIListGenericTree *content_tree = selectable_content_trees[which_collection];

    //
    //  By default, everything in the tree should have a check-mark indicating it is off
    //
    
    turnOffTree (content_tree);
    
    //
    //  We have to iterate over the playlist whole content tree and turn check marks
    //  on and off to reflect the reality of the playlist in question
    //
    
    ClientPlaylist *playlist = getAudioPlaylist(which_collection, which_playlist);

    QValueList<PlaylistEntry> *the_list = playlist->getListPtr();
    QValueList<PlaylistEntry>::iterator l_it;
    for(l_it = the_list->begin(); l_it != the_list->end(); ++l_it)
    {
        int multiplier = 1;
        int node_id = (*l_it).getId();
        if ( (*l_it).isAnotherPlaylist())
        {
            multiplier = -1;
        }

        SelectableContentMap::iterator it;
        for (it  = selectable_content_map.lower_bound(((which_collection * METADATA_UNIVERSAL_ID_DIVIDER) + node_id) * multiplier);
             it != selectable_content_map.upper_bound(((which_collection * METADATA_UNIVERSAL_ID_DIVIDER) + node_id) * multiplier);
             ++it)
        {
            it->second->setCheck(2);
            checkParent(it->second);
        }
    }
    
    ClientPlaylist *subject = getAudioPlaylist(which_collection, which_playlist);
    
    //
    //  Grey out playlists that would lead to infinite recursion
    //
    
    QIntDictIterator<ClientPlaylist> pl_it( audio_playlist_dictionary ); 
    for ( ; pl_it.current(); ++pl_it )
    {
        if((int) pl_it.current()->getId() == which_playlist || 
           crossReferenceExists(subject, pl_it.current(), 0))
        {
            SelectableContentMap::iterator scm_it;
            for (scm_it  = selectable_content_map.lower_bound(((which_collection * METADATA_UNIVERSAL_ID_DIVIDER) + pl_it.current()->getId()) * -1);
                 scm_it != selectable_content_map.upper_bound(((which_collection * METADATA_UNIVERSAL_ID_DIVIDER) + pl_it.current()->getId()) * -1);
                 ++scm_it)
            {
                scm_it->second->setActive(false);
                checkParent(scm_it->second);
            }
        }
    }
    
    //
    //  Hand it back to the caller
    //
    
    return content_tree;
    

}


void MfdContentCollection::turnOffTree(UIListGenericTree *node)
{
    QPtrList<GenericTree> *all_children = node->getAllChildren();
    QPtrListIterator<GenericTree> it(*all_children);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        UIListGenericTree *ui_child = (UIListGenericTree *) child;
        ui_child->setItem(NULL);    //  TEMP FIX ME
        ui_child->setCheck(0);
        ui_child->setActive(true);
        turnOffTree(ui_child);
        ++it;
    }
}

void MfdContentCollection::toggleItem(UIListGenericTree *node, bool turn_on)
{
    //
    //  Toggle on or off stuff in the content selection tree
    //

    SelectableContentMap::iterator it;
    
    int node_id = node->getInt();
    int multiplier = 1;
    
    if (node_id < 0)
    {
        multiplier = -1;
        node_id = node_id * -1;
    }
    
    for (it  = selectable_content_map.lower_bound(((node->getAttribute(0) * METADATA_UNIVERSAL_ID_DIVIDER) + node_id) * multiplier);
         it != selectable_content_map.upper_bound(((node->getAttribute(0) * METADATA_UNIVERSAL_ID_DIVIDER) + node_id) * multiplier);
         ++it)
        {
            if (turn_on)
            {
                it->second->setCheck(2);
            }
            else
            {
                it->second->setCheck(0);
            }
            checkParent(it->second);
        }
}

void MfdContentCollection::toggleTree(UIListTreeType *menu, UIListGenericTree *playlist_tree, UIListGenericTree *node, bool turn_on)
{

    //
    //  Iterate over all the subnodes 
    //

    QPtrList<GenericTree> *all_children = node->getAllChildren();
    QPtrListIterator<GenericTree> it(*all_children);
    GenericTree *child = NULL;
    while ((child = it.current()) != 0)
    {
        ++it;
        
        if(UIListGenericTree *ui_child = dynamic_cast<UIListGenericTree*> (child))
        {
            if (ui_child->getInt() < 0 && ui_child->getActive())
            {

                //
                //  It's a playlist
                //

                bool at_least_one = true;
                SelectableContentMap::iterator oit;
                long node_key = ((ui_child->getAttribute(0) * 
                                METADATA_UNIVERSAL_ID_DIVIDER )
                                - ui_child->getInt()) * -1;
                for (oit  = selectable_content_map.lower_bound(node_key);
                     oit != selectable_content_map.upper_bound(node_key);
                     ++oit)
                {
                    if (turn_on)
                    {
                        if(oit->second->getCheck() == 0)
                        {
                            oit->second->setCheck(2);
                            checkParent(oit->second);
                            if(at_least_one)
                            {
                                alterPlaylist(menu, playlist_tree, oit->second, turn_on);
                                at_least_one = false;
                            }
                        }
                    }
                    else
                    {
                        if(oit->second->getCheck() == 2)
                        {
                            oit->second->setCheck(0);
                            checkParent(oit->second);
                            if(at_least_one)
                            {
                                alterPlaylist(menu, playlist_tree, oit->second, turn_on);
                                at_least_one = false;
                            }
                        }
                    }
                }
            
            }
            else if (ui_child->getInt() > 0 && ui_child->getActive())
            {
                //
                //  It's a track
                //

                bool at_least_one = true;
                SelectableContentMap::iterator oit;
                long node_key = (ui_child->getAttribute(0) * 
                                METADATA_UNIVERSAL_ID_DIVIDER )
                                + ui_child->getInt();
                for (oit  = selectable_content_map.lower_bound(node_key);
                     oit != selectable_content_map.upper_bound(node_key);
                     ++oit)
                {
                    if (turn_on)
                    {
                        if(oit->second->getCheck() == 0)
                        {
                            oit->second->setCheck(2);
                            checkParent(oit->second);
                            if(at_least_one)
                            {
                                alterPlaylist(menu, playlist_tree, oit->second, turn_on);
                                at_least_one = false;
                            }
                        }
                    }
                    else
                    {
                        if(oit->second->getCheck() == 2)
                        {
                            oit->second->setCheck(0);
                            checkParent(oit->second);
                            if(at_least_one)
                            {
                                alterPlaylist(menu, playlist_tree, oit->second, turn_on);
                                at_least_one = false;
                            }
                        }
                    }
                }
            }
            else
            {
                //
                //  This is neither a playlist nor a track, but a node with
                //  stuff below it. We iterate downwards.
                //
            
                toggleTree(menu, playlist_tree, ui_child, turn_on);
            }
        }
    }        
}

void MfdContentCollection::alterPlaylist(UIListTreeType *menu, UIListGenericTree *playlist_tree, UIListGenericTree *node, bool turn_on)
{
    if (turn_on)
    {

        if (node->getInt() > 0)
        {
            AudioMetadata *new_item = getAudioItem(node->getAttribute(0), node->getInt());
            if(!new_item)
            {
                cerr << "mfdcontent.o: alterPlaylist() couldn't find metadata" << endl;
                return;
            }
            
            QString artist = new_item->getArtist();
            QString title  = new_item->getTitle();
            
            UIListGenericTree *new_pl_node = new UIListGenericTree(playlist_tree, QString("%1 ~ %2").arg(artist).arg(title));
            new_pl_node->setPixmap(pixtrack);
            new_pl_node->setInt(node->getInt());
            new_pl_node->setAttribute(0, node->getAttribute(0));
            new_pl_node->setAttribute(1, 2);
            new_pl_node->setAttribute(3, 1);
        }
        else if (node->getInt() < 0)
        {
            UIListGenericTree *new_pl_node = new UIListGenericTree(playlist_tree, node->getString());
            new_pl_node->setPixmap(pixplaylist);
            new_pl_node->setInt(node->getInt() * -1);
            new_pl_node->setAttribute(0, node->getAttribute(0));
            new_pl_node->setAttribute(1, 2);
            new_pl_node->setAttribute(3, -1);
        }
    }
    else
    {
        //
        //  Find the node already on the playlist with node's item id and
        //  remove it
        //
        
        long target_id = node->getInt();
        QPtrList<GenericTree> *all_children = playlist_tree->getAllChildren();
        QPtrListIterator<GenericTree> it(*all_children);
        GenericTree *child = NULL;
        while ((child = it.current()) != 0)
        {
            ++it;
            if (target_id > 0)
            {
                if (child->getInt() == target_id && child->getAttribute(3) > 0)
                {
                    UIListGenericTree *ui_child = (UIListGenericTree *)child;
                    menu->moveAwayFrom(ui_child);
                    ui_child->RemoveFromParent();   // sortable removeRef() deletes this
                }
           }
           else if (target_id < 0)
           {
                if (child->getInt() == target_id * -1 && child->getAttribute(3) < 0)
                {
                    UIListGenericTree *ui_child = (UIListGenericTree *)child;
                    menu->moveAwayFrom(ui_child);
                    ui_child->RemoveFromParent();   // sortable removeRef() deletes this
                }
           }
        }
    }    
}

void MfdContentCollection::checkParent(UIListGenericTree *node)
{

    if (!node)
    {
        return;
    }

    if (UIListGenericTree *parent = dynamic_cast<UIListGenericTree*>(node->getParent()))
    {
        bool all_on = true;
        bool one_on = false;

        QPtrList<GenericTree> *all_children = parent->getAllChildren();
        QPtrListIterator<GenericTree> it(*all_children);
        GenericTree *child;
        while ((child = it.current()) != 0)
        {
            UIListGenericTree *ui_child = (UIListGenericTree *) child;
            if (ui_child->getCheck() > 0 && ui_child->getActive())
            {
                one_on = true;
            }
            if (ui_child->getCheck() == 0 && ui_child->getActive())
            {
                all_on = false;
            }
            ++it;
        }

        if (all_on)
        {
            parent->setCheck(2);
        }
        else if (one_on)
        {
            parent->setCheck(1);
        }
        else
        {
            parent->setCheck(0);
        }

        if (parent->getParent())
        {
            checkParent(parent);
        }
    }
}

bool MfdContentCollection::crossReferenceExists(ClientPlaylist *subject, ClientPlaylist *object, int depth)
{
    if(subject == NULL)
    {
        cerr << "mfdcontent.o: can't check for cross references to a subject "
             << "that does not exist" << endl;
        return false;
    }

    if(object == NULL)
    {
        cerr << "mfdcontent.o: can't check for cross references to an object "
             << "that does not exist" << endl;
        return false;
    }

    if(depth > 10)
    {
        cerr << "mfdcontent.o: I'm recursively checking playlists, and have "
             << "reached a search depth over 10 " << endl ;
        return false;
    }


    bool ref_exists = false;
    uint check;

    QValueList<PlaylistEntry> *the_list = object->getListPtr();
    QValueList<PlaylistEntry>::iterator l_it;
    for(l_it = the_list->begin(); l_it != the_list->end(); ++l_it)
    {
        if ( (*l_it).isAnotherPlaylist())
        {
            check = (*l_it).getId();
            if(check == subject->getId())
            {
                ref_exists = true;
                return ref_exists;
            }
            else
            {
                //  Recurse down one level
                int new_depth = depth + 1;
                ClientPlaylist *new_check = getAudioPlaylist(
                                                subject->getCollectionId(),
                                                (*l_it).getId()
                                                            );
;
                if (new_check)
                {
                    ref_exists = crossReferenceExists(subject, new_check, new_depth);
                    if(ref_exists == true)
                    {
                        return ref_exists;
                    }
                }
            }
        }
    }  

    return ref_exists;
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
    
    if (editable_playlist_tree)
    {
        editable_playlist_tree->sortByString();
        editable_playlist_tree->reOrderAsSorted();
    }
    
    //
    //  Sort all the selectable content trees
    //  
    //  Yes, yes, this is all a lot of sorting, but it's getting done off in
    //  it's own thread.
    //
    
    QIntDictIterator<UIListGenericTree> it( selectable_content_trees ); 
    for ( ; it.current(); ++it )
    {
        it.current()->sortByAttributeThenByString(2);
        it.current()->reOrderAsSorted();
    }
    
}



void MfdContentCollection::setupPixmaps()
{
    if (!pixmaps_are_setup)
    {
        if (client_height != 600 || client_width != 800)
        {
            pixcontainer = scalePixmap((const char **)container_pix_xpm, client_wmult, client_hmult);
            pixeditplaylist = scalePixmap((const char **)edit_playlist_pix_xpm, client_wmult, client_hmult);
            pixedittrack = scalePixmap((const char **)edit_track_pix_xpm, client_wmult, client_hmult);
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
            pixedittrack = new QPixmap((const char **)edit_track_pix_xpm);
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

void MfdContentCollection::markNodeAsHeld(UIListGenericTree *node, bool held_or_not)
{

    if (node->getAttribute(3) < 0)
    {
        //
        //  It's a playlist
        //
        
        if(held_or_not)
        {
            node->setPixmap(pixeditplaylist);
        }
        else
        {
            node->setPixmap(pixplaylist);
        }
    }
    else if (node->getAttribute(3) > 0)
    {
        //
        //  It's a track
        //
        
        if(held_or_not)
        {
            node->setPixmap(pixedittrack);
        }
        else
        {
            node->setPixmap(pixtrack);
        }
    }
    else
    {
        cerr << "mfdcontent.o: asked to mark a node as held, "
             << "but it's neither a track nor a playlist"
             << endl;
    }
}


MfdContentCollection::~MfdContentCollection()
{
    audio_item_dictionary.clear();
    audio_playlist_dictionary.clear();
    selectable_content_trees.clear();
    
    if (audio_artist_tree)
    {
        delete audio_artist_tree;
        audio_artist_tree = NULL;
    }

    if (audio_genre_tree)
    {
        delete audio_genre_tree;
        audio_genre_tree = NULL;
    }

    if (audio_playlist_tree)
    {
        delete audio_playlist_tree;
        audio_playlist_tree = NULL;
    }

    if (audio_collection_tree)
    {
        delete audio_collection_tree;
        audio_collection_tree = NULL;
    }
    
    if (new_playlist_tree)
    {
        delete new_playlist_tree;
        new_playlist_tree = NULL;
    }

    if (editable_playlist_tree)
    {
        delete editable_playlist_tree;
        editable_playlist_tree = NULL;
    }
}

