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
#include <qevent.h>
#include <qdir.h>
#include <qmatrix.h>
#include <QKeyEvent>
#include <Q3ValueList>
#include <QPixmap>
#include <QFileInfo>

// MythTV headers
#include <mythtv/util.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/httpcomms.h>
#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythmainwindow.h>
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

IconView::IconView(MythScreenStack *parent, const char *name,
                   const QString &galleryDir, MythMediaDevice *initialDevice)
        : MythScreenType(parent, name)
{
    m_itemList.setAutoDelete(true);
    m_itemDict.setAutoDelete(false);

    m_galleryDir = galleryDir;

    m_menuList = m_submenuList = NULL,

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

    QDir dir(m_galleryDir);
    if (!dir.exists() || !dir.isReadable())
    {
        m_errorStr = tr("MythGallery Directory '%1' does not exist "
                        "or is unreadable.").arg(m_galleryDir);
        return;
    }

}

IconView::~IconView()
{
    ClearMenu(m_submenuList);
    ClearMenu(m_menuList);

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

    m_menuList = dynamic_cast<MythListButton *>
                (GetChild("menu"));
    m_submenuList = dynamic_cast<MythListButton *>
                (GetChild("submenu"));
    m_imageList = dynamic_cast<MythListButton *>
                (GetChild("images"));

    m_captionText = dynamic_cast<MythUIText *>
                (GetChild("text"));

    if (!m_menuList || !m_submenuList || !m_imageList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_imageList, SIGNAL(itemClicked( MythListButtonItem*)),
            this, SLOT( HandleItemSelect(MythListButtonItem*)));
    connect(m_imageList, SIGNAL(itemSelected( MythListButtonItem*)),
            this, SLOT( UpdateText(MythListButtonItem*)));
    connect(m_menuList, SIGNAL(itemClicked( MythListButtonItem*)),
            this, SLOT( HandleMenuButtonPress(MythListButtonItem*)));
    connect(m_submenuList, SIGNAL(itemClicked( MythListButtonItem*)),
            this, SLOT( HandleMenuButtonPress(MythListButtonItem*)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    // Set Menu Text and Handlers
    MythListButtonItem *item;
    item = new MythListButtonItem(m_menuList, tr("SlideShow"));
    item->setData(new MenuAction(&IconView::HandleSlideShow));
    item = new MythListButtonItem(m_menuList, tr("Random"));
    item->setData(new MenuAction(&IconView::HandleRandomShow));
    item = new MythListButtonItem(m_menuList, tr("Meta Data..."));
    item->setData(new MenuAction(&IconView::HandleSubMenuMetadata));
    item = new MythListButtonItem(m_menuList, tr("Marking..."));
    item->setData(new MenuAction(&IconView::HandleSubMenuMark));
    item = new MythListButtonItem(m_menuList, tr("File..."));
    item->setData(new MenuAction(&IconView::HandleSubMenuFile));
    item = new MythListButtonItem(m_menuList, tr("Settings"));
    item->setData(new MenuAction(&IconView::HandleSettings));

    SetupMediaMonitor();

    SetFocusWidget(m_imageList);
    m_imageList->SetActive(true);
    m_menuList->SetActive(false);
    m_submenuList->SetActive(false);
    m_submenuList->SetVisible(false);

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
    m_itemDict.clear();
    m_imageList->Reset();

    m_isGallery = GalleryUtil::LoadDirectory(m_itemList, dir, m_sortorder,
                                             false, &m_itemDict, m_thumbGen);

    ThumbItem *thumbitem;
    for ( thumbitem = m_itemList.first(); thumbitem; thumbitem = m_itemList.next() )
    {
        MythListButtonItem* item =
            new MythListButtonItem(m_imageList, "", 0, true,
                                    MythListButtonItem::NotChecked);
        item->setData(thumbitem);

        LoadThumbnail(thumbitem);

        MythImage *image = GetMythPainter()->GetFormatImage();
        QPixmap *pixmap = thumbitem->GetPixmap();
        if (pixmap)
        {
            image->Assign(*pixmap);
            thumbitem->SetPixmap(NULL);

            item->setImage(image);
        }
    }

    uint buttonwidth = m_imageList->ItemWidth() -
                                            (2 * m_imageList->GetMarginX());
    uint buttonheight = m_imageList->ItemHeight() -
                                            (2 * m_imageList->GetMarginY());
    m_thumbGen->setSize((int)buttonwidth, (int)buttonheight);

    if (m_thumbGen && !m_thumbGen->running())
    {
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
                                .arg(m_thumbGen->getThumbcacheDir(m_currDir))
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

void IconView::UpdateText(MythListButtonItem *item)
{
    if (!m_captionText)
        return;

    ThumbItem *thumbitem = (ThumbItem *)item->getData();

    QString caption = "";
    if (thumbitem)
    {
        thumbitem->InitCaption(m_showcaption);
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
            if (m_menuList == GetFocusWidget())
                SetFocusWidget(m_imageList);
            else if (m_submenuList == GetFocusWidget())
            {
                m_submenuList->SetVisible(false);
                SetFocusWidget(m_menuList);
            }
            else
                SetFocusWidget(m_menuList);
        }
        else if (action == "ESCAPE")
        {
            if (m_menuList == GetFocusWidget())
                SetFocusWidget(m_imageList);
            else if (m_submenuList == GetFocusWidget())
            {
                m_submenuList->SetVisible(false);
                SetFocusWidget(m_menuList);
            }
            else
                GetScreenStack()->PopScreen();
        }
        else if (action == "ROTRIGHT")
            HandleRotateCW();
        else if (action == "ROTLEFT")
            HandleRotateCCW();
        else if (action == "DELETE")
            HandleDelete();
        else if (action == "MARK")
        {
            MythListButtonItem *item = m_imageList->GetItemCurrent();
            ThumbItem *thumbitem = (ThumbItem *)item->getData();

            if (!m_itemMarked.contains(thumbitem->GetPath()))
            {
                m_itemMarked.append(thumbitem->GetPath());
                item->setChecked(MythListButtonItem::FullChecked);
            }
            else
            {
                m_itemMarked.remove(thumbitem->GetPath());
                item->setChecked(MythListButtonItem::NotChecked);
            }

        }
        else if (action == "SLIDESHOW")
            HandleSlideShow();
        else if (action == "RANDOMSHOW")
            HandleRandomShow();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void IconView::HandleItemSelect(MythListButtonItem *item)
{
    bool handled = false;

    ThumbItem *thumbitem = (ThumbItem *)item->getData();

    if (!thumbitem)
        return;

    // if the selected thumbitem is a Media Device
    // attempt to mount it if it isn't already
    if (thumbitem->GetMediaDevice())
        handled = HandleMediaDeviceSelect(thumbitem);

    if (!handled && thumbitem->IsDir())
    {
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
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(),
            tr("Error"),
            tr("The selected device is no longer available"));

        HandleShowDevices();
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
    MythListButtonItem *item = m_imageList->GetItemCurrent();
    ThumbItem *thumbitem = (ThumbItem *)item->getData();

    if (!thumbitem || (thumbitem->IsDir() && !m_recurse))
        return false;

    int slideShow = ((action == "PLAY" || action == "SLIDESHOW") ? 1 : 
                     (action == "RANDOMSHOW") ? 2 : 0);

    int pos = m_imageList->GetCurrentPos();

#ifdef USING_OPENGL
    if (m_useOpenGL && QGLFormat::hasOpenGL())
    {
        GLSDialog gv(m_itemList, pos,
                        slideShow, m_sortorder,
                        gContext->GetMainWindow());
        gv.exec();
    }
    else
#endif
    {
        SingleView sv(m_itemList, pos, slideShow, m_sortorder,
                      gContext->GetMainWindow());
        sv.exec();
    }

    // if the user deleted files while in single view mode
    // the cached contents of the directory will be out of
    // sync, reload the current directory to refresh the view
    LoadDirectory(m_currDir);

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
                item = m_itemDict.find((*it)->getVolumeID());
            else
                item = m_itemDict.find((*it)->getDevicePath());

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

        // Make sure up-directory is visible and selected
        ThumbItem *item = m_itemDict.find(oldDirName);
        if (item)
        {
            int pos = m_itemList.find(item);
            m_imageList->SetItemCurrent(pos);
        }

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
    ThumbGenEvent *tge = (ThumbGenEvent *)event;

    if ((ThumbGenEvent::Type)tge->type() == ThumbGenEvent::ImageReady)
    {
        ThumbData *td = tge->thumbData;
        if (!td) return;

        ThumbItem *thumbitem = m_itemDict.find(td->fileName);
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

                MythListButtonItem *item = m_imageList->GetItemAt(pos);
                item->setImage(image);
            }

            thumbitem->SetPixmap(NULL);
        }
        delete td;
    }

}

void IconView::HandleMenuButtonPress(MythListButtonItem *item)
{
    if (!item || !item->getData())
        return;

    if (GetFocusWidget() == m_submenuList)
    {
        SetFocusWidget(m_imageList);
        m_submenuList->SetVisible(false);
    }

    MenuAction *act = (MenuAction*) item->getData();
    (this->*(*act))();
}

void IconView::HandleMainMenu(void)
{
    if (m_showDevices)
    {
        QDir d(m_currDir);
        if (!d.exists())
            m_currDir = m_galleryDir;

        LoadDirectory(m_currDir);
        m_showDevices = false;
    }

    ClearMenu(m_submenuList);
    m_submenuList->Reset();
    m_submenuList->SetVisible(false);

    SetFocusWidget(m_menuList);
}

void IconView::HandleSubMenuMetadata(void)
{
    ClearMenu(m_submenuList);
    m_submenuList->Reset();
    m_submenuList->SetVisible(true);

    MythListButtonItem *item;

    item = new MythListButtonItem(m_submenuList, tr("Return"));
    item->setData(new MenuAction(&IconView::HandleMainMenu));

    item = new MythListButtonItem(m_submenuList, tr("Rotate CW"));
    item->setData(new MenuAction(&IconView::HandleRotateCW));

    item = new MythListButtonItem(m_submenuList, tr("Rotate CCW"));
    item->setData(new MenuAction(&IconView::HandleRotateCCW));

    SetFocusWidget(m_submenuList);
}

void IconView::HandleSubMenuMark(void)
{
    ClearMenu(m_submenuList);
    m_submenuList->Reset();
    m_submenuList->SetVisible(true);

    MythListButtonItem *item;

    item = new MythListButtonItem(m_submenuList, tr("Return"));
    item->setData(new MenuAction(&IconView::HandleMainMenu));

    item = new MythListButtonItem(m_submenuList, tr("Clear Marked"));
    item->setData(new MenuAction(&IconView::HandleClearMarked));

    item = new MythListButtonItem(m_submenuList, tr("Select All"));
    item->setData(new MenuAction(&IconView::HandleSelectAll));

    SetFocusWidget(m_submenuList);
}

void IconView::HandleSubMenuFile(void)
{
    ClearMenu(m_submenuList);
    m_submenuList->Reset();
    m_submenuList->SetVisible(true);

    MythListButtonItem *item;

    item = new MythListButtonItem(m_submenuList, tr("Return"));
    item->setData(new MenuAction(&IconView::HandleMainMenu));

    item = new MythListButtonItem(m_submenuList, tr("Show Devices"));
    item->setData(new MenuAction(&IconView::HandleShowDevices));

    item = new MythListButtonItem(m_submenuList, tr("Import"));
    item->setData(new MenuAction(&IconView::HandleImport));

    item = new MythListButtonItem(m_submenuList, tr("Copy here"));
    item->setData(new MenuAction(&IconView::HandleCopyHere));

    item = new MythListButtonItem(m_submenuList, tr("Move here"));
    item->setData(new MenuAction(&IconView::HandleMoveHere));

    item = new MythListButtonItem(m_submenuList, tr("Delete"));
    item->setData(new MenuAction(&IconView::HandleDelete));

    item = new MythListButtonItem(m_submenuList, tr("Create Dir"));
    item->setData(new MenuAction(&IconView::HandleMkDir));

    item = new MythListButtonItem(m_submenuList, tr("Rename"));
    item->setData(new MenuAction(&IconView::HandleRename));

    SetFocusWidget(m_submenuList);
}

void IconView::HandleRotateCW(void)
{
    MythListButtonItem *item = m_imageList->GetItemCurrent();
    ThumbItem *thumbitem = (ThumbItem *)item->getData();

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
    MythListButtonItem *item = m_imageList->GetItemCurrent();
    ThumbItem *thumbitem = (ThumbItem *)item->getData();

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
    MythListButtonItem *item = m_imageList->GetItemCurrent();
    ThumbItem *thumbitem = (ThumbItem *)item->getData();

    QString title = tr("Delete Current File or Folder");
    QString msg = (thumbitem->IsDir()) ?
        tr("Deleting 1 folder, including any subfolders and files.") :
        tr("Deleting 1 image.");

    bool cont = MythPopupBox::showOkCancelPopup(
        gContext->GetMainWindow(), title, msg, false);

    if (cont)
    {
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
    m_itemDict.insert(item->GetName(), item);
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
    m_itemDict.clear();

    m_thumbGen->cancel();

    // add gallery directory
    ThumbItem *item = new ThumbItem("Gallery", m_galleryDir, true);
    m_itemList.append(item);
    m_itemDict.insert(item->GetName(), item);

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
                m_itemDict.insert(item->GetName(), item);

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
    bool cont = MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(),
                    tr("Delete Marked Files"),
                    QString(tr("Deleting %1 images and folders, including "
                               "any subfolders and files."))
                            .arg(m_itemMarked.count()),
                    false);

    if (cont)
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
    m_imageList->SetAllChecked(MythListButtonItem::NotChecked);
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

    m_imageList->SetAllChecked(MythListButtonItem::FullChecked);
}

void IconView::HandleMkDir(void)
{
    QString folderName = tr("New Folder");

    bool res = MythPopupBox::showGetTextPopup(
        gContext->GetMainWindow(), tr("Create New Folder"),
        tr("Create New Folder"), folderName);

    if (res)
    {
        QDir cdir(m_currDir);
        cdir.mkdir(folderName);

        LoadDirectory(m_currDir);
    }
}


void IconView::HandleRename(void)
{
    MythListButtonItem *item = m_imageList->GetItemCurrent();
    ThumbItem *thumbitem = (ThumbItem *)item->getData();

    if (!thumbitem)
        return;

    QString folderName = thumbitem->GetName();

    bool res = MythPopupBox::showGetTextPopup(
        gContext->GetMainWindow(), tr("Rename"),
        tr("Rename"), folderName);

    if (folderName.isEmpty() || folderName == "." || folderName == "..")
        return;

    if (res)
    {
        if (!GalleryUtil::Rename(m_currDir, thumbitem->GetName(), folderName))
        {
            QString msg;
            if (thumbitem->IsDir())
                msg = tr("Failed to rename directory");
            else
                msg = tr("Failed to rename file");

//             DialogBox *dlg = new DialogBox(gContext->GetMainWindow(), msg);
//             dlg->AddButton(tr("OK"));
//             dlg->exec();
//             dlg->deleteLater();

            return;
        }

        LoadDirectory(m_currDir);
    }
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
            QString cmd = "cp \"" + fi->absFilePath().local8Bit() +
                          "\" \"" + toDir.local8Bit() + "\"";

            myth_system(cmd);
        }
    }
}

void IconView::CopyMarkedFiles(bool move)
{
    if (m_itemMarked.isEmpty())
        return;

    QStringList::iterator it;
    QFileInfo fi;
    QFileInfo dest;
    int count = 0;

    QString msg = (move) ?
        tr("Moving marked images...") : tr("Copying marked images...");

    MythProgressDialog *progress =
        new MythProgressDialog(msg, m_itemMarked.count());

    for (it = m_itemMarked.begin(); it != m_itemMarked.end(); it++)
    {
        fi.setFile(*it);
        dest.setFile(QDir(m_currDir), fi.fileName());

        if (fi.exists())
            GalleryUtil::CopyMove(fi, dest, move);

        progress->setProgress(++count);
    }

    progress->Close();
    progress->deleteLater();

    LoadDirectory(m_currDir);
}

void IconView::ClearMenu(MythListButton *menu)
{
    if (!menu || menu->IsEmpty())
        return;

    for (int i = 0; i < menu->GetCount(); i++)
    {
        MythListButtonItem *item = menu->GetItemAt(i);
        if (item)
        {
            MenuAction *act = (MenuAction*) item->getData();
            if (act)
            {
                delete act;
                item->setData(NULL);
            }
        }
    }
}

void IconView::mediaStatusChanged(MediaStatus oldStatus,
                                  MythMediaDevice *pMedia)
{
    (void) oldStatus;
    if (m_currDevice == pMedia)
    {
        HandleShowDevices();

        UpdateText();
    }
}
