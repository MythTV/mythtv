/* ============================================================
 * File  : iconview.h
 * Description : 
 * 

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * ============================================================ */


#ifndef ICONVIEW_H
#define ICONVIEW_H

#include <qptrlist.h>
#include <qdict.h>
#include <qstring.h>
#include <qpixmap.h>

#include <mythtv/mythdialogs.h>

class XMLParse;
class UIListBtnType;

class ThumbGenerator;

class ThumbItem
{
public:

    ThumbItem() {
        pixmap = 0;
        name   = "";
        path   = "";
        isDir  = false;
    }

    ~ThumbItem() {
        if (pixmap)
            delete pixmap;
    }

    int GetRotationAngle();
    void SetRotationAngle(int angle);

    QPixmap *pixmap;
    QString  name;
    QString  path;
    bool     isDir;
};

typedef QPtrList<ThumbItem> ThumbList;
typedef QDict<ThumbItem>    ThumbDict;

class IconView : public MythDialog
{
     Q_OBJECT

public:

    IconView(const QString& galleryDir, MythMainWindow* parent, 
             const char* name = 0);
    ~IconView();

protected:

    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);
    
private:

    void loadTheme();
    void loadDirectory(const QString& dir);

    void updateMenu();
    void updateText();
    void updateView();

    bool moveUp();
    bool moveDown();
    bool moveLeft();
    bool moveRight();

    void actionRotateCW();
    void actionRotateCCW();
    void actionSlideShow();
    void actionRandomShow();
    void actionSettings();
    void actionImport();

    void pressMenu();

    void loadThumbnail(ThumbItem *item);
    void importFromDir(const QString &fromDir, const QString &toDir);
    
    QPtrList<ThumbItem> m_itemList;
    QDict<ThumbItem>    m_itemDict;
    QString             m_galleryDir;

    XMLParse           *m_theme;
    QRect               m_menuRect;
    QRect               m_textRect;
    QRect               m_viewRect;

    bool                m_inMenu;
    UIListBtnType      *m_menuType;
    
    QPixmap             m_backRegPix;
    QPixmap             m_backSelPix;
    QPixmap             m_folderRegPix;
    QPixmap             m_folderSelPix;

    QString             m_currDir;
    bool                m_isGallery;

    int                 m_currRow;
    int                 m_currCol;
    int                 m_lastRow;
    int                 m_lastCol;
    int                 m_topRow;
    int                 m_nRows;
    int                 m_nCols;
    
    int                 m_spaceW;
    int                 m_spaceH;
    int                 m_thumbW;
    int                 m_thumbH;

    ThumbGenerator     *m_thumbGen;

    typedef void (IconView::*Action)();
};


#endif /* ICONVIEW_H */
