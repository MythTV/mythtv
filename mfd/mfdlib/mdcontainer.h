#ifndef MDCONTAINER_H_
#define MDCONTAINER_H_
/*
	mdcontainer.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for metadata container and the thread(s) that fill(s) it

*/

#include <qstring.h>
#include <qintdict.h>
#include <qptrlist.h>
#include <qthread.h>
#include <qwaitcondition.h>
#include <qvaluelist.h>
#include <qdeepcopy.h>

#include "metadata.h"


class MFD;


enum MetadataCollectionContentType
{
    MCCT_unknown = 0,
    MCCT_audio,
    MCCT_video
};

enum MetadataCollectionLocationType
{
    MCLT_host = 0,
    MCLT_lan,
    MCLT_net
};

enum MetadataCollectionServerType
{
    MST_unknown = 0,
    MST_daap_myth,
    MST_daap_itunes41,
    MST_daap_itunes42,
    MST_daap_itunes43,
    MST_daap_itunes45,
    MST_daap_itunes46,
    MST_daap_itunes47
};

class MetadataContainer
{
    //
    //  A base class for objects that hold metadata
    //


  public:
  
    MetadataContainer(
                        QString a_name,
                        MFD *l_parent,
                        int l_unique_identifier,
                        MetadataCollectionContentType  l_content_type,
                        MetadataCollectionLocationType l_location_type,
                        MetadataCollectionServerType l_server_type = MST_unknown
                     );

    virtual ~MetadataContainer();

    void                log(const QString &log_message, int verbosity);
    void                warning(const QString &warning_message);
    int                 getIdentifier(){return unique_identifier;}
    bool                isAudio();
    bool                isVideo();
    bool                isLocal();
    bool                isEditable() { return editable; }
    void                setEditable(bool x) { editable = x; }
    bool                isRipable() { return ripable; }
    void                setRipable(bool x) { ripable = x; }
    uint                getMetadataCount();
    uint                getPlaylistCount();
    void                setName(const QString &a_name){my_name = a_name;}
    QString             getName(){return my_name;}
    int                 getGeneration(){ return generation;}

    Metadata*           getMetadata(int item_id);
    QIntDict<Metadata>* getMetadata(){return current_metadata;}

    Playlist*           getPlaylist(int pl_id);
    QIntDict<Playlist>* getPlaylists(){return current_playlists;}

    QValueList<int>     getMetadataAdditions(){return metadata_additions;}
    QValueList<int>     getMetadataDeletions(){return metadata_deletions;}
    QValueList<int>     getPlaylistAdditions(){return playlist_additions;}
    QValueList<int>     getPlaylistDeletions(){return playlist_deletions;}

    void                dataSwap(   
                                    QIntDict<Metadata>* new_metadata, 
                                    QValueList<int> metadata_in,
                                    QValueList<int> metadata_out,
                                    QIntDict<Playlist>* new_playlists,
                                    QValueList<int> playlist_in,
                                    QValueList<int> playlist_out,
                                    bool rewrite_playlists = false,
                                    bool prune_dead = false
                                    
                                );

    void                dataDelta(   
                                    QIntDict<Metadata>* new_metadata, 
                                    QValueList<int> metadata_in,
                                    QValueList<int> metadata_out,
                                    QIntDict<Playlist>* new_playlists,
                                    QValueList<int> playlist_in,
                                    QValueList<int> playlist_out,
                                    bool rewrite_playlists = false,
                                    bool prune_dead = false,
                                    bool map_out_all_playlists = false
                                    
                                );

    MetadataCollectionContentType  getContentType(){ return content_type;}
    MetadataCollectionLocationType getLocationType(){ return location_type;}
    MetadataCollectionServerType   getServerType(){ return server_type;}
    
    int     bumpPlaylistId(){current_playlist_id++; return current_playlist_id;}
    
  protected:

    void    mapPlaylists(
                            QIntDict<Playlist>* new_playlists, 
                            QValueList<int> playlist_in,
                            QValueList<int> playlist_out,
                            bool delta,
                            bool prune_dead = false
                        );
    void    checkPlaylists();
  
    MFD *parent;
    int unique_identifier;
    
    MetadataCollectionContentType  content_type;
    MetadataCollectionLocationType location_type;
    MetadataCollectionServerType   server_type;

    QIntDict<Metadata>          *current_metadata;
    QDeepCopy<QValueList<int> > metadata_additions;
    QDeepCopy<QValueList<int> > metadata_deletions;

    QIntDict<Playlist>          *current_playlists;
    QDeepCopy<QValueList<int> > playlist_additions;
    QDeepCopy<QValueList<int> > playlist_deletions;
    
    QString my_name;
    int     current_playlist_id;
    int     generation;                        
    bool    editable;
    bool    ripable;
};

#endif
