#ifndef MFDINFO_H_
#define MFDINFO_H_
/*
	mfdinfo.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <qstring.h>
#include <qstringlist.h>

#include <mfdclient/mfdcontent.h>
#include <mfdclient/metadata.h>

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

    void                    setCurrentPlayingData(int which_container, int which_metadata, int numb_seconds);
    QString                 getPlayingString(){return playing_string;}
    void                    clearCurrentPlayingData(){playing_string = "";}
    double                  getPercentPlayed(){return played_percentage;}
    
    void                    setPauseState(bool new_state){ pause_state = new_state; }
    bool                    getPauseState(){return pause_state;}
    
  private:
  
    int         id;
    QString     name;
    QString     host;
    bool        showing_menu;

    MfdContentCollection *mfd_content_collection;
    QStringList previous_tree_position;
    QString playing_string;
    double      played_percentage;
    bool        pause_state;
};


#endif
