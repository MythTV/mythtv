#include <qapplication.h>
#include <qsqldatabase.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

#include <qdir.h>

#include "videotree.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/uitypes.h>
#include <mythtv/util.h>

VideoTree::VideoTree(QSqlDatabase *ldb,
                     MythMainWindow *parent, const char *name)
         : VideoDialog(DLG_TREE, ldb, parent, "videotree", name)
{
    file_browser = gContext->GetNumSetting("VideoTreeNoDB", 0);
    browser_mode_files.clear();
        
    //
    //  Theme and tree stuff
    //

    wireUpTheme();
    video_tree_root = new GenericTree("video root", -2, false);
    video_tree_data = video_tree_root->addNode("videos", -2, false);

    fetchVideos();
    
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
    
    handleTreeListEntry(video_tree_list->getCurrentNode()->getInt());
}

VideoTree::~VideoTree()
{
    if (curitem)
        delete curitem;

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
        else if (action == "MENU")
            doMenu(false);
        else if (action == "INFO")
            doMenu(true);           
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
        else if (action == "1" || action == "2" || action == "3" ||
                 action == "4")
        {
            setParentalLevel(action.toInt());
        }
        else
            handled = false;
    }

    if (!handled) 
        MythThemedDialog::keyPressEvent(e);
}

void VideoTree::slotParentalLevelChanged()
{
    pl_value->SetText(QString("%1").arg(currentParentalLevel));
}

bool VideoTree::ignoreExtension(QString extension)
{
    QString q_string = QString("SELECT f_ignore FROM videotypes WHERE extension = \"%1\" ;")
                              .arg(extension);

    QSqlQuery a_query(q_string, db);
    if(a_query.isActive() && a_query.numRowsAffected() > 0)
    {
        a_query.next();
        return a_query.value(0).toBool();
    }
    
    return !gContext->GetNumSetting("VideoListUnknownFileTypes", 1);

}

void VideoTree::buildFileList(QString directory)
{
    QDir d(directory);

    d.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;
    QRegExp r;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || 
            fi->fileName() == ".." ||
            fi->fileName() == "Thumbs.db")
        {
            continue;
        }
        
        if(!fi->isDir())
        {
            if(ignoreExtension(fi->extension(false)))
            {
                continue;
            }
        }
        
        QString filename = fi->absFilePath();
        if (fi->isDir())
            buildFileList(filename);
        else
        {
            browser_mode_files.append(filename);
        }
    }
}

void VideoTree::fetchVideos()
{
    video_tree_data->deleteAllChildren();

    if (file_browser)
        fillTreeFromFilesystem();
    else
        VideoDialog::fetchVideos();
    
    video_tree_list->assignTreeData(video_tree_root);
    video_tree_list->sortTreeByString();
    video_tree_list->sortTreeBySelectable();
    
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

    handleTreeListEntry(video_tree_list->getCurrentNode()->getInt());
    updateForeground();
}


void VideoTree::fillTreeFromFilesystem()
{
    //
    //  Fill metadata from directory structure
    //
    buildFileList(gContext->GetSetting("VideoStartupDir"));
    
    for (uint i=0; i < browser_mode_files.count(); i++)
    {
        QString file_string = *(browser_mode_files.at(i));
        QString prefix = gContext->GetSetting("VideoStartupDir");
        if(prefix.length() < 1)
        {
            cerr << "videotree.o: Seems unlikely that this is going to work" << endl;
        }
        file_string.remove(0, prefix.length());
        QStringList list(QStringList::split("/", file_string));
    
        GenericTree *where_to_add;
        where_to_add = video_tree_data;
        int a_counter = 0;
        QStringList::Iterator an_it = list.begin();
        for ( ; an_it != list.end(); ++an_it)
        {
            if (a_counter + 1 >= (int) list.count())
            {
                QString title = (*an_it);
                where_to_add->addNode(title.section(".",0,-2), i, true);
                
            }
            else
            {
                QString dirname = *an_it + "/";
                GenericTree *sub_node;
                sub_node = where_to_add->getChildByName(dirname);
                if(!sub_node)
                {
                    sub_node = where_to_add->addNode(dirname, -1, false);
                }
                where_to_add = sub_node;
            }
            ++a_counter;
        }
    }
    
    if (video_tree_data->childCount() < 1)
    {
        //  Nothing survived the requirements
        video_tree_data->addNode(tr("No files found"), -1, false);
    }        
}


void VideoTree::handleMetaFetch(Metadata* myData)
{
    QString prefix = gContext->GetSetting("VideoStartupDir");
    QString file_string = myData->Filename();
    
    file_string.remove(0, prefix.length());
    QStringList list(QStringList::split("/", file_string));
    
    GenericTree *where_to_add;
    where_to_add = video_tree_data;
    int a_counter = 0;
    QStringList::Iterator an_it = list.begin();
    for( ; an_it != list.end(); ++an_it)
    {
        if(a_counter + 1 >= (int) list.count())
        {
            where_to_add->addNode(myData->Title(), myData->ID(), true);                    
        }
        else
        {
            QString dirname = *an_it + "/";
            GenericTree *sub_node;
            sub_node = where_to_add->getChildByName(dirname);
            if(!sub_node)
            {
                sub_node = where_to_add->addNode(dirname, -1, false);
            }
            where_to_add = sub_node;
        }
        ++a_counter;
    }
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
            
        if(file_browser)
        {
            if(node_int >= (int) browser_mode_files.count())
            {
                cerr << "videotree.o: Uh Oh. Reference larger than count of browser_mode_files" << endl;
                
            }
            else
            {
                QString the_file = *(browser_mode_files.at(node_int));
                QString base_name = the_file.section("/", -1);
                video_title->SetText(base_name.section(".", 0, -2));
                video_file->SetText(base_name);
                extension = the_file.section(".", -1);
                player = gContext->GetSetting("VideoDefaultPlayer");
                
                if (curitem)
                    delete curitem;
                
                curitem = new Metadata(the_file);
            }
        }
        else
        {
            Metadata *node_data;
            node_data = new Metadata();
            node_data->setID(node_int);
            node_data->fillDataFromID(db);
            video_title->SetText(node_data->Title());
            video_file->SetText(node_data->Filename().section("/", -1));
            video_poster->SetImage(node_data->CoverFile());
            video_poster->LoadImage();
            extension = node_data->Filename().section(".", -1, -1);
            unique_player = node_data->PlayCommand();
            if(unique_player.length() > 0)
            {
                player = unique_player;
            }
            else
            {
                player = gContext->GetSetting("VideoDefaultPlayer");
            }
            
            if (curitem)
                delete curitem;

            curitem = node_data;
        }

        //
        //  Find the player for this file
        //
        


        QString q_string = QString("SELECT playcommand, use_default FROM "
                                   "videotypes WHERE extension = \"%1\" ;")
                                   .arg(extension);

        QSqlQuery a_query(q_string, db);
        
        if(a_query.isActive() && a_query.numRowsAffected() > 0)
        {
            a_query.next();
            if(!a_query.value(1).toBool() && unique_player.length() < 1)
            {
                //
                //  This file type is defined and
                //  it is not set to use default player
                //
                player = a_query.value(0).toString();                
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
        
        video_title->SetText("");
        video_file->SetText("");
        video_player->SetText(""); 
        
    }
    
}

void VideoTree::syncMetaToTree(int nodeNumber)
{
    if(file_browser)
    {
        QString filename;
        filename = *(browser_mode_files.at(nodeNumber));
        if (curitem)
            delete curitem;
    
        curitem = new Metadata(filename);
    }
    else
    {
        if (curitem)
            delete curitem;
            
        curitem = new Metadata();
        curitem->setID(nodeNumber);
        curitem->fillDataFromID(db);
    }
}


void VideoTree::slotWatchVideo()
{
    VideoDialog::slotWatchVideo();
    video_tree_list->deactivate();
}


void VideoTree::handleTreeListSelection(int node_int, IntVector *)
{
    if(node_int > -1)
    {
        //
        //  Play this (chain of?) file(s) 
        //
        syncMetaToTree(node_int);
        slotWatchVideo();
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
    connect(video_tree_list, SIGNAL(nodeSelected(int, IntVector*)), this, 
            SLOT(handleTreeListSelection(int, IntVector*)));
    
    connect(video_tree_list, SIGNAL(nodeEntered(int, IntVector*)), this, 
            SLOT(handleTreeListEntry(int, IntVector*)));

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
        pl_value->SetText(QString("%1").arg(currentParentalLevel));
    }
    
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
            if (!file_browser)
                focusButton = popup->addButton(tr("Filter Display"), this, SLOT(slotDoFilter()));
            
            QButton *tempButton = addDests();
            focusButton = focusButton ? focusButton : tempButton;
        }
        
        popup->addButton(tr("Cancel"), this, SLOT(slotDoCancel()));
        
        popup->ShowPopup(this, SLOT(slotDoCancel()));
        if (focusButton)
            focusButton->setFocus();
    }
    
}

