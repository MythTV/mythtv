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

#include <cmath>

#include <algorithm>
using namespace std;

#include <qevent.h>
#include <qimage.h>
#include <qdir.h>

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>
#include <mythtv/dialogbox.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>

#ifndef _WIN32
#include <mythtv/mythmediamonitor.h>
#endif

#include "galleryutil.h"
#include "gallerysettings.h"
#include "thumbgenerator.h"
#include "singleview.h"
#include "iconview.h"

#include "config.h"
#include "constants.h"

#ifdef OPENGL_SUPPORT
#include "glsingleview.h"
#endif

#define LOC QString("IconView: ")
#define LOC_ERR QString("IconView, Error: ")

int ThumbItem::GetRotationAngle()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT angle FROM gallerymetadata WHERE "
                  "image = :PATH ;");
    query.bindValue(":PATH", path.utf8());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toInt();
    }

    return GalleryUtil::getNaturalRotation(path);
}

void ThumbItem::SetRotationAngle(int angle)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("REPLACE INTO gallerymetadata SET image = :IMAGE , "
                  "angle = :ANGLE ;");
    query.bindValue(":IMAGE", path.utf8());
    query.bindValue(":ANGLE", angle);
    query.exec();
}

bool ThumbItem::Remove(void)
{
    if( QFile::exists(path) && QFile::remove(path) ) {
	    MSqlQuery query(MSqlQuery::InitCon());
	    query.prepare("DELETE FROM gallerymetadata WHERE image = :PATH ;");
	    query.bindValue(":PATH", path.utf8());
	    query.exec();
        return true;
    }
    return false;
}

IconView::IconView(const QString& galleryDir, MythMediaDevice *initialDevice,
                   MythMainWindow* parent, const char* name )
    : MythDialog(parent, name)
{
    m_galleryDir = galleryDir;    

    m_inMenu     = false;
    m_inSubMenu  = false;
    m_itemList.setAutoDelete(true);
    m_itemDict.setAutoDelete(false);

    setNoErase();
    loadTheme();

    m_thumbGen = new ThumbGenerator(this, (int)(m_thumbW-10*wmult),
                                    (int)(m_thumbH-10*hmult));

    m_showcaption = gContext->GetNumSetting("GalleryOverlayCaption", 0);

    m_currRow = 0;
    m_currCol = 0;
    m_lastRow = 0;
    m_lastCol = 0;
    m_topRow  = 0;
    m_isGallery   = false;
    m_showDevices = false;
    m_currDevice  = initialDevice;

    srand(time(NULL));

    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (m_currDevice && mon && mon->ValidateAndLock(m_currDevice))
    {
        if (!m_currDevice->isMounted(true))
            m_currDevice->mount();

        connect(m_currDevice,
                SIGNAL(statusChanged(MediaStatus, MythMediaDevice*)),
                SLOT(mediaStatusChanged(MediaStatus, MythMediaDevice*)));

        loadDirectory(m_currDevice->getMountPath());

        mon->Unlock(m_currDevice);
    }
    else
    {
        m_currDevice = NULL;
        loadDirectory(galleryDir);
    }
}

IconView::~IconView()
{
    clearMenu(m_submenuType);
    clearMenu(m_menuType);
    
    delete m_thumbGen;
    delete m_theme;
}

void IconView::paintEvent(QPaintEvent *e)
{
    if (e->rect().intersects(m_menuRect))
        updateMenu();
    if (e->rect().intersects(m_textRect))
        updateText();
    if (e->rect().intersects(m_viewRect))
        updateView();
}

void IconView::updateMenu()
{
    QPixmap pix(m_menuRect.size());
    pix.fill(this, m_menuRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_theme->GetSet("menu");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, m_menuRect.left(), m_menuRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void IconView::updateText()
{
    QPixmap pix(m_textRect.size());
    pix.fill(this, m_textRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_theme->GetSet("text");
    if (container) {
        UITextType *ttype = (UITextType*)container->GetType("text");
        if (ttype) {
            ThumbItem* item = m_itemList.at(m_currRow * m_nCols +
                                            m_currCol);

            if (item)
            {
                if(item->caption == "" && m_showcaption)
                    item->caption = GalleryUtil::getCaption(item->path);
                if(item->caption == "")
                    item->caption = item->name;

                ttype->SetText(item->caption);
            }
            else
                ttype->SetText(QString(""));
        }
        
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, m_textRect.left(), m_textRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void IconView::updateView()
{
    QPixmap pix(m_viewRect.size());
    pix.fill(this, m_viewRect.topLeft());
    QPainter p(&pix);
    p.setPen(Qt::white);

    LayerSet* container = m_theme->GetSet("view");
    if (container) {

        int upArrow = (m_topRow == 0) ? 0 : 1;
        int dnArrow = (m_currRow == m_lastRow) ? 0 : 1;

        container->Draw(&p, 0, upArrow);
        container->Draw(&p, 1, dnArrow);
    }

    int bw  = m_backRegPix.width();
    int bh  = m_backRegPix.height();
    int bw2 = m_backRegPix.width()/2;
    int bh2 = m_backRegPix.height()/2;
    int sw  = (int)(7*wmult);
    int sh  = (int)(7*hmult);

    int curPos = m_topRow*m_nCols;

    for (int y = 0; y < m_nRows; y++)    {

        int ypos = m_spaceH * (y + 1) + m_thumbH * y;

        for (int x = 0; x < m_nCols; x++)
        {
            if (curPos >= (int)m_itemList.count())
                continue;

            ThumbItem* item = m_itemList.at(curPos);
            if (!item->pixmap)
                loadThumbnail(item);

            int xpos = m_spaceW * (x + 1) + m_thumbW * x;

            if (item->isDir) {

                if (curPos == (m_currRow*m_nCols+m_currCol))
                    p.drawPixmap(xpos, ypos , m_folderSelPix);
                else
                    p.drawPixmap(xpos, ypos , m_folderRegPix);

                if (item->pixmap) 
                    p.drawPixmap(xpos + sw, ypos + sh + (int)(15*hmult),
                                 *item->pixmap,
                                 item->pixmap->width()/2-bw2+sw,
                                 item->pixmap->height()/2-bh2+sh,
                                 bw-2*sw, bh-2*sh-(int)(15*hmult));

                if (m_itemMarked.contains(item->path))
                    p.drawPixmap(xpos, ypos, m_MrkPix);

            }
            else {

                if (curPos == (m_currRow*m_nCols+m_currCol))
                    p.drawPixmap(xpos, ypos , m_backSelPix);
                else
                    p.drawPixmap(xpos, ypos , m_backRegPix);
               
                if (item->pixmap) 
                    p.drawPixmap(xpos + sw, ypos + sh,
                                 *item->pixmap,
                                 item->pixmap->width()/2-bw2+sw,
                                 item->pixmap->height()/2-bh2+sh,
                                 bw-2*sw, bh-2*sh);

                if (m_itemMarked.contains(item->path))
                    p.drawPixmap(xpos, ypos, m_MrkPix);
            }
           
            curPos++;
        }
    }

    p.end();
    
    bitBlt(this, m_viewRect.left(), m_viewRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

static bool has_action(QString action, const QStringList &actions)
{
    QStringList::const_iterator it;
    for (it = actions.begin(); it != actions.end(); ++it)
    {
        if (action == *it)
            return true;
    }
    return false;
}

void IconView::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    bool menuHandled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Gallery", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled && !menuHandled; i++)
    {
        QString action = actions[i];
        if (action == "MENU")
        {
            m_inMenu = !m_inMenu;
            m_menuType->SetActive(m_inMenu & !m_inSubMenu);
            m_submenuType->SetActive(m_inMenu & m_inSubMenu);
            menuHandled = true;
        }
        else if (action == "ESCAPE")
        {
            if (m_inMenu & m_inSubMenu)
            {
                actionMainMenu();
                m_menuType->SetActive(m_inMenu & !m_inSubMenu);
                m_submenuType->SetActive(m_inMenu & m_inSubMenu);
                menuHandled = true;
            }
        }
        else if (action == "UP")
        {
            if (m_inMenu & !m_inSubMenu)
            {
                m_menuType->MoveUp();
                menuHandled = true;
            }
            else if (m_inMenu & m_inSubMenu)
            {
                m_submenuType->MoveUp();
                menuHandled = true;
            }
            else
            {
                handled = moveUp();
            }
        }
        else if (action == "DOWN")
        {
            if (m_inMenu & !m_inSubMenu)
            {
                m_menuType->MoveDown();
                menuHandled = true;
            }
            else if (m_inMenu & m_inSubMenu)
            {
                m_submenuType->MoveDown();
                menuHandled = true;
            }
            else
            {
                handled = moveDown();
            }
        }
        else if (action == "LEFT")
        {
            handled = moveLeft();
        }
        else if (action == "RIGHT")
        {
            handled = moveRight();
        }
        else if (action == "PAGEUP")
        {
            bool h = true;
            for (int i = 0; i < m_nRows && h; i++) 
                h = moveUp();
            handled = true;
        }
        else if (action == "PAGEDOWN")
        {
            bool h = true;
            for (int i = 0; i < m_nRows && h; i++)
                h = moveDown();
            handled = true;
        }
        else if (action == "HOME")
        {
            m_topRow = m_currRow = m_currCol = 0;
            handled = true;
        }
        else if (action == "END")
        {
            m_currRow = m_lastRow;
            m_currCol = m_lastCol;
            m_topRow  = QMAX(m_currRow-(m_nRows-1),0);
            handled = true;
        }
        else if (action == "ROTRIGHT") 
        {
            actionRotateCW();
            handled = true;
        }
        else if (action == "ROTLEFT") 
        {
            actionRotateCCW();
            handled = true;
        }
        else if (action == "DELETE")
        {
            actionDelete();
            handled = true;
        }
        else if (action == "MARK")
        {
            int pos = m_currRow * m_nCols + m_currCol;
            ThumbItem *item = m_itemList.at(pos);
            if (!item)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "The impossible happened");
                break;
            }

            if (!m_itemMarked.contains(item->path))
                m_itemMarked.append(item->path);
            else
                m_itemMarked.remove(item->path);

            handled = true;
        }
        else if (action == "SELECT" || action == "PLAY" ||
                 action == "SLIDESHOW" || action == "RANDOMSHOW" )
        {
            if (m_inMenu && (action == "SELECT" || action == "PLAY"))
            {
                pressMenu();
                menuHandled = true;
            }
            else {
                int pos = m_currRow * m_nCols + m_currCol;
                ThumbItem *item = m_itemList.at(pos);
                if (!item)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR + "The impossible happened");
                    break;
                }
        
                QFileInfo fi(item->path);

                // if the selected item is a Media Device
                // attempt to mount it if it isn't already
                if (item->mediaDevice && (action == "SELECT" ||
                                          action == "PLAY"))
                {
                    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
                    if (mon && mon->ValidateAndLock(item->mediaDevice))
                    {
                        m_currDevice = item->mediaDevice;

                        if (!m_currDevice->isMounted())
                            m_currDevice->mount();

                        item->path = m_currDevice->getMountPath();

                        connect(m_currDevice,
                                SIGNAL(statusChanged(MediaStatus,
                                                     MythMediaDevice*)),
                                SLOT(mediaStatusChanged(MediaStatus,
                                                        MythMediaDevice*)));

                        mon->Unlock(m_currDevice);
                    }
                    else
                    {
                        // device was removed
                        MythPopupBox::showOkPopup(
                            gContext->GetMainWindow(),
                            tr("Error"),
                            tr("The selected device is no longer available"));

                        actionShowDevices();
                        m_currRow = 0;
                        m_currCol = 0;
                        handled = true;
                        break;
                    }
                }

                if (!handled && item->isDir &&
                    (action == "SELECT" || action == "PLAY") )
                {
                    loadDirectory(item->path);
                    handled = true;
                }
                else
                {
                    handled = true;
                    
                    int slideShow = 0;
                    if( action == "PLAY" || action == "SLIDESHOW" )
                    {
                        slideShow = 1;
                    }
                    else if( action == "RANDOMSHOW" )
                    {
                        slideShow = 2;
                    }

#ifdef OPENGL_SUPPORT
                    int useOpenGL =
                        gContext->GetNumSetting("SlideshowUseOpenGL");
                    if (useOpenGL)
                    {

                        if (QGLFormat::hasOpenGL())
                        {
                            GLSDialog gv(m_itemList, pos, slideShow,
                                         gContext->GetMainWindow());
                            gv.exec();
                        }
                        else
                        {
                            MythPopupBox::showOkPopup(
                                gContext->GetMainWindow(),
                                tr("Error"),
                                tr("Sorry: OpenGL support not available"));
                        }
                    }
                    else 
#endif
                    {
                        SingleView sv(m_itemList, pos, slideShow,
                                      gContext->GetMainWindow());
                        sv.exec();
                    }                         

                    // if the user deleted files while in single view mode
                    // the cached contents of the directory will be out of
                    // sync, reload the current directory to refresh the view
                    loadDirectory(m_currDir);
                }
            }
        }
        
    }
    
    if (!handled && !menuHandled)
    {
        gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);
        if (has_action("ESCAPE", actions))
            handled = HandleEscape();
    }

    if (handled || menuHandled)
        update();
    else
        MythDialog::keyPressEvent(e);
}

bool IconView::HandleEscape(void)
{
    //VERBOSE(VB_IMPORTANT, "ESCAPE: showDevices("<<m_showDevices<<")");

    if (m_showDevices)
        return false;

    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (!mon)
        return false;

    bool handled = false;
    QDir curdir(m_currDir);
    QValueList<MythMediaDevice*> removables = mon->GetMedias(MEDIATYPE_DATA);
    QValueList<MythMediaDevice*>::iterator it = removables.begin();
    for (; !handled && (it != removables.end()); it++)
    {
        if (!mon->ValidateAndLock(*it))
            continue;

        if (curdir == QDir((*it)->getMountPath()))
        {
            actionShowDevices();

            // Make sure previous devices are visible and selected
            ThumbItem *item = NULL;
            if (!(*it)->getVolumeID().isEmpty())
                item = m_itemDict.find((*it)->getVolumeID());
            else
                item = m_itemDict.find((*it)->getDevicePath());

            if (item)
            {
                int pos = m_itemList.find(item);
                if (pos != -1)
                {
                    m_currRow = pos / m_nCols;
                    m_currCol = pos - (m_currRow * m_nCols);
                    m_topRow  = max(0, m_currRow + 1 - m_nRows);
                }
            }

            handled = true;
        }

        mon->Unlock(*it);
    }

    if (!handled && (curdir != QDir(m_galleryDir)))
    {
        QString oldDirName = curdir.dirName();
        curdir.cdUp();
        loadDirectory(curdir.absPath());

        // Make sure up-directory is visible and selected
        ThumbItem *item = m_itemDict.find(oldDirName);
        if (item)
        {
            int pos = m_itemList.find(item);
            if (pos != -1)
            {
                m_currRow = pos / m_nCols;
                m_currCol = pos - (m_currRow * m_nCols);
                m_topRow  = max(0, m_currRow + 1 - m_nRows);
            }
        }

        handled = true;
    }

    return handled;
}

void IconView::customEvent(QCustomEvent *e)
{
    if (!e || (e->type() != QEvent::User))
        return;

    ThumbData* td = (ThumbData*)(e->data());
    if (!td) return;

    ThumbItem* item = m_itemDict.find(td->fileName);
    if (item) {
        if (item->pixmap)
            delete item->pixmap;
        item->pixmap = 0;

        int rotateAngle = item->GetRotationAngle();

        if (rotateAngle)
        {
            QWMatrix matrix;
            matrix.rotate(rotateAngle);
            td->thumb = td->thumb.xForm(matrix);
        }


        int pos = m_itemList.find(item);
        
        if ((m_topRow*m_nCols <= pos) &&
            (pos <= (m_topRow*m_nCols + m_nRows*m_nCols)))
            update(m_viewRect);
        
    }
    delete td;

}

void IconView::loadTheme()
{
    m_theme = new XMLParse();
    m_theme->SetWMult(wmult);
    m_theme->SetHMult(hmult);

    QDomElement xmldata;
    m_theme->LoadTheme(xmldata, "gallery", "gallery-");

    for (QDomNode child = xmldata.firstChild(); !child.isNull();
         child = child.nextSibling()) {
        
        QDomElement e = child.toElement();
        if (!e.isNull()) {

            if (e.tagName() == "font") {
                m_theme->parseFont(e);
            }
            else if (e.tagName() == "container") {
                QRect area;
                QString name;
                int context;
                m_theme->parseContainer(e, name, context, area);

                if (name.lower() == "menu")
                    m_menuRect = area;
                else if (name.lower() == "text")
                    m_textRect = area;
                else if (name.lower() == "view")
                    m_viewRect = area;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Unknown element: " << e.tagName());
                exit(-1);
            }
        }
    }

    LayerSet *container = m_theme->GetSet("menu");
    if (!container)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get menu container.");
        exit(-1);
    }

    m_menuType = (UIListBtnType*)container->GetType("menu");
    if (!m_menuType)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get menu area.");
        exit(-1);
    }

    m_submenuType = (UIListBtnType*)container->GetType("submenu");
    if (!m_submenuType)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get submenu area.");
        exit(-1);
    }

    // Menu Actions

    UIListBtnTypeItem* item;
    item = new UIListBtnTypeItem(m_menuType, tr("SlideShow"));
    item->setData(new Action(&IconView::actionSlideShow));
    item = new UIListBtnTypeItem(m_menuType, tr("Random"));
    item->setData(new Action(&IconView::actionRandomShow));
    item = new UIListBtnTypeItem(m_menuType, tr("Meta Data..."));
    item->setData(new Action(&IconView::actionSubMenuMetadata));
    item = new UIListBtnTypeItem(m_menuType, tr("Marking..."));
    item->setData(new Action(&IconView::actionSubMenuMark));
    item = new UIListBtnTypeItem(m_menuType, tr("File..."));
    item->setData(new Action(&IconView::actionSubMenuFile));
    item = new UIListBtnTypeItem(m_menuType, tr("Settings"));
    item->setData(new Action(&IconView::actionSettings));
    
    m_menuType->SetActive(false);

    container = m_theme->GetSet("view");
    if (!container)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get view container.");
        exit(-1);
    }

    UIBlackHoleType* bhType = (UIBlackHoleType*)container->GetType("view");
    if (!bhType)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get view area.");
        exit(-1);
    }

    {
        QImage *img = gContext->LoadScaleImage("gallery-back-reg.png");
        if (!img)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to load gallery-back-reg.png");
            exit(-1);
        }
        m_backRegPix = QPixmap(*img);
        delete img;

        img = gContext->LoadScaleImage("gallery-back-sel.png");
        if (!img)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to load gallery-back-sel.png");
            exit(-1);
        }
        m_backSelPix = QPixmap(*img);
        delete img;

        img = gContext->LoadScaleImage("gallery-folder-reg.png");
        if (!img)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to load gallery-folder-reg.png");
            exit(-1);
        }
        m_folderRegPix = QPixmap(*img);
        delete img;

        img = gContext->LoadScaleImage("gallery-folder-sel.png");
        if (!img)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to load gallery-folder-sel.png");
            exit(-1);
        }
        m_folderSelPix = QPixmap(*img);
        delete img;

        img = gContext->LoadScaleImage("gallery-mark.png");
        if (!img)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to load gallery-mark.png");
            exit(-1);
        }
        m_MrkPix = QPixmap(*img);
        delete img;

        m_thumbW = m_backRegPix.width();
        m_thumbH = m_backRegPix.height();
        m_nCols  = m_viewRect.width()/m_thumbW - 1;
        m_nRows  = m_viewRect.height()/m_thumbH - 1;
        m_spaceW = m_thumbW / (m_nCols + 1);
        m_spaceH = m_thumbH / (m_nRows + 1);

    }
}

void IconView::loadDirectory(const QString& dir, bool topleft)
{
    QDir d(dir);
    if (!d.exists())
        return;

    m_showDevices = false;

    m_currDir = d.absPath();
    m_itemList.clear();
    m_itemDict.clear();

    if( topleft ) {
        m_currRow = 0;
        m_currCol = 0;
        m_topRow  = 0;
    }
    m_lastRow = 0;
    m_lastCol = 0;

    m_isGallery = GalleryUtil::loadDirectory(m_itemList, dir, false, &m_itemDict, m_thumbGen);;
    m_lastRow = QMAX((int)ceilf((float)m_itemList.count()/(float)m_nCols)-1,0);
    m_lastCol = QMAX(m_itemList.count()-m_lastRow*m_nCols-1,0);

    if( !topleft ) {
        if( (unsigned int)(m_currRow * m_nCols + m_currCol) > (m_itemList.count()-1) ) {
            m_currRow = (m_itemList.count()-1) / m_nCols;
            m_currCol = (m_itemList.count()-1) % m_nCols;
            if( m_topRow > m_currRow ) {
                m_topRow = m_currRow;
            }
        }
    }
}

void IconView::loadThumbnail(ThumbItem *item)
{
    if (!item)
        return;

    bool canLoadGallery = m_isGallery;
    QImage image;
    
    if (canLoadGallery) {

        if (item->isDir) {
            // try to find a highlight
            QDir subdir(item->path, "*.highlight.*", QDir::Name, 
                        QDir::Files);
            if (subdir.count() > 0) {
                // check if the image format is understood
                QString path(subdir.entryInfoList()->getFirst()->absFilePath());
                image.load(path);
            }
        }
        else {
            QString fn = item->name;
            int firstDot = fn.find('.');
            if (firstDot > 0) {
                fn.insert(firstDot, ".thumb");
                QString galThumbPath(m_currDir + "/" + fn);
                image.load(galThumbPath);
            }
        }

        canLoadGallery = !(image.isNull());
    }

    if (!canLoadGallery) {
        QString cachePath = m_thumbGen->getThumbcacheDir(m_currDir) + item->name;
        image.load(cachePath);
    }

    if (!image.isNull()) {
        image = image.smoothScale((int)(m_thumbW-10*wmult),
                                  (int)(m_thumbW-10*wmult),
                                  QImage::ScaleMax);
        int rotateAngle = 0;

        rotateAngle = item->GetRotationAngle();

        if (rotateAngle != 0)
        {
            QWMatrix matrix;
            matrix.rotate(rotateAngle);
            image = image.xForm(matrix);
        }

        item->pixmap = new QPixmap(image);
    }
}

bool IconView::moveUp()
{
    if (m_currRow == 0)
        return false;

    m_currRow--;
    if (m_currRow < m_topRow)
        m_topRow = m_currRow;

    return true;
}

bool IconView::moveDown()
{
    if (m_currRow == m_lastRow)
        return false;

    m_currRow++;
    if (m_currRow >= m_topRow+m_nRows)
        m_topRow++;
    if (m_currRow == m_lastRow)
        m_currCol = QMIN(m_currCol,m_lastCol);

    return true;
}

bool IconView::moveLeft()
{
    if (m_currRow == 0 && m_currCol == 0)
        return false;

    m_currCol--;
    if (m_currCol < 0) {
        m_currCol = m_nCols - 1;
        m_currRow--;
        if (m_currRow < m_topRow)
            m_topRow = m_currRow;
    }

    return true;
}

bool IconView::moveRight()
{
    if (m_currRow*m_nCols+m_currCol >= (int)m_itemList.count()-1)
        return false;

    m_currCol++;
    if (m_currCol >= m_nCols) {
        m_currCol = 0;
        m_currRow++;
        if (m_currRow >= m_topRow+m_nRows)
            m_topRow++;
    }
    
    return true;
}

void IconView::pressMenu()
{
    UIListBtnTypeItem* item;

    if (m_inSubMenu)
        item = m_submenuType->GetItemCurrent();
    else
        item = m_menuType->GetItemCurrent();

    if (!item || !item->getData())
        return;

    Action *act = (Action*) item->getData();
    (this->*(*act))();

    m_menuType->SetActive(m_inMenu & !m_inSubMenu);
    m_submenuType->SetActive(m_inMenu & m_inSubMenu);
}

void IconView::actionMainMenu()
{
    if (m_showDevices)
    {
        QDir d(m_currDir);
        if (!d.exists())
            m_currDir = m_galleryDir;

        loadDirectory(m_currDir);
        m_showDevices = false;
    }

    clearMenu(m_submenuType);
    m_submenuType->Reset();

    m_inSubMenu = false;
}

void IconView::actionSubMenuMetadata()
{
    clearMenu(m_submenuType);
    m_submenuType->Reset();

    UIListBtnTypeItem *item;
    item = new UIListBtnTypeItem(m_submenuType, tr("Return"));
    item->setData(new Action(&IconView::actionMainMenu));
    item = new UIListBtnTypeItem(m_submenuType, tr("Rotate CW"));
    item->setData(new Action(&IconView::actionRotateCW));
    item = new UIListBtnTypeItem(m_submenuType, tr("Rotate CCW"));
    item->setData(new Action(&IconView::actionRotateCCW));

    m_inSubMenu = true;
}

void IconView::actionSubMenuMark()
{
    clearMenu(m_submenuType);
    m_submenuType->Reset();

    UIListBtnTypeItem *item;
    item = new UIListBtnTypeItem(m_submenuType, tr("Return"));
    item->setData(new Action(&IconView::actionMainMenu));
    item = new UIListBtnTypeItem(m_submenuType, tr("Clear Marked"));
    item->setData(new Action(&IconView::actionClearMarked));
    item = new UIListBtnTypeItem(m_submenuType, tr("Select All"));
    item->setData(new Action(&IconView::actionSelectAll));

    m_inSubMenu = true;
}

void IconView::actionSubMenuFile()
{
    clearMenu(m_submenuType);
    m_submenuType->Reset();

    UIListBtnTypeItem *item;
    item = new UIListBtnTypeItem(m_submenuType, tr("Return"));
    item->setData(new Action(&IconView::actionMainMenu));
    item = new UIListBtnTypeItem(m_submenuType, tr("Show Devices"));
    item->setData(new Action(&IconView::actionShowDevices));
    item = new UIListBtnTypeItem(m_submenuType, tr("Import"));
    item->setData(new Action(&IconView::actionImport));
    item = new UIListBtnTypeItem(m_submenuType, tr("Copy here"));
    item->setData(new Action(&IconView::actionCopyHere));
    item = new UIListBtnTypeItem(m_submenuType, tr("Move here"));
    item->setData(new Action(&IconView::actionMoveHere));
    item = new UIListBtnTypeItem(m_submenuType, tr("Delete"));
    item->setData(new Action(&IconView::actionDelete));
    item = new UIListBtnTypeItem(m_submenuType, tr("Create Dir"));
    item->setData(new Action(&IconView::actionMkDir));

    m_inSubMenu = true;
}

void IconView::actionRotateCW()
{
    ThumbItem* item = m_itemList.at(m_currRow * m_nCols +
                                    m_currCol);
    if (!item || item->isDir)
        return;

    int rotAngle = item->GetRotationAngle();
    
    rotAngle += 90;
    if (rotAngle >= 360)
        rotAngle -= 360;
    if (rotAngle < 0)
        rotAngle += 360;

    item->SetRotationAngle(rotAngle);

    if (item->pixmap) {
        delete item->pixmap;
        item->pixmap = 0;
    }
}

void IconView::actionRotateCCW()
{
    ThumbItem* item = m_itemList.at(m_currRow * m_nCols +
                                    m_currCol);
    if (!item || item->isDir)
        return;

    int rotAngle = item->GetRotationAngle();
    
    rotAngle -= 90;
    if (rotAngle >= 360)
        rotAngle -= 360;
    if (rotAngle < 0)
        rotAngle += 360;

    item->SetRotationAngle(rotAngle);

    if (item->pixmap) {
        delete item->pixmap;
        item->pixmap = 0;
    }
}

void IconView::actionDeleteCurrent()
{
    ThumbItem* item = m_itemList.at(m_currRow * m_nCols + m_currCol);

    if (!item)
        return;

    QString title = tr("Delete Current File or Folder");
    QString msg = (item->isDir) ?
        tr("Deleting 1 folder, including any subfolders and files.") :
        tr("Deleting 1 image.");

    bool cont = MythPopupBox::showOkCancelPopup(
        gContext->GetMainWindow(), title, msg, false);

    if (cont)
    {
        QFileInfo fi;
        fi.setFile(item->path);
        GalleryUtil::Delete(fi);

        loadDirectory(m_currDir);
    }
}

void IconView::actionSlideShow()
{
    ThumbItem* item = m_itemList.at(m_currRow * m_nCols +
                                    m_currCol);
    bool recurse = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);
    if (!item || (item->isDir && !recurse))
        return;

    int pos = m_currRow * m_nCols + m_currCol;

#ifdef OPENGL_SUPPORT
    int useOpenGL = gContext->GetNumSetting("SlideshowUseOpenGL");
    if (useOpenGL) {
        if (QGLFormat::hasOpenGL()) {
            GLSDialog gv(m_itemList, pos, 1, gContext->GetMainWindow());
            gv.exec();
        }
        else {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      tr("Error"),
                                      tr("Sorry: OpenGL support not available"));
        }
    }
    else 
#endif
    {
        SingleView sv(m_itemList, pos, 1, gContext->GetMainWindow());
        sv.exec();
    }                         
}

void IconView::actionRandomShow()
{
    ThumbItem* item = m_itemList.at(m_currRow * m_nCols +
                                    m_currCol);
    bool recurse = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);
    if (!item || (item->isDir && !recurse))
        return;

    int pos = m_currRow * m_nCols + m_currCol;

#ifdef OPENGL_SUPPORT
    int useOpenGL = gContext->GetNumSetting("SlideshowUseOpenGL");
    if (useOpenGL) {
        if (QGLFormat::hasOpenGL()) {
            GLSDialog gv(m_itemList, pos, 2, gContext->GetMainWindow());
            gv.exec();
        }
        else {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      tr("Error"),
                                      tr("Sorry: OpenGL support not available"));
        }
    }
    else 
#endif
    {
        SingleView sv(m_itemList, pos, 2, gContext->GetMainWindow());
        sv.exec();
    }                         
}

void IconView::actionSettings()
{
    GallerySettings settings;
    settings.exec();
}

void IconView::actionImport()
{
    QFileInfo path;
    QDir importdir;

    DialogBox importDiag(gContext->GetMainWindow(), tr("Import pictures?"));
    importDiag.AddButton(tr("No"));
    importDiag.AddButton(tr("Yes"));
    if (importDiag.exec() != 2)
        return;

    QStringList paths = QStringList::split(":",
                                           gContext->GetSetting("GalleryImportDirs"));

    // Makes import directory samba/windows friendly (no colon)
    QString idirname(m_currDir + "/" +
                     (QDateTime::currentDateTime()).toString("yyyy-MM-dd_hh-mm-ss"));
    importdir.mkdir(idirname);
    importdir.setPath(idirname);

    for (QStringList::Iterator it = paths.begin(); it != paths.end(); ++it)
    {
        path.setFile(*it);
        if (path.isDir() && path.isReadable())
        {
            importFromDir(*it,importdir.absPath());
        } 
        else if (path.isFile() && path.isExecutable())
        {
            QString cmd = *it + " " + importdir.absPath();
            cerr << "Executing " << cmd << "\n";
            myth_system(cmd);
        } 
        else
        {
            cerr << "couldn't read/execute" << *it << "\n";
        }
    }

#if QT_VERSION >= 0x030100
    importdir.refresh();
    if (importdir.count() == 0)
#endif
        // (QT < 3.1) rely on automatic fail if dir not empty
        if (importdir.rmdir(importdir.absPath()))
        {
            DialogBox nopicsDiag(gContext->GetMainWindow(),
                                 tr("Nothing found to import"));
            nopicsDiag.AddButton(tr("OK"));
            nopicsDiag.exec();
            return;
        }

    ThumbItem *item = new ThumbItem;
    item->name  = importdir.dirName();
    item->path  = importdir.absPath();
    item->isDir = true;
    m_itemList.append(item);
    m_itemDict.insert(item->name, item);
    m_thumbGen->addFile(item->name);

    if (!m_thumbGen->running())
    {
        m_thumbGen->start();
    }
}

void IconView::actionShowDevices(void)
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
    ThumbItem *item = new ThumbItem;
    item->name = QString("Gallery");
    item->path = m_galleryDir;
    item->isDir = true;
    m_itemList.append(item);
    m_itemDict.insert(item->name, item);

#ifndef _WIN32
    if (mon)
    {
        QValueList<MythMediaDevice*> removables =
            mon->GetMedias(MEDIATYPE_DATA);
        QValueList<MythMediaDevice*>::Iterator it = removables.begin();
        for (; it != removables.end(); it++)
        {
            if (mon->ValidateAndLock(*it))
            {
                item = new ThumbItem;
                if ((*it)->getVolumeID() != "")
                    item->name = (*it)->getVolumeID();
                else
                    item->name = (*it)->getDevicePath();
                item->path = (*it)->getMountPath();
                item->isDir = true;
                item->mediaDevice = *it;
                m_itemList.append(item);
                m_itemDict.insert(item->name, item);

                mon->Unlock(*it);
            }
        }
    }
#endif

    m_lastRow = QMAX((int)ceilf((float)m_itemList.count()/(float)m_nCols)-1,0);
    m_lastCol = QMAX(m_itemList.count()-m_lastRow*m_nCols-1,0);
}

void IconView::actionCopyHere(void)
{
    CopyMarkedFiles(false);
    actionClearMarked();
}

void IconView::actionMoveHere(void)
{
    CopyMarkedFiles(true);
    actionClearMarked();
}

void IconView::actionDelete(void)
{
    if (m_itemMarked.isEmpty())
        actionDeleteCurrent();
    else
        actionDeleteMarked();
}

void IconView::actionDeleteMarked(void)
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

        loadDirectory(m_currDir);
    }
}

void IconView::actionClearMarked(void)
{
    m_itemMarked.clear();
}

void IconView::actionSelectAll(void)
{
    ThumbItem *item;
    for (item = m_itemList.first(); item; item = m_itemList.next())
    {
        if (!m_itemMarked.contains(item->path))
            m_itemMarked.append(item->path);
    }
}

void IconView::actionMkDir(void)
{
    QString folderName = tr("New Folder");

    bool res = MythPopupBox::showGetTextPopup(
        gContext->GetMainWindow(), tr("Create New Folder"),
        tr("Create New Folder"), folderName);

    if (res)
    {
        QDir cdir(m_currDir);
        cdir.mkdir(folderName);

        loadDirectory(m_currDir);
    }
}

void IconView::importFromDir(const QString &fromDir, const QString &toDir)
{
    QDir d(fromDir);

    if (!d.exists())
        return;

    d.setNameFilter(MEDIA_FILENAMES);
    d.setSorting(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);
    d.setFilter(QDir::Files | QDir::Dirs | QDir::NoSymLinks  | QDir::Readable);
    d.setMatchAllDirs(true);
    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;
    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;

        if (fi->isDir())
        {
            QString newdir(toDir + "/" + fi->fileName());
            d.mkdir(newdir);
            importFromDir(fi->absFilePath(), newdir);
        } 
        else 
        {
            cerr << "copying " << fi->absFilePath().local8Bit() 
                 << " to " << toDir.local8Bit() << "\n";
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
    delete progress;

    loadDirectory(m_currDir);
}

void IconView::clearMenu(UIListBtnType *menu)
{
    UIListBtnTypeItem* item = menu->GetItemFirst();
    while (item)
    {
        Action *act = (Action*) item->getData();
        if (act)
            delete act;
        item = menu->GetItemNext(item);
    }
}

void IconView::mediaStatusChanged(MediaStatus oldStatus,
                                  MythMediaDevice *pMedia)
{
    (void) oldStatus;
    if (m_currDevice == pMedia)
    {
        actionShowDevices();

        m_currRow = 0;
        m_currCol = 0;

        updateView();
        updateText();
    }
}
