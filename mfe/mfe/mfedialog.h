#ifndef MFEDIALOG_H_
#define MFEDIALOG_H_
/*
	mfedialog.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <qintdict.h>

#include <mythtv/mythdialogs.h>
#include <mfdclient/mfdinterface.h>

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
    void clearPlayingDisplay();

  public slots:

    void handleTreeListSignals(int node_int, IntVector *attributes);
    void mfdDiscovered(int which_mfd, QString name, QString host, bool found);
    void paused(int which_mfd, bool paused); 
    void stopped(int which_mfd);
    void playing(int, int, int, int, int, int, int, int, int);
    void changeMetadata(int, GenericTree*);
    void cycleMfd();
    void stopAudio();

  private:

    void wireUpTheme();
    void connectUpMfd();    
    
    MfdInterface    *mfd_interface;   

    //
    //  Collection of available mfd's
    //
    
    QIntDict<MfdInfo> available_mfds;
    MfdInfo           *current_mfd;
    
    //
    //  Theme-related "widgets"
    //

    UIManagedTreeListType *music_tree_list;

    UITextType            *mfd_text;
    UITextType            *playlist_text;
    UITextType            *container_text;
    UITextType            *metadata_text;
    UITextType            *audio_text;

    GenericTree           *no_mfds_tree;
};



#endif
