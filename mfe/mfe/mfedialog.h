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

typedef QValueVector<int> IntVector;


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

    void handleTreeSignals(UIListGenericTree *node);
    void mfdDiscovered(int which_mfd, QString name, QString host, bool found);
    void paused(int which_mfd, bool paused); 
    void stopped(int which_mfd);
    void playing(int, int, int, int, int, int, int, int, int);
    void changeMetadata(int, MfdContentCollection*);
    void cycleMfd();
    void stopAudio();
    void togglePause();
    void seekAudio(bool forward_or_back);
    void nextPrevAudio(bool next_or_prev);

  private:

    void wireUpTheme();
    void connectUpMfd();    
    void syncToCurrentMfd();
    void updateConnectionList();
    void switchToMfd(int an_mfd_id);
    
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
    //  GUI widgets
    //
    
    UITextType       *current_mfd_text;
    UITextType       *now_playing_text;
    UIStatusBarType  *time_progress;

    UIImageType *stop_button;
    UIImageType *play_button;
    UIImageType *pause_button;
};



#endif
