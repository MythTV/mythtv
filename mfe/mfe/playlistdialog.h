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

class PlaylistDialog : public MythThemedDialog
{

    Q_OBJECT

  public:

    PlaylistDialog(
                    MythMainWindow *parent, 
                    QString window_name,
                    QString theme_filename,
                    MfdInfo *an_mfd,
                    UIListGenericTree *a_playlist_tree,
                    UIListGenericTree *all_content_tree,
                    const QString&  playlist_name
                  ); 

    void keyPressEvent(QKeyEvent *e);

    ~PlaylistDialog();

  public slots:
  
    void handlePlaylistEntered(UIListTreeType*, UIListGenericTree*);
    void handlePlaylistSelected(UIListTreeType*, UIListGenericTree*);
    void startHoldingTrack(UIListGenericTree *node);
    void stopHoldingTrack();
    void moveHeldUpDown(bool up_or_down);
    void handleContentEntered(UIListTreeType*, UIListGenericTree*);
    void handleContentSelected(UIListTreeType*, UIListGenericTree*);
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

  private:
  
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

    UIImageType         *mfe_genre_icon;
    UIImageType         *mfe_artist_icon;
    UIImageType         *mfe_album_icon;
    UIImageType         *mfe_title_icon;

    UITextType          *playlist_name_label;
    UITextType          *playlist_source_label;
    UITextType          *playlist_type_label;
    UITextType          *playlist_numtracks_label;
    UITextType          *edit_title;

    //
    //  The playlist tree that gets handed to us
    //
    
    UIListGenericTree   *playlist_tree;
    UIListGenericTree   *content_tree;


    MfdInfo    *current_mfd;   

    bool                holding_track;
    UIListGenericTree*  held_track;
};



#endif
