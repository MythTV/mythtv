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

// POSIX headers
#include <unistd.h>

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
#include <QList>
#include <QFileInfo>

// MythTV headers
#include <mythdate.h>
#include <mythdbcon.h>
#include <mythsystem.h>
#include <mythcontext.h>
#include <mythlogging.h>
#include <mythmainwindow.h>
#include <mythprogressdialog.h>
#include <mythmediamonitor.h>

// MythGallery headers
#include "galleryutil.h"
#include "gallerysettings.h"
#include "galleryfilter.h"
#include "thumbgenerator.h"
#include "iconview.h"
#include "singleview.h"
#include "glsingleview.h"

#define LOC QString("IconView: ")

QEvent::Type ChildCountEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

class FileCopyThread : public MThread
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

FileCopyThread::FileCopyThread(IconView *parent, bool move) :
    MThread("FileCopy"), m_move(move), m_parent(parent), m_progress(0)
{
}

void FileCopyThread::run()
{
    RunProlog();

    QStringList::iterator it;
    QFileInfo fi;
    QFileInfo dest;

    m_progress = 0;

    for (it = m_parent->m_itemMarked.begin();
         it != m_parent->m_itemMarked.end(); ++it)
    {
        fi.setFile(*it);
        dest.setFile(QDir(m_parent->m_currDir), fi.fileName());

        if (fi.exists())
            GalleryUtil::CopyMove(fi, dest, m_move);

        m_progress++;
    }

    RunEpilog();
}

///////////////////////////////////////////////////////////////////////////////

IconView::IconView(MythScreenStack *parent, const char *name,
                   const QString &galleryDir, MythMediaDevice *initialDevice)
        : MythScreenType(parent, name)
{
    m_galleryDir = galleryDir;
    m_galleryFilter = new GalleryFilter();

    m_isGallery = false;
    m_showDevices = false;
    m_currDevice = initialDevice;

    m_thumbGen = new ThumbGenerator(this, 0, 0);
    m_childCountThread = new ChildCountThread(this);

    m_showcaption = gCoreContext->GetNumSetting("GalleryOverlayCaption", 0);
    m_sortorder = gCoreContext->GetNumSetting("GallerySortOrder", 0);
    m_useOpenGL = gCoreContext->GetNumSetting("SlideshowUseOpenGL", 0);
    m_recurse = gCoreContext->GetNumSetting("GalleryRecursiveSlideshow", 0);
    m_paths = gCoreContext->GetSetting("GalleryImportDirs").split(":");
    m_errorStr = QString::null;

    m_captionText = NULL;
    m_noImagesText = NULL;
    m_selectedImage = NULL;

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
    if (m_galleryFilter)
    {
        delete m_galleryFilter;
        m_galleryFilter = NULL;
    }
    if (m_childCountThread)
    {
        delete m_childCountThread;
        m_childCountThread = NULL;
    }
}

bool IconView::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("gallery-ui.xml", "gallery", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_imageList,     "images", &err);
    UIUtilW::Assign(this, m_captionText,   "title");
    UIUtilW::Assign(this, m_noImagesText,  "noimages");
    UIUtilW::Assign(this, m_selectedImage, "selectedimage");
    UIUtilW::Assign(this, m_positionText,  "position");
    UIUtilW::Assign(this, m_crumbsText,    "breadcrumbs");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'gallery'");
        return false;
    }

    connect(m_imageList, SIGNAL(itemClicked( MythUIButtonListItem*)),
            this, SLOT( HandleItemSelect(MythUIButtonListItem*)));
    connect(m_imageList, SIGNAL(itemSelected( MythUIButtonListItem*)),
            this, SLOT( UpdateText(MythUIButtonListItem*)));
    connect(m_imageList, SIGNAL(itemSelected( MythUIButtonListItem*)),
            this, SLOT( UpdateImage(MythUIButtonListItem*)));

    if (m_noImagesText)
    {
        m_noImagesText->SetText(tr("No images found in this folder."));
        m_noImagesText->SetVisible(false);
    }

    BuildFocusList();

    // TODO Not accurate, the image may be smaller than the button
    int thumbWidth  = m_imageList->ItemWidth();
    int thumbHeight = m_imageList->ItemHeight();
    if (m_selectedImage && (m_selectedImage->GetArea().width()  > thumbWidth ||
                            m_selectedImage->GetArea().height() > thumbHeight))
    {
       thumbWidth  = m_selectedImage->GetArea().width();
       thumbHeight = m_selectedImage->GetArea().height();
    }

    if (m_thumbGen)
        m_thumbGen->setSize(thumbWidth, thumbHeight);

    SetupMediaMonitor();
    if (!m_currDevice)
        LoadDirectory(m_galleryDir);

    return true;
}

void IconView::LoadDirectory(const QString &dir)
{
    if (m_thumbGen && m_thumbGen->isRunning())
        m_thumbGen->cancel();

    if (m_childCountThread && m_childCountThread->isRunning())
        m_childCountThread->cancel();

    QDir d(dir);
    if (!d.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "LoadDirectory called with " +
                QString("non-existant directory: '%1'").arg(dir));
        return;
    }

    m_showDevices = false;

    m_currDir = d.absolutePath();

    while (!m_itemList.isEmpty())
        delete m_itemList.takeFirst();

    m_itemHash.clear();
    m_imageList->Reset();

    m_isGallery = GalleryUtil::LoadDirectory(m_itemList, dir, *m_galleryFilter,
                                             false, &m_itemHash, m_thumbGen);

    if (m_thumbGen && !m_thumbGen->isRunning())
        m_thumbGen->start();

    ThumbItem *thumbitem;
    for (int x = 0; x < m_itemList.size(); x++)
    {
        thumbitem = m_itemList.at(x);

        thumbitem->InitCaption(m_showcaption);
        MythUIButtonListItem* item =
            new MythUIButtonListItem(m_imageList, thumbitem->GetCaption(), 0,
                                     true, MythUIButtonListItem::NotChecked);
        item->SetData(qVariantFromValue(thumbitem));
        if (thumbitem->IsDir())
        {
            item->DisplayState("subfolder", "nodetype");
            m_childCountThread->addFile(thumbitem->GetPath());
        }

        LoadThumbnail(thumbitem);

        if (QFile(thumbitem->GetImageFilename()).exists())
            item->SetImage(thumbitem->GetImageFilename());

        if (m_itemMarked.contains(thumbitem->GetPath()))
            item->setChecked(MythUIButtonListItem::FullChecked);
    }

    if (m_childCountThread && !m_childCountThread->isRunning())
                m_childCountThread->start();

    if (m_noImagesText)
        m_noImagesText->SetVisible(m_itemList.isEmpty());

    if (!m_itemList.isEmpty())
    {
        UpdateText(m_imageList->GetItemCurrent());
        UpdateImage(m_imageList->GetItemCurrent());
    }
}

void IconView::LoadThumbnail(ThumbItem *item)
{
    if (!item)
        return;

    bool canLoadGallery = m_isGallery;

    QString imagePath;
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
                    imagePath = it->absoluteFilePath();
                }
            }
        }
        else
        {
            QString fn = item->GetName();
            int firstDot = fn.indexOf('.');
            if (firstDot > 0)
            {
                fn.insert(firstDot, ".thumb");
                imagePath = QString("%1/%2").arg(m_currDir).arg(fn);
            }
        }

        canLoadGallery = !(QFile(imagePath).exists());
    }

    if (!canLoadGallery)
        imagePath = QString("%1%2.jpg")
                            .arg(ThumbGenerator::getThumbcacheDir(m_currDir))
                            .arg(item->GetName());

    item->SetImageFilename(imagePath);
}

void IconView::SetupMediaMonitor(void)
{
#ifdef _WIN32
    if (m_currDevice)
        LoadDirectory(m_currDevice->getDevicePath());
#else
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (m_currDevice && mon && mon->ValidateAndLock(m_currDevice))
    {
        bool mounted = m_currDevice->isMounted();
        if (!mounted)
            mounted = m_currDevice->mount();

        if (mounted)
        {
            connect(m_currDevice,
                SIGNAL(statusChanged(MythMediaStatus, MythMediaDevice*)),
                SLOT(mediaStatusChanged(MythMediaStatus, MythMediaDevice*)));

            LoadDirectory(m_currDevice->getMountPath());

            mon->Unlock(m_currDevice);
            return;
        }
        else
        {
//             DialogBox *dlg = new DialogBox(GetMythMainWindow(),
//                              tr("Failed to mount device: ") +
//                              m_currDevice->getDevicePath() + "\n\n" +
//                              tr("Showing the default MythGallery directory."));
//             dlg->AddButton(tr("OK"));
//             dlg->exec();
//             dlg->deleteLater();
            mon->Unlock(m_currDevice);
        }
    }
#endif // _WIN32
}

void IconView::UpdateText(MythUIButtonListItem *item)
{
    if (!item)
    {
        if (m_positionText)
            m_positionText->Reset();
        return;
    }

    if (m_positionText)
        m_positionText->SetText(tr("%1 of %2")
                                .arg(m_imageList->GetCurrentPos() + 1)
                                .arg(m_imageList->GetCount()));

    ThumbItem *thumbitem = qVariantValue<ThumbItem *>(item->GetData());
    if (!thumbitem)
        return;

    if (m_crumbsText)
    {
        QString path = thumbitem->GetPath();
        path.replace(m_galleryDir, tr("Gallery Home"));
        path.replace("/", " > ");
        m_crumbsText->SetText(path);
    }

    if (m_captionText)
    {
        QString caption;
        caption = thumbitem->GetCaption();
        caption = (caption.isNull()) ? "" : caption;
        m_captionText->SetText(caption);
    }
}

void IconView::UpdateImage(MythUIButtonListItem *item)
{
    if (!m_selectedImage)
        return;

    ThumbItem *thumbitem = qVariantValue<ThumbItem *>(item->GetData());

    QString selectedimage;
    if (thumbitem)
    {
        selectedimage = thumbitem->GetImageFilename();
        selectedimage = (selectedimage.isNull()) ? "" : selectedimage;
    }
    m_selectedImage->SetFilename(selectedimage);
    m_selectedImage->Load();
}


bool IconView::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Gallery", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (!m_itemList.isEmpty())
        {
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
            else if (action == "EDIT")
                HandleRename();
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
                        m_itemMarked.removeAll(thumbitem->GetPath());
                        item->setChecked(MythUIButtonListItem::NotChecked);
                    }
                }
            }
            else if (action == "SLIDESHOW")
                HandleSlideShow();
            else if (action == "RANDOMSHOW")
                HandleRandomShow();
            else
                handled = false;
        }

        if (action == "ESCAPE")
        {
            if (GetMythMainWindow()->IsExitingToMain())
            {
                while ( m_currDir != m_galleryDir &&
                        HandleSubDirEscape(m_galleryDir) );
            }
            handled = HandleEscape();
        }
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

#ifdef _WIN32
        LoadDirectory(m_currDevice->getDevicePath());
#else
        if (!m_currDevice->isMounted(false))
            m_currDevice->mount();

        item->SetPath(m_currDevice->getMountPath(), true);

        connect(m_currDevice,
                SIGNAL(statusChanged(MythMediaStatus,
                                     MythMediaDevice*)),
                SLOT(mediaStatusChanged(MythMediaStatus,
                                        MythMediaDevice*)));

        LoadDirectory(m_currDevice->getMountPath());
#endif

        mon->Unlock(m_currDevice);
    }
    else
    {
        // device was removed
        QString msg = tr("Error") + '\n' +
                tr("The selected device is no longer available");
        ShowOkPopup(msg, this, SLOT(HandleShowDevices()));
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
                        GetMythMainWindow());
        gv.exec();
    }
    else
#endif
    {
        SingleView sv(m_itemList, &pos, slideShow, m_sortorder,
                      GetMythMainWindow());
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
    QList<MythMediaDevice*> removables = mon->GetMedias(MEDIATYPE_DATA);
    QList<MythMediaDevice*>::iterator it = removables.begin();
    for (; !handled && (it != removables.end()); ++it)
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
                int pos = m_itemList.indexOf(item);
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
    QString pstr = QDir::cleanPath(parent.path());
    QString cstr = QDir::cleanPath(subdir.path());
    bool ret = !cstr.indexOf(pstr);

    return ret;
}

bool IconView::HandleSubDirEscape(const QString &parent)
{
    bool handled = false;

    QDir curdir(m_currDir);
    QDir pdir(parent);
    if ((curdir != pdir) && is_subdir(pdir, curdir) && !m_history.empty())
    {
        QString oldDirName = curdir.dirName();
        curdir.cdUp();
        LoadDirectory(curdir.absolutePath());

        int pos = m_history.back();
        m_history.pop_back();
        m_imageList->SetItemCurrent(pos);
        handled = true;
    }

    return handled;
}

bool IconView::HandleEscape(void)
{
#if 0
    LOG(VB_GENERAL, LOG_INFO, LOC + "HandleEscape() " +
        QString("showDevices: %1").arg(m_showDevices));
#endif

    bool handled = false;

    // If we are showing the attached devices, ESCAPE should always exit..
    if (m_showDevices)
    {
#if 0
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "HandleEscape() exiting on showDevices");
#endif
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

#if 0
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("HandleEscape() handled: %1")
            .arg(handled));
#endif

    return handled;
}

void IconView::customEvent(QEvent *event)
{
    if (event->type() == ThumbGenEvent::kEventType)
    {
        ThumbGenEvent *tge = dynamic_cast<ThumbGenEvent *>(event);

        if (!tge)
            return;

        ThumbData *td = tge->thumbData;
        if (!td)
            return;

        ThumbItem *thumbitem = m_itemHash.value(td->fileName);
        if (thumbitem)
        {
            int rotateAngle = thumbitem->GetRotationAngle();

            if (rotateAngle)
            {
                QMatrix matrix;
                matrix.rotate(rotateAngle);
                td->thumb = td->thumb.transformed(
                    matrix, Qt::SmoothTransformation);
            }

            int pos = m_itemList.indexOf(thumbitem);

            LoadThumbnail(thumbitem);

            MythUIButtonListItem *item = m_imageList->GetItemAt(pos);
            if (QFile(thumbitem->GetImageFilename()).exists())
                item->SetImage(thumbitem->GetImageFilename());

            if (m_imageList->GetCurrentPos() == pos)
                UpdateImage(item);
        }
        delete td;
    }
    else if (event->type() == ChildCountEvent::kEventType)
    {
        ChildCountEvent *cce = dynamic_cast<ChildCountEvent *>(event);

        if (!cce)
            return;

        ChildCountData *ccd = cce->childCountData;
        if (!ccd)
            return;

        ThumbItem *thumbitem = m_itemHash.value(ccd->fileName);
        if (thumbitem)
        {
            int pos = m_itemList.indexOf(thumbitem);
            MythUIButtonListItem *item = m_imageList->GetItemAt(pos);
            if (item)
                item->SetText(QString("%1").arg(ccd->count), "childcount");
        }
        delete ccd;
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

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
                    break;
                case 3:
                    break;
                case 4:
                    HandleSubMenuFilter();
                    break;
                case 5:
                    break;
                case 6:
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
                    HandleSelectOne();
                    break;
                case 1:
                    HandleClearOneMarked();
                    break;
                case 2:
                    HandleSelectAll();
                    break;
                case 3:
                    HandleClearMarked();
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
                    HandleEject();
                    break;
                case 2:
                    HandleImport();
                    break;
                case 3:
                    HandleCopyHere();
                    break;
                case 4:
                    HandleMoveHere();
                    break;
                case 5:
                    HandleDelete();
                    break;
                case 6:
                    HandleMkDir();
                    break;
                case 7:
                    HandleRename();
                    break;
            }
        }

        m_menuPopup = NULL;

    }

}

void IconView::reloadData()
{
    LoadDirectory(m_galleryDir);
}

void IconView::HandleMainMenu(void)
{
    QString label = tr("Gallery Options");

    MythMenu *menu = new MythMenu(label, this, "mainmenu");

    menu->AddItem(tr("SlideShow"));
    menu->AddItem(tr("Random"));
    menu->AddItem(tr("Meta Data Options"), NULL, CreateMetadataMenu());
    menu->AddItem(tr("Marking Options"), NULL, CreateMarkingMenu());
    menu->AddItem(tr("Filter / Sort..."));
    menu->AddItem(tr("File Options"), NULL, CreateFileMenu());
    menu->AddItem(tr("Settings..."));
//     if (m_showDevices)
//     {
//         QDir d(m_currDir);
//         if (!d.exists())
//             m_currDir = m_galleryDir;
//
//         LoadDirectory(m_currDir);
//         m_showDevices = false;
//     }

    m_menuPopup = new MythDialogBox(menu, m_popupStack, "mythgallerymenupopup");

    if (!m_menuPopup->Create())
    {
        delete m_menuPopup;
        m_menuPopup = NULL;
        return;
    }

    m_popupStack->AddScreen(m_menuPopup);
}

MythMenu* IconView::CreateMetadataMenu(void)
{
    QString label = tr("Metadata Options");

    MythMenu *menu = new MythMenu(label, this, "metadatamenu");

    menu->AddItem(tr("Rotate CW"));
    menu->AddItem(tr("Rotate CCW"));

    return menu;
}

MythMenu* IconView::CreateMarkingMenu(void)
{
    QString label = tr("Marking Options");

    MythMenu *menu = new MythMenu(label, this, "markingmenu");

    menu->AddItem(tr("Select One"));
    menu->AddItem(tr("Clear One Marked"));
    menu->AddItem(tr("Select All"));
    menu->AddItem(tr("Clear Marked"));

    return menu;
}

void IconView::HandleSubMenuFilter(void)
{
    MythScreenStack *mainStack = GetScreenStack();

    GalleryFilterDialog *filterdialog =
        new GalleryFilterDialog(mainStack, "galleryfilter", m_galleryFilter);

    if (filterdialog->Create())
        mainStack->AddScreen(filterdialog);

    connect(filterdialog, SIGNAL(filterChanged()), SLOT(reloadData()));
}

MythMenu* IconView::CreateFileMenu(void)
{
    QString label = tr("File Options");

    MythMenu *menu = new MythMenu(label, this, "filemenu");

    menu->AddItem(tr("Show Devices"));
    menu->AddItem(tr("Eject"));
    menu->AddItem(tr("Import"));
    menu->AddItem(tr("Copy here"));
    menu->AddItem(tr("Move here"));
    menu->AddItem(tr("Delete"));
    menu->AddItem(tr("Create folder"));
    menu->AddItem(tr("Rename"));

    return menu;
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

    if (!thumbitem)
        return;

    QString title = tr("Delete Current File or Folder");
    QString msg = (thumbitem->IsDir()) ?
        tr("Deleting 1 folder, including any subfolders and files.") :
        tr("Deleting 1 image.");

    ShowOkPopup(title + '\n' + msg, this, SLOT(DoDeleteCurrent(bool)), true);
}

void IconView::DoDeleteCurrent(bool doDelete)
{
    if (doDelete)
    {
        ThumbItem *thumbitem = GetCurrentThumb();

        int currPos = 0;
        MythUIButtonListItem *item = m_imageList->GetItemCurrent();
        if (item)
            currPos = m_imageList->GetCurrentPos();

        if (!thumbitem)
            return;

        QFileInfo fi;
        fi.setFile(thumbitem->GetPath());
        GalleryUtil::Delete(fi);

        LoadDirectory(m_currDir);

        m_imageList->SetItemCurrent(currPos);
    }
}

void IconView::HandleSettings(void)
{
    GallerySettings settings;
    settings.exec();
    gCoreContext->ClearSettingsCache();

    // reload settings
    m_showcaption = gCoreContext->GetNumSetting("GalleryOverlayCaption", 0);
    m_sortorder   = gCoreContext->GetNumSetting("GallerySortOrder", 0);
    m_useOpenGL   = gCoreContext->GetNumSetting("SlideshowUseOpenGL", 0);
    m_recurse     = gCoreContext->GetNumSetting("GalleryRecursiveSlideshow", 0);
    m_paths       = gCoreContext->GetSetting("GalleryImportDirs").split(":");

    // reload directory
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (m_currDevice && mon && mon->ValidateAndLock(m_currDevice))
    {
#ifdef _WIN32
        LoadDirectory(m_currDevice->getDevicePath());
#else
        LoadDirectory(m_currDevice->getMountPath());
#endif
        mon->Unlock(m_currDevice);
    }
    else
    {
        m_currDevice = NULL;
        LoadDirectory(m_galleryDir);
    }

    SetFocusWidget(m_imageList);
}

void IconView::HandleEject(void)
{
    MediaMonitor::ejectOpticalDisc();
}

void IconView::HandleImport(void)
{
    QFileInfo path;
    QDir importdir;

#if 0
    DialogBox *importDlg = new DialogBox(GetMythMainWindow(),
                                         tr("Import pictures?"));

    importDlg->AddButton(tr("No"));
    importDlg->AddButton(tr("Yes"));
    DialogCode code = importDlg->exec();
    importDlg->deleteLater();
    if (kDialogCodeButton1 != code)
        return;
#endif

    // Makes import directory samba/windows friendly (no colon)
    QString idirname = m_currDir + "/" +
        MythDate::current().toString("yyyy-MM-dd_hh-mm-ss");

    importdir.mkdir(idirname);
    importdir.setPath(idirname);

    for (QStringList::const_iterator it = m_paths.begin();
         it != m_paths.end(); ++it)
    {
        path.setFile(*it);
        if (path.isDir() && path.isReadable())
        {
            ImportFromDir(*it, importdir.absolutePath());
        }
#if 0
        else if (path.isFile() && path.isExecutable())
        {
            // TODO this should not be enabled by default!!!
            QString cmd = *it + " " + importdir.absolutePath();
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Executing %1").arg(cmd));
            myth_system(cmd);
        }
#endif
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Could not read or execute %1").arg(*it));
        }
    }

    importdir.refresh();
    if (importdir.count() == 0)
    {
#if 0
        DialogBox *nopicsDlg = new DialogBox(GetMythMainWindow(),
                                             tr("Nothing found to import"));

        nopicsDlg->AddButton(tr("OK"));
        nopicsDlg->exec();
        nopicsDlg->deleteLater();
#endif

        return;
    }

    LoadDirectory(m_currDir);
}

void IconView::HandleShowDevices(void)
{
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
#ifndef _WIN32
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

    while (!m_itemList.isEmpty())
        delete m_itemList.takeFirst();

    m_itemHash.clear();
    m_imageList->Reset();

    m_thumbGen->cancel();
    m_childCountThread->cancel();

    // add gallery directory
    ThumbItem *item = new ThumbItem("Gallery", m_galleryDir, true);
    m_itemList.append(item);
    m_itemHash.insert(item->GetName(), item);

    if (mon)
    {
        MythMediaType type = MythMediaType(MEDIATYPE_DATA | MEDIATYPE_MGALLERY);
        QList<MythMediaDevice*> removables = mon->GetMedias(type);
        QList<MythMediaDevice*>::Iterator it = removables.begin();
        for (; it != removables.end(); ++it)
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

    ThumbItem *thumbitem;
    for (int x = 0; x < m_itemList.size(); x++)
    {
        thumbitem = m_itemList.at(x);

        thumbitem->InitCaption(m_showcaption);
        MythUIButtonListItem* item =
            new MythUIButtonListItem(m_imageList, thumbitem->GetCaption(), 0,
                                     true, MythUIButtonListItem::NotChecked);
        item->SetData(qVariantFromValue(thumbitem));
    }

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
    ShowOkPopup(msg, this, SLOT(DoDeleteMarked(bool)), true);
}

void IconView::DoDeleteMarked(bool doDelete)
{
    if (doDelete)
    {
        QStringList::iterator it;
        QFileInfo fi;

        for (it = m_itemMarked.begin(); it != m_itemMarked.end(); ++it)
        {
            fi.setFile(*it);

            GalleryUtil::Delete(fi);
        }

        m_itemMarked.clear();

        LoadDirectory(m_currDir);
    }
}

void IconView::HandleClearOneMarked(void)
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;
    item->setChecked(MythUIButtonListItem::NotChecked);
}

void IconView::HandleSelectOne(void)
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;
    item->setChecked(MythUIButtonListItem::FullChecked);
}

void IconView::HandleClearMarked(void)
{
    m_itemMarked.clear();
    m_imageList->SetAllChecked(MythUIButtonListItem::NotChecked);
}

void IconView::HandleSelectAll(void)
{
    ThumbItem *item;
    for (int x = 0; x < m_itemList.size(); x++)
    {
        item = m_itemList.at(x);

        if (!m_itemMarked.contains(item->GetPath()))
            m_itemMarked.append(item->GetPath());
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

    int currPos = 0;
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (item)
    {
        currPos = m_imageList->GetCurrentPos() + 1;

        if (currPos >= m_imageList->GetCount())
            currPos = m_imageList->GetCount() - 1;
    }

    if (!thumbitem)
        return;

    if (!GalleryUtil::Rename(m_currDir, thumbitem->GetName(), folderName))
    {
        QString msg;
        if (thumbitem->IsDir())
            msg = tr("Failed to rename folder");
        else
            msg = tr("Failed to rename file");

        ShowOkPopup(msg, NULL, NULL);

        return;
    }

    LoadDirectory(m_currDir);

    m_imageList->SetItemCurrent(currPos);
}

void IconView::ImportFromDir(const QString &fromDir, const QString &toDir)
{
    QDir d(fromDir);

    if (!d.exists())
        return;

    d.setNameFilters(GalleryUtil::GetMediaFilter());
    d.setSorting((QDir::SortFlag)m_sortorder);
    d.setFilter(QDir::Files       | QDir::AllDirs |
                QDir::NoSymLinks  | QDir::Readable |
                QDir::NoDotAndDotDot);
    QFileInfoList list = d.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;

        if (fi->isDir())
        {
            QString newdir(toDir + "/" + fi->fileName());
            d.mkdir(newdir);
            ImportFromDir(fi->absoluteFilePath(), newdir);
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Copying %1 to %2")
                    .arg(fi->absoluteFilePath())
                    .arg(toDir));

            // TODO FIXME, we shouldn't need a myth_system call here
            QString cmd = QString("cp \"%1\" \"%2\"")
                .arg(fi->absoluteFilePath()).arg(toDir);
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

void IconView::mediaStatusChanged(MythMediaStatus oldStatus,
                                  MythMediaDevice *pMedia)
{
    (void) oldStatus;
    if (m_currDevice == pMedia)
    {
        HandleShowDevices();

        // UpdateText();
    }
}

ThumbItem *IconView::GetCurrentThumb(void)
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (item)
        return qVariantValue<ThumbItem *>(item->GetData());
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////

ChildCountThread::ChildCountThread(QObject *parent) :
    MThread("ChildCountThread"), m_parent(parent)
{
}

ChildCountThread::~ChildCountThread()
{
    cancel();
    wait();
}

void ChildCountThread::addFile(const QString& filePath)
{
    m_mutex.lock();
    m_fileList.append(filePath);
    m_mutex.unlock();
}

void ChildCountThread::cancel()
{
    m_mutex.lock();
    m_fileList.clear();
    m_mutex.unlock();
}

void ChildCountThread::run()
{
    RunProlog();

    while (moreWork())
    {
        QString file;

        m_mutex.lock();
        file = m_fileList.first();
        if (!m_fileList.isEmpty())
            m_fileList.pop_front();
        m_mutex.unlock();

        if (file.isEmpty())
            continue;
        int count = getChildCount(file);

        ChildCountData *ccd = new ChildCountData;
        ccd->fileName  = file.section('/', -1);
        ccd->count     = count;

        // inform parent we have got a count ready for it
        QApplication::postEvent(m_parent, new ChildCountEvent(ccd));
    }

    RunEpilog();
}

bool ChildCountThread::moreWork()
{
    bool result;
    m_mutex.lock();
    result = !m_fileList.isEmpty();
    m_mutex.unlock();
    return result;
}

int ChildCountThread::getChildCount(const QString &filepath)
{
    QDir d(filepath);

    bool isGallery;
    QFileInfoList gList = d.entryInfoList(QStringList("serial*.dat"),
                                          QDir::Files);
    isGallery = (gList.count() != 0);

    QFileInfoList list = d.entryInfoList(GalleryUtil::GetMediaFilter(),
                                         QDir::Files | QDir::AllDirs |
                                         QDir::NoDotAndDotDot);

    if (list.isEmpty())
        return 0;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    int count = 0;
    while (it != list.end())
    {
        fi = &(*it);
        ++it;

        // remove these already-resized pictures.
        if (isGallery && (
                (fi->fileName().indexOf(".thumb.") > 0) ||
                (fi->fileName().indexOf(".sized.") > 0) ||
                (fi->fileName().indexOf(".highlight.") > 0)))
            continue;

        count++;
    }

    return count;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
