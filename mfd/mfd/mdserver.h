#ifndef MDSERVER_H_
#define MDSERVER_H_
/*
	mdserver.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a threaded object that opens a server
	port and explains the state of the metadata to any
	client that asks

*/

#include <qvaluelist.h>

#include "../mfdlib/mfd_plugin.h"

class MFD;

class MetadataServer : public MFDServicePlugin
{
    //
    //  Don't be confused by the fact that this inherits from
    //  MFDServcePlugin. It is not dynamically loaded, but is part of the
    //  mfd core (just so happens that MFDServicePlugin has most of the bits
    //  and pieces we need for a metadata serving object already built in).
    //

  public:
  
    MetadataServer(MFD *owner, int port);
    ~MetadataServer();

    void    run();
    void    doSomething(const QStringList &tokens, int socket_identifier);

    void                         lockMetadata();
    void                         unlockMetadata();

    void                         lockPlaylists();
    void                         unlockPlaylists();

    QPtrList<MetadataContainer>* getMetadataContainers(){return metadata_containers;}
    uint                         getMetadataAudioGeneration();
    uint                         getAllAudioMetadataCount();
    uint                         getAllAudioPlaylistCount();

    Metadata*                    getMetadataByUniversalId(uint id);
    Playlist*                    getPlaylistByUniversalId(uint id);
    


    MetadataContainer*           createContainer(
                                                    MetadataCollectionContentType content_type, 
                                                    MetadataCollectionLocationType location_type
                                                );

    void                         doAtomicDataSwap(
                                                    MetadataContainer *which_one,
                                                    QIntDict<Metadata>* new_metadata,
                                                    QValueList<int> metadata_additions,
                                                    QValueList<int> metadata_deletions,
                                                    QIntDict<Playlist>* new_playlists
                                                 );

    //uint                         getMetadataVideoGeneration(){return metadata_video_generation;}
    //int                          bumpMetadataId();
    //int                          bumpPlaylistId();
    //void                         lockPlaylists();
    //void                         unlockPlaylists();
    //Playlist*                    getPlaylist(int id);

    // void makeMetadataContainers();
    //int  bumpMetadataAudioGeneration();
    //int  bumpMetadataVideoGeneration();
    


  private:

    int                          bumpContainerId();


    QPtrList<MetadataContainer> *metadata_containers;
    QMutex                      metadata_mutex;
    QMutex                      playlists_mutex;

    QMutex                      metadata_audio_generation_mutex;
    uint                        metadata_audio_generation;

    int                         container_identifier;
    QMutex                      container_identifier_mutex;

    //uint                        metadata_video_generation;
    
    //int                         metadata_id;
    //int                         playlist_id;

    //QMutex                      bump_metadata_id_mutex;
    //QMutex                      bump_playlist_id_mutex;

  
};


#endif
