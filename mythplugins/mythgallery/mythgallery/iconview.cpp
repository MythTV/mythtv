#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qdir.h>
#include <qfileinfo.h>

#include "iconview.h"
#include "singleview.h"
#include "gallerysettings.h"

#include "mythtv/mythcontext.h"
#include "mythtv/dialogbox.h"

IconView::IconView(QSqlDatabase *db, const QString &startdir, 
                   MythMainWindow *parent, const char *name)
        : MythDialog(parent, name)
{
    m_db = db;
    cacheprogress = NULL;

    fgcolor = paletteForegroundColor();
    highlightcolor = fgcolor;

    m_font = new QFont("Arial", (int)(gContext->GetMediumFontSize() * hmult), 
                       QFont::Bold);

    thumbw = screenwidth / (THUMBS_W + 1);
    thumbh = screenheight / (THUMBS_H + 1);

    spacew = thumbw / (THUMBS_W + 1);
    spaceh = (thumbh * 5 / 4) / (THUMBS_H + 1);

    thumbh = (screenheight - spaceh * (THUMBS_H + 1)) / THUMBS_H;

    fillList(startdir);
    screenposition = 0;
    currow = 0;
    curcol = 0;

    QImage *tmpimage = gContext->LoadScaleImage("galleryfolder.png");
    if (tmpimage)
    {
        QImage tmp2 = tmpimage->smoothScale(thumbw, thumbh, QImage::ScaleMin);

        foldericon = new QPixmap();
        foldericon->convertFromImage(tmp2);

        delete tmpimage;
    }

    setNoErase();
}

IconView::~IconView()
{
    while (thumbs.size() > 0)
    {
        if (thumbs.back().pixmap)
            delete thumbs.back().pixmap;
        thumbs.pop_back();
    }

    delete m_font;
    if (foldericon)
        delete foldericon;
}

void IconView::paintEvent(QPaintEvent *e)
{
    e = e;

    QString bgtype = gContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette (QColor(bgtype)));

    if (bgtype == "black") 
    {
        fgcolor = Qt::white;
        highlightcolor = fgcolor;
    }

    QRect r = QRect(0, 0, screenwidth, screenheight);
    QPainter p(this);

    QPixmap pix(r.size());
    pix.fill(this, r.topLeft());

    QPainter tmp;
    tmp.begin(&pix, this);

    tmp.setFont(*m_font);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));

    unsigned int curpos = screenposition;
    for (int y = 0; y < THUMBS_H; y++)
    {
        int ypos = spaceh * (y + 1) + thumbh * y;

        for (int x = 0; x < THUMBS_W; x++)
        {
             if (curpos >= thumbs.size())
                 continue;

             Thumbnail *thumb = &(thumbs[curpos]);

             int xpos = spacew * (x + 1) + thumbw * x;

             if (thumb->isdir && thumb->thumbfilename.isNull())
             {
                 if (foldericon)
                     tmp.drawPixmap(xpos + (thumbw - foldericon->width()) / 2,
                                    ypos, *foldericon);
             }
             else
             {
                 if (!thumb->pixmap)
                     loadThumbPixmap(thumb);

                 if (thumb->pixmap)
                     tmp.drawPixmap(xpos + 
                                    (thumbw - thumb->pixmap->width()) / 2, 
                                    ypos, *thumb->pixmap);        
             }

             tmp.drawText(xpos, ypos + thumbh, thumbw, spaceh, 
                          AlignVCenter | AlignCenter, thumb->name); 

             if (currow == y && curcol == x)
             {
                 tmp.setPen(QPen(highlightcolor, (int)(3.75 * wmult))); 
                 tmp.drawRect(xpos, ypos - (int)(10 * hmult),
                              thumbw, thumbh + spaceh);
                 tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
             }

             curpos++;
        }
    }
 
    if (cacheprogress)
    {
        cacheprogress->Close();
        delete cacheprogress;
        cacheprogress = NULL;
    }
 
    tmp.flush();
    tmp.end();

    p.drawPixmap(r.topLeft(), pix);
}

void IconView::fillList(const QString &dir)
{
    QDir d(dir);

    if (!d.exists())
        return;

    curdir = d.absPath();

    bool isGallery = d.entryInfoList("serial*.dat", QDir::Files);

    QFileInfo cdir(curdir + "/.thumbcache");
    if (!cdir.exists())
        d.mkdir(".thumbcache");

    d.setNameFilter("*.jpg *.JPG *.jpeg *.JPEG *.png *.PNG *.tiff *.TIFF "
                    "*.bmp *.BMP *.gif *.GIF");
    d.setSorting(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

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

        // remove these already-resized pictures.  we'll look
        // specifially for thumbnails a few lines down..
        if (isGallery && (
            (fi->fileName().find(".thumb.") > 0) ||
            (fi->fileName().find(".sized.") > 0) ||
            (fi->fileName().find(".highlight.") > 0)))
            continue;

        QString filename = fi->absFilePath();

        Thumbnail thumb;
        thumb.filename = filename;
        if (isGallery) 
        {
            // if the image name is xyz.jpg, then look
            // for a file named xyz.thumb.jpg.
            QString fn = fi->fileName();
            int firstDot = fn.find('.');
            if (firstDot > 0) 
            {
                fn.insert(firstDot, ".thumb");
                QFileInfo galThumb(curdir + "/" + fn);
                if (galThumb.exists()) 
                {
                    thumb.thumbfilename = galThumb.absFilePath();
                }
            }
        }

        if (fi->isDir()) 
        {
            thumb.isdir = true;
            if (isGallery) 
            {
                // try to find a highlight so we can replace the
                // normal folder pixmap with the chosen one
                QDir subdir(fi->absFilePath(), "*.highlight.*", QDir::Name, 
                            QDir::Files);
                if (subdir.count()>0) {
                    thumb.thumbfilename = subdir.entryInfoList()->getFirst()->absFilePath();
                }
            }
            thumb.name = fi->fileName();
        }
        else
            thumb.name = fi->baseName(true);

        thumbs.push_back(thumb);
    }
}

void IconView::loadThumbPixmap(Thumbnail *thumb)
{
    QImage tmpimage;
    QFileInfo thumbinfo(thumb->filename);
    QString cachename;

    if (!thumb->thumbfilename.isNull())
        cachename = thumb->thumbfilename;
    else
        cachename = curdir + "/.thumbcache/" + thumbinfo.fileName();
    QFileInfo cacheinfo(cachename);

    // scale thumb and cache it if outdated or not exist
    if (!cacheinfo.exists() ||
        ((cacheinfo.lastModified() < thumbinfo.lastModified()) &&
        thumb->thumbfilename.isNull()))
    {
        if (!cacheprogress)
            cacheprogress = new MythProgressDialog(tr("Caching thumbnails..."),
                                                   0);
        tmpimage.load(thumb->filename);
        if (tmpimage.width() == 0 || tmpimage.height() == 0)
            return;
        QImage tmp2 = tmpimage.smoothScale(thumbw, thumbh, QImage::ScaleMin);
        tmp2.save(cachename, "JPEG");
        tmpimage = tmp2;
    }
    else
        tmpimage.load(cachename);

    if (tmpimage.width() == 0 || tmpimage.height() == 0)
        return;

    QString querystr = "SELECT angle FROM gallerymetadata WHERE image =\"" +
                       thumb->filename + "\";";
    QSqlQuery query = m_db->exec(querystr);

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
        tmpimage = tmpimage.xForm(matrix);
    }

    thumb->pixmap = new QPixmap();
    thumb->pixmap->convertFromImage(tmpimage);
}

bool IconView::moveDown() 
{
    currow++;
    if (currow >= THUMBS_H)
    {
        if (screenposition < thumbs.size() - 1)  
            screenposition += 3;
        currow = THUMBS_H - 1;
    }

    if (screenposition + currow * THUMBS_H + curcol >= thumbs.size())
    {
        if (screenposition + currow * THUMBS_H < thumbs.size())
        {
            curcol = 0;
            return true;
        }
        else
            return false;
    }
    else
        return true;
}

bool IconView::moveUp() 
{
    currow--;
    if (currow < 0)
    {
        if (screenposition > 0)
            screenposition -= 3;
        currow = 0;
    }
    return true;
}

bool IconView::moveLeft() 
{
    curcol--;
    if (curcol < 0)
    {
        currow--;
        curcol = THUMBS_W - 1;
        if (currow < 0)
        {
            if (screenposition > 0)
                screenposition -= 3;
            else
                curcol = 0;
            currow = 0;
        }
    }
    return true;
}

bool IconView::moveRight() 
{
    curcol++;
    if (curcol >= THUMBS_W)
    {
        currow++;
        if (currow >= THUMBS_H)
        {
            if (screenposition < thumbs.size() - 1)
                screenposition += 3;
            currow = THUMBS_H - 1;
        }
        curcol = 0;
    }

    if (screenposition + currow * THUMBS_H + curcol >= thumbs.size())
        return false;
    else 
        return true;
}

void IconView::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
 
    int oldrow = currow;
    int oldcol = curcol;
    int oldpos = screenposition;

    switch (e->key())
    {
        case Key_Up:
        {
            handled = moveUp();
            break;
        }
        case Key_Left:
        {
            handled = moveLeft();
            break;
        }
        case Key_Down:
        {
            handled = moveDown();
            break;
        }
        case Key_Right:
        {
            handled = moveRight();
            break;
        }
        case Key_Space:
        case Key_Enter:
        case Key_Return:
        case Key_P:
        {
            int pos = screenposition + currow * THUMBS_W + curcol;

            if (thumbs[pos].isdir)
            {
                IconView iv(m_db, thumbs[pos].filename, 
                            gContext->GetMainWindow()); 
                iv.exec();
            }
            else
            {
                SingleView sv(m_db, &thumbs, pos, gContext->GetMainWindow());
                if (e->key() == Key_P)
                    sv.startShow();
                sv.exec();
            }

            handled = true;
            break;
        }
        case Key_PageUp:
        {
            for (int i = 0; i < THUMBS_H; ++i) 
                 moveUp();
            handled = true;
            break;
        }
        case Key_PageDown:
        {
            for (int i = 0; i < THUMBS_H; ++i) 
                 moveDown();
            handled = true;
            break;
        }
        case Key_Home:
        {
            screenposition = curcol = currow = 0;
            handled = true;
            break;
        }
        case Key_End:
        {
            screenposition = ((thumbs.size() - 1) / (THUMBS_W * THUMBS_H)) * 
                             (THUMBS_W * THUMBS_H);
            currow = (thumbs.size() - screenposition) / THUMBS_W;
            if (screenposition + currow * THUMBS_W + curcol >= thumbs.size())
                curcol = thumbs.size() - (screenposition + currow * THUMBS_W) -
                         1;
            handled = true;
            break;
        }
        case Key_M:
        {
            GallerySettings settings;
            settings.exec(QSqlDatabase::database());
            break;
        }
        case Key_I:
        {
            handled = true;
            importPics();
            screenposition = 0;
            break;
        }
        default: break;
    }

    if (handled)
    {
        update();
    }
    else
    {
        screenposition = oldpos;
        currow = oldrow;
        curcol = oldcol;
        MythDialog::keyPressEvent(e);
    }
}

void IconView::importPics()
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

    QString idirname(curdir + "/" +
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

    Thumbnail thumb;
    thumb.filename = importdir.absPath();
    thumb.isdir = true;
    thumb.name = importdir.dirName();

    vector<Thumbnail>::iterator thumbshead = thumbs.begin();
    thumbs.insert(thumbshead, thumb);
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
            cerr << "copying " << fi->absFilePath() << "to " << toDir << "\n";
            QString cmd = "cp \"" + fi->absFilePath() + "\" \"" + toDir + "\"";
            system(cmd);
        }
    }
}

