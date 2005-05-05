#ifndef MFEDIALOG_H_
#define MFEDIALOG_H_
/*
	mfedialog.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <qintdict.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/uilistbtntype.h>
#include <mfdclient/mfdinterface.h>
#include <mfdclient/mfdcontent.h>

#include "mfdinfo.h"
#include "netflasher.h"
#include "playlistdialog.h"

typedef QValueVector<int> IntVector;

class SpeakerTracker;
class StereoScope;

class MfeDialog : public MythThemedDialog
{

    Q_OBJECT

  public:

    MfeDialog(
                MythMainWindow *parent, 
                QString window_name,
                QString theme_filename,
                MfdInterface *an_mfd_interface
             ); 

    ~MfeDialog();

    void keyPressEvent(QKeyEvent *e);

  public slots:

    void doSillyThings();
    void handleTreeSignals(UIListGenericTree *node);
    void doPlaylistDialog(int collection_id, int playlist_id, const QString playlist_name);
    void mfdDiscovered(int which_mfd, QString name, QString host, bool found);
    void audioPluginDiscovered(int which_mfd);
    void paused(int which_mfd, bool paused); 
    void stopped(int which_mfd);
    void playing(int, int, int, int, int, int, int, int, int);
    void changeMetadata(int, MfdContentCollection*);
    void playlistCheckDone();
    void cycleMfd();
    void stopAudio();
    void togglePause();
    void seekAudio(bool forward_or_back);
    void nextPrevAudio(bool next_or_prev);
    void speakerList(int which_mfd, QPtrList<SpeakerTracker>* speakers);
    void hideNewPlaylistPopup();
    void createNewPlaylist();
    void audioData(int which_mfd, uchar *audio_data, int length);

  private:

    void wireUpTheme();
    void connectUpMfd();    
    void syncToCurrentMfd();
    void updateConnectionList();
    void switchToMfd(int an_mfd_id);
    void updateSpeakerDisplay();
    void showNewPlaylistPopup();
    QString constructPlaylistName();
    
    MfdInterface    *mfd_interface;   

    //
    //  Collection of available mfd's content
    //
    
    QIntDict<MfdInfo>  available_mfds;
    MfdInfo           *current_mfd;
    
    //
    //  Pointer to the theme defined GUI widget that holds the tree and draw
    //  it when asked to
    //

    UIListTreeType      *menu;

    //
    //  Root node of the menu tree
    //

    UIListGenericTree   *menu_root_node;

    //
    //  Main subnodes of the root node
    //

    UIListGenericTree   *browse_node;
    UIListGenericTree   *manage_node;
    UIListGenericTree   *connect_node;
    UIListGenericTree   *setup_node;
    
    //
    //  Speaker Node, subset of setup
    //
    
    UIListGenericTree   *speaker_node;
    
    //
    //  GUI widgets
    //
    
    UITextType       *current_mfd_text;
    UIMultiTextType  *now_playing_texts;
    UIStatusBarType  *time_progress;

    UIImageType *stop_button;
    UIImageType *play_button;
    UIImageType *pause_button;

    UIPushButtonType *ff_button;
    UIPushButtonType *rew_button;

    UIPushButtonType *next_button;
    UIPushButtonType *prev_button;

    UIImageType *network_icon;
    UIImageType *background_image;

    UIBlackHoleType *visual_blackhole;
    StereoScope     *visualizer;

    //
    //  Thing that flashes to indicate network (ie. mdcap data arrival)
    //  activity.
    //
    
    NetFlasher *net_flasher;

    //
    //  A pointer to the playlist editing dialog (so we can see if it's
    //  active when, for example, new data arrives)
    //
    
    PlaylistDialog *playlist_dialog;
    int mfd_id_for_playlist_dialog;
    int content_collection_for_new_playlist;


    //
    //  Widgets for the New Playlist popup
    //
    
    MythPopupBox        *playlist_popup;
    MythRemoteLineEdit  *playlist_name_edit;

    //
    //  Some data storage for building random playlist names
    //    
    
    QStringList possessors;
    QStringList modifiers;
    QStringList nouns;

    //
    //  A little timer to decide to make a menu either appear at the
    //  beginning or where the user last was depending on how much time has
    //  elapsed
    //

    QTime   menu_lastuse_time;

};



#endif
