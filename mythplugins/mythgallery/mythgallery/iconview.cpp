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

#include <iostream>

#include <qsqldatabase.h>
#include <qevent.h>
#include <qimage.h>
#include <qdir.h>

extern "C" {
#include <math.h>
}

#include "gallerysettings.h"
#include "thumbgenerator.h"
#include "singleview.h"
#include "iconview.h"

#include "config.h"

#ifdef OPENGL_SUPPORT
#include "glsingleview.h"
#endif

IconView::IconView(QSqlDatabase *db, const QString& galleryDir,
                   MythMainWindow* parent, const char* name )
    : MythDialog(parent, name)
{
    m_db         = db;
    m_galleryDir = galleryDir;    

    m_inMenu     = false;
    m_itemList.setAutoDelete(true);
    m_itemDict.setAutoDelete(false);

    setNoErase();
    loadTheme();

    m_thumbGen = new ThumbGenerator(this, (int)(m_thumbW-10*wmult),
                                    (int)(m_thumbH-10*hmult));
    
    loadDirectory(galleryDir);
}

IconView::~IconView()
{
    delete m_thumbGen;    
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
            ttype->SetText(item ? item->name : "");
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
            }
           
            curPos++;
        }
    }

    p.end();
    
    bitBlt(this, m_viewRect.left(), m_viewRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
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
        if (action == "MENU") {
            m_inMenu = !m_inMenu;
            m_menuType->SetActive(m_inMenu);
            menuHandled = true;
        }
        else if (action == "UP") {
            if (m_inMenu) {
                m_menuType->MoveUp();
                menuHandled = true;
            }
            else
                handled = moveUp();
        }
        else if (action == "DOWN") {
            if (m_inMenu) {
                m_menuType->MoveDown();
                menuHandled = true;
            }
            else
                handled = moveDown();
        }
        else if (action == "LEFT") {
            handled = moveLeft();
        }
        else if (action == "RIGHT") {
            handled = moveRight();
        }
        else if (action == "PAGEUP") {
            bool h = true;
            for (int i = 0; i < m_nRows && h; i++) 
                h = moveUp();
            handled = true;
        }
        else if (action == "PAGEDOWN") {
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
        else if (action == "SELECT" || action == "PLAY")
        {
            if (m_inMenu) {
                m_menuType->Toggle();
                menuHandled = true;
            }
            else {
                int pos = m_currRow * m_nCols + m_currCol;
                ThumbItem *item = m_itemList.at(pos);
                if (!item) {
                    std::cerr << "The impossible happened" << std::endl;
                    break;
                }

                if (item->isDir) {
                    loadDirectory(item->path);
                    handled = true;
                }
                else {

                    handled = true;
                    
                    bool slideShow = (action == "PLAY");
#ifdef OPENGL_SUPPORT
                    int useOpenGL = gContext->GetNumSetting("SlideshowUseOpenGL");
                    if (useOpenGL) {
                        GLSDialog gv(m_db, m_itemList, pos, slideShow,
                                     gContext->GetMainWindow());
                        gv.exec();
                    }
                    else 
#endif
                    {
                        SingleView sv(m_db, m_itemList, pos, slideShow,
                                      gContext->GetMainWindow());
                        sv.exec();
                    }                         
                }
            }
        }
        
    }
    
    if (!handled && !menuHandled) {
        gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE") {
                QDir d(m_currDir);
                if (d != QDir(m_galleryDir)) {

                    QString oldDirName = d.dirName();
                    d.cdUp();
                    loadDirectory(d.absPath());

                    // make sure up-directory is visible and selected
                    ThumbItem* item = m_itemDict.find(oldDirName);
                    if (item) {
                        int pos = m_itemList.find(item);
                        if (pos != -1) {
                            m_currRow = pos/m_nCols;
                            m_currCol = pos-m_currRow*m_nCols;
                            m_topRow  = QMAX(0, m_currRow-(m_nRows-1));
                        }
                    }
                    handled = true;
                }
            }
        }
    }

    if (handled || menuHandled) {
        update();
    }
    else
    {
        MythDialog::keyPressEvent(e);
    }

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

        QString queryStr = "SELECT angle FROM gallerymetadata WHERE image =\"" +
                           item->path + "\";";
        QSqlQuery query = m_db->exec(queryStr);
        int rotateAngle = 0;

        if (query.isActive() && query.numRowsAffected() > 0) 
        {
            query.next();
            rotateAngle = query.value(0).toInt();
        }

        if (rotateAngle)
        {
            QWMatrix matrix;
            matrix.rotate(rotateAngle);
            td->thumb = td->thumb.xForm(matrix);
        }

        item->pixmap = new QPixmap(td->thumb);

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
            else {
                std::cerr << "Unknown element: " << e.tagName()
                          << std::endl;
                exit(-1);
            }
        }
    }

    LayerSet *container = m_theme->GetSet("menu");
    if (!container) {
        std::cerr << "MythGallery: Failed to get menu container."
                  << std::endl;
        exit(-1);
    }

    m_menuType = (UIListBtnType*)container->GetType("menu");
    if (!m_menuType) {
        std::cerr << "MythGallery: Failed to get menu area."
                  << std::endl;
        exit(-1);
    }
    connect(m_menuType, SIGNAL(itemChecked(int,bool)),
            SLOT(slotMenuPressed(int,bool)));

    // Menu Actions (Insert in reverse order)
    m_actions.insert("SlideShow", &IconView::actionSlideShow);
    m_actions.insert("Rotate CW", &IconView::actionRotateCW);
    m_actions.insert("Rotate CCW", &IconView::actionRotateCCW);
    m_actions.insert("Import", &IconView::actionImport);
    m_actions.insert("Settings", &IconView::actionSettings);

    // sucks ... can't use an iterator. qmap sorts items alphabetically
    m_menuType->AddItem("SlideShow");
    m_menuType->AddItem("Rotate CW");
    m_menuType->AddItem("Rotate CCW");
    m_menuType->AddItem("Import");
    m_menuType->AddItem("Settings");
    m_menuType->SetActive(false);

    container = m_theme->GetSet("view");
    if (!container) {
        std::cerr << "MythGallery: Failed to get view container."
                  << std::endl;
        exit(-1);
    }

    UIBlackHoleType* bhType = (UIBlackHoleType*)container->GetType("view");
    if (!bhType) {
        std::cerr << "MythGallery: Failed to get view area."
                  << std::endl;
        exit(-1);
    }

    {
        QImage *img = gContext->LoadScaleImage("gallery-back-reg.png");
        if (!img) {
            std::cerr << "Failed to load gallery-back-reg.png"
                      << std::endl;
            exit(-1);
        }
        m_backRegPix = QPixmap(*img);
        delete img;

        img = gContext->LoadScaleImage("gallery-back-sel.png");
        if (!img) {
            std::cerr << "Failed to load gallery-back-sel.png"
                      << std::endl;
            exit(-1);
        }
        m_backSelPix = QPixmap(*img);
        delete img;

        img = gContext->LoadScaleImage("gallery-folder-reg.png");
        if (!img) {
            std::cerr << "Failed to load gallery-folder-reg.png"
                      << std::endl;
            exit(-1);
        }
        m_folderRegPix = QPixmap(*img);
        delete img;

        img = gContext->LoadScaleImage("gallery-folder-sel.png");
        if (!img) {
            std::cerr << "Failed to load gallery-folder-sel.png"
                      << std::endl;
            exit(-1);
        }
        m_folderSelPix = QPixmap(*img);
        delete img;

        m_thumbW = m_backRegPix.width();
        m_thumbH = m_backRegPix.height();
        m_nCols  = m_viewRect.width()/m_thumbW - 1;
        m_nRows  = m_viewRect.height()/m_thumbH - 1;
        m_spaceW = m_thumbW / (m_nCols + 1);
        m_spaceH = m_thumbH / (m_nRows + 1);

    }
}

void IconView::loadDirectory(const QString& dir)
{
    QDir d(dir);
    if (!d.exists())
        return;

    m_currDir = d.absPath();
    m_itemList.clear();
    m_itemDict.clear();

    m_currRow = 0;
    m_currCol = 0;
    m_lastRow = 0;
    m_lastCol = 0;
    m_topRow  = 0;
    
    bool isGallery = d.entryInfoList("serial*.dat", QDir::Files);

    QFileInfo cdir(d.absPath() + "/.thumbcache");
    if (!cdir.exists())
        d.mkdir(".thumbcache");

    d.setNameFilter("*.jpg *.JPG *.jpeg *.JPEG *.png *.PNG *.tiff *.TIFF "
                    "*.tif *.TIF *.bmp *.BMP *.gif *.GIF *.ppm *.PPM");
    d.setSorting(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

    d.setMatchAllDirs(true);
    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    m_thumbGen->cancel();
    m_thumbGen->setDirectory(m_currDir, isGallery);
        
    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;

        // remove these already-resized pictures.  
        if (isGallery && (
                (fi->fileName().find(".thumb.") > 0) ||
                (fi->fileName().find(".sized.") > 0) ||
                (fi->fileName().find(".highlight.") > 0)))
            continue;
        
        ThumbItem* item = new ThumbItem;
        item->name      = fi->fileName();
        item->path      = QDir::cleanDirPath(fi->absFilePath());
        item->isDir     = fi->isDir();

        m_itemList.append(item);
        m_itemDict.insert(item->name, item);
        m_thumbGen->addFile(item->name);
    }

    m_lastRow = QMAX((int)ceilf((float)m_itemList.count()/(float)m_nCols)-1,0);
    m_lastCol = QMAX(m_itemList.count()-m_lastRow*m_nCols-1,0);
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

void IconView::actionRotateCW()
{
    ThumbItem* item = m_itemList.at(m_currRow * m_nCols +
                                    m_currCol);
    if (!item || item->isDir)
        return;

    int rotAngle = 0;
    
    QString queryStr = "SELECT angle FROM gallerymetadata WHERE "
                       "image=\"" + item->path + "\";";
    QSqlQuery query = m_db->exec(queryStr);
			
    if (query.isActive()  && query.numRowsAffected() > 0) 
    {
        query.next();
        rotAngle = query.value(0).toInt();
    }

    rotAngle += 90;
    if (rotAngle >= 360)
        rotAngle -= 360;
    if (rotAngle < 0)
        rotAngle += 360;

    queryStr = "REPLACE INTO gallerymetadata SET image=\"" +
               item->path + "\", angle=" + 
               QString::number(rotAngle) + ";";
    query = m_db->exec(queryStr);

    if (item->pixmap) {
        QPixmap pix(*item->pixmap);

        QWMatrix matrix;
        matrix.rotate(90);
        pix = pix.xForm(matrix);

        delete item->pixmap;
        item->pixmap = new QPixmap(pix);
    }
}

void IconView::actionRotateCCW()
{
    ThumbItem* item = m_itemList.at(m_currRow * m_nCols +
                                    m_currCol);
    if (!item || item->isDir)
        return;

    int rotAngle = 0;
    
    QString queryStr = "SELECT angle FROM gallerymetadata WHERE "
                       "image=\"" + item->path + "\";";
    QSqlQuery query = m_db->exec(queryStr);
			
    if (query.isActive()  && query.numRowsAffected() > 0) 
    {
        query.next();
        rotAngle = query.value(0).toInt();
    }

    rotAngle -= 90;
    if (rotAngle >= 360)
        rotAngle -= 360;
    if (rotAngle < 0)
        rotAngle += 360;

    queryStr = "REPLACE INTO gallerymetadata SET image=\"" +
               item->path + "\", angle=" + 
               QString::number(rotAngle) + ";";
    query = m_db->exec(queryStr);

    if (item->pixmap) {
        QPixmap pix(*item->pixmap);

        QWMatrix matrix;
        matrix.rotate(-90);
        pix = pix.xForm(matrix);

        delete item->pixmap;
        item->pixmap = new QPixmap(pix);
    }
}

void IconView::actionSlideShow()
{
    ThumbItem* item = m_itemList.at(m_currRow * m_nCols +
                                    m_currCol);
    if (!item || item->isDir)
        return;

    int pos = m_currRow * m_nCols + m_currCol;

#ifdef OPENGL_SUPPORT
    int useOpenGL = gContext->GetNumSetting("SlideshowUseOpenGL");
    if (useOpenGL) {
        GLSDialog gv(m_db, m_itemList, pos, true,
                     gContext->GetMainWindow());
        gv.exec();
    }
    else 
#endif
    {
        SingleView sv(m_db, m_itemList, pos, true,
                      gContext->GetMainWindow());
        sv.exec();
    }                         
}

void IconView::actionSettings()
{
    GallerySettings settings;
    settings.exec(QSqlDatabase::database());
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

    QString idirname(m_currDir + "/" +
                     (QDateTime::currentDateTime()).toString(Qt::ISODate));
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
            system(cmd);
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
}

void IconView::importFromDir(const QString &fromDir, const QString &toDir)
{
    QDir d(fromDir);

    if (!d.exists())
        return;

    d.setNameFilter("*.jpg *.JPG *.jpeg *.JPEG *.png *.PNG *.tiff *.TIFF "
                    "*.bmp *.BMP *.gif *.GIF");
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
            cerr << "copying " << fi->absFilePath() << " to " << toDir << "\n";
            QString cmd = "cp \"" + fi->absFilePath() + "\" \"" + toDir + "\"";
            system(cmd);
        }
    }
}


void IconView::slotMenuPressed(int itemPos, bool)
{
    const UIListBtnType::UIListBtnTypeItem* item =
        m_menuType->GetItem(itemPos);

    if (!item)
        return;

    if (m_actions.contains(item->text)) {
        Action act = m_actions[item->text];
        (this->*act)();
    }
}
