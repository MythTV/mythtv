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

// MythTV headers
#include <mythscreentype.h>
#include <mythuitext.h>
#include <mythuibuttonlist.h>
#include <mythuiimage.h>
#include <mythdialogbox.h>
#include <mythmedia.h>
#include <mthread.h>

// MythGallery headers
#include "galleryfilter.h"
#include "galleryfilterdlg.h"
#include "thumbview.h"

using namespace std;

class ThumbGenerator;
class MediaMonitor;
class ChildCountThread;

Q_DECLARE_METATYPE(ThumbItem*);

class IconView : public MythScreenType
{
    Q_OBJECT

  public:
    IconView(MythScreenStack *parent, const char *name,
             const QString &galleryDir, MythMediaDevice *initialDevice);
    ~IconView();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent*) override; // MythUIType
    void HandleRandomShow(void);
    void HandleSeasonalShow(void);

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
    MythMenu* CreateMetadataMenu(void);
    MythMenu*  CreateMarkingMenu(void);
    void HandleSubMenuFilter(void);
    MythMenu*  CreateFileMenu(void);

  private slots:
    void HandleRotateCW(void);
    void HandleRotateCCW(void);
    void HandleDeleteCurrent(void);
    void HandleSlideShow(void);
    void HandleSettings(void);
    void ReloadSettings(void);
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

    void DoMkDir(const QString& folderName);
    void DoDeleteMarked(bool doDelete);
    void DoRename(const QString& folderName);
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
    GalleryFilter      *m_galleryFilter      {nullptr};

    MythUIButtonList   *m_imageList          {nullptr};
    MythUIText         *m_captionText        {nullptr};
    MythUIText         *m_crumbsText         {nullptr};
    MythUIText         *m_positionText       {nullptr};
    MythUIText         *m_noImagesText       {nullptr};
    MythUIImage        *m_selectedImage      {nullptr};
    MythDialogBox      *m_menuPopup          {nullptr};
    MythScreenStack    *m_popupStack         {nullptr};

    bool                m_isGallery          {false};
    bool                m_showDevices        {false};
    QString             m_currDir;
    MythMediaDevice    *m_currDevice         {nullptr};

    ThumbGenerator     *m_thumbGen           {nullptr};
    ChildCountThread   *m_childCountThread   {nullptr};

    bool                m_showcaption        {false};
    int                 m_sortorder          {0};
    bool                m_useOpenGL          {false};
    bool                m_recurse            {false};
    QStringList         m_paths;

    QString             m_errorStr;

    bool                m_allowImportScripts {false};

  protected slots:
    void reloadData();

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
    explicit ChildCountEvent(ChildCountData *ccd) :
        QEvent(kEventType), childCountData(ccd) {}
    ~ChildCountEvent() = default;

    ChildCountData *childCountData {nullptr};

    static Type kEventType;
};

class ChildCountThread : public MThread
{
public:

    explicit ChildCountThread(QObject *parent) :
        MThread("ChildCountThread"), m_parent(parent) {}
    ~ChildCountThread();

    void addFile(const QString &filePath);
    void cancel();

protected:
    void run() override; // MThread

private:

    bool moreWork();
    int  getChildCount(const QString &filepath);

    QObject     *m_parent {nullptr};
    QStringList  m_fileList;
    QMutex       m_mutex;
};

class ImportThread: public MThread
{
    public:
        explicit ImportThread(const QString &cmd)
            : MThread("import"), m_command(cmd) {}
        void run() override; // MThread
    private:
        QString m_command;
};

#endif /* ICONVIEW_H */
