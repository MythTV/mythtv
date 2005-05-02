#ifndef PLAYLISTDIALOG_H_
#define PLAYLISTDIALOG_H_
/*
	playlistdialog.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <mythtv/mythdialogs.h>
#include <mythtv/uilistbtntype.h>
#include <mfdclient/mfdinterface.h>

#include "mfdinfo.h"
#include "netflasher.h"

class PlaylistDialog : public MythThemedDialog
{

    Q_OBJECT

  public:

    PlaylistDialog(
                    MythMainWindow *parent, 
                    QString window_name,
                    QString theme_filename,
                    MfdInterface *an_mfd_interface,
                    MfdInfo *an_mfd,
                    UIListGenericTree *a_pristine_playlist_tree,
                    UIListGenericTree *a_pristine_content_tree,
                    UIListGenericTree *a_working_playlist_tree,
                    UIListGenericTree *a_working_content_tree,
                    const QString&  playlist_name
                  ); 

    void keyPressEvent(QKeyEvent *e);
    void playlistCheckDone();
    void swapToNewPristine();
    bool commitEdits();
    ~PlaylistDialog();

    UIListGenericTree* getWorkingPlaylist(){return working_playlist_tree; }
    bool playlistDeletedElsewhere(){ return playlist_deleted_elsewhere; }

  public slots:
  
    void handlePlaylistEntered(UIListTreeType*, UIListGenericTree*);
    void handlePlaylistSelected(UIListTreeType*, UIListGenericTree*);
    void handleContentEntered(UIListTreeType*, UIListGenericTree*);
    void handleContentSelected(UIListTreeType*, UIListGenericTree*);

  private:
  
    void allowEditing(bool yes_or_no);
    void startHoldingTrack(UIListGenericTree *node);
    void stopHoldingTrack();
    void moveHeldUpDown(bool up_or_down);
    void countAndDisplayPlaylistTracks();
    void setDisplayInfo(
                        bool is_a_playlist,
                        const QString &string1 = "", 
                        const QString &string2 = "",
                        const QString &string3 = "",
                        const QString &string4 = ""
                       );
    void clearDisplayInfo(bool clear_only_right = true);
    void fillWhatWeKnow(
                        UIListGenericTree *node, 
                        QString &genre_string, 
                        QString &artist_string, 
                        QString &album_string
                       );

    void toggleItem(UIListGenericTree *node, bool turn_on);
    void toggleTree(UIListGenericTree *node, bool turn_on);
    void updatePlaylistDeltas(bool addition, int item_id);
    void wireUpTheme();

    //
    //  Pointer to the theme defined GUI widget that holds the playlist tree
    //

    UIListTreeType      *playlist_tree_menu;
    UIListTreeType      *content_tree_menu;
    UIListTreeType      *current_menu;

    UIImageType         *leftarrow_on;
    UIImageType         *leftarrow_off;
    UIImageType         *rightarrow_on;
    UIImageType         *rightarrow_off;

    UITextType          *genre_text;
    UITextType          *artist_text;
    UITextType          *album_text;
    UITextType          *title_text;
        
    UITextType          *genre_label;
    UITextType          *artist_label;
    UITextType          *album_label;
    UITextType          *title_label;

    UITextType          *playlist_subtitle;
    UIImageType         *empty_grey;
    UIImageType         *empty_grey_high;

    UIImageType         *mfe_genre_icon;
    UIImageType         *mfe_artist_icon;
    UIImageType         *mfe_album_icon;
    UIImageType         *mfe_title_icon;

    UITextType          *playlist_name_label;
    UITextType          *playlist_source_label;
    UITextType          *playlist_type_label;
    UITextType          *playlist_numtracks_label;
    UITextType          *edit_title;
    UIImageType         *network_icon;

    //
    //  Thing that flashes to indicate network (ie. mdcap data arrival)
    //  activity.
    //
    
    NetFlasher *net_flasher;

    //
    //  Pointers to uneditable, editable, and current playlist and content
    //  tree
    //
    
    UIListGenericTree   *working_playlist_tree;
    UIListGenericTree   *working_content_tree;

    UIListGenericTree   *pristine_playlist_tree;
    UIListGenericTree   *pristine_content_tree;
    
    UIListGenericTree   *current_playlist_tree;
    UIListGenericTree   *current_content_tree;
    


    MfdInterface        *mfd_interface;
    MfdInfo             *current_mfd;   

    bool                holding_track;
    UIListGenericTree*  held_track;

    bool                editing_allowed;
    bool                something_changed_position;

    //
    //  Fast Qt collection class for keeping track of edits
    //
    
    QIntDict<bool>  playlist_additions;
    QIntDict<bool>  playlist_deletions;

    //
    //  String list for saving the route to the current playlist node
    //  through a working->pristine->working cycyle
    //
    
    QStringList persistent_route_to_playlist;

    //
    //  A flag to keep track of if the playlist we were editing was deleted
    //  out from under us
    //
    
    bool    playlist_deleted_elsewhere;

};



#endif
