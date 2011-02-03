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

#include <vector>

// Qt headers
#include <QStringList>
#include <QList>
#include <QHash>
#include <QThread>

// MythTV headers
#include <mythscreentype.h>
#include <mythuitext.h>
#include <mythuibuttonlist.h>
#include <mythuiimage.h>
#include <mythdialogbox.h>
#include <mythmedia.h>

// MythGallery headers
#include "thumbview.h"

using namespace std;

class ThumbGenerator;
class MediaMonitor;
class ChildCountThread;

Q_DECLARE_METATYPE(ThumbItem*)

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

    void LoadDirectory(const QString &dir);

    bool HandleEscape(void);
    bool HandleMediaEscape(MediaMonitor*);
    bool HandleSubDirEscape(const QString &parent);

    bool HandleMediaDeviceSelect(ThumbItem *item);
    bool HandleImageSelect(const QString &action);

    void HandleMainMenu(void);
    void HandleSubMenuMetadata(void);
    void HandleSubMenuMark(void);
    void HandleSubMenuFile(void);

  private slots:
    void HandleRotateCW(void);
    void HandleRotateCCW(void);
    void HandleDeleteCurrent(void);
    void HandleSlideShow(void);
    void HandleRandomShow(void);
    void HandleSettings(void);
    void HandleEject(void);
    void HandleImport(void);
    void HandleShowDevices(void);
    void HandleCopyHere(void);
    void HandleMoveHere(void);
    void HandleDelete(void);
    void HandleDeleteMarked(void);
    void HandleClearMarked(void);
    void HandleClearOneMarked(void);
    void HandleSelectAll(void);
    void HandleSelectOne(void);
    void HandleMkDir(void);
    void HandleRename(void);

    void DoMkDir(QString folderName);
    void DoDeleteMarked(bool doDelete);
    void DoRename(QString folderName);
    void DoDeleteCurrent(bool doDelete);

  private:
    void LoadThumbnail(ThumbItem *item);
    void ImportFromDir(const QString &fromDir, const QString &toDir);
    void CopyMarkedFiles(bool move = false);
    ThumbItem *GetCurrentThumb(void);
    int GetChildCount(ThumbItem *thumbitem);

    QList<ThumbItem*>           m_itemList;
    QHash<QString, ThumbItem*>  m_itemHash;
    QStringList         m_itemMarked;
    QString             m_galleryDir;
    vector<int>         m_history;

    MythUIButtonList   *m_imageList;
    MythUIText         *m_captionText;
    MythUIText         *m_crumbsText;
    MythUIText         *m_positionText;
    MythUIText         *m_noImagesText;
    MythUIImage        *m_selectedImage;
    MythDialogBox      *m_menuPopup;
    MythScreenStack    *m_popupStack;

    bool                m_isGallery;
    bool                m_showDevices;
    QString             m_currDir;
    MythMediaDevice    *m_currDevice;

    ThumbGenerator     *m_thumbGen;
    ChildCountThread   *m_childCountThread;

    int                 m_showcaption;
    int                 m_sortorder;
    bool                m_useOpenGL;
    bool                m_recurse;
    QStringList         m_paths;

    QString             m_errorStr;

  public slots:
    void mediaStatusChanged(MythMediaStatus oldStatus, MythMediaDevice *pMedia);
    void HandleItemSelect(MythUIButtonListItem *);
    void UpdateText(MythUIButtonListItem *);
    void UpdateImage(MythUIButtonListItem *);

    friend class FileCopyThread;
};

typedef struct 
{
    QString fileName;
    int count;
} ChildCountData;

class ChildCountEvent : public QEvent
{
  public:
    ChildCountEvent(ChildCountData *ccd) :
        QEvent(kEventType), childCountData(ccd) {}
    ~ChildCountEvent() {}

    ChildCountData *childCountData;

    static Type kEventType;
};

class ChildCountThread : public QThread
{
public:

    ChildCountThread(QObject *parent);
    ~ChildCountThread();

    void addFile(const QString &fileName);
    void cancel();

protected:
    void run();

private:

    bool moreWork();
    int  getChildCount(const QString &fileName);

    QObject     *m_parent;
    QStringList  m_fileList;
    QMutex       m_mutex;
};

#endif /* ICONVIEW_H */
