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

#include "metadata.h"
#include <mythtv/mythwidgets.h>
#include <qdom.h>
#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/uilistbtntype.h>
#include "videofilter.h"

class QSqlDatabase;
typedef QValueList<Metadata>  ValueMetadata;

class VideoGallery : public MythDialog
{
    Q_OBJECT
  public:
    VideoGallery(MythMainWindow *parent, 
		QSqlDatabase *ldb, const char *name = 0);
    ~VideoGallery();
    void VideoGallery::processEvents() { qApp->processEvents(); }
    

  protected slots:
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();
    void exitWin();
    void setParentalLevel(int which_level);
    bool checkParentPassword();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    bool updateML;
    VideoFilterSettings	*currentVideoFilter;

    QPixmap getPixmap(QString &level);
    QSqlDatabase *db;

    GenericTree *video_tree_root;
    GenericTree *video_tree_data;
    GenericTree *where_we_are;

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    void LoadIconWindow();

    int currentParentalLevel;
    void BuildVideoList();

    void updateMenu(QPainter *);
    void updateText(QPainter *);
    void updateView(QPainter *);
    void updateArrows(QPainter *);
    void updateSingleIcon(QPainter *,int,int);
    void drawIcon(QPainter *,GenericTree*,int,int,int);

    void actionChangeView(UIListBtnTypeItem*);
    void actionChangeIcons(UIListBtnTypeItem*);
    void actionFilter(UIListBtnTypeItem*);

    void positionIcon();

    int curView;
    bool smallIcons;
    QString curPath;

    QRect fullRect;
    QRect menuRect;
    QRect textRect;
    QRect viewRect;
    QRect arrowsRect;

    bool inMenu;
    UIListBtnType *menuType;

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

    typedef void (VideoGallery::*Action)(UIListBtnTypeItem*);
};

#endif
