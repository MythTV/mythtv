/*
Copyright (c) 2004 Koninklijke Philips Electronics NV. All rights reserved. 
Based on "iconview.cpp" of MythGallery.

This is free software; you can redistribute it and/or modify it under the 
terms of version 2 of the GNU General Public License as published by the 
Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT 
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS 
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include <qlayout.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <unistd.h>

using namespace std;

#include "metadata.h"
#include "videogallery.h"
#include "videoselected.h"
#include <mythtv/mythcontext.h>
#include <mythtv/util.h>

VideoGallery::VideoGallery(MythMainWindow *parent, QSqlDatabase *ldb, const char *name)
            : MythDialog(parent, name)
{
    db = ldb;
    updateML = false;
    currentVideoFilter = new VideoFilterSettings(db);

    fullRect = QRect(0, 0, (int)(800*wmult), (int)(600*hmult));

    inMenu = false;

    // load default settings from the database 
    currentParentalLevel = gContext->GetNumSetting("VideoDefaultParentalLevel", 4);
    curView              = gContext->GetNumSetting("VideoDefaultView",0);
    smallIcons           = gContext->GetNumSetting("VideoDefaultSmallIcons",0);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "gallery", "video-");
    LoadWindow(xmldata);

    QString videodir = gContext->GetSetting("VideoStartupDir");
    QStringList an_it(QStringList::split("/", videodir));

    if (!an_it.empty())
        video_tree_root = new GenericTree(an_it.last() + "/", -3, false);
    else
        video_tree_root = new GenericTree(QString("/"), -3, false);

    video_tree_data = video_tree_root;

    BuildVideoList();

    setNoErase();
}

VideoGallery::~VideoGallery()
{
    // save current settings as default
    gContext->SaveSetting("VideoDefaultView", curView);
    gContext->SaveSetting("VideoDefaultSmallIcons", smallIcons);

    // delete menu items
    UIListBtnTypeItem* item = menuType->GetItemFirst();
    while (item) {
        Action *act = (Action *)item->getData();
        if (act)
            delete act;
        item = menuType->GetItemNext(item);
    }

    if (currentVideoFilter)
	delete currentVideoFilter;
    delete theme;
    delete video_tree_root;
}

bool VideoGallery::checkParentPassword()
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    QString password = gContext->GetSetting("VideoAdminPassword");
    if(password.length() < 1)
    {
        return true;
    }

    //
    //  See if we recently (and succesfully) asked for a password
    //
    
    if(last_time_stamp.length() < 1)
    {
        //
        //  Probably first time used
        //

        cerr << "videogallery.o: Could not read password/pin time stamp. "
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

void VideoGallery::setParentalLevel(int which_level)
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
        currentParentalLevel = which_level;

        BuildVideoList();
    
        update(); // renew screen
    }
}

void VideoGallery::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
	    if (inMenu) {     // selection made inside the menu
                UIListBtnTypeItem* item = menuType->GetItemCurrent();

                if (!item || !item->getData()) {
                    handled = false;
                } else {      // execute the individual menu items
                    Action *act = (Action*) item->getData();
                    (this->*(*act))(item);
                }

                // and deactive the menu
                inMenu = 0;
                menuType->SetActive(inMenu);
            } else {          // one of the icons is selected
                if (allowselect) {
                    if (where_we_are->getInt() == -1) {      // subdirectory

                        //
                        // move one node down in the video tree
                        //
                        int list_count = where_we_are->childCount();
                        if (list_count > 0) { // should be
                            curPath.append(where_we_are->getString());

  	        	    topRow = currRow = currCol = 0;  // top-left corner

                            where_we_are = where_we_are->getChildAt(0,0);

                            lastRow = QMAX((int)ceilf((float)list_count/(float)nCols)-1,0);
                            lastCol = QMAX(list_count-lastRow*nCols-1,0);
                        }

                        allowselect = (bool)(list_count > 0);
                    } else
                    if (where_we_are->getInt() == -2) {      // up-directory
                        GenericTree *parent = where_we_are->getParent();
                        if (parent) {
                            if (parent != video_tree_root) {

                                //
                                // move one node up in the video tree
                                //
                                QString subdir = parent->getString();
                                int length = curPath.length() - subdir.length();
                                curPath.truncate(length);

                                where_we_are = parent;

                                // determine the x,y position of the current icon anew
                                positionIcon();

                                allowselect = (bool)(where_we_are->siblingCount() > 0);
                            }
                        }
                    } else {                                 // video
                        VideoSelected *selected = 
                            new VideoSelected(QSqlDatabase::database(),
                                              gContext->GetMainWindow(),
                                              "video selected",
                                              where_we_are->getInt());
                        selected->exec();
                        delete selected;
                    }
                }
                update(); // renew the screen
            }
        }            
        else if (action == "UP") {
            if (inMenu) {
                menuType->MoveUp();
 		update(menuRect);
            }
            else
                cursorUp();
        }
        else if (action == "DOWN") {
            if (inMenu) {
                menuType->MoveDown();
                update(menuRect);
            }
            else
                cursorDown();
        }
        else if (action == "LEFT") {
            if (!inMenu)
                cursorLeft();
        }
        else if (action == "RIGHT") {
            if (!inMenu)
                cursorRight();
        }
        else if (action == "1" || action == "2" || action == "3" ||
                 action == "4")
        {
            setParentalLevel(action.toInt());        // parental control
        }
	else if (action == "FILTER")
        {
	        actionFilter((UIListBtnTypeItem*)0); // filter video listing
	}
        else if (action == "INFO") {                 // pop-up menu with video description
            if ((where_we_are->getInt() != -1) && !inMenu) {
                Metadata *curitem = new Metadata();
                curitem->setID(where_we_are->getInt());
                curitem->fillDataFromID(db);

		if (curitem){
			MythPopupBox * plotbox
			   = new MythPopupBox(gContext->GetMainWindow());
			QLabel *plotLabel = plotbox->addLabel(curitem->Plot(),MythPopupBox::Small,true);
			plotLabel->setAlignment(Qt::AlignJustify | Qt::WordBreak);
			QButton * okButton = plotbox->addButton(tr("Ok"));
			okButton->setFocus();
			plotbox->ExecPopup();
//			plotbox->showOkPopup(gContext->GetMainWindow(),
//				"test",
//				curitem->Plot());
			delete plotbox;

                        update(); // renew the screen
		}
                delete curitem;
            }
	}
        else if (action == "MENU"){     // active/de-active menu
                inMenu = !inMenu;
                menuType->SetActive(inMenu);

                QPainter p(this);
                updateSingleIcon(&p,currCol,currRow);
		updateMenu(&p);		
        }
        else if (action == "ESCAPE") {
                // one dir up
                if ((curView == 1) && !inMenu) {
                    GenericTree *parent = where_we_are->getParent();
                    if (parent) {
			if (parent != video_tree_root) {

                            //
                            // move one node up in the video tree
                            //
			    QString subdir = parent->getString();
			    int length = curPath.length() - subdir.length();
			    curPath.truncate(length);

			    where_we_are = parent;

                            // determine the x,y position of the current icon anew
                            positionIcon();
  
			    update(); // renew the screen
                        } else
                            handled = false;
                    } else
                        handled = false;
                } else 
                    handled = false;
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void VideoGallery::BuildVideoList()
{
    if (updateML == true)
        return;
    updateML = true;
    video_tree_data->deleteAllChildren();

    // retrieve video listing from database
    QString thequery = QString("SELECT intid FROM %1 %2 %3")
		.arg(currentVideoFilter->BuildClauseFrom())
		.arg(currentVideoFilter->BuildClauseWhere())
		.arg(currentVideoFilter->BuildClauseOrderBy());
    QSqlQuery query(thequery,db);
    Metadata *myData;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        QString prefix = gContext->GetSetting("VideoStartupDir");
        if(prefix.length() < 1)
        {
            cerr << "videogallery.o: Seems unlikely that this is going to work" << endl;
        }

        while (query.next())
        {
            unsigned int idnum = query.value(0).toUInt();

            //
            //  Create metadata object and fill it
            //

            myData = new Metadata();
            myData->setID(idnum);
            myData->fillDataFromID(db);

            if (myData->ShowLevel() <= currentParentalLevel && myData->ShowLevel() != 0)
            {
		if (curView == 0) {     // create a flat linear list

                    video_tree_data->addNode(myData->Title(), idnum, true);

                } else {                // create hierarchical tree based on the dir structure

                    QString file_string = myData->Filename();

                    file_string.remove(0, prefix.length());
                    QStringList list(QStringList::split("/", file_string));

		    // 
                    // Analyze the complete path listing inside the filename.
                    // Each directory is a node and each video a leaf node.
                    //
                    GenericTree *where_to_add = video_tree_data;

                    int a_counter = 0;
                    QStringList::Iterator an_it = list.begin();
                    for( ; an_it != list.end(); ++an_it)
                    {
                        // Attributes are used to set the ordering
                        // The ordering is:
                        //   0 up-folder
                        //   1 subfolders
                        //   2 videos

                        GenericTree *sub_node;

                        if (a_counter + 1 >= (int) list.count())     // video
                        {
                            sub_node = where_to_add->addNode(myData->Title(), idnum, true);
                            sub_node->setAttribute(0, 2);
                            sub_node->setOrderingIndex(0);
                        }
                        else
                        {
                            QString dirname = *an_it + "/";
                            sub_node = where_to_add->getChildByName(dirname);
                            if (!sub_node)                           // create subfolder
                            {
                                // subfolder
                                sub_node = where_to_add->addNode(dirname, -1, true);
                                sub_node->setAttribute(0, 1);
                                sub_node->setOrderingIndex(0);

                                // up-folder
				GenericTree *up_node = sub_node->addNode(where_to_add->getString(), -2, true);
                                up_node->setAttribute(0, 0);
                                up_node->setOrderingIndex(0);
                            }
                            where_to_add = sub_node;
                        }
                        ++a_counter;
                    }
                }
            }

            delete myData;
	}
    }

    video_tree_data->setOrderingIndex(0);
    video_tree_data->sortByAttributeThenByString(0);
	
    //
    // Select initial view
    //

    curPath="";

    topRow = currRow = currCol = 0; // icon in top-left corner
    lastRow = lastCol = 0;

    where_we_are = video_tree_data; // root directory
    int list_count = where_we_are->childCount();
    if (list_count > 0)             // move already one node down if there are videos
    {
        where_we_are = video_tree_data->getChildAt(0,0);
        lastRow = QMAX((int)ceilf((float)list_count/(float)nCols)-1,0);
        lastCol = QMAX(list_count-lastRow*nCols-1,0);
    }
    
    allowselect = (bool)(where_we_are != video_tree_root);

    updateML = false;
}

void VideoGallery::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(menuRect))
        updateMenu(&p);

    if (r.intersects(textRect))
        updateText(&p);

    if (r.intersects(viewRect))
        updateView(&p);

    if (r.intersects(arrowsRect))
        updateArrows(&p);
}

void VideoGallery::updateMenu(QPainter *p)
{
    QRect pr = menuRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet* container = theme->GetSet("menu");
    if (container) {

        // In the folder view, the subdirectory name is listed in the menu
        UITextType *ttype = (UITextType*)container->GetType("subdir");
        if (ttype) {
	    QString subdir = curPath.section('/',-2,-2);
            ttype->SetText(subdir);
        }

        // Print the parental control value
        UITextType *pl_value = (UITextType *)container->GetType("pl_value");
        if (pl_value)
        {
            pl_value->SetText(QString("%1").arg(currentParentalLevel));
        }

        container->Draw(&tmp, 0, 0);
    }
    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);

    //bitBlt(this, menuRect.left(), menuRect.top(), &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void VideoGallery::updateText(QPainter *p)
{
    //
    // Print the video title 
    //

    QRect pr = textRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet* container = theme->GetSet("text");
    if (container) {
        UITextType *ttype = (UITextType*)container->GetType("text");
        if (ttype) {
            ttype->SetText(where_we_are->getString());
        }

        container->Draw(&tmp, 0, 0);
    }
    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
}

void VideoGallery::updateArrows(QPainter *p)
{
    //
    // up/down arrows
    //

    QRect pr = arrowsRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet* container = theme->GetSet("arrows");
    if (container) {

        int upArrow = (topRow == 0) ? 0 : 1;
        int dnArrow = (topRow + nRows >= (lastRow+1)) ? 0 : 1;

        container->Draw(&tmp, 0, upArrow);
        container->Draw(&tmp, 1, dnArrow);
    }
    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
}

void VideoGallery::updateView(QPainter *p)
{
    //
    // Draw all video icons
    //

    GenericTree *parent = where_we_are->getParent();
    if (!parent)
        return;

    // redraw the complete view rectangle
    QRect pr = viewRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    tmp.setPen(Qt::white);

    int list_count = parent->childCount();

    int curPos = topRow*nCols;

    for (int y = 0; y < nRows; y++)    {

        int ypos = y * (spaceH + thumbH);

        for (int x = 0; x < nCols; x++)
        {
            if (curPos >= list_count)
                continue;

            int xpos = x * (spaceW + thumbW);

            GenericTree* curTreePos = parent->getChildAt(curPos,0);

            drawIcon(&tmp, curTreePos, curPos, xpos, ypos);

            curPos++;
        }
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix); // redraw area
}

void VideoGallery::updateSingleIcon(QPainter *p, int x, int y)
{
    //
    // Draw a single video icon
    //

    if ( (y < topRow) || (y >= topRow + nRows) || (x < 0) || (x >= nCols) )
        return;     // x,y invalid or not show on the screen

    GenericTree *parent = where_we_are->getParent();
    if (!parent)
        return;

    int curPos = x + y*nCols;

    GenericTree* curTreePos = parent->getChildAt(curPos,0);
    if (!curTreePos)
        return;

    int ypos = (y - topRow) * (spaceH + thumbH);
    int xpos = x * (spaceW + thumbW);

    // only redraw part of the view rectangle
    QRect pr(viewRect.left() + xpos, viewRect.top() + ypos, thumbW, thumbH + spaceH);
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    tmp.setPen(Qt::white);

    drawIcon(&tmp, curTreePos, curPos, 0, 0);

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix); // redraw area
}

void VideoGallery::drawIcon(QPainter *p, GenericTree* curTreePos, int curPos, int xpos, int ypos)
{
    QImage *image = 0;
    int yoffset = 0;

    if (curTreePos->getInt() < 0) {     // directory
        if (curPos == (currRow*nCols+currCol) && !inMenu)
            p->drawPixmap(xpos, ypos, folderSelPix); // highlighted
        else
            p->drawPixmap(xpos, ypos, folderRegPix);

	if (curTreePos->getInt() == -1) { // subdirectory
        
            // load folder icon
            QString prefix = gContext->GetSetting("VideoStartupDir");
            prefix.append('/');

            QString filename = curTreePos->getString();
            filename.prepend(prefix + curPath);
            filename.append("folder");

            image = new QImage();
            // folder.[png|jpg|gif]
	    if (!image->load(filename + ".png"))
                if (!image->load(filename + ".jpg"))
                         image->load(filename + ".gif");
        } else    
	if (curTreePos->getInt() == -2) { // up-directory
            image = gContext->LoadScaleImage("mv_gallery_dir_up.png");
        }

        // directory icons are 10% smaller
        yoffset = (int)(0.1*thumbH);
    } else {                              // video
        if (curPos == (currRow*nCols+currCol) && !inMenu)
            p->drawPixmap(xpos, ypos, backSelPix); // highlighted
        else
            p->drawPixmap(xpos, ypos, backRegPix);

        // load video icon
        Metadata *curitem = new Metadata();
        curitem->setID(curTreePos->getInt());
        curitem->fillDataFromID(db);

        image = new QImage();
        if (!image->load(curitem->CoverFile()))
            VERBOSE(VB_ALL, QString("Unable to load file %1.").arg(curitem->CoverFile()));

        delete curitem;
    }

    int bw  = backRegPix.width();
    int bh  = backRegPix.height();
    int sw  = (int)(7*wmult);
    int sh  = (int)(7*hmult);

    if (!image->isNull()) {
        QPixmap *pixmap = new QPixmap(image->smoothScale((int)(thumbW-2*sw),
                                                         (int)(thumbH-2*sh-yoffset),
                                                         QImage::ScaleMin));

        if (!pixmap->isNull())
            p->drawPixmap(xpos + sw, ypos + sh + yoffset,
                          *pixmap,
                          (pixmap->width()-bw)/2+sw,
                          (pixmap->height()-bh+yoffset)/2+sh,
                          bw-2*sw, bh-2*sh-yoffset);

       delete pixmap;
    }

    UITextType *itype = (UITextType*)0;
    UITextType *ttype = (UITextType*)0;

    LayerSet* container = theme->GetSet("view");
    if (container) {
        itype = (UITextType*)container->GetType("icontext");
        ttype = (UITextType*)container->GetType("subtext");
    }

    // text instead of an image
    if (itype && image->isNull()) {
        QRect area = itype->DisplayArea();

        area.setX(xpos + sw);
        area.setY(ypos + sh + yoffset);
        area.setWidth(bw-2*sw);
        area.setHeight(bh-2*sh-yoffset);
        itype->SetDisplayArea(area);

        itype->SetText(curTreePos->getString());

        itype->Draw(p, 0, 0);
        itype->Draw(p, 1, 0);
        itype->Draw(p, 2, 0);
        itype->Draw(p, 3, 0);
    }

    // text underneath an icon
    if (ttype && !smallIcons) {
        QRect area = ttype->DisplayArea();

        area.setX(xpos + sw);
        area.setY(ypos + thumbH);
        area.setWidth(bw-2*sw);
        area.setHeight(spaceH);
        ttype->SetDisplayArea(area);

        ttype->SetText(curTreePos->getString());

        ttype->Draw(p, 0, 0);
        ttype->Draw(p, 1, 0);
        ttype->Draw(p, 2, 0);
        ttype->Draw(p, 3, 0);
    }

    if (image) 
        delete image;
}

void VideoGallery::LoadWindow(QDomElement &element)
{
    //
    // parse ui definition in xml file
    //

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }

    LayerSet *container = theme->GetSet("menu");
    if (!container) {
        std::cerr << "MythVideo: Failed to get menu container."
                  << std::endl;
        exit(-1);
    }

    menuType = (UIListBtnType*)container->GetType("menu");
    if (!menuType) {
        std::cerr << "MythVideo: Failed to get menu area."
                  << std::endl;
        exit(-1);
    }

    // Initialize menu actions

    UIListBtnTypeItem* item;

    if (curView)
        item = new UIListBtnTypeItem(menuType, tr("Plain view"));
    else
        item = new UIListBtnTypeItem(menuType, tr("Folder view"));
    item->setData(new Action(&VideoGallery::actionChangeView));

    if (smallIcons)
        item = new UIListBtnTypeItem(menuType, tr("Large icons"));
    else
        item = new UIListBtnTypeItem(menuType, tr("Small icons"));
    item->setData(new Action(&VideoGallery::actionChangeIcons));

    item = new UIListBtnTypeItem(menuType, tr("Filters video list"));
    item->setData(new Action(&VideoGallery::actionFilter));

    menuType->SetActive(false);

    LoadIconWindow(); // load icon settings
}

void VideoGallery::LoadIconWindow()
{
    //
    // parse ui definition in xml file and load the icon settings
    //

    LayerSet *container = theme->GetSet("view");
    if (!container) {
        std::cerr << "MythVideo: Failed to get view container."
                  << std::endl;
        exit(-1);
    }

    UIBlackHoleType* bhType = (UIBlackHoleType*)container->GetType("view");
    if (!bhType) {
        std::cerr << "MythVideo: Failed to get view area."
                  << std::endl;
        exit(-1);
    }
 
    //
    // The minimum height of the subtitles is determined by the xml ui file.
    // The location and width depend on the screen layout and are calculated.
    //

    spaceH = 0;
    if (!smallIcons) {
      UITextType *ttype = (UITextType*)container->GetType("subtext");
      if (ttype) {
          QRect area = ttype->DisplayArea();
          spaceH = area.height();
      }
    }
    
    QString size = (smallIcons ? "small" : "big");

    // 
    // download icon and folder backgrounds
    //

    QImage *img = gContext->LoadScaleImage(QString("mv_gallery_back_reg_%1.png").arg(size));
    if (!img) {
        std::cerr << "Failed to load mv_gallery_back_reg_" << size << ".png"
                  << std::endl;
        exit(-1);
    }
    backRegPix = QPixmap(*img);
    delete img;
    img = gContext->LoadScaleImage(QString("mv_gallery_back_sel_%1.png").arg(size));
    if (!img) {
        std::cerr << "Failed to load mv_gallery_back_sel_" << size << ".png"
                  << std::endl;
        exit(-1);
    }
    backSelPix = QPixmap(*img);
    delete img;
    img = gContext->LoadScaleImage(QString("mv_gallery_folder_reg_%1.png").arg(size));
    if (!img) {
        std::cerr << "Failed to load mv_gallery_folder_reg_" << size << ".png"
                  << std::endl;
        exit(-1);
    }
    folderRegPix = QPixmap(*img);
    delete img;
    img = gContext->LoadScaleImage(QString("mv_gallery_folder_sel_%1.png").arg(size));
    if (!img) {
        std::cerr << "Failed to load mv_gallery_folder_sel_" << size << ".png"
                  << std::endl;
        exit(-1);
    }
    folderSelPix = QPixmap(*img);
    delete img;

    // calculate nr of rows and columns and the spacing in between
    thumbW = backRegPix.width();
    thumbH = backRegPix.height();
    nCols  = (int)floorf((float)viewRect.width()/(float)thumbW);
    nRows  = (int)floorf((float)viewRect.height()/(float)(thumbH + spaceH));
    spaceW = (nCols <= 1 ? 0 : (viewRect.width()  - (nCols * thumbW)) / (nCols - 1));
    spaceH = (viewRect.height() - (nRows * thumbH)) / nRows;
}

void VideoGallery::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "menu")
	menuRect = area;
    else if (name.lower() == "text")
        textRect = area;
    else if (name.lower() == "view")
        viewRect = area;
    else if (name.lower() == "arrows")
        arrowsRect = area;
}
 
void VideoGallery::exitWin()
{
    emit accept();
}

void VideoGallery::cursorLeft()
{
    if (currRow == 0 && currCol == 0)
        return;

    int prevCol = currCol;
    int prevRow = currRow;
    bool updateScreen = false;

    currCol--;
    if (currCol < 0) {
        currCol = nCols - 1;
        currRow--;
        if (currRow < topRow) {
            topRow = currRow;
            updateScreen = true;
        }
    }

    GenericTree *parent = where_we_are->getParent();
    if (parent)
        where_we_are = parent->getChildAt(currRow * nCols + currCol,0);

    if (updateScreen)     // renew the whole screen
        update();
    else {                // partial update only
        QPainter p(this);
        updateSingleIcon(&p,prevCol,prevRow);
        updateSingleIcon(&p,currCol,currRow);
        updateText(&p);
    }
}

void VideoGallery::cursorRight()
{
    if (currRow*nCols+currCol >= (int)(where_we_are->siblingCount())-1)
        return;

    int prevCol = currCol;
    int prevRow = currRow;
    bool updateScreen = false;

    currCol++;
    if (currCol >= nCols) {
        currCol = 0;
        currRow++;
        if (currRow >= topRow+nRows) {
            topRow++;
            updateScreen = true;
        }
    }

    GenericTree *parent = where_we_are->getParent();
    if (parent) 
	where_we_are = parent->getChildAt(currRow * nCols + currCol,0);

    if (updateScreen)     // renew the whole screen
        update();
    else {                // partial update only
        QPainter p(this);
        updateSingleIcon(&p,prevCol,prevRow);
        updateSingleIcon(&p,currCol,currRow);
        updateText(&p);
    }
}

void VideoGallery::cursorUp()
{
    int prevCol = currCol;
    int prevRow = currRow;
    bool updateScreen = false;

    if (currRow == 0) {
        currRow = lastRow;
        currCol = QMIN(currCol,lastCol);
        topRow  = QMAX(currRow - nRows + 1,0);
        updateScreen = true;
    } else {
        currRow--;
        if (currRow < topRow) {
            topRow = currRow;
            updateScreen = true;
        }
    }

    GenericTree *parent = where_we_are->getParent();
    if (parent) 
	where_we_are = parent->getChildAt(currRow * nCols + currCol,0);

    if (updateScreen)     // renew the whole screen
        update();
    else {                // partial update only
        QPainter p(this);
        updateSingleIcon(&p,prevCol,prevRow);
        updateSingleIcon(&p,currCol,currRow);
        updateText(&p);
    }
}

void VideoGallery::cursorDown()
{
    int prevCol = currCol;
    int prevRow = currRow;
    bool updateScreen = false;

    if (currRow == lastRow) {
        currRow = 0;
        topRow = 0;
        updateScreen = true;
    } else {
        currRow++;

        if (currRow == lastRow)
            currCol = QMIN(currCol,lastCol);

        if (currRow >= topRow+nRows) {
            topRow++;
            updateScreen = true;
        }             
    }

    GenericTree *parent = where_we_are->getParent();
    if (parent) 
	where_we_are = parent->getChildAt(currRow * nCols + currCol,0);

    if (updateScreen)     // renew the whole screen
        update();
    else {                // partial update only
        QPainter p(this);
        updateSingleIcon(&p,prevCol,prevRow);
        updateSingleIcon(&p,currCol,currRow);
        updateText(&p);
    }
}

void VideoGallery::actionChangeView(UIListBtnTypeItem* item)
{
    // 
    // menu option to toggle between plain and folder view
    //

    curView = 1-curView; 

    if (curView)
        item->setText(tr("Plain view"))
;    else
        item->setText(tr("Folder view"));

    BuildVideoList(); // reload videos
    
    update();         // renew the screen
}

void VideoGallery::actionChangeIcons(UIListBtnTypeItem* item)
{
    //
    // menu option to toggle between plain and folder view
    //

    smallIcons = !smallIcons;

    // set menu text
    if (smallIcons)
        item->setText(tr("Large icons"));
    else
        item->setText(tr("Small icons"));

    LoadIconWindow(); // reload icon settings

    // nCols, nRows changed, so determine the x,y position of the current icon anew
    positionIcon();

    update(); // renew the screen
}

void VideoGallery::actionFilter(UIListBtnTypeItem*)
{
    //
    // menu option to filter the video list
    //

    VideoFilterDialog * vfd = new VideoFilterDialog(db,
        currentVideoFilter,
        gContext->GetMainWindow(),
        "filter",
        "video-",
        "Video Filter Dialog");
    vfd->exec();
    delete vfd;

    BuildVideoList(); // reload videos
    
    update();         // renew the screen
}

void VideoGallery::positionIcon()
{
    // determine the x,y position of the current icon anew
    int inData = where_we_are->getPosition(0);
    currRow = (int)floorf((float)inData/(float)nCols);
    currCol = (int)(inData-currRow*nCols);

    // determine which part of the list is shown
    int list_count = where_we_are->siblingCount();
    lastRow = QMAX((int)ceilf((float)list_count/(float)nCols)-1,0);
    lastCol = QMAX(list_count-lastRow*nCols-1,0);
    topRow  = QMIN(currRow, QMAX(lastRow - nRows + 1, 0));
}
