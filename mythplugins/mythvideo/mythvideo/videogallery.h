/*
Copyright (c) 2004 Koninklijke Philips Electronics NV. All rights reserved.
Based on "iconview.h" of MythGallery.

This is free software; you can redistribute it and/or modify it under the
terms of version 2 of the GNU General Public License as published by the
Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef VIDEOGALLERY_H_
#define VIDEOGALLERY_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qapplication.h>
#include <qstringlist.h>

#include <mythtv/mythwidgets.h>
#include <qdom.h>
#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/uilistbtntype.h>

#include "videodlg.h"


typedef QValueList<Metadata>  ValueMetadata;

class VideoGallery : public VideoDialog
{
    Q_OBJECT
  public:
    VideoGallery(MythMainWindow *parent, const char *name = 0);
    ~VideoGallery();
    
    void VideoGallery::processEvents() { qApp->processEvents(); }
    
    
  protected slots:
    void moveCursor(QString action);
    void exitWin();
    void slotChangeView();
    void handleVideoSelect();
    
  protected:
    virtual void parseContainer(QDomElement &element);
    virtual void handleMetaFetch(Metadata*);
    virtual void fetchVideos();
    void doMenu(bool info=false);
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    bool handleSelect();
    void handleDirSelect();
    void handleUpDirSelect();
    bool goBack();

  private:

    QPixmap getPixmap(QString &level);
    void LoadIconWindow();

    
    void updateText(QPainter *);
    void updateView(QPainter *);
    void updateArrows(QPainter *);
    void updateSingleIcon(QPainter *,int,int);
    void drawIcon(QPainter *,GenericTree*,int,int,int);

    void actionChangeView(UIListBtnTypeItem*);
    void actionFilter(UIListBtnTypeItem*);

    void positionIcon();
    
    //typedef QValueVector<Metadata> MetaVector;
    //MetaVector metas;
    QMap<int, Metadata> metas;
    
    int curView;
    bool subtitleOn;
    bool keepAspectRatio;
    QString curPath;

    
    QRect textRect;
    QRect viewRect;
    QRect arrowsRect;

    QPixmap backRegPix;
    QPixmap backSelPix;
    QPixmap folderRegPix;
    QPixmap folderSelPix;

    int currRow;
    int currCol;
    int lastRow;
    int lastCol;
    int topRow;
    int nRows;
    int nCols;

    int spaceW;
    int spaceH;
    int thumbW;
    int thumbH;

    bool allowselect;
    bool updateML;
    
    QString prefix;
    
    GenericTree *video_tree_root;
    GenericTree *video_tree_data;
    GenericTree *where_we_are;

    //typedef void (VideoGallery::*Action)(UIListBtnTypeItem*);
};

#endif
