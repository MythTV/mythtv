#ifndef PLAYLISTCHECKER_H_
#define PLAYLISTCHECKER_H_
/*
	playlistchecker.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Threaded object. You hand it a tree of content and a playlist, it will
	mark all the checkmarks correctly in the content tree, and fire back
	(via an event) the two objects when it's done

*/

#include <qthread.h>
#include <qmutex.h>

#include "mfdinterface.h"
#include "mdnsd/mdnsd.h"
#include "discovered.h"



class PlaylistChecker : public QThread
{

  public:

    PlaylistChecker(MfdInterface *the_interface);
    ~PlaylistChecker();
    
    void run();
    void stop();
    void wakeUp();

    void startChecking(
                        MfdContentCollection *an_mfd_content, 
                        UIListGenericTree *a_playlist, 
                        UIListGenericTree *some_content
                      );
    void stopChecking();
    void check();
    bool keepChecking();
    
    
  private:

    void   turnOffTree(UIListGenericTree *node);
  
    QMutex keep_going_mutex;
    bool   keep_going;
    
    QMutex keep_checking_mutex;
    bool   keep_checking;
    bool   is_checking;

    struct  timeval timeout;
    QMutex  u_shaped_pipe_mutex;
    int     u_shaped_pipe[2];

    
    MfdInterface            *mfd_interface;
    
    MfdContentCollection    *mfd_content;
    UIListGenericTree       *playlist_tree;
    UIListGenericTree       *content_tree;
};

#endif
