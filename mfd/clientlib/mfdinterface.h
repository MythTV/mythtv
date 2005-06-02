#ifndef MFDINTERFACE_H_
#define MFDINTERFACE_H_
/*
	mfdinterface.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <qobject.h>
#include <qptrlist.h>
#include <qintdict.h>

#include "mfdcontent.h"

class DiscoveryThread;
class PlaylistChecker;
class MfdInstance;
class SpeakerTracker;
class VisualBase;

class MfdInterface : public QObject
{

  Q_OBJECT

  public:

    MfdInterface(int client_screen_width = 800, int client_screen_height = 600);
    ~MfdInterface();

    int getClientWidth(){  return client_width;  }
    int getClientHeight(){ return client_height; }

    //
    //  Methods that the linking client application can call to make an mfd
    //  do something
    //

    void playAudio(int which_mfd, int container, int type, int which_id, int index=0);
    void stopAudio(int which_mfd);
    void pauseAudio(int which_mfd, bool on_or_off);
    void seekAudio(int which_mfd, int how_much);
    void nextAudio(int which_mfd);
    void prevAudio(int which_mfd);
    void askForStatus(int which_mfd);
    void toggleSpeakers(int which_mfd, int which_speaker, bool on_or_off);

    //
    //  For sending updates back the mfd metadata server
    //
    
    void commitListEdits(
                            int which_mfd, 
                            UIListGenericTree *playlist_tree,
                            bool new_playlist,
                            QString playlist_name
                        );

    //
    //  To get rid of playlists
    //
    
    void    deletePlaylist(
                            int which_mfd,
                            int which_collection,
                            int which_playlist
                          );
                            
    //
    //  Rip a playlist
    //
    
    void    ripPlaylist(
                        int which_mfd,
                        int which_collection,
                        int which_playlist
                       );
                

    //
    //  Methods that will ask this library to perform some background work
    //  on behalf of the linked in client
    //    
    
    void startPlaylistCheck(
                            MfdContentCollection *mfd_collection,
                            UIListGenericTree *playlist, 
                            UIListGenericTree *content,
                            QIntDict<bool> *playlist_additions,
                            QIntDict<bool> *playlist_deletions
                           );
    void stopPlaylistCheck();


    //
    //  Methods that have something to do with visualizations
    //

    void        turnVizDataStreamOn(int which_mfd, bool yes_or_no);
    void        registerVisualization(VisualBase *viz);
    void        deregisterVisualization();
    VisualBase* getRegisteredVisualization(){ return visualization; }
    
  signals:

    //
    //  Signals we send out when something happens. Linking client wants to
    //  connect to all or some of these
    //

    void mfdDiscovery(int, QString, QString, bool);
    void audioPluginDiscovery(int);
    void transcoderPluginDiscovery(int);
    void audioPaused(int, bool);
    void audioStopped(int);
    void audioPlaying(int, int, int, int, int, int, int, int, int);
    void metadataChanged(int, MfdContentCollection*);
    void playlistCheckDone();
    void speakerList(int, QPtrList<SpeakerTracker> *speakers);
    void audioData(int, uchar*, int);

  protected:
  
    void customEvent(QCustomEvent *ce);
 

  private:

    int             bumpMfdId(){ ++mfd_id_counter; return mfd_id_counter;}
    MfdInstance*    findMfd(
                            const QString &a_host,
                            const QString &an_ip_addesss,
                            int a_port
                           );
    void            swapMetadata(int which_mfd, MfdContentCollection *new_collection);
  
    DiscoveryThread       *discovery_thread;
    QPtrList<MfdInstance> *mfd_instances;
    int                   mfd_id_counter;
    PlaylistChecker       *playlist_checker;
    
    QIntDict<MfdContentCollection>   mfd_collections;
    
    int client_width;
    int client_height;
    
    VisualBase *visualization;

};

#endif
