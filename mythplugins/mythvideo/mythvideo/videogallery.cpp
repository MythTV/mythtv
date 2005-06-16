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
#include <qstringlist.h>
#include <qpixmap.h>
#include <unistd.h>

#include <cmath>
using namespace std;

#include "metadata.h"
#include "videogallery.h"
#include "videoselected.h"
#include "videolist.h"
#include <mythtv/mythcontext.h>
#include <mythtv/util.h>

enum GAL_VIEWS { VIEW_DB, VIEW_FILE };

VideoGallery::VideoGallery(MythMainWindow *parent, const char *name)
            : VideoDialog(DLG_GALLERY, parent, "gallery", name)
{
    updateML = false;

    // load default settings from the database
    
    if (gContext->GetNumSetting("VideoGalleryNoDB", false) == false)
        curView = VIEW_DB;
    else
        curView = VIEW_FILE;
        
    nCols                = gContext->GetNumSetting("VideoGalleryColsPerPage",4);
    nRows                = gContext->GetNumSetting("VideoGalleryRowsPerPage",3);
    subtitleOn           = gContext->GetNumSetting("VideoGallerySubtitle",1);
    keepAspectRatio      = gContext->GetNumSetting("VideoGalleryAspectRatio",1);


    // XXX Fixme: this is wrong...
    prefix = gContext->GetSetting("VideoStartupDir");

    if (curView == VIEW_DB)
    {
        setFlatList(true);
        setFileBrowser(false);
    }
    else
    {
        setFlatList(false);
        setFileBrowser(true);
    }
    
    
    loadWindow(xmldata);
    LoadIconWindow(); // load icon settings

    fetchVideos();

    setNoErase();
}

VideoGallery::~VideoGallery()
{
    // save current settings as default
    gContext->SaveSetting("VideoDefaultView", curView);
}



void VideoGallery::keyPressEvent(QKeyEvent *e)
{
    
    bool handled = false;
    QStringList actions;
    
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];

        if (action == "SELECT")
        {
            handled = true;
            handled = handleSelect();
        }
        else if ( (action == "UP")  || (action == "DOWN") ||
                  (action == "LEFT") ||(action == "RIGHT") ||
                  (action == "PAGEUP") || (action == "PAGEDOWN"))
        {
            handled = true;
            moveCursor(action);
        }
        else if (action == "1" || action == "2" ||
                 action == "3" || action == "4")
        {
            handled = true;
            setParentalLevel(action.toInt());        // parental control
        }
        
        if (handled)
            break;
    }    
    
    
    if (!handled)
    {
        gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;
    
            if (action == "SELECT")
            {
                handled = handleSelect();
            }
            else if (action == "FILTER")
            {
                slotDoFilter();
            }
            else if (action == "INFO")
            {
                // pop-up menu with video description
                if ((where_we_are->getInt() > SUB_FOLDER ))
                {
                    doMenu(true);
                }
            }
            else if (action == "INCPARENT")
                shiftParental(1);
            else if (action == "DECPARENT")
                shiftParental(-1);            
            else if (action == "MENU")
            {
                doMenu(false);
            }
            else if (action == "ESCAPE")
            {
                 handled = goBack();
            }
            else
                handled = false;
            
            if (handled)
                break;
        }
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
                slotWatchVideo();
            }
        }            
    }
    
    if (!handled)        
        MythDialog::keyPressEvent(e);
}

bool VideoGallery::goBack()
{
    bool handled = false;
    // one dir up
    if ((curView == VIEW_FILE) && !jumping)
    {
        GenericTree *parent = where_we_are->getParent();
        if (parent)
        {
            if (parent != video_tree_root)
            {
                // move one node up in the video tree
                QString subdir = parent->getString();
                int length = curPath.length() - subdir.length();
                curPath.truncate(length);

                where_we_are = parent;

                // determine the x,y position of the current icon anew
                positionIcon();

                update(); // renew the screen
                handled = true;
            }
        }
     }
     return handled;
}

void VideoGallery::fetchVideos()
{
    if (updateML == true)
        return;
    updateML = true;
    
    VideoDialog::fetchVideos();
    video_tree_root = VideoDialog::getVideoTreeRoot();

    video_list->wantVideoListUpdirs(curView != VIEW_DB);
    video_tree_root->setOrderingIndex(0);
    video_tree_root->sortByAttributeThenByString(0);

    //
    // Select initial view
    //
    curPath="";

    topRow = currRow = currCol = 0; // icon in top-left corner
    lastRow = lastCol = 0;

    if (video_tree_root->childCount() > 0)
        where_we_are = video_tree_root->getChildAt(0,0);
    else where_we_are = video_tree_root;
    
    // Move a node down if there is a single directory item here...
    if (where_we_are->siblingCount() == 1 && where_we_are->getInt() < 0)
    {
        // Get rid of the up node, if it's there, it _should_ be the first
        // child...
        GenericTree *upnode = where_we_are->getChildAt(0,0);
        if ((upnode != NULL) && (upnode->getInt() == UP_FOLDER))
                where_we_are->removeNode(upnode);
        if (where_we_are->childCount() > 1)
                where_we_are = where_we_are->getChildAt(0,0);
        // else { we have an empty tree! }
    }
    int list_count = where_we_are->siblingCount();
    lastRow = QMAX((int)ceilf((float)list_count/(float)nCols)-1,0);
    lastCol = QMAX(list_count-lastRow*nCols-1,0);
    
    allowselect = (bool)(where_we_are->siblingCount() > 0);

    updateML = false;
    update();         // renew the screen
    
    if (where_we_are->getInt() >= 0)
        curitem = video_list->getVideoListMetadata(where_we_are->getInt());
    else curitem = NULL;
}

void VideoGallery::paintEvent(QPaintEvent *e)
{
    if (!allowPaint)
        return;

    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(textRect))
        updateText(&p);

    if (r.intersects(viewRect))
        updateView(&p);

    if (r.intersects(arrowsRect))
        updateArrows(&p);

    MythDialog::paintEvent(e);
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
            Metadata *meta;
            meta = video_list->getVideoListMetadata(where_we_are->getInt());
            ttype->SetText(meta ? meta->Title() : where_we_are->getString());
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
    bool myImage = true;
    Metadata *meta = NULL;

    if (curTreePos->getInt() < 0) // directory
    {
        if (curPos == (currRow*nCols+currCol))
            p->drawPixmap(xpos, ypos, folderSelPix); // highlighted
        else
            p->drawPixmap(xpos, ypos, folderRegPix);

        if (curTreePos->getInt() == SUB_FOLDER)  // subdirectory
        {
            // load folder icon
            QString filename = QString("%1/%2%3folder")
                                      .arg(prefix).arg(curPath)
                                      .arg(curTreePos->getString());

            image = new QImage();
            // folder.[png|jpg|gif]

            if (!image->load(filename + ".png"))
                if (!image->load(filename + ".jpg"))
                         image->load(filename + ".gif");
        }
        else if (curTreePos->getInt() == UP_FOLDER) // up-directory
        {
            image = gContext->LoadScaleImage("mv_gallery_dir_up.png");
        }

        // directory icons are 10% smaller
        yoffset = (int)(0.1*thumbH);
    }
    else // video
    {
        if (curPos == (currRow*nCols+currCol))
            p->drawPixmap(xpos, ypos, backSelPix); // highlighted
        else
            p->drawPixmap(xpos, ypos, backRegPix);

        // load video icon
        meta = video_list->getVideoListMetadata(curTreePos->getInt());

        image = meta->getCoverImage();
        myImage = false;
    }

    int bw  = backRegPix.width();
    int bh  = backRegPix.height();
    int sw  = (int)(11*wmult);
    int sh  = (int)(11*hmult);


    if (image && !image->isNull())
    {

        QPixmap *pixmap = NULL;
        if (!myImage && meta && meta->haveCoverPixmap())
            pixmap = meta->getCoverPixmap();

        if (!pixmap)
            pixmap = new QPixmap(image->smoothScale((int)(thumbW-2*sw),
                                                    (int)(thumbH-2*sh-yoffset),
                                               (keepAspectRatio ? QImage::ScaleMin : QImage::ScaleFree) ));

        if (!pixmap->isNull())
            p->drawPixmap(xpos + sw, ypos + sh + yoffset,
                          *pixmap,
                          (pixmap->width()-bw)/2+sw,
                          (pixmap->height()-bh+yoffset)/2+sh,
                          bw-2*sw, bh-2*sh-yoffset);

        if (myImage)
            delete pixmap;
        else
            meta->setCoverPixmap(pixmap);
    }


    UITextType *itype = (UITextType*)0;
    UITextType *ttype = (UITextType*)0;

    LayerSet* container = theme->GetSet("view");

    if (container)
    {
        itype = (UITextType*)container->GetType("icontext");
        ttype = (UITextType*)container->GetType("subtext");
    }
    else
    {
        cerr << "Failed to get view Container" << endl;
    }

    // text instead of an image
    if (itype && (!image || image->isNull()))
    {
        QRect area = itype->DisplayArea();

        area.setX(xpos + sw);
        area.setY(ypos + sh + yoffset);
        area.setWidth(bw-2*sw);
        area.setHeight(bh-2*sh-yoffset);
        itype->SetDisplayArea(area);
        itype->calculateScreenArea();
        itype->SetText(meta ? meta->Title() : curTreePos->getString());

        itype->Draw(p, 0, 0);
        itype->Draw(p, 1, 0);
        itype->Draw(p, 2, 0);
        itype->Draw(p, 3, 0);
    }


    // text underneath an icon
    if (ttype && subtitleOn)
    {
        QRect area = ttype->DisplayArea();

        area.setX(xpos + sw);
        area.setY(ypos + thumbH);
        area.setWidth(bw-2*sw);
        area.setHeight(spaceH);
        ttype->SetDisplayArea(area);
        ttype->calculateScreenArea();
        itype->SetText(meta ? meta->Title() : curTreePos->getString());

        ttype->Draw(p, 0, 0);
        ttype->Draw(p, 1, 0);
        ttype->Draw(p, 2, 0);
        ttype->Draw(p, 3, 0);
    }

    if (image && myImage)
        delete image;
}


void VideoGallery::doMenu(bool info)
{
    if (createPopup())
    {
        QButton *focusButton = NULL;
        if(info)
        {
            focusButton = popup->addButton(tr("Watch This Video"), this, SLOT(slotWatchVideo()));
            popup->addButton(tr("View Full Plot"), this, SLOT(slotViewPlot()));
            popup->addButton(tr("View Details"), this, SLOT(handleVideoSelect()));

        }
        else
        {
            if (!isFileBrowser)
                focusButton = popup->addButton(tr("Filter Display"), this, SLOT(slotDoFilter()));
            addDests();
        }

        popup->addButton(tr("Cancel"), this, SLOT(slotDoCancel()));

        popup->ShowPopup(this, SLOT(slotDoCancel()));

        focusButton->setFocus();
    }

}


void VideoGallery::LoadIconWindow()
{
    const float margin = 0.05;

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
    if (subtitleOn) {
      UITextType *ttype = (UITextType*)container->GetType("subtext");
      if (ttype) {
          QRect area = ttype->DisplayArea();
          spaceH = area.height();
      }
    }

    // nr of rows and columns are given by the setup menu
    thumbW = (int)floorf((float)(viewRect.width()) / ((float)nCols * (1 + margin) - margin));
    thumbH = (int)floorf((float)(viewRect.height() - nRows * spaceH) / ((float)nRows * (1 + margin)));
    spaceW = (nCols <= 1 ? 0 : (viewRect.width()  - (nCols * thumbW)) / (nCols - 1));
    spaceH = (viewRect.height() - (nRows * thumbH)) / nRows;

    //
    // download icon and folder backgrounds
    //

    struct {
        char *filename;
        QPixmap *name;
    } const backgrounds[4] = {
        { "mv_gallery_back_reg.png",   &backRegPix   },
        { "mv_gallery_back_sel.png",   &backSelPix   },
        { "mv_gallery_folder_reg.png", &folderRegPix },
        { "mv_gallery_folder_sel.png", &folderSelPix }
    };

    QImage *img;
    for (unsigned int i = 0; i < 4; i++) {
        img = gContext->LoadScaleImage(QString(backgrounds[i].filename));
        if (!img) {
            std::cerr << "Failed to load " << backgrounds[i].filename << std::endl;
            exit(-1);
        }
        *(backgrounds[i].name) = QPixmap(img->smoothScale((int)thumbW,(int)thumbH,QImage::ScaleFree));
        delete img;
    }
}


void VideoGallery::exitWin()
{
    emit accept();
}

void VideoGallery::moveCursor(QString action)
{
    int prevCol = currCol;
    int prevRow = currRow;
    int oldRow  = topRow;

    if (action == "LEFT")
    {
        if (currRow == 0 && currCol == 0)
            return;

        currCol--;
        if (currCol < 0) {
            currCol = nCols - 1;
            currRow--;
            if (currRow < topRow)
                topRow = currRow;
        }
    }
    else if (action == "RIGHT")
    {
        if (currRow*nCols+currCol >= (int)(where_we_are->siblingCount())-1)
            return;

        currCol++;
        if (currCol >= nCols) {
            currCol = 0;
            currRow++;
            if (currRow >= topRow+nRows)
                topRow++;
        }
    }
    else if (action == "UP")
    {
        if (currRow == 0) {
            currRow = lastRow;
            currCol = QMIN(currCol,lastCol);
            topRow  = QMAX(currRow - nRows + 1,0);
        } else {
            currRow--;
            if (currRow < topRow)
                topRow = currRow;
        }
    }
    else if (action == "DOWN")
    {
        if (currRow == lastRow) {
            currRow = 0;
            topRow = 0;
        } else {
            currRow++;

            if (currRow == lastRow)
                currCol = QMIN(currCol,lastCol);

            if (currRow >= topRow+nRows)
                topRow++;
        }
    }
    else if (action == "PAGEUP")
    {
        if (currRow == 0)
            return; // or wrap around: currRow = QMAX(lastRow - nRows + 1, 0);
        else
            currRow = QMAX(currRow - nRows, 0);

        topRow = currRow;
    }
    else if (action == "PAGEDOWN")
    {
        if (currRow == lastRow)
            return; // or wrap around: currRow = QMAX(nRows - 1,0);
        else
            currRow += nRows;

        if (currRow >= lastRow) {
            currRow = lastRow;
            currCol = QMIN(currCol,lastCol);
        }

        topRow = QMAX(currRow - nRows + 1,0);
    }
    else
        return;

    GenericTree *parent = where_we_are->getParent();
    if (parent)
        where_we_are = parent->getChildAt(currRow * nCols + currCol,0);


    curitem = video_list->getVideoListMetadata(where_we_are->getInt());


    if (topRow != oldRow)     // renew the whole screen
    {
        update(viewRect);
        update(textRect);
    }
    else                     // partial update only
    {
        QPainter p(this);
        updateSingleIcon(&p,prevCol,prevRow);
        updateSingleIcon(&p,currCol,currRow);
        updateText(&p);
    }
}

void VideoGallery::slotChangeView()
{

    //
    // menu option to toggle between plain and folder view
    //
    cancelPopup();
    curView = curView ? VIEW_DB : VIEW_FILE;

    if (curView == VIEW_DB)
    {
        setFlatList(true);
        setFileBrowser(false);
    }
    else
    {
        setFlatList(false);
        setFileBrowser(true);
    }
    
    fetchVideos(); // reload videos
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


void VideoGallery::handleDirSelect()
{
    // move one node down in the video tree
    int list_count = where_we_are->childCount();
    if (list_count > 0)  // should be
    {
        curPath.append(where_we_are->getString());

        topRow = currRow = currCol = 0;  // top-left corner

        where_we_are = where_we_are->getChildAt(0,0);

        lastRow = QMAX((int)ceilf((float)list_count/(float)nCols)-1,0);
        lastCol = QMAX(list_count-lastRow*nCols-1,0);
    }

    allowselect = (bool)(list_count > 0);
}

void VideoGallery::handleUpDirSelect()
{
    GenericTree *parent = where_we_are->getParent();
    if (parent)
    {
        if (parent != video_tree_root)
        {
            // move one node up in the video tree
            QString subdir = parent->getString();
            int length = curPath.length() - subdir.length();
            curPath.truncate(length);

            where_we_are = parent;

            // determine the x,y position of the current icon anew
            positionIcon();

            allowselect = (bool)(where_we_are->siblingCount() > 0);
        }
    }
}

void VideoGallery::handleVideoSelect()
{
    cancelPopup();

    VideoSelected *selected = new VideoSelected( video_list,
                                gContext->GetMainWindow(),
                                "video selected", where_we_are->getInt());
    qApp->unlock();
    selected->exec();
    qApp->lock();
    delete selected;
}

bool VideoGallery::handleSelect()
{
    bool handled = true;

    if (allowselect)
    {
        switch (where_we_are->getInt())
        {
            case SUB_FOLDER:
                handleDirSelect();
                break;
            case UP_FOLDER:
                handleUpDirSelect();
                break;
            default:
                handleVideoSelect();
        };

        update(); // renew the screen
    }

    return handled;
}


void VideoGallery::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "text")
        textRect = area;
    else if (name.lower() == "view")
        viewRect = area;
    else if (name.lower() == "arrows")
        arrowsRect = area;
}
