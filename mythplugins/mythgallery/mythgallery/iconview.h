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
#include <QKeyEvent>
#include <Q3PtrList>

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythlistbutton.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/mythmedia.h>

// MythGallery headers
#include "thumbview.h"

class ThumbGenerator;
class MediaMonitor;

class IconView : public MythScreenType
{
    Q_OBJECT

  public:
    IconView(MythScreenStack *parent, const char *name,
             const QString &galleryDir, MythMediaDevice *initialDevice);
    ~IconView();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

    QString GetError(void) { return m_errorStr; }

  private:
    void SetupMediaMonitor(void);

    bool LoadTheme(void);

    void LoadDirectory(const QString &dir);

    void UpdateText(void);

    bool HandleEscape(void);
    bool HandleMediaEscape(MediaMonitor*);
    bool HandleSubDirEscape(const QString &parent);

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
    void HandleRename(void);

    void LoadThumbnail(ThumbItem *item);
    void ImportFromDir(const QString &fromDir, const QString &toDir);
    void CopyMarkedFiles(bool move = false);

    Q3PtrList<ThumbItem> m_itemList;
    Q3Dict<ThumbItem>    m_itemDict;
    QStringList         m_itemMarked;
    QString             m_galleryDir;

    MythListButton     *m_imageList;
    MythUIText         *m_captionText;
    MythDialogBox      *m_menuPopup;

    bool                m_isGallery;
    bool                m_showDevices;
    QString             m_currDir;
    MythMediaDevice    *m_currDevice;

    ThumbGenerator     *m_thumbGen;

    int                 m_showcaption;
    int                 m_sortorder;
    bool                m_useOpenGL;
    bool                m_recurse;
    QStringList         m_paths;

    QString             m_errorStr;

  public slots:
    void mediaStatusChanged(MediaStatus oldStatus, MythMediaDevice *pMedia);
    void HandleItemSelect(MythListButtonItem *);
    void HandleMenuButtonPress(MythListButtonItem *);
    void UpdateText(MythListButtonItem *);
};


#endif /* ICONVIEW_H */
