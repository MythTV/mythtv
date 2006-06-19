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
#include <mythtv/mythmedia.h>

class XMLParse;
class UIListBtnType;

class ThumbGenerator;

class ThumbItem
{
  public:
    ThumbItem() :
        name(""),     caption(""),
        path(""),     isDir(false),
        pixmap(NULL), mediaDevice(NULL) { }

    ~ThumbItem()
    {
        if (pixmap)
            delete pixmap;
    }

    int GetRotationAngle();
    void SetRotationAngle(int angle);
    bool Remove();

    QString  name;
    QString  caption;
    QString  path;
    bool     isDir;
    QPixmap *pixmap;
    MythMediaDevice *mediaDevice;
};

typedef QPtrList<ThumbItem> ThumbList;
typedef QDict<ThumbItem>    ThumbDict;

class IconView : public MythDialog
{
    Q_OBJECT
  public:
    IconView(const QString& galleryDir, MythMediaDevice *initialDevice,
             MythMainWindow* parent, const char* name = 0);
    ~IconView();

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);
    bool HandleEscape(void);

  private:
    void loadTheme(void);
    void loadDirectory(const QString& dir, bool topleft = true);

    void updateMenu(void);
    void updateText(void);
    void updateView(void);

    bool moveUp(void);
    bool moveDown(void);
    bool moveLeft(void);
    bool moveRight(void);

    void actionMainMenu(void);
    void actionSubMenuMetadata(void);
    void actionSubMenuMark(void);
    void actionSubMenuFile(void);

    void actionRotateCW(void);
    void actionRotateCCW(void);
    void actionDeleteCurrent(void);
    void actionSlideShow(void);
    void actionRandomShow(void);
    void actionSettings(void);
    void actionImport(void);
    void actionShowDevices(void);
    void actionCopyHere(void);
    void actionMoveHere(void);
    void actionDelete(void);
    void actionDeleteMarked(void);
    void actionClearMarked(void);
    void actionSelectAll(void);
    void actionMkDir(void);

    void pressMenu(void);

    void loadThumbnail(ThumbItem *item);
    void importFromDir(const QString &fromDir, const QString &toDir);
    void CopyMarkedFiles(bool move = false);

    void clearMenu(UIListBtnType *menu);
    
    QPtrList<ThumbItem> m_itemList;
    QDict<ThumbItem>    m_itemDict;
    QStringList         m_itemMarked;
    QString             m_galleryDir;

    XMLParse           *m_theme;
    QRect               m_menuRect;
    QRect               m_textRect;
    QRect               m_viewRect;

    bool                m_inMenu;
    bool                m_inSubMenu;
    UIListBtnType      *m_menuType;
    UIListBtnType      *m_submenuType;
    
    QPixmap             m_backRegPix;
    QPixmap             m_backSelPix;
    QPixmap             m_folderRegPix;
    QPixmap             m_folderSelPix;
    QPixmap             m_MrkPix;

    QString             m_currDir;
    bool                m_isGallery;
    bool                m_showDevices;
    MythMediaDevice    *m_currDevice;

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
    int                 m_showcaption;

    typedef void (IconView::*Action)(void);

  public slots:
    void mediaStatusChanged(MediaStatus oldStatus, MythMediaDevice* pMedia);
};


#endif /* ICONVIEW_H */
