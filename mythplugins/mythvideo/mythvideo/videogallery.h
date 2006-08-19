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

#include "videodlg.h"

class UIListBtnTypeItem;

class VideoGallery : public VideoDialog
{
    Q_OBJECT

  public:
    VideoGallery(MythMainWindow *lparent, const QString &lname,
                 VideoList *video_list);

  protected slots:
    void moveCursor(const QString& action);
    void exitWin();
    void slotChangeView();
    void handleVideoSelect();

  protected:
    virtual void parseContainer(QDomElement &element);
    virtual void fetchVideos();
    void doMenu(bool info = false);
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    bool handleSelect();
    void handleDirSelect();
    void handleUpDirSelect();
    bool goBack();

  private:
    void LoadIconWindow();

    void updateText(QPainter *p);
    void updateView(QPainter *p);
    void updateArrows(QPainter *p);
    void updateSingleIcon(QPainter *p, int lx, int ly);
    void drawIcon(QPainter *p, GenericTree *curTreePos, int curPos, int xpos,
                  int ypos);

    void positionIcon();

    void computeLastRowCol(int list_count);

  private:
    bool subtitleOn;
    bool keepAspectRatio;

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

    GenericTree *video_tree_root;
    GenericTree *where_we_are;
};

#endif
