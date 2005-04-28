#ifndef MFDINFO_H_
#define MFDINFO_H_
/*
	mfdinfo.h

	Copyright (c) 2004-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <qstring.h>
#include <qstringlist.h>

#include <mfdclient/mfdcontent.h>
#include <mfdclient/metadata.h>
#include <mfdclient/speakertracker.h>


class MfdInfo
{

  public:

    MfdInfo(int an_id, const QString &a_name, const QString &a_host);
    ~MfdInfo();

    int                     getId(){ return id; }

    void                    setMfdContentCollection(MfdContentCollection *new_content){ mfd_content_collection=new_content; }
    MfdContentCollection*   getMfdContentCollection(){ return mfd_content_collection; } 
      
    QString                 getName(){return name;}
    QString                 getHost(){return host;}
    
    void                    setPreviousTreePosition(QStringList tree_list){previous_tree_position = tree_list;}
    QStringList             getPreviousTreePosition(){return previous_tree_position;}
    
    bool                    getShowingMenu(){return showing_menu;}
    void                    setShowingMenu(bool y_or_n){showing_menu = y_or_n;}
    
    AudioMetadata*          getAudioMetadata(int collection_id, int item_id);
    ClientPlaylist*         getAudioPlaylist(int collection_id, int item_id);
    UIListGenericTree*      getPlaylistTree(int collection_id, int playlist_id, bool pristine=false);
    UIListGenericTree*      getContentTree(int collection_id, bool pristine=false);
    void                    toggleItem(UIListGenericTree *node, bool turn_on);

    void                    toggleTree(
                                        UIListTreeType *menu, 
                                        UIListGenericTree *playlist_tree, 
                                        UIListGenericTree *node, 
                                        bool turn_on,
                                        QIntDict<bool> *playlist_additions,
                                        QIntDict<bool> *playlist_deletions
                                      );

    void                    updatePlaylistDeltas(
                                                    QIntDict<bool> *playlist_additions,
                                                    QIntDict<bool> *playlist_deletions,
                                                    bool addition,
                                                    int item_id
                                                );

    void                        alterPlaylist(UIListTreeType *menu, UIListGenericTree *playlist_tree, UIListGenericTree *node, bool turn_on);
    void                        setCurrentPlayingData();
    bool                        setCurrentPlayingData(int which_container, int which_metadata, int numb_seconds);
    QStringList                 getPlayingStrings(){return playing_strings;}
    void                        clearCurrentPlayingData(){playing_strings.clear(); previous_item = -2; previous_container = -2;}
    double                      getPercentPlayed(){return played_percentage;}
    
    void                        setPauseState(bool new_state){ pause_state = new_state; }
    bool                        getPauseState(){return pause_state;}

    bool                        knowsWhatsPlaying(){return knows_whats_playing;}
    bool                        isStopped(){return is_stopped;}
    void                        isStopped(bool yon){is_stopped = yon;}
    void                        markNodeAsHeld(UIListGenericTree* node, bool held_or_not);
    void                        printTree(UIListGenericTree* node, int depth=0);
    int                         countTracks(UIListGenericTree *playlist_tree);
    void                        setSpeakerList(QPtrList<SpeakerTracker>* speakers);
    QPtrList<SpeakerTracker>   *getSpeakerList(){ return &my_speakers; }
    
  private:
  
    int         id;
    QString     name;
    QString     host;
    bool        showing_menu;
    bool        knows_whats_playing;
    int         current_container;
    int         current_item;
    int         previous_container;
    int         previous_item;
    int         current_elapsed;

    MfdContentCollection       *mfd_content_collection;
    QStringList                 previous_tree_position;
    QStringList                 playing_strings;
    double                      played_percentage;
    bool                        pause_state;
    bool                        is_stopped;
    QPtrList<SpeakerTracker>    my_speakers;
};


#endif
