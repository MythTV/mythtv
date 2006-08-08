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

// Qt headers
#include <qstringlist.h>

// MythTV plugin headers
#include <mythtv/mythdialogs.h>
#include <mythtv/mythmedia.h>

// MythGallery headers
#include "thumbview.h"

class XMLParse;
class UIListBtnType;
class ThumbGenerator;
class MediaMonitor;

class IconView : public MythDialog
{
    Q_OBJECT

  public:
    IconView(const QString   &galleryDir,
             MythMediaDevice *initialDevice,
             MythMainWindow  *parent);
    ~IconView();

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

  private:
    void SetupMediaMonitor(void);
    void LoadTheme(void);
    void LoadDirectory(const QString &dir, bool topleft);

    void UpdateMenu(void);
    void UpdateText(void);
    void UpdateView(void);

    bool MoveUp(void);
    bool MoveDown(void);
    bool MoveLeft(void);
    bool MoveRight(void);

    bool HandleEscape(void);
    bool HandleMediaEscape(MediaMonitor*);
    bool HandleSubDirEscape(const QString &parent);

    bool HandleItemSelect(const QString &action);
    bool HandleMediaDeviceSelect(ThumbItem *item);
    bool HandleImageSelect(const QString &action);

    void HandleMainMenu(void);
    void HandleSubMenuMetadata(void);
    void HandleSubMenuMark(void);
    void HandleSubMenuFile(void);

    void HandleRotateCW(void);
    void HandleRotateCCW(void);
    void HandleDeleteCurrent(void);
    void HandleSlideShow(void);
    void HandleRandomShow(void);
    void HandleSettings(void);
    void HandleImport(void);
    void HandleShowDevices(void);
    void HandleCopyHere(void);
    void HandleMoveHere(void);
    void HandleDelete(void);
    void HandleDeleteMarked(void);
    void HandleClearMarked(void);
    void HandleSelectAll(void);
    void HandleMkDir(void);

    void HandleMenuButtonPress(void);

    void LoadThumbnail(ThumbItem *item);
    void ImportFromDir(const QString &fromDir, const QString &toDir);
    void CopyMarkedFiles(bool move = false);

    void ClearMenu(UIListBtnType *menu);

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

    bool                m_isGallery;
    bool                m_showDevices;
    QString             m_currDir;
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
    int                 m_sortorder;
    bool                m_useOpenGL;
    bool                m_recurse;
    QStringList         m_paths;

    typedef void (IconView::*MenuAction)(void);

  public slots:
    void mediaStatusChanged(MediaStatus oldStatus, MythMediaDevice *pMedia);
};


#endif /* ICONVIEW_H */
