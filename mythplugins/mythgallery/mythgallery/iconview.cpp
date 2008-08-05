/* ============================================================
 * File  : iconview.cpp
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

// ANSI C headers
#include <cmath>

// C++ headers
#include <algorithm>

using namespace std;

// Qt headers
#include <QApplication>
#include <QEvent>
#include <QDir>
#include <QMatrix>
#include <Q3ValueList>
#include <QPixmap>
#include <QFileInfo>

// MythTV headers
#include <mythtv/util.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/httpcomms.h>
#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythprogressdialog.h>
#include <mythtv/mythmediamonitor.h>

// MythGallery headers
#include "galleryutil.h"
#include "gallerysettings.h"
#include "thumbgenerator.h"
#include "iconview.h"
#include "singleview.h"
#include "glsingleview.h"

#include "constants.h"

#define LOC QString("IconView: ")
#define LOC_ERR QString("IconView, Error: ")

class FileCopyThread: public QThread
{
  public:
    FileCopyThread(IconView *parent, bool move);
    virtual void run();
    int GetProgress(void) { return m_progress; }

  private:
    bool         m_move;
    IconView    *m_parent;
    volatile int m_progress;
};

FileCopyThread::FileCopyThread(IconView *parent, bool move)
{
    m_move = move;
    m_parent = parent;
}

void FileCopyThread::run()
{
    QStringList::iterator it;
    QFileInfo fi;
    QFileInfo dest;

    m_progress = 0;

    for (it = m_parent->m_itemMarked.begin(); it != m_parent->m_itemMarked.end(); it++)
    {
        fi.setFile(*it);
        dest.setFile(QDir(m_parent->m_currDir), fi.fileName());

        if (fi.exists())
            GalleryUtil::CopyMove(fi, dest, m_move);

        m_progress++;
    }
}

///////////////////////////////////////////////////////////////////////////////

IconView::IconView(MythScreenStack *parent, const char *name,
                   const QString &galleryDir, MythMediaDevice *initialDevice)
        : MythScreenType(parent, name)
{
    m_itemList.setAutoDelete(true);

    m_galleryDir = galleryDir;

    m_isGallery = false;
    m_showDevices = false;
    m_currDir = QString::null;
    m_currDevice = initialDevice;

    m_thumbGen = new ThumbGenerator(this,0,0);

    m_showcaption = gContext->GetNumSetting("GalleryOverlayCaption", 0);
    m_sortorder = gContext->GetNumSetting("GallerySortOrder", 0);
    m_useOpenGL = gContext->GetNumSetting("SlideshowUseOpenGL", 0);
    m_recurse = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);
    m_paths = QStringList::split(
                ":", gContext->GetSetting("GalleryImportDirs"));
    m_errorStr = QString::null;

    m_captionText = NULL;

    m_menuPopup = NULL;

    QDir dir(m_galleryDir);
    if (!dir.exists() || !dir.isReadable())
    {
        m_errorStr = tr("MythGallery Directory '%1' does not exist "
                        "or is unreadable.").arg(m_galleryDir);
        return;
    }

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

IconView::~IconView()
{
    if (m_thumbGen)
    {
        delete m_thumbGen;
        m_thumbGen = NULL;
    }
}

bool IconView::Create(void)
{

    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("gallery-ui.xml", "gallery", this);

    if (!foundtheme)
        return false;

    m_imageList = dynamic_cast<MythUIButtonList *>
                (GetChild("images"));

    m_captionText = dynamic_cast<MythUIText *>
                (GetChild("text"));

    if (!m_imageList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_imageList, SIGNAL(itemClicked( MythUIButtonListItem*)),
            this, SLOT( HandleItemSelect(MythUIButtonListItem*)));
    connect(m_imageList, SIGNAL(itemSelected( MythUIButtonListItem*)),
            this, SLOT( UpdateText(MythUIButtonListItem*)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetupMediaMonitor();

    SetFocusWidget(m_imageList);
    m_imageList->SetActive(true);

    return true;
}

void IconView::LoadDirectory(const QString &dir)
{
    QDir d(dir);
    if (!d.exists())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "LoadDirectory called with " +
                QString("non-existant directory: '%1'").arg(dir));
        return;
    }

    m_showDevices = false;

    m_currDir = d.absPath();
    m_itemList.clear();
    m_itemHash.clear();
    m_imageList->Reset();

    m_isGallery = GalleryUtil::LoadDirectory(m_itemList, dir, m_sortorder,
                                             false, &m_itemHash, m_thumbGen);

    ThumbItem *thumbitem;
    for ( thumbitem = m_itemList.first(); thumbitem; thumbitem = m_itemList.next() )
    {
        thumbitem->InitCaption(m_showcaption);
        MythUIButtonListItem* item =
            new MythUIButtonListItem(m_imageList, thumbitem->GetCaption(), 0,
                                     true, MythUIButtonListItem::NotChecked);
        item->SetData(qVariantFromValue(thumbitem));

        LoadThumbnail(thumbitem);

        MythImage *image = GetMythPainter()->GetFormatImage();
        QPixmap *pixmap = thumbitem->GetPixmap();
        if (pixmap)
        {
            image->Assign(*pixmap);
            thumbitem->SetPixmap(NULL);

            item->setImage(image);
        }

        if (m_itemMarked.contains(thumbitem->GetPath()))
            item->setChecked(MythUIButtonListItem::FullChecked);
    }

    // TODO Not accurate, the image may be smaller than the button
    uint buttonwidth = m_imageList->ItemWidth();
    uint buttonheight = m_imageList->ItemHeight();

    if (m_thumbGen)
    {
        m_thumbGen->setSize((int)buttonwidth, (int)buttonheight);
        if (!m_thumbGen->running())
            m_thumbGen->start();
    }
}

void IconView::LoadThumbnail(ThumbItem *item)
{
    if (!item)
        return;

    bool canLoadGallery = m_isGallery;
    QImage image;

    if (canLoadGallery)
    {
        if (item->IsDir())
        {
            // try to find a highlight
            QDir subdir(item->GetPath(), "*.highlight.*",
                        QDir::Name, QDir::Files);

            if (subdir.count() > 0)
            {
                // check if the image format is understood
                QFileInfoList::const_iterator it = subdir.entryInfoList().begin();
                if (it != subdir.entryInfoList().end())
                {
                    QString path = it->absFilePath();
                    image.load(path);
                }
            }
        }
        else
        {
            QString fn = item->GetName();
            int firstDot = fn.find('.');
            if (firstDot > 0)
            {
                fn.insert(firstDot, ".thumb");
                QString galThumbPath(m_currDir + "/" + fn);
                image.load(galThumbPath);
            }
        }

        canLoadGallery = !(image.isNull());
    }

    if (!canLoadGallery)
    {
        QString cachePath = QString("%1%2.jpg")
                                .arg(ThumbGenerator::getThumbcacheDir(m_currDir))
                                .arg(item->GetName());

        if (!image.load(cachePath))
            VERBOSE(VB_IMPORTANT, QString("Could not load thumbnail: %1")
                                            .arg(cachePath));
    }

    if (!image.isNull())
    {
        int rotateAngle = 0;

        rotateAngle = item->GetRotationAngle();

        if (rotateAngle != 0)
        {
            QMatrix matrix;
            matrix.rotate(rotateAngle);
            image = image.xForm(matrix);
        }

        item->SetPixmap(new QPixmap(image));
    }
}

void IconView::SetupMediaMonitor(void)
{
#ifndef _WIN32
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (m_currDevice && mon && mon->ValidateAndLock(m_currDevice))
    {
        bool mounted = m_currDevice->isMounted(true);
        if (!mounted)
            mounted = m_currDevice->mount();

        if (mounted)
        {
            connect(m_currDevice,
                SIGNAL(statusChanged(MediaStatus, MythMediaDevice*)),
                SLOT(mediaStatusChanged(MediaStatus, MythMediaDevice*)));

            LoadDirectory(m_currDevice->getMountPath());

            mon->Unlock(m_currDevice);
            return;
        }
        else
        {
//             DialogBox *dlg = new DialogBox(gContext->GetMainWindow(),
//                              tr("Failed to mount device: ") +
//                              m_currDevice->getDevicePath() + "\n\n" +
//                              tr("Showing the default MythGallery directory."));
//             dlg->AddButton(tr("OK"));
//             dlg->exec();
//             dlg->deleteLater();
            mon->Unlock(m_currDevice);
        }
    }
    m_currDevice = NULL;
    LoadDirectory(m_galleryDir);
#endif // _WIN32
}

void IconView::UpdateText(MythUIButtonListItem *item)
{
    if (!m_captionText)
        return;

    ThumbItem *thumbitem = qVariantValue<ThumbItem *>(item->GetData());

    QString caption = "";
    if (thumbitem)
    {
        caption = thumbitem->GetCaption();
        caption = (caption.isNull()) ? "" : caption;
    }
    m_captionText->SetText(caption);
}

bool IconView::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Gallery", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            HandleMainMenu();
        }
        else if (action == "ROTRIGHT")
            HandleRotateCW();
        else if (action == "ROTLEFT")
            HandleRotateCCW();
        else if (action == "DELETE")
            HandleDelete();
        else if (action == "MARK")
        {
            ThumbItem *thumbitem = GetCurrentThumb();
            MythUIButtonListItem *item = m_imageList->GetItemCurrent();

            if (thumbitem)
            {
                if (!m_itemMarked.contains(thumbitem->GetPath()))
                {
                    m_itemMarked.append(thumbitem->GetPath());
                    item->setChecked(MythUIButtonListItem::FullChecked);
                }
                else
                {
                    m_itemMarked.remove(thumbitem->GetPath());
                    item->setChecked(MythUIButtonListItem::NotChecked);
                }
            }
        }
        else if (action == "SLIDESHOW")
            HandleSlideShow();
        else if (action == "RANDOMSHOW")
            HandleRandomShow();
        else if (action == "ESCAPE")
        {
            if (!HandleEscape())
                GetScreenStack()->PopScreen();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void IconView::HandleItemSelect(MythUIButtonListItem *item)
{
    bool handled = false;

    ThumbItem *thumbitem = qVariantValue<ThumbItem *>(item->GetData());

    if (!thumbitem)
        return;

    // if the selected thumbitem is a Media Device
    // attempt to mount it if it isn't already
    if (thumbitem->GetMediaDevice())
        handled = HandleMediaDeviceSelect(thumbitem);

    if (!handled && thumbitem->IsDir())
    {
        m_history.push_back(m_imageList->GetCurrentPos());
        LoadDirectory(thumbitem->GetPath());

        handled = true;
    }

    if (!handled)
        HandleImageSelect("SELECT");
}

bool IconView::HandleMediaDeviceSelect(ThumbItem *item)
{
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon && mon->ValidateAndLock(item->GetMediaDevice()))
    {
        m_currDevice = item->GetMediaDevice();

        if (!m_currDevice->isMounted())
            m_currDevice->mount();

        item->SetPath(m_currDevice->getMountPath(), true);

        connect(m_currDevice,
                SIGNAL(statusChanged(MediaStatus,
                                     MythMediaDevice*)),
                SLOT(mediaStatusChanged(MediaStatus,
                                        MythMediaDevice*)));

        LoadDirectory(m_currDevice->getMountPath());

        mon->Unlock(m_currDevice);
    }
    else
    {
        // device was removed
        QString msg = tr("Error") + '\n' + 
                tr("The selected device is no longer available");
        ShowOKDialog(msg, SLOT(HandleShowDevices()));
    }

    return true;
}

void IconView::HandleSlideShow(void)
{
    HandleImageSelect("SLIDESHOW");

    SetFocusWidget(m_imageList);
}

void IconView::HandleRandomShow(void)
{
    HandleImageSelect("RANDOMSHOW");

    SetFocusWidget(m_imageList);
}

bool IconView::HandleImageSelect(const QString &action)
{
    ThumbItem *thumbitem = GetCurrentThumb();

    if (!thumbitem || (thumbitem->IsDir() && !m_recurse))
        return false;

    int slideShow = ((action == "PLAY" || action == "SLIDESHOW") ? 1 :
                     (action == "RANDOMSHOW") ? 2 : 0);

    int pos = m_imageList->GetCurrentPos();

#ifdef USING_OPENGL
    if (m_useOpenGL && QGLFormat::hasOpenGL())
    {
        GLSDialog gv(m_itemList, &pos,
                        slideShow, m_sortorder,
                        gContext->GetMainWindow());
        gv.exec();
    }
    else
#endif
    {
        SingleView sv(m_itemList, &pos, slideShow, m_sortorder,
                      gContext->GetMainWindow());
        sv.exec();
    }

    // if the user deleted files while in single view mode
    // the cached contents of the directory will be out of
    // sync, reload the current directory to refresh the view
    LoadDirectory(m_currDir);

    m_imageList->SetItemCurrent(pos);

    return true;
}

bool IconView::HandleMediaEscape(MediaMonitor *mon)
{
    bool handled = false;
    QDir curdir(m_currDir);
    Q3ValueList<MythMediaDevice*> removables = mon->GetMedias(MEDIATYPE_DATA);
    Q3ValueList<MythMediaDevice*>::iterator it = removables.begin();
    for (; !handled && (it != removables.end()); it++)
    {
        if (!mon->ValidateAndLock(*it))
            continue;

        if (curdir == QDir((*it)->getMountPath()))
        {
            HandleShowDevices();

            // Make sure previous devices are visible and selected
            ThumbItem *item = NULL;
            if (!(*it)->getVolumeID().isEmpty())
                item = m_itemHash.value((*it)->getVolumeID());
            else
                item = m_itemHash.value((*it)->getDevicePath());

            if (item)
            {
                int pos = m_itemList.find(item);
                m_imageList->SetItemCurrent(pos);
            }

            handled = true;
        }
        else
        {
            handled = HandleSubDirEscape((*it)->getMountPath());
        }

        mon->Unlock(*it);
    }

    return handled;
}

static bool is_subdir(const QDir &parent, const QDir &subdir)
{
    QString pstr = parent.canonicalPath();
    QString cstr = subdir.canonicalPath();
    bool ret = !cstr.find(pstr);

    return ret;
}

bool IconView::HandleSubDirEscape(const QString &parent)
{
    bool handled = false;

    QDir curdir(m_currDir);
    QDir pdir(parent);
    if ((curdir != pdir) && is_subdir(pdir, curdir))
    {
        QString oldDirName = curdir.dirName();
        curdir.cdUp();
        LoadDirectory(curdir.absPath());

        int pos = m_history.back();
        m_history.pop_back();
        m_imageList->SetItemCurrent(pos);
        handled = true;
    }

    return handled;
}

bool IconView::HandleEscape(void)
{
    //VERBOSE(VB_IMPORTANT, LOC + "HandleEscape() " +
    //        QString("showDevices: %1").arg(m_showDevices));

    bool handled = false;

    // If we are showing the attached devices, ESCAPE should always exit..
    if (m_showDevices)
    {
        //VERBOSE(VB_IMPORTANT, LOC + "HandleEscape() exiting on showDevices");
        return false;
    }

    // If we are viewing an attached device we should show the attached devices
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon && m_currDevice)
        handled = HandleMediaEscape(mon);

    // If we are viewing a subdirectory of the gallery directory, we should
    // move up the directory tree, otherwise ESCAPE should exit..
    if (!handled)
        handled = HandleSubDirEscape(m_galleryDir);

    //VERBOSE(VB_IMPORTANT, LOC + "HandleEscape() handled: "<<handled<<"\n");

    return handled;
}

void IconView::customEvent(QEvent *event)
{

    if (event->type() == kMythGalleryThumbGenEventType)
    {
        ThumbGenEvent *tge = (ThumbGenEvent *)event;

        ThumbData *td = tge->thumbData;
        if (!td) return;

        ThumbItem *thumbitem = m_itemHash.value(td->fileName);
        if (thumbitem)
        {
            thumbitem->SetPixmap(NULL);

            int rotateAngle = thumbitem->GetRotationAngle();

            if (rotateAngle)
            {
                QMatrix matrix;
                matrix.rotate(rotateAngle);
                td->thumb = td->thumb.xForm(matrix);
            }

            int pos = m_itemList.find(thumbitem);

            LoadThumbnail(thumbitem);

            MythImage *image = GetMythPainter()->GetFormatImage();
            QPixmap *pixmap = thumbitem->GetPixmap();
            if (pixmap)
            {
                image->Assign(*pixmap);

                MythUIButtonListItem *item = m_imageList->GetItemAt(pos);
                item->setImage(image);
            }

            thumbitem->SetPixmap(NULL);
        }
        delete td;
    }
    else if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                                dynamic_cast<DialogCompletionEvent*>(event);

        QString resultid= dce->GetId();
        int buttonnum  = dce->GetResult();

        if (resultid == "mainmenu")
        {
            switch (buttonnum)
            {
                case 0:
                    HandleSlideShow();
                    break;
                case 1:
                    HandleRandomShow();
                    break;
                case 2:
                    HandleSubMenuMetadata();
                    break;
                case 3:
                    HandleSubMenuMark();
                    break;
                case 4:
                    HandleSubMenuFile();
                    break;
                case 5:
                    HandleSettings();
                    break;
            }
        }
        else if (resultid == "metadatamenu")
        {
            switch (buttonnum)
            {
                case 0:
                    HandleRotateCW();
                    break;
                case 1:
                    HandleRotateCCW();
                    break;
            }
        }
        else if (resultid == "markingmenu")
        {
            switch (buttonnum)
            {
                case 0:
                    HandleClearMarked();
                    break;
                case 1:
                    HandleSelectAll();
                    break;
            }
        }
        else if (resultid == "filemenu")
        {
            switch (buttonnum)
            {
                case 0:
                    HandleShowDevices();
                    break;
                case 1:
                    HandleImport();
                    break;
                case 2:
                    HandleCopyHere();
                    break;
                case 3:
                    HandleMoveHere();
                    break;
                case 4:
                    HandleDelete();
                    break;
                case 5:
                    HandleMkDir();
                    break;
                case 6:
                    HandleRename();
                    break;
            }
        }

        m_menuPopup = NULL;

    }

}

void IconView::HandleMainMenu(void)
{
    QString label = tr("Gallery Options");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "mythgallerymenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "mainmenu");

    m_menuPopup->AddButton(tr("SlideShow"));
    m_menuPopup->AddButton(tr("Random"));
    m_menuPopup->AddButton(tr("Meta Data Menu"));
    m_menuPopup->AddButton(tr("Marking Menu"));
    m_menuPopup->AddButton(tr("File Menu"));
    m_menuPopup->AddButton(tr("Settings"));
//     if (m_showDevices)
//     {
//         QDir d(m_currDir);
//         if (!d.exists())
//             m_currDir = m_galleryDir;
//
//         LoadDirectory(m_currDir);
//         m_showDevices = false;
//     }
}

void IconView::HandleSubMenuMetadata(void)
{
    QString label = tr("Metadata Options");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "mythgallerymenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "metadatamenu");

    m_menuPopup->AddButton(tr("Rotate CW"));
    m_menuPopup->AddButton(tr("Rotate CCW"));
}

void IconView::HandleSubMenuMark(void)
{
    QString label = tr("Marking Options");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "mythgallerymenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "markingmenu");

    m_menuPopup->AddButton(tr("Clear Marked"));
    m_menuPopup->AddButton(tr("Select All"));
}

void IconView::HandleSubMenuFile(void)
{
    QString label = tr("File Options");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "mythgallerymenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "filemenu");

    m_menuPopup->AddButton(tr("Show Devices"));
    m_menuPopup->AddButton(tr("Import"));
    m_menuPopup->AddButton(tr("Copy here"));
    m_menuPopup->AddButton(tr("Move here"));
    m_menuPopup->AddButton(tr("Delete"));
    m_menuPopup->AddButton(tr("Create Dir"));
    m_menuPopup->AddButton(tr("Rename"));
}

void IconView::HandleRotateCW(void)
{
    ThumbItem *thumbitem = GetCurrentThumb();

    if (!thumbitem || thumbitem->IsDir())
        return;

    int rotAngle = thumbitem->GetRotationAngle();

    rotAngle += 90;

    if (rotAngle >= 360)
        rotAngle -= 360;

    if (rotAngle < 0)
        rotAngle += 360;

    thumbitem->SetRotationAngle(rotAngle);
}

void IconView::HandleRotateCCW(void)
{
    ThumbItem *thumbitem = GetCurrentThumb();

    if (!thumbitem || thumbitem->IsDir())
        return;

    int rotAngle = thumbitem->GetRotationAngle();

    rotAngle -= 90;

    if (rotAngle >= 360)
        rotAngle -= 360;

    if (rotAngle < 0)
        rotAngle += 360;

    thumbitem->SetRotationAngle(rotAngle);
}

void IconView::HandleDeleteCurrent(void)
{
    ThumbItem *thumbitem = GetCurrentThumb();

    QString title = tr("Delete Current File or Folder");
    QString msg = (thumbitem->IsDir()) ?
        tr("Deleting 1 folder, including any subfolders and files.") :
        tr("Deleting 1 image.");

    ShowOKDialog(title + '\n' + msg, SLOT(DoDeleteCurrent(bool)), true);
}

void IconView::DoDeleteCurrent(bool doDelete)
{
    if (doDelete)
    {
        ThumbItem *thumbitem = GetCurrentThumb();
        QFileInfo fi;
        fi.setFile(thumbitem->GetPath());
        GalleryUtil::Delete(fi);

        LoadDirectory(m_currDir);
    }
}

void IconView::HandleSettings(void)
{
    GallerySettings settings;
    settings.exec();
    gContext->ClearSettingsCache();

    // reload settings
    m_showcaption = gContext->GetNumSetting("GalleryOverlayCaption", 0);
    m_sortorder   = gContext->GetNumSetting("GallerySortOrder", 0);
    m_useOpenGL   = gContext->GetNumSetting("SlideshowUseOpenGL", 0);
    m_recurse     = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);
    m_paths       = QStringList::split(
        ":", gContext->GetSetting("GalleryImportDirs"));

    // reload directory
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (m_currDevice && mon && mon->ValidateAndLock(m_currDevice))
    {
        LoadDirectory(m_currDevice->getMountPath());
        mon->Unlock(m_currDevice);
    }
    else
    {
        m_currDevice = NULL;
        LoadDirectory(m_galleryDir);
    }

    SetFocusWidget(m_imageList);
}

void IconView::HandleImport(void)
{
    QFileInfo path;
    QDir importdir;

//     DialogBox *importDlg = new DialogBox(
//         gContext->GetMainWindow(), tr("Import pictures?"));
//
//     importDlg->AddButton(tr("No"));
//     importDlg->AddButton(tr("Yes"));
//     DialogCode code = importDlg->exec();
//     importDlg->deleteLater();
//     if (kDialogCodeButton1 != code)
//         return;

    // Makes import directory samba/windows friendly (no colon)
    QString idirname = m_currDir + "/" +
        QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");

    importdir.mkdir(idirname);
    importdir.setPath(idirname);

    for (QStringList::const_iterator it = m_paths.begin();
         it != m_paths.end(); ++it)
    {
        path.setFile(*it);
        if (path.isDir() && path.isReadable())
        {
            ImportFromDir(*it, importdir.absPath());
        }
        else if (path.isFile() && path.isExecutable())
        {
            // TODO this should not be enabled by default!!!
            QString cmd = *it + " " + importdir.absPath();
            VERBOSE(VB_GENERAL, LOC + QString("Executing %1").arg(cmd));
            myth_system(cmd);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Could not read or execute %1").arg(*it));
        }
    }


    importdir.refresh();
    if (importdir.count() == 0)
    {
//         DialogBox *nopicsDlg = new DialogBox(
//             gContext->GetMainWindow(), tr("Nothing found to import"));
//
//         nopicsDlg->AddButton(tr("OK"));
//         nopicsDlg->exec();
//         nopicsDlg->deleteLater();

        return;
    }

    ThumbItem *item = new ThumbItem(importdir.dirName(),
                                    importdir.absPath(), true);
    m_itemList.append(item);
    m_itemHash.insert(item->GetName(), item);
    m_thumbGen->addFile(item->GetName());

    if (!m_thumbGen->running())
    {
        m_thumbGen->start();
    }
}

void IconView::HandleShowDevices(void)
{
#ifndef _WIN32
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (m_currDevice && mon && mon->ValidateAndLock(m_currDevice))
    {
        m_currDevice->disconnect(this);
        mon->Unlock(m_currDevice);
    }
    else
        m_currDir = m_galleryDir;
#endif

    m_currDevice = NULL;

    m_showDevices = true;

    m_itemList.clear();
    m_itemHash.clear();

    m_thumbGen->cancel();

    // add gallery directory
    ThumbItem *item = new ThumbItem("Gallery", m_galleryDir, true);
    m_itemList.append(item);
    m_itemHash.insert(item->GetName(), item);

#ifndef _WIN32
    if (mon)
    {
        Q3ValueList<MythMediaDevice*> removables =
            mon->GetMedias(MEDIATYPE_DATA);
        Q3ValueList<MythMediaDevice*>::Iterator it = removables.begin();
        for (; it != removables.end(); it++)
        {
            if (mon->ValidateAndLock(*it))
            {
                item = new ThumbItem(
                    (*it)->getVolumeID().isEmpty() ?
                    (*it)->getDevicePath() : (*it)->getVolumeID(),
                    (*it)->getMountPath(), true, *it);

                m_itemList.append(item);
                m_itemHash.insert(item->GetName(), item);

                mon->Unlock(*it);
            }
        }
    }
#endif

    // exit from menu on show devices action..
    SetFocusWidget(m_imageList);
}

void IconView::HandleCopyHere(void)
{
    CopyMarkedFiles(false);
    HandleClearMarked();
}

void IconView::HandleMoveHere(void)
{
    CopyMarkedFiles(true);
    HandleClearMarked();
}

void IconView::HandleDelete(void)
{
    if (m_itemMarked.isEmpty())
        HandleDeleteCurrent();
    else
        HandleDeleteMarked();
}

void IconView::HandleDeleteMarked(void)
{
    QString msg = /*tr("Delete Marked Files") + "\n\n" +*/ 
            tr("Deleting %1 images and folders, including "
               "any subfolders and files.").arg(m_itemMarked.count());
    ShowOKDialog(msg, SLOT(DoDeleteMarked(bool)), true);
}

void IconView::DoDeleteMarked(bool doDelete)
{
    if (doDelete)
    {
        QStringList::iterator it;
        QFileInfo fi;

        for (it = m_itemMarked.begin(); it != m_itemMarked.end(); it++)
        {
            fi.setFile(*it);

            GalleryUtil::Delete(fi);
        }

        m_itemMarked.clear();

        LoadDirectory(m_currDir);
    }
}

void IconView::HandleClearMarked(void)
{
    m_itemMarked.clear();
    m_imageList->SetAllChecked(MythUIButtonListItem::NotChecked);
}

void IconView::HandleSelectAll(void)
{
    ThumbItem *item;
    for (item = m_itemList.first(); item; item = m_itemList.next())
    {
        if (!m_itemMarked.contains(item->GetPath()))
        {
            m_itemMarked.append(item->GetPath());
        }
    }

    m_imageList->SetAllChecked(MythUIButtonListItem::FullChecked);
}

void IconView::HandleMkDir(void)
{
    QString folderName = tr("New Folder");

    QString message = tr("Create New Folder");

    MythTextInputDialog *dialog = new MythTextInputDialog(m_popupStack, message);

    if (dialog->Create())
       m_popupStack->AddScreen(dialog);

     connect(dialog, SIGNAL(haveResult(QString)),
            SLOT(DoMkDir(QString)), Qt::QueuedConnection);
}

void IconView::DoMkDir(QString folderName)
{
    QDir cdir(m_currDir);
    cdir.mkdir(folderName);

    LoadDirectory(m_currDir);
}


void IconView::HandleRename(void)
{
    ThumbItem *thumbitem = GetCurrentThumb();

    if (!thumbitem)
        return;

    QString folderName = thumbitem->GetName();

    QString message = tr("Rename");

    MythTextInputDialog *dialog = new MythTextInputDialog(m_popupStack,
            message, FilterNone, false, folderName);

    if (dialog->Create())
       m_popupStack->AddScreen(dialog);

     connect(dialog, SIGNAL(haveResult(QString)),
            SLOT(DoRename(QString)), Qt::QueuedConnection);
}

void IconView::DoRename(QString folderName)
{
    if (folderName.isEmpty() || folderName == "." || folderName == "..")
        return;

    ThumbItem *thumbitem = GetCurrentThumb();

    if (!thumbitem)
        return;

    if (!GalleryUtil::Rename(m_currDir, thumbitem->GetName(), folderName))
    {
        QString msg;
        if (thumbitem->IsDir())
            msg = tr("Failed to rename directory");
        else
            msg = tr("Failed to rename file");

        ShowOKDialog(msg, NULL);

        return;
    }

    LoadDirectory(m_currDir);
}

void IconView::ImportFromDir(const QString &fromDir, const QString &toDir)
{
    QDir d(fromDir);

    if (!d.exists())
        return;

    d.setNameFilter(MEDIA_FILENAMES);
    d.setSorting((QDir::SortFlag)m_sortorder);
    d.setFilter(QDir::Files | QDir::Dirs | QDir::NoSymLinks  | QDir::Readable);
    d.setMatchAllDirs(true);
    QFileInfoList list = d.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;

        if (fi->isDir())
        {
            QString newdir(toDir + "/" + fi->fileName());
            d.mkdir(newdir);
            ImportFromDir(fi->absFilePath(), newdir);
        }
        else
        {
            VERBOSE(VB_GENERAL, LOC + QString("Copying %1 to %2")
                    .arg(fi->absFilePath())
                    .arg(toDir));

            // TODO FIXME, we shouldn't need a myth_system call here
            QString cmd = QString("cp \"%1\" \"%2\"")
                .arg(fi->absFilePath()).arg(toDir);
            cmd = QString(cmd.toLocal8Bit().constData());
            myth_system(cmd);
        }
    }
}

void IconView::CopyMarkedFiles(bool move)
{
    if (m_itemMarked.isEmpty())
        return;

    QString msg = (move) ?
        tr("Moving marked images...") : tr("Copying marked images...");

    MythUIProgressDialog *copy_progress = new MythUIProgressDialog(msg,
                                                    m_popupStack,
                                                    "copyprogressdialog");

    if (copy_progress->Create())
    {
        m_popupStack->AddScreen(copy_progress, false);
        copy_progress->SetTotal(m_itemMarked.count());
    }
    else
    {
        delete copy_progress;
        copy_progress = NULL;
    }

    FileCopyThread *filecopy = new FileCopyThread(this, move);
    int progress = -1;
    filecopy->start();

    while (!filecopy->isFinished())
    {
        if (copy_progress)
        {
            if (progress != filecopy->GetProgress())
            {
                progress = filecopy->GetProgress();
                copy_progress->SetProgress(progress);
            }
        }

        usleep(500);
        qApp->processEvents();
    }

    delete filecopy;

    if (copy_progress)
        copy_progress->Close();

    LoadDirectory(m_currDir);
}

void IconView::mediaStatusChanged(MediaStatus oldStatus,
                                  MythMediaDevice *pMedia)
{
    (void) oldStatus;
    if (m_currDevice == pMedia)
    {
        HandleShowDevices();

        // UpdateText();
    }
}

void IconView::ShowOKDialog(const QString &message, const char *slot, bool showCancel)
{
    MythConfirmationDialog *dialog = new MythConfirmationDialog(
            m_popupStack, message, showCancel);

    if (dialog->Create())
        m_popupStack->AddScreen(dialog);

    if (slot)
        connect(dialog, SIGNAL(haveResult(bool)), slot, Qt::QueuedConnection);
}

ThumbItem *IconView::GetCurrentThumb(void)
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    return qVariantValue<ThumbItem *>(item->GetData());
}
