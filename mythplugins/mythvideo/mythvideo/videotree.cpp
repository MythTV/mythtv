#include <qapplication.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

#include <qdir.h>

#include "videotree.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/uitypes.h>
#include <mythtv/util.h>
#include <mythtv/mythmedia.h>
#include <mythtv/mythmediamonitor.h>
#include <mythtv/mythdbcon.h>

#include "videofilter.h"
#include "videolist.h"
const long WATCHED_WATERMARK = 10000; // Less than this and the chain of videos will 
                                      // not be followed when playing.


VideoTree::VideoTree(MythMainWindow *parent, QString window_name, 
                     QString theme_filename, const char *name)
         : MythThemedDialog(parent, window_name, theme_filename, name)
{
    curitem = NULL;
    popup = NULL;
    expectingPopup = false;
    video_tree_data = NULL;
    current_parental_level = gContext->GetNumSetting("VideoDefaultParentalLevel", 1);

    file_browser = gContext->GetNumSetting("VideoTreeNoDB", 0);

    //
    //  Theme and tree stuff
    //

    wireUpTheme();
    video_list = new VideoList("VideoTree");
    video_tree_root = NULL;
    currentVideoFilter = new VideoFilterSettings(true, "VideoTree");
    
    buildVideoList();
    
    //  
    //  Tell the tree list to highlight the 
    //  first entry and then display it
    //
    
    video_tree_list->setCurrentNode(video_tree_data);
    if(video_tree_data->childCount() > 0)
    {
        video_tree_list->setCurrentNode(video_tree_data->getChildAt(0, 0));
    }

    updateForeground();
    video_tree_list->enter();
}

VideoTree::~VideoTree()
{
    if (currentVideoFilter)
        delete currentVideoFilter;

    delete video_tree_root;
}

void VideoTree::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
            video_tree_list->select();
        else if (action == "UP")
            video_tree_list->moveUp();
        else if (action == "DOWN")
            video_tree_list->moveDown();
        else if (action == "LEFT")
            video_tree_list->popUp();
        else if (action == "RIGHT")
            video_tree_list->pushDown();
        else if (action == "PAGEUP")
            video_tree_list->pageUp();
        else if (action == "PAGEDOWN")
            video_tree_list->pageDown();
        else if (action == "INFO")
            doMenu(true);            
        else if (action == "MENU")
            doMenu(false);

        else if (action == "1" || action == "2" || action == "3" ||
                 action == "4")
        {
            setParentalLevel(action.toInt());
        }
        else
            handled = false;
    }

        if (!handled)
    {
    
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "PLAYBACK")
            {
                handled = true;
                playVideo(-1);
            }
        }            
    }

    
    if (!handled) 
        MythThemedDialog::keyPressEvent(e);
}

bool VideoTree::checkParentPassword()
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    QString password = gContext->GetSetting("VideoAdminPassword");
    if(password.length() < 1)
    {
        return true;
    }
    //
    //  See if we recently (and succesfully)
    //  asked for a password
    //
    
    if(last_time_stamp.length() < 1)
    {
        //
        //  Probably first time used
        //

        cerr << "videotree.o: Could not read password/pin time stamp. "
             << "This is only an issue if it happens repeatedly. " << endl;
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp, Qt::TextDate);
        if(last_time.secsTo(curr_time) < 120)
        {
            //
            //  Two minute window
            //
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    
    //
    //  See if there is a password set
    //
    
    if(password.length() > 0)
    {
        bool ok = false;
        MythPasswordDialog *pwd = new MythPasswordDialog(tr("Parental Pin:"),
                                                         &ok,
                                                         password,
                                                         gContext->GetMainWindow());
        pwd->exec();
        delete pwd;
        if(ok)
        {
            //
            //  All is good
            //

            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }   
    else
    {
        return true;
    } 
    return false;
}

void VideoTree::setParentalLevel(int which_level)
{
    if(which_level < 1)
    {
        which_level = 1;
    }
    if(which_level > 4)
    {
        which_level = 4;
    }
    
    if(checkParentPassword())
    {
        current_parental_level = which_level;
        pl_value->SetText(QString("%1").arg(current_parental_level));
        video_tree_data = NULL;
        video_tree_root->deleteAllChildren();

        buildVideoList();
    
        //  
        //  Tell the tree list to highlight the 
        //  first child and then draw the GUI as
        //  it now stands
        //
    
        video_tree_list->setCurrentNode(video_tree_data);
        if(video_tree_data->childCount() > 0)
        {
            video_tree_list->setCurrentNode(video_tree_data->getChildAt(0, 0));
        }
        updateForeground();
        video_tree_list->enter();
    }
}

void VideoTree::buildVideoList()
{
    int file_browser = gContext->GetNumSetting("VideoTreeNoDB", 0);

    video_tree_root = video_list->buildVideoList(file_browser, false,
                                                current_parental_level);
    if (video_tree_root->childCount() > 0)
        video_tree_data = video_tree_root->getChildAt(0,0);
    else video_tree_data = video_tree_root;
    video_list->wantVideoListUpdirs(false);
    
    video_tree_list->assignTreeData(video_tree_root);
    video_tree_list->sortTreeByString();
    video_tree_list->sortTreeBySelectable();
}

void VideoTree::handleTreeListEntry(int node_int, IntVector*)
{
    if(node_int > -1)
    {
        //
        //  User has navigated to a video file
        //
        
        QString extension = "";
        QString player = "";
        QString unique_player;
            
        curitem = video_list->getVideoListMetadata(node_int);
        if (curitem == NULL)
            return;

        video_title->SetText(curitem->Title());
        video_file->SetText(curitem->Filename().section("/", -1));
        video_poster->SetImage(curitem->CoverFile());
        video_poster->LoadImage();
        if (video_plot)
                video_plot->SetText(curitem->Plot());

        extension = curitem->Filename().section(".", -1, -1);

        unique_player = curitem->PlayCommand();
        if(unique_player.length() > 0)
        {
            player = unique_player;
        }
        else
        {
            player = gContext->GetSetting("VideoDefaultPlayer");
        }

        //
        //  Find the player for this file
        //
        

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT playcommand, use_default FROM "
                      "videotypes WHERE extension = :EXT ;");
        query.bindValue(":EXT", extension);

        if(query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            if(!query.value(1).toBool() && unique_player.length() < 1)
            {
                //
                //  This file type is defined and
                //  it is not set to use default player
                //
                player = query.value(0).toString();                
            }
        }

        video_player->SetText(player); 
    }
    else
    {
        //
        //  not a leaf node 
        //  (no video file here, just a directory)
        //
        curitem = NULL;
        
        if (video_poster)
            video_poster->ResetImage();
        
        if (video_plot)
            video_plot->SetText("");
        
        if (video_title)            
            video_title->SetText("");

        if (video_file)            
            video_file->SetText("");
        
        if (video_player)                
            video_player->SetText(""); 
        
                    
        
    }
}

void VideoTree::playVideo(int node_number)
{
    if(node_number > -1)
    {
        playVideo(curitem);        
    }
}

void VideoTree::handleTreeListSelection(int node_int, IntVector *)
{
    if(node_int > -1)
    {
        //
        //  Play this (chain of?) file(s) 
        //
        
        int which_file = node_int;
        QTime playing_time;

        while(which_file > -1)
        {
            playing_time.start();
            playVideo(which_file);
            if(!file_browser)
            {
                if(playing_time.elapsed() > 10000)
                {
                    //
                    //  More than ten seconds have gone by,
                    //  so we'll keep following the chain of
                    //  files to play
                    //

                    Metadata *node_data = new Metadata();
                    node_data->setID(which_file);
                    node_data->fillDataFromID();
                    which_file = node_data->ChildID();
                    //cout << "Just set which file to " << which_file << endl;
                    delete node_data;
                }
                else
                {
                    which_file = -1;
                }
            }
            else
            {
                which_file = -1;
            }
        }
        
        //
        //  Go back to tree browsing
        //

        video_tree_list->deactivate();
        gContext->GetMainWindow()->raise();
        gContext->GetMainWindow()->setActiveWindow();
        gContext->GetMainWindow()->currentWidget()->setFocus();
    }
}

void VideoTree::wireUpTheme()
{
    //
    //  Use inherited getUI* methods to hook pointers
    //  to objects on the screen we need to be able to
    //  control
    //
    
    //
    //  Big tree thingy at the top
    //
    
    video_tree_list = getUIManagedTreeListType("videotreelist");
    if(!video_tree_list)
    {
        cerr << "videotree.o: Couldn't find a video tree list in your theme" << endl;
        exit(0);
    }
    video_tree_list->showWholeTree(true);
    video_tree_list->colorSelectables(true);
    connect(video_tree_list, SIGNAL(nodeSelected(int, IntVector*)), this, SLOT(handleTreeListSelection(int, IntVector*)));
    connect(video_tree_list, SIGNAL(nodeEntered(int, IntVector*)), this, SLOT(handleTreeListEntry(int, IntVector*)));

    //
    //  Text area's
    //
    video_title = getUITextType("video_title");
    if(!video_title)
    {
        cerr << "videotree.o: Couldn't find a text area called video_title in your theme" << endl;
    }
    video_file = getUITextType("video_file");
    if(!video_file)
    {
        cerr << "videotree.o: Couldn't find a text area called video_file in your theme" << endl;
    }
    video_player = getUITextType("video_player");
    if(!video_player)
    {
        cerr << "videotree.o: Couldn't find a text area called video_player in your theme" << endl;
    }
    
    //
    //  Cover Art
    //
    video_poster = getUIImageType("video_poster");
    if(!video_poster)
    {
        cerr << "videotree.o: Couldn't find an image called video_poster in your theme" << endl;
    }
    
    //
    //  Status of Parental Level
    //
    
    pl_value = getUITextType("pl_value");
    if(pl_value)
    {
        pl_value->SetText(QString("%1").arg(current_parental_level));
    }
    
    video_plot = getUITextType("plot");
}

bool VideoTree::createPopup()
{
    if (!popup)
    {
        //allowPaint = false;
        popup = new MythPopupBox(gContext->GetMainWindow(), "video popup");
    
        expectingPopup = true;
    
        popup->addLabel(tr("Select action"));
        popup->addLabel("");
    }
    
    return (popup != NULL);
}


void VideoTree::doMenu(bool info)
{
    if (createPopup())
    {
        QButton *focusButton = NULL;
        if(info)
        {
            focusButton = popup->addButton(tr("Watch This Video"), this, SLOT(slotWatchVideo())); 
            popup->addButton(tr("View Full Plot"), this, SLOT(slotViewPlot()));
        }
        else
        {
            QButton *tempButton = NULL;
            if (!file_browser)
                focusButton = popup->addButton(tr("Filter Display"), this, SLOT(slotDoFilter()));
            
            tempButton = popup->addButton(tr("Switch to Browse View"), this, SLOT(slotVideoBrowser()));  
            
            if (!focusButton)
                focusButton = tempButton;
            
            popup->addButton(tr("Switch to Gallery View"), this, SLOT(slotVideoGallery()));
        }
        
        popup->addButton(tr("Cancel"), this, SLOT(slotDoCancel()));
        
        popup->ShowPopup(this, SLOT(slotDoCancel()));
    
        focusButton->setFocus();
    }
    
}


void VideoTree::slotVideoBrowser()
{
    cancelPopup();
    gContext->GetMainWindow()->JumpTo("Video Browser");
}

void VideoTree::slotVideoGallery()
{
    cancelPopup();
    gContext->GetMainWindow()->JumpTo("Video Gallery");
}

void VideoTree::slotDoCancel(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();
}

void VideoTree::cancelPopup(void)
{
    //allowPaint = true;
    expectingPopup = false;

    if (popup)
    {
        popup->hide();
        delete popup;

        popup = NULL;

        updateForeground();
        qApp->processEvents();
        setActiveWindow();
    }
}


void VideoTree::slotViewPlot()
{
    cancelPopup();
    
    if (curitem)
    {
        //allowPaint = false;
        MythPopupBox * plotbox = new MythPopupBox(gContext->GetMainWindow());
        
        QLabel *plotLabel = plotbox->addLabel(curitem->Plot(),MythPopupBox::Small,true);
        plotLabel->setAlignment(Qt::AlignJustify | Qt::WordBreak);
        
        QButton * okButton = plotbox->addButton(tr("Ok"));
        okButton->setFocus();
        
        plotbox->ExecPopup();
        delete plotbox;
        //allowPaint = true;
    }
    else
    {
        cerr << "no Item to view" << endl;
    }
}

void VideoTree::slotWatchVideo()
{
    cancelPopup();
    
    if (curitem)
        playVideo(curitem);
    else
        cerr << "no Item to watch" << endl;

}


void VideoTree::playVideo(Metadata *someItem)
{
    if (!someItem)
        return;
        
    QString filename = someItem->Filename();
    QString handler = getHandler(someItem);
    QString year = QString("%1").arg(someItem->Year());
    // See if this is being handled by a plugin..
    if(gContext->GetMainWindow()->HandleMedia(handler, filename, someItem->Plot(), 
                                              someItem->Title(), someItem->Director(),
                                              someItem->Length(), year))
    {
        return;
    }

    QString command = getCommand(someItem);
        
    
    QTime playing_time;
    playing_time.start();
    
    // Play the movie
    myth_system((QString("%1 ").arg(command)).local8Bit());

    // Show a please wait message
    
/*    LayerSet *container = getTheme()->GetSet("playwait");
    
    if (container)
    {
         UITextType *type = (UITextType *)container->GetType("title");
         if (type)
             type->SetText(someItem->Title());
    }
    updateForeground();
    */
    //allowPaint = false;        
    Metadata *childItem = new Metadata;
    Metadata *parentItem = new Metadata(*someItem);

    while (parentItem->ChildID() > 0 && playing_time.elapsed() > WATCHED_WATERMARK)
    {
        childItem->setID(parentItem->ChildID());
        childItem->fillDataFromID();

        if (parentItem->ChildID() > 0)
        {
            //Load up data about this child
            command = getCommand(childItem);
            playing_time.start();
            myth_system((QString("%1 ") .arg(command)).local8Bit());
        }

        delete parentItem;
        parentItem = new Metadata(*childItem);
    }

    delete childItem;
    delete parentItem;
    
    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->setActiveWindow();
    gContext->GetMainWindow()->currentWidget()->setFocus();
    
    //allowPaint = true;
    
    updateForeground();
}


QString VideoTree::getHandler(Metadata *someItem)
{
    
    if (!someItem)
        return "";
        
    QString filename = someItem->Filename();
    QString ext = someItem->Filename().section('.',-1);

    QString handler = gContext->GetSetting("VideoDefaultPlayer");
    QString special_handler = someItem->PlayCommand();
 
    //
    //  Does this specific metadata have its own
    //  unique player command?
    //
    if(special_handler.length() > 1)
    {
        handler = special_handler;
    }
    
    else
    {
        //
        //  Do we have a specialized player for this
        //  type of file?
        //
        
        QString extension = filename.section(".", -1, -1);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT playcommand, use_default FROM "
                      "videotypes WHERE extension = :EXT ;");
        query.bindValue(":EXT", extension);

        if(query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            if(!query.value(1).toBool())
            {
                //
                //  This file type is defined and
                //  it is not set to use default player
                //

                handler = query.value(0).toString();                
            }
        }
    }
    
    return handler;
}

QString VideoTree::getCommand(Metadata *someItem)
{
    if (!someItem)
        return "";
        
    QString filename = someItem->Filename();
    QString handler = getHandler(someItem);
    QString arg;
    arg.sprintf("\"%s\"",
                filename.replace(QRegExp("\""), "\\\"").utf8().data());

    QString command = "";
    
    // If handler contains %d, substitute default player command
    // This would be used to add additional switches to the default without
    // needing to retype the whole default command.  But, if the
    // command and the default command both contain %s, drop the %s from
    // the default since the new command already has it
    //
    // example: default: mplayer -fs %s
    //          custom : %d -ao alsa9:spdif %s
    //          result : mplayer -fs -ao alsa9:spdif %s
    if (handler.contains("%d"))
    {
        QString default_handler = gContext->GetSetting("VideoDefaultPlayer");
        if (handler.contains("%s") && default_handler.contains("%s"))
        {
            default_handler = default_handler.replace(QRegExp("%s"), "");
        }
        command = handler.replace(QRegExp("%d"), default_handler);
    }

    if (handler.contains("%s"))
    {
        command = handler.replace(QRegExp("%s"), arg);
    }
    else
    {
        command = handler + " " + arg;
    }
    
    return command;
}

void VideoTree::slotDoFilter()
{
    cancelPopup();
    VideoFilterDialog * vfd = new VideoFilterDialog(currentVideoFilter,
                                                    gContext->GetMainWindow(),
                                                    "filter", "video-",
                                                    "Video Filter Dialog");
    vfd->exec();
    delete vfd;
    
    video_tree_data->deleteAllChildren();
    buildVideoList();
    updateForeground();
}
