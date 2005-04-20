/*
	playlistchecker.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Threaded object. You hand it a tree of content and a playlist, it will
	mark all the checkmarks correctly in the content tree, and fire back
	(via an event) the two objects when it's done


*/

#include <iostream>
using namespace std;
#include <unistd.h>

#include <qapplication.h>

#include "playlistchecker.h"
#include "playlistentry.h"
#include "events.h"

PlaylistChecker::PlaylistChecker(MfdInterface *the_interface)
{
    //
    //  Get a pointer to the main MfdInterface objects so we can send it
    //  events when we're done
    //
    
    mfd_interface = the_interface;

    //
    //  By default, keep thread alive
    //

    keep_going = true;
    
    //
    //  At startup, we're not checking anything
    //
    
    keep_checking = false;
    is_checking = false;

    //
    //  Create a u shaped pipe so others can wake us out of a select
    //
    

    if(pipe(u_shaped_pipe) < 0)
    {
        warning("could not create a u shaped pipe");
    }

    //
    //  We start with no playlist and content to check
    //
    
    playlist_tree = NULL;
    content_tree = NULL;
    
}

void PlaylistChecker::run()
{

    while(keep_going)
    {

        //
        //  See if we have something to do
        //

        if(keep_checking)
        {
            is_checking = true;
            check();
            keep_checking_mutex.lock();
                keep_checking = false;
            keep_checking_mutex.unlock();
            is_checking = false;
        }

        fd_set fds;
        int nfds = 0;
        FD_ZERO(&fds);

        //
        //  Add the control pipe
        //
            
        FD_SET(u_shaped_pipe[0], &fds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    
        //
        //  We can sleep for a long time (if we need to do anything, our
        //  wakeUp() method will make the select return immediately
        //    
        
        timeout.tv_sec = 60;
        timeout.tv_usec = 0;
        
        
        //
        //  Sit in select until something happens
        //
        
        int result = select(nfds,&fds,NULL,NULL, &timeout);
        
        if(result < 0)
        {
            cerr << "playlistchecker.o: got an error on select (?), "
                 << "which is far from a good thing" 
                 << endl;
        }
    
        //
        //  In case data came in on out u_shaped_pipe, clean it out
        //

        if(FD_ISSET(u_shaped_pipe[0], &fds))
        {
            u_shaped_pipe_mutex.lock();
                char read_back[2049];
                read(u_shaped_pipe[0], read_back, 2048);
            u_shaped_pipe_mutex.unlock();
        }
    }
}

void PlaylistChecker::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
    wakeUp();
}

void PlaylistChecker::wakeUp()
{
    //
    //  Tell the main thread to wake up by sending some data to ourselves on
    //  our u_shaped_pipe. This may seem odd. It isn't.
    //

    u_shaped_pipe_mutex.lock();
        write(u_shaped_pipe[1], "wakeup\0", 7);
    u_shaped_pipe_mutex.unlock();
}

void PlaylistChecker::startChecking(
                                    MfdContentCollection *an_mfd_content,
                                    UIListGenericTree    *a_playlist, 
                                    UIListGenericTree    *some_content,
                                    QIntDict<bool>       *some_playlist_additions,
                                    QIntDict<bool>       *some_playlist_deletions
                                   )
{
    stopChecking();
    
    mfd_content = an_mfd_content;
    playlist_tree = a_playlist;
    content_tree = some_content;
    playlist_additions = some_playlist_additions;
    playlist_deletions = some_playlist_deletions;

    keep_checking_mutex.lock();
        keep_checking = true;
    keep_checking_mutex.unlock();

    wakeUp();
}

void PlaylistChecker::stopChecking()
{
    if (is_checking)
    {
        keep_checking_mutex.lock();
            keep_checking = false;
        keep_checking_mutex.unlock();
        
    }
}

void PlaylistChecker::check()
{
    if(mfd_content == NULL)
    {
        cerr << "playlistchecker.o: started to run check(), but mfd_content is NULL" << endl;
        return;
    }

    if(playlist_tree == NULL)
    {
        cerr << "playlistchecker.o: started to run check(), but playlist_tree is NULL" << endl;
        return;
    }

    if(content_tree == NULL)
    {
        cerr << "playlistchecker.o: started to run check(), but content_tree is NULL" << endl;
        return;
    }

    if(playlist_additions == NULL)
    {
        cerr << "playlistchecker.o: started to run check(), but playlist_additions is NULL" << endl;
        return;
    }

    if(playlist_deletions == NULL)
    {
        cerr << "playlistchecker.o: started to run check(), but playlist_deletions is NULL" << endl;
        return;
    }


    //
    //  Ask the MfdContentCollection object to check this content tree
    //  correctly (the excution of which, of course, happens in this thread,
    //  not in the main GUI thread!)
    //

    mfd_content->processContentTree(
                                    this, 
                                    playlist_tree, 
                                    content_tree, 
                                    playlist_additions,
                                    playlist_deletions
                                   );

    
    /*
    
        Fake load
        
    int i = 0;
    while(i < 1000000000)
    {
        i++;
        if(!keep_checking)
        {
            i = 1000000001;
        }
    }
    
    */
    
    
    //
    //  We're done, post an event to say "content is checked, the user may
    //  now edit with impunity"
    //

    if(keep_checking)
    {
        MfdPlaylistCheckedEvent *pce = new MfdPlaylistCheckedEvent();
        QApplication::postEvent(mfd_interface, pce);
    }
}

bool PlaylistChecker::keepChecking()
{
    bool return_value = true;
    keep_checking_mutex.lock();
        return_value = keep_checking;
    keep_checking_mutex.unlock();
    return return_value;
}


PlaylistChecker::~PlaylistChecker()
{
}

 
